// android.hardware.neuralnetworks 的 vivo 变体（OriginOS IVASAlgoProvider 协议兼容）。
// 只在 --variant vivo（-DCDH_NN_VIVO=1）时编入。
#include "nn_vendor_ext.h"

#include <android/binder_auto_utils.h>
#include <android/binder_parcel.h>
#include <android/binder_status.h>

#include <cstdint>
#include <cstring>
#include <string>

namespace cdh::nn {
namespace {

constexpr const char* kVivoVasAlgoProviderDescriptor =
    "vendor.vivo.hardware.vivoalgoservice.IVASAlgoProvider";
constexpr const char* kVivoVasAlgoContextDescriptor =
    "vendor.vivo.hardware.vivoalgoservice.IVASAlgoContext";
constexpr const char* kVivoVasAlgoInterfaceHash =
    "88a8a75e50b62b2dd308786c7bf7d5587a47d9d8";

void* OnSubCreate(void* /*args*/) { return reinterpret_cast<void*>(1); }
void OnSubDestroy(void* /*user_data*/) {}

AIBinder* gVivoVasAlgoContextBinder = nullptr;

binder_status_t WriteVivoVasResultIpc(AParcel* out) {
  static constexpr const char* kDummyAlgoInfo =
      "{name:dummy;ver:1;cam:Master;}";
  const int32_t dummyLen = static_cast<int32_t>(std::strlen(kDummyAlgoInfo));
  binder_status_t st = AParcel_writeInt32(out, 1);
  if (st != STATUS_OK) return st;
  const int32_t startPos = AParcel_getDataPosition(out);
  AParcel_writeInt32(out, 0);
  AParcel_writeInt32(out, 0);
  AParcel_writeInt64(out, 0);
  AParcel_writeInt64(out, 0);
  AParcel_writeInt64(out, 0);
  AParcel_writeInt64(out, 0);
  AParcel_writeString(out, "", 0);
  AParcel_writeInt64(out, 0);
  AParcel_writeString(out, "", 0);
  AParcel_writeBool(out, true);
  AParcel_writeInt32(out, 0);
  AParcel_writeString(out, kDummyAlgoInfo, dummyLen);
  AParcel_writeByteArray(out, nullptr, 0);
  AParcel_writeInt32(out, 0);
  AParcel_writeInt32(out, 0);
  AParcel_writeInt64(out, 0);
  AParcel_writeFloat(out, 0.0f);
  AParcel_writeDouble(out, 0.0);
  AParcel_writeString(out, kDummyAlgoInfo, dummyLen);
  const int32_t endPos = AParcel_getDataPosition(out);
  AParcel_setDataPosition(out, startPos);
  AParcel_writeInt32(out, endPos - startPos);
  AParcel_setDataPosition(out, endPos);
  return STATUS_OK;
}

binder_status_t OnVivoVasAlgoContextTransact(AIBinder* /*binder*/,
                                             transaction_code_t code,
                                             const AParcel* /*in*/,
                                             AParcel* out) {
  if (out == nullptr) return STATUS_BAD_VALUE;
  ndk::ScopedAStatus ok = ndk::ScopedAStatus::ok();
  if (code == FIRST_CALL_TRANSACTION + 0 || code == 0xffffff) {
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    return st == STATUS_OK ? AParcel_writeInt32(out, 2) : st;
  }
  if (code == 0xfffffe) {
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    return st == STATUS_OK
               ? AParcel_writeString(out, kVivoVasAlgoInterfaceHash, 40)
               : st;
  }
  if (code == FIRST_CALL_TRANSACTION + 1) {
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    return st == STATUS_OK ? WriteVivoVasResultIpc(out) : st;
  }
  if (code == FIRST_CALL_TRANSACTION + 2) {
    return AParcel_writeStatusHeader(out, ok.get());
  }
  return AParcel_writeStatusHeader(out, ok.get());
}

AIBinder* GetOrCreateVivoVasAlgoContext() {
  if (gVivoVasAlgoContextBinder == nullptr) {
    static AIBinder_Class* ctxClass = AIBinder_Class_define(
        kVivoVasAlgoContextDescriptor, OnSubCreate, OnSubDestroy,
        OnVivoVasAlgoContextTransact);
    if (ctxClass != nullptr) {
      gVivoVasAlgoContextBinder = AIBinder_new(ctxClass, nullptr);
      if (gVivoVasAlgoContextBinder != nullptr) {
        AIBinder_markVintfStability(gVivoVasAlgoContextBinder);
      }
    }
  }
  return gVivoVasAlgoContextBinder;
}

binder_status_t OnVivoVasAlgoProviderTransact(AIBinder* /*binder*/,
                                              transaction_code_t code,
                                              const AParcel* /*in*/,
                                              AParcel* out) {
  if (out == nullptr) return STATUS_BAD_VALUE;
  ndk::ScopedAStatus ok = ndk::ScopedAStatus::ok();
  if (code == FIRST_CALL_TRANSACTION + 0 || code == 0xffffff) {
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    return st == STATUS_OK ? AParcel_writeInt32(out, 2) : st;
  }
  if (code == 0xfffffe) {
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    return st == STATUS_OK
               ? AParcel_writeString(out, kVivoVasAlgoInterfaceHash, 40)
               : st;
  }
  if (code == FIRST_CALL_TRANSACTION + 1 ||
      code == FIRST_CALL_TRANSACTION + 3 ||
      code == FIRST_CALL_TRANSACTION + 8) {
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    return st == STATUS_OK ? AParcel_writeInt32(out, 0) : st;
  }
  if (code == FIRST_CALL_TRANSACTION + 2) {
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    return st == STATUS_OK
               ? AParcel_writeStrongBinder(out, GetOrCreateVivoVasAlgoContext())
               : st;
  }
  if (code == FIRST_CALL_TRANSACTION + 4) {
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    return st == STATUS_OK ? WriteVivoVasResultIpc(out) : st;
  }
  if (code == FIRST_CALL_TRANSACTION + 5 ||
      code == FIRST_CALL_TRANSACTION + 7 ||
      code == FIRST_CALL_TRANSACTION + 9) {
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    return st == STATUS_OK ? AParcel_writeStrongBinder(out, nullptr) : st;
  }
  return AParcel_writeStatusHeader(out, ok.get());
}

}  // namespace

AIBinder_Class_onTransact ResolveVendorPlaceholderTransact(
    const std::string& descriptor) {
  if (descriptor == kVivoVasAlgoProviderDescriptor) {
    return OnVivoVasAlgoProviderTransact;
  }
  return nullptr;
}

}  // namespace cdh::nn
