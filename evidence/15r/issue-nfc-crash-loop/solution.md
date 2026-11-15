# 15R 云机：com.android.nfc 崩溃风暴把 AMS 拖死（Watchdog → sysrq 复位）

- **设备/机型**：OnePlus 15R（CPH2769 / ColorOS 16.0.10.500_EX01 / SM8750 `canoe` /
  Android 16 + lineage-23.2 底座）
- **首次暴露**：实例 11 运行约 900 s 后整机「卡死 → 黑屏 → 自动重启」，重启后 45 s 内
  `screencap` / `dumpsys window` 又开始超时
- **结论**：`com.android.nfc` 是 persistent 应用，它启动链第一件事就要
  `vendor.oplus.hardware.nfc@1.0` 的 HIDL 实例；我们的 `/odm` 整棵重定向（#130）把该服务的
  rc 与二进制一起屏蔽后，`getService()` 抛 `NoSuchElementException`，system_server 无限重启它
  （实测 **9 次/秒**），AMS 的 monitor/main/ui/display/anim 五条线程被 crash 处理占死 75 s，
  `Watchdog` 判 `WATCHDOG KILLING SYSTEM PROCESS` → `sysrq: Resetting` → UEFI 重新引导。
  修法是让这个包**根本不存在**（APK 目录指向载体里一定不存在的路径），而不是补一个空 HAL。

---

## 一、问题

实例 11（`/odm` 整棵重定向后的第一台）在 uptime ≈ 900 s 附近失去响应：

- `adb shell` 单条命令 22 ms 到 40 s 全部超时，但 `adb devices` 仍显示 `device`；
- 宿主侧 `qemu` 进程 `%CPU` 480–537（8 vCPU 里约 5 个在跑），guest `top` 显示
  `800%cpu 4%user 537%sys 195%idle`；
- `dmesg` 出现 10 条 `soft lockup - CPU#n stuck for 26s/49s`，栈全是
  `queued_spin_lock_slowpath ← _raw_spin_lock_bh ← lock_sock_nested ← tcp_recvmsg`；
- 之后 `pidof surfaceflinger` 短暂为空、`/proc/uptime` 归零、qemu.log 出现
  `EFI stub: Booting Linux Kernel...`——整机复位，**不是 panic，是 init 主动 sysrq 重启**。

## 二、根因

复位前 60 秒（uptime 840–900 s）的事件窗里：

```
am_proc_start = 555 次
am_crash      = 555 次        （≈ 9 次/秒）
avc: denied   = 3663 条
Waited one second for … = 75 条
```

`am_crash` 的进程名全部是 `com.android.nfc`，异常栈：

```
FATAL EXCEPTION: main
java.util.NoSuchElementException
  at android.os.HwBinder.getService(HwBinder.java:150)
  at vendor.oplus.hardware.nfc.V1_0.IOplusNfc.getService(IOplusNfc.java:34)
  at NfcChipManager.getChipVersion(NfcChipManager.java:200)
  at NfcChipManager.<clinit>(NfcChipManager.java:)
  at NfcService.<init>(NfcService.java:)
  at NfcApplication.onCreate(NfcApplication.java:)
```

该 HIDL 服务的实现是官方 `/odm/bin/hw/vendor.oplus.hardware.nfc@1.0-service`，rc 在
`/odm/etc/init` —— 正是 #130 整棵屏蔽掉的那部分。于是每次进程起来都必崩，而
`com.android.nfc` 在 `/system/priv-app` 里带 persistent 标记，system_server 的
`OomAdjuster`/`crash` 处理路径会立刻重启它。

后果链：**9 次/秒的 fork+exec+crash 循环占满 AMS 五条关键线程（monitor / main / ui /
display / anim）达 75 s → Watchdog 触发 → init 走 `sysrq: Resetting`**。同一时间窗里
8 个 CPU 全卡在 `lock_sock_nested`（TCP 锁）+ 98% system，是 crash 风暴把调度与 RCU 拖崩
之后的次生现象（`khugepaged` 卡在 `lru_add_drain_all`、system_server 的 `OomAdjuster`
卡在 `synchronize_rcu`），不是主因。

