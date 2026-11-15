// engine/vendor/hardware 线 —— android.hardware.security（ISecureClock v1 + ISharedSecret v1）。
//
// 这两个 keymint 配套接口通常由远程 keymint 服务进程一并提供。hal 线里作为独立默认桩
// 落地：无 TEE，故
//   * ISharedSecret/default：getSharedSecretParameters / computeSharedSecret 返回不支持；
//   * ISecureClock/default ：generateTimeStamp 返回不支持。
// 若某设备的集成层改由真实 keymint 进程合并注册这两项，应避免与本桩争抢同名实例：
// 远程 keymint（android.hardware.keymint 模块，vendor.keymint-default）会一并注册
// ISecureClock/default 与 ISharedSecret/default，且对 addService 结果 CHECK 硬失败。
// 因此本桩遇到 STATUS_ALREADY_EXISTS 不报错：已有实现先到时本桩让位（它的后端是
// host TEE，语义更强），本桩只在对方缺席（无 keymint 通道的机型）时作为默认实现。
// 设备无关，不内置机型常量。

#include <android/binder_ibinder.h>
#include <android/binder_status.h>

extern "C" {
void ABinderProcess_setThreadPoolMaxThreadCount(uint32_t numThreads);
void ABinderProcess_joinThreadPool(void);
binder_status_t AServiceManager_addService(AIBinder* binder,
                                           const char* instance);
}

#include <aidl/android/hardware/security/secureclock/BnSecureClock.h>
#include <aidl/android/hardware/security/secureclock/TimeStampToken.h>
#include <aidl/android/hardware/security/sharedsecret/BnSharedSecret.h>
#include <aidl/android/hardware/security/sharedsecret/SharedSecretParameters.h>

#include <cstdint>
#include <vector>

using aidl::android::hardware::security::secureclock::BnSecureClock;
using aidl::android::hardware::security::secureclock::TimeStampToken;
using aidl::android::hardware::security::sharedsecret::BnSharedSecret;
using aidl::android::hardware::security::sharedsecret::SharedSecretParameters;
using ndk::ScopedAStatus;

static constexpr const char* kSecureClockName =
    "android.hardware.security.secureclock.ISecureClock/default";
static constexpr const char* kSharedSecretName =
    "android.hardware.security.sharedsecret.ISharedSecret/default";

namespace {

class SecureClockService : public BnSecureClock {
 public:
  ScopedAStatus generateTimeStamp(int64_t /*in_challenge*/,
                                  TimeStampToken* /*_aidl_return*/) override {
    return ScopedAStatus::fromExceptionCodeWithMessage(
        EX_UNSUPPORTED_OPERATION, "no secure clock / TEE");
  }
};

class SharedSecretService : public BnSharedSecret {
 public:
  ScopedAStatus getSharedSecretParameters(
      SharedSecretParameters* /*_aidl_return*/) override {
    return ScopedAStatus::fromExceptionCodeWithMessage(
        EX_UNSUPPORTED_OPERATION, "no shared secret / TEE");
  }

  ScopedAStatus computeSharedSecret(
      const std::vector<SharedSecretParameters>& /*in_parameters*/,
      std::vector<uint8_t>* /*_aidl_return*/) override {
    return ScopedAStatus::fromExceptionCodeWithMessage(
        EX_UNSUPPORTED_OPERATION, "no shared secret / TEE");
  }
};

}  // namespace

int main() {
  ABinderProcess_setThreadPoolMaxThreadCount(2);
  auto sc = ndk::SharedRefBase::make<SecureClockService>();
  auto ss = ndk::SharedRefBase::make<SharedSecretService>();
  const binder_status_t ssStatus =
      AServiceManager_addService(ss->asBinder().get(), kSharedSecretName);
  const binder_status_t scStatus =
      AServiceManager_addService(sc->asBinder().get(), kSecureClockName);
  // 远程 keymint 进程先注册同名实例时退让（见文件头）：不算失败。
  const auto yielded = [](binder_status_t status) {
    return status == STATUS_OK || status == STATUS_ALREADY_EXISTS;
  };
  if (!yielded(ssStatus) || !yielded(scStatus)) {
    return 2;
  }
  ABinderProcess_joinThreadPool();
  return 0;
}
