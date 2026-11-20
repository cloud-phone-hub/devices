// engine/vendor/hardware —— Android 13 HIDL android.hardware.boot@1.2 自研服务。

#define LOG_TAG "CdhBootHidl"

#include <android/hardware/boot/1.2/IBootControl.h>
#include <hidl/HidlTransportSupport.h>
#include <log/log.h>

namespace {

using ::android::sp;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::boot::V1_0::BoolResult;
using ::android::hardware::boot::V1_0::CommandResult;
using ::android::hardware::boot::V1_0::Slot;
using ::android::hardware::boot::V1_1::MergeStatus;
using ::android::hardware::boot::V1_2::IBootControl;

class BootControlImpl : public IBootControl {
 public:
  Return<uint32_t> getNumberSlots() override { return 2; }
  Return<Slot> getCurrentSlot() override { return 0; }
  Return<void> markBootSuccessful(markBootSuccessful_cb _hidl_cb) override {
    _hidl_cb(CommandResult{true, ""});
    return Void();
  }
  Return<void> setActiveBootSlot(Slot slot, setActiveBootSlot_cb _hidl_cb) override {
    if (slot < 2) {
      activeSlot_ = slot;
      _hidl_cb(CommandResult{true, ""});
    } else {
      _hidl_cb(CommandResult{false, "Invalid slot"});
    }
    return Void();
  }
  Return<void> setSlotAsUnbootable(Slot slot, setSlotAsUnbootable_cb _hidl_cb) override {
    _hidl_cb(CommandResult{slot < 2, slot < 2 ? "" : "Invalid slot"});
    return Void();
  }
  Return<BoolResult> isSlotBootable(Slot slot) override {
    return slot < 2 ? BoolResult::TRUE : BoolResult::INVALID_SLOT;
  }
  Return<BoolResult> isSlotMarkedSuccessful(Slot slot) override {
    return slot < 2 ? BoolResult::TRUE : BoolResult::INVALID_SLOT;
  }
  Return<void> getSuffix(Slot slot, getSuffix_cb _hidl_cb) override {
    if (slot == 0) {
      _hidl_cb("_a");
    } else if (slot == 1) {
      _hidl_cb("_b");
    } else {
      _hidl_cb("");
    }
    return Void();
  }
  Return<bool> setSnapshotMergeStatus(MergeStatus status) override {
    mergeStatus_ = status;
    return true;
  }
  Return<MergeStatus> getSnapshotMergeStatus() override { return mergeStatus_; }
  Return<Slot> getActiveBootSlot() override { return activeSlot_; }

 private:
  Slot activeSlot_ = 0;
  MergeStatus mergeStatus_ = MergeStatus::NONE;
};

}  // namespace

int main() {
  ::android::hardware::configureRpcThreadpool(2, true);
  sp<IBootControl> svc = new BootControlImpl();
  if (svc->registerAsService("default") != ::android::OK) {
    ALOGE("Failed to register IBootControl@1.2/default");
    return 1;
  }
  ::android::hardware::joinRpcThreadpool();
  return 1;
}
