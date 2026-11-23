// Copyright (c) 2025-2026 fei_cong(https://github.com/feicong/feicong-course)
//
// android.hardware.radio 主入口（纯 AOSP 基础版 + 平台变体扩展）：
//   - default：纯标准 AOSP IRadio* + 通用空桩；
//   - mtk（--variant mtk，-DCDH_RADIO_MTK=1）：src/mtk/（IMtkRadioExSim 真实现 + radioex 别名）；
//   - qti（--variant qti，-DCDH_RADIO_QTI=1）：src/qti/（IQtiOemHook 回调占位与 IQtiRadioStable 映射）；
//   - unisoc（--variant unisoc，-DCDH_RADIO_UNISOC=1）：src/unisoc/（IExtRadio* 回调占位）。
#include "channel.h"
#include "radio_config.h"
#include "radio_data.h"
#include "radio_messaging.h"
#include "radio_modem.h"
#if CDH_RADIO_MTK
#include "radio_mtkex_sim.h"
#endif
#include "radio_network.h"
#include "radio_placeholder_ext.h"
#include "radio_service.h"
#include "radio_sim.h"
#include "radio_voice.h"

#include <android/binder_auto_utils.h>
#include <android/binder_ibinder.h>
#include <android/binder_manager.h>
#include <android/binder_parcel.h>
#include <android/binder_process.h>
#include <android/binder_status.h>
#include <android/log.h>

#include <cstdio>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

extern "C" {
void AIBinder_markVintfStability(AIBinder *binder);
void AIBinder_Class_disableInterfaceTokenHeader(AIBinder_Class *clazz);
}

namespace {

constexpr char kLogTag[] = "cdh-radio";

template <typename Interface>
bool RegisterService(const std::shared_ptr<Interface> &service,
                     const char *name) {
  if (service == nullptr)
    return false;
  const binder_exception_t result =
      AServiceManager_addService(service->asBinder().get(), name);
  if (result != EX_NONE) {
    __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                        "register service failed: %s (%d)", name, result);
    return false;
  }
  return true;
}

void *OnPlaceholderCreate(void *args) {
  return args != nullptr ? args : reinterpret_cast<void *>(1);
}

void OnPlaceholderDestroy(void * /*user_data*/) {}

binder_status_t OnPlaceholderTransact(AIBinder * /*binder*/,
                                      transaction_code_t /*code*/,
                                      const AParcel * /*in*/,
                                      AParcel * /*out*/) {
  return STATUS_UNKNOWN_TRANSACTION;
}

std::string TrimWhitespace(const std::string &input) {
  const std::string::size_type first = input.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return "";
  }
  const std::string::size_type last = input.find_last_not_of(" \t\r\n");
  return input.substr(first, last - first + 1);
}

// 走 C stdio 而不是 std::ifstream：android14 线链接 lineage-21.0 的 libc++.so
// 不导出 basic_filebuf / basic_ifstream，用 std::ifstream 会在链接期报 undefined symbol。
bool ReadNextLine(std::FILE *file, std::string *out) {
  out->clear();
  char buf[4096];
  while (std::fgets(buf, sizeof(buf), file) != nullptr) {
    out->append(buf);
    if (!out->empty() && out->back() == '\n') {
      return true;
    }
  }
  return !out->empty();
}

