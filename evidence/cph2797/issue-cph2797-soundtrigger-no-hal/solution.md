# CPH2797 启动卡在 SoundTrigger：vendor 侧既无实现也无 VINTF 声明

- 设备：OPPO Find X9 / CPH2797 / ColorOS 16.0.8.301_EX01（`board=mt6993`，联发科）
- 实例：失败 10（旧镜像）→ 通过 11（新镜像）
- 日期：2026-10-30
- 上游条目：#77（bringup 第一轮：图形 + 音频 VINTF）
- 日志：`log-fail.txt`（实例 10，`/tmp/cph10.log`）/ `log-pass.txt`（实例 11）

## 1. 问题

修完图形与音频 VINTF 之后（#77），`sys.boot_completed` 仍恒空，VNC 停在 OPPO
「Find X」开机动画。实例 10 的日志 163,765 行里，system_server 的启动进度停在：

```
SystemServerTiming: StartSoundTriggerMiddlewareService      ← 52.36 s，全日志最后一条
```

之后 150 s 内再无任何启动进度，最终：

```
[  151.422893][  T910] sysrq: Resetting
```

即 watchdog 判定 system_server 卡死，用 sysrq 把整机硬重启；重启后又卡在同一处，
形成「卡 150 s → 硬重启 → 再卡」的循环。

## 2. 根因

ColorOS 的 `SoundTriggerMiddlewareDefaultHalFactory` 拿 HAL 分两步，两步都失败且
**不允许失败**：

1. 先看 VINTF：`AServiceManager_isDeclared("android.hardware.soundtrigger3.ISoundTriggerHw/default")`。
   我们的 vendor 装配树里没有这个 fragment → servicemanager 回
   `Could not find ... in the VINTF manifest. No alternative instances declared in VINTF.`（52,900 次）。
2. 退回 HIDL：`ISoundTriggerHw::getService()`（`soundtrigger-V2.x`）也拿不到默认实例
   → 返回 null（41,129 次 `Connecting to default soundtrigger-V2.x.ISoundTriggerHw`）。

两次都失败时 framework 不降级，而是**在 system_server 主线程紧循环重试**
（2~5 ms 一次），于是：
- 主线程再也走不到下一个 `SystemServerTiming`（boot 卡死）；
- Watchdog 到点后 sysrq 重启整机。

为什么「本该有」：官方 ColorOS vendor **是有完整实现的**——
`/vendor/lib64/hw/android.hardware.soundtrigger3-impl.so`（85,400 B）、
`/vendor/lib64/hw/sound_trigger.primary.default.so`（101,888 B）、
`/vendor/lib64/android.hardware.soundtrigger3-V1/V3-ndk.so`，官方
`/etc/vintf/manifest.xml` 里也有独立的 `<hal format="aidl"><name>android.hardware.soundtrigger3</name>
<version>3</version><fqname>ISoundTriggerHw/default</fqname></hal>`。但我们的 vendor.img
是从 lineage 底座重建的（官方 `/vendor` 不整树沿用），这些件全丢。

为什么官方没有独立进程却能注册服务：真机上 soundtrigger 由 MTK audio HAL
（`vendor.audio-hal-aidl` → `/vendor/bin/hw/android.hardware.audio.service-aidl.mediatek`）
**进程内注册**；而 lineage 的 audio HAL 是 AOSP 形态，没有这一段。

## 3. 证据

失败侧（实例 10）：

| 指标 | 值 |
| :--- | :--- |
| `Could not find android.hardware.soundtrigger3...` | 52,900 |
| `Connecting to default soundtrigger-V2.x.ISoundTriggerHw` | 41,129 |
| `sysrq: Resetting` | 1 |
| 最后一条 `SystemServerTiming` | `StartSoundTriggerMiddlewareService`（52.36 s） |

镜像侧：我方 `vendor.img` 的 `/bin/hw` 只有 audio 两个服务，`/etc/vintf/manifest/` 下
无 soundtrigger3 片段；官方 vendor 镜像里三件实现齐备（`dump.erofs --ls/--cat` 回读）。

关键前置结论（决定方案方向，实测于 AOSP 源码）：
`hardware/interfaces/soundtrigger/2.3/default/SoundTriggerHw.cpp` 里
`SoundTriggerHw::onFirstRef()` 的 `hw_get_module_by_class()` 失败**只打 ALOGE 就 return**，
`mHwDevice` 保持 NULL，服务照常 `addService` 成功；构造函数也不打开硬件。
→ **一个 0 module 的空 HAL 就能让 framework「注册成功」并降级**，不需要任何语音硬件。

## 4. 改动

新增自研 HAL 模块 `engine/vendor/hardware/android.hardware.soundtrigger3/`（约 200 行）：

- `src/SoundTriggerHw.h/.cpp`：`BnSoundTriggerHw` 的 11 个方法。
  `getProperties()` 返回 `Properties{}`（**0 module**，这是打断循环的关键）；
  `loadSoundModel`/`getParameter` 返回 `-ENODEV`；`startRecognition` 返回
  `EX_UNSUPPORTED_OPERATION`；`queryParameter` 返回 `nullopt`（表示不支持查询）；其余 `ok()`。
