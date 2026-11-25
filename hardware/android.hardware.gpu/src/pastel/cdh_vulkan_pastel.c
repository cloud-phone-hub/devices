// SwiftShader (vulkan.pastel.so) 变体兼容补丁（仅在 variant=pastel 时参与编译）：
//
// 职责：
//   1. SPIR-V Float16 -> Float32 寄存器级提升（修复 MiGlassV13F16 等半精度着色器在 SwiftShader
//      EmitCompositeConstruct 中的空指针崩溃）；
//   2. 将帧缓冲与纹理多重采样上限收敛为 1x (VK_SAMPLE_COUNT_1_BIT)，避免 Flutter Impeller
//      在纯 CPU 软光栅上开启 4x SwapchainMSAA / Offscreen MSAA 导致算力与显存开销放大 4 倍；
//   3. 补齐 Android Impeller 硬性要求的 VK_KHR_external_fence_fd（SYNC_FD 语义）：
//      向 Loader 与应用宣告扩展及导入/导出能力，剥离 SwiftShader 不认识的扩展名与
//      VkExportFenceCreateInfo::SYNC_FD 位，并提供完整的 vkGetFenceFdKHR / vkImportFenceFdKHR
//      与 VkFence 状态跟踪（打通澎湃 OS 3/4 相册 com.miui.gallery 与天气 com.miui.weather2）。

#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <android/log.h>

#include "cdh_gpu_cfg.h"
#include "cdh_spv_fp16.h"
#include "cdh_vk_pastel_fence.h"
#include "cdh_vulkan_ext.h"

#define CDH_LOG_TAG "cdh-gpu"
#define CDH_SYNC_FD_POLL_TIMEOUT_MS 3000

static PFN_vkEnumerateDeviceExtensionProperties cdh_real_edep;
static PFN_vkGetPhysicalDeviceExternalFenceProperties cdh_real_gpdefp;
static PFN_vkGetPhysicalDeviceExternalFencePropertiesKHR cdh_real_gpdefp_khr;
static PFN_vkGetPhysicalDeviceImageFormatProperties cdh_real_gpifp;
static PFN_vkGetPhysicalDeviceImageFormatProperties2 cdh_real_gpifp2;
static PFN_vkCreateDevice cdh_real_create_device;
static PFN_vkGetDeviceProcAddr cdh_cached_gdpa;

static PFN_vkCreateShaderModule cdh_real_csm;
static PFN_vkCreateFence cdh_real_create_fence;
static PFN_vkDestroyFence cdh_real_destroy_fence;
static PFN_vkResetFences cdh_real_reset_fences;
static PFN_vkGetFenceStatus cdh_real_get_fence_status;
static PFN_vkWaitForFences cdh_real_wait_for_fences;
static PFN_vkQueueSubmit cdh_real_queue_submit;
static PFN_vkQueueSubmit2 cdh_real_queue_submit2;
static PFN_vkQueueSubmit2KHR cdh_real_queue_submit2_khr;

static int cdh_vk_is_pastel_base;
static int cdh_vk_pastel_clamp_msaa;

static pthread_mutex_t cdh_fence_mu = PTHREAD_MUTEX_INITIALIZER;
static cdh_vk_fence_table_t cdh_imported_fences;

static inline uint64_t cdh_fence_to_bits(VkFence fence) {
    return (uint64_t)(uintptr_t)fence;
}

static void cdh_fence_mark_signaled(VkFence fence) {
    uint64_t bits = cdh_fence_to_bits(fence);
    if (bits == 0u) {
        return;
    }
    pthread_mutex_lock(&cdh_fence_mu);
    cdh_vk_fence_table_mark(&cdh_imported_fences, bits);
    pthread_mutex_unlock(&cdh_fence_mu);
}

static void cdh_fence_unmark_signaled(VkFence fence) {
    uint64_t bits = cdh_fence_to_bits(fence);
    if (bits == 0u) {
        return;
    }
    pthread_mutex_lock(&cdh_fence_mu);
    cdh_vk_fence_table_unmark(&cdh_imported_fences, bits);
    pthread_mutex_unlock(&cdh_fence_mu);
}

static int cdh_fence_is_marked_signaled(VkFence fence) {
    uint64_t bits = cdh_fence_to_bits(fence);
    if (bits == 0u) {
        return 0;
    }
    pthread_mutex_lock(&cdh_fence_mu);
    int hit = cdh_vk_fence_table_contains(&cdh_imported_fences, bits);
    pthread_mutex_unlock(&cdh_fence_mu);
    return hit;
}

