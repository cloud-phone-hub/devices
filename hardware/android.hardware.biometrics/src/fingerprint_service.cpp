// engine/vendor/hardware 线 —— android.hardware.biometrics.fingerprint（IFingerprint v4）自研服务实现。
//
// 注册 android.hardware.biometrics.fingerprint.IFingerprint/default。指纹 HAL 通过
// getSensorProps 上报传感器、再按传感器 createSession 做录入/识别。
//
// 云端虚拟机没有指纹传感器，默认（基础变体）如实上报空列表、createSession 直接
// 返回 EX_UNSUPPORTED_OPERATION。
//
// 变体 fakefingerprint（ColorOS 机型专用）：上报 1 块不接硬件的占位传感器，
// 并允许 createSession 成功。原因见下面两处 #if 处的注释——ColorOS 的 biometrics
// 扩展对「空传感器表」与「createSession 抛异常」都不做容错，两处都会把
// system_server 打死。设备无关，不内置机型常量。

#include <android/binder_ibinder.h>
#include <android/binder_status.h>

extern "C" {
void ABinderProcess_setThreadPoolMaxThreadCount(uint32_t numThreads);
void ABinderProcess_joinThreadPool(void);
binder_status_t AServiceManager_addService(AIBinder* binder,
                                           const char* instance);
}

#include <aidl/android/hardware/biometrics/common/BnCancellationSignal.h>
#include <aidl/android/hardware/biometrics/common/CommonProps.h>
#include <aidl/android/hardware/biometrics/common/OperationContext.h>
#include <aidl/android/hardware/biometrics/common/SensorStrength.h>
#include <aidl/android/hardware/biometrics/fingerprint/BnFingerprint.h>
#include <aidl/android/hardware/biometrics/fingerprint/BnSession.h>
#include <aidl/android/hardware/biometrics/fingerprint/FingerprintSensorType.h>
#include <aidl/android/hardware/biometrics/fingerprint/ISession.h>
#include <aidl/android/hardware/biometrics/fingerprint/ISessionCallback.h>
#include <aidl/android/hardware/biometrics/fingerprint/PointerContext.h>
#include <aidl/android/hardware/biometrics/fingerprint/SensorProps.h>
#include <aidl/android/hardware/keymaster/HardwareAuthToken.h>

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

using aidl::android::hardware::biometrics::common::BnCancellationSignal;
using aidl::android::hardware::biometrics::common::OperationContext;
using aidl::android::hardware::biometrics::common::SensorStrength;
using aidl::android::hardware::biometrics::fingerprint::BnFingerprint;
using aidl::android::hardware::biometrics::fingerprint::BnSession;
using aidl::android::hardware::biometrics::fingerprint::FingerprintSensorType;
using aidl::android::hardware::biometrics::fingerprint::ISession;
using aidl::android::hardware::biometrics::fingerprint::ISessionCallback;
using aidl::android::hardware::biometrics::fingerprint::PointerContext;
using aidl::android::hardware::biometrics::fingerprint::SensorProps;
using aidl::android::hardware::keymaster::HardwareAuthToken;
using ndk::ScopedAStatus;

static constexpr const char* kFingerprintServiceName =
    "android.hardware.biometrics.fingerprint.IFingerprint/default";

