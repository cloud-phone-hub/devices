// Vulkan interposer 变体扩展钩子（cdh_vulkan_ext.h）：
//
// - default 变体（gold / oriole / cph2797 等常规 Venus 机型）：未定义 CDH_VK_PASTEL_COMPAT，
//   全部钩子编译期展开为空内联函数，不引入任何 SPIR-V 改写或 MSAA 收敛代码；
// - pastel 变体（popsicle 等 SwiftShader Vulkan 机型）：定义 CDH_VK_PASTEL_COMPAT=1，
//   链入 src/pastel/cdh_vulkan_pastel.c 实现软光栅兼容层。
#ifndef CDH_VULKAN_EXT_H
#define CDH_VULKAN_EXT_H

#include "cdh_hwvulkan_abi.h"

#ifdef CDH_VK_PASTEL_COMPAT

void cdh_vk_pastel_on_open(const char *base_soname);
void cdh_vk_pastel_stamp_limits(VkPhysicalDeviceProperties *p);
void cdh_vk_pastel_stamp_v12(VkPhysicalDeviceVulkan12Properties *v12);
PFN_vkVoidFunction cdh_vk_pastel_intercept_gipa(VkInstance instance,
                                                const char *pName,
                                                PFN_vkGetInstanceProcAddr base_gipa);
PFN_vkVoidFunction cdh_vk_pastel_intercept_gdpa(VkDevice device,
                                                const char *pName,
                                                PFN_vkGetDeviceProcAddr real_gdpa);

#else

static inline void cdh_vk_pastel_on_open(const char *base_soname) {
    (void)base_soname;
}

static inline void cdh_vk_pastel_stamp_limits(VkPhysicalDeviceProperties *p) {
    (void)p;
}

static inline void cdh_vk_pastel_stamp_v12(VkPhysicalDeviceVulkan12Properties *v12) {
    (void)v12;
}

static inline PFN_vkVoidFunction cdh_vk_pastel_intercept_gipa(
    VkInstance instance, const char *pName, PFN_vkGetInstanceProcAddr base_gipa) {
    (void)instance;
    (void)pName;
    (void)base_gipa;
    return NULL;
}

static inline PFN_vkVoidFunction cdh_vk_pastel_intercept_gdpa(
    VkDevice device, const char *pName, PFN_vkGetDeviceProcAddr real_gdpa) {
    (void)device;
    (void)pName;
    (void)real_gdpa;
    return NULL;
}

#endif  // CDH_VK_PASTEL_COMPAT

#endif  // CDH_VULKAN_EXT_H
