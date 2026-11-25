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
#ifdef __linux__
#include <linux/ipv6.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#endif

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

#ifdef __linux__
struct AddrEntry {
  uint8_t family = 0;
  uint8_t prefixlen = 0;
  uint8_t scope = 0;
  uint8_t bytes[16] = {};
  size_t addr_len = 0;
};

void AppendRtAttr(std::vector<uint8_t> *buf, uint16_t type, const void *data_ptr,
                  size_t data_len) {
  const size_t rta_len = RTA_LENGTH(data_len);
  const size_t aligned = RTA_ALIGN(rta_len);
  const size_t offset = buf->size();
  buf->resize(offset + aligned, 0);
  auto *rta = reinterpret_cast<struct rtattr *>(buf->data() + offset);
  rta->rta_type = type;
  rta->rta_len = static_cast<unsigned short>(rta_len);
  if (data_len > 0 && data_ptr != nullptr) {
    std::memcpy(RTA_DATA(rta), data_ptr, data_len);
  }
}

bool NetlinkModifyIPv6Addr(uint16_t nlmsg_type, int ifindex,
                           const struct in6_addr &addr6, uint8_t prefixlen,
                           uint8_t scope) {
  const int nl = ::socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE);
  if (nl < 0) {
    return false;
  }
  struct sockaddr_nl local {};
  local.nl_family = AF_NETLINK;
  ::bind(nl, reinterpret_cast<struct sockaddr *>(&local), sizeof(local));

  std::vector<uint8_t> req(NLMSG_SPACE(sizeof(struct ifaddrmsg)), 0);
  auto *nlh = reinterpret_cast<struct nlmsghdr *>(req.data());
  nlh->nlmsg_type = nlmsg_type;
  nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
  if (nlmsg_type == RTM_NEWADDR) {
    nlh->nlmsg_flags |= (NLM_F_CREATE | NLM_F_REPLACE);
  }
  nlh->nlmsg_seq = 1;

  auto *ifa = reinterpret_cast<struct ifaddrmsg *>(NLMSG_DATA(nlh));
  ifa->ifa_family = AF_INET6;
  ifa->ifa_prefixlen = prefixlen;
  ifa->ifa_flags = IFA_F_PERMANENT;
  ifa->ifa_scope = scope;
  ifa->ifa_index = static_cast<uint32_t>(ifindex);

  AppendRtAttr(&req, IFA_LOCAL, &addr6, sizeof(addr6));
  AppendRtAttr(&req, IFA_ADDRESS, &addr6, sizeof(addr6));
  if (nlmsg_type == RTM_NEWADDR) {
    const uint32_t flags32 = IFA_F_PERMANENT | IFA_F_NODAD;
    AppendRtAttr(&req, IFA_FLAGS, &flags32, sizeof(flags32));
  }
  nlh = reinterpret_cast<struct nlmsghdr *>(req.data());
  nlh->nlmsg_len = static_cast<uint32_t>(req.size());

  struct sockaddr_nl kern {};
  kern.nl_family = AF_NETLINK;
  const ssize_t sent = ::sendto(nl, req.data(), req.size(), 0,
                                reinterpret_cast<struct sockaddr *>(&kern),
                                sizeof(kern));
  bool ok = false;
  if (sent == static_cast<ssize_t>(req.size())) {
    uint8_t resp[512] = {};
    const ssize_t n = ::recv(nl, resp, sizeof(resp), 0);
    if (n >= static_cast<ssize_t>(sizeof(struct nlmsghdr))) {
      const auto *rh = reinterpret_cast<const struct nlmsghdr *>(resp);
      if (rh->nlmsg_type == NLMSG_ERROR &&
          rh->nlmsg_len >= NLMSG_LENGTH(sizeof(struct nlmsgerr))) {
        const auto *err =
            reinterpret_cast<const struct nlmsgerr *>(NLMSG_DATA(rh));
        ok = (err->error == 0);
      }
    }
  }
  ::close(nl);
  return ok;
}

