// engine/vendor/hardware 线 —— android.hardware.health 的 samsung 变体。
//
// 三星 OneUI 的健康链路比 AOSP 多一层：framework 的
// com.android.server.health.HealthServiceWrapperAidl 拿到 android.hardware.health.IHealth/default
// 之后，还会对这个 binder 调 AIBinder_getExtension() 取
// vendor.samsung.hardware.health.ISehHealth；拿不到就对 null 代理调 registerCallback
// → HealthRegCallbackAidl.onRegistration NPE → BatteryService 起不来 →
// system_server 自杀循环。官方服务是把 ISehHealth 通过 AIBinder_setExtension 挂在
// 自己的 IHealth binder 上（不是独立命名服务，VINTF 里也只声明 IHealth）。
//
// 本变体就在自研健康服务上复刻这条旁路：IHealth 仍由基础实现提供，本文件只补
// ISehHealth 服务端并挂成 extension，因此不需要三星二进制，也不影响其它云机
// （基座构建不含本目录，`#if CDH_HEALTH_SAMSUNG` 由 modules/*.json 的变体注入）。
//
// AIDL 定义在同层的 aidl/samsung/vendor/samsung/hardware/health/，是按官方
// vendor.samsung.hardware.health-V1-ndk.so 的生成代码取证重写的等价定义。

#pragma once

#include <aidl/android/hardware/health/HealthInfo.h>
#include <aidl/vendor/samsung/hardware/health/BnSehHealth.h>
#include <aidl/vendor/samsung/hardware/health/ISehHealthInfoCallback.h>

#include <android/binder_ibinder.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace cdh::health {

// 取一份当前模拟电池口径的 HealthInfo。由基础健康服务提供（它持有
// /vendor/etc/battery_config.xml 的解析结果），本变体不重复造电池模型。
using HealthInfoFiller = std::function<void(
    aidl::android::hardware::health::HealthInfo *out)>;

// ISehHealth 服务端。四个方法全部与官方实现同 transaction code（1..4），
// 版本 1 由 codegen 的 --version=1 决定（BnSehHealth::getInterfaceVersion 是 final，
// 生成值即官方的 1，不再手工覆盖）。
class SehHealth final
    : public aidl::vendor::samsung::hardware::health::BnSehHealth {
 public:
  explicit SehHealth(HealthInfoFiller filler);

  ndk::ScopedAStatus registerCallback(
      const std::shared_ptr<
          aidl::vendor::samsung::hardware::health::ISehHealthInfoCallback>
          &callback) override;

  ndk::ScopedAStatus unregisterCallback(
      const std::shared_ptr<
          aidl::vendor::samsung::hardware::health::ISehHealthInfoCallback>
          &callback) override;

  ndk::ScopedAStatus update() override;

  ndk::ScopedAStatus sehWriteEnableToParam(int32_t param, bool enable) override;

  // 按当前模拟电池口径向已注册的三星回调推一轮（update() 与周期线程共用）。
  void notifyCallbacks();

 private:
  HealthInfoFiller filler_;
  std::mutex mutex_;
  std::vector<
      std::shared_ptr<
          aidl::vendor::samsung::hardware::health::ISehHealthInfoCallback>>
      callbacks_;
};

// 创建 ISehHealth 并挂到 healthBinder 的 extension 上（AIBinder_setExtension），
// 随后起一个周期线程按 intervalSeconds 推送回调（<=0 表示不起线程，只响应 update()）。
//
// 必须在 AServiceManager_addService("android.hardware.health.IHealth/default")
// **之前**调用：客户端拿到 binder 就立刻 getExtension，晚于 addService 会撞上
// “extension 还没设”的窗口。返回的对象由本模块静态持有（进程活到关机为止）。
std::shared_ptr<SehHealth> AttachSehHealth(AIBinder *healthBinder,
                                           HealthInfoFiller filler,
                                           int32_t intervalSeconds);

}  // namespace cdh::health
