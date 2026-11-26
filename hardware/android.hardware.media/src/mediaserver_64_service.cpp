// 64 位轻量级 mediaserver（发布系统服务 "media.player"，即 android.media.IMediaPlayerService）：
//
// 背景：部分 Android 13/14 固件（如 blu/G1130、hmd/ARW 等）在 64 位 system.img 中仅内置了
// 32 位 /system/bin/mediaserver 与 /system/lib/libmediaplayerservice.so，在纯 64 位云机内核
// （ARMv9 / Apple Silicon HVF 无 AArch32 EL0）上无法执行。若仅将其重定向为空桩屏蔽，
// "media.player" 服务未注册会导致相机切换 Video 模式（new MediaRecorder）、MediaProvider
// （MediaMetadataRetriever）与 SystemUI（MediaPlayer）在 getMediaPlayerService() 中无限阻塞。
//
// 本服务通过 cdh_redirect 将 /system/bin/mediaserver 重定向至 /mnt/vendorpatch/vendor/bin/hw/cdh-mediaserver
// （SELinux 标签 u:object_r:mediaserver_exec:s0，逆向 readlink 还原为 /system/bin/mediaserver，
// 运行于正牌 u:r:mediaserver:s0 域与 system linker 命名空间），实现：
//   1. IMediaPlayerService：创建 IMediaRecorder / IMediaPlayer / IMediaMetadataRetriever 及桥接原生 MediaCodecList；
//   2. IMediaRecorder：基于系统 64 位 libmediandk.so（AMediaCodec + InputSurface + AMediaMuxer）
//      向 Camera2 提供真实 IGraphicBufferProducer 编码输入 Surface，支持 Photo <-> Video 零阻塞切换与 H.264 MP4 录像落盘；
//   3. IMediaPlayer / IMediaMetadataRetriever：完整异步状态机与回调（MEDIA_PREPARED / PLAYBACK_COMPLETE），消除全系统死等。

#include <android/binder_ibinder.h>
#include <android/binder_parcel.h>
#include <android/binder_status.h>
#include <android/log.h>
#include <android/native_window.h>

#include <dlfcn.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

extern "C" {
void ABinderProcess_setThreadPoolMaxThreadCount(uint32_t numThreads);
void ABinderProcess_startThreadPool(void);
void ABinderProcess_joinThreadPool(void);
binder_status_t AServiceManager_addService(AIBinder *binder,
                                           const char *instance);
}

namespace {

constexpr const char *kLogTag = "cdh-mediaserver";

constexpr const char *kDescMediaPlayerService =
    "android.media.IMediaPlayerService";
constexpr const char *kDescMediaRecorder = "android.media.IMediaRecorder";
constexpr const char *kDescMediaPlayer = "android.media.IMediaPlayer";
constexpr const char *kDescMediaMetadataRetriever =
    "android.media.IMediaMetadataRetriever";

constexpr char16_t kDescPlayerClient16[] =
    u"android.media.IMediaPlayerClient";
constexpr size_t kDescPlayerClient16Len =
    (sizeof(kDescPlayerClient16) / sizeof(char16_t)) - 1;

[[maybe_unused]] constexpr char16_t kDescRecorderClient16[] =
    u"android.media.IMediaRecorderClient";
[[maybe_unused]] constexpr size_t kDescRecorderClient16Len =
    (sizeof(kDescRecorderClient16) / sizeof(char16_t)) - 1;

// Android Media 通知常量
constexpr int32_t kMediaPrepared = 1;
constexpr int32_t kMediaPlaybackComplete = 2;
constexpr int32_t kMediaSeekComplete = 4;

// NDK MediaCodec 结构体与常量前向声明（运行时通过 libmediandk.so 动态加载）
struct AMediaCodec;
struct AMediaFormat;
struct AMediaMuxer;
struct AMediaCrypto;

struct AMediaCodecBufferInfo {
  int32_t offset;
  int32_t size;
  int64_t presentationTimeUs;
  uint32_t flags;
};

constexpr uint32_t kCodecConfigureFlagEncode = 1;
constexpr uint32_t kCodecBufferFlagCodecConfig = 2;
constexpr uint32_t kCodecBufferFlagEndOfStream = 4;
constexpr ssize_t kCodecInfoTryAgainLater = -1;
constexpr ssize_t kCodecInfoOutputFormatChanged = -2;
constexpr int32_t kColorFormatSurface = 0x7F000789;
constexpr int kMuxerOutputFormatMpeg4 = 0;

// 与 android::hardware::hidl_vec<uint8_t> 在 arm64 下完全同构的 16 字节布局
struct HidlVecU8 {
  uint8_t *mBuffer = nullptr;
  uint32_t mSize = 0;
  bool mOwnsBuffer = false;
  uint8_t mPad[3] = {0, 0, 0};
};
static_assert(sizeof(HidlVecU8) == 16, "hidl_vec<uint8_t> size mismatch");

// 与 android::sp<T> 在 AAPCS64 下完全同构的非平凡析构单指针包装（触发 x8 间接返回槽约定）
struct AndroidSp {
  void *ptr = nullptr;
  AndroidSp() = default;
  AndroidSp(const AndroidSp &) = delete;
  AndroidSp &operator=(const AndroidSp &) = delete;
  ~AndroidSp();
  void reset();
};

struct PlatformBridge {
  void *(*viewParcelMut)(AParcel *) = nullptr;
  const void *(*viewParcelConst)(const AParcel *) = nullptr;

  int (*parcelReadFd)(const void *) = nullptr;
  AndroidSp (*parcelReadStrongBinder)(const void *) = nullptr;
  int32_t (*parcelWriteStrongBinder)(void *, const AndroidSp *) = nullptr;
  int32_t (*parcelWriteCString)(void *, const char *) = nullptr;
  int32_t (*parcelWriteInterfaceToken16)(void *, const char16_t *, size_t) =
      nullptr;
  void (*refBaseIncStrong)(const void *, const void *) = nullptr;
  void (*refBaseDecStrong)(const void *, const void *) = nullptr;
  AndroidSp (*interfaceAsBinder)(const AndroidSp *) = nullptr;

  AndroidSp (*surfaceGetGbp)(const void *) = nullptr;
  AndroidSp (*gbpAsInterface)(const AndroidSp *) = nullptr;
  AndroidSp (*retrieveHalInterface)(const HidlVecU8 *) = nullptr;
  bool (*deleteHalToken)(const HidlVecU8 *) = nullptr;

  AndroidSp (*mediaCodecListGetLocal)() = nullptr;
  void *(*ccodecCreateInputSurface)() = nullptr;
  int32_t (*mediaCodecSetInputSurface)(void *, const void *) = nullptr;

  // libmediandk.so
  AMediaCodec *(*codecCreateByName)(const char *) = nullptr;
  AMediaCodec *(*codecCreateEncoder)(const char *) = nullptr;
  int (*codecConfigure)(AMediaCodec *, const AMediaFormat *, ANativeWindow *,
                        AMediaCrypto *, uint32_t) = nullptr;
  int (*codecCreateInputSurface)(AMediaCodec *, ANativeWindow **) = nullptr;
  int (*codecStart)(AMediaCodec *) = nullptr;
  int (*codecStop)(AMediaCodec *) = nullptr;
  int (*codecDelete)(AMediaCodec *) = nullptr;
  int (*codecSignalEos)(AMediaCodec *) = nullptr;
  ssize_t (*codecDequeueOutput)(AMediaCodec *, AMediaCodecBufferInfo *,
                                int64_t) = nullptr;
  uint8_t *(*codecGetOutputBuffer)(AMediaCodec *, size_t, size_t *) = nullptr;
  AMediaFormat *(*codecGetOutputFormat)(AMediaCodec *) = nullptr;
  int (*codecReleaseOutput)(AMediaCodec *, size_t, bool) = nullptr;

  AMediaFormat *(*formatNew)() = nullptr;
  int (*formatDelete)(AMediaFormat *) = nullptr;
  void (*formatSetString)(AMediaFormat *, const char *, const char *) = nullptr;
  void (*formatSetInt32)(AMediaFormat *, const char *, int32_t) = nullptr;

  AMediaMuxer *(*muxerNew)(int, int) = nullptr;
  int (*muxerDelete)(AMediaMuxer *) = nullptr;
  ssize_t (*muxerAddTrack)(AMediaMuxer *, const AMediaFormat *) = nullptr;
  int (*muxerStart)(AMediaMuxer *) = nullptr;
  int (*muxerStop)(AMediaMuxer *) = nullptr;
  int (*muxerWriteSample)(AMediaMuxer *, size_t, const uint8_t *,
                          const AMediaCodecBufferInfo *) = nullptr;
  int (*muxerSetOrientation)(AMediaMuxer *, int) = nullptr;

