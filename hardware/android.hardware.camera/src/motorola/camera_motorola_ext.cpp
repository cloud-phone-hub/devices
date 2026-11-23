// android.hardware.camera 的 motorola 变体（154 项 Vendor Tags + supports_mcf=0 + sensor.info.name）。
// 只在 --variant motorola（-DCDH_CAMERA_HAS_VENDOR_EXT=1 -DCDH_CAMERA_MOTOROLA=1）时编入。
#include "camera_vendor_ext.h"

#include <sys/system_properties.h>

#include <cctype>
#include <string>

namespace cdh::camera {
namespace {
#include "cdh_motorola_vendor_tags.inc"

bool PropContainsIgnoreCase(const char *propName, const char *needle) {
  char buf[PROP_VALUE_MAX] = {};
  if (__system_property_get(propName, buf) <= 0) {
    return false;
  }
  std::string hay(buf);
  std::string sub(needle);
  for (char &c : hay) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  for (char &c : sub) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return hay.find(sub) != std::string::npos;
}

} // namespace

void AppendVendorTagDescs(std::vector<Note13VendorTagDesc> *out) {
  for (const auto &d : kMotorolaVendorTagDescs) {
    out->push_back(d);
  }
}

bool CollectExposedCameraTargets(const CameraTargetDefaults &,
                                 std::vector<const Note13CameraTarget *> *) {
  return false;
}

bool AppendVendorStaticMetadata(camera_metadata_t *metadata,
                                const Note13CameraTarget &target) {
  const uint8_t zeroByte = 0;
  const bool isAito =
      PropContainsIgnoreCase("ro.product.vendor.device", "aito") ||
      PropContainsIgnoreCase("ro.product.device", "aito") ||
      PropContainsIgnoreCase("ro.product.system.device", "aito");
  std::string sensorStr;
  if (isAito) {
    sensorStr = (target.lensFacing == ANDROID_LENS_FACING_BACK)
                    ? "mot_aito_s5kgn8_mipi_raw"
                    : "mot_aito_ov32b_mipi_raw";
  } else {
    sensorStr = (target.lensFacing == ANDROID_LENS_FACING_BACK)
                    ? "mot_rear_main_mipi_raw"
                    : "mot_front_main_mipi_raw";
  }
  std::vector<uint8_t> sensorBytes(sensorStr.begin(), sensorStr.end());
  sensorBytes.push_back(0);
  return AddMetaEntry(metadata, kMotoSupportsMcfTag, &zeroByte, 1) &&
         AddMetaEntry(metadata, kMotoSupportsReprocTag, &zeroByte, 1) &&
         AddMetaEntry(metadata, kMotoSupportIspHidlTag, &zeroByte, 1) &&
         AddMetaEntry(metadata, kMotoSensorInfoNameTag, sensorBytes);
}

void AppendVendorCharacteristicKeys(std::vector<int32_t> *keys,
                                    const Note13CameraTarget &) {
  keys->push_back(static_cast<int32_t>(kMotoSupportsMcfTag));
  keys->push_back(static_cast<int32_t>(kMotoSupportsReprocTag));
  keys->push_back(static_cast<int32_t>(kMotoSupportIspHidlTag));
  keys->push_back(static_cast<int32_t>(kMotoSensorInfoNameTag));
}

bool AppendVendorDefaultRequest(camera_metadata_t *,
                                const Note13CameraTarget &) {
  return true;
}

void RegisterVendorExtraServices() {}

} // namespace cdh::camera
