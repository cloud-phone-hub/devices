// android.hardware.light@meizu 变体扩展：注册 vendor.meizu.hardware.aw_light.IAwLight/default。
#include "light_meizu_ext.h"

#include <aidl/vendor/meizu/hardware/aw_light/BnAwLight.h>
#include <aidl/vendor/meizu/hardware/aw_light/IAwLightCallback.h>
#include <android/binder_manager.h>
#include <android/log.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace cdh::light {
namespace {

using aidl::vendor::meizu::hardware::aw_light::BnAwLight;
using aidl::vendor::meizu::hardware::aw_light::IAwLightCallback;
using ndk::ScopedAStatus;

constexpr char kAwLightServiceName[] =
    "vendor.meizu.hardware.aw_light.IAwLight/default";

class AwLight final : public BnAwLight {
 public:
  ScopedAStatus aw_color_set_phase(int32_t /*r*/, int32_t /*g*/, int32_t /*b*/,
                                   bool* _aidl_return) override {
    *_aidl_return = true;
    return ScopedAStatus::ok();
  }
  ScopedAStatus aw_get_duration_time(int32_t /*phase*/,
                                     int32_t* _aidl_return) override {
    *_aidl_return = 0;
    return ScopedAStatus::ok();
  }
  ScopedAStatus aw_light_init(bool* _aidl_return) override {
    *_aidl_return = true;
    return ScopedAStatus::ok();
  }
  ScopedAStatus aw_light_start(bool* _aidl_return) override {
    *_aidl_return = true;
    return ScopedAStatus::ok();
  }
  ScopedAStatus aw_light_stop(bool* _aidl_return) override {
    *_aidl_return = true;
    return ScopedAStatus::ok();
  }
  ScopedAStatus aw_light_switchEffect(int32_t /*effect*/,
                                      bool* _aidl_return) override {
    *_aidl_return = true;
    return ScopedAStatus::ok();
  }
  ScopedAStatus aw_music_phase(const std::vector<int32_t>& /*phases*/,
                               bool* _aidl_return) override {
    *_aidl_return = true;
    return ScopedAStatus::ok();
  }
  ScopedAStatus aw_set_Semaphore_letter(int8_t /*letter*/,
                                        bool* _aidl_return) override {
    *_aidl_return = true;
    return ScopedAStatus::ok();
  }
  ScopedAStatus aw_set_cl_brightness_phase(int32_t /*phase*/,
                                           bool* _aidl_return) override {
    *_aidl_return = true;
    return ScopedAStatus::ok();
  }
  ScopedAStatus aw_set_light_over_callback(
      const std::shared_ptr<IAwLightCallback>& callback,
      bool* _aidl_return) override {
    callback_ = callback;
    *_aidl_return = true;
    return ScopedAStatus::ok();
  }
  ScopedAStatus aw_set_progress(int32_t /*progress*/,
                                bool* _aidl_return) override {
    *_aidl_return = true;
    return ScopedAStatus::ok();
  }

 private:
  std::shared_ptr<IAwLightCallback> callback_;
};

}  // namespace

std::shared_ptr<ndk::ICInterface> RegisterMeizuAwLight() {
  auto svc = ndk::SharedRefBase::make<AwLight>();
  binder_status_t st =
      AServiceManager_addService(svc->asBinder().get(), kAwLightServiceName);
  if (st != STATUS_OK) {
    __android_log_print(ANDROID_LOG_WARN, "cdh-light",
                        "addService(%s) failed: %d", kAwLightServiceName, st);
    return nullptr;
  }
  return svc;
}

}  // namespace cdh::light
