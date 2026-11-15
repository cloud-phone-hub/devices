// engine/vendor/hardware 线 —— android.hardware.identity（IIdentityCredentialStore v5）自研服务实现。
//
// 注册 android.hardware.identity.IIdentityCredentialStore/default。Identity Credential HAL
// （ISO 18013-5 mdoc）依赖安全元件做凭据个性化/出示与远程配置证明密钥。云端虚拟机没有
// 承载这些私钥的 SE/TEE：本桩把 store 的全部入口都判为"无能力"，一律返回
// EX_UNSUPPORTED_OPERATION（不产出 IWritableIdentityCredential / IIdentityCredential /
// IPresentationSession / IRemotelyProvisionedComponent 句柄）。
// 设备无关，不内置机型常量。

#include <android/binder_ibinder.h>
#include <android/binder_status.h>

extern "C" {
void ABinderProcess_setThreadPoolMaxThreadCount(uint32_t numThreads);
void ABinderProcess_joinThreadPool(void);
binder_status_t AServiceManager_addService(AIBinder* binder,
                                           const char* instance);
}

#include <aidl/android/hardware/identity/BnIdentityCredentialStore.h>
#include <aidl/android/hardware/identity/CipherSuite.h>
#include <aidl/android/hardware/identity/HardwareInformation.h>
#include <aidl/android/hardware/identity/IIdentityCredential.h>
#include <aidl/android/hardware/identity/IPresentationSession.h>
#include <aidl/android/hardware/identity/IWritableIdentityCredential.h>
#include <aidl/android/hardware/security/keymint/IRemotelyProvisionedComponent.h>

#include <memory>
#include <string>
#include <vector>

using aidl::android::hardware::identity::BnIdentityCredentialStore;
using aidl::android::hardware::identity::CipherSuite;
using aidl::android::hardware::identity::HardwareInformation;
using aidl::android::hardware::identity::IIdentityCredential;
using aidl::android::hardware::identity::IPresentationSession;
using aidl::android::hardware::identity::IWritableIdentityCredential;
using aidl::android::hardware::security::keymint::IRemotelyProvisionedComponent;
using ndk::ScopedAStatus;

static constexpr const char* kIdentityServiceName =
    "android.hardware.identity.IIdentityCredentialStore/default";

namespace {

constexpr const char* kNoSe = "no identity credential / secure element";

class IdentityCredentialStoreService : public BnIdentityCredentialStore {
 public:
  ScopedAStatus getHardwareInformation(
      HardwareInformation* /*_aidl_return*/) override {
    return ScopedAStatus::fromExceptionCodeWithMessage(
        EX_UNSUPPORTED_OPERATION, kNoSe);
  }

  ScopedAStatus createCredential(
      const std::string& /*in_docType*/, bool /*in_testCredential*/,
      std::shared_ptr<IWritableIdentityCredential>* /*_aidl_return*/) override {
    return ScopedAStatus::fromExceptionCodeWithMessage(
        EX_UNSUPPORTED_OPERATION, kNoSe);
  }

  ScopedAStatus getCredential(
      CipherSuite /*in_cipherSuite*/,
      const std::vector<uint8_t>& /*in_credentialData*/,
      std::shared_ptr<IIdentityCredential>* /*_aidl_return*/) override {
    return ScopedAStatus::fromExceptionCodeWithMessage(
        EX_UNSUPPORTED_OPERATION, kNoSe);
  }

  ScopedAStatus createPresentationSession(
      CipherSuite /*in_cipherSuite*/,
      std::shared_ptr<IPresentationSession>* /*_aidl_return*/) override {
    return ScopedAStatus::fromExceptionCodeWithMessage(
        EX_UNSUPPORTED_OPERATION, kNoSe);
  }

  ScopedAStatus getRemotelyProvisionedComponent(
      std::shared_ptr<IRemotelyProvisionedComponent>* /*_aidl_return*/) override {
    return ScopedAStatus::fromExceptionCodeWithMessage(
        EX_UNSUPPORTED_OPERATION, kNoSe);
  }
};

}  // namespace

int main() {
  ABinderProcess_setThreadPoolMaxThreadCount(2);
  auto svc = ndk::SharedRefBase::make<IdentityCredentialStoreService>();
  if (AServiceManager_addService(svc->asBinder().get(), kIdentityServiceName) !=
      STATUS_OK) {
    return 2;
  }
  ABinderProcess_joinThreadPool();
  return 0;
}
