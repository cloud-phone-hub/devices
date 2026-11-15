/*
 * Copyright (C) 2026 cloud-phone-hub
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * IEffect Factory 实现：支持从 /vendor/etc/audio_effects_descriptors.xml（或环境变量
 * AUDIO_EFFECTS_DESCRIPTORS_XML）按机型加载音效描述符表，供 AudioEffect.queryEffects()
 * 返回与真机一致的音效清单；未提供该 XML 时保持空列表回退，不内置任何机型常量。
 */

#include "EffectFactoryImpl.h"

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <string>
#include <vector>

#include <android/log.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

using ::aidl::android::media::audio::common::AudioUuid;

namespace aidl::android::hardware::audio::effect {

namespace {

constexpr char kLogTag[] = "cdh_audio_effect";
constexpr char kDefaultDescriptorsXml[] = "/vendor/etc/audio_effects_descriptors.xml";

int hexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// 解析标准 36 字符 UUID（xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx）到 AudioUuid。
bool parseAudioUuid(const std::string& text, AudioUuid* out) {
    if (out == nullptr) return false;
    uint8_t bytes[16] = {0};
    size_t idx = 0;
    for (size_t i = 0; i < text.size();) {
        if (text[i] == '-' || std::isspace(static_cast<unsigned char>(text[i]))) {
            ++i;
            continue;
        }
        if (idx >= 16 || i + 1 >= text.size()) return false;
        int hi = hexNibble(text[i]);
        int lo = hexNibble(text[i + 1]);
        if (hi < 0 || lo < 0) return false;
        bytes[idx++] = static_cast<uint8_t>((hi << 4) | lo);
        i += 2;
    }
    if (idx != 16) return false;

    uint32_t timeLow = (static_cast<uint32_t>(bytes[0]) << 24) |
                       (static_cast<uint32_t>(bytes[1]) << 16) |
                       (static_cast<uint32_t>(bytes[2]) << 8) |
                       static_cast<uint32_t>(bytes[3]);
    uint16_t timeMid = static_cast<uint16_t>((bytes[4] << 8) | bytes[5]);
    uint16_t timeHi = static_cast<uint16_t>((bytes[6] << 8) | bytes[7]);
    uint16_t clockSeq = static_cast<uint16_t>((bytes[8] << 8) | bytes[9]);

    out->timeLow = static_cast<int32_t>(timeLow);
    out->timeMid = static_cast<int32_t>(timeMid);
    out->timeHiAndVersion = static_cast<int32_t>(timeHi);
    out->clockSeq = static_cast<int32_t>(clockSeq);
    out->node.assign(bytes + 10, bytes + 16);
    return true;
}

Flags::Type parseConnectMode(const std::string& mode) {
    if (mode == "Auxiliary" || mode == "AUXILIARY" || mode == "auxiliary") {
        return Flags::Type::AUXILIARY;
    }
    if (mode == "Pre Processing" || mode == "PRE_PROC" || mode == "pre_proc") {
        return Flags::Type::PRE_PROC;
    }
    if (mode == "Post Processing" || mode == "POST_PROC" || mode == "post_proc") {
        return Flags::Type::POST_PROC;
    }
    if (mode == "Replace" || mode == "REPLACE" || mode == "replace") {
        return Flags::Type::REPLACE;
    }
    return Flags::Type::INSERT;
}

std::string xmlAttr(const xmlNode* node, const char* attr) {
    if (node == nullptr) return {};
    xmlChar* val = xmlGetProp(const_cast<xmlNode*>(node), BAD_CAST attr);
    if (val == nullptr) return {};
    std::string out(reinterpret_cast<const char*>(val));
    xmlFree(val);
    return out;
}

const std::vector<Descriptor>& getConfiguredEffects() {
    static std::once_flag sOnce;
    static std::vector<Descriptor> sEffects;
    std::call_once(sOnce, []() {
        xmlInitParser();
        const char* envPath = std::getenv("AUDIO_EFFECTS_DESCRIPTORS_XML");
        const char* xmlPath = (envPath && *envPath) ? envPath : kDefaultDescriptorsXml;
        xmlDoc* doc = xmlReadFile(xmlPath, nullptr,
                                  XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
        if (doc == nullptr) {
            return;
        }
        xmlNode* root = xmlDocGetRootElement(doc);
        for (xmlNode* cur = root ? root->children : nullptr; cur != nullptr; cur = cur->next) {
            if (cur->type != XML_ELEMENT_NODE ||
                xmlStrcmp(cur->name, BAD_CAST "effect") != 0) {
                continue;
            }
            const std::string name = xmlAttr(cur, "name");
            const std::string implementor = xmlAttr(cur, "implementor");
            const std::string typeStr = xmlAttr(cur, "type");
            const std::string uuidStr = xmlAttr(cur, "uuid");
            const std::string proxyStr = xmlAttr(cur, "proxy");
            std::string modeStr = xmlAttr(cur, "connectMode");
            if (modeStr.empty()) {
                modeStr = xmlAttr(cur, "connect");
            }

            AudioUuid typeUuid{};
            AudioUuid implUuid{};
            if (name.empty() || !parseAudioUuid(typeStr, &typeUuid) ||
                !parseAudioUuid(uuidStr, &implUuid)) {
                continue;
            }

            Descriptor desc{};
            desc.common.id.type = typeUuid;
            desc.common.id.uuid = implUuid;
            AudioUuid proxyUuid{};
            if (!proxyStr.empty() && parseAudioUuid(proxyStr, &proxyUuid)) {
                desc.common.id.proxy = proxyUuid;
            }
            desc.common.flags.type = parseConnectMode(modeStr);
            desc.common.name = name;
            desc.common.implementor = implementor;
            sEffects.push_back(std::move(desc));
        }
        xmlFreeDoc(doc);
        __android_log_print(ANDROID_LOG_INFO, kLogTag,
                            "已从 %s 加载 %zu 项音效描述符", xmlPath, sEffects.size());
    });
    return sEffects;
}

}  // namespace

ndk::ScopedAStatus EffectFactoryImpl::queryEffects(
        const std::optional<AudioUuid>& in_type,
        const std::optional<AudioUuid>& in_implementation,
        const std::optional<AudioUuid>& in_proxy,
        std::vector<Descriptor>* out) {
    out->clear();
    const auto& all = getConfiguredEffects();
    for (const auto& desc : all) {
        if (in_type.has_value() && !(desc.common.id.type == *in_type)) {
            continue;
        }
        if (in_implementation.has_value() && !(desc.common.id.uuid == *in_implementation)) {
            continue;
        }
        if (in_proxy.has_value()) {
            if (!desc.common.id.proxy.has_value() || !(*desc.common.id.proxy == *in_proxy)) {
                continue;
            }
        }
        out->push_back(desc);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus EffectFactoryImpl::queryProcessing(
        const std::optional<Processing::Type>&, std::vector<Processing>* out) {
    out->clear();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus EffectFactoryImpl::createEffect(const AudioUuid&, std::shared_ptr<IEffect>*) {
    // 仅注册静态描述符查询；具体音效实例不接管音频流，返回非法参数使框架优雅降级为直通。
    return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
}

ndk::ScopedAStatus EffectFactoryImpl::destroyEffect(const std::shared_ptr<IEffect>&) {
    return ndk::ScopedAStatus::ok();
}

}  // namespace aidl::android::hardware::audio::effect