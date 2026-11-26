// engine/vendor/hardware 线 —— android.hardware.radio@samsung 变体实现。
//
// 根因与契约（取证自 S25 Ultra / OneUI Android 16 framework.jar + telephony-common.jar +
// vendor.samsung.hardware.radio.bridge-V1-ndk.so）：
//   OneUI 的 SemRIL.getIccCardStatus 不调 AOSP IRadioSim.getIccCardStatus，而是直接走
//   SehRadioSimProxy -> vendor.samsung.hardware.radio.sim.ISehRadioSim/slot1..2，由
//   SehSimResponse.getIccCardStatusResponse(SehRadioResponseInfo, SehCardStatus) 构造
//   IccCardStatus 喂给 UiccController。若仅注册通用空桩（STATUS_UNKNOWN_TRANSACTION），
//   getInterfaceVersion / setResponseFunctions 均抛 RemoteException，导致 SIM 卡恒为
//   UNKNOWN、无法建立订阅与蜂窝数据连接。
//
// 本文件以精确冻结版 VERSION / HASH 及字节级对齐的 AParcel 布局实现全套 ISehRadio* 服务，
// 其中 ISehRadioSim 直接绑定宿主 StateStore 实时快照与监听回调。

#include "samsung/radio_samsung_ext.h"

#include "radio_placeholder_ext.h"
#include "radio_service.h"

#include <android/binder_auto_utils.h>
#include <android/binder_ibinder.h>
#include <android/binder_manager.h>
#include <android/binder_parcel.h>
#include <android/binder_status.h>
#include <android/log.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

extern "C" {
void AIBinder_markVintfStability(AIBinder *binder);
}

namespace cdh::radio {
namespace {

constexpr const char *kLogTag = "cdh-radio-samsung";
constexpr const char *kUsimAid = "A0000000871002FF86FFFF89FFFFFFFF";
constexpr const char *kSimAtr =
    "3B9F96801FC78031E073FE211B66D0017A0801000073";

// RadioResponseType / RadioIndicationType / RadioError 常量
constexpr int32_t kResponseSolicited = 0;
constexpr int32_t kIndicationUnsolicited = 0;
constexpr int32_t kErrorNone = 0;
constexpr int32_t kErrorRadioNotAvailable = 1;
constexpr int32_t kErrorRequestNotSupported = 6;

// 三星冻结接口元数据（均实测自 guest DEX / NDK .so）
constexpr const char *kSehSimDesc =
    "vendor.samsung.hardware.radio.sim.ISehRadioSim";
constexpr const char *kSehSimRespDesc =
    "vendor.samsung.hardware.radio.sim.ISehRadioSimResponse";
constexpr const char *kSehSimIndDesc =
    "vendor.samsung.hardware.radio.sim.ISehRadioSimIndication";
constexpr const char *kSehSimHash =
    "e94363ef2041c98feabe84e756d983ad196394e8";

constexpr const char *kSehDataDesc =
    "vendor.samsung.hardware.radio.data.ISehRadioData";
constexpr const char *kSehDataRespDesc =
    "vendor.samsung.hardware.radio.data.ISehRadioDataResponse";
constexpr const char *kSehDataIndDesc =
    "vendor.samsung.hardware.radio.data.ISehRadioDataIndication";
constexpr const char *kSehDataHash =
    "e3b3d94c19c2a59e3f7d77ec28473be99c4a2393";

constexpr const char *kSehNetDesc =
    "vendor.samsung.hardware.radio.network.ISehRadioNetwork";
constexpr const char *kSehNetRespDesc =
    "vendor.samsung.hardware.radio.network.ISehRadioNetworkResponse";
constexpr const char *kSehNetIndDesc =
    "vendor.samsung.hardware.radio.network.ISehRadioNetworkIndication";
constexpr const char *kSehNetHash =
    "0fc15a1faf82e2c79f263a7fd6e8b5ffba3644f3";

constexpr const char *kSehMsgDesc =
    "vendor.samsung.hardware.radio.messaging.ISehRadioMessaging";
constexpr const char *kSehMsgRespDesc =
    "vendor.samsung.hardware.radio.messaging.ISehRadioMessagingResponse";
constexpr const char *kSehMsgIndDesc =
    "vendor.samsung.hardware.radio.messaging.ISehRadioMessagingIndication";
constexpr const char *kSehMsgHash =
    "c1a8596db57e3bcc8e4e86f1eb7b2df7839ca140";

constexpr const char *kSehBridgeDesc =
    "vendor.samsung.hardware.radio.bridge.ISehRadioBridge";
constexpr const char *kSehBridgeRespDesc =
    "vendor.samsung.hardware.radio.bridge.ISehRadioBridgeResponse";
constexpr const char *kSehBridgeIndDesc =
    "vendor.samsung.hardware.radio.bridge.ISehRadioBridgeIndication";
constexpr const char *kSehBridgeHash =
    "514415cdaf07e8758ca98947620cb76eb30f3b65";

constexpr const char *kSehChannelDesc =
    "vendor.samsung.hardware.radio.channel.ISehRadioChannel";
constexpr const char *kSehChannelCbDesc =
    "vendor.samsung.hardware.radio.channel.ISehRadioChannelCallback";
constexpr const char *kSehChannelHash = "notfrozen";

constexpr const char *kSehSatDesc =
    "vendor.samsung.hardware.radio.satellite.ISehRadioSatellite";
constexpr const char *kSehSatRespDesc =
    "vendor.samsung.hardware.radio.satellite.ISehRadioSatelliteResponse";
constexpr const char *kSehSatIndDesc =
    "vendor.samsung.hardware.radio.satellite.ISehRadioSatelliteIndication";
constexpr const char *kSehSatHash =
    "0fa24f61670bd166a3e8f188603c8a99dcb4582a";

// ---- Parcel 辅助函数 ----

class ScopedParcelable {
 public:
  explicit ScopedParcelable(AParcel *parcel) : parcel_(parcel) {
    AParcel_writeInt32(parcel_, 1);  // non-null typed object 标记
    start_pos_ = AParcel_getDataPosition(parcel_);
    AParcel_writeInt32(parcel_, 0);  // 占位 size，Finish() 回填
  }

