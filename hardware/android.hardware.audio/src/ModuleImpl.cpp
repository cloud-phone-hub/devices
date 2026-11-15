/*
 * Copyright (C) 2026 cloud-phone-hub
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ModuleImpl.h"

#include <android/log.h>

#include <cinttypes>

#include "AlsaPcm.h"
#include <aidl/android/hardware/audio/common/SinkMetadata.h>
#include <aidl/android/hardware/audio/common/SourceMetadata.h>
#include <aidl/android/media/audio/common/AudioDeviceType.h>
#include <aidl/android/media/audio/common/AudioMMapPolicy.h>
#include <aidl/android/media/audio/common/AudioMMapPolicyInfo.h>
#include <aidl/android/media/audio/common/AudioOutputFlags.h>

// 诊断日志（CDH issue-scrcpy-audio-vdac）：policy 驱动层触达确认。
#define CDH_TAG "cdh_audio"
#define CDH_LOG(...) __android_log_print(ANDROID_LOG_INFO, CDH_TAG, __VA_ARGS__)

using ::aidl::android::hardware::audio::common::SinkMetadata;
using ::aidl::android::hardware::audio::common::SourceMetadata;
using ::aidl::android::hardware::audio::core::AudioPatch;
using ::aidl::android::hardware::audio::core::AudioRoute;
using ::aidl::android::hardware::audio::core::ModuleDebug;
using ::aidl::android::hardware::audio::core::VendorParameter;
using ::aidl::android::media::audio::common::AudioChannelLayout;
using ::aidl::android::media::audio::common::AudioDevice;
using ::aidl::android::media::audio::common::AudioDeviceAddress;
using ::aidl::android::media::audio::common::AudioDeviceDescription;
using ::aidl::android::media::audio::common::AudioDeviceType;
using ::aidl::android::media::audio::common::AudioFormatDescription;
using ::aidl::android::media::audio::common::AudioFormatType;
using ::aidl::android::media::audio::common::AudioGain;
using ::aidl::android::media::audio::common::AudioIoFlags;
using ::aidl::android::media::audio::common::AudioMMapPolicy;
using ::aidl::android::media::audio::common::AudioMMapPolicyInfo;
using ::aidl::android::media::audio::common::AudioMMapPolicyType;
using ::aidl::android::media::audio::common::AudioMode;
using ::aidl::android::media::audio::common::AudioOutputFlags;
using ::aidl::android::media::audio::common::AudioPort;
using ::aidl::android::media::audio::common::AudioPortConfig;
using ::aidl::android::media::audio::common::AudioPortDeviceExt;
using ::aidl::android::media::audio::common::AudioPortExt;
using ::aidl::android::media::audio::common::AudioPortMixExt;
using ::aidl::android::media::audio::common::AudioProfile;
using ::aidl::android::media::audio::common::AudioEncapsulationType;
using ::aidl::android::media::audio::common::PcmType;

namespace aidl::android::hardware::audio::core {

static AudioFormatDescription makePcm16Format() {
    AudioFormatDescription fmt;
    fmt.type = AudioFormatType::PCM;
    fmt.pcm = PcmType::INT_16_BIT;
    return fmt;
}

// PCM16 / 48kHz / stereo 输出 profile（固定 48000 Hz：AOSP AudioProfile 将 sampleRates 存入
// std::set 升序集合并在包含 44100 时优先回退 AUDIO_SAMPLE_RATE_DEFAULT(44100)，锁定 48000 Hz
// 使 AudioPolicyManager mPrimaryOutput 与 VirtualDac/AlsaPcm 48kHz 时钟严格一致，对齐真机）。
static AudioProfile makeOutProfile() {
    AudioProfile profile;
    profile.name = "pcm16-48k";
    profile.format = makePcm16Format();
    profile.channelMasks = {
            AudioChannelLayout::make<AudioChannelLayout::layoutMask>(
                    AudioChannelLayout::LAYOUT_STEREO),
            AudioChannelLayout::make<AudioChannelLayout::layoutMask>(
                    AudioChannelLayout::LAYOUT_MONO)};
    profile.sampleRates = {48000};
    profile.encapsulationType = AudioEncapsulationType::NONE;
    return profile;
}

// 扬声器默认设备口固定 48kHz stereo（使 AudioPolicyManager 默认输出流锁定 48000 Hz，
// 避免命中 AOSP 44100 Hz 旧回退值，其余采样率音轨由 AudioFlinger Mixer 自动重采样）。
static AudioProfile makeSpeakerProfile() {
    AudioProfile profile;
    profile.name = "pcm16-48k-speaker";
    profile.format = makePcm16Format();
    profile.channelMasks = {
            AudioChannelLayout::make<AudioChannelLayout::layoutMask>(
                    AudioChannelLayout::LAYOUT_STEREO)};
    profile.sampleRates = {48000};
    profile.encapsulationType = AudioEncapsulationType::NONE;
    return profile;
}

// PCM16 / 48kHz 输入 profile（mono + stereo，与 primary policy 声明对齐）
static AudioProfile makeInProfile() {
    AudioProfile profile;
    profile.name = "pcm16-48k-in";
    profile.format = makePcm16Format();
    profile.channelMasks = {
            AudioChannelLayout::make<AudioChannelLayout::layoutMask>(
                    AudioChannelLayout::LAYOUT_STEREO),
            AudioChannelLayout::make<AudioChannelLayout::layoutMask>(
                    AudioChannelLayout::LAYOUT_MONO)};
    profile.sampleRates = {8000, 11025, 16000, 32000, 44100, 48000};
    profile.encapsulationType = AudioEncapsulationType::NONE;
    return profile;
}

// PCM16 / 48kHz / stereo 输入 profile（r_submix 模块要求 IN_STEREO）
static AudioProfile makeStereoInProfile() {
    AudioProfile profile = makeInProfile();
    profile.name = "pcm16-48k-stereo-in";
    profile.channelMasks = {
            AudioChannelLayout::make<AudioChannelLayout::layoutMask>(
                    AudioChannelLayout::LAYOUT_STEREO)};
    return profile;
}

// A2DP 输出口 profile（对齐 /vendor/etc/bluetooth_audio_policy_configuration_7_0.xml）
static AudioProfile makeA2dpProfile() {
    AudioProfile profile;
    profile.name = "pcm16-a2dp";
    profile.format = makePcm16Format();
    profile.channelMasks = {
            AudioChannelLayout::make<AudioChannelLayout::layoutMask>(
                    AudioChannelLayout::LAYOUT_STEREO)};
    profile.sampleRates = {44100, 48000, 88200, 96000};
    profile.encapsulationType = AudioEncapsulationType::NONE;
    return profile;
}

// 端口 flags 与 MIUI policy 请求精确对齐（Hal2AidlMapper 按**精确 flags**匹配 mix port）：
//  - 输出 mix 口：output 标签 + PRIMARY（policy 开默认输出请求 output:2 == 1<<PRIMARY）
//  - 输出设备口：output 标签（默认端口判定 port.flags.getTag()==output 才计入）
//  - 输入口：input 标签（默认 AudioIoFlags{} 即 input tag，这里显式化防歧义）
static AudioIoFlags makeOutMixFlags() {
    AudioIoFlags flags;
    flags.set<AudioIoFlags::output>(1 << static_cast<int32_t>(AudioOutputFlags::PRIMARY));
    return flags;
}

static AudioIoFlags makeOutputFlags() {
    AudioIoFlags flags;
    flags.set<AudioIoFlags::output>(0);
    return flags;
}

static AudioIoFlags makeInputFlags() {
    AudioIoFlags flags;
    flags.set<AudioIoFlags::input>(0);
    return flags;
}

static AudioPortMixExt makeMixExt(int32_t handle) {
    AudioPortMixExt ext;
    ext.handle = handle;
    ext.maxOpenStreamCount = 0;  // unlimited
    ext.maxActiveStreamCount = 0;
    ext.recommendedMuteDurationMs = 0;
    return ext;
}

static AudioPortDeviceExt makeDeviceExt(AudioDeviceType type, bool isDefault,
                                        const std::string& address = "",
                                        const std::string& connection = "") {
    AudioPortDeviceExt ext;
    // AIDL v2 attached 判定：内置设备端口 connection 为空；SUBMIX 设备在 AOSP
    // AidlConversion 映射表中必须为 CONNECTION_VIRTUAL ("virtual")，否则
    // aidl2legacy_AudioPort_audio_port_v7 返回 BAD_VALUE 导致 loadFromAidl 失败。
    // 若显式传入 connection（如 CONNECTION_BT_A2DP），优先采用调用方指定值。
    if (!connection.empty()) {
        ext.device.type.connection = connection;
    } else if (type == AudioDeviceType::IN_SUBMIX || type == AudioDeviceType::OUT_SUBMIX) {
        ext.device.type.connection = AudioDeviceDescription::CONNECTION_VIRTUAL;
    } else {
        ext.device.type.connection = "";  // 无连接类型 = 内置 attached 设备
    }
    ext.device.type.type = type;
    // Hal2AidlMapper 默认端口判定：port.flags tag==input/output 且
    // deviceExt.flags 带 FLAG_INDEX_DEFAULT_DEVICE。只有 speaker（出）与
    // IN_DEFAULT 口（入）打默认位；其余设备口 flags=0，避免覆盖默认端口 id。
    ext.flags = isDefault ? (1 << AudioPortDeviceExt::FLAG_INDEX_DEFAULT_DEVICE) : 0;
    if (!address.empty()) {
        ext.device.address.set<AudioDeviceAddress::id>(address);
    }
    return ext;
}

ModuleImpl::ModuleImpl(Role role) : mRole(role) {
    // 进程内单例：primary 写入、r_submix 读取同一 VirtualDac。
    static std::shared_ptr<cdh::audio::VirtualDac> sDac =
            std::make_shared<cdh::audio::VirtualDac>();
    mDac = sDac;
    buildConfig();
}

const char* ModuleImpl::roleName() const {
    switch (mRole) {
        case Role::Primary:
            return "primary";
        case Role::RemoteSubmix:
            return "r_submix";
        case Role::Bluetooth:
            return "bluetooth";
    }
    return "unknown";
}

void ModuleImpl::buildConfig() {
    switch (mRole) {
        case Role::Primary:
            buildPrimaryConfig();
            break;
        case Role::RemoteSubmix:
            buildSubmixConfig();
            break;
        case Role::Bluetooth:
            buildBluetoothConfig();
            break;
    }
}

void ModuleImpl::buildPrimaryConfig() {
    // 1. out mix 端口（flags= PRIMARY，与 policy 默认输出请求精确匹配）
    AudioPort outMix;
    outMix.id = 1;
    outMix.name = "out";
    outMix.profiles = {makeOutProfile()};
    outMix.flags = makeOutMixFlags();
    outMix.extraAudioDescriptors = {};
    outMix.gains = {};
    outMix.ext.set<AudioPortExt::mix>(makeMixExt(1));

    // 2. speaker 设备端口（默认输出：output 标签 + 默认位，固定 48kHz）
    AudioPort outSpeaker;
    outSpeaker.id = 2;
    outSpeaker.name = "speaker";
    outSpeaker.profiles = {makeSpeakerProfile()};
    outSpeaker.flags = makeOutputFlags();
    outSpeaker.extraAudioDescriptors = {};
    outSpeaker.gains = {};
    outSpeaker.ext.set<AudioPortExt::device>(makeDeviceExt(AudioDeviceType::OUT_SPEAKER, true));

    // 3. in mix 端口（录音）
    AudioPort inMix;
    inMix.id = 3;
    inMix.name = "in";
    inMix.profiles = {makeInProfile()};
    inMix.flags = makeInputFlags();
    inMix.extraAudioDescriptors = {};
    inMix.gains = {};
    inMix.ext.set<AudioPortExt::mix>(makeMixExt(2));

    // 4. 麦克风设备端口（IN_BUILTIN_MIC 默认地址 bottom，policy augmentData 后按 @:bottom 匹配）
    AudioPort inMic;
    inMic.id = 4;
    inMic.name = "mic";
    inMic.profiles = {makeInProfile()};
    inMic.flags = makeInputFlags();
    inMic.extraAudioDescriptors = {};
    inMic.gains = {};
    inMic.ext.set<AudioPortExt::device>(makeDeviceExt(AudioDeviceType::IN_MICROPHONE, false, "bottom"));

    // 5. in-submix 模板设备端口（CONNECTION_VIRTUAL 动态端口，address 为空模板，由 Hal2AidlMapper 连接 "0"）
    AudioPort inSubmix;
    inSubmix.id = 5;
    inSubmix.name = "in_submix";
    inSubmix.profiles = {makeInProfile()};
    inSubmix.flags = makeInputFlags();
    inSubmix.extraAudioDescriptors = {};
    inSubmix.gains = {};
    inSubmix.ext.set<AudioPortExt::device>(makeDeviceExt(AudioDeviceType::IN_SUBMIX, false));

    // 6. out-submix 模板设备端口（CONNECTION_VIRTUAL 动态端口，address 为空模板）
    AudioPort outSubmix;
    outSubmix.id = 6;
    outSubmix.name = "out_submix";
    outSubmix.profiles = {makeOutProfile()};
    outSubmix.flags = makeOutputFlags();
    outSubmix.extraAudioDescriptors = {};
    outSubmix.gains = {};
    outSubmix.ext.set<AudioPortExt::device>(makeDeviceExt(AudioDeviceType::OUT_SUBMIX, false));

    // 7. in-default 设备端口（MIUI policy 对 REMOTE_SUBMIX 内部回退的默认输入端；默认输入位）
    AudioPort inDefault;
    inDefault.id = 8;
    inDefault.name = "microphone_1";
    inDefault.profiles = {makeInProfile()};
    inDefault.flags = makeInputFlags();
    inDefault.extraAudioDescriptors = {};
    inDefault.gains = {};
    inDefault.ext.set<AudioPortExt::device>(makeDeviceExt(AudioDeviceType::IN_DEFAULT, true));

    mPorts = {outMix, outSpeaker, inMix, inMic, inSubmix, outSubmix, inDefault};

    // 路由：播放 out->speaker；录音 mic->in；捕获 in-submix->in；播放镜像 out->out-submix
    AudioRoute playRoute;
    playRoute.sourcePortIds = {1};
    playRoute.sinkPortId = 2;
    playRoute.isExclusive = false;
    AudioRoute recRoute;
    recRoute.sourcePortIds = {4};
    recRoute.sinkPortId = 3;
    recRoute.isExclusive = false;
    AudioRoute inSubmixRoute;
    inSubmixRoute.sourcePortIds = {5};
    inSubmixRoute.sinkPortId = 3;
    inSubmixRoute.isExclusive = false;
    AudioRoute outSubmixRoute;
    outSubmixRoute.sourcePortIds = {1};
    outSubmixRoute.sinkPortId = 6;
    outSubmixRoute.isExclusive = false;
    AudioRoute inDefaultRoute;
    inDefaultRoute.sourcePortIds = {8};
    inDefaultRoute.sinkPortId = 3;
    inDefaultRoute.isExclusive = false;
    mRoutes = {playRoute, recRoute, inSubmixRoute, outSubmixRoute, inDefaultRoute};

    mOutMixPortId = 1;
    mInMixPortId = 3;
}

// r_submix 模块拓扑（对齐 MIUI /vendor/etc/r_submix_audio_policy_configuration.xml：
// 48k stereo 16bit，Remote Submix In/Out 设备；输入流读 VirtualDac 回声）。
// 注意：端口 flags 与 policy 一致保持 0（Hal2AidlMapper 按精确 flags 匹配 mix port）。
void ModuleImpl::buildSubmixConfig() {
    AudioPort outMix;  // 10
    outMix.id = 10;
    outMix.name = "r_submix output";
    outMix.profiles = {makeOutProfile()};
    outMix.flags = makeOutputFlags();
    outMix.extraAudioDescriptors = {};
    outMix.gains = {};
    outMix.ext.set<AudioPortExt::mix>(makeMixExt(10));

    AudioPort outDev;  // 11 Remote Submix Out（CONNECTION_VIRTUAL 模板口，address 为空）
    outDev.id = 11;
    outDev.name = "Remote Submix Out";
    outDev.profiles = {makeOutProfile()};
    outDev.flags = makeOutputFlags();
    outDev.extraAudioDescriptors = {};
    outDev.gains = {};
    outDev.ext.set<AudioPortExt::device>(makeDeviceExt(AudioDeviceType::OUT_SUBMIX, false));

    AudioPort inMix;  // 12 r_submix input（policy 声明 IN_STEREO，用立体声 profile）
    inMix.id = 12;
    inMix.name = "r_submix input";
    inMix.profiles = {makeStereoInProfile()};
    inMix.flags = makeInputFlags();
    inMix.extraAudioDescriptors = {};
    inMix.gains = {};
    inMix.ext.set<AudioPortExt::mix>(makeMixExt(12));

    AudioPort inDev;  // 13 Remote Submix In（CONNECTION_VIRTUAL 模板口，address 为空）
    inDev.id = 13;
    inDev.name = "Remote Submix In";
    inDev.profiles = {makeStereoInProfile()};
    inDev.flags = makeInputFlags();
    inDev.extraAudioDescriptors = {};
    inDev.gains = {};
    inDev.ext.set<AudioPortExt::device>(makeDeviceExt(AudioDeviceType::IN_SUBMIX, false));

    mPorts = {outMix, outDev, inMix, inDev};

    AudioRoute outRoute;
    outRoute.sourcePortIds = {10};
    outRoute.sinkPortId = 11;
    outRoute.isExclusive = false;
    AudioRoute inRoute;
    inRoute.sourcePortIds = {13};
    inRoute.sinkPortId = 12;
    inRoute.isExclusive = false;
    mRoutes = {outRoute, inRoute};

    mOutMixPortId = 10;
    mInMixPortId = 12;
}

// bluetooth 模块拓扑（对齐 MIUI /vendor/etc/bluetooth_audio_policy_configuration_7_0.xml：
// 仅暴露非默认动态 A2DP 模板口，mix port 不带 PRIMARY 标志、设备口不设默认位，
// 确保不与 IModule/default 的 speaker 主输出冲突）。
void ModuleImpl::buildBluetoothConfig() {
    AudioPort outMix;  // 20
    outMix.id = 20;
    outMix.name = "a2dp output";
    outMix.profiles = {makeA2dpProfile()};
    outMix.flags = makeOutputFlags();
    outMix.extraAudioDescriptors = {};
    outMix.gains = {};
    outMix.ext.set<AudioPortExt::mix>(makeMixExt(20));

    AudioPort btOut;  // 21
    btOut.id = 21;
    btOut.name = "BT A2DP Out";
    btOut.profiles = {makeA2dpProfile()};
    btOut.flags = makeOutputFlags();
    btOut.extraAudioDescriptors = {};
    btOut.gains = {};
    btOut.ext.set<AudioPortExt::device>(makeDeviceExt(
            AudioDeviceType::OUT_DEVICE, false, "", AudioDeviceDescription::CONNECTION_BT_A2DP));

    AudioPort btHeadphones;  // 22
    btHeadphones.id = 22;
    btHeadphones.name = "BT A2DP Headphones";
    btHeadphones.profiles = {makeA2dpProfile()};
    btHeadphones.flags = makeOutputFlags();
    btHeadphones.extraAudioDescriptors = {};
    btHeadphones.gains = {};
    btHeadphones.ext.set<AudioPortExt::device>(makeDeviceExt(
            AudioDeviceType::OUT_HEADPHONE, false, "", AudioDeviceDescription::CONNECTION_BT_A2DP));

    AudioPort btSpeaker;  // 23
    btSpeaker.id = 23;
    btSpeaker.name = "BT A2DP Speaker";
    btSpeaker.profiles = {makeA2dpProfile()};
    btSpeaker.flags = makeOutputFlags();
    btSpeaker.extraAudioDescriptors = {};
    btSpeaker.gains = {};
    btSpeaker.ext.set<AudioPortExt::device>(makeDeviceExt(
            AudioDeviceType::OUT_SPEAKER, false, "", AudioDeviceDescription::CONNECTION_BT_A2DP));

    mPorts = {outMix, btOut, btHeadphones, btSpeaker};

    AudioRoute r1;
    r1.sourcePortIds = {20};
    r1.sinkPortId = 21;
    r1.isExclusive = false;
    AudioRoute r2;
    r2.sourcePortIds = {20};
    r2.sinkPortId = 22;
    r2.isExclusive = false;
    AudioRoute r3;
    r3.sourcePortIds = {20};
    r3.sinkPortId = 23;
    r3.isExclusive = false;
    mRoutes = {r1, r2, r3};

    mOutMixPortId = 20;
    mInMixPortId = -1;
}

// ---------------- 配置/元数据 ----------------

ndk::ScopedAStatus ModuleImpl::setModuleDebug(const ModuleDebug&) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleImpl::getAudioPatches(std::vector<AudioPatch>* out) {
    *out = mPatches;
    return ndk::ScopedAStatus::ok();
}

const AudioPort* ModuleImpl::findPort(int32_t portId) const {
    for (const auto& p : mPorts) {
        if (p.id == portId) return &p;
    }
    return nullptr;
}

ndk::ScopedAStatus ModuleImpl::getAudioPort(int32_t portId, AudioPort* out) {
    std::lock_guard<std::mutex> lock(mMutex);
    const AudioPort* port = findPort(portId);
    if (port == nullptr) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    *out = *port;
    CDH_LOG("getAudioPort: %s port=%" PRId32 "(%s)", roleName(),
            portId, out->name.c_str());
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleImpl::getAudioPortConfigs(std::vector<AudioPortConfig>* out) {
    std::lock_guard<std::mutex> lock(mMutex);
    *out = mPortConfigs;
    CDH_LOG("getAudioPortConfigs: %s config_count=%zu", roleName(),
            mPortConfigs.size());
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleImpl::getAudioPorts(std::vector<AudioPort>* out) {
    std::lock_guard<std::mutex> lock(mMutex);
    *out = mPorts;
    CDH_LOG("getAudioPorts: role=%s port_count=%zu", roleName(),
            mPorts.size());
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleImpl::getAudioRoutes(std::vector<AudioRoute>* out) {
    std::lock_guard<std::mutex> lock(mMutex);
    *out = mRoutes;
    CDH_LOG("getAudioRoutes: role=%s route_count=%zu", roleName(),
            mRoutes.size());
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleImpl::getAudioRoutesForAudioPort(int32_t portId,
                                                         std::vector<AudioRoute>* out) {
    std::lock_guard<std::mutex> lock(mMutex);
    out->clear();
    for (const auto& route : mRoutes) {
        if (route.sinkPortId == portId) {
            out->push_back(route);
            continue;
        }
        for (int32_t src : route.sourcePortIds) {
            if (src == portId) {
                out->push_back(route);
                break;
            }
        }
    }
    CDH_LOG("getAudioRoutesForAudioPort: %s port=%" PRId32 " routes=%zu",
            roleName(), portId, out->size());
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleImpl::getSupportedPlaybackRateFactors(
        SupportedPlaybackRateFactors* out) {
    out->minSpeed = 0.5f;
    out->maxSpeed = 2.0f;
    out->minPitch = 0.5f;
    out->maxPitch = 2.0f;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleImpl::getMicrophones(
        std::vector<::aidl::android::media::audio::common::MicrophoneInfo>* out) {
    out->clear();  // 无内置麦克风
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleImpl::getMmapPolicyInfos(
        ::aidl::android::media::audio::common::AudioMMapPolicyType,
        std::vector<::aidl::android::media::audio::common::AudioMMapPolicyInfo>* out) {
    // 非 Primary 模块保持空表；Primary 声明支持 AUTO，配合 AAudio burst 参数使 audioserver 注册 media.aaudio
    out->clear();
    if (mRole == Role::Primary) {
        AudioMMapPolicyInfo info;
        info.device.type.type = AudioDeviceType::OUT_SPEAKER;
        info.mmapPolicy = AudioMMapPolicy::AUTO;
        out->push_back(info);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleImpl::supportsVariableLatency(bool* out) {
    *out = false;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleImpl::getAAudioMixerBurstCount(int32_t* out) {
    *out = 2;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleImpl::getAAudioHardwareBurstMinUsec(int32_t* out) {
    *out = 2000;
    return ndk::ScopedAStatus::ok();
}

// ---------------- 端口/补丁控制 ----------------

ndk::ScopedAStatus ModuleImpl::connectExternalDevice(const AudioPort& requested, AudioPort* out) {
    std::lock_guard<std::mutex> lock(mMutex);
    const AudioPort* tmpl = findPort(requested.id);
    if (tmpl == nullptr || tmpl->ext.getTag() != AudioPortExt::device ||
        tmpl->ext.get<AudioPortExt::device>().device.type.connection.empty()) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    const int32_t tmplId = tmpl->id;
    AudioPort connected = *tmpl;
    if (requested.ext.getTag() == AudioPortExt::device) {
        connected.ext.get<AudioPortExt::device>().device =
                requested.ext.get<AudioPortExt::device>().device;
    }
    if (!requested.profiles.empty()) {
        connected.profiles = requested.profiles;
    }
    const auto& reqDev = connected.ext.get<AudioPortExt::device>().device;
    // 幂等复用：若相同 (device.type, device.address) 已在动态连接集中（如 audioserver 重启重连），直接返回既有实例
    for (const auto& p : mPorts) {
        if (mConnectedExternalPorts.count(p.id) > 0 &&
            p.ext.getTag() == AudioPortExt::device &&
            p.ext.get<AudioPortExt::device>().device == reqDev) {
            *out = p;
            return ndk::ScopedAStatus::ok();
        }
    }
    connected.id = mNextPortId++;
    mPorts.push_back(connected);
    mConnectedExternalPorts.insert(connected.id);

    // 继承模板端口路由：sink 为模板口则克隆一条新路由；source 含模板口则将新端口并入 sourcePortIds
    std::vector<AudioRoute> extraRoutes;
    for (auto& route : mRoutes) {
        if (route.sinkPortId == tmplId) {
            AudioRoute cloned = route;
            cloned.sinkPortId = connected.id;
            extraRoutes.push_back(std::move(cloned));
        }
        if (std::find(route.sourcePortIds.begin(), route.sourcePortIds.end(), tmplId) !=
            route.sourcePortIds.end()) {
            route.sourcePortIds.push_back(connected.id);
        }
    }
    for (auto& r : extraRoutes) {
        mRoutes.push_back(std::move(r));
    }
    *out = connected;
    CDH_LOG("connectExternalDevice: %s tmpl=%" PRId32 " -> connected=%" PRId32,
            roleName(), tmplId, connected.id);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleImpl::disconnectExternalDevice(int32_t portId) {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mConnectedExternalPorts.erase(portId) == 0) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    mPorts.erase(std::remove_if(mPorts.begin(), mPorts.end(),
                                [portId](const AudioPort& p) { return p.id == portId; }),
                 mPorts.end());
    mPortConfigs.erase(
            std::remove_if(mPortConfigs.begin(), mPortConfigs.end(),
                           [portId](const AudioPortConfig& c) { return c.portId == portId; }),
            mPortConfigs.end());
    for (auto& route : mRoutes) {
        route.sourcePortIds.erase(
                std::remove(route.sourcePortIds.begin(), route.sourcePortIds.end(), portId),
                route.sourcePortIds.end());
    }
    mRoutes.erase(std::remove_if(mRoutes.begin(), mRoutes.end(),
                                 [portId](const AudioRoute& r) {
                                     return r.sinkPortId == portId || r.sourcePortIds.empty();
                                 }),
                  mRoutes.end());
    CDH_LOG("disconnectExternalDevice: %s port=%" PRId32,
            roleName(), portId);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleImpl::prepareToDisconnectExternalDevice(int32_t portId) {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mConnectedExternalPorts.count(portId) == 0) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    return ndk::ScopedAStatus::ok();
}

AudioPortConfig* ModuleImpl::findPortConfig(int32_t id) {
    for (auto& c : mPortConfigs) {
        if (c.id == id) return &c;
    }
    return nullptr;
}

const AudioPortConfig* ModuleImpl::findPortConfig(int32_t id) const {
    for (const auto& c : mPortConfigs) {
        if (c.id == id) return &c;
    }
    return nullptr;
}

ndk::ScopedAStatus ModuleImpl::setAudioPortConfig(const AudioPortConfig& requested,
                                                 AudioPortConfig* suggested, bool* applied) {
    std::lock_guard<std::mutex> lock(mMutex);
    const AudioPort* port = findPort(requested.portId);
    if (port == nullptr) {
        CDH_LOG("setAudioPortConfig: %s REJECTED unknown portId=%" PRId32 " (cfgId=%" PRId32 ")",
                roleName(), requested.portId, requested.id);
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    CDH_LOG("setAudioPortConfig: %s port=%" PRId32 "(%s) cfgId=%" PRId32
            " maxActiveStreamCount=%" PRId32 " sampleRate=%" PRId32,
            roleName(), requested.portId, port->name.c_str(),
            requested.id, requested.ext.getTag() == AudioPortExt::mix
                                 ? requested.ext.get<AudioPortExt::mix>().maxActiveStreamCount
                                 : -1,
            requested.sampleRate.has_value() ? requested.sampleRate.value().value : 0);
    if (requested.id != 0) {
        AudioPortConfig* existing = findPortConfig(requested.id);
        if (existing == nullptr) {
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        }
        *existing = requested;
        *suggested = requested;
        *applied = true;
        return ndk::ScopedAStatus::ok();
    }
    // 新建配置：取端口第一个 profile 兜底填 sampleRate/format/channelMask
    AudioPortConfig cfg = requested;
    cfg.id = mNextPortConfigId++;
    if (!cfg.sampleRate.has_value() && !port->profiles.empty()) {
        cfg.sampleRate = ::aidl::android::media::audio::common::Int{
                port->profiles[0].sampleRates.empty() ? 0
                                                      : port->profiles[0].sampleRates[0]};
    }
    if (!cfg.format.has_value() && !port->profiles.empty()) {
        cfg.format = port->profiles[0].format;
    }
    if (!cfg.channelMask.has_value() && !port->profiles.empty() &&
        !port->profiles[0].channelMasks.empty()) {
        cfg.channelMask = port->profiles[0].channelMasks[0];
    }
    if (!cfg.flags.has_value()) {
        cfg.flags = port->flags;
    }
    if (cfg.ext.getTag() == AudioPortExt::Tag::unspecified) {
        cfg.ext = port->ext;
    }
    mPortConfigs.push_back(cfg);
    *suggested = cfg;
    *applied = true;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleImpl::resetAudioPortConfig(int32_t portConfigId) {
    std::lock_guard<std::mutex> lock(mMutex);
    for (auto it = mPortConfigs.begin(); it != mPortConfigs.end(); ++it) {
        if (it->id == portConfigId) {
            mPortConfigs.erase(it);
            return ndk::ScopedAStatus::ok();
        }
    }
    return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
}

ndk::ScopedAStatus ModuleImpl::setAudioPatch(const AudioPatch& requested, AudioPatch* out) {
    std::lock_guard<std::mutex> lock(mMutex);
    // 校验两端 port config 存在
    for (int32_t id : requested.sourcePortConfigIds) {
        if (findPortConfig(id) == nullptr) {
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        }
    }
    for (int32_t id : requested.sinkPortConfigIds) {
        if (findPortConfig(id) == nullptr) {
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        }
    }
    AudioPatch patch = requested;
    if (patch.id != 0) {
        // 更新已存在 patch
        for (auto& p : mPatches) {
            if (p.id == patch.id) {
                p = patch;
                *out = p;
                return ndk::ScopedAStatus::ok();
            }
        }
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    patch.id = mNextPatchId++;
    patch.minimumStreamBufferSizeFrames = 256;
    patch.latenciesMs = std::vector<int32_t>(patch.sinkPortConfigIds.size(), 1);
    mPatches.push_back(patch);
    *out = patch;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleImpl::resetAudioPatch(int32_t patchId) {
    std::lock_guard<std::mutex> lock(mMutex);
    for (auto it = mPatches.begin(); it != mPatches.end(); ++it) {
        if (it->id == patchId) {
            mPatches.erase(it);
            return ndk::ScopedAStatus::ok();
        }
    }
    return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
}

// ---------------- 主音量/静音 ----------------

ndk::ScopedAStatus ModuleImpl::getMasterMute(bool* out) {
    *out = mMasterMute;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleImpl::setMasterMute(bool mute) {
    mMasterMute = mute;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleImpl::getMasterVolume(float* out) {
    *out = mMasterVolume;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleImpl::setMasterVolume(float volume) {
    if (volume < 0.0f || volume > 1.0f) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    mMasterVolume = volume;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleImpl::getMicMute(bool* out) {
    *out = mMicMute;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleImpl::setMicMute(bool mute) {
    mMicMute = mute;
    return ndk::ScopedAStatus::ok();
}

// ---------------- 通知 ----------------

ndk::ScopedAStatus ModuleImpl::updateAudioMode(AudioMode) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleImpl::updateScreenRotation(IModule::ScreenRotation) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleImpl::updateScreenState(bool) {
    return ndk::ScopedAStatus::ok();
}

// ---------------- 可选子接口 ----------------

ndk::ScopedAStatus ModuleImpl::getTelephony(std::shared_ptr<ITelephony>* _aidl_return) {
    *_aidl_return = nullptr;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleImpl::getBluetooth(std::shared_ptr<IBluetooth>* _aidl_return) {
    if (_aidl_return) *_aidl_return = nullptr;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleImpl::getBluetoothA2dp(std::shared_ptr<IBluetoothA2dp>* _aidl_return) {
    if (_aidl_return) *_aidl_return = nullptr;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleImpl::getBluetoothLe(std::shared_ptr<IBluetoothLe>* _aidl_return) {
    if (_aidl_return) *_aidl_return = nullptr;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleImpl::getSoundDose(
        std::shared_ptr<sounddose::ISoundDose>* _aidl_return) {
    if (_aidl_return) *_aidl_return = nullptr;
    return ndk::ScopedAStatus::ok();
}

// ---------------- 流（virtual DAC：输出落盘回环、输入回声） ----------------

namespace {

// 输入流请求缓冲最小值（帧），保证 data FMQ 容量不小于一次 burst。
constexpr int32_t kMinInputBufferFrames = 2048;
// 输出流默认/最小缓冲帧数（对齐真机 PROPERTY_OUTPUT_FRAMES_PER_BUFFER = 256）。
constexpr int32_t kDefaultOutputBufferFrames = 256;
constexpr int32_t kMaxStreamBufferBytes = 1 << 18;  // 256 KiB
constexpr int32_t kInputFrameSizeBytes = 2;         // PCM16 mono（in profile）
constexpr int32_t kOutputFrameSizeBytes = 4;        // PCM16 stereo（out profile）
constexpr int32_t kNominalLatencyMs = 20;

size_t clampBufferBytes(uint64_t requestedBytes, size_t minBytes) {
    size_t bytes = static_cast<size_t>(requestedBytes);
    if (bytes < minBytes) bytes = minBytes;
    if (bytes > kMaxStreamBufferBytes) bytes = kMaxStreamBufferBytes;
    return bytes;
}

}  // namespace

ndk::ScopedAStatus ModuleImpl::openInputStream(const IModule::OpenInputStreamArguments& args,
                                               IModule::OpenInputStreamReturn* ret) {
    std::lock_guard<std::mutex> lock(mMutex);
    const AudioPortConfig* cfg = findPortConfig(args.portConfigId);
    if (cfg == nullptr) {
        CDH_LOG("openInputStream: %s config NOT FOUND cfgId=%" PRId32,
                roleName(), args.portConfigId);
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    const AudioPort* port = findPort(cfg->portId);
    if (port == nullptr || port->ext.getTag() != AudioPortExt::mix || port->id != mInMixPortId) {
        // 只允许在本模块的 in mix 端口上开输入流。
        CDH_LOG("openInputStream: %s REJECTED portId=%" PRId32 " mixId=%" PRId32
                " cfg=%" PRId32 " wantMix=%" PRId32,
                roleName(),
                (port ? port->id : -1), port ? (port->ext.getTag() == AudioPortExt::mix
                                                ? port->ext.get<AudioPortExt::mix>().handle : -1) : -1,
                args.portConfigId, mInMixPortId);
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    // 帧大小由角色决定：primary mono(2B)、r_submix stereo(4B)。
    const int32_t frameSize = (mRole == Role::Primary) ? kInputFrameSizeBytes
                                                       : kOutputFrameSizeBytes;
    if (mInStream) {
        if (mInStream->channel() != nullptr) {
            CDH_LOG("openInputStream: %s REJECTED stream busy", roleName());
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
        }
        mInStream.reset();
    }
    const size_t bufferBytes = clampBufferBytes(
            static_cast<uint64_t>(args.bufferSizeFrames) * frameSize,
            kMinInputBufferFrames * frameSize);
    auto channel = std::make_unique<VdacStreamChannel>(
            bufferBytes, cdh::audio::kSampleRateHz, frameSize, kNominalLatencyMs);
    auto stream = ndk::SharedRefBase::make<VdacStreamIn>(std::move(channel), mDac);
    if (!stream->init()) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
    mInStream = stream;
    CDH_LOG("openInputStream: %s OK cfgId=%" PRId32 " frames=%" PRId64 " buffer=%zu",
            roleName(), args.portConfigId,
            args.bufferSizeFrames, bufferBytes);
    ret->stream = stream;
    ret->desc = StreamDescriptor{};
    stream->channel()->fillDescriptor(&ret->desc);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleImpl::openOutputStream(const IModule::OpenOutputStreamArguments& args,
                                                IModule::OpenOutputStreamReturn* ret) {
    std::lock_guard<std::mutex> lock(mMutex);
    const AudioPortConfig* cfg = findPortConfig(args.portConfigId);
    if (cfg == nullptr) {
        CDH_LOG("openOutputStream: %s config NOT FOUND cfgId=%" PRId32,
                roleName(), args.portConfigId);
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    const AudioPort* port = findPort(cfg->portId);
    if (port == nullptr || port->ext.getTag() != AudioPortExt::mix || port->id != mOutMixPortId) {
        // 只允许在本模块的 out mix 端口上开输出流。
        CDH_LOG("openOutputStream: %s REJECTED portId=%" PRId32 " cfg=%" PRId32 " wantMix=%" PRId32,
                roleName(),
                (port ? port->id : -1), args.portConfigId, mOutMixPortId);
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    if (mOutStream) {
        if (mOutStream->channel() != nullptr) {
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
        }
        mOutStream.reset();
    }
    const int64_t reqFrames =
            args.bufferSizeFrames > 0 ? args.bufferSizeFrames : kDefaultOutputBufferFrames;
    const size_t bufferBytes = clampBufferBytes(
            static_cast<uint64_t>(reqFrames) * kOutputFrameSizeBytes,
            kDefaultOutputBufferFrames * kOutputFrameSizeBytes);
    auto channel = std::make_unique<VdacStreamChannel>(
            bufferBytes, cdh::audio::kSampleRateHz, kOutputFrameSizeBytes, kNominalLatencyMs);
    // 对接 QEMU intel-hda 声卡：ALSA write 按声卡消费节奏阻塞，为 AudioFlinger
    // 提供真实时钟背压（修复音频时钟偏快 1.25x -> 在线播放 2 倍速快进）。
    // 设备固定 PCM16 / 48kHz / stereo（VirtualDac 同参）；打开失败回退纯内存
    // VirtualDac（REMOTE_SUBMIX 捕获不受影响），不阻断播放。
    auto alsa = std::make_unique<cdh::audio::AlsaPcm>();
    if (!alsa->open("/dev/snd/pcmC0D0p", cdh::audio::kSampleRateHz,
                    cdh::audio::kChannelCount)) {
        alsa.reset();
    }
    auto stream = ndk::SharedRefBase::make<VdacStreamOut>(std::move(channel), mDac,
                                                          std::move(alsa));
    if (!stream->init()) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
    mOutStream = stream;
    CDH_LOG("openOutputStream: %s OK cfgId=%" PRId32 " frames=%" PRId64 " buffer=%zu",
            roleName(), args.portConfigId,
            args.bufferSizeFrames, bufferBytes);
    ret->stream = stream;
    ret->desc = StreamDescriptor{};
    stream->channel()->fillDescriptor(&ret->desc);
    return ndk::ScopedAStatus::ok();
}

// ---------------- 其它 ----------------

ndk::ScopedAStatus ModuleImpl::generateHwAvSyncId(int32_t* out) {
    *out = mNextHwAvSyncId++;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleImpl::getVendorParameters(const std::vector<std::string>&,
                                                  std::vector<VendorParameter>*) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus ModuleImpl::setVendorParameters(const std::vector<VendorParameter>&, bool) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus ModuleImpl::addDeviceEffect(
        int32_t, const std::shared_ptr<::aidl::android::hardware::audio::effect::IEffect>&) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus ModuleImpl::removeDeviceEffect(
        int32_t, const std::shared_ptr<::aidl::android::hardware::audio::effect::IEffect>&) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

}  // namespace aidl::android::hardware::audio::core