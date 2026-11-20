/*
 * Copyright (C) 2026 cloud-phone-hub
 * SPDX-License-Identifier: Apache-2.0
 *
 * Android 13 (lineage-20.0) 64-bit Audio HIDL passthrough 宿主服务：
 * lineage-20.0 基座自带的 /vendor/bin/hw/android.hardware.audio.service 为 32 位 ARM ELF，
 * 在无 AArch32 EL0 的宿主虚拟化硬件（如 Apple Silicon HVF）上会触发 Exec format error，
 * 并因 onrestart restart audioserver 导致 audioserver 反复重启。
 * 本 64 位宿主进程按 AOSP 13 标准顺序通过 libhidlbase passthrough 加载 /vendor/lib64/hw/
 * 下的 64 位 android.hardware.audio@7.1-impl*.so 与 android.hardware.audio.effect@7.0-impl.so。
 */

#include <android/binder_process.h>
#include <android/log.h>
#include <hidl/HidlTransportSupport.h>
#include <hidl/LegacySupport.h>
#include <unistd.h>

#include <string>
#include <vector>

#undef LOG_TAG
#define LOG_TAG "audiohalservice"

using android::OK;
using android::status_t;
using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;
using android::hardware::registerPassthroughServiceImplementation;

namespace {

bool registerFirstAvailable(const std::vector<std::string>& interfaces) {
    for (const auto& iface : interfaces) {
        status_t status = registerPassthroughServiceImplementation(iface, iface, "default");
        if (status == OK) {
            __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Registered %s/default", iface.c_str());
            return true;
        }
    }
    return false;
}

}  // namespace

int main(int /* argc */, char* /* argv */[]) {
    configureRpcThreadpool(16, true /* callerWillJoin */);

    ABinderProcess_setThreadPoolMaxThreadCount(16);
    ABinderProcess_startThreadPool();

    // lineage-20.0 的 android.hardware.audio@7.1-impl.ranchu.so 构造函数内部调用了
    // 不带前导斜杠的相对路径 dlopen("vendor/lib64/hw/android.hardware.audio.legacy@7.1-impl.ranchu.so")，
    // 会绕过 cdh_redirect 的 "/vendor/lib64" 绝对路径匹配；注册期临时将 cwd 切至 /mnt/vendorpatch 使其命中，随后恢复 "/"。
    const bool switchedCwd = (access("/mnt/vendorpatch/vendor/lib64/hw", R_OK) == 0 && chdir("/mnt/vendorpatch") == 0);

    const bool hasDevicesFactory = registerFirstAvailable({
        "android.hardware.audio@7.1::IDevicesFactory",
        "android.hardware.audio@7.0::IDevicesFactory",
        "android.hardware.audio@6.0::IDevicesFactory",
        "android.hardware.audio@5.0::IDevicesFactory",
        "android.hardware.audio@4.0::IDevicesFactory",
    });
    if (switchedCwd) {
        (void)chdir("/");
    }
    if (!hasDevicesFactory) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Could not register IDevicesFactory");
        return 1;
    }

    const bool hasEffectsFactory = registerFirstAvailable({
        "android.hardware.audio.effect@7.0::IEffectsFactory",
        "android.hardware.audio.effect@6.0::IEffectsFactory",
        "android.hardware.audio.effect@5.0::IEffectsFactory",
        "android.hardware.audio.effect@4.0::IEffectsFactory",
    });
    if (!hasEffectsFactory) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Could not register IEffectsFactory");
        return 1;
    }

    // 可选附属接口：存在实现库时顺带注册，不存在不阻断主服务。
    (void)registerFirstAvailable({
        "android.hardware.soundtrigger@2.3::ISoundTriggerHw",
        "android.hardware.soundtrigger@2.2::ISoundTriggerHw",
        "android.hardware.soundtrigger@2.1::ISoundTriggerHw",
        "android.hardware.soundtrigger@2.0::ISoundTriggerHw",
    });
    (void)registerFirstAvailable({
        "android.hardware.bluetooth.audio@2.2::IBluetoothAudioProvidersFactory",
        "android.hardware.bluetooth.audio@2.1::IBluetoothAudioProvidersFactory",
        "android.hardware.bluetooth.audio@2.0::IBluetoothAudioProvidersFactory",
    });
    (void)registerFirstAvailable({
        "android.hardware.bluetooth.a2dp@1.0::IBluetoothAudioOffload",
    });

    joinRpcThreadpool();
    return 1;
}
