// Copyright (c) 2025-2026 fei_cong(https://github.com/feicong/feicong-course)
#include "radio_modem.h"

#include "radio_service.h"

#include <aidl/android/hardware/radio/RadioAccessFamily.h>
#include <aidl/android/hardware/radio/RadioIndicationType.h>
#include <aidl/android/hardware/radio/RadioTechnology.h>
#include <aidl/android/hardware/radio/modem/ActivityStatsInfo.h>
#include <aidl/android/hardware/radio/modem/BnRadioModem.h>
#include <aidl/android/hardware/radio/modem/HardwareConfig.h>
#include <aidl/android/hardware/radio/modem/HardwareConfigModem.h>
#include <aidl/android/hardware/radio/modem/IRadioModemIndication.h>
#include <aidl/android/hardware/radio/modem/IRadioModemResponse.h>
#include <aidl/android/hardware/radio/modem/ImeiInfo.h>
#include <aidl/android/hardware/radio/modem/RadioCapability.h>
#include <aidl/android/hardware/radio/modem/RadioState.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <sys/system_properties.h>
#include <utility>
#include <vector>

namespace cdh::radio {
namespace {

namespace modem = ::aidl::android::hardware::radio::modem;

constexpr std::int32_t kRadioAccessFamily =
    static_cast<std::int32_t>(aidl_radio::RadioAccessFamily::GSM) |
    static_cast<std::int32_t>(aidl_radio::RadioAccessFamily::UMTS) |
    static_cast<std::int32_t>(aidl_radio::RadioAccessFamily::LTE) |
    static_cast<std::int32_t>(aidl_radio::RadioAccessFamily::NR);

// 基带版本与设备识别码缺省值：MTK 变体内置 gold(MIUI/MTK) 官方 GSMA TAC / MEID / SVN，
// 同时支持通过只读属性（ro.vendor.radio.imei.tac / ro.vendor.radio.meid.prefix /
// ro.vendor.radio.imei.svn / ro.vendor.oem.imei* / ro.vendor.oem.meid）按机型覆盖。
#if CDH_RADIO_MTK
constexpr const char *kBasebandVersion = "MOLY.NR15.R3.MP.V120.3.P82";
constexpr const char *kDefaultImeiTac = "86717107";
constexpr const char *kDefaultMeidPrefix = "99002158";
constexpr const char *kDefaultImeiSvn = "78";
#else
constexpr const char *kBasebandVersion = "";
constexpr const char *kDefaultImeiTac = "35948214";
constexpr const char *kDefaultMeidPrefix = "99000843";
constexpr const char *kDefaultImeiSvn = "01";
#endif

std::string ReadProperty(const char *name) {
  char value[PROP_VALUE_MAX] = {};
  if (__system_property_get(name, value) <= 0 || value[0] == '\0')
    return {};
  return std::string(value);
}

bool IsExactDigits(const std::string &s, std::size_t expected_len) {
  if (s.size() != expected_len)
    return false;
  return std::all_of(s.begin(), s.end(), [](unsigned char c) {
    return std::isdigit(c) != 0;
  });
}

bool IsExactHex(const std::string &s, std::size_t expected_len) {
  if (s.size() != expected_len)
    return false;
  return std::all_of(s.begin(), s.end(), [](unsigned char c) {
    return std::isxdigit(c) != 0;
  });
}

// 取 per-slot 列表属性（AOSP sysprop List 语义：逗号分隔，项内的逗号/反斜杠以
// '\' 转义，见 TelephonyProperties.escape/formatList）第 slot 项。
//
// 框架对每条 Phone 各调一次 getBasebandVersion，再用
// TelephonyManager.setBasebandVersionForPhone -> baseband_version(List) 按槽拼回
// 整条属性（列表长度 = modem 数）。若这里把「整条属性」当本槽版本上报，逗号会被
// 当成项内字符转义，拼出的串超过 91 字节 -> SystemProperties.set 抛
// IllegalArgumentException -> com.android.phone 起来即崩（实例 82 实测）。
// 越界（属性项数少于 modem 数）退回首项，单卡属性下两槽即得同一版本。
std::string PropertyListEntry(const char *name, std::int32_t slot) {
  char value[PROP_VALUE_MAX] = {};
  if (__system_property_get(name, value) <= 0 || value[0] == '\0')
    return {};
  std::vector<std::string> items;
  std::string current;
  for (const char *cursor = value; *cursor != '\0'; ++cursor) {
    if (*cursor == '\\' && (cursor[1] == ',' || cursor[1] == '\\')) {
      current.push_back(cursor[1]);
      ++cursor;
      continue;
    }
    if (*cursor == ',') {
      items.push_back(std::move(current));
      current.clear();
      continue;
    }
    current.push_back(*cursor);
  }
  items.push_back(std::move(current));
  const auto index =
      static_cast<std::size_t>(slot > 0 ? slot : 0);
  return index < items.size() ? items[index] : items.front();
}

modem::RadioState ToRadioState(RadioPower power) {
  switch (power) {
  case RadioPower::kOff:
    return modem::RadioState::OFF;
  case RadioPower::kOn:
    return modem::RadioState::ON;
  case RadioPower::kUnavailable:
    return modem::RadioState::UNAVAILABLE;
  }
  return modem::RadioState::UNAVAILABLE;
}

std::string DigitsOnly(const std::string &input) {
  std::string digits;
  digits.reserve(input.size());
  for (const char value : input) {
    if (std::isdigit(static_cast<unsigned char>(value)) != 0)
      digits.push_back(value);
  }
  return digits;
}

char LuhnCheckDigit(const std::string &body) {
  int sum = 0;
  bool double_digit = true;
  for (auto iterator = body.rbegin(); iterator != body.rend(); ++iterator) {
    int digit = *iterator - '0';
    if (double_digit) {
      digit *= 2;
      if (digit > 9)
        digit -= 9;
    }
    sum += digit;
    double_digit = !double_digit;
  }
  return static_cast<char>('0' + ((10 - sum % 10) % 10));
}

struct ModemIdentity {
  modem::ImeiInfo imei_info;
  std::string meid;
};

std::uint32_t DeriveBaseSnr(const std::optional<Snapshot> &snapshot) {
  std::string seed = ReadProperty("ro.boot.serialno");
  if (seed.empty())
    seed = ReadProperty("ro.serialno");
  if (seed.empty() && snapshot.has_value())
    seed = DigitsOnly(snapshot->sim.identity.iccid);
  if (seed.empty())
    seed = "gold-default";
  std::uint32_t hash = 2166136261u;
  for (unsigned char ch : seed) {
    hash ^= static_cast<std::uint32_t>(ch);
    hash *= 16777619u;
  }
  // 映射到 [100000, 999996] 偶数区间，保证槽 0 为偶数 SNR、槽 1 为相邻奇数 SNR。
  return 100000u + ((hash % 449999u) * 2u);
}

std::string FormatSixDigits(std::uint32_t value) {
  std::string out(6, '0');
  for (int idx = 5; idx >= 0; --idx) {
    out[static_cast<std::size_t>(idx)] = static_cast<char>('0' + (value % 10u));
    value /= 10u;
  }
  return out;
}

std::optional<ModemIdentity>
DeviceIdentityFromSnapshot(const std::optional<Snapshot> &snapshot,
                           std::int32_t slot) {
  if (!snapshot.has_value())
    return std::nullopt;

  std::string tac = ReadProperty("ro.vendor.radio.imei.tac");
  if (!IsExactDigits(tac, 8))
    tac = kDefaultImeiTac;

  std::string svn = ReadProperty("ro.vendor.radio.imei.svn");
  if (!IsExactDigits(svn, 2))
    svn = kDefaultImeiSvn;

  std::string meid_prefix = ReadProperty("ro.vendor.radio.meid.prefix");
  if (!IsExactHex(meid_prefix, 8))
    meid_prefix = kDefaultMeidPrefix;

  const std::uint32_t base_snr = DeriveBaseSnr(snapshot);
  const std::uint32_t slot_offset = slot > 0 ? static_cast<std::uint32_t>(slot % 2) : 0u;
  const std::string base_snr_str = FormatSixDigits(base_snr);
  const std::string slot_snr_str = FormatSixDigits(base_snr + slot_offset);

  std::string imei =
      slot <= 0 ? ReadProperty("ro.vendor.oem.imei")
                : ReadProperty("ro.vendor.oem.imei2");
  if (!IsExactDigits(imei, 15)) {
    const std::string body = tac + slot_snr_str;
    imei = body + LuhnCheckDigit(body);
  }

  std::string meid = ReadProperty("ro.vendor.oem.meid");
  if (!IsExactHex(meid, 14)) {
    meid = meid_prefix + base_snr_str;
  }

  return ModemIdentity{
      .imei_info =
          modem::ImeiInfo{
              .type = slot <= 0 ? modem::ImeiInfo::ImeiType::PRIMARY
                                : modem::ImeiInfo::ImeiType::SECONDARY,
              .imei = std::move(imei),
              .svn = std::move(svn),
          },
      .meid = std::move(meid),
  };
}

modem::RadioCapability RadioCapability() {
  return modem::RadioCapability{
      .session = 0,
      .phase = modem::RadioCapability::PHASE_CONFIGURED,
      .raf = kRadioAccessFamily,
      .logicalModemUuid = "cdh-radio-modem-0",
      .status = modem::RadioCapability::STATUS_SUCCESS,
  };
}

std::vector<modem::HardwareConfig> HardwareConfig() {
  modem::HardwareConfigModem modem_config{
      .rilModel = 0,
      .rat = aidl_radio::RadioTechnology::LTE,
      .maxVoiceCalls = 1,
      .maxDataCalls = 1,
      .maxStandby = 1,
  };
  return {modem::HardwareConfig{
      .type = modem::HardwareConfig::TYPE_MODEM,
      .uuid = "cdh-radio-modem-0",
      .state = modem::HardwareConfig::STATE_ENABLED,
      .modem = {std::move(modem_config)},
      .sim = {},
  }};
}

class RadioModem final : public modem::BnRadioModem, private RadioServiceState {
public:
  RadioModem(Channel &channel, StateStore &state_store, std::int32_t slot)
      : RadioServiceState(channel, state_store, slot),
        observer_(state_store, [this](const Snapshot &snapshot) {
          const auto indication = callbacks_.IndicationCallback();
          if (indication == nullptr)
            return;
          const auto type = aidl_radio::RadioIndicationType::UNSOLICITED;
          indication->radioStateChanged(type,
                                        ToRadioState(snapshot.modem.power));
          const auto identity = DeviceIdentityFromSnapshot(snapshot, this->slot());
          if (identity.has_value())
            indication->onImeiMappingChanged(type, identity->imei_info);
        }) {}

