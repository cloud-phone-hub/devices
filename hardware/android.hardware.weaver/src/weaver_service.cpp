// engine/vendor/hardware 线 —— android.hardware.weaver（IWeaver v2）自研服务实现。
//
// 注册 android.hardware.weaver.IWeaver/default。Weaver 为锁屏凭据提供受限次数（rate-limit）
// 的安全槽位。云端虚拟机没有安全存储硬件：getConfig 报 0 个槽位，read 一律返回失败，
// write 接受调用但不落任何值。设备无关，不内置机型常量。

#include <android/binder_ibinder.h>
#include <android/binder_status.h>

extern "C" {
void ABinderProcess_setThreadPoolMaxThreadCount(uint32_t numThreads);
void ABinderProcess_joinThreadPool(void);
binder_status_t AServiceManager_addService(AIBinder* binder,
                                           const char* instance);
}

#include <aidl/android/hardware/weaver/BnWeaver.h>
#include <aidl/android/hardware/weaver/WeaverConfig.h>
#include <aidl/android/hardware/weaver/WeaverReadResponse.h>
#include <aidl/android/hardware/weaver/WeaverReadStatus.h>

#include <cstdint>
#include <vector>

using aidl::android::hardware::weaver::BnWeaver;
using aidl::android::hardware::weaver::WeaverConfig;
using aidl::android::hardware::weaver::WeaverReadResponse;
using aidl::android::hardware::weaver::WeaverReadStatus;
using ndk::ScopedAStatus;

static constexpr const char* kWeaverServiceName =
    "android.hardware.weaver.IWeaver/default";

namespace {

class WeaverService : public BnWeaver {
 public:
  ScopedAStatus getConfig(WeaverConfig* _aidl_return) override {
    _aidl_return->slots = 0;      // 无安全槽位
    _aidl_return->keySize = 0;
    _aidl_return->valueSize = 0;
    return ScopedAStatus::ok();
  }

  ScopedAStatus read(int32_t /*in_slotId*/,
                     const std::vector<uint8_t>& /*in_key*/,
                     WeaverReadResponse* _aidl_return) override {
    _aidl_return->timeout = 0;
    _aidl_return->value.clear();
    _aidl_return->status = WeaverReadStatus::FAILED;
    return ScopedAStatus::ok();
  }

  ScopedAStatus write(int32_t /*in_slotId*/,
                      const std::vector<uint8_t>& /*in_key*/,
                      const std::vector<uint8_t>& /*in_value*/) override {
    // 无槽位可写：接受调用但不产生任何状态。
    return ScopedAStatus::ok();
  }
};

}  // namespace

int main() {
  ABinderProcess_setThreadPoolMaxThreadCount(2);
  auto svc = ndk::SharedRefBase::make<WeaverService>();
  if (AServiceManager_addService(svc->asBinder().get(), kWeaverServiceName) !=
      STATUS_OK) {
    return 2;
  }
  ABinderProcess_joinThreadPool();
  return 0;
}
