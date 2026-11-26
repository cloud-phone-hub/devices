// 厂商 NeuralNetworks 占位服务扩展钩子接口：
//   - default：无厂商分支，ResolveVendorPlaceholderTransact 恒返回 nullptr；
//   - mtk（--variant mtk，-DCDH_NN_MTK=1）：src/mtk/nn_mtk_ext.cpp（IComposerExt）；
//   - honor（--variant honor，-DCDH_NN_HONOR=1）：src/honor/nn_honor_ext.cpp（INxpNfc）；
//   - vivo（--variant vivo，-DCDH_NN_VIVO=1）：src/vivo/nn_vivo_ext.cpp（IVASAlgoProvider）；
//   - nothing（--variant nothing，-DCDH_NN_NOTHING=1）：src/nothing/nn_nothing_ext.cpp（INtCamService）；
//   - qti（--variant qti，-DCDH_NN_QTI=1）：src/qti/nn_qti_ext.cpp（同进程注册 vendor.qti.diaghal.Idiag/default）。
#pragma once

#include <android/binder_ibinder.h>

#include <string>

extern "C" {
void AIBinder_markVintfStability(AIBinder* binder);
void AIBinder_Class_disableInterfaceTokenHeader(AIBinder_Class* clazz);
}

namespace cdh::nn {

#if CDH_NN_MTK || CDH_NN_HONOR || CDH_NN_VIVO || CDH_NN_NOTHING || CDH_NN_SAMSUNG
AIBinder_Class_onTransact ResolveVendorPlaceholderTransact(
    const std::string& descriptor);
#else
inline AIBinder_Class_onTransact ResolveVendorPlaceholderTransact(
    const std::string&) {
  return nullptr;
}
#endif

}  // namespace cdh::nn
