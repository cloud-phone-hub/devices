// 自编的 HIDL composer 2.4 服务壳。
//
// 与 AOSP android-14 的 graphics/composer/2.4/default/service.cpp 等价，只做三处收敛：
//
//   1. 不走 HwcLoader::load()。那条路会在 hwc2 模块打不开时回落到 hwc1 / fbdev，
//      于是可执行文件对 libhwc2on1adapter / libhwc2onfbadapter 留下 DT_NEEDED，
//      这两件底包没有、也不想从官方镜像借。云机只有 hwc2（drm_hwcomposer），
//      所以直接按模块名取 hwc2 设备。
//   2. 不引用 composer@2.2 的类型。2.2 只服务老客户端，本机不需要。
//   3. 兼容 GRF 跨代厂商扩展命令（如高通/Nothing A15/A16 SurfaceFlinger 下发的 0x8000000 等
//      >= 0x08000000 私有命令）：安全跳过 length 个 uint32 参数并返回 true，避免
//      CommandReaderBase::endCommand() 触发 "too little data read" 与整帧 BadParameter。
//
// 本文件是本模块唯一的自写代码：上游实现体（external/drm_hwcomposer）与
// ComposerResources.cpp 都由 cdh-vendor 从 platform 源码树里取，见
// build/modules/android.hardware.graphics.composer.json。
#define LOG_TAG "hwc-composer"

#include <errno.h>
#include <sched.h>
#include <stdio.h>

#include <memory>
#include <string>
#include <vector>

#include <android/hardware/graphics/composer/2.4/IComposer.h>
#include <binder/ProcessState.h>
#include <composer-hal/2.4/Composer.h>
#include <composer-hal/2.4/ComposerClient.h>
#include <composer-hal/2.4/ComposerCommandEngine.h>
#include <composer-passthrough/2.4/HwcHal.h>
#include <hardware/hardware.h>
#include <hidl/HidlTransportSupport.h>
#include <log/log.h>

using android::hardware::Return;
using android::hardware::Void;
using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;
using android::hardware::graphics::composer::V2_4::Error;
using android::hardware::graphics::composer::V2_4::IComposer;
using android::hardware::graphics::composer::V2_4::IComposerClient;
using android::hardware::graphics::composer::V2_4::hal::Composer;
using android::hardware::graphics::composer::V2_4::hal::ComposerClient;
using android::hardware::graphics::composer::V2_4::hal::ComposerCommandEngine;
using android::hardware::graphics::composer::V2_4::hal::ComposerHal;
using android::hardware::graphics::composer::V2_4::passthrough::HwcHal;

namespace {

// CdhComposerCommandEngine 在标准 HIDL 2.4 命令解析之上，优雅跳过高通/MTK 等厂商扩展命令
//（opcode >= 0x08000000），避免 GRF Android 15/16 SurfaceFlinger 每帧 presentOrValidate 返回 BadParameter。
class CdhComposerCommandEngine : public ComposerCommandEngine {
   public:
    CdhComposerCommandEngine(ComposerHal* hal,
                             android::hardware::graphics::composer::V2_2::hal::ComposerResources* resources)
        : ComposerCommandEngine(hal, resources), mHal24(hal) {}

   protected:
    bool executeCommand(android::hardware::graphics::composer::V2_1::IComposerClient::Command command,
                        uint16_t length) override {
        const uint32_t raw = static_cast<uint32_t>(command);
        if (raw >= 0x08000000u) {
            for (uint16_t i = 0; i < length; ++i) {
                (void)read();
            }
            return true;
        }
        if (static_cast<IComposerClient::Command>(command) ==
            IComposerClient::Command::SET_LAYER_GENERIC_METADATA) {
            return executeSetLayerGenericMetadata24(length);
        }
        return android::hardware::graphics::composer::V2_3::hal::ComposerCommandEngine::executeCommand(
                command, length);
    }

