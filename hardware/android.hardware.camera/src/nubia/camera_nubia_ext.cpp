// android.hardware.camera 的 nubia 变体（RedMagic / Nubia 199 项 Vendor Tags + Feature1Mode=0）。
// 只在 --variant nubia（-DCDH_CAMERA_HAS_VENDOR_EXT=1 -DCDH_CAMERA_NUBIA=1）时编入。
#include "camera_vendor_ext.h"

#include <sys/system_properties.h>

#include <cstring>

namespace cdh::camera {
namespace {
#include "cdh_nubia_vendor_tags.inc"
} // namespace

void AppendVendorTagDescs(std::vector<Note13VendorTagDesc> *out) {
  for (const auto &d : kNubiaVendorTagDescs) {
    out->push_back(d);
  }
}

bool CollectExposedCameraTargets(const CameraTargetDefaults &,
                                 std::vector<const Note13CameraTarget *> *) {
  return false;
}

bool AppendVendorStaticMetadata(camera_metadata_t *,
                                const Note13CameraTarget &) {
  return true;
}

void AppendVendorCharacteristicKeys(std::vector<int32_t> *,
                                    const Note13CameraTarget &) {}

bool AppendVendorDefaultRequest(camera_metadata_t *metadata,
                                const Note13CameraTarget &) {
  const uint8_t zeroByte = 0;
  if (!AddMetaSingle(metadata, kNubiaQcomTuningFeature1ModeTag, zeroByte)) {
    return false;
  }
  // 兼顾展锐 Nubia 机型（如 P616F05）：UnisocCamera3Device::getSlowMotion 硬编码查询 0x8000000A。
  char camHw[PROP_VALUE_MAX] = {};
  if (__system_property_get("ro.hardware.camera", camHw) > 0 &&
      std::strcmp(camHw, "unisoc") == 0) {
    const int32_t sprdSlowMotionDefault = 0;
    return AddMetaSingle(metadata, 0x8000000Au, sprdSlowMotionDefault);
  }
  return true;
}

void RegisterVendorExtraServices() {}

} // namespace cdh::camera
