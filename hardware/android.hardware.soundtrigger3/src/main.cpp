/*
 * cdh：android.hardware.soundtrigger3 服务入口。
 *
 * 注册名必须是 <descriptor>/default，与 ColorOS vendor manifest 里的
 * <fqname>ISoundTriggerHw/default</fqname> 对齐；framework 在 AIDL 路径
 * （AServiceManager_isDeclared 为真时）会 waitForService 无超时等待，所以本
 * 服务必须在 system_server 走到 StartSoundTriggerMiddlewareService 之前就位
 * —— 靠 rc 里的 class hal 保证（servicemanager 之后、boot phase 200 之前）。
 */
#include <android/binder_ibinder.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <log/log.h>

#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "SoundTriggerHw.h"

using aidl::android::hardware::soundtrigger3::SoundTriggerHw;

extern "C" {
void AIBinder_markVintfStability(AIBinder* binder);
void AIBinder_Class_disableInterfaceTokenHeader(AIBinder_Class* clazz);
}

namespace {

void* OnPlaceholderCreate(void* /*args*/) {
    return reinterpret_cast<void*>(1);
}

void OnPlaceholderDestroy(void* /*user_data*/) {}

binder_status_t OnPlaceholderTransact(AIBinder* /*binder*/,
                                      transaction_code_t /*code*/,
                                      const AParcel* /*in*/,
                                      AParcel* /*out*/) {
    return STATUS_UNKNOWN_TRANSACTION;
}

std::string TrimWhitespace(const std::string& input) {
    const std::string::size_type first = input.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const std::string::size_type last = input.find_last_not_of(" \t\r\n");
    return input.substr(first, last - first + 1);
}

// 按行读取配置文件（C stdio），在 hal_audio_default 域下注册音频相关未实现服务空桩。
void RegisterPlaceholdersFromFile(const char* path) {
    FILE* input = fopen(path, "re");
    if (input == nullptr) {
        return;
    }
    static std::map<std::string, AIBinder_Class*> class_cache;
    static std::vector<AIBinder*> placeholder_refs;

    char* raw = nullptr;
    size_t capacity = 0;
    ssize_t length = 0;
    int registered = 0;
    while ((length = getline(&raw, &capacity, input)) >= 0) {
        const std::string name =
                TrimWhitespace(std::string(raw, static_cast<size_t>(length)));
        if (name.empty() || name[0] == '#') {
            continue;
        }
        if (AIBinder* existing = AServiceManager_checkService(name.c_str())) {
            AIBinder_decStrong(existing);
            continue;
        }
        const std::string::size_type slash = name.find('/');
        const std::string descriptor =
                slash == std::string::npos ? name : name.substr(0, slash);
        if (descriptor.empty()) {
            continue;
        }
        auto it = class_cache.find(descriptor);
        if (it == class_cache.end()) {
            it = class_cache.emplace(descriptor, nullptr).first;
            it->second = AIBinder_Class_define(
                    it->first.c_str(), OnPlaceholderCreate, OnPlaceholderDestroy,
                    OnPlaceholderTransact);
            if (it->second == nullptr) {
                ALOGW("define placeholder binder class failed: %s",
                      descriptor.c_str());
                class_cache.erase(it);
                continue;
            }
            AIBinder_Class_disableInterfaceTokenHeader(it->second);
        }
        AIBinder* binder = AIBinder_new(it->second, nullptr);
        if (binder == nullptr) {
            ALOGW("allocate placeholder binder failed: %s", name.c_str());
            continue;
        }
        AIBinder_markVintfStability(binder);
        binder_exception_t status = AServiceManager_addService(binder, name.c_str());
        if (status != EX_NONE) {
            ALOGW("register placeholder service failed: %s (%d)",
                  name.c_str(), status);
            AIBinder_decStrong(binder);
            continue;
        }
        placeholder_refs.push_back(binder);
        ++registered;
    }
    free(raw);
    fclose(input);
    ALOGI("registered %d audio placeholder services from %s", registered, path);
}

}  // namespace

int main() {
    ABinderProcess_setThreadPoolMaxThreadCount(2);
    ABinderProcess_startThreadPool();

    std::shared_ptr<SoundTriggerHw> hw = ndk::SharedRefBase::make<SoundTriggerHw>();
    const std::string instance = std::string() + SoundTriggerHw::descriptor + "/default";

    binder_exception_t status = AServiceManager_addService(hw->asBinder().get(), instance.c_str());
    if (status != EX_NONE) {
        ALOGE("failed to register %s (status=%d)", instance.c_str(), status);
        return 1;
    }
    ALOGI("registered %s (stub, 0 modules)", instance.c_str());
    RegisterPlaceholdersFromFile("/vendor/etc/cdh_audio_placeholders.txt");

    ABinderProcess_joinThreadPool();
    return 0;
}
