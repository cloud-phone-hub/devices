#pragma once

#include <memory>

namespace ndk {
class ICInterface;
}

namespace cdh::vibrator {

// Meizu 变体（--variant meizu，-DCDH_VIBRATOR_MEIZU=1）：注册 vendor.awa.reality.IRealityTap/default。
std::shared_ptr<ndk::ICInterface> RegisterMeizuRealityTap();

}  // namespace cdh::vibrator
