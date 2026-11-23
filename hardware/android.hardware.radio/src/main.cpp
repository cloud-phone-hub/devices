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
  // 默认回报 REQUEST_NOT_SUPPORTED (6)；但展锐 RadioInteractorHandler$SyncHandler
  // 的少数同步查询分支（如 getSimlockDummys / getCpComLogVersion /
  // getVoLTEAllowedPLMN）在 ar.exception != null 时向 ThreadRequest.result 写入 null
  // 并 notifyAll()，导致主线程 waitForResult() 的 while (result == null) 永久死锁 ANR，
  // 对这类只读查询回报 NONE (0) + 安全空默认值。
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
    {3, 1, UnisocRespTail::kInt32Int32},     // getFacilityLockForAppExt
    {4, 2, UnisocRespTail::kNullParcelable}, // getIccCardStatusExt
    {5, 3, UnisocRespTail::kEmptyArray},     // getSimCapacity
    {6, 4, UnisocRespTail::kEmptyArray, 0},  // getSimlockDummys
    {7, 5, UnisocRespTail::kInt32},          // getSimlockRemaintimes
    {8, 6, UnisocRespTail::kInt32},          // getSimlockStatus
    {9, 7, UnisocRespTail::kString},         // getSimlockWhitelist
    {10, 8, UnisocRespTail::kInt32},         // getSubsidyLockdyStatus
    {11, 9, UnisocRespTail::kNullParcelable},// iccIOForAppExt
    {12, 10, UnisocRespTail::kString},       // queryPlmn
    {13, 11, UnisocRespTail::kNone},         // updatePlmnPriority
    {14, 12, UnisocRespTail::kInt32},        // setFacilityLock
    {15, 13, UnisocRespTail::kNone},         // setFacilityLockForUser
    {16, 14, UnisocRespTail::kString},       // simGetAtr
    {17, 15, UnisocRespTail::kEmptyArray},   // iccIOForAllFile
    {18, 16, UnisocRespTail::kString},       // readNvcodeFromMiscdata
    {19, 17, UnisocRespTail::kNone},         // setNvcodeToMiscdata
};

constexpr UnisocExtOp kUnisocModemOps[] = {
    {6, 1, UnisocRespTail::kString, 0},  // getCpComLogVersion
    {7, 2, UnisocRespTail::kString},  // getExceptionEvents
    {8, 3, UnisocRespTail::kString},  // getPcba
    {9, 4, UnisocRespTail::kNone},    // resetModem
    {10, 5, UnisocRespTail::kString}, // sendCmdAsync
    {11, 6, UnisocRespTail::kNone},   // setMNOParam
    {12, 7, UnisocRespTail::kInt32},  // getIdentityState
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
    {3, 1, UnisocRespTail::kNone},            // explicitCallTransferExt
    {4, 2, UnisocRespTail::kNullParcelable},  // getNumberControl
    {5, 3, UnisocRespTail::kInt32},           // queryColp
    {6, 4, UnisocRespTail::kInt32},           // queryColr
    {7, 5, UnisocRespTail::kNone},            // setEmergencyOnly
    {8, 7, UnisocRespTail::kInt32Int32},      // getCnap
    {9, 6, UnisocRespTail::kNone},            // setImsUserAgent
    {10, 8, UnisocRespTail::kNone},           // setSSflag
    {11, 9, UnisocRespTail::kNone},           // setTPMRState
    {12, 10, UnisocRespTail::kNone},          // updateClip
    {13, 11, UnisocRespTail::kNone},          // updateEcclist
    {14, 12, UnisocRespTail::kNone},          // videoPhoneControlIFrame
    {15, 13, UnisocRespTail::kNone},          // videoPhoneFallback
    {16, 14, UnisocRespTail::kNone},          // videoPhoneLocalMedia
    {17, 15, UnisocRespTail::kNone},          // videoPhoneString
    {18, 16, UnisocRespTail::kInt32},         // getSpecialRatcap
    {19, 17, UnisocRespTail::kNone},          // setLocationInfo
    {20, 18, UnisocRespTail::kNone},          // setFdnList
    {21, 19, UnisocRespTail::kNone},          // sendScreenInteractionExtras
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
     kUnisocMessagingOps, sizeof(kUnisocMessagingOps) / sizeof(kUnisocMessagingOps[0])},
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

