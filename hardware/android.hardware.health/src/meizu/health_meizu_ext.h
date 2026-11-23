#pragma once

#include <memory>

namespace ndk {
class ICInterface;
}

namespace cdh::health {

// Meizu 变体（--variant meizu，-DCDH_HEALTH_MEIZU=1）：在 android.hardware.health-service
// 进程内同步注册 vendor.meizu.hardware.meizu_charging.IMeizuCharging/default。
std::shared_ptr<ndk::ICInterface> RegisterMeizuCharging();

}  // namespace cdh::health
