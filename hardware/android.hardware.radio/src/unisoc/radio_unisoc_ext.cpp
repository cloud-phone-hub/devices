// android.hardware.radio 的 unisoc 变体（展锐 IExtRadio* 与 Radio/IMS 占位适配）。
// 只在 --variant unisoc（-DCDH_RADIO_UNISOC=1）时编入。
#include "radio_placeholder_ext.h"

#include <android/binder_auto_utils.h>
#include <android/binder_ibinder.h>
#include <android/binder_parcel.h>
#include <android/binder_status.h>
#include <android/log.h>

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>

namespace cdh::radio {
namespace {

constexpr char kLogTag[] = "cdh-radio-unisoc";

// 展锐 IExtRadio*Response 回调参数类型枚举（与 AIDL Java Stub$onTransact 里的
// enforceNoDataAvail() 严格字节对齐）。
enum class UnisocRespTail {
  kNone,           // (ExtRadioResponseInfo)
  kInt32,          // (ExtRadioResponseInfo, int32)
  kInt32Int32,     // (ExtRadioResponseInfo, int32, int32)
  kString,         // (ExtRadioResponseInfo, String)
  kNullParcelable, // (ExtRadioResponseInfo, null Parcelable -> int32 0)
  kEmptyArray,     // (ExtRadioResponseInfo, empty non-null Array -> int32 0)
};

struct UnisocExtOp {
  transaction_code_t req_tx;
  transaction_code_t resp_tx;
  UnisocRespTail tail;
  std::int32_t error = 6;
};

struct UnisocExtSpec {
  const char *req_descriptor;
  const char *resp_descriptor;
  transaction_code_t set_resp_tx;
  const UnisocExtOp *ops;
  std::size_t op_count;
};

constexpr UnisocExtOp kUnisocSimOps[] = {
    {3, 1, UnisocRespTail::kInt32Int32},      // getFacilityLockForAppExt
    {4, 2, UnisocRespTail::kNullParcelable},  // getIccCardStatusExt
    {5, 3, UnisocRespTail::kEmptyArray},      // getSimCapacity
    {6, 4, UnisocRespTail::kEmptyArray, 0},   // getSimlockDummys
    {7, 5, UnisocRespTail::kInt32},           // getSimlockRemaintimes
    {8, 6, UnisocRespTail::kInt32},           // getSimlockStatus
    {9, 7, UnisocRespTail::kString},          // getSimlockWhitelist
    {10, 8, UnisocRespTail::kInt32},          // getSubsidyLockdyStatus
    {11, 9, UnisocRespTail::kNullParcelable}, // iccIOForAppExt
    {12, 10, UnisocRespTail::kString},        // queryPlmn
    {13, 11, UnisocRespTail::kNone},          // updatePlmnPriority
    {14, 12, UnisocRespTail::kInt32},         // setFacilityLock
    {15, 13, UnisocRespTail::kNone},          // setFacilityLockForUser
    {16, 14, UnisocRespTail::kString},        // simGetAtr
    {17, 15, UnisocRespTail::kEmptyArray},    // iccIOForAllFile
    {18, 16, UnisocRespTail::kString},        // readNvcodeFromMiscdata
    {19, 17, UnisocRespTail::kNone},          // setNvcodeToMiscdata
};

constexpr UnisocExtOp kUnisocModemOps[] = {
    {6, 1, UnisocRespTail::kString, 0}, // getCpComLogVersion
    {7, 2, UnisocRespTail::kString},    // getExceptionEvents
    {8, 3, UnisocRespTail::kString},    // getPcba
    {9, 4, UnisocRespTail::kNone},      // resetModem
    {10, 5, UnisocRespTail::kString},   // sendCmdAsync
    {11, 6, UnisocRespTail::kNone},     // setMNOParam
    {12, 7, UnisocRespTail::kInt32},    // getIdentityState
};

constexpr UnisocExtOp kUnisocNetworkOps[] = {
    {3, 1, UnisocRespTail::kNone},            // enableLTE
    {4, 2, UnisocRespTail::kInt32},           // getPreferredNetworkTypeExt
    {5, 3, UnisocRespTail::kInt32, 0},        // getVoLTEAllowedPLMN
    {6, 4, UnisocRespTail::kNullParcelable},  // requestLteSpeedAndSignalStrength
    {7, 5, UnisocRespTail::kNone},            // setPreferredNetworkTypeExt
    {8, 6, UnisocRespTail::kNone},            // updateOperatorName
    {9, 7, UnisocRespTail::kNone},            // setRadioPowerFallback
    {10, 8, UnisocRespTail::kString},         // getRadioPreference
    {11, 9, UnisocRespTail::kNone},           // setRadioPreference
    {12, 10, UnisocRespTail::kNone},          // enableNrSwitch
    {13, 11, UnisocRespTail::kNone},          // setSA
    {14, 12, UnisocRespTail::kInt32},         // getSA
    {15, 13, UnisocRespTail::kNone},          // enableSmartNrSwitch
    {16, 14, UnisocRespTail::kEmptyArray},    // getBigDataOfNetAndCall
    {17, 15, UnisocRespTail::kNone},          // setSCGAllow
    {18, 16, UnisocRespTail::kInt32},         // getNsaConnStatus
    {19, 17, UnisocRespTail::kNone},          // setRadioPowerFallbackWithType
    {20, 18, UnisocRespTail::kNullParcelable},// requestNrSpeedAndSignalStrength
    {21, 19, UnisocRespTail::kNone},          // setFastReturnNetwork
};

constexpr UnisocExtOp kUnisocDataOps[] = {
    {3, 1, UnisocRespTail::kNone},  // attachData
    {4, 2, UnisocRespTail::kNone},  // enableRauNotify
    {5, 3, UnisocRespTail::kNone},  // forceDeatch
    {6, 4, UnisocRespTail::kInt32}, // getActivedPdpCount
    {7, 5, UnisocRespTail::kNone},  // reAttach
    {8, 6, UnisocRespTail::kNone},  // setPsDataOff
    {9, 7, UnisocRespTail::kNone},  // setUsbShareStateSwitch
};

constexpr UnisocExtOp kUnisocMessagingOps[] = {
    {3, 1, UnisocRespTail::kInt32},  // getSmsBearer
    {4, 2, UnisocRespTail::kString}, // querySmsStorageMode
    {5, 3, UnisocRespTail::kNone},   // storeSmsToSim
    {6, 4, UnisocRespTail::kInt32},  // getTPMRState
};

constexpr UnisocExtOp kUnisocVoiceOps[] = {
    {3, 1, UnisocRespTail::kNone},           // explicitCallTransferExt
    {4, 2, UnisocRespTail::kNullParcelable}, // getNumberControl
    {5, 3, UnisocRespTail::kInt32},          // queryColp
    {6, 4, UnisocRespTail::kInt32},          // queryColr
    {7, 5, UnisocRespTail::kNone},           // setEmergencyOnly
    {8, 7, UnisocRespTail::kInt32Int32},     // getCnap
    {9, 6, UnisocRespTail::kNone},           // setImsUserAgent
    {10, 8, UnisocRespTail::kNone},          // setSSflag
    {11, 9, UnisocRespTail::kNone},          // setTPMRState
    {12, 10, UnisocRespTail::kNone},         // updateClip
    {13, 11, UnisocRespTail::kNone},         // updateEcclist
    {14, 12, UnisocRespTail::kNone},         // videoPhoneControlIFrame
    {15, 13, UnisocRespTail::kNone},         // videoPhoneFallback
    {16, 14, UnisocRespTail::kNone},         // videoPhoneLocalMedia
    {17, 15, UnisocRespTail::kNone},         // videoPhoneString
    {18, 16, UnisocRespTail::kInt32},        // getSpecialRatcap
    {19, 17, UnisocRespTail::kNone},         // setLocationInfo
    {20, 18, UnisocRespTail::kNone},         // setFdnList
    {21, 19, UnisocRespTail::kNone},         // sendScreenInteractionExtras
};

constexpr UnisocExtSpec kUnisocExtSpecs[] = {
    {"vendor.unisoc.hardware.radio.sim.IExtRadioSim",
     "vendor.unisoc.hardware.radio.sim.IExtRadioSimResponse", 2,
     kUnisocSimOps, sizeof(kUnisocSimOps) / sizeof(kUnisocSimOps[0])},
    {"vendor.unisoc.hardware.radio.modem.IExtRadioModem",
     "vendor.unisoc.hardware.radio.modem.IExtRadioModemResponse", 4,
     kUnisocModemOps, sizeof(kUnisocModemOps) / sizeof(kUnisocModemOps[0])},
    {"vendor.unisoc.hardware.radio.network.IExtRadioNetwork",
     "vendor.unisoc.hardware.radio.network.IExtRadioNetworkResponse", 2,
     kUnisocNetworkOps, sizeof(kUnisocNetworkOps) / sizeof(kUnisocNetworkOps[0])},
    {"vendor.unisoc.hardware.radio.data.IExtRadioData",
     "vendor.unisoc.hardware.radio.data.IExtRadioDataResponse", 2,
     kUnisocDataOps, sizeof(kUnisocDataOps) / sizeof(kUnisocDataOps[0])},
    {"vendor.unisoc.hardware.radio.messaging.IExtRadioMessaging",
     "vendor.unisoc.hardware.radio.messaging.IExtRadioMessagingResponse", 2,
     kUnisocMessagingOps,
     sizeof(kUnisocMessagingOps) / sizeof(kUnisocMessagingOps[0])},
    {"vendor.unisoc.hardware.radio.voice.IExtRadioVoice",
     "vendor.unisoc.hardware.radio.voice.IExtRadioVoiceResponse", 2,
     kUnisocVoiceOps, sizeof(kUnisocVoiceOps) / sizeof(kUnisocVoiceOps[0])},
};

const UnisocExtSpec *FindUnisocExtSpec(const std::string &descriptor) {
  for (const auto &spec : kUnisocExtSpecs) {
    if (descriptor == spec.req_descriptor) {
      return &spec;
    }
  }
  return nullptr;
}

struct UnisocExtPlaceholderState {
  const UnisocExtSpec *spec = nullptr;
  AIBinder_Class *resp_class = nullptr;
  std::mutex mu;
  AIBinder *resp_binder = nullptr;

