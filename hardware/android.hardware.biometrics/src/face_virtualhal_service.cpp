// engine/vendor/hardware 线 —— 虚拟人脸控制平面（face virtualhal）空桩。
//
// Flyme 的 system_server 在本机型上按名同步取
//   android.hardware.biometrics.face.virtualhal.IVirtualHal/virtual
// 取不到就每秒重试一次（事件日志里 1472 条）。该接口是 hardware/interfaces 的
// unstable AIDL：不进 FCM，lineage 底座也没有实现，故由本服务补空桩——配置类
// 调用一律忽略，getFaceHal 返回空指针。真正的 IFace 由同模块的 face_service.cpp
// 提供（独立进程，见 vendor/etc/init 的两个 rc）。
// 设备无关，不内置机型常量。

#include <android/binder_ibinder.h>
#include <android/binder_status.h>

extern "C" {
void ABinderProcess_setThreadPoolMaxThreadCount(uint32_t numThreads);
void ABinderProcess_joinThreadPool(void);
binder_status_t AServiceManager_addService(AIBinder* binder,
                                           const char* instance);
}

#include <aidl/android/hardware/biometrics/face/virtualhal/BnVirtualHal.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace facevhal = aidl::android::hardware::biometrics::face::virtualhal;
namespace face = aidl::android::hardware::biometrics::face;
namespace bio = aidl::android::hardware::biometrics::common;

using ndk::ScopedAStatus;

static constexpr const char* kVirtualHalName =
    "android.hardware.biometrics.face.virtualhal.IVirtualHal/virtual";

namespace {

// 没有可配置的虚拟传感器：全部配置调用都是 no-op。
class FaceVirtualHal : public facevhal::BnVirtualHal {
 public:
  ScopedAStatus setEnrollments(const std::vector<int32_t>& /*in_id*/) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus setEnrollmentHit(int32_t /*in_hit_id*/) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus setNextEnrollment(
      const facevhal::NextEnrollment& /*in_next_enrollment*/) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus setAuthenticatorId(int64_t /*in_id*/) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus setChallenge(int64_t /*in_challenge*/) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus setOperationAuthenticateFails(bool /*in_fail*/) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus setOperationAuthenticateLatency(
      const std::vector<int32_t>& /*in_latencyMs*/) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus setOperationAuthenticateDuration(
      int32_t /*in_durationMs*/) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus setOperationAuthenticateError(int32_t /*in_error*/) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus setOperationAuthenticateAcquired(
      const std::vector<facevhal::AcquiredInfoAndVendorCode>& /*in_acquired*/)
      override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus setOperationEnrollLatency(
      const std::vector<int32_t>& /*in_latencyMs*/) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus setOperationDetectInteractionLatency(
      const std::vector<int32_t>& /*in_latencyMs*/) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus setOperationDetectInteractionFails(bool /*in_error*/) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus setLockout(bool /*in_lockout*/) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus setLockoutEnable(bool /*in_enable*/) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus setLockoutTimedEnable(bool /*in_enable*/) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus setLockoutTimedThreshold(int32_t /*in_threshold*/) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus setLockoutTimedDuration(int32_t /*in_durationMs*/) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus setLockoutPermanentThreshold(int32_t /*in_threshold*/) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus resetConfigurations() override { return ScopedAStatus::ok(); }
  ScopedAStatus setType(face::FaceSensorType /*in_type*/) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus setSensorStrength(bio::SensorStrength /*in_strength*/) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus getFaceHal(std::shared_ptr<face::IFace>* _aidl_return) override {
    _aidl_return->reset();  // 虚拟控制面不接管真实 HAL 实例
    return ScopedAStatus::ok();
  }
};

}  // namespace

int main() {
  ABinderProcess_setThreadPoolMaxThreadCount(2);
  auto svc = ndk::SharedRefBase::make<FaceVirtualHal>();
  if (AServiceManager_addService(svc->asBinder().get(), kVirtualHalName) !=
      STATUS_OK) {
    return 2;
  }
  ABinderProcess_joinThreadPool();
  return 0;
}
