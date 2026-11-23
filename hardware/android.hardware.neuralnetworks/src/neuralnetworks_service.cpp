// engine/vendor/hardware 线 —— android.hardware.neuralnetworks（IDevice v4）自研服务实现。
//
// 注册 android.hardware.neuralnetworks.IDevice/default。NNAPI 设备 HAL 负责模型编译/执行。
// 云端虚拟机没有可用的 NPU/GPU/CPU 加速后端：本桩报一个"存在但无能力"的通用设备——
//   * getType=OTHER、getVersionString 空串、getCapabilities/getSupportedExtensions/
//     getNumberOfCacheFilesNeeded/getSupportedOperations 返回空/默认（即不支持任何算子）；
//   * allocate / prepareModel* 无实设备可分配/编译 → 返回 EX_UNSUPPORTED_OPERATION。
// 设备无关，不内置机型常量。

#include <android/binder_ibinder.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <android/binder_status.h>
#include <android/log.h>

#include <aidl/android/hardware/neuralnetworks/BnDevice.h>
#include <aidl/android/hardware/neuralnetworks/BufferDesc.h>
#include <aidl/android/hardware/neuralnetworks/Capabilities.h>
#include <aidl/android/hardware/neuralnetworks/DeviceBuffer.h>
#include <aidl/android/hardware/neuralnetworks/DeviceType.h>
#include <aidl/android/hardware/neuralnetworks/Extension.h>
#include <aidl/android/hardware/neuralnetworks/Model.h>
#include <aidl/android/hardware/neuralnetworks/NumberOfCacheFiles.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

using aidl::android::hardware::neuralnetworks::BnDevice;
using aidl::android::hardware::neuralnetworks::BufferDesc;
using aidl::android::hardware::neuralnetworks::BufferRole;
using aidl::android::hardware::neuralnetworks::Capabilities;
using aidl::android::hardware::neuralnetworks::DeviceBuffer;
using aidl::android::hardware::neuralnetworks::DeviceType;
using aidl::android::hardware::neuralnetworks::ExecutionPreference;
using aidl::android::hardware::neuralnetworks::Extension;
using aidl::android::hardware::neuralnetworks::IPreparedModelCallback;
using aidl::android::hardware::neuralnetworks::IPreparedModelParcel;
using aidl::android::hardware::neuralnetworks::Model;
using aidl::android::hardware::neuralnetworks::NumberOfCacheFiles;
using aidl::android::hardware::neuralnetworks::PrepareModelConfig;
using aidl::android::hardware::neuralnetworks::Priority;
using ndk::ScopedAStatus;

static constexpr const char* kNeuralNetworksServiceName =
    "android.hardware.neuralnetworks.IDevice/default";

extern "C" {
void AIBinder_markVintfStability(AIBinder* binder);
void AIBinder_Class_disableInterfaceTokenHeader(AIBinder_Class* clazz);
}

namespace {

constexpr const char* kNoDevice = "no neural network accelerator backend";

class DeviceService : public BnDevice {
 public:
  ScopedAStatus getType(DeviceType* _aidl_return) override {
    *_aidl_return = DeviceType::OTHER;
    return ScopedAStatus::ok();
  }

  ScopedAStatus getVersionString(std::string* _aidl_return) override {
    _aidl_return->clear();
    return ScopedAStatus::ok();
  }

  ScopedAStatus getCapabilities(Capabilities* /*_aidl_return*/) override {
    // 默认构造的空能力（不声明任何 operand 性能）。
    return ScopedAStatus::ok();
  }

  ScopedAStatus getSupportedOperations(
      const Model& /*in_model*/,
      std::vector<bool>* _aidl_return) override {
    _aidl_return->clear();  // 无算子受支持
    return ScopedAStatus::ok();
  }

  ScopedAStatus getSupportedExtensions(
      std::vector<Extension>* _aidl_return) override {
    _aidl_return->clear();
    return ScopedAStatus::ok();
  }

  ScopedAStatus getNumberOfCacheFilesNeeded(
      NumberOfCacheFiles* /*_aidl_return*/) override {
    return ScopedAStatus::ok();
  }

