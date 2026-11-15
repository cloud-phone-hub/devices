#pragma once

#include <memory>

#include <android/binder_interface_utils.h>

namespace aidl::android::hardware::graphics::composer3::cdh {

// 欧加变体（只在 `--variant oplus` / `-DCDH_COMPOSER_OPLUS=1` 时编入，不进基础产物）：
// 若当前设备的 VINTF 清单（/odm 或 /vendor）声明了 OPPO/OnePlus/realme 显示色彩扩展接口
// vendor.oplus.hardware.displaycolorfeature.IDisplayColorFeature/default，
// 则在 HWC3（hal_graphics_composer_default）进程内注册其兼容实现并返回持有对象。
std::shared_ptr<::ndk::ICInterface> RegisterOplusDisplayColorFeatureIfDeclared();

}  // namespace aidl::android::hardware::graphics::composer3::cdh
