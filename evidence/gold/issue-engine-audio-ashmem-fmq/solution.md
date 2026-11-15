# Issue: engine 镜像音频全哑（`listAudioPorts error -19` 每 101 ms、`Devices:` 全空）——audio HAL 缺 `/dev/ashmem` 致 FMQ 失败

- 设备：Xiaomi Redmi Note 13 5G（`gold`），**engine 路线**镜像 `gold_cn_OS3.0.10.0.VNQCNXM`
- 宿主：macOS（darwin，QEMU + hvf）
- 对照实例：`gold_cn_OS3.0.10.0.VNQCNXM/1`（gold-15 路线，audio 正常，`Devices: speaker(2)`）
- 状态：**已打通**（2026-10-27，实例 `/3`）

---

## 1. 问题现象（修复前）

system_server 侧 AudioService 每 101 ms 刷一条错误，`dumpsys audio` 里所有流都没有输出设备：

```
$ adb shell "dumpsys audio | grep -m4 'Devices:'"
   Devices:
   Devices:
   Devices:
   Devices:

$ adb shell "dumpsys audio | sed -n '/Stream volumes/,/STREAM_RING/p'"
- STREAM_VOICE_CALL:
   Current: 40000000 (default): 8
   Devices:
   Volume Group: n/a                    <- 对照实例是 AUDIO_STREAM_VOICE_CALL

$ adb shell "logcat -d -b main -b system | grep -c 'listAudioPorts error'"
656                                     <- 每次查询 +1，永不停止

$ adb shell "dumpsys audio | grep -c 'initStreamVolume failed'"
（持续重试，`failed with 1 will retry`）
```

干扰项（排查时反复踩）：

- `dumpsys media.audio_policy` / `dumpsys media.audio_flinger` 均报
  `FAILED_TRANSACTION`，但**对照实例 1 也报同样的错** —— 这是该环境的通用现象
  （shell 权限下 dump AIDL audioserver 服务），**不是 bug**，不能当判据。
- `setprop ctl.restart audioserver` 后 HAL 侧日志一切正常（`openOutputStream: primary OK`、
  `getAudioPorts: primary port_count=7`），但 `Devices:` 依旧空 —— 因为 system_server
  缓存的 APS binder 失效后不会自动重连，**验证必须整机重启**。
- `IModule/primary` 是否注册也**不是**判据（注册成功时音频仍然全哑，见 §3 时间线）。

## 2. 根因

`hal_audio_default` 域缺少 `/dev/ashmem` 的 `chr_file` 授权（lineage 基座的音频 HAL 在
APEX 内，vendor 策略表里没有自研服务这一行）。失败链如下：

1. `VdacStreamChannel` 用 `AidlMessageQueue` 建命令/应答/数据三条 FMQ，分配共享内存
   必须 `open("/dev/ashmem")`；被 SELinux 拒绝后 ashmem fd = -1。
2. `fillDescriptor()` 把三条 MQ 的 `MQDescriptor` 塞进 `StreamDescriptor`，其中
   `ParcelFileDescriptor` 变成 -1。
3. `IModule::openOutputStream` 的 reply parcel 反序列化失败：AudioFlinger 拿到
   `stream = nullptr`、`status = -2147483640`（= `0x80000008` = `STATUS_BAD_INDEX`），
   尽管 HAL 自己已经走到 `return ndk::ScopedAStatus::ok()` 并打了 `openOutputStream: primary OK`。
4. APM 第一次打开默认输出设备就失败 →
   `Cannot open output stream ... is unreachable` → `The policy configuration does not
   declare a primary output` → **AudioPolicyManager 构造失败、当场析构**。
5. 此后 `AudioPolicyInterfaceImpl` 恒打印 `mAudioPolicyManager == NULL`，
   `getDevicesForStrategyInt no device found for strategy 0..9`。
6. system_server 侧表现为：`listAudioPorts` 返回 `-19`（`NO_INIT`，APS 拿不到 APM）、
   `initStreamVolume` 永久重试、`Devices:` 为空。

关键区分点：**服务注册（binder）只需要 `service_manager add/find`，不需要共享内存**。
所以「`IModule/primary` 注册成功」与「音频能不能出声」是两件独立的事 —— 这也是本 issue
第一次修复（只补 CIL 注册授权 + `vendor_service_contexts` 映射）未能解决的原因。

