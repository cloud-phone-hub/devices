// engine/vendor/hardware 线 —— android.hardware.neuralnetworks（IDevice v4）自研服务实现。
//
// 注册 android.hardware.neuralnetworks.IDevice/default 并承载 /vendor/etc/cdh_nn_placeholders.txt
// 声明的厂商占位服务。厂商专属协议分支均拆分至变体目录（src/{mtk,honor,vivo,nothing}/），
// 基础 default 版本零跨厂商代码。

#include "nn_vendor_ext.h"
#if CDH_NN_QTI
#include "qti/nn_qti_ext.h"
#endif

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

namespace {

constexpr const char* kNoDevice = "no neural network accelerator backend";
constexpr const char* kLogTag = "cdh-nn";

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
    return ScopedAStatus::ok();
  }

  ScopedAStatus getSupportedOperations(
      const Model& /*in_model*/,
      std::vector<bool>* _aidl_return) override {
    _aidl_return->clear();
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

std::string TrimWhitespace(const std::string& input) {
  const std::string::size_type first = input.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return "";
  }
  const std::string::size_type last = input.find_last_not_of(" \t\r\n");
  return input.substr(first, last - first + 1);
}

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
      const AIBinder_Class_onTransact custom_fn =
          cdh::nn::ResolveVendorPlaceholderTransact(descriptor);
      const AIBinder_Class_onTransact transact_fn =
          custom_fn != nullptr ? custom_fn : OnPlaceholderTransact;
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
      if (custom_fn == nullptr) {
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
  int extraRegistered = 0;
#if CDH_NN_QTI
  // 高通变体：在占位服务扫描前先以真 AIDL NDK stub 注册 vendor.qti.diaghal.Idiag/default。
  auto qtiDiagHal = cdh::nn::RegisterQtiDiagHal();
  if (qtiDiagHal != nullptr) {
    ++extraRegistered;
  }
#endif
  const int placeholders =
      RegisterPlaceholdersFromFile("/vendor/etc/cdh_nn_placeholders.txt");
  if (main_status != EX_NONE && placeholders <= 0 && extraRegistered <= 0) {
    return 2;
  }
  ABinderProcess_joinThreadPool();
  return 0;
}
