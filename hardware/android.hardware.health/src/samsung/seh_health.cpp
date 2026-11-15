// engine/vendor/hardware 线 —— android.hardware.health 的 samsung 变体实现。
//
// 只补 OneUI 需要的那层旁路（ISehHealth），电池数据仍取自基础实现的模拟口径。
// 取证来源见同层 ../aidl/samsung/vendor/samsung/hardware/health/*.aidl 的文件头：
// 官方 vendor.samsung.hardware.health-V1-ndk.so 的生成代码里，
//   - descriptor：vendor.samsung.hardware.health.ISehHealth
//   - 版本 1，4 个方法（registerCallback / unregisterCallback / update /
//     sehWriteEnableToParam），transaction code 与声明顺序一致（1..4）
//   - 回调 ISehHealthInfoCallback.healthInfoChanged 是同步调用（无 FLAG_ONEWAY），
//     所以推送失败要当成对端已死，从表里摘掉。

#include "samsung/seh_health.h"

#include <android/log.h>

#include <chrono>
#include <thread>
#include <utility>

using aidl::android::hardware::health::HealthInfo;
using aidl::vendor::samsung::hardware::health::ISehHealthInfoCallback;
using aidl::vendor::samsung::hardware::health::SehHealthInfo;
using ndk::ScopedAStatus;

namespace cdh::health {
namespace {

constexpr const char *kTag = "cdh_health";

// 进程内唯一的 SEH 实例：extension binder 的生命周期必须跟进程一致，
// 挂在函数静态变量上最省事（main 永不返回，joinThreadPool 之后才结束）。
std::shared_ptr<SehHealth> &Instance() {
  static std::shared_ptr<SehHealth> instance;
  return instance;
}

}  // namespace

SehHealth::SehHealth(HealthInfoFiller filler) : filler_(std::move(filler)) {}

ScopedAStatus SehHealth::registerCallback(
    const std::shared_ptr<ISehHealthInfoCallback> &callback) {
  if (callback == nullptr) {
    return ScopedAStatus::fromExceptionCode(EX_NULL_POINTER);
  }
  std::lock_guard<std::mutex> lock(mutex_);
  callbacks_.push_back(callback);
  return ScopedAStatus::ok();
}

ScopedAStatus SehHealth::unregisterCallback(
    const std::shared_ptr<ISehHealthInfoCallback> &callback) {
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto it = callbacks_.begin(); it != callbacks_.end(); ++it) {
    if (it->get() == callback.get()) {
      callbacks_.erase(it);
      break;
    }
  }
  return ScopedAStatus::ok();
}

ScopedAStatus SehHealth::update() {
  notifyCallbacks();
  return ScopedAStatus::ok();
}

ScopedAStatus SehHealth::sehWriteEnableToParam(int32_t param, bool enable) {
  // 真机是往 /efs 写电池参数（BatteryInfoBackUp 那条链路），云机没有 efs，
  // 也不做参数持久化：记录一笔便于取证即可。
  __android_log_print(ANDROID_LOG_INFO, kTag,
                      "sehWriteEnableToParam(param=%d, enable=%d)", param,
                      enable ? 1 : 0);
  return ScopedAStatus::ok();
}

void SehHealth::notifyCallbacks() {
  SehHealthInfo info;
  HealthInfo health;
  if (filler_) {
    filler_(&health);
  }
  info.healthInfo = std::move(health);
  // 尾部 10 个三星私有字段保持 0：语义未公开，官方件在“无电池硬件”时也是默认值，
  // 电量/充电状态全部来自前面的 HealthInfo。
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto it = callbacks_.begin(); it != callbacks_.end();) {
    if (!it->get()->healthInfoChanged(info).isOk()) {
      it = callbacks_.erase(it);
    } else {
      ++it;
    }
  }
}

std::shared_ptr<SehHealth> AttachSehHealth(AIBinder *healthBinder,
                                           HealthInfoFiller filler,
                                           int32_t intervalSeconds) {
  if (healthBinder == nullptr) {
    __android_log_print(ANDROID_LOG_ERROR, kTag,
                        "AttachSehHealth: healthBinder 为 null");
    return nullptr;
  }
  auto seh = ndk::SharedRefBase::make<SehHealth>(std::move(filler));
  const binder_status_t status =
      AIBinder_setExtension(healthBinder, seh->asBinder().get());
  if (status != STATUS_OK) {
    __android_log_print(ANDROID_LOG_ERROR, kTag,
                        "AIBinder_setExtension(ISehHealth) failed: %d", status);
    return nullptr;
  }

  // 自检：setExtension 报 OK 不代表挂到了“稍后要 addService 的那个 binder”上
  // （asBinder() 只做弱缓存，临时对象一析构 extension 就跟着没了）。这里立刻
  // 读一次，把结果写进日志，起机排查时无需再上 dex 反查。
  AIBinder *probe = nullptr;
  const binder_status_t probeStatus = AIBinder_getExtension(healthBinder, &probe);
  if (probeStatus != STATUS_OK || probe == nullptr) {
    __android_log_print(ANDROID_LOG_ERROR, kTag,
                        "ISehHealth extension 自检失败: status=%d ext=%p",
                        probeStatus, static_cast<void *>(probe));
    return nullptr;
  }
  AIBinder_decStrong(probe);
  __android_log_print(ANDROID_LOG_INFO, kTag,
                      "ISehHealth 已挂到 IHealth binder 的 extension 上"
                      "（interval=%ds）",
                      intervalSeconds);

  Instance() = seh;

  if (intervalSeconds > 0) {
    std::thread([seh, intervalSeconds] {
      while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(intervalSeconds));
        seh->notifyCallbacks();
      }
    }).detach();
  }
  return seh;
}

}  // namespace cdh::health