AIBinder_Class *GetUnisocRespClass(const char *resp_descriptor) {
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
// 并阻塞等待 IQtiOemHookResponse.oemHookRawResponse() 回调。若仅注册通用空桩
// （返回 STATUS_UNKNOWN_TRANSACTION 且不回调），com.android.phone 主线程会永久
// 死锁，导致高通 PAKALA/KAANAPALI/MILOS 机型 SIM 卡停在 UNKNOWN、Radio 停在
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

// 展锐 IExtRadio* 占位服务端：
// - setExtResponseFunctions：保存客户端传入的 IExtRadio*Response 远端代理并回 EX_NONE；
// - 异步请求（如 HMDSimLockController 在主线程同步等待的 getSimlockWhitelist）：
//   读出 serial 并立即通过 IExtRadio*Response 以 REQUEST_NOT_SUPPORTED (6) 回调解锁等待线程，
//   避免 com.android.phone 主线程在 RadioInteractorHandler.waitForResult() 永久死锁 ANR。
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
            // ExtRadioResponseInfo: nonNull(1), size(16), type(0=SOLICITED),
            // serial, error(op.error: 默认 6=REQUEST_NOT_SUPPORTED，防死锁只读查询 0=NONE)
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
    if (st != STATUS_OK) {
      return st;
    }
    return AParcel_writeString(out, "", 0);
  }
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
    const UnisocExtSpec *unisoc_ext_spec = FindUnisocExtSpec(descriptor);
    const bool is_qti_oemhook = (descriptor == kQtiOemHookDescriptor);
    auto it = class_cache.find(descriptor);
    if (it == class_cache.end()) {
      // map 节点保证 key 地址稳定，先插入再传入 interfaceDescriptor
      it = class_cache.emplace(descriptor, nullptr).first;
      if (unisoc_ext_spec != nullptr) {
        it->second = AIBinder_Class_define(
            it->first.c_str(), OnPlaceholderCreate, OnUnisocExtDestroy,
            OnUnisocExtTransact);
      } else if (is_qti_oemhook) {
        it->second = AIBinder_Class_define(
            it->first.c_str(), OnPlaceholderCreate, OnQtiOemHookDestroy,
            OnQtiOemHookTransact);
      } else {
        const AIBinder_Class_onTransact transact_fn =
            (descriptor.rfind("vendor.unisoc.hardware.radio.", 0) == 0)
                ? OnUnisocRadioPlaceholderTransact
                : OnPlaceholderTransact;
        it->second = AIBinder_Class_define(
            it->first.c_str(), OnPlaceholderCreate, OnPlaceholderDestroy,
            transact_fn);
      }
      if (it->second == nullptr) {
        __android_log_print(ANDROID_LOG_WARN, kLogTag,
                            "define placeholder binder class failed: %s",
                            descriptor.c_str());
        class_cache.erase(it);
        continue;
      }
      if (unisoc_ext_spec == nullptr && !is_qti_oemhook) {
        AIBinder_Class_disableInterfaceTokenHeader(it->second);
      }
    }
    void *user_args = nullptr;
    if (unisoc_ext_spec != nullptr) {
      auto *st = new UnisocExtPlaceholderState();
      st->spec = unisoc_ext_spec;
      st->resp_class = GetUnisocRespClass(unisoc_ext_spec->resp_descriptor);
      user_args = st;
    } else if (is_qti_oemhook) {
      auto *st = new QtiOemHookPlaceholderState();
      st->resp_class = GetUnisocRespClass(kQtiOemHookRespDescriptor);
      user_args = st;
    }
    AIBinder *binder = AIBinder_new(it->second, user_args);
    if (binder == nullptr) {
      if (unisoc_ext_spec != nullptr) {
        delete static_cast<UnisocExtPlaceholderState *>(user_args);
      } else if (is_qti_oemhook) {
        delete static_cast<QtiOemHookPlaceholderState *>(user_args);
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
