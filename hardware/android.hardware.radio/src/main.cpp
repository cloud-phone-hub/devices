// Copyright (c) 2025-2026 fei_cong(https://github.com/feicong/feicong-course)
#include "channel.h"
#include "radio_config.h"
#include "radio_data.h"
#include "radio_messaging.h"
#include "radio_modem.h"
#if CDH_RADIO_MTK
#include "radio_mtkex_sim.h"
#endif
#include "radio_network.h"
#include "radio_sim.h"
#include "radio_service.h"
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

void *OnPlaceholderCreate(void * /*args*/) {
  return reinterpret_cast<void *>(1);
}

void OnPlaceholderDestroy(void * /*user_data*/) {}

binder_status_t OnPlaceholderTransact(AIBinder * /*binder*/,
                                      transaction_code_t /*code*/,
                                      const AParcel * /*in*/,
                                      AParcel * /*out*/) {
  return STATUS_UNKNOWN_TRANSACTION;
}

// 展锐 Radio/IMS AIDL 客户端会将 UNKNOWN_TRANSACTION 视为接口未实现而置空代理并同步
// 重复 sendToTarget 导致崩溃（最终触发 RescueParty 重启）；这里按其 Stub$Proxy
// 的最小协议契约应答 EX_NONE 与版本/空串。
binder_status_t OnUnisocRadioPlaceholderTransact(AIBinder * /*binder*/,
                                                 transaction_code_t code,
                                                 const AParcel * /*in*/,
                                                 AParcel *out) {
  if (out == nullptr) {
    return STATUS_OK;
  }
  ndk::ScopedAStatus ok = ndk::ScopedAStatus::ok();
  if (code == 0xffffff) {
    // getInterfaceVersion -> 2（对应 EXT_RADIO_HAL_VERSION_2_1 / IMS_RADIO_HAL_VERSION_2_1）
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    if (st != STATUS_OK) {
      return st;
    }
    return AParcel_writeInt32(out, 2);
  }
  if (code == 0xfffffe) {
    // getInterfaceHash -> "notfrozen"
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    if (st != STATUS_OK) {
      return st;
    }
    return AParcel_writeString(out, "notfrozen", 9);
  }
  if (code >= FIRST_CALL_TRANSACTION) {
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    if (st != STATUS_OK) {
      return st;
    }
    return AParcel_writeString(out, "", 0);
  }
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

// 按行读取配置文件，为机型专属未实现 AIDL 名称注册通用空桩 binder（配置为空或缺失时跳过）
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
    if (descriptor == "vendor.qti.hardware.radio.qtiradio.IQtiRadioStable") {
      descriptor = "vendor.qti.hardware.radio.qtiradio.IQtiRadio";
    }
    auto it = class_cache.find(descriptor);
    if (it == class_cache.end()) {
      // map 节点保证 key 地址稳定，先插入再传入 interfaceDescriptor
      it = class_cache.emplace(descriptor, nullptr).first;
      const AIBinder_Class_onTransact transact_fn =
          (descriptor.rfind("vendor.unisoc.hardware.radio.", 0) == 0)
              ? OnUnisocRadioPlaceholderTransact
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
      AIBinder_Class_disableInterfaceTokenHeader(it->second);
    }
    AIBinder *binder = AIBinder_new(it->second, nullptr);
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
  std::fclose(file);
  __android_log_print(ANDROID_LOG_INFO, kLogTag,
                      "registered %d radio placeholder services from %s",
                      registered, path);
}

} // namespace

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

  // dsds 双槽注册：gold 真机每个 radio 接口都有 slot1/slot2 实例（框架按 modem
  // 数建两条 Phone，各自连自己的 slot 实例），radioex 桩同样两槽。
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
    // radioex sim 用真实现注册：MtkRIL 取到代理后会回填响应函数并接收
    // iccidChanged，这是 MTK 侧订阅门禁的唯一解锁点。
    if (!RegisterService(mtkex_sim,
                         ("vendor.mediatek.hardware.mtkradioex.sim."
                          "IMtkRadioExSim" +
                          suffix)
                             .c_str())) {
      channel.Stop();
      return 1;
    }

    // MTK radioex 别名注册（sim 除外，sim 是真实现）：gold(MIUI/MTK) 的 MtkRIL 有
    // 两个机制依赖这些 vendor.mediatek.hardware.mtkradioex.* AIDL 名字：
    //   1) isRildReady() 用 ServiceManager.checkService 轮询这些名字，要求都可
    //      解锁才继续初始化 —— 因此必须在 servicemanager 注册（主 servicemanager
    //      只按名返回 binder，不校验接口描述符；别名注册即用标准 AIDL 服务 binder
    //      以 radioex 名再注册一份）。
    //   2) getMtkRadioExServiceProxy() 用 waitForDeclaredService 取代理（要求 VINTF
    //      declared）。别名（标准 radio binder 挂在 radioex 名下）【不可】声明：
    //      一旦 declared，waitForDeclaredService 返回它并当作 radioex 接口调用，
    //      事务码整体错位（radioex 的 setResponseFunctionsMtk=37 撞上 IRadioSim 的
    //      真实方法），com.android.phone 会反复崩溃。故 data/messaging/modem/
    //      network/voice 只做别名、不声明：checkService 命中（isRildReady 解锁）、
    //      waitForDeclaredService 落空（MtkRIL 跳过这些 radioex 分支持续走标准
    //      android.hardware.radio.* 接口）。
    //   例外是 sim：MTK 的 MtkUiccController.mIccid[] 只有 iccidChanged 一条写入
    //      路径，取不到 iccid 会卡死 SMSVC 的订阅门禁 checkAllIccIdReady。所以 sim
    //      这个名字上挂的是真实现（BnMtkRadioExSim，事务码与服务端一一对应）并在
    //      VINTF 声明，让 MtkRIL 正常握手。
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
