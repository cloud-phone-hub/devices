// engine/vendor/hardware 线 —— vendor.meizu.hardware.meizu_charging（IMeizuCharging v1）
// 自研服务实现，替换原厂预编译二进制
//（vendor/bin/hw/vendor.meizu.hardware.meizu_charging-service）。
//
// 背景：MEIZU 私有 HAL（原厂编译自 vendor/meizu/proprietary/meizu_charging/），
// AOSP/LineageOS 无公开源码。接口按原厂 lib64/vendor.meizu.hardware.meizu_charging-V1-ndk.so
// 反解（11 条同步事务 + 6 个枚举入参），见 IMeizuCharging.aidl 头注释。
//
// 原厂语义：读写 /sys/class/meizu/charger/* 与 /sys/class/power_supply/battery/
// charge_control_end_threshold、/sys/class/qcom-battery/charge_control_en 等节点，
// 实现充电上限、SOH、UPS 模式、反向无线充电、无线档位与风扇模式。
//
// 云机语义：virtio 底座没有 Meizu 充电控制器，也没有上述 sysfs 节点，故：
//   * set 系列只把值记进内存并回 true（不写任何 sysfs），get 系列回显最近一次设置，
//     未设置时给「未知」值（SOH=-1、阈值/控制=0）。echo 而不是恒定额，是为了让
//     Flyme「设置充电上限 → 回读校验」的流程自洽，不触发重试或报错；
//   * getWirelessTxModel 回空串（没有发射端）；
//   * 不 open 任何设备节点，故策略里不含 sysfs / /dev 授权。
// 设备无关，不内置机型常量；rc / VINTF 在模块 vendor/ 静态树里。

#include <android/binder_auto_utils.h>
#include <android/binder_ibinder.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <android/binder_status.h>

#include <aidl/vendor/meizu/hardware/meizu_charging/BnMeizuCharging.h>

#include <cstdint>
#include <memory>
#include <string>

using aidl::vendor::meizu::hardware::meizu_charging::BatteryUPSMode;
using aidl::vendor::meizu::hardware::meizu_charging::BnMeizuCharging;
using aidl::vendor::meizu::hardware::meizu_charging::CharingThresholdCtrl;
using aidl::vendor::meizu::hardware::meizu_charging::ReverseWirelssChargingMode;
using aidl::vendor::meizu::hardware::meizu_charging::WiredCharingLevel;
using aidl::vendor::meizu::hardware::meizu_charging::WirelessChargingLevel;
using aidl::vendor::meizu::hardware::meizu_charging::WirelessTxFanMode;
using ndk::ScopedAStatus;

namespace {

// 必须与模块 vendor/etc/init/*.rc 的 interface 行、VINTF 的 <fqname> 逐字一致。
constexpr char kServiceName[] = "vendor.meizu.hardware.meizu_charging.IMeizuCharging/default";

class MeizuCharging final : public BnMeizuCharging {
  public:
    // code 1：无线发射端型号。云机没有发射端。
    ScopedAStatus getWirelessTxModel(std::string* _aidl_return) override {
        *_aidl_return = tx_model_;
        return ScopedAStatus::ok();
    }

    // code 2：反向无线充电模式。
    ScopedAStatus setReverseWirelssChargingMode(ReverseWirelssChargingMode mode,
                                                bool* _aidl_return) override {
        reverse_mode_ = mode;
        *_aidl_return = true;
        return ScopedAStatus::ok();
    }

    // code 3：有线充电档位。
    ScopedAStatus setWiredCharingLevel(WiredCharingLevel level, bool* _aidl_return) override {
        wired_level_ = level;
        *_aidl_return = true;
        return ScopedAStatus::ok();
    }

    // code 4：无线充电档位。
    ScopedAStatus setWirelessChargingLevel(WirelessChargingLevel level,
                                           bool* _aidl_return) override {
        wireless_level_ = level;
        *_aidl_return = true;
        return ScopedAStatus::ok();
    }

    // code 5：无线发射端风扇模式。
    ScopedAStatus setWirelessTxFanMode(WirelessTxFanMode mode, bool* _aidl_return) override {
        fan_mode_ = mode;
        *_aidl_return = true;
        return ScopedAStatus::ok();
    }

    // code 6：电池 SOH。无电池模型 → -1（未知）。
    ScopedAStatus getBatterySOH(int32_t* _aidl_return) override {
        *_aidl_return = soh_;
        return ScopedAStatus::ok();
    }

    // code 7：UPS 模式。
    ScopedAStatus setBatteryUPSMode(BatteryUPSMode mode, bool* _aidl_return) override {
        ups_mode_ = mode;
        *_aidl_return = true;
        return ScopedAStatus::ok();
    }

    // code 8：充电阈值控制状态（回显最近一次设置，默认 0=未启用）。
    ScopedAStatus getCharingThresholdCtrl(int32_t* _aidl_return) override {
        *_aidl_return = static_cast<int32_t>(ctrl_);
        return ScopedAStatus::ok();
    }

    // code 9：设置充电阈值控制。
    ScopedAStatus setCharingThresholdCtrl(CharingThresholdCtrl ctrl, bool* _aidl_return) override {
        ctrl_ = ctrl;
        *_aidl_return = true;
        return ScopedAStatus::ok();
    }

    // code 10：充电停止阈值（百分比，默认 0=未设置）。
    ScopedAStatus getCharingThreshold(int32_t* _aidl_return) override {
        *_aidl_return = threshold_;
        return ScopedAStatus::ok();
    }

    // code 11：设置充电停止阈值。
    ScopedAStatus setCharingThreshold(int32_t threshold, bool* _aidl_return) override {
        threshold_ = threshold;
        *_aidl_return = true;
        return ScopedAStatus::ok();
    }

  private:
    std::string tx_model_;
    ReverseWirelssChargingMode reverse_mode_ = ReverseWirelssChargingMode::UNKNOWN;
    WiredCharingLevel wired_level_ = WiredCharingLevel::UNKNOWN;
    WirelessChargingLevel wireless_level_ = WirelessChargingLevel::UNKNOWN;
    WirelessTxFanMode fan_mode_ = WirelessTxFanMode::UNKNOWN;
    BatteryUPSMode ups_mode_ = BatteryUPSMode::UNKNOWN;
    CharingThresholdCtrl ctrl_ = CharingThresholdCtrl::UNKNOWN;
    int32_t soh_ = -1;
    int32_t threshold_ = 0;
};

}  // namespace

int main() {
    // 单线程池足够：全部方法常数时间返回，没有回调、没有阻塞 IO。
    ABinderProcess_setThreadPoolMaxThreadCount(1);
    std::shared_ptr<MeizuCharging> service = ndk::SharedRefBase::make<MeizuCharging>();
    binder_status_t status = AServiceManager_addService(service->asBinder().get(), kServiceName);
    if (status != STATUS_OK) {
        return 1;
    }
    ABinderProcess_joinThreadPool();
    return 0;
}
