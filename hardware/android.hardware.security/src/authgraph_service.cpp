// engine/vendor/hardware 线 —— android.hardware.security.authgraph（IAuthGraphKeyExchange v1）。
//
// 注册 android.hardware.security.authgraph.IAuthGraphKeyExchange/default。AuthGraph 在
// 多个 TEE 之间做认证图密钥协商。云端虚拟机无 TEE：所有协商方法返回不支持（不填充出参）。
// 与 secureclock/sharedsecret 同理——若集成层改由真实 keymint/authgraph 进程提供，应避开
// 同名实例争抢。设备无关，不内置机型常量。

#include <android/binder_ibinder.h>
#include <android/binder_status.h>

extern "C" {
void ABinderProcess_setThreadPoolMaxThreadCount(uint32_t numThreads);
void ABinderProcess_joinThreadPool(void);
binder_status_t AServiceManager_addService(AIBinder* binder,
                                           const char* instance);
}

#include <aidl/android/hardware/security/authgraph/Arc.h>
#include <aidl/android/hardware/security/authgraph/BnAuthGraphKeyExchange.h>
#include <aidl/android/hardware/security/authgraph/Identity.h>
#include <aidl/android/hardware/security/authgraph/KeInitResult.h>
#include <aidl/android/hardware/security/authgraph/Key.h>
#include <aidl/android/hardware/security/authgraph/PubKey.h>
#include <aidl/android/hardware/security/authgraph/SessionIdSignature.h>
#include <aidl/android/hardware/security/authgraph/SessionInfo.h>
#include <aidl/android/hardware/security/authgraph/SessionInitiationInfo.h>

#include <array>
#include <cstdint>
#include <vector>

using aidl::android::hardware::security::authgraph::Arc;
using aidl::android::hardware::security::authgraph::BnAuthGraphKeyExchange;
using aidl::android::hardware::security::authgraph::Identity;
using aidl::android::hardware::security::authgraph::KeInitResult;
using aidl::android::hardware::security::authgraph::Key;
using aidl::android::hardware::security::authgraph::PubKey;
using aidl::android::hardware::security::authgraph::SessionIdSignature;
using aidl::android::hardware::security::authgraph::SessionInfo;
using aidl::android::hardware::security::authgraph::SessionInitiationInfo;
using ndk::ScopedAStatus;

static constexpr const char* kAuthGraphName =
    "android.hardware.security.authgraph.IAuthGraphKeyExchange/default";

namespace {

constexpr const char* kNoTee = "no authgraph / TEE";

class AuthGraphService : public BnAuthGraphKeyExchange {
 public:
  ScopedAStatus create(SessionInitiationInfo* /*_aidl_return*/) override {
    return ScopedAStatus::fromExceptionCodeWithMessage(
        EX_UNSUPPORTED_OPERATION, kNoTee);
  }

  ScopedAStatus init(const PubKey& /*in_peerPubKey*/,
                     const Identity& /*in_peerId*/,
                     const std::vector<uint8_t>& /*in_peerNonce*/,
                     int32_t /*in_peerVersion*/,
                     KeInitResult* /*_aidl_return*/) override {
    return ScopedAStatus::fromExceptionCodeWithMessage(
        EX_UNSUPPORTED_OPERATION, kNoTee);
  }

  ScopedAStatus finish(const PubKey& /*in_peerPubKey*/,
                       const Identity& /*in_peerId*/,
                       const SessionIdSignature& /*in_peerSignature*/,
                       const std::vector<uint8_t>& /*in_peerNonce*/,
                       int32_t /*in_peerVersion*/, const Key& /*in_ownKey*/,
                       SessionInfo* /*_aidl_return*/) override {
    return ScopedAStatus::fromExceptionCodeWithMessage(
        EX_UNSUPPORTED_OPERATION, kNoTee);
  }

  ScopedAStatus authenticationComplete(
      const SessionIdSignature& /*in_peerSignature*/,
      const std::array<Arc, 2>& /*in_sharedKeys*/,
      std::array<Arc, 2>* /*_aidl_return*/) override {
    return ScopedAStatus::fromExceptionCodeWithMessage(
        EX_UNSUPPORTED_OPERATION, kNoTee);
  }
};

}  // namespace

int main() {
  ABinderProcess_setThreadPoolMaxThreadCount(2);
  auto svc = ndk::SharedRefBase::make<AuthGraphService>();
  if (AServiceManager_addService(svc->asBinder().get(), kAuthGraphName) !=
      STATUS_OK) {
    return 2;
  }
  ABinderProcess_joinThreadPool();
  return 0;
}
