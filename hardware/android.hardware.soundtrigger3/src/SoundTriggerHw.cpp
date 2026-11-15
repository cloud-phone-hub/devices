#include "SoundTriggerHw.h"

#include <log/log.h>

#include <cerrno>

namespace aidl::android::hardware::soundtrigger3 {

using ::aidl::android::media::soundtrigger::ModelParameter;
using ::aidl::android::media::soundtrigger::ModelParameterRange;
using ::aidl::android::media::soundtrigger::PhraseSoundModel;
using ::aidl::android::media::soundtrigger::Properties;
using ::aidl::android::media::soundtrigger::RecognitionConfig;
using ::aidl::android::media::soundtrigger::SoundModel;

ndk::ScopedAStatus SoundTriggerHw::getProperties(Properties* _aidl_return) {
    // modules 留空：SoundTriggerMiddleware 看到 0 个 module 即停止探测。
    // 这是打断 system_server 紧循环的关键返回，不能返回异常（异常会让上层重试）。
    *_aidl_return = Properties{};
    ALOGI("getProperties: 0 modules (stub HAL)");
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SoundTriggerHw::registerGlobalCallback(
        const std::shared_ptr<ISoundTriggerHwGlobalCallback>& in_callback) {
    (void)in_callback;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SoundTriggerHw::loadSoundModel(
        const SoundModel& in_soundModel,
        const std::shared_ptr<ISoundTriggerHwCallback>& in_callback, int32_t* _aidl_return) {
    (void)in_soundModel;
    (void)in_callback;
    *_aidl_return = -ENODEV;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SoundTriggerHw::loadPhraseSoundModel(
        const PhraseSoundModel& in_soundModel,
        const std::shared_ptr<ISoundTriggerHwCallback>& in_callback, int32_t* _aidl_return) {
    (void)in_soundModel;
    (void)in_callback;
    *_aidl_return = -ENODEV;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SoundTriggerHw::unloadSoundModel(int32_t in_modelHandle) {
    (void)in_modelHandle;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SoundTriggerHw::startRecognition(int32_t in_modelHandle,
                                                    int32_t in_deviceHandle,
                                                    int32_t in_ioHandle,
                                                    const RecognitionConfig& in_config) {
    (void)in_modelHandle;
    (void)in_deviceHandle;
    (void)in_ioHandle;
    (void)in_config;
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus SoundTriggerHw::stopRecognition(int32_t in_modelHandle) {
    (void)in_modelHandle;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SoundTriggerHw::forceRecognitionEvent(int32_t in_modelHandle) {
    (void)in_modelHandle;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SoundTriggerHw::getParameter(int32_t in_modelHandle,
                                                ModelParameter in_modelParam,
                                                int32_t* _aidl_return) {
    (void)in_modelHandle;
    (void)in_modelParam;
    *_aidl_return = -ENODEV;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SoundTriggerHw::setParameter(int32_t in_modelHandle,
                                                ModelParameter in_modelParam, int32_t in_value) {
    (void)in_modelHandle;
    (void)in_modelParam;
    (void)in_value;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SoundTriggerHw::queryParameter(
        int32_t in_modelHandle, ModelParameter in_modelParam,
        std::optional<ModelParameterRange>* _aidl_return) {
    (void)in_modelHandle;
    (void)in_modelParam;
    // nullopt 表示「该参数不支持查询」，上层据此跳过参数协商，不再重试。
    *_aidl_return = std::nullopt;
    return ndk::ScopedAStatus::ok();
}

}  // namespace aidl::android::hardware::soundtrigger3
