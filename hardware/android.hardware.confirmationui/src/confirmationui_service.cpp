// engine/vendor/hardware 线 —— android.hardware.confirmationui（IConfirmationUI v1）自研服务实现。
//
// 注册 android.hardware.confirmationui.IConfirmationUI/default。ConfirmationUI 在受信任
// 显示/输入通路（TEE 上的图形栈）上向用户展示交易内容并回签一个 HardwareAuthToken。云端
// 虚拟机没有可信显示/输入子系统：
//   * abort：no-op（本就没有在途请求）；
//   * deliverSecureInputEvent：丢弃（无可信输入产生）；
//   * promptUserConfirmation：立即通过 resultCB 回一个 UNEXPECTED（UI 侧不可用）错误码，
//     不产出确认令牌。
// 设备无关，不内置机型常量。

#include <android/binder_ibinder.h>
#include <android/binder_status.h>

extern "C" {
void ABinderProcess_setThreadPoolMaxThreadCount(uint32_t numThreads);
void ABinderProcess_joinThreadPool(void);
binder_status_t AServiceManager_addService(AIBinder* binder,
                                           const char* instance);
}

#include <aidl/android/hardware/confirmationui/BnConfirmationUI.h>
#include <aidl/android/hardware/confirmationui/IConfirmationResultCallback.h>
#include <aidl/android/hardware/confirmationui/UIOption.h>
#include <aidl/android/hardware/security/keymint/HardwareAuthToken.h>

#include <cstdint>
#include <string>
#include <vector>

using aidl::android::hardware::confirmationui::BnConfirmationUI;
using aidl::android::hardware::confirmationui::IConfirmationResultCallback;
using aidl::android::hardware::confirmationui::UIOption;
using aidl::android::hardware::security::keymint::HardwareAuthToken;
using ndk::ScopedAStatus;

static constexpr const char* kConfirmationUIServiceName =
    "android.hardware.confirmationui.IConfirmationUI/default";

// IConfirmationUI 中声明的结果码（error 参数口径）。
static constexpr int32_t kResultUnexpected = 7;  // UNEXPECTED

namespace {

class ConfirmationUIService : public BnConfirmationUI {
 public:
  ScopedAStatus abort() override { return ScopedAStatus::ok(); }

  ScopedAStatus deliverSecureInputEvent(
      const HardwareAuthToken& /*in_secureInputToken*/) override {
    // 无可信输入通路，静默丢弃。
    return ScopedAStatus::ok();
  }

  ScopedAStatus promptUserConfirmation(
      const std::shared_ptr<IConfirmationResultCallback>& in_resultCB,
      const std::vector<uint8_t>& /*in_promptText*/,
      const std::vector<uint8_t>& in_extraData,
      const std::string& /*in_locale*/,
      const std::vector<UIOption>& /*in_uiOptions*/) override {
    if (in_resultCB) {
      // 无确认令牌，透传 extraData 供调用方关联。
      in_resultCB->result(kResultUnexpected, std::vector<uint8_t>(),
                          in_extraData);
    }
    return ScopedAStatus::ok();
  }
};

}  // namespace

int main() {
  ABinderProcess_setThreadPoolMaxThreadCount(2);
  auto svc = ndk::SharedRefBase::make<ConfirmationUIService>();
  if (AServiceManager_addService(svc->asBinder().get(),
                                 kConfirmationUIServiceName) != STATUS_OK) {
    return 2;
  }
  ABinderProcess_joinThreadPool();
  return 0;
}
