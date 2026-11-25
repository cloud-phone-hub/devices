// Copyright (c) 2025-2026 fei_cong(https://github.com/feicong/feicong-course)
// 宿主单元测试：由 CDH_RADIO_CHANNEL_HOST_TEST 宏保护，避免 halbuild 扫描 src/*.cpp 时重复链接 main。
// 验证 /dev/hvc4 在启动窗口存在前导 ANSI CSI、半截帧、以及 N_TTY ICRNL(0x0d->0x0a) 污染载荷时，
// Channel 能滑窗重同步并在下一帧干净 HELLO 到达后顺利完成握手，不误关 fd。
#ifdef CDH_RADIO_CHANNEL_HOST_TEST

#include "channel.h"
#include "state.h"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace {

void PutU16(std::vector<std::uint8_t> *out, std::uint16_t v) {
  out->push_back(static_cast<std::uint8_t>(v >> 8));
  out->push_back(static_cast<std::uint8_t>(v & 0xff));
}

void PutU32(std::vector<std::uint8_t> *out, std::uint32_t v) {
  out->push_back(static_cast<std::uint8_t>((v >> 24) & 0xff));
  out->push_back(static_cast<std::uint8_t>((v >> 16) & 0xff));
  out->push_back(static_cast<std::uint8_t>((v >> 8) & 0xff));
  out->push_back(static_cast<std::uint8_t>(v & 0xff));
}

void PutU64(std::vector<std::uint8_t> *out, std::uint64_t v) {
  for (int shift = 56; shift >= 0; shift -= 8) {
    out->push_back(static_cast<std::uint8_t>((v >> shift) & 0xff));
  }
}

void TlvBool(std::vector<std::uint8_t> *out, std::uint16_t tag, bool val) {
  PutU16(out, tag);
  PutU16(out, 1);
  PutU32(out, 1);
  out->push_back(val ? 1 : 0);
}

void TlvU8(std::vector<std::uint8_t> *out, std::uint16_t tag, std::uint8_t val) {
  PutU16(out, tag);
  PutU16(out, 2);
  PutU32(out, 1);
  out->push_back(val);
}

void TlvU64(std::vector<std::uint8_t> *out, std::uint16_t tag, std::uint64_t val) {
  PutU16(out, tag);
  PutU16(out, 4);
  PutU32(out, 8);
  PutU64(out, val);
}

void TlvI32(std::vector<std::uint8_t> *out, std::uint16_t tag, std::int32_t val) {
  PutU16(out, tag);
  PutU16(out, 5);
  PutU32(out, 4);
  PutU32(out, static_cast<std::uint32_t>(val));
}

void TlvStr(std::vector<std::uint8_t> *out, std::uint16_t tag, const std::string &val) {
  PutU16(out, tag);
  PutU16(out, 6);
  PutU32(out, static_cast<std::uint32_t>(val.size()));
  out->insert(out->end(), val.begin(), val.end());
}

void TlvObj(std::vector<std::uint8_t> *out, std::uint16_t tag,
            const std::vector<std::uint8_t> &val) {
  PutU16(out, tag);
  PutU16(out, 8);
  PutU32(out, static_cast<std::uint32_t>(val.size()));
  out->insert(out->end(), val.begin(), val.end());
}

std::vector<std::uint8_t> BuildValidSnapshotPayload(std::uint64_t gen,
                                                    std::uint64_t seq) {
  std::vector<std::uint8_t> modem;
  TlvU8(&modem, 1, 2); // kOn
  TlvU8(&modem, 2, 1); // kGsm
  TlvStr(&modem, 3, "4G_MODEM_22B_W26.02.5_P2");

  std::vector<std::uint8_t> id;
  TlvStr(&id, 1, "86");
  TlvStr(&id, 2, "13215555910");
  TlvStr(&id, 3, "+8613215555910");
  TlvStr(&id, 4, "460010000005910");
  TlvStr(&id, 5, "89860100000000059106");
  TlvStr(&id, 6, "46001");
  TlvStr(&id, 7, "中国联通");

  std::vector<std::uint8_t> sim;
  TlvBool(&sim, 1, true);
  TlvU8(&sim, 2, 4); // SimState::kReady
  TlvObj(&sim, 3, id);

  std::vector<std::uint8_t> sub;
  TlvI32(&sub, 1, 1);
  TlvI32(&sub, 2, 0);
  TlvBool(&sub, 3, true);
  TlvStr(&sub, 4, "+8613215555910");
  TlvStr(&sub, 5, "89860100000000059106");

  std::vector<std::uint8_t> op;
  TlvStr(&op, 1, "46001");
  TlvStr(&op, 2, "中国联通");
  TlvStr(&op, 3, "CUCC");
  TlvStr(&op, 4, "cn");

  std::vector<std::uint8_t> reg;
  TlvU8(&reg, 1, 4); // RegistrationState::kHome
  TlvU8(&reg, 2, 4); // RadioAccessTechnology::kLte
  TlvBool(&reg, 3, false);

  std::vector<std::uint8_t> sig;
  TlvBool(&sig, 1, true);
  TlvI32(&sig, 2, -65);
  TlvI32(&sig, 3, -90);
  TlvI32(&sig, 4, -10);
  TlvI32(&sig, 5, 20);

  std::vector<std::uint8_t> net;
  TlvObj(&net, 1, op);
  TlvObj(&net, 2, reg);
  TlvObj(&net, 3, reg);
  TlvObj(&net, 4, sig);

  std::vector<std::uint8_t> snap;
  TlvU8(&snap, 1, 1); // kSimulated
  TlvU64(&snap, 2, gen);
  TlvU64(&snap, 3, seq);
  TlvU64(&snap, 4, 0x7ff);
  TlvObj(&snap, 5, modem);
  TlvObj(&snap, 6, sim);
  TlvObj(&snap, 7, sub);
  TlvObj(&snap, 8, net);
  return snap;
}

