/*
 * cdh：android.hardware.soundtrigger3 的空实现（0 module）。
 *
 * 背景（CPH2797 bring-up 实例 10 实测）：ColorOS 的 system_server 在
 * StartSoundTriggerMiddlewareService 里同步等 HAL。真机由 MTK audio HAL 进程内
 * 注册 ISoundTriggerHw/default；云机的 vendor 基座是 lineage，没有实现也没有
 * VINTF 声明，于是 SoundTriggerMiddlewareDefaultHalFactory 每次 getService() 都
 * 拿到 null，主线程紧循环重试（2~5 ms 一次、5 万条日志），151 s 被 watchdog
 * 用 sysrq 硬重启整机。
 *
 * 这里不接任何真实语音硬件：getProperties() 返回 modules 为空，上层据此认为
 * 「本机没有 soundtrigger 能力」而正常降级。AOSP 的 SoundTriggerHw 同名实现
 * 在 HAL 缺失时也是这个语义（mHwDevice == NULL → -ENODEV），区别只在于它仍会
 * 走 hw_get_module_by_class 去开 primary 模块；本文件把那段直接省掉。
 */
#pragma once

#include <aidl/android/hardware/soundtrigger3/BnSoundTriggerHw.h>
#include <aidl/android/hardware/soundtrigger3/ISoundTriggerHwCallback.h>
#include <aidl/android/hardware/soundtrigger3/ISoundTriggerHwGlobalCallback.h>
#include <aidl/android/media/soundtrigger/ModelParameter.h>
#include <aidl/android/media/soundtrigger/ModelParameterRange.h>
#include <aidl/android/media/soundtrigger/PhraseSoundModel.h>
#include <aidl/android/media/soundtrigger/Properties.h>
#include <aidl/android/media/soundtrigger/RecognitionConfig.h>
#include <aidl/android/media/soundtrigger/SoundModel.h>

#include <memory>
#include <optional>

namespace aidl::android::hardware::soundtrigger3 {

class SoundTriggerHw : public BnSoundTriggerHw {
  public:
    SoundTriggerHw() = default;

    ndk::ScopedAStatus getProperties(
            ::aidl::android::media::soundtrigger::Properties* _aidl_return) override;

    ndk::ScopedAStatus registerGlobalCallback(
            const std::shared_ptr<ISoundTriggerHwGlobalCallback>& in_callback) override;

    ndk::ScopedAStatus loadSoundModel(
            const ::aidl::android::media::soundtrigger::SoundModel& in_soundModel,
            const std::shared_ptr<ISoundTriggerHwCallback>& in_callback,
            int32_t* _aidl_return) override;

    ndk::ScopedAStatus loadPhraseSoundModel(
            const ::aidl::android::media::soundtrigger::PhraseSoundModel& in_soundModel,
            const std::shared_ptr<ISoundTriggerHwCallback>& in_callback,
            int32_t* _aidl_return) override;

    ndk::ScopedAStatus unloadSoundModel(int32_t in_modelHandle) override;

    ndk::ScopedAStatus startRecognition(
            int32_t in_modelHandle, int32_t in_deviceHandle, int32_t in_ioHandle,
            const ::aidl::android::media::soundtrigger::RecognitionConfig& in_config) override;

    ndk::ScopedAStatus stopRecognition(int32_t in_modelHandle) override;

    ndk::ScopedAStatus forceRecognitionEvent(int32_t in_modelHandle) override;

    ndk::ScopedAStatus getParameter(
            int32_t in_modelHandle,
            ::aidl::android::media::soundtrigger::ModelParameter in_modelParam,
            int32_t* _aidl_return) override;

    ndk::ScopedAStatus setParameter(
            int32_t in_modelHandle,
            ::aidl::android::media::soundtrigger::ModelParameter in_modelParam,
            int32_t in_value) override;

    ndk::ScopedAStatus queryParameter(
            int32_t in_modelHandle,
            ::aidl::android::media::soundtrigger::ModelParameter in_modelParam,
            std::optional<::aidl::android::media::soundtrigger::ModelParameterRange>* _aidl_return)
            override;
};

}  // namespace aidl::android::hardware::soundtrigger3
