// SwiftShader (vulkan.pastel.so) 变体兼容补丁（仅在 variant=pastel 时参与编译）：
//
// 职责：
//   1. SPIR-V Float16 -> Float32 寄存器级提升（修复 MiGlassV13F16 等半精度着色器在 SwiftShader
//      EmitCompositeConstruct 中的空指针崩溃）；
//   2. 将帧缓冲与纹理多重采样上限收敛为 1x (VK_SAMPLE_COUNT_1_BIT)，避免 Flutter Impeller
//      在纯 CPU 软光栅上开启 4x SwapchainMSAA / Offscreen MSAA 导致算力与显存开销放大 4 倍。

#include <stdlib.h>
#include <string.h>

#include <android/log.h>

#include "cdh_gpu_cfg.h"
#include "cdh_spv_fp16.h"
#include "cdh_vulkan_ext.h"

#define CDH_LOG_TAG "cdh-gpu"

static PFN_vkGetPhysicalDeviceImageFormatProperties cdh_real_gpifp;
static PFN_vkGetPhysicalDeviceImageFormatProperties2 cdh_real_gpifp2;
static PFN_vkCreateShaderModule cdh_real_csm;
static int cdh_vk_is_pastel_base;

static int cdh_pastel_should_clamp_msaa(void) {
    if (!cdh_vk_is_pastel_base) {
        return 0;
    }
    uint32_t keep_msaa = 0;
    if (cdh_prop_u32("ro.vendor.cdh.vulkan.keep_msaa", &keep_msaa) && keep_msaa != 0) {
        return 0;
    }
    return 1;
}

void cdh_vk_pastel_on_open(const char *base_soname) {
    cdh_vk_is_pastel_base =
        (base_soname && strstr(base_soname, "pastel") != NULL) ? 1 : 0;
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
    return NULL;
}

PFN_vkVoidFunction cdh_vk_pastel_intercept_gdpa(
    VkDevice device, const char *pName, PFN_vkGetDeviceProcAddr real_gdpa) {
    if (!pName || !real_gdpa) {
        return NULL;
    }
    if (strcmp(pName, "vkCreateShaderModule") == 0) {
        PFN_vkCreateShaderModule fn =
            (PFN_vkCreateShaderModule)real_gdpa(device, pName);
        if (fn) {
            cdh_real_csm = fn;
            return (PFN_vkVoidFunction)cdh_pastel_csm;
        }
    }
    return NULL;
}
