// engine/vendor/hardware 线 —— android.hardware.nfc（INfc v1）宿主通道直通服务实现。
//
// 注册 android.hardware.nfc.INfc/default，通过 virtio-console /dev/hvc11 与宿主侧
// 虚拟 NCI 2.0 NFCC（channels/nfc.sock）收发原始 NCI 帧：
//   - open() 打开 /dev/hvc11、切 cfmakeraw 原始字节流模式并启动读循环线程，随后异步回
//     OPEN_CPLT(OK)；若 /dev/hvc11 不可打开（旧宿主未挂 NFC 通道），优雅回退
//     OPEN_CPLT(FAILED)，避免 com.android.nfc WatchDog 超时自杀。
//   - write() 将 libnfc-nci 下发的 NCI Command/Data 完整写入 /dev/hvc11；
//   - 读线程按 3 字节 NCI 头（[MT/GID, OID, LEN]）+ LEN 字节负载切帧，经
//     INfcClientCallback::sendData() 上报给 libnfc-nci。

#include <android/binder_ibinder.h>
#include <android/binder_status.h>
#include <log/log.h>

extern "C" {
void ABinderProcess_setThreadPoolMaxThreadCount(uint32_t numThreads);
void ABinderProcess_joinThreadPool(void);
binder_status_t AServiceManager_addService(AIBinder* binder,
                                           const char* instance);
}

#include <aidl/android/hardware/nfc/BnNfc.h>
#include <aidl/android/hardware/nfc/INfcClientCallback.h>
#include <aidl/android/hardware/nfc/NfcCloseType.h>
#include <aidl/android/hardware/nfc/NfcConfig.h>
#include <aidl/android/hardware/nfc/NfcEvent.h>
#include <aidl/android/hardware/nfc/NfcStatus.h>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

using aidl::android::hardware::nfc::BnNfc;
using aidl::android::hardware::nfc::INfcClientCallback;
using aidl::android::hardware::nfc::NfcCloseType;
using aidl::android::hardware::nfc::NfcConfig;
using aidl::android::hardware::nfc::NfcEvent;
using aidl::android::hardware::nfc::NfcStatus;
using ndk::ScopedAStatus;

static constexpr const char* kNfcServiceName =
    "android.hardware.nfc.INfc/default";
static constexpr const char* kNfcDevicePath = "/dev/hvc11";
static constexpr auto kEventDelay = std::chrono::milliseconds(15);

namespace {

int openNfcChannel() {
  int fd = ::open(kNfcDevicePath, O_RDWR | O_NOCTTY | O_CLOEXEC);
  if (fd < 0) {
    ALOGW("open(%s) failed: %s (%d)", kNfcDevicePath, std::strerror(errno),
          errno);
    return -1;
  }
  struct termios tio {};
  if (::tcgetattr(fd, &tio) == 0) {
    ::cfmakeraw(&tio);
    tio.c_cc[VMIN] = 1;
    tio.c_cc[VTIME] = 0;
    if (::tcsetattr(fd, TCSANOW, &tio) != 0) {
      ALOGW("tcsetattr(%s) failed: %s", kNfcDevicePath, std::strerror(errno));
    }
  }
  return fd;
}

bool readExact(int fd, uint8_t* dst, size_t len,
               const std::atomic<bool>& stopFlag) {
  size_t got = 0;
  while (got < len) {
    if (stopFlag.load(std::memory_order_acquire)) {
      return false;
    }
    struct pollfd pfd {};
    pfd.fd = fd;
    pfd.events = POLLIN;
    int pr = ::poll(&pfd, 1, 200);
    if (pr == 0) {
      continue;
    }
    if (pr < 0) {
      if (errno == EINTR) continue;
      return false;
    }
    if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
      return false;
    }
    ssize_t n = ::read(fd, dst + got, len - got);
    if (n > 0) {
      got += static_cast<size_t>(n);
    } else if (n < 0 && (errno == EINTR || errno == EAGAIN)) {
      continue;
    } else {
      return false;
    }
  }
  return true;
}

