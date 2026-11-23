// android.hardware.neuralnetworks 的 nothing 变体（Nothing OS INtCamService 协议兼容）。
// 只在 --variant nothing（-DCDH_NN_NOTHING=1）时编入。
#include "nn_vendor_ext.h"

#include <android/binder_auto_utils.h>
#include <android/binder_parcel.h>
#include <android/binder_status.h>

#include <string>

namespace cdh::nn {
namespace {

constexpr const char* kNothingCamServiceDescriptor =
    "vendor.noth.hardware.camera.INtCamService";
constexpr const char* kNothingCamSessionDescriptor =
    "vendor.noth.hardware.camera.INtCamSession";
constexpr const char* kNothingCamInterfaceHash =
    "ec128e41418deb99810012bba5a2699bdae7534b";

void* OnSubCreate(void* /*args*/) { return reinterpret_cast<void*>(1); }
void OnSubDestroy(void* /*user_data*/) {}

AIBinder* gNothingCamSessionBinder = nullptr;

binder_status_t OnNothingCamSessionTransact(AIBinder* /*binder*/,
                                            transaction_code_t code,
                                            const AParcel* /*in*/,
                                            AParcel* out) {
  if (out == nullptr) return STATUS_BAD_VALUE;
  ndk::ScopedAStatus ok = ndk::ScopedAStatus::ok();
  if (code == 0xffffff) {
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    return st == STATUS_OK ? AParcel_writeInt32(out, 1) : st;
  }
  if (code == 0xfffffe) {
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    return st == STATUS_OK
               ? AParcel_writeString(out, kNothingCamInterfaceHash, 40)
               : st;
  }
  return AParcel_writeStatusHeader(out, ok.get());
}

AIBinder* GetOrCreateNothingCamSession() {
  if (gNothingCamSessionBinder == nullptr) {
    static AIBinder_Class* sessClass = AIBinder_Class_define(
        kNothingCamSessionDescriptor, OnSubCreate, OnSubDestroy,
        OnNothingCamSessionTransact);
    if (sessClass != nullptr) {
      gNothingCamSessionBinder = AIBinder_new(sessClass, nullptr);
      if (gNothingCamSessionBinder != nullptr) {
        AIBinder_markVintfStability(gNothingCamSessionBinder);
      }
    }
  }
  return gNothingCamSessionBinder;
}

binder_status_t OnNothingCamServiceTransact(AIBinder* /*binder*/,
                                            transaction_code_t code,
                                            const AParcel* /*in*/,
                                            AParcel* out) {
  if (out == nullptr) return STATUS_BAD_VALUE;
  ndk::ScopedAStatus ok = ndk::ScopedAStatus::ok();
  if (code == FIRST_CALL_TRANSACTION + 0) {
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    return st == STATUS_OK
               ? AParcel_writeStrongBinder(out, GetOrCreateNothingCamSession())
               : st;
  }
  if (code == 0xffffff) {
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    return st == STATUS_OK ? AParcel_writeInt32(out, 1) : st;
  }
  if (code == 0xfffffe) {
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    return st == STATUS_OK
               ? AParcel_writeString(out, kNothingCamInterfaceHash, 40)
               : st;
  }
  return STATUS_UNKNOWN_TRANSACTION;
}

}  // namespace

AIBinder_Class_onTransact ResolveVendorPlaceholderTransact(
    const std::string& descriptor) {
  if (descriptor == kNothingCamServiceDescriptor) {
    return OnNothingCamServiceTransact;
  }
  return nullptr;
}

}  // namespace cdh::nn
