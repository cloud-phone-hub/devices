// note13-5G-15 Vulkan HAL interposer：产物 lib64/hw/vulkan.mali.so。
//
// 顶替原理：Android 的 libvulkan.so 按 ro.hardware.vulkan 后缀 dlopen
// vulkan.<suffix>.so，dlsym "HMI" 拿到 hwvulkan_module_t，再调
// methods->open(module, "vk0", &device) 得到 hwvulkan_device_t，之后**所有**入口
// 都从该结构的三个函数指针走。所以 interposer 只需：
//   1) 自己实现 open()，在里面转手打开基座 venus（vulkan.virtio.so）；
//   2) 把 venus device 的三个函数指针原样抄过来，只把 GetInstanceProcAddr
//      换成自己的包装；
//   3) 包装里命中要改写的名字就返回覆写实现，其余全部转发。
// 基座 venus 是 Mesa Venus over virtio-gpu，宿主 QEMU 已带 venus=on。
//
// 覆写面刻意收窄到 deviceName 一项：真机 vkGetPhysicalDeviceProperties 的
// vendorID/deviceID/apiVersion/driverVersion 尚未采集（真机离线），编造这些值会与
// deviceName 自相矛盾，反而更容易被识别。宁可只对齐已实测的那一项。
//
// 失败姿态：本库只在 ro.hardware.vulkan=mali 时被加载；venus 打不开就 open() 失败，
// loader 退回「无 Vulkan HAL」——即改动前的现状，不会让图形栈崩掉。

#include <dlfcn.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/system_properties.h>

#include <android/log.h>

#include "cdh_gpu_targets.inc"
#include "cdh_hwvulkan_abi.h"

#define CDH_LOG_TAG "cdh-gpu"
#define CDH_VK_BASE_SONAME CDH_ID_VK_BASE_SONAME

#define CDH_SPV_MAGIC 0x07230203u
#define CDH_SPV_OP_CAPABILITY 17u
#define CDH_SPV_OP_TYPE_FLOAT 22u
#define CDH_SPV_OP_CONSTANT 43u
#define CDH_SPV_OP_SPEC_CONSTANT 50u
#define CDH_SPV_OP_COPY_OBJECT 83u
#define CDH_SPV_OP_FCONVERT 115u
#define CDH_SPV_CAP_SHADER 1u
#define CDH_SPV_CAP_FLOAT16 9u

static cdh_hwvulkan_device_t cdh_dev;          // 我们对外暴露的 device
static cdh_hwvulkan_device_t *cdh_base_dev;    // 基座的 device
static int (*cdh_base_close)(cdh_hw_device_t *);
static PFN_vkGetInstanceProcAddr cdh_base_gipa;
static PFN_vkGetDeviceProcAddr cdh_real_gdpa;
static PFN_vkGetPhysicalDeviceProperties cdh_real_gppd;
static PFN_vkGetPhysicalDeviceProperties2 cdh_real_gppd2;
static PFN_vkCreateShaderModule cdh_real_csm;

#if defined(__LP64__)
#define CDH_VK_CFG_PATH "/vendor/lib64/egl/gpu_id.cfg"
#else
#define CDH_VK_CFG_PATH "/vendor/lib/egl/gpu_id.cfg"
#endif

#define CDH_VK_CFG_MAX_ENTRIES 64
#define CDH_VK_CFG_BUF_SIZE 4096

static char cdh_vk_cfg_raw[CDH_VK_CFG_BUF_SIZE];
static const char *cdh_vk_cfg_keys[CDH_VK_CFG_MAX_ENTRIES];
static const char *cdh_vk_cfg_vals[CDH_VK_CFG_MAX_ENTRIES];
static size_t cdh_vk_cfg_count;
static int cdh_vk_cfg_loaded;

