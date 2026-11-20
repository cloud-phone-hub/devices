#define LOG_TAG "drmhwc"

#include "CdhOplusDisplayColor.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <android/binder_ibinder.h>
#include <android/binder_manager.h>
#include <android/binder_parcel.h>
#include <android/binder_parcel_utils.h>
#include <android/binder_stability.h>
#include <android/binder_status.h>
#include <log/log.h>

namespace aidl::android::hardware::graphics::composer3::cdh {
namespace {

constexpr const char* kServiceName =
    "vendor.oplus.hardware.displaycolorfeature.IDisplayColorFeature/default";
constexpr const char* kDescriptor =
    "vendor.oplus.hardware.displaycolorfeature.IDisplayColorFeature";
constexpr int32_t kVersion = 1;
constexpr const char* kHash = "d6787dd098a56609b744bb3fa3a0fdc00f9f2513";

bool FileContainsDisplayColorFeature(const char* path) {
  FILE* fp = std::fopen(path, "r");
  if (!fp) {
    return false;
  }
  char buf[4096] = {0};
  const size_t n = std::fread(buf, 1, sizeof(buf) - 1, fp);
  std::fclose(fp);
  if (n == 0) {
    return false;
  }
  buf[n] = '\0';
  return std::strstr(buf, "IDisplayColorFeature") != nullptr;
}

bool HasOplusDisplayColorVintfDeclaration() {
  static const char* const kCandidatePaths[] = {
      "/odm/etc/vintf/manifest/vintf_manifest_displaycolorfeature_aidl.xml",
      "/odm/etc/vintf/manifest/manifest_displaycolorfeature_aidl.xml",
      "/vendor/etc/vintf/manifest/vintf_manifest_displaycolorfeature_aidl.xml",
      "/vendor/etc/vintf/manifest/manifest_displaycolorfeature_aidl.xml",
  };
  for (const char* path : kCandidatePaths) {
    if (FileContainsDisplayColorFeature(path)) {
      return true;
    }
  }
  return false;
}

binder_status_t OplusDisplayColor_onTransact(AIBinder* /*binder*/,
                                             transaction_code_t code,
                                             const AParcel* in,
                                             AParcel* out) {
  switch (code) {
    case (FIRST_CALL_TRANSACTION + 0 /*getDisplayColorFeatureValue*/): {
      int32_t feature_type = 0;
      binder_status_t st = AParcel_readInt32(in, &feature_type);
      if (st != STATUS_OK) return st;

      int32_t out_len = 0;
      st = AParcel_readInt32(in, &out_len);
      if (st != STATUS_OK) return st;
      if (out_len < 0) out_len = 0;
      if (out_len > 256) out_len = 256;

      ndk::ScopedAStatus ok = ndk::ScopedAStatus::ok();
      st = AParcel_writeStatusHeader(out, ok.get());
      if (st != STATUS_OK) return st;

      const int32_t ret_code = 0;
      st = AParcel_writeInt32(out, ret_code);
      if (st != STATUS_OK) return st;

      std::vector<int32_t> out_values(static_cast<size_t>(out_len), 0);
      return ::ndk::AParcel_writeVector(out, out_values);
    }
    case (FIRST_CALL_TRANSACTION + 1 /*setDisplayColorFeatureValue*/): {
      int32_t feature_type = 0;
      binder_status_t st = AParcel_readInt32(in, &feature_type);
      if (st != STATUS_OK) return st;

      std::vector<int32_t> in_values;
      st = ::ndk::AParcel_readVector(in, &in_values);
      if (st != STATUS_OK) return st;

      ndk::ScopedAStatus ok = ndk::ScopedAStatus::ok();
      st = AParcel_writeStatusHeader(out, ok.get());
      if (st != STATUS_OK) return st;

      const int32_t ret_code = 0;
      return AParcel_writeInt32(out, ret_code);
    }
    case (FIRST_CALL_TRANSACTION + 16777214 /*getInterfaceVersion*/): {
      ndk::ScopedAStatus ok = ndk::ScopedAStatus::ok();
      binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
      if (st != STATUS_OK) return st;
      return AParcel_writeInt32(out, kVersion);
    }
    case (FIRST_CALL_TRANSACTION + 16777213 /*getInterfaceHash*/): {
      ndk::ScopedAStatus ok = ndk::ScopedAStatus::ok();
      binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
      if (st != STATUS_OK) return st;
      const std::string hash(kHash);
      return ::ndk::AParcel_writeString(out, hash);
    }
    default:
      return STATUS_UNKNOWN_TRANSACTION;
  }
}

AIBinder_Class* GetOplusDisplayColorClass() {
  static AIBinder_Class* cls = ::ndk::ICInterface::defineClass(
      kDescriptor, OplusDisplayColor_onTransact, nullptr, 0);
  return cls;
}

class IOplusDisplayColorFeature : public ::ndk::ICInterface {
 public:
  IOplusDisplayColorFeature() = default;
  ~IOplusDisplayColorFeature() override = default;
};

class OplusDisplayColorFeatureService
    : public ::ndk::BnCInterface<IOplusDisplayColorFeature> {
 public:
  OplusDisplayColorFeatureService() = default;
  ~OplusDisplayColorFeatureService() override = default;

 protected:
  ::ndk::SpAIBinder createBinder() override {
    AIBinder* binder =
        AIBinder_new(GetOplusDisplayColorClass(), static_cast<void*>(this));
    AIBinder_markVintfStability(binder);
    return ::ndk::SpAIBinder(binder);
  }
};

}  // namespace

std::shared_ptr<::ndk::ICInterface> RegisterOplusDisplayColorFeatureIfDeclared() {
  if (!HasOplusDisplayColorVintfDeclaration()) {
    return nullptr;
  }
  auto service = ndk::SharedRefBase::make<OplusDisplayColorFeatureService>();
  if (!service) {
    ALOGE("Failed to allocate OplusDisplayColorFeatureService");
    return nullptr;
  }
  const binder_status_t status =
      AServiceManager_addService(service->asBinder().get(), kServiceName);
  if (status != STATUS_OK) {
    ALOGW("Skipped registering %s (status=%d)", kServiceName,
          static_cast<int>(status));
    return nullptr;
  }
  ALOGI("Registered %s in HWC3 process", kServiceName);
  return service;
}

}  // namespace aidl::android::hardware::graphics::composer3::cdh