  bool hasMediaNdk() const {
    return (codecCreateByName || codecCreateEncoder) && codecConfigure &&
           codecCreateInputSurface &&
           codecStart && codecStop && codecDelete && codecSignalEos &&
           codecDequeueOutput && codecGetOutputBuffer && codecGetOutputFormat &&
           codecReleaseOutput && formatNew && formatDelete && formatSetString &&
           formatSetInt32 && muxerNew && muxerDelete && muxerAddTrack &&
           muxerStart && muxerStop && muxerWriteSample;
  }
};

const PlatformBridge &Bridge() {
  static const PlatformBridge kBridge = []() {
    PlatformBridge b;
    void *hBinderNdk = dlopen("libbinder_ndk.so", RTLD_NOW | RTLD_GLOBAL);
    if (hBinderNdk) {
      b.viewParcelMut = reinterpret_cast<decltype(b.viewParcelMut)>(
          dlsym(hBinderNdk, "_Z26AParcel_viewPlatformParcelP7AParcel"));
      b.viewParcelConst = reinterpret_cast<decltype(b.viewParcelConst)>(
          dlsym(hBinderNdk, "_Z26AParcel_viewPlatformParcelPK7AParcel"));
    }

    void *hBinder = dlopen("libbinder.so", RTLD_NOW | RTLD_GLOBAL);
    if (hBinder) {
      b.parcelReadFd = reinterpret_cast<decltype(b.parcelReadFd)>(
          dlsym(hBinder, "_ZNK7android6Parcel18readFileDescriptorEv"));
      b.parcelReadStrongBinder =
          reinterpret_cast<decltype(b.parcelReadStrongBinder)>(
              dlsym(hBinder, "_ZNK7android6Parcel16readStrongBinderEv"));
      b.parcelWriteStrongBinder =
          reinterpret_cast<decltype(b.parcelWriteStrongBinder)>(dlsym(
              hBinder,
              "_ZN7android6Parcel17writeStrongBinderERKNS_2spINS_7IBinderEEE"));
      b.parcelWriteCString = reinterpret_cast<decltype(b.parcelWriteCString)>(
          dlsym(hBinder, "_ZN7android6Parcel12writeCStringEPKc"));
      b.parcelWriteInterfaceToken16 =
          reinterpret_cast<decltype(b.parcelWriteInterfaceToken16)>(dlsym(
              hBinder, "_ZN7android6Parcel19writeInterfaceTokenEPKDsm"));
      b.interfaceAsBinder = reinterpret_cast<decltype(b.interfaceAsBinder)>(
          dlsym(hBinder, "_ZN7android10IInterface8asBinderERKNS_2spIS0_EE"));
    }

    void *hUtils = dlopen("libutils.so", RTLD_NOW | RTLD_GLOBAL);
    if (hUtils) {
      b.refBaseIncStrong = reinterpret_cast<decltype(b.refBaseIncStrong)>(
          dlsym(hUtils, "_ZNK7android7RefBase9incStrongEPKv"));
      b.refBaseDecStrong = reinterpret_cast<decltype(b.refBaseDecStrong)>(
          dlsym(hUtils, "_ZNK7android7RefBase9decStrongEPKv"));
    }

    void *hGui = dlopen("libgui.so", RTLD_NOW | RTLD_GLOBAL);
    if (hGui) {
      b.surfaceGetGbp = reinterpret_cast<decltype(b.surfaceGetGbp)>(
          dlsym(hGui, "_ZNK7android7Surface25getIGraphicBufferProducerEv"));
      b.gbpAsInterface = reinterpret_cast<decltype(b.gbpAsInterface)>(dlsym(
          hGui,
          "_ZN7android22IGraphicBufferProducer11asInterfaceERKNS_2spINS_7IBinderEEE"));
    }

    void *hToken =
        dlopen("android.hidl.token@1.0-utils.so", RTLD_NOW | RTLD_LOCAL);
    if (hToken == nullptr) {
      hToken = hGui;
    }
    if (hToken) {
      b.retrieveHalInterface =
          reinterpret_cast<decltype(b.retrieveHalInterface)>(dlsym(
              hToken,
              "_ZN7android20retrieveHalInterfaceERKNS_8hardware8hidl_vecIhEE"));
      b.deleteHalToken = reinterpret_cast<decltype(b.deleteHalToken)>(dlsym(
          hToken, "_ZN7android14deleteHalTokenERKNS_8hardware8hidl_vecIhEE"));
    }

    void *hStagefright = dlopen("libstagefright.so", RTLD_NOW | RTLD_LOCAL);
    if (hStagefright) {
      b.mediaCodecListGetLocal =
          reinterpret_cast<decltype(b.mediaCodecListGetLocal)>(dlsym(
              hStagefright, "_ZN7android14MediaCodecList16getLocalInstanceEv"));
      b.ccodecCreateInputSurface =
          reinterpret_cast<decltype(b.ccodecCreateInputSurface)>(
              dlsym(hStagefright, "_ZN7android6CCodec18CreateInputSurfaceEv"));
      b.mediaCodecSetInputSurface =
          reinterpret_cast<decltype(b.mediaCodecSetInputSurface)>(dlsym(
              hStagefright,
              "_ZN7android10MediaCodec15setInputSurfaceERKNS_2spINS_17PersistentSurfaceEEE"));
    }

    void *hMediaNdk = dlopen("libmediandk.so", RTLD_NOW | RTLD_LOCAL);
    if (hMediaNdk) {
      b.codecCreateByName = reinterpret_cast<decltype(b.codecCreateByName)>(
          dlsym(hMediaNdk, "AMediaCodec_createCodecByName"));
      b.codecCreateEncoder = reinterpret_cast<decltype(b.codecCreateEncoder)>(
          dlsym(hMediaNdk, "AMediaCodec_createEncoderByType"));
      b.codecConfigure = reinterpret_cast<decltype(b.codecConfigure)>(
          dlsym(hMediaNdk, "AMediaCodec_configure"));
      b.codecCreateInputSurface =
          reinterpret_cast<decltype(b.codecCreateInputSurface)>(
              dlsym(hMediaNdk, "AMediaCodec_createInputSurface"));
      b.codecStart = reinterpret_cast<decltype(b.codecStart)>(
          dlsym(hMediaNdk, "AMediaCodec_start"));
      b.codecStop = reinterpret_cast<decltype(b.codecStop)>(
          dlsym(hMediaNdk, "AMediaCodec_stop"));
      b.codecDelete = reinterpret_cast<decltype(b.codecDelete)>(
          dlsym(hMediaNdk, "AMediaCodec_delete"));
      b.codecSignalEos = reinterpret_cast<decltype(b.codecSignalEos)>(
          dlsym(hMediaNdk, "AMediaCodec_signalEndOfInputStream"));
      b.codecDequeueOutput = reinterpret_cast<decltype(b.codecDequeueOutput)>(
          dlsym(hMediaNdk, "AMediaCodec_dequeueOutputBuffer"));
      b.codecGetOutputBuffer =
          reinterpret_cast<decltype(b.codecGetOutputBuffer)>(
              dlsym(hMediaNdk, "AMediaCodec_getOutputBuffer"));
      b.codecGetOutputFormat =
          reinterpret_cast<decltype(b.codecGetOutputFormat)>(
              dlsym(hMediaNdk, "AMediaCodec_getOutputFormat"));
      b.codecReleaseOutput = reinterpret_cast<decltype(b.codecReleaseOutput)>(
          dlsym(hMediaNdk, "AMediaCodec_releaseOutputBuffer"));

      b.formatNew = reinterpret_cast<decltype(b.formatNew)>(
          dlsym(hMediaNdk, "AMediaFormat_new"));
      b.formatDelete = reinterpret_cast<decltype(b.formatDelete)>(
          dlsym(hMediaNdk, "AMediaFormat_delete"));
      b.formatSetString = reinterpret_cast<decltype(b.formatSetString)>(
          dlsym(hMediaNdk, "AMediaFormat_setString"));
      b.formatSetInt32 = reinterpret_cast<decltype(b.formatSetInt32)>(
          dlsym(hMediaNdk, "AMediaFormat_setInt32"));

      b.muxerNew = reinterpret_cast<decltype(b.muxerNew)>(
          dlsym(hMediaNdk, "AMediaMuxer_new"));
      b.muxerDelete = reinterpret_cast<decltype(b.muxerDelete)>(
          dlsym(hMediaNdk, "AMediaMuxer_delete"));
      b.muxerAddTrack = reinterpret_cast<decltype(b.muxerAddTrack)>(
          dlsym(hMediaNdk, "AMediaMuxer_addTrack"));
      b.muxerStart = reinterpret_cast<decltype(b.muxerStart)>(
          dlsym(hMediaNdk, "AMediaMuxer_start"));
      b.muxerStop = reinterpret_cast<decltype(b.muxerStop)>(
          dlsym(hMediaNdk, "AMediaMuxer_stop"));
      b.muxerWriteSample = reinterpret_cast<decltype(b.muxerWriteSample)>(
          dlsym(hMediaNdk, "AMediaMuxer_writeSampleData"));
      b.muxerSetOrientation =
          reinterpret_cast<decltype(b.muxerSetOrientation)>(
              dlsym(hMediaNdk, "AMediaMuxer_setOrientationHint"));
    }

    __android_log_print(
        ANDROID_LOG_INFO, kLogTag,
        "bridge ready: parcelFd=%d writeSb=%d gbp=%d mediaNdk=%d mcl=%d",
        b.parcelReadFd != nullptr, b.parcelWriteStrongBinder != nullptr,
        b.surfaceGetGbp != nullptr, b.hasMediaNdk(),
        b.mediaCodecListGetLocal != nullptr);
    return b;
  }();
  return kBridge;
}

void AndroidSp::reset() {
  if (ptr != nullptr) {
    void *obj = ptr;
    ptr = nullptr;
    const auto &b = Bridge();
    if (b.refBaseDecStrong != nullptr) {
      // Itanium C++ ABI：virtual RefBase 基类偏移存放在 vtable[-3]（即 *(vptr - 0x18)）
      auto *vptr = *reinterpret_cast<intptr_t *const *>(obj);
      if (vptr != nullptr) {
        const intptr_t vbaseOffset = *(vptr - 3);
        const void *refBase =
            reinterpret_cast<const char *>(obj) + vbaseOffset;
        b.refBaseDecStrong(refBase, this);
      }
    }
  }
}

AndroidSp::~AndroidSp() { reset(); }

int ReadRawFdFromParcel(const AParcel *in) {
  const auto &b = Bridge();
  if (in == nullptr || b.viewParcelConst == nullptr ||
      b.parcelReadFd == nullptr) {
    return -1;
  }
  const void *platParcel = b.viewParcelConst(in);
  return platParcel != nullptr ? b.parcelReadFd(platParcel) : -1;
}

bool WriteNullStrongBinder(AParcel *out) {
  return AParcel_writeStrongBinder(out, nullptr) == STATUS_OK;
}

AIBinder_Class *gPlayerServiceClass = nullptr;
AIBinder_Class *gRecorderClass = nullptr;
AIBinder_Class *gPlayerClass = nullptr;
AIBinder_Class *gRetrieverClass = nullptr;
AIBinder_Class *gPlayerClientProxyClass = nullptr;
AIBinder_Class *gRecorderClientProxyClass = nullptr;

void SendLegacyClientNotify(AIBinder *client, AIBinder_Class *proxyCls,
                            const char16_t *desc16, size_t descLen, int32_t msg,
                            int32_t ext1, int32_t ext2) {
  if (client == nullptr || proxyCls == nullptr) {
    return;
  }
  const auto &b = Bridge();
  if (b.viewParcelMut == nullptr || b.parcelWriteInterfaceToken16 == nullptr) {
    return;
  }
  AIBinder_associateClass(client, proxyCls);
  AParcel *in = nullptr;
  if (AIBinder_prepareTransaction(client, &in) != STATUS_OK || in == nullptr) {
    return;
  }
  void *plat = b.viewParcelMut(in);
  b.parcelWriteInterfaceToken16(plat, desc16, descLen);
  AParcel_writeInt32(in, msg);
  AParcel_writeInt32(in, ext1);
  AParcel_writeInt32(in, ext2);
  AParcel *out = nullptr;
  (void)AIBinder_transact(client, FIRST_CALL_TRANSACTION /* NOTIFY = 1 */, &in,
                          &out, FLAG_ONEWAY);
  if (out != nullptr) {
    AParcel_delete(out);
  }
}

// ============================================================================
// 1. IMediaRecorder 实现（支撑相机 Video 预览切换与真实 H.264 MP4 录像落盘）
// ============================================================================

enum MediaRecorderTx : transaction_code_t {
  MR_RELEASE = FIRST_CALL_TRANSACTION, // 1
  MR_INIT,                             // 2
  MR_CLOSE,                            // 3
  MR_SET_INPUT_SURFACE,                // 4
  MR_QUERY_SURFACE_MEDIASOURCE,        // 5
  MR_RESET,                            // 6
  MR_STOP,                             // 7
  MR_START,                            // 8
  MR_PREPARE,                          // 9
  MR_GET_MAX_AMPLITUDE,                // 10
  MR_SET_VIDEO_SOURCE,                 // 11
  MR_SET_AUDIO_SOURCE,                 // 12
  MR_SET_OUTPUT_FORMAT,                // 13
  MR_SET_VIDEO_ENCODER,                // 14
  MR_SET_AUDIO_ENCODER,                // 15
  MR_SET_OUTPUT_FILE_FD,               // 16
  MR_SET_NEXT_OUTPUT_FILE_FD,          // 17
  MR_SET_VIDEO_SIZE,                   // 18
  MR_SET_VIDEO_FRAMERATE,              // 19
  MR_SET_PARAMETERS,                   // 20
  MR_SET_PREVIEW_SURFACE,              // 21
  MR_SET_CAMERA,                       // 22
  MR_SET_LISTENER,                     // 23
  MR_SET_CLIENT_NAME,                  // 24
  MR_PAUSE,                            // 25
  MR_RESUME,                           // 26
  MR_GET_METRICS,                      // 27
  MR_SET_INPUT_DEVICE,                 // 28
  MR_GET_ROUTED_DEVICE_ID,             // 29
  MR_ENABLE_AUDIO_DEVICE_CALLBACK,     // 30
  MR_GET_ACTIVE_MICROPHONES,           // 31
  MR_GET_PORT_ID,                      // 32
  MR_GET_RTP_DATA_USAGE,               // 33
  MR_SET_PREFERRED_MIC_DIRECTION,      // 34
  MR_SET_PREFERRED_MIC_FIELD_DIM,      // 35
  MR_SET_PRIVACY_SENSITIVE,            // 36
  MR_GET_PRIVACY_SENSITIVE,            // 37
};

class CDHMediaRecorder {
public:
  ~CDHMediaRecorder() {
    ResetInternal();
    std::lock_guard<std::mutex> lk(mu_);
    if (listener_ != nullptr) {
      AIBinder_decStrong(listener_);
      listener_ = nullptr;
    }
  }

