// android.hardware.camera 厂商扩展钩子接口：
//   - default：无厂商分支（CDH_CAMERA_HAS_VENDOR_EXT 未定义），所有钩子为空内联；
//   - pixel / oplus / samsung / meizu / honor / vivo / nothing / motorola / nubia / unisoc：
//     各自独立位于 src/<variant>/，由 modules/android.hardware.camera.json 的 variants 按需编入。
#pragma once

#include <system/camera_metadata.h>

#include <cstddef>
#include <cstdint>
#include <vector>

#ifndef CDH_NOTE13_VENDOR_TAG_DESC_DEFINED
#define CDH_NOTE13_VENDOR_TAG_DESC_DEFINED
struct Note13VendorTagDesc {
  uint32_t tagId;
  const char *section;
  const char *tagName;
  int tagType;
};
#endif

struct Note13CameraTarget {
  const char *id;
  uint8_t lensFacing;
  int32_t sensorOrientation;
  uint8_t flashAvailable;
  int32_t pixelWidth;
  int32_t pixelHeight;
  float physicalWidth;
  float physicalHeight;
  float lensDistortion[5];
  float lensIntrinsicCalibration[5];
  float lensPoseRotation[4];
  float lensPoseTranslation[3];
  float aperture;
  float focalLength;
  uint8_t opticalStabilization[2];
  size_t opticalStabilizationCount;
  float maxDigitalZoom;
  uint8_t colorFilterArrangement;
  int64_t minExposureTime;
  int64_t maxFrameDuration;
  int32_t minSensitivity;
  int32_t maxSensitivity;
  int32_t maxRegions[3];
  uint8_t requestCapabilities[16];
  size_t requestCapabilitiesCount;
  const int32_t *jpegSizes;
  size_t jpegSizeCount;
  const int32_t *streamSizes;
  size_t streamSizeCount;
  const int32_t *highSpeedVideo;
  size_t highSpeedVideoCount;
  const uint8_t *physicalCameraIds;
  size_t physicalCameraIdCount;
  const Note13CameraTarget *physicalTargets;
  size_t physicalTargetCount;
};

namespace cdh::camera {

struct CameraTargetDefaults {
  const Note13CameraTarget *baseTargets;
  size_t baseTargetCount;
  const int32_t *backHighSpeedVideo;
  size_t backHighSpeedVideoCount;
  const int32_t *frontJpegSizes;
  size_t frontJpegSizeCount;
  const int32_t *frontStreamSizes;
  size_t frontStreamSizeCount;
};

static constexpr int32_t kExtOutputStream = 0;
static constexpr int32_t kExtBlob = 0x21;
static constexpr int32_t kExtImplementationDefined = 0x22;
static constexpr int32_t kExtYCbCr420888 = 0x23;

template <typename T>
inline bool AddMetaEntry(camera_metadata_t *metadata, uint32_t tag,
                         const T *values, size_t count) {
  return add_camera_metadata_entry(metadata, tag, values, count) == 0;
}

template <typename T>
inline bool AddMetaEntry(camera_metadata_t *metadata, uint32_t tag,
                         const std::vector<T> &values) {
  return !values.empty() &&
         AddMetaEntry(metadata, tag, values.data(), values.size());
}

template <typename T>
inline bool AddMetaSingle(camera_metadata_t *metadata, uint32_t tag, T value) {
  return AddMetaEntry(metadata, tag, &value, 1);
}

inline void AppendExtStreamConfigs(std::vector<int32_t> *out, int32_t format,
                                   const int32_t *sizes, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    out->push_back(format);
    out->push_back(sizes[i * 2]);
    out->push_back(sizes[i * 2 + 1]);
    out->push_back(kExtOutputStream);
  }
}

#if CDH_CAMERA_HAS_VENDOR_EXT
void AppendVendorTagDescs(std::vector<Note13VendorTagDesc> *out);
// 返回 true 表示变体完全接管暴露的相机目标列表；返回 false 表示在默认 kTargets 后追加。
bool CollectExposedCameraTargets(const CameraTargetDefaults &defaults,
                                 std::vector<const Note13CameraTarget *> *out);
bool AppendVendorStaticMetadata(camera_metadata_t *metadata,
                                const Note13CameraTarget &target);
void AppendVendorCharacteristicKeys(std::vector<int32_t> *keys,
                                    const Note13CameraTarget &target);
bool AppendVendorDefaultRequest(camera_metadata_t *metadata,
                                const Note13CameraTarget &target);
void RegisterVendorExtraServices();
#else
inline void AppendVendorTagDescs(std::vector<Note13VendorTagDesc> *) {}
inline bool CollectExposedCameraTargets(const CameraTargetDefaults &,
                                        std::vector<const Note13CameraTarget *> *) {
  return false;
}
inline bool AppendVendorStaticMetadata(camera_metadata_t *,
                                       const Note13CameraTarget &) {
  return true;
}
inline void AppendVendorCharacteristicKeys(std::vector<int32_t> *,
                                           const Note13CameraTarget &) {}
inline bool AppendVendorDefaultRequest(camera_metadata_t *,
                                       const Note13CameraTarget &) {
  return true;
}
inline void RegisterVendorExtraServices() {}
#endif

#if CDH_CAMERA_UNISOC
void AppendVendorRequestAndResultKeys(std::vector<uint32_t> *requestKeys,
                                      std::vector<uint32_t> *resultKeys,
                                      const Note13CameraTarget &target);
bool AppendVendorResultMetadata(camera_metadata_t *metadata,
                                const Note13CameraTarget &target);
#else
inline void AppendVendorRequestAndResultKeys(std::vector<uint32_t> *,
                                             std::vector<uint32_t> *,
                                             const Note13CameraTarget &) {}
inline bool AppendVendorResultMetadata(camera_metadata_t *,
                                       const Note13CameraTarget &) {
  return true;
}
#endif

} // namespace cdh::camera
