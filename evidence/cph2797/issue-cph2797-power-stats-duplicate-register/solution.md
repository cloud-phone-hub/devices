# CPH2797：OPPO 的 power.stats 实现与空 HAL 抢注同名 AIDL，system_server 主线程死等 65 s 被 Watchdog 杀掉

- 设备：OPPO Find X9 / CPH2797 / ColorOS 16.0.8.301_EX01（`board=mt6993`，联发科）
- 实例：失败 13 / 14（两者死在这一处）→ 通过 15
- 日期：2026-10-30
- 上游条目：#79（指纹空表 / createSession）
- 日志：`log-fail.txt`（实例 13）/ `log-pass.txt`（实例 15）

## 1. 问题

指纹修好之后（#79），实例 13 的 `FATAL EXCEPTION IN SYSTEM PROCESS` 归零、进度推到
`OnBootPhase_500`，但 **65 s 后 system_server 被 Watchdog 杀死、整机 `sysrq:
Resetting` 硬重启**，如此两轮：

```
W Watchdog: *** WATCHDOG KILLING SYSTEM PROCESS: Blocked in handler on main thread (main) for 65s
W Watchdog: main annotated stack trace:
W Watchdog:     at java.util.concurrent.locks.LockSupport.park(LockSupport.java:221)
W Watchdog:     at java.util.concurrent.FutureTask.awaitDone(FutureTask.java:500)
[  135.016479][  T928] sysrq: Resetting
```

Watchdog 的 dump 只截到 3 帧就被 sysrq 打断，必须去 guest 的
`/data/anr/traces_SystemServer_WDT*` 取完整栈。

## 2. 根因

完整栈（`traces_SystemServer_WDT31_12_19_02_06.752_pid716`，`"main" prio=5 tid=1 Native`）：

```
native: android::CppBackendShim::waitForService
        android::android_os_ServiceManager_waitForServiceNative
    at android.os.ServiceManager.waitForService(ServiceManager.java:334)
    at android.os.ServiceManager.waitForDeclaredService(ServiceManager.java:353)
    at com.android.server.powerstats.PowerStatsHALWrapper$VintfHalCache.get(PowerStatsHALWrapper.java:350)
    at com.android.server.powerstats.PowerStatsHALWrapper$PowerStatsHAL20WrapperImpl.getPowerEntityInfo(...)
    at com.android.server.powerstats.PowerStatsService$LocalService.getPowerEntityInfo(...)
    at com.android.server.powerstats.StatsPullAtomCallbackImpl.initSubsystemSleepState(...)
    at com.android.server.powerstats.StatsPullAtomCallbackImpl.<init>(...)
    at com.android.server.powerstats.PowerStatsService.onBootPhase(PowerStatsService.java:308)
    at com.android.server.SystemServiceManager.startBootPhase(SystemServiceManager.java:328)
    at com.android.server.SystemServer.startOtherServices(SystemServer.java:3451)
```

`waitForDeclaredService` 的语义是「**声明存在就必须等到服务出现**」，NDK 的
`waitForService` 没有超时。而声明确实存在：

- `/odm/etc/vintf/manifest/android.hardware.power.stats.IPowerStats-V2.xml`（官方件）；
- 我方 `vendor.power.stats-default`（`android.hardware.power.stats-service`，README
  里说的「不落 VINTF 片段」）本该提供实现。

问题在于**两个实现抢注同一个实例**：官方 `/odm/etc/init/android.hardware.power.stats-service.oplus.rc`
定义了

```
service vendor.power.stats /odm/bin/hw/android.hardware.power.stats-impl.oplus
    class hal
```

它启动后先 `addService` 两个名字，第二个被 SELinux 拒绝：

```
avc: denied { add } for pid=975 uid=1000
     name=vendor.oplus.hardware.handlefactory.IHandleFactory/default
     scontext=u:r:hal_power_stats_default:s0
     tcontext=u:object_r:default_android_service:s0 tclass=service_manager permissive=0
F android.hardware.power.stats-impl.oplus: Check failed: s == STATUS_OK
DEBUG   : Abort message: 'Check failed: s == STATUS_OK'
init: Service 'vendor.power.stats' (pid 649) received signal 6
```

