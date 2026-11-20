#include "CdhOplusCharger.h"

#include <aidl/android/hardware/health/BatteryStatus.h>
#include <android/binder_ibinder.h>
#include <android/binder_parcel.h>
#include <android/binder_parcel_utils.h>
#include <android/binder_status.h>
#include <android/log.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>

extern "C" {
binder_status_t AServiceManager_addService(AIBinder *binder,
                                           const char *instance);
void AIBinder_markVintfStability(AIBinder *binder);
}

namespace cdh::health {
namespace {

using aidl::android::hardware::health::BatteryStatus;
using aidl::android::hardware::health::HealthInfo;

constexpr const char *kTag = "cdh_health";
constexpr const char *kServiceName =
    "vendor.oplus.hardware.charger.ICharger/default";
constexpr const char *kDescriptor =
    "vendor.oplus.hardware.charger.ICharger";

// RMX6688 /odm/lib64/vendor.oplus.hardware.charger-V7-ndk.so 与
// /system_ext/framework/vendor.oplus.hardware.charger-V8-java.jar 实测接口哈希。
constexpr const char *kHashV7 = "faa5fc9d45d1b3a786b0e17b204bb0342b828db1";
constexpr const char *kHashV8 = "a9cfe73255b21498ae403fd3357ee700e1c9675e";

bool ParseOplusChargerVintfVersion(const char *path, int32_t *outVersion) {
  FILE *fp = std::fopen(path, "r");
  if (!fp) {
    return false;
  }
  char buf[4096] = {0};
  const size_t n = std::fread(buf, 1, sizeof(buf) - 1, fp);
  std::fclose(fp);
  if (n == 0) {
    return false;
  }
  buf[n] = '\0';
  if (std::strstr(buf, "vendor.oplus.hardware.charger") == nullptr ||
      std::strstr(buf, "ICharger") == nullptr) {
    return false;
  }
  const char *verTag = std::strstr(buf, "<version>");
  if (verTag != nullptr) {
    const long parsed = std::strtol(verTag + std::strlen("<version>"), nullptr, 10);
    if (parsed > 0 && parsed <= 100) {
      *outVersion = static_cast<int32_t>(parsed);
      return true;
    }
  }
  *outVersion = 7;
  return true;
}

bool DetectOplusChargerVintf(int32_t *outVersion) {
  static const char *const kCandidatePaths[] = {
      "/odm/etc/vintf/manifest/manifest_oplus_charger_aidl.xml",
      "/odm/etc/vintf/manifest/vintf_manifest_oplus_charger_aidl.xml",
      "/vendor/etc/vintf/manifest/manifest_oplus_charger_aidl.xml",
      "/vendor/etc/vintf/manifest/vintf_manifest_oplus_charger_aidl.xml",
  };
  for (const char *path : kCandidatePaths) {
    if (ParseOplusChargerVintfVersion(path, outVersion)) {
      return true;
    }
  }
  return false;
}

const char *HashForVersion(int32_t version) {
  if (version >= 8) {
    return kHashV8;
  }
  return kHashV7;
}

const char *BatteryStatusText(BatteryStatus status) {
  switch (status) {
    case BatteryStatus::CHARGING:
      return "Charging";
    case BatteryStatus::FULL:
      return "Full";
    case BatteryStatus::NOT_CHARGING:
      return "Not charging";
    default:
      return "Discharging";
  }
}

int32_t ToMah(int32_t valueUahOrMah) {
  if (valueUahOrMah >= 100000) {
    return valueUahOrMah / 1000;
  }
  return valueUahOrMah;
}

binder_status_t WriteOkInt32(AParcel *out, int32_t value) {
  ndk::ScopedAStatus ok = ndk::ScopedAStatus::ok();
  binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
  if (st != STATUS_OK) {
    return st;
  }
  return AParcel_writeInt32(out, value);
}

binder_status_t WriteOkString(AParcel *out, const char *value) {
  ndk::ScopedAStatus ok = ndk::ScopedAStatus::ok();
  binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
  if (st != STATUS_OK) {
    return st;
  }
  const std::string s(value != nullptr ? value : "");
  return ::ndk::AParcel_writeString(out, s);
}

class IOplusCharger : public ::ndk::ICInterface {
 public:
  IOplusCharger() = default;
  ~IOplusCharger() override = default;
};

class OplusChargerService final : public ::ndk::BnCInterface<IOplusCharger> {
 public:
  OplusChargerService(int32_t interfaceVersion, OplusHealthSnapshotFn snapshotFn)
      : interfaceVersion_(interfaceVersion),
        snapshotFn_(std::move(snapshotFn)) {}
  ~OplusChargerService() override = default;

