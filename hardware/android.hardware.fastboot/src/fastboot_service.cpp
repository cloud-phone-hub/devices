// engine/vendor/hardware 线 —— android.hardware.fastboot（IFastboot v1）自研服务实现。
//
// 注册 android.hardware.fastboot.IFastboot/default，仅在 fastbootd 中被使用，向 PC 侧
// fastboot 暴露 OEM 命令 / 分区类型 / 变体字符串等。云端虚拟机没有真实刷写通路：flash
// 相关的 OEM 命令、擦除、分区类型查询一律返回不支持；电压阈值/离线充电等只读项返回安全
// 默认。设备无关，不内置机型常量。

#include <android/binder_ibinder.h>
#include <android/binder_status.h>

extern "C" {
void ABinderProcess_setThreadPoolMaxThreadCount(uint32_t numThreads);
void ABinderProcess_joinThreadPool(void);
binder_status_t AServiceManager_addService(AIBinder* binder,
                                           const char* instance);
}

#include <aidl/android/hardware/fastboot/BnFastboot.h>
#include <aidl/android/hardware/fastboot/FileSystemType.h>

#include <string>

using aidl::android::hardware::fastboot::BnFastboot;
using aidl::android::hardware::fastboot::FileSystemType;
using ndk::ScopedAStatus;

static constexpr const char* kFastbootServiceName =
    "android.hardware.fastboot.IFastboot/default";

namespace {

class FastbootService : public BnFastboot {
 public:
  ScopedAStatus doOemCommand(const std::string& /*in_oemCmd*/,
                             std::string* /*_aidl_return*/) override {
    return ScopedAStatus::fromExceptionCodeWithMessage(
        EX_UNSUPPORTED_OPERATION, "no fastboot OEM backend");
  }

  ScopedAStatus doOemSpecificErase() override {
    return ScopedAStatus::fromExceptionCodeWithMessage(
        EX_UNSUPPORTED_OPERATION, "no fastboot OEM backend");
  }

  ScopedAStatus getBatteryVoltageFlashingThreshold(
      int32_t* _aidl_return) override {
    *_aidl_return = 0;
    return ScopedAStatus::ok();
  }

  ScopedAStatus getOffModeChargeState(bool* _aidl_return) override {
    *_aidl_return = false;
    return ScopedAStatus::ok();
  }

  ScopedAStatus getPartitionType(const std::string& /*in_partitionName*/,
                                 FileSystemType* /*_aidl_return*/) override {
    return ScopedAStatus::fromExceptionCodeWithMessage(
        EX_UNSUPPORTED_OPERATION, "no fastboot partition table");
  }

  ScopedAStatus getVariant(std::string* _aidl_return) override {
    _aidl_return->clear();
    return ScopedAStatus::ok();
  }
};

}  // namespace

int main() {
  ABinderProcess_setThreadPoolMaxThreadCount(2);
  auto fb = ndk::SharedRefBase::make<FastbootService>();
  if (AServiceManager_addService(fb->asBinder().get(), kFastbootServiceName) !=
      STATUS_OK) {
    return 2;
  }
  ABinderProcess_joinThreadPool();
  return 0;
}
