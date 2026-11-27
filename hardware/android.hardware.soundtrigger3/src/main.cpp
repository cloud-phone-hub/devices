/*
 * cdh：android.hardware.soundtrigger3 服务入口。
 *
 * 注册名必须是 <descriptor>/default，与 ColorOS vendor manifest 里的
 * <fqname>ISoundTriggerHw/default</fqname> 对齐；framework 在 AIDL 路径
 * （AServiceManager_isDeclared 为真时）会 waitForService 无超时等待，所以本
 * 服务必须在 system_server 走到 StartSoundTriggerMiddlewareService 之前就位
 * —— 靠 rc 里的 class hal 保证（servicemanager 之后、boot phase 200 之前）。
 */
#include <android/binder_auto_utils.h>
#include <android/binder_ibinder.h>
#include <android/binder_manager.h>
#include <android/binder_parcel.h>
#include <android/binder_process.h>
#include <android/binder_status.h>
#include <log/log.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
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

constexpr const char* kBluetoothAudioProviderFactoryDescriptor =
        "android.hardware.bluetooth.audio.IBluetoothAudioProviderFactory";
constexpr const char* kBluetoothAudioProviderFactoryHash =
        "ac25616dda1c45dc2915d3f5ac82687a1a6f6e46";

// 蓝牙协议栈 libbluetooth_jni.so 在 GetAudioCapabilities 调用
// IBluetoothAudioProviderFactory::getProviderCapabilities 时，若收到
// STATUS_UNKNOWN_TRANSACTION (-129) 会触发 bluetooth::log::fatal SIGABRT 循环崩溃；
// 返回空 AudioCapabilities[] 数组（长度 0）与 openProvider=EX_UNSUPPORTED_OPERATION，
// 使蓝牙栈平稳回退至系统软件 A2DP/BLE 音频编码管线。
binder_status_t OnBluetoothAudioProviderFactoryTransact(AIBinder* /*binder*/,
                                                        transaction_code_t code,
                                                        const AParcel* /*in*/,
                                                        AParcel* out) {
    if (out == nullptr) {
        return STATUS_BAD_VALUE;
    }
    ndk::ScopedAStatus ok = ndk::ScopedAStatus::ok();
    if (code == FIRST_CALL_TRANSACTION + 0) {
        // AudioCapabilities[] getProviderCapabilities(in SessionType) -> empty array (len=0)
        binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
        return st != STATUS_OK ? st : AParcel_writeInt32(out, 0);
    }
    if (code == FIRST_CALL_TRANSACTION + 1) {
        // IBluetoothAudioProvider openProvider(in SessionType) -> unsupported
        ndk::ScopedAStatus unsup =
                ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
        return AParcel_writeStatusHeader(out, unsup.get());
    }
    if (code == FIRST_CALL_TRANSACTION + 2) {
        // @nullable ProviderInfo getProviderInfo(in SessionType) -> null parcelable (0)
        binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
        return st != STATUS_OK ? st : AParcel_writeInt32(out, 0);
    }
    if (code == 0xffffff) {
        binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
        return st != STATUS_OK ? st : AParcel_writeInt32(out, 4);
    }
    if (code == 0xfffffe) {
        binder_status_t st = AParcel_writeStatusHeader(out, ok.get());
        return st != STATUS_OK
                       ? st
                       : AParcel_writeString(
                                 out, kBluetoothAudioProviderFactoryHash,
                                 static_cast<int32_t>(std::strlen(kBluetoothAudioProviderFactoryHash)));
    }
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
            const AIBinder_Class_onTransact transact_fn =
                    (descriptor == kBluetoothAudioProviderFactoryDescriptor)
                            ? OnBluetoothAudioProviderFactoryTransact
                            : OnPlaceholderTransact;
            it->second = AIBinder_Class_define(
                    it->first.c_str(), OnPlaceholderCreate, OnPlaceholderDestroy,
                    transact_fn);
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
