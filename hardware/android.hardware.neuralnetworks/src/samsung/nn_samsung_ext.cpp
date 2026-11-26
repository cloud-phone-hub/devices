// android.hardware.neuralnetworks@samsung 变体扩展 —— 三星 ISehHyPer V2 占位应答。
//
// 契约取证自 guest /system/lib64/vendor.samsung.hardware.hyper-V2-ndk.so：
//   - Descriptor: "vendor.samsung.hardware.hyper.ISehHyPer"
//   - VERSION = 2, HASH = "a441ea0d6fcf9a8d771c17914859ac86346259a5"
//   - TX 1: acquire(int[], int, int, int, String) [oneway]
//   - TX 2: release(int, int) [oneway]
//   - TX 3: int[] getSupportedFrequency(int, int) -> int[0]
//   - TX 4: writeSysfs(int, String) [oneway]
//   - TX 5: String readSysfs(int) -> ""
//   - TX 6: boolean checkSysfsIdExist(int) -> false
//   - TX 7: boolean checkHintExist(int) -> false
//   - TX 8: boolean checkResourceExist(int) -> false
//   - TX 9: acquireWithProcName(int[], int, int, int, String, String) [oneway]

#include "nn_vendor_ext.h"

#include <android/binder_auto_utils.h>
#include <android/binder_parcel.h>
#include <android/binder_status.h>

#include <cstdint>
#include <cstring>
#include <string>

namespace cdh::nn {
namespace {

constexpr const char* kSehHyPerDescriptor =
    "vendor.samsung.hardware.hyper.ISehHyPer";
constexpr const char* kSehHyPerHash =
    "a441ea0d6fcf9a8d771c17914859ac86346259a5";
constexpr int32_t kSehHyPerVersion = 2;

binder_status_t OnSehHyPerTransact(AIBinder* /*binder*/,
                                   transaction_code_t code,
                                   const AParcel* /*in*/, AParcel* out) {
  ndk::ScopedAStatus ok = ndk::ScopedAStatus::ok();
  if (code == 0xffffff) {
    if (out == nullptr) return STATUS_OK;
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    return st != STATUS_OK ? st : AParcel_writeInt32(out, kSehHyPerVersion);
  }
  if (code == 0xfffffe) {
    if (out == nullptr) return STATUS_OK;
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    return st != STATUS_OK
               ? st
               : AParcel_writeString(
                     out, kSehHyPerHash,
                     static_cast<int32_t>(std::strlen(kSehHyPerHash)));
  }

  switch (code) {
    case 1:  // acquire (oneway)
    case 2:  // release (oneway)
    case 4:  // writeSysfs (oneway)
    case 9:  // acquireWithProcName (oneway)
      if (out != nullptr) {
        return AParcel_writeStatusHeader(out, ok.get());
      }
      return STATUS_OK;
    case 3: {  // int[] getSupportedFrequency(int, int)
      if (out == nullptr) return STATUS_OK;
      binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
      return st != STATUS_OK ? st : AParcel_writeInt32(out, 0);
    }
    case 5: {  // String readSysfs(int)
      if (out == nullptr) return STATUS_OK;
      binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
      return st != STATUS_OK ? st : AParcel_writeString(out, "", 0);
    }
    case 6:    // boolean checkSysfsIdExist(int)
    case 7:    // boolean checkHintExist(int)
    case 8: {  // boolean checkResourceExist(int)
      if (out == nullptr) return STATUS_OK;
      binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
      return st != STATUS_OK ? st : AParcel_writeBool(out, false);
    }
    default:
      return out == nullptr ? STATUS_OK : STATUS_UNKNOWN_TRANSACTION;
  }
}

}  // namespace

AIBinder_Class_onTransact ResolveVendorPlaceholderTransact(
    const std::string& descriptor) {
  if (descriptor == kSehHyPerDescriptor) {
    return OnSehHyPerTransact;
  }
  return nullptr;
}

}  // namespace cdh::nn
