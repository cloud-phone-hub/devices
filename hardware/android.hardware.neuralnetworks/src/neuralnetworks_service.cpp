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
      const AIBinder_Class_onTransact transact_fn =
          (descriptor == kComposerExtDescriptor) ? OnComposerExtTransact
                                                 : OnPlaceholderTransact;
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
      if (descriptor != kComposerExtDescriptor) {
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
