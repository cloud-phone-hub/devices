// engine/vendor/hardware 线 —— android.hardware.bluetooth（IBluetoothHci/default v1）自研服务。
//
// 注册 android.hardware.bluetooth.IBluetoothHci/default，供 framework Bluetooth 栈
// （Fluoride）使用。本 HAL 是**纯 HCI 传输层**：把栈下发的 HCI Command/ACL/SCO/ISO 以
// H4 帧（1 字节类型前缀 + HCI 包体）写入串口设备节点，把控制器回上来的 Event/ACL/SCO/ISO
// H4 帧拆出包体回调给栈。扫描、连接、配对等全部逻辑在 framework 栈，本层不解释、不改写，
// 忠实透传——只要宿主对端（gold/note13：stage front channels/bt-hci.sock + netsimd 仿真）
// 正确应答 HCI 命令，蓝牙即可工作。
//
// 设备节点默认 /dev/hvc5（virtio-console，与 AOSP reference default 同默认值），
// 可被属性 vendor.ser.bt-uart 覆盖（换机型/线号）；再可被 env CDH_BT_HCI_DEV 覆盖（联调）。
// H4 帧长表逐字对齐 AOSP hci/hci_internals.h 与本仓 host 侧 bthci/stream.go：
//   Command(0x01) body 头 3B(len@2,1B) / ACL(0x02) 4B(len@2,2B LE) /
//   SCO(0x03) 3B(len@2,1B) / Event(0x04) 2B(len@1,1B) / ISO(0x05) 4B(len@2,2B LE & 0x3fff)。
// 闭包最小：只用 std + bionic（fcntl/termios/poll/sys/system_properties），-fno-exceptions。

#include <android/binder_ibinder.h>
#include <android/binder_status.h>
#if CDH_BLUETOOTH_SAMSUNG
#include "samsung/bt_samsung_ext.h"
#endif

extern "C" {
void ABinderProcess_setThreadPoolMaxThreadCount(uint32_t numThreads);
void ABinderProcess_joinThreadPool(void);
binder_status_t AServiceManager_addService(AIBinder *binder,
                                           const char *instance);
}

#include <aidl/android/hardware/bluetooth/BnBluetoothHci.h>
#include <aidl/android/hardware/bluetooth/IBluetoothHciCallbacks.h>
#include <aidl/android/hardware/bluetooth/Status.h>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>

#include <sys/system_properties.h>

using aidl::android::hardware::bluetooth::BnBluetoothHci;
using aidl::android::hardware::bluetooth::IBluetoothHciCallbacks;
using aidl::android::hardware::bluetooth::Status;
using ndk::ScopedAStatus;

