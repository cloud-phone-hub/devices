// engine/vendor/hardware 线 —— android.hardware.usb.gadget（IUsbGadget v1）自研服务实现。
//
// 注册 android.hardware.usb.gadget.IUsbGadget/default。Gadget HAL 用于在设备作为 USB
// 从机时编排 MTP/ADB 等复合功能。云端虚拟机没有真实 gadget 控制器：请求按 no-op 成功受理
// （不回拨功能就绪回调，运行期功能编排属设备集成层职责）。设备无关，不内置机型常量。

#if !defined(CDH_ANDROID_VERSION) || CDH_ANDROID_VERSION >= 14

#include <android/binder_ibinder.h>
#include <android/binder_status.h>

extern "C" {
void ABinderProcess_setThreadPoolMaxThreadCount(uint32_t numThreads);
void ABinderProcess_joinThreadPool(void);
binder_status_t AServiceManager_addService(AIBinder* binder,
                                           const char* instance);
}

#include <aidl/android/hardware/usb/gadget/BnUsbGadget.h>
#include <aidl/android/hardware/usb/gadget/IUsbGadgetCallback.h>

using aidl::android::hardware::usb::gadget::BnUsbGadget;
using aidl::android::hardware::usb::gadget::IUsbGadgetCallback;
using ndk::ScopedAStatus;

static constexpr const char* kUsbGadgetServiceName =
    "android.hardware.usb.gadget.IUsbGadget/default";

namespace {

class UsbGadgetService : public BnUsbGadget {
 public:
  ScopedAStatus setCurrentUsbFunctions(
      int64_t /*in_functions*/,
      const std::shared_ptr<IUsbGadgetCallback>& /*in_callback*/,
      int64_t /*in_timeoutMs*/, int64_t /*in_transactionId*/) override {
    return ScopedAStatus::ok();
  }

  ScopedAStatus getCurrentUsbFunctions(
      const std::shared_ptr<IUsbGadgetCallback>& /*in_callback*/,
      int64_t /*in_transactionId*/) override {
    return ScopedAStatus::ok();
  }

  ScopedAStatus getUsbSpeed(
      const std::shared_ptr<IUsbGadgetCallback>& /*in_callback*/,
      int64_t /*in_transactionId*/) override {
    return ScopedAStatus::ok();
  }

  ScopedAStatus reset(
      const std::shared_ptr<IUsbGadgetCallback>& /*in_callback*/,
      int64_t /*in_transactionId*/) override {
    return ScopedAStatus::ok();
  }
};

}  // namespace

int main() {
  ABinderProcess_setThreadPoolMaxThreadCount(2);
  auto svc = ndk::SharedRefBase::make<UsbGadgetService>();
  if (AServiceManager_addService(svc->asBinder().get(), kUsbGadgetServiceName) !=
      STATUS_OK) {
    return 2;
  }
  ABinderProcess_joinThreadPool();
  return 0;
}

#endif