namespace {

#if defined(CDH_FINGERPRINT_FAKE_SENSOR)

// 不可取消的取消信号：占位流程都是「什么都不做、也永不回调」，所以 cancel() 只
// 需要返回成功——框架要的只是「有对象可调 cancel」，不会等它的结果。
class NoopCancellationSignal : public BnCancellationSignal {
 public:
  ScopedAStatus cancel() override { return ScopedAStatus::ok(); }
};

std::shared_ptr<ISession> MakeNoopSession() {
  class NoopSession : public BnSession {
   public:
    ScopedAStatus generateChallenge() override { return ScopedAStatus::ok(); }
    ScopedAStatus revokeChallenge(int64_t /*in_challenge*/) override {
      return ScopedAStatus::ok();
    }
    ScopedAStatus enroll(
        const HardwareAuthToken& /*in_hat*/,
        std::shared_ptr<aidl::android::hardware::biometrics::common::
                            ICancellationSignal>* _aidl_return) override {
      *_aidl_return = ndk::SharedRefBase::make<NoopCancellationSignal>();
      return ScopedAStatus::ok();
    }
    ScopedAStatus authenticate(
        int64_t /*in_operationId*/,
        std::shared_ptr<aidl::android::hardware::biometrics::common::
                            ICancellationSignal>* _aidl_return) override {
      *_aidl_return = ndk::SharedRefBase::make<NoopCancellationSignal>();
      return ScopedAStatus::ok();
    }
    ScopedAStatus detectInteraction(
        std::shared_ptr<aidl::android::hardware::biometrics::common::
                            ICancellationSignal>* _aidl_return) override {
      *_aidl_return = ndk::SharedRefBase::make<NoopCancellationSignal>();
      return ScopedAStatus::ok();
    }
    ScopedAStatus authenticateWithContext(
        int64_t /*in_operationId*/, const OperationContext& /*in_context*/,
        std::shared_ptr<aidl::android::hardware::biometrics::common::
                            ICancellationSignal>* _aidl_return) override {
      *_aidl_return = ndk::SharedRefBase::make<NoopCancellationSignal>();
      return ScopedAStatus::ok();
    }
    ScopedAStatus enrollWithContext(
        const HardwareAuthToken& /*in_hat*/, const OperationContext& /*in_context*/,
        std::shared_ptr<aidl::android::hardware::biometrics::common::
                            ICancellationSignal>* _aidl_return) override {
      *_aidl_return = ndk::SharedRefBase::make<NoopCancellationSignal>();
      return ScopedAStatus::ok();
    }
    ScopedAStatus detectInteractionWithContext(
        const OperationContext& /*in_context*/,
        std::shared_ptr<aidl::android::hardware::biometrics::common::
                            ICancellationSignal>* _aidl_return) override {
      *_aidl_return = ndk::SharedRefBase::make<NoopCancellationSignal>();
      return ScopedAStatus::ok();
    }
    ScopedAStatus enumerateEnrollments() override { return ScopedAStatus::ok(); }
    ScopedAStatus removeEnrollments(const std::vector<int32_t>& /*ids*/) override {
      return ScopedAStatus::ok();
    }
    ScopedAStatus getAuthenticatorId() override { return ScopedAStatus::ok(); }
    ScopedAStatus invalidateAuthenticatorId() override { return ScopedAStatus::ok(); }
    ScopedAStatus resetLockout(const HardwareAuthToken& /*in_hat*/) override {
      return ScopedAStatus::ok();
    }
    ScopedAStatus close() override { return ScopedAStatus::ok(); }
    ScopedAStatus onPointerDown(int32_t /*pointerId*/, int32_t /*x*/, int32_t /*y*/,
                               float /*minor*/, float /*major*/) override {
      return ScopedAStatus::ok();
    }
    ScopedAStatus onPointerUp(int32_t /*pointerId*/) override {
      return ScopedAStatus::ok();
    }
    ScopedAStatus onUiReady() override { return ScopedAStatus::ok(); }
    ScopedAStatus onPointerDownWithContext(
        const PointerContext& /*in_context*/) override {
      return ScopedAStatus::ok();
    }
    ScopedAStatus onPointerUpWithContext(
        const PointerContext& /*in_context*/) override {
      return ScopedAStatus::ok();
    }
    ScopedAStatus onPointerCancelWithContext(
        const PointerContext& /*in_context*/) override {
      return ScopedAStatus::ok();
    }
    ScopedAStatus onContextChanged(const OperationContext& /*in_context*/) override {
      return ScopedAStatus::ok();
    }
    ScopedAStatus setIgnoreDisplayTouches(bool /*shouldIgnore*/) override {
      return ScopedAStatus::ok();
    }
  };
  return ndk::SharedRefBase::make<NoopSession>();
}

#endif  // CDH_FINGERPRINT_FAKE_SENSOR

class FingerprintService : public BnFingerprint {
 public:
  ScopedAStatus getSensorProps(
      std::vector<SensorProps>* _aidl_return) override {
    _aidl_return->clear();  // 默认无指纹传感器
#if defined(CDH_FINGERPRINT_FAKE_SENSOR)
    // ColorOS 变体（变体 fakefingerprint）：必须至少上报 1 个传感器。
    //
    // 官方 /odm/etc/vintf/manifest/manifest_oplus_fingerprint_aidl_v3.xml 声明了
    // IFingerprint/default，所以 ColorOS 的 FingerprintProvider 一定会走 AIDL 探测；
    // 而 OPPO 的 FingerprintServiceProviderExtImpl.init() 对 SensorList 直接取
    // valueAt(0)，空表抛 ArrayIndexOutOfBoundsException → "FATAL EXCEPTION IN
    // SYSTEM PROCESS" → system_server 自杀、zygote 重启（实测约 15 s 一轮无尽头）。
    //
    // 这里给的是一块「合法但不接硬件」的占位传感器：只为让框架的探测流程走完。
    SensorProps sensor;
    sensor.commonProps.sensorId = 0;
    sensor.commonProps.sensorStrength = SensorStrength::CONVENIENCE;
    sensor.commonProps.maxEnrollmentsPerUser = 1;
    sensor.sensorType = FingerprintSensorType::REAR;
    _aidl_return->push_back(std::move(sensor));
#endif
    return ScopedAStatus::ok();
  }

  ScopedAStatus createSession(
      int32_t /*in_sensorId*/, int32_t /*in_userId*/,
      const std::shared_ptr<ISessionCallback>& /*in_cb*/,
      std::shared_ptr<ISession>* _aidl_return) override {
#if defined(CDH_FINGERPRINT_FAKE_SENSOR)
    // 有了传感器之后，framework 在开机阶段就会自己调一次 createSession：
    //   FingerprintProvider.scheduleInternalCleanup
    //     → FingerprintStartUserClient.startHalOperation
    //       → IFingerprint.createSession（FingerprintHandler 线程，不 catch 异常）
    // 抛 EX_UNSUPPORTED_OPERATION 会被当成未捕获异常 → 同样是
    // "FATAL EXCEPTION IN SYSTEM PROCESS" → system_server 自杀（实测于实例 12，
    // 每轮 boot 5 次、约 15 s 一轮）。所以这里必须返回一个可用的 session 对象，
    // 哪怕它什么都不做。
    *_aidl_return = MakeNoopSession();
    return ScopedAStatus::ok();
#else
    return ScopedAStatus::fromExceptionCodeWithMessage(
        EX_UNSUPPORTED_OPERATION, "no fingerprint sensor");
#endif
  }
};

}  // namespace

int main() {
  ABinderProcess_setThreadPoolMaxThreadCount(2);
  auto svc = ndk::SharedRefBase::make<FingerprintService>();
  if (AServiceManager_addService(svc->asBinder().get(), kFingerprintServiceName) !=
      STATUS_OK) {
    return 2;
  }
  ABinderProcess_joinThreadPool();
  return 0;
}
