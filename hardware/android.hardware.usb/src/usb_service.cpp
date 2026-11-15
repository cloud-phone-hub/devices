// engine/vendor/hardware 线 —— android.hardware.usb（IUsb v3）自研服务实现。
//
// 注册 android.hardware.usb.IUsb/default。该 HAL 面向有真实 USB Type-C 控制器/角色切换
// 的设备。云端虚拟机没有物理 USB 端口与角色协商：所有请求按 no-op 成功受理，仅保存 framework
// 下发的回调引用（不回拨任何端口事件）。设备无关，不内置机型常量。

#include <android/binder_ibinder.h>
#include <android/binder_status.h>

extern "C" {
void ABinderProcess_setThreadPoolMaxThreadCount(uint32_t numThreads);
void ABinderProcess_joinThreadPool(void);
binder_status_t AServiceManager_addService(AIBinder* binder,
                                           const char* instance);
}

#include <aidl/android/hardware/usb/BnUsb.h>
#include <aidl/android/hardware/usb/IUsbCallback.h>
#include <aidl/android/hardware/usb/PortRole.h>

#include <mutex>
#include <string>

using aidl::android::hardware::usb::BnUsb;
using aidl::android::hardware::usb::IUsbCallback;
using aidl::android::hardware::usb::PortRole;
using ndk::ScopedAStatus;

static constexpr const char* kUsbServiceName =
    "android.hardware.usb.IUsb/default";

namespace {

class UsbService : public BnUsb {
 public:
  ScopedAStatus enableContaminantPresenceDetection(
      const std::string& /*in_portName*/, bool /*in_enable*/,
      int64_t /*in_transactionId*/) override {
    return ScopedAStatus::ok();
  }

  ScopedAStatus enableUsbData(const std::string& /*in_portName*/,
                              bool /*in_enable*/,
                              int64_t /*in_transactionId*/) override {
    return ScopedAStatus::ok();
  }

  ScopedAStatus enableUsbDataWhileDocked(
      const std::string& /*in_portName*/,
      int64_t /*in_transactionId*/) override {
    return ScopedAStatus::ok();
  }

  ScopedAStatus queryPortStatus(int64_t /*in_transactionId*/) override {
    return ScopedAStatus::ok();
  }

  ScopedAStatus setCallback(
      const std::shared_ptr<IUsbCallback>& in_callback) override {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = in_callback;
    return ScopedAStatus::ok();
  }

  ScopedAStatus switchRole(const std::string& /*in_portName*/,
                           const PortRole& /*in_role*/,
                           int64_t /*in_transactionId*/) override {
    return ScopedAStatus::ok();
  }

  ScopedAStatus limitPowerTransfer(const std::string& /*in_portName*/,
                                   bool /*in_limit*/,
                                   int64_t /*in_transactionId*/) override {
    return ScopedAStatus::ok();
  }

  ScopedAStatus resetUsbPort(const std::string& /*in_portName*/,
                             int64_t /*in_transactionId*/) override {
    return ScopedAStatus::ok();
  }

 private:
  std::mutex mutex_;
  std::shared_ptr<IUsbCallback> callback_;
};

}  // namespace

int main() {
  ABinderProcess_setThreadPoolMaxThreadCount(2);
  auto svc = ndk::SharedRefBase::make<UsbService>();
  if (AServiceManager_addService(svc->asBinder().get(), kUsbServiceName) !=
      STATUS_OK) {
    return 2;
  }
  ABinderProcess_joinThreadPool();
  return 0;
}
