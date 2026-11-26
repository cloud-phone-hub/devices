// engine/vendor/hardware 线 —— android.hardware.bluetooth@samsung 变体实现。
//
// 同进程注册 vendor.samsung.hardware.bluetooth.ISehBluetooth/default（域 hal_bluetooth_default，
// 服务上下文 u:object_r:hal_bluetooth_service:s0），解决 OneUI 打开蓝牙时 system_server
// 阻塞死等 ISehBluetooth/default（10s 超时 + ctl.interface_start 风暴）的问题。
//
// 契约取证自 guest /apex/com.android.bt/javalib/service-bluetooth.jar：
//   - Descriptor: "vendor.samsung.hardware.bluetooth.ISehBluetooth"
//   - TX 1: boolean writeFile(byte type, String data)
//   - TX 2: String  readFile(byte type)
//   - TX 3: boolean setProperty(byte key, String val)
//   - TX 4: String  getProperty(byte key)

#include "samsung/bt_samsung_ext.h"

#include <android/binder_auto_utils.h>
#include <android/binder_ibinder.h>
#include <android/binder_manager.h>
#include <android/binder_parcel.h>
#include <android/binder_status.h>
#include <android/log.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <string>

extern "C" {
void AIBinder_markVintfStability(AIBinder *binder);
}

namespace cdh::bluetooth {
namespace {

constexpr const char *kLogTag = "cdh-bt-samsung";
constexpr const char *kSehBtDescriptor =
    "vendor.samsung.hardware.bluetooth.ISehBluetooth";
constexpr const char *kSehBtServiceName =
    "vendor.samsung.hardware.bluetooth.ISehBluetooth/default";
constexpr const char *kSehBtHash = "notfrozen";

bool StdStringAllocator(void *stringData, int32_t length, char **buffer) {
  if (length < 0) {
    *buffer = nullptr;
    return true;
  }
  auto *out = static_cast<std::string *>(stringData);
  out->assign(static_cast<size_t>(length), '\0');
  *buffer = out->data();
  return true;
}

bool ReadStdString(const AParcel *in, std::string *out) {
  out->clear();
  return AParcel_readString(in, out, StdStringAllocator) == STATUS_OK;
}

struct SehBluetoothState {
  std::mutex mu;
  std::map<int8_t, std::string> props;
  std::map<int8_t, std::string> files;
};

void *OnSehBtCreate(void *args) { return args; }

void OnSehBtDestroy(void *userData) {
  delete static_cast<SehBluetoothState *>(userData);
}

binder_status_t OnSehBtTransact(AIBinder *binder, transaction_code_t code,
                                const AParcel *in, AParcel *out) {
  ndk::ScopedAStatus ok = ndk::ScopedAStatus::ok();
  if (code == 0xffffff) {
    if (out == nullptr) return STATUS_OK;
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    return st != STATUS_OK ? st : AParcel_writeInt32(out, 1);
  }
  if (code == 0xfffffe) {
    if (out == nullptr) return STATUS_OK;
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    return st != STATUS_OK
               ? st
               : AParcel_writeString(out, kSehBtHash,
                                     static_cast<int32_t>(std::strlen(kSehBtHash)));
  }

  auto *state = static_cast<SehBluetoothState *>(AIBinder_getUserData(binder));
  if (state == nullptr || in == nullptr || out == nullptr) {
    return STATUS_UNKNOWN_TRANSACTION;
  }

  switch (code) {
    case 1: {  // boolean writeFile(byte type, String data)
      int8_t type = 0;
      std::string data;
      if (AParcel_readByte(in, &type) != STATUS_OK) return STATUS_BAD_VALUE;
      ReadStdString(in, &data);
      {
        std::lock_guard<std::mutex> lock(state->mu);
        state->files[type] = std::move(data);
      }
      binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
      return st != STATUS_OK ? st : AParcel_writeBool(out, true);
    }
    case 2: {  // String readFile(byte type)
      int8_t type = 0;
      if (AParcel_readByte(in, &type) != STATUS_OK) return STATUS_BAD_VALUE;
      std::string val;
      {
        std::lock_guard<std::mutex> lock(state->mu);
        auto it = state->files.find(type);
        if (it != state->files.end()) {
          val = it->second;
        }
      }
      binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
      return st != STATUS_OK
                 ? st
                 : AParcel_writeString(out, val.c_str(),
                                       static_cast<int32_t>(val.size()));
    }
    case 3: {  // boolean setProperty(byte key, String val)
      int8_t key = 0;
      std::string val;
      if (AParcel_readByte(in, &key) != STATUS_OK) return STATUS_BAD_VALUE;
      ReadStdString(in, &val);
      {
        std::lock_guard<std::mutex> lock(state->mu);
        state->props[key] = std::move(val);
      }
      binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
      return st != STATUS_OK ? st : AParcel_writeBool(out, true);
    }
    case 4: {  // String getProperty(byte key)
      int8_t key = 0;
      if (AParcel_readByte(in, &key) != STATUS_OK) return STATUS_BAD_VALUE;
      std::string val;
      {
        std::lock_guard<std::mutex> lock(state->mu);
        auto it = state->props.find(key);
        if (it != state->props.end()) {
          val = it->second;
        }
      }
      binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
      return st != STATUS_OK
                 ? st
                 : AParcel_writeString(out, val.c_str(),
                                       static_cast<int32_t>(val.size()));
    }
    default:
      return STATUS_UNKNOWN_TRANSACTION;
  }
}

}  // namespace

bool RegisterSamsungBluetooth() {
  static AIBinder_Class *clazz = AIBinder_Class_define(
      kSehBtDescriptor, OnSehBtCreate, OnSehBtDestroy, OnSehBtTransact);
  if (clazz == nullptr) {
    __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                        "define ISehBluetooth class failed");
    return false;
  }
  auto *state = new SehBluetoothState();
  AIBinder *binder = AIBinder_new(clazz, state);
  if (binder == nullptr) {
    delete state;
    __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                        "allocate ISehBluetooth binder failed");
    return false;
  }
  AIBinder_markVintfStability(binder);
  const binder_exception_t st =
      AServiceManager_addService(binder, kSehBtServiceName);
  if (st != EX_NONE) {
    __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                        "addService(%s) failed: %d", kSehBtServiceName, st);
    AIBinder_decStrong(binder);
    return false;
  }
  // 常驻引用随进程存活。
  static AIBinder *gBinderRef = binder;
  (void)gBinderRef;
  __android_log_print(ANDROID_LOG_INFO, kLogTag, "registered %s",
                      kSehBtServiceName);
  return true;
}

}  // namespace cdh::bluetooth
