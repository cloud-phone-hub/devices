// android.hardware.neuralnetworks 的 honor 变体（Honor INxpNfc 协议兼容）。
// 只在 --variant honor（-DCDH_NN_HONOR=1）时编入。
#include "nn_vendor_ext.h"

#include <android/binder_auto_utils.h>
#include <android/binder_parcel.h>
#include <android/binder_status.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace cdh::nn {
namespace {

constexpr const char* kHonorNxpNfcDescriptor =
    "vendor.honor.hardware.nxpnfc.INxpNfc";
constexpr const char* kHonorNxpNfcInterfaceHash =
    "b1e9a470c2e6879277d271b0fa9c327b876d341e";

constexpr const char* kHonorMotionDescriptor =
    "vendor.honor.hardware.motion.IMotion";
constexpr const char* kHonorMotionInterfaceHash =
    "23e142b84a03669b0e133b4e70a081427def7462";

bool ReadByteVecAllocator(void* arrayData, int32_t length, int8_t** outBuffer) {
  auto* vec = static_cast<std::vector<int8_t>*>(arrayData);
  if (length <= 0) {
    vec->clear();
    *outBuffer = nullptr;
    return true;
  }
  vec->resize(static_cast<size_t>(length));
  *outBuffer = vec->data();
  return true;
}

// 荣耀 MagicOS libnfc-nci-snxxx.so 强依赖 vendor.honor.hardware.nxpnfc.INxpNfc/default：
// ioctl(code=2) 回传 0x1020 字节并在 +0x10 回写输入 +0x00 的 NfcAdaptation*。
binder_status_t OnHonorNxpNfcTransact(AIBinder* /*binder*/,
                                      transaction_code_t code,
                                      const AParcel* in,
                                      AParcel* out) {
  if (out == nullptr) {
    return STATUS_BAD_VALUE;
  }
  ndk::ScopedAStatus ok = ndk::ScopedAStatus::ok();
  if (code == FIRST_CALL_TRANSACTION + 0) {
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    if (st != STATUS_OK) return st;
    return AParcel_writeString(out, "", 0);
  }
  if (code == FIRST_CALL_TRANSACTION + 1) {
    int64_t ioctlType = 0;
    std::vector<int8_t> inBuf;
    if (in != nullptr) {
      AParcel_readInt64(in, &ioctlType);
      AParcel_readByteArray(in, &inBuf, ReadByteVecAllocator);
    }
    std::vector<int8_t> resp(0x1020, 0);
    std::memcpy(resp.data(), &ioctlType, sizeof(ioctlType));
    if (inBuf.size() >= 8) {
      std::memcpy(resp.data() + 0x10, inBuf.data(), 8);
    }
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    if (st != STATUS_OK) return st;
    return AParcel_writeByteArray(out, resp.data(),
                                  static_cast<int32_t>(resp.size()));
  }
  if (code == FIRST_CALL_TRANSACTION + 2 ||
      code == FIRST_CALL_TRANSACTION + 3) {
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    if (st != STATUS_OK) return st;
    return AParcel_writeBool(out, false);
  }
  if (code >= FIRST_CALL_TRANSACTION + 4 &&
      code <= FIRST_CALL_TRANSACTION + 7) {
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    if (st != STATUS_OK) return st;
    return AParcel_writeBool(out, true);
  }
  if (code == 0xffffff) {
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    if (st != STATUS_OK) return st;
    return AParcel_writeInt32(out, 1);
  }
  if (code == 0xfffffe) {
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    if (st != STATUS_OK) return st;
    return AParcel_writeString(out, kHonorNxpNfcInterfaceHash, 40);
  }
  return STATUS_UNKNOWN_TRANSACTION;
}

// 荣耀 MagicOS system_server (HWExtMotionService) 轮询 vendor.honor.hardware.motion.IMotion/default：
// 若返回 STATUS_UNKNOWN_TRANSACTION 会陷入每 10ms 死循环重试并刷屏 "Max Aidl transport errors this cycle : 51"。
// 按官方 vendor.honor.hardware.motion-V1-ndk.so 协议响应全方法，使 HWExtMotionService 正常完成初始化并静默休眠。
binder_status_t OnHonorMotionTransact(AIBinder* /*binder*/,
                                      transaction_code_t code,
                                      const AParcel* /*in*/,
                                      AParcel* out) {
  if (out == nullptr) {
    return STATUS_BAD_VALUE;
  }
  ndk::ScopedAStatus ok = ndk::ScopedAStatus::ok();
  // code 1..3, 5..8, 10：deinitMotion / disableMotionAttr / enableMotionAttr /
  // registerMotionCallback / initMoiton / startMotionReco / stopMotionReco / sendInfoToMotion -> int32(0)
  if ((code >= FIRST_CALL_TRANSACTION + 0 && code <= FIRST_CALL_TRANSACTION + 2) ||
      (code >= FIRST_CALL_TRANSACTION + 4 && code <= FIRST_CALL_TRANSACTION + 7) ||
      code == FIRST_CALL_TRANSACTION + 9) {
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    if (st != STATUS_OK) return st;
    return AParcel_writeInt32(out, 0);
  }
  // code 4：pollMotionResult(out Motions_event_t, int32* _aidl_return)
  // 真机该调用阻塞等待底层动作事件；云机适度休眠 500ms 后返回 -1（无事件），避免调用线程零延迟忙等。
  if (code == FIRST_CALL_TRANSACTION + 3) {
    usleep(500 * 1000);
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    if (st != STATUS_OK) return st;
    st = AParcel_writeInt32(out, -1);
    if (st != STATUS_OK) return st;
    // Motions_event_t non-null parcelable: present=1, size=16, field0=0, field1=0, empty vec=0
    AParcel_writeInt32(out, 1);
    AParcel_writeInt32(out, 16);
    AParcel_writeInt32(out, 0);
    AParcel_writeInt32(out, 0);
    return AParcel_writeInt32(out, 0);
  }
  // code 9：supportMotionFeature(out int[] features, int32* _aidl_return)
  if (code == FIRST_CALL_TRANSACTION + 8) {
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    if (st != STATUS_OK) return st;
    st = AParcel_writeInt32(out, 0);
    if (st != STATUS_OK) return st;
    return AParcel_writeInt32(out, 0);  // length = 0（无支持的物理动作特性）
  }
  if (code == 0xffffff) {
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    if (st != STATUS_OK) return st;
    return AParcel_writeInt32(out, 1);
  }
  if (code == 0xfffffe) {
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    if (st != STATUS_OK) return st;
    return AParcel_writeString(out, kHonorMotionInterfaceHash, 40);
  }
  return STATUS_UNKNOWN_TRANSACTION;
}

}  // namespace

AIBinder_Class_onTransact ResolveVendorPlaceholderTransact(
    const std::string& descriptor) {
  if (descriptor == kHonorNxpNfcDescriptor) {
    return OnHonorNxpNfcTransact;
  }
  if (descriptor == kHonorMotionDescriptor) {
    return OnHonorMotionTransact;
  }
  return nullptr;
}

}  // namespace cdh::nn
