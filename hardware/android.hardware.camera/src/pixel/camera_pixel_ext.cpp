// android.hardware.camera 的 pixel 变体（Google Pixel 双后摄逻辑多摄拓扑）。
// 只在 --variant pixel（-DCDH_CAMERA_HAS_VENDOR_EXT=1 -DCDH_CAMERA_PIXEL=1）时编入。
#include "camera_vendor_ext.h"

#include <array>
#include <iterator>

namespace cdh::camera {
namespace {

static const int32_t kPixelWideJpegSizes[] = {
    4064, 3072, 4000, 3000, 4000, 2256, 3840, 2160, 3264, 2448, 2560, 1920,
    1920, 1440, 1920, 1080, 1440, 1080, 1280, 960,  1280, 720,  640,  480,
};

static const int32_t kPixelWideStreamSizes[] = {
    4064, 3072, 4000, 3000, 4000, 2256, 3840, 2160, 3264, 2448, 2560, 1920,
    2560, 1440, 2400, 1080, 2304, 1728, 2160, 1080, 1920, 1440, 1920, 1080,
    1600, 1200, 1440, 1080, 1280, 960,  1280, 720,  1080, 1080, 960,  720,
    720,  480,  640,  480,  640,  360,  352,  288,  320,  240,  176,  144,
};

static const int32_t kPixelUwJpegSizes[] = {
    4032, 3016, 4000, 3000, 3840, 2160, 3264, 2448, 2560, 1920,
    1920, 1440, 1920, 1080, 1440, 1080, 1280, 960,  1280, 720,  640,  480,
};

static const int32_t kPixelUwStreamSizes[] = {
    4032, 3016, 4000, 3000, 3840, 2160, 3264, 2448, 2560, 1920, 2560, 1440,
    2400, 1080, 2304, 1728, 2160, 1080, 1920, 1440, 1920, 1080, 1600, 1200,
    1440, 1080, 1280, 960,  1280, 720,  1080, 1080, 960,  720,  720,  480,
    640,  480,  640,  360,  352,  288,  320,  240,  176,  144,
};

static const uint8_t kPixelBackPhysicalCameraIds[] = {'2', '\0', '3', '\0'};

const std::array<Note13CameraTarget, 2> &GetPixelTargets(
    const CameraTargetDefaults &defaults) {
  static const std::array<Note13CameraTarget, 2> kPhys = [&]() {
    return std::array<Note13CameraTarget, 2>{
        Note13CameraTarget{"2",
                           ANDROID_LENS_FACING_BACK,
                           90,
                           ANDROID_FLASH_INFO_AVAILABLE_TRUE,
                           4064,
                           3072,
                           9.8304f,
                           7.3728f,
                           {0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
                           {2764.8f, 2764.8f, 2032.0f, 1536.0f, 0.0f},
                           {0.0f, 0.0f, 0.0f, 1.0f},
                           {0.0f, 0.0f, 0.0f},
                           1.85f,
                           6.81f,
                           {0, 1},
                           2,
                           10.0f,
                           ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_GBRG,
                           100000LL,
                           30000000000LL,
                           50,
                           12800,
                           {1, 0, 1},
                           {0, 1, 2, 5, 3, 6, 7, 9},
                           8,
                           kPixelWideJpegSizes,
                           std::size(kPixelWideJpegSizes) / 2,
                           kPixelWideStreamSizes,
                           std::size(kPixelWideStreamSizes) / 2,
                           defaults.backHighSpeedVideo,
                           defaults.backHighSpeedVideoCount,
                           nullptr,
                           0,
                           nullptr,
                           0},
        Note13CameraTarget{"3",
                           ANDROID_LENS_FACING_BACK,
                           90,
                           ANDROID_FLASH_INFO_AVAILABLE_TRUE,
                           4032,
                           3016,
                           5.60f,
                           4.20f,
                           {0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
                           {1680.0f, 1680.0f, 2016.0f, 1508.0f, 0.0f},
                           {0.0f, 0.0f, 0.0f, 1.0f},
                           {0.014f, 0.0f, 0.0f},
                           2.2f,
                           2.35f,
                           {0},
                           1,
                           10.0f,
                           ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_GBRG,
                           100000LL,
                           30000000000LL,
                           50,
                           6400,
                           {1, 0, 1},
                           {0, 1, 2, 5, 3, 6, 7},
                           7,
                           kPixelUwJpegSizes,
                           std::size(kPixelUwJpegSizes) / 2,
                           kPixelUwStreamSizes,
                           std::size(kPixelUwStreamSizes) / 2,
                           nullptr,
                           0,
                           nullptr,
                           0,
                           nullptr,
                           0},
    };
  }();
  static const std::array<Note13CameraTarget, 2> kTargets = [&]() {
    return std::array<Note13CameraTarget, 2>{
        Note13CameraTarget{"0",
                           ANDROID_LENS_FACING_BACK,
                           90,
                           ANDROID_FLASH_INFO_AVAILABLE_TRUE,
                           4064,
                           3072,
                           9.8304f,
                           7.3728f,
                           {0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
                           {2764.8f, 2764.8f, 2032.0f, 1536.0f, 0.0f},
                           {0.0f, 0.0f, 0.0f, 1.0f},
                           {0.0f, 0.0f, 0.0f},
                           1.85f,
                           6.81f,
                           {0, 1},
                           2,
                           10.0f,
                           ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_GBRG,
                           100000LL,
                           30000000000LL,
                           50,
                           12800,
                           {1, 0, 1},
                           {0, 1, 2, 5, 3, 6, 7, 9, 11},
                           9,
                           kPixelWideJpegSizes,
                           std::size(kPixelWideJpegSizes) / 2,
                           kPixelWideStreamSizes,
                           std::size(kPixelWideStreamSizes) / 2,
                           defaults.backHighSpeedVideo,
                           defaults.backHighSpeedVideoCount,
                           kPixelBackPhysicalCameraIds,
                           std::size(kPixelBackPhysicalCameraIds),
                           kPhys.data(),
                           kPhys.size()},
        Note13CameraTarget{"1",
                           ANDROID_LENS_FACING_FRONT,
                           270,
                           ANDROID_FLASH_INFO_AVAILABLE_FALSE,
                           2304,
                           1728,
                           4.64f,
                           3.48f,
                           {0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
                           {1618.56f, 1618.56f, 1152.0f, 864.0f, 0.0f},
                           {0.0f, 0.0f, 0.0f, 1.0f},
                           {0.0f, 0.0f, 0.0f},
                           2.0f,
                           2.74f,
                           {0},
                           1,
                           10.0f,
                           ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_RGGB,
                           100000LL,
                           278443000LL,
                           50,
                           3200,
                           {1, 0, 0},
                           {0, 1, 2, 5, 3, 6, 7},
                           7,
                           defaults.frontJpegSizes,
                           defaults.frontJpegSizeCount,
                           defaults.frontStreamSizes,
                           defaults.frontStreamSizeCount,
                           nullptr,
                           0,
                           nullptr,
                           0,
                           nullptr,
                           0},
    };
  }();
  return kTargets;
}

} // namespace

void AppendVendorTagDescs(std::vector<Note13VendorTagDesc> *) {}

bool CollectExposedCameraTargets(const CameraTargetDefaults &defaults,
                                 std::vector<const Note13CameraTarget *> *out) {
  for (const auto &target : GetPixelTargets(defaults)) {
    out->push_back(&target);
  }
  return true;
}

bool AppendVendorStaticMetadata(camera_metadata_t *,
                                const Note13CameraTarget &) {
  return true;
}

void AppendVendorCharacteristicKeys(std::vector<int32_t> *keys,
                                    const Note13CameraTarget &) {
  keys->push_back(ANDROID_LENS_POSE_REFERENCE);
  keys->push_back(ANDROID_LENS_POSE_ROTATION);
  keys->push_back(ANDROID_LENS_POSE_TRANSLATION);
  keys->push_back(ANDROID_LENS_INTRINSIC_CALIBRATION);
  keys->push_back(ANDROID_LENS_DISTORTION);
  keys->push_back(ANDROID_DEPTH_AVAILABLE_DEPTH_STREAM_CONFIGURATIONS);
  keys->push_back(ANDROID_DEPTH_AVAILABLE_DEPTH_MIN_FRAME_DURATIONS);
  keys->push_back(ANDROID_DEPTH_AVAILABLE_DEPTH_STALL_DURATIONS);
  keys->push_back(ANDROID_DEPTH_DEPTH_IS_EXCLUSIVE);
}

bool AppendVendorDefaultRequest(camera_metadata_t *,
                                const Note13CameraTarget &) {
  return true;
}

void RegisterVendorExtraServices() {}

} // namespace cdh::camera