// 仅从 gpu_id.cfg 读取可选开关，避免普通 App 反复调用 __system_property_get 触发
// Bionic "Access denied finding property ro.vendor.cdh.vulkan.keep_msaa" 告警。
static int cdh_cfg_only_u32(const char *key, uint32_t *out) {
    if (!key || !out) {
        return 0;
    }
    cdh_load_cfg_once();
    for (size_t i = 0; i < g_cdh_gpu_cfg.count; i++) {
        if (strcmp(g_cdh_gpu_cfg.keys[i], key) == 0) {
            const char *val = g_cdh_gpu_cfg.vals[i];
            if (!val || val[0] == '\0') {
                return 0;
            }
            char *end = NULL;
            unsigned long v = strtoul(val, &end, 0);
            if (end == val || *end != '\0') {
                return 0;
            }
            *out = (uint32_t)v;
            return 1;
        }
    }
    return 0;
}

static int cdh_pastel_should_clamp_msaa(void) {
    return cdh_vk_is_pastel_base && cdh_vk_pastel_clamp_msaa;
}

void cdh_vk_pastel_on_open(const char *base_soname) {
    cdh_vk_is_pastel_base =
        (base_soname && strstr(base_soname, "pastel") != NULL) ? 1 : 0;
    if (cdh_vk_is_pastel_base) {
        uint32_t keep_msaa = 0;
        cdh_vk_pastel_clamp_msaa =
            (cdh_cfg_only_u32("ro.vendor.cdh.vulkan.keep_msaa", &keep_msaa) &&
             keep_msaa != 0u)
                ? 0
                : 1;
    } else {
        cdh_vk_pastel_clamp_msaa = 0;
    }
}

void cdh_vk_pastel_stamp_limits(VkPhysicalDeviceProperties *p) {
    if (!p || !cdh_pastel_should_clamp_msaa()) {
        return;
    }
    p->limits.framebufferColorSampleCounts = VK_SAMPLE_COUNT_1_BIT;
    p->limits.framebufferDepthSampleCounts = VK_SAMPLE_COUNT_1_BIT;
    p->limits.framebufferStencilSampleCounts = VK_SAMPLE_COUNT_1_BIT;
    p->limits.framebufferNoAttachmentsSampleCounts = VK_SAMPLE_COUNT_1_BIT;
    p->limits.sampledImageColorSampleCounts = VK_SAMPLE_COUNT_1_BIT;
    p->limits.sampledImageIntegerSampleCounts = VK_SAMPLE_COUNT_1_BIT;
    p->limits.sampledImageDepthSampleCounts = VK_SAMPLE_COUNT_1_BIT;
    p->limits.sampledImageStencilSampleCounts = VK_SAMPLE_COUNT_1_BIT;
    p->limits.storageImageSampleCounts = VK_SAMPLE_COUNT_1_BIT;
    p->limits.maxSamplerAnisotropy = 1.0f;
}

void cdh_vk_pastel_stamp_v12(VkPhysicalDeviceVulkan12Properties *v12) {
    if (!v12 || !cdh_pastel_should_clamp_msaa()) {
        return;
    }
    v12->framebufferIntegerColorSampleCounts = VK_SAMPLE_COUNT_1_BIT;
}

static VkResult VKAPI_CALL cdh_pastel_edep(
    VkPhysicalDevice physicalDevice, const char *pLayerName,
    uint32_t *pPropertyCount, VkExtensionProperties *pProperties) {
    if (!cdh_real_edep) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if (!cdh_vk_is_pastel_base || (pLayerName && pLayerName[0] != '\0') ||
        !pPropertyCount) {
        return cdh_real_edep(physicalDevice, pLayerName, pPropertyCount,
                             pProperties);
    }

    uint32_t base_count = 0;
    VkResult res = cdh_real_edep(physicalDevice, NULL, &base_count, NULL);
    if (res != VK_SUCCESS) {
        return res;
    }

    int base_has_fence_fd = 0;
    if (base_count > 0u) {
        VkExtensionProperties *tmp = (VkExtensionProperties *)malloc(
            (size_t)base_count * sizeof(VkExtensionProperties));
        if (tmp) {
            uint32_t n = base_count;
            if (cdh_real_edep(physicalDevice, NULL, &n, tmp) == VK_SUCCESS) {
                for (uint32_t i = 0; i < n; i++) {
                    if (strcmp(tmp[i].extensionName,
                               CDH_VK_EXT_EXTERNAL_FENCE_FD_NAME) == 0) {
                        base_has_fence_fd = 1;
                        break;
                    }
                }
            }
            free(tmp);
        }
    }

    if (base_has_fence_fd) {
        return cdh_real_edep(physicalDevice, NULL, pPropertyCount, pProperties);
    }

    if (!pProperties) {
        *pPropertyCount = base_count + 1u;
        return VK_SUCCESS;
    }

    uint32_t caller_cap = *pPropertyCount;
    if (caller_cap == 0u) {
        return (base_count + 1u > 0u) ? VK_INCOMPLETE : VK_SUCCESS;
    }

    if (caller_cap <= base_count) {
        res = cdh_real_edep(physicalDevice, NULL, pPropertyCount, pProperties);
        return (res == VK_SUCCESS) ? VK_INCOMPLETE : res;
    }

    uint32_t written = base_count;
    res = cdh_real_edep(physicalDevice, NULL, &written, pProperties);
    if (res != VK_SUCCESS && res != VK_INCOMPLETE) {
        return res;
    }

    memset(&pProperties[written], 0, sizeof(VkExtensionProperties));
    strncpy(pProperties[written].extensionName,
            CDH_VK_EXT_EXTERNAL_FENCE_FD_NAME,
            VK_MAX_EXTENSION_NAME_SIZE - 1u);
    pProperties[written].specVersion = CDH_VK_EXT_EXTERNAL_FENCE_FD_SPEC_VERSION;
    *pPropertyCount = written + 1u;
    return VK_SUCCESS;
}

