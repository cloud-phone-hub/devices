// engine/vendor/hardware 线 —— vendor.meizu.sensor_aux.ISensorAux **桩**服务实现。
//
// 存在的唯一理由：Flyme framework 的 libsensorservice（system_server 内）在
// android::Listener::onSampleResult 里**同步** AServiceManager_getService
// 取 vendor.meizu.sensor_aux.ISensorAux/default。该 HAL 的实现随官方 vendor 被替换成
// lineage virtio 底包后消失 ⇒ 客户端每轮 "Waiting for service…" 睡满 5 s 才拿到 NULL；
// 而这条回调正是它所在 binder 节点的 oneway 闸门持有者（内核里 oneway 按节点串行，
// 闸门只在 BC_FREE_BUFFER 开一格）⇒ 放行上限 1/5 s ≈ 0.2 对/s，低于
// surfaceflinger 的 IRegionSamplingListener 成对发射速率 ≈2 对/s ⇒ 异步预算单调填到
// 天花板 ⇒ 全系统发往 system_server 的 oneway 报 -ENOSPC(-28) → networkstack 自杀 →
// AOSP 故意崩 system_server → persistent 崩溃风暴。
//
// 因此本桩的验收指标只有一条：**持闸时长**。十个方法都不碰任何外部资源、就地写零并回
// OK，单次事务在微秒级 ⇒ 闸门从"每 5 s 开一格"变回随叫随到。
//
// 为什么不带官方实现：官方 updateRGBAvg / getColor / sensorAuxCtrl / setSensorAuxConfig
// 四个方法体走 SSC(ADSP) QMI over qipcrtr，云机没有该节点且方法体内没有任何超时常量
// ⇒ 等于用未知持闸时长替换已知的 5 s，而判据恰恰是持闸时长。
// 接口面（方法顺序 = transact code 1..10、参数类型、@nullable 位置）来自对官方
// vendor.meizu.sensor_aux-V1-ndk.so 与实现库的静态取证，见
// devices/flyme/m22/build/doc/evidence/issue-m22-sf-oneway-async-saturation。
//
// 不落 VINTF 片段：客户端用的是 native getService（不查 VINTF），servicemanager 的
// add/find 只看 service_contexts 标签；声明一个不在 FCM 里的包反而引入 VINTF 校验面。
// 设备无关，不内置机型常量。

#include <android/binder_ibinder.h>
#include <android/binder_status.h>

extern "C" {
void ABinderProcess_setThreadPoolMaxThreadCount(uint32_t numThreads);
void ABinderProcess_joinThreadPool(void);
binder_status_t AServiceManager_addService(AIBinder *binder,
                                           const char *instance);
}

#include <aidl/vendor/meizu/sensor_aux/BnSensorAux.h>

#include <android/log.h>

#include <cstdint>
#include <memory>
#include <optional>

using aidl::vendor::meizu::sensor_aux::BnSensorAux;
using aidl::vendor::meizu::sensor_aux::SAuxStatus;
using aidl::vendor::meizu::sensor_aux::ScreenDataProviderWorkMode;
using aidl::vendor::meizu::sensor_aux::SensorAuxArgs;
using aidl::vendor::meizu::sensor_aux::SensorAuxConfig;
using aidl::vendor::meizu::sensor_aux::SensorAuxDisplayInfo;
using ndk::ScopedAStatus;

namespace {
constexpr const char *kTag = "cdh-sensor-aux";

// system_server 侧 Listener::onSampleResult 查询的实例名（与机型 service_contexts
// 的标签、官方 manifest 的 fqname 三处一致；写错即等于没修）。
constexpr const char *kSensorAuxServiceName =
    "vendor.meizu.sensor_aux.ISensorAux/default";
} // namespace

class SensorAuxService : public BnSensorAux {
public:
  // 1
  ScopedAStatus getColor(int32_t *_aidl_return) override {
    *_aidl_return = 0;
    return ScopedAStatus::ok();
  }

  // 2：回 absent optional ⇒ 线上写 presence=0，客户端读到 0 即不再读字段
  ScopedAStatus getDisplayInfo(
      std::optional<SensorAuxDisplayInfo> *_aidl_return) override {
    _aidl_return->reset();
    return ScopedAStatus::ok();
  }

  // 3：桩不回调 provider，收下即回 0（不持有引用，避免与 system_server 形成引用环）
  ScopedAStatus registerScreenDataProvider(
      const std::shared_ptr<
          aidl::vendor::meizu::sensor_aux::IScreenDataProviderCallback>
          &in_provider,
      int32_t *_aidl_return) override {
    (void)in_provider;
    *_aidl_return = 0;
    return ScopedAStatus::ok();
  }

  // 4
  ScopedAStatus sensorAuxCtrl(bool in_enable, int32_t *_aidl_return) override {
    (void)in_enable;
    *_aidl_return = 0;
    return ScopedAStatus::ok();
  }

  // 5
  ScopedAStatus sensorAuxExecuteCmd(
      int32_t in_cmd, const std::optional<SensorAuxArgs> &in_args,
      int32_t *_aidl_return) override {
    (void)in_cmd;
    (void)in_args;
    *_aidl_return = 0;
    return ScopedAStatus::ok();
  }

  // 6
  ScopedAStatus setSensorAuxConfig(
      const std::optional<SensorAuxConfig> &in_config,
      int32_t *_aidl_return) override {
    (void)in_config;
    *_aidl_return = 0;
    return ScopedAStatus::ok();
  }

  // 7
  ScopedAStatus unregisterScreenDataProvider(int32_t in_token,
                                             SAuxStatus *_aidl_return) override {
    (void)in_token;
    *_aidl_return = SAuxStatus::OK;
    return ScopedAStatus::ok();
  }

  // 8：热路径（区域采样 → 平均色）。必须无条件秒回。
  ScopedAStatus updateRGBAvg(int32_t in_value1, int32_t in_value2,
                             SAuxStatus *_aidl_return) override {
    (void)in_value1;
    (void)in_value2;
    *_aidl_return = SAuxStatus::OK;
    return ScopedAStatus::ok();
  }

  // 9
  ScopedAStatus updateSensorStatus(int32_t in_value1, bool in_flag,
                                   SAuxStatus *_aidl_return) override {
    (void)in_value1;
    (void)in_flag;
    *_aidl_return = SAuxStatus::OK;
    return ScopedAStatus::ok();
  }

  // 10
  ScopedAStatus writeFPSToPromixity(float in_fps) override {
    (void)in_fps;
    return ScopedAStatus::ok();
  }
};

int main() {
  ABinderProcess_setThreadPoolMaxThreadCount(2);
  auto service = ndk::SharedRefBase::make<SensorAuxService>();
  // asBinder() 返回临时 SpAIBinder：AIBinder 只在**本条完整表达式**内保活，裸指针必须与
  // addService 同处一句（先赋给变量再用会得到悬垂指针，实测 SIGSEGV）。
  binder_status_t status =
      AServiceManager_addService(service->asBinder().get(), kSensorAuxServiceName);
  if (status != STATUS_OK) {
    __android_log_print(ANDROID_LOG_ERROR, kTag, "addService(%s) failed: %d",
                        kSensorAuxServiceName, status);
    return 2;
  }
  __android_log_print(ANDROID_LOG_INFO, kTag, "%s registered",
                      kSensorAuxServiceName);
  ABinderProcess_joinThreadPool();
  return 0;
}