- `src/main.cpp`：`AServiceManager_addService(... /default)` + `ABinderProcess_joinThreadPool()`。
- `aidl/android/hardware/soundtrigger3/*.aidl`：模块自带的 3 个接口（从 AOSP 树拷）。
- `vendor/etc/vintf/manifest/android.hardware.soundtrigger3.xml`：
  `<manifest version="1.0" type="device">` + `ISoundTriggerHw/default`（**version 属性不能省**，
  裸 `<manifest/>` 会让整个 device manifest 变成 `NULL VINTF MANIFEST`，见 #77 实例 9 的教训）。
- `vendor/etc/init/android.hardware.soundtrigger3-service.rc`：
  `service vendor.soundtrigger3-hal /vendor/bin/hw/android.hardware.soundtrigger3-service.example`
  + `class hal` / `user audioserver` / `seclabel u:r:hal_audio_default:s0`。
- `sepolicy/vendor_service_contexts`：
  `android.hardware.soundtrigger3.ISoundTriggerHw  u:object_r:hal_audio_service:s0`。

构建配置 `engine/vendor/build/modules/android.hardware.soundtrigger3.json`：
`kind=aidl`、`depsHal=audio`、`extraAidlRoots`（模块自带 AIDL 根，`hasHash:false`）、
`frameworkTypes=[android/media/soundtrigger, android/media/audio/common]`
（后者是 `RecognitionEvent` 依赖 `AudioConfig` 的编译期头，缺它会
`fatal error: 'aidl/android/media/audio/common/AudioConfig.h' file not found`）。
机型清单 `manifest/oppo/cph2797/device.json` 增加一条 `android.hardware.soundtrigger3@default`。

为什么借用 audio 域：基底 plat 策略里没有 `hal_soundtrigger*` 类型，自造标签会落
unlabeled 导致 `addService` 拿 `-EPERM`；`hal_audio_default` + `hal_audio_service`
的 add/find 授权现成（audio 模块的 cil），且真机上 soundtrigger 本就跑在 audio HAL 进程内，
语义一致。可执行标签由 07 的 `ensureHalExecLabels` 从 rc 的 `seclabel` 反推补全
（本轮新增 `android.hardware.soundtrigger3-service.example -> hal_audio_default_exec`）。

## 5. 验证

实例 11（新 vendor.img，20 分钟观察）：

| 指标 | 失败（实例 10） | 通过（实例 11） |
| :--- | :--- | :--- |
| `Could not find android.hardware.soundtrigger3` | 52,900 | **0** |
| `Connecting to default soundtrigger-V2.x` | 41,129 | **0** |
| `sysrq: Resetting` | 1 | **0** |
| `SystemServerTiming` 进展 | 停在 `StartSoundTriggerMiddlewareService` | 155 ms 后进入 `StartSoundTrigger` 并继续 |

- servicemanager 由「找不到」变为 `Found android.hardware.soundtrigger3.ISoundTriggerHw/default
  in device VINTF manifest.`；framework 走 AIDL（`Connecting to default soundtrigger3.ISoundTriggerHw`），
  不再退回 HIDL。
- HAL 自己的日志 `getProperties: 0 modules (stub HAL)` 与 `Connecting` 同量级（每轮 boot 一次）。
- guest 内 `ps -A | grep soundtrigger`：`audioserver ... S android.hardware.soundtrigger3-service.example`
  —— 进程常驻 `joinThreadPool`，状态 S，无崩溃重启。
- 镜像回读：`bin/hw/android.hardware.soundtrigger3-service.example` 1,538,264 B(0755)、
  rc 与 VINTF 片段 0644、`vendor_service_contexts` 追加带 marker 的模块层块、
  `vendor_file_contexts` 补 25 条可执行标签。
- 同一实例上 `input buffer not gpu readable` / `TemporaryPrecompile` / `IModule/virtual`
  均为 0，`NULL VINTF MANIFEST` 也为 0（未回退 #77 的成果）。

## 6. 遗留

1. **本轮仍未 boot_completed，但瓶颈已换人**：每一轮 system_server 死在
   `BiometricServiceRegistry` 线程的
   `java.lang.ArrayIndexOutOfBoundsException`（`FingerprintServiceProviderExtImpl.init()` →
   `SensorList.valueAt(0)` 撞空表）→ `FATAL EXCEPTION IN SYSTEM PROCESS` → 自杀，
   zygote 约 15 s 一轮。已按同一套「模块 + 变体」办法处理（`fakefingerprint` 变体上报 1 个
   不接硬件的占位传感器），另起一条 issue 固化。
2. **HAL 是空实现**：不接任何语音硬件，`startRecognition` 恒 `EX_UNSUPPORTED_OPERATION`。
   若后续要「语音唤醒」能力，得走 virtio-snd 采集 + 自研检测，属功能面而非启动面。
3. **服务名带 `.example`**：`services[].name` 直接映射 `bin/hw/<name>`，AOSP 该模块的
   目录名就带 `.example`，沿用未改名。若后续要贴合官方命名（`android.hardware.soundtrigger3-service`），
   只改 json 的 `name` 与 rc 里的路径即可，不影响 VINTF 的 fqname。
4. **updatable 崩溃面（#77 遗留 1）未动**：`power.stats-impl.oplus` 的
   `Check failed: s == STATUS_OK`（每 5 s 一次）仍在，属独立条目。