static void cdh_patch_external_fence_props(
    const VkPhysicalDeviceExternalFenceInfo *pInfo,
    VkExternalFenceProperties *pProps) {
    if (!cdh_vk_is_pastel_base || !pInfo || !pProps) {
        return;
    }
    if ((pInfo->handleType & VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT) != 0) {
        pProps->exportFromImportedHandleTypes =
            VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT;
        pProps->compatibleHandleTypes =
            VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT;
        pProps->externalFenceFeatures =
            VK_EXTERNAL_FENCE_FEATURE_EXPORTABLE_BIT |
            VK_EXTERNAL_FENCE_FEATURE_IMPORTABLE_BIT;
    }
}

static void VKAPI_CALL cdh_pastel_gpdefp(
    VkPhysicalDevice physicalDevice,
    const VkPhysicalDeviceExternalFenceInfo *pExternalFenceInfo,
    VkExternalFenceProperties *pExternalFenceProperties) {
    if (cdh_real_gpdefp) {
        cdh_real_gpdefp(physicalDevice, pExternalFenceInfo,
                        pExternalFenceProperties);
    } else if (cdh_real_gpdefp_khr) {
        cdh_real_gpdefp_khr(physicalDevice, pExternalFenceInfo,
                            pExternalFenceProperties);
    }
    cdh_patch_external_fence_props(pExternalFenceInfo, pExternalFenceProperties);
}

static void VKAPI_CALL cdh_pastel_gpdefp_khr(
    VkPhysicalDevice physicalDevice,
    const VkPhysicalDeviceExternalFenceInfo *pExternalFenceInfo,
    VkExternalFenceProperties *pExternalFenceProperties) {
    if (cdh_real_gpdefp_khr) {
        cdh_real_gpdefp_khr(physicalDevice, pExternalFenceInfo,
                            pExternalFenceProperties);
    } else if (cdh_real_gpdefp) {
        cdh_real_gpdefp(physicalDevice, pExternalFenceInfo,
                        pExternalFenceProperties);
    }
    cdh_patch_external_fence_props(pExternalFenceInfo, pExternalFenceProperties);
}

static void cdh_bind_device_procs(VkDevice device,
                                  PFN_vkGetDeviceProcAddr real_gdpa) {
    if (!device || !real_gdpa) {
        return;
    }
    if (!cdh_real_csm) {
        cdh_real_csm =
            (PFN_vkCreateShaderModule)real_gdpa(device, "vkCreateShaderModule");
    }
    if (!cdh_real_create_fence) {
        cdh_real_create_fence =
            (PFN_vkCreateFence)real_gdpa(device, "vkCreateFence");
    }
    if (!cdh_real_destroy_fence) {
        cdh_real_destroy_fence =
            (PFN_vkDestroyFence)real_gdpa(device, "vkDestroyFence");
    }
    if (!cdh_real_reset_fences) {
        cdh_real_reset_fences =
            (PFN_vkResetFences)real_gdpa(device, "vkResetFences");
    }
    if (!cdh_real_get_fence_status) {
        cdh_real_get_fence_status =
            (PFN_vkGetFenceStatus)real_gdpa(device, "vkGetFenceStatus");
    }
    if (!cdh_real_wait_for_fences) {
        cdh_real_wait_for_fences =
            (PFN_vkWaitForFences)real_gdpa(device, "vkWaitForFences");
    }
    if (!cdh_real_queue_submit) {
        cdh_real_queue_submit =
            (PFN_vkQueueSubmit)real_gdpa(device, "vkQueueSubmit");
    }
    if (!cdh_real_queue_submit2) {
        cdh_real_queue_submit2 =
            (PFN_vkQueueSubmit2)real_gdpa(device, "vkQueueSubmit2");
    }
    if (!cdh_real_queue_submit2_khr) {
        cdh_real_queue_submit2_khr =
            (PFN_vkQueueSubmit2KHR)real_gdpa(device, "vkQueueSubmit2KHR");
    }
}

