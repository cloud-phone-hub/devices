// engine/vendor/hardware —— Android 13 HIDL android.hardware.bluetooth@1.1 自研服务。
//
// 优先连接 /dev/hvc5（virtio-console H4 宿主通道，同 AIDL 版 bluetooth_service.cpp）；
// 若实例未挂载 /dev/hvc5，自动回退内置轻量虚拟 HCI 应答器，确保蓝牙栈初始化恒成功。

#define LOG_TAG "CdhBtHidl"

#include <android/hardware/bluetooth/1.1/IBluetoothHci.h>
#include <android/hardware/bluetooth/1.1/IBluetoothHciCallbacks.h>
#include <hidl/HidlTransportSupport.h>
#include <log/log.h>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <sys/system_properties.h>
#include <termios.h>
#include <unistd.h>

namespace {

using ::android::sp;
using ::android::hardware::hidl_death_recipient;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::bluetooth::V1_0::HciPacket;
using ::android::hardware::bluetooth::V1_0::Status;
using V1_0_Cb = ::android::hardware::bluetooth::V1_0::IBluetoothHciCallbacks;
using V1_1_Cb = ::android::hardware::bluetooth::V1_1::IBluetoothHciCallbacks;
using ::android::hardware::bluetooth::V1_1::IBluetoothHci;

enum H4Type : uint8_t {
  kCommand = 0x01,
  kAcl = 0x02,
  kSco = 0x03,
  kEvent = 0x04,
  kIso = 0x05,
};

constexpr uint8_t kResetCommand[] = {0x03, 0x0c, 0x00};
constexpr uint8_t kEventCommandComplete = 0x0e;

uint16_t readLe16(const uint8_t *p) {
  return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

size_t h4HeaderLen(uint8_t type) {
  switch (type) {
    case kCommand:
    case kSco:
      return 3;
    case kAcl:
    case kIso:
      return 4;
    case kEvent:
      return 2;
    default:
      return 0;
  }
}

size_t h4PayloadLen(uint8_t type, const uint8_t *hdr) {
  switch (type) {
    case kCommand:
    case kSco:
      return hdr[2];
    case kAcl:
      return readLe16(hdr + 2);
    case kEvent:
      return hdr[1];
    case kIso:
      return readLe16(hdr + 2) & 0x3fff;
    default:
      return 0;
  }
}

bool readFull(int fd, uint8_t *buf, size_t n, const std::atomic<bool> &run, int deadlineMs) {
  size_t got = 0;
  auto start = std::chrono::steady_clock::now();
  while (got < n) {
    if (!run.load()) return false;
    int waitMs = 100;
    if (deadlineMs >= 0) {
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now() - start)
                         .count();
      int remain = deadlineMs - static_cast<int>(elapsed);
      if (remain <= 0) return false;
      if (remain < waitMs) waitMs = remain;
    }
    pollfd p{fd, POLLIN, 0};
    int pr = ::poll(&p, 1, waitMs);
    if (pr < 0) {
      if (errno == EINTR) continue;
      return false;
    }
    if (pr == 0) continue;
    ssize_t r = ::read(fd, buf + got, n - got);
    if (r > 0) {
      got += static_cast<size_t>(r);
      continue;
    }
    if (r < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) {
      continue;
    }
    return false;
  }
  return true;
}

bool writeAll(int fd, const uint8_t *buf, size_t n) {
  size_t off = 0;
  while (off < n) {
    ssize_t w = ::write(fd, buf + off, n - off);
    if (w > 0) {
      off += static_cast<size_t>(w);
      continue;
    }
    if (w < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) {
      continue;
    }
    return false;
  }
  return true;
}

std::string resolveDevPath() {
  const char *env = std::getenv("CDH_BT_HCI_DEV");
  if (env != nullptr && env[0] != '\0') return env;
  char prop[PROP_VALUE_MAX] = {0};
  if (__system_property_get("vendor.ser.bt-uart", prop) > 0 && prop[0] != '\0') {
    return prop;
  }
  return "/dev/hvc5";
}

// 内置极简虚拟 HCI 命令应答器（无 /dev/hvc5 时兜底）
std::vector<uint8_t> buildVirtualCommandComplete(uint16_t opcode, const std::vector<uint8_t> &params) {
  std::vector<uint8_t> ev;
  ev.reserve(5 + params.size());
  ev.push_back(0x0e);  // Command Complete
  ev.push_back(static_cast<uint8_t>(3 + params.size()));
  ev.push_back(0x01);  // Num_HCI_Command_Packets
  ev.push_back(static_cast<uint8_t>(opcode & 0xff));
  ev.push_back(static_cast<uint8_t>((opcode >> 8) & 0xff));
  ev.insert(ev.end(), params.begin(), params.end());
  return ev;
}

std::vector<uint8_t> handleVirtualHciCommand(const HciPacket &cmd) {
  if (cmd.size() < 3) {
    return buildVirtualCommandComplete(0x0000, {0x00});
  }
  const uint16_t opcode = readLe16(cmd.data());
  switch (opcode) {
    case 0x1001:  // Read Local Version Information
      return buildVirtualCommandComplete(opcode, {0x00, 0x0b, 0x00, 0x00, 0x0b, 0x1d, 0x00, 0x00, 0x00});
    case 0x1002: {  // Read Local Supported Commands (64B)
      std::vector<uint8_t> p(65, 0x00);
      p[0] = 0x00;
      p[1] = 0x20;
      p[6] = 0xc0;
      p[15] = 0xe8;
      p[16] = 0x3f;
      p[21] = 0x10;  // Octet 20 bit 4: Read Encryption Key Size (Android 14+ controller.cc 强校验)
      p[26] = 0xf7;
      p[27] = 0xff;
      p[28] = 0xff;
      return buildVirtualCommandComplete(opcode, p);
    }
    case 0x1408: {  // Read Encryption Key Size (Status + Connection_Handle + Key_Size)
      uint8_t lo = (cmd.size() >= 4) ? cmd[3] : 0x00;
      uint8_t hi = (cmd.size() >= 5) ? cmd[4] : 0x00;
      return buildVirtualCommandComplete(opcode, {0x00, lo, hi, 0x10});
    }
    case 0x1003:  // Read Local Supported Features
      return buildVirtualCommandComplete(opcode, {0x00, 0xff, 0xfe, 0x8f, 0xfe, 0xd8, 0x3f, 0x5b, 0x87});
    case 0x1004: {  // Read Local Extended Features
      const uint8_t page = (cmd.size() >= 4) ? cmd[3] : 0x00;
      if (page == 0x00) {
        return buildVirtualCommandComplete(
            opcode, {0x00, 0x00, 0x01, 0xff, 0xfe, 0x8f, 0xfe, 0xd8, 0x3f, 0x5b, 0x87});
      }
      if (page == 0x01) {
        return buildVirtualCommandComplete(
            opcode, {0x00, 0x01, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
      }
      return buildVirtualCommandComplete(
          opcode, {0x00, page, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
    }
    case 0x1005:  // Read Buffer Size
      return buildVirtualCommandComplete(opcode, {0x00, 0x00, 0x04, 0x40, 0x08, 0x00, 0x08, 0x00});
    case 0x1009:  // Read BD_ADDR
      return buildVirtualCommandComplete(opcode, {0x00, 0x56, 0x34, 0x12, 0x78, 0x9a, 0x22});
    case 0x0c14: {  // Read Local Name (Status + 248B UTF-8 Name)
      std::vector<uint8_t> p(249, 0x00);
      const char kName[] = "Android Bluetooth";
      std::memcpy(p.data() + 1, kName, sizeof(kName) - 1);
      return buildVirtualCommandComplete(opcode, p);
    }
    case 0x0c23:  // Read Class of Device
      return buildVirtualCommandComplete(opcode, {0x00, 0x0c, 0x02, 0x5a});
    case 0x0c25:  // Read Voice Setting
      return buildVirtualCommandComplete(opcode, {0x00, 0x60, 0x00});
    case 0x0c38:  // Read Number of Supported IAC
      return buildVirtualCommandComplete(opcode, {0x00, 0x02});
    case 0x0c39:  // Read Current IAC LAP
      return buildVirtualCommandComplete(opcode, {0x00, 0x01, 0x33, 0x8b, 0x9e});
    case 0x0c44:  // Read Inquiry Mode
      return buildVirtualCommandComplete(opcode, {0x00, 0x02});
    case 0x0c46:  // Read Page Scan Type
      return buildVirtualCommandComplete(opcode, {0x00, 0x00});
    case 0x0c51: {  // Read Extended Inquiry Response (Status + FEC + 240B EIR)
      std::vector<uint8_t> p(242, 0x00);
      return buildVirtualCommandComplete(opcode, p);
    }
    case 0x0c55:  // Read Simple Pairing Mode
      return buildVirtualCommandComplete(opcode, {0x00, 0x01});
    case 0x0c58:  // Read Inquiry Response Transmit Power Level
      return buildVirtualCommandComplete(opcode, {0x00, 0x00});
    case 0x0c6d:  // Read LE Host Support
      return buildVirtualCommandComplete(opcode, {0x00, 0x01, 0x00});
    case 0x100b:  // Read Local Supported Codecs
      return buildVirtualCommandComplete(opcode, {0x00, 0x00, 0x00});
    case 0x2002:  // LE Read Buffer Size [v1]
      return buildVirtualCommandComplete(opcode, {0x00, 0xfb, 0x00, 0x0f});
    case 0x2003:  // LE Read Local Supported Features
      return buildVirtualCommandComplete(opcode, {0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
    case 0x2007:  // LE Read Advertising Channel Tx Power
      return buildVirtualCommandComplete(opcode, {0x00, 0x00});
    case 0x200f:  // LE Read Filter Accept List Size
      return buildVirtualCommandComplete(opcode, {0x00, 0x10});
    case 0x201c:  // LE Read Supported States
      return buildVirtualCommandComplete(opcode, {0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0x03, 0x00, 0x00});
    case 0x2023:  // LE Read Suggested Default Data Length
      return buildVirtualCommandComplete(opcode, {0x00, 0x1b, 0x00, 0x48, 0x01});
    case 0x202a:  // LE Read Resolving List Size
      return buildVirtualCommandComplete(opcode, {0x00, 0x08});
    case 0x202f:  // LE Read Maximum Data Length
      return buildVirtualCommandComplete(opcode, {0x00, 0xfb, 0x00, 0x90, 0x08, 0xfb, 0x00, 0x90, 0x08});
    case 0x2036:  // LE Read Maximum Advertising Data Length
      return buildVirtualCommandComplete(opcode, {0x00, 0x67, 0x06});
    case 0x2037:  // LE Read Number of Supported Advertising Sets
      return buildVirtualCommandComplete(opcode, {0x00, 0x04});
    case 0x2038:  // LE Read Periodic Advertiser List Size
      return buildVirtualCommandComplete(opcode, {0x00, 0x08});
    case 0x204b:  // LE Read Transmit Power
      return buildVirtualCommandComplete(opcode, {0x00, 0xf0, 0x0c});
    case 0x204c:  // LE Read RF Path Compensation
      return buildVirtualCommandComplete(opcode, {0x00, 0x00, 0x00, 0x00, 0x00});
    case 0x2060:  // LE Read Buffer Size [v2]
      return buildVirtualCommandComplete(opcode, {0x00, 0xfb, 0x00, 0x0f, 0xfb, 0x00, 0x08});
    case 0xfd53:  // Android Vendor: LE Get Vendor Capabilities -> Unknown HCI Command (0x01)
      return buildVirtualCommandComplete(opcode, {0x01});
    case 0xfd5e:  // Android Vendor: Bluetooth Quality Report (BQR) -> Status + 4B Mask
      return buildVirtualCommandComplete(opcode, {0x00, 0x00, 0x00, 0x00, 0x00});
    default: {
      // 未实现的 Vendor Specific Command (OGF 0x3f, 0xfc00..0xffff) 回 Unknown HCI Command (0x01)，
      // 避免蓝牙栈因收到 Success 但参数长度为 1B 触发长度断言崩溃。
      if ((opcode & 0xfc00) == 0xfc00) {
        return buildVirtualCommandComplete(opcode, {0x01});
      }
      // Inquiry / Create Connection 等异步命令回 Command Status，其余回 Command Complete OK
      if (opcode == 0x0401 || opcode == 0x0405 || opcode == 0x0419) {
        return {0x0f, 0x04, 0x00, 0x01, static_cast<uint8_t>(opcode & 0xff),
                static_cast<uint8_t>((opcode >> 8) & 0xff)};
      }
      return buildVirtualCommandComplete(opcode, {0x00});
    }
  }
}

class BluetoothHciImpl : public IBluetoothHci, public hidl_death_recipient {
 public:
  ~BluetoothHciImpl() override { halt(); }

  void serviceDied(uint64_t, const ::android::wp<::android::hidl::base::V1_0::IBase> &) override {
    ALOGW("Bluetooth stack died; closing HCI");
    halt();
  }

  Return<void> initialize(const sp<V1_0_Cb> &cb) override {
    return initCommon(cb, nullptr);
  }

  Return<void> initialize_1_1(const sp<V1_1_Cb> &cb) override {
    return initCommon(cb, cb);
  }

  Return<void> sendHciCommand(const hidl_vec<uint8_t> &command) override {
    bool useVirtual = false;
    sp<V1_0_Cb> cb;
    {
      std::lock_guard<std::mutex> lk(mu_);
      useVirtual = virtualMode_;
      cb = cb10_;
    }
    if (useVirtual) {
      if (cb != nullptr) {
        HciPacket ev = handleVirtualHciCommand(command);
        auto ret = cb->hciEventReceived(ev);
        (void)ret.isOk();
      }
      return Void();
    }
    sendPacket(kCommand, command);
    return Void();
  }

  Return<void> sendAclData(const hidl_vec<uint8_t> &data) override {
    if (!isVirtual()) sendPacket(kAcl, data);
    return Void();
  }

  Return<void> sendScoData(const hidl_vec<uint8_t> &data) override {
    if (!isVirtual()) sendPacket(kSco, data);
    return Void();
  }

  Return<void> sendIsoData(const hidl_vec<uint8_t> &data) override {
    if (!isVirtual()) sendPacket(kIso, data);
    return Void();
  }

  Return<void> close() override {
    halt();
    return Void();
  }

 private:
  bool isVirtual() {
    std::lock_guard<std::mutex> lk(mu_);
    return virtualMode_;
  }

  Return<void> initCommon(const sp<V1_0_Cb> &cb10, const sp<V1_1_Cb> &cb11) {
    if (cb10 == nullptr) return Void();
    halt();

    const std::string devPath = resolveDevPath();
    int fd = ::open(devPath.c_str(), O_RDWR | O_NOCTTY | O_CLOEXEC);
    bool useVirtual = false;
    if (fd >= 0) {
      termios tio{};
      if (::tcgetattr(fd, &tio) == 0) {
        ::cfmakeraw(&tio);
        ::tcsetattr(fd, TCSANOW, &tio);
        ::tcflush(fd, TCIOFLUSH);
      }
      if (!handshakeReset(fd)) {
        ::close(fd);
        fd = -1;
        useVirtual = true;
      }
    } else {
      useVirtual = true;
    }

    {
      std::lock_guard<std::mutex> lk(mu_);
      cb10_ = cb10;
      cb11_ = cb11;
      fd_ = fd;
      virtualMode_ = useVirtual;
      running_.store(!useVirtual);
      (void)cb10_->linkToDeath(this, 0);
      if (!useVirtual) {
        rxThread_ = std::thread(&BluetoothHciImpl::rxLoop, this, fd);
      }
    }
    ALOGI("Bluetooth HIDL 1.1 initialized (mode=%s)", useVirtual ? "virtual" : devPath.c_str());
    auto ret = cb10->initializationComplete(Status::SUCCESS);
    (void)ret.isOk();
    return Void();
  }

  bool handshakeReset(int fd) {
    uint8_t frame[1 + sizeof(kResetCommand)];
    frame[0] = kCommand;
    std::memcpy(frame + 1, kResetCommand, sizeof(kResetCommand));
    if (!writeAll(fd, frame, sizeof(frame))) return false;

    std::atomic<bool> alive{true};
    for (int i = 0; i < 16; ++i) {
      uint8_t type = 0;
      if (!readFull(fd, &type, 1, alive, 1500)) return false;
      const size_t hlen = h4HeaderLen(type);
      if (hlen == 0) return false;
      uint8_t hdr[4] = {0};
      if (!readFull(fd, hdr, hlen, alive, 1500)) return false;
      const size_t plen = h4PayloadLen(type, hdr);
      std::vector<uint8_t> payload(plen);
      if (plen > 0 && !readFull(fd, payload.data(), plen, alive, 1500)) return false;
      if (type == kEvent && hdr[0] == kEventCommandComplete && plen >= 3 &&
          payload[1] == kResetCommand[0] && payload[2] == kResetCommand[1]) {
        return true;
      }
    }
    return false;
  }

  void sendPacket(uint8_t type, const hidl_vec<uint8_t> &body) {
    if (body.size() == 0) return;
    std::lock_guard<std::mutex> wlk(writeMu_);
    int fd = -1;
    {
      std::lock_guard<std::mutex> lk(mu_);
      fd = fd_;
    }
    if (fd < 0) return;
    std::vector<uint8_t> frame(1 + body.size());
    frame[0] = type;
    std::memcpy(frame.data() + 1, body.data(), body.size());
    (void)writeAll(fd, frame.data(), frame.size());
  }

  void rxLoop(int fd) {
    while (running_.load()) {
      uint8_t type = 0;
      if (!readFull(fd, &type, 1, running_, -1)) break;
      const size_t hlen = h4HeaderLen(type);
      if (hlen == 0) break;
      uint8_t hdr[4] = {0};
      if (!readFull(fd, hdr, hlen, running_, 2000)) break;
      const size_t plen = h4PayloadLen(type, hdr);
      HciPacket pkt(hlen + plen);
      std::memcpy(pkt.data(), hdr, hlen);
      if (plen > 0 && !readFull(fd, pkt.data() + hlen, plen, running_, 2000)) break;

      sp<V1_0_Cb> cb10;
      sp<V1_1_Cb> cb11;
      {
        std::lock_guard<std::mutex> lk(mu_);
        cb10 = cb10_;
        cb11 = cb11_;
      }
      if (cb10 == nullptr) break;
      switch (type) {
        case kEvent:
          (void)cb10->hciEventReceived(pkt).isOk();
          break;
        case kAcl:
          (void)cb10->aclDataReceived(pkt).isOk();
          break;
        case kSco:
          (void)cb10->scoDataReceived(pkt).isOk();
          break;
        case kIso:
          if (cb11 != nullptr) (void)cb11->isoDataReceived(pkt).isOk();
          break;
        default:
          break;
      }
    }
  }

  void halt() {
    running_.store(false);
    std::thread rx;
    int fd = -1;
    sp<V1_0_Cb> cb10;
    {
      std::lock_guard<std::mutex> lk(mu_);
      rx = std::move(rxThread_);
      fd = fd_;
      fd_ = -1;
      virtualMode_ = false;
      cb10 = std::move(cb10_);
      cb11_ = nullptr;
    }
    if (cb10 != nullptr) {
      (void)cb10->unlinkToDeath(this);
    }
    if (rx.joinable()) rx.join();
    if (fd >= 0) ::close(fd);
  }

  std::mutex mu_;
  std::mutex writeMu_;
  sp<V1_0_Cb> cb10_;
  sp<V1_1_Cb> cb11_;
  int fd_ = -1;
  bool virtualMode_ = false;
  std::atomic<bool> running_{false};
  std::thread rxThread_;
};

}  // namespace

int main() {
  ::android::hardware::configureRpcThreadpool(2, true);
  sp<IBluetoothHci> service = new BluetoothHciImpl();
  if (service->registerAsService("default") != ::android::OK) {
    ALOGE("Failed to register IBluetoothHci@1.1/default");
    return 1;
  }
  ALOGI("IBluetoothHci@1.1/default service ready");
  ::android::hardware::joinRpcThreadpool();
  return 1;
}
