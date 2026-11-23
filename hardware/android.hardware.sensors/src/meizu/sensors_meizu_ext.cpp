// android.hardware.sensors@meizu 变体扩展：注册 vendor.meizu.sensor_aux.ISensorAux/default。
#include "sensors_meizu_ext.h"

#include <aidl/vendor/meizu/sensor_aux/BnSensorAux.h>
#include <android/binder_manager.h>
#include <android/log.h>

#include <cstdint>
#include <memory>
#include <optional>

namespace cdh::sensors {
namespace {

using aidl::vendor::meizu::sensor_aux::BnSensorAux;
using aidl::vendor::meizu::sensor_aux::IScreenDataProviderCallback;
using aidl::vendor::meizu::sensor_aux::SAuxStatus;
using aidl::vendor::meizu::sensor_aux::SensorAuxArgs;
using aidl::vendor::meizu::sensor_aux::SensorAuxConfig;
using aidl::vendor::meizu::sensor_aux::SensorAuxDisplayInfo;
using ndk::ScopedAStatus;

constexpr char kSensorAuxServiceName[] =
    "vendor.meizu.sensor_aux.ISensorAux/default";

class SensorAuxService final : public BnSensorAux {
 public:
  ScopedAStatus getColor(int32_t* _aidl_return) override {
    *_aidl_return = 0;
    return ScopedAStatus::ok();
  }
  ScopedAStatus getDisplayInfo(
      std::optional<SensorAuxDisplayInfo>* _aidl_return) override {
    _aidl_return->reset();
    return ScopedAStatus::ok();
  }
  ScopedAStatus registerScreenDataProvider(
      const std::shared_ptr<IScreenDataProviderCallback>& in_provider,
      int32_t* _aidl_return) override {
    (void)in_provider;
    *_aidl_return = 0;
    return ScopedAStatus::ok();
  }
  ScopedAStatus sensorAuxCtrl(bool in_enable, int32_t* _aidl_return) override {
    (void)in_enable;
    *_aidl_return = 0;
    return ScopedAStatus::ok();
  }
  ScopedAStatus sensorAuxExecuteCmd(
      int32_t in_cmd, const std::optional<SensorAuxArgs>& in_args,
      int32_t* _aidl_return) override {
    (void)in_cmd;
    (void)in_args;
    *_aidl_return = 0;
    return ScopedAStatus::ok();
  }
  ScopedAStatus setSensorAuxConfig(
      const std::optional<SensorAuxConfig>& in_config,
      int32_t* _aidl_return) override {
    (void)in_config;
    *_aidl_return = 0;
    return ScopedAStatus::ok();
  }
  ScopedAStatus unregisterScreenDataProvider(
      int32_t in_token, SAuxStatus* _aidl_return) override {
    (void)in_token;
    *_aidl_return = SAuxStatus::OK;
    return ScopedAStatus::ok();
  }
  ScopedAStatus updateRGBAvg(int32_t in_value1, int32_t in_value2,
                             SAuxStatus* _aidl_return) override {
    (void)in_value1;
    (void)in_value2;
    *_aidl_return = SAuxStatus::OK;
    return ScopedAStatus::ok();
  }
  ScopedAStatus updateSensorStatus(int32_t in_value1, bool in_flag,
                                   SAuxStatus* _aidl_return) override {
    (void)in_value1;
    (void)in_flag;
    *_aidl_return = SAuxStatus::OK;
    return ScopedAStatus::ok();
  }
  ScopedAStatus writeFPSToPromixity(float in_fps) override {
    (void)in_fps;
    return ScopedAStatus::ok();
  }
};

}  // namespace

std::shared_ptr<ndk::ICInterface> RegisterMeizuSensorAux() {
  auto svc = ndk::SharedRefBase::make<SensorAuxService>();
  binder_status_t st =
      AServiceManager_addService(svc->asBinder().get(), kSensorAuxServiceName);
  if (st != STATUS_OK) {
    __android_log_print(ANDROID_LOG_WARN, "cdh-sensors",
                        "addService(%s) failed: %d", kSensorAuxServiceName, st);
    return nullptr;
  }
  return svc;
}

}  // namespace cdh::sensors