  void SetListener(AIBinder *listener) {
    std::lock_guard<std::mutex> lk(mu_);
    if (listener_ != nullptr) {
      AIBinder_decStrong(listener_);
    }
    listener_ = listener;
  }

  void SetOutputFd(int rawFd) {
    std::lock_guard<std::mutex> lk(mu_);
    if (outputFd_ >= 0) {
      ::close(outputFd_);
      outputFd_ = -1;
    }
    if (rawFd >= 0) {
      outputFd_ = ::dup(rawFd);
    }
  }

  void SetParametersFromParcel(const AParcel *in) {
    const auto &b = Bridge();
    if (b.viewParcelConst == nullptr) {
      return;
    }
    int32_t len = 0;
    if (AParcel_readInt32(in, &len) != STATUS_OK || len <= 0 || len > 4096) {
      return;
    }
    // String8 在 Parcel 中紧跟 int32 长度存放以 '\0' 结尾的 UTF-8 字节序列
    std::vector<char> buf(static_cast<size_t>(len) + 4, 0);
    // 逐 4 字节读取避免依赖额外符号
    const size_t words = (static_cast<size_t>(len) + 1 + 3) / 4;
    for (size_t i = 0; i < words; ++i) {
      int32_t w = 0;
      if (AParcel_readInt32(in, &w) != STATUS_OK) {
        break;
      }
      std::memcpy(buf.data() + i * 4, &w, 4);
    }
    std::string params(buf.data(), static_cast<size_t>(len));
    std::lock_guard<std::mutex> lk(mu_);
    ParseParamInt(params, "video-param-encoding-bitrate=", &bitRate_);
    ParseParamInt(params, "video-param-rotation-angle-degrees=",
                  &orientationDegrees_);
  }

