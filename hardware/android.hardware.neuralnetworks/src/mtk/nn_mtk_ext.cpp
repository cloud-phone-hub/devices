// android.hardware.neuralnetworks 的 mtk 变体（MTK IComposerExt 协议兼容）。
// 只在 --variant mtk（-DCDH_NN_MTK=1）时编入。
#include "nn_vendor_ext.h"

#include <android/binder_auto_utils.h>
#include <android/binder_parcel.h>
#include <android/binder_status.h>
#include <android/log.h>

#include <cstdint>
#include <string>

namespace cdh::nn {
namespace {

constexpr const char* kLogTag = "cdh-nn-mtk";
constexpr const char* kComposerExtDescriptor =
    "vendor.mediatek.hardware.composer_ext.IComposerExt";
constexpr const char* kComposerExtInterfaceHash =
    "022c728d211f6b275911cb0602f6641afdd275d6";

// MTK Android 16 SurfaceFlinger / libcomposer_ext 连接 IComposerExt/default：
// syncPerform op=1 (checkOpCodeMax) 返回 err=0 与 int64_t(1)，其余 op>1 返回 -ENOSYS (-38)。
binder_status_t OnComposerExtTransact(AIBinder* /*binder*/,
                                      transaction_code_t code,
                                      const AParcel* in,
                                      AParcel* out) {
  if (code == FIRST_CALL_TRANSACTION + 0) {
    return STATUS_OK;
  }
  if (out == nullptr) {
    return STATUS_BAD_VALUE;
  }
  ndk::ScopedAStatus ok = ndk::ScopedAStatus::ok();
  if (code == FIRST_CALL_TRANSACTION + 1) {
    int64_t hnd = 0;
    int64_t op = 0;
    if (in != nullptr) {
      AParcel_readInt64(in, &hnd);
      AParcel_readInt64(in, &op);
    }
    __android_log_print(ANDROID_LOG_INFO, kLogTag,
                        "IComposerExt syncPerform hnd=%lld op=%lld",
                        static_cast<long long>(hnd),
                        static_cast<long long>(op));
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    if (st != STATUS_OK) {
      return st;
    }
    AParcel_writeInt32(out, 1);
    const int32_t start_pos = AParcel_getDataPosition(out);
    AParcel_writeInt32(out, 0);
    if (op == 1) {
      AParcel_writeInt32(out, 0);
      const int8_t max_op_le[8] = {1, 0, 0, 0, 0, 0, 0, 0};
      AParcel_writeByteArray(out, max_op_le, 8);
    } else {
      AParcel_writeInt32(out, -38);
      AParcel_writeInt32(out, 0);
    }
    AParcel_writeInt32(out, 0);
    AParcel_writeInt32(out, 0);
    const int32_t end_pos = AParcel_getDataPosition(out);
    AParcel_setDataPosition(out, start_pos);
    AParcel_writeInt32(out, end_pos - start_pos);
    AParcel_setDataPosition(out, end_pos);
    return STATUS_OK;
  }
  if (code == FIRST_CALL_TRANSACTION + 2) {
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    if (st != STATUS_OK) {
      return st;
    }
    AParcel_writeInt32(out, 1);
    AParcel_writeInt32(out, 16);
    AParcel_writeInt32(out, 0);
    AParcel_writeInt64(out, 1);
    return STATUS_OK;
  }
  if (code == 0xffffff) {
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    if (st != STATUS_OK) {
      return st;
    }
    AParcel_writeInt32(out, 1);
    return STATUS_OK;
  }
  if (code == 0xfffffe) {
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    if (st != STATUS_OK) {
      return st;
    }
    AParcel_writeString(out, kComposerExtInterfaceHash, 40);
    return STATUS_OK;
  }
  return STATUS_UNKNOWN_TRANSACTION;
}

}  // namespace

AIBinder_Class_onTransact ResolveVendorPlaceholderTransact(
    const std::string& descriptor) {
  if (descriptor == kComposerExtDescriptor) {
    return OnComposerExtTransact;
  }
  return nullptr;
}

}  // namespace cdh::nn