  ScopedAStatus allocate(
      const BufferDesc& /*in_desc*/,
      const std::vector<IPreparedModelParcel>& /*in_preparedModels*/,
      const std::vector<BufferRole>& /*in_inputRoles*/,
      const std::vector<BufferRole>& /*in_outputRoles*/,
      DeviceBuffer* /*_aidl_return*/) override {
    return ScopedAStatus::fromExceptionCodeWithMessage(
        EX_UNSUPPORTED_OPERATION, kNoDevice);
  }

  ScopedAStatus prepareModel(
      const Model& /*in_model*/, ExecutionPreference /*in_preference*/,
      Priority /*in_priority*/, int64_t /*in_deadlineNs*/,
      const std::vector<ndk::ScopedFileDescriptor>& /*in_modelCache*/,
      const std::vector<ndk::ScopedFileDescriptor>& /*in_dataCache*/,
      const std::vector<uint8_t>& /*in_token*/,
      const std::shared_ptr<IPreparedModelCallback>& /*in_callback*/) override {
    return ScopedAStatus::fromExceptionCodeWithMessage(
        EX_UNSUPPORTED_OPERATION, kNoDevice);
  }

  ScopedAStatus prepareModelFromCache(
      int64_t /*in_deadlineNs*/,
      const std::vector<ndk::ScopedFileDescriptor>& /*in_modelCache*/,
      const std::vector<ndk::ScopedFileDescriptor>& /*in_dataCache*/,
      const std::vector<uint8_t>& /*in_token*/,
      const std::shared_ptr<IPreparedModelCallback>& /*in_callback*/) override {
    return ScopedAStatus::fromExceptionCodeWithMessage(
        EX_UNSUPPORTED_OPERATION, kNoDevice);
  }