void AddIPv6Address(int ifindex, const struct in6_addr &addr6, uint8_t prefixlen,
                    uint8_t scope) {
  if (ifindex <= 0) {
    return;
  }
  if (NetlinkModifyIPv6Addr(RTM_NEWADDR, ifindex, addr6, prefixlen, scope)) {
    return;
  }
  const int sock6 = ::socket(AF_INET6, SOCK_DGRAM | SOCK_CLOEXEC, 0);
  if (sock6 < 0) {
    return;
  }
  struct in6_ifreq ifr6 {};
  ifr6.ifr6_addr = addr6;
  ifr6.ifr6_prefixlen = prefixlen;
  ifr6.ifr6_ifindex = ifindex;
  ::ioctl(sock6, SIOCSIFADDR, &ifr6);
  ::close(sock6);
}

void FlushInterfaceAddresses(const std::string &ifname) {
  const unsigned int ifindex = ::if_nametoindex(ifname.c_str());
  if (ifindex == 0) {
    return;
  }
  // 1) 通过 NETLINK_ROUTE 枚举并删除该接口上的全部 IPv4 / IPv6 地址（含残留或 EUI-64 link-local）
  const int nl = ::socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE);
  if (nl >= 0) {
    struct sockaddr_nl local {};
    local.nl_family = AF_NETLINK;
    ::bind(nl, reinterpret_cast<struct sockaddr *>(&local), sizeof(local));

    struct {
      struct nlmsghdr nlh;
      struct ifaddrmsg ifa;
    } dump_req{};
    dump_req.nlh.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
    dump_req.nlh.nlmsg_type = RTM_GETADDR;
    dump_req.nlh.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
    dump_req.nlh.nlmsg_seq = 1;
    dump_req.ifa.ifa_family = AF_UNSPEC;

    struct sockaddr_nl kern {};
    kern.nl_family = AF_NETLINK;
    std::vector<AddrEntry> to_delete;
    if (::sendto(nl, &dump_req, dump_req.nlh.nlmsg_len, 0,
                 reinterpret_cast<struct sockaddr *>(&kern),
                 sizeof(kern)) > 0) {
      uint8_t buf[8192];
      bool done = false;
      while (!done) {
        const ssize_t len = ::recv(nl, buf, sizeof(buf), 0);
        if (len <= 0) {
          break;
        }
        unsigned int rem = static_cast<unsigned int>(len);
        for (auto *nlh = reinterpret_cast<struct nlmsghdr *>(buf);
             NLMSG_OK(nlh, rem); nlh = NLMSG_NEXT(nlh, rem)) {
          if (nlh->nlmsg_type == NLMSG_DONE) {
            done = true;
            break;
          }
          if (nlh->nlmsg_type == NLMSG_ERROR) {
            done = true;
            break;
          }
          if (nlh->nlmsg_type != RTM_NEWADDR) {
            continue;
          }
          const auto *ifa =
              reinterpret_cast<const struct ifaddrmsg *>(NLMSG_DATA(nlh));
          if (ifa->ifa_index != ifindex) {
            continue;
          }
          int rta_len = IFA_PAYLOAD(nlh);
          for (const auto *rta = IFA_RTA(ifa); RTA_OK(rta, rta_len);
               rta = RTA_NEXT(rta, rta_len)) {
            if (rta->rta_type == IFA_LOCAL || rta->rta_type == IFA_ADDRESS) {
              const size_t alen = RTA_PAYLOAD(rta);
              if ((ifa->ifa_family == AF_INET && alen == 4) ||
                  (ifa->ifa_family == AF_INET6 && alen == 16)) {
                AddrEntry entry;
                entry.family = ifa->ifa_family;
                entry.prefixlen = ifa->ifa_prefixlen;
                entry.scope = ifa->ifa_scope;
                entry.addr_len = alen;
                std::memcpy(entry.bytes, RTA_DATA(rta), alen);
                to_delete.push_back(entry);
                break;
              }
            }
          }
        }
      }
    }
    uint32_t seq = 2;
    for (const AddrEntry &e : to_delete) {
      std::vector<uint8_t> del_req(NLMSG_SPACE(sizeof(struct ifaddrmsg)), 0);
      auto *nlh = reinterpret_cast<struct nlmsghdr *>(del_req.data());
      nlh->nlmsg_type = RTM_DELADDR;
      nlh->nlmsg_flags = NLM_F_REQUEST;
      nlh->nlmsg_seq = seq++;
      auto *ifa = reinterpret_cast<struct ifaddrmsg *>(NLMSG_DATA(nlh));
      ifa->ifa_family = e.family;
      ifa->ifa_prefixlen = e.prefixlen;
      ifa->ifa_scope = e.scope;
      ifa->ifa_index = ifindex;
      AppendRtAttr(&del_req, IFA_LOCAL, e.bytes, e.addr_len);
      AppendRtAttr(&del_req, IFA_ADDRESS, e.bytes, e.addr_len);
      nlh = reinterpret_cast<struct nlmsghdr *>(del_req.data());
      nlh->nlmsg_len = static_cast<uint32_t>(del_req.size());
      ::sendto(nl, del_req.data(), del_req.size(), 0,
               reinterpret_cast<struct sockaddr *>(&kern), sizeof(kern));
    }
    ::close(nl);
  }

  // 2) 兜底通过 SIOCSIFADDR 0.0.0.0 清空主 IPv4 地址
  const int sock4 = ::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
  if (sock4 >= 0) {
    struct ifreq ifr {};
    std::strncpy(ifr.ifr_name, ifname.c_str(), IFNAMSIZ - 1);
    auto *sin = reinterpret_cast<struct sockaddr_in *>(&ifr.ifr_addr);
    sin->sin_family = AF_INET;
    sin->sin_addr.s_addr = 0;
    ::ioctl(sock4, SIOCSIFADDR, &ifr);
    ::close(sock4);
  }
}
#endif