  int32_t SetInputSurfaceFromParcel(const AParcel *in) {
    const auto &b = Bridge();
    if (in == nullptr || b.viewParcelConst == nullptr ||
        b.parcelReadStrongBinder == nullptr || b.gbpAsInterface == nullptr ||
        b.ccodecCreateInputSurface == nullptr ||
        b.refBaseIncStrong == nullptr || b.refBaseDecStrong == nullptr) {
      return 0;
    }
    const void *platParcel = b.viewParcelConst(in);
    if (platParcel == nullptr) {
      return 0;
    }
    AndroidSp ibinder = b.parcelReadStrongBinder(platParcel);
    int32_t haveHidlTarget = 0;
    (void)AParcel_readInt32(in, &haveHidlTarget);

    AndroidSp gbp =
        (ibinder.ptr != nullptr) ? b.gbpAsInterface(&ibinder) : AndroidSp{};

    AndroidSp hidlTarget = [&]() -> AndroidSp {
      if (haveHidlTarget == 0) {
        return AndroidSp{};
      }
      int32_t tokenLen = 0;
      if (AParcel_readInt32(in, &tokenLen) != STATUS_OK || tokenLen <= 0 ||
          tokenLen > 4096) {
        return AndroidSp{};
      }
      const size_t padded = (static_cast<size_t>(tokenLen) + 3u) & ~3u;
      std::vector<uint8_t> tokenBuf(padded, 0);
      const size_t words = padded / 4u;
      for (size_t i = 0; i < words; ++i) {
        int32_t w = 0;
        if (AParcel_readInt32(in, &w) != STATUS_OK) {
          break;
        }
        std::memcpy(tokenBuf.data() + i * 4u, &w, 4u);
      }
      HidlVecU8 vec;
      vec.mBuffer = tokenBuf.data();
      vec.mSize = static_cast<uint32_t>(tokenLen);
      vec.mOwnsBuffer = false;
      struct TokenCleanup {
        const PlatformBridge &bridge;
        const HidlVecU8 *vecPtr;
        ~TokenCleanup() {
          if (bridge.deleteHalToken != nullptr) {
            (void)bridge.deleteHalToken(vecPtr);
          }
        }
      } cleanup{b, &vec};
      if (b.retrieveHalInterface != nullptr) {
        return b.retrieveHalInterface(&vec);
      }
      return AndroidSp{};
    }();

    if (gbp.ptr == nullptr && hidlTarget.ptr == nullptr) {
      return 0;
    }

    // 借由 libstagefright.so 原生 CCodec::CreateInputSurface() 分配具备合法 CFI vtable 的
    // PersistentSurface（非虚继承 RefBase，+0x10 为 mBufferProducer，+0x18 为 mHidlTarget），
    // 再通过指针交换移交客户端传入的 PersistentSurface 成员引用。
    void *rawPs = b.ccodecCreateInputSurface();
    if (rawPs == nullptr) {
      __android_log_print(ANDROID_LOG_WARN, kLogTag,
                          "CCodec::CreateInputSurface returned null");
      return 0;
    }
    b.refBaseIncStrong(rawPs, rawPs);
    std::swap(
        *reinterpret_cast<void **>(reinterpret_cast<char *>(rawPs) + 0x10),
        gbp.ptr);
    std::swap(
        *reinterpret_cast<void **>(reinterpret_cast<char *>(rawPs) + 0x18),
        hidlTarget.ptr);

    std::lock_guard<std::mutex> lk(mu_);
    ReleasePersistentSurfaceLocked();
    persistentSurface_ = rawPs;
    __android_log_print(ANDROID_LOG_INFO, kLogTag,
                        "recorder bound persistent input surface ps=%p", rawPs);
    return 0;
  }

  int32_t Prepare() {
    std::lock_guard<std::mutex> lk(mu_);
    CleanupCodecLocked();
    if (videoSource_ < 0) {
      prepared_ = true;
      return 0;
    }
    const auto &b = Bridge();
    if (!b.hasMediaNdk()) {
      __android_log_print(ANDROID_LOG_WARN, kLogTag,
                          "libmediandk unavailable, prepare fallback OK");
      prepared_ = true;
      return 0;
    }
    // 优先显式选用 media.swcodec 软件编码器（c2.android.*.encoder），
    // 避免命中仅支持线性缓冲、不支持 C2InputSurface 的 c2.v4l2.*.encoder。
    const char *mime = "video/avc";
    const char *swCodecName = "c2.android.avc.encoder";
    switch (videoEncoder_) {
    case 1: // H263
      mime = "video/3gpp";
      swCodecName = "c2.android.h263.encoder";
      break;
    case 3: // MPEG_4_SP
      mime = "video/mp4v-es";
      swCodecName = "c2.android.mpeg4.encoder";
      break;
    case 4: // VP8
      mime = "video/x-vnd.on2.vp8";
      swCodecName = "c2.android.vp8.encoder";
      break;
    case 5: // HEVC
      mime = "video/hevc";
      swCodecName = "c2.android.hevc.encoder";
      break;
    default:
      mime = "video/avc";
      swCodecName = "c2.android.avc.encoder";
      break;
    }

    AMediaCodec *codec = nullptr;
    if (b.codecCreateByName != nullptr) {
      codec = b.codecCreateByName(swCodecName);
      if (codec == nullptr &&
          std::strcmp(swCodecName, "c2.android.avc.encoder") != 0) {
        mime = "video/avc";
        codec = b.codecCreateByName("c2.android.avc.encoder");
      }
    }
    if (codec == nullptr && b.codecCreateEncoder != nullptr) {
      codec = b.codecCreateEncoder(mime);
      if (codec == nullptr && std::strcmp(mime, "video/avc") != 0) {
        mime = "video/avc";
        codec = b.codecCreateEncoder(mime);
      }
    }
    if (codec == nullptr) {
      __android_log_print(ANDROID_LOG_WARN, kLogTag,
                          "createEncoder(%s) returned null", mime);
      prepared_ = true;
      return 0;
    }

    const int32_t w = ((width_ > 0 ? width_ : 1280) + 1) & ~1;
    const int32_t h = ((height_ > 0 ? height_ : 720) + 1) & ~1;
    const int32_t fps = frameRate_ > 0 ? frameRate_ : 30;
    const int32_t br = bitRate_ > 0 ? bitRate_ : 4000000;

    AMediaFormat *fmt = b.formatNew();
    b.formatSetString(fmt, "mime", mime);
    b.formatSetInt32(fmt, "width", w);
    b.formatSetInt32(fmt, "height", h);
    b.formatSetInt32(fmt, "color-format", kColorFormatSurface);
    b.formatSetInt32(fmt, "color-standard", 4 /* BT601_625 */);
    b.formatSetInt32(fmt, "color-range", 2 /* LIMITED */);
    b.formatSetInt32(fmt, "color-transfer", 3 /* SDR_VIDEO */);
    b.formatSetInt32(fmt, "bitrate", br);
    b.formatSetInt32(fmt, "frame-rate", fps);
    b.formatSetInt32(fmt, "i-frame-interval", 1);

    int rc = b.codecConfigure(codec, fmt, nullptr, nullptr,
                              kCodecConfigureFlagEncode);
    b.formatDelete(fmt);
    if (rc != 0) {
      __android_log_print(ANDROID_LOG_WARN, kLogTag,
                          "AMediaCodec_configure(%dx%d@%d) failed: %d", w, h,
                          fps, rc);
      b.codecDelete(codec);
      prepared_ = true;
      return 0;
    }

    if (persistentSurface_ != nullptr &&
        b.mediaCodecSetInputSurface != nullptr) {
      void *nativeMediaCodec = *reinterpret_cast<void **>(codec);
      if (nativeMediaCodec != nullptr) {
        const int32_t setRc =
            b.mediaCodecSetInputSurface(nativeMediaCodec, &persistentSurface_);
        if (setRc == 0) {
          codec_ = codec;
          prepared_ = true;
          __android_log_print(
              ANDROID_LOG_INFO, kLogTag,
              "recorder prepared (persistent): %s %dx%d@%dfps br=%d", mime, w,
              h, fps, br);
          return 0;
        }
        __android_log_print(ANDROID_LOG_WARN, kLogTag,
                            "MediaCodec::setInputSurface failed: %d, fallback",
                            setRc);
      }
    }

    ANativeWindow *anw = nullptr;
    rc = b.codecCreateInputSurface(codec, &anw);
    if (rc != 0 || anw == nullptr) {
      __android_log_print(ANDROID_LOG_WARN, kLogTag,
                          "AMediaCodec_createInputSurface failed: %d", rc);
      b.codecDelete(codec);
      prepared_ = true;
      return 0;
    }

    codec_ = codec;
    inputWindow_ = anw;
    prepared_ = true;
    __android_log_print(ANDROID_LOG_INFO, kLogTag,
                        "recorder prepared: %s %dx%d@%dfps br=%d anw=%p", mime,
                        w, h, fps, br, anw);
    return 0;
  }

