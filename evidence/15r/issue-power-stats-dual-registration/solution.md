# 15R 云机：官方 power.stats 实现与自建空 HAL 抢注同一 AIDL 实例

- **设备/机型**：OnePlus 15R（CPH2769 / ColorOS 16.0.10.500_EX01 / 骁龙 SM8750 `canoe` /
  Android 16 + lineage-23.2 底座）
- **固件基线**：`devices/manifest/oneplus/15r/baselines/CPH2769_16.0.10.500_EX01_A51_full.json`
- **首次暴露**：实例 8（2026-11-03 09:19 启动），全程 193 次 SIGABRT、3131 次 lazy start 失败
- **结论**：官方 `/odm/etc/init/android.hardware.power.stats-service.oplus.rc` 定义的
  `vendor.power.stats`（`class hal`，无 `interface aidl` 行）被 `class_start hal` 拉起，
  它注册完 `android.hardware.power.stats.IPowerStats/default` 后接着注册 Oplus 私有的
  `vendor.oplus.hardware.handlefactory.IHandleFactory/default`，被 SELinux 拒 →
  `CHECK(s == STATUS_OK)` 失败 → SIGABRT；而它的同名二次注册已经把 servicemanager 里
  这个名字移除，我们的实现从此「进程活着但查不到」。修复 = 用 cdh_redirect 把这份官方 rc
  整份换掉（服务不再定义），让我们的实现独占注册。

---

## 一、问题

实例 8 能到桌面、`sys.boot_completed=1`、system_server 稳定，但两个指标一直异常：

```
$ adb -s 127.0.0.1:5911 shell "getprop init.svc.vendor.power.stats"
restarting
$ adb shell "getprop init.svc.vendor.power.stats-default"
running
$ adb shell "service check android.hardware.power.stats.IPowerStats/default"
Service android.hardware.power.stats.IPowerStats/default: not found
```

日志侧：`android.hardware.power.stats-impl.oplus` 共 1036 行、
`Check failed: s == STATUS_OK` 193 次、`signal 6` 469 次，
`IPowerStats/default' could not be found trying to start it as a lazy AIDL service` 3131 次。
表现为 init 每 ~5 s 拉起一轮官方 HAL、每轮新 pid、每轮立刻 SIGABRT。

## 二、根因

三条链叠加：

1. **官方实现靠 class 启动，不受懒启动语义约束。**
   固件 `odm_a/etc/init/android.hardware.power.stats-service.oplus.rc` 只有四行：

   ```
   service vendor.power.stats /odm/bin/hw/android.hardware.power.stats-impl.oplus
       class hal
       user system
       group system root
   ```

   **没有 `interface aidl` 行**，所以 `class_start hal`（boot 阶段 12.14 s）无条件起它；
   而懒启动（`ctl.interface_start aidl/...IPowerStats/default`）只会命中 07 自建、
   带 `interface aidl` 行的那一个。

2. **它一启动就抢注同名实例，然后自己崩。**
   注册顺序是 `IPowerStats/default` → `IHandleFactory/default`。第二个 add 被 SELinux 拒：

   ```
   avc: denied { add } for pid=629 name=vendor.oplus.hardware.handlefactory.IHandleFactory/default
        scontext=u:r:hal_power_stats_default:s0 tcontext=u:object_r:default_android_service:s0
   ```

   `AServiceManager_addService` 返回非 OK，代码里是 `CHECK(s == STATUS_OK)` →
   `Fatal signal 6`。崩溃前还打两条 `Scan dir /sys/devices/platform/soc failed`
   （云机没有 SoC 平台目录，它的能量统计本就没有数据源）。

3. **同名二次注册把名字从服务表里摘掉，且我们的实现不会重注册。**
   servicemanager 只认第一个注册者：

   ```
   servicemanager: Service 'android.hardware.power.stats.IPowerStats/default'
     originally registered from PID 542 but it is being registered again from PID 629.
     Bad state? Late death notification? Multiple instances installed?
   ```

   我们的 `vendor.power.stats-default`（pid 542）在 19:00:09.805 注册成功，之后只在启动时
   注册一次；名字被摘掉后它仍然 `running`（日志里每次 `requested start, but it is already
   running`），但 `service check` 永远 `not found`。system_server 于是反复
   `could not be found trying to start it as a lazy AIDL service`。

VINTF 一侧：`IPowerStats/default` 的声明在 `android.hardware.power.stats.IPowerStats-V2.xml`，
`IHandleFactory/default` 的声明在 `android.hardware.power.stats-service.oplus.xml`——
后者此前已经用空 manifest 覆盖件顶掉了（云机无实现、无客户端），但**遮盖声明并不能阻止 rc
把进程拉起来**，这正是本条的缺口。

## 三、证据

