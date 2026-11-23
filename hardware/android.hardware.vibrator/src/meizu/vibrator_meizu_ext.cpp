// android.hardware.vibrator@meizu 变体扩展：注册 vendor.awa.reality.IRealityTap/default。
#include "vibrator_meizu_ext.h"

#include <aidl/android/hardware/vibrator/EffectStrength.h>
#include <aidl/android/hardware/vibrator/IVibratorCallback.h>
#include <aidl/vendor/awa/reality/BnRealityTap.h>
#include <aidl/vendor/awa/reality/RealityTapHe.h>
#include <aidl/vendor/awa/reality/RealityTapRtp.h>
#include <android/binder_manager.h>
#include <android/log.h>

#include <cstdint>
#include <memory>

namespace cdh::vibrator {
namespace {

using aidl::android::hardware::vibrator::EffectStrength;
using aidl::android::hardware::vibrator::IVibratorCallback;
using aidl::vendor::awa::reality::BnRealityTap;
using aidl::vendor::awa::reality::RealityTapHe;
using aidl::vendor::awa::reality::RealityTapRtp;
using ndk::ScopedAStatus;

constexpr char kRealityTapServiceName[] =
    "vendor.awa.reality.IRealityTap/default";

void completeNow(const std::shared_ptr<IVibratorCallback>& callback) {
  if (callback != nullptr) {
    callback->onComplete();
  }
}

class RealityTap final : public BnRealityTap {
 public:
  ScopedAStatus performTap(const RealityTapHe& /*he*/,
                           EffectStrength /*strength*/,
                           const std::shared_ptr<IVibratorCallback>& callback,
                           int32_t* _aidl_return) override {
    completeNow(callback);
    *_aidl_return = 0;
    return ScopedAStatus::ok();
  }
  ScopedAStatus performRtp(const RealityTapRtp& /*rtp*/,
                           EffectStrength /*strength*/,
                           const std::shared_ptr<IVibratorCallback>& callback,
                           int32_t* _aidl_return) override {
    completeNow(callback);
    *_aidl_return = 0;
    return ScopedAStatus::ok();
  }
  ScopedAStatus cancel(int32_t* _aidl_return) override {
    *_aidl_return = 0;
    return ScopedAStatus::ok();
  }
  ScopedAStatus calibrate(int32_t* _aidl_return) override {
    *_aidl_return = 0;
    return ScopedAStatus::ok();
  }
  ScopedAStatus getF0(int32_t* _aidl_return) override {
    *_aidl_return = 0;
    return ScopedAStatus::ok();
  }
};

}  // namespace

std::shared_ptr<ndk::ICInterface> RegisterMeizuRealityTap() {
  auto svc = ndk::SharedRefBase::make<RealityTap>();
  binder_status_t st =
      AServiceManager_addService(svc->asBinder().get(), kRealityTapServiceName);
  if (st != STATUS_OK) {
    __android_log_print(ANDROID_LOG_WARN, "cdh-vibrator",
                        "addService(%s) failed: %d", kRealityTapServiceName, st);
    return nullptr;
  }
  return svc;
}

}  // namespace cdh::vibrator
