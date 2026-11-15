// engine/vendor/hardware 线 —— android.hardware.memtrack（IMemtrack v1）自研服务实现。
//
// 注册 android.hardware.memtrack.IMemtrack/default，供 framework memtrackproxy 统计
// 进程 GPU/图形内存。云端虚拟机没有厂商 GPU 计账后端，故按 AOSP example 语义返回空：
// getMemory 无附加记录、getGpuDeviceInfo 无 GPU 设备。设备无关，不内置机型常量。

#include <android/binder_ibinder.h>
#include <android/binder_status.h>

extern "C" {
void ABinderProcess_setThreadPoolMaxThreadCount(uint32_t numThreads);
void ABinderProcess_joinThreadPool(void);
binder_status_t AServiceManager_addService(AIBinder* binder,
                                           const char* instance);
}

#include <aidl/android/hardware/memtrack/BnMemtrack.h>
#include <aidl/android/hardware/memtrack/DeviceInfo.h>
#include <aidl/android/hardware/memtrack/MemtrackRecord.h>
#include <aidl/android/hardware/memtrack/MemtrackType.h>

#include <vector>

using aidl::android::hardware::memtrack::BnMemtrack;
using aidl::android::hardware::memtrack::DeviceInfo;
using aidl::android::hardware::memtrack::MemtrackRecord;
using aidl::android::hardware::memtrack::MemtrackType;
using ndk::ScopedAStatus;

static constexpr const char* kMemtrackServiceName =
    "android.hardware.memtrack.IMemtrack/default";

namespace {

class MemtrackService : public BnMemtrack {
 public:
  ScopedAStatus getMemory(int32_t /*pid*/, MemtrackType /*type*/,
                          std::vector<MemtrackRecord>* _aidl_return) override {
    // 无厂商计账：不追加任何记录（framework 视之为 0）。
    _aidl_return->clear();
    return ScopedAStatus::ok();
  }

  ScopedAStatus getGpuDeviceInfo(std::vector<DeviceInfo>* _aidl_return) override {
    // 云机无独立 GPU 设备计账，返回空设备列表。
    _aidl_return->clear();
    return ScopedAStatus::ok();
  }
};

}  // namespace

int main() {
  ABinderProcess_setThreadPoolMaxThreadCount(2);
  auto memtrack = ndk::SharedRefBase::make<MemtrackService>();
  if (AServiceManager_addService(memtrack->asBinder().get(),
                                 kMemtrackServiceName) != STATUS_OK) {
    return 2;
  }
  ABinderProcess_joinThreadPool();
  return 0;
}
