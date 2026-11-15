// engine/vendor/hardware 线 —— android.hardware.power.stats（IPowerStats v2）自研服务实现。
//
// 注册 android.hardware.power.stats.IPowerStats/default。框架侧 PowerStatsService 会
// tryGetService 该实例；实体缺失时每次查询都回 NULL（servicemanager 每个 pid 打一条
// "Could not find … in the VINTF manifest"），BatteryStats / 功耗页面拿不到数据。
// 云机没有能量计（无 PMIC 计量通道、无 EnergyConsumer），故本服务是**如实上报空**的
// 实现：六个查询方法一律返回空数组，不宣称任何 PowerEntity/EnergyConsumer/Channel。
//
// 版本固定 v2：Android 16 的 FCM（202504）把 android.hardware.power.stats 精确钉在
// v2（v2 相对 v1 新增 getEnergyMeterInfo / readEnergyMeter，正是本文件末尾两个方法），
// v2 是唯一可满足冻结的版本。
//
// 不落 VINTF 片段（设备集成层职责）：该 HAL 在 m22 官方与 lineage 底座均未声明，
// 若模块自带分件，一旦集成层再补一条同 FqInstance 声明就会触发 libvintf
// "Conflicting FqInstance" → NULL VINTF MANIFEST。设备无关，不内置机型常量。

#include <android/binder_ibinder.h>
#include <android/binder_status.h>

extern "C" {
void ABinderProcess_setThreadPoolMaxThreadCount(uint32_t numThreads);
void ABinderProcess_joinThreadPool(void);
binder_status_t AServiceManager_addService(AIBinder *binder,
                                           const char *instance);
}

#include <aidl/android/hardware/power/stats/BnPowerStats.h>

#include <cstdint>
#include <memory>
#include <vector>

using aidl::android::hardware::power::stats::BnPowerStats;
using aidl::android::hardware::power::stats::Channel;
using aidl::android::hardware::power::stats::EnergyConsumer;
using aidl::android::hardware::power::stats::EnergyConsumerResult;
using aidl::android::hardware::power::stats::EnergyMeasurement;
using aidl::android::hardware::power::stats::PowerEntity;
using aidl::android::hardware::power::stats::StateResidencyResult;
using ndk::ScopedAStatus;

namespace {
constexpr const char *kPowerStatsServiceName =
    "android.hardware.power.stats.IPowerStats/default";
} // namespace

// 虚拟 power.stats：无能量计/无实体，六个查询方法一律返回空数组。
class PowerStatsService : public BnPowerStats {
public:
  ScopedAStatus getPowerEntityInfo(std::vector<PowerEntity> *out) override {
    out->clear();
    return ScopedAStatus::ok();
  }

  ScopedAStatus
  getStateResidency(const std::vector<int32_t> &powerEntityIds,
                    std::vector<StateResidencyResult> *out) override {
    (void)powerEntityIds;
    out->clear();
    return ScopedAStatus::ok();
  }

  ScopedAStatus getEnergyConsumerInfo(std::vector<EnergyConsumer> *out) override {
    out->clear();
    return ScopedAStatus::ok();
  }

  ScopedAStatus
  getEnergyConsumed(const std::vector<int32_t> &energyConsumerIds,
                    std::vector<EnergyConsumerResult> *out) override {
    (void)energyConsumerIds;
    out->clear();
    return ScopedAStatus::ok();
  }

  ScopedAStatus getEnergyMeterInfo(std::vector<Channel> *out) override {
    out->clear();
    return ScopedAStatus::ok();
  }

  ScopedAStatus readEnergyMeter(const std::vector<int32_t> &channelIds,
                                std::vector<EnergyMeasurement> *out) override {
    (void)channelIds;
    out->clear();
    return ScopedAStatus::ok();
  }
};

int main() {
  ABinderProcess_setThreadPoolMaxThreadCount(2);
  auto stats = ndk::SharedRefBase::make<PowerStatsService>();
  if (AServiceManager_addService(stats->asBinder().get(),
                                 kPowerStatsServiceName) != STATUS_OK) {
    return 2;
  }
  ABinderProcess_joinThreadPool();
  return 0;
}
