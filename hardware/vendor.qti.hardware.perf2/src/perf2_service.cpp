// engine/vendor/hardware 线 —— vendor.qti.hardware.perf2（IPerf v1）自研服务实现。
//
// 注册 vendor.qti.hardware.perf2.IPerf/default（instance 名见设备侧 rc：
// interface aidl vendor.qti.hardware.perf2.IPerf/default）。
//
// 背景（3a-15 冷启动 ANR 根因）：官方 system 的 framework（BoostFramework →
// system_ext 的 vendor.perfservice）在 handleBindApplication 里同步连 QTI perf HAL；
// HAL 缺失时事务永久阻塞，app 主线程全部停在 IPerfManager.setClientBinder 上。
// 云机没有可调优的 CPU/GPU 频率策略，故本服务是**应答即成功的空实现**：
//   * 所有 int 返回方法立即返回 0（提示/锁/配置均 no-op）；
//   * perfGetProp 回显调用方给的默认值（等价于"属性不存在"）；
//   * perfSyncRequest 返回空串（无同步配置）；
//   * perfCallbackRegister 只登记回调 binder、永不回调；Deregister 清空。
// 设备无关，不内置机型常量；不落 rc/vintf（设备集成层职责，见模块 build.py）。

#include <android/binder_ibinder.h>
#include <android/binder_status.h>

extern "C" {
void ABinderProcess_setThreadPoolMaxThreadCount(uint32_t numThreads);
void ABinderProcess_joinThreadPool(void);
binder_status_t AServiceManager_addService(AIBinder* binder,
                                           const char* instance);
}

#include <aidl/vendor/qti/hardware/perf2/BnPerf.h>
#include <aidl/vendor/qti/hardware/perf2/IPerfCallback.h>

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

using aidl::vendor::qti::hardware::perf2::BnPerf;
using aidl::vendor::qti::hardware::perf2::IPerfCallback;
using ndk::ScopedAStatus;

static constexpr const char* kPerfServiceName =
    "vendor.qti.hardware.perf2.IPerf/default";

namespace {

class PerfService : public BnPerf {
 public:
  ScopedAStatus perfAsyncRequest(int32_t /*in_a*/,
                                 const std::string& /*in_b*/,
                                 const std::vector<int32_t>& /*in_c*/,
                                 int32_t* _aidl_return) override {
    *_aidl_return = 0;
    return ScopedAStatus::ok();
  }

  ScopedAStatus perfCallbackDeregister(
      const std::shared_ptr<IPerfCallback>& in_cb, int32_t /*in_a*/,
      int32_t* _aidl_return) override {
    {
      std::lock_guard<std::mutex> lock(mMutex);
      if (mCallback == in_cb) {
        mCallback = nullptr;
      }
    }
    *_aidl_return = 0;
    return ScopedAStatus::ok();
  }

  ScopedAStatus perfCallbackRegister(
      const std::shared_ptr<IPerfCallback>& in_cb, int32_t /*in_a*/,
      int32_t* _aidl_return) override {
    // 只保留引用：云机无 perf 事件来源，本服务永不回调。
    {
      std::lock_guard<std::mutex> lock(mMutex);
      mCallback = in_cb;
    }
    *_aidl_return = 0;
    return ScopedAStatus::ok();
  }

  ScopedAStatus perfEvent(int32_t /*in_a*/, const std::string& /*in_b*/,
                          const std::vector<int32_t>& /*in_c*/) override {
    return ScopedAStatus::ok();
  }

  ScopedAStatus perfGetFeedback(int32_t /*in_a*/,
                                const std::string& /*in_b*/,
                                const std::vector<int32_t>& /*in_c*/,
                                int32_t* _aidl_return) override {
    *_aidl_return = 0;
    return ScopedAStatus::ok();
  }

  ScopedAStatus perfGetProp(const std::string& /*in_key*/,
                            const std::string& in_default,
                            std::string* _aidl_return) override {
    // 等价于"属性未设置"：回显调用方的默认值。
    *_aidl_return = in_default;
    return ScopedAStatus::ok();
  }

  ScopedAStatus perfHint(int32_t /*in_a*/, const std::string& /*in_b*/,
                         int32_t /*in_c*/, int32_t /*in_d*/,
                         const std::vector<int32_t>& /*in_e*/,
                         int32_t* _aidl_return) override {
    *_aidl_return = 0;
    return ScopedAStatus::ok();
  }

  ScopedAStatus perfHintAcqRel(int32_t /*in_a*/, int32_t /*in_b*/,
                               const std::string& /*in_c*/,
                               int32_t /*in_d*/, int32_t /*in_e*/,
                               const std::vector<int32_t>& /*in_f*/,
                               int32_t* _aidl_return) override {
    *_aidl_return = 0;
    return ScopedAStatus::ok();
  }

  ScopedAStatus perfHintRenew(int32_t /*in_a*/, int32_t /*in_b*/,
                              const std::string& /*in_c*/,
                              int32_t /*in_d*/, int32_t /*in_e*/,
                              const std::vector<int32_t>& /*in_f*/,
                              int32_t* _aidl_return) override {
    *_aidl_return = 0;
    return ScopedAStatus::ok();
  }

  ScopedAStatus perfLockAcqAndRelease(
      int32_t /*in_a*/, int32_t /*in_b*/,
      const std::vector<int32_t>& /*in_c*/,
      const std::vector<int32_t>& /*in_d*/,
      int32_t* _aidl_return) override {
    *_aidl_return = 0;
    return ScopedAStatus::ok();
  }

  ScopedAStatus perfLockAcquire(int32_t /*in_a*/, int32_t /*in_b*/,
                                const std::vector<int32_t>& /*in_c*/,
                                const std::vector<int32_t>& /*in_d*/,
                                int32_t* _aidl_return) override {
    *_aidl_return = 0;
    return ScopedAStatus::ok();
  }

  ScopedAStatus perfLockCmd(int32_t /*in_a*/, int32_t /*in_b*/) override {
    return ScopedAStatus::ok();
  }

  ScopedAStatus perfLockRelease(
      int32_t /*in_a*/, const std::vector<int32_t>& /*in_b*/) override {
    return ScopedAStatus::ok();
  }

  ScopedAStatus perfProfile(int32_t /*in_a*/, int32_t /*in_b*/,
                            int32_t /*in_c*/,
                            int32_t* _aidl_return) override {
    *_aidl_return = 0;
    return ScopedAStatus::ok();
  }

  ScopedAStatus perfSetProp(const std::string& /*in_key*/,
                            const std::string& /*in_value*/,
                            int32_t* _aidl_return) override {
    *_aidl_return = 0;
    return ScopedAStatus::ok();
  }

  ScopedAStatus perfSyncRequest(int32_t /*in_a*/,
                                const std::string& /*in_b*/,
                                const std::vector<int32_t>& /*in_c*/,
                                std::string* _aidl_return) override {
    _aidl_return->clear();
    return ScopedAStatus::ok();
  }

 private:
  std::mutex mMutex;
  std::shared_ptr<IPerfCallback> mCallback;  // 只登记、永不回调
};

}  // namespace

int main() {
  ABinderProcess_setThreadPoolMaxThreadCount(2);
  auto svc = ndk::SharedRefBase::make<PerfService>();
  if (AServiceManager_addService(svc->asBinder().get(), kPerfServiceName) !=
      STATUS_OK) {
    return 2;
  }
  ABinderProcess_joinThreadPool();
  return 0;
}
