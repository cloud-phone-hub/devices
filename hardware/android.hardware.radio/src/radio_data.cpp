// Copyright (c) 2025-2026 fei_cong(https://github.com/feicong/feicong-course)
#include "radio_data.h"

#include "data_policy.h"
#include "radio_service.h"

#include <aidl/android/hardware/radio/RadioIndicationType.h>
#include <aidl/android/hardware/radio/data/BnRadioData.h>
#include <aidl/android/hardware/radio/data/DataCallFailCause.h>
#include <aidl/android/hardware/radio/data/IRadioDataIndication.h>
#include <aidl/android/hardware/radio/data/IRadioDataResponse.h>
#include <aidl/android/hardware/radio/data/KeepaliveStatus.h>
#include <aidl/android/hardware/radio/data/LinkAddress.h>
#include <aidl/android/hardware/radio/data/PdpProtocolType.h>
#include <aidl/android/hardware/radio/data/SetupDataCallResult.h>
#include <aidl/android/hardware/radio/data/SlicingConfig.h>

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace cdh::radio {
namespace {

namespace data = ::aidl::android::hardware::radio::data;

// ConfigureCellularInterface 在 setupDataCall 成功后按宿主下发的网口名/IPv4/MTU
// 将物理承载网卡（如 ccmni2）拉 UP 并配好地址与掩码，确保 netd 路由生效后可立即收发数据。
void ConfigureCellularInterface(const DataCall &call) {
  if (call.interface_name.empty() || call.addresses.empty()) {
    return;
  }
  const int sock = ::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
  if (sock < 0) {
    return;
  }
  struct ifreq ifr {};
  std::strncpy(ifr.ifr_name, call.interface_name.c_str(), IFNAMSIZ - 1);

  // 解析首个 IPv4 地址（如 "100.64.155.15/24"）
  for (const std::string &cidr : call.addresses) {
    const auto slash = cidr.find('/');
    const std::string ip_str =
        slash == std::string::npos ? cidr : cidr.substr(0, slash);
    int prefix = 24;
    if (slash != std::string::npos) {
      const int parsed = std::atoi(cidr.c_str() + slash + 1);
      if (parsed >= 1 && parsed <= 32) {
        prefix = parsed;
      }
    }
    struct in_addr addr {};
    if (::inet_pton(AF_INET, ip_str.c_str(), &addr) != 1) {
      continue;
    }
    auto *sin = reinterpret_cast<struct sockaddr_in *>(&ifr.ifr_addr);
    std::memset(sin, 0, sizeof(*sin));
    sin->sin_family = AF_INET;
    sin->sin_addr = addr;
    ::ioctl(sock, SIOCSIFADDR, &ifr);

    const uint32_t mask_host = prefix == 0 ? 0U : (~0U << (32 - prefix));
    std::memset(sin, 0, sizeof(*sin));
    sin->sin_family = AF_INET;
    sin->sin_addr.s_addr = htonl(mask_host);
    ::ioctl(sock, SIOCSIFNETMASK, &ifr);
    break;
  }

  if (call.mtu > 0) {
    std::memset(&ifr, 0, sizeof(ifr));
    std::strncpy(ifr.ifr_name, call.interface_name.c_str(), IFNAMSIZ - 1);
    ifr.ifr_mtu = call.mtu;
    ::ioctl(sock, SIOCSIFMTU, &ifr);
  }

  std::memset(&ifr, 0, sizeof(ifr));
  std::strncpy(ifr.ifr_name, call.interface_name.c_str(), IFNAMSIZ - 1);
  if (::ioctl(sock, SIOCGIFFLAGS, &ifr) == 0) {
    ifr.ifr_flags |= (IFF_UP | IFF_RUNNING);
    ::ioctl(sock, SIOCSIFFLAGS, &ifr);
  }
  ::close(sock);
}

void BringDownCellularInterface(const std::string &ifname) {
  if (ifname.empty()) {
    return;
  }
  const int sock = ::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
  if (sock < 0) {
    return;
  }
  struct ifreq ifr {};
  std::strncpy(ifr.ifr_name, ifname.c_str(), IFNAMSIZ - 1);
  if (::ioctl(sock, SIOCGIFFLAGS, &ifr) == 0) {
    ifr.ifr_flags &= ~IFF_UP;
    ::ioctl(sock, SIOCSIFFLAGS, &ifr);
  }
  ::close(sock);
}

data::PdpProtocolType ToPdpProtocol(const std::string &protocol) {
  if (protocol == "IP")
    return data::PdpProtocolType::IP;
  if (protocol == "IPV6")
    return data::PdpProtocolType::IPV6;
  return data::PdpProtocolType::IPV4V6;
}

data::SetupDataCallResult ToDataCall(const DataCall &source,
                                     std::int32_t pdu_session_id = 0) {
  data::SetupDataCallResult output;
  output.cause = data::DataCallFailCause::NONE;
  output.suggestedRetryTime = -1;
  output.cid = source.id;
  output.active = source.state == DataCallState::kActive ? 2 : 0;
  output.type = data::PdpProtocolType::IPV4V6;
  output.ifname = source.interface_name;
  for (const std::string &address : source.addresses) {
    output.addresses.push_back(data::LinkAddress{
        .address = address,
        .addressProperties = data::LinkAddress::ADDRESS_PROPERTY_NONE,
        .deprecationTime = std::numeric_limits<std::int64_t>::max(),
        .expirationTime = std::numeric_limits<std::int64_t>::max(),
    });
  }
  output.dnses = source.dns_servers;
  output.gateways = source.gateways;
  output.mtuV4 = source.mtu;
  output.mtuV6 = source.mtu;
  output.handoverFailureMode =
      data::SetupDataCallResult::HANDOVER_FAILURE_MODE_LEGACY;
  output.pduSessionId = pdu_session_id;
  return output;
}

data::SetupDataCallResult FailedDataCall(data::DataCallFailCause cause,
                                         std::int64_t retry_time,
                                         std::int32_t pdu_session_id) {
  data::SetupDataCallResult output;
  output.cause = cause;
  output.suggestedRetryTime = retry_time;
  output.active =
      data::SetupDataCallResult::DATA_CONNECTION_STATUS_INACTIVE;
  output.type = data::PdpProtocolType::IP;
  output.handoverFailureMode =
      data::SetupDataCallResult::HANDOVER_FAILURE_MODE_LEGACY;
  output.pduSessionId = pdu_session_id;
  return output;
}

std::vector<data::SetupDataCallResult>
ToDataCalls(const std::optional<Snapshot> &snapshot) {
  std::vector<data::SetupDataCallResult> output;
  if (!snapshot.has_value())
    return output;
  output.reserve(snapshot->data_calls.size());
  for (const DataCall &call : snapshot->data_calls) {
    output.push_back(ToDataCall(call));
  }
  return output;
}

class RadioData final : public data::BnRadioData, private RadioServiceState {
public:
  RadioData(Channel &channel, StateStore &state_store, std::int32_t slot)
      : RadioServiceState(channel, state_store, slot),
        observer_(state_store, [this](const Snapshot &snapshot) {
          const auto indication = callbacks_.IndicationCallback();
          if (indication != nullptr) {
            indication->dataCallListChanged(
                aidl_radio::RadioIndicationType::UNSOLICITED,
                ToDataCalls(snapshot));
          }
        }) {}

  ::ndk::ScopedAStatus allocatePduSessionId(int32_t serial) override {
    const auto response = callbacks_.ResponseCallback();
    if (response == nullptr)
      return MissingResponseFunctions();
    return response->allocatePduSessionIdResponse(
        ResponseInfo(serial, aidl_radio::RadioError::REQUEST_NOT_SUPPORTED),
        -1);
  }

  ::ndk::ScopedAStatus cancelHandover(int32_t serial, int32_t) override {
    return RespondSimple(callbacks_.ResponseCallback(), serial,
                         aidl_radio::RadioError::REQUEST_NOT_SUPPORTED,
                         &data::IRadioDataResponse::cancelHandoverResponse);
  }

  ::ndk::ScopedAStatus deactivateDataCall(int32_t serial, int32_t cid,
                                          data::DataRequestReason) override {
    Command command;
    command.kind = CommandKind::kDeactivateDataCall;
    command.data_call.id = cid;
    const CommandResult result = Execute(std::move(command));
    const auto snapshot = SnapshotState();
    if (!snapshot.has_value() || snapshot->data_calls.empty()) {
      BringDownCellularInterface("ccmni2");
    }
    return RespondSimple(callbacks_.ResponseCallback(), serial,
                         ToRadioError(result.status),
                         &data::IRadioDataResponse::deactivateDataCallResponse);
  }

  ::ndk::ScopedAStatus getDataCallList(int32_t serial) override {
    const auto response = callbacks_.ResponseCallback();
    if (response == nullptr)
      return MissingResponseFunctions();
    const auto snapshot = SnapshotState();
    return response->getDataCallListResponse(
        ResponseInfo(serial, snapshot.has_value()
                                 ? aidl_radio::RadioError::NONE
                                 : aidl_radio::RadioError::RADIO_NOT_AVAILABLE),
        ToDataCalls(snapshot));
  }

  ::ndk::ScopedAStatus getSlicingConfig(int32_t serial) override {
    const auto response = callbacks_.ResponseCallback();
    if (response == nullptr)
      return MissingResponseFunctions();
    return response->getSlicingConfigResponse(
        ResponseInfo(serial, aidl_radio::RadioError::REQUEST_NOT_SUPPORTED),
        data::SlicingConfig{});
  }

  ::ndk::ScopedAStatus releasePduSessionId(int32_t serial, int32_t) override {
    return RespondSimple(
        callbacks_.ResponseCallback(), serial,
        aidl_radio::RadioError::REQUEST_NOT_SUPPORTED,
        &data::IRadioDataResponse::releasePduSessionIdResponse);
  }

  ::ndk::ScopedAStatus responseAcknowledgement() override {
    return ::ndk::ScopedAStatus::ok();
  }

  ::ndk::ScopedAStatus setDataAllowed(int32_t serial, bool allow) override {
    data_allowed_.store(allow);
    return RespondSimple(callbacks_.ResponseCallback(), serial,
                         aidl_radio::RadioError::NONE,
                         &data::IRadioDataResponse::setDataAllowedResponse);
  }

  ::ndk::ScopedAStatus
  setDataProfile(int32_t serial,
                 const std::vector<data::DataProfileInfo> &) override {
    return RespondSimple(callbacks_.ResponseCallback(), serial,
                         aidl_radio::RadioError::NONE,
                         &data::IRadioDataResponse::setDataProfileResponse);
  }

  ::ndk::ScopedAStatus setDataThrottling(int32_t serial,
                                         data::DataThrottlingAction,
                                         int64_t) override {
    return RespondSimple(callbacks_.ResponseCallback(), serial,
                         aidl_radio::RadioError::REQUEST_NOT_SUPPORTED,
                         &data::IRadioDataResponse::setDataThrottlingResponse);
  }

  ::ndk::ScopedAStatus
  setInitialAttachApn(int32_t serial,
                      const std::optional<data::DataProfileInfo> &) override {
    return RespondSimple(
        callbacks_.ResponseCallback(), serial, aidl_radio::RadioError::NONE,
        &data::IRadioDataResponse::setInitialAttachApnResponse);
  }

  ::ndk::ScopedAStatus setResponseFunctions(
      const std::shared_ptr<data::IRadioDataResponse> &response,
      const std::shared_ptr<data::IRadioDataIndication> &indication) override {
    if (response == nullptr || indication == nullptr) {
      return ::ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    callbacks_.Set(response, indication);
    const auto snapshot = SnapshotState();
    if (!snapshot.has_value())
      return ::ndk::ScopedAStatus::ok();
    return indication->dataCallListChanged(
        aidl_radio::RadioIndicationType::UNSOLICITED, ToDataCalls(snapshot));
  }

  ::ndk::ScopedAStatus
  setupDataCall(int32_t serial, aidl_radio::AccessNetwork,
                const data::DataProfileInfo &profile, bool,
                data::DataRequestReason, const std::vector<data::LinkAddress> &,
                const std::vector<std::string> &, int32_t pdu_session_id,
                const std::optional<data::SliceInfo> &, bool) override {
    const auto response = callbacks_.ResponseCallback();
    if (response == nullptr)
      return MissingResponseFunctions();
    if (!data_allowed_.load()) {
      return response->setupDataCallResponse(
          ResponseInfo(serial, aidl_radio::RadioError::OPERATION_NOT_ALLOWED),
          data::SetupDataCallResult{});
    }
    if (!data_policy_.SupportsDefault(profile.supportedApnTypesBitmap)) {
      return response->setupDataCallResponse(
          ResponseInfo(serial, aidl_radio::RadioError::NONE),
          FailedDataCall(data::DataCallFailCause::MISSING_UNKNOWN_APN,
                         kRetryNever, pdu_session_id));
    }
    Command command;
    command.kind = CommandKind::kSetupDataCall;
    command.data_call.id = data_policy_.NextCID();
    command.data_call.apn = profile.apn.empty() ? "internet" : profile.apn;
    command.data_call.protocol = data::toString(profile.protocol);
    const CommandResult result = Execute(std::move(command));
    if (result.status == Status::kNetworkRejected) {
      return response->setupDataCallResponse(
          ResponseInfo(serial, aidl_radio::RadioError::NONE),
          FailedDataCall(data::DataCallFailCause::INSUFFICIENT_RESOURCES,
                         kBusyRetryMS, pdu_session_id));
    }
    data::SetupDataCallResult data_call;
    if (result.data_call.has_value()) {
      ConfigureCellularInterface(*result.data_call);
      data_call = ToDataCall(*result.data_call, pdu_session_id);
      data_call.type = ToPdpProtocol(data::toString(profile.protocol));
    }
    return response->setupDataCallResponse(
        ResponseInfo(serial, ToRadioError(result.status)), data_call);
  }

  ::ndk::ScopedAStatus startHandover(int32_t serial, int32_t) override {
    return RespondSimple(callbacks_.ResponseCallback(), serial,
                         aidl_radio::RadioError::REQUEST_NOT_SUPPORTED,
                         &data::IRadioDataResponse::startHandoverResponse);
  }

  ::ndk::ScopedAStatus startKeepalive(int32_t serial,
                                      const data::KeepaliveRequest &) override {
    const auto response = callbacks_.ResponseCallback();
    if (response == nullptr)
      return MissingResponseFunctions();
    return response->startKeepaliveResponse(
        ResponseInfo(serial, aidl_radio::RadioError::REQUEST_NOT_SUPPORTED),
        data::KeepaliveStatus{.sessionHandle = -1,
                              .code = data::KeepaliveStatus::CODE_INACTIVE});
  }

  ::ndk::ScopedAStatus stopKeepalive(int32_t serial, int32_t) override {
    return RespondSimple(callbacks_.ResponseCallback(), serial,
                         aidl_radio::RadioError::REQUEST_NOT_SUPPORTED,
                         &data::IRadioDataResponse::stopKeepaliveResponse);
  }

private:
  ResponseFunctions<data::IRadioDataResponse, data::IRadioDataIndication>
      callbacks_;
  std::atomic_bool data_allowed_{true};
  StateObserver observer_;
  DataPolicy data_policy_;
};

} // namespace

std::shared_ptr<data::IRadioData> CreateRadioData(Channel &channel,
                                                  StateStore &state_store,
                                                  std::int32_t slot) {
  return ::ndk::SharedRefBase::make<RadioData>(channel, state_store, slot);
}

} // namespace cdh::radio