namespace {

constexpr const char *kServiceName =
    "android.hardware.bluetooth.IBluetoothHci/default";
constexpr const char *kDefaultDevPath = "/dev/hvc5";
constexpr const char *kDevPathProperty = "vendor.ser.bt-uart";

// H4 包类型（Bluetooth Core Vol 4 Part A §2）
enum H4Type : uint8_t {
  kCommand = 0x01,
  kAcl = 0x02,
  kSco = 0x03,
  kEvent = 0x04,
  kIso = 0x05,
};

// HCI Reset（OGF 0x03 / OCF 0x0003 = opcode 0x0c03，参数长 0）包体，供初始化握手。
constexpr uint8_t kResetCommand[] = {0x03, 0x0c, 0x00};
constexpr uint8_t kEventCommandComplete = 0x0e;

uint16_t readLe16(const uint8_t *p) {
  return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

// 给定类型，返回 body 头长度（不含类型字节）；未知类型返回 0。
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

// 从 body 头（不含类型字节）算出后续 payload 长度，语义同 hci_internals.h/stream.go。
size_t h4PayloadLen(uint8_t type, const uint8_t *hdr /* body 头 */) {
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

// 读满 n 字节。每 <=100ms 复查 run（close 置位后即退出），deadlineMs>=0 时超时失败。
// 返回 false：run 已停 / 超时 / EOF / 不可恢复错误。
bool readFull(int fd, uint8_t *buf, size_t n, const std::atomic<bool> &run,
              int deadlineMs) {
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
    if (pr == 0) continue;  // 超时：复查 run / deadline
    ssize_t r = ::read(fd, buf + got, n - got);
    if (r > 0) {
      got += static_cast<size_t>(r);
      continue;
    }
    if (r < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) {
      continue;
    }
    return false;  // r==0（EOF）或硬错误
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
  if (__system_property_get(kDevPathProperty, prop) > 0 && prop[0] != '\0') {
    return prop;
  }
  return kDefaultDevPath;
}

class BluetoothHciService : public BnBluetoothHci {
 public:
  BluetoothHciService() {
    mDeathRecipient = AIBinder_DeathRecipient_new(&BluetoothHciService::OnDeath);
  }
  ~BluetoothHciService() override {
    halt();
    if (mDeathRecipient != nullptr) {
      AIBinder_DeathRecipient_delete(mDeathRecipient);
    }
  }

  BluetoothHciService(const BluetoothHciService &) = delete;
  BluetoothHciService &operator=(const BluetoothHciService &) = delete;

  ScopedAStatus initialize(
      const std::shared_ptr<IBluetoothHciCallbacks> &in_callback) override {
    if (in_callback == nullptr) {
      return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    {
      std::lock_guard<std::mutex> guard(mStateMutex);
      if (mRunning.load()) {
        in_callback->initializationComplete(Status::ALREADY_INITIALIZED);
        return ScopedAStatus::ok();
      }
      mCallback = in_callback;
    }

    int fd = openDevice();
    if (fd < 0) {
      std::lock_guard<std::mutex> guard(mStateMutex);
      mCallback.reset();
      in_callback->initializationComplete(Status::UNABLE_TO_OPEN_INTERFACE);
      return ScopedAStatus::ok();
    }
    {
      std::lock_guard<std::mutex> guard(mStateMutex);
      mFd = fd;
    }
    // 栈客户端死亡 → 关闭接口，释放设备节点以便重启（下次 initialize 可重连）。
    AIBinder_linkToDeath(in_callback->asBinder().get(), mDeathRecipient, this);

    // 初始化期做一次 HCI Reset 握手（对齐 AOSP emulator 路径）：清控制器/宿主仿真状态。
    // 失败非致命（best-effort，1s 超时）。
    mRunning.store(true);
    performReset();

    mReader = std::thread(&BluetoothHciService::readerLoop, this);

    auto cbStatus = in_callback->initializationComplete(Status::SUCCESS);
    if (!cbStatus.isOk()) {
      halt();
      return ScopedAStatus::fromExceptionCode(EX_TRANSACTION_FAILED);
    }
    return ScopedAStatus::ok();
  }

  ScopedAStatus close() override {
    halt();
    return ScopedAStatus::ok();
  }

  ScopedAStatus sendHciCommand(const std::vector<uint8_t> &in_command) override {
    return send(kCommand, in_command);
  }
  ScopedAStatus sendAclData(const std::vector<uint8_t> &in_data) override {
    return send(kAcl, in_data);
  }
  ScopedAStatus sendScoData(const std::vector<uint8_t> &in_data) override {
    return send(kSco, in_data);
  }
  ScopedAStatus sendIsoData(const std::vector<uint8_t> &in_data) override {
    return send(kIso, in_data);
  }

 private:
  static void OnDeath(void *cookie) {
    auto *self = static_cast<BluetoothHciService *>(cookie);
    self->close();
  }

  int openDevice() {
    std::string path = resolveDevPath();
    int fd = ::open(path.c_str(), O_RDWR | O_NOCTTY | O_CLOEXEC);
    if (fd < 0) return -1;
    termios ts;
    if (tcgetattr(fd, &ts) == 0) {
      cfmakeraw(&ts);
      tcsetattr(fd, TCSANOW, &ts);
    }
    return fd;
  }

  // 在 mWriteMutex 下写一个完整 H4 帧（类型字节 + body）。
  bool writePacket(uint8_t type, const uint8_t *body, size_t len) {
    std::lock_guard<std::mutex> guard(mWriteMutex);
    if (mFd < 0) return false;
    if (!writeAll(mFd, &type, 1)) return false;
    if (len > 0 && !writeAll(mFd, body, len)) return false;
    return true;
  }

  ScopedAStatus send(uint8_t type, const std::vector<uint8_t> &packet) {
    if (packet.empty()) {
      return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    std::lock_guard<std::mutex> guard(mStateMutex);
    if (!mRunning.load() || mFd < 0) {
      return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
    if (!writePacket(type, packet.data(), packet.size())) {
      return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
    return ScopedAStatus::ok();
  }

  // 读一个完整 H4 帧，outBody 收“不含类型字节”的包体（与 AIDL byte[] 语义一致）。
  bool readPacket(uint8_t &outType, std::vector<uint8_t> &outBody,
                  int deadlineMs) {
    int fd;
    {
      std::lock_guard<std::mutex> guard(mStateMutex);
      fd = mFd;
    }
    if (fd < 0) return false;
    uint8_t type = 0;
    if (!readFull(fd, &type, 1, mRunning, deadlineMs)) return false;
    size_t hdrLen = h4HeaderLen(type);
    if (hdrLen == 0) return false;  // 未知类型：视为流失步，停
    uint8_t hdr[4];
    if (!readFull(fd, hdr, hdrLen, mRunning, deadlineMs)) return false;
    size_t payload = h4PayloadLen(type, hdr);
    outType = type;
    outBody.resize(hdrLen + payload);
    std::memcpy(outBody.data(), hdr, hdrLen);
    if (payload > 0 &&
        !readFull(fd, outBody.data() + hdrLen, payload, mRunning, deadlineMs)) {
      return false;
    }
    return true;
  }

  void performReset() {
    if (!writePacket(kCommand, kResetCommand, sizeof(kResetCommand))) {
      return;
    }
    // 在读侧等 Reset 的 Command Complete（body: [0]=0x0e, [2..3]=opcode 0x0c03）。
    uint8_t type;
    std::vector<uint8_t> body;
    while (readPacket(type, body, /*deadlineMs=*/1000)) {
      if (type == kEvent && body.size() >= 4 && body[0] == kEventCommandComplete &&
          readLe16(body.data() + 2) == 0x0c03) {
        return;  // Reset 完成
      }
      // 其余事件在初始化期丢弃（栈尚未收到 initializationComplete，不应有并发数据）。
    }
  }

  void readerLoop() {
    uint8_t type;
    std::vector<uint8_t> body;
    while (mRunning.load()) {
      if (!readPacket(type, body, /*deadlineMs=*/-1)) {
        break;  // EOF/错误/run 停止
      }
      std::shared_ptr<IBluetoothHciCallbacks> cb;
      {
        std::lock_guard<std::mutex> guard(mStateMutex);
        cb = mCallback;
      }
      if (cb == nullptr) continue;
      switch (type) {
        case kAcl:
          cb->aclDataReceived(body);
          break;
        case kSco:
          cb->scoDataReceived(body);
          break;
        case kEvent:
          cb->hciEventReceived(body);
          break;
        case kIso:
          cb->isoDataReceived(body);
          break;
        default:
          break;  // 控制器不该回命令帧，忽略
      }
    }
  }

  // 幂等停：置停 → join reader → 关 fd → 清状态（可从 binder 线程或死亡回调进入）。
  void halt() {
    bool was = mRunning.exchange(false);
    std::shared_ptr<IBluetoothHciCallbacks> cb;
    int fd = -1;
    {
      std::lock_guard<std::mutex> guard(mStateMutex);
      fd = mFd;
      mFd = -1;
      cb.swap(mCallback);
    }
    if (mReader.joinable()) {
      if (mReader.get_id() == std::this_thread::get_id()) {
        mReader.detach();  // 不会发生：readerLoop 不回调 halt
      } else {
        mReader.join();
      }
    }
    if (fd >= 0) {
      ::close(fd);
    }
    if (was && cb != nullptr) {
      AIBinder_unlinkToDeath(cb->asBinder().get(), mDeathRecipient, this);
    }
  }

  std::mutex mStateMutex;
  std::mutex mWriteMutex;
  int mFd = -1;
  std::atomic<bool> mRunning{false};
  std::shared_ptr<IBluetoothHciCallbacks> mCallback;
  std::thread mReader;
  AIBinder_DeathRecipient *mDeathRecipient = nullptr;
};

binder_status_t registerAsService(
    const std::shared_ptr<ndk::ICInterface> &service, const char *name) {
  return AServiceManager_addService(service->asBinder().get(), name);
}

}  // namespace

int main() {
  // 4 个 binder 线程处理下行 send*，读侧另有一根 std::thread。
  ABinderProcess_setThreadPoolMaxThreadCount(4);
  auto service = ndk::SharedRefBase::make<BluetoothHciService>();
  if (registerAsService(service, kServiceName) != STATUS_OK) {
    return 2;
  }
#if CDH_BLUETOOTH_SAMSUNG
  cdh::bluetooth::RegisterSamsungBluetooth();
#endif
  ABinderProcess_joinThreadPool();
  return 0;
}