static VkResult VKAPI_CALL cdh_pastel_create_device(
    VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkDevice *pDevice) {
    if (!cdh_real_create_device) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if (!cdh_vk_is_pastel_base || !pCreateInfo) {
        return cdh_real_create_device(physicalDevice, pCreateInfo, pAllocator,
                                      pDevice);
    }

    const char **filtered_exts = NULL;
    uint32_t filtered_count = pCreateInfo->enabledExtensionCount;
    VkDeviceCreateInfo modified_info = *pCreateInfo;
    if (cdh_vk_filter_device_extensions(
            pCreateInfo->ppEnabledExtensionNames,
            pCreateInfo->enabledExtensionCount, &filtered_exts,
            &filtered_count)) {
        modified_info.enabledExtensionCount = filtered_count;
        modified_info.ppEnabledExtensionNames = filtered_exts;
    }

    VkResult res = cdh_real_create_device(physicalDevice, &modified_info,
                                          pAllocator, pDevice);
    if (filtered_exts) {
        free(filtered_exts);
    }
    if (res == VK_SUCCESS && pDevice && *pDevice && cdh_cached_gdpa) {
        cdh_bind_device_procs(*pDevice, cdh_cached_gdpa);
    }
    return res;
}

static VkResult VKAPI_CALL cdh_pastel_create_fence(
    VkDevice device, const VkFenceCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkFence *pFence) {
    if (!cdh_real_create_fence && cdh_cached_gdpa) {
        cdh_bind_device_procs(device, cdh_cached_gdpa);
    }
    if (!cdh_real_create_fence) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    VkFenceCreateInfo modified_info;
    VkExportFenceCreateInfo patched_export;
    const VkFenceCreateInfo *pass_info = pCreateInfo;
    if (cdh_vk_is_pastel_base && pCreateInfo && pCreateInfo->pNext) {
        const VkBaseInStructure *head =
            (const VkBaseInStructure *)pCreateInfo->pNext;
        if (head->sType == VK_STRUCTURE_TYPE_EXPORT_FENCE_CREATE_INFO) {
            const VkExportFenceCreateInfo *exp =
                (const VkExportFenceCreateInfo *)pCreateInfo->pNext;
            if ((exp->handleTypes & VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT) !=
                0) {
                modified_info = *pCreateInfo;
                VkExternalFenceHandleTypeFlags rem =
                    exp->handleTypes &
                    ~(VkExternalFenceHandleTypeFlags)
                        VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT;
                if (rem == 0) {
                    modified_info.pNext = exp->pNext;
                } else {
                    patched_export = *exp;
                    patched_export.handleTypes = rem;
                    modified_info.pNext = &patched_export;
                }
                pass_info = &modified_info;
            }
        }
    }
    VkResult res = cdh_real_create_fence(device, pass_info, pAllocator, pFence);
    if (res == VK_SUCCESS && pFence && *pFence != VK_NULL_HANDLE) {
        cdh_fence_unmark_signaled(*pFence);
    }
    return res;
}

static void VKAPI_CALL cdh_pastel_destroy_fence(
    VkDevice device, VkFence fence, const VkAllocationCallbacks *pAllocator) {
    if (fence != VK_NULL_HANDLE) {
        cdh_fence_unmark_signaled(fence);
    }
    if (!cdh_real_destroy_fence && cdh_cached_gdpa) {
        cdh_bind_device_procs(device, cdh_cached_gdpa);
    }
    if (cdh_real_destroy_fence) {
        cdh_real_destroy_fence(device, fence, pAllocator);
    }
}

static VkResult VKAPI_CALL cdh_pastel_reset_fences(uint32_t fenceCount,
                                                   const VkFence *pFences,
                                                   VkDevice device) {
    if (pFences) {
        for (uint32_t i = 0; i < fenceCount; i++) {
            cdh_fence_unmark_signaled(pFences[i]);
        }
    }
    if (!cdh_real_reset_fences && cdh_cached_gdpa) {
        cdh_bind_device_procs(device, cdh_cached_gdpa);
    }
    return cdh_real_reset_fences
               ? cdh_real_reset_fences(device, fenceCount, pFences)
               : VK_ERROR_INITIALIZATION_FAILED;
}