   private:
    bool executeSetLayerGenericMetadata24(uint16_t length) {
        if (length < 3) {
            return false;
        }
        const uint32_t maxBytes = static_cast<uint32_t>(length) * sizeof(uint32_t);
        const uint32_t keySize = read();
        if (keySize > maxBytes) {
            return false;
        }
        std::string key;
        key.resize(keySize);
        readBlob(keySize, key.data());

        const bool mandatory = read();

        const uint32_t valueSize = read();
        if (valueSize > maxBytes) {
            return false;
        }
        std::vector<uint8_t> value(valueSize);
        readBlob(valueSize, value.data());

        auto error = mHal24->setLayerGenericMetadata(mCurrentDisplay, mCurrentLayer, key, mandatory,
                                                     value);
        if (error != Error::NONE) {
            android::hardware::graphics::composer::V2_1::hal::ComposerCommandEngine::mWriter->setError(
                    getCommandLoc(),
                    static_cast<android::hardware::graphics::composer::V2_1::Error>(error));
        }
        return true;
    }

    ComposerHal* mHal24;
};

class CdhComposerClient : public ComposerClient {
   public:
    explicit CdhComposerClient(ComposerHal* hal) : ComposerClient(hal), mHal24(hal) {}

    static std::unique_ptr<CdhComposerClient> create(ComposerHal* hal) {
        auto client = std::make_unique<CdhComposerClient>(hal);
        return client->init() ? std::move(client) : nullptr;
    }

   protected:
    std::unique_ptr<android::hardware::graphics::composer::V2_1::hal::ComposerCommandEngine>
    createCommandEngine() override {
        using Base21Client = android::hardware::graphics::composer::V2_1::hal::detail::
                ComposerClientImpl<IComposerClient, ComposerHal>;
        return std::make_unique<CdhComposerCommandEngine>(
                mHal24,
                static_cast<android::hardware::graphics::composer::V2_2::hal::ComposerResources*>(
                        Base21Client::mResources.get()));
    }

   private:
    ComposerHal* mHal24;
};

class CdhComposer : public Composer {
   public:
    static std::unique_ptr<CdhComposer> create(std::unique_ptr<ComposerHal> hal) {
        return std::make_unique<CdhComposer>(std::move(hal));
    }

    explicit CdhComposer(std::unique_ptr<ComposerHal> hal) : Composer(std::move(hal)) {}

    Return<void> createClient_2_4(IComposer::createClient_2_4_cb hidl_cb) override {
        using Base21Composer = android::hardware::graphics::composer::V2_1::hal::detail::
                ComposerImpl<IComposer, ComposerHal>;
        std::unique_lock<std::mutex> lock(Base21Composer::mClientMutex);
        if (!Base21Composer::waitForClientDestroyedLocked(lock)) {
            hidl_cb(Error::NO_RESOURCES, nullptr);
            return Void();
        }

        android::sp<CdhComposerClient> client =
                CdhComposerClient::create(Base21Composer::mHal.get()).release();
        if (!client) {
            hidl_cb(Error::NO_RESOURCES, nullptr);
            return Void();
        }

        auto clientDestroyed = [this]() {
            using Base21C = android::hardware::graphics::composer::V2_1::hal::detail::ComposerImpl<
                    IComposer, ComposerHal>;
            Base21C::onClientDestroyed();
        };
        client->setOnClientDestroyed(clientDestroyed);

        Base21Composer::mClient = client;
        hidl_cb(Error::NONE, client);
        return Void();
    }
};

// loadComposer 打开 hwc2 模块并用 HwcHal 包起来，最后交给 CdhComposer::create。
android::sp<IComposer> loadComposer() {
    const hw_module_t* module = nullptr;
    // 云机统一使用 drm_hwcomposer (hwcomposer.drm.so)，而各机型 vendor/build.prop 为对齐真机指纹
    // 常保留 ro.hardware.hwcomposer=unisoc/mtk 等原厂值；故优先显式加载 "drm" 实现。
    int err = hw_get_module_by_class(HWC_HARDWARE_MODULE_ID, "drm", &module);
    if (err != 0) {
        err = hw_get_module(HWC_HARDWARE_MODULE_ID, &module);
    }
    if (err != 0) {
        ALOGE("hw_get_module(%s) 失败: %d", HWC_HARDWARE_MODULE_ID, err);
        return nullptr;
    }
    auto hal = std::make_unique<HwcHal>();
    if (!hal->initWithModule(module)) {
        ALOGE("HwcHal::initWithModule 失败");
        return nullptr;
    }
    std::unique_ptr<IComposer> composer = CdhComposer::create(std::move(hal));
    if (!composer) {
        ALOGE("CdhComposer::create 失败");
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
