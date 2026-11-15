// engine/vendor/hardware 线 —— android.hardware.power（IPower v5）自研服务实现。
//
// 注册 android.hardware.power.IPower/default。Power HAL 全是性能提示（hint/boost）通道，
// 由 HAL 后端把提示翻译成 DVFS/EPP 等调优动作。云端虚拟机没有可调优的 CPU/GPU 频率策略：
//   * setMode / setBoost / closeSessionChannel：接受即忽略（no-op OK）；
//   * isModeSupported / isBoostSupported：一律报不支持（false）；
//   * createHintSession*：返回一个 all-no-op 的 IPowerHintSession 句柄，getHintSessionPreferredRate
//     返回 0（表示无提示速率约束）；
//   * getSessionChannel：无 FMQ 通道，返回不支持。
// 设备无关，不内置机型常量。

#include <android/binder_ibinder.h>
#include <android/binder_status.h>

extern "C" {
void ABinderProcess_setThreadPoolMaxThreadCount(uint32_t numThreads);
void ABinderProcess_joinThreadPool(void);
binder_status_t AServiceManager_addService(AIBinder* binder,
                                           const char* instance);
}

#include <aidl/android/hardware/power/Boost.h>
#include <aidl/android/hardware/power/BnPower.h>
#include <aidl/android/hardware/power/BnPowerHintSession.h>
#include <aidl/android/hardware/power/ChannelConfig.h>
#if CDH_POWER_AIDL_VERSION >= 6
#include <aidl/android/hardware/power/CompositionData.h>
#include <aidl/android/hardware/power/CompositionUpdate.h>
#include <aidl/android/hardware/power/CpuHeadroomParams.h>
#include <aidl/android/hardware/power/CpuHeadroomResult.h>
#include <aidl/android/hardware/power/GpuHeadroomParams.h>
#include <aidl/android/hardware/power/GpuHeadroomResult.h>
#include <aidl/android/hardware/power/SupportInfo.h>
#endif
#include <aidl/android/hardware/power/IPowerHintSession.h>
#include <aidl/android/hardware/power/Mode.h>
#include <aidl/android/hardware/power/SessionConfig.h>
#include <aidl/android/hardware/power/SessionHint.h>
#include <aidl/android/hardware/power/SessionMode.h>
#include <aidl/android/hardware/power/SessionTag.h>
#include <aidl/android/hardware/power/WorkDuration.h>

#include <cstdint>
#include <memory>
#include <vector>

using aidl::android::hardware::power::Boost;
using aidl::android::hardware::power::BnPower;
using aidl::android::hardware::power::BnPowerHintSession;
using aidl::android::hardware::power::ChannelConfig;
#if CDH_POWER_AIDL_VERSION >= 6
using aidl::android::hardware::power::CompositionData;
using aidl::android::hardware::power::CompositionUpdate;
using aidl::android::hardware::power::CpuHeadroomParams;
using aidl::android::hardware::power::CpuHeadroomResult;
using aidl::android::hardware::power::GpuHeadroomParams;
using aidl::android::hardware::power::GpuHeadroomResult;
using aidl::android::hardware::power::SupportInfo;
#endif
using aidl::android::hardware::power::IPowerHintSession;
using aidl::android::hardware::power::Mode;
using aidl::android::hardware::power::SessionConfig;
using aidl::android::hardware::power::SessionHint;
using aidl::android::hardware::power::SessionMode;
using aidl::android::hardware::power::SessionTag;
using aidl::android::hardware::power::WorkDuration;
using ndk::ScopedAStatus;

static constexpr const char* kPowerServiceName =
    "android.hardware.power.IPower/default";

namespace {

// 提示会话：所有上报/控制均为 no-op；getSessionConfig 返回一份带 id 的默认配置。
class PowerHintSession : public BnPowerHintSession {
 public:
  explicit PowerHintSession(int64_t id) : mId(id) {}

  ScopedAStatus updateTargetWorkDuration(int64_t /*in_targetDurationNanos*/) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus reportActualWorkDuration(
      const std::vector<WorkDuration>& /*in_durations*/) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus pause() override { return ScopedAStatus::ok(); }
  ScopedAStatus resume() override { return ScopedAStatus::ok(); }
  ScopedAStatus close() override { return ScopedAStatus::ok(); }
  ScopedAStatus sendHint(SessionHint /*in_hint*/) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus setThreads(
      const std::vector<int32_t>& /*in_threadIds*/) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus setMode(SessionMode /*in_type*/,
                        bool /*in_enabled*/) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus getSessionConfig(SessionConfig* _aidl_return) override {
    _aidl_return->id = mId;
    return ScopedAStatus::ok();
  }

 private:
  int64_t mId;
};

class PowerService : public BnPower {
 public:
  ScopedAStatus setMode(Mode /*in_type*/, bool /*in_enabled*/) override {
    return ScopedAStatus::ok();
  }