  ~UnisocExtPlaceholderState() {
    if (resp_binder != nullptr) {
      AIBinder_decStrong(resp_binder);
      resp_binder = nullptr;
    }
  }
};

void OnUnisocExtDestroy(void *user_data) {
  delete static_cast<UnisocExtPlaceholderState *>(user_data);
}

void OnUnisocNoopDestroy(void * /*user_data*/) {}

binder_status_t OnUnisocExtTransact(AIBinder *binder,
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
    return st != STATUS_OK ? st : AParcel_writeString(out, "notfrozen", 9);
  }

  auto *state =
      static_cast<UnisocExtPlaceholderState *>(AIBinder_getUserData(binder));
  if (state != nullptr && state->spec != nullptr && in != nullptr) {
    if (code == state->spec->set_resp_tx) {
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
                            "unisoc ext placeholder bound response: %s",
                            state->spec->resp_descriptor);
      }
      if (out != nullptr) {
        return AParcel_writeStatusHeader(out, ok.get());
      }
      return STATUS_OK;
    }

    for (std::size_t i = 0; i < state->spec->op_count; ++i) {
      const UnisocExtOp &op = state->spec->ops[i];
      if (op.req_tx != code) {
        continue;
      }
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
            binder_status_t wst = AParcel_writeInt32(cb_in, 1);
            if (wst == STATUS_OK) wst = AParcel_writeInt32(cb_in, 16);
            if (wst == STATUS_OK) wst = AParcel_writeInt32(cb_in, 0);
            if (wst == STATUS_OK) wst = AParcel_writeInt32(cb_in, serial);
            if (wst == STATUS_OK) wst = AParcel_writeInt32(cb_in, op.error);
            if (wst == STATUS_OK) {
              switch (op.tail) {
              case UnisocRespTail::kNone:
                break;
              case UnisocRespTail::kInt32:
                wst = AParcel_writeInt32(cb_in, 0);
                break;
              case UnisocRespTail::kInt32Int32:
                wst = AParcel_writeInt32(cb_in, 0);
                if (wst == STATUS_OK) wst = AParcel_writeInt32(cb_in, 0);
                break;
              case UnisocRespTail::kString:
                wst = AParcel_writeString(cb_in, "", 0);
                break;
              case UnisocRespTail::kNullParcelable:
                wst = AParcel_writeInt32(cb_in, 0);
                break;
              case UnisocRespTail::kEmptyArray:
                wst = AParcel_writeInt32(cb_in, 0);
                break;
              }
            }
            if (wst == STATUS_OK) {
              AParcel *cb_out = nullptr;
              binder_status_t tst = AIBinder_transact(
                  resp, op.resp_tx, &cb_in, &cb_out, FLAG_ONEWAY);
              if (cb_out != nullptr) {
                AParcel_delete(cb_out);
              }
              if (tst != STATUS_OK) {
                __android_log_print(
                    ANDROID_LOG_WARN, kLogTag,
                    "unisoc ext response transact failed: %s tx=%u serial=%d st=%d",
                    state->spec->resp_descriptor, op.resp_tx, serial, tst);
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

  if (out == nullptr) {
    return STATUS_OK;
  }
  if (code >= FIRST_CALL_TRANSACTION) {
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    return st != STATUS_OK ? st : AParcel_writeString(out, "", 0);
  }
  return STATUS_UNKNOWN_TRANSACTION;
}

binder_status_t OnUnisocRadioPlaceholderTransact(AIBinder * /*binder*/,
                                                 transaction_code_t code,
                                                 const AParcel * /*in*/,
                                                 AParcel *out) {
  if (out == nullptr) {
    return STATUS_OK;
  }
  ndk::ScopedAStatus ok = ndk::ScopedAStatus::ok();
  if (code == 0xffffff) {
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    return st != STATUS_OK ? st : AParcel_writeInt32(out, 2);
  }
  if (code == 0xfffffe) {
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    return st != STATUS_OK ? st : AParcel_writeString(out, "notfrozen", 9);
  }
  if (code >= FIRST_CALL_TRANSACTION) {
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    return st != STATUS_OK ? st : AParcel_writeString(out, "", 0);
  }
  return STATUS_UNKNOWN_TRANSACTION;
}

} // namespace

void NormalizeVendorPlaceholderDescriptor(std::string * /*descriptor*/) {}

bool ResolveVendorPlaceholderBinding(const std::string &descriptor,
                                     PlaceholderBinding *out) {
  if (out == nullptr) {
    return false;
  }
  if (const UnisocExtSpec *spec = FindUnisocExtSpec(descriptor)) {
    auto *st = new UnisocExtPlaceholderState();
    st->spec = spec;
    st->resp_class = GetOrDefineRemoteCallbackClass(spec->resp_descriptor);
    out->on_destroy = OnUnisocExtDestroy;
    out->on_transact = OnUnisocExtTransact;
    out->disable_token_header = false;
    out->user_args = st;
    out->free_user_args = OnUnisocExtDestroy;
    return true;
  }
  if (descriptor.rfind("vendor.unisoc.hardware.radio.", 0) == 0) {
    out->on_destroy = OnUnisocNoopDestroy;
    out->on_transact = OnUnisocRadioPlaceholderTransact;
    out->disable_token_header = true;
    out->user_args = nullptr;
    out->free_user_args = nullptr;
    return true;
  }
  return false;
}

} // namespace cdh::radio
