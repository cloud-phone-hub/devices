// engine/vendor/hardware 线 —— android.hardware.drm（IDrmFactory v1）自研服务实现。
//
// 注册 android.hardware.drm.IDrmFactory/clearkey。DRM HAL 按 UUID 派发具体 DRM/crypto
// 插件（widevine / clearkey 等）。本桩作为 minimal-unsupported 工厂落地：不声明任何受支持
// 的 crypto scheme，任何 UUID 的 createDrmPlugin / createCryptoPlugin 一律回空句柄（nullptr），
// 让 framework 判定该 scheme 不受支持——不内置真实解密逻辑，也不硬编解码器后端。
// 设备无关，不内置机型常量。

#include <android/binder_ibinder.h>
#include <android/binder_status.h>

extern "C" {
void ABinderProcess_setThreadPoolMaxThreadCount(uint32_t numThreads);
void ABinderProcess_joinThreadPool(void);
binder_status_t AServiceManager_addService(AIBinder* binder,
                                           const char* instance);
}

#include <aidl/android/hardware/drm/BnDrmFactory.h>
#include <aidl/android/hardware/drm/CryptoSchemes.h>
#include <aidl/android/hardware/drm/ICryptoPlugin.h>
#include <aidl/android/hardware/drm/IDrmPlugin.h>
#include <aidl/android/hardware/drm/Uuid.h>

#include <memory>
#include <string>
#include <vector>

using aidl::android::hardware::drm::BnDrmFactory;
using aidl::android::hardware::drm::CryptoSchemes;
using aidl::android::hardware::drm::ICryptoPlugin;
using aidl::android::hardware::drm::IDrmPlugin;
using aidl::android::hardware::drm::Uuid;
using ndk::ScopedAStatus;

static constexpr const char* kDrmFactoryName =
    "android.hardware.drm.IDrmFactory/clearkey";

namespace {

class DrmFactoryService : public BnDrmFactory {
 public:
  ScopedAStatus createDrmPlugin(
      const Uuid& /*in_uuid*/, const std::string& /*in_appPackageName*/,
      std::shared_ptr<IDrmPlugin>* _aidl_return) override {
    *_aidl_return = nullptr;  // 不支持任何 DRM scheme
    return ScopedAStatus::ok();
  }

  ScopedAStatus createCryptoPlugin(
      const Uuid& /*in_uuid*/, const std::vector<uint8_t>& /*in_initData*/,
      std::shared_ptr<ICryptoPlugin>* _aidl_return) override {
    *_aidl_return = nullptr;
    return ScopedAStatus::ok();
  }

  ScopedAStatus getSupportedCryptoSchemes(
      CryptoSchemes* _aidl_return) override {
    if (_aidl_return != nullptr) {
      // 标准 W3C Common PSSH Box (1077efec-c0b2-4d02-ace3-3c1e52e2fb4b) 与
      // DASH-IF ClearKey (e2719d58-a985-b3c9-781a-b030af78d30e) 公开方案标识，
      // 与 AOSP android.hardware.drm-service.clearkey 返回口径一致。
      Uuid commonPssh{};
      commonPssh.uuid = {0x10, 0x77, 0xef, 0xec, 0xc0, 0xb2, 0x4d, 0x02,
                         0xac, 0xe3, 0x3c, 0x1e, 0x52, 0xe2, 0xfb, 0x4b};
      Uuid clearKey{};
      clearKey.uuid = {0xe2, 0x71, 0x9d, 0x58, 0xa9, 0x85, 0xb3, 0xc9,
                       0x78, 0x1a, 0xb0, 0x30, 0xaf, 0x78, 0xd3, 0x0e};
      _aidl_return->uuids = {commonPssh, clearKey};
      _aidl_return->mimeTypes.clear();
    }
    return ScopedAStatus::ok();
  }
};

}  // namespace

int main() {
  ABinderProcess_setThreadPoolMaxThreadCount(2);
  auto svc = ndk::SharedRefBase::make<DrmFactoryService>();
  if (AServiceManager_addService(svc->asBinder().get(), kDrmFactoryName) !=
      STATUS_OK) {
    return 2;
  }
  ABinderProcess_joinThreadPool();
  return 0;
}
