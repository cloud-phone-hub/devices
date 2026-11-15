# CPH2797：/odm 声明了 contexthub v4 但实现不存在，ContextHubSystemService 把 system_server 主线程拖死 65 s

- 设备：OPPO Find X9 / CPH2797 / ColorOS 16.0.8.301_EX01（`board=mt6993`，联发科）
- 实例：失败 14 → 通过 15
- 日期：2026-10-30
- 上游条目：#80（power.stats 抢注）
- 日志：`log-fail.txt`（实例 14）/ `log-pass.txt`（实例 15）

## 1. 问题

power.stats 修掉之后（#80），实例 14 又出现同形态的 Watchdog 判死：

```
W Watchdog: *** WATCHDOG KILLING SYSTEM PROCESS: Blocked in handler on main thread (main) for 65s
W Watchdog:     at java.util.concurrent.FutureTask.awaitDone(FutureTask.java:500)
W Watchdog:     at com.android.internal.util.ConcurrentUtils.waitForFutureNoInterrupt(ConcurrentUtils.java:86)
W Watchdog:     at com.android.server.ContextHubSystemService.onBootPhase(ContextHubSystemService.java:52)
[  134.506054][  T901] sysrq: Resetting
```

堆栈比对：卡点从 `PowerStatsService.onBootPhase` 换成了
`ContextHubSystemService.onBootPhase`，位置在后面一个 boot phase。

## 2. 根因

官方 `/odm` 声明了 contexthub，但实现不在我们的 vendor 里：

```
$ cat /odm/etc/vintf/manifest/android.hardware.contexthub-service.tinysys.xml
<manifest version="1.0" type="device">
    <hal format="aidl">
        <name>android.hardware.contexthub</name>
        <version>4</version>
        <fqname>IContextHub/default</fqname>
    </hal>
</manifest>

$ cat /odm/etc/init/android.hardware.contexthub-service.tinysys.rc
service vendor.contexthub-default /vendor/bin/hw/android.hardware.contexthub-service.tinysys
    class hal late_start
    user context_hub
    ...
```

`/odm/bin/hw` 里没有这个二进制（MTK 把 tinysys 版放在官方 `/vendor`，本机
`/vendor` 是 lineage 底座 + 自研 HAL），而且该 rc **没有 `interface aidl` 行**，
所以 servicemanager 的懒启动也找不到它，只能每秒打一条：

```
init: Could not start service 'vendor.contexthub-default' as part of class 'hal':
      Cannot find '/vendor/bin/hw/android.hardware.contexthub-service.tinysys': No such file or directory
servicemanager: Caller(pid=767,uid=1000,sid=u:r:system_server:s0) Tried to start aidl service
                android.hardware.contexthub.IContextHub/default as a lazy service
```

ColorOS 的 `ContextHubSystemService.onBootPhase` 在
`PHASE_THIRD_PARTY_APPS_CAN_START` 用 `ConcurrentUtils.waitForFutureNoInterrupt`
等后台 Future，那个 Future 正是取 `IContextHub` 实例；**声明存在 → 等到死也不放弃**，
主线程 65 s 不动 → Watchdog 杀 system_server → sysrq 硬重启。

## 3. 证据

- 实例 14：`contexthub` 相关日志 1,200 行、`WATCHDOG KILLING` 1 次、`sysrq: Resetting` 1 次、
  `boot_progress_enable_screen` 0；
- 实例 15（遮掉声明后）日志里出现决定性一行：

  ```
  IContextHubWrapper: Context Hub AIDL service is not declared
  ```

  说明框架走的确实是「声明判定」分支，未声明即立刻返回空实例；
- 对照 m22：`out/core/daemon/runtime/m22_12.5.0.0A/45/qemu.log` 里 `contexthub`
  引用 0 次，照样能开机——「无 hub」是合法姿态，不需要自研 HAL。

## 4. 改动

沿用 #77 已建立的「空 manifest 遮蔽」手法，只加一条 mount：

`devices/manifest/oppo/cph2797/vendor/etc/init/init.virtio_compat.rc` 的
`on early-init` 末尾追加

```
mount none /mnt/patch/vendor/etc/compat/vintf_empty.xml \
      /odm/etc/vintf/manifest/android.hardware.contexthub-service.tinysys.xml bind wait
```

为什么可行：

- servicemanager 在 3.2 s 首次读 device VINTF 并缓存，本 rc 的 early-init 在 1.1 s
  执行（早于缓存建立），遮蔽后 `isDeclared` 直接为假；
- 不用自研空 HAL：contexthub v4 有 20+ 个方法（nanoapp 加载、endpoint 通信、
  session 状态），空实现只会把「卡等待」变成「运行期才出错」，遮蔽声明更贴近
  「云机没有低功耗传感器中枢」的物理事实。

## 5. 验证

实例 15（`out/core/daemon/runtime/cph2797_16.0.8.301_EX01/15/qemu.log`）：

| 指标 | 实例 14 | 实例 15 |
| --- | --- | --- |
| `contexthub` 相关日志 | 1,200 行 | 7 行（全是 rc 解析/一次 class_start 失败） |
| `WATCHDOG KILLING` | 1 | **0** |
| `sysrq: Resetting` | 1 | **0** |
| `IContextHubWrapper` | — | `Context Hub AIDL service is not declared` |
| `boot_progress_enable_screen` | 0 | **1**（68,564 ms） |
| `sys.boot_completed` | 空 | **1** |

## 6. 遗留

- 剩下 7 行 contexthub 日志里有一次
  `init: Could not start service 'vendor.contexthub-default' ... No such file or directory`
  （rc 还在、二进制缺失），无害但吵；可考虑连 rc 一起遮，但 `/odm/etc/init/*.rc`
  在 0.86 s 解析、早于 early-init（1.1 s），init 的 mount **帮不了已解析的 rc**，
  因此保留。
- 同类「官方声明 + 实现旷缺」在 `/odm/etc/vintf/manifest/` 还有 40+ 条目，本轮只按
  实际卡点处理（virtual audio、power.stats、contexthub）。建议后续在 07 步骤加一条
  巡检：把「odm 声明了但 /vendor/bin/hw 里没有对应二进制、也无我方实现」的 HAL 列出来，
  提前决定「遮蔽声明」还是「补空实现」。
- 云机的传感器中枢能力（nanoapp/CHRE）永久缺失，依赖 contexthub 的
  AI/低功耗特性不可用。
