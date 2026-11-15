// engine/vendor/hardware 线 —— vendor.awa.reality（IRealityTap v3）自研服务实现，
// 替换原厂预编译二进制（vendor/bin/hw/vendor.awa.realitytap.service）。
//
// 背景：MEIZU/awa 私有触感 HAL（原厂编译自 vendor/meizu/proprietary/haptic_aidl/
// realitytap/），AOSP/LineageOS 无公开源码。接口按原厂
// lib64/vendor.awa.reality-V3-ndk.so 反解，见 aidl/.../3/ 与同目录 .hash。
//
// 原厂语义：驱动 awa 触感芯片（/dev/awa_realitytap_dev），performTap/performRtp
// 把波形（int[] 或波形 fd）下发给芯片，cancel 停止，calibrate 做校准，getF0 返回
// 谐振频率；原厂 rc 还要 chmod/chown 该设备节点。
//
// 云机语义：底座（lineage-23.2 virtio）没有 awa 触感芯片、也没有该设备节点，故：
//   * performTap/performRtp 一律回 0（= 已受理），并在回调非空时立即
//     onComplete()，避免 framework 的振动超时等待；
//   * cancel/calibrate 回 0，getF0 回 0（频率未知）；
//   * 不 open /dev/awa_realitytap_dev、不解析波形，因此策略里不含设备节点授权，
//     rc 也不搬原厂 `on init` / `on boot` 的 chmod/chown（节点不存在，
//     chmod 只会刷噪音）。
// 设备无关，不内置机型常量；rc / VINTF 在模块 vendor/ 静态树里。
//
// 注意：本进程只实现 vendor HAL；Flyme framework 的 RealityTapManagerService
// （android.os.vibrator.IRealityTapManagerService，服务名 "realitytap"）由
// system_server 自己发布，不在 vendor 侧，见机型层 sepolicy。

#include <android/binder_auto_utils.h>
#include <android/binder_ibinder.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <android/binder_status.h>

#include <aidl/android/hardware/vibrator/EffectStrength.h>
#include <aidl/android/hardware/vibrator/IVibratorCallback.h>
#include <aidl/vendor/awa/reality/BnRealityTap.h>
#include <aidl/vendor/awa/reality/RealityTapHe.h>
#include <aidl/vendor/awa/reality/RealityTapRtp.h>

#include <cstdint>
#include <memory>

using aidl::android::hardware::vibrator::EffectStrength;
using aidl::android::hardware::vibrator::IVibratorCallback;
using aidl::vendor::awa::reality::BnRealityTap;
using aidl::vendor::awa::reality::RealityTapHe;
using aidl::vendor::awa::reality::RealityTapRtp;
using ndk::ScopedAStatus;

namespace {

// 必须与模块 vendor/etc/init/*.rc 的 interface 行、VINTF 的 <fqname> 逐字一致。
constexpr char kServiceName[] = "vendor.awa.reality.IRealityTap/default";

// 通知调用方「这一次已经结束」。云机立即振完，故同步回调；onComplete 是 oneway，
// 抛错不影响返回值。
void completeNow(const std::shared_ptr<IVibratorCallback>& callback) {
    if (callback != nullptr) {
        callback->onComplete();
    }
}

class RealityTap final : public BnRealityTap {
  public:
    // code 1：下发触感波形（int[] 形式）。
    ScopedAStatus performTap(const RealityTapHe& /*he*/, EffectStrength /*strength*/,
                             const std::shared_ptr<IVibratorCallback>& callback,
                             int32_t* _aidl_return) override {
        completeNow(callback);
        *_aidl_return = 0;
        return ScopedAStatus::ok();
    }

    // code 2：下发触感波形（ParcelFileDescriptor 形式）。
    ScopedAStatus performRtp(const RealityTapRtp& /*rtp*/, EffectStrength /*strength*/,
                             const std::shared_ptr<IVibratorCallback>& callback,
                             int32_t* _aidl_return) override {
        completeNow(callback);
        *_aidl_return = 0;
        return ScopedAStatus::ok();
    }

    // code 3：停止。
    ScopedAStatus cancel(int32_t* _aidl_return) override {
        *_aidl_return = 0;
        return ScopedAStatus::ok();
    }

    // code 4：校准。
    ScopedAStatus calibrate(int32_t* _aidl_return) override {
        *_aidl_return = 0;
        return ScopedAStatus::ok();
    }

    // code 5：谐振频率（Hz）。没有芯片 → 0（未知）。
    ScopedAStatus getF0(int32_t* _aidl_return) override {
        *_aidl_return = 0;
        return ScopedAStatus::ok();
    }
};

}  // namespace

int main() {
    // 单线程池足够：全部方法常数时间返回，唯一的外部调用是 oneway onComplete。
    ABinderProcess_setThreadPoolMaxThreadCount(1);
    std::shared_ptr<RealityTap> service = ndk::SharedRefBase::make<RealityTap>();
    binder_status_t status = AServiceManager_addService(service->asBinder().get(), kServiceName);
    if (status != STATUS_OK) {
        return 1;
    }
    ABinderProcess_joinThreadPool();
    return 0;
}
