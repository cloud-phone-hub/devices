#pragma once

// 宿主相机视频输入接收器（/dev/hvc12 CDCM 协议）与 AHardwareBuffer 帧渲染器。
// 架构参考 cloud-droid-hub/devices/nothingphone4a-16/camera/buffer.cc，使用
// dlopen("libnativewindow.so") 零额外链接依赖实现 A13~A17 全代通用。

#include <aidl/android/hardware/camera/device/Stream.h>
#include <aidl/android/hardware/camera/device/StreamBuffer.h>
#include <aidl/android/hardware/common/NativeHandle.h>
#include <aidl/android/hardware/graphics/common/BufferUsage.h>
#include <aidl/android/hardware/graphics/common/Dataspace.h>
#include <aidl/android/hardware/graphics/common/PixelFormat.h>
#include <android/hardware_buffer.h>
#include <android/log.h>

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

struct native_handle;

namespace cdh_camera {

static constexpr const char *kVideoTag = "cdh_camera_video";
static constexpr const char *kCameraHvcPath = "/dev/hvc12";
static constexpr size_t kCdcmHeaderSize = 24;
static constexpr uint32_t kMaxFrameBytes = 8U * 1024U * 1024U;

static constexpr int32_t kFmtRgba8888 = 1;
static constexpr int32_t kFmtBlob = 0x21;
static constexpr int32_t kFmtImplDefined = 0x22;
static constexpr int32_t kFmtYuv420888 = 0x23;
static constexpr int32_t kFmtYv12 = 0x32315659;

struct VideoFrame {
  uint16_t width = 0;
  uint16_t height = 0;
  uint32_t seq = 0;
  std::vector<uint8_t> i420;
  std::vector<uint8_t> jpeg;
};

struct StreamMeta {
  int32_t id = -1;
  int32_t width = 0;
  int32_t height = 0;
  int32_t reqFormat = 0;
  int32_t overrideFormat = 0;
  int32_t bufferSize = 0;
  uint64_t usage = 0;
  std::string physicalCameraId;
};

inline int32_t selectOverrideFormat(int32_t format) {
  // IMPLEMENTATION_DEFINED 统一覆盖为 RGBA_8888，确保 gralloc mapper 支持 CPU_WRITE_OFTEN
  if (format == kFmtImplDefined) {
    return kFmtRgba8888;
  }
  return format;
}

class NativeWindowApi {
 public:
  using ImportFn = int (*)(const AHardwareBuffer_Desc *, const native_handle *,
                           int32_t, AHardwareBuffer **);

  static NativeWindowApi &Instance() {
    static NativeWindowApi api;
    return api;
  }

  bool ready() const {
    return importFn_ != nullptr && lockPlanesFn_ != nullptr &&
           unlockFn_ != nullptr && releaseFn_ != nullptr;
  }

  int createFromHandle(const AHardwareBuffer_Desc *desc,
                       const native_handle *handle, int32_t method,
                       AHardwareBuffer **out) const {
    return importFn_(desc, handle, method, out);
  }

  int lock(AHardwareBuffer *buf, uint64_t usage, int32_t fence,
           const ARect *rect, void **outVirtualAddress) const {
    if (lockFn_ == nullptr) return -ENOSYS;
    return lockFn_(buf, usage, fence, rect, outVirtualAddress);
  }

  int lockPlanes(AHardwareBuffer *buf, uint64_t usage, int32_t fence,
                 const ARect *rect, AHardwareBuffer_Planes *out) const {
    return lockPlanesFn_(buf, usage, fence, rect, out);
  }

  int unlock(AHardwareBuffer *buf, int32_t *fence) const {
    return unlockFn_(buf, fence);
  }

  void release(AHardwareBuffer *buf) const {
    if (buf != nullptr && releaseFn_ != nullptr) {
      releaseFn_(buf);
    }
  }