  void WriteInputSurfaceToParcel(AParcel *out) {
    std::lock_guard<std::mutex> lk(mu_);
    const auto &b = Bridge();
    if (b.interfaceAsBinder == nullptr ||
        b.parcelWriteStrongBinder == nullptr || b.viewParcelMut == nullptr) {
      AParcel_writeInt32(out, 1); // returnedNull = 1
      return;
    }
    if (inputWindow_ != nullptr && b.surfaceGetGbp != nullptr) {
      // arm64 Android Surface 继承 ANativeObjectBase<ANativeWindow, Surface, RefBase>，
      // ANativeWindow 子对象固定位于 Surface 起始地址 + 0x10 处。
      const void *surfacePtr =
          reinterpret_cast<const char *>(inputWindow_) - 0x10;
      AndroidSp gbp = b.surfaceGetGbp(surfacePtr);
      if (gbp.ptr != nullptr) {
        AndroidSp ibinder = b.interfaceAsBinder(&gbp);
        if (ibinder.ptr != nullptr) {
          AParcel_writeInt32(out, 0);
          b.parcelWriteStrongBinder(b.viewParcelMut(out), &ibinder);
          return;
        }
      }
    }
    if (persistentSurface_ != nullptr) {
      void *rawGbp = *reinterpret_cast<void *const *>(
          reinterpret_cast<const char *>(persistentSurface_) + 0x10);
      if (rawGbp != nullptr) {
        // 借用只读 sp 视图调用 IInterface::asBinder(const sp<IInterface>&)，不转移所有权
        AndroidSp borrowedGbp;
        borrowedGbp.ptr = rawGbp;
        AndroidSp ibinder = b.interfaceAsBinder(&borrowedGbp);
        borrowedGbp.ptr = nullptr;
        if (ibinder.ptr != nullptr) {
          AParcel_writeInt32(out, 0);
          b.parcelWriteStrongBinder(b.viewParcelMut(out), &ibinder);
          return;
        }
      }
    }
    AParcel_writeInt32(out, 1); // returnedNull = 1
  }

  int32_t Start() {
    std::lock_guard<std::mutex> lk(mu_);
    if (started_) {
      return 0;
    }
    const auto &b = Bridge();
    if (codec_ != nullptr && b.hasMediaNdk()) {
      if (outputFd_ >= 0) {
        muxer_ = b.muxerNew(outputFd_, kMuxerOutputFormatMpeg4);
        if (muxer_ != nullptr && orientationDegrees_ != 0 &&
            b.muxerSetOrientation != nullptr) {
          b.muxerSetOrientation(muxer_, orientationDegrees_);
        }
      }
      int rc = b.codecStart(codec_);
      if (rc == 0) {
        codecStarted_ = true;
        stopRequested_.store(false);
        muxerStarted_ = false;
        videoTrackIndex_ = -1;
        drainThread_ = std::thread([this]() { DrainLoop(); });
      } else {
        __android_log_print(ANDROID_LOG_WARN, kLogTag,
                            "AMediaCodec_start failed: %d", rc);
      }
    }
    started_ = true;
    paused_ = false;
    __android_log_print(ANDROID_LOG_INFO, kLogTag,
                        "recorder started (codecStarted=%d muxer=%p)",
                        codecStarted_, muxer_);
    return 0;
  }

  int32_t Stop() {
    std::thread joiner;
    {
      std::lock_guard<std::mutex> lk(mu_);
      if (!started_) {
        return 0;
      }
      started_ = false;
      paused_ = false;
      stopRequested_.store(true);
      if (codec_ != nullptr && codecStarted_) {
        Bridge().codecSignalEos(codec_);
      }
      if (drainThread_.joinable()) {
        joiner = std::move(drainThread_);
      }
    }
    if (joiner.joinable()) {
      joiner.join();
    }
    std::lock_guard<std::mutex> lk(mu_);
    FinishMuxerAndCodecLocked();
    __android_log_print(ANDROID_LOG_INFO, kLogTag, "recorder stopped cleanly");
    return 0;
  }

  int32_t ResetInternal() {
    Stop();
    std::lock_guard<std::mutex> lk(mu_);
    CleanupCodecLocked();
    ReleasePersistentSurfaceLocked();
    if (outputFd_ >= 0) {
      ::close(outputFd_);
      outputFd_ = -1;
    }
    videoSource_ = -1;
    audioSource_ = -1;
    outputFormat_ = 0;
    videoEncoder_ = 0;
    audioEncoder_ = 0;
    width_ = 1280;
    height_ = 720;
    frameRate_ = 30;
    bitRate_ = 4000000;
    orientationDegrees_ = 0;
    prepared_ = false;
    privacySensitive_ = false;
    return 0;
  }

  int32_t videoSource_ = -1;
  int32_t audioSource_ = -1;
  int32_t outputFormat_ = 0;
  int32_t videoEncoder_ = 0;
  int32_t audioEncoder_ = 0;
  int32_t width_ = 1280;
  int32_t height_ = 720;
  int32_t frameRate_ = 30;
  int32_t bitRate_ = 4000000;
  int32_t orientationDegrees_ = 0;
  bool privacySensitive_ = false;
  bool paused_ = false;

private:
  static void ParseParamInt(const std::string &params, const char *key,
                            int32_t *out) {
    const size_t pos = params.find(key);
    if (pos == std::string::npos) {
      return;
    }
    const char *valStart = params.c_str() + pos + std::strlen(key);
    char *endPtr = nullptr;
    const long v = std::strtol(valStart, &endPtr, 10);
    if (endPtr != valStart) {
      *out = static_cast<int32_t>(v);
    }
  }

  void DrainLoop() {
    const auto &b = Bridge();
    auto stopDeadline = std::chrono::steady_clock::time_point::max();
    bool deadlineSet = false;

    while (true) {
      if (stopRequested_.load()) {
        if (!deadlineSet) {
          stopDeadline =
              std::chrono::steady_clock::now() + std::chrono::milliseconds(800);
          deadlineSet = true;
        } else if (std::chrono::steady_clock::now() >= stopDeadline) {
          break;
        }
      }

      AMediaCodecBufferInfo info = {};
      const ssize_t idx = b.codecDequeueOutput(codec_, &info, 20000 /* 20ms */);
      if (idx == kCodecInfoTryAgainLater) {
        continue;
      }
      if (idx == kCodecInfoOutputFormatChanged) {
        AMediaFormat *outFmt = b.codecGetOutputFormat(codec_);
        if (outFmt != nullptr) {
          if (muxer_ != nullptr && !muxerStarted_) {
            const ssize_t track = b.muxerAddTrack(muxer_, outFmt);
            if (track >= 0 && b.muxerStart(muxer_) == 0) {
              videoTrackIndex_ = static_cast<int>(track);
              muxerStarted_ = true;
            }
          }
          b.formatDelete(outFmt);
        }
        continue;
      }
      if (idx < 0) {
        break;
      }

      if ((info.flags & kCodecBufferFlagCodecConfig) != 0) {
        info.size = 0;
      }
      if (info.size > 0 && muxer_ != nullptr && muxerStarted_ &&
          videoTrackIndex_ >= 0 && !paused_) {
        size_t bufSize = 0;
        uint8_t *buf =
            b.codecGetOutputBuffer(codec_, static_cast<size_t>(idx), &bufSize);
        if (buf != nullptr &&
            static_cast<size_t>(info.offset + info.size) <= bufSize) {
          b.muxerWriteSample(muxer_, static_cast<size_t>(videoTrackIndex_), buf,
                             &info);
        }
      }
      b.codecReleaseOutput(codec_, static_cast<size_t>(idx), false);
      if ((info.flags & kCodecBufferFlagEndOfStream) != 0) {
        break;
      }
    }
  }

