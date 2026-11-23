// android.hardware.power@qti 变体扩展 —— 高通 IPerf v1 + IServicetracker v1 自研服务实现。
//
// 由 android.hardware.power-service（域 hal_power_default）同进程注册：
//   - vendor.qti.hardware.perf2.IPerf/default
//   - vendor.qti.hardware.servicetrackeraidl.IServicetracker/default
// 只在 --variant qti（-DCDH_POWER_QTI=1）时编入，default 变体零高通符号。

#include "power_qti_ext.h"

#include <android/binder_ibinder.h>
#include <android/binder_status.h>
#include <android/log.h>

extern "C" {
binder_status_t AServiceManager_addService(AIBinder* binder,
                                           const char* instance);
}

#include <aidl/vendor/qti/hardware/perf2/BnPerf.h>
#include <aidl/vendor/qti/hardware/perf2/IPerfCallback.h>
#include <aidl/vendor/qti/hardware/servicetrackeraidl/BnServicetracker.h>

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace cdh::power {
namespace {

using aidl::vendor::qti::hardware::perf2::BnPerf;
using aidl::vendor::qti::hardware::perf2::IPerfCallback;
using aidl::vendor::qti::hardware::servicetrackeraidl::BnServicetracker;
using aidl::vendor::qti::hardware::servicetrackeraidl::ClientConnection;
using aidl::vendor::qti::hardware::servicetrackeraidl::ClientData;
using aidl::vendor::qti::hardware::servicetrackeraidl::ClientRecord;
using aidl::vendor::qti::hardware::servicetrackeraidl::ServiceConnection;
using aidl::vendor::qti::hardware::servicetrackeraidl::ServiceData;
using aidl::vendor::qti::hardware::servicetrackeraidl::ServiceRecord;
using ndk::ScopedAStatus;

constexpr const char* kLogTag = "cdh-power-qti";
constexpr const char* kPerfServiceName =
    "vendor.qti.hardware.perf2.IPerf/default";
constexpr const char* kServicetrackerServiceName =
    "vendor.qti.hardware.servicetrackeraidl.IServicetracker/default";

// 云机无 CPU/GPU 调频后端，IPerf 所有提示/锁/配置方法均为立即返回 0 的 no-op。
class PerfService final : public BnPerf {
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
  std::shared_ptr<IPerfCallback> mCallback;
};

// 高通服务跟踪 HAL 空实现：上报与 killProcess 均 no-op（绝不真的 kill），查询返回空/-1/false。
class ServicetrackerService final : public BnServicetracker {
 public:
  ScopedAStatus bindService(const ServiceData& /*service*/,
                            const ClientData& /*client*/) override {
    return ScopedAStatus::ok();
  }

  ScopedAStatus destroyService(const ServiceData& /*service*/) override {
    return ScopedAStatus::ok();
  }

  ScopedAStatus getClientConnections(
      const std::string& /*client*/,
      std::vector<ClientConnection>* connections) override {
    connections->clear();
    return ScopedAStatus::ok();
  }

  ScopedAStatus getPid(const std::string& /*service*/,
                       int32_t* _aidl_return) override {
    *_aidl_return = -1;
    return ScopedAStatus::ok();
  }

  ScopedAStatus getPids(const std::vector<std::string>& services,
                        std::vector<int32_t>* pids) override {
    pids->assign(services.size(), -1);
    return ScopedAStatus::ok();
  }

  ScopedAStatus getRunningServicePid(std::vector<int32_t>* pids) override {
    pids->clear();
    return ScopedAStatus::ok();
  }

  ScopedAStatus getServiceBCount(std::vector<ServiceRecord>* services,
                                 int32_t* _aidl_return) override {
    services->clear();
    *_aidl_return = 0;
    return ScopedAStatus::ok();
  }

  ScopedAStatus getServiceConnections(
      const std::string& /*service*/,
      std::vector<ServiceConnection>* connections) override {
    connections->clear();
    return ScopedAStatus::ok();
  }

  ScopedAStatus getclientInfo(const std::string& client,
                              ClientRecord* record) override {
    *record = ClientRecord{};
    record->clientPackage = client;
    return ScopedAStatus::ok();
  }

  ScopedAStatus getserviceInfo(const std::string& service,
                               ServiceRecord* record) override {
    *record = ServiceRecord{};
    record->serviceName = service;
    return ScopedAStatus::ok();
  }

  ScopedAStatus isServiceB(const std::string& /*service*/,
                           bool* _aidl_return) override {
    *_aidl_return = false;
    return ScopedAStatus::ok();
  }

  ScopedAStatus killProcess(int32_t /*pid*/) override {
    return ScopedAStatus::ok();
  }

  ScopedAStatus startService(const ServiceData& /*service*/) override {
    return ScopedAStatus::ok();
  }

  ScopedAStatus unbindService(const ServiceData& /*service*/,
                              const ClientData& /*client*/) override {
    return ScopedAStatus::ok();
  }
};

}  // namespace

std::vector<std::shared_ptr<ndk::ICInterface>> RegisterQtiServices() {
  std::vector<std::shared_ptr<ndk::ICInterface>> keepAlive;
  auto perf = ndk::SharedRefBase::make<PerfService>();
  if (AServiceManager_addService(perf->asBinder().get(), kPerfServiceName) ==
      STATUS_OK) {
    keepAlive.push_back(std::move(perf));
  } else {
    __android_log_print(ANDROID_LOG_WARN, kLogTag,
                        "addService(%s) failed", kPerfServiceName);
  }
  auto tracker = ndk::SharedRefBase::make<ServicetrackerService>();
  if (AServiceManager_addService(tracker->asBinder().get(),
                                 kServicetrackerServiceName) == STATUS_OK) {
    keepAlive.push_back(std::move(tracker));
  } else {
    __android_log_print(ANDROID_LOG_WARN, kLogTag,
                        "addService(%s) failed", kServicetrackerServiceName);
  }
  return keepAlive;
}

}  // namespace cdh::power
