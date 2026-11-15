// engine/vendor/hardware 线 —— android.hardware.boot（IBootControl v1）自研服务实现。
//
// 注册 android.hardware.boot.IBootControl/default，供 update_verifier / bootable 检查等
// 读取 A/B 槽位状态。云端虚拟机不做真正的 A/B 切换：以固定「槽 0 激活且成功、两槽可用」
// 的口径返回，mark/set 类写操作一律 no-op 成功。设备无关，不内置机型常量。

#include <android/binder_ibinder.h>
#include <android/binder_status.h>

extern "C" {
void ABinderProcess_setThreadPoolMaxThreadCount(uint32_t numThreads);
void ABinderProcess_joinThreadPool(void);
binder_status_t AServiceManager_addService(AIBinder* binder,
                                           const char* instance);
}

#include <aidl/android/hardware/boot/BnBootControl.h>
#include <aidl/android/hardware/boot/MergeStatus.h>

#include <string>

using aidl::android::hardware::boot::BnBootControl;
using aidl::android::hardware::boot::MergeStatus;
using ndk::ScopedAStatus;

static constexpr const char* kBootServiceName =
    "android.hardware.boot.IBootControl/default";

namespace {

class BootControlService : public BnBootControl {
 public:
  ScopedAStatus getActiveBootSlot(int32_t* _aidl_return) override {
    *_aidl_return = 0;
    return ScopedAStatus::ok();
  }

  ScopedAStatus getCurrentSlot(int32_t* _aidl_return) override {
    *_aidl_return = 0;
    return ScopedAStatus::ok();
  }

  ScopedAStatus getNumberSlots(int32_t* _aidl_return) override {
    *_aidl_return = 2;
    return ScopedAStatus::ok();
  }

  ScopedAStatus getSnapshotMergeStatus(MergeStatus* _aidl_return) override {
    *_aidl_return = MergeStatus::NONE;
    return ScopedAStatus::ok();
  }

  ScopedAStatus getSuffix(int32_t slot, std::string* _aidl_return) override {
    *_aidl_return = (slot == 0) ? "_a" : (slot == 1 ? "_b" : "");
    return ScopedAStatus::ok();
  }

  ScopedAStatus isSlotBootable(int32_t /*slot*/, bool* _aidl_return) override {
    *_aidl_return = true;
    return ScopedAStatus::ok();
  }

  ScopedAStatus isSlotMarkedSuccessful(int32_t /*slot*/,
                                       bool* _aidl_return) override {
    *_aidl_return = true;
    return ScopedAStatus::ok();
  }

  ScopedAStatus markBootSuccessful() override { return ScopedAStatus::ok(); }

  ScopedAStatus setActiveBootSlot(int32_t /*slot*/) override {
    return ScopedAStatus::ok();
  }

  ScopedAStatus setSlotAsUnbootable(int32_t /*slot*/) override {
    return ScopedAStatus::ok();
  }

  ScopedAStatus setSnapshotMergeStatus(MergeStatus /*status*/) override {
    return ScopedAStatus::ok();
  }
};

}  // namespace

int main() {
  ABinderProcess_setThreadPoolMaxThreadCount(2);
  auto boot = ndk::SharedRefBase::make<BootControlService>();
  if (AServiceManager_addService(boot->asBinder().get(), kBootServiceName) !=
      STATUS_OK) {
    return 2;
  }
  ABinderProcess_joinThreadPool();
  return 0;
}