## 3. 证据

见同目录 `log-fail.txt`（修复前）与 `log-pass.txt`（修复后）。决定性片段：

```
01-01 08:00:16.627 I/AudioFlinger: loadHwModule() Loaded primary audio interface, handle 10   <- HAL 加载成功
01-01 08:00:16.630 E/APM_Config   : aidl2legacy_AudioHwModule_HwModule Line: 88 Failed result (BAD_VALUE)
01-01 08:00:16.630 E/APM_Config   : loadFromAidl Line: 275 Failed
01-01 08:00:16.681 D/AudioFlinger: AudioStreamOut::open(), HAL returned stream 0x0, ... status -2147483640
01-01 08:00:16.681 W/APM_AudioPolicyManager: Cannot open output stream for devices AUDIO_DEVICE_OUT_SPEAKER, @: on hw module primary
01-01 08:00:16.682 E/APM_AudioPolicyManager: Default device AUDIO_DEVICE_OUT_SPEAKER, @: is unreachable
01-01 08:00:16.682 W/APM_AudioPolicyManager: The policy configuration does not declare a primary output
01-01 08:00:16.682 D/AudioPolicyManagerCustomImpl: ~AudioPolicyManagerCustomImpl()   <- APM 被析构
07-15 21:03:13.631 V/AudioPolicyInterfaceImpl: mAudioPolicyManager == NULL            <- 之后永久为 NULL
```

同一时刻 HAL 侧（pid 537）打的是成功：

```
01-01 08:00:16.681 I/cdh_audio: setAudioPortConfig: primary port=2(speaker) cfgId=0 ...
01-01 08:00:16.681 I/cdh_audio: openOutputStream: primary OK cfgId=2 frames=1 buffer=8192
```

**「HAL 说 OK、framework 收到 null」这一对矛盾**是本 issue 的定位锚点：问题不在 HAL 逻辑，
而在 reply 里的 FMQ 描述符打包失败。

### 行号核对（源码在 `engine/vendor/out/android16/aosp/platform/frameworks/av/`）

- `audiopolicy/common/managerdefinitions/src/AudioPolicyConfig.cpp:88` 正是
  `legacy2aidl_audio_port_v7_AudioPortFw(legacyPort)`。
- `services/audioflinger/datapath/AudioStreamOut.cpp:106` 的
  `status = hwDev()->openOutputStream(...)` 直接透传 HAL 返回值，
  `-2147483640` 即 `STATUS_BAD_INDEX`（`utils/Errors.h` 的 `UNKNOWN_ERROR + 8`）。

### 与 gold-15 的既有结论对齐

`devices/xiaomi/gold-15/build/vendor/build/sepolicy.py` 的 `audio_hal_cil_v2()`
文档已记录同源现象（实例59）：

```
avc denied {open} hal_audio_default ashmem_device chr_file
-> ashmem fd -1 -> mmap EBADF 9 -> StreamDescriptor ParcelFileDescriptor=-1
-> openOutput/openInputStream 在 framework 侧失败
```

## 4. 改动

### 4.1 audio sepolicy 片段（新增 ashmem + /dev/snd 两段）

`engine/vendor/hardware/android.hardware.audio/sepolicy/vendor_sepolicy.cil`
（`07_build_vendor` 会把它并入装配树的 `etc/selinux/vendor_sepolicy.cil`，
`{{vers}}` 替换为 `plat_sepolicy_vers.txt` 的值，本机 = 202404）：

```cil
(allow hal_audio_default hal_audio_service_{{vers}} (service_manager (add find)))
(allow hal_audio_default default_android_service_{{vers}} (service_manager (add)))
(allow audioserver_{{vers}} default_android_service_{{vers}} (service_manager (find)))
(allow audioserver_{{vers}} hal_audio_service_{{vers}} (service_manager (find)))
(allow hal_audio_default ashmem_device_{{vers}} (chr_file (ioctl read write getattr map open)))   ;; 本 issue 的关键
(allow hal_audio_default audio_device_{{vers}} (chr_file (ioctl read write getattr map open)))
```

前四条是**上一轮**加的（保证 `IModule/primary` 能注册），本 issue 证明**只加这四条不够**。

### 4.2 服务名映射

