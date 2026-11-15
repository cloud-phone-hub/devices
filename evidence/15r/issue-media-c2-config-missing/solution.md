# 15R 云机：/vendor/etc/media_config.xml 缺失导致两个 c2 store 每 5 s 崩一轮

- **设备/机型**：OnePlus 15R（CPH2769 / ColorOS 16.0.10.500_EX01 / 骁龙 SM8750 `canoe` /
  Android 16 + lineage-23.2 底座）
- **固件基线**：`devices/manifest/oneplus/15r/baselines/CPH2769_16.0.10.500_EX01_A51_full.json`
- **首次暴露**：实例 8/9（2026-11-03），`android-hardware-media-c2-hal` 从 11.9 s 起每
  ~5 s 退出一次；实例 9 全程 352 次
- **结论**：`engine/vendor/hardware/android.hardware.media` 带进来的两个「假登记」c2 store
  （`android.hardware.media.c2-mediatek` → `IComponentStore/default1`、
  `vendor.dolby.media.c2-default-service-dax` → `default9`）启动时必须读
  `/vendor/etc/media_config.xml` 才知道自己要注册哪个 store 名、挂哪些组件条目。15r 的
  `/vendor` 是 lineage-23.2 基座 + 引擎自编 HAL，**没有这份文件**（官方 OnePlus vendor 的
  c2 走 `media_codecs*.xml` + `c2.qti.*`，与云机的 store 架构不同源），于是两个二进制都
  「读到空配置 → status 1 退出」，init 按 `class hal` 反复重启。修复 = 在机型静态树
  `manifest/oneplus/15r/vendor/etc/` 下自编一份，07 会随 `etc` 子树整体装配进 vendor.img。

---

## 一、问题

实例 9 能到桌面、`sys.boot_completed=1`、system_server 稳定，媒体却一直没就绪：

```
$ adb -s 127.0.0.1:5911 shell getprop init.svc.android-hardware-media-c2-hal
restarting
$ adb -s 127.0.0.1:5911 shell getprop init.svc.vendor-dolby-media-c2-hal
restarting
$ adb -s 127.0.0.1:5911 shell service list | grep media.c2
41      android.hardware.media.c2.IComponentStore/default: [...]
42      android.hardware.media.c2.IComponentStore/software: [...]
```

即：vendor VINTF 声明的 `IComponentStore/default1`（mediatek）与 `default9`（dolby）
**始终没有实例**，而这两个二进制在 init 里是 `class hal` 的常驻服务，失败即重启。

qemu.log 里每 ~5 s 一轮，pid 每轮换：

```
[   11.947122][    T1] init: ... started service 'android-hardware-media-c2-hal' has pid 623
[   12.152155][    T1] init: Service 'android-hardware-media-c2-hal' (pid 623) exited with status 1
[   11.991744][    T1] init: ... started service 'vendor-dolby-media-c2-hal' has pid 628
[   12.158184][    T1] init: Service 'vendor-dolby-media-c2-hal' (pid 628) exited with status 1
[   16.950018][    T1] init: ... started service 'android-hardware-media-c2-hal' has pid 1032
[   16.955929][    T1] init: Service 'android-hardware-media-c2-hal' (pid 1032) exited with status 1
```

副作用是 init 的 «updatable 进程崩了» 状态一直被点亮：
`getprop sys.init.updatable_crashing_process_name` = `android-hardware-media-c2-hal`，
`sys.init.updatable_crashing` 全程被反复置 1（实例 9：503 次；实例 10 只剩 46 次）。

## 二、根因

手动跑二进制，报错直接指到文件：

```
$ adb -s 127.0.0.1:5911 shell /vendor/bin/hw/android.hardware.media.c2-mediatek 2>&1 | head -3
I/O warning : failed to load "/vendor/etc/media_config.xml": No such file or directory
$ echo $?
1
```

