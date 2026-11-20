// engine/vendor/hardware —— Android 13 HIDL android.hardware.confirmationui@1.0 自研服务。

#define LOG_TAG "CdhConfirmationUiHidl"

#include <android/hardware/confirmationui/1.0/IConfirmationUI.h>
#include <hidl/HidlTransportSupport.h>
#include <log/log.h>

namespace {

using ::android::sp;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::confirmationui::V1_0::IConfirmationResultCallback;
using ::android::hardware::confirmationui::V1_0::IConfirmationUI;
using ::android::hardware::confirmationui::V1_0::ResponseCode;
using ::android::hardware::confirmationui::V1_0::UIOption;
using ::android::hardware::keymaster::V4_0::HardwareAuthToken;

class ConfirmationUIImpl : public IConfirmationUI {
 public:
  Return<ResponseCode> promptUserConfirmation(
      const sp<IConfirmationResultCallback> &, const hidl_string &,
      const hidl_vec<uint8_t> &, const hidl_string &, const hidl_vec<UIOption> &) override {
    return ResponseCode::Unimplemented;
  }
  Return<ResponseCode> deliverSecureInputEvent(const HardwareAuthToken &) override {
    return ResponseCode::Ignored;
  }
  Return<void> abort() override { return Void(); }
};

}  // namespace

int main() {
  ::android::hardware::configureRpcThreadpool(2, true);
  sp<IConfirmationUI> svc = new ConfirmationUIImpl();
  if (svc->registerAsService("default") != ::android::OK) {
    ALOGE("Failed to register IConfirmationUI@1.0/default");
    return 1;
  }
  ::android::hardware::joinRpcThreadpool();
  return 1;
}