`engine/vendor/hardware/android.hardware.audio/sepolicy/vendor_service_contexts`
（三条 `android.hardware.audio.core.*` → `hal_audio_service` 的前缀映射）+
`engine/build/07_build_vendor/sepolicy.go` 的 `sepolicyPatches` 增加
`vendor_service_contexts` 条目（否则片段不会被装配）。

### 4.3 构建

改的是 `/vendor` 内容，**必须重打 patch**（patch.img 含 `/vendor` 覆盖层，会盖掉 super 里的同路径）：

```
cd engine && ./out/bin/cdh device build gold --from 07_build_vendor --refresh
```

产物：`patch.img` sha256 `94caf245d410`（`vendor_sepolicy.cil` 222494 B，
装配树与 img 内大小一致，含 `hal_audio_default ashmem_device_202404`）。

> 注意：改 `engine/build/**.go` 后必须 `cd engine/build && go build -o ../out/bin/cdh .`
> 重编译，否则改动不会生效（本轮踩过一次：cdh 是旧二进制，片段没被装配）。

## 5. 验证

整机新建实例（`/api/stop` → `/api/delete` → `/api/create {"index":0}`，pid 80344，boot 70 s）：

| 指标 | 修复前 | 修复后 | 对照实例 1 |
| :--- | :--- | :--- | :--- |
| `dumpsys audio` 的 `Devices:` | 全空 | **`speaker(2)`** | `speaker(2)` |
| `Current:` 音量行 | `40000000 (default)` | **`2 (speaker): 8, 40000000 (default): 8`** | 同 |
| `Volume Group` | `n/a` | **`AUDIO_STREAM_VOICE_CALL`** | 同 |
| `listAudioPorts error` 计数 | 656（持续增） | **1** | 1 |
| `initStreamVolume failed` | 持续重试 | **0** | 0 |
| `Cannot open output stream` / `is unreachable` / `mAudioPolicyManager == NULL` | 反复出现 | **全部消失** | — |
| audio 相关 `permissive=0` avc | `hal_audio_default→ashmem` | **0 条** | — |
| 属性回归 `persist.system.powerhal.applist_enable` | — | **0** | — |
| 属性回归 `debug.sf.vsync_reactor_ignore_present_fences` | — | **true** | — |
| `Waiting for service` 计数 | 20+ | **1** | — |
| `IModule/primary` | 缺失 | 已注册（31/32/33） | 同 |

原始症状「`listAudioPorts error -19` 每 101 ms」从 656 次降到 1 次，音频链路回到与
gold-15 路线一致的状态。

## 6. 遗留

1. **`/dev/snd` 无 PCM 节点**：QEMU 当前只暴露 `/dev/snd/timer`，没有 `pcmC0D0p`，
   `AlsaPcm::open()` 失败后 HAL 回退纯内存 VirtualDac —— 音频链路通、但主机侧听不到声音。
   **对照实例 1（gold-15）同样只有 `timer`**，属 QEMU 声卡配置的共性问题，不是本 issue 的回归。
   `audio_device_{{vers}}` 授权已预先补上，声卡节点具备后可直接生效。
2. **`input keyevent` 全部失败**（`BR_FAILED_REPLY` / 客户端显示 `-28 ENOSPC`），
   与本 issue 无关，是独立问题。
3. **`cmd notification post` 失败**（`Failed transaction (2147483646)`），
   与 #2 可能同源（system_server 侧 `cmd`/`input` 通道的 binder 事务失败），
   而 `dumpsys` 通道正常 —— 待单独定位。
4. `dumpsys media.audio_policy` / `media.audio_flinger` 仍 `FAILED_TRANSACTION`，
   对照实例 1 相同，属环境通用现象，未列为缺陷。

## 排查方法沉淀

- **判据要用「system_server 视角」**：`dumpsys audio` 的 `Devices:` / `Volume Group`，
  而不是 `service list` 里的 `IModule/*` 是否存在。
- **「HAL 说成功、framework 收到 null」** = reply parcel 里的文件描述符/共享内存打包失败，
  优先查 `*_device`（ashmem）/`service_manager`/`chr_file` 这类**非 binder-对象**的 selinux 授权。
- `-2147483640` 就是 `STATUS_BAD_INDEX`，出现在 audio HAL 的 stream 打开路径上时，
  基本可以断定 `StreamDescriptor` 的 FMQ 描述符无效。