**与用户操作无关**：对照实验（实例 12）开机后完全不碰界面，73 s 时 load 已 14.55、
133 s 涨到 20.86、线程 4358→6260；NFC 崩溃计数在实例 9/10/11 分别是 4133 / 7973 / 4107，
说明它是**长期存在**的（只是先前被别的问题掩盖），而 #130 把 `/odm` 整棵屏蔽后，NFC
服务的 rc 与二进制同时消失，风暴强度被放大到能触发 Watchdog。

## 三、证据

失败侧（`log-fail.txt`）：

- 实例 11：60 s 内 555 次 `am_proc_start` / 555 次 `am_crash`（≈9 次/秒）、
  `avc: denied` 3663、`Waited one second for` 75；
- 实例 11：`WATCHDOG KILLING SYSTEM PROCESS: Blocked in monitor ActivityManagerService …
  for 75s / Blocked in handler on main thread (main) for 75s / (android.ui) for 75s /
  (android.display) for 75s / (android.anim) for 75s`，随后 `sysrq: Resetting`，
  1398800 行附近出现第二次 `EFI stub: Booting Linux Kernel...`；
- 实例 11：`binder_alloc: 1012: pid 626 spamming oneway? 5559→6471 buffers`
  （pid 626 = composer HAL `drmhwc`，`EventThread: Faking VSYNC due to driver stall`）；
- 实例 11：8 核 `soft lockup`，栈首全为 `lock_sock_nested ← tcp_recvmsg ← inet6_recvmsg`，
  受害进程 `NetworkMonitor` / `AI-download` / `netd` / `OwmBaseUtils` / `Okio Watchdog` /
  `cdh-wifi-rx` / `binder` / `CronetNet`；
- 实例 9/10/11 的 NFC 进程重启次数：4133 / 7973 / 4107（长期存在，非本轮引入）；
- `pm disable-user --user 0 com.android.nfc` 返回 `new state: default`（persistent 应用，
  runtime 禁用无效，只能从镜像层解决）。

通过侧（`log-pass.txt`，实例 13，8 vCPU）：

- `pm list packages | grep -i nfc` 只剩 `com.oplus.nfcengineertest` 与
  `com.google.android.apps.walletnfcrel`，**`com.android.nfc` 消失**；
- `ps -A | grep -c com.android.nfc` = **0**；`ls -d /system/priv-app/NfcNci` = ENOENT；
- `logcat -b events | grep -c am_crash`：60 s 窗口 555 → **9**（且不再增长）；
- `dmesg | grep -c 'soft lockup - CPU'` = **0**；`sys.init.updatable_crashing` 为空；
- load **4.92**（对照实例 12 的 14.55）、线程 **2572**（对照 4358）；
- `init.svc.surfaceflinger` = running、`sys.boot_completed` = 1；
- 后续 4 vCPU 实例 14（同一镜像）：137 s load 27.87（GApps 首启）、248 s 回落到 4.31，
  线程 2453，`am_crash` 停在 18（开机期两批各 9 次）后不再增长，`soft lockup` / `hung task`
  在 744 s 之前一直为 **0**。

## 四、改动

**`devices/manifest/oneplus/15r/kernel/cdh_redirect.rules`**（唯一改动，纯配置）：

```
dir  /system/priv-app/NfcNci  /mnt/vendorpatch/system/priv-app/NfcNci
```

- 指向载体里**一定不存在**的路径：PackageManager 扫描 APK 用的是 Java `File` 绝对路径，
  会过 `getname_flags`，因此直接 ENOENT，连包记录都不建；
- framework 侧 `NfcServiceImpl` 绑定失败是框架支持的状态（`mNfc == null` 时 NFC 开关直接
  报不可用），不会崩；
- framework 的 `/system/framework`（`NfcNci` 的 codePath 之外）与 `com.android.nfc_extras`
  不受影响；
