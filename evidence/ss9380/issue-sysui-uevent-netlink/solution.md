# SystemUI 死于 `UEventObserver`（platform_app 缺 netlink_kobject_uevent_socket）→ 画面全黑

- 机型：samsung / Galaxy S25 Ultra（`SM-S9380`，`ss9380`，OneUI Android 16）
- 发现时间：2026-10-30
- 盘集：build10 → build12
- 状态：已解决（画面首次出图）

## 一、问题

`debug.hwui.renderer=skiagl` 之后 `!gpuCount` 崩溃归零、`sys.boot_completed=1`，
但**画面依然全黑，只有右下角一个返回箭头**；`dumpsys window` 里
`isKeyguardShowing=true`、`mCurrentFocus=null`，锁屏根本画不出来。
`wm dismiss-keyguard` + `input swipe` 都没用，因为每一秒都回到原状。

继续查进程：`pidof com.android.systemui` 每 1~4 s 换一次 pid（一次实测
`31683` → `3513`，crash buffer 里同一栈出现 **82 次**）。

## 二、根因

崩溃栈：

```
FATAL EXCEPTION: UEventObserver
Process: com.android.systemui, PID: 23638
java.lang.RuntimeException: Unable to open socket for UEventObserver
    at android.os.UEventObserver.nativeSetup(Native Method)
```

`UEventObserver` 要建一个 `netlink_kobject_uevent_socket`，被 SELinux 拒：

```
avc: denied { create } for pid=19136 comm="UEventObserver"
     scontext=u:r:platform_app:s0:c512,c768
     tcontext=u:r:platform_app:s0:c512,c768
     tclass=netlink_kobject_uevent_socket permissive=0
```

注意 **scontext 是 `platform_app`**：OneUI 把 SystemUI 跑在 `platform_app` 域里
（`ps -Z` 实测 `u:r:platform_app:s0:c512,c768 u0_a45 … com.android.systemui`），
而不是 AOSP 的 `systemui` 域。

这条授权在官方策略里**只存在于 vendor 侧**：

```
官方 vendor_sepolicy.cil 第 15175 行
(allow platform_app_202404 self (netlink_kobject_uevent_socket (read create bind setopt)))
官方 vendor_sepolicy.cil 第 16676 行
(allow system_app_202404  self (netlink_kobject_uevent_socket (read create bind setopt)))
```

我们用的是官方 system + lineage vendor，vendor 策略一换这两条就没了；plat 侧没有
等价条目（`grep netlink_kobject_uevent_socket plat_sepolicy.cil` 只有
`audioserver`/`gpsd` 等，没有 `platform_app`）。SystemUI 因此每个周期都死在
`nativeSetup`，锁屏窗口永远画不出来 → 全黑。

## 三、证据

- 失败态：`log-fail.txt`
  （82 次 `Unable to open socket for UEventObserver`、avc 原文、`ps -Z` 的域、
  pid 每 1~4 s 变化、两张全黑截图）
- 通过态：`log-pass.txt`
  （崩溃计数 0、pid 20 s 内不变、`dmesg` 里 netlink 拒绝归零、锁屏正常出图）

## 四、改动

`devices/manifest/samsung/ss9380/sepolicy/vendor_sepolicy.cil` 追加**第八段**，
除本条外一并按官方原句补齐同一实例上取证的其它噪音（perfmond / perfsdkserver /
dumpstate / surfaceflinger / zygote / system_app / coredomain）：

```cil
(allow platform_app self (netlink_kobject_uevent_socket (read create bind setopt)))
(allow system_app self (netlink_kobject_uevent_socket (read create bind setopt)))

(allow perfmond servicemanager (binder (call transfer)))
(allow perfmond servicemanager (fd (use)))
(allow perfmond proc_uptime (file (ioctl read getattr lock map open watch watch_reads)))
(allow perfsdkserver servicemanager (binder (call transfer)))
(allow perfsdkserver servicemanager (fd (use)))
(allow system_server socket_device (sock_file (write)))

(allow dumpstate dumplog_data_file (dir (...)))
(allow dumpstate dumplog_data_file (file (...)))
(allow dumpstate packages_list_file (file (ioctl read getattr lock map open watch watch_reads)))
(allow surfaceflinger dumplog_data_file (dir (...)))
(allow surfaceflinger dumplog_data_file (file (...)))
(allow zygote dumplog_data_file (dir (...)))
(allow zygote dumplog_data_file (file (...)))
(allow system_app dumplog_data_file (dir (...)))
(allow system_app dumplog_data_file (file (...)))

(allow coredomain em_version_prop (file (read getattr map open)))
```

