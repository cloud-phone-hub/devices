/*
 * Copyright (C) 2026 cloud-phone-hub
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * IHalAdapterVendorExtension/default 兼容服务：
 * 承载 android.media.audio.IHalAdapterVendorExtension/default，桥接 MTK 等厂商系统侧
 * AudioSystem::getParameters("APP_GET_FO#<name>") 与 AIDL Audio HAL getVendorParameters，
 * 消除 libaudioutilmtk.so 因收到空字符串触发 std::stoi("") 未捕获异常导致 audioserver 崩溃。
 */

#include <android/binder_auto_utils.h>
#include <android/binder_ibinder.h>
#include <android/binder_manager.h>
#include <android/binder_parcel.h>
#include <android/binder_parcel_utils.h>
#include <android/binder_process.h>
#include <android/log.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

// 本二进制经 cdh_redirect 替代 /system_ext/bin/hw/android.hardware.audio.parameter_parser.service
// 运行在 system linker namespace，直接动态链接官方 /system/lib64/libc++.so；Android 14/15
// 官方 libc++.so（LLVM 17）尚未导出 LLVM 18+ 头文件引用的 __libcpp_verbose_abort，
// 此处提供进程内隐藏定义以保持跨代 ABI 自包含。
#ifndef _LIBCPP_VERBOSE_ABORT_NOEXCEPT
#define _LIBCPP_VERBOSE_ABORT_NOEXCEPT noexcept
#endif
#if !defined(__ANDROID_API__) || __ANDROID_API__ >= 35
namespace std {
inline namespace __1 {
__attribute__((weak, noreturn)) void __libcpp_verbose_abort(const char* format, ...) _LIBCPP_VERBOSE_ABORT_NOEXCEPT {
    (void)format;
    __builtin_trap();
}
}  // namespace __1
}  // namespace std
#endif