// ConfigureCellularInterface 在 setupDataCall 成功后按宿主下发的网口名/IPv4/IPv6/MTU
// 将承载网卡（ccmni0=IMS 或 ccmni1=DEFAULT）拉 UP 并配置双栈地址与非 EUI-64 link-local。
void ConfigureCellularInterface(const DataCall &call) {
  if (call.interface_name.empty() || call.addresses.empty()) {
    return;
  }
#ifdef __linux__
  FlushInterfaceAddresses(call.interface_name);
#endif
  const int sock = ::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
  if (sock < 0) {
    return;
  }
  struct ifreq ifr {};
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

#ifdef __linux__
  // 拉 UP 后再清一次内核可能自动生成的 ff:fe EUI-64 link-local，随后写入真实非 EUI-64 地址。
  FlushInterfaceAddresses(call.interface_name);
  const int ifindex = static_cast<int>(::if_nametoindex(call.interface_name.c_str()));
#endif

  bool configured_v4 = false;
  bool configured_ll6 = false;
  for (const std::string &cidr : call.addresses) {
    const auto slash = cidr.find('/');
    const std::string ip_str =
        slash == std::string::npos ? cidr : cidr.substr(0, slash);
    int prefix = -1;
    if (slash != std::string::npos) {
      prefix = std::atoi(cidr.c_str() + slash + 1);
    }

    struct in_addr addr4 {};
    if (!configured_v4 && ::inet_pton(AF_INET, ip_str.c_str(), &addr4) == 1) {
      const int p4 = (prefix >= 1 && prefix <= 32) ? prefix : 24;
      std::memset(&ifr, 0, sizeof(ifr));
      std::strncpy(ifr.ifr_name, call.interface_name.c_str(), IFNAMSIZ - 1);
      auto *sin = reinterpret_cast<struct sockaddr_in *>(&ifr.ifr_addr);
      sin->sin_family = AF_INET;
      sin->sin_addr = addr4;
      ::ioctl(sock, SIOCSIFADDR, &ifr);

      const uint32_t mask_host = p4 == 0 ? 0U : (~0U << (32 - p4));
      std::memset(sin, 0, sizeof(*sin));
      sin->sin_family = AF_INET;
      sin->sin_addr.s_addr = htonl(mask_host);
      ::ioctl(sock, SIOCSIFNETMASK, &ifr);
      configured_v4 = true;
      continue;
    }

#ifdef __linux__
    struct in6_addr addr6 {};
    if (::inet_pton(AF_INET6, ip_str.c_str(), &addr6) == 1) {
      const uint8_t p6 =
          static_cast<uint8_t>((prefix >= 1 && prefix <= 128) ? prefix : 64);
      AddIPv6Address(ifindex, addr6, p6, RT_SCOPE_UNIVERSE);
      if (!configured_ll6) {
        struct in6_addr ll6 {};
        ll6.s6_addr[0] = 0xfe;
        ll6.s6_addr[1] = 0x80;
        std::memcpy(&ll6.s6_addr[8], &addr6.s6_addr[8], 8);
        AddIPv6Address(ifindex, ll6, 64, RT_SCOPE_LINK);
        configured_ll6 = true;
      }
    }
#endif
  }

  ::close(sock);
}

