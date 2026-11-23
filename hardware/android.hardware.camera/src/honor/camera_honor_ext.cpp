// android.hardware.camera 的 honor 变体（MagicOS 19 项专有 Vendor Tags）。
// 只在 --variant honor（-DCDH_CAMERA_HAS_VENDOR_EXT=1 -DCDH_CAMERA_HONOR=1）时编入。
#include "camera_vendor_ext.h"

namespace cdh::camera {
namespace {

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

void RegisterVendorExtraServices() {}

} // namespace cdh::camera