  ::ndk::ScopedAStatus enableModem(int32_t serial, bool on) override {
    return SetPower(serial, on,
                    &modem::IRadioModemResponse::enableModemResponse);
  }

  ::ndk::ScopedAStatus getBasebandVersion(int32_t serial) override {
    const auto response = callbacks_.ResponseCallback();
    if (response == nullptr)
      return MissingResponseFunctions();
    const auto snapshot = SnapshotState();
    // host 快照注入了基带版本则用之；否则回退顺序：
    //   1) 属性 gsm.version.baseband 的本槽项（vendor/build.prop 注入，框架按槽拼接）；
    //   2) 与 gold 官方一致的 MTK 版本号 kBasebandVersion。
    std::string version;
    if (snapshot.has_value() && !snapshot->modem.baseband_version.empty()) {
      version = snapshot->modem.baseband_version;
    } else {
      version = PropertyListEntry("gsm.version.baseband", slot());
      if (version.empty()) {
        version = kBasebandVersion;
      }
    }
    return response->getBasebandVersionResponse(
        ResponseInfo(serial, snapshot.has_value()
                                 ? aidl_radio::RadioError::NONE
                                 : aidl_radio::RadioError::RADIO_NOT_AVAILABLE),
        version);
  }

  ::ndk::ScopedAStatus getDeviceIdentity(int32_t serial) override {
    const auto response = callbacks_.ResponseCallback();
    if (response == nullptr)
      return MissingResponseFunctions();
    const auto identity = DeviceIdentityFromSnapshot(SnapshotState(), slot());
    return response->getDeviceIdentityResponse(
        ResponseInfo(serial, identity.has_value()
                                 ? aidl_radio::RadioError::NONE
                                 : aidl_radio::RadioError::RADIO_NOT_AVAILABLE),
        identity.has_value() ? identity->imei_info.imei : "",
        identity.has_value() ? identity->imei_info.svn : "", "",
        identity.has_value() ? identity->meid : "");
  }