  void Finish() {
    if (finished_) return;
    finished_ = true;
    const int32_t end_pos = AParcel_getDataPosition(parcel_);
    AParcel_setDataPosition(parcel_, start_pos_);
    AParcel_writeInt32(parcel_, end_pos - start_pos_);
    AParcel_setDataPosition(parcel_, end_pos);
  }

 private:
  AParcel *parcel_;
  int32_t start_pos_ = 0;
  bool finished_ = false;
};

void WriteStdString(AParcel *p, const std::string &s) {
  AParcel_writeString(p, s.c_str(), static_cast<int32_t>(s.size()));
}

void WriteSehResponseInfo(AParcel *p, int32_t serial, int32_t error) {
  ScopedParcelable obj(p);
  AParcel_writeInt32(p, kResponseSolicited);
  AParcel_writeInt32(p, serial);
  AParcel_writeInt32(p, error);
  obj.Finish();
}

binder_status_t HandleVersionAndHash(transaction_code_t code, AParcel *out,
                                     int32_t version, const char *hash) {
  ndk::ScopedAStatus ok = ndk::ScopedAStatus::ok();
  if (code == 0xffffff) {
    if (out == nullptr) return STATUS_OK;
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    return st != STATUS_OK ? st : AParcel_writeInt32(out, version);
  }
  if (code == 0xfffffe) {
    if (out == nullptr) return STATUS_OK;
    binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
    return st != STATUS_OK
               ? st
               : AParcel_writeString(out, hash,
                                     static_cast<int32_t>(std::strlen(hash)));
  }
  return STATUS_UNKNOWN_TRANSACTION;
}

binder_status_t ReturnOkIfSync(AParcel *out) {
  if (out != nullptr) {
    ndk::ScopedAStatus ok = ndk::ScopedAStatus::ok();
    return AParcel_writeStatusHeader(out, ok.get());
  }
  return STATUS_OK;
}

struct CallbackPair {
  AIBinder_Class *resp_class = nullptr;
  AIBinder_Class *ind_class = nullptr;
  std::mutex mu;
  AIBinder *resp = nullptr;
  AIBinder *ind = nullptr;

  ~CallbackPair() {
    if (resp != nullptr) AIBinder_decStrong(resp);
    if (ind != nullptr) AIBinder_decStrong(ind);
  }

  void BindFromParcel(const AParcel *in) {
    AIBinder *new_resp = nullptr;
    AIBinder *new_ind = nullptr;
    AParcel_readStrongBinder(in, &new_resp);
    AParcel_readStrongBinder(in, &new_ind);
    if (new_resp != nullptr && resp_class != nullptr) {
      AIBinder_associateClass(new_resp, resp_class);
    }
    if (new_ind != nullptr && ind_class != nullptr) {
      AIBinder_associateClass(new_ind, ind_class);
    }
    std::lock_guard<std::mutex> lock(mu);
    if (resp != nullptr) AIBinder_decStrong(resp);
    if (ind != nullptr) AIBinder_decStrong(ind);
    resp = new_resp;
    ind = new_ind;
  }

  AIBinder *AcquireResp() {
    std::lock_guard<std::mutex> lock(mu);
    if (resp != nullptr) AIBinder_incStrong(resp);
    return resp;
  }

