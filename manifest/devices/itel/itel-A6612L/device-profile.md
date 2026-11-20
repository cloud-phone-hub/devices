# 真机硬件规格与云机对齐留档 · itel A6612L (itel/itel-A6612L)

- **生成时间**: `2026-11-19T11:32:57+08:00`
- **ROM Build ID**: `13-BP2A.250605.031.A3-20260823`
- **DSH 聊天会话 ID**: `measured-only`（标题：`[02_prepare] 真机规格采集 · itel A6612L (itel/itel-A6612L · 13-BP2A.250605.031.A3-20260823)`）

---

## 一、设备表对齐规格（对应 `devices/manifest/devices/itel/itel-A6612L.json`）

| 字段 | 对齐值 | 说明 |
| --- | --- | --- |
| `official` | `传音` | 厂商集团 |
| `brand` | `itel` | 子品牌标识 |
| `model` | `itel A6612L` | 商业上市机型名  |
| `device` | `itel-A6612L` | 设备代号 |
| `platform` / `board` | `unisoc` / `sp9863a` | 芯片平台 / 主板代号 |
| `resolution` | `1080x2400` | 屏幕物理像素分辨率 |
| `inch` | `70x155` | 屏幕可视物理宽×高 mm（对角线 `0.00"`） |
| `density` | `320` | Android 逻辑密度 dpi（物理 PPI `0`，xdpi `0.00` / ydpi `0.00`） |
| `smp` | `8` | CPU 核心总数 |
| `memory` | `[8192]` | 全系发售内存档位 MB（8G，-） |
| `storage` | `[128000 256000]` | 全系发售存储档位（128G / 256G，-） |

## 二、真机深层硬件对齐画像（参考 `redmi/gold` 标准）

### 1. SoC 与 CPU 簇全量频点表（`cdh_cpufreq` / `module/config.json` 对齐）
- **SoC 芯片**:   (`` , 0nm)
- **CPU 拓扑**: `-`（驱动: `-`，调速器: `-`，可选调速器: `[]`，BogoMIPS: `-`）

| 策略簇 | 核心列表 | 微架构 | Part / MIDR | 容量权重 | 最小 / 驻留 / 最大 (kHz) | 频点档数 |
| --- | --- | --- | --- | --- | --- | --- |
| policy0 | `[0 1 2 3 4 5 6 7]` | arm,cortex-a55 | `-` (`-`) | `1024` | `0 / 0 / 0` | `0 档` |

- **`policy0` (`arm,cortex-a55`, cpus `[0 1 2 3 4 5 6 7]`) 全量 `available-frequencies` (0 档, kHz)**:
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
          3,
          4,
          5,
          6,
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

- `marketName`: `` | `modelCode`: `itel A6612L` | `manufacturer`: `ITEL`
- `board`: `sp9863a` | `soc`: `Spreadtrum  ()`
- `densityDpi`: `320` | `egl`: `` | `vulkan`: `` | `abilist`: `arm64-v8a,armeabi-v7a,armeabi`
- `securityPatch`: `2026-04-05` | `buildDate`: `20260823` | `kernelLine`: `5.15` | `vintfVersion`: `7.0`
- `uname`: `Linux localhost 5.15.197-android13-8-00005-g2d8ad9139b89-ab15063902 #1 SMP PREEMPT Fri Mar 20 11:14:06 UTC 2026 aarch64 Toybox`
- `fingerprint`: `itel/A6612L-OP64/itel-A6612L:16/BP2A.250605.031.A3/A6612L_260822V4:user/release-keys`
- `vendor_boot.cmdline` (4 项): `androidboot.product.hardware.sku=A6612L-OP64`, `androidboot.secureboot=1`, `androidboot.slot=a`, `androidboot.veritymode.managed=yes`
- `thermal.zones` (5 个温区): `gpu-thmzone`, `cpu-thmzone`, `board-thmzone`, `pa-thmzone`, `chg-thmzone`
- `vintfPlaceholders`: radio=`15` 项 · audio=`1` 项 · gnss=`1` 项 · nn=`12` 项

### `vendor/etc` 与 `odm/etc` 硬件配置实测详情

- **Permissions 硬件特性**（共 `71` 项）: 红外=`true` · NFC=`false` · 指纹=`true` · BLE=`true` · Wi-Fi Direct=`true` · Audio LowLatency=`false` · Vulkan Level=`0` (API `1.3`)
  - 声明的传感器特性 (`7` 项): `accelerometer`, `compass`, `dynamic.head_tracker`, `gyroscope`, `proximity`, `stepcounter`, `stepdetector`
- **Sensors 原厂配置**: SubHALs=`[android.hardware.sensors@2.X-sprd_subhal.so sensors.dynamic_sensor_subhal.so android.hardware.sensors@2.X-virtual_light_subhal.so]` · 芯片归属=`map[]`
- **Audio 策略与音效配置**: 模块=`[]` · 采样率=`[]` · 听筒=`false` · 有线耳机路由=`false` · 双麦=`false` · 音效描述符=`14` 项（源: `vendor/etc/audio_effects.xml`）
  - 实测音效清单: `Acoustic Echo Canceler (The Android Open Source Project)`, `Auxiliary Environmental Reverb (NXP Software Ltd.)`, `Auxiliary Preset Reverb (NXP Software Ltd.)`, `Dynamic Bass Boost (NXP Software Ltd.)`, `DynamicsProcessing (The Android Open Source Project)`, `EqualizerBundle (NXP Software Ltd.)`, `Insert Environmental Reverb (NXP Software Ltd.)`, `Insert Preset Reverb (NXP Software Ltd.)`, `Loudness Enhancer (The Android Open Source Project)`, `Multichannel Downmix To Stereo (The Android Open Source Project)`, `Virtualizer (NXP Software Ltd.)`, `Visualizer (The Android Open Source Project)`, `Volume (NXP Software Ltd.)`, `dtsaudio (OEM Audio Effect)`
- **Media Codec2 硬编解码配置**: Store实例=`[android.hardware.media.c2@1.1-unisoc-service->default]` · 厂商/杜比组件=`15` 项 (`c2.unisoc.imaadpcm.decoder`, `c2.unisoc.mp3.decoder`, `c2.unisoc.mpl1.decoder`, `c2.unisoc.mpl2.decoder`, `c2.unisoc.avc.decoder`, `c2.unisoc.hevc.decoder`, `c2.unisoc.mpeg4.decoder`, `c2.unisoc.h263.decoder`, `c2.unisoc.vp8.decoder`, `c2.unisoc.vp9.decoder`, `c2.unisoc.jpeg.decoder`, `c2.unisoc.mp3.encoder`, `c2.unisoc.jpeg.encoder`, `c2.unisoc.avc.encoder`, `c2.unisoc.hevc.encoder`)
- **ODM SKU / NFC / Wi-Fi / 影像场景**: NFC芯片=`-` · RSC区域=`[]` · 相机Tuning角色=`[]`
- **Power 调频/核数控制表**: 各簇最大核数=`[]` · 配置文件=`[vendor/etc/task_profiles.json]`
- **已扫描解析的 `vendor/etc` & `odm/etc` 配置文件**: 共 `65` 个

## 四、DSH 聊天界面回复原文

CDH_PREPARE_MEASURED_ONLY=1
