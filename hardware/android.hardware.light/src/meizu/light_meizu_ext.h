#pragma once

#include <memory>

namespace ndk {
class ICInterface;
}

namespace cdh::light {

// Meizu 变体（--variant meizu，-DCDH_LIGHT_MEIZU=1）：注册 vendor.meizu.hardware.aw_light.IAwLight/default。
std::shared_ptr<ndk::ICInterface> RegisterMeizuAwLight();

}  // namespace cdh::light
