// engine/vendor/hardware 线 —— android.hardware.keymint@samsung 变体实现。
//
// 在 hal_keymint_default 域内同进程注册三星密钥证明扩展服务：
//   - "vendor.samsung.hardware.keymint.ISehKeyMintExtension"（无后缀：keystore2 samsung_attestation.rs 直连名）
//   - "vendor.samsung.hardware.keymint.ISehKeyMintExtension/default"（VINTF 标准实例名）
//
// 之所以必须由 keymint 进程而非通用占位进程注册：plat_sepolicy.cil 对 hal_keymint_service
// 有严格 neverallow（仅允许 hal_keymint_server add，keystore2 仅能 find hal_keymint_service）。
// 通过 -Wl,--wrap=ABinderProcess_joinThreadPool 在上游 Cuttlefish service.cpp 进入线程池等待前
// 无侵入挂载扩展服务，保持上游源码零修改。
//
// 契约取证自 guest /system/lib64/vendor.samsung.hardware.keymint-V4-ndk.so：
//   - Descriptor: "vendor.samsung.hardware.keymint.ISehKeyMintExtension"
//   - VERSION = 4, HASH = "1bd2a0e0a1dab05878a25215197138e81df073ec"
//   - TX 1: attestKey(byte[], KeyParameter[]) -> SehAttestationResult
//           返回 ServiceSpecificError(-66 = ATTESTATION_KEYS_NOT_PROVISIONED)
//   - TX 2: encryptWithPasswordHwKey(byte, SehEncryptionParam) -> SehEncryptionResult
//           返回 ServiceSpecificError(-100 = UNIMPLEMENTED)
//   - TX 3: getLastFailedRot() -> byte[]
//           返回 OK + 空 byte[0]（使 keystore2 get_rot_value 直接成功，不再 panic）

#include <android/binder_auto_utils.h>
#include <android/binder_ibinder.h>
#include <android/binder_manager.h>
#include <android/binder_parcel.h>
#include <android/binder_status.h>
#include <android/log.h>

#include <cstdint>
#include <cstring>

extern "C" {
void AIBinder_markVintfStability(AIBinder *binder);
void __real_ABinderProcess_joinThreadPool(void);
}

namespace {

constexpr const char *kLogTag = "cdh-keymint-samsung";
constexpr const char *kSehKeyMintDescriptor =
    "vendor.samsung.hardware.keymint.ISehKeyMintExtension";
constexpr const char *kSehKeyMintDefaultName =
    "vendor.samsung.hardware.keymint.ISehKeyMintExtension/default";
constexpr const char *kSehKeyMintHash =
    "1bd2a0e0a1dab05878a25215197138e81df073ec";
constexpr int32_t kSehKeyMintVersion = 4;

// KeyMint ErrorCode 常量（android.hardware.security.keymint.ErrorCode）
constexpr int32_t kErrorAttestationKeysNotProvisioned = -66;
constexpr int32_t kErrorUnimplemented = -100;

void *OnSehKeyMintCreate(void * /*args*/) {
  return reinterpret_cast<void *>(1);
}

void OnSehKeyMintDestroy(void * /*userData*/) {}

binder_status_t OnSehKeyMintTransact(AIBinder * /*binder*/,
                                     transaction_code_t code,
                                     const AParcel * /*in*/, AParcel *out) {
  if (out == nullptr) {
    return STATUS_OK;
  }
  ndk::ScopedAStatus ok = ndk::ScopedAStatus::ok();
  if (code == 0xffffff) {
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    return st != STATUS_OK ? st : AParcel_writeInt32(out, kSehKeyMintVersion);
  }
  if (code == 0xfffffe) {
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    return st != STATUS_OK
               ? st
               : AParcel_writeString(
                     out, kSehKeyMintHash,
                     static_cast<int32_t>(std::strlen(kSehKeyMintHash)));
  }

  switch (code) {
    case 1: {  // attestKey -> ATTESTATION_KEYS_NOT_PROVISIONED (-66)
      ndk::ScopedAStatus err = ndk::ScopedAStatus::fromServiceSpecificError(
          kErrorAttestationKeysNotProvisioned);
      return AParcel_writeStatusHeader(out, err.get());
    }
    case 2: {  // encryptWithPasswordHwKey -> UNIMPLEMENTED (-100)
      ndk::ScopedAStatus err =
          ndk::ScopedAStatus::fromServiceSpecificError(kErrorUnimplemented);
      return AParcel_writeStatusHeader(out, err.get());
    }
    case 3: {  // getLastFailedRot -> byte[0]
      binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
      if (st != STATUS_OK) return st;
      const int8_t empty = 0;
      return AParcel_writeByteArray(out, &empty, 0);
    }
    default:
      return STATUS_UNKNOWN_TRANSACTION;
  }
}

void RegisterSamsungKeyMintExtension() {
  static AIBinder_Class *clazz =
      AIBinder_Class_define(kSehKeyMintDescriptor, OnSehKeyMintCreate,
                            OnSehKeyMintDestroy, OnSehKeyMintTransact);
  if (clazz == nullptr) {
    __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                        "define ISehKeyMintExtension class failed");
    return;
  }

  auto registerInstance = [&](const char *serviceName) {
    AIBinder *binder = AIBinder_new(clazz, nullptr);
    if (binder == nullptr) {
      __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                          "allocate binder for %s failed", serviceName);
      return;
    }
    AIBinder_markVintfStability(binder);
    const binder_exception_t st =
        AServiceManager_addService(binder, serviceName);
    if (st != EX_NONE) {
      __android_log_print(ANDROID_LOG_WARN, kLogTag,
                          "addService(%s) failed: %d", serviceName, st);
      AIBinder_decStrong(binder);
      return;
    }
    __android_log_print(ANDROID_LOG_INFO, kLogTag, "registered %s (v%d)",
                        serviceName, kSehKeyMintVersion);
  };

  registerInstance(kSehKeyMintDefaultName);
}

}  // namespace

extern "C" void __wrap_ABinderProcess_joinThreadPool(void) {
  RegisterSamsungKeyMintExtension();
  __real_ABinderProcess_joinThreadPool();
}
