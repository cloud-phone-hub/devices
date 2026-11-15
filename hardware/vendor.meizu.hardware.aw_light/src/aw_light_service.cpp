// engine/vendor/hardware 线 —— vendor.meizu.hardware.aw_light（IAwLight v1）
// 自研服务实现，替换原厂预编译二进制（vendor/bin/hw/vendor.meizu.hardware.aw_light-service）。
//
// 背景：MEIZU 私有 HAL（原厂编译自 vendor/meizu/proprietary/aw_light/），AOSP/LineageOS
// 无公开源码。接口按原厂 lib64/vendor.meizu.hardware.aw_light-V1-ndk.so 反解，
// 见 aidl/.../1/.hash 与 IAwLight.aidl 头注释。
//
// 原厂语义：驱动 i2c 上挂的 rgb_led（/dev/rgb_led + /sys/.../leds/rgb_led/*），
// 提供呼吸/律动/进度/信号灯/音乐律动等灯效，停止时通过 callback 通知 framework。
//
// 云机语义：云机没有该 led 控制器，也没有实体指示灯，故：
//   * 所有「设置类」一律回 true（= 接受但无动作），避免 framework 反复重试或报错；
//   * aw_get_duration_time 回 0（时长未知/无动画），调用方按立即结束处理；
//   * 从不回调 IAwLightCallback（没有"灯效结束"事件），只保存 binder 引用；
//   * 不打开 /dev/rgb_led、不写 sysfs，因此策略里不含任何设备节点授权。
// 设备无关，不内置机型常量；rc / VINTF 在模块 vendor/ 静态树里。

#include <android/binder_auto_utils.h>
#include <android/binder_ibinder.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <android/binder_status.h>

#include <aidl/vendor/meizu/hardware/aw_light/BnAwLight.h>
#include <aidl/vendor/meizu/hardware/aw_light/IAwLightCallback.h>

#include <cstdint>
#include <memory>
#include <vector>

using aidl::vendor::meizu::hardware::aw_light::BnAwLight;
using aidl::vendor::meizu::hardware::aw_light::IAwLightCallback;
using ndk::ScopedAStatus;

namespace {

// 必须与模块 vendor/etc/init/*.rc 的 interface 行、VINTF 的 <fqname> 逐字一致。
constexpr char kServiceName[] = "vendor.meizu.hardware.aw_light.IAwLight/default";

class AwLight final : public BnAwLight {
  public:
    // code 1：三色通道相位。
    ScopedAStatus aw_color_set_phase(int32_t /*r*/, int32_t /*g*/, int32_t /*b*/,
                                     bool* _aidl_return) override {
        *_aidl_return = true;
        return ScopedAStatus::ok();
    }

    // code 2：相位时长（ms）。无硬件 → 0。
    ScopedAStatus aw_get_duration_time(int32_t /*phase*/, int32_t* _aidl_return) override {
        *_aidl_return = 0;
        return ScopedAStatus::ok();
    }

    // code 3：初始化。
    ScopedAStatus aw_light_init(bool* _aidl_return) override {
        *_aidl_return = true;
        return ScopedAStatus::ok();
    }

    // code 4：开始灯效。
    ScopedAStatus aw_light_start(bool* _aidl_return) override {
        *_aidl_return = true;
        return ScopedAStatus::ok();
    }

    // code 5：停止灯效。
    ScopedAStatus aw_light_stop(bool* _aidl_return) override {
        *_aidl_return = true;
        return ScopedAStatus::ok();
    }

    // code 6：切换灯效编号。
    ScopedAStatus aw_light_switchEffect(int32_t /*effect*/, bool* _aidl_return) override {
        *_aidl_return = true;
        return ScopedAStatus::ok();
    }

    // code 7：音乐律动相位序列。
    ScopedAStatus aw_music_phase(const std::vector<int32_t>& /*phases*/,
                                 bool* _aidl_return) override {
        *_aidl_return = true;
        return ScopedAStatus::ok();
    }

    // code 8：信号灯字符（int8）。
    ScopedAStatus aw_set_Semaphore_letter(int8_t /*letter*/, bool* _aidl_return) override {
        *_aidl_return = true;
        return ScopedAStatus::ok();
    }

    // code 9：CL 亮度相位。
    ScopedAStatus aw_set_cl_brightness_phase(int32_t /*phase*/, bool* _aidl_return) override {
        *_aidl_return = true;
        return ScopedAStatus::ok();
    }

    // code 10：登记停止通知回调。持有引用即可，云机从不触发 onStopNotify。
    ScopedAStatus aw_set_light_over_callback(const std::shared_ptr<IAwLightCallback>& callback,
                                             bool* _aidl_return) override {
        callback_ = callback;
        *_aidl_return = true;
        return ScopedAStatus::ok();
    }

    // code 11：进度 0..100。
    ScopedAStatus aw_set_progress(int32_t /*progress*/, bool* _aidl_return) override {
        *_aidl_return = true;
        return ScopedAStatus::ok();
    }

  private:
    std::shared_ptr<IAwLightCallback> callback_;
};

}  // namespace

int main() {
    // 单线程池足够：全部方法常数时间返回，只有 code 10 会持有客户端 binder。
    ABinderProcess_setThreadPoolMaxThreadCount(1);
    std::shared_ptr<AwLight> service = ndk::SharedRefBase::make<AwLight>();
    binder_status_t status = AServiceManager_addService(service->asBinder().get(), kServiceName);
    if (status != STATUS_OK) {
        return 1;
    }
    ABinderProcess_joinThreadPool();
    return 0;
}