  ::ndk::ScopedAStatus getHardwareConfig(int32_t serial) override {
    const auto response = callbacks_.ResponseCallback();
    if (response == nullptr)
      return MissingResponseFunctions();
    return response->getHardwareConfigResponse(
        ResponseInfo(serial, aidl_radio::RadioError::NONE), HardwareConfig());
  }

  ::ndk::ScopedAStatus getModemActivityInfo(int32_t serial) override {
    const auto response = callbacks_.ResponseCallback();
    if (response == nullptr)
      return MissingResponseFunctions();
    return response->getModemActivityInfoResponse(
        ResponseInfo(serial, aidl_radio::RadioError::NONE),
        modem::ActivityStatsInfo{});
  }

  ::ndk::ScopedAStatus getModemStackStatus(int32_t serial) override {
    const auto response = callbacks_.ResponseCallback();
    if (response == nullptr)
      return MissingResponseFunctions();
    const auto snapshot = SnapshotState();
    return response->getModemStackStatusResponse(
        ResponseInfo(serial, snapshot.has_value()
                                 ? aidl_radio::RadioError::NONE
                                 : aidl_radio::RadioError::RADIO_NOT_AVAILABLE),
        snapshot.has_value() &&
            snapshot->modem.power != RadioPower::kUnavailable);
  }