  ScopedAStatus prepareModelWithConfig(
      const Model& /*in_model*/, const PrepareModelConfig& /*in_config*/,
      const std::shared_ptr<IPreparedModelCallback>& /*in_callback*/) override {
    return ScopedAStatus::fromExceptionCodeWithMessage(
        EX_UNSUPPORTED_OPERATION, kNoDevice);
  }
};

constexpr const char* kLogTag = "cdh-nn";
constexpr const char* kComposerExtDescriptor =
    "vendor.mediatek.hardware.composer_ext.IComposerExt";
constexpr const char* kComposerExtInterfaceHash =
    "022c728d211f6b275911cb0602f6641afdd275d6";
constexpr const char* kHonorNxpNfcDescriptor =
    "vendor.honor.hardware.nxpnfc.INxpNfc";
constexpr const char* kHonorNxpNfcInterfaceHash =
    "b1e9a470c2e6879277d271b0fa9c327b876d341e";
constexpr const char* kVivoVasAlgoProviderDescriptor =
    "vendor.vivo.hardware.vivoalgoservice.IVASAlgoProvider";
constexpr const char* kVivoVasAlgoContextDescriptor =
    "vendor.vivo.hardware.vivoalgoservice.IVASAlgoContext";
constexpr const char* kVivoVasAlgoInterfaceHash =
    "88a8a75e50b62b2dd308786c7bf7d5587a47d9d8";
constexpr const char* kNothingCamServiceDescriptor =
    "vendor.noth.hardware.camera.INtCamService";
constexpr const char* kNothingCamSessionDescriptor =
    "vendor.noth.hardware.camera.INtCamSession";
constexpr const char* kNothingCamInterfaceHash =
    "ec128e41418deb99810012bba5a2699bdae7534b";

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

// 荣耀 MagicOS libnfc-nci-snxxx.so 在 NfcAdaptation::InitializeAidlHalContext 中强依赖
// vendor.honor.hardware.nxpnfc.INxpNfc/default，并在 NfcConfig::loadConfig 中调用
// ioctl(code=2) 后直接解引用输出字节数组 +0x10 处（调用方在入参 +0x10 写入的 NfcAdaptation*）。
// 此处实现最小协议兼容响应：ioctl 原样回传输入缓冲区（至少 4416 字节以保留头部指针与输出结构），
// 其余查询方法返回默认布尔/空串，保障荣耀 NfcNci 完整通过 JNI 初始化并走标准 AIDL INfc/default。
binder_status_t OnHonorNxpNfcTransact(AIBinder* /*binder*/,
                                      transaction_code_t code,
                                      const AParcel* in,
                                      AParcel* out) {
  if (out == nullptr) {
    return STATUS_BAD_VALUE;
  }
  ndk::ScopedAStatus ok = ndk::ScopedAStatus::ok();
  if (code == FIRST_CALL_TRANSACTION + 0) {
    // getVendorParam(String key) -> String
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    if (st != STATUS_OK) return st;
    return AParcel_writeString(out, "", 0);
  }
  if (code == FIRST_CALL_TRANSACTION + 1) {
    // ioctl(long ioctlType, byte[] inOutData) -> byte[]
    // 调用方在入参 +0x00 写入 NfcAdaptation* context，返回后从出参 +0x10 读回该指针
    // 并向其 mIoctlData + 0x120 拷贝 0x1020 (4128) 字节（nfc_nci_ExtnOutputData_t）。
    int64_t ioctlType = 0;
    std::vector<int8_t> inBuf;
    if (in != nullptr) {
      AParcel_readInt64(in, &ioctlType);
      AParcel_readByteArray(in, &inBuf, ReadByteVecAllocator);
    }
    std::vector<int8_t> resp(0x1020, 0);
    memcpy(resp.data(), &ioctlType, sizeof(ioctlType));
    if (inBuf.size() >= 8) {
      memcpy(resp.data() + 0x10, inBuf.data(), 8);
    }
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    if (st != STATUS_OK) return st;
    return AParcel_writeByteArray(out, resp.data(),
                                  static_cast<int32_t>(resp.size()));
  }
  if (code == FIRST_CALL_TRANSACTION + 2 ||
      code == FIRST_CALL_TRANSACTION + 3) {
    // isJcopUpdateRequired / isLsUpdateRequired -> false
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    if (st != STATUS_OK) return st;
    return AParcel_writeBool(out, false);
  }
  if (code >= FIRST_CALL_TRANSACTION + 4 &&
      code <= FIRST_CALL_TRANSACTION + 7) {
    // resetEse / setEseUpdateState / setNxpTransitConfig / setVendorParam -> true
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    if (st != STATUS_OK) return st;
    return AParcel_writeBool(out, true);
  }
  if (code == 0xffffff) {
    // getInterfaceVersion -> 1
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    if (st != STATUS_OK) return st;
    return AParcel_writeInt32(out, 1);
  }
  if (code == 0xfffffe) {
    // getInterfaceHash
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    if (st != STATUS_OK) return st;
    return AParcel_writeString(out, kHonorNxpNfcInterfaceHash, 40);
  }
  return STATUS_UNKNOWN_TRANSACTION;
}

void* OnPlaceholderCreate(void* /*args*/) {
  return reinterpret_cast<void*>(1);
}

void OnPlaceholderDestroy(void* /*user_data*/) {}

binder_status_t OnPlaceholderTransact(AIBinder* /*binder*/,
                                      transaction_code_t /*code*/,
                                      const AParcel* /*in*/,
                                      AParcel* /*out*/) {
  return STATUS_UNKNOWN_TRANSACTION;
}

// MTK Android 16 SurfaceFlinger / libcomposer_ext 无空指针保护或循环重试连接 IComposerExt/default：
// 实现最小协议兼容响应（asyncPerform / syncPerform / registerClient / interfaceVersion / interfaceHash）。
// 其中 syncPerform op=1 (checkOpCodeMax) 返回 err=0 与 8 字节 int64_t(1) 作为最大支持 opCode，
// 其余 op>1 返回 err=-38 (-ENOSYS)，使客户端优雅降级、不再访问空 retData。
binder_status_t OnComposerExtTransact(AIBinder* /*binder*/,
                                      transaction_code_t code,
                                      const AParcel* in,
                                      AParcel* out) {
  if (code == FIRST_CALL_TRANSACTION + 0) {
    // asyncPerform (oneway)
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
    AParcel_writeInt32(out, 1);  // non-null parcelable
    const int32_t start_pos = AParcel_getDataPosition(out);
    AParcel_writeInt32(out, 0);  // size placeholder
    if (op == 1) {
      // checkOpCodeMax -> err=0, retData=int64_t(1) (8 bytes LE)
      AParcel_writeInt32(out, 0);
      const int8_t max_op_le[8] = {1, 0, 0, 0, 0, 0, 0, 0};
      AParcel_writeByteArray(out, max_op_le, 8);
    } else {
      // unsupported extension op -> err=-ENOSYS (-38), empty retData
      AParcel_writeInt32(out, -38);
      AParcel_writeInt32(out, 0);
    }
    AParcel_writeInt32(out, 0);  // retFds length = 0
    AParcel_writeInt32(out, 0);  // retHandles length = 0
    const int32_t end_pos = AParcel_getDataPosition(out);
    AParcel_setDataPosition(out, start_pos);
    AParcel_writeInt32(out, end_pos - start_pos);
    AParcel_setDataPosition(out, end_pos);
    return STATUS_OK;
  }
  if (code == FIRST_CALL_TRANSACTION + 2) {
    // registerClient -> parcelable_registerClient { ret=0, hnd=1 }
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    if (st != STATUS_OK) {
      return st;
    }
    AParcel_writeInt32(out, 1);   // non-null parcelable
    AParcel_writeInt32(out, 16);  // parcelable total size (bytes)
    AParcel_writeInt32(out, 0);   // ret = 0 (OK)
    AParcel_writeInt64(out, 1);   // valid non-zero client handle
    return STATUS_OK;
  }
  if (code == 0xffffff) {
    // getInterfaceVersion -> 1
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    if (st != STATUS_OK) {
      return st;
    }
    AParcel_writeInt32(out, 1);
    return STATUS_OK;
  }
  if (code == 0xfffffe) {
    // getInterfaceHash
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    if (st != STATUS_OK) {
      return st;
    }
    AParcel_writeString(out, kComposerExtInterfaceHash, 40);
    return STATUS_OK;
  }
  return STATUS_UNKNOWN_TRANSACTION;
}

// vivo OriginOS Vivo3rdAlgoService 在 ContentProvider.onCreate() 后台线程调用
// IVASAlgoProvider::createContext -> IVASAlgoContext::submitRequest("QueryAlgoInfo;")；
// 若未返回可被 analyzeAlgoInfo() 解析的条目，VivoAlgoServiceContentProvider 会阻塞
// com.android.camera 主线程 30~60s 后 System.exit(0) 触发连坐重启死循环。
// 此处按 vendor.vivo.hardware.vivoalgoservice-V2-ndk.so 的精确 VASResultIPC 布局
// 返回最小合法结果 "{name:dummy;ver:1;cam:Master;}"，让 Vivo3rdAlgoService 毫秒就绪。
AIBinder* gVivoVasAlgoContextBinder = nullptr;

binder_status_t WriteVivoVasResultIpc(AParcel* out) {
  static constexpr const char* kDummyAlgoInfo = "{name:dummy;ver:1;cam:Master;}";
  const int32_t dummyLen = static_cast<int32_t>(strlen(kDummyAlgoInfo));
  binder_status_t st = AParcel_writeInt32(out, 1);  // non-null parcelable
  if (st != STATUS_OK) return st;
  const int32_t startPos = AParcel_getDataPosition(out);
  AParcel_writeInt32(out, 0);  // parcelable size placeholder
  AParcel_writeInt32(out, 0);  // +0x00 status = 0 (OK)
  AParcel_writeInt64(out, 0);  // +0x08
  AParcel_writeInt64(out, 0);  // +0x10
  AParcel_writeInt64(out, 0);  // +0x18
  AParcel_writeInt64(out, 0);  // +0x20
  AParcel_writeString(out, "", 0);               // +0x28
  AParcel_writeInt64(out, 0);                    // +0x40
  AParcel_writeString(out, "", 0);               // +0x48
  AParcel_writeBool(out, true);                  // +0x60
  AParcel_writeInt32(out, 0);                    // +0x64
  AParcel_writeString(out, kDummyAlgoInfo, dummyLen);  // +0x68
  AParcel_writeByteArray(out, nullptr, 0);       // +0x80
  AParcel_writeInt32(out, 0);                    // +0x98 parcelable array len=0
  AParcel_writeInt32(out, 0);                    // +0xb0
  AParcel_writeInt64(out, 0);                    // +0xb8
  AParcel_writeFloat(out, 0.0f);                 // +0xc0
  AParcel_writeDouble(out, 0.0);                 // +0xc8
  AParcel_writeString(out, kDummyAlgoInfo, dummyLen);  // +0xd0
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
    // getVersion / getInterfaceVersion -> 2
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
    // submitRequest -> VASResultIPC
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    return st == STATUS_OK ? WriteVivoVasResultIpc(out) : st;
  }
  if (code == FIRST_CALL_TRANSACTION + 2) {
    // submitRequestAsync
    return AParcel_writeStatusHeader(out, ok.get());
  }
  return AParcel_writeStatusHeader(out, ok.get());
}

