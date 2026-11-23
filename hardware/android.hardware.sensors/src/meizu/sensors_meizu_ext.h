#pragma once

#include <memory>

namespace ndk {
class ICInterface;
}

namespace cdh::sensors {

// Meizu 变体（--variant meizu，-DCDH_SENSORS_MEIZU=1）：注册 vendor.meizu.sensor_aux.ISensorAux/default。
std::shared_ptr<ndk::ICInterface> RegisterMeizuSensorAux();

}  // namespace cdh::sensors
