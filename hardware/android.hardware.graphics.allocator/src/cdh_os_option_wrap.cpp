// CDH Minigbm 软件渲染属性识别桥接（cdh_os_option_wrap.cpp）：
//
// 背景：
//   Android 16 上游 external/minigbm/cros_gralloc/cros_gralloc_driver.cc 在 lock() 中新增了门控：
//     if (!is_running_with_software_rendering_) {
//       if ((hnd->usage & (GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK)) == 0)
//         return -EINVAL; // "Attempted to lock() a buffer that was not allocated with a BufferUsage::CPU_* usage."
//     }
//   而上游 is_running_with_software_rendering() 仅读取 drv_get_os_option("ro.hardware.vulkan")
//   是否含 "pastel"。当云机启用 Vulkan interposer（ro.hardware.vulkan=mali）并将基座驱动设为
//   SwiftShader（ro.kernel.qemu.vulkan.base=pastel / gpu_id.cfg 的 ro.vendor.cdh.vulkan.base=pastel）时，
//   上游判断为 false，导致 vulkan.pastel.so 在呈现首帧（QueueSignalReleaseImageANDROID）锁定
//   未带 CPU_* 标志的图形缓冲区时被 -EINVAL 拒绝，进而引发 Impeller Vulkan Swapchain 断连黑屏。
//
// 方案：
//   通过链接期 -Wl,--wrap=drv_get_os_option 在不改动上游 external/minigbm 源码的前提下，
//   当查询 "ro.hardware.vulkan" 且基座配置为 pastel 时返回 "pastel"，使 allocator 与 mapper
//   准确识别 SwiftShader 软件光栅模式。

#include <stdio.h>
#include <string.h>

#include <cutils/properties.h>

// Android 13/14 的 external/minigbm 尚未引入 drv_get_os_option 与 !is_running_with_software_rendering_ 门控；
// 仅在 Android 15+ 编译该链接期 --wrap 符号，避免低版本 mapper.minigbm.so 残留未定义 __real_drv_get_os_option。
#if !defined(CDH_ANDROID_VERSION) || CDH_ANDROID_VERSION >= 15

#if defined(__LP64__)
#define CDH_GPU_ID_CFG_PATH "/vendor/lib64/egl/gpu_id.cfg"
#else
#define CDH_GPU_ID_CFG_PATH "/vendor/lib/egl/gpu_id.cfg"
#endif

extern "C" const char *__real_drv_get_os_option(const char *name);

static bool cdh_cfg_has_pastel_base() {
  FILE *fp = fopen(CDH_GPU_ID_CFG_PATH, "re");
  if (fp == nullptr) {
    return false;
  }
  char line[256];
  bool found = false;
  while (fgets(line, sizeof(line), fp) != nullptr) {
    char *p = line;
    while (*p == ' ' || *p == '\t') {
      ++p;
    }
    if (strncmp(p, "ro.vendor.cdh.vulkan.base=", 26) == 0) {
      if (strstr(p + 26, "pastel") != nullptr) {
        found = true;
      }
      break;
    }
  }
  fclose(fp);
  return found;
}

extern "C" const char *__wrap_drv_get_os_option(const char *name) {
  const char *val = __real_drv_get_os_option(name);
  if (name != nullptr && strcmp(name, "ro.hardware.vulkan") == 0) {
    if (val != nullptr && strstr(val, "pastel") != nullptr) {
      return val;
    }
    char base[PROPERTY_VALUE_MAX] = {0};
    if (property_get("ro.kernel.qemu.vulkan.base", base, "") > 0 &&
        strstr(base, "pastel") != nullptr) {
      return "pastel";
    }
    if (property_get("ro.vendor.cdh.vulkan.base", base, "") > 0 &&
        strstr(base, "pastel") != nullptr) {
      return "pastel";
    }
    if (cdh_cfg_has_pastel_base()) {
      return "pastel";
    }
  }
  return val;
}

#endif  // !defined(CDH_ANDROID_VERSION) || CDH_ANDROID_VERSION >= 15