类型名一律用**未版本化**写法：本机型没有 `precompiled_sepolicy`，init 现场用
`/system/bin/secilc` 把 plat / system_ext / product / vendor 的 cil 与
`system/etc/selinux/mapping/202504.cil` 一批编译，未版本化名字都是定义态。

## 五、验证

1. `07_build_vendor`→`11_pack_vda` 重跑通过（build12，`patch.img` sha256 `dfaffe889277`）；
2. `debugfs` 读回 `/vendor/etc/selinux/vendor_sepolicy.cil`，第八段落在第 4977 行起；
3. 新实例（index 2，全新 runtime 目录）：
   - `logcat -b crash -d | grep -c UEventObserver` → `0`；
   - `pidof com.android.systemui` 连续 20 s 稳定不变；
   - `dmesg` 里 `netlink_kobject_uevent_socket` 的 denied 归零；
4. 画面首次正常出图：锁屏可渲染（状态栏信号/电池、快捷图标、紧急呼叫按钮、
   右下返回箭头），`wm dismiss-keyguard` 后变成标准「滑动来打开」锁屏。

## 六、遗留

- 首次出图时锁屏上有一个 `Bouncer` 卡片，文案是
  「由于Device Services在未经授权的情况下被卸载，您的手机已锁定。」
  （framework-res 的模板 + 某三星组件把 `%1$s` 填成 `Device Services`）。
  `dumpsys device_policy` 里**没有**任何 device admin / device owner，属 SystemUI
  自绘的锁定提示，`wm dismiss-keyguard` 即可消掉 —— 但它会随 SystemUI 重启重现，
  需要在 SetupWizard 流程里彻底处理（另开条目）。
  → **已消（2026-10-31 复测）**：根因是 KnoxGuard 把锁屏置为 `secure`，见
  `../issue-knoxguard-china-region/`；该条修好后锁屏自行消失，`isKeyguardShowing=false`。
- 触摸注入（RFB 指针 / `input swipe`）目前在锁屏上打不开，需要单独确认
  `sec_touchscreen` 这条输入链路；`getevent -pl` 里没有 virtio 指针设备。
  → **已消（2026-10-31 复测）**：`input tap` 能完整走完 SetupWizard（勾选条款、翻页、
  点 HOME），触摸链路是通的；“点不动”只是 SystemUI 每秒重建窗口的假象。
- `com.qualcomm.location` 的 `DebugReportingMonitor` 每 0.7 s 崩一次（非致命）。
  → **仍在（2026-10-31 复测）**：`pm disable-user --user 0 com.qualcomm.location` 可
  暂时止住，但重启后会复活；属 APK 侧问题，不影响可用性。

## 七、复测记录（2026-10-31，最终盘集）

build12 之后又叠加了第九段（`sys.locksecured`）、第十段（`ro.csc.country_code`）策略，
因此在新盘集上重跑了一遍本条的通过性检查：

- 盘集：`patch.img` sha256 `53e483856316d50c05f117efeb13d9b2012aca02bf9e0afe760700a156f6259d`
  （`engine/out/tmp/ss9380-build14.log` 的那一次构建；注意构建编号在日志名与本文档间
  不统一，以 sha256 为准）；
- 实例：`ss9380_S9380ZCSDCZH1` / index 5，08:14 开机；
- 结果：`UEventObserver` 崩溃 0、`Unable to open socket` 0、
  `dmesg` 里 `netlink_kobject_uevent_socket` 0、SystemUI pid 2609 连续 20 s 不变、
  `isKeyguardShowing=false`、`topResumedActivity=…launcher/.activities.LauncherActivity`；
- 证据：同目录 `log-pass.txt`（含全部命令与输出）、`pass-desktop.png`（OneUI 桌面）。

