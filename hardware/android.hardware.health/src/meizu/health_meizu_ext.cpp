// android.hardware.health@meizu 变体扩展：注册 vendor.meizu.hardware.meizu_charging.IMeizuCharging/default。
#include "health_meizu_ext.h"

#include <aidl/vendor/meizu/hardware/meizu_charging/BnMeizuCharging.h>
#include <android/binder_manager.h>
#include <android/log.h>

#include <cstdint>
#include <memory>
#include <string>

namespace cdh::health {
namespace {

using aidl::vendor::meizu::hardware::meizu_charging::BatteryUPSMode;
using aidl::vendor::meizu::hardware::meizu_charging::BnMeizuCharging;
using aidl::vendor::meizu::hardware::meizu_charging::CharingThresholdCtrl;
using aidl::vendor::meizu::hardware::meizu_charging::ReverseWirelssChargingMode;
using aidl::vendor::meizu::hardware::meizu_charging::WiredCharingLevel;
using aidl::vendor::meizu::hardware::meizu_charging::WirelessChargingLevel;
using aidl::vendor::meizu::hardware::meizu_charging::WirelessTxFanMode;
using ndk::ScopedAStatus;

constexpr char kMeizuChargingServiceName[] =
    "vendor.meizu.hardware.meizu_charging.IMeizuCharging/default";

class MeizuCharging final : public BnMeizuCharging {
 public:
  ScopedAStatus getWirelessTxModel(std::string* _aidl_return) override {
    *_aidl_return = tx_model_;
    return ScopedAStatus::ok();
  }
  ScopedAStatus setReverseWirelssChargingMode(ReverseWirelssChargingMode mode,
                                              bool* _aidl_return) override {
    reverse_mode_ = mode;
    *_aidl_return = true;
    return ScopedAStatus::ok();
  }
  ScopedAStatus setWiredCharingLevel(WiredCharingLevel level,
                                     bool* _aidl_return) override {
    wired_level_ = level;
    *_aidl_return = true;
    return ScopedAStatus::ok();
  }
  ScopedAStatus setWirelessChargingLevel(WirelessChargingLevel level,
                                         bool* _aidl_return) override {
    wireless_level_ = level;
    *_aidl_return = true;
    return ScopedAStatus::ok();
  }
  ScopedAStatus setWirelessTxFanMode(WirelessTxFanMode mode,
                                     bool* _aidl_return) override {
    fan_mode_ = mode;
    *_aidl_return = true;
    return ScopedAStatus::ok();
  }
  ScopedAStatus getBatterySOH(int32_t* _aidl_return) override {
    *_aidl_return = soh_;
    return ScopedAStatus::ok();
  }
  ScopedAStatus setBatteryUPSMode(BatteryUPSMode mode,
                                  bool* _aidl_return) override {
    ups_mode_ = mode;
    *_aidl_return = true;
    return ScopedAStatus::ok();
  }
  ScopedAStatus getCharingThresholdCtrl(int32_t* _aidl_return) override {
    *_aidl_return = static_cast<int32_t>(ctrl_);
    return ScopedAStatus::ok();
  }
  ScopedAStatus setCharingThresholdCtrl(CharingThresholdCtrl ctrl,
                                        bool* _aidl_return) override {
    ctrl_ = ctrl;
    *_aidl_return = true;
    return ScopedAStatus::ok();
  }
  ScopedAStatus getCharingThreshold(int32_t* _aidl_return) override {
    *_aidl_return = threshold_;
    return ScopedAStatus::ok();
  }
  ScopedAStatus setCharingThreshold(int32_t threshold,
                                    bool* _aidl_return) override {
    threshold_ = threshold;
    *_aidl_return = true;
    return ScopedAStatus::ok();
  }

 private:
  std::string tx_model_;
  ReverseWirelssChargingMode reverse_mode_ =
      ReverseWirelssChargingMode::UNKNOWN;
  WiredCharingLevel wired_level_ = WiredCharingLevel::UNKNOWN;
  WirelessChargingLevel wireless_level_ = WirelessChargingLevel::UNKNOWN;
  WirelessTxFanMode fan_mode_ = WirelessTxFanMode::UNKNOWN;
  BatteryUPSMode ups_mode_ = BatteryUPSMode::UNKNOWN;
  CharingThresholdCtrl ctrl_ = CharingThresholdCtrl::UNKNOWN;
  int32_t soh_ = -1;
  int32_t threshold_ = 0;
};

}  // namespace

std::shared_ptr<ndk::ICInterface> RegisterMeizuCharging() {
  auto svc = ndk::SharedRefBase::make<MeizuCharging>();
  binder_status_t st =
      AServiceManager_addService(svc->asBinder().get(), kMeizuChargingServiceName);
  if (st != STATUS_OK) {
    __android_log_print(ANDROID_LOG_WARN, "cdh-health",
                        "addService(%s) failed: %d", kMeizuChargingServiceName, st);
    return nullptr;
  }
  return svc;
}

}  // namespace cdh::health
