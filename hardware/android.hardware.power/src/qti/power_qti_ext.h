#pragma once

#include <memory>
#include <vector>

namespace ndk {
class ICInterface;
}

namespace cdh::power {

// QTI 变体（--variant qti，-DCDH_POWER_QTI=1）：在 android.hardware.power-service
// 进程内同步注册高通 vendor.qti.hardware.perf2.IPerf/default 与
// vendor.qti.hardware.servicetrackeraidl.IServicetracker/default。
std::vector<std::shared_ptr<ndk::ICInterface>> RegisterQtiServices();

}  // namespace cdh::power
