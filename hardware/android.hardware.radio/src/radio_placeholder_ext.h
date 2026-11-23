// 厂商 Radio 扩展占位服务钩子接口：
//   - default / mtk：不编入 qti/unisoc 扩展，零额外符号；
//   - qti（--variant qti，-DCDH_RADIO_QTI=1）：src/qti/radio_qti_ext.cpp
//     提供高通 IQtiOemHook 回调占位与 IQtiRadioStable 描述符规范化；
//   - unisoc（--variant unisoc，-DCDH_RADIO_UNISOC=1）：src/unisoc/radio_unisoc_ext.cpp
//     提供展锐 IExtRadio* 回调占位与 vendor.unisoc.hardware.radio.* 契约应答。
#pragma once

#include <android/binder_ibinder.h>

#include <string>

namespace cdh::radio {

struct PlaceholderBinding {
  AIBinder_Class_onDestroy on_destroy = nullptr;
  AIBinder_Class_onTransact on_transact = nullptr;
  bool disable_token_header = true;
  void *user_args = nullptr;
  void (*free_user_args)(void *) = nullptr;
};

// 定义或复用用于 AIBinder_associateClass 的远端回调类。
AIBinder_Class *GetOrDefineRemoteCallbackClass(const char *resp_descriptor);

#if CDH_RADIO_QTI || CDH_RADIO_UNISOC
void NormalizeVendorPlaceholderDescriptor(std::string *descriptor);
bool ResolveVendorPlaceholderBinding(const std::string &descriptor,
                                     PlaceholderBinding *out);
#else
inline void NormalizeVendorPlaceholderDescriptor(std::string *) {}
inline bool ResolveVendorPlaceholderBinding(const std::string &,
                                            PlaceholderBinding *) {
  return false;
}
#endif

} // namespace cdh::radio