（`vendor.dolby.media.c2-default-service-dax` 打同一句；mediatek 版直接 status 1 退出。）

三件事叠在一起：

1. **这份文件在 15r 的 /vendor 里根本不存在**。官方 ColorOS 的 codec2 是
   `media_codecs*.xml` + qcom `c2.qti.*` 那套，没有 AOSP 的 `media_config.xml`；
   而云机的 `/vendor/bin|lib64` 又被 cdh_redirect 重定向到 patch 盘，官方的
   `c2.qti.*` 对云机完全不可见，只剩引擎自编的两个「假登记」store。
2. **二进制把「配置缺失」当致命错误**：读不到文件 → 不注册任何 store → 立即 status 1。
   所以症状不是「某个编解码器不可用」，而是服务永远起不来、init 每 5 s 重拉一轮。
3. **一份配置管两个 instance**：`<instance>` 决定该 store 注册成哪个实例名
   （`default1` / `default9`），`<component>` 决定它对外报哪些编解码器。缺文件时两个
   instance 一起消失，服务表里只剩 v4l2（`default`）与软解（`software`）。

为什么要「假登记」而不是让它们直接不启动：vintf 的
`android.hardware.media.c2` manifest 片段（`manifest_media_c2_default.xml` + dolby 的
manifest）已经声明了这两个 fqname，mediaserver/codec2 会按声明去等；把 rc 删掉只会让
framework 侧一直等不到、反复重试，比「注册一个 createComponent 一律 CANNOT_DO 的空 store」
更糟。真实编解码由 v4l2（`default`）与 `software` store 承担。

## 三、证据

失败侧（实例 9，`log-fail.txt`）：

- `Service 'android-hardware-media-c2-hal' (pid …) exited with status 1` **352** 次、
  `vendor-dolby-media-c2-hal` **74** 次，首轮 11.947 s / 11.991 s，之后严格 ~5 s 一轮；
- `service list` 只有 `IComponentStore/default` 与 `/software`，`default1` / `default9` 缺失；
- 手动执行两个二进制均打 `I/O warning : failed to load "/vendor/etc/media_config.xml"`，
  mediatek 版 `EXIT=1`；
- `sys.init.updatable_crashing_process_name = android-hardware-media-c2-hal`，
  `sys.init.updatable_crashing=1` 被反复触发的次数 503。

**顺带否掉一个误判**：实例 9 的 zygote 在 12.596 s 被 SIGKILL，看着像被 c2 循环连坐；但实例 10
（c2 已修好）zygote 同样在 11.476 s 被 SIGKILL，且两次都紧跟在
`Service 'surfaceflinger' (pid …) received signal 6` 之后 9~11 ms。zygote 被杀的触发者是
**surfaceflinger 首启 abort**，与 media c2 无关（该残留记在 #127/#128 的遗留里）。

通过侧（实例 10，`log-pass.txt`）：

- `init.svc.android-hardware-media-c2-hal` / `init.svc.vendor-dolby-media-c2-hal` = `running`；
- `service list` 出现 `IComponentStore/default1` 与 `/default9`（4 个 store 齐）；
- `ls -l /vendor/etc/media_config.xml` = 3592 B，md5 `73641234cea2…`（与自编件一致）；
- 两个进程 `ps -o ELAPSED` = 04:50，与开机时长等长 → 一次启动、零重启；
- `media_config.xml` 相关报错 0、`exited with status 1` 名单里不再有这两个服务。

## 四、改动

**新增** `devices/manifest/oneplus/15r/vendor/etc/media_config.xml`（69 行，07 随 `etc`
子树整棵装配进 vendor.img，无需改 cdh_redirect.rules —— 文件在 `/vendor`，而 `/vendor`
本身由 patch 盘提供）：