void BringDownCellularInterface(const std::string &ifname) {
  if (ifname.empty()) {
    return;
  }
#ifdef __linux__
  // 先清空接口上的全部 IPv4/IPv6 地址，杜绝 DOWN 状态网卡仍因残留地址被 NetworkInterface 枚举暴露。
  FlushInterfaceAddresses(ifname);
#endif
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

data::SetupDataCallResult ToDataCall(const DataCall &source,
                                     std::int32_t pdu_session_id = 0) {
  data::SetupDataCallResult output;
  output.cause = data::DataCallFailCause::NONE;
  output.suggestedRetryTime = -1;
  output.cid = source.id;
  output.active = source.state == DataCallState::kActive ? 2 : 0;
  bool has_v4 = false;
  bool has_v6 = false;
  for (const std::string &address : source.addresses) {
    if (address.find(':') != std::string::npos) {
      has_v6 = true;
    } else {
      has_v4 = true;
    }
    output.addresses.push_back(data::LinkAddress{
        .address = address,
        .addressProperties = data::LinkAddress::ADDRESS_PROPERTY_NONE,
        .deprecationTime = std::numeric_limits<std::int64_t>::max(),
        .expirationTime = std::numeric_limits<std::int64_t>::max(),
    });
  }
  if (has_v4 && has_v6) {
    output.type = data::PdpProtocolType::IPV4V6;
  } else if (has_v6) {
    output.type = data::PdpProtocolType::IPV6;
  } else {
    output.type = data::PdpProtocolType::IP;
  }
  output.ifname = source.interface_name;
  output.dnses = source.dns_servers;
  output.gateways = source.gateways;
  output.pcscf = source.pcscf_addresses;
  output.mtuV4 = has_v4 ? source.mtu : 0;
  output.mtuV6 = has_v6 ? source.mtu : 0;
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
    std::string target_iface;
    if (const auto before = SnapshotState(); before.has_value()) {
      for (const DataCall &c : before->data_calls) {
        if (c.id == cid) {
          target_iface = c.interface_name;
          break;
        }
      }
    }
    Command command;
    command.kind = CommandKind::kDeactivateDataCall;
    command.data_call.id = cid;
    const CommandResult result = Execute(std::move(command));
    const auto snapshot = SnapshotState();
    if (!target_iface.empty()) {
      bool still_used = false;
      if (snapshot.has_value()) {
        for (const DataCall &c : snapshot->data_calls) {
          if (c.state == DataCallState::kActive &&
              c.interface_name == target_iface) {
            still_used = true;
            break;
          }
        }
      }
      if (!still_used) {
        BringDownCellularInterface(target_iface);
      }
    } else if (!snapshot.has_value() || snapshot->data_calls.empty()) {
      BringDownCellularInterface("ccmni1");
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
    const bool pure_ims =
        data_policy_.IsPureImsApn(profile.supportedApnTypesBitmap);
    if (!data_allowed_.load() && !pure_ims) {
      return response->setupDataCallResponse(
          ResponseInfo(serial, aidl_radio::RadioError::OPERATION_NOT_ALLOWED),
          data::SetupDataCallResult{});
    }
    if (!data_policy_.SupportsApnTypes(profile.supportedApnTypesBitmap)) {
      return response->setupDataCallResponse(
          ResponseInfo(serial, aidl_radio::RadioError::NONE),
          FailedDataCall(data::DataCallFailCause::MISSING_UNKNOWN_APN,
                         kRetryNever, pdu_session_id));
    }
    Command command;
    command.kind = CommandKind::kSetupDataCall;
    command.data_call.id = data_policy_.NextCID();
    if (pure_ims) {
      command.data_call.apn = "ims";
    } else {
      command.data_call.apn = profile.apn.empty() ? "internet" : profile.apn;
    }
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
