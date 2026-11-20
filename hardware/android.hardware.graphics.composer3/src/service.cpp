#define LOG_TAG "drmhwc"
#define ATRACE_TAG (ATRACE_TAG_GRAPHICS | ATRACE_TAG_HAL)

#include <cerrno>
#include <cstdlib>
#include <sched.h>
#include <string>

#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <log/log.h>

#if defined(CDH_ANDROID_VERSION) && CDH_ANDROID_VERSION <= 13

#include <memory>
#include <vector>
#include <aidl/android/hardware/graphics/composer3/BnComposer.h>
#if CDH_COMPOSER_OPLUS
#include "oplus/CdhOplusDisplayColor.h"
#endif

namespace {
class StubComposerV1 : public aidl::android::hardware::graphics::composer3::BnComposer {
 public:
  ndk::ScopedAStatus createClient(
      std::shared_ptr<aidl::android::hardware::graphics::composer3::IComposerClient>* out_client) override {
    if (out_client != nullptr) {
      out_client->reset();
    }
    return ndk::ScopedAStatus::fromServiceSpecificError(IComposer::EX_NO_RESOURCES);
  }
  ndk::ScopedAStatus getCapabilities(
      std::vector<aidl::android::hardware::graphics::composer3::Capability>* caps) override {
    if (caps != nullptr) {
      caps->clear();
    }
    return ndk::ScopedAStatus::ok();
  }
};
}  // namespace

int main(int /*argc*/, char* /*argv*/[]) {
  ABinderProcess_setThreadPoolMaxThreadCount(4);
  ABinderProcess_startThreadPool();
  auto composer = ndk::SharedRefBase::make<StubComposerV1>();
  const std::string instance = std::string(StubComposerV1::descriptor) + "/default";
  const auto status = AServiceManager_addService(composer->asBinder().get(), instance.c_str());
  if (status != STATUS_OK) {
    ALOGE("Failed to register HWC3 v1 service %s: %d", instance.c_str(), static_cast<int>(status));
    return -EINVAL;
  }
#if CDH_COMPOSER_OPLUS
  auto oplus_color_service =
      aidl::android::hardware::graphics::composer3::cdh::RegisterOplusDisplayColorFeatureIfDeclared();
  (void)oplus_color_service;
#endif
  ABinderProcess_joinThreadPool();
  return EXIT_FAILURE;
}

#else

#include "CdhComposer.h"
#include "CdhDisplayConfig.h"
#if CDH_COMPOSER_OPLUS
#include "oplus/CdhOplusDisplayColor.h"
#endif
#include "hwc3/Composer.h"

using aidl::android::hardware::graphics::composer3::cdh::CdhComposer;
using aidl::android::hardware::graphics::composer3::cdh::LoadCdhDisplayConfig;
#if CDH_COMPOSER_OPLUS
using aidl::android::hardware::graphics::composer3::cdh::RegisterOplusDisplayColorFeatureIfDeclared;
#endif
using UpstreamComposer = aidl::android::hardware::graphics::composer3::impl::Composer;

int main(int /*argc*/, char* /*argv*/[]) {
  ALOGI("hwc3-drm (cdh source build) starting up");

  // 与 SurfaceFlinger 主线程对齐 SCHED_FIFO 实时优先级
  struct sched_param param = {0};
  param.sched_priority = 2;
  if (sched_setscheduler(0, SCHED_FIFO | SCHED_RESET_ON_FORK, &param) != 0) {
    ALOGE("Couldn't set SCHED_FIFO: %d", errno);
  }

  ABinderProcess_setThreadPoolMaxThreadCount(4);
  ABinderProcess_startThreadPool();

  auto inner = ndk::SharedRefBase::make<UpstreamComposer>();
  if (!inner) {
    ALOGE("Failed to create upstream composer");
    return -ENOMEM;
  }

  auto cfg = LoadCdhDisplayConfig();
  auto composer = ndk::SharedRefBase::make<CdhComposer>(inner, std::move(cfg));
  if (!composer) {
    ALOGE("Failed to create CdhComposer wrapper");
    return -ENOMEM;
  }

  const std::string instance = std::string(CdhComposer::descriptor) + "/default";
  ALOGI("HWC3 service name %s", instance.c_str());

  const auto status = AServiceManager_addServiceWithFlags(
      composer->asBinder().get(), instance.c_str(),
      AServiceManager_AddServiceFlag::ADD_SERVICE_ALLOW_ISOLATED);
  if (status != STATUS_OK) {
    ALOGE("Failed to register HWC3 service %s: %d", instance.c_str(),
          static_cast<int>(status));
    return -EINVAL;
  }

#if CDH_COMPOSER_OPLUS
  auto oplus_color_service = RegisterOplusDisplayColorFeatureIfDeclared();
  (void)oplus_color_service;
#endif

  ABinderProcess_joinThreadPool();
  return EXIT_FAILURE;
}

#endif