static VkResult VKAPI_CALL cdh_pastel_reset_fences_entry(
    VkDevice device, uint32_t fenceCount, const VkFence *pFences) {
    return cdh_pastel_reset_fences(fenceCount, pFences, device);
}

static VkResult VKAPI_CALL cdh_pastel_get_fence_status(VkDevice device,
                                                       VkFence fence) {
    if (cdh_fence_is_marked_signaled(fence)) {
        return VK_SUCCESS;
    }
    if (!cdh_real_get_fence_status && cdh_cached_gdpa) {
        cdh_bind_device_procs(device, cdh_cached_gdpa);
    }
    return cdh_real_get_fence_status
               ? cdh_real_get_fence_status(device, fence)
               : VK_ERROR_INITIALIZATION_FAILED;
}

static VkResult VKAPI_CALL cdh_pastel_wait_for_fences(
    VkDevice device, uint32_t fenceCount, const VkFence *pFences,
    VkBool32 waitAll, uint64_t timeout) {
    if (fenceCount == 0u || !pFences) {
        return VK_SUCCESS;
    }
    if (!cdh_real_wait_for_fences && cdh_cached_gdpa) {
        cdh_bind_device_procs(device, cdh_cached_gdpa);
    }
    if (!cdh_real_wait_for_fences) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkFence stack_buf[16];
    VkFence *rem_fences = stack_buf;
    if (fenceCount > 16u) {
        rem_fences = (VkFence *)malloc((size_t)fenceCount * sizeof(VkFence));
        if (!rem_fences) {
            return cdh_real_wait_for_fences(device, fenceCount, pFences,
                                            waitAll, timeout);
        }
    }

    uint32_t rem_count = 0;
    int any_marked = 0;
    for (uint32_t i = 0; i < fenceCount; i++) {
        if (cdh_fence_is_marked_signaled(pFences[i])) {
            any_marked = 1;
        } else {
            rem_fences[rem_count++] = pFences[i];
        }
    }

    VkResult res = VK_SUCCESS;
    if ((!waitAll && any_marked) || rem_count == 0u) {
        res = VK_SUCCESS;
    } else {
        res = cdh_real_wait_for_fences(device, rem_count, rem_fences, waitAll,
                                       timeout);
    }

    if (rem_fences != stack_buf) {
        free(rem_fences);
    }
    return res;
}

static VkResult VKAPI_CALL cdh_pastel_queue_submit(
    VkQueue queue, uint32_t submitCount, const VkSubmitInfo *pSubmits,
    VkFence fence) {
    if (fence != VK_NULL_HANDLE) {
        cdh_fence_unmark_signaled(fence);
    }
    if (!cdh_real_queue_submit) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    return cdh_real_queue_submit(queue, submitCount, pSubmits, fence);
}

static VkResult VKAPI_CALL cdh_pastel_queue_submit2(
    VkQueue queue, uint32_t submitCount, const VkSubmitInfo2 *pSubmits,
    VkFence fence) {
    if (fence != VK_NULL_HANDLE) {
        cdh_fence_unmark_signaled(fence);
    }
    if (cdh_real_queue_submit2) {
        return cdh_real_queue_submit2(queue, submitCount, pSubmits, fence);
    }
    if (cdh_real_queue_submit2_khr) {
        return cdh_real_queue_submit2_khr(queue, submitCount, pSubmits, fence);
    }
    return VK_ERROR_INITIALIZATION_FAILED;
}

static VkResult VKAPI_CALL cdh_pastel_get_fence_fd(
    VkDevice device, const VkFenceGetFdInfoKHR *pGetFdInfo, int *pFd) {
    if (!pGetFdInfo || !pFd) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if ((pGetFdInfo->handleType & VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT) ==
        0) {
        return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    }
    if (!cdh_real_wait_for_fences && cdh_cached_gdpa) {
        cdh_bind_device_procs(device, cdh_cached_gdpa);
    }
    VkFence fence = pGetFdInfo->fence;
    if (fence != VK_NULL_HANDLE && !cdh_fence_is_marked_signaled(fence) &&
        cdh_real_wait_for_fences) {
        (void)cdh_real_wait_for_fences(device, 1u, &fence, VK_TRUE, UINT64_MAX);
    }
    // 按 Vulkan SYNC_FD 拷贝传递规范：导出后源 VkFence 回到未信号态；
    // 已完成的 SYNC_FD 允许直接返回 -1（Android sync_fd 已完成语义）。
    if (fence != VK_NULL_HANDLE) {
        cdh_fence_unmark_signaled(fence);
        if (cdh_real_reset_fences) {
            (void)cdh_real_reset_fences(device, 1u, &fence);
        }
    }
    *pFd = -1;
    return VK_SUCCESS;
}