  ::ndk::ScopedAStatus getRadioCapability(int32_t serial) override {
    const auto response = callbacks_.ResponseCallback();
    if (response == nullptr)
      return MissingResponseFunctions();
    return response->getRadioCapabilityResponse(
        ResponseInfo(serial, aidl_radio::RadioError::NONE), RadioCapability());
  }

  ::ndk::ScopedAStatus nvReadItem(int32_t serial, modem::NvItem) override {
    const auto response = callbacks_.ResponseCallback();
    if (response == nullptr)
      return MissingResponseFunctions();
    return response->nvReadItemResponse(
        ResponseInfo(serial, aidl_radio::RadioError::REQUEST_NOT_SUPPORTED),
        "");
  }

  ::ndk::ScopedAStatus nvResetConfig(int32_t serial,
                                     modem::ResetNvType) override {
    return RespondSimple(callbacks_.ResponseCallback(), serial,
                         aidl_radio::RadioError::REQUEST_NOT_SUPPORTED,
                         &modem::IRadioModemResponse::nvResetConfigResponse);
  }

  ::ndk::ScopedAStatus nvWriteCdmaPrl(int32_t serial,
                                      const std::vector<uint8_t> &) override {
    return RespondSimple(callbacks_.ResponseCallback(), serial,
                         aidl_radio::RadioError::REQUEST_NOT_SUPPORTED,
                         &modem::IRadioModemResponse::nvWriteCdmaPrlResponse);
  }

  ::ndk::ScopedAStatus nvWriteItem(int32_t serial,
                                   const modem::NvWriteItem &) override {
    return RespondSimple(callbacks_.ResponseCallback(), serial,
                         aidl_radio::RadioError::REQUEST_NOT_SUPPORTED,
                         &modem::IRadioModemResponse::nvWriteItemResponse);
  }

