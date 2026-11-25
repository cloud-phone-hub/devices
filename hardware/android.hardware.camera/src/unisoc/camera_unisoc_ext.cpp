// android.hardware.camera 的 unisoc 变体：
//   1. 补齐展锐（Unisoc）原生相机 DreamCamera2 / SprdCamera3Setting 所需的全量
//      com.addParameters.* 与 com.sensor.info.* 私有 Vendor Tags，并与 HMD 定制相机兼容；
//   2. 在默认请求模板中预置 tagId=0x8000000A，防止 libunisoccameraservice.so 空指针崩溃。
// 只在 --variant unisoc（-DCDH_CAMERA_HAS_VENDOR_EXT=1 -DCDH_CAMERA_UNISOC=1）时编入。
#include "camera_vendor_ext.h"

#include <cstring>
#include <string>
#include <unistd.h>
#include <unordered_set>

namespace cdh::camera {
namespace {

bool HasUnisocDreamCameraApp() {
  static const char *kPaths[] = {
      "/system/priv-app/DreamCamera2",
      "/product/priv-app/DreamCamera2",
      "/product/app/DreamCamera2",
      "/system/app/DreamCamera2",
      "/system_ext/priv-app/DreamCamera2",
      "/system_ext/app/DreamCamera2",
      "/system_ext/app/DreamCamera2/DreamCamera2.apk",
  };
  for (const char *p : kPaths) {
    if (access(p, F_OK) == 0) {
      return true;
    }
  }
  return false;
}

// 展锐 DreamCamera2（com.android.camera2）与 SprdCamera3Setting 引用的扩展私有 Vendor Tags
// （0xC2000011 起紧接 camera_vendor_tags.inc 既有 0xC2000000..0xC2000010 段，无 ID 冲突）。
static const Note13VendorTagDesc kUnisocExtraVendorTags[] = {
    {0xC2000011, "com.addParameters", "adjustSprdFlashLevel", TYPE_INT32},
    {0xC2000012, "com.addParameters", "availableContrast", TYPE_INT32},
    {0xC2000013, "com.addParameters", "availableFaceFeatureMask", TYPE_BYTE},
    {0xC2000014, "com.addParameters", "availableISO", TYPE_INT32},
    {0xC2000015, "com.addParameters", "availableMeteringMode", TYPE_INT32},
    {0xC2000016, "com.addParameters", "availableSaturation", TYPE_INT32},
    {0xC2000017, "com.addParameters", "availableSensorType", TYPE_INT32},
    {0xC2000018, "com.addParameters", "availableSlowMotion", TYPE_INT32},
    {0xC2000019, "com.addParameters", "availableSprdFlashLevel", TYPE_INT32},
    {0xC200001A, "com.addParameters", "contrast", TYPE_INT32},
    {0xC200001B, "com.addParameters", "isHdrSceneForAuto", TYPE_INT32},
    {0xC200001C, "com.addParameters", "isTakePictureWithFlash", TYPE_INT32},
    {0xC200001D, "com.addParameters", "iso", TYPE_INT32},
    {0xC200001E, "com.addParameters", "maxPreviewSize", TYPE_INT32},
    {0xC200001F, "com.addParameters", "mirror", TYPE_BYTE},
    {0xC2000020, "com.addParameters", "perfectskinlevel", TYPE_INT32},
    {0xC2000021, "com.addParameters", "saturation", TYPE_INT32},
    {0xC2000022, "com.addParameters", "sprd3AvailableSensorSelfShot", TYPE_INT32},
    {0xC2000023, "com.addParameters", "sprd3BlurCapVersion", TYPE_INT32},
    {0xC2000024, "com.addParameters", "sprd3availableAuto3dnr", TYPE_INT32},
    {0xC2000025, "com.addParameters", "sprd3is3dnrScene", TYPE_INT32},
    {0xC2000026, "com.addParameters", "sprdAIScene", TYPE_INT32},
    {0xC2000027, "com.addParameters", "sprdAISceneEnabled", TYPE_INT32},
    {0xC2000028, "com.addParameters", "sprdAfDistance", TYPE_INT64},
    {0xC2000029, "com.addParameters", "sprdAfStateEnable", TYPE_INT32},
    {0xC200002A, "com.addParameters", "sprdAutoChasingEnable", TYPE_INT32},
    {0xC200002B, "com.addParameters", "sprdAutoChasingRegion", TYPE_INT32},
    {0xC200002C, "com.addParameters", "sprdAutoChasingTraceRegion", TYPE_INT32},
    {0xC200002D, "com.addParameters", "sprdAutoHdrEnabled", TYPE_INT32},
    {0xC200002E, "com.addParameters", "sprdAvailableAutoHdr", TYPE_INT32},
    {0xC200002F, "com.addParameters", "sprdAvailableGesturedetect", TYPE_INT32},
    {0xC2000030, "com.addParameters", "sprdAvailableLogoWatermark", TYPE_INT32},
    {0xC2000031, "com.addParameters", "sprdAvailableTimeWatermark", TYPE_INT32},
    {0xC2000032, "com.addParameters", "sprdAwbCtRange", TYPE_INT32},
    {0xC2000033, "com.addParameters", "sprdCamFeatureList", TYPE_INT32},
    {0xC2000034, "com.addParameters", "sprdCamIdSensorRoleType", TYPE_BYTE},
    {0xC2000035, "com.addParameters", "sprdDualCamCoveredVal", TYPE_INT32},
    {0xC2000036, "com.addParameters", "sprdEisEnabled", TYPE_BYTE},
    {0xC2000037, "com.addParameters", "sprdEndRecoder", TYPE_INT32},
    {0xC2000038, "com.addParameters", "sprdFaceAttributes", TYPE_INT32},
    {0xC2000039, "com.addParameters", "sprdFaceAttributesEnable", TYPE_INT32},
    {0xC200003A, "com.addParameters", "sprdFlashLcdMode", TYPE_BYTE},
    {0xC200003B, "com.addParameters", "sprdGesturedetectCurrent", TYPE_INT32},
    {0xC200003C, "com.addParameters", "sprdGesturedetectEnabled", TYPE_BYTE},
    {0xC200003D, "com.addParameters", "sprdHdrPlusEnabled", TYPE_INT32},
    {0xC200003E, "com.addParameters", "sprdHighSize", TYPE_INT32},
    {0xC200003F, "com.addParameters", "sprdLogoWatermarkEnabled", TYPE_INT32},
    {0xC2000040, "com.addParameters", "sprdMotionPhoto", TYPE_INT32},
    {0xC2000041, "com.addParameters", "sprdNotifyNextCapture", TYPE_INT32},
    {0xC2000042, "com.addParameters", "sprdResetSetting", TYPE_BYTE},
    {0xC2000043, "com.addParameters", "sprdSendorExposureLongTime", TYPE_DOUBLE},
    {0xC2000044, "com.addParameters", "sprdSetHighSize", TYPE_INT32},
    {0xC2000045, "com.addParameters", "sprdSimleShutterEnable", TYPE_INT32},
    {0xC2000046, "com.addParameters", "sprdSlidePosition", TYPE_INT32},
    {0xC2000047, "com.addParameters", "sprdSmileCapture", TYPE_INT32},
    {0xC2000048, "com.addParameters", "sprdStabilityCurrent", TYPE_INT32},
    {0xC2000049, "com.addParameters", "sprdStartRecoder", TYPE_INT32},
    {0xC200004A, "com.addParameters", "sprdSwitchCaptureRequestEnable", TYPE_INT32},
    {0xC200004B, "com.addParameters", "sprdTimeWatermarkEnabled", TYPE_INT32},
    {0xC200004C, "com.addParameters", "sprdTouchInfo", TYPE_INT32},
    {0xC200004D, "com.addParameters", "sprdUltraDistortionCorrectEnable", TYPE_INT32},
    {0xC200004E, "com.addParameters", "sprdVideoBlurEnable", TYPE_INT32},
    {0xC200004F, "com.addParameters", "sprdVideoFrameComplete", TYPE_INT32},
    {0xC2000050, "com.addParameters", "sprdVideoHdrEnable", TYPE_BYTE},
    {0xC2000051, "com.addParameters", "sprdVideoMaxZoom", TYPE_FLOAT},
    {0xC2000052, "com.addParameters", "sprdWaterMarkFlag", TYPE_INT32},
    {0xC2000053, "com.addParameters", "sprdWaterMarkPara", TYPE_INT32},
    {0xC2000054, "com.addParameters", "sprdZoomRatio", TYPE_FLOAT},
    {0xC2000055, "com.addParameters", "sprdZoomRatioSection", TYPE_FLOAT},
    {0xC2000056, "com.addParameters", "sprdfastthumb", TYPE_INT32},
    {0xC2000057, "com.addParameters", "sprdfiltertype", TYPE_INT32},
    {0xC2000058, "com.addParameters", "sprdlightPortraitTips", TYPE_INT32},
    {0xC2000059, "com.addParameters", "sprdlightPortraitType", TYPE_INT32},
    {0xC200005A, "com.addParameters", "sprdportraitbackgroundreplacementtype", TYPE_INT32},
    {0xC200005B, "com.addParameters", "srpd3dCalibrationEnable", TYPE_INT32},
    {0xC200005C, "com.addParameters", "srpd3dCalibrationSize", TYPE_INT32},
    {0xC200005D, "com.addParameters", "supermacrophoto", TYPE_INT32},
    {0xC200005E, "com.addParameters", "uShutterTimeNanos", TYPE_INT64},
    {0xC200005F, "com.addParameters", "videomotioncheck", TYPE_INT32},
    {0xC20C0000, "com.sensor.info", "ab", TYPE_BYTE},
};

} // namespace

void AppendVendorTagDescs(std::vector<Note13VendorTagDesc> *out) {
  if (out == nullptr) {
    return;
  }
  // DreamCamera2 中 capMode / meteringMode / sprdZslEnabled 为 Integer.TYPE (TYPE_INT32)，
  // 而 HMD 定制相机 HMDCamera 中为 Byte.TYPE (TYPE_BYTE)；按是否内置 DreamCamera2 精确分流。
  const bool dreamCamera = HasUnisocDreamCameraApp();
  std::unordered_set<std::string> existingKeys;
  existingKeys.reserve(out->size() + 96);
  for (auto &desc : *out) {
    if (desc.section == nullptr || desc.tagName == nullptr) {
      continue;
    }
    if (dreamCamera && std::strcmp(desc.section, "com.addParameters") == 0 &&
        (std::strcmp(desc.tagName, "capMode") == 0 ||
         std::strcmp(desc.tagName, "meteringMode") == 0 ||
         std::strcmp(desc.tagName, "sprdZslEnabled") == 0)) {
      desc.tagType = TYPE_INT32;
    }
    existingKeys.emplace(std::string(desc.section) + "." + desc.tagName);
  }
  for (const auto &extra : kUnisocExtraVendorTags) {
    std::string full = std::string(extra.section) + "." + extra.tagName;
    if (existingKeys.insert(full).second) {
      out->push_back(extra);
    }
  }
}

bool CollectExposedCameraTargets(const CameraTargetDefaults &,
                                 std::vector<const Note13CameraTarget *> *) {
  return false;
}

static constexpr uint32_t kUnisocCharacteristicKeys[] = {
    0xC2000012u, 0xC2000013u, 0xC2000014u, 0xC2000015u, 0xC2000016u,
    0xC2000017u, 0xC2000018u, 0xC2000019u, 0xC200001Eu, 0xC2000022u,
    0xC2000024u, 0xC200002Eu, 0xC200002Fu, 0xC2000030u, 0xC2000031u,
    0xC2000032u, 0xC2000033u, 0xC2000034u, 0xC2000051u, 0xC2000055u,
};

static constexpr uint32_t kUnisocResultKeys[] = {
    0xC2000004u, 0xC200001Bu, 0xC200001Cu, 0xC2000023u, 0xC2000025u,
    0xC2000026u, 0xC2000028u, 0xC2000029u, 0xC200002Cu, 0xC2000035u,
    0xC2000037u, 0xC2000038u, 0xC200003Bu, 0xC2000041u, 0xC2000048u,
    0xC2000049u, 0xC200004Au, 0xC200004Fu, 0xC2000052u, 0xC2000058u,
};

static constexpr uint32_t kUnisocRequestKeys[] = {
    0xC2000000u, 0xC2000001u, 0xC2000002u, 0xC2000003u, 0xC2000005u,
    0xC2000006u, 0xC2000007u, 0xC2000008u, 0xC200000Bu, 0xC200000Cu,
    0xC200000Du, 0xC200000Eu, 0xC2000010u, 0xC2000011u, 0xC200001Au,
    0xC200001Du, 0xC200001Fu, 0xC2000020u, 0xC2000021u, 0xC2000027u,
    0xC200002Au, 0xC200002Bu, 0xC200002Du, 0xC2000036u, 0xC2000039u,
    0xC200003Au, 0xC200003Cu, 0xC200003Du, 0xC200003Fu, 0xC2000040u,
    0xC2000042u, 0xC2000044u, 0xC2000045u, 0xC2000046u, 0xC2000047u,
    0xC200004Bu, 0xC200004Cu, 0xC200004Du, 0xC200004Eu, 0xC2000050u,
    0xC2000053u, 0xC2000054u, 0xC2000056u, 0xC2000057u, 0xC2000059u,
    0xC200005Au, 0xC200005Bu, 0xC200005Du, 0xC200005Eu, 0xC200005Fu,
};

bool AppendVendorStaticMetadata(camera_metadata_t *metadata,
                                const Note13CameraTarget &target) {
  const int32_t kSevenLevels[] = {0, 1, 2, 3, 4, 5, 6};
  const int32_t kIsoLevels[] = {0, 1, 2, 3, 4, 5};
  const int32_t kMeteringModes[] = {0, 1, 2};
  const int32_t kSlowMotionModes[] = {0, 1};
  const int32_t maxPrevW = target.pixelWidth >= 1920 ? 1920 : target.pixelWidth;
  const int32_t maxPrevH =
      target.pixelHeight >= 1080 ? 1080 : target.pixelHeight;
  const int32_t maxPreviewSize[] = {maxPrevW, maxPrevH};
  const int32_t kAwbCtRange[] = {2000, 8000};
  const int32_t kFeatureList[] = {0};
  const float kZoomRatioSection[] = {1.0f, 4.0f};
  const uint8_t kZeroByte = 0;
  const int32_t kZeroInt = 0;
  const float kVideoMaxZoom = 4.0f;

  return AddMetaEntry(metadata, 0xC2000012u, kSevenLevels,
                      std::size(kSevenLevels)) &&
         AddMetaSingle(metadata, 0xC2000013u, kZeroByte) &&
         AddMetaEntry(metadata, 0xC2000014u, kIsoLevels,
                      std::size(kIsoLevels)) &&
         AddMetaEntry(metadata, 0xC2000015u, kMeteringModes,
                      std::size(kMeteringModes)) &&
         AddMetaEntry(metadata, 0xC2000016u, kSevenLevels,
                      std::size(kSevenLevels)) &&
         AddMetaSingle(metadata, 0xC2000017u, kZeroInt) &&
         AddMetaEntry(metadata, 0xC2000018u, kSlowMotionModes,
                      std::size(kSlowMotionModes)) &&
         AddMetaSingle(metadata, 0xC2000019u, kZeroInt) &&
         AddMetaEntry(metadata, 0xC200001Eu, maxPreviewSize,
                      std::size(maxPreviewSize)) &&
         AddMetaSingle(metadata, 0xC2000022u, kZeroInt) &&
         AddMetaSingle(metadata, 0xC2000024u, kZeroInt) &&
         AddMetaSingle(metadata, 0xC200002Eu, kZeroInt) &&
         AddMetaSingle(metadata, 0xC200002Fu, kZeroInt) &&
         AddMetaSingle(metadata, 0xC2000030u, kZeroInt) &&
         AddMetaSingle(metadata, 0xC2000031u, kZeroInt) &&
         AddMetaEntry(metadata, 0xC2000032u, kAwbCtRange,
                      std::size(kAwbCtRange)) &&
         AddMetaEntry(metadata, 0xC2000033u, kFeatureList,
                      std::size(kFeatureList)) &&
         AddMetaSingle(metadata, 0xC2000034u, kZeroByte) &&
         AddMetaSingle(metadata, 0xC2000051u, kVideoMaxZoom) &&
         AddMetaEntry(metadata, 0xC2000055u, kZoomRatioSection,
                      std::size(kZoomRatioSection));
}

void AppendVendorCharacteristicKeys(std::vector<int32_t> *keys,
                                    const Note13CameraTarget &) {
  if (keys == nullptr) {
    return;
  }
  for (uint32_t tag : kUnisocCharacteristicKeys) {
    keys->push_back(static_cast<int32_t>(tag));
  }
}

void AppendVendorRequestAndResultKeys(std::vector<uint32_t> *requestKeys,
                                      std::vector<uint32_t> *resultKeys,
                                      const Note13CameraTarget &) {
  if (requestKeys != nullptr) {
    for (uint32_t tag : kUnisocRequestKeys) {
      requestKeys->push_back(tag);
    }
  }
  if (resultKeys != nullptr) {
    for (uint32_t tag : kUnisocResultKeys) {
      resultKeys->push_back(tag);
    }
  }
}

bool AppendVendorDefaultRequest(camera_metadata_t *metadata,
                                const Note13CameraTarget &) {
  // 展锐 libunisoccameraservice.so 的 UnisocCamera3Device::getSlowMotion 硬编码查询
  // tagId=0x8000000A 且未判空直接解引用 entry.data.u8[0]，预置为 0 防止 SIGSEGV。
  const int32_t sprdSlowMotionDefault = 0;
  return AddMetaSingle(metadata, 0x8000000Au, sprdSlowMotionDefault);
}

bool AppendVendorResultMetadata(camera_metadata_t *metadata,
                                const Note13CameraTarget &) {
  const int32_t kZeroInt = 0;
  // sprdSwitchCaptureRequestEnable(0xC200004A)=1 表示变焦切换已就绪允许按快门，
  // sprdNotifyNextCapture(0xC2000041)=1 用于拍照后回调 onNextCaptureReady 重新解锁快门。
  const int32_t kOneInt = 1;
  const int64_t kZeroLong = 0;
  const int32_t kZeroRect[] = {0, 0, 0, 0};
  const int32_t kZeroFaceAttr[] = {0};

  return AddMetaSingle(metadata, 0xC2000004u, kZeroInt) &&
         AddMetaSingle(metadata, 0xC200001Bu, kZeroInt) &&
         AddMetaSingle(metadata, 0xC200001Cu, kZeroInt) &&
         AddMetaSingle(metadata, 0xC2000023u, kZeroInt) &&
         AddMetaSingle(metadata, 0xC2000025u, kZeroInt) &&
         AddMetaSingle(metadata, 0xC2000026u, kZeroInt) &&
         AddMetaSingle(metadata, 0xC2000028u, kZeroLong) &&
         AddMetaSingle(metadata, 0xC2000029u, kZeroInt) &&
         AddMetaEntry(metadata, 0xC200002Cu, kZeroRect,
                      std::size(kZeroRect)) &&
         AddMetaSingle(metadata, 0xC2000035u, kZeroInt) &&
         AddMetaSingle(metadata, 0xC2000037u, kZeroInt) &&
         AddMetaEntry(metadata, 0xC2000038u, kZeroFaceAttr,
                      std::size(kZeroFaceAttr)) &&
         AddMetaSingle(metadata, 0xC200003Bu, kZeroInt) &&
         AddMetaSingle(metadata, 0xC2000041u, kOneInt) &&
         AddMetaSingle(metadata, 0xC2000048u, kZeroInt) &&
         AddMetaSingle(metadata, 0xC2000049u, kZeroInt) &&
         AddMetaSingle(metadata, 0xC200004Au, kOneInt) &&
         AddMetaSingle(metadata, 0xC200004Fu, kZeroInt) &&
         AddMetaSingle(metadata, 0xC2000052u, kZeroInt) &&
         AddMetaSingle(metadata, 0xC2000058u, kZeroInt);
}

void RegisterVendorExtraServices() {}

} // namespace cdh::camera
