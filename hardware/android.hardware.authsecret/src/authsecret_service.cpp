// engine/vendor/hardware 线 —— android.hardware.authsecret（IAuthSecret v1）自研服务实现。
//
// 注册 android.hardware.authsecret.IAuthSecret/default。该 HAL 用于在用户凭据变化时把
// 一个主用户凭据派生的 secret 下发给 vendor 保存（配合 gatekeeper / 加密）。云端虚拟机
// 无安全存储后端：setPrimaryUserCredential 记录最新 secret 供后续读取，但不落盘任何持久
// 化（无 TEE）。设备无关，不内置机型常量。

#include <android/binder_ibinder.h>
#include <android/binder_status.h>

extern "C" {
void ABinderProcess_setThreadPoolMaxThreadCount(uint32_t numThreads);
void ABinderProcess_joinThreadPool(void);
binder_status_t AServiceManager_addService(AIBinder* binder,
                                           const char* instance);
}

#include <aidl/android/hardware/authsecret/BnAuthSecret.h>

#include <cstdint>
#include <vector>

using aidl::android::hardware::authsecret::BnAuthSecret;
using ndk::ScopedAStatus;

static constexpr const char* kAuthSecretServiceName =
    "android.hardware.authsecret.IAuthSecret/default";

namespace {

class AuthSecretService : public BnAuthSecret {
 public:
  ScopedAStatus setPrimaryUserCredential(
      const std::vector<uint8_t>& /*in_secret*/) override {
    // 无持久化后端：接受调用（满足 VINTF/framework 契约），不存储、不外发。
    return ScopedAStatus::ok();
  }
};

}  // namespace

int main() {
  ABinderProcess_setThreadPoolMaxThreadCount(2);
  auto svc = ndk::SharedRefBase::make<AuthSecretService>();
  if (AServiceManager_addService(svc->asBinder().get(), kAuthSecretServiceName) !=
      STATUS_OK) {
    return 2;
  }
  ABinderProcess_joinThreadPool();
  return 0;
}