namespace {

constexpr const char* kLogTag = "cdh_audio_param";
constexpr const char* kDescriptor = "android.media.audio.IHalAdapterVendorExtension";
constexpr const char* kInstanceName = "android.media.audio.IHalAdapterVendorExtension/default";
constexpr const char* kAppGetFoPrefix = "APP_GET_FO#";

std::string Trim(const std::string& s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

// 尝试从 /vendor/etc/audio_param/AudioParamOptions*.xml 提取非空 <Param name="..." value="..."/>；
// 若运行在无 vendor_configs_file 读取权限的受限域或文件缺失，自动回退内置安全默认表。
const std::unordered_map<std::string, std::string>& GetMtkFeatureDefaults() {
    static const std::unordered_map<std::string, std::string> kMap = []() {
        std::unordered_map<std::string, std::string> m;
        for (const char* path : {
                 "/vendor/etc/audio_param/AudioParamOptions_vext.xml",
                 "/vendor/etc/audio_param/AudioParamOptions.xml",
                 "/odm/etc/audio_param/AudioParamOptions.xml",
             }) {
            std::FILE* fp = std::fopen(path, "r");
            if (fp == nullptr) continue;
            char buf[1024];
            while (std::fgets(buf, sizeof(buf), fp) != nullptr) {
                std::string line(buf);
                auto np = line.find("name=\"");
                if (np == std::string::npos) continue;
                np += 6;
                auto ne = line.find('"', np);
                if (ne == std::string::npos) continue;
                auto vp = line.find("value=\"", ne);
                if (vp == std::string::npos) continue;
                vp += 7;
                auto ve = line.find('"', vp);
                if (ve == std::string::npos) continue;
                std::string name = line.substr(np, ne - np);
                std::string val = line.substr(vp, ve - vp);
                // 仅透传纯十进制整数参数（供 FeatureOption::getValues 等 std::stoi 消费），
                // 不开启 value="yes"/"true" 的厂商专有硬件 DSP/Aurisys 开关，保持标准软件音频通路。
                bool isIntVal = !val.empty();
                for (size_t idx = 0; idx < val.size(); ++idx) {
                    const char c = val[idx];
                    if (idx == 0 && (c == '-' || c == '+') && val.size() > 1) continue;
                    if (c < '0' || c > '9') {
                        isIntVal = false;
                        break;
                    }
                }
                if (!name.empty() && isIntVal && m.find(name) == m.end()) {
                    m.emplace(std::move(name), std::move(val));
                }
            }
            std::fclose(fp);
        }
        // 保底数值型 FeatureOption，防止受限 SELinux 域无法读取 XML 时返回空串触发 std::stoi("")
        if (m["MTK_AUDIO_NUMBER_OF_SPEAKER"].empty()) {
            m["MTK_AUDIO_NUMBER_OF_SPEAKER"] = "2";
        }
        return m;
    }();
    return kMap;
}

void* OnCreate(void* /*args*/) {
    return reinterpret_cast<void*>(1);
}

void OnDestroy(void* /*user_data*/) {}

// IHalAdapterVendorExtension AIDL 事务处理（unstable AIDL，事务号 FIRST_CALL_TRANSACTION + 0..4）：
//   +0 (1): parseVendorParameterIds(ParameterScope scope, String rawKeys) -> String[]
//   +1 (2): parseVendorParameters(ParameterScope scope, String rawKeysAndValues,
//                                 out VendorParameter[] syncParams, out VendorParameter[] asyncParams)
//   +2 (3): parseBluetoothA2dpReconfigureOffload(String rawValue) -> VendorParameter[]
//   +3 (4): parseBluetoothLeReconfigureOffload(String rawValue) -> VendorParameter[]
//   +4 (5): processVendorParameters(ParameterScope scope, VendorParameter[] params) -> String
binder_status_t OnTransact(AIBinder* /*binder*/,
                           transaction_code_t code,
                           const AParcel* in,
                           AParcel* out) {
    if (out == nullptr) {
        return STATUS_BAD_VALUE;
    }
    ndk::ScopedAStatus ok = ndk::ScopedAStatus::ok();

    if (code == FIRST_CALL_TRANSACTION + 0) {
        int8_t scope = 0;
        std::string rawKeys;
        if (in != nullptr) {
            AParcel_readByte(in, &scope);
            ndk::AParcel_readString(in, &rawKeys);
        }
        std::vector<std::string> ids;
        size_t pos = 0;
        while (pos <= rawKeys.size()) {
            size_t semi = rawKeys.find(';', pos);
            std::string tok = Trim(semi == std::string::npos ? rawKeys.substr(pos)
                                                             : rawKeys.substr(pos, semi - pos));
            if (tok.rfind(kAppGetFoPrefix, 0) == 0) {
                ids.push_back(std::move(tok));
            }
            if (semi == std::string::npos) break;
            pos = semi + 1;
        }
        binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
        if (st != STATUS_OK) return st;
        return ndk::AParcel_writeVector(out, ids);
    }

    if (code == FIRST_CALL_TRANSACTION + 1) {
        binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
        if (st != STATUS_OK) return st;
        AParcel_writeInt32(out, 0);  // syncParameters empty
        AParcel_writeInt32(out, 0);  // asyncParameters empty
        return STATUS_OK;
    }

    if (code == FIRST_CALL_TRANSACTION + 2 || code == FIRST_CALL_TRANSACTION + 3) {
        binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
        if (st != STATUS_OK) return st;
        AParcel_writeInt32(out, 0);  // parameters empty
        return STATUS_OK;
    }

    if (code == FIRST_CALL_TRANSACTION + 4) {
        int8_t scope = 0;
        int32_t count = 0;
        std::vector<std::string> ids;
        if (in != nullptr) {
            AParcel_readByte(in, &scope);
            if (AParcel_readInt32(in, &count) == STATUS_OK && count > 0 && count <= 256) {
                ids.reserve(static_cast<size_t>(count));
                for (int32_t i = 0; i < count; ++i) {
                    int32_t nonNull = 0;
                    if (AParcel_readInt32(in, &nonNull) != STATUS_OK) break;
                    if (nonNull == 0) continue;
                    const int32_t startPos = AParcel_getDataPosition(in);
                    int32_t parcelSize = 0;
                    if (AParcel_readInt32(in, &parcelSize) != STATUS_OK || parcelSize < 4) break;
                    std::string id;
                    ndk::AParcel_readString(in, &id);
                    if (!id.empty()) {
                        ids.push_back(std::move(id));
                    }
                    AParcel_setDataPosition(in, startPos + parcelSize);
                }
            }
        }

        const auto& defaults = GetMtkFeatureDefaults();
        std::string result;
        for (const auto& id : ids) {
            if (id.rfind(kAppGetFoPrefix, 0) != 0) continue;
            const std::string featName = id.substr(11);
            auto it = defaults.find(featName);
            if (it == defaults.end() || it->second.empty()) continue;
            if (!result.empty()) result.push_back(';');
            result.append(id);
            result.push_back('=');
            result.append(it->second);
        }
        binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
        if (st != STATUS_OK) return st;
        return ndk::AParcel_writeString(out, result);
    }

    return STATUS_UNKNOWN_TRANSACTION;
}

}  // namespace

int main() {
    ABinderProcess_setThreadPoolMaxThreadCount(4);
    ABinderProcess_startThreadPool();

    AIBinder_Class* cls = AIBinder_Class_define(kDescriptor, OnCreate, OnDestroy, OnTransact);
    if (cls == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "AIBinder_Class_define failed");
        return 1;
    }
    ndk::SpAIBinder binder(AIBinder_new(cls, nullptr));
    binder_status_t status = AServiceManager_addService(binder.get(), kInstanceName);
    if (status == STATUS_OK) {
        __android_log_print(ANDROID_LOG_INFO, kLogTag, "registered %s", kInstanceName);
    } else {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "FAILED to register %s (status=%d)",
                            kInstanceName, status);
        return 1;
    }

    ABinderProcess_joinThreadPool();
    return 1;
}