  void FinishMuxerAndCodecLocked() {
    const auto &b = Bridge();
    if (codec_ != nullptr) {
      if (codecStarted_ && b.codecStop != nullptr) {
        b.codecStop(codec_);
        codecStarted_ = false;
      }
      if (b.codecDelete != nullptr) {
        b.codecDelete(codec_);
      }
      codec_ = nullptr;
    }
    if (inputWindow_ != nullptr) {
      ANativeWindow_release(inputWindow_);
      inputWindow_ = nullptr;
    }
    if (muxer_ != nullptr) {
      if (muxerStarted_ && b.muxerStop != nullptr) {
        b.muxerStop(muxer_);
        muxerStarted_ = false;
      }
      if (b.muxerDelete != nullptr) {
        b.muxerDelete(muxer_);
      }
      muxer_ = nullptr;
    }
    videoTrackIndex_ = -1;
  }

  void CleanupCodecLocked() { FinishMuxerAndCodecLocked(); }

  void ReleasePersistentSurfaceLocked() {
    if (persistentSurface_ != nullptr) {
      void *ps = persistentSurface_;
      persistentSurface_ = nullptr;
      const auto &b = Bridge();
      if (b.refBaseDecStrong != nullptr) {
        b.refBaseDecStrong(ps, ps);
      }
    }
  }

  std::mutex mu_;
  AIBinder *listener_ = nullptr;
  int outputFd_ = -1;
  AMediaCodec *codec_ = nullptr;
  ANativeWindow *inputWindow_ = nullptr;
  void *persistentSurface_ = nullptr;
  AMediaMuxer *muxer_ = nullptr;
  int videoTrackIndex_ = -1;
  bool muxerStarted_ = false;
  bool codecStarted_ = false;
  bool prepared_ = false;
  bool started_ = false;
  std::atomic<bool> stopRequested_{false};
  std::thread drainThread_;
};

void *RecorderOnCreate(void *args) { return args; }
void RecorderOnDestroy(void *userData) {
  delete static_cast<CDHMediaRecorder *>(userData);
}

binder_status_t RecorderOnTransact(AIBinder *binder, transaction_code_t code,
                                   const AParcel *in, AParcel *out) {
  auto *rec = static_cast<CDHMediaRecorder *>(AIBinder_getUserData(binder));
  if (rec == nullptr) {
    return STATUS_UNEXPECTED_NULL;
  }

  switch (code) {
  case MR_RELEASE:
  case MR_CLOSE:
  case MR_RESET:
    AParcel_writeInt32(out, rec->ResetInternal());
    return STATUS_OK;
  case MR_INIT:
    AParcel_writeInt32(out, 0);
    return STATUS_OK;
  case MR_SET_INPUT_SURFACE:
    AParcel_writeInt32(out, rec->SetInputSurfaceFromParcel(in));
    return STATUS_OK;
  case MR_QUERY_SURFACE_MEDIASOURCE:
    rec->WriteInputSurfaceToParcel(out);
    return STATUS_OK;
  case MR_STOP:
    AParcel_writeInt32(out, rec->Stop());
    return STATUS_OK;
  case MR_START:
    AParcel_writeInt32(out, rec->Start());
    return STATUS_OK;
  case MR_PREPARE:
    AParcel_writeInt32(out, rec->Prepare());
    return STATUS_OK;
  case MR_PAUSE:
    rec->paused_ = true;
    AParcel_writeInt32(out, 0);
    return STATUS_OK;
  case MR_RESUME:
    rec->paused_ = false;
    AParcel_writeInt32(out, 0);
    return STATUS_OK;
  case MR_GET_MAX_AMPLITUDE:
    AParcel_writeInt32(out, 0); // max
    AParcel_writeInt32(out, 0); // status = NO_ERROR
    return STATUS_OK;
  case MR_SET_VIDEO_SOURCE:
    AParcel_readInt32(in, &rec->videoSource_);
    AParcel_writeInt32(out, 0);
    return STATUS_OK;
  case MR_SET_AUDIO_SOURCE:
    AParcel_readInt32(in, &rec->audioSource_);
    AParcel_writeInt32(out, 0);
    return STATUS_OK;
  case MR_SET_OUTPUT_FORMAT:
    AParcel_readInt32(in, &rec->outputFormat_);
    AParcel_writeInt32(out, 0);
    return STATUS_OK;
  case MR_SET_VIDEO_ENCODER:
    AParcel_readInt32(in, &rec->videoEncoder_);
    AParcel_writeInt32(out, 0);
    return STATUS_OK;
  case MR_SET_AUDIO_ENCODER:
    AParcel_readInt32(in, &rec->audioEncoder_);
    AParcel_writeInt32(out, 0);
    return STATUS_OK;
  case MR_SET_OUTPUT_FILE_FD: {
    const int fd = ReadRawFdFromParcel(in);
    rec->SetOutputFd(fd);
    AParcel_writeInt32(out, 0);
    return STATUS_OK;
  }
  case MR_SET_NEXT_OUTPUT_FILE_FD: {
    (void)ReadRawFdFromParcel(in);
    AParcel_writeInt32(out, 0);
    return STATUS_OK;
  }
  case MR_SET_VIDEO_SIZE:
    AParcel_readInt32(in, &rec->width_);
    AParcel_readInt32(in, &rec->height_);
    AParcel_writeInt32(out, 0);
    return STATUS_OK;
  case MR_SET_VIDEO_FRAMERATE:
    AParcel_readInt32(in, &rec->frameRate_);
    AParcel_writeInt32(out, 0);
    return STATUS_OK;
  case MR_SET_PARAMETERS:
    rec->SetParametersFromParcel(in);
    AParcel_writeInt32(out, 0);
    return STATUS_OK;
  case MR_SET_PREVIEW_SURFACE:
  case MR_SET_CAMERA:
  case MR_SET_CLIENT_NAME:
  case MR_SET_INPUT_DEVICE:
  case MR_ENABLE_AUDIO_DEVICE_CALLBACK:
  case MR_SET_PREFERRED_MIC_DIRECTION:
  case MR_SET_PREFERRED_MIC_FIELD_DIM:
    AParcel_writeInt32(out, 0);
    return STATUS_OK;
  case MR_SET_LISTENER: {
    AIBinder *listener = nullptr;
    AParcel_readStrongBinder(in, &listener);
    rec->SetListener(listener);
    AParcel_writeInt32(out, 0);
    return STATUS_OK;
  }
  case MR_GET_METRICS:
    return STATUS_OK;
  case MR_GET_ROUTED_DEVICE_ID:
  case MR_GET_PORT_ID:
    AParcel_writeInt32(out, 0); // status = NO_ERROR
    AParcel_writeInt32(out, 0); // id = 0
    return STATUS_OK;
  case MR_GET_ACTIVE_MICROPHONES:
    AParcel_writeInt32(out, 0); // status = NO_ERROR
    AParcel_writeInt32(out, 0); // empty vector length
    return STATUS_OK;
  case MR_GET_RTP_DATA_USAGE:
    AParcel_writeInt32(out, 0);  // status = NO_ERROR
    AParcel_writeUint64(out, 0); // bytes = 0
    return STATUS_OK;
  case MR_SET_PRIVACY_SENSITIVE: {
    int32_t v = 0;
    AParcel_readInt32(in, &v);
    rec->privacySensitive_ = (v == 1);
    AParcel_writeInt32(out, 0);
    return STATUS_OK;
  }
  case MR_GET_PRIVACY_SENSITIVE:
    AParcel_writeInt32(out, 0);
    AParcel_writeInt32(out, rec->privacySensitive_ ? 1 : 0);
    return STATUS_OK;
  default:
    AParcel_writeInt32(out, 0);
    return STATUS_OK;
  }
}

// ============================================================================
// 2. IMediaPlayer 实现（支撑相机提示音、SystemUI 音效同步/异步准备与完播回调）
// ============================================================================

enum MediaPlayerTx : transaction_code_t {
  MP_DISCONNECT = FIRST_CALL_TRANSACTION, // 1
  MP_SET_DATA_SOURCE_URL,                 // 2
  MP_SET_DATA_SOURCE_FD,                  // 3
  MP_SET_DATA_SOURCE_STREAM,              // 4
  MP_SET_DATA_SOURCE_CALLBACK,            // 5
  MP_SET_DATA_SOURCE_RTP,                 // 6
  MP_SET_BUFFERING_SETTINGS,              // 7
  MP_GET_BUFFERING_SETTINGS,              // 8
  MP_PREPARE_ASYNC,                       // 9
  MP_START,                               // 10
  MP_STOP,                                // 11
  MP_IS_PLAYING,                          // 12
  MP_SET_PLAYBACK_SETTINGS,               // 13
  MP_GET_PLAYBACK_SETTINGS,               // 14
  MP_SET_SYNC_SETTINGS,                   // 15
  MP_GET_SYNC_SETTINGS,                   // 16
  MP_PAUSE,                               // 17
  MP_SEEK_TO,                             // 18
  MP_GET_CURRENT_POSITION,                // 19
  MP_GET_DURATION,                        // 20
  MP_RESET,                               // 21
  MP_NOTIFY_AT,                           // 22
  MP_SET_AUDIO_STREAM_TYPE,               // 23
  MP_SET_LOOPING,                         // 24
  MP_SET_VOLUME,                          // 25
  MP_INVOKE,                              // 26
  MP_SET_METADATA_FILTER,                 // 27
  MP_GET_METADATA,                        // 28
  MP_SET_AUX_EFFECT_SEND_LEVEL,           // 29
  MP_ATTACH_AUX_EFFECT,                   // 30
  MP_SET_VIDEO_SURFACETEXTURE,            // 31
  MP_SET_PARAMETER,                       // 32
  MP_GET_PARAMETER,                       // 33
  MP_SET_RETRANSMIT_ENDPOINT,             // 34
  MP_GET_RETRANSMIT_ENDPOINT,             // 35
  MP_SET_NEXT_PLAYER,                     // 36
  MP_APPLY_VOLUME_SHAPER,                 // 37
  MP_GET_VOLUME_SHAPER_STATE,             // 38
  MP_PREPARE_DRM,                         // 39
  MP_RELEASE_DRM,                         // 40
  MP_SET_OUTPUT_DEVICE,                   // 41
  MP_GET_ROUTED_DEVICE_ID,                // 42
  MP_ENABLE_AUDIO_DEVICE_CALLBACK,        // 43
};

class CDHMediaPlayer {
public:
  explicit CDHMediaPlayer(AIBinder *client) : client_(client) {}