- 不选「补一个 NFC HAL 空壳」：官方 rc 还依赖 NFC 硬件节点与 `vendor_nxp` 权限，空壳只会把
  「崩溃」换成「死等」（contexthub / weaver 两次教训），且空壳还要多维护一份 VINTF 声明。

## 五、验证

| 指标 | 实例 11/12（改前） | 实例 13（改后，8 vCPU） | 实例 14（改后，4 vCPU） |
| :--- | :--- | :--- | :--- |
| `com.android.nfc` 包 | 存在 | **不存在** | 不存在 |
| 进程数 | 9 次/秒重启 | **0** | 0 |
| `am_crash`（60 s 窗口） | 555 | **9**（不增长） | 18（开机期两批，之后不增长） |
| `soft lockup` | 10 条 / 900 s | **0** | **0**（744 s 前） |
| load（首次采样） | 14.55（73 s） | **4.92** | 27.87（GApps 首启峰，248 s 回 4.31） |
| 线程数 | 4358 | **2572** | 2453 |
| AMS `WATCHDOG KILLING` | 有 | **0** | 0 |

> 说明：实例 14 在 744 s 出现另一族 `soft lockup`，栈顶是
> `cdh_rx_thread → netif_receive_skb → tcp_v4_rcv`（wifi 数据面自死锁），与本条目的 NFC
> 崩溃风暴无关，见 `../issue-hwsim-rx-self-deadlock/`。

## 六、遗留

1. NFC 功能在云机不可用（无 NFCC 硬件、无官方 HAL）。`com.google.android.apps.walletnfcrel`
   （Google 钱包）仍在，点击支付会提示设备不支持，属预期。
2. `com.oplus.nfcengineertest`（工程模式测试 app）仍在，它不会在开机时自动跑，暂不屏蔽；
   若后续发现它触发异常再按同样手法处理。
3. 本条目只解决「风暴」，不解决「NFC 缺失」：若将来要支持 NFC 模拟，需要
   `host-hvc` 加一路 NFC 通道 + 自建 `IOplusNfc` 实现，属于新特性而非修 bug。

## 七、更正（2026-11-03，#133 反编译与静态核查后）

正文中「该 HIDL 服务的实现是官方 `/odm/bin/hw/vendor.oplus.hardware.nfc@1.0-service`」
这句推断与实测不符。原文保留不动，记下两处更正（完整证据见
`../issue-nfc-oplus-hidl-missing/`）：

1. **厂商 odm 里没有 HIDL V1_0 的服务**。`odm_a/bin/hw` 三件套全是 AIDL——
   `android.hardware.nfc-service.nxp`（`android.hardware.nfc` V2 AIDL）、
   `vendor.oplus.hardware.nfc_aidl-service`（ColorOS 私有 `IOplusNfc`，AIDL）、
   `vendor.oplus.hardware.nfcExtns-service`（`INfcExtns`，AIDL）；`odm_a/lib64` 里也没有
   `vendor.oplus.hardware.nfc@1.0.so`。所以「`/odm` 整棵屏蔽后拿不到 HIDL 实现」这个因果
   链不成立：云机是 AIDL-only 系统（`HidlServiceManagement: hwservicemanager is not
   supported on the device`），HIDL 分支本来就不可达，`HwBinder.getService()` 抛
   `NoSuchElementException` 与 `/odm` 无关。
2. **`/odm` 整棵真正切掉的是 AIDL 分支**：`NfcChipManager.getChipVersion()` 先查
   `vendor.oplus.hardware.nfc.IOplusNfc/default`，其实现正是 `/odm/bin/hw/`
   `vendor.oplus.hardware.nfc_aidl-service`。而 `getChipVersion()` 的 `try` 只 catch
   `RemoteException`，接不住 HIDL 侧的 `NoSuchElementException`。

本条的处置（让包不存在）不变，实例 18 复验：`com.android.nfc` 0 进程、`am_crash` 中 0 条。