class NfcService : public BnNfc {
 public:
  ~NfcService() override { stopChannelLockedWrapper(); }

  ScopedAStatus open(
      const std::shared_ptr<INfcClientCallback>& in_clientCallback) override {
    if (in_clientCallback == nullptr) {
      return ScopedAStatus::fromExceptionCode(EX_NULL_POINTER);
    }
    stopChannelLockedWrapper();
    int fd = openNfcChannel();
    {
      std::lock_guard<std::mutex> lock(mutex_);
      callback_ = in_clientCallback;
      fd_ = fd;
    }
    if (fd < 0) {
      ALOGW("open: %s unavailable, reply OPEN_CPLT(FAILED)", kNfcDevicePath);
      sendEventLater(in_clientCallback, NfcEvent::OPEN_CPLT, NfcStatus::FAILED);
      return ScopedAStatus::ok();
    }
    stopRead_.store(false, std::memory_order_release);
    readThread_ = std::thread(&NfcService::readLoop, this, fd, in_clientCallback);
    ALOGI("open: %s opened (fd=%d), reply OPEN_CPLT(OK)", kNfcDevicePath, fd);
    sendEventLater(in_clientCallback, NfcEvent::OPEN_CPLT, NfcStatus::OK);
    return ScopedAStatus::ok();
  }

  ScopedAStatus close(NfcCloseType in_type) override {
    ALOGI("close: type=%d", static_cast<int>(in_type));
    auto cb = stopChannelLockedWrapper();
    if (cb != nullptr) {
      sendEventLater(cb, NfcEvent::CLOSE_CPLT, NfcStatus::OK);
    }
    return ScopedAStatus::ok();
  }

  ScopedAStatus coreInitialized() override {
    auto cb = currentCallback();
    const bool ready = hasOpenFd();
    ALOGI("coreInitialized: ready=%d", ready ? 1 : 0);
    if (cb != nullptr) {
      sendEventLater(cb, NfcEvent::POST_INIT_CPLT,
                     ready ? NfcStatus::OK : NfcStatus::FAILED);
    }
    return ScopedAStatus::ok();
  }

  ScopedAStatus factoryReset() override { return ScopedAStatus::ok(); }

  ScopedAStatus getConfig(NfcConfig* _aidl_return) override {
    NfcConfig cfg{};
    cfg.maxIsoDepTransceiveLength = 261;
    *_aidl_return = cfg;
    return ScopedAStatus::ok();
  }

  ScopedAStatus powerCycle() override {
    auto cb = currentCallback();
    if (cb == nullptr) {
      return ScopedAStatus::ok();
    }
    stopChannelLockedWrapper();
    int fd = openNfcChannel();
    {
      std::lock_guard<std::mutex> lock(mutex_);
      callback_ = cb;
      fd_ = fd;
    }
    if (fd < 0) {
      sendEventLater(cb, NfcEvent::OPEN_CPLT, NfcStatus::FAILED);
      return ScopedAStatus::ok();
    }
    stopRead_.store(false, std::memory_order_release);
    readThread_ = std::thread(&NfcService::readLoop, this, fd, cb);
    sendEventLater(cb, NfcEvent::OPEN_CPLT, NfcStatus::OK);
    return ScopedAStatus::ok();
  }

  ScopedAStatus preDiscover() override {
    auto cb = currentCallback();
    const bool ready = hasOpenFd();
    if (cb != nullptr) {
      sendEventLater(cb, NfcEvent::PRE_DISCOVER_CPLT,
                     ready ? NfcStatus::OK : NfcStatus::FAILED);
    }
    return ScopedAStatus::ok();
  }