  ~CDHMediaPlayer() {
    std::lock_guard<std::mutex> lk(mu_);
    if (client_ != nullptr) {
      AIBinder_decStrong(client_);
      client_ = nullptr;
    }
  }

  void NotifyAsync(int32_t msg, int32_t ext1, int32_t ext2, int delayMs) {
    AIBinder *target = nullptr;
    {
      std::lock_guard<std::mutex> lk(mu_);
      if (client_ == nullptr) {
        return;
      }
      AIBinder_incStrong(client_);
      target = client_;
    }
    std::thread([target, msg, ext1, ext2, delayMs]() {
      if (delayMs > 0) {
        ::usleep(static_cast<useconds_t>(delayMs) * 1000);
      }
      SendLegacyClientNotify(target, gPlayerClientProxyClass,
                             kDescPlayerClient16, kDescPlayerClient16Len, msg,
                             ext1, ext2);
      AIBinder_decStrong(target);
    }).detach();
  }

  void Disconnect() {
    std::lock_guard<std::mutex> lk(mu_);
    playing_.store(false);
    if (client_ != nullptr) {
      AIBinder_decStrong(client_);
      client_ = nullptr;
    }
  }

  std::atomic<bool> playing_{false};
  std::atomic<bool> looping_{false};
  std::atomic<int32_t> positionMs_{0};
  std::atomic<int32_t> durationMs_{200};

private:
  std::mutex mu_;
  AIBinder *client_ = nullptr;
};

void *PlayerOnCreate(void *args) { return args; }
void PlayerOnDestroy(void *userData) {
  delete static_cast<CDHMediaPlayer *>(userData);
}

binder_status_t PlayerOnTransact(AIBinder *binder, transaction_code_t code,
                                 const AParcel *in, AParcel *out) {
  auto *mp = static_cast<CDHMediaPlayer *>(AIBinder_getUserData(binder));
  if (mp == nullptr) {
    return STATUS_UNEXPECTED_NULL;
  }

  switch (code) {
  case MP_DISCONNECT:
    mp->Disconnect();
    return STATUS_OK;
  case MP_SET_DATA_SOURCE_FD:
    (void)ReadRawFdFromParcel(in);
    AParcel_writeInt32(out, 0);
    return STATUS_OK;
  case MP_SET_DATA_SOURCE_URL:
  case MP_SET_DATA_SOURCE_STREAM:
  case MP_SET_DATA_SOURCE_CALLBACK:
  case MP_SET_DATA_SOURCE_RTP:
  case MP_SET_BUFFERING_SETTINGS:
  case MP_SET_PLAYBACK_SETTINGS:
  case MP_SET_SYNC_SETTINGS:
  case MP_NOTIFY_AT:
  case MP_SET_AUDIO_STREAM_TYPE:
  case MP_SET_VOLUME:
  case MP_SET_METADATA_FILTER:
  case MP_SET_AUX_EFFECT_SEND_LEVEL:
  case MP_ATTACH_AUX_EFFECT:
  case MP_SET_VIDEO_SURFACETEXTURE:
  case MP_SET_PARAMETER:
  case MP_SET_RETRANSMIT_ENDPOINT:
  case MP_SET_NEXT_PLAYER:
  case MP_APPLY_VOLUME_SHAPER:
  case MP_RELEASE_DRM:
  case MP_SET_OUTPUT_DEVICE:
  case MP_ENABLE_AUDIO_DEVICE_CALLBACK:
    AParcel_writeInt32(out, 0);
    return STATUS_OK;
  case MP_GET_BUFFERING_SETTINGS:
    AParcel_writeInt32(out, 0);
    AParcel_writeInt32(out, 0);
    AParcel_writeInt32(out, 0);
    return STATUS_OK;
  case MP_PREPARE_ASYNC:
    // MediaPlayer::prepare() 在客户端同步等待 MEDIA_PREPARED 回调
    mp->NotifyAsync(kMediaPrepared, 0, 0, 5);
    AParcel_writeInt32(out, 0);
    return STATUS_OK;
  case MP_START:
    mp->playing_.store(true);
    if (!mp->looping_.load()) {
      mp->NotifyAsync(kMediaPlaybackComplete, 0, 0, 120);
    }
    AParcel_writeInt32(out, 0);
    return STATUS_OK;
  case MP_STOP:
  case MP_PAUSE:
  case MP_RESET:
    mp->playing_.store(false);
    AParcel_writeInt32(out, 0);
    return STATUS_OK;
  case MP_IS_PLAYING:
    AParcel_writeInt32(out, mp->playing_.load() ? 1 : 0);
    AParcel_writeInt32(out, 0);
    return STATUS_OK;
  case MP_GET_PLAYBACK_SETTINGS:
    AParcel_writeInt32(out, 0);
    AParcel_writeFloat(out, 1.0f);
    AParcel_writeFloat(out, 1.0f);
    AParcel_writeInt32(out, 0);
    AParcel_writeInt32(out, 0);
    return STATUS_OK;
  case MP_GET_SYNC_SETTINGS:
    AParcel_writeInt32(out, 0);
    AParcel_writeInt32(out, 0);
    AParcel_writeInt32(out, 0);
    AParcel_writeFloat(out, 0.05f);
    AParcel_writeFloat(out, 30.0f);
    return STATUS_OK;
  case MP_SEEK_TO: {
    int32_t msec = 0;
    AParcel_readInt32(in, &msec);
    mp->positionMs_.store(msec);
    mp->NotifyAsync(kMediaSeekComplete, 0, 0, 5);
    AParcel_writeInt32(out, 0);
    return STATUS_OK;
  }
  case MP_GET_CURRENT_POSITION:
    AParcel_writeInt32(out, mp->positionMs_.load());
    AParcel_writeInt32(out, 0);
    return STATUS_OK;
  case MP_GET_DURATION:
    AParcel_writeInt32(out, mp->durationMs_.load());
    AParcel_writeInt32(out, 0);
    return STATUS_OK;
  case MP_SET_LOOPING: {
    int32_t loop = 0;
    AParcel_readInt32(in, &loop);
    mp->looping_.store(loop != 0);
    AParcel_writeInt32(out, 0);
    return STATUS_OK;
  }
  case MP_GET_METADATA:
    AParcel_writeInt32(out, -38 /* ENOSYS / INVALID_OPERATION */);
    return STATUS_OK;
  case MP_GET_ROUTED_DEVICE_ID:
    AParcel_writeInt32(out, 0);
    AParcel_writeInt32(out, 0);
    return STATUS_OK;
  default:
    AParcel_writeInt32(out, -38);
    return STATUS_OK;
  }
}

// ============================================================================
// 3. IMediaMetadataRetriever 实现（消除 MediaProvider 后台死等与崩溃）
// ============================================================================

enum MetadataRetrieverTx : transaction_code_t {
  MMR_DISCONNECT = FIRST_CALL_TRANSACTION, // 1
  MMR_SET_DATA_SOURCE_URL,                 // 2
  MMR_SET_DATA_SOURCE_FD,                  // 3
  MMR_SET_DATA_SOURCE_CALLBACK,            // 4
  MMR_GET_FRAME_AT_TIME,                   // 5
  MMR_GET_IMAGE_AT_INDEX,                  // 6
  MMR_GET_IMAGE_RECT_AT_INDEX,             // 7
  MMR_GET_FRAME_AT_INDEX,                  // 8
  MMR_EXTRACT_ALBUM_ART,                   // 9
  MMR_EXTRACT_METADATA,                    // 10
};

struct CDHMetadataRetriever {};

void *RetrieverOnCreate(void *args) { return args; }
void RetrieverOnDestroy(void *userData) {
  delete static_cast<CDHMetadataRetriever *>(userData);
}

binder_status_t RetrieverOnTransact(AIBinder *, transaction_code_t code,
                                    const AParcel *in, AParcel *out) {
  switch (code) {
  case MMR_DISCONNECT:
    return STATUS_OK;
  case MMR_SET_DATA_SOURCE_FD:
    (void)ReadRawFdFromParcel(in);
    AParcel_writeInt32(out, 0);
    return STATUS_OK;
  case MMR_SET_DATA_SOURCE_URL:
  case MMR_SET_DATA_SOURCE_CALLBACK:
    AParcel_writeInt32(out, 0);
    return STATUS_OK;
  case MMR_EXTRACT_METADATA: {
    int32_t keyCode = 0;
    AParcel_readInt32(in, &keyCode);
    const char *val = nullptr;
    switch (keyCode) {
    case 9: // METADATA_KEY_DURATION
      val = "1000";
      break;
    case 12: // METADATA_KEY_MIMETYPE
      val = "video/mp4";
      break;
    case 17: // METADATA_KEY_HAS_VIDEO
      val = "yes";
      break;
    case 18: // METADATA_KEY_VIDEO_WIDTH
      val = "1280";
      break;
    case 19: // METADATA_KEY_VIDEO_HEIGHT
      val = "720";
      break;
    default:
      break;
    }
    const auto &b = Bridge();
    if (val != nullptr && b.viewParcelMut != nullptr &&
        b.parcelWriteCString != nullptr) {
      AParcel_writeInt32(out, 0);
      b.parcelWriteCString(b.viewParcelMut(out), val);
    } else {
      AParcel_writeInt32(out, -1);
    }
    return STATUS_OK;
  }
  case MMR_GET_FRAME_AT_TIME:
  case MMR_GET_IMAGE_AT_INDEX:
  case MMR_GET_IMAGE_RECT_AT_INDEX:
  case MMR_GET_FRAME_AT_INDEX:
  case MMR_EXTRACT_ALBUM_ART:
  default:
    AParcel_writeInt32(out, -1 /* UNKNOWN_ERROR -> returns NULL sp<IMemory> */);
    return STATUS_OK;
  }
}

// ============================================================================
// 4. IMediaPlayerService 主服务（注册名 "media.player"）
// ============================================================================

enum MediaPlayerServiceTx : transaction_code_t {
  MPS_CREATE = FIRST_CALL_TRANSACTION, // 1
  MPS_CREATE_MEDIA_RECORDER,           // 2
  MPS_CREATE_METADATA_RETRIEVER,       // 3
  MPS_ADD_BATTERY_DATA,                // 4
  MPS_PULL_BATTERY_DATA,               // 5
  MPS_LISTEN_FOR_REMOTE_DISPLAY,       // 6
  MPS_GET_CODEC_LIST,                  // 7
};

void *ServiceOnCreate(void *args) { return args; }
void ServiceOnDestroy(void *) {}

binder_status_t ProxyNoopTransact(AIBinder *, transaction_code_t,
                                  const AParcel *, AParcel *) {
  return STATUS_OK;
}

binder_status_t ServiceOnTransact(AIBinder *, transaction_code_t code,
                                  const AParcel *in, AParcel *out) {
  switch (code) {
  case MPS_CREATE: {
    AIBinder *client = nullptr;
    AParcel_readStrongBinder(in, &client);
    auto *player = new CDHMediaPlayer(client);
    AIBinder *ib = AIBinder_new(gPlayerClass, player);
    AParcel_writeStrongBinder(out, ib);
    AIBinder_decStrong(ib);
    return STATUS_OK;
  }
  case MPS_CREATE_MEDIA_RECORDER: {
    auto *rec = new CDHMediaRecorder();
    AIBinder *ib = AIBinder_new(gRecorderClass, rec);
    AParcel_writeStrongBinder(out, ib);
    AIBinder_decStrong(ib);
    return STATUS_OK;
  }
  case MPS_CREATE_METADATA_RETRIEVER: {
    auto *ret = new CDHMetadataRetriever();
    AIBinder *ib = AIBinder_new(gRetrieverClass, ret);
    AParcel_writeStrongBinder(out, ib);
    AIBinder_decStrong(ib);
    return STATUS_OK;
  }
  case MPS_ADD_BATTERY_DATA:
  case MPS_PULL_BATTERY_DATA:
    return STATUS_OK;
  case MPS_LISTEN_FOR_REMOTE_DISPLAY:
    WriteNullStrongBinder(out);
    return STATUS_OK;
  case MPS_GET_CODEC_LIST: {
    const auto &b = Bridge();
    if (b.mediaCodecListGetLocal != nullptr && b.interfaceAsBinder != nullptr &&
        b.parcelWriteStrongBinder != nullptr && b.viewParcelMut != nullptr) {
      AndroidSp mcl = b.mediaCodecListGetLocal();
      if (mcl.ptr != nullptr) {
        AndroidSp ib = b.interfaceAsBinder(&mcl);
        if (ib.ptr != nullptr) {
          b.parcelWriteStrongBinder(b.viewParcelMut(out), &ib);
          return STATUS_OK;
        }
      }
    }
    WriteNullStrongBinder(out);
    return STATUS_OK;
  }
  default:
    return STATUS_UNKNOWN_TRANSACTION;
  }
}

} // namespace

