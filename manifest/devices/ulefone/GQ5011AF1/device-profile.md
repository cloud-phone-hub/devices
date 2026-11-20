# 真机硬件规格与云机对齐留档 · Ulefone Armor 28 Ultra (ulefone/GQ5011AF1)

- **生成时间**: `2026-11-19T10:45:02+08:00`
- **ROM Build ID**: `14-GQ5011AF1_EEA_V17-20260429`
- **DSH 聊天会话 ID**: `measured-only`（标题：`[02_prepare] 真机规格采集 · Ulefone Armor 28 Ultra (ulefone/GQ5011AF1 · 14-GQ5011AF1_EEA_V17-20260429)`）

---

## 一、设备表对齐规格（对应 `devices/manifest/devices/ulefone/GQ5011AF1.json`）

| 字段 | 对齐值 | 说明 |
| --- | --- | --- |
| `official` | `Ulefone` | 厂商集团 |
| `brand` | `ulefone` | 子品牌标识 |
| `model` | `Ulefone Armor 28 Ultra` | 商业上市机型名  |
| `device` | `GQ5011AF1` | 设备代号 |
| `platform` / `board` | `mtk` / `mt6989` | 芯片平台 / 主板代号 |
| `resolution` | `1080x2400` | 屏幕物理像素分辨率 |
| `inch` | `70x155` | 屏幕可视物理宽×高 mm（对角线 `0.00"`） |
| `density` | `480` | Android 逻辑密度 dpi（物理 PPI `0`，xdpi `0.00` / ydpi `0.00`） |
| `smp` | `8` | CPU 核心总数 |
| `memory` | `[8192]` | 全系发售内存档位 MB（8G，-） |
| `storage` | `[128000 256000]` | 全系发售存储档位（128G / 256G，-） |

## 二、真机深层硬件对齐画像（参考 `redmi/gold` 标准）

### 1. SoC 与 CPU 簇全量频点表（`cdh_cpufreq` / `module/config.json` 对齐）
- **SoC 芯片**:   (`` , 0nm)
- **CPU 拓扑**: `-`（驱动: `-`，调速器: `-`，可选调速器: `[]`，BogoMIPS: `-`）

| 策略簇 | 核心列表 | 微架构 | Part / MIDR | 容量权重 | 最小 / 驻留 / 最大 (kHz) | 频点档数 |
| --- | --- | --- | --- | --- | --- | --- |
| policy0 | `[0 1 2 3]` |  | `-` (`-`) | `766` | `260000 / 286000 / 1300000` | `41 档` |
| policy4 | `[4 5 6]` |  | `-` (`-`) | `958` | `0 / 0 / 0` | `0 档` |
| policy7 | `[7]` |  | `-` (`-`) | `1024` | `0 / 0 / 0` | `0 档` |

- **`policy0` (``, cpus `[0 1 2 3]`) 全量 `available-frequencies` (41 档, kHz)**:
  `[1300000 1274000 1248000 1222000 1196000 1170000 1144000 1118000 1092000 1066000 1040000 1014000 988000 962000 936000 910000 884000 858000 832000 806000 780000 754000 728000 702000 676000 650000 624000 598000 572000 546000 520000 494000 468000 442000 416000 390000 364000 338000 312000 286000 260000]`
- **`policy4` (``, cpus `[4 5 6]`) 全量 `available-frequencies` (0 档, kHz)**:
  `[]`
- **`policy7` (``, cpus `[7]`) 全量 `available-frequencies` (0 档, kHz)**:
  `[]`

<details>
<summary>可直接用于 <code>module/config.json</code> 的 <code>cpufreq</code> 完整配置片段</summary>

```json
{
  "cpufreq": {
    "transition-latency": 1000,
    "scaling-readable": false,
    "package-per-cluster": false,
    "policies": [
      {
        "policy": 0,
        "cpus": [
          0,
          1,
          2,
          3
        ],
        "min-freq": 260000,
        "max-freq": 1300000,
        "cur-freq": 286000,
        "available-frequencies": [
          1300000,
          1274000,
          1248000,
          1222000,
          1196000,
          1170000,
          1144000,
          1118000,
          1092000,
          1066000,
          1040000,
          1014000,
          988000,
          962000,
          936000,
          910000,
          884000,
          858000,
          832000,
          806000,
          780000,
          754000,
          728000,
          702000,
          676000,
          650000,
          624000,
          598000,
          572000,
          546000,
          520000,
          494000,
          468000,
          442000,
          416000,
          390000,
          364000,
          338000,
          312000,
          286000,
          260000
        ],
        "implementer": "0x41",
        "variant": 0,
        "revision": 0,
        "capacity": 766,
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 4,
        "cpus": [
          4,
          5,
          6
        ],
        "min-freq": 0,
        "max-freq": 0,
        "cur-freq": 0,
        "available-frequencies": [],
        "implementer": "0x41",
        "variant": 0,
        "revision": 0,
        "capacity": 958,
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 7,
        "cpus": [
          7
        ],
        "min-freq": 0,
        "max-freq": 0,
        "cur-freq": 0,
        "available-frequencies": [],
        "implementer": "0x41",
        "variant": 0,
        "revision": 0,
        "capacity": 1024,
        "revidr": "0x0000000000000000"
      }
    ]
  }
}
```
</details>

