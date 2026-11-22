#pragma once

#include <cstddef>
#include <cstring>
#include <dirent.h>
#include <string>
#include <unistd.h>
#include <utility>

#include <android-base/properties.h>

#include "cros_gralloc/aidl/Allocator.h"
#include "cros_gralloc/cros_gralloc_handle.h"
#include "cros_gralloc/cros_gralloc_helpers.h"

namespace aidl::android::hardware::graphics::allocator::cdh {

// CdhAllocator 继承上游 Minigbm AIDL Allocator，完整复用缓冲区分配（allocate / allocate2）、
// 格式能力查询（isSupported）与实时调度继承（createBinder -> AIBinder_setInheritRt），
// 仅覆写 getIMapperLibrarySuffix()：优先读取系统属性 ro.hardware.gralloc（若存在），
// 未配置时直接枚举 /vendor/lib64/hw/mapper.<suffix>.so 自动识别当前机型的 IMapper SP-HAL 库后缀
// （如 mediatek / qti / pixel，回退 minigbm），无需在 vendor/build.prop 注入额外属性。
class CdhAllocator : public aidl::android::hardware::graphics::allocator::impl::Allocator {
 public:
  CdhAllocator() = default;

  ndk::ScopedAStatus allocate2(
      const ::aidl::android::hardware::graphics::allocator::BufferDescriptorInfo& descriptor,
      int32_t count,
      ::aidl::android::hardware::graphics::allocator::AllocationResult* outResult) override {
    auto status =
        aidl::android::hardware::graphics::allocator::impl::Allocator::allocate2(
            descriptor, count, outResult);
    if (status.isOk()) {
      return status;
    }
    // VirGL minigbm 对带硬件直通位（VIDEO_ENCODER / CAMERA_* / GPU_DATA_BUFFER 等）
    // 且叠加 CPU_WRITE_OFTEN 的 YCbCr_420_888 组合会拒绝分配（如 GoogleCamera YUV 流
    // usage=0x10133）；按阶梯剥离非必需直通位重试，保证 Camera/Media 缓冲正常分配。
    const uint64_t origUsage = static_cast<uint64_t>(descriptor.usage);
    static constexpr uint64_t kStripPass1 =
        (1ULL << 16) |  // VIDEO_ENCODER
        (1ULL << 17) |  // CAMERA_OUTPUT
        (1ULL << 18) |  // CAMERA_INPUT
        (1ULL << 22) |  // VIDEO_DECODER
        (1ULL << 23) |  // SENSOR_DIRECT_DATA
        (1ULL << 24);   // GPU_DATA_BUFFER
    if ((origUsage & kStripPass1) != 0) {
      auto relaxed = descriptor;
      uint64_t u1 = origUsage & ~kStripPass1;
      if (u1 == 0) {
        u1 = 0x33ULL;  // CPU_READ_OFTEN | CPU_WRITE_OFTEN
      }
      relaxed.usage =
          static_cast<::aidl::android::hardware::graphics::common::BufferUsage>(u1);
      auto st1 =
          aidl::android::hardware::graphics::allocator::impl::Allocator::allocate2(
              relaxed, count, outResult);
      if (st1.isOk()) {
        return st1;
      }
    }
    // 第二级兜底：部分格式附带 GPU_TEXTURE(0x100) 等非 CPU 标志时 VirGL minigbm 无对应组合，
    // 降级为纯 CPU 读写（0x33）重试。
    if ((origUsage & ~0x33ULL) != 0) {
      auto cpuOnly = descriptor;
      cpuOnly.usage =
          static_cast<::aidl::android::hardware::graphics::common::BufferUsage>(
              0x33ULL);
      auto st2 =
          aidl::android::hardware::graphics::allocator::impl::Allocator::allocate2(
              cpuOnly, count, outResult);
      if (st2.isOk()) {
        return st2;
      }
    }
    // 第三级兜底：minigbm cros_gralloc_convert_format 仅映射了 RAW16(32)，
    // 未映射 RAW10(37) / RAW_OPAQUE(36) / RAW12(38)。用 RAW16(DRM_FORMAT_R16，每像素 2B >= RAW10 1.25B)
    // 分配底层 BO，并将返回 cros_gralloc_handle 中的 droid_format 回写为调用方原始格式，
    // 保证跨进程 GraphicBufferMapper::validateBufferSize / PIXEL_FORMAT_REQUESTED 校验一致。
    const int32_t fmt = static_cast<int32_t>(descriptor.format);
    if (fmt == 37 || fmt == 36 || fmt == 38) {
      auto asRaw16 = descriptor;
      asRaw16.format =
          static_cast<::aidl::android::hardware::graphics::common::PixelFormat>(32);
      asRaw16.usage =
          static_cast<::aidl::android::hardware::graphics::common::BufferUsage>(
              0x33ULL);
      auto st3 =
          aidl::android::hardware::graphics::allocator::impl::Allocator::allocate2(
              asRaw16, count, outResult);
      if (st3.isOk() && outResult != nullptr) {
        constexpr size_t kStrides0DataIdx =
            (offsetof(struct cros_gralloc_handle, strides[0]) -
             offsetof(struct cros_gralloc_handle, fds[0])) /
            sizeof(int32_t);
        constexpr size_t kMagicDataIdx =
            (offsetof(struct cros_gralloc_handle, magic) -
             offsetof(struct cros_gralloc_handle, fds[0])) /
            sizeof(int32_t);
        constexpr size_t kPixelStrideDataIdx =
            (offsetof(struct cros_gralloc_handle, pixel_stride) -
             offsetof(struct cros_gralloc_handle, fds[0])) /
            sizeof(int32_t);
        constexpr size_t kDroidFmtDataIdx =
            (offsetof(struct cros_gralloc_handle, droid_format) -
             offsetof(struct cros_gralloc_handle, fds[0])) /
            sizeof(int32_t);
        int32_t byteStrideOut = 0;
        for (auto& nh : outResult->buffers) {
          const size_t nFds = nh.fds.size();
          if (nFds <= kStrides0DataIdx && nFds <= kMagicDataIdx &&
              nFds <= kPixelStrideDataIdx && nFds <= kDroidFmtDataIdx) {
            const size_t strides0Idx = kStrides0DataIdx - nFds;
            const size_t magicIdx = kMagicDataIdx - nFds;
            const size_t pixelStrideIdx = kPixelStrideDataIdx - nFds;
            const size_t fmtIdx = kDroidFmtDataIdx - nFds;
            if (strides0Idx < nh.ints.size() && magicIdx < nh.ints.size() &&
                pixelStrideIdx < nh.ints.size() && fmtIdx < nh.ints.size() &&
                static_cast<uint32_t>(nh.ints[magicIdx]) == cros_gralloc_magic &&
                nh.ints[fmtIdx] == 32) {
              nh.ints[fmtIdx] = fmt;
              // Android ImageReader 对 RAW10/RAW12/RAW_OPAQUE 要求 stride 单位为字节
              // （如 RAW10 要求 stride >= width * 10 / 8 且为 16 的倍数）；底层 R16 BO 的
              // strides[0] 为 >= width*2 字节（64B 对齐），与 pixel_stride/outResult->stride
              // 同步后既通过 validateBufferSize 又满足 ImageReader 字节步长断言。
              const int32_t byteStride = nh.ints[strides0Idx];
              if (byteStride > 0) {
                nh.ints[pixelStrideIdx] = byteStride;
                byteStrideOut = byteStride;
              }
            }
          }
        }
        if (byteStrideOut > 0) {
          outResult->stride = byteStrideOut;
        }
        return st3;
      }
    }
    return status;
  }

