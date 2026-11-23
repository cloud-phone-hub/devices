// android.hardware.neuralnetworks@qti 变体扩展 —— 高通 vendor.qti.diaghal（Idiag v1）自研实现。
//
// 由 android.hardware.neuralnetworks-service（域 hal_neuralnetworks_default）同进程注册：
//   - vendor.qti.diaghal.Idiag/default
// 云机无 /dev/diag 与高通 diag 驱动：open/close 返回 0，ioctl/write 返回 -1（确定性不可用）。
// 只在 --variant qti（-DCDH_NN_QTI=1）时编入，default 变体零高通符号。

#include "nn_qti_ext.h"

#include <android/binder_auto_utils.h>
#include <android/binder_ibinder.h>
#include <android/binder_manager.h>
#include <android/binder_status.h>
#include <android/log.h>

#include <aidl/vendor/qti/diaghal/BnIdiag.h>
#include <aidl/vendor/qti/diaghal/IIdiagcallback.h>
#include <aidl/vendor/qti/diaghal/ParcelableMemory.h>

#include <cstdint>
#include <memory>

namespace cdh::nn {
namespace {

using aidl::vendor::qti::diaghal::BnIdiag;
using aidl::vendor::qti::diaghal::IIdiagcallback;
using aidl::vendor::qti::diaghal::ParcelableMemory;
using ndk::ScopedAStatus;

constexpr const char* kLogTag = "cdh-nn-qti";
constexpr const char* kDiagServiceName = "vendor.qti.diaghal.Idiag/default";

class IdiagService final : public BnIdiag {
 public:
  ScopedAStatus close(int32_t* _aidl_return) override {
    callback_.reset();
    *_aidl_return = 0;
    return ScopedAStatus::ok();
  }

  ScopedAStatus ioctl(int32_t /*cmd*/, const ParcelableMemory& /*memory*/,
                      int32_t /*flag*/, int32_t* _aidl_return) override {
    *_aidl_return = -1;
    return ScopedAStatus::ok();
  }

  ScopedAStatus open(const std::shared_ptr<IIdiagcallback>& callback,
                     int32_t* _aidl_return) override {
    callback_ = callback;
    *_aidl_return = 0;
    return ScopedAStatus::ok();
  }

  ScopedAStatus write(const ParcelableMemory& /*memory*/, int32_t /*length*/,
                      int32_t* _aidl_return) override {
    *_aidl_return = -1;
    return ScopedAStatus::ok();
  }

 private:
  std::shared_ptr<IIdiagcallback> callback_;
};

}  // namespace

std::shared_ptr<ndk::ICInterface> RegisterQtiDiagHal() {
  auto svc = ndk::SharedRefBase::make<IdiagService>();
  const binder_status_t st =
      AServiceManager_addService(svc->asBinder().get(), kDiagServiceName);
  if (st != STATUS_OK) {
    __android_log_print(ANDROID_LOG_WARN, kLogTag, "addService(%s) failed: %d",
                        kDiagServiceName, static_cast<int>(st));
    return nullptr;
  }
  return svc;
}

}  // namespace cdh::nn
