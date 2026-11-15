// engine/vendor/hardware 线 —— android.hardware.nfc（INfc v1）自研服务实现。
//
// 注册 android.hardware.nfc.INfc/default。云端虚拟机没有 NFC 控制器，本服务按真机
// 「控制器上电失败」的语义应答：open() 先受理，随后异步回 OPEN_CPLT(FAILED)，
// 让 libnfc-nci 的 HalOpen() 立刻拿到失败、NfcService 干净地停在 OFF，
// 不再等一个永远不会来的事件。设备无关，不内置机型常量。
//
// 为什么不能只在 open() 里返回异常：INfc 的失败有两条路——返回 ScopedAStatus 异常，
// 或回 sendEvent(OPEN_CPLT, status)。栈里 NfcAdaptation::HalOpen 只认后者：返回异常时
// 它打一句 `HalOpenInternal: Open Error=OK` 就进 CondVar::wait，永远等不到事件，90 s 后
// NfcService$WatchDogThread("enableInternal", 90 s) 调 nfcManager_doAbort → SIGABRT，
// com.android.nfc 于是每 90 s 崩一次（实例 36 实测 4 次 / 8.4 min，tombstone 里
// `nfcManager_doInitialize+820 → CondVar::wait` 就是这条等）。close / coreInitialized /
// preDiscover / powerCycle 同理，各有一个 CPLT 事件要回。

#include <android/binder_ibinder.h>
#include <android/binder_status.h>
#include <log/log.h>

extern "C" {
void ABinderProcess_setThreadPoolMaxThreadCount(uint32_t numThreads);
void ABinderProcess_joinThreadPool(void);
binder_status_t AServiceManager_addService(AIBinder* binder,
                                           const char* instance);
}

#include <aidl/android/hardware/nfc/BnNfc.h>
#include <aidl/android/hardware/nfc/INfcClientCallback.h>
#include <aidl/android/hardware/nfc/NfcCloseType.h>
#include <aidl/android/hardware/nfc/NfcConfig.h>
#include <aidl/android/hardware/nfc/NfcEvent.h>
#include <aidl/android/hardware/nfc/NfcStatus.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

using aidl::android::hardware::nfc::BnNfc;
using aidl::android::hardware::nfc::INfcClientCallback;
using aidl::android::hardware::nfc::NfcCloseType;
using aidl::android::hardware::nfc::NfcConfig;
using aidl::android::hardware::nfc::NfcEvent;
using aidl::android::hardware::nfc::NfcStatus;
using ndk::ScopedAStatus;

static constexpr const char* kNfcServiceName =
    "android.hardware.nfc.INfc/default";

// 客户端 open() 返回后到进 CondVar::wait 之间的空档；太早回事件会被丢掉。
static constexpr auto kEventDelay = std::chrono::milliseconds(20);

namespace {

class NfcService : public BnNfc {
 public:
  // 受理 + 异步回 OPEN_CPLT(FAILED)：与真机控制器上电失败同形。
  ScopedAStatus open(
      const std::shared_ptr<INfcClientCallback>& in_clientCallback) override {
    if (in_clientCallback == nullptr) {
      return ScopedAStatus::fromExceptionCode(EX_NULL_POINTER);
    }
    {
      std::lock_guard<std::mutex> lock(mutex_);
      mCallback = in_clientCallback;
    }
    ALOGI("open: no nfc controller, reply OPEN_CPLT(FAILED) asynchronously");
    sendEventLater(in_clientCallback, NfcEvent::OPEN_CPLT, NfcStatus::FAILED);
    return ScopedAStatus::ok();
  }

  ScopedAStatus close(NfcCloseType in_type) override {
    std::shared_ptr<INfcClientCallback> callback = takeCallback();
    ALOGI("close: type=%d", static_cast<int>(in_type));
    if (callback != nullptr) {
      sendEventLater(callback, NfcEvent::CLOSE_CPLT, NfcStatus::OK);
    }
    return ScopedAStatus::ok();
  }

  // 栈在 open 成功后依次调 coreInitialized / preDiscover，各自等一个 CPLT 事件；
  // 本服务没有控制器，统一回 FAILED，保证调用方不等。
  ScopedAStatus coreInitialized() override {
    replyPending(NfcEvent::POST_INIT_CPLT);
    return ScopedAStatus::ok();
  }

  ScopedAStatus factoryReset() override { return ScopedAStatus::ok(); }

  ScopedAStatus getConfig(NfcConfig* _aidl_return) override {
    *_aidl_return = NfcConfig();  // 全 0：无 NCI 版本、无厂商能力
    return ScopedAStatus::ok();
  }

  ScopedAStatus powerCycle() override {
    replyPending(NfcEvent::OPEN_CPLT);
    return ScopedAStatus::ok();
  }

  ScopedAStatus preDiscover() override {
    replyPending(NfcEvent::PRE_DISCOVER_CPLT);
    return ScopedAStatus::ok();
  }

  // 没有控制器可写。
  ScopedAStatus write(const std::vector<uint8_t>& /*in_data*/,
                      int32_t* _aidl_return) override {
    *_aidl_return = 0;  // 没有控制器可写
    return ScopedAStatus::fromExceptionCodeWithMessage(
        EX_UNSUPPORTED_OPERATION, "no nfc controller");
  }

  ScopedAStatus setEnableVerboseLogging(bool /*in_enable*/) override {
    return ScopedAStatus::ok();
  }

  ScopedAStatus isVerboseLoggingEnabled(bool* _aidl_return) override {
    *_aidl_return = false;
    return ScopedAStatus::ok();
  }

 private:
  std::shared_ptr<INfcClientCallback> takeCallback() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto callback = mCallback;
    mCallback.reset();
    return callback;
  }

  // 对当前回调发事件；open 之前就来的请求没有回调，直接丢弃。
  void replyPending(NfcEvent event) {
    ALOGI("reply event=%d(FAILED) to pending callback",
          static_cast<int>(event));
    std::shared_ptr<INfcClientCallback> callback = takeCallback();
    if (callback != nullptr) {
      sendEventLater(callback, event, NfcStatus::FAILED);
    }
  }

  // 延迟一小段再发：调用方此刻正卡在 binder 调用里，事件必须等它进 wait。
  static void sendEventLater(const std::shared_ptr<INfcClientCallback>& callback,
                             NfcEvent event, NfcStatus status) {
    std::thread([callback, event, status] {
      std::this_thread::sleep_for(kEventDelay);
      ScopedAStatus result = callback->sendEvent(event, status);
      if (!result.isOk()) {
        ALOGE("sendEvent(event=%d) failed: %s", static_cast<int>(event),
              result.getDescription().c_str());
      }
    }).detach();
  }

  std::mutex mutex_;
  std::shared_ptr<INfcClientCallback> mCallback;
};

}  // namespace

int main() {
  ABinderProcess_setThreadPoolMaxThreadCount(2);
  auto svc = ndk::SharedRefBase::make<NfcService>();
  if (AServiceManager_addService(svc->asBinder().get(), kNfcServiceName) !=
      STATUS_OK) {
    return 2;
  }
  ABinderProcess_joinThreadPool();
  return 0;
}
