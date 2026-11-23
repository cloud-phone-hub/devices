// android.hardware.camera 的 nothing 变体（Nothing OS 180 项零前缀碰撞 Vendor Tags）。
// 只在 --variant nothing（-DCDH_CAMERA_HAS_VENDOR_EXT=1 -DCDH_CAMERA_NOTHING=1）时编入。
#include "camera_vendor_ext.h"

namespace cdh::camera {
namespace {
#include "cdh_nothing_vendor_tags.inc"
} // namespace

void AppendVendorTagDescs(std::vector<Note13VendorTagDesc> *out) {
  for (const auto &d : kNothingVendorTagDescs) {
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

bool AppendVendorDefaultRequest(camera_metadata_t *,
                                const Note13CameraTarget &) {
  return true;
}

void RegisterVendorExtraServices() {}

} // namespace cdh::camera
