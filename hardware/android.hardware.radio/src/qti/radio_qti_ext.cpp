// android.hardware.radio 的 qti 变体（高通 QTI 扩展占位适配）。
// 只在 --variant qti（-DCDH_RADIO_QTI=1）时编入。
#include "radio_placeholder_ext.h"

#include <android/binder_auto_utils.h>
#include <android/binder_ibinder.h>
#include <android/binder_parcel.h>
#include <android/binder_status.h>
#include <android/log.h>

#include <cstdint>
#include <mutex>
#include <string>

namespace cdh::radio {
namespace {

constexpr char kLogTag[] = "cdh-radio-qti";
constexpr char kQtiOemHookDescriptor[] =
    "vendor.qti.hardware.radio.qcrilhook.IQtiOemHook";
constexpr char kQtiOemHookRespDescriptor[] =
    "vendor.qti.hardware.radio.qcrilhook.IQtiOemHookResponse";
constexpr char kQtiOemHookHash[] =
    "b291bffe971400c6f62a00cbe3032427743e4d92";

struct QtiOemHookPlaceholderState {
  AIBinder_Class *resp_class = nullptr;
  std::mutex mu;
  AIBinder *resp_binder = nullptr;

  ~QtiOemHookPlaceholderState() {
    if (resp_binder != nullptr) {
      AIBinder_decStrong(resp_binder);
      resp_binder = nullptr;
    }
  }
};

void OnQtiOemHookDestroy(void *user_data) {
  delete static_cast<QtiOemHookPlaceholderState *>(user_data);
}

// 高通 IQtiOemHook 占位服务端：
// QtiPhoneSwitcher.queryMaxDataAllowed() 在 com.android.phone 主线程同步调用
// IQcrilMsgTunnel.sendOemRilRequestRaw() -> IQtiOemHook.oemHookRawRequest(oneway)
// 并阻塞等待 IQtiOemHookResponse.oemHookRawResponse() 回调。若仅注册通用空桩，
// com.android.phone 主线程会永久死锁，导致高通机型 SIM 停在 UNKNOWN、Radio 停在
// POWER_OFF。这里保存回调代理并立刻回报 REQUEST_NOT_SUPPORTED (6) + 空字节数组。
binder_status_t OnQtiOemHookTransact(AIBinder *binder,
                                     transaction_code_t code,
                                     const AParcel *in,
                                     AParcel *out) {
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
               : AParcel_writeString(out, kQtiOemHookHash,
                                     sizeof(kQtiOemHookHash) - 1);
  }

  auto *state =
      static_cast<QtiOemHookPlaceholderState *>(AIBinder_getUserData(binder));
  if (state != nullptr && in != nullptr) {
    if (code == 1) {
      // setCallback(IQtiOemHookResponse, IQtiOemHookIndication)
      AIBinder *resp = nullptr;
      if (AParcel_readStrongBinder(in, &resp) == STATUS_OK &&
          resp != nullptr) {
        if (state->resp_class != nullptr) {
          AIBinder_associateClass(resp, state->resp_class);
        }
        std::lock_guard<std::mutex> lock(state->mu);
        if (state->resp_binder != nullptr) {
          AIBinder_decStrong(state->resp_binder);
        }
        state->resp_binder = resp;
        __android_log_print(ANDROID_LOG_INFO, kLogTag,
                            "qti oemhook placeholder bound response");
      }
      if (out != nullptr) {
        return AParcel_writeStatusHeader(out, ok.get());
      }
      return STATUS_OK;
    }
    if (code == 2) {
      // oemHookRawRequest(int serial, byte[] data)
      std::int32_t serial = 0;
      if (AParcel_readInt32(in, &serial) == STATUS_OK) {
        AIBinder *resp = nullptr;
        {
          std::lock_guard<std::mutex> lock(state->mu);
          if (state->resp_binder != nullptr) {
            resp = state->resp_binder;
            AIBinder_incStrong(resp);
          }
        }
        if (resp != nullptr) {
          AParcel *cb_in = nullptr;
          if (AIBinder_prepareTransaction(resp, &cb_in) == STATUS_OK &&
              cb_in != nullptr) {
            // oemHookRawResponse(int serial, int error, byte[] data)
            // RadioError.REQUEST_NOT_SUPPORTED = 6；data 必须为非空 byte[0]
            const int8_t empty = 0;
            binder_status_t wst = AParcel_writeInt32(cb_in, serial);
            if (wst == STATUS_OK) wst = AParcel_writeInt32(cb_in, 6);
            if (wst == STATUS_OK) {
              wst = AParcel_writeByteArray(cb_in, &empty, 0);
            }
            if (wst == STATUS_OK) {
              AParcel *cb_out = nullptr;
              binder_status_t tst =
                  AIBinder_transact(resp, 1, &cb_in, &cb_out, FLAG_ONEWAY);
              if (cb_out != nullptr) {
                AParcel_delete(cb_out);
              }
              if (tst != STATUS_OK) {
                __android_log_print(
                    ANDROID_LOG_WARN, kLogTag,
                    "qti oemhook response transact failed: serial=%d st=%d",
                    serial, tst);
              }
            } else {
              AParcel_delete(cb_in);
            }
          }
          AIBinder_decStrong(resp);
        }
      }
      if (out != nullptr) {
        return AParcel_writeStatusHeader(out, ok.get());
      }
      return STATUS_OK;
    }
  }
  return out == nullptr ? STATUS_OK : STATUS_UNKNOWN_TRANSACTION;
}

} // namespace

void NormalizeVendorPlaceholderDescriptor(std::string *descriptor) {
  if (descriptor != nullptr &&
      *descriptor == "vendor.qti.hardware.radio.qtiradio.IQtiRadioStable") {
    *descriptor = "vendor.qti.hardware.radio.qtiradio.IQtiRadio";
  }
}

bool ResolveVendorPlaceholderBinding(const std::string &descriptor,
                                     PlaceholderBinding *out) {
  if (out == nullptr || descriptor != kQtiOemHookDescriptor) {
    return false;
  }
  auto *st = new QtiOemHookPlaceholderState();
  st->resp_class = GetOrDefineRemoteCallbackClass(kQtiOemHookRespDescriptor);
  out->on_destroy = OnQtiOemHookDestroy;
  out->on_transact = OnQtiOemHookTransact;
  out->disable_token_header = false;
  out->user_args = st;
  out->free_user_args = OnQtiOemHookDestroy;
  return true;
}

} // namespace cdh::radio