static VkResult VKAPI_CALL cdh_pastel_import_fence_fd(
    VkDevice device, const VkImportFenceFdInfoKHR *pImportFenceFdInfo) {
    (void)device;
    if (!pImportFenceFdInfo) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if ((pImportFenceFdInfo->handleType &
         VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT) == 0) {
        return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    }
    int fd = pImportFenceFdInfo->fd;
    if (fd >= 0) {
        struct pollfd pfd;
        memset(&pfd, 0, sizeof(pfd));
        pfd.fd = fd;
        pfd.events = POLLIN;
        int rc;
        do {
            rc = poll(&pfd, 1, CDH_SYNC_FD_POLL_TIMEOUT_MS);
        } while (rc < 0 && errno == EINTR);
        // Vulkan SYNC_FD 导入成功后由驱动接管并关闭 fd 所有权。
        close(fd);
    }
    if (pImportFenceFdInfo->fence != VK_NULL_HANDLE) {
        cdh_fence_mark_signaled(pImportFenceFdInfo->fence);
    }
    return VK_SUCCESS;
}

static VkResult VKAPI_CALL cdh_pastel_gpifp(
    VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type,
    VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags,
    VkImageFormatProperties *pImageFormatProperties) {
    if (!cdh_real_gpifp) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    VkResult res = cdh_real_gpifp(physicalDevice, format, type, tiling, usage,
                                  flags, pImageFormatProperties);
    if (res == VK_SUCCESS && pImageFormatProperties &&
        cdh_pastel_should_clamp_msaa()) {
        pImageFormatProperties->sampleCounts = VK_SAMPLE_COUNT_1_BIT;
    }
    return res;
}

static VkResult VKAPI_CALL cdh_pastel_gpifp2(
    VkPhysicalDevice physicalDevice,
    const VkPhysicalDeviceImageFormatInfo2 *pImageFormatInfo,
    VkImageFormatProperties2 *pImageFormatProperties) {
    if (!cdh_real_gpifp2) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    VkResult res = cdh_real_gpifp2(physicalDevice, pImageFormatInfo,
                                   pImageFormatProperties);
    if (res == VK_SUCCESS && pImageFormatProperties &&
        cdh_pastel_should_clamp_msaa()) {
        pImageFormatProperties->imageFormatProperties.sampleCounts =
            VK_SAMPLE_COUNT_1_BIT;
    }
    return res;
}

static VkResult VKAPI_CALL cdh_pastel_csm(
    VkDevice device, const VkShaderModuleCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkShaderModule *pShaderModule) {
    if (!cdh_real_csm && cdh_cached_gdpa) {
        cdh_bind_device_procs(device, cdh_cached_gdpa);
    }
    if (!cdh_real_csm) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if (!pCreateInfo || !pCreateInfo->pCode) {
        return cdh_real_csm(device, pCreateInfo, pAllocator, pShaderModule);
    }
    cdh_spv_patch_stats_t stats;
    uint32_t *patched = cdh_spv_promote_fp16_copy(
        pCreateInfo->pCode, pCreateInfo->codeSize, &stats);
    if (!patched) {
        return cdh_real_csm(device, pCreateInfo, pAllocator, pShaderModule);
    }
    VkShaderModuleCreateInfo modified_info = *pCreateInfo;
    modified_info.pCode = patched;
    VkResult res =
        cdh_real_csm(device, &modified_info, pAllocator, pShaderModule);
    free(patched);
    __android_log_print(
        ANDROID_LOG_INFO, CDH_LOG_TAG,
        "vulkan[pastel]: SPIR-V Float16->Float32 已改写 (size=%zu, fp16_types=%zu, "
        "consts=%u, fconverts=%u) -> %d",
        pCreateInfo->codeSize, stats.fp16_types, stats.consts, stats.fconverts,
        (int)res);
    return res;
}

