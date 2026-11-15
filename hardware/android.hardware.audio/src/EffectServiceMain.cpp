/*
 * Copyright (C) 2026 cloud-phone-hub
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * effect HAL 服务入口：注册 android.hardware.audio.effect.IFactory/default。
 */

#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <android/log.h>

#include <string>

#include "EffectFactoryImpl.h"

using ::aidl::android::hardware::audio::effect::EffectFactoryImpl;
using ::aidl::android::hardware::audio::effect::IFactory;

namespace {
void logRegister(const char* what, binder_status_t status) {
    if (status == STATUS_OK) {
        __android_log_print(ANDROID_LOG_INFO, "cdh_audio", "registered %s", what);
    } else {
        __android_log_print(ANDROID_LOG_ERROR, "cdh_audio", "FAILED to register %s (status=%d)",
                            what, status);
    }
}
}  // namespace

int main() {
    ABinderProcess_setThreadPoolMaxThreadCount(8);

    auto factory = ndk::SharedRefBase::make<EffectFactoryImpl>();
    const std::string factoryFqn = std::string(IFactory::descriptor) + "/default";
    binder_status_t status = AServiceManager_addService(factory->asBinder().get(),
                                                        factoryFqn.c_str());
    logRegister(factoryFqn.c_str(), status);

    ABinderProcess_joinThreadPool();
    return 1;  // 不应到达
}