### 2. 显示面板、GPU 与图形栈（`gpu_id.cfg` / `buildVendor` 对标 `gold`）
- **面板类型与刷新率**: - · 刷新率 `[] Hz`（默认 `0 Hz`） · 广色域 `false` · EDID `-` (`-`, product=`0`, year=`0`W`0`)
- **OpenGL ES**: `-` / `-` / `-`
- **Vulkan**: `ro.hardware.vulkan=-` · VendorID `-` · DeviceID `-` · API `-` (`-`) · DriverID `0` (`-`)

### 3. 触控、指纹、外设与输入节点对齐（`module/config.json` & `hardware/config.json` 对标 `gold`）
- **指纹方案**: -
- **触控与按键对齐**: touch=`-` · keyboard=`-` · input_identity 外设数=`0` 项
- **硬件 HAL 变体与开关**: radioVariant=`-` · NFC=`false` · 红外=`false` · 3.5mm=`false` · -
- **影像规格**: 后置 - | 前置 -
- **电池与通信**: `0 mAh`（有线 `0W` / 无线 `0W`） · -（Wi-Fi 6=`false`） · BT `-` · 5G=`false` · Widevine `-` (sysId=`-`) · TAC `-` · MAC OUI `[]`

### 4. `vendor/etc/*_config.xml` 虚拟 HAL AI 对齐清单（对标 `gold-android15/vendor/etc`）
- **`sensors_config.xml`**: 共 `0` 项对齐传感器
- **`battery_config.xml`**: `Li-poly` · 容量 `0 mAh` · 满电/充电电压 `0 / 0 mV`
- **`thermal_config.xml`**: 皮肤基温 `0.000 ℃` · 温区传感器 `0` 项 · Cooling=`-`
- **`gnss_config.xml`**: systemInfo=`-` · yearOfHw=`0` · L5=`false`
- **`vibrator_config.xml` & `light_config.xml`**: 马达 `-` (cap=`0`, effects=`[]`) · 逻辑灯 `0` 项
- **`audio_effects_descriptors.xml` & `media_config.xml`**: AI 音效描述符 `0` 项 · AI Codec2 实例 `0` 项 / 组件 `0` 项

## 三、官方固件包解包实测对账（`build.prop` + `boot.img` + `VINTF` + `DTB` + `fstab` + `vendor/etc`）

- `marketName`: `` | `modelCode`: `Armor 28 Ultra` | `manufacturer`: `Ulefone`
- `board`: `mt6989` | `soc`: `  ()`
- `densityDpi`: `480` | `egl`: `meow` | `vulkan`: `mali` | `abilist`: `arm64-v8a`
- `securityPatch`: `2026-03-05` | `buildDate`: `20260429` | `kernelLine`: `` | `vintfVersion`: `8.0`
- `fingerprint`: `Ulefone/GQ5011AF1_EEA/GQ5011AF1:15/AP3A.240905.015.A2/1777434795:user/release-keys`
- `vendor_boot.cmdline` (4 项): `androidboot.product.hardware.sku=GQ5011AF1_EEA`, `androidboot.secureboot=1`, `androidboot.slot=a`, `androidboot.veritymode.managed=yes`
- `thermal.zones` (62 个温区): `soc_max`, `cpu-little-core0`, `cpu-little-core1`, `cpu-little-core2`, `cpu-little-core3`, `cpu-medium-core4-0`, `cpu-medium-core4-1`, `cpu-medium-core5-0`, `cpu-medium-core5-1`, `cpu-medium-core6-0`, `cpu-medium-core6-1`, `cpu-big-core7-0`, `cpu-big-core7-1`, `cpu-dsu0`, `cpu-dsu1`, `cpu-dsu2`, `cpu-dsu3`, `apu-a0`, `apu-a1`, `apu-a2`, `apu-a3`, `apu-b0`, `gpu0`, `gpu1`, `gpu2`, `soc-top0`, `soc-top1`, `soc-top2`, `soc-top3`, `soc-bot0`, `soc-bot1`, `soc-bot2`, `soc-bot3`, `ap_ntc`, `ltepa_ntc`, `nrpa_ntc`, `tsx-ntc`, `quiet_ntc`, `quiet-mdpa-ntc`, `quiet-pmic-ntc-ntc`, `wifi-ntc`, `wifi-adie-0`, `wifi-ddie-0`, `wifi-ddie-1`, `wifi-ddie-2`, `wifi-ddie-3`, `ambient-ntc`, `pmic6363-vio18`, `pmic6363-vs1-vs3`, `pmic6363-bk3-bk7`, `pmic6363-vs2`, `pmic6373-bk0`, `pmic6373-ldo`, `pmic6373-bk3-bk7`, `pmic6373-bk4-bk8`, `vtskin-max`, `vtskin1`, `vtskin2`, `vtskin3`, `vtskin4`, `vtskin5`, `vtskin6`
- `vda.partitions` (5 个附加分区): `vendor_persist.img->/mnt/vendor/persist`, `protect_f.img->/mnt/vendor/protect_f`, `protect_s.img->/mnt/vendor/protect_s`, `nvdata.img->/mnt/vendor/nvdata`, `nvcfg.img->/mnt/vendor/nvcfg`
- `vintfPlaceholders`: radio=`34` 项 · audio=`1` 项 · gnss=`2` 项 · nn=`48` 项