  AIBinder *AcquireInd() {
    std::lock_guard<std::mutex> lock(mu);
    if (ind != nullptr) AIBinder_incStrong(ind);
    return ind;
  }
};

template <typename WriterFn>
void SendOnewayCallback(AIBinder *target, transaction_code_t tx_code,
                        WriterFn &&writer) {
  if (target == nullptr) return;
  AParcel *cb_in = nullptr;
  if (AIBinder_prepareTransaction(target, &cb_in) == STATUS_OK &&
      cb_in != nullptr) {
    writer(cb_in);
    AParcel *cb_out = nullptr;
    AIBinder_transact(target, tx_code, &cb_in, &cb_out, FLAG_ONEWAY);
    if (cb_out != nullptr) {
      AParcel_delete(cb_out);
    }
  }
  AIBinder_decStrong(target);
}

// Response 附加尾载荷类型
enum class TailKind {
  kNone,       // (SehRadioResponseInfo)
  kIntZero,    // (SehRadioResponseInfo, int=0 或 null typed object 或 0长数组)
  kNegOne,     // (SehRadioResponseInfo, int=-1)
  kEmptyStr,   // (SehRadioResponseInfo, String="")
  kEmptyBytes, // (SehRadioResponseInfo, byte[0])
  kIntIntBytes // (SehRadioResponseInfo, int=0, int=0, byte[0])
};

void ReplyWithTail(CallbackPair *cbs, transaction_code_t resp_tx,
                   int32_t serial, int32_t error, TailKind tail) {
  SendOnewayCallback(cbs->AcquireResp(), resp_tx, [&](AParcel *p) {
    WriteSehResponseInfo(p, serial, error);
    switch (tail) {
      case TailKind::kNone:
        break;
      case TailKind::kIntZero:
        AParcel_writeInt32(p, 0);
        break;
      case TailKind::kNegOne:
        AParcel_writeInt32(p, -1);
        break;
      case TailKind::kEmptyStr:
        AParcel_writeString(p, "", 0);
        break;
      case TailKind::kEmptyBytes: {
        const int8_t empty = 0;
        AParcel_writeByteArray(p, &empty, 0);
        break;
      }
      case TailKind::kIntIntBytes: {
        const int8_t empty = 0;
        AParcel_writeInt32(p, 0);
        AParcel_writeInt32(p, 0);
        AParcel_writeByteArray(p, &empty, 0);
        break;
      }
    }
  });
}

// ============================================================================
// 1. ISehRadioSim（实时对接 StateStore）
// ============================================================================

int32_t ToSehAppState(SimState state) {
  switch (state) {
    case SimState::kPinRequired:
      return 2;  // APP_STATE_PIN
    case SimState::kPukRequired:
      return 3;  // APP_STATE_PUK
    case SimState::kReady:
      return 5;  // APP_STATE_READY
    default:
      return 0;  // APP_STATE_UNKNOWN
  }
}

int32_t ToSehPinState(SimState state) {
  switch (state) {
    case SimState::kPinRequired:
      return 1;  // ENABLED_NOT_VERIFIED
    case SimState::kPukRequired:
      return 2;  // ENABLED_BLOCKED
    default:
      return 3;  // DISABLED
  }
}

void WriteSehCardStatus(AParcel *p, const std::optional<Snapshot> &snapshot,
                        bool card_power_up, int32_t slot) {
  const bool present =
      snapshot.has_value() && snapshot->sim.present && card_power_up;
  ScopedParcelable card(p);
  AParcel_writeInt32(p, present ? 1 : 0);   // cardState: 1=PRESENT, 0=ABSENT
  AParcel_writeInt32(p, 3);                 // universalPinState: 3=DISABLED
  AParcel_writeInt32(p, present ? 0 : -1);  // gsmUmtsSubscriptionAppIndex
  AParcel_writeInt32(p, -1);                // cdmaSubscriptionAppIndex
  AParcel_writeInt32(p, -1);                // imsSubscriptionAppIndex

  // applications: SehAppStatus[]
  if (present) {
    AParcel_writeInt32(p, 1);  // array length = 1
    ScopedParcelable app(p);
    AParcel_writeInt32(p, 2);  // appType: 2 = APP_TYPE_USIM
    AParcel_writeInt32(p, ToSehAppState(snapshot->sim.state));  // appState
    AParcel_writeInt32(p, 2);  // persoSubstate: 2 = READY
    WriteStdString(p, kUsimAid);
    WriteStdString(p, "USIM");
    AParcel_writeInt32(p, 0);  // pin1Replaced (int32)
    AParcel_writeInt32(p, ToSehPinState(snapshot->sim.state));  // pin1
    AParcel_writeInt32(p, 3);   // pin2: DISABLED
    AParcel_writeInt32(p, 3);   // pin1NumRetries
    AParcel_writeInt32(p, 10);  // puk1NumRetries
    AParcel_writeInt32(p, 3);   // pin2NumRetries
    AParcel_writeInt32(p, 10);  // puk2NumRetries
    AParcel_writeInt32(p, 10);  // persoUnblockRetries
    app.Finish();
  } else {
    AParcel_writeInt32(p, 0);  // empty array
  }

  WriteStdString(p, present ? kSimAtr : "");
  WriteStdString(p, present ? snapshot->sim.identity.iccid : "");
  WriteStdString(p, "");  // eid

  // slotMap: SehSlotPortMapping（physicalSlotId 必须随 slot 变化，防止槽 1 覆盖槽 0）
  {
    ScopedParcelable slot_map(p);
    AParcel_writeInt32(p, slot);  // physicalSlotId
    AParcel_writeInt32(p, 0);     // portId
    slot_map.Finish();
  }

  AParcel_writeInt32(p, 0);  // supportedMepMode: NONE
  card.Finish();
}

struct SehSimService : private RadioServiceState {
  CallbackPair cbs;
  std::atomic_bool card_power_up{true};
  StateObserver observer;

