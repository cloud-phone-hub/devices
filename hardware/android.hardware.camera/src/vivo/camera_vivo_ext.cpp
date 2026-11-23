// android.hardware.camera 的 vivo 变体（OriginOS 23 项 Vendor Tags + IVivoCameraProvider AIDL 服务）。
// 只在 --variant vivo（-DCDH_CAMERA_HAS_VENDOR_EXT=1 -DCDH_CAMERA_VIVO=1）时编入。
#include "camera_vendor_ext.h"

#include <android/binder_ibinder.h>
#include <android/binder_manager.h>
#include <android/binder_parcel.h>
#include <android/binder_status.h>
#include <android/log.h>

#include <cstring>
#include <iterator>
#include <string>

extern "C" {
void AIBinder_markVintfStability(AIBinder *binder);
}

namespace cdh::camera {
namespace {

constexpr const char *kLogTag = "cdh-camera-vivo";

static constexpr uint32_t kVivoChiCameraTypeTag = 0x80400001;
static constexpr uint32_t kVivoChiLargeSizeThumbTag = 0x80400002;
static constexpr uint32_t kVivoChiSensorSizeTag = 0x80400003;
static constexpr uint32_t kVivoFeedbackSensorNameTag = 0x80410001;
static constexpr uint32_t kVivoSensorFocallen35mmTag = 0x80420001;
static constexpr uint32_t kVivoImageSizesTag = 0x80430001;
static constexpr uint32_t kVivoSensorModeSizeListTag = 0x80430002;
static constexpr uint32_t kVivoRemosaicTypeTag = 0x80430003;
static constexpr uint32_t kVivoHalVersionTag = 0x80430004;
static constexpr uint32_t kVivoDisableReconfigTag = 0x80430005;
static constexpr uint32_t kVivoCurrentModeVersionTag = 0x80430006;
static constexpr uint32_t kVivoJpegNeedUpscaleTag = 0x80430007;
static constexpr uint32_t kVivoIsNv21PlatformTag = 0x80430008;
static constexpr uint32_t kVivoSupportedMaxCfaPatternTag = 0x80430009;
static constexpr uint32_t kVivoProExpTimeMinTag = 0x8043000a;
static constexpr uint32_t kVivoProExpTimeMaxTag = 0x8043000b;
static constexpr uint32_t kVivoProIsoMinTag = 0x8043000c;
static constexpr uint32_t kVivoProIsoMaxTag = 0x8043000d;
static constexpr uint32_t kVivoLogicalMcPhysicalIdsTag = 0x8043000e;
static constexpr uint32_t kVivoPhysicalCamCombinationTag = 0x8043000f;
static constexpr uint32_t kVivoMultiCamTypeMapSessionIdTag = 0x80430010;
static constexpr uint32_t kVivoMultiCamBokehSensorSizeTag = 0x80430011;
static constexpr uint32_t kVivoSensorNotSetStreamPhysIdTag = 0x80440001;

static const Note13VendorTagDesc kVivoVendorTagDescs[] = {
    {kVivoChiCameraTypeTag, "com.vivo.chi.override", "CameraType", TYPE_BYTE},
    {kVivoChiLargeSizeThumbTag, "com.vivo.chi.override", "LargeSizeThumbnail", TYPE_BYTE},
    {kVivoChiSensorSizeTag, "com.vivo.chi.override", "SensorSize", TYPE_INT32},
    {kVivoFeedbackSensorNameTag, "vivo.feedback", "sensor_name", TYPE_BYTE},
    {kVivoSensorFocallen35mmTag, "vivo.camera.sensor", "focallen.35mm", TYPE_FLOAT},
    {kVivoImageSizesTag, "com.vivo", "image_sizes", TYPE_INT32},
    {kVivoSensorModeSizeListTag, "com.vivo", "sensorModeSizeList", TYPE_INT32},
    {kVivoRemosaicTypeTag, "com.vivo", "RemosaicType", TYPE_INT32},
    {kVivoHalVersionTag, "com.vivo", "halversion", TYPE_INT64},
    {kVivoDisableReconfigTag, "com.vivo", "disablereconfig", TYPE_INT32},
    {kVivoCurrentModeVersionTag, "com.vivo", "CurrentModeVersion", TYPE_INT32},
    {kVivoJpegNeedUpscaleTag, "com.vivo", "JPEGNeedUpscale", TYPE_INT32},
    {kVivoIsNv21PlatformTag, "com.vivo", "isNV21Platform", TYPE_INT32},
    {kVivoSupportedMaxCfaPatternTag, "com.vivo", "SupportedMaxCFAPattern", TYPE_INT32},
    {kVivoProExpTimeMinTag, "com.vivo", "pro_exptimeMin", TYPE_INT64},
    {kVivoProExpTimeMaxTag, "com.vivo", "pro_exptimeMax", TYPE_INT64},
    {kVivoProIsoMinTag, "com.vivo", "pro_isoMin", TYPE_INT32},
    {kVivoProIsoMaxTag, "com.vivo", "pro_isoMax", TYPE_INT32},
    {kVivoLogicalMcPhysicalIdsTag, "com.vivo", "logicalMCPhysicalIds", TYPE_BYTE},
    {kVivoPhysicalCamCombinationTag, "com.vivo", "PhysicalCameraCombination", TYPE_BYTE},
    {kVivoMultiCamTypeMapSessionIdTag, "com.vivo", "MultiCameraTypeMapSessionId", TYPE_BYTE},
    {kVivoMultiCamBokehSensorSizeTag, "com.vivo", "MultiCameraBokehSensorSize", TYPE_BYTE},
    {kVivoSensorNotSetStreamPhysIdTag, "vivo.control", "sensorNotSetStreamPhysicalId", TYPE_BYTE},
};

constexpr const char *kVivoCameraProviderDesc =
    "vendor.vivo.hardware.vivocameraprovider.IVivoCameraProvider";
constexpr const char *kVivoCameraProviderInstance =
    "vendor.vivo.hardware.vivocameraprovider.IVivoCameraProvider/default";
constexpr const char *kVivoCameraProviderHash =
    "eee68c0710662f59503dcfb62464d1c7139183a0";

constexpr const char *kVivoCameraDeviceDesc =
    "vendor.vivo.hardware.camera.vivodevice.IVivoCameraDevice";
constexpr const char *kVivoCameraDeviceHash =
    "c351d76052cc9e032c73327d70efe585464cc31f";

constexpr const char *kVivoCameraVifDesc =
    "vendor.vivo.hardware.camera.vif.ICameraVIF";
constexpr const char *kVivoCameraVifHash =
    "eb565c2632918fe598f93a93288030332232108f";

constexpr transaction_code_t kAidlGetInterfaceVersion = 16777215;
constexpr transaction_code_t kAidlGetInterfaceHash = 16777214;

AIBinder *gVivoCameraProviderBinder = nullptr;
AIBinder *gVivoCameraDeviceBinder = nullptr;
AIBinder *gVivoCameraVifBinder = nullptr;

void *vivoStubOnCreate(void *args) { return args; }
void vivoStubOnDestroy(void *) {}

binder_status_t writeStatusOk(AParcel *out) {
  AStatus *ok = AStatus_newOk();
  binder_status_t st = AParcel_writeStatusHeader(out, ok);
  AStatus_delete(ok);
  return st;
}

binder_status_t onVivoCameraDeviceTransact(AIBinder *, transaction_code_t code,
                                           const AParcel *, AParcel *out) {
  switch (code) {
    case 1:
    case kAidlGetInterfaceVersion: {
      binder_status_t st = writeStatusOk(out);
      return st == STATUS_OK ? AParcel_writeInt32(out, 1) : st;
    }
    case kAidlGetInterfaceHash: {
      binder_status_t st = writeStatusOk(out);
      return st == STATUS_OK
                 ? AParcel_writeString(out, kVivoCameraDeviceHash,
                                       static_cast<int32_t>(
                                           std::strlen(kVivoCameraDeviceHash)))
                 : st;
    }
    case 2:
    case 3:
    case 4:
    case 7:
      return writeStatusOk(out);
    case 5: {
      binder_status_t st = writeStatusOk(out);
      return st == STATUS_OK ? AParcel_writeInt32(out, 0) : st;
    }
    case 6: {
      binder_status_t st = writeStatusOk(out);
      return st == STATUS_OK ? AParcel_writeStrongBinder(out, nullptr) : st;
    }
    default:
      return writeStatusOk(out);
  }
}

binder_status_t onVivoCameraVifTransact(AIBinder *, transaction_code_t code,
                                        const AParcel *, AParcel *out) {
  switch (code) {
    case 1:
    case kAidlGetInterfaceVersion: {
      binder_status_t st = writeStatusOk(out);
      return st == STATUS_OK ? AParcel_writeInt32(out, 1) : st;
    }
    case kAidlGetInterfaceHash: {
      binder_status_t st = writeStatusOk(out);
      return st == STATUS_OK
                 ? AParcel_writeString(out, kVivoCameraVifHash,
                                       static_cast<int32_t>(
                                           std::strlen(kVivoCameraVifHash)))
                 : st;
    }
    case 3: {
      binder_status_t st = writeStatusOk(out);
      return st == STATUS_OK ? AParcel_writeStrongBinder(out, nullptr) : st;
    }
    default:
      return writeStatusOk(out);
  }
}

binder_status_t onVivoCameraProviderTransact(AIBinder *,
                                             transaction_code_t code,
                                             const AParcel *, AParcel *out) {
  switch (code) {
    case 1:
    case kAidlGetInterfaceVersion: {
      binder_status_t st = writeStatusOk(out);
      return st == STATUS_OK ? AParcel_writeInt32(out, 1) : st;
    }
    case kAidlGetInterfaceHash: {
      binder_status_t st = writeStatusOk(out);
      return st == STATUS_OK
                 ? AParcel_writeString(out, kVivoCameraProviderHash,
                                       static_cast<int32_t>(
                                           std::strlen(kVivoCameraProviderHash)))
                 : st;
    }
    case 2:
    case 4: {
      binder_status_t st = writeStatusOk(out);
      return st == STATUS_OK
                 ? AParcel_writeStrongBinder(out, gVivoCameraVifBinder)
                 : st;
    }
    case 3:
    case 5: {
      binder_status_t st = writeStatusOk(out);
      return st == STATUS_OK
                 ? AParcel_writeStrongBinder(out, gVivoCameraDeviceBinder)
                 : st;
    }
    case 6:
    case 7:
    case 8:
    case 10:
    case 12:
    case 14:
    case 26:
    case 27:
    case 29: {
      binder_status_t st = writeStatusOk(out);
      return st == STATUS_OK ? AParcel_writeInt32(out, 0) : st;
    }
    case 9:
    case 21:
    case 22:
    case 23:
    case 24:
    case 25: {
      binder_status_t st = writeStatusOk(out);
      return st == STATUS_OK ? AParcel_writeString(out, "", 0) : st;
    }
    case 16:
    case 17:
    case 18:
    case 19:
    case 20: {
      binder_status_t st = writeStatusOk(out);
      return st == STATUS_OK ? AParcel_writeStrongBinder(out, nullptr) : st;
    }
    case 28: {
      binder_status_t st = writeStatusOk(out);
      return st == STATUS_OK ? AParcel_writeInt32(out, 0) : st;
    }
    default: {
      __android_log_print(ANDROID_LOG_INFO, kLogTag,
                          "IVivoCameraProvider unhandled code=%u, returning OK",
                          code);
      return writeStatusOk(out);
    }
  }
}

} // namespace

void AppendVendorTagDescs(std::vector<Note13VendorTagDesc> *out) {
  for (const auto &d : kVivoVendorTagDescs) {
    out->push_back(d);
  }
}

bool CollectExposedCameraTargets(const CameraTargetDefaults &,
                                 std::vector<const Note13CameraTarget *> *) {
  return false;
}

bool AppendVendorStaticMetadata(camera_metadata_t *metadata,
                                const Note13CameraTarget &target) {
  const uint8_t camType[] = {'M', 'a', 's', 't', 'e', 'r', 0};
  const uint8_t sensorName[] = {'i', 'm', 'x', '8', '9', '0', 0};
  const uint8_t largeThumb = 0;
  const int32_t sensorSize[] = {target.pixelWidth, target.pixelHeight};
  const int32_t imageSizes[] = {
      target.pixelWidth, target.pixelHeight,
      3840, 2160,
      1920, 1080,
      1440, 1080,
      1280, 720,
  };
  const int32_t zero32 = 0;
  const int32_t one32 = 1;
  const int64_t halVer = 2;
  const float focal35mm =
      target.lensFacing == ANDROID_LENS_FACING_BACK ? 23.0f : 26.0f;
  const int64_t expMin = target.minExposureTime;
  const int64_t expMax = 30000000000LL;
  const int32_t isoMin = target.minSensitivity;
  const int32_t isoMax = target.maxSensitivity;
  const std::string physComb = std::string("Master,") + target.id + ";";
  const std::vector<uint8_t> physCombBytes(physComb.begin(), physComb.end());
  const uint8_t emptyStr[] = {0};

  return AddMetaEntry(metadata, kVivoChiCameraTypeTag, camType, std::size(camType)) &&
         AddMetaEntry(metadata, kVivoChiLargeSizeThumbTag, &largeThumb, 1) &&
         AddMetaEntry(metadata, kVivoChiSensorSizeTag, sensorSize, std::size(sensorSize)) &&
         AddMetaEntry(metadata, kVivoFeedbackSensorNameTag, sensorName, std::size(sensorName)) &&
         AddMetaEntry(metadata, kVivoSensorFocallen35mmTag, &focal35mm, 1) &&
         AddMetaEntry(metadata, kVivoImageSizesTag, imageSizes, std::size(imageSizes)) &&
         AddMetaEntry(metadata, kVivoSensorModeSizeListTag, sensorSize, std::size(sensorSize)) &&
         AddMetaEntry(metadata, kVivoRemosaicTypeTag, &zero32, 1) &&
         AddMetaEntry(metadata, kVivoHalVersionTag, &halVer, 1) &&
         AddMetaEntry(metadata, kVivoDisableReconfigTag, &zero32, 1) &&
         AddMetaEntry(metadata, kVivoCurrentModeVersionTag, &one32, 1) &&
         AddMetaEntry(metadata, kVivoJpegNeedUpscaleTag, &zero32, 1) &&
         AddMetaEntry(metadata, kVivoIsNv21PlatformTag, &zero32, 1) &&
         AddMetaEntry(metadata, kVivoSupportedMaxCfaPatternTag, &zero32, 1) &&
         AddMetaEntry(metadata, kVivoProExpTimeMinTag, &expMin, 1) &&
         AddMetaEntry(metadata, kVivoProExpTimeMaxTag, &expMax, 1) &&
         AddMetaEntry(metadata, kVivoProIsoMinTag, &isoMin, 1) &&
         AddMetaEntry(metadata, kVivoProIsoMaxTag, &isoMax, 1) &&
         AddMetaEntry(metadata, kVivoLogicalMcPhysicalIdsTag, emptyStr, 1) &&
         AddMetaEntry(metadata, kVivoPhysicalCamCombinationTag, physCombBytes) &&
         AddMetaEntry(metadata, kVivoMultiCamTypeMapSessionIdTag, emptyStr, 1) &&
         AddMetaEntry(metadata, kVivoMultiCamBokehSensorSizeTag, emptyStr, 1) &&
         AddMetaEntry(metadata, kVivoSensorNotSetStreamPhysIdTag, emptyStr, 1);
}

void AppendVendorCharacteristicKeys(std::vector<int32_t> *keys,
                                    const Note13CameraTarget &) {
  for (const auto &d : kVivoVendorTagDescs) {
    keys->push_back(static_cast<int32_t>(d.tagId));
  }
}

bool AppendVendorDefaultRequest(camera_metadata_t *,
                                const Note13CameraTarget &) {
  return true;
}

void RegisterVendorExtraServices() {
  AIBinder_Class *devClass = AIBinder_Class_define(
      kVivoCameraDeviceDesc, vivoStubOnCreate, vivoStubOnDestroy,
      onVivoCameraDeviceTransact);
  AIBinder_Class *vifClass = AIBinder_Class_define(
      kVivoCameraVifDesc, vivoStubOnCreate, vivoStubOnDestroy,
      onVivoCameraVifTransact);
  AIBinder_Class *provClass = AIBinder_Class_define(
      kVivoCameraProviderDesc, vivoStubOnCreate, vivoStubOnDestroy,
      onVivoCameraProviderTransact);
  if (devClass == nullptr || vifClass == nullptr || provClass == nullptr) {
    __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                        "failed to define Vivo camera AIDL binder classes");
    return;
  }
  gVivoCameraDeviceBinder = AIBinder_new(devClass, nullptr);
  gVivoCameraVifBinder = AIBinder_new(vifClass, nullptr);
  gVivoCameraProviderBinder = AIBinder_new(provClass, nullptr);
  if (gVivoCameraDeviceBinder != nullptr) {
    AIBinder_markVintfStability(gVivoCameraDeviceBinder);
  }
  if (gVivoCameraVifBinder != nullptr) {
    AIBinder_markVintfStability(gVivoCameraVifBinder);
  }
  if (gVivoCameraProviderBinder == nullptr) {
    return;
  }
  AIBinder_markVintfStability(gVivoCameraProviderBinder);
  binder_status_t st = AServiceManager_addService(gVivoCameraProviderBinder,
                                                  kVivoCameraProviderInstance);
  if (st != STATUS_OK) {
    __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                        "failed to register %s: %d",
                        kVivoCameraProviderInstance, st);
  } else {
    __android_log_print(ANDROID_LOG_INFO, kLogTag,
                        "registered %s (unblocks cameraserver & vivocameraserver)",
                        kVivoCameraProviderInstance);
  }
}

} // namespace cdh::camera
