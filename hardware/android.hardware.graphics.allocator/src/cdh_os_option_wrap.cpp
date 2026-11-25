// CDH Minigbm 链接期桥接（cdh_os_option_wrap.cpp）：
//
// 1. SwiftShader 软件光栅识别（drv_get_os_option，Android 14+）：
//    Android 16 上游 external/minigbm/cros_gralloc/cros_gralloc_driver.cc 在 lock() 中新增了门控：
//      if (!is_running_with_software_rendering_) {
//        if ((hnd->usage & (GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK)) == 0)
//          return -EINVAL;
//      }
//    上游 is_running_with_software_rendering() 仅检查 drv_get_os_option("ro.hardware.vulkan")
//    是否含 "pastel"。当云机启用 Vulkan interposer（ro.hardware.vulkan=mali）且基座为 SwiftShader
//    （ro.kernel.qemu.vulkan.base=pastel / gpu_id.cfg 的 ro.vendor.cdh.vulkan.base=pastel）时，
//    通过 -Wl,--wrap=drv_get_os_option 返回 "pastel"，避免队列呈现阶段 lock() 被 -EINVAL 拒绝。
//
// 2. HAL_PIXEL_FORMAT_YV12 平面步幅/偏移契约修正与宿主 VirGL 显式布局同步（drv_bo_create*，Android 13+）：
//    a) 上游 minigbm virtgpu_virgl.c:virgl_get_emulated_metadata() 将 DRM_FORMAT_YVU420_ANDROID
//       的 Cb/Cr 平面步幅与偏移均按全宽 metadata->width（ALIGN(width, 32)）计算：
//         strides = {W32, W32, W32}, offsets = {0, W32*H, W32*H + W32*(H/2)}
//       这违反了 Android <system/graphics.h> 对 HAL_PIXEL_FORMAT_YV12 的硬性约定：
//         yStride = ALIGN(width, 16), cStride = ALIGN(yStride / 2, 16)
//         V(Cr) 偏移 = yStride * height，U(Cb) 偏移 = yStride * height + cStride * (height / 2)
//    b) 同时，virtgpu_virgl 在非 host-gbm 宿主（如 macOS CGL）上走 DRM_IOCTL_VIRTGPU_RESOURCE_CREATE
//       创建宽 ALIGN(width, 32)、高 2*height 的 R8_UNORM 打包纹理（blob_mem=0），未修改的原厂/Lineage
//       Mesa libgallium_dri.so 导入此类非 blob 资源时不会主动发送 VIRGL_CCMD_PIPE_RESOURCE_SET_TYPE，
//       导致宿主 virglrenderer 无法获知真实可见宽度（如 1200 vs 1216）与半宽色度步幅（608 vs 1216），
//       出现全屏偏绿、y=H/2 水平断层与右侧 16px 对齐填充边。
//    c) 本桥接在分配完成后既修正 Guest bo->meta 的 Y/V/U 步幅与偏移，又立即通过 virtio-gpu
//       EXECBUFFER 向宿主发送 VIRGL_CCMD_PIPE_RESOURCE_SET_TYPE（按着色器采样序 Y, U, V 登记真实宽高与步幅），
//       对预编译 Mesa 库保持零修改。

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <xf86drm.h>

#include <cutils/properties.h>

extern "C" {
#include "drv_priv.h"
#include "external/virgl_hw.h"
#include "external/virtgpu_drm.h"
}

#define CDH_VIRGL_CCMD_PIPE_RESOURCE_SET_TYPE 49U
#define CDH_VIRGL_PIPE_RES_SET_TYPE_SIZE_3 14U
#define CDH_VIRGL_CMD0(cmd, obj, len) \
  ((uint32_t)(cmd) | ((uint32_t)(obj) << 8) | ((uint32_t)(len) << 16))

