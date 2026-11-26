// android.hardware.camera 的 honor 变体（MagicOS 19 项专有 Vendor Tags +
// IHwCameraResourceService AIDL 服务）。
// 只在 --variant honor（-DCDH_CAMERA_HAS_VENDOR_EXT=1 -DCDH_CAMERA_HONOR=1）时编入。
#include "camera_vendor_ext.h"

#include <android/binder_ibinder.h>
#include <android/binder_manager.h>
#include <android/binder_parcel.h>
#include <android/binder_status.h>
#include <android/log.h>

#include <cstring>

extern "C" {
void AIBinder_markVintfStability(AIBinder *binder);
}

namespace cdh::camera {
namespace {

constexpr const char *kLogTag = "cdh-camera-honor";

static constexpr uint32_t kHonorCaptureCameraSceneModeTag = 0x80300001;
static constexpr uint32_t kHonorCaptureFaceBeautyModeTag = 0x80300002;
static constexpr uint32_t kHonorCaptureFaceBeautyLevelTag = 0x80300003;
static constexpr uint32_t kHonorCaptureColorEffectModeTag = 0x80300004;
static constexpr uint32_t kHonorCaptureHwCamera2FlagTag = 0x80300005;
static constexpr uint32_t kHonorCaptureExposureCompValueTag = 0x80300006;
static constexpr uint32_t kHonorCaptureApiVersionTag = 0x80300007;
static constexpr uint32_t kHonorCaptureHwVideoFpsTag = 0x80300008;
static constexpr uint32_t kHonorCaptureCaptureStreamResTag = 0x80300009;
static constexpr uint32_t kHonorCaptureVideoStreamResTag = 0x8030000a;
static constexpr uint32_t kHonorCapturePortraitModeTag = 0x8030000b;
static constexpr uint32_t kHonorCaptureLivePhotoModeTag = 0x8030000c;
static constexpr uint32_t kHonorCaptureSmartCaptureEnableTag = 0x8030000d;
static constexpr uint32_t kHonorCaptureSensorHdrModeTag = 0x8030000e;
static constexpr uint32_t kHonorCaptureVideoHdrModeTag = 0x8030000f;
static constexpr uint32_t kHonorCaptureMasterAiEnableTag = 0x80300010;
static constexpr uint32_t kHonorCaptureHdrVividEnableTag = 0x80300011;
static constexpr uint32_t kHonorDevCapSitSupportedTag = 0x80310001;
static constexpr uint32_t kHonorDevCapMainModeSupportedTag = 0x80310002;

static const Note13VendorTagDesc kHonorVendorTagDescs[] = {
    {kHonorCaptureCameraSceneModeTag, "com.hihonor.capture.metadata", "cameraSceneMode", TYPE_INT32},
    {kHonorCaptureFaceBeautyModeTag, "com.hihonor.capture.metadata", "faceBeautyMode", TYPE_BYTE},
    {kHonorCaptureFaceBeautyLevelTag, "com.hihonor.capture.metadata", "faceBeautyLevel", TYPE_INT32},
    {kHonorCaptureColorEffectModeTag, "com.hihonor.capture.metadata", "colorEffectMode", TYPE_BYTE},
    {kHonorCaptureHwCamera2FlagTag, "com.hihonor.capture.metadata", "hwCamera2Flag", TYPE_BYTE},
    {kHonorCaptureExposureCompValueTag, "com.hihonor.capture.metadata", "exposureCompValue", TYPE_FLOAT},
    {kHonorCaptureApiVersionTag, "com.hihonor.capture.metadata", "apiVersion", TYPE_INT32},
    {kHonorCaptureHwVideoFpsTag, "com.hihonor.capture.metadata", "hw-video-fps", TYPE_INT32},
    {kHonorCaptureCaptureStreamResTag, "com.hihonor.capture.metadata", "captureStreamResolution", TYPE_INT32},
    {kHonorCaptureVideoStreamResTag, "com.hihonor.capture.metadata", "videoStreamResolution", TYPE_INT32},
    {kHonorCapturePortraitModeTag, "com.hihonor.capture.metadata", "portraitMode", TYPE_BYTE},
    {kHonorCaptureLivePhotoModeTag, "com.hihonor.capture.metadata", "livePhotoMode", TYPE_BYTE},
    {kHonorCaptureSmartCaptureEnableTag, "com.hihonor.capture.metadata", "smartCaptureEnable", TYPE_BYTE},
    {kHonorCaptureSensorHdrModeTag, "com.hihonor.capture.metadata", "sensorHdrMode", TYPE_BYTE},
    {kHonorCaptureVideoHdrModeTag, "com.hihonor.capture.metadata", "videoHdrMode", TYPE_BYTE},
    {kHonorCaptureMasterAiEnableTag, "com.hihonor.capture.metadata", "masterAIEnable", TYPE_BYTE},
    {kHonorCaptureHdrVividEnableTag, "com.hihonor.capture.metadata", "HDRVividEnable", TYPE_INT32},
    {kHonorDevCapSitSupportedTag, "com.hihonor.device.capabilities", "honor_sit_supported", TYPE_BYTE},
    {kHonorDevCapMainModeSupportedTag, "com.hihonor.device.capabilities", "mainModeSupported", TYPE_BYTE},
};

// 荣耀 MagicOS /system/bin/HwServiceHost（CPlatformAdapter::getImpl）优先查询
// AIDL vendor.honor.hardware.camera.camResource.IHwCameraResourceService/default，
// 缺席时会回落 HIDL camResource@1.0 并因阻塞超时触发 SIGABRT("failed to get hidl service")，
// 导致 com.hihonor.camera 收到 ServiceHost death error 每 5 秒循环重启无法出图。
constexpr const char *kHonorCamResourceDesc =
    "vendor.honor.hardware.camera.camResource.IHwCameraResourceService";
constexpr const char *kHonorCamResourceInstance =
    "vendor.honor.hardware.camera.camResource.IHwCameraResourceService/default";
constexpr const char *kHonorCamResourceHash =
    "cff0830a2ff3f8186c2846e0231042d30445f481";

constexpr transaction_code_t kAidlGetInterfaceVersion = 16777215;
constexpr transaction_code_t kAidlGetInterfaceHash = 16777214;

AIBinder *gHonorCamResourceBinder = nullptr;

void *honorStubOnCreate(void *args) { return args; }
void honorStubOnDestroy(void *) {}

binder_status_t writeStatusOk(AParcel *out) {
  AStatus *ok = AStatus_newOk();
  binder_status_t st = AParcel_writeStatusHeader(out, ok);
  AStatus_delete(ok);
  return st;
}

binder_status_t onHonorCamResourceTransact(AIBinder *, transaction_code_t code,
                                           const AParcel *, AParcel *out) {
  switch (code) {
    case 1:   // connect(IBinder, int) -> int
    case 2:   // copybit(HwCopybitParam) -> int
    case 3:   // disconnect(IBinder, int) -> int
    case 7:   // jpegCloseClient() -> int
    case 8:   // jpegOpenClient(int, int) -> int
    case 10:  // offLineAlgoDeinit(HwOffLineAlgoParam) -> int
    case 11:  // offLineAlgoInit(HwOffLineAlgoParam) -> int
    case 12:  // offLineAlgoProcess(HwOffLineAlgoParam) -> int
    {
      binder_status_t st = writeStatusOk(out);
      return st == STATUS_OK ? AParcel_writeInt32(out, 0) : st;
    }
    case 4:  // getCopybitDefaultOutFormat() -> int (HAL_PIXEL_FORMAT_YCrCb_420_SP)
    {
      binder_status_t st = writeStatusOk(out);
      return st == STATUS_OK ? AParcel_writeInt32(out, 0x11) : st;
    }
    case 5:  // getCopybitOutUsage(int) -> int (CPU_READ_OFTEN | CPU_WRITE_OFTEN)
    {
      binder_status_t st = writeStatusOk(out);
      return st == STATUS_OK ? AParcel_writeInt32(out, 0x33) : st;
    }
    case 6:  // isCopybitSupportedOutFormat(int) -> bool
    {
      binder_status_t st = writeStatusOk(out);
      return st == STATUS_OK ? AParcel_writeBool(out, true) : st;
    }
    case 9:  // jpegencode(HwJpegEncoderConfig, out HwCamResouceLongType) -> int
    {
      binder_status_t st = writeStatusOk(out);
      if (st != STATUS_OK) return st;
      st = AParcel_writeInt32(out, 0);
      if (st != STATUS_OK) return st;
      // non-null parcelable header + parcelable size (int32 size + int64 value = 12) + int64(0)
      st = AParcel_writeInt32(out, 1);
      if (st != STATUS_OK) return st;
      st = AParcel_writeInt32(out, 12);
      if (st != STATUS_OK) return st;
      return AParcel_writeInt64(out, 0);
    }
    case 13:  // offLineAlgoQueryCap(String) -> bool
    {
      binder_status_t st = writeStatusOk(out);
      return st == STATUS_OK ? AParcel_writeBool(out, false) : st;
    }
    case 14:  // registerListener(IHwCamResourceListener, int, long) -> void
    case 15:  // unregisterListener(IHwCamResourceListener, long) -> void
      return writeStatusOk(out);
    case kAidlGetInterfaceVersion: {
      binder_status_t st = writeStatusOk(out);
      return st == STATUS_OK ? AParcel_writeInt32(out, 1) : st;
    }
    case kAidlGetInterfaceHash: {
      binder_status_t st = writeStatusOk(out);
      return st == STATUS_OK
                 ? AParcel_writeString(
                       out, kHonorCamResourceHash,
                       static_cast<int32_t>(std::strlen(kHonorCamResourceHash)))
                 : st;
    }
    default: {
      __android_log_print(ANDROID_LOG_INFO, kLogTag,
                          "IHwCameraResourceService unhandled code=%u, returning OK",
                          code);
      return writeStatusOk(out);
    }
  }
}

} // namespace

void AppendVendorTagDescs(std::vector<Note13VendorTagDesc> *out) {
  for (const auto &d : kHonorVendorTagDescs) {
    out->push_back(d);
  }
}

bool CollectExposedCameraTargets(const CameraTargetDefaults &,
                                 std::vector<const Note13CameraTarget *> *) {
  return false;
}

bool AppendVendorStaticMetadata(camera_metadata_t *metadata,
                                const Note13CameraTarget &target) {
  const uint8_t sitSupported = 1;
  const uint8_t mainModeSupported =
      target.lensFacing == ANDROID_LENS_FACING_BACK ? 1 : 0;
  return AddMetaEntry(metadata, kHonorDevCapSitSupportedTag, &sitSupported, 1) &&
         AddMetaEntry(metadata, kHonorDevCapMainModeSupportedTag,
                      &mainModeSupported, 1);
}

void AppendVendorCharacteristicKeys(std::vector<int32_t> *keys,
                                    const Note13CameraTarget &) {
  keys->push_back(static_cast<int32_t>(kHonorDevCapSitSupportedTag));
  keys->push_back(static_cast<int32_t>(kHonorDevCapMainModeSupportedTag));
}

bool AppendVendorDefaultRequest(camera_metadata_t *,
                                const Note13CameraTarget &) {
  return true;
}

void RegisterVendorExtraServices() {
  AIBinder_Class *resClass = AIBinder_Class_define(
      kHonorCamResourceDesc, honorStubOnCreate, honorStubOnDestroy,
      onHonorCamResourceTransact);
  if (resClass == nullptr) {
    __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                        "failed to define %s binder class",
                        kHonorCamResourceDesc);
    return;
  }
  gHonorCamResourceBinder = AIBinder_new(resClass, nullptr);
  if (gHonorCamResourceBinder == nullptr) {
    return;
  }
  AIBinder_markVintfStability(gHonorCamResourceBinder);
  binder_status_t st = AServiceManager_addService(gHonorCamResourceBinder,
                                                  kHonorCamResourceInstance);
  if (st != STATUS_OK) {
    __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                        "failed to register %s: %d",
                        kHonorCamResourceInstance, st);
  } else {
    __android_log_print(ANDROID_LOG_INFO, kLogTag,
                        "registered %s (unblocks HwServiceHost & Honor Camera)",
                        kHonorCamResourceInstance);
  }
}

} // namespace cdh::camera
