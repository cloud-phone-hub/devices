// engine/vendor/hardware 线 —— android.hardware.radio 的 samsung 变体接口。
#pragma once

#include "channel.h"

namespace cdh::radio {

// 在 hal_radio_default 进程内主动注册三星 OneUI SemRIL 所需的全套 ISehRadio* 服务：
//   - vendor.samsung.hardware.radio.sim.ISehRadioSim/slot1..2（实时对接 StateStore）
//   - vendor.samsung.hardware.radio.data.ISehRadioData/slot1..2
//   - vendor.samsung.hardware.radio.network.ISehRadioNetwork/slot1..2
//   - vendor.samsung.hardware.radio.messaging.ISehRadioMessaging/slot1..2
//   - vendor.samsung.hardware.radio.bridge.ISehRadioBridge/slot1..2
//   - vendor.samsung.hardware.radio.channel.ISehRadioChannel/{imsd,imsd2,epdgd,epdgd2}
//   - vendor.samsung.hardware.radio.satellite.ISehRadioSatellite/default
void RegisterSamsungRadioServices(Channel &channel, StateStore &state_store);

}  // namespace cdh::radio