  ScopedAStatus isModeSupported(Mode /*in_type*/,
                                bool* _aidl_return) override {
    *_aidl_return = false;
    return ScopedAStatus::ok();
  }

  ScopedAStatus setBoost(Boost /*in_type*/,
                         int32_t /*in_durationMs*/) override {
    return ScopedAStatus::ok();
  }

  ScopedAStatus isBoostSupported(Boost /*in_type*/,
                                 bool* _aidl_return) override {
    *_aidl_return = false;
    return ScopedAStatus::ok();
  }

  ScopedAStatus createHintSession(
      int32_t /*in_tgid*/, int32_t /*in_uid*/,
      const std::vector<int32_t>& /*in_threadIds*/,
      int64_t /*in_durationNanos*/,
      std::shared_ptr<IPowerHintSession>* _aidl_return) override {
    *_aidl_return = ndk::SharedRefBase::make<PowerHintSession>(0);
    return ScopedAStatus::ok();
  }

  ScopedAStatus getHintSessionPreferredRate(
      int64_t* _aidl_return) override {
    *_aidl_return = 0;
    return ScopedAStatus::ok();
  }

  ScopedAStatus createHintSessionWithConfig(
      int32_t /*in_tgid*/, int32_t /*in_uid*/,
      const std::vector<int32_t>& /*in_threadIds*/,
      int64_t /*in_durationNanos*/, SessionTag /*in_tag*/,
      SessionConfig* config,
      std::shared_ptr<IPowerHintSession>* _aidl_return) override {
    static int64_t nextId = 1;
    const int64_t id = nextId++;
    config->id = id;
    *_aidl_return = ndk::SharedRefBase::make<PowerHintSession>(id);
    return ScopedAStatus::ok();
  }

  ScopedAStatus getSessionChannel(int32_t /*in_tgid*/, int32_t /*in_uid*/,
                                  ChannelConfig* /*_aidl_return*/) override {
    return ScopedAStatus::fromExceptionCodeWithMessage(
        EX_UNSUPPORTED_OPERATION, "no session channel / FMQ");
  }

  ScopedAStatus closeSessionChannel(int32_t /*in_tgid*/,
                                    int32_t /*in_uid*/) override {
    return ScopedAStatus::ok();
  }

  // ---- AIDL v6 新增：帧率余量 / 合成数据上报 ----
  // 虚拟机里 CPU/GPU 没有可供调度的频率空间，余量统一报 0（0% 余量）；
  // 合成数据上报直接接收即丢，不影响渲染。
  // CpuHeadroomResult / GpuHeadroomResult 是 AIDL union，默认构造即
  // globalHeadroom=0。
#if CDH_POWER_AIDL_VERSION >= 6
  ScopedAStatus getCpuHeadroom(const CpuHeadroomParams& /*in_params*/,
                               CpuHeadroomResult* _aidl_return) override {
    *_aidl_return = CpuHeadroomResult();
    return ScopedAStatus::ok();
  }

  ScopedAStatus getGpuHeadroom(const GpuHeadroomParams& /*in_params*/,
                               GpuHeadroomResult* _aidl_return) override {
    *_aidl_return = GpuHeadroomResult();
    return ScopedAStatus::ok();
  }

  ScopedAStatus sendCompositionData(
      const std::vector<CompositionData>& /*in_data*/) override {
    return ScopedAStatus::ok();
  }

  ScopedAStatus sendCompositionUpdate(
      const CompositionUpdate& /*in_update*/) override {
    return ScopedAStatus::ok();
  }

  // getSupportInfo 必须返回非 null 的 SupportInfo，而且 headroom 不能留空：
  // Flyme/MTK 的 com.android.server.power.hint.HintManagerService 构造函数直接读
  // supportInfo.headroom；AIDL 默认实现只置错误码不写出参，framework 侧拿到就是 null
  // → NPE 打死 system_server（zygote/system_server 无限重启）。
  ScopedAStatus getSupportInfo(SupportInfo* _aidl_return) override {
    SupportInfo info;
    info.usesSessions = false;
    info.boosts = 0;
    info.modes = 0;
    info.sessionHints = 0;
    info.sessionModes = 0;
    info.sessionTags = 0;
    // HeadroomSupportInfo 是 SupportInfo 的嵌套 parcelable：CPU/GPU 帧率余量查询在
    // 虚拟机上没有意义，两个 isXxxSupported 都报 false。
    info.headroom.isCpuSupported = false;
    info.headroom.isGpuSupported = false;
    info.headroom.cpuMinIntervalMillis = 0;
    info.headroom.gpuMinIntervalMillis = 0;
    *_aidl_return = info;
    return ScopedAStatus::ok();
  }
#endif
};

}  // namespace

int main() {
  ABinderProcess_setThreadPoolMaxThreadCount(2);
  auto svc = ndk::SharedRefBase::make<PowerService>();
  if (AServiceManager_addService(svc->asBinder().get(), kPowerServiceName) !=
      STATUS_OK) {
    return 2;
  }
  ABinderProcess_joinThreadPool();
  return 0;
}
