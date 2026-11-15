/*
 * Copyright (C) 2026 cloud-phone-hub
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * audio HAL 服务入口：注册
 *   android.hardware.audio.core.IConfig/default
 *   android.hardware.audio.core.IModule/primary
 * 与 lineage audio/aidl/default/main.cpp 对齐（不注册 MST/MST 无关项）；
 * 模块配置硬编码，只有 IConfig 的引擎音量组读 vendor XML（见 EngineXml.cpp）。
 */

#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <android/log.h>

#include <string>

#include "ConfigImpl.h"
#include "EngineXml.h"
#include "ModuleImpl.h"

using ::aidl::android::hardware::audio::core::ConfigImpl;
using ::aidl::android::hardware::audio::core::IConfig;
using ::aidl::android::hardware::audio::core::IModule;
using ::aidl::android::hardware::audio::core::ModuleImpl;

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
    ABinderProcess_setThreadPoolMaxThreadCount(16);

    auto config = ndk::SharedRefBase::make<ConfigImpl>();
    const std::string configFqn = std::string(IConfig::descriptor) + "/default";
    binder_status_t status = AServiceManager_addService(config->asBinder().get(),
                                                        configFqn.c_str());
    logRegister(configFqn.c_str(), status);

    // 按 /vendor/etc/audio_config.xml <configuration modules="..."> 声明注册 IModule 实例：
    // default 与 primary 共享 Primary 模块实例；r_submix 走 REMOTE_SUBMIX 回声实例；
    // bluetooth 走对齐 bluetooth_audio_policy_configuration_7_0.xml 的 Role::Bluetooth。
    const auto instances = cdh::audio::loadConfiguredModules("/vendor/etc/audio_config.xml");
    auto primaryModule = ndk::SharedRefBase::make<ModuleImpl>(ModuleImpl::Role::Primary);
    for (const auto& instance : instances) {
        std::shared_ptr<ModuleImpl> mod;
        if (instance == "r_submix") {
            mod = ndk::SharedRefBase::make<ModuleImpl>(ModuleImpl::Role::RemoteSubmix);
        } else if (instance == "bluetooth") {
            mod = ndk::SharedRefBase::make<ModuleImpl>(ModuleImpl::Role::Bluetooth);
        } else {
            mod = primaryModule;
        }
        const std::string moduleFqn = std::string(IModule::descriptor) + "/" + instance;
        status = AServiceManager_addService(mod->asBinder().get(), moduleFqn.c_str());
        logRegister(moduleFqn.c_str(), status);
    }

    ABinderProcess_joinThreadPool();
    return 1;  // 不应到达
}