  binder_status_t handleTransact(transaction_code_t code, const AParcel * /*in*/,
                                 AParcel *out) {
    if (code < FIRST_CALL_TRANSACTION) {
      return STATUS_UNKNOWN_TRANSACTION;
    }
    const uint32_t tx = code - FIRST_CALL_TRANSACTION + 1;

    if (tx == 16777215u /*getInterfaceVersion*/) {
      return WriteOkInt32(out, interfaceVersion_);
    }
    if (tx == 16777214u /*getInterfaceHash*/) {
      return WriteOkString(out, HashForVersion(interfaceVersion_));
    }
    if (tx < 1u || tx > 130u) {
      return STATUS_UNKNOWN_TRANSACTION;
    }

    // 唯一返回自定义 Parcelable 的工厂测试方法（正常系统不调用）。
    if (tx == 108u /*testKitFeatureTest*/) {
      ndk::ScopedAStatus unsupported =
          ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
      return AParcel_writeStatusHeader(out, unsupported.get());
    }

    HealthInfo info{};
    int32_t soh = 100;
    if (snapshotFn_) {
      snapshotFn_(&info, &soh);
    }

    // 1) 返回 String 的 25 个事务码（其中 getChgConfig 等会被 OplusBatteryService
    //    直接传给 Integer.parseInt，须返回合法十进制串 "0"）。
    switch (tx) {
      case 15u:   // getBmsHeatingRunningStatus
      case 17u:   // getChargerControl
      case 50u:   // getPsyWirelessRX
      case 52u:   // getPsyWirelessTX
      case 55u:   // getQuickModeGain
      case 56u:   // getReserveSocDebug
      case 67u:   // getWirelessDeviated
      case 73u:   // getWirelessTXEnable
      case 128u:  // getChgConfig
      case 130u:  // getUsbCurrentEyeDiagram
        return WriteOkString(out, "0");

      case 38u:  // getPsyBatteryStatus
        return WriteOkString(out, BatteryStatusText(info.batteryStatus));

      case 8u:    // getBattParamNoplug
      case 13u:   // getBccCsvData
      case 23u:   // getDevinfoFastchg
      case 51u:   // getPsyWirelessRxVersion
      case 53u:   // getPsyWirelessTxVersion
      case 59u:   // getUisohDebugParameterInfo
      case 76u:   // healthd_update_ui_soc_decimal
      case 79u:   // queryChargeInfo
      case 109u:  // testKitGetFeatureList
      case 110u:  // testKitGetFeatureName
      case 113u:  // queryWlsPencilInfo
      case 114u:  // getChgOlcConfig
      case 125u:  // getPsyBatterySN
      case 126u:  // getBattGaugeInfo
        return WriteOkString(out, "");

      default:
        break;
    }

    // 2) 其余 1..130 事务码全部返回 int32_t。
    switch (tx) {
      case 5u:   // getBattAuthenticate
      case 30u:  // getPsyBatteryHmac
        return WriteOkInt32(out, 1);
      case 12u:  // getBatteryVoltageNow
        return WriteOkInt32(out, info.batteryVoltageMillivolts);
      case 26u:  // getPsyAcOnline
        return WriteOkInt32(out, info.chargerAcOnline ? 1 : 0);
      case 27u:  // getPsyBatteryCC
        return WriteOkInt32(out, info.batteryCycleCount);
      case 28u:  // getPsyBatteryCurrentNow
        return WriteOkInt32(out, info.batteryCurrentMicroamps / 1000);
      case 29u:  // getPsyBatteryFcc
        return WriteOkInt32(out, ToMah(info.batteryFullChargeUah));
      case 31u:  // getPsyBatteryLevel
        return WriteOkInt32(out, info.batteryLevel);
      case 32u:  // getPsyBatteryNotify（返回 0 消除 notifycode=-1 误触发弹窗）
        return WriteOkInt32(out, 0);
      case 35u:  // getPsyBatteryRm
        return WriteOkInt32(out, ToMah(info.batteryChargeCounterUah));
      case 39u:  // getPsyBatteryTemp
        return WriteOkInt32(out, info.batteryTemperatureTenthsCelsius);
      case 48u:  // getPsyUsbOnline
        return WriteOkInt32(out, info.chargerUsbOnline ? 1 : 0);
      case 58u:  // getUIsohValue
        return WriteOkInt32(out, soh > 0 ? soh : 100);
      default:
        return WriteOkInt32(out, 0);
    }
  }

