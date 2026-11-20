// 自编的 HIDL composer 2.4 服务壳。
//
// 与 AOSP android-14 的 graphics/composer/2.4/default/service.cpp 等价，只做两处收敛：
//
//   1. 不走 HwcLoader::load()。那条路会在 hwc2 模块打不开时回落到 hwc1 / fbdev，
//      于是可执行文件对 libhwc2on1adapter / libhwc2onfbadapter 留下 DT_NEEDED，
//      这两件底包没有、也不想从官方镜像借。云机只有 hwc2（drm_hwcomposer），
//      所以直接按模块名取 hwc2 设备。
//   2. 不引用 composer@2.2 的类型。2.2 只服务老客户端，本机不需要。
//
// 其余（进程初始化、线程池、注册服务）与官方实现保持一致。
//
// 本文件是本模块唯一的自写代码：上游实现体（external/drm_hwcomposer）与
// ComposerResources.cpp 都由 cdh-vendor 从 platform 源码树里取，见
// build/modules/android.hardware.graphics.composer.json。
#define LOG_TAG "hwc-composer"

#include <errno.h>
#include <sched.h>
#include <stdio.h>

#include <memory>

#include <android/hardware/graphics/composer/2.4/IComposer.h>
#include <binder/ProcessState.h>
#include <composer-hal/2.4/Composer.h>
#include <composer-passthrough/2.4/HwcHal.h>
#include <hardware/hardware.h>
#include <hidl/HidlTransportSupport.h>
#include <log/log.h>

using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;
using android::hardware::graphics::composer::V2_4::IComposer;
using android::hardware::graphics::composer::V2_4::hal::Composer;
using android::hardware::graphics::composer::V2_4::passthrough::HwcHal;

namespace {

// loadComposer 打开 hwc2 模块并用 HwcHal 包起来，最后交给 Composer::create。
android::sp<IComposer> loadComposer() {
    const hw_module_t* module = nullptr;
    int err = hw_get_module(HWC_HARDWARE_MODULE_ID, &module);
    if (err != 0) {
        ALOGE("hw_get_module(%s) 失败: %d", HWC_HARDWARE_MODULE_ID, err);
        return nullptr;
    }
    auto hal = std::make_unique<HwcHal>();
    if (!hal->initWithModule(module)) {
        ALOGE("HwcHal::initWithModule 失败");
        return nullptr;
    }
    std::unique_ptr<IComposer> composer = Composer::create(std::move(hal));
    if (!composer) {
        ALOGE("Composer::create 失败");
        return nullptr;
    }
    return android::sp<IComposer>(composer.release());
}

}  // namespace

int main() {
    // 与官方实现一致：composer 是 vendor 侧服务，走 vndbinder。
    android::ProcessState::initWithDriver("/dev/vndbinder");
    android::ProcessState::self()->setThreadPoolMaxThreadCount(4);
    android::ProcessState::self()->startThreadPool();

    struct sched_param param = {0};
    param.sched_priority = 2;
    if (sched_setscheduler(0, SCHED_FIFO | SCHED_RESET_ON_FORK, &param) != 0) {
        ALOGW("Couldn't set SCHED_FIFO: %d", errno);
    }

    configureRpcThreadpool(4, true /* will join */);

    android::sp<IComposer> composer = loadComposer();
    if (composer == nullptr) {
        ALOGE("没有可用的 hwcomposer 模块");
        return 1;
    }
    if (composer->registerAsService() != android::NO_ERROR) {
        ALOGE("注册 composer 服务失败");
        return 1;
    }

    joinRpcThreadpool();

    ALOGE("service is terminating");
    return 1;
}