  SehSimService(Channel &channel, StateStore &state_store, int32_t slot)
      : RadioServiceState(channel, state_store, slot),
        observer(state_store, [this](const Snapshot &) { NotifyReady(); }) {
    cbs.resp_class = GetOrDefineRemoteCallbackClass(kSehSimRespDesc);
    cbs.ind_class = GetOrDefineRemoteCallbackClass(kSehSimIndDesc);
  }

  std::optional<Snapshot> CurrentSlotSnapshot() const { return SnapshotState(); }
  int32_t SlotId() const { return slot(); }

  void NotifyReady() {
    const auto snap = SnapshotState();
    if (!snap.has_value() || !snap->sim.present || !card_power_up.load()) {
      return;
    }
    // Ind TX 1: simPhonebookReadyIndication(int type)
    SendOnewayCallback(cbs.AcquireInd(), 1, [](AParcel *p) {
      AParcel_writeInt32(p, kIndicationUnsolicited);
    });
    // Ind TX 2: phonebookInitCompleteIndication(int type)
    SendOnewayCallback(cbs.AcquireInd(), 2, [](AParcel *p) {
      AParcel_writeInt32(p, kIndicationUnsolicited);
    });
  }
};

void *OnCreatePassArgs(void *args) { return args; }
void OnSehSimDestroy(void *user_data) {
  delete static_cast<SehSimService *>(user_data);
}

binder_status_t OnSehSimTransact(AIBinder *binder, transaction_code_t code,
                                 const AParcel *in, AParcel *out) {
  if (code == 0xffffff || code == 0xfffffe) {
    return HandleVersionAndHash(code, out, 1, kSehSimHash);
  }
  auto *svc = static_cast<SehSimService *>(AIBinder_getUserData(binder));
  if (svc == nullptr || in == nullptr) return STATUS_UNKNOWN_TRANSACTION;

  if (code == 13) {  // setResponseFunctions(resp, ind)
    svc->cbs.BindFromParcel(in);
    svc->NotifyReady();
    return ReturnOkIfSync(out);
  }

  int32_t serial = 0;
  if (AParcel_readInt32(in, &serial) != STATUS_OK) {
    return ReturnOkIfSync(out);
  }

  switch (code) {
    case 1: {  // getIccCardStatus(int serial) -> Resp TX 1
      const auto snap = svc->CurrentSlotSnapshot();
      const int32_t err =
          snap.has_value() ? kErrorNone : kErrorRadioNotAvailable;
      SendOnewayCallback(svc->cbs.AcquireResp(), 1, [&](AParcel *p) {
        WriteSehResponseInfo(p, serial, err);
        WriteSehCardStatus(p, snap, svc->card_power_up.load(), svc->SlotId());
      });
      return ReturnOkIfSync(out);
    }
    case 2:  // supplyNetworkDepersonalization -> Resp TX 2 (info, int)
      ReplyWithTail(&svc->cbs, 2, serial, kErrorRequestNotSupported,
                    TailKind::kNegOne);
      return ReturnOkIfSync(out);
    case 3:  // getPhonebookStorageInfo -> Resp TX 3 (info, SehPhonebookInfo)
    case 4:  // getUsimPhonebookCapability -> Resp TX 4 (info, int[])
    case 7:  // getSimLockInfo -> Resp TX 7 (info, SehSimLockInfo)
    case 10: // getPhonebookEntry -> Resp TX 10 (info, SehSimPhonebookResponse)
      ReplyWithTail(&svc->cbs, code, serial, kErrorRequestNotSupported,
                    TailKind::kIntZero);
      return ReturnOkIfSync(out);
    case 5: {  // setSimOnOff(int serial, int mode) -> Resp TX 5 (info)
      int32_t mode = 1;
      if (AParcel_readInt32(in, &mode) == STATUS_OK) {
        svc->card_power_up.store(mode != 0);
      }
      ReplyWithTail(&svc->cbs, 5, serial, kErrorNone, TailKind::kNone);
      return ReturnOkIfSync(out);
    }
    case 6:  // setSimInitEvent(int serial) -> Resp TX 6 (info)
      ReplyWithTail(&svc->cbs, 6, serial, kErrorNone, TailKind::kNone);
      return ReturnOkIfSync(out);
    case 8:  // supplyIccPersonalization -> Resp TX 8 (info)
    case 9:  // changeIccPersonalization -> Resp TX 9 (info)
      ReplyWithTail(&svc->cbs, code, serial, kErrorRequestNotSupported,
                    TailKind::kNone);
      return ReturnOkIfSync(out);
    case 11:  // accessPhonebookEntry -> Resp TX 11 (info, int)
      ReplyWithTail(&svc->cbs, 11, serial, kErrorRequestNotSupported,
                    TailKind::kNegOne);
      return ReturnOkIfSync(out);
    case 12: {  // getAtr(int serial) -> Resp TX 12 (info, String)
      const auto snap = svc->CurrentSlotSnapshot();
      const bool present =
          snap.has_value() && snap->sim.present && svc->card_power_up.load();
      SendOnewayCallback(svc->cbs.AcquireResp(), 12, [&](AParcel *p) {
        WriteSehResponseInfo(
            p, serial, snap.has_value() ? kErrorNone : kErrorRadioNotAvailable);
        WriteStdString(p, present ? kSimAtr : "");
      });
      return ReturnOkIfSync(out);
    }
    default:
      return ReturnOkIfSync(out);
  }
}

// ============================================================================
// 2. ISehRadioData（VERSION = 2）
// ============================================================================

void OnCallbackPairDestroy(void *user_data) {
  delete static_cast<CallbackPair *>(user_data);
}

binder_status_t OnSehDataTransact(AIBinder *binder, transaction_code_t code,
                                  const AParcel *in, AParcel *out) {
  if (code == 0xffffff || code == 0xfffffe) {
    return HandleVersionAndHash(code, out, 2, kSehDataHash);
  }
  auto *cbs = static_cast<CallbackPair *>(AIBinder_getUserData(binder));
  if (cbs == nullptr || in == nullptr) return STATUS_UNKNOWN_TRANSACTION;

  if (code == 3) {  // setResponseFunctions
    cbs->BindFromParcel(in);
    return ReturnOkIfSync(out);
  }
  int32_t serial = 0;
  if (AParcel_readInt32(in, &serial) != STATUS_OK) return ReturnOkIfSync(out);

  switch (code) {
    case 1:  // setDataAllowed -> Resp TX 1
      ReplyWithTail(cbs, 1, serial, kErrorNone, TailKind::kNone);
      break;
    case 2:  // setMobileDataSetting -> Resp TX 2
      ReplyWithTail(cbs, 2, serial, kErrorNone, TailKind::kNone);
      break;
    case 4:  // setThroughputPeriodicTime -> Resp TX 3
      ReplyWithTail(cbs, 3, serial, kErrorNone, TailKind::kNone);
      break;
    case 5:  // getThroughputState -> Resp TX 4 (info, SehThroughputState)
      ReplyWithTail(cbs, 4, serial, kErrorRequestNotSupported,
                    TailKind::kIntZero);
      break;
    default:
      break;
  }
  return ReturnOkIfSync(out);
}

// ============================================================================
// 3. ISehRadioNetwork（VERSION = 1，实时推送 SehSignalBar 满格信号条）
// ============================================================================

struct SehNetService : private RadioServiceState {
  CallbackPair cbs;
  std::atomic_bool had_present{false};
  StateObserver observer;