### `vendor/etc` 与 `odm/etc` 硬件配置实测详情

- **Permissions 硬件特性**（共 `90` 项）: 红外=`true` · NFC=`true` · 指纹=`true` · BLE=`true` · Wi-Fi Direct=`true` · Audio LowLatency=`true` · Vulkan Level=`1` (API `1.3`)
  - 声明的传感器特性 (`9` 项): `accelerometer`, `barometer`, `compass`, `dynamic.head_tracker`, `gyroscope`, `light`, `proximity`, `stepcounter`, `stepdetector`
- **Sensors 原厂配置**: SubHALs=`[android.hardware.sensors@2.X-subhal-mediatek.so sensors.dynamic_sensor_hal.so]` · 芯片归属=`map[]`
- **Audio 策略与音效配置**: 模块=`[primary]` · 采样率=`[44100 48000]` · 听筒=`true` · 有线耳机路由=`true` · 双麦=`true` · 音效描述符=`16` 项（源: `vendor/etc/audio_effects.xml`）
  - 实测音效清单: `Acoustic Echo Canceler (The Android Open Source Project)`, `Automatic Gain Control (The Android Open Source Project)`, `Auxiliary Environmental Reverb (NXP Software Ltd.)`, `Auxiliary Preset Reverb (NXP Software Ltd.)`, `Dynamic Bass Boost (NXP Software Ltd.)`, `DynamicsProcessing (The Android Open Source Project)`, `EqualizerBundle (NXP Software Ltd.)`, `Insert Environmental Reverb (NXP Software Ltd.)`, `Insert Preset Reverb (NXP Software Ltd.)`, `Loudness Enhancer (The Android Open Source Project)`, `Multichannel Downmix To Stereo (The Android Open Source Project)`, `Noise Suppression (The Android Open Source Project)`, `Spatializer (The Android Open Source Project)`, `Virtualizer (NXP Software Ltd.)`, `Visualizer (The Android Open Source Project)`, `Volume (NXP Software Ltd.)`
- **Media Codec2 硬编解码配置**: Store实例=`[android.hardware.media.c2@1.2-mediatek-64b->default1]` · 厂商/杜比组件=`23` 项 (`c2.mtk.adpcm-ms.decoder`, `c2.mtk.adpcm-dvi-ima.decoder`, `c2.mtk.alac.decoder`, `c2.mtk.wma.decoder`, `c2.mtk.mp3.decoder`, `c2.mtk.avc.decoder`, `c2.mtk.avc.decoder.secure`, `c2.mtk.avc.decoder.lowlatency`, `c2.mtk.hevc.decoder`, `c2.mtk.hevc.decoder.secure`, `c2.mtk.hevc.decoder.lowlatency`, `c2.mtk.heif.decoder`, `c2.mtk.vp9.decoder`, `c2.mtk.vp9.decoder.secure`, `c2.mtk.vp9.decoder.lowlatency`, `c2.mtk.av1.decoder`, `c2.mtk.av1.decoder.secure`, `c2.mtk.av1.decoder.lowlatency`, `c2.mtk.avc.encoder`, `c2.mtk.avc.encoder.secure`, `c2.mtk.hevc.encoder`, `c2.mtk.hevc.encoder.secure`, `c2.mtk.heif.encoder`)
- **GNSS 定位配置**: 源文件=`vendor/etc/MNL_Config.xml` · 版本=`21111601.6.00.00` · 模式=`10 (GPS+GLONASS+Galileo+BeiDou+NavIC)` · L1Only=`false` · L5配置=`true`
- **Display 面板与 GPU 平台配置**: DSI面板型号=`[]` · screen_light.xml=`false` · Mali参数=`map[OSU_PROTECTED_MEMORY_HEAP_NAME:mtk_svp_page-uncached PLATFORM_AGT_FREQUENCY_KHZ:13000]`
- **ODM SKU / NFC / Wi-Fi / 影像场景**: NFC芯片=`STMicroelectronics ST21NFC` · RSC区域=`[]` · 相机Tuning角色=`[]`
- **Power 调频/核数控制表**: 各簇最大核数=`[4 3 1]` · 配置文件=`[vendor/etc/power_app_cfg.xml vendor/etc/powercontable.xml vendor/etc/powerscntbl.xml vendor/etc/task_profiles.json]`
- **已扫描解析的 `vendor/etc` & `odm/etc` 配置文件**: 共 `75` 个

## 四、DSH 聊天界面回复原文

CDH_PREPARE_MEASURED_ONLY=1
