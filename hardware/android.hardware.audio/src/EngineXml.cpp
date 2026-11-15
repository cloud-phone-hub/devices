/*
 * Copyright (C) 2026 cloud-phone-hub
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * 引擎 XML 解析实现（libxml2 DOM；旧布局的 XInclude 仍兼容）。
 * 入口为合并版单文件 /vendor/etc/audio_config.xml（原 4 个 AOSP 文件已内联）。
 * 兼容两种元素写法：
 *   旧式（小米/AOSP 现行设备）  <volumeGroup><name>music</name><indexMin>0</indexMin>…
 *   新式（XSD 生成）            <volumeGroup name="music" minIndex="0" …>
 * 曲线点同理：<point>1,-5800</point> 与 <point index="1" attenuationMb="-5800"/>。
 */

#include "EngineXml.h"

#include <cctype>
#include <cstdlib>
#include <map>
#include <mutex>
#include <utility>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xinclude.h>

namespace cdh::audio {
namespace {

using Points = std::vector<CurvePoint>;
using RefTable = std::map<std::string, Points>;

constexpr const char* kStreamPrefix = "AUDIO_STREAM_";
constexpr const char* kCategoryPrefix = "DEVICE_CATEGORY_";

bool isElement(const xmlNode* node, const char* name) {
    return node != nullptr && node->type == XML_ELEMENT_NODE &&
           xmlStrcmp(node->name, BAD_CAST name) == 0;
}

std::string trim(std::string s) {
    size_t a = 0;
    size_t b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

std::string nodeText(xmlNode* node) {
    xmlChar* content = xmlNodeGetContent(node);
    if (content == nullptr) return {};
    std::string out(reinterpret_cast<const char*>(content));
    xmlFree(content);
    return trim(out);
}

std::string prop(xmlNode* node, const char* name) {
    xmlChar* value = xmlGetProp(node, BAD_CAST name);
    if (value == nullptr) return {};
    std::string out(reinterpret_cast<const char*>(value));
    xmlFree(value);
    return trim(out);
}

int32_t parseInt(const std::string& s) {
    if (s.empty()) return 0;
    return static_cast<int32_t>(strtol(s.c_str(), nullptr, 10));
}

int8_t clampIndex(int32_t idx) {
    if (idx < 0) return 0;
    if (idx > 100) return 100;
    return static_cast<int8_t>(idx);
}

// 旧式 <point>idx,att</point> 或新式 <point index="" attenuationMb=""/>。
bool parsePoint(xmlNode* node, CurvePoint* out) {
    std::string text = nodeText(node);
    if (!text.empty()) {
        size_t comma = text.find(',');
        if (comma == std::string::npos) return false;
        out->index = clampIndex(parseInt(trim(text.substr(0, comma))));
        out->attenuationMb = parseInt(trim(text.substr(comma + 1)));
        return true;
    }
    std::string idx = prop(node, "index");
    if (idx.empty()) return false;
    out->index = clampIndex(parseInt(idx));
    out->attenuationMb = parseInt(prop(node, "attenuationMb"));
    return true;
}

void collectPoints(xmlNode* first, Points* out) {
    for (xmlNode* cur = first; cur != nullptr; cur = cur->next) {
        if (!isElement(cur, "point")) continue;
        CurvePoint point;
        if (parsePoint(cur, &point)) out->push_back(point);
    }
}

// <reference name="DEFAULT_MEDIA_VOLUME_CURVE"><point>…</point></reference>
void collectReferences(xmlNode* first, RefTable* refs) {
    for (xmlNode* cur = first; cur != nullptr; cur = cur->next) {
        if (cur->type != XML_ELEMENT_NODE) continue;
        if (isElement(cur, "reference")) {
            std::string name = prop(cur, "name");
            if (!name.empty()) {
                Points points;
                collectPoints(cur->children, &points);
                if (!points.empty()) (*refs)[name] = std::move(points);
            }
        }
        collectReferences(cur->children, refs);
    }
}

std::string stripCategoryPrefix(std::string category) {
    const size_t prefixLen = std::string(kCategoryPrefix).size();
    if (category.size() > prefixLen && category.compare(0, prefixLen, kCategoryPrefix) == 0) {
        return category.substr(prefixLen);
    }
    return category;
}

// audioserver 内置策略用 AUDIO_STREAM_<X> 做卷组名，设备 XML 里通常是 music / voice_call 这类
// 小写流名；返回给 AIDL 前统一补前缀，否则内置策略找不到卷组（音量组塌缩为 min=max=0）。
std::string normalizeGroupName(const std::string& name) {
    if (name.rfind(kStreamPrefix, 0) == 0) return name;
    std::string out = kStreamPrefix;
    for (char c : name) {
        out += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return out;
}

bool parseCurve(xmlNode* node, const RefTable& refs, VolumeCurve* out) {
    out->deviceCategory = stripCategoryPrefix(prop(node, "deviceCategory"));
    if (out->deviceCategory.empty()) return false;

    const std::string ref = prop(node, "ref");
    if (!ref.empty()) {
        auto it = refs.find(ref);
        if (it == refs.end()) return false;  // 引用了未知命名曲线：跳过该设备类别
        out->curvePoints = it->second;
        return !out->curvePoints.empty();
    }
    collectPoints(node->children, &out->curvePoints);
    return !out->curvePoints.empty();
}

VolumeGroup parseGroup(xmlNode* node, const RefTable& refs) {
    VolumeGroup group;
    group.name = prop(node, "name");
    std::string minText = prop(node, "minIndex");
    std::string maxText = prop(node, "maxIndex");

    for (xmlNode* cur = node->children; cur != nullptr; cur = cur->next) {
        if (cur->type != XML_ELEMENT_NODE) continue;
        if (isElement(cur, "name")) {
            group.name = nodeText(cur);
        } else if (isElement(cur, "indexMin")) {
            minText = nodeText(cur);
        } else if (isElement(cur, "indexMax")) {
            maxText = nodeText(cur);
        } else if (isElement(cur, "volume")) {
            VolumeCurve curve;
            if (parseCurve(cur, refs, &curve)) group.volumeCurves.push_back(std::move(curve));
        }
    }
    group.minIndex = parseInt(minText);
    group.maxIndex = parseInt(maxText);
    group.name = normalizeGroupName(group.name);
    return group;
}

void collectGroups(xmlNode* first, const RefTable& refs, std::vector<VolumeGroup>* out) {
    for (xmlNode* cur = first; cur != nullptr; cur = cur->next) {
        if (cur->type != XML_ELEMENT_NODE) continue;
        if (isElement(cur, "volumeGroup")) {
            VolumeGroup group = parseGroup(cur, refs);
            if (!group.name.empty() && group.name != kStreamPrefix) out->push_back(std::move(group));
        }
        collectGroups(cur->children, refs, out);
    }
}

// STRATEGY_CALL_ASSISTANT 是 AOSP 内置策略之一，但小米 XML 没有对应 volumeGroup；
// 缺了会让该策略无曲线可用，因此按 assistant 组复制一份补齐（与原 cfg 的 13 组一致）。
void addCallAssistantIfMissing(std::vector<VolumeGroup>* groups) {
    const VolumeGroup* assistant = nullptr;
    for (const auto& group : *groups) {
        if (group.name == "AUDIO_STREAM_CALL_ASSISTANT") return;
        if (group.name == "AUDIO_STREAM_ASSISTANT") assistant = &group;
    }
    if (assistant == nullptr) return;
    VolumeGroup clone = *assistant;
    clone.name = "AUDIO_STREAM_CALL_ASSISTANT";
    groups->push_back(std::move(clone));
}

}  // namespace

bool loadEngineXml(const std::string& path, std::vector<VolumeGroup>* out, std::string* err) {
    static std::once_flag initOnce;
    std::call_once(initOnce, [] { xmlInitParser(); });

    // XML_PARSE_NONET：设备上不解析网络实体；错误由本函数返回，不刷 stderr。
    xmlDoc* doc = xmlReadFile(path.c_str(), nullptr,
                              XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
    if (doc == nullptr) {
        if (err) *err = "无法读取/解析 " + path;
        return false;
    }
    bool includeFailed = false;

    if (xmlXIncludeProcess(doc) < 0) {
        // 个别 include 缺失时不直接放弃：已展开的部分仍可用，最终按有无 volumeGroup 判定。
        includeFailed = true;
    }

    xmlNode* root = xmlDocGetRootElement(doc);
    RefTable refs;
    std::vector<VolumeGroup> groups;
    if (root != nullptr) {
        collectReferences(root, &refs);
        collectGroups(root, refs, &groups);
    }
    xmlFreeDoc(doc);

    if (groups.empty()) {
        if (err) *err = (includeFailed ? "xi:include 展开失败 " : "没有任何 volumeGroup: ") + path;
        return false;
    }
    addCallAssistantIfMissing(&groups);
    *out = std::move(groups);
    return true;
}

std::vector<std::string> loadConfiguredModules(const std::string& path) {
    static std::once_flag initOnce;
    std::call_once(initOnce, [] { xmlInitParser(); });

    std::vector<std::string> modules;
    xmlDoc* doc = xmlReadFile(path.c_str(), nullptr,
                              XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
    if (doc != nullptr) {
        xmlNode* root = xmlDocGetRootElement(doc);
        if (root != nullptr) {
            std::string raw = prop(root, "modules");
            size_t start = 0;
            while (start <= raw.size()) {
                size_t comma = raw.find(',', start);
                std::string token = (comma == std::string::npos)
                                            ? raw.substr(start)
                                            : raw.substr(start, comma - start);
                token = trim(std::move(token));
                if (!token.empty()) {
                    bool seen = false;
                    for (const auto& existing : modules) {
                        if (existing == token) {
                            seen = true;
                            break;
                        }
                    }
                    if (!seen) modules.push_back(std::move(token));
                }
                if (comma == std::string::npos) break;
                start = comma + 1;
            }
        }
        xmlFreeDoc(doc);
    }

    if (modules.empty()) {
        return {"default", "primary", "r_submix"};
    }
    return modules;
}

}  // namespace cdh::audio
