#define LOG_TAG "AllocatorService"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <sched.h>
#include <string>

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <log/log.h>

#include "CdhAllocator.h"

using aidl::android::hardware::graphics::allocator::cdh::CdhAllocator;

int main(int /*argc*/, char** /*argv*/) {
  ALOGI("Minigbm AIDL allocator (cdh source build) starting up...");

  // 与 SurfaceFlinger 主线程对齐 SCHED_FIFO 实时优先级
  struct sched_param param = {0};
  param.sched_priority = 2;
  if (sched_setscheduler(0, SCHED_FIFO | SCHED_RESET_ON_FORK, &param) != 0) {
    ALOGI("%s: failed to set priority: %s", __FUNCTION__, strerror(errno));
  }

  auto allocator = ndk::SharedRefBase::make<CdhAllocator>();
  CHECK(allocator != nullptr);

  if (!allocator->init()) {
    ALOGE("Failed to initialize Minigbm AIDL allocator.");
    return EXIT_FAILURE;
  }

  const std::string instance = std::string(CdhAllocator::descriptor) + "/default";
  const binder_status_t status =
      AServiceManager_addService(allocator->asBinder().get(), instance.c_str());
  CHECK_EQ(status, STATUS_OK);

  ABinderProcess_setThreadPoolMaxThreadCount(4);
  ABinderProcess_startThreadPool();
  ABinderProcess_joinThreadPool();

  return EXIT_FAILURE;
}
