# vendor/etc 机型 config 与官方口径核对

- 机型：samsung / Galaxy S25 Ultra（SM-S9380，`ss9380`，国行 CHC）
- 官方对照件：`out/roms/samsung/ss9380/S9380ZCSDCZH1/super/vendor_a.img`（EROFS，
  `dump.erofs --ls/--cat`）
- 时间：2026-10-31

## 1. 问题

`devices/manifest/samsung/ss9380/vendor/etc` 下 9 份 config 一直没人跟官方 `/vendor/etc`
逐份核过，此前只是按需挑了几份 audio_effects 拷过来。问题是：这 9 份里既有官方件、
有官方件的改名副本、也有云机自编件，混在一起看不出哪些能改、哪些不能动；唯一真正
“没对上”的 `excluded-input-devices.xml` 还是手写版（只排 ACPI 电源键）。

## 2. 根因 / 分类

逐份 md5 + `dump.erofs` 抽取后，9 份可分三类（明细见 `log-fail.txt`）：

| 类别 | 文件 | 结论 |
| --- | --- | --- |
| 与官方**逐字节一致**（5 份） | `audio_effects.xml`、`audio_effects_config_sec.xml`、`audio_effects_haptic.xml`、`audio_effects_sec.xml`、`audio_effects_spatializer.xml` | 不需要动 |
| 官方件的**改名别名**（1 份） | `audio_effects_config.xml`（= `audio_effects.xml`，md5 均 `17f7efa4…`） | 必须留：自研 audio HAL 出自 AOSP `audio/aidl/default`，`EffectMain.cpp:29` 的默认名就是 `audio_effects_config.xml`；三星自己的 HAL 读 `audio_effects.xml` |
| 官方**没有同名件**、云机自编（2 份） | `media_config.xml`（c2 store 实例登记）、`ueventd.qcom.rc`（`/dev/hvc*` 权限） | 不能用官方件替换（官方是 `media_codecs_sun*.xml` / `ueventd.rc`） |
| 官方有同名件但**内容不同**（1 份） | `excluded-input-devices.xml` | 唯一真正“没对齐”的 |

`excluded-input-devices.xml` 的差异：官方排 `qti-haptics`、`qcom-hv-haptics`（真机触感
输入设备），本地那版排 `Power Button`（手写，行数从 30 行缩到 13 行）。

## 3. 证据

- 官方 `/vendor/etc` 共 182 个文件，音频/媒体相关 39 份（`log-fail.txt` 第 [1] 段）；
- md5 对照矩阵：5 同 1 异（第 [2] 段）；
- 官方抽取报错证明三份“官方没有”（第 [3] 段）：
  `<E> erofs: read inode failed @ /etc/audio_effects_config.xml`；
- 别名关系的证据：两份 md5 相同 + `EffectMain.cpp:29 kDefaultConfigName`（第 [4] 段）；
- 本地手写版的完整 diff（第 [5] 段）；
- 两份自编件在设备上确实生效（第 [7] 段）：`/dev/hvc0` 是 `0666 system:system`，
  不是 devtmpfs 默认的 `0600 root:root`。

## 4. 改动

1. `excluded-input-devices.xml`：换成官方 2216 B 整文件（md5 `325cb325…`）。
   依据：本 VM 的 InputReader 设备表里根本没有 `Power Button`（旧盘集，`log-fail.txt`
   第 [6] 段），说明那条排除项一直空转；官方排的两项在云机上也不存在，纯对齐无副作用。
2. 给三份“容易被误删/误换”的文件补文件头注释（内容不改）：
   - `audio_effects_config.xml`：写明是官方 `audio_effects.xml` 的逐字节副本及原因；
   - `media_config.xml`：写明官方无同名件、codec 二进制不在本 vendor、不可替换；
   - `ueventd.qcom.rc`：写明官方是 `ueventd.rc`、本文件只管 hvc* 权限、已实测生效。
3. 5 份与官方一致的 `audio_effects*.xml` 未改动。

## 5. 验证

- `xmllint --noout` 三份改过的 XML 全通过；
- 整机 `cdh device build ss9380` → `[ok] 已完成`，`机型静态树: 写入 12`，
  `patch.img` sha256 `5f81a5ecf385…`（`log-align-build.txt`）；
- 实例 `index 4`（全新 `/data`，adb `127.0.0.1:5911`，`log-pass.txt`）：
  - `sys.boot_completed=1`、`init.svc.zygote=running`、crash 里 `BatteryService` 计数 0；
  - 设备侧四份文件尺寸/md5 与源一致：
    `excluded-input-devices.xml` 2216 B（md5 `325cb3252e59fd72cebbc74af7365738`）、
    `audio_effects_config.xml` 2331 B、`media_config.xml` 3327 B、`ueventd.qcom.rc` 1985 B；
  - 输入：`sec_touchscreen` 仍在设备表，键鼠照旧；
  - `/dev/hvc0` `0666 system:system`、`/dev/hvc3` `0666 system:system`（KeyMint 通道权限没退化）；
  - `audioserver` running（pid 746），AudioFlinger 正常打日志。

## 6. 遗留

- **副作用**：换成官方排除表后，ACPI `Power Button` 不再被排除，`dumpsys input` 里多出
  `Device 4: Power Button`（旧盘集没有这一项）。它只是一台 KEYBOARD 类设备，不影响触摸与
  键鼠；若要藏回去，在官方件里补一行 `<device name="Power Button"/>` 即可（会与官方不再逐字节
  一致，按需取舍）。
- 官方 etc 里还有约 30 份音频/媒体 config 没搬（`audio_policy_configuration_base.xml`、
  `audio_policy_volumes.xml`、`media_codecs_sun*.xml`、`media_profiles_sun.xml`、
  `sec_audio_volume_curve.xml`、`audio/`、`media_sun/`…）。本轮按“只修真有差异的”处理，
  没动；官方 codec 二进制不在本 vendor（lineage 基座 + 自编 HAL），要按官方口径做媒体
  得先解决 codec 来源，另立方案。
- mediaserver 仍会重试 `aidl/android.hardware.media.c2.IComponentStore/software`
  （`PROP_ERROR_HANDLE_CONTROL_MESSAGE`），是既有的“假 c2 登记”遗留，与本轮改动无关。