AIBinder* GetOrCreateVivoVasAlgoContext() {
  if (gVivoVasAlgoContextBinder == nullptr) {
    static AIBinder_Class* ctxClass = AIBinder_Class_define(
        kVivoVasAlgoContextDescriptor, OnPlaceholderCreate,
        OnPlaceholderDestroy, OnVivoVasAlgoContextTransact);
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
    // getVersion / getInterfaceVersion -> 2
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    return st == STATUS_OK ? AParcel_writeInt32(out, 2) : st;
  }
  if (code == 0xfffffe) {
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    return st == STATUS_OK
               ? AParcel_writeString(out, kVivoVasAlgoInterfaceHash, 40)
               : st;
  }
  if (code == FIRST_CALL_TRANSACTION + 1 || code == FIRST_CALL_TRANSACTION + 3 ||
      code == FIRST_CALL_TRANSACTION + 8) {
    // connectService / destroyContext / EstablishConnection_VAS -> int32(0)
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    return st == STATUS_OK ? AParcel_writeInt32(out, 0) : st;
  }
  if (code == FIRST_CALL_TRANSACTION + 2) {
    // createContext -> IVASAlgoContext
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    return st == STATUS_OK
               ? AParcel_writeStrongBinder(out, GetOrCreateVivoVasAlgoContext())
               : st;
  }
  if (code == FIRST_CALL_TRANSACTION + 4) {
    // submitRequest -> VASResultIPC
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    return st == STATUS_OK ? WriteVivoVasResultIpc(out) : st;
  }
  if (code == FIRST_CALL_TRANSACTION + 5 || code == FIRST_CALL_TRANSACTION + 7 ||
      code == FIRST_CALL_TRANSACTION + 9) {
    // CreateContext / CreateContextForClient / CreateContext_VAS -> null IAlgoContext
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    return st == STATUS_OK ? AParcel_writeStrongBinder(out, nullptr) : st;
  }
  return AParcel_writeStatusHeader(out, ok.get());
}