int main() {
  ABinderProcess_setThreadPoolMaxThreadCount(8);
  ABinderProcess_startThreadPool();

  (void)Bridge();

  gPlayerServiceClass = AIBinder_Class_define(
      kDescMediaPlayerService, ServiceOnCreate, ServiceOnDestroy,
      ServiceOnTransact);
  gRecorderClass = AIBinder_Class_define(
      kDescMediaRecorder, RecorderOnCreate, RecorderOnDestroy,
      RecorderOnTransact);
  gPlayerClass = AIBinder_Class_define(kDescMediaPlayer, PlayerOnCreate,
                                       PlayerOnDestroy, PlayerOnTransact);
  gRetrieverClass = AIBinder_Class_define(
      kDescMediaMetadataRetriever, RetrieverOnCreate, RetrieverOnDestroy,
      RetrieverOnTransact);

  gPlayerClientProxyClass = AIBinder_Class_define(
      "android.media.IMediaPlayerClient", ServiceOnCreate, ServiceOnDestroy,
      ProxyNoopTransact);
  AIBinder_Class_disableInterfaceTokenHeader(gPlayerClientProxyClass);

  gRecorderClientProxyClass = AIBinder_Class_define(
      "android.media.IMediaRecorderClient", ServiceOnCreate, ServiceOnDestroy,
      ProxyNoopTransact);
  AIBinder_Class_disableInterfaceTokenHeader(gRecorderClientProxyClass);

  AIBinder *service = AIBinder_new(gPlayerServiceClass, nullptr);
  const binder_status_t st = AServiceManager_addService(service, "media.player");
  if (st != STATUS_OK) {
    __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                        "failed to register media.player: %d", st);
    return 1;
  }
  __android_log_print(ANDROID_LOG_INFO, kLogTag,
                      "registered 64-bit media.player service");

  ABinderProcess_joinThreadPool();
  return 0;
}
