/*
 * Copyright (C) 2026 cloud-phone-hub
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * IConfig 实现。getEngineConfig 的卷组/曲线**由设备侧引擎 XML 外置**：
 *   启动时读 /vendor/etc/audio_config.xml（env AUDIO_ENGINE_CONFIG_XML 可覆盖路径），
 *   由 EngineXml.cpp 用 libxml2 解析（合并版单文件，含 <reference> 命名曲线引用），
 *   据此构建 volumeGroups；读不到则返回**空**（=音频引擎内置默认卷组）。
 *
 * libxml2 由 build/modules/android.hardware.audio.json 的 staticLibs **静态编入**
 * 本服务二进制，vendor 分区里不存在也不需要 libxml2.so。
 *
 * productStrategies 恒为空：默认引擎用内置策略集，填了会触发小米闭源扩展
 * AudioPolicyManagerCustomImpl map::at 崩溃 audioserver。卷组名由 EngineXml 归一为
 * AUDIO_STREAM_<X>，与内置策略引用的名字对齐。
 */

#include "ConfigImpl.h"

#include <android/log.h>

#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "EngineXml.h"

#include <aidl/android/hardware/audio/core/SurroundSoundConfig.h>
#include <aidl/android/media/audio/common/AudioHalEngineConfig.h>
#include <aidl/android/media/audio/common/AudioHalVolumeCurve.h>
#include <aidl/android/media/audio/common/AudioHalVolumeGroup.h>

using ::aidl::android::hardware::audio::core::SurroundSoundConfig;
using ::aidl::android::media::audio::common::AudioHalEngineConfig;
using ::aidl::android::media::audio::common::AudioHalVolumeCurve;
using ::aidl::android::media::audio::common::AudioHalVolumeGroup;

namespace aidl::android::hardware::audio::core {

ndk::ScopedAStatus ConfigImpl::getSurroundSoundConfig(SurroundSoundConfig* out) {
    // 无环绕声硬件：返回默认（全空）配置。AudioPolicyManager 初始化期调用一次。
    *out = SurroundSoundConfig{};
    __android_log_print(ANDROID_LOG_INFO, "cdh_audio",
                        "IConfig.getSurroundSoundConfig -> default(empty)");
    return ndk::ScopedAStatus::ok();
}

namespace {

constexpr const char* kDefaultXmlPath = "/vendor/etc/audio_config.xml";

using Cat = AudioHalVolumeCurve::DeviceCategory;

// EngineXml 已去掉 DEVICE_CATEGORY_ 前缀；未知类别直接跳过该曲线。
bool toCategory(const std::string& name, Cat* out) {
    if (name == "HEADSET") {
        *out = Cat::HEADSET;
    } else if (name == "SPEAKER") {
        *out = Cat::SPEAKER;
    } else if (name == "EARPIECE") {
        *out = Cat::EARPIECE;
    } else if (name == "EXT_MEDIA") {
        *out = Cat::EXT_MEDIA;
    } else if (name == "HEARING_AID") {
        *out = Cat::HEARING_AID;
    } else {
        return false;
    }
    return true;
}

// EngineXml 的纯数据结构 -> AIDL AudioHalVolumeGroup。
std::vector<AudioHalVolumeGroup> toAidlGroups(const std::vector<cdh::audio::VolumeGroup>& groups) {
    std::vector<AudioHalVolumeGroup> out;
    out.reserve(groups.size());
    for (const auto& group : groups) {
        AudioHalVolumeGroup dst;
        dst.name = group.name;
        dst.minIndex = group.minIndex;
        dst.maxIndex = group.maxIndex;
        for (const auto& curve : group.volumeCurves) {
            Cat category;
            if (!toCategory(curve.deviceCategory, &category)) continue;
            AudioHalVolumeCurve converted;
            converted.deviceCategory = category;
            converted.curvePoints.reserve(curve.curvePoints.size());
            for (const auto& point : curve.curvePoints) {
                AudioHalVolumeCurve::CurvePoint convertedPoint;
                convertedPoint.index = point.index;
                convertedPoint.attenuationMb = point.attenuationMb;
                converted.curvePoints.push_back(convertedPoint);
            }
            dst.volumeCurves.push_back(std::move(converted));
        }
        out.push_back(std::move(dst));
    }
    return out;
}

}  // namespace

ndk::ScopedAStatus ConfigImpl::getEngineConfig(AudioHalEngineConfig* out) {
    *out = AudioHalEngineConfig{};
    const char* envPath = std::getenv("AUDIO_ENGINE_CONFIG_XML");
    const std::string xmlPath =
            (envPath != nullptr && envPath[0] != '\0') ? envPath : kDefaultXmlPath;

    std::vector<cdh::audio::VolumeGroup> groups;
    std::string err;
    if (!cdh::audio::loadEngineXml(xmlPath, &groups, &err)) {
        // 缺 XML / 解析失败：保持空卷组，音量交由音频引擎内置默认表。
        __android_log_print(ANDROID_LOG_WARN, "cdh_audio",
                            "IConfig.getEngineConfig -> xml=%s 未生效（%s），回退空 volumeGroups",
                            xmlPath.c_str(), err.c_str());
        return ndk::ScopedAStatus::ok();
    }

    out->volumeGroups = toAidlGroups(groups);

    // 查找 MUSIC 卷组便于日志核对（无则 -1）。
    int musicMin = -1;
    int musicMax = -1;
    size_t curveCount = 0;
    for (const auto& group : out->volumeGroups) {
        curveCount += group.volumeCurves.size();
        if (group.name == "AUDIO_STREAM_MUSIC") {
            musicMin = group.minIndex;
            musicMax = group.maxIndex;
        }
    }
    __android_log_print(ANDROID_LOG_INFO, "cdh_audio",
                        "IConfig.getEngineConfig -> xml=%s volumeGroups=%zu volumeCurves=%zu "
                        "productStrategies=%zu music[min=%d,max=%d]",
                        xmlPath.c_str(), out->volumeGroups.size(), curveCount,
                        out->productStrategies.size(), musicMin, musicMax);
    return ndk::ScopedAStatus::ok();
}

}  // namespace aidl::android::hardware::audio::core