  ScopedAStatus write(const std::vector<uint8_t>& in_data,
                      int32_t* _aidl_return) override {
    int fd = -1;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      fd = fd_;
    }
    if (fd < 0) {
      *_aidl_return = 0;
      return ScopedAStatus::fromExceptionCodeWithMessage(
          EX_UNSUPPORTED_OPERATION, "nfc channel not open");
    }
    std::lock_guard<std::mutex> wlock(writeMutex_);
    size_t sent = 0;
    while (sent < in_data.size()) {
      ssize_t n = ::write(fd, in_data.data() + sent, in_data.size() - sent);
      if (n > 0) {
        sent += static_cast<size_t>(n);
      } else if (n < 0 && (errno == EINTR || errno == EAGAIN)) {
        continue;
      } else {
        ALOGE("write(%s) failed: %s", kNfcDevicePath, std::strerror(errno));
        break;
      }
    }
    *_aidl_return = static_cast<int32_t>(sent);
    return ScopedAStatus::ok();
  }

  ScopedAStatus setEnableVerboseLogging(bool in_enable) override {
    verbose_.store(in_enable, std::memory_order_relaxed);
    return ScopedAStatus::ok();
  }

  ScopedAStatus isVerboseLoggingEnabled(bool* _aidl_return) override {
    *_aidl_return = verbose_.load(std::memory_order_relaxed);
    return ScopedAStatus::ok();
  }

 private:
  void readLoop(int fd, std::shared_ptr<INfcClientCallback> cb) {
    uint8_t hdr[3];
    while (!stopRead_.load(std::memory_order_acquire)) {
      if (!readExact(fd, hdr, sizeof(hdr), stopRead_)) {
        break;
      }
      const size_t plen = hdr[2];
      std::vector<uint8_t> pkt(3 + plen);
      std::memcpy(pkt.data(), hdr, 3);
      if (plen > 0 && !readExact(fd, pkt.data() + 3, plen, stopRead_)) {
        break;
      }
      if (verbose_.load(std::memory_order_relaxed)) {
        ALOGD("rx NCI frame mt=%u gid=%u oid=%u len=%zu",
              (pkt[0] >> 5) & 0x7, pkt[0] & 0x0f, pkt[1] & 0x3f, plen);
      }
      if (cb != nullptr) {
        ScopedAStatus st = cb->sendData(pkt);
        if (!st.isOk()) {
          ALOGW("sendData failed: %s", st.getDescription().c_str());
        }
      }
    }
  }

  std::shared_ptr<INfcClientCallback> stopChannelLockedWrapper() {
    stopRead_.store(true, std::memory_order_release);
    int fdToClose = -1;
    std::shared_ptr<INfcClientCallback> cb;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      fdToClose = fd_;
      fd_ = -1;
      cb = std::move(callback_);
    }
    if (readThread_.joinable()) {
      readThread_.join();
    }
    if (fdToClose >= 0) {
      ::close(fdToClose);
    }
    return cb;
  }

  std::shared_ptr<INfcClientCallback> currentCallback() {
    std::lock_guard<std::mutex> lock(mutex_);
    return callback_;
  }

  bool hasOpenFd() {
    std::lock_guard<std::mutex> lock(mutex_);
    return fd_ >= 0;
  }

  static void sendEventLater(const std::shared_ptr<INfcClientCallback>& callback,
                             NfcEvent event, NfcStatus status) {
    std::thread([callback, event, status] {
      std::this_thread::sleep_for(kEventDelay);
      ScopedAStatus result = callback->sendEvent(event, status);
      if (!result.isOk()) {
        ALOGE("sendEvent(event=%d) failed: %s", static_cast<int>(event),
              result.getDescription().c_str());
      }
    }).detach();
  }

  std::mutex mutex_;
  std::mutex writeMutex_;
  std::shared_ptr<INfcClientCallback> callback_;
  int fd_ = -1;
  std::atomic<bool> stopRead_{true};
  std::atomic<bool> verbose_{false};
  std::thread readThread_;
};

}  // namespace

int main() {
  ABinderProcess_setThreadPoolMaxThreadCount(4);
  auto svc = ndk::SharedRefBase::make<NfcService>();
  if (AServiceManager_addService(svc->asBinder().get(), kNfcServiceName) !=
      STATUS_OK) {
    return 2;
  }
  ABinderProcess_joinThreadPool();
  return 0;
}
