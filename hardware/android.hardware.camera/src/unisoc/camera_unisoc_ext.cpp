// android.hardware.camera 的 unisoc 变体（展锐默认请求模板 0x8000000A 预置）。
// 只在 --variant unisoc（-DCDH_CAMERA_HAS_VENDOR_EXT=1 -DCDH_CAMERA_UNISOC=1）时编入。
#include "camera_vendor_ext.h"

namespace cdh::camera {

void AppendVendorTagDescs(std::vector<Note13VendorTagDesc> *) {}

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
  // 展锐 libunisoccameraservice.so 的 UnisocCamera3Device::getSlowMotion 硬编码查询
  // tagId=0x8000000A 且未判空直接解引用 entry.data.u8[0]，预置为 0 防止 SIGSEGV。
  const int32_t sprdSlowMotionDefault = 0;
  return AddMetaSingle(metadata, 0x8000000Au, sprdSlowMotionDefault);
}

void RegisterVendorExtraServices() {}

} // namespace cdh::camera
