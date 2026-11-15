// engine/vendor/hardware 线 —— android.hardware.health 的 oplus 变体
// （OPPO / OnePlus / realme 充电扩展 AIDL 兼容层）。
//
// 官方 /odm/etc/vintf/manifest/manifest_oplus_charger_aidl.xml 声明了
// vendor.oplus.hardware.charger.ICharger/default。若云机未注册该服务：
//   1. system_server 的 OplusBatteryServiceFeature.getPsyBatteryNotify() 连接失败返回 -1；
//   2. OplusBatteryService 将 notifycode=-1（0xffffffff）写入粘性广播
//      android.intent.action.ADDITIONAL_BATTERY_CHANGED；
//   3. com.oplus.battery 的 SmartChargeProtectionController 未判负值直接位与
//      (0x10000000 & notifycode) != 0（NOTIFY_CODE_LONG_CHARGE_LEVEL_ONE），开机即弹出
//      TYPE_SYSTEM_ALERT 全局遮挡弹窗「建议充电上限设为 80%」。
//
// 本变体只在 `--variant oplus`（`-DCDH_HEALTH_OPLUS=1`）时编入，不进入基础（default）
// 或 samsung 产物；运行时检测到设备 VINTF 声明了 ICharger/default 即在 health-service
// 同进程注册该服务，按当前 HealthInfo 快照返回 notifycode=0 及一致的电池数据。

#pragma once

#include <aidl/android/hardware/health/HealthInfo.h>
#include <android/binder_interface_utils.h>

#include <cstdint>
#include <functional>
#include <memory>

namespace cdh::health {

using OplusHealthSnapshotFn = std::function<void(
    aidl::android::hardware::health::HealthInfo *out, int32_t *outSoh)>;

std::shared_ptr<::ndk::ICInterface> RegisterOplusChargerIfDeclared(
    OplusHealthSnapshotFn snapshotFn);

}  // namespace cdh::health
