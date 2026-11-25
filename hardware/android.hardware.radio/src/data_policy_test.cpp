// Copyright (c) 2025-2026 fei_cong(https://github.com/feicong/feicong-course)
// 宿主单元测试：由 CDH_RADIO_HOST_TEST 宏保护，避免 halbuild 扫描 src/*.cpp 时重复链接 main。
#ifdef CDH_RADIO_HOST_TEST

#include "data_policy.h"
#include "state.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {

void AppendU16(std::vector<std::uint8_t> *out, std::uint16_t v) {
  out->push_back(static_cast<std::uint8_t>(v >> 8));
  out->push_back(static_cast<std::uint8_t>(v & 0xff));
}

void AppendU32(std::vector<std::uint8_t> *out, std::uint32_t v) {
  out->push_back(static_cast<std::uint8_t>((v >> 24) & 0xff));
  out->push_back(static_cast<std::uint8_t>((v >> 16) & 0xff));
  out->push_back(static_cast<std::uint8_t>((v >> 8) & 0xff));
  out->push_back(static_cast<std::uint8_t>(v & 0xff));
}

void AppendTlvInt32(std::vector<std::uint8_t> *out, std::uint16_t tag,
                    std::int32_t val) {
  AppendU16(out, tag);
  AppendU16(out, 5); // kTypeInt32
  AppendU32(out, 4);
  AppendU32(out, static_cast<std::uint32_t>(val));
}

void AppendTlvUint8(std::vector<std::uint8_t> *out, std::uint16_t tag,
                    std::uint8_t val) {
  AppendU16(out, tag);
  AppendU16(out, 2); // kTypeUint8
  AppendU32(out, 1);
  out->push_back(val);
}

void AppendTlvString(std::vector<std::uint8_t> *out, std::uint16_t tag,
                     const std::string &val) {
  AppendU16(out, tag);
  AppendU16(out, 6); // kTypeString
  AppendU32(out, static_cast<std::uint32_t>(val.size()));
  out->insert(out->end(), val.begin(), val.end());
}

} // namespace

int main() {
  using cdh::radio::DataCall;
  using cdh::radio::DataCallState;
  using cdh::radio::DataPolicy;

  DataPolicy policy;
  // DEFAULT(1) 与 IMS(64) 均须支持，彩信单请求 MMS(2) 不支持
  assert(policy.SupportsDefault(1));
  assert(!policy.SupportsDefault(64));
  assert(policy.SupportsApnTypes(1));
  assert(policy.SupportsApnTypes(64));
  assert(policy.SupportsApnTypes(1 | 64));
  assert(!policy.SupportsApnTypes(2));
  assert(policy.IsPureImsApn(64));
  assert(!policy.IsPureImsApn(1));
  assert(!policy.IsPureImsApn(1 | 64));

  // 验证 DecodeDataCallPayload 正确解码 IPv6 + P-CSCF (tag=8)
  std::vector<std::uint8_t> payload;
  AppendTlvInt32(&payload, 1, 1); // id = 1
  AppendTlvUint8(&payload, 2, static_cast<std::uint8_t>(DataCallState::kActive));
  AppendTlvString(&payload, 3, "ccmni0");
  AppendTlvString(&payload, 4, "2408:8456:1234:5678:18ea:9ab8:fe97:f086/64");
  AppendTlvString(&payload, 5, "2408:8888::8");
  AppendTlvString(&payload, 5, "2408:8899::8");
  AppendTlvString(&payload, 6, "::");
  AppendTlvInt32(&payload, 7, 1400);
  AppendTlvString(&payload, 8, "2408:8000:9000::1");
  AppendTlvString(&payload, 8, "2408:8000:9000::2");

  DataCall decoded;
  std::string err;
  assert(cdh::radio::DecodeDataCallPayload(payload, &decoded, &err));
  assert(err.empty());
  assert(decoded.interface_name == "ccmni0");
  assert(decoded.addresses.size() == 1);
  assert(decoded.dns_servers.size() == 2);
  assert(decoded.gateways.size() == 1 && decoded.gateways[0] == "::");
  assert(decoded.pcscf_addresses.size() == 2);
  assert(decoded.pcscf_addresses[0] == "2408:8000:9000::1");
  assert(decoded.mtu == 1400);

  std::printf("PASS: data_policy_test\n");
  return 0;
}

#endif // CDH_RADIO_HOST_TEST