  ndk::ScopedAStatus isSupported(
      const ::aidl::android::hardware::graphics::allocator::BufferDescriptorInfo& descriptor,
      bool* outResult) override {
    auto status =
        aidl::android::hardware::graphics::allocator::impl::Allocator::isSupported(
            descriptor, outResult);
    if (status.isOk() && outResult != nullptr && *outResult) {
      return status;
    }
    const uint64_t origUsage = static_cast<uint64_t>(descriptor.usage);
    static constexpr uint64_t kStripPass1 =
        (1ULL << 16) | (1ULL << 17) | (1ULL << 18) | (1ULL << 22) | (1ULL << 23) | (1ULL << 24);
    if ((origUsage & kStripPass1) != 0) {
      auto relaxed = descriptor;
      uint64_t u1 = origUsage & ~kStripPass1;
      relaxed.usage =
          static_cast<::aidl::android::hardware::graphics::common::BufferUsage>(
              u1 != 0 ? u1 : 0x33ULL);
      auto st1 = aidl::android::hardware::graphics::allocator::impl::Allocator::isSupported(
          relaxed, outResult);
      if (st1.isOk() && outResult != nullptr && *outResult) {
        return st1;
      }
    }
    if ((origUsage & ~0x33ULL) != 0) {
      auto cpuOnly = descriptor;
      cpuOnly.usage =
          static_cast<::aidl::android::hardware::graphics::common::BufferUsage>(
              0x33ULL);
      auto st2 = aidl::android::hardware::graphics::allocator::impl::Allocator::isSupported(
          cpuOnly, outResult);
      if (st2.isOk() && outResult != nullptr && *outResult) {
        return st2;
      }
    }
    const int32_t fmt = static_cast<int32_t>(descriptor.format);
    if (fmt == 37 || fmt == 36 || fmt == 38) {
      auto asRaw16 = descriptor;
      asRaw16.format =
          static_cast<::aidl::android::hardware::graphics::common::PixelFormat>(32);
      asRaw16.usage =
          static_cast<::aidl::android::hardware::graphics::common::BufferUsage>(
              0x33ULL);
      return aidl::android::hardware::graphics::allocator::impl::Allocator::isSupported(
          asRaw16, outResult);
    }
    return status;
  }

#if !defined(ANDROID_API_LEVEL) || ANDROID_API_LEVEL >= 34
  ndk::ScopedAStatus getIMapperLibrarySuffix(std::string* outResult) override {
    if (outResult == nullptr) {
      return ndk::ScopedAStatus::fromStatus(STATUS_BAD_VALUE);
    }
    static const std::string kCachedSuffix = ResolveMapperSuffix();
    *outResult = kCachedSuffix;
    return ndk::ScopedAStatus::ok();
  }
#endif