 private:
  NativeWindowApi() {
    lib_ = dlopen("libnativewindow.so", RTLD_NOW | RTLD_LOCAL);
    if (!lib_) {
      __android_log_print(ANDROID_LOG_WARN, kVideoTag,
                          "dlopen(libnativewindow.so) failed: %s", dlerror());
      return;
    }
    importFn_ = reinterpret_cast<ImportFn>(
        dlsym(lib_, "AHardwareBuffer_createFromHandle"));
    lockFn_ = reinterpret_cast<decltype(&AHardwareBuffer_lock)>(
        dlsym(lib_, "AHardwareBuffer_lock"));
    lockPlanesFn_ = reinterpret_cast<decltype(&AHardwareBuffer_lockPlanes)>(
        dlsym(lib_, "AHardwareBuffer_lockPlanes"));
    unlockFn_ = reinterpret_cast<decltype(&AHardwareBuffer_unlock)>(
        dlsym(lib_, "AHardwareBuffer_unlock"));
    releaseFn_ = reinterpret_cast<decltype(&AHardwareBuffer_release)>(
        dlsym(lib_, "AHardwareBuffer_release"));
  }

  void *lib_ = nullptr;
  ImportFn importFn_ = nullptr;
  decltype(&AHardwareBuffer_lock) lockFn_ = nullptr;
  decltype(&AHardwareBuffer_lockPlanes) lockPlanesFn_ = nullptr;
  decltype(&AHardwareBuffer_unlock) unlockFn_ = nullptr;
  decltype(&AHardwareBuffer_release) releaseFn_ = nullptr;
};

struct HwBufDeleter {
  void operator()(AHardwareBuffer *buf) const {
    NativeWindowApi::Instance().release(buf);
  }
};
using HwBufPtr = std::unique_ptr<AHardwareBuffer, HwBufDeleter>;

inline uint16_t readBE16(const uint8_t *p) {
  return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
}

inline uint32_t readBE32(const uint8_t *p) {
  return (static_cast<uint32_t>(p[0]) << 24) |
         (static_cast<uint32_t>(p[1]) << 16) |
         (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

inline uint8_t clampByte(int v) {
  if (v < 0) return 0;
  if (v > 255) return 255;
  return static_cast<uint8_t>(v);
}

// HostVideoReceiver 全局单例：从 /dev/hvc12 持续接收宿主推送的 CDCM 视频帧并维护最新帧快照。
class HostVideoReceiver {
 public:
  static HostVideoReceiver &Instance() {
    static HostVideoReceiver inst;
    return inst;
  }

  void ensureStarted() {
    bool expected = false;
    if (started_.compare_exchange_strong(expected, true)) {
      worker_ = std::thread(&HostVideoReceiver::runLoop, this);
      worker_.detach();
    }
  }

  std::shared_ptr<const VideoFrame> latest() const {
    std::lock_guard<std::mutex> lk(mu_);
    return latest_;
  }

  uint64_t receivedCount() const {
    return rxFrames_.load(std::memory_order_relaxed);
  }

 private:
  static int openRawHvc() {
    int fd = ::open(kCameraHvcPath, O_RDWR | O_NOCTTY | O_CLOEXEC);
    if (fd < 0) {
      return -1;
    }
    struct termios tio {};
    if (::tcgetattr(fd, &tio) == 0) {
      ::cfmakeraw(&tio);
      tio.c_cc[VMIN] = 1;
      tio.c_cc[VTIME] = 0;
      ::tcsetattr(fd, TCSANOW, &tio);
    }
    return fd;
  }

  static bool readExact(int fd, uint8_t *dst, size_t len) {
    size_t got = 0;
    while (got < len) {
      struct pollfd pfd {};
      pfd.fd = fd;
      pfd.events = POLLIN;
      int pr = ::poll(&pfd, 1, 1000);
      if (pr == 0) continue;
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

  static bool syncHeader(int fd, uint8_t hdr[kCdcmHeaderSize]) {
    // 逐字节对齐魔数 "CDCM"；若持续 2 秒未收到宿主帧则重发 CDCM_READY 心跳，
    // 避免 Guest 先于 QEMU socket 连接发送的首包握手丢失造成双向死等。
    static constexpr uint8_t kMagic[4] = {'C', 'D', 'C', 'M'};
    static constexpr char kReadyMsg[] = "CDCM_READY\n";
    size_t matched = 0;
    int idlePolls = 0;
    while (matched < 4) {
      struct pollfd pfd {};
      pfd.fd = fd;
      pfd.events = POLLIN;
      int pr = ::poll(&pfd, 1, 1000);
      if (pr == 0) {
        if (++idlePolls >= 2) {
          idlePolls = 0;
          (void)::write(fd, kReadyMsg, sizeof(kReadyMsg) - 1);
        }
        continue;
      }
      if (pr < 0) {
        if (errno == EINTR) continue;
        return false;
      }
      if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
        return false;
      }
      uint8_t b = 0;
      ssize_t n = ::read(fd, &b, 1);
      if (n <= 0) {
        if (n < 0 && (errno == EINTR || errno == EAGAIN)) continue;
        return false;
      }
      idlePolls = 0;
      if (b == kMagic[matched]) {
        hdr[matched++] = b;
      } else if (b == kMagic[0]) {
        hdr[0] = b;
        matched = 1;
      } else {
        matched = 0;
      }
    }
    return readExact(fd, hdr + 4, kCdcmHeaderSize - 4);
  }

  void runLoop() {
    bool warnedOpen = false;
    for (;;) {
      int fd = openRawHvc();
      if (fd < 0) {
        if (!warnedOpen) {
          __android_log_print(ANDROID_LOG_WARN, kVideoTag,
                              "open(%s) failed: %s (%d), will retry",
                              kCameraHvcPath, std::strerror(errno), errno);
          warnedOpen = true;
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));
        continue;
      }
      warnedOpen = false;
      // 向宿主发送 CDCM_READY 握手信号，触发宿主侧开始推流（避免 UEFI/启动期拥塞共享 virtio-serial）
      static constexpr char kReadyMsg[] = "CDCM_READY\n";
      (void)::write(fd, kReadyMsg, sizeof(kReadyMsg) - 1);
      __android_log_print(ANDROID_LOG_INFO, kVideoTag,
                          "connected to host camera channel %s (fd=%d), sent CDCM_READY",
                          kCameraHvcPath, fd);
      uint8_t hdr[kCdcmHeaderSize];
      while (syncHeader(fd, hdr)) {
        const uint8_t ver = hdr[4];
        const uint16_t width = readBE16(hdr + 6);
        const uint16_t height = readBE16(hdr + 8);
        const uint32_t seq = readBE32(hdr + 12);
        const uint32_t i420Len = readBE32(hdr + 16);
        const uint32_t jpegLen = readBE32(hdr + 20);
        const uint32_t wantI420 =
            static_cast<uint32_t>(width) * static_cast<uint32_t>(height) * 3 / 2;
        if (ver != 1 || width == 0 || height == 0 || i420Len != wantI420 ||
            i420Len + jpegLen > kMaxFrameBytes) {
          continue;
        }
        auto frame = std::make_shared<VideoFrame>();
        frame->width = width;
        frame->height = height;
        frame->seq = seq;
        frame->i420.resize(i420Len);
        if (!readExact(fd, frame->i420.data(), i420Len)) {
          break;
        }
        if (jpegLen > 0) {
          frame->jpeg.resize(jpegLen);
          if (!readExact(fd, frame->jpeg.data(), jpegLen)) {
            break;
          }
        }
        {
          std::lock_guard<std::mutex> lk(mu_);
          latest_ = std::move(frame);
        }
        const uint64_t n = rxFrames_.fetch_add(1, std::memory_order_relaxed) + 1;
        if (n == 1 || n % 150 == 0) {
          __android_log_print(ANDROID_LOG_INFO, kVideoTag,
                              "host camera frame rx=#%llu seq=%u %ux%u jpeg=%uB",
                              static_cast<unsigned long long>(n), seq, width,
                              height, jpegLen);
        }
      }
      ::close(fd);
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
  }

  std::atomic<bool> started_{false};
  std::atomic<uint64_t> rxFrames_{0};
  mutable std::mutex mu_;
  std::shared_ptr<const VideoFrame> latest_;
  std::thread worker_;
};

inline HwBufPtr importBuffer(
    const StreamMeta &s,
    const aidl::android::hardware::common::NativeHandle &handle) {
  auto &api = NativeWindowApi::Instance();
  if (!api.ready() || handle.fds.empty() || handle.fds.size() > 64 ||
      handle.ints.size() > 1024) {
    return {};
  }
  std::vector<int> raw(3 + handle.fds.size() + handle.ints.size());
  raw[0] = static_cast<int>(3 * sizeof(int));
  raw[1] = static_cast<int>(handle.fds.size());
  raw[2] = static_cast<int>(handle.ints.size());
  for (size_t i = 0; i < handle.fds.size(); ++i) {
    raw[3 + i] = handle.fds[i].get();
    if (raw[3 + i] < 0) return {};
  }
  std::copy(handle.ints.begin(), handle.ints.end(),
            raw.begin() + 3 + handle.fds.size());

  const bool isBlob = s.overrideFormat == kFmtBlob;
  const uint32_t blobCap =
      s.bufferSize > 0 ? static_cast<uint32_t>(s.bufferSize) : (2U * 1024U * 1024U);
  AHardwareBuffer_Desc desc = {};
  desc.width = isBlob ? blobCap : static_cast<uint32_t>(s.width);
  desc.height = isBlob ? 1 : static_cast<uint32_t>(s.height);
  desc.layers = 1;
  desc.format = static_cast<uint32_t>(s.overrideFormat);
  desc.usage = s.usage | AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN;
  desc.stride = desc.width;
  // 若 handle 为 cros_gralloc_handle（magic=0xABCDDCBA 位于 data[26]，pixel_stride 位于 data[27]），
  // 精确采用其 pixel_stride，避免对齐步长（如 4064 -> 4096）触发 validateBufferSize(-22)。
  if (!isBlob && raw.size() >= 3 + 29 &&
      static_cast<uint32_t>(raw[3 + 26]) == 0xABCDDCBAU) {
    const uint32_t ps = static_cast<uint32_t>(raw[3 + 27]);
    if (ps >= desc.width && ps <= desc.width * 4U) {
      desc.stride = ps;
    }
  }

  AHardwareBuffer *buf = nullptr;
  const auto *nh = reinterpret_cast<const native_handle *>(raw.data());
  // method = 3 -> AHARDWAREBUFFER_CREATE_FROM_HANDLE_METHOD_CLONE
  const int rc = api.createFromHandle(&desc, nh, 3, &buf);
  if (rc != 0 || buf == nullptr) {
    __android_log_print(ANDROID_LOG_WARN, kVideoTag,
                        "AHardwareBuffer_createFromHandle failed: rc=%d %ux%u fmt=0x%x",
                        rc, desc.width, desc.height, desc.format);
    return {};
  }
  return HwBufPtr(buf);
}

inline void sampleFallbackYUV(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                              uint32_t frameNum, uint8_t *Y, uint8_t *U,
                              uint8_t *V) {
  static constexpr uint8_t kBars[8][3] = {
      {235, 128, 128}, {210, 16, 146},  {170, 166, 16}, {145, 54, 34},
      {106, 202, 222}, {81, 90, 240},   {41, 240, 110}, {32, 128, 128},
  };
  const uint32_t shift = (frameNum * 4U) % (w > 0 ? w : 1U);
  const uint32_t idx = (((x + shift) % (w > 0 ? w : 1U)) * 8U) / (w > 0 ? w : 1U);
  const uint32_t b = idx < 8 ? idx : 7;
  *Y = kBars[b][0];
  *U = kBars[b][1];
  *V = kBars[b][2];
  (void)y;
  (void)h;
}

inline void waitAcquireFence(
    const aidl::android::hardware::common::NativeHandle &fence) {
  if (fence.fds.empty()) return;
  const int fd = fence.fds[0].get();
  if (fd < 0) return;
  struct pollfd pfd {};
  pfd.fd = fd;
  pfd.events = POLLIN;
  ::poll(&pfd, 1, 100);
}

inline bool renderFrameToBuffer(
    const StreamMeta &s, AHardwareBuffer *buf,
    const aidl::android::hardware::common::NativeHandle &acquireFence,
    const std::shared_ptr<const VideoFrame> &hostFrame, uint32_t frameNumber) {
  if (buf == nullptr) return false;
  auto &api = NativeWindowApi::Instance();
  if (!api.ready()) return false;

  waitAcquireFence(acquireFence);

  AHardwareBuffer_Planes planes = {};
  const int rc = api.lockPlanes(buf, AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN, -1,
                                nullptr, &planes);
  if (rc != 0) {
    // RAW10(0x25) / RAW_OPAQUE(0x24) / RAW16(0x20) 等非多平面格式不被 NDK lockPlanes 支持，
    // 回退单平面 AHardwareBuffer_lock 并写入安全亮度占位数据，保证整帧回调正常完成。
    void *vaddr = nullptr;
    const int lrc = api.lock(buf, AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN, -1,
                             nullptr, &vaddr);
    if (lrc != 0 || vaddr == nullptr) {
      __android_log_print(ANDROID_LOG_WARN, kVideoTag,
                          "AHardwareBuffer_lockPlanes/lock failed: rc=%d lrc=%d fmt=0x%x",
                          rc, lrc, s.overrideFormat);
      return false;
    }
    const uint8_t fillVal =
        (hostFrame != nullptr && !hostFrame->i420.empty()) ? hostFrame->i420[0] : 128;
    const size_t fillBytes =
        static_cast<size_t>(std::max<int32_t>(0, s.width)) *
        static_cast<size_t>(std::max<int32_t>(0, s.height));
    // 仅在首几帧或低频刷新全帧 RAW 占位，降低大分辨率（4064x3072）纯 CPU 填充开销；
    // RAW10/RAW_OPAQUE 至少占 w*h*5/4 字节且底层 BO 按 R16(2B/px) 分配，填充 w*h 绝对安全不越界。
    if (fillBytes > 0 && (frameNumber < 4 || (frameNumber % 15) == 0)) {
      std::memset(vaddr, fillVal, fillBytes);
    }
    api.unlock(buf, nullptr);
    return true;
  }

  const bool hasHost =
      hostFrame != nullptr && hostFrame->width > 0 && hostFrame->height > 0 &&
      hostFrame->i420.size() >=
          static_cast<size_t>(hostFrame->width) * hostFrame->height * 3 / 2;
  const uint32_t sw = hasHost ? hostFrame->width : 0;
  const uint32_t sh = hasHost ? hostFrame->height : 0;
  const uint8_t *srcY = hasHost ? hostFrame->i420.data() : nullptr;
  const uint8_t *srcU = hasHost ? (srcY + sw * sh) : nullptr;
  const uint8_t *srcV = hasHost ? (srcU + (sw / 2) * (sh / 2)) : nullptr;

  bool ok = false;
  if (s.overrideFormat == kFmtBlob && planes.planeCount >= 1 &&
      planes.planes[0].data != nullptr) {
    const size_t cap = s.bufferSize > 0
                           ? static_cast<size_t>(s.bufferSize)
                           : (2U * 1024U * 1024U);
    if (hasHost && hostFrame->jpeg.size() >= 4 &&
        hostFrame->jpeg.size() + 8 <= cap) {
      auto *dst = static_cast<uint8_t *>(planes.planes[0].data);
      std::memcpy(dst, hostFrame->jpeg.data(), hostFrame->jpeg.size());
      const uint32_t jlen = static_cast<uint32_t>(hostFrame->jpeg.size());
      // Android camera3_jpeg_blob_t（末尾 8 字节）
      const uint8_t tail[8] = {
          0xff, 0x00, 0x00, 0x00,
          static_cast<uint8_t>(jlen & 0xff),
          static_cast<uint8_t>((jlen >> 8) & 0xff),
          static_cast<uint8_t>((jlen >> 16) & 0xff),
          static_cast<uint8_t>((jlen >> 24) & 0xff),
      };
      std::memcpy(dst + cap - sizeof(tail), tail, sizeof(tail));
      ok = true;
    }
  } else if (s.overrideFormat == kFmtRgba8888 && planes.planeCount >= 1 &&
             planes.planes[0].data != nullptr) {
    auto *dstBase = static_cast<uint8_t *>(planes.planes[0].data);
    const uint32_t stride = planes.planes[0].rowStride;
    const uint32_t dw = static_cast<uint32_t>(s.width);
    const uint32_t dh = static_cast<uint32_t>(s.height);
    for (uint32_t y = 0; y < dh; ++y) {
      uint8_t *row = dstBase + static_cast<size_t>(y) * stride;
      const uint32_t sy = hasHost ? (y * sh / dh) : 0;
      const uint32_t suy = sy >> 1;
      for (uint32_t x = 0; x < dw; ++x) {
        uint8_t Y, U, V;
        if (hasHost) {
          const uint32_t sx = x * sw / dw;
          Y = srcY[sy * sw + sx];
          U = srcU[suy * (sw >> 1) + (sx >> 1)];
          V = srcV[suy * (sw >> 1) + (sx >> 1)];
        } else {
          sampleFallbackYUV(x, y, dw, dh, frameNumber, &Y, &U, &V);
        }
        const int c = static_cast<int>(Y) - 16;
        const int d = static_cast<int>(U) - 128;
        const int e = static_cast<int>(V) - 128;
        row[x * 4 + 0] = clampByte((298 * c + 409 * e + 128) >> 8);
        row[x * 4 + 1] = clampByte((298 * c - 100 * d - 208 * e + 128) >> 8);
        row[x * 4 + 2] = clampByte((298 * c + 516 * d + 128) >> 8);
        row[x * 4 + 3] = 255;
      }
    }
    ok = true;
  } else if ((s.overrideFormat == kFmtYuv420888 || s.overrideFormat == kFmtYv12) &&
             planes.planeCount >= 3 && planes.planes[0].data != nullptr &&
             planes.planes[1].data != nullptr &&
             planes.planes[2].data != nullptr) {
    const uint32_t dw = static_cast<uint32_t>(s.width);
    const uint32_t dh = static_cast<uint32_t>(s.height);
    auto *dy = static_cast<uint8_t *>(planes.planes[0].data);
    auto *du = static_cast<uint8_t *>(planes.planes[1].data);
    auto *dv = static_cast<uint8_t *>(planes.planes[2].data);
    const uint32_t yRow = planes.planes[0].rowStride;
    const uint32_t yPix = std::max<uint32_t>(1, planes.planes[0].pixelStride);
    const uint32_t uRow = planes.planes[1].rowStride;
    const uint32_t uPix = std::max<uint32_t>(1, planes.planes[1].pixelStride);
    const uint32_t vRow = planes.planes[2].rowStride;
    const uint32_t vPix = std::max<uint32_t>(1, planes.planes[2].pixelStride);

    for (uint32_t y = 0; y < dh; ++y) {
      uint8_t *rowY = dy + static_cast<size_t>(y) * yRow;
      const uint32_t sy = hasHost ? (y * sh / dh) : 0;
      for (uint32_t x = 0; x < dw; ++x) {
        if (hasHost) {
          rowY[x * yPix] = srcY[sy * sw + (x * sw / dw)];
        } else {
          uint8_t Y, U, V;
          sampleFallbackYUV(x, y, dw, dh, frameNumber, &Y, &U, &V);
          rowY[x * yPix] = Y;
        }
      }
    }
    const uint32_t cw = dw / 2;
    const uint32_t ch = dh / 2;
    for (uint32_t y = 0; y < ch; ++y) {
      uint8_t *rowU = du + static_cast<size_t>(y) * uRow;
      uint8_t *rowV = dv + static_cast<size_t>(y) * vRow;
      const uint32_t suy = hasHost ? (y * (sh / 2) / ch) : 0;
      for (uint32_t x = 0; x < cw; ++x) {
        if (hasHost) {
          const uint32_t sux = x * (sw / 2) / cw;
          rowU[x * uPix] = srcU[suy * (sw / 2) + sux];
          rowV[x * vPix] = srcV[suy * (sw / 2) + sux];
        } else {
          uint8_t Y, U, V;
          sampleFallbackYUV(x * 2, y * 2, dw, dh, frameNumber, &Y, &U, &V);
          rowU[x * uPix] = U;
          rowV[x * vPix] = V;
        }
      }
    }
    ok = true;
  } else if (planes.planeCount >= 1 && planes.planes[0].data != nullptr) {
    // RAW_SENSOR / RAW10 / RAW_PRIVATE 等单平面格式：用 Host 帧亮度或常量填充，避免回调报错
    const uint32_t dh = static_cast<uint32_t>(s.height);
    const uint32_t stride = planes.planes[0].rowStride;
    auto *dst = static_cast<uint8_t *>(planes.planes[0].data);
    if (stride > 0 && dh > 0) {
      std::memset(dst, hasHost ? srcY[0] : 128, static_cast<size_t>(stride) * dh);
    }
    ok = true;
  }

  api.unlock(buf, nullptr);
  return ok;
}

}  // namespace cdh_camera
