#pragma once

#include <memory>

namespace ndk {
class ICInterface;
}

namespace cdh::nn {

// QTI 变体（--variant qti，-DCDH_NN_QTI=1）：在 android.hardware.neuralnetworks-service
// 进程内同步注册 vendor.qti.diaghal.Idiag/default。
std::shared_ptr<ndk::ICInterface> RegisterQtiDiagHal();

}  // namespace cdh::nn
