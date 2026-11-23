// android.hardware.camera 的 samsung 变体（One UI 20 项 Vendor Tags + 逻辑多摄 ID）。
// 只在 --variant samsung（-DCDH_CAMERA_HAS_VENDOR_EXT=1 -DCDH_CAMERA_SAMSUNG=1）时编入。
#include "camera_vendor_ext.h"

#include <array>
#include <iterator>

namespace cdh::camera {
namespace {

static constexpr uint32_t kSamScalerAvailPreviewConfigsTag = 0x80200001;
static constexpr uint32_t kSamScalerAvailPictureConfigsTag = 0x80200002;
static constexpr uint32_t kSamScalerAvailThumbConfigsTag = 0x80200003;
static constexpr uint32_t kSamScalerAvailVideoConfigsTag = 0x80200004;
static constexpr uint32_t kSamScalerAvailMaxDigitalZoomTag = 0x80200005;
static constexpr uint32_t kSamScalerAvailMinDigitalZoomTag = 0x80200006;
static constexpr uint32_t kSamScalerAvailMaxDigitalZoomListTag = 0x80200007;
static constexpr uint32_t kSamScalerAvailMinDigitalZoomListTag = 0x80200008;
static constexpr uint32_t kSamScalerFlipAvailModesTag = 0x80200009;
static constexpr uint32_t kSamCtrlAeAvailModesTag = 0x80210001;
static constexpr uint32_t kSamCtrlAfAvailModesTag = 0x80210002;
static constexpr uint32_t kSamCtrlAwbAvailModesTag = 0x80210003;
static constexpr uint32_t kSamCtrlAvailEffectsTag = 0x80210004;
static constexpr uint32_t kSamCtrlAvailFeaturesTag = 0x80210005;
static constexpr uint32_t kSamCtrlColorSpaceAvailModesTag = 0x80210006;
static constexpr uint32_t kSamCtrlLdcAvailModesTag = 0x80210007;
static constexpr uint32_t kSamCtrlMeteringAvailModeTag = 0x80210008;
static constexpr uint32_t kSamCtrlLiveHdrAvailModesTag = 0x80210009;
static constexpr uint32_t kSamUnihalBeautyFaceDetectModesTag = 0x80220001;
static constexpr uint32_t kSamSensorInfoPhysicalTypeTag = 0x80230001;

static const Note13VendorTagDesc kSamsungVendorTagDescs[] = {
    {kSamScalerAvailPreviewConfigsTag, "samsung.android.scaler", "availablePreviewStreamConfigurations", TYPE_INT32},
    {kSamScalerAvailPictureConfigsTag, "samsung.android.scaler", "availablePictureStreamConfigurations", TYPE_INT32},
    {kSamScalerAvailThumbConfigsTag, "samsung.android.scaler", "availableThumbnailStreamConfigurations", TYPE_INT32},
    {kSamScalerAvailVideoConfigsTag, "samsung.android.scaler", "availableVideoConfigurations", TYPE_INT32},
    {kSamScalerAvailMaxDigitalZoomTag, "samsung.android.scaler", "availableMaxDigitalZoom", TYPE_FLOAT},
    {kSamScalerAvailMinDigitalZoomTag, "samsung.android.scaler", "availableMinDigitalZoom", TYPE_FLOAT},
    {kSamScalerAvailMaxDigitalZoomListTag, "samsung.android.scaler", "availableMaxDigitalZoomList", TYPE_FLOAT},
    {kSamScalerAvailMinDigitalZoomListTag, "samsung.android.scaler", "availableMinDigitalZoomList", TYPE_FLOAT},
    {kSamScalerFlipAvailModesTag, "samsung.android.scaler", "flipAvailableModes", TYPE_INT32},
    {kSamCtrlAeAvailModesTag, "samsung.android.control", "aeAvailableModes", TYPE_INT32},
    {kSamCtrlAfAvailModesTag, "samsung.android.control", "afAvailableModes", TYPE_INT32},
    {kSamCtrlAwbAvailModesTag, "samsung.android.control", "awbAvailableModes", TYPE_INT32},
    {kSamCtrlAvailEffectsTag, "samsung.android.control", "availableEffects", TYPE_INT32},
    {kSamCtrlAvailFeaturesTag, "samsung.android.control", "availableFeatures", TYPE_INT32},
    {kSamCtrlColorSpaceAvailModesTag, "samsung.android.control", "colorSpaceAvailableModes", TYPE_INT32},
    {kSamCtrlLdcAvailModesTag, "samsung.android.control", "lensDistortionCorrectionAvailableModes", TYPE_INT32},
    {kSamCtrlMeteringAvailModeTag, "samsung.android.control", "meteringAvailableMode", TYPE_INT32},
    {kSamCtrlLiveHdrAvailModesTag, "samsung.android.control", "liveHdrAvailableModes", TYPE_INT32},
    {kSamUnihalBeautyFaceDetectModesTag, "samsung.android.unihal", "statisticsBeautyAvailableFaceDetectModes", TYPE_INT32},
    {kSamSensorInfoPhysicalTypeTag, "samsung.android.sensor.info", "physicalType", TYPE_INT32},
};

static const uint8_t kLogicalBackPhysicalIds02[] = {'0', '\0', '2', '\0'};

const std::vector<Note13CameraTarget> &GetSamsungExtraTargets(
    const CameraTargetDefaults &defaults) {
  static const std::array<Note13CameraTarget, 2> kPhys = [&]() {
    std::array<Note13CameraTarget, 2> arr = {defaults.baseTargets[0],
                                             defaults.baseTargets[0]};
    arr[0].id = "0";
    arr[0].focalLength = 5.31f;
    arr[1].id = "2";
    arr[1].focalLength = 1.95f;
    return arr;
  }();
  static const std::vector<Note13CameraTarget> kExtras = [&]() {
    std::vector<Note13CameraTarget> out;
    auto addBackAlias = [&](const char *id, bool logical02) {
      Note13CameraTarget t = defaults.baseTargets[0];
      t.id = id;
      if (logical02) {
        if (t.requestCapabilitiesCount < std::size(t.requestCapabilities)) {
          t.requestCapabilities[t.requestCapabilitiesCount++] =
              ANDROID_REQUEST_AVAILABLE_CAPABILITIES_LOGICAL_MULTI_CAMERA;
        }
        t.physicalCameraIds = kLogicalBackPhysicalIds02;
        t.physicalCameraIdCount = std::size(kLogicalBackPhysicalIds02);
        t.physicalTargets = kPhys.data();
        t.physicalTargetCount = kPhys.size();
      }
      out.push_back(t);
    };
    addBackAlias("2", false);
    addBackAlias("3", false);
    addBackAlias("20", true);
    addBackAlias("21", false);
    addBackAlias("23", false);
    addBackAlias("52", false);
    return out;
  }();
  return kExtras;
}

} // namespace

void AppendVendorTagDescs(std::vector<Note13VendorTagDesc> *out) {
  for (const auto &d : kSamsungVendorTagDescs) {
    out->push_back(d);
  }
}

bool CollectExposedCameraTargets(const CameraTargetDefaults &defaults,
                                 std::vector<const Note13CameraTarget *> *out) {
  for (const auto &extra : GetSamsungExtraTargets(defaults)) {
    out->push_back(&extra);
  }
  return false;
}

bool AppendVendorStaticMetadata(camera_metadata_t *metadata,
                                const Note13CameraTarget &target) {
  std::vector<int32_t> previewConfigs;
  previewConfigs.reserve(target.streamSizeCount * 2 * 4);
  AppendExtStreamConfigs(&previewConfigs, kExtImplementationDefined,
                         target.streamSizes, target.streamSizeCount);
  AppendExtStreamConfigs(&previewConfigs, kExtYCbCr420888,
                         target.streamSizes, target.streamSizeCount);

  std::vector<int32_t> pictureConfigs;
  pictureConfigs.reserve((target.jpegSizeCount + target.streamSizeCount) * 4);
  AppendExtStreamConfigs(&pictureConfigs, kExtBlob,
                         target.jpegSizes, target.jpegSizeCount);
  AppendExtStreamConfigs(&pictureConfigs, kExtYCbCr420888,
                         target.streamSizes, target.streamSizeCount);

  const int32_t thumbConfigs[] = {
      kExtYCbCr420888, 512, 384, kExtOutputStream,
      kExtYCbCr420888, 384, 384, kExtOutputStream,
      kExtYCbCr420888, 320, 240, kExtOutputStream,
      kExtYCbCr420888, 192, 144, kExtOutputStream,
  };
  const int32_t videoConfigs[] = {
      3840, 2160, 30, 30, 0, 0,
      1920, 1080, 30, 30, 0, 0,
      1920, 1080, 60, 60, 0, 0,
      1280, 720,  30, 30, 0, 0,
      720,  480,  30, 30, 0, 0,
  };
  const float maxZoom = target.maxDigitalZoom;
  const float minZoom = 1.0f;
  const float maxZoomList[] = {maxZoom};
  const float minZoomList[] = {minZoom};
  const int32_t flipModes[] = {0, 1};
  const int32_t aeModes[] = {0, 1, 2, 3};
  const int32_t afModesBack[] = {0, 1, 2, 3, 4};
  const int32_t afModesFront[] = {0};
  const int32_t awbModes[] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
  const int32_t availEffects[] = {0};
  const int32_t availFeatures[] = {0};
  const int32_t colorSpaceModes[] = {0};
  const int32_t ldcModes[] = {0, 1};
  const int32_t meteringModes[] = {0, 1, 2};
  const int32_t liveHdrModes[] = {0, 1, 2};
  const int32_t beautyFaceModes[] = {0, 1};
  const int32_t physicalType = 0;
  const bool isBack = target.lensFacing == ANDROID_LENS_FACING_BACK;

  return AddMetaEntry(metadata, kSamScalerAvailPreviewConfigsTag, previewConfigs) &&
         AddMetaEntry(metadata, kSamScalerAvailPictureConfigsTag, pictureConfigs) &&
         AddMetaEntry(metadata, kSamScalerAvailThumbConfigsTag, thumbConfigs,
                      std::size(thumbConfigs)) &&
         AddMetaEntry(metadata, kSamScalerAvailVideoConfigsTag, videoConfigs,
                      std::size(videoConfigs)) &&
         AddMetaSingle(metadata, kSamScalerAvailMaxDigitalZoomTag, maxZoom) &&
         AddMetaSingle(metadata, kSamScalerAvailMinDigitalZoomTag, minZoom) &&
         AddMetaEntry(metadata, kSamScalerAvailMaxDigitalZoomListTag, maxZoomList,
                      std::size(maxZoomList)) &&
         AddMetaEntry(metadata, kSamScalerAvailMinDigitalZoomListTag, minZoomList,
                      std::size(minZoomList)) &&
         AddMetaEntry(metadata, kSamScalerFlipAvailModesTag, flipModes,
                      std::size(flipModes)) &&
         AddMetaEntry(metadata, kSamCtrlAeAvailModesTag, aeModes,
                      std::size(aeModes)) &&
         AddMetaEntry(metadata, kSamCtrlAfAvailModesTag,
                      isBack ? afModesBack : afModesFront,
                      isBack ? std::size(afModesBack) : std::size(afModesFront)) &&
         AddMetaEntry(metadata, kSamCtrlAwbAvailModesTag, awbModes,
                      std::size(awbModes)) &&
         AddMetaEntry(metadata, kSamCtrlAvailEffectsTag, availEffects,
                      std::size(availEffects)) &&
         AddMetaEntry(metadata, kSamCtrlAvailFeaturesTag, availFeatures,
                      std::size(availFeatures)) &&
         AddMetaEntry(metadata, kSamCtrlColorSpaceAvailModesTag, colorSpaceModes,
                      std::size(colorSpaceModes)) &&
         AddMetaEntry(metadata, kSamCtrlLdcAvailModesTag, ldcModes,
                      std::size(ldcModes)) &&
         AddMetaEntry(metadata, kSamCtrlMeteringAvailModeTag, meteringModes,
                      std::size(meteringModes)) &&
         AddMetaEntry(metadata, kSamCtrlLiveHdrAvailModesTag, liveHdrModes,
                      std::size(liveHdrModes)) &&
         AddMetaEntry(metadata, kSamUnihalBeautyFaceDetectModesTag, beautyFaceModes,
                      std::size(beautyFaceModes)) &&
         AddMetaSingle(metadata, kSamSensorInfoPhysicalTypeTag, physicalType);
}

void AppendVendorCharacteristicKeys(std::vector<int32_t> *keys,
                                    const Note13CameraTarget &) {
  for (const auto &d : kSamsungVendorTagDescs) {
    keys->push_back(static_cast<int32_t>(d.tagId));
  }
}

bool AppendVendorDefaultRequest(camera_metadata_t *,
                                const Note13CameraTarget &) {
  return true;
}

void RegisterVendorExtraServices() {}

} // namespace cdh::camera
