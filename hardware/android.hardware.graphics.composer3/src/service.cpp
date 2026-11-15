#define LOG_TAG "drmhwc"
#define ATRACE_TAG (ATRACE_TAG_GRAPHICS | ATRACE_TAG_HAL)

#include <cerrno>
#include <cstdlib>
#include <sched.h>
#include <string>

#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <log/log.h>

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