  SehNetService(Channel &channel, StateStore &state_store, int32_t slot)
      : RadioServiceState(channel, state_store, slot),
        observer(state_store, [this](const Snapshot &) { NotifySignalBar(); }) {
    cbs.resp_class = GetOrDefineRemoteCallbackClass(kSehNetRespDesc);
    cbs.ind_class = GetOrDefineRemoteCallbackClass(kSehNetIndDesc);
  }

  void NotifySignalBar() {
    const auto snap = SnapshotState();
    const bool present = snap.has_value() && snap->sim.present &&
                         snap->sim.state == SimState::kReady;
    const bool prev = had_present.exchange(present);
    if (!present && !prev) {
      return;
    }
    const int32_t level = present ? 4 : 0;
    // Ind TX 14: signalLevelInfoChanged(int type, SehSignalBar signalBarInfo)
    // OneUI SystemUI 状态栏信号格读取 SignalBarInfo（由该回调驱动），不看 AOSP CellSignalStrength.level。
    SendOnewayCallback(cbs.AcquireInd(), 14, [level](AParcel *p) {
      AParcel_writeInt32(p, kIndicationUnsolicited);
      ScopedParcelable bar(p);
      AParcel_writeInt32(p, 0);      // cdmaLevel
      AParcel_writeInt32(p, 0);      // evdoLevel
      AParcel_writeInt32(p, level);  // gsmLevel
      AParcel_writeInt32(p, level);  // wcdmaLevel
      AParcel_writeInt32(p, 0);      // tdscdmaLevel
      AParcel_writeInt32(p, level);  // lteLevel
      AParcel_writeInt32(p, level);  // nrLevel
      bar.Finish();
    });
  }
};

void OnSehNetDestroy(void *user_data) {
  delete static_cast<SehNetService *>(user_data);
}

binder_status_t OnSehNetTransact(AIBinder *binder, transaction_code_t code,
                                 const AParcel *in, AParcel *out) {
  if (code == 0xffffff || code == 0xfffffe) {
    return HandleVersionAndHash(code, out, 1, kSehNetHash);
  }
  auto *svc = static_cast<SehNetService *>(AIBinder_getUserData(binder));
  if (svc == nullptr || in == nullptr) return STATUS_UNKNOWN_TRANSACTION;
  CallbackPair *cbs = &svc->cbs;

  if (code == 21) {  // setResponseFunctions
    cbs->BindFromParcel(in);
    svc->had_present.store(false);
    svc->NotifySignalBar();
    return ReturnOkIfSync(out);
  }
  int32_t serial = 0;
  if (AParcel_readInt32(in, &serial) != STATUS_OK) return ReturnOkIfSync(out);

  // TX 1..20 -> Resp TX 1..20；TX 22..31 -> Resp TX 21..30
  const transaction_code_t resp_tx = code < 21 ? code : (code - 1);
  switch (code) {
    case 7:   // getNrIconType -> (info, int=0)
    case 8:   // getNrMode -> (info, int=0)
    case 11:  // getVendorSpecificConfiguration -> (info, array len=0)
      ReplyWithTail(cbs, resp_tx, serial, kErrorNone, TailKind::kIntZero);
      break;
    case 18:  // setImsCallList -> (info)
    case 19:  // setNrMode -> (info)
    case 24:  // setVendorSpecificConfiguration -> Resp TX 23 (info)
      ReplyWithTail(cbs, resp_tx, serial, kErrorNone, TailKind::kNone);
      break;
    case 2:   // emergencySearch -> (info, int)
    case 3:   // getAvailableNetworks -> (info, array)
    case 4:   // getCnap -> (info, int)
    case 5:   // getCsgList -> (info, array)
    case 6:   // getDisable2g -> (info, int)
    case 9:   // getPreferredNetworkList -> (info, array)
    case 10:  // getRoamingNetworkInfoViaBLE -> (info, array)
    case 15:  // sendRequestStrings -> (info, String[])
      ReplyWithTail(cbs, resp_tx, serial, kErrorRequestNotSupported,
                    TailKind::kIntZero);
      break;
    case 14:  // sendRequestRaw -> (info, byte[])
      ReplyWithTail(cbs, resp_tx, serial, kErrorRequestNotSupported,
                    TailKind::kEmptyBytes);
      break;
    case 25:  // getCpaiModelVersion -> Resp TX 24 (info, int, int, byte[])
    case 27:  // getCpaiFeatureInfo -> Resp TX 26 (info, int, int, byte[])
      ReplyWithTail(cbs, resp_tx, serial, kErrorRequestNotSupported,
                    TailKind::kIntIntBytes);
      break;
    default:  // 其余 set*/exec* 方法均为 (SehRadioResponseInfo)
      if (code >= 1 && code <= 31) {
        ReplyWithTail(cbs, resp_tx, serial, kErrorRequestNotSupported,
                      TailKind::kNone);
      }
      break;
  }
  return ReturnOkIfSync(out);
}

// ============================================================================
// 4. ISehRadioMessaging（VERSION = 1）
// ============================================================================

binder_status_t OnSehMsgTransact(AIBinder *binder, transaction_code_t code,
                                 const AParcel *in, AParcel *out) {
  if (code == 0xffffff || code == 0xfffffe) {
    return HandleVersionAndHash(code, out, 1, kSehMsgHash);
  }
  auto *cbs = static_cast<CallbackPair *>(AIBinder_getUserData(binder));
  if (cbs == nullptr || in == nullptr) return STATUS_UNKNOWN_TRANSACTION;

  if (code == 9) {  // setResponseFunctions
    cbs->BindFromParcel(in);
    return ReturnOkIfSync(out);
  }
  int32_t serial = 0;
  if (AParcel_readInt32(in, &serial) != STATUS_OK) return ReturnOkIfSync(out);

  switch (code) {
    case 1:  // getCellBroadcastConfig -> Resp TX 8 (info, SehCbConfigArgs)
      ReplyWithTail(cbs, 8, serial, kErrorRequestNotSupported,
                    TailKind::kIntZero);
      break;
    case 2:  // getImsRegistrationState -> Resp TX 9 (info, int[4])
      SendOnewayCallback(cbs->AcquireResp(), 9, [&](AParcel *p) {
        WriteSehResponseInfo(p, serial, kErrorNone);
        const int32_t states[4] = {0, 0, 0, 0};
        AParcel_writeInt32Array(p, states, 4);
      });
      break;
    case 3:  // getStoredMsgCountFromSim -> Resp TX 5
      ReplyWithTail(cbs, 5, serial, kErrorRequestNotSupported,
                    TailKind::kIntZero);
      break;
    case 4:  // readSmsFromSim -> Resp TX 6
      ReplyWithTail(cbs, 6, serial, kErrorRequestNotSupported,
                    TailKind::kIntZero);
      break;
    case 5:  // sendCdmaSms -> Resp TX 3
      ReplyWithTail(cbs, 3, serial, kErrorRequestNotSupported,
                    TailKind::kIntZero);
      break;
    case 6:  // sendCdmaSmsExpectMore -> Resp TX 4
      ReplyWithTail(cbs, 4, serial, kErrorRequestNotSupported,
                    TailKind::kIntZero);
      break;
    case 7:  // sendSMSExpectMore -> Resp TX 2
      ReplyWithTail(cbs, 2, serial, kErrorRequestNotSupported,
                    TailKind::kIntZero);
      break;
    case 8:  // sendSms -> Resp TX 1
      ReplyWithTail(cbs, 1, serial, kErrorRequestNotSupported,
                    TailKind::kIntZero);
      break;
    case 10:  // writeSmsToSim -> Resp TX 7 (info, int)
      ReplyWithTail(cbs, 7, serial, kErrorRequestNotSupported,
                    TailKind::kNegOne);
      break;
    default:
      break;
  }
  return ReturnOkIfSync(out);
}

// ============================================================================
// 5. ISehRadioBridge（VERSION = 1）
// ============================================================================

binder_status_t OnSehBridgeTransact(AIBinder *binder, transaction_code_t code,
                                    const AParcel *in, AParcel *out) {
  if (code == 0xffffff || code == 0xfffffe) {
    return HandleVersionAndHash(code, out, 1, kSehBridgeHash);
  }
  auto *cbs = static_cast<CallbackPair *>(AIBinder_getUserData(binder));
  if (cbs == nullptr || in == nullptr) return STATUS_UNKNOWN_TRANSACTION;

  if (code == 1) {  // setResponseFunctions
    cbs->BindFromParcel(in);
    return ReturnOkIfSync(out);
  }
  if (code == 2) {  // sendRequestRaw(int serial, byte[], int) -> Resp TX 1
    int32_t serial = 0;
    if (AParcel_readInt32(in, &serial) == STATUS_OK) {
      SendOnewayCallback(cbs->AcquireResp(), 1, [&](AParcel *p) {
        WriteSehResponseInfo(p, serial, kErrorRequestNotSupported);
        const int8_t empty = 0;
        AParcel_writeByteArray(p, &empty, 0);
        AParcel_writeInt32(p, 0);
      });
    }
    return ReturnOkIfSync(out);
  }
  return ReturnOkIfSync(out);
}

// ============================================================================
// 6. ISehRadioChannel（imsd / epdgd 通道，VERSION = 1）
// ============================================================================

struct SehChannelState {
  AIBinder_Class *cb_class = nullptr;
  std::mutex mu;
  AIBinder *cb = nullptr;
  ~SehChannelState() {
    if (cb != nullptr) AIBinder_decStrong(cb);
  }
};

void OnSehChannelDestroy(void *user_data) {
  delete static_cast<SehChannelState *>(user_data);
}

binder_status_t OnSehChannelTransact(AIBinder *binder, transaction_code_t code,
                                     const AParcel *in, AParcel *out) {
  if (code == 0xffffff || code == 0xfffffe) {
    return HandleVersionAndHash(code, out, 1, kSehChannelHash);
  }
  auto *st = static_cast<SehChannelState *>(AIBinder_getUserData(binder));
  if (st == nullptr || in == nullptr) return STATUS_UNKNOWN_TRANSACTION;

  if (code == 1) {  // setCallback(ISehRadioChannelCallback)
    AIBinder *cb = nullptr;
    if (AParcel_readStrongBinder(in, &cb) == STATUS_OK && cb != nullptr) {
      if (st->cb_class != nullptr) {
        AIBinder_associateClass(cb, st->cb_class);
      }
      std::lock_guard<std::mutex> lock(st->mu);
      if (st->cb != nullptr) AIBinder_decStrong(st->cb);
      st->cb = cb;
    }
    return ReturnOkIfSync(out);
  }
  if (code == 2) {  // send(byte[])
    return ReturnOkIfSync(out);
  }
  return ReturnOkIfSync(out);
}

// ============================================================================
// 7. ISehRadioSatellite（VERSION = 1）
// ============================================================================

binder_status_t OnSehSatTransact(AIBinder *binder, transaction_code_t code,
                                 const AParcel *in, AParcel *out) {
  if (code == 0xffffff || code == 0xfffffe) {
    return HandleVersionAndHash(code, out, 1, kSehSatHash);
  }
  auto *cbs = static_cast<CallbackPair *>(AIBinder_getUserData(binder));
  if (cbs == nullptr || in == nullptr) return STATUS_UNKNOWN_TRANSACTION;

  if (code == 37) {  // setResponseFunctions
    cbs->BindFromParcel(in);
    return ReturnOkIfSync(out);
  }
  int32_t serial = 0;
  if (AParcel_readInt32(in, &serial) != STATUS_OK) return ReturnOkIfSync(out);

  // 注意 hangup(TX 3)->Resp 5, getCallEndReason(TX 4)->Resp 3, getCallState(TX 5)->Resp 4
  // sendSMSExpectMore(TX 21)->Resp 22, sendSms(TX 22)->Resp 21
  transaction_code_t resp_tx = code;
  if (code == 3) resp_tx = 5;
  else if (code == 4) resp_tx = 3;
  else if (code == 5) resp_tx = 4;
  else if (code == 21) resp_tx = 22;
  else if (code == 22) resp_tx = 21;

  switch (code) {
    case 4:   // getCallEndReason
    case 5:   // getCallState
    case 8:   // getRegistrationState
    case 9:   // getSignalStrength
    case 19:  // getAbsoluteRfChannelNumber
    case 20:  // getTxPower
    case 21:  // sendSMSExpectMore
    case 22:  // sendSms
    case 27:  // getSatelliteId
    case 34:  // getIotMode
    case 35:  // getIotRegistrationState
      ReplyWithTail(cbs, resp_tx, serial, kErrorRequestNotSupported,
                    TailKind::kIntZero);
      break;
    case 18:  // getSerialNumber
    case 29:  // sendRawAtCommand
      ReplyWithTail(cbs, resp_tx, serial, kErrorRequestNotSupported,
                    TailKind::kEmptyStr);
      break;
    default:
      if (code >= 1 && code <= 36) {
        ReplyWithTail(cbs, resp_tx, serial, kErrorRequestNotSupported,
                      TailKind::kNone);
      }
      break;
  }
  return ReturnOkIfSync(out);
}

// ---- 注册辅助 ----

bool RegisterBinderInstance(AIBinder_Class *clazz, void *user_data,
                            void (*free_fn)(void *), const char *service_name,
                            std::vector<AIBinder *> *keep_alive) {
  if (clazz == nullptr) {
    if (user_data != nullptr && free_fn != nullptr) free_fn(user_data);
    return false;
  }
  AIBinder *binder = AIBinder_new(clazz, user_data);
  if (binder == nullptr) {
    if (user_data != nullptr && free_fn != nullptr) free_fn(user_data);
    return false;
  }
  AIBinder_markVintfStability(binder);
  const binder_exception_t st =
      AServiceManager_addService(binder, service_name);
  if (st != EX_NONE) {
    __android_log_print(ANDROID_LOG_WARN, kLogTag,
                        "addService(%s) failed: %d", service_name, st);
    AIBinder_decStrong(binder);
    return false;
  }
  keep_alive->push_back(binder);
  return true;
}

CallbackPair *NewCallbackPair(const char *resp_desc, const char *ind_desc) {
  auto *cbs = new CallbackPair();
  cbs->resp_class = GetOrDefineRemoteCallbackClass(resp_desc);
  cbs->ind_class = GetOrDefineRemoteCallbackClass(ind_desc);
  return cbs;
}

}  // namespace

void RegisterSamsungRadioServices(Channel &channel, StateStore &state_store) {
  static std::vector<AIBinder *> gKeepAlive;

  static AIBinder_Class *sim_cls = AIBinder_Class_define(
      kSehSimDesc, OnCreatePassArgs, OnSehSimDestroy, OnSehSimTransact);
  static AIBinder_Class *data_cls = AIBinder_Class_define(
      kSehDataDesc, OnCreatePassArgs, OnCallbackPairDestroy, OnSehDataTransact);
  static AIBinder_Class *net_cls = AIBinder_Class_define(
      kSehNetDesc, OnCreatePassArgs, OnSehNetDestroy, OnSehNetTransact);
  static AIBinder_Class *msg_cls = AIBinder_Class_define(
      kSehMsgDesc, OnCreatePassArgs, OnCallbackPairDestroy, OnSehMsgTransact);
  static AIBinder_Class *bridge_cls =
      AIBinder_Class_define(kSehBridgeDesc, OnCreatePassArgs,
                            OnCallbackPairDestroy, OnSehBridgeTransact);
  static AIBinder_Class *chan_cls =
      AIBinder_Class_define(kSehChannelDesc, OnCreatePassArgs,
                            OnSehChannelDestroy, OnSehChannelTransact);
  static AIBinder_Class *sat_cls = AIBinder_Class_define(
      kSehSatDesc, OnCreatePassArgs, OnCallbackPairDestroy, OnSehSatTransact);

  int count = 0;
  for (int32_t slot = 0; slot < kLogicalModemCount; ++slot) {
    const std::string suffix = "/slot" + std::to_string(slot + 1);
    if (RegisterBinderInstance(
            sim_cls, new SehSimService(channel, state_store, slot),
            OnSehSimDestroy, (std::string(kSehSimDesc) + suffix).c_str(),
            &gKeepAlive)) {
      ++count;
    }
    if (RegisterBinderInstance(
            data_cls, NewCallbackPair(kSehDataRespDesc, kSehDataIndDesc),
            OnCallbackPairDestroy, (std::string(kSehDataDesc) + suffix).c_str(),
            &gKeepAlive)) {
      ++count;
    }
    if (RegisterBinderInstance(
            net_cls, new SehNetService(channel, state_store, slot),
            OnSehNetDestroy, (std::string(kSehNetDesc) + suffix).c_str(),
            &gKeepAlive)) {
      ++count;
    }
    if (RegisterBinderInstance(
            msg_cls, NewCallbackPair(kSehMsgRespDesc, kSehMsgIndDesc),
            OnCallbackPairDestroy, (std::string(kSehMsgDesc) + suffix).c_str(),
            &gKeepAlive)) {
      ++count;
    }
    if (RegisterBinderInstance(
            bridge_cls, NewCallbackPair(kSehBridgeRespDesc, kSehBridgeIndDesc),
            OnCallbackPairDestroy,
            (std::string(kSehBridgeDesc) + suffix).c_str(), &gKeepAlive)) {
      ++count;
    }
  }

  for (const char *inst : {"/imsd", "/imsd2", "/epdgd", "/epdgd2"}) {
    auto *st = new SehChannelState();
    st->cb_class = GetOrDefineRemoteCallbackClass(kSehChannelCbDesc);
    if (RegisterBinderInstance(
            chan_cls, st, OnSehChannelDestroy,
            (std::string(kSehChannelDesc) + inst).c_str(), &gKeepAlive)) {
      ++count;
    }
  }

  if (RegisterBinderInstance(
          sat_cls, NewCallbackPair(kSehSatRespDesc, kSehSatIndDesc),
          OnCallbackPairDestroy,
          (std::string(kSehSatDesc) + "/default").c_str(), &gKeepAlive)) {
    ++count;
  }

  __android_log_print(ANDROID_LOG_INFO, kLogTag,
                      "registered %d Samsung ISehRadio* services", count);
}

}  // namespace cdh::radio
