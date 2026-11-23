// android.hardware.camera 的 oplus 变体（OPPO / OnePlus / realme 逻辑多摄与 cameraid.type）。
// 只在 --variant oplus（-DCDH_CAMERA_HAS_VENDOR_EXT=1 -DCDH_CAMERA_OPLUS=1）时编入。
#include "camera_vendor_ext.h"

#include <array>
#include <cstring>
#include <iterator>

namespace cdh::camera {
namespace {

static constexpr uint32_t kOplusSupportedCameraIdTypeTag = 0x81170003;
static const uint8_t kOplusBackPhysicalIds[] = {'3', '\0', '2', '\0'};

uint8_t OplusCameraIdType(const Note13CameraTarget &target) {
  if (std::strcmp(target.id, "0") == 0) return 12;
  if (std::strcmp(target.id, "1") == 0) return 1;
  if (std::strcmp(target.id, "2") == 0) return 0;
  if (std::strcmp(target.id, "3") == 0) return 2;
  return 0;
}

const std::vector<Note13CameraTarget> &GetOplusTargets(
    const CameraTargetDefaults &defaults) {
  static const std::array<Note13CameraTarget, 2> kPhys = [&]() {
    std::array<Note13CameraTarget, 2> arr = {defaults.baseTargets[0],
                                             defaults.baseTargets[0]};
    arr[0].id = "2";
    arr[0].focalLength = 5.59f;
    arr[1].id = "3";
    arr[1].focalLength = 1.68f;
    return arr;
  }();
  static const std::vector<Note13CameraTarget> kOplus = [&]() {
    std::vector<Note13CameraTarget> out;
    Note13CameraTarget back = defaults.baseTargets[0];
    if (back.requestCapabilitiesCount < std::size(back.requestCapabilities)) {
      back.requestCapabilities[back.requestCapabilitiesCount++] =
          ANDROID_REQUEST_AVAILABLE_CAPABILITIES_LOGICAL_MULTI_CAMERA;
    }
    back.physicalCameraIds = kOplusBackPhysicalIds;
    back.physicalCameraIdCount = std::size(kOplusBackPhysicalIds);
    back.physicalTargets = kPhys.data();
    back.physicalTargetCount = kPhys.size();
    out.push_back(back);
    out.push_back(defaults.baseTargets[1]);
    out.push_back(kPhys[0]);
    out.push_back(kPhys[1]);
    return out;
  }();
  return kOplus;
}

} // namespace

void AppendVendorTagDescs(std::vector<Note13VendorTagDesc> *out) {
  out->push_back({kOplusSupportedCameraIdTypeTag, "com.oplus",
                  "supported.cameraid.type", TYPE_BYTE});
}

bool CollectExposedCameraTargets(const CameraTargetDefaults &defaults,
                                 std::vector<const Note13CameraTarget *> *out) {
  for (const auto &target : GetOplusTargets(defaults)) {
    out->push_back(&target);
  }
  return true;
}

bool AppendVendorStaticMetadata(camera_metadata_t *metadata,
                                const Note13CameraTarget &target) {
  const uint8_t cameraIdType = OplusCameraIdType(target);
  return AddMetaEntry(metadata, kOplusSupportedCameraIdTypeTag, &cameraIdType, 1);
}

void AppendVendorCharacteristicKeys(std::vector<int32_t> *keys,
                                    const Note13CameraTarget &) {
  keys->push_back(static_cast<int32_t>(kOplusSupportedCameraIdTypeTag));
}

bool AppendVendorDefaultRequest(camera_metadata_t *,
                                const Note13CameraTarget &) {
  return true;
}

void RegisterVendorExtraServices() {}

} // namespace cdh::camera
