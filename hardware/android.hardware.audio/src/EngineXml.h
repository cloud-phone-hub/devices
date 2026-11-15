/*
 * Copyright (C) 2026 cloud-phone-hub
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * 引擎音量配置解析：读合并版引擎 XML
 *   /vendor/etc/audio_config.xml
 *     （原 4 文件声明的 ProductStrategies / volumeGroups / 默认音量曲线已内联；
 *      AOSP 旧布局的 xi:include 仍兼容）
 * 只取 volumeGroups（含 <reference> 命名曲线引用），productStrategies 不取 ——
 * 默认策略集由 audioserver 内置，且小米闭源扩展在收到自定义策略时会 map::at 崩溃。
 *
 * 依赖 libxml2；该库由 build/modules/android.hardware.audio.json 的 staticLibs
 * 静态编入服务二进制，镜像里不存在 libxml2.so。
 *
 * 本文件刻意不依赖 AIDL 头，便于在主机侧单独编译验证解析结果。
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cdh::audio {

struct CurvePoint {
    int8_t index = 0;
    int32_t attenuationMb = 0;
};

struct VolumeCurve {
    // 已去掉 DEVICE_CATEGORY_ 前缀：HEADSET / SPEAKER / EARPIECE / EXT_MEDIA / HEARING_AID
    std::string deviceCategory;
    std::vector<CurvePoint> curvePoints;
};

struct VolumeGroup {
    // 已归一为 AUDIO_STREAM_<X>：audioserver 内置策略按该名字引用卷组
    std::string name;
    int32_t minIndex = 0;
    int32_t maxIndex = 0;
    std::vector<VolumeCurve> volumeCurves;
};

// 解析引擎 XML（含 xi:include 展开）。成功返回 true；
// 文件缺失、XML 非法或没有任何 volumeGroup 时返回 false，并由 err 说明原因。
bool loadEngineXml(const std::string& path, std::vector<VolumeGroup>* out, std::string* err);

// 读取引擎 XML 根 <configuration modules="..."> 声明的 IModule 实例名列表；
// 文件缺失、XML 解析失败或未声明 modules 属性时返回默认 {"default", "primary", "r_submix"}。
std::vector<std::string> loadConfiguredModules(const std::string& path);

}  // namespace cdh::audio