// 按行读取配置文件，为机型专属未实现 AIDL 名称注册空桩或变体扩展 binder。
void RegisterPlaceholdersFromFile(const char *path) {
  std::FILE *file = std::fopen(path, "r");
  if (file == nullptr) {
    return;
  }
  static std::map<std::string, AIBinder_Class *> class_cache;
  static std::vector<AIBinder *> placeholder_refs;

  std::string line;
  int registered = 0;
  while (ReadNextLine(file, &line)) {
    const std::string name = TrimWhitespace(line);
    if (name.empty() || name[0] == '#') {
      continue;
    }
    if (AIBinder *existing = AServiceManager_checkService(name.c_str())) {
      AIBinder_decStrong(existing);
      continue;
    }
    const std::string::size_type slash = name.find('/');
    std::string descriptor =
        slash == std::string::npos ? name : name.substr(0, slash);
    if (descriptor.empty()) {
      continue;
    }
    cdh::radio::NormalizeVendorPlaceholderDescriptor(&descriptor);

    cdh::radio::PlaceholderBinding binding;
    const bool has_vendor_binding =
        cdh::radio::ResolveVendorPlaceholderBinding(descriptor, &binding);

    auto it = class_cache.find(descriptor);
    if (it == class_cache.end()) {
      // map 节点保证 key 地址稳定，先插入再传入 interfaceDescriptor
      it = class_cache.emplace(descriptor, nullptr).first;
      const AIBinder_Class_onDestroy destroy_fn =
          (has_vendor_binding && binding.on_destroy != nullptr)
              ? binding.on_destroy
              : OnPlaceholderDestroy;
      const AIBinder_Class_onTransact transact_fn =
          (has_vendor_binding && binding.on_transact != nullptr)
              ? binding.on_transact
              : OnPlaceholderTransact;
      it->second = AIBinder_Class_define(
          it->first.c_str(), OnPlaceholderCreate, destroy_fn, transact_fn);
      if (it->second == nullptr) {
        if (binding.user_args != nullptr && binding.free_user_args != nullptr) {
          binding.free_user_args(binding.user_args);
        }
        __android_log_print(ANDROID_LOG_WARN, kLogTag,
                            "define placeholder binder class failed: %s",
                            descriptor.c_str());
        class_cache.erase(it);
        continue;
      }
      if (!has_vendor_binding || binding.disable_token_header) {
        AIBinder_Class_disableInterfaceTokenHeader(it->second);
      }
    }
    AIBinder *binder = AIBinder_new(it->second, binding.user_args);
    if (binder == nullptr) {
      if (binding.user_args != nullptr && binding.free_user_args != nullptr) {
        binding.free_user_args(binding.user_args);
      }
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
  std::fclose(file);
  __android_log_print(ANDROID_LOG_INFO, kLogTag,
                      "registered %d radio placeholder services from %s",
                      registered, path);
}

} // namespace

namespace cdh::radio {

AIBinder_Class *GetOrDefineRemoteCallbackClass(const char *resp_descriptor) {
  static std::mutex mu;
  static std::map<std::string, AIBinder_Class *> cache;
  std::lock_guard<std::mutex> lock(mu);
  auto it = cache.find(resp_descriptor);
  if (it != cache.end()) {
    return it->second;
  }
  it = cache.emplace(resp_descriptor, nullptr).first;
  it->second = AIBinder_Class_define(
      it->first.c_str(), OnPlaceholderCreate, OnPlaceholderDestroy,
      OnPlaceholderTransact);
  return it->second;
}

} // namespace cdh::radio

int main() {
  cdh::radio::StateStore state_store;
  // v3 顺延 virtconsole 布局：radio = hvc4（hvc0/1 留给固件 console，hvc2
  // 空置预留，keymint 占 Cuttlefish 基线编号 hvc3，其后通道整体 +1 顺延）
  cdh::radio::Channel channel("/dev/hvc4");
  std::string error;
  if (!channel.Start(&state_store, &error)) {
    __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                        "start guest channel failed: %s", error.c_str());
    return 1;
  }

  if (!ABinderProcess_setThreadPoolMaxThreadCount(8)) {
    __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                        "configure binder thread pool failed");
    return 1;
  }

  const auto config = cdh::radio::CreateRadioConfig(channel, state_store);
  for (std::int32_t slot = 0; slot < cdh::radio::kLogicalModemCount; ++slot) {
    const std::string suffix = "/slot" + std::to_string(slot + 1);
    const auto data = cdh::radio::CreateRadioData(channel, state_store, slot);
    const auto messaging =
        cdh::radio::CreateRadioMessaging(channel, state_store, slot);
    const auto modem = cdh::radio::CreateRadioModem(channel, state_store, slot);
    const auto network =
        cdh::radio::CreateRadioNetwork(channel, state_store, slot);
    const auto sim = cdh::radio::CreateRadioSim(channel, state_store, slot);
#if CDH_RADIO_MTK
    const auto mtkex_sim =
        cdh::radio::CreateMtkRadioExSim(channel, state_store, slot);
#endif
    const auto voice = cdh::radio::CreateRadioVoice(channel, state_store, slot);

    if (!RegisterService(
            data, ("android.hardware.radio.data.IRadioData" + suffix).c_str()) ||
        !RegisterService(messaging,
                         ("android.hardware.radio.messaging.IRadioMessaging" +
                          suffix)
                             .c_str()) ||
        !RegisterService(modem,
                         ("android.hardware.radio.modem.IRadioModem" + suffix)
                             .c_str()) ||
        !RegisterService(
            network,
            ("android.hardware.radio.network.IRadioNetwork" + suffix).c_str()) ||
        !RegisterService(sim,
                         ("android.hardware.radio.sim.IRadioSim" + suffix)
                             .c_str()) ||
        !RegisterService(voice,
                         ("android.hardware.radio.voice.IRadioVoice" + suffix)
                             .c_str())) {
      channel.Stop();
      return 1;
    }

#if CDH_RADIO_MTK
    if (!RegisterService(mtkex_sim,
                         ("vendor.mediatek.hardware.mtkradioex.sim."
                          "IMtkRadioExSim" +
                          suffix)
                             .c_str())) {
      channel.Stop();
      return 1;
    }
    RegisterService(config,
                    ("vendor.mediatek.hardware.mtkradioex.data.IMtkRadioExData" +
                     suffix)
                        .c_str());
    RegisterService(messaging,
                    ("vendor.mediatek.hardware.mtkradioex.messaging."
                     "IMtkRadioExMessaging" +
                     suffix)
                        .c_str());
    RegisterService(modem,
                    ("vendor.mediatek.hardware.mtkradioex.modem."
                     "IMtkRadioExModem" +
                     suffix)
                        .c_str());
    RegisterService(network,
                    ("vendor.mediatek.hardware.mtkradioex.network."
                     "IMtkRadioExNetwork" +
                     suffix)
                        .c_str());
    RegisterService(voice,
                    ("vendor.mediatek.hardware.mtkradioex.voice."
                     "IMtkRadioExVoice" +
                     suffix)
                        .c_str());
#endif // CDH_RADIO_MTK
  }

  if (!RegisterService(config,
                       "android.hardware.radio.config.IRadioConfig/default")) {
    channel.Stop();
    return 1;
  }

  RegisterPlaceholdersFromFile("/vendor/etc/cdh_radio_placeholders.txt");

  ABinderProcess_startThreadPool();
  ABinderProcess_joinThreadPool();
  channel.Stop();
  return 1;
}