- `log-fail.txt`：实例 8 的双注册告警、官方实现单轮崩溃链（含 avc、CHECK、signal 6）、
  init 每 5 s 重启的 pid 序列、system_server 3131 次 lazy 失败、运行态 `not found`，
  以及固件里两份 manifest 与 rc 的原文。
- `log-pass.txt`：实例 9 的 `Parsing file /odm/etc/init/android.hardware.power.stats-service.oplus.rc`
  （解析到的是注释版，`grep -v '^#' | wc -l` = 0）、`vendor.power.stats` 全程 0 次、
  `service check ... = found`、崩溃类计数全部归零。

## 四、改动

| # | 文件 | 改动 |
| :--- | :--- | :--- |
| 1 | `manifest/oneplus/15r/odm/etc/init/android.hardware.power.stats-service.oplus.rc`（新增） | 官方 rc 的覆盖件：整份删空（0 条有效行），原件四行注释保留备查 |
| 2 | `manifest/oneplus/15r/kernel/cdh_redirect.rules` | 新增 `file /odm/etc/init/android.hardware.power.stats-service.oplus.rc /mnt/vendorpatch/odm/etc/init/android.hardware.power.stats-service.oplus.rc` |
| 3 | 重建 | `build_odm` → `build_vendor_boot`（新 rules 进 vendor_ramdisk）→ `build_patch` → `pack_vda` |

为什么用「整份 rc 覆盖」而不是 cph2797 #80 的 `stop vendor.power.stats`：

- 两者等效（`stop` 靠 `Service::StopOrReset` 置 `SVC_DISABLED`，`class_start` 走
  `StartIfNotDisabled` 跳过），但 `/odm` 是只读 erofs，本机没有 cph2797 那种
  `vendor/etc/init/init.virtio_compat.rc` 载体；
- 本机型已有「cdh_redirect 换掉 `/odm` 下整份 rc」的既有手法（同目录 `oplus_performance.rc`，
  以及规则表里的 contexthub qmi rc），覆盖件比 `stop` 更彻底：进程根本不创建，既不被
  `class_start hal` 拉起，也不会被 lazy start 命中官方二进制；
- 语义上也不需要这个实现：云机没有 per-rail 能量计，它 `Scan dir /sys/devices/platform/soc`
  本来就读不到数据；07 自建的空实现（六个查询方法回空数组）已经稳定服务了实例 7~9。

## 五、验证

实例 9（2026-11-03 09:35 启动，`adb -s 127.0.0.1:5911`）：

| 判据 | 修复前（实例 8） | 修复后（实例 9） |
| :--- | :--- | :--- |
| `power.stats-impl.oplus` 日志行 | 1036 | **0** |
| `Check failed: s == STATUS_OK` | 193 | **0** |
| `signal 6`（全机） | 469 | **9**（全是 surfaceflinger 首启 1 次 + webview_zygote，与本条无关） |
| `IPowerStats` lazy 失败 | 3131 | **0** |
| `service check android.hardware.power.stats.IPowerStats/default` | not found | **found** |
| `init.svc.vendor.power.stats` | restarting | **（无此服务，getprop 为空）** |
| `init.svc.vendor.power.stats-default` | running（但名字已丢） | **running** |
| `ps -A \| grep power` | 官方 impl 反复出现 | **只有我们的 `android.hardware.power.stats-service`** |
| `sys.boot_completed` | 1 | **1** |
| `topResumedActivity` | launcher | **`com.android.launcher/.Launcher`** |
| `FATAL EXCEPTION IN SYSTEM PROCESS` / Watchdog | 0 / 0 | **0 / 0** |

## 六、遗留

1. `webview_zygote` 反复 abort：`Abort message: 'stack corruption detected (-fstack-protector)'`，
   实例 8 与实例 9 各 8 次（实例 7 因 wifi NPE 循环重启只跑了 8 分钟、尚未触发）。进程能重启、
   桌面可用，但 WebView 容器（`com.google.android.webview:webview_service`）会随之中断。
   待下一轮定位（怀疑是 lineage-23.2 的 webview_zygote 与 ColorOS 的 `libwebviewchromium`
   或 `-fstack-protector` 生成的 twi 栈守卫与云机内核 `PAC` 组合不兼容）。
2. `com.android.nfc` 反复 `NoSuchElementException`（栈在 `HwBinder.java`）、
   `avc: denied { find } ... vendor.oplus.hardware.urcc.IUrcc/default`，
   均不阻塞开机，待定位。
3. `19395` 条 `default_prop` 属性 set 类 avc 仍未收口（见 #127 遗留 3）。
4. 本条只处理「同名抢注 + 官方进程崩溃」。若将来要恢复真实的能量统计，
   需要给 `hal_power_stats_default` 补 `add` 到 `vendor.oplus.hardware.handlefactory`
   的权限并让它与我们的实现二选一，而不是并存。