// Nothing OS 原生相机 UFSManager 连接 vendor.noth.hardware.camera.INtCamService/default
// 并调用 openSession() 获取 INtCamSession；若返回 null 会在 UFSService.onUnbind 时对
// INtCamSession.setParameters 抛 NPE 崩溃。此处实现最小合规 VINTF 会话桩。
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
  // registerCallback(1)/setParameters(2)/addMetaData(3)/processImgRequest(4)/closeSession(5) 均为 void
  return AParcel_writeStatusHeader(out, ok.get());
}

AIBinder* GetOrCreateNothingCamSession() {
  if (gNothingCamSessionBinder == nullptr) {
    static AIBinder_Class* sessClass = AIBinder_Class_define(
        kNothingCamSessionDescriptor, OnPlaceholderCreate,
        OnPlaceholderDestroy, OnNothingCamSessionTransact);
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
    // openSession() -> INtCamSession
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

std::string TrimWhitespace(const std::string& input) {
  const std::string::size_type first = input.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return "";
  }
  const std::string::size_type last = input.find_last_not_of(" \t\r\n");
  return input.substr(first, last - first + 1);
}

// 按行读取配置文件（C stdio，避免依赖高版本 libc++ iostream），为未实现接口注册空桩。
int RegisterPlaceholdersFromFile(const char* path) {
  FILE* input = fopen(path, "re");
  if (input == nullptr) {
    return 0;
  }
  static std::map<std::string, AIBinder_Class*> class_cache;
  static std::vector<AIBinder*> placeholder_refs;

  char* raw = nullptr;
  size_t capacity = 0;
  ssize_t length = 0;
  int registered = 0;
  while ((length = getline(&raw, &capacity, input)) >= 0) {
    const std::string name =
        TrimWhitespace(std::string(raw, static_cast<size_t>(length)));
    if (name.empty() || name[0] == '#') {
      continue;
    }
    if (AIBinder* existing = AServiceManager_checkService(name.c_str())) {
      AIBinder_decStrong(existing);
      continue;
    }
    const std::string::size_type slash = name.find('/');
    const std::string descriptor =
        slash == std::string::npos ? name : name.substr(0, slash);
    if (descriptor.empty()) {
      continue;
    }
    auto it = class_cache.find(descriptor);
    if (it == class_cache.end()) {
      it = class_cache.emplace(descriptor, nullptr).first;
      const bool has_custom_handler =
          (descriptor == kComposerExtDescriptor) ||
          (descriptor == kHonorNxpNfcDescriptor) ||
          (descriptor == kVivoVasAlgoProviderDescriptor) ||
          (descriptor == kNothingCamServiceDescriptor);
      const AIBinder_Class_onTransact transact_fn =
          (descriptor == kComposerExtDescriptor)
              ? OnComposerExtTransact
              : ((descriptor == kHonorNxpNfcDescriptor)
                     ? OnHonorNxpNfcTransact
                     : ((descriptor == kVivoVasAlgoProviderDescriptor)
                            ? OnVivoVasAlgoProviderTransact
                            : ((descriptor == kNothingCamServiceDescriptor)
                                   ? OnNothingCamServiceTransact
                                   : OnPlaceholderTransact)));
      it->second = AIBinder_Class_define(
          it->first.c_str(), OnPlaceholderCreate, OnPlaceholderDestroy,
          transact_fn);
      if (it->second == nullptr) {
        __android_log_print(ANDROID_LOG_WARN, kLogTag,
                            "define placeholder binder class failed: %s",
                            descriptor.c_str());
        class_cache.erase(it);
        continue;
      }
      if (!has_custom_handler) {
        AIBinder_Class_disableInterfaceTokenHeader(it->second);
      }
    }
    AIBinder* binder = AIBinder_new(it->second, nullptr);
    if (binder == nullptr) {
      __android_log_print(ANDROID_LOG_WARN, kLogTag,
                          "allocate placeholder binder failed: %s",
                          name.c_str());
      continue;
    }
    AIBinder_markVintfStability(binder);
    const binder_exception_t status =
        AServiceManager_addService(binder, name.c_str());
    if (status != EX_NONE) {
      __android_log_print(ANDROID_LOG_WARN, kLogTag,
                          "register placeholder service failed: %s (%d)",
                          name.c_str(), status);
      AIBinder_decStrong(binder);
      continue;
    }
    placeholder_refs.push_back(binder);
    ++registered;
  }
  free(raw);
  fclose(input);
  __android_log_print(ANDROID_LOG_INFO, kLogTag,
                      "registered %d nn placeholder services from %s",
                      registered, path);
  return registered;
}

}  // namespace

int main(int argc, char** argv) {
  // 支持通过 argv[1] 指定实例名（如 mtk-neuron_shim），默认仍为 default
  std::string serviceName = kNeuralNetworksServiceName;
  if (argc > 1 && argv[1] != nullptr && argv[1][0] != '\0') {
    serviceName =
        std::string(::aidl::android::hardware::neuralnetworks::IDevice::descriptor) +
        "/" + argv[1];
  }
  ABinderProcess_setThreadPoolMaxThreadCount(4);
  ABinderProcess_startThreadPool();
  auto svc = ndk::SharedRefBase::make<DeviceService>();
  const binder_exception_t main_status =
      AServiceManager_addService(svc->asBinder().get(), serviceName.c_str());
  if (main_status != EX_NONE) {
    __android_log_print(ANDROID_LOG_WARN, kLogTag,
                        "primary NN service %s not registered (%d); continuing for placeholders",
                        serviceName.c_str(), main_status);
  }
  const int placeholders =
      RegisterPlaceholdersFromFile("/vendor/etc/cdh_nn_placeholders.txt");
  if (main_status != EX_NONE && placeholders <= 0) {
    return 2;
  }
  ABinderProcess_joinThreadPool();
  return 0;
}