 protected:
  ::ndk::SpAIBinder createBinder() override;

 private:
  int32_t interfaceVersion_;
  OplusHealthSnapshotFn snapshotFn_;
};

binder_status_t OplusCharger_onTransact(AIBinder *binder,
                                        transaction_code_t code,
                                        const AParcel *in, AParcel *out) {
  std::shared_ptr<::ndk::ICInterface> base =
      ::ndk::ICInterface::asInterface(binder);
  if (!base) {
    return STATUS_UNEXPECTED_NULL;
  }
  auto service = std::static_pointer_cast<OplusChargerService>(base);
  return service->handleTransact(code, in, out);
}

AIBinder_Class *GetOplusChargerClass() {
  static AIBinder_Class *cls =
      ::ndk::ICInterface::defineClass(kDescriptor, OplusCharger_onTransact);
  return cls;
}

::ndk::SpAIBinder OplusChargerService::createBinder() {
  AIBinder *binder =
      AIBinder_new(GetOplusChargerClass(), static_cast<void *>(this));
  AIBinder_markVintfStability(binder);
  return ::ndk::SpAIBinder(binder);
}

}  // namespace

std::shared_ptr<::ndk::ICInterface> RegisterOplusChargerIfDeclared(
    OplusHealthSnapshotFn snapshotFn) {
  int32_t version = 7;
  if (!DetectOplusChargerVintf(&version)) {
    return nullptr;
  }
  auto service = ndk::SharedRefBase::make<OplusChargerService>(
      version, std::move(snapshotFn));
  if (!service) {
    __android_log_print(ANDROID_LOG_ERROR, kTag,
                        "分配 OplusChargerService 失败");
    return nullptr;
  }
  // BnCInterface::asBinder() 内部只做弱缓存（mWeakBinder），此处静态强持有一次
  // SpAIBinder 与进程同寿命，避免循环引用触发 SharedRefBase 析构断言。
  static ndk::SpAIBinder sHeldBinder;
  sHeldBinder = service->asBinder();
  const binder_status_t st =
      AServiceManager_addService(sHeldBinder.get(), kServiceName);
  if (st != STATUS_OK) {
    sHeldBinder = ndk::SpAIBinder();
    __android_log_print(ANDROID_LOG_WARN, kTag,
                        "跳过注册 %s（status=%d）", kServiceName,
                        static_cast<int>(st));
    return nullptr;
  }
  __android_log_print(ANDROID_LOG_INFO, kTag,
                      "已在 health-service 进程内注册 %s (V%d)",
                      kServiceName, version);
  return service;
}

}  // namespace cdh::health
