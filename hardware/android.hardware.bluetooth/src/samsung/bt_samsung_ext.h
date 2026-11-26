// engine/vendor/hardware 线 —— android.hardware.bluetooth 的 samsung 变体扩展接口。
#pragma once

namespace cdh::bluetooth {

// 在 hal_bluetooth_default 同进程内注册 vendor.samsung.hardware.bluetooth.ISehBluetooth/default。
bool RegisterSamsungBluetooth();

}  // namespace cdh::bluetooth