static void cdh_vk_load_cfg_once(void) {
    if (cdh_vk_cfg_loaded) {
        return;
    }
    FILE *fp = fopen(CDH_VK_CFG_PATH, "re");
    if (!fp) {
        cdh_vk_cfg_loaded = 1;
        return;
    }
    size_t n = fread(cdh_vk_cfg_raw, 1, sizeof(cdh_vk_cfg_raw) - 1, fp);
    fclose(fp);
    cdh_vk_cfg_raw[n] = '\0';

    size_t cnt = 0;
    char *cur = cdh_vk_cfg_raw;
    while (*cur && cnt < CDH_VK_CFG_MAX_ENTRIES) {
        char *nl = strchr(cur, '\n');
        if (nl) {
            *nl = '\0';
        }
        while (*cur == ' ' || *cur == '\t') {
            cur++;
        }
        if (*cur != '\0' && *cur != '#') {
            char *eq = strchr(cur, '=');
            if (eq) {
                *eq = '\0';
                char *val = eq + 1;
                size_t vlen = strlen(val);
                while (vlen > 0 && (val[vlen - 1] == '\r' || val[vlen - 1] == ' ' || val[vlen - 1] == '\t')) {
                    val[--vlen] = '\0';
                }
                cdh_vk_cfg_keys[cnt] = cur;
                cdh_vk_cfg_vals[cnt] = val;
                cnt++;
            }
        }
        if (!nl) {
            break;
        }
        cur = nl + 1;
    }
    cdh_vk_cfg_count = cnt;
    cdh_vk_cfg_loaded = 1;
}

static int cdh_vk_cfg_get(const char *key, char *out, size_t cap) {
    if (!key || !out || cap == 0) {
        return 0;
    }
    cdh_vk_load_cfg_once();
    for (size_t i = 0; i < cdh_vk_cfg_count; i++) {
        if (strcmp(cdh_vk_cfg_keys[i], key) == 0) {
            if (cdh_vk_cfg_vals[i][0] == '\0') {
                out[0] = '\0';
                return 0;
            }
            strncpy(out, cdh_vk_cfg_vals[i], cap - 1);
            out[cap - 1] = '\0';
            return (int)strlen(out);
        }
    }
    char prop_buf[PROP_VALUE_MAX] = {0};
    int len = __system_property_get(key, prop_buf);
    if (len > 0 && prop_buf[0] != '\0') {
        strncpy(out, prop_buf, cap - 1);
        out[cap - 1] = '\0';
        return (int)strlen(out);
    }
    out[0] = '\0';
    return 0;
}

