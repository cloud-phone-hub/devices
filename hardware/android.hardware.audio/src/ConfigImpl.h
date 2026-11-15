/*
 * Copyright (C) 2026 cloud-phone-hub
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * IConfig 实现：返回 CDH 虚机的环绕声配置与音频策略引擎配置。
 * 语义参照 lineage-22 hardware/interfaces/audio/aidl/default/Config.cpp：
 * 环绕声无硬件，返回空配置；引擎配置解析
 * /vendor/etc/audio_config.xml（EngineXml.cpp，libxml2 静态编入）。
 */

#pragma once

#include <aidl/android/hardware/audio/core/BnConfig.h>

namespace aidl::android::hardware::audio::core {

class ConfigImpl : public BnConfig {
  public:
    ndk::ScopedAStatus getSurroundSoundConfig(
            SurroundSoundConfig* _aidl_return) override;
    ndk::ScopedAStatus getEngineConfig(
            ::aidl::android::media::audio::common::AudioHalEngineConfig* _aidl_return) override;
};

}  // namespace aidl::android::hardware::audio::core