 private:
  static bool MapperLibExists(const std::string& suffix) {
    if (suffix.empty()) {
      return false;
    }
    const char* const kHwDirs[] = {"/vendor/lib64/hw", "/system/vendor/lib64/hw"};
    for (const char* dir_path : kHwDirs) {
      std::string full = std::string(dir_path) + "/mapper." + suffix + ".so";
      if (access(full.c_str(), F_OK) == 0) {
        return true;
      }
    }
    return false;
  }

  static std::string ResolveMapperSuffix() {
    std::string prop = ::android::base::GetProperty("ro.hardware.gralloc", "");
    if (!prop.empty() && MapperLibExists(prop)) {
      return prop;
    }
    const char* const kHwDirs[] = {"/vendor/lib64/hw", "/system/vendor/lib64/hw"};
    for (const char* dir_path : kHwDirs) {
      DIR* dir = opendir(dir_path);
      if (dir == nullptr) {
        continue;
      }
      std::string found;
      struct dirent* ent = nullptr;
      while ((ent = readdir(dir)) != nullptr) {
        const char* name = ent->d_name;
        if (std::strncmp(name, "mapper.", 7) != 0) {
          continue;
        }
        const std::size_t len = std::strlen(name);
        if (len <= 10 || std::strcmp(name + len - 3, ".so") != 0) {
          continue;
        }
        std::string cand(name + 7, len - 10);
        if (!cand.empty() && cand != "default" && cand.rfind("minigbm", 0) != 0) {
          found = std::move(cand);
          break;
        }
      }
      closedir(dir);
      if (!found.empty()) {
        return found;
      }
    }
    return "minigbm";
  }
};

}  // namespace aidl::android::hardware::graphics::allocator::cdh