`CHECK` 失败 → SIGABRT → init 每 ~5 s 重启一轮（实例 13 里 `Check failed` 281 次、
`received signal 6` 151 次、`handlefactory` 567 行）。而它的第一次注册已经把
servicemanager 里我们的名字顶掉：

```
servicemanager: Service 'android.hardware.power.stats.IPowerStats/default'
                originally registered from PID 536 but it is being registered again
                from PID 649. Bad state? Late death notification? Multiple instances installed?
```

随后它崩溃，名字随进程销毁被移除；我们的 `vendor.power.stats-default` 进程（pid 536）
还活着但**名字已经不在服务表里**，于是 `waitForDeclaredService` 永远等不到。

## 3. 证据

- 实例 13 / 14 都命中同一栈（`PowerStatsService.onBootPhase`）；
- 实例 13 中 `IPowerStats/default` 注册冲突日志 2 处（PID 536↔649、PID 527↔642，
  两次 boot 各一次）；
- 对照：实例 13 里 `PowerStatsHALWrapper: PowerStats HAL died` 与
  `Failed to get energy meter info: DeadObjectException` 正是「注册被顶掉后老 binder 已死」；
- 另一个佐证：`vendor.power.stats-default` 的 `init.svc.*` 一直是 `running`，
  但 `service check android.hardware.power.stats.IPowerStats/default` 返回
  `not found`——进程活着、名字没了。

## 4. 改动

只改机型静态树的兼容 rc，不动模块（m22 / flyme 没有 OPPO 的这个进程，不受影响）：

`devices/manifest/oppo/cph2797/vendor/etc/init/init.virtio_compat.rc` 的
`on early-init` 末尾追加 `stop vendor.power.stats`，并写清原理注释：

- init 的 `Service::StopOrReset(SVC_DISABLED)` 会给「尚未启动」的服务也打上
  `SVC_DISABLED`，`class_start hal` 走 `Service::StartIfNotDisabled()` 直接跳过
  （`cuttlefish/source/system-core/init/service.cpp:855`）；
- OPPO 的 rc **没有** `interface aidl` 行，所以懒启动（`ctl.interface_start`）
  只会命中我们的服务（我们的 rc 有），不存在被懒启动重新拉起的可能；
- 时序：本 rc 的 early-init 在 1.10 s 执行，`class_start hal` 在 23.3 s，早 22 s。

## 5. 验证

实例 15（`out/core/daemon/runtime/cph2797_16.0.8.301_EX01/15/qemu.log`）：

| 指标 | 实例 13 / 14 | 实例 15 |
| --- | --- | --- |
| `WATCHDOG KILLING SYSTEM PROCESS` | 1 / 1 | **0** |
| `sysrq: Resetting` | 2 / 1 | **0** |
| `power.stats-impl.oplus` 日志 | 567 行 | **0** |
| `Check failed: s == STATUS_OK` | 281 | **0** |
| `starting service 'vendor.power.stats'` | 每 boot 1 次 | **0**（被 stop） |
| `boot_progress_enable_screen` | 0 | **1**（68,564 ms） |
| `sys.boot_completed` | 空 | **1** |

## 6. 遗留

- 这是一个「官方进程与自研 HAL 抢注同名实例」的通用模式：实例 13 的日志里
  `android.hardware.vibrator.IVibrator/default` 也出现了同样的 `Bad state?` 一行
  （PID 542↔852），本轮未阻塞开机，但同源风险存在，后续若振动异常先查这里。
- `handlefactory` 被 SELinux 拒绝（`default_android_service`）说明 lineage 底座缺少
  OPPO 的服务类型定义，这条路（让官方 power.stats 跑起来）比「停掉它」代价大得多，
  故选择后者；如后续需要真实功耗数据，应先补 `vendor.oplus.hardware.handlefactory`
  的域/服务类型再启用。
- `stop` 依赖「服务名固定」，若 OPPO 改 rc 名需同步；宜在 07 步骤加一条校验：
  `/odm/etc/init/*power.stats*.rc` 定义的服务名是否在我们的 stop 列表里。