static void cdh_notify_virgl_yv12_layout(struct bo *bo, uint32_t y_stride,
                                         uint32_t c_stride, uint32_t u_offset,
                                         uint32_t v_offset) {
  if (bo == nullptr) {
    return;
  }
  const uint32_t bo_handle = drv_bo_get_plane_handle(bo, 0).u32;
  if (bo->is_test_buffer || bo_handle == 0 ||
      bo->drv == nullptr || bo->drv->fd < 0 || bo->drv->backend == nullptr ||
      bo->drv->backend->name == nullptr ||
      strcmp(bo->drv->backend->name, "virtgpu_virgl") != 0) {
    return;
  }

  struct drm_virtgpu_resource_info res_info = {};
  res_info.bo_handle = bo_handle;
  if (drmIoctl(bo->drv->fd, DRM_IOCTL_VIRTGPU_RESOURCE_INFO, &res_info) != 0 ||
      res_info.res_handle == 0) {
    return;
  }

  // 按 DRM_FORMAT_YVU420_ANDROID 物理与着色器采样槽位顺序（plane 0=Y, plane 1=V/Cr, plane 2=U/Cb）上报。
  uint32_t cmd[CDH_VIRGL_PIPE_RES_SET_TYPE_SIZE_3 + 1] = {
      CDH_VIRGL_CMD0(CDH_VIRGL_CCMD_PIPE_RESOURCE_SET_TYPE, 0,
                     CDH_VIRGL_PIPE_RES_SET_TYPE_SIZE_3),
      res_info.res_handle,
      VIRGL_FORMAT_R8_UNORM,
      0U,
      bo->meta.width,
      bo->meta.height,
      0U,
      0U,
      0U,
      y_stride,
      0U,
      c_stride,
      v_offset,
      c_stride,
      u_offset,
  };

  struct drm_virtgpu_execbuffer exec = {};
  exec.size = sizeof(cmd);
  exec.command = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(cmd));
  exec.bo_handles =
      static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&bo_handle));
  exec.num_bo_handles = 1;
  exec.fence_fd = -1;
  (void)drmIoctl(bo->drv->fd, DRM_IOCTL_VIRTGPU_EXECBUFFER, &exec);
}

static void cdh_fixup_yv12_bo_layout(struct bo *bo) {
  if (bo == nullptr || bo->meta.format != DRM_FORMAT_YVU420_ANDROID ||
      bo->meta.num_planes != 3) {
    return;
  }
  const uint32_t y_stride = bo->meta.strides[0];
  if (y_stride < 16U || bo->meta.strides[1] != y_stride) {
    return;
  }
  const uint32_t c_stride = ((y_stride / 2U) + 15U) & ~15U;
  const uint32_t y_rows = bo->meta.height;
  const uint32_t c_rows = (y_rows + 1U) / 2U;
  const uint64_t y_size = static_cast<uint64_t>(y_stride) * y_rows;
  const uint64_t c_size = static_cast<uint64_t>(c_stride) * c_rows;
  if (y_size == 0 || c_size == 0 ||
      y_size + 2ULL * c_size > bo->meta.total_size ||
      y_size + 2ULL * c_size > UINT32_MAX) {
    return;
  }

  const uint32_t v_offset = static_cast<uint32_t>(y_size);
  const uint32_t u_offset = static_cast<uint32_t>(y_size + c_size);

  bo->meta.strides[0] = y_stride;
  bo->meta.offsets[0] = 0;
  bo->meta.sizes[0] = static_cast<uint32_t>(y_size);

  bo->meta.strides[1] = c_stride;
  bo->meta.offsets[1] = v_offset;
  bo->meta.sizes[1] = static_cast<uint32_t>(c_size);

  bo->meta.strides[2] = c_stride;
  bo->meta.offsets[2] = u_offset;
  bo->meta.sizes[2] = static_cast<uint32_t>(c_size);

  cdh_notify_virgl_yv12_layout(bo, y_stride, c_stride, u_offset, v_offset);
}

extern "C" struct bo *__real_drv_bo_create(struct driver *drv, uint32_t width,
                                           uint32_t height, uint32_t format,
                                           uint64_t use_flags);

extern "C" struct bo *__wrap_drv_bo_create(struct driver *drv, uint32_t width,
                                           uint32_t height, uint32_t format,
                                           uint64_t use_flags) {
  struct bo *bo = __real_drv_bo_create(drv, width, height, format, use_flags);
  cdh_fixup_yv12_bo_layout(bo);
  return bo;
}

extern "C" struct bo *__real_drv_bo_create_with_modifiers(
    struct driver *drv, uint32_t width, uint32_t height, uint32_t format,
    const uint64_t *modifiers, uint32_t count);

extern "C" struct bo *__wrap_drv_bo_create_with_modifiers(
    struct driver *drv, uint32_t width, uint32_t height, uint32_t format,
    const uint64_t *modifiers, uint32_t count) {
  struct bo *bo = __real_drv_bo_create_with_modifiers(
      drv, width, height, format, modifiers, count);
  cdh_fixup_yv12_bo_layout(bo);
  return bo;
}

// Android 13 的 external/minigbm 尚未引入 drv_get_os_option 与 !is_running_with_software_rendering_ 门控；
// 仅在 Android 14+ 编译该链接期 --wrap 符号，避免 Android 13 mapper.minigbm.so 残留未定义 __real_drv_get_os_option。
#if !defined(CDH_ANDROID_VERSION) || CDH_ANDROID_VERSION >= 14

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

#endif  // !defined(CDH_ANDROID_VERSION) || CDH_ANDROID_VERSION >= 14
