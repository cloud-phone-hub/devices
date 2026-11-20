// engine/vendor/hardware —— Android 13 HIDL android.hardware.fastboot@1.1 自研服务。

#define LOG_TAG "CdhFastbootHidl"

#include <android/hardware/fastboot/1.1/IFastboot.h>
#include <hidl/HidlTransportSupport.h>
#include <log/log.h>

namespace {

using ::android::sp;
using ::android::hardware::hidl_string;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::fastboot::V1_0::FileSystemType;
using ::android::hardware::fastboot::V1_0::Result;
using ::android::hardware::fastboot::V1_0::Status;
using ::android::hardware::fastboot::V1_1::IFastboot;

class FastbootImpl : public IFastboot {
 public:
  Return<void> getPartitionType(const hidl_string &, getPartitionType_cb _hidl_cb) override {
    _hidl_cb(FileSystemType::RAW, Result{Status::SUCCESS, ""});
    return Void();
  }
  Return<void> doOemCommand(const hidl_string &, doOemCommand_cb _hidl_cb) override {
    _hidl_cb(Result{Status::NOT_SUPPORTED, "Unsupported"});
    return Void();
  }
  Return<void> getVariant(getVariant_cb _hidl_cb) override {
    _hidl_cb("US", Result{Status::SUCCESS, ""});
    return Void();
  }
  Return<void> getOffModeChargeState(getOffModeChargeState_cb _hidl_cb) override {
    _hidl_cb(false, Result{Status::SUCCESS, ""});
    return Void();
  }
  Return<void> getBatteryVoltageFlashingThreshold(
      getBatteryVoltageFlashingThreshold_cb _hidl_cb) override {
    _hidl_cb(3500, Result{Status::SUCCESS, ""});
    return Void();
  }
  Return<void> doOemSpecificErase(doOemSpecificErase_cb _hidl_cb) override {
    _hidl_cb(Result{Status::SUCCESS, ""});
    return Void();
  }
};

}  // namespace

int main() {
  ::android::hardware::configureRpcThreadpool(2, true);
  sp<IFastboot> svc = new FastbootImpl();
  if (svc->registerAsService("default") != ::android::OK) {
    ALOGE("Failed to register IFastboot@1.1/default");
    return 1;
  }
  ::android::hardware::joinRpcThreadpool();
  return 1;
}
