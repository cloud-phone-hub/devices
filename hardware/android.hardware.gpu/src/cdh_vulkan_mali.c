// CDH Vulkan HAL 核心转发与身份覆写底座（产物 lib64/hw/vulkan.mali.so）：
//
// 职责边界：
//   - 本文件仅负责通用 hwvulkan HMI 设备装载与基于 gpu_id.cfg 的硬件身份覆写
//     （deviceName / vendorID / deviceID / driverVersion / deviceType / apiVersion /
//      pipelineCacheUUID / DriverProperties / deviceUUID / driverUUID）；
//   - default 变体（gold / oriole / cph2797 等 Venus 机型）仅编译本文件，零软光栅补丁开销；
//   - pastel 变体（popsicle 等 SwiftShader 软光栅机型）通过 cdh_vulkan_ext.h 编译期挂接
//     src/pastel/cdh_vulkan_pastel.c（SPIR-V Float16->Float32 与 1x MSAA 收敛）。

#include <dlfcn.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <android/log.h>

#include "cdh_gpu_cfg.h"
#include "cdh_gpu_targets.inc"
#include "cdh_hwvulkan_abi.h"
#include "cdh_vulkan_ext.h"

#define CDH_LOG_TAG "cdh-gpu"
#define CDH_VK_BASE_SONAME CDH_ID_VK_BASE_SONAME

static cdh_hwvulkan_device_t cdh_dev;          // 对外暴露的 device
static cdh_hwvulkan_device_t *cdh_base_dev;    // 基座驱动的 device
static int (*cdh_base_close)(cdh_hw_device_t *);
static PFN_vkGetInstanceProcAddr cdh_base_gipa;
static PFN_vkGetDeviceProcAddr cdh_real_gdpa;
static PFN_vkGetPhysicalDeviceProperties cdh_real_gppd;
static PFN_vkGetPhysicalDeviceProperties2 cdh_real_gppd2;

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

static void cdh_resolve_dev_name(char *out, size_t cap) {
    if (!out || cap == 0) {
        return;
    }
    char name_prop[PROP_VALUE_MAX] = {0};
    const char *dev_name = CDH_ID_VK_DEVICE_NAME;
    if ((cdh_sys_prop_get("ro.kernel.qemu.vulkan.name", name_prop) > 0 ||
         cdh_cfg_get("ro.vendor.cdh.vulkan.name", name_prop, sizeof(name_prop)) > 0) &&
        name_prop[0] != '\0') {
        dev_name = name_prop;
    }
    strncpy(out, dev_name, cap - 1);
    out[cap - 1] = '\0';
}

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
    if (cdh_cfg_get("ro.vendor.cdh.vulkan.pipeline_uuid", uuid_prop,
                    sizeof(uuid_prop)) > 0 &&
        cdh_parse_uuid16(uuid_prop, uuid_bytes)) {
        memcpy(p->pipelineCacheUUID, uuid_bytes, VK_UUID_SIZE);
    }

    cdh_vk_pastel_stamp_limits(p);
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

static void cdh_stamp_pnext(void *pNext) {
    char drv_name[PROP_VALUE_MAX] = {0};
    char drv_info[PROP_VALUE_MAX] = {0};
    char conf_ver[PROP_VALUE_MAX] = {0};
    char dev_uuid_str[PROP_VALUE_MAX] = {0};
    char drv_uuid_str[PROP_VALUE_MAX] = {0};

    if (cdh_cfg_get("ro.vendor.cdh.vulkan.driver_name", drv_name,
                    sizeof(drv_name)) <= 0 ||
        drv_name[0] == '\0') {
        cdh_resolve_dev_name(drv_name, sizeof(drv_name));
    }
    cdh_cfg_get("ro.vendor.cdh.vulkan.driver_info", drv_info, sizeof(drv_info));
    cdh_cfg_get("ro.vendor.cdh.vulkan.conformance", conf_ver, sizeof(conf_ver));
    cdh_cfg_get("ro.vendor.cdh.vulkan.device_uuid", dev_uuid_str,
                sizeof(dev_uuid_str));
    cdh_cfg_get("ro.vendor.cdh.vulkan.driver_uuid", drv_uuid_str,
                sizeof(drv_uuid_str));

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
            cdh_vk_pastel_stamp_v12(v12);
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

static PFN_vkVoidFunction VKAPI_CALL cdh_gdpa(VkDevice device,
                                              const char *pName) {
    if (!cdh_real_gdpa) {
        return NULL;
    }
    if (pName) {
        if (strcmp(pName, "vkGetDeviceProcAddr") == 0) {
            return (PFN_vkVoidFunction)cdh_gdpa;
        }
        PFN_vkVoidFunction ext_fn =
            cdh_vk_pastel_intercept_gdpa(device, pName, cdh_real_gdpa);
        if (ext_fn) {
            return ext_fn;
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
        PFN_vkVoidFunction ext_fn =
            cdh_vk_pastel_intercept_gipa(instance, pName, cdh_base_gipa);
        if (ext_fn) {
            return ext_fn;
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
    if ((cdh_sys_prop_get("ro.kernel.qemu.vulkan.base", base_prop) > 0 ||
         cdh_cfg_get("ro.vendor.cdh.vulkan.base", base_prop, sizeof(base_prop)) > 0) &&
        base_prop[0] != '\0') {
        snprintf(path_buf, sizeof(path_buf), "/vendor/lib64/hw/vulkan.%s.so",
                 base_prop);
        snprintf(soname_buf, sizeof(soname_buf), "vulkan.%s.so", base_prop);
        base_path = path_buf;
        base_soname = soname_buf;
    }
    cdh_vk_pastel_on_open(base_soname);

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
