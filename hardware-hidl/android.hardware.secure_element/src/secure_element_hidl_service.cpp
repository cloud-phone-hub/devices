// engine/vendor/hardware —— Android 13 HIDL android.hardware.secure_element@1.2 自研服务。

#define LOG_TAG "CdhSecureElementHidl"

#include <android/hardware/secure_element/1.2/ISecureElement.h>
#include <hidl/HidlTransportSupport.h>
#include <log/log.h>

namespace {

using ::android::sp;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::secure_element::V1_0::LogicalChannelResponse;
using ::android::hardware::secure_element::V1_0::SecureElementStatus;
using V1_0_Cb = ::android::hardware::secure_element::V1_0::ISecureElementHalCallback;
using V1_1_Cb = ::android::hardware::secure_element::V1_1::ISecureElementHalCallback;
using ::android::hardware::secure_element::V1_2::ISecureElement;

class SecureElementImpl : public ISecureElement {
 public:
  Return<void> init(const sp<V1_0_Cb> &cb) override {
    if (cb != nullptr) {
      (void)cb->onStateChange(false).isOk();
    }
    return Void();
  }
  Return<void> init_1_1(const sp<V1_1_Cb> &cb) override {
    if (cb != nullptr) {
      (void)cb->onStateChange_1_1(false, "No secure element present").isOk();
    }
    return Void();
  }
  Return<void> getAtr(getAtr_cb _hidl_cb) override {
    hidl_vec<uint8_t> empty;
    _hidl_cb(empty);
    return Void();
  }
  Return<bool> isCardPresent() override { return false; }
  Return<void> transmit(const hidl_vec<uint8_t> &, transmit_cb _hidl_cb) override {
    hidl_vec<uint8_t> empty;
    _hidl_cb(empty);
    return Void();
  }
  Return<void> openLogicalChannel(const hidl_vec<uint8_t> &, uint8_t,
                                  openLogicalChannel_cb _hidl_cb) override {
    _hidl_cb(LogicalChannelResponse{}, SecureElementStatus::IOERROR);
    return Void();
  }
  Return<void> openBasicChannel(const hidl_vec<uint8_t> &, uint8_t,
                                openBasicChannel_cb _hidl_cb) override {
    hidl_vec<uint8_t> empty;
    _hidl_cb(empty, SecureElementStatus::IOERROR);
    return Void();
  }
  Return<SecureElementStatus> closeChannel(uint8_t) override {
    return SecureElementStatus::FAILED;
  }
  Return<SecureElementStatus> reset() override {
    return SecureElementStatus::SUCCESS;
  }
};

}  // namespace

int main() {
  ::android::hardware::configureRpcThreadpool(2, true);
  sp<ISecureElement> svc = new SecureElementImpl();
  if (svc->registerAsService("eSE1") != ::android::OK) {
    ALOGE("Failed to register ISecureElement@1.2/eSE1");
    return 1;
  }
  ::android::hardware::joinRpcThreadpool();
  return 1;
}