PFN_vkVoidFunction cdh_vk_pastel_intercept_gipa(
    VkInstance instance, const char *pName,
    PFN_vkGetInstanceProcAddr base_gipa) {
    if (!pName || !base_gipa) {
        return NULL;
    }
    if (!cdh_cached_gdpa && instance) {
        cdh_cached_gdpa =
            (PFN_vkGetDeviceProcAddr)base_gipa(instance, "vkGetDeviceProcAddr");
    }
    if (strcmp(pName, "vkEnumerateDeviceExtensionProperties") == 0) {
        cdh_real_edep =
            (PFN_vkEnumerateDeviceExtensionProperties)base_gipa(instance,
                                                                pName);
        return cdh_real_edep ? (PFN_vkVoidFunction)cdh_pastel_edep : NULL;
    }
    if (strcmp(pName, "vkGetPhysicalDeviceExternalFenceProperties") == 0) {
        cdh_real_gpdefp =
            (PFN_vkGetPhysicalDeviceExternalFenceProperties)base_gipa(instance,
                                                                      pName);
        return cdh_real_gpdefp ? (PFN_vkVoidFunction)cdh_pastel_gpdefp : NULL;
    }
    if (strcmp(pName, "vkGetPhysicalDeviceExternalFencePropertiesKHR") == 0) {
        cdh_real_gpdefp_khr =
            (PFN_vkGetPhysicalDeviceExternalFencePropertiesKHR)base_gipa(
                instance, pName);
        if (!cdh_real_gpdefp) {
            cdh_real_gpdefp =
                (PFN_vkGetPhysicalDeviceExternalFenceProperties)base_gipa(
                    instance, "vkGetPhysicalDeviceExternalFenceProperties");
        }
        return (cdh_real_gpdefp_khr || cdh_real_gpdefp)
                   ? (PFN_vkVoidFunction)cdh_pastel_gpdefp_khr
                   : NULL;
    }
    if (strcmp(pName, "vkCreateDevice") == 0) {
        cdh_real_create_device =
            (PFN_vkCreateDevice)base_gipa(instance, pName);
        return cdh_real_create_device
                   ? (PFN_vkVoidFunction)cdh_pastel_create_device
                   : NULL;
    }
    if (strcmp(pName, "vkGetPhysicalDeviceImageFormatProperties") == 0) {
        cdh_real_gpifp =
            (PFN_vkGetPhysicalDeviceImageFormatProperties)base_gipa(instance,
                                                                    pName);
        return cdh_real_gpifp ? (PFN_vkVoidFunction)cdh_pastel_gpifp : NULL;
    }
    if (strcmp(pName, "vkGetPhysicalDeviceImageFormatProperties2") == 0 ||
        strcmp(pName, "vkGetPhysicalDeviceImageFormatProperties2KHR") == 0) {
        cdh_real_gpifp2 =
            (PFN_vkGetPhysicalDeviceImageFormatProperties2)base_gipa(instance,
                                                                     pName);
        return cdh_real_gpifp2 ? (PFN_vkVoidFunction)cdh_pastel_gpifp2 : NULL;
    }
    if (strcmp(pName, "vkCreateShaderModule") == 0) {
        cdh_real_csm = (PFN_vkCreateShaderModule)base_gipa(instance, pName);
        return cdh_real_csm ? (PFN_vkVoidFunction)cdh_pastel_csm : NULL;
    }
    if (cdh_vk_is_pastel_base) {
        if (strcmp(pName, "vkCreateFence") == 0) {
            cdh_real_create_fence =
                (PFN_vkCreateFence)base_gipa(instance, pName);
            return cdh_real_create_fence
                       ? (PFN_vkVoidFunction)cdh_pastel_create_fence
                       : NULL;
        }
        if (strcmp(pName, "vkDestroyFence") == 0) {
            cdh_real_destroy_fence =
                (PFN_vkDestroyFence)base_gipa(instance, pName);
            return cdh_real_destroy_fence
                       ? (PFN_vkVoidFunction)cdh_pastel_destroy_fence
                       : NULL;
        }
        if (strcmp(pName, "vkResetFences") == 0) {
            cdh_real_reset_fences =
                (PFN_vkResetFences)base_gipa(instance, pName);
            return cdh_real_reset_fences
                       ? (PFN_vkVoidFunction)cdh_pastel_reset_fences_entry
                       : NULL;
        }
        if (strcmp(pName, "vkGetFenceStatus") == 0) {
            cdh_real_get_fence_status =
                (PFN_vkGetFenceStatus)base_gipa(instance, pName);
            return cdh_real_get_fence_status
                       ? (PFN_vkVoidFunction)cdh_pastel_get_fence_status
                       : NULL;
        }
        if (strcmp(pName, "vkWaitForFences") == 0) {
            cdh_real_wait_for_fences =
                (PFN_vkWaitForFences)base_gipa(instance, pName);
            return cdh_real_wait_for_fences
                       ? (PFN_vkVoidFunction)cdh_pastel_wait_for_fences
                       : NULL;
        }
        if (strcmp(pName, "vkQueueSubmit") == 0) {
            cdh_real_queue_submit =
                (PFN_vkQueueSubmit)base_gipa(instance, pName);
            return cdh_real_queue_submit
                       ? (PFN_vkVoidFunction)cdh_pastel_queue_submit
                       : NULL;
        }
        if (strcmp(pName, "vkQueueSubmit2") == 0 ||
            strcmp(pName, "vkQueueSubmit2KHR") == 0) {
            PFN_vkVoidFunction fn = base_gipa(instance, pName);
            if (strcmp(pName, "vkQueueSubmit2") == 0) {
                cdh_real_queue_submit2 = (PFN_vkQueueSubmit2)fn;
            } else {
                cdh_real_queue_submit2_khr = (PFN_vkQueueSubmit2KHR)fn;
            }
            return fn ? (PFN_vkVoidFunction)cdh_pastel_queue_submit2 : NULL;
        }
        if (strcmp(pName, "vkGetFenceFdKHR") == 0) {
            return (PFN_vkVoidFunction)cdh_pastel_get_fence_fd;
        }
        if (strcmp(pName, "vkImportFenceFdKHR") == 0) {
            return (PFN_vkVoidFunction)cdh_pastel_import_fence_fd;
        }
    }
    return NULL;
}