  ::ndk::ScopedAStatus requestShutdown(int32_t serial) override {
    return SetPower(serial, false,
                    &modem::IRadioModemResponse::requestShutdownResponse);
  }

  ::ndk::ScopedAStatus responseAcknowledgement() override {
    return ::ndk::ScopedAStatus::ok();
  }

  ::ndk::ScopedAStatus sendDeviceState(int32_t serial, modem::DeviceStateType,
                                       bool) override {
    return RespondSimple(callbacks_.ResponseCallback(), serial,
                         aidl_radio::RadioError::NONE,
                         &modem::IRadioModemResponse::sendDeviceStateResponse);
  }

  ::ndk::ScopedAStatus
  setRadioCapability(int32_t serial, const modem::RadioCapability &) override {
    const auto response = callbacks_.ResponseCallback();
    if (response == nullptr)
      return MissingResponseFunctions();
    return response->setRadioCapabilityResponse(
        ResponseInfo(serial, aidl_radio::RadioError::REQUEST_NOT_SUPPORTED),
        RadioCapability());
  }

  ::ndk::ScopedAStatus setRadioPower(int32_t serial, bool power_on, bool,
                                     bool) override {
    return SetPower(serial, power_on,
                    &modem::IRadioModemResponse::setRadioPowerResponse);
  }

  ::ndk::ScopedAStatus setResponseFunctions(
      const std::shared_ptr<modem::IRadioModemResponse> &response,
      const std::shared_ptr<modem::IRadioModemIndication> &indication)
      override {
    if (response == nullptr || indication == nullptr) {
      return ::ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    callbacks_.Set(response, indication);
    const auto snapshot = SnapshotState();
    const auto type = aidl_radio::RadioIndicationType::UNSOLICITED;
    auto status = indication->radioStateChanged(
        type, snapshot.has_value() ? ToRadioState(snapshot->modem.power)
                                   : modem::RadioState::UNAVAILABLE);
    if (!status.isOk())
      return status;
    status = indication->rilConnected(type);
    if (!status.isOk())
      return status;
    const auto identity = DeviceIdentityFromSnapshot(snapshot, slot());
    if (identity.has_value())
      return indication->onImeiMappingChanged(type, identity->imei_info);
    return ::ndk::ScopedAStatus::ok();
  }

  ::ndk::ScopedAStatus getImei(int32_t serial) override {
    const auto response = callbacks_.ResponseCallback();
    if (response == nullptr)
      return MissingResponseFunctions();
    const auto identity = DeviceIdentityFromSnapshot(SnapshotState(), slot());
    std::optional<modem::ImeiInfo> imei;
    if (identity.has_value())
      imei = identity->imei_info;
    return response->getImeiResponse(
        ResponseInfo(serial, imei.has_value()
                                 ? aidl_radio::RadioError::NONE
                                 : aidl_radio::RadioError::RADIO_NOT_AVAILABLE),
        imei);
  }

private:
  using SimpleResponse = ::ndk::ScopedAStatus (modem::IRadioModemResponse::*)(
      const aidl_radio::RadioResponseInfo &);

  ::ndk::ScopedAStatus SetPower(int32_t serial, bool on,
                                SimpleResponse method) {
    Command command;
    command.kind = CommandKind::kSetRadioPower;
    command.radio_power = on ? RadioPower::kOn : RadioPower::kOff;
    const CommandResult result = Execute(std::move(command));
    return RespondSimple(callbacks_.ResponseCallback(), serial,
                         ToRadioError(result.status), method);
  }

  ResponseFunctions<modem::IRadioModemResponse, modem::IRadioModemIndication>
      callbacks_;
  StateObserver observer_;
};

} // namespace

std::shared_ptr<modem::IRadioModem> CreateRadioModem(Channel &channel,
                                                     StateStore &state_store,
                                                     std::int32_t slot) {
  return ::ndk::SharedRefBase::make<RadioModem>(channel, state_store, slot);
}

} // namespace cdh::radio