std::vector<std::uint8_t> BuildHelloFrame(std::uint64_t session_id,
                                          std::uint64_t boot_gen,
                                          std::uint64_t prov_gen,
                                          std::uint64_t seq,
                                          const std::vector<std::uint8_t> &payload) {
  std::vector<std::uint8_t> frame;
  PutU32(&frame, 0x4344524d); // CDRM
  PutU16(&frame, 1);          // version
  PutU16(&frame, 1);          // kHello
  PutU16(&frame, 0);          // flags
  PutU16(&frame, 0);          // kOk
  PutU64(&frame, session_id);
  PutU64(&frame, boot_gen);
  PutU64(&frame, prov_gen);
  PutU64(&frame, seq);
  PutU64(&frame, 0);
  PutU64(&frame, 0);
  PutU32(&frame, static_cast<std::uint32_t>(payload.size()));
  frame.insert(frame.end(), payload.begin(), payload.end());
  return frame;
}

} // namespace

int main() {
  int fds[2] = {-1, -1};
  assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

  cdh::radio::StateStore store;
  cdh::radio::Channel channel;
  std::string err;
  assert(channel.Start(fds[1], &store, &err));

  // 1. 注入前导 UEFI ANSI 清屏序列 + 被截断的半截帧 + 伪魔数噪声
  const char noise[] = "\x1b[2J\x1b[01;01Hpartial-hello-tail-CDRM\x00\x99\x00\x01garbage";
  assert(write(fds[0], noise, sizeof(noise) - 1) ==
         static_cast<ssize_t>(sizeof(noise) - 1));

  // 2. 注入一帧头部合法但载荷被 N_TTY ICRNL 污染的坏 HELLO 帧，验证通道不会误关 fd
  std::vector<std::uint8_t> corrupted_payload = {0x00, 0x01, 0x00, 0x02, 0x0a};
  auto bad_hello = BuildHelloFrame(1001, 1, 1, 1, corrupted_payload);
  assert(write(fds[0], bad_hello.data(), bad_hello.size()) ==
         static_cast<ssize_t>(bad_hello.size()));

  // 3. 注入下一拍干净的完整 HELLO 帧，验证滑窗同步 + 快照落库 + HelloResponse 回包
  auto good_payload = BuildValidSnapshotPayload(1, 1);
  auto good_hello = BuildHelloFrame(1001, 1, 1, 1, good_payload);
  assert(write(fds[0], good_hello.data(), good_hello.size()) ==
         static_cast<ssize_t>(good_hello.size()));

  assert(channel.WaitUntilReady(std::chrono::milliseconds(2000)));
  assert(channel.IsReady());
  assert(channel.IsRunning());
  const auto snap = store.Current();
  assert(snap.has_value());
  assert(snap->modem.baseband_version == "4G_MODEM_22B_W26.02.5_P2");
  assert(snap->sim.identity.operator_alpha == "中国联通");

  // 读取 Guest 回发的 64 字节 HelloResponse
  std::uint8_t resp[64] = {};
  assert(read(fds[0], resp, sizeof(resp)) == 64);
  assert(resp[0] == 0x43 && resp[1] == 0x44 && resp[2] == 0x52 && resp[3] == 0x4d);
  assert(resp[7] == 2); // FrameKind::kHelloResponse

  channel.Stop();
  close(fds[0]);
  std::printf("PASS: channel_test\n");
  return 0;
}

#endif // CDH_RADIO_CHANNEL_HOST_TEST