PFN_vkVoidFunction cdh_vk_pastel_intercept_gdpa(
    VkDevice device, const char *pName, PFN_vkGetDeviceProcAddr real_gdpa) {
    if (!pName || !real_gdpa) {
        return NULL;
    }
    cdh_cached_gdpa = real_gdpa;
    if (strcmp(pName, "vkCreateShaderModule") == 0) {
        PFN_vkCreateShaderModule fn =
            (PFN_vkCreateShaderModule)real_gdpa(device, pName);
        if (fn) {
            cdh_real_csm = fn;
            return (PFN_vkVoidFunction)cdh_pastel_csm;
        }
    }
    if (!cdh_vk_is_pastel_base) {
        return NULL;
    }
    if (strcmp(pName, "vkCreateFence") == 0) {
        PFN_vkCreateFence fn = (PFN_vkCreateFence)real_gdpa(device, pName);
        if (fn) {
            cdh_real_create_fence = fn;
            return (PFN_vkVoidFunction)cdh_pastel_create_fence;
        }
    }
    if (strcmp(pName, "vkDestroyFence") == 0) {
        PFN_vkDestroyFence fn = (PFN_vkDestroyFence)real_gdpa(device, pName);
        if (fn) {
            cdh_real_destroy_fence = fn;
            return (PFN_vkVoidFunction)cdh_pastel_destroy_fence;
        }
    }
    if (strcmp(pName, "vkResetFences") == 0) {
        PFN_vkResetFences fn = (PFN_vkResetFences)real_gdpa(device, pName);
        if (fn) {
            cdh_real_reset_fences = fn;
            return (PFN_vkVoidFunction)cdh_pastel_reset_fences_entry;
        }
    }
    if (strcmp(pName, "vkGetFenceStatus") == 0) {
        PFN_vkGetFenceStatus fn =
            (PFN_vkGetFenceStatus)real_gdpa(device, pName);
        if (fn) {
            cdh_real_get_fence_status = fn;
            return (PFN_vkVoidFunction)cdh_pastel_get_fence_status;
        }
    }
    if (strcmp(pName, "vkWaitForFences") == 0) {
        PFN_vkWaitForFences fn = (PFN_vkWaitForFences)real_gdpa(device, pName);
        if (fn) {
            cdh_real_wait_for_fences = fn;
            return (PFN_vkVoidFunction)cdh_pastel_wait_for_fences;
        }
    }
    if (strcmp(pName, "vkQueueSubmit") == 0) {
        PFN_vkQueueSubmit fn = (PFN_vkQueueSubmit)real_gdpa(device, pName);
        if (fn) {
            cdh_real_queue_submit = fn;
            return (PFN_vkVoidFunction)cdh_pastel_queue_submit;
        }
    }
    if (strcmp(pName, "vkQueueSubmit2") == 0 ||
        strcmp(pName, "vkQueueSubmit2KHR") == 0) {
        PFN_vkVoidFunction fn = real_gdpa(device, pName);
        if (fn) {
            if (strcmp(pName, "vkQueueSubmit2") == 0) {
                cdh_real_queue_submit2 = (PFN_vkQueueSubmit2)fn;
            } else {
                cdh_real_queue_submit2_khr = (PFN_vkQueueSubmit2KHR)fn;
            }
            return (PFN_vkVoidFunction)cdh_pastel_queue_submit2;
        }
    }
    if (strcmp(pName, "vkGetFenceFdKHR") == 0) {
        cdh_bind_device_procs(device, real_gdpa);
        return (PFN_vkVoidFunction)cdh_pastel_get_fence_fd;
    }
    if (strcmp(pName, "vkImportFenceFdKHR") == 0) {
        cdh_bind_device_procs(device, real_gdpa);
        return (PFN_vkVoidFunction)cdh_pastel_import_fence_fd;
    }
    return NULL;
}