```xml
<media-c2-config>
    <instance binary="android.hardware.media.c2-mediatek" store="default1"/>
    <instance binary="vendor.dolby.media.c2-default-service-dax" store="default9"/>
    <component store="default1" name="c2.mtk.avc.decoder" …         rank="1"/>
    <component store="default9" name="c2.dolby.eac3.decoder" …      rank="1"/>
    <component store="default9" name="c2.dolby.ac4.decoder" …       rank="1"/>
</media-c2-config>
```

三条硬约束（与 `manifest/samsung/ss9380` 的同名文件同口径，本机复现）：

1. 文件必须存在，否则 status 1（本条）；
2. 每个 `instance` 至少一条 `component`，空表 → status 3（ss9380 实测）；
3. `component` 的 `rank` 必须远低于软解的 512，这里统一写 `rank="1"`，保证软解优先，
   假 store 只在「完全没有该编解码器」时才被选中（结果仍是 CANNOT_DO，语义与真机一致）。

刻意只登记 **1 条 video + 2 条 dolby 音频**：video 只为让 `default1` 的表非空，
把 gold 的 15 条全搬过来会让 framework 误判「有 hevc/vp9/mpeg2 硬解」。

构建链：`build_vendor`（07，机型静态树写入 3，vendor.img 200577024 B）→ `build_patch`
（09，675 条目）→ `pack_vda`（11，patch.img sha256 `d293eb0594bd`）。

## 五、验证

实例 10（2026-11-03 09:43 启动），`sys.boot_completed=1`、`topResumedActivity=
com.android.launcher/.Launcher`：

| 指标 | 实例 9（修前） | 实例 10（修后） |
| :--- | :--- | :--- |
| `init.svc.android-hardware-media-c2-hal` | restarting | **running** |
| `init.svc.vendor-dolby-media-c2-hal` | restarting | **running** |
| 两个服务被拉起次数 | 352 / 74 | 1 / 1（另 1 次是我手工 `ctl.restart` 复验） |
| `IComponentStore/default1` | 无 | **有** |
| `IComponentStore/default9` | 无 | **有** |
| `media_config.xml` 读失败 | 每轮 1 次 | **0** |
| `sys.init.updatable_crashing_process_name` | android-hardware-media-c2-hal | vendor.oplus.vibrator（下一卡点） |
| 进程 ELAPSED | 每 5 s 换 pid | 与开机等长（未重启） |
| `FATAL EXCEPTION IN SYSTEM PROCESS` / `WATCHDOG KILLING` | 0 / 0 | 0 / 0 |

回归确认 #128 未回退：`power.stats-impl.oplus` 0 次、`Check failed: s == STATUS_OK` 0 次、
`service check android.hardware.power.stats.IPowerStats/default` = **found**。

**调试注意**：二进制是「单实例独占注册」的。在活实例上手工再跑一次同一个二进制，会把
servicemanager 里的 `default1` 抢走（kill 掉手工进程后原名也不回来），必须
`setprop ctl.restart android-hardware-media-c2-hal` 才能恢复 —— 与本仓库
`issue-power-stats-dual-registration` 是同一套 servicemanager 语义，属调试陷阱，不是产品缺陷。

## 六、遗留

- `vendor.secure_element`（108 次）与 `vendor.oplus.vibrator`（106 次）仍在 5 s 一轮的
  status 1 循环，`sys.init.updatable_crashing` 因此长期为 1；下一个条目处理；
- `webview_zygote` 仍反复 `stack corruption detected (-fstack-protector)`（实例 10：10 次，
  栈在 `AndroidRuntime::start` 的退出路径，不影响开机）—— #128 遗留延续；
- `surfaceflinger` 首启 1 次 `received signal 6` → init 连坐 SIGKILL zygote 一次（11.47 s），
  自恢复；两条实例一致，属既有残留，尚未定位 SF abort 的栈；
- 真编解码能力没验：本条目只闭环「store 实例存在」，`c2.mtk.avc.decoder` 仍是假条目，
  真实视频解码走 `IComponentStore/default`（v4l2），需要单独跑一路播放验证。
