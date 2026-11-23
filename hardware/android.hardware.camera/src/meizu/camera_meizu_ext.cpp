// android.hardware.camera 的 meizu 变体（Flyme 逻辑后摄 "4" 与辅摄 "2"/"3"）。
// 只在 --variant meizu（-DCDH_CAMERA_HAS_VENDOR_EXT=1 -DCDH_CAMERA_MEIZU=1）时编入。
#include "camera_vendor_ext.h"

#include <array>
#include <iterator>

namespace cdh::camera {
namespace {

static const uint8_t kLogicalBackPhysicalIds02[] = {'0', '\0', '2', '\0'};

const std::vector<Note13CameraTarget> &GetMeizuExtraTargets(
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
    addBackAlias("4", true);
    return out;
  }();
  return kExtras;
}

} // namespace

void AppendVendorTagDescs(std::vector<Note13VendorTagDesc> *) {}

bool CollectExposedCameraTargets(const CameraTargetDefaults &defaults,
                                 std::vector<const Note13CameraTarget *> *out) {
  for (const auto &extra : GetMeizuExtraTargets(defaults)) {
    out->push_back(&extra);
  }
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
