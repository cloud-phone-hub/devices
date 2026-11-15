// engine/vendor/hardware 线 —— android.hardware.biometrics.face（IFace v4）自研服务实现。
//
// 注册 android.hardware.biometrics.face.IFace/default。人脸 HAL 通过 getSensorProps 上报
// 传感器、再按传感器 createSession 做录入/识别。云端虚拟机没有人脸传感器：getSensorProps
// 返回空列表，createSession 无从建立 → 返回 EX_UNSUPPORTED_OPERATION。
// 设备无关，不内置机型常量。

#include <android/binder_ibinder.h>
#include <android/binder_status.h>

extern "C" {
void ABinderProcess_setThreadPoolMaxThreadCount(uint32_t numThreads);
void ABinderProcess_joinThreadPool(void);
binder_status_t AServiceManager_addService(AIBinder* binder,
                                           const char* instance);
}

#include <aidl/android/hardware/biometrics/face/BnFace.h>
#include <aidl/android/hardware/biometrics/face/ISession.h>
#include <aidl/android/hardware/biometrics/face/ISessionCallback.h>
#include <aidl/android/hardware/biometrics/face/SensorProps.h>

#include <cstdint>
#include <memory>
#include <vector>

using aidl::android::hardware::biometrics::face::BnFace;
using aidl::android::hardware::biometrics::face::ISession;
using aidl::android::hardware::biometrics::face::ISessionCallback;
using aidl::android::hardware::biometrics::face::SensorProps;
using ndk::ScopedAStatus;

static constexpr const char* kFaceServiceName =
    "android.hardware.biometrics.face.IFace/default";

namespace {

class FaceService : public BnFace {
 public:
  ScopedAStatus getSensorProps(
      std::vector<SensorProps>* _aidl_return) override {
    _aidl_return->clear();  // 无人脸传感器
    return ScopedAStatus::ok();
  }

  ScopedAStatus createSession(
      int32_t /*in_sensorId*/, int32_t /*in_userId*/,
      const std::shared_ptr<ISessionCallback>& /*in_cb*/,
      std::shared_ptr<ISession>* /*_aidl_return*/) override {
    return ScopedAStatus::fromExceptionCodeWithMessage(
        EX_UNSUPPORTED_OPERATION, "no face sensor");
  }
};

}  // namespace

int main() {
  ABinderProcess_setThreadPoolMaxThreadCount(2);
  auto svc = ndk::SharedRefBase::make<FaceService>();
  if (AServiceManager_addService(svc->asBinder().get(), kFaceServiceName) !=
      STATUS_OK) {
    return 2;
  }
  ABinderProcess_joinThreadPool();
  return 0;
}