static int cdh_hex_nibble(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

// 解析 32 个十六进制字符（允许夹杂 '-' 或 ':'）到 16 字节 UUID 数组。
static int cdh_parse_uuid16(const char *s, uint8_t out[VK_UUID_SIZE]) {
    if (!s || !out) {
        return 0;
    }
    size_t idx = 0;
    while (*s && idx < VK_UUID_SIZE) {
        if (*s == '-' || *s == ':' || *s == ' ') {
            s++;
            continue;
        }
        int hi = cdh_hex_nibble(s[0]);
        int lo = s[1] ? cdh_hex_nibble(s[1]) : -1;
        if (hi < 0 || lo < 0) {
            return 0;
        }
        out[idx++] = (uint8_t)((hi << 4) | lo);
        s += 2;
    }
    while (*s == ' ') {
        s++;
    }
    return (idx == VK_UUID_SIZE && *s == '\0') ? 1 : 0;
}

static int cdh_prop_u32(const char *key, uint32_t *out) {
    char buf[PROP_VALUE_MAX] = {0};
    if (cdh_vk_cfg_get(key, buf, sizeof(buf)) <= 0 || buf[0] == '\0') {
        return 0;
    }
    char *end = NULL;
    unsigned long v = strtoul(buf, &end, 0);
    if (end == buf || *end != '\0') {
        return 0;
    }
    *out = (uint32_t)v;
    return 1;
}

static void cdh_resolve_dev_name(char *out, size_t cap) {
    if (!out || cap == 0) {
        return;
    }
    char name_prop[PROP_VALUE_MAX] = {0};
    const char *dev_name = CDH_ID_VK_DEVICE_NAME;
    if ((__system_property_get("ro.kernel.qemu.vulkan.name", name_prop) > 0 ||
         cdh_vk_cfg_get("ro.vendor.cdh.vulkan.name", name_prop, sizeof(name_prop)) > 0) &&
        name_prop[0] != '\0') {
        dev_name = name_prop;
    }
    strncpy(out, dev_name, cap - 1);
    out[cap - 1] = '\0';
}

// 按机型属性覆写 VkPhysicalDeviceProperties（未配置的属性保持基座原值）：
//   - ro.vendor.cdh.vulkan.name           -> deviceName
//   - ro.vendor.cdh.vulkan.vendor_id      -> vendorID（如 ARM 0x13b5）
//   - ro.vendor.cdh.vulkan.device_id      -> deviceID（如 Mali-G57 0x90930010）
//   - ro.vendor.cdh.vulkan.driver_version -> driverVersion
//   - ro.vendor.cdh.vulkan.device_type    -> deviceType（1 = INTEGRATED_GPU）
//   - ro.vendor.cdh.vulkan.api_version    -> apiVersion（仅在不高于基座主次版本时覆写，防越级调用崩溃）
//   - ro.vendor.cdh.vulkan.pipeline_uuid  -> pipelineCacheUUID（32 hex）
static void cdh_stamp(VkPhysicalDeviceProperties *p) {
    if (!p) {
        return;
    }
    cdh_resolve_dev_name(p->deviceName, sizeof(p->deviceName));

    uint32_t v = 0;
    if (cdh_prop_u32("ro.vendor.cdh.vulkan.vendor_id", &v)) {
        p->vendorID = v;
    }
    if (cdh_prop_u32("ro.vendor.cdh.vulkan.device_id", &v)) {
        p->deviceID = v;
    }
    if (cdh_prop_u32("ro.vendor.cdh.vulkan.driver_version", &v)) {
        p->driverVersion = v;
    }
    if (cdh_prop_u32("ro.vendor.cdh.vulkan.device_type", &v) && v <= 4u) {
        p->deviceType = (VkPhysicalDeviceType)v;
    }
    if (cdh_prop_u32("ro.vendor.cdh.vulkan.api_version", &v)) {
        if ((v >> 12) <= (p->apiVersion >> 12)) {
            p->apiVersion = v;
        }
    }
    char uuid_prop[PROP_VALUE_MAX] = {0};
    uint8_t uuid_bytes[VK_UUID_SIZE];
    if (cdh_vk_cfg_get("ro.vendor.cdh.vulkan.pipeline_uuid", uuid_prop, sizeof(uuid_prop)) > 0 &&
        cdh_parse_uuid16(uuid_prop, uuid_bytes)) {
        memcpy(p->pipelineCacheUUID, uuid_bytes, VK_UUID_SIZE);
    }
}

static void cdh_parse_conformance(const char *s, VkConformanceVersion *out) {
    if (!s || !out) {
        return;
    }
    unsigned ma = 0, mi = 0, sub = 0, pa = 0;
    if (sscanf(s, "%u.%u.%u.%u", &ma, &mi, &sub, &pa) == 4) {
        out->major = (uint8_t)ma;
        out->minor = (uint8_t)mi;
        out->subminor = (uint8_t)sub;
        out->patch = (uint8_t)pa;
    }
}

// 同步覆写 vkGetPhysicalDeviceProperties2 的 pNext 扩展结构链：
//   - VkPhysicalDeviceDriverProperties / VkPhysicalDeviceVulkan12Properties（消除 Mesa/Venus 驱动名泄露）
//   - VkPhysicalDeviceIDProperties / VkPhysicalDeviceVulkan11Properties（对齐 deviceUUID / driverUUID）
static void cdh_stamp_pnext(void *pNext) {
    char drv_name[PROP_VALUE_MAX] = {0};
    char drv_info[PROP_VALUE_MAX] = {0};
    char conf_ver[PROP_VALUE_MAX] = {0};
    char dev_uuid_str[PROP_VALUE_MAX] = {0};
    char drv_uuid_str[PROP_VALUE_MAX] = {0};

    if (cdh_vk_cfg_get("ro.vendor.cdh.vulkan.driver_name", drv_name, sizeof(drv_name)) <= 0 ||
        drv_name[0] == '\0') {
        cdh_resolve_dev_name(drv_name, sizeof(drv_name));
    }
    cdh_vk_cfg_get("ro.vendor.cdh.vulkan.driver_info", drv_info, sizeof(drv_info));
    cdh_vk_cfg_get("ro.vendor.cdh.vulkan.conformance", conf_ver, sizeof(conf_ver));
    cdh_vk_cfg_get("ro.vendor.cdh.vulkan.device_uuid", dev_uuid_str, sizeof(dev_uuid_str));
    cdh_vk_cfg_get("ro.vendor.cdh.vulkan.driver_uuid", drv_uuid_str, sizeof(drv_uuid_str));

    uint32_t drv_id = 0;
    int has_drv_id = cdh_prop_u32("ro.vendor.cdh.vulkan.driver_id", &drv_id);

    uint8_t dev_uuid[VK_UUID_SIZE];
    uint8_t drv_uuid[VK_UUID_SIZE];
    int has_dev_uuid = cdh_parse_uuid16(dev_uuid_str, dev_uuid);
    int has_drv_uuid = cdh_parse_uuid16(drv_uuid_str, drv_uuid);

    VkBaseOutStructure *cur = (VkBaseOutStructure *)pNext;
    while (cur) {
        switch ((int)cur->sType) {
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES: {
            VkPhysicalDeviceDriverProperties *dp =
                (VkPhysicalDeviceDriverProperties *)cur;
            if (has_drv_id) {
                dp->driverID = (VkDriverId)drv_id;
            }
            if (drv_name[0] != '\0') {
                strncpy(dp->driverName, drv_name, sizeof(dp->driverName) - 1);
                dp->driverName[sizeof(dp->driverName) - 1] = '\0';
            }
            if (drv_info[0] != '\0') {
                strncpy(dp->driverInfo, drv_info, sizeof(dp->driverInfo) - 1);
                dp->driverInfo[sizeof(dp->driverInfo) - 1] = '\0';
            }
            if (conf_ver[0] != '\0') {
                cdh_parse_conformance(conf_ver, &dp->conformanceVersion);
            }
            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES: {
            VkPhysicalDeviceVulkan12Properties *v12 =
                (VkPhysicalDeviceVulkan12Properties *)cur;
            if (has_drv_id) {
                v12->driverID = (VkDriverId)drv_id;
            }
            if (drv_name[0] != '\0') {
                strncpy(v12->driverName, drv_name, sizeof(v12->driverName) - 1);
                v12->driverName[sizeof(v12->driverName) - 1] = '\0';
            }
            if (drv_info[0] != '\0') {
                strncpy(v12->driverInfo, drv_info, sizeof(v12->driverInfo) - 1);
                v12->driverInfo[sizeof(v12->driverInfo) - 1] = '\0';
            }
            if (conf_ver[0] != '\0') {
                cdh_parse_conformance(conf_ver, &v12->conformanceVersion);
            }
            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES: {
            VkPhysicalDeviceIDProperties *idp =
                (VkPhysicalDeviceIDProperties *)cur;
            if (has_dev_uuid) {
                memcpy(idp->deviceUUID, dev_uuid, VK_UUID_SIZE);
            }
            if (has_drv_uuid) {
                memcpy(idp->driverUUID, drv_uuid, VK_UUID_SIZE);
            }
            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES: {
            VkPhysicalDeviceVulkan11Properties *v11 =
                (VkPhysicalDeviceVulkan11Properties *)cur;
            if (has_dev_uuid) {
                memcpy(v11->deviceUUID, dev_uuid, VK_UUID_SIZE);
            }
            if (has_drv_uuid) {
                memcpy(v11->driverUUID, drv_uuid, VK_UUID_SIZE);
            }
            break;
        }
        default:
            break;
        }
        cur = cur->pNext;
    }
}

static void VKAPI_CALL cdh_gppd(VkPhysicalDevice physicalDevice,
                                VkPhysicalDeviceProperties *pProperties) {
    if (!cdh_real_gppd) {
        return;
    }
    cdh_real_gppd(physicalDevice, pProperties);
    cdh_stamp(pProperties);
}

// VkPhysicalDeviceProperties2 的首个成员是 VkPhysicalDeviceProperties，同时遍历 pNext 链。
static void VKAPI_CALL cdh_gppd2(VkPhysicalDevice physicalDevice,
                                 VkPhysicalDeviceProperties2 *pProperties) {
    if (!cdh_real_gppd2) {
        return;
    }
    cdh_real_gppd2(physicalDevice, pProperties);
    if (pProperties) {
        cdh_stamp(&pProperties->properties);
        cdh_stamp_pnext(pProperties->pNext);
    }
}

// SwiftShader (vulkan.pastel.so) 未实现 SPIR-V Float16 / OpFConvert：
// 遇到 OpFConvert 时只打 WARNING 并跳过 DefineResult，导致后续 Emit 阶段引用该 SSA ID
// 时查 defs 表解引用 NULL 崩溃（HyperOS 4 桌面 MiGlassV13F16 材质着色器即触发此问题）。
// 在 vkCreateShaderModule 入口将纯寄存器级的 Float16 提升为 Float32：
//   - OpCapability Float16 (17, 9) -> OpCapability Shader (17, 1)
//   - OpTypeFloat %id 16 (22, %id, 16) -> OpTypeFloat %id 32
//   - OpConstant / OpSpecConstant (%fp16_type) -> IEEE 754 binary16 转 binary32
//   - OpFConvert (115) -> OpCopyObject (83)
static int cdh_spv_has_fp16(const uint32_t *code, size_t word_count) {
    if (word_count < 5 || code[0] != CDH_SPV_MAGIC) {
        return 0;
    }
    size_t off = 5;
    while (off < word_count) {
        uint32_t w0 = code[off];
        uint32_t wc = w0 >> 16;
        uint32_t op = w0 & 0xffffu;
        if (wc == 0 || off + wc > word_count) {
            break;
        }
        if ((op == CDH_SPV_OP_CAPABILITY && wc == 2 &&
             code[off + 1] == CDH_SPV_CAP_FLOAT16) ||
            (op == CDH_SPV_OP_TYPE_FLOAT && wc == 3 && code[off + 2] == 16u) ||
            (op == CDH_SPV_OP_FCONVERT && wc == 4)) {
            return 1;
        }
        off += wc;
    }
    return 0;
}

static uint32_t cdh_fp16_to_fp32_bits(uint32_t w) {
    uint16_t h_bits = (uint16_t)(w & 0xffffu);
    __fp16 h_val;
    memcpy(&h_val, &h_bits, sizeof(h_val));
    float f_val = (float)h_val;
    uint32_t f_bits = 0;
    memcpy(&f_bits, &f_val, sizeof(f_bits));
    return f_bits;
}

static VkResult VKAPI_CALL cdh_csm(
    VkDevice device, const VkShaderModuleCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkShaderModule *pShaderModule) {
    if (!cdh_real_csm) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if (!pCreateInfo || !pCreateInfo->pCode || pCreateInfo->codeSize < 20 ||
        (pCreateInfo->codeSize & 3u) != 0) {
        return cdh_real_csm(device, pCreateInfo, pAllocator, pShaderModule);
    }
    size_t word_count = pCreateInfo->codeSize >> 2;
    if (!cdh_spv_has_fp16(pCreateInfo->pCode, word_count)) {
        return cdh_real_csm(device, pCreateInfo, pAllocator, pShaderModule);
    }
    uint32_t *patched = (uint32_t *)malloc(pCreateInfo->codeSize);
    if (!patched) {
        return cdh_real_csm(device, pCreateInfo, pAllocator, pShaderModule);
    }
    memcpy(patched, pCreateInfo->pCode, pCreateInfo->codeSize);

    uint32_t fp16_types[16];
    size_t fp16_type_count = 0;
    unsigned fconvert_count = 0;
    unsigned const_count = 0;

    size_t off = 5;
    while (off < word_count) {
        uint32_t w0 = patched[off];
        uint32_t wc = w0 >> 16;
        uint32_t op = w0 & 0xffffu;
        if (wc == 0 || off + wc > word_count) {
            break;
        }
        if (op == CDH_SPV_OP_CAPABILITY && wc == 2 &&
            patched[off + 1] == CDH_SPV_CAP_FLOAT16) {
            patched[off + 1] = CDH_SPV_CAP_SHADER;
        } else if (op == CDH_SPV_OP_TYPE_FLOAT && wc == 3 &&
                   patched[off + 2] == 16u) {
            if (fp16_type_count < sizeof(fp16_types) / sizeof(fp16_types[0])) {
                fp16_types[fp16_type_count++] = patched[off + 1];
            }
            patched[off + 2] = 32u;
        } else if ((op == CDH_SPV_OP_CONSTANT ||
                    op == CDH_SPV_OP_SPEC_CONSTANT) &&
                   wc == 4) {
            uint32_t res_type = patched[off + 1];
            for (size_t i = 0; i < fp16_type_count; i++) {
                if (fp16_types[i] == res_type) {
                    patched[off + 3] = cdh_fp16_to_fp32_bits(patched[off + 3]);
                    const_count++;
                    break;
                }
            }
        } else if (op == CDH_SPV_OP_FCONVERT && wc == 4) {
            patched[off] = (4u << 16) | CDH_SPV_OP_COPY_OBJECT;
            fconvert_count++;
        }
        off += wc;
    }

    VkShaderModuleCreateInfo modified_info = *pCreateInfo;
    modified_info.pCode = patched;
    VkResult res =
        cdh_real_csm(device, &modified_info, pAllocator, pShaderModule);
    free(patched);
    __android_log_print(
        ANDROID_LOG_INFO, CDH_LOG_TAG,
        "vulkan: SPIR-V Float16->Float32 已改写 (size=%zu, fp16_types=%zu, "
        "consts=%u, fconverts=%u) -> %d",
        pCreateInfo->codeSize, fp16_type_count, const_count, fconvert_count,
        (int)res);
    return res;
}

static PFN_vkVoidFunction VKAPI_CALL cdh_gdpa(VkDevice device,
                                              const char *pName) {
    if (!cdh_real_gdpa) {
        return NULL;
    }
    if (pName) {
        if (strcmp(pName, "vkGetDeviceProcAddr") == 0) {
            return (PFN_vkVoidFunction)cdh_gdpa;
        }
        if (strcmp(pName, "vkCreateShaderModule") == 0) {
            PFN_vkCreateShaderModule fn =
                (PFN_vkCreateShaderModule)cdh_real_gdpa(device, pName);
            if (fn) {
                cdh_real_csm = fn;
                return (PFN_vkVoidFunction)cdh_csm;
            }
            return NULL;
        }
    }
    return cdh_real_gdpa(device, pName);
}

static PFN_vkVoidFunction VKAPI_CALL cdh_gipa(VkInstance instance,
                                              const char *pName) {
    if (!cdh_base_gipa) {
        return NULL;
    }
    if (pName) {
        if (strcmp(pName, "vkGetPhysicalDeviceProperties") == 0) {
            cdh_real_gppd = (PFN_vkGetPhysicalDeviceProperties)cdh_base_gipa(
                instance, pName);
            return cdh_real_gppd ? (PFN_vkVoidFunction)cdh_gppd : NULL;
        }
        if (strcmp(pName, "vkGetPhysicalDeviceProperties2") == 0 ||
            strcmp(pName, "vkGetPhysicalDeviceProperties2KHR") == 0) {
            cdh_real_gppd2 = (PFN_vkGetPhysicalDeviceProperties2)cdh_base_gipa(
                instance, pName);
            return cdh_real_gppd2 ? (PFN_vkVoidFunction)cdh_gppd2 : NULL;
        }
        if (strcmp(pName, "vkGetDeviceProcAddr") == 0) {
            cdh_real_gdpa =
                (PFN_vkGetDeviceProcAddr)cdh_base_gipa(instance, pName);
            return cdh_real_gdpa ? (PFN_vkVoidFunction)cdh_gdpa : NULL;
        }
        if (strcmp(pName, "vkCreateShaderModule") == 0) {
            cdh_real_csm =
                (PFN_vkCreateShaderModule)cdh_base_gipa(instance, pName);
            return cdh_real_csm ? (PFN_vkVoidFunction)cdh_csm : NULL;
        }
    }
    return cdh_base_gipa(instance, pName);
}

static int cdh_close(cdh_hw_device_t *device) {
    (void)device;
    if (cdh_base_close && cdh_base_dev) {
        return cdh_base_close(&cdh_base_dev->common);
    }
    return 0;
}

static int cdh_open(const cdh_hw_module_t *module, const char *id,
                    cdh_hw_device_t **device) {
    (void)module;
    if (!id || strcmp(id, CDH_HWVULKAN_DEVICE_0) != 0) {
        return -EINVAL;
    }
    char base_prop[PROP_VALUE_MAX] = {0};
    char path_buf[128];
    char soname_buf[64];
    const char *base_path = CDH_ID_VK_BASE_PATH;
    const char *base_soname = CDH_VK_BASE_SONAME;
    if ((__system_property_get("ro.kernel.qemu.vulkan.base", base_prop) > 0 ||
         cdh_vk_cfg_get("ro.vendor.cdh.vulkan.base", base_prop, sizeof(base_prop)) > 0) &&
        base_prop[0] != '\0') {
        snprintf(path_buf, sizeof(path_buf), "/vendor/lib64/hw/vulkan.%s.so",
                 base_prop);
        snprintf(soname_buf, sizeof(soname_buf), "vulkan.%s.so", base_prop);
        base_path = path_buf;
        base_soname = soname_buf;
    }
    // 直接加载原样未修改的基座驱动（/proc/<pid>/maps 逆向伪装由内核 cdh_redirect 处理）。
    void *h = dlopen(base_path, RTLD_NOW | RTLD_LOCAL);
    if (!h) {
        h = dlopen(base_soname, RTLD_NOW | RTLD_LOCAL);
    }
    if (!h) {
        const char *e = dlerror();
        __android_log_print(ANDROID_LOG_ERROR, CDH_LOG_TAG,
                            "vulkan: dlopen(%s) 失败: %s", base_soname,
                            e ? e : "?");
        return -ENOENT;
    }
    cdh_hwvulkan_module_t *base =
        (cdh_hwvulkan_module_t *)dlsym(h, CDH_HAL_MODULE_INFO_SYM_AS_STR);
    if (!base || !base->common.methods || !base->common.methods->open) {
        __android_log_print(ANDROID_LOG_ERROR, CDH_LOG_TAG,
                            "vulkan: %s 里没有可用的 %s/methods->open",
                            base_soname, CDH_HAL_MODULE_INFO_SYM_AS_STR);
        return -ENOENT;
    }
    cdh_hw_device_t *base_common = NULL;
    int rc = base->common.methods->open(&base->common, id, &base_common);
    if (rc != 0 || !base_common) {
        __android_log_print(ANDROID_LOG_ERROR, CDH_LOG_TAG,
                            "vulkan: 基座 open(%s) 失败 rc=%d", id, rc);
        return rc != 0 ? rc : -EIO;
    }
    cdh_base_dev = (cdh_hwvulkan_device_t *)base_common;
    cdh_base_close = cdh_base_dev->common.close;
    cdh_base_gipa = cdh_base_dev->GetInstanceProcAddr;

    // 抄基座 device 的 tag/version（loader 会校验 version），close 换成自己的，
    // module 指向本模块，三个入口只替换 GetInstanceProcAddr。
    cdh_dev.common = cdh_base_dev->common;
    cdh_dev.common.module = (cdh_hw_module_t *)module;
    cdh_dev.common.close = cdh_close;
    cdh_dev.EnumerateInstanceExtensionProperties =
        cdh_base_dev->EnumerateInstanceExtensionProperties;
    cdh_dev.CreateInstance = cdh_base_dev->CreateInstance;
    cdh_dev.GetInstanceProcAddr = cdh_gipa;

    *device = &cdh_dev.common;
    __android_log_print(ANDROID_LOG_INFO, CDH_LOG_TAG,
                        "vulkan: 已接管，基座 %s", base_soname);
    return 0;
}

static cdh_hw_module_methods_t cdh_methods = {
    .open = cdh_open,
};

// 符号名必须是 HMI（loader 按该名 dlsym）；描述信息去除虚拟化/转发器字样，对齐原厂 HAL。
__attribute__((visibility("default")))
cdh_hwvulkan_module_t HMI = {
    .common =
        {
            .tag = CDH_HARDWARE_MODULE_TAG,
            .module_api_version = CDH_HWVULKAN_MODULE_API_VERSION_0_1,
            .hal_api_version = CDH_HARDWARE_HAL_API_VERSION,
            .id = CDH_HWVULKAN_HARDWARE_MODULE_ID,
            .name = "Android Vulkan HAL",
            .author = "ARM",
            .methods = &cdh_methods,
            .dso = NULL,
            .reserved = {0},
        },
};
