// android.hardware.neuralnetworks 的 mtk 变体（MTK IComposerExt 与传音 ITZService 协议兼容）。
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
constexpr const char* kTranTzServiceDescriptor =
    "vendor.transsion.hardware.camera.tzservice.ITZService";
constexpr const char* kTranTzServiceInterfaceHash =
    "0000000000000000000000000000000000000000";
constexpr const char* kMtkNvramDescriptor =
    "vendor.mediatek.hardware.nvram.INvram";
constexpr const char* kMtkNvramInterfaceHash =
    "0000000000000000000000000000000000000000";
constexpr const char* kMtkPowerApplistDescriptor =
    "vendor.mediatek.hardware.mtkpower_applist.IMtkpower_applist";
constexpr const char* kMtkPowerApplistInterfaceHash =
    "f2fe2189a9da42ccf608e55da9420bad2fa42c81";
constexpr const char* kMtkPowerServiceDescriptor =
    "vendor.mediatek.hardware.mtkpower.IMtkPowerService";
constexpr const char* kMtkPowerServiceInterfaceHash =
    "a484177ceab5bb684c14f2e87cf686ac32b6757c";

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

// 传音 MTK 相机（com.transsion.camera TZServiceProxy）连接 ITZService/default：
//   code 1..6（setTZCaptureCallback / setTZCaptureJpegCallback / setTZCapConfig /
//              setPhoneWorkingState / setMappingInfo / setAppIOOperationStart）返回 int32(0)；
//   code 7（getOffPeakModeSupportCfg）返回长度 0 的空 int[]（int32(0)）；
// 避免 getService 返回 null 或 STATUS_UNKNOWN_TRANSACTION 导致 UI 主线程每次操作阻塞 500ms。
binder_status_t OnTranTzServiceTransact(AIBinder* /*binder*/,
                                        transaction_code_t code,
                                        const AParcel* /*in*/,
                                        AParcel* out) {
  if (out == nullptr) {
    return STATUS_OK;
  }
  ndk::ScopedAStatus ok = ndk::ScopedAStatus::ok();
  if (code >= FIRST_CALL_TRANSACTION + 0 &&
      code <= FIRST_CALL_TRANSACTION + 6) {
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    if (st != STATUS_OK) {
      return st;
    }
    AParcel_writeInt32(out, 0);
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
    AParcel_writeString(out, kTranTzServiceInterfaceHash, 40);
    return STATUS_OK;
  }
  return STATUS_UNKNOWN_TRANSACTION;
}

// MTK INvram AIDL 兼容应答桩（system_server MtkSecurityHelper / radio 查询 proinfo/nvram）：
//   code 1（readFileByName）与 code 4（getNvramData）返回空 String("")；
//   code 2（readFileByNamevec）返回长度 0 的空 byte[]（int32(0)）；
//   code 3 / 5..8（writeFileByNamevec 等）返回 int8/int32(0)。
binder_status_t OnMtkNvramTransact(AIBinder* /*binder*/,
                                   transaction_code_t code,
                                   const AParcel* /*in*/,
                                   AParcel* out) {
  if (out == nullptr) {
    return STATUS_OK;
  }
  ndk::ScopedAStatus ok = ndk::ScopedAStatus::ok();
  if (code == FIRST_CALL_TRANSACTION + 0 ||
      code == FIRST_CALL_TRANSACTION + 3) {
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    if (st != STATUS_OK) {
      return st;
    }
    AParcel_writeString(out, "", 0);
    return STATUS_OK;
  }
  if (code >= FIRST_CALL_TRANSACTION + 1 &&
      code <= FIRST_CALL_TRANSACTION + 7) {
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    if (st != STATUS_OK) {
      return st;
    }
    AParcel_writeInt32(out, 0);
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
    AParcel_writeString(out, kMtkNvramInterfaceHash, 40);
    return STATUS_OK;
  }
  return STATUS_UNKNOWN_TRANSACTION;
}

