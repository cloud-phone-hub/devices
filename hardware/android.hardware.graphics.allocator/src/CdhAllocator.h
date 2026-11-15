#pragma once

#include <cstring>
#include <dirent.h>
#include <string>
#include <utility>

#include <android-base/properties.h>

#include "cros_gralloc/aidl/Allocator.h"

namespace aidl::android::hardware::graphics::allocator::cdh {

// CdhAllocator 继承上游 Minigbm AIDL Allocator，完整复用缓冲区分配（allocate / allocate2）、
// 格式能力查询（isSupported）与实时调度继承（createBinder -> AIBinder_setInheritRt），
// 仅覆写 getIMapperLibrarySuffix()：优先读取系统属性 ro.hardware.gralloc（若存在），
// 未配置时直接枚举 /vendor/lib64/hw/mapper.<suffix>.so 自动识别当前机型的 IMapper SP-HAL 库后缀
// （如 mediatek / qti / pixel，回退 minigbm），无需在 vendor/build.prop 注入额外属性。
class CdhAllocator : public aidl::android::hardware::graphics::allocator::impl::Allocator {
 public:
  CdhAllocator() = default;

  ndk::ScopedAStatus getIMapperLibrarySuffix(std::string* outResult) override {
    if (outResult == nullptr) {
      return ndk::ScopedAStatus::fromStatus(STATUS_BAD_VALUE);
    }
    static const std::string kCachedSuffix = ResolveMapperSuffix();
    *outResult = kCachedSuffix;
    return ndk::ScopedAStatus::ok();
  }

 private:
  static std::string ResolveMapperSuffix() {
    std::string prop = ::android::base::GetProperty("ro.hardware.gralloc", "");
    if (!prop.empty()) {
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