// MTK IMtkpower_applist V3 AIDL 兼容应答桩（system_server PowerHalWrapper 焦点切换调用）：
//   code 1..3（notifyAppState / notifyAppStateInfo / setSysInfo）：void 返回 STATUS_OK；
//   code 4（getSysInfo）：返回空 String("")；
//   getInterfaceVersion=3, getInterfaceHash=f2fe2189a9da42ccf608e55da9420bad2fa42c81。
binder_status_t OnMtkPowerApplistTransact(AIBinder* /*binder*/,
                                          transaction_code_t code,
                                          const AParcel* /*in*/,
                                          AParcel* out) {
  if (out == nullptr) {
    return STATUS_OK;
  }
  ndk::ScopedAStatus ok = ndk::ScopedAStatus::ok();
  if (code >= FIRST_CALL_TRANSACTION + 0 &&
      code <= FIRST_CALL_TRANSACTION + 2) {
    return AParcel_writeStatusHeader(out, ok.get());
  }
  if (code == FIRST_CALL_TRANSACTION + 3) {
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    if (st != STATUS_OK) {
      return st;
    }
    AParcel_writeString(out, "", 0);
    return STATUS_OK;
  }
  if (code == 0xffffff) {
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    if (st != STATUS_OK) {
      return st;
    }
    AParcel_writeInt32(out, 3);
    return STATUS_OK;
  }
  if (code == 0xfffffe) {
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    if (st != STATUS_OK) {
      return st;
    }
    AParcel_writeString(out, kMtkPowerApplistInterfaceHash, 40);
    return STATUS_OK;
  }
  return STATUS_UNKNOWN_TRANSACTION;
}

// MTK IMtkPowerService V3 AIDL 兼容应答桩（libpowerhalwrap / libmtkperf_client 性能锁与场景提示）：
//   code +2/+4/+5/+10（perfLockRelease / mtkCusPowerHint / mtkPowerHint / setSysInfoAsync）：void；
//   其余 code +0..+13（perfCusLockHint / perfLockAcquire / querySysInfo 等）：返回 int32(0)；
//   getInterfaceVersion=3, getInterfaceHash=a484177ceab5bb684c14f2e87cf686ac32b6757c。
binder_status_t OnMtkPowerServiceTransact(AIBinder* /*binder*/,
                                          transaction_code_t code,
                                          const AParcel* /*in*/,
                                          AParcel* out) {
  if (out == nullptr) {
    return STATUS_OK;
  }
  ndk::ScopedAStatus ok = ndk::ScopedAStatus::ok();
  if (code == FIRST_CALL_TRANSACTION + 2 ||
      code == FIRST_CALL_TRANSACTION + 4 ||
      code == FIRST_CALL_TRANSACTION + 5 ||
      code == FIRST_CALL_TRANSACTION + 10) {
    return AParcel_writeStatusHeader(out, ok.get());
  }
  if (code >= FIRST_CALL_TRANSACTION + 0 &&
      code <= FIRST_CALL_TRANSACTION + 13) {
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    if (st != STATUS_OK) {
      return st;
    }
    AParcel_writeInt32(out, 0);
    return STATUS_OK;
  }
  if (code == 0xffffff) {
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    if (st != STATUS_OK) {
      return st;
    }
    AParcel_writeInt32(out, 3);
    return STATUS_OK;
  }
  if (code == 0xfffffe) {
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    if (st != STATUS_OK) {
      return st;
    }
    AParcel_writeString(out, kMtkPowerServiceInterfaceHash, 40);
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
  if (descriptor == kTranTzServiceDescriptor) {
    return OnTranTzServiceTransact;
  }
  if (descriptor == kMtkNvramDescriptor) {
    return OnMtkNvramTransact;
  }
  if (descriptor == kMtkPowerApplistDescriptor) {
    return OnMtkPowerApplistTransact;
  }
  if (descriptor == kMtkPowerServiceDescriptor) {
    return OnMtkPowerServiceTransact;
  }
  return nullptr;
}

}  // namespace cdh::nn
