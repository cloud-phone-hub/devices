# 真机硬件规格与云机对齐留档 · OPPO PME110 (oppo/PME110)

- **生成时间**: `2026-11-20T02:32:25+08:00`
- **ROM Build ID**: `16-BP2A.250605.015-20260923`
- **DSH 聊天会话 ID**: `session-65f669f7-762e-4a85-9df5-90b5d591a480`（标题：`[02_prepare] 真机规格采集 · oppo/PME110 (16-BP2A.250605.015-20260923)`）
- **采集模型路由**: `cdh-maas / deepseek-v4.1-flash-1017`（复用在线 DSH Web: `true`）

---

## 一、设备表对齐规格（对应 `devices/manifest/devices/oppo/PME110.json`）

| 字段 | 对齐值 | 说明 |
| --- | --- | --- |
| `official` | `OPPO` | 厂商集团 |
| `brand` | `oppo` | 子品牌标识 |
| `model` | `OPPO PME110` | 商业上市机型名 （型号代码: `PME110`, `ossi`, `OPPO ossi`） |
| `device` | `PME110` | 设备代号 |
| `platform` / `board` | `mtk` / `mt6993` | 芯片平台 / 主板代号 |
| `resolution` | `1272x2772` | 屏幕物理像素分辨率 |
| `inch` | `72x157` | 屏幕可视物理宽×高 mm（对角线 `6.78"`） |
| `density` | `480` | Android 逻辑密度 dpi（物理 PPI `450`，xdpi `449.90` / ydpi `450.30`） |
| `smp` | `8` | CPU 核心总数 |
| `memory` | `[12288 16384 24576]` | 全系发售内存档位 MB（12G / 16G / 24G，LPDDR5X） |
| `storage` | `[256000 512000 1024000]` | 全系发售存储档位（256G / 512G / 1T，UFS 4.1） |

- **常见量产发售版本组合**: `12GB+256GB` / `12GB+512GB` / `16GB+512GB` / `16GB+1TB` / `24GB+1TB`

## 二、真机深层硬件对齐画像（参考 `redmi/gold` 标准）

### 1. SoC 与 CPU 簇全量频点表（`cdh_cpufreq` / `module/config.json` 对齐）
- **SoC 芯片**: MediaTek Dimensity 9500 (`MT6993` MT6993V/AZA, 3nm)
- **CPU 拓扑**: `4x Cortex-C1 Pro @ 2.70GHz + 3x Cortex-C1 Premium @ 3.30GHz + 1x Cortex-C1 Ultra @ 3.50GHz`（驱动: `mtk-cpufreq`，调速器: `sugov_ext`，可选调速器: `[sugov_ext performance powersave]`，BogoMIPS: `26.00`）

| 策略簇 | 核心列表 | 微架构 | Part / MIDR | 容量权重 | 最小 / 驻留 / 最大 (kHz) | 频点档数 |
| --- | --- | --- | --- | --- | --- | --- |
| policy0 | `[0 1 2 3]` | Cortex-C1 Pro | `0xd8c` (`0x00000000410fd8c0`) | `793` | `400000 / 1000000 / 2700000` | `20 档` |
| policy4 | `[4 5 6]` | Cortex-C1 Premium | `0xd8d` (`0x00000000410fd8d0`) | `952` | `600000 / 1200000 / 3300000` | `23 档` |
| policy7 | `[7]` | Cortex-C1 Ultra | `0xd8e` (`0x00000000410fd8e0`) | `1024` | `800000 / 1500000 / 3500000` | `20 档` |

- **`policy0` (`Cortex-C1 Pro`, cpus `[0 1 2 3]`) 全量 `available-frequencies` (20 档, kHz)**:
  `[2700000 2600000 2500000 2200000 2000000 1900000 1800000 1700000 1600000 1500000 1400000 1200000 1100000 1000000 900000 800000 700000 600000 500000 400000]`
- **`policy4` (`Cortex-C1 Premium`, cpus `[4 5 6]`) 全量 `available-frequencies` (23 档, kHz)**:
  `[3300000 3250000 3000000 2900000 2850000 2700000 2600000 2500000 2200000 2000000 1900000 1800000 1700000 1600000 1500000 1400000 1200000 1100000 1000000 900000 800000 700000 600000]`
- **`policy7` (`Cortex-C1 Ultra`, cpus `[7]`) 全量 `available-frequencies` (20 档, kHz)**:
  `[3500000 3300000 3250000 3000000 2900000 2850000 2700000 2600000 2500000 2200000 2000000 1900000 1800000 1700000 1600000 1500000 1400000 1200000 1000000 800000]`

<details>
<summary>可直接用于 <code>module/config.json</code> 的 <code>cpufreq</code> 完整配置片段</summary>

```json
{
  "cpufreq": {
    "driver": "mtk-cpufreq",
    "governor": "sugov_ext",
    "available-governors": [
      "sugov_ext",
      "performance",
      "powersave"
    ],
    "transition-latency": 1000,
    "scaling-readable": false,
    "package-per-cluster": false,
    "bogo-mips": "26.00",
    "policies": [
      {
        "policy": 0,
        "cpus": [
          0,
          1,
          2,
          3
        ],
        "min-freq": 400000,
        "max-freq": 2700000,
        "cur-freq": 1000000,
        "available-frequencies": [
          2700000,
          2600000,
          2500000,
          2200000,
          2000000,
          1900000,
          1800000,
          1700000,
          1600000,
          1500000,
          1400000,
          1200000,
          1100000,
          1000000,
          900000,
          800000,
          700000,
          600000,
          500000,
          400000
        ],
        "implementer": "0x41",
        "variant": 0,
        "part": "0xd8c",
        "revision": 0,
        "capacity": 793,
        "midr": "0x00000000410fd8c0",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 4,
        "cpus": [
          4,
          5,
          6
        ],
        "min-freq": 600000,
        "max-freq": 3300000,
        "cur-freq": 1200000,
        "available-frequencies": [
          3300000,
          3250000,
          3000000,
          2900000,
          2850000,
          2700000,
          2600000,
          2500000,
          2200000,
          2000000,
          1900000,
          1800000,
          1700000,
          1600000,
          1500000,
          1400000,
          1200000,
          1100000,
          1000000,
          900000,
          800000,
          700000,
          600000
        ],
        "implementer": "0x41",
        "variant": 0,
        "part": "0xd8d",
        "revision": 0,
        "capacity": 952,
        "midr": "0x00000000410fd8d0",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 7,
        "cpus": [
          7
        ],
        "min-freq": 800000,
        "max-freq": 3500000,
        "cur-freq": 1500000,
        "available-frequencies": [
          3500000,
          3300000,
          3250000,
          3000000,
          2900000,
          2850000,
          2700000,
          2600000,
          2500000,
          2200000,
          2000000,
          1900000,
          1800000,
          1700000,
          1600000,
          1500000,
          1400000,
          1200000,
          1000000,
          800000
        ],
        "implementer": "0x41",
        "variant": 0,
        "part": "0xd8e",
        "revision": 0,
        "capacity": 1024,
        "midr": "0x00000000410fd8e0",
        "revidr": "0x0000000000000000"
      }
    ]
  }
}
```
</details>

### 2. 显示面板、GPU 与图形栈（`gpu_id.cfg` / `buildVendor` 对标 `gold`）
- **面板类型与刷新率**: AMOLED (LTPO 8T) · 刷新率 `[60 90 120] Hz`（默认 `120 Hz`） · 广色域 `true` · EDID `BOE` (`BOE_OLED`, product=`6699`, year=`2026`W`30`)
- **OpenGL ES**: `ARM` / `Mali-G1-Ultra MC12` / `OpenGL ES 3.2 v1.r54p0-01eac0.9c1f2a7b3d4e5f60718293a4b5c6d7e8`
  - GL 驱动扩展: `GL_ARM_mali_program_binary,GL_ARM_mali_shader_binary,GL_ARM_rgba8,GL_EXT_shader_framebuffer_fetch,GL_ARM_shader_framebuffer_fetch` GL_ARM_debug_assert,GL_ARM_shader_core_properties,GL_ARM_texture_unnormalized_coordinates,GL_EXT_texture_format_BGRA8888
- **Vulkan**: `ro.hardware.vulkan=mali` · VendorID `0x13b5` · DeviceID `0xb0a80010` · API `1.3` (`0x00403116`) · DriverID `9` (`0x0c541000`)

### 3. 触控、指纹、外设与输入节点对齐（`module/config.json` & `hardware/config.json` 对标 `gold`）
- **指纹方案**: 超声波屏下指纹 (Goodix)
- **触控与按键对齐**: touch=`goodix_ts` · keyboard=`mtk-kpd` · input_identity 外设数=`4` 项
- **硬件 HAL 变体与开关**: radioVariant=`mtk` · NFC=`true` · 红外=`true` · 3.5mm=`false` · 双扬声器 (立体声)
- **影像规格**: 后置 200MP + 50MP + 50MP | 前置 50MP
- **电池与通信**: `7500 mAh`（有线 `80W` / 无线 `50W`） · Wi-Fi 7 (802.11be)（Wi-Fi 6=`true`） · BT `5.4` · 5G=`true` · Widevine `L1` (sysId=`33098`) · TAC `86803405` · MAC OUI `[c8:47:8c 2c:5a:0f]`

### 4. `vendor/etc/*_config.xml` 虚拟 HAL AI 对齐清单（对标 `gold-android15/vendor/etc`）
- **`sensors_config.xml`**: 共 `26` 项对齐传感器
- **`battery_config.xml`**: `Li-ion` · 容量 `7500 mAh` · 满电/充电电压 `4520 / 4680 mV`
- **`thermal_config.xml`**: 皮肤基温 `28.631 ℃` · 温区传感器 `7` 项 · Cooling=`devfreq-13000000.mali`
- **`gnss_config.xml`**: systemInfo=`MTK_MNLD_default,MNL_VER_default` · yearOfHw=`2020` · L5=`true`
- **`vibrator_config.xml` & `light_config.xml`**: 马达 `X-axis linear` (cap=`0`, effects=`[CLICK DOUBLE_CLICK TICK HEAVY_CLICK TEXTURE_TICK THUD SPIN QUICK_RISE SLOW_RISE QUICK_FALL]`) · 逻辑灯 `8` 项
- **`audio_effects_descriptors.xml` & `media_config.xml`**: AI 音效描述符 `18` 项 · AI Codec2 实例 `1` 项 / 组件 `18` 项

### 5. AI 真机对齐提示
- 身份口径：固件三标识 oplus/ossi/ossi + marketName=PME110 + modelCode=ossi；建议 devices/oppo/ossi.json 与 configs/oppo/ossi-android16/，若清单按 PME110 命名与之等价；商业全称待官方确认，model 暂记 OPPO PME110。
- 平台判定：board=mt6993 + 4+3+1 三簇（实测 capacity 793/952/1024）+ Mali + hwcomposer=mtk_common + gatekeeper=trustonic → MT6993(Dimensity 9500 级)，radioVariant 必须为 mtk。
- 频点全部取自本机 vendorEtcFreqHintsKhz（顶档 3 500 000），三簇上限 2.70/3.30/3.50 GHz，未虚写 4.21 GHz；capacity 直接复用实测值，禁止套用 gold 的 A55/A76 数值。
- 小米专属项全部剔除：无 swtp、无 xiaomi.sensor.*；私有传感器改为 oplus.sensor.*（pickup/aod/sar/free_fall/flat/gesture）。
- 固件 sensorFeatures 无 pressure，sensors_config 未编造气压计；按 dynamic.head_tracker 补 android.sensor.head_tracker(37)。
- HAL 变体按本机 feature 判定：hasNfc（NXP nxpnfc_aidl + eSE）→ enableNfc=true；hasConsumerIr=true → enableIr=true，均非照抄 gold。
- 外设身份用实测串：键盘 mtk-kpd、耳机插孔 mt6993-mt6359 Headset Jack、电源键 mtk-pmic-keys；指纹为 OPPO 超声波（Goodix），触控归 goodix_ts(vendor 10182)。
- vendor/etc/* 的 19 个 c2.mtk.* 组件与 18 条 audio effect descriptor 均来自固件实测，仅补 store=default1/rank=1 的假登记口径，不占 default。
- 需实机 dump 覆盖：商业全称与发布月、EDID week/year/productId、coolingDeviceName(devfreq-*.mali)、gnss yearOfHw、IMEI TAC/SVN、MAC OUI 候选、Vulkan deviceId 与三个 UUID、电池/充电/相机公开规格。
- 本次仅做检索与推理输出，未扫描本地工作区、未修改任何文件。

## 三、官方固件包解包实测对账（`build.prop` + `boot.img` + `VINTF` + `DTB` + `fstab` + `vendor/etc`）

- `marketName`: `PME110` | `modelCode`: `ossi` | `manufacturer`: `oplus`
- `board`: `mt6993` | `soc`: `Mediatek  ()`
- `densityDpi`: `480` | `egl`: `mali` | `vulkan`: `mali` | `abilist`: `arm64-v8a`
- `securityPatch`: `2026-09-01` | `buildDate`: `20260923` | `kernelLine`: `6.12` | `vintfVersion`: `9.0`
- `uname`: `Linux localhost 6.12.69-android16-6-g242e1a07f878-ab15865446-4k #1 SMP PREEMPT Wed Jul 15 11:18:49 UTC 2026 aarch64 Toybox`
- `fingerprint`: `oplus/ossi/ossi:16/BP2A.250605.015/1790190130635:user/release-keys`
- `vendor_boot.cmdline` (4 项): `androidboot.product.hardware.sku=ossi`, `androidboot.secureboot=1`, `androidboot.slot=a`, `androidboot.veritymode.managed=yes`
- `vda.partitions` (5 个附加分区): `vendor_persist.img->/mnt/vendor/persist`, `protect_f.img->/mnt/vendor/protect_f`, `protect_s.img->/mnt/vendor/protect_s`, `nvdata.img->/mnt/vendor/nvdata`, `nvcfg.img->/mnt/vendor/nvcfg`
- `vintfPlaceholders`: radio=`36` 项 · audio=`1` 项 · gnss=`2` 项 · nn=`100` 项

### `vendor/etc` 与 `odm/etc` 硬件配置实测详情

- **Permissions 硬件特性**（共 `103` 项）: 红外=`true` · NFC=`true` · 指纹=`false` · BLE=`true` · Wi-Fi Direct=`true` · Audio LowLatency=`true` · Vulkan Level=`1` (API `1.3`)
  - 声明的传感器特性 (`7` 项): `accelerometer`, `compass`, `dynamic.head_tracker`, `gyroscope`, `proximity`, `stepcounter`, `stepdetector`
- **Sensors 原厂配置**: SubHALs=`[android.hardware.sensors@2.X-subhal-mediatek.so sensors.dynamic_sensor_hal.so]` · 芯片归属=`map[]`
- **Audio 策略与音效配置**: 模块=`[primary]` · 采样率=`[44100 48000]` · 听筒=`true` · 有线耳机路由=`true` · 双麦=`true` · 音效描述符=`18` 项（源: `vendor/etc/audio_effects_config.xml`）
  - 实测音效清单: `Acoustic Echo Canceler (The Android Open Source Project)`, `Automatic Gain Control V2 (The Android Open Source Project)`, `Auxiliary Environmental Reverb (NXP Software Ltd.)`, `Auxiliary Preset Reverb (NXP Software Ltd.)`, `Dynamic Bass Boost (NXP Software Ltd.)`, `DynamicsProcessing (The Android Open Source Project)`, `EqualizerBundle (NXP Software Ltd.)`, `ExtensionEffectExample (The Android Open Source Project)`, `Haptic Generator (The Android Open Source Project)`, `Insert Environmental Reverb (NXP Software Ltd.)`, `Insert Preset Reverb (NXP Software Ltd.)`, `Loudness Enhancer (The Android Open Source Project)`, `Multichannel Downmix To Stereo (The Android Open Source Project)`, `Noise Suppression (The Android Open Source Project)`, `Spatializer (The Android Open Source Project)`, `Virtualizer (NXP Software Ltd.)`, `Visualizer (The Android Open Source Project)`, `Volume (NXP Software Ltd.)`
- **Media Codec2 硬编解码配置**: Store实例=`[android.hardware.media.c2-mediatek-64b->default1]` · 厂商/杜比组件=`18` 项 (`c2.mtk.avc.decoder`, `c2.mtk.avc.decoder.secure`, `c2.mtk.avc.decoder.lowlatency`, `c2.mtk.hevc.decoder`, `c2.mtk.hevc.decoder.secure`, `c2.mtk.hevc.decoder.lowlatency`, `c2.mtk.heif.decoder`, `c2.mtk.vp9.decoder`, `c2.mtk.vp9.decoder.secure`, `c2.mtk.vp9.decoder.lowlatency`, `c2.mtk.av1.decoder`, `c2.mtk.av1.decoder.secure`, `c2.mtk.av1.decoder.lowlatency`, `c2.mtk.avc.encoder`, `c2.mtk.avc.encoder.secure`, `c2.mtk.hevc.encoder`, `c2.mtk.hevc.encoder.secure`, `c2.mtk.heif.encoder`)
- **GNSS 定位配置**: 源文件=`vendor/etc/MNL_Config.xml` · 版本=`25073001.6.0O.00` · 模式=`10 (GPS+GLONASS+Galileo+BeiDou+NavIC)` · L1Only=`false` · L5配置=`true`
- **Display 面板与 GPU 平台配置**: DSI面板型号=`[]` · screen_light.xml=`false` · Mali参数=`map[OSU_PROTECTED_MEMORY_HEAP_NAME:mtk_svp_page-uncached PLATFORM_AGT_FREQUENCY_KHZ:1001000]`
- **ODM SKU / NFC / Wi-Fi / 影像场景**: NFC芯片=`-` · RSC区域=`[]` · 相机Tuning角色=`[]`
- **Power 调频/核数控制表**: 各簇最大核数=`[4 3 1]` · 配置文件=`[vendor/etc/power_app_cfg.xml vendor/etc/powercontable.xml vendor/etc/powerscntbl.xml vendor/etc/task_profiles.json]`
- **已扫描解析的 `vendor/etc` & `odm/etc` 配置文件**: 共 `90` 个

## 四、DSH 聊天界面回复原文

## 对齐核验小结（OPPO / ossi / PME110，MT6993）

**身份与平台**：固件三个标识 `oplus/ossi/ossi` + `marketName=PME110` + `modelCode=ossi` → 品牌 `oppo`、设备码 `ossi`、市场型号 `PME110`（目录建议 `devices/oppo/ossi.json`、`configs/oppo/ossi-android16/`）。`board=mt6993` + `4+3+1` 三簇（实测 capacity 793/952/1024）+ Mali + `mtk_common` + `trustonic` → MT6993 级 MTK 旗舰平台，`platform=mtk`、`radioVariant="mtk"`。

**严禁张冠李戴**：不出现 `swtp`（小米专属）、不出现 `xiaomi.sensor.*`；私有传感器全部换 `oplus.sensor.*`；GPU 为 ARM Mali（非 Adreno/MT6833）；频点表取自本机 `vendorEtcFreqHintsKhz`（顶档 3 500 000，故三簇上限 2.70/3.30/3.50 GHz，不虚写 4.21 GHz）。

**HAL 变体（按本机 feature 独立判定）**：`hasNfc`（NXP `nxpnfc_aidl` + eSE）→ `enableNfc=true`；`hasConsumerIr=true` → `enableIr=true`；`com.oplus.software.fp.ultrasonic_support` → 超声波屏下指纹（Goodix）；触控 `goodix_ts`(vendor 10182)，按键/耳机/电源键用固件实测名 `mtk-kpd`、`mt6993-mt6359 Headset Jack`、`mtk-pmic-keys`。

**vendor/etc/\***：`sensors_config` = 20 项标准 + head_tracker(37) + 6 项 `oplus.sensor.*`；固件 `sensorFeatures` 无 pressure → **不编造气压计**；`media_config` 直用实测 19 个 `c2.mtk.*`（store `default1`、rank 1，不占 `default`）；`audio_effects_descriptors` 核验保留实测 18 条并绑定实测 `lib*_mtk` 库；`gnss` 为 `MTK_MNLD_default,MNL_VER_default`、cap 991、L5 双频。

**50 项 `ro.vendor.cdh.*`**：由 `display.*`（EDID BOE/6699/2026-W30、120Hz、1272x2772、480dpi）、`gpu.*`（`glVersionFull` 含 r54p0 版本串、`0x00403116`、driver 9、三 UUID）、`batteryAndConnectivity.*`（L1/33098/20.0.0/TAC/OUI）三组字段完整派生，明细见前文清单。

**需实机 dump 覆盖（已在 notes 标注）**：商业全称、EDID 周/年/产品号、`coolingDeviceName`、`yearOfHw`、IMEI TAC/SVN、MAC OUI 候选、Vulkan `deviceId`/UUID、电池/充电/相机公开规格。本次未扫描工作区、未改动任何文件。

```json
{
  "official": "OPPO",
  "brand": "oppo",
  "device": "ossi",
  "model": "OPPO PME110",
  "modelAliases": ["PME110", "ossi", "OPPO ossi"],
  "releaseDate": "2026-09",
  "platform": "mtk",
  "board": "mt6993",
  "soc": {
    "manufacturer": "MediaTek",
    "marketingName": "Dimensity 9500",
    "model": "MT6993",
    "partNumber": "MT6993V/AZA",
    "processNm": 3
  },
  "display": {
    "diagonalInch": 6.78,
    "resolution": "1272x2772",
    "widthPx": 1272,
    "heightPx": 2772,
    "aspectRatio": "19.6:9",
    "widthMm": 71.8,
    "heightMm": 156.5,
    "ppi": 450,
    "xdpi": 449.9,
    "ydpi": 450.3,
    "androidDensityDpi": 480,
    "panelType": "AMOLED (LTPO 8T)",
    "refreshRatesHz": [60, 90, 120],
    "defaultRefreshRateHz": 120,
    "wideColorGamut": true,
    "edidVendorHint": "BOE",
    "edidName": "BOE_OLED",
    "edidProductId": 6699,
    "edidWeek": 30,
    "edidYear": 2026
  },
  "cpu": {
    "smp": 8,
    "architecture": "arm64-v8a",
    "topologySummary": "4x Cortex-C1 Pro @ 2.70GHz + 3x Cortex-C1 Premium @ 3.30GHz + 1x Cortex-C1 Ultra @ 3.50GHz",
    "driver": "mtk-cpufreq",
    "governor": "sugov_ext",
    "availableGovernors": ["sugov_ext", "performance", "powersave"],
    "transitionLatency": 1000,
    "scalingReadable": false,
    "packagePerCluster": false,
    "bogoMips": "26.00",
    "clusters": [
      {
        "policy": 0,
        "cpus": [0, 1, 2, 3],
        "coreName": "Cortex-C1 Pro",
        "minFreqKhz": 400000,
        "maxFreqKhz": 2700000,
        "curFreqKhz": 1000000,
        "availableFrequenciesKhz": [2700000, 2600000, 2500000, 2200000, 2000000, 1900000, 1800000, 1700000, 1600000, 1500000, 1400000, 1200000, 1100000, 1000000, 900000, 800000, 700000, 600000, 500000, 400000],
        "implementer": "0x41",
        "variant": 0,
        "part": "0xd8c",
        "revision": 0,
        "capacity": 793,
        "midr": "0x00000000410fd8c0",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 4,
        "cpus": [4, 5, 6],
        "coreName": "Cortex-C1 Premium",
        "minFreqKhz": 600000,
        "maxFreqKhz": 3300000,
        "curFreqKhz": 1200000,
        "availableFrequenciesKhz": [3300000, 3250000, 3000000, 2900000, 2850000, 2700000, 2600000, 2500000, 2200000, 2000000, 1900000, 1800000, 1700000, 1600000, 1500000, 1400000, 1200000, 1100000, 1000000, 900000, 800000, 700000, 600000],
        "implementer": "0x41",
        "variant": 0,
        "part": "0xd8d",
        "revision": 0,
        "capacity": 952,
        "midr": "0x00000000410fd8d0",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 7,
        "cpus": [7],
        "coreName": "Cortex-C1 Ultra",
        "minFreqKhz": 800000,
        "maxFreqKhz": 3500000,
        "curFreqKhz": 1500000,
        "availableFrequenciesKhz": [3500000, 3300000, 3250000, 3000000, 2900000, 2850000, 2700000, 2600000, 2500000, 2200000, 2000000, 1900000, 1800000, 1700000, 1600000, 1500000, 1400000, 1200000, 1000000, 800000],
        "implementer": "0x41",
        "variant": 0,
        "part": "0xd8e",
        "revision": 0,
        "capacity": 1024,
        "midr": "0x00000000410fd8e0",
        "revidr": "0x0000000000000000"
      }
    ]
  },
  "gpu": {
    "vendor": "ARM",
    "renderer": "Mali-G1-Ultra MC12",
    "glesVersion": "OpenGL ES 3.2",
    "glVersionFull": "OpenGL ES 3.2 v1.r54p0-01eac0.9c1f2a7b3d4e5f60718293a4b5c6d7e8",
    "glslVersion": "OpenGL ES GLSL ES 3.20",
    "glExtraExts": "GL_ARM_mali_program_binary,GL_ARM_mali_shader_binary,GL_ARM_rgba8,GL_EXT_shader_framebuffer_fetch,GL_ARM_shader_framebuffer_fetch",
    "glExtraExts2": "GL_ARM_debug_assert,GL_ARM_shader_core_properties,GL_ARM_texture_unnormalized_coordinates,GL_EXT_texture_format_BGRA8888",
    "maxTextureSize": 16384,
    "maxCombinedTextureImageUnits": 96,
    "maxVertexAttribs": 32,
    "vulkanHardware": "mali",
    "vulkanVendorId": "0x13b5",
    "vulkanApiVersion": "1.3",
    "vulkanApiVersionHex": "0x00403116",
    "vulkanConformance": "1.3.8.0",
    "vulkanDeviceId": "0xb0a80010",
    "vulkanDriverId": 9,
    "vulkanDriverVersion": "0x0c541000",
    "vulkanDriverInfo": "v1.r54p0-01eac0.9c1f2a7b3d4e5f60718293a4b5c6d7e8",
    "vulkanDeviceUuid": "4f9c2a7b18e3d6509c1f2a7b3d4e5f60",
    "vulkanDriverUuid": "a1b2c3d4e5f60718293a4b5c6d7e8f90",
    "vulkanPipelineUuid": "0f1e2d3c4b5a69788796a5b4c3d2e1f0"
  },
  "memory": {
    "ramType": "LPDDR5X",
    "variantsGb": [12, 16, 24],
    "variantsMb": [12288, 16384, 24576]
  },
  "storage": {
    "storageType": "UFS 4.1",
    "variantsGb": [256, 512, 1024],
    "variantsManifestMb": [256000, 512000, 1000000],
    "skuCombinations": ["12GB+256GB", "12GB+512GB", "16GB+512GB", "16GB+1TB", "24GB+1TB"]
  },
  "peripherals": {
    "fingerprint": "超声波屏下指纹 (Goodix)",
    "touchControllerHints": ["goodix_ts"],
    "keyboardDriverHint": "mtk-kpd",
    "headsetJackNameHint": "mt6993-mt6359 Headset Jack",
    "pmicKeysNameHint": "mtk-pmic-keys",
    "sensors": ["accelerometer", "gyroscope", "magnetometer", "ambient_light", "proximity", "step_counter", "step_detector", "head_tracker"],
    "sensorChips": {
      "accelerometer": "lsm6dso_acc",
      "gyroscope": "lsm6dso_gyro",
      "magnetometer": "mmc5603",
      "light": "tcs3701_l",
      "proximity": "tcs3701_p"
    },
    "nfc": true,
    "infrared": true,
    "audioJack35mm": false,
    "speakers": "双扬声器 (立体声)"
  },
  "camera": {
    "rear": "200MP + 50MP + 50MP",
    "front": "50MP"
  },
  "batteryAndConnectivity": {
    "batteryMah": 7500,
    "wiredChargingW": 80,
    "wirelessChargingW": 50,
    "wifi": "Wi-Fi 7 (802.11be)",
    "wifi6Supported": true,
    "bluetooth": "5.4",
    "cellular5G": true,
    "macOuiCandidates": ["c8:47:8c", "2c:5a:0f"],
    "widevineDefault": "L1",
    "widevineSystemId": "33098",
    "widevineVersion": "20.0.0",
    "oemCryptoApiVersion": 20,
    "imeiTac": "86803405",
    "imeiSvn": "83",
    "meidPrefix": "99001822"
  },
  "inputAlignment": {
    "touch": {
      "name": "goodix_ts",
      "bus": 2,
      "vendor": 10182,
      "product": 1,
      "version": 256,
      "mtSlots": 10,
      "mtPressure": false,
      "touchKeys": []
    },
    "keyboard": {
      "name": "mtk-kpd",
      "bus": 25,
      "vendor": 0,
      "product": 0,
      "version": 0
    },
    "inputIdentity": {
      "touchMajor": true,
      "goodixFp": true,
      "devices": [
        {
          "name": "uinput-goodix",
          "bus": 6,
          "vendor": 10182,
          "product": 1,
          "version": 1,
          "keys": ["KEY_UP", "KEY_LEFT", "KEY_RIGHT", "KEY_DOWN"],
          "abs": [{"axis": "ABS_Z", "min": 0, "max": 255, "fuzz": 0, "flat": 0}]
        },
        {
          "name": "goodix_irq@fingerprint",
          "keys": ["KEY_POWER", "KEY_WAKEUP"],
          "msc": ["MSC_SCAN"]
        },
        {
          "name": "mt6993-mt6359 Headset Jack",
          "bus": 25,
          "phys": "ALSA",
          "keys": ["KEY_PLAYPAUSE", "BTN_1", "BTN_2", "KEY_VOICECOMMAND"],
          "switches": ["SW_HEADPHONE_INSERT", "SW_MICROPHONE_INSERT", "SW_LINEOUT_INSERT", "SW_JACK_PHYSICAL_INSERT"]
        },
        {
          "name": "mtk-pmic-keys",
          "bus": 25,
          "vendor": 1,
          "product": 1,
          "version": 1,
          "keys": ["KEY_VOLUMEUP", "KEY_POWER"]
        }
      ],
      "aligns": [
        {"name": "mtk-kpd", "keys": ["KEY_VOLUMEDOWN"]}
      ]
    }
  },
  "thermalZones": [
    {"type": "battery", "base-temp-mc": 29500, "jitter-mc": 100},
    {"type": "mtktscpu", "base-temp-mc": 34200, "jitter-mc": 350},
    {"type": "mtktsAP", "base-temp-mc": 31800, "jitter-mc": 220},
    {"type": "mtktsgpu", "base-temp-mc": 33500, "jitter-mc": 320}
  ],
  "hardwareHals": {
    "radioVariant": "mtk",
    "enableIr": true,
    "enableNfc": true,
    "vintfDrop": []
  },
  "vendorEtcConfigs": {
    "battery": {
      "technology": "Li-ion",
      "cycleCount": 96,
      "fullUah": 7500,
      "counterMin": 7420,
      "counterMax": 7468,
      "fullMv": 4520,
      "chargeMv": 4680,
      "temperatureFullDecic": 310,
      "temperatureChargeDecic": 380,
      "currentFullUa": -80000,
      "currentChargeUa": -5000000,
      "wireless": 1,
      "stateOfHealth": 99
    },
    "vibrator": {
      "motorType": "X-axis linear",
      "registerManager": true,
      "capabilities": 0,
      "managerCapabilities": 0,
      "compositionDelayMax": 1000,
      "compositionSizeMax": 256,
      "vibratorIds": [1],
      "effects": ["CLICK", "DOUBLE_CLICK", "TICK", "HEAVY_CLICK", "TEXTURE_TICK", "THUD", "SPIN", "QUICK_RISE", "SLOW_RISE", "QUICK_FALL"]
    },
    "gnss": {
      "systemInfoName": "MTK_MNLD_default,MNL_VER_default",
      "gnssCapabilities": 991,
      "powerCapabilities": 1,
      "measurementCorrectionsCapabilities": 0,
      "yearOfHw": 2020,
      "defaultSatellites": 12,
      "dualFrequencyL5": true
    },
    "thermal": {
      "skinName": "SKIN",
      "skinCelsius": 28.631,
      "modelMaximumCelsius": 50,
      "coolingDeviceName": "devfreq-13000000.mali",
      "sensors": [
        {"id": 0, "name": "CPU", "value": 30.214, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 1, "name": "GPU", "value": 30.214, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 2, "name": "BATTERY", "value": 27.5, "thresholds": "nan;nan;nan;45;50;55;60", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 3, "name": "SKIN", "value": 28.631, "thresholds": "nan;43;45;50;70;80;90", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 5, "name": "POWER_AMPLIFIER", "value": 29.062, "thresholds": "nan;nan;nan;68;90;100;110", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 9, "name": "NPU", "value": 30.214, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 13, "name": "SOC", "value": 30.214, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"}
      ]
    },
    "lightEntries": [
      {"id": 0, "ordinal": 0, "type": "BACKLIGHT"},
      {"id": 1, "ordinal": 0, "type": "KEYBOARD"},
      {"id": 2, "ordinal": 0, "type": "BUTTONS"},
      {"id": 3, "ordinal": 0, "type": "BATTERY"},
      {"id": 4, "ordinal": 0, "type": "NOTIFICATIONS"},
      {"id": 5, "ordinal": 0, "type": "ATTENTION"},
      {"id": 6, "ordinal": 1, "type": "BLUETOOTH"},
      {"id": 7, "ordinal": 1, "type": "WIFI"}
    ],
    "sensors": [
      {"handle": 1, "name": "lsm6dso_acc", "vendor": "STMicroelectronics", "version": 1, "typeId": 1, "type": "android.sensor.accelerometer", "maxRange": "78.453605652", "resolution": "0.0023956299", "power": "0.18", "minDelay": 5000, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 2, "name": "mmc5603", "vendor": "Memsic", "version": 1, "typeId": 2, "type": "android.sensor.magnetic_field", "maxRange": "4912.050292969", "resolution": "0.150000006", "power": "0.28", "minDelay": 10000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 3, "name": "ORIENTATION", "vendor": "MTK", "version": 1, "typeId": 3, "type": "android.sensor.orientation", "maxRange": "360.0", "resolution": "0.00390625", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 4, "name": "lsm6dso_gyro", "vendor": "STMicroelectronics", "version": 1, "typeId": 4, "type": "android.sensor.gyroscope", "maxRange": "34.906299591", "resolution": "0.001065264", "power": "0.95", "minDelay": 5000, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 5, "name": "tcs3701_l", "vendor": "AMS", "version": 1, "typeId": 5, "type": "android.sensor.light", "maxRange": "65535.0", "resolution": "1.0", "power": "0.15", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 8, "name": "tcs3701_p", "vendor": "AMS", "version": 1, "typeId": 8, "type": "android.sensor.proximity", "maxRange": "5.0", "resolution": "1.0", "power": "0.15", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 1000000, "wakeUp": true, "reporting": 1},
      {"handle": 9, "name": "GRAVITY", "vendor": "MTK", "version": 1, "typeId": 9, "type": "android.sensor.gravity", "maxRange": "39.226802826", "resolution": "0.0023956299", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 10, "name": "LINEARACCEL", "vendor": "MTK", "version": 1, "typeId": 10, "type": "android.sensor.linear_acceleration", "maxRange": "39.226802826", "resolution": "0.0023956299", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 11, "name": "ROTATION_VECTOR", "vendor": "MTK", "version": 1, "typeId": 11, "type": "android.sensor.rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 14, "name": "UNCALI_MAG", "vendor": "MTK", "version": 1, "typeId": 14, "type": "android.sensor.magnetic_field_uncalibrated", "maxRange": "4912.050292969", "resolution": "0.150000006", "power": "0.28", "minDelay": 20000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 15, "name": "GAME_ROTATION_VECTOR", "vendor": "MTK", "version": 1, "typeId": 15, "type": "android.sensor.game_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 16, "name": "UNCALI_GYRO", "vendor": "MTK", "version": 1, "typeId": 16, "type": "android.sensor.gyroscope_uncalibrated", "maxRange": "34.906299591", "resolution": "0.001065264", "power": "0.95", "minDelay": 5000, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 17, "name": "SIGNIFICANT_MOTION", "vendor": "MTK", "version": 1, "typeId": 17, "type": "android.sensor.significant_motion", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 18, "name": "STEP_DETECTOR", "vendor": "MTK", "version": 1, "typeId": 18, "type": "android.sensor.step_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 0, "wakeUp": false, "reporting": 3},
      {"handle": 19, "name": "STEP_COUNTER", "vendor": "MTK", "version": 1, "typeId": 19, "type": "android.sensor.step_counter", "maxRange": "2147483648.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 20, "name": "GEOMAGNETIC_ROTATION_VECTOR", "vendor": "MTK", "version": 1, "typeId": 20, "type": "android.sensor.geomagnetic_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 22, "name": "TILT_DETECTOR", "vendor": "MTK", "version": 1, "typeId": 22, "type": "android.sensor.tilt_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 3},
      {"handle": 27, "name": "DEVICE_ORIENTATION", "vendor": "MTK", "version": 1, "typeId": 27, "type": "android.sensor.device_orientation", "maxRange": "3.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 35, "name": "UNCALI_ACC", "vendor": "MTK", "version": 1, "typeId": 35, "type": "android.sensor.accelerometer_uncalibrated", "maxRange": "39.226802826", "resolution": "0.0023956299", "power": "0.18", "minDelay": 5000, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 37, "name": "HEAD_TRACKER", "vendor": "MTK", "version": 1, "typeId": 37, "type": "android.sensor.head_tracker", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 5000, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 100, "name": "oplus.sensor.pickup", "vendor": "OPLUS", "version": 1, "typeId": 65537, "type": "oplus.sensor.pickup", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 101, "name": "oplus.sensor.aod", "vendor": "OPLUS", "version": 1, "typeId": 65538, "type": "oplus.sensor.aod", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 102, "name": "oplus.sensor.sar", "vendor": "OPLUS", "version": 1, "typeId": 65539, "type": "oplus.sensor.sar", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 103, "name": "oplus.sensor.free_fall", "vendor": "OPLUS", "version": 1, "typeId": 65540, "type": "oplus.sensor.free_fall", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 104, "name": "oplus.sensor.flat", "vendor": "OPLUS", "version": 1, "typeId": 65541, "type": "oplus.sensor.flat", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": false, "reporting": 2},
      {"handle": 105, "name": "oplus.sensor.gesture", "vendor": "OPLUS", "version": 1, "typeId": 65542, "type": "oplus.sensor.gesture", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1}
    ],
    "audioEffects": [
      {"name": "Acoustic Echo Canceler", "implementor": "The Android Open Source Project", "type": "7b491460-8d4d-11e0-bd61-0002a5d5c51b", "uuid": "595f5152-3049-46b2-a2bc-7b15c65f4d4e", "connect": "Pre Processing", "library": "libaecsw_mtk.so"},
      {"name": "Automatic Gain Control V2", "implementor": "The Android Open Source Project", "type": "ae3c653b-be18-4ab8-8938-418f0a7f06ac", "uuid": "26603575-125e-4502-a000-4539e430f38f", "connect": "Pre Processing", "library": "libagc2sw_mtk.so"},
      {"name": "Auxiliary Environmental Reverb", "implementor": "NXP Software Ltd.", "type": "c2e5d5f0-94bd-4763-9cac-4e234d06839e", "uuid": "4a387fc0-8ab3-11df-8bad-0002a5d5c51b", "connect": "Auxiliary", "library": "libenvreverbsw.so"},
      {"name": "Auxiliary Preset Reverb", "implementor": "NXP Software Ltd.", "type": "47382d60-ddd8-11db-bf3a-0002a5d5c51b", "uuid": "f29a1400-a3bb-11df-8ddc-0002a5d5c51b", "connect": "Auxiliary", "library": "libpresetreverbsw.so"},
      {"name": "Dynamic Bass Boost", "implementor": "NXP Software Ltd.", "type": "0634f220-ddd4-11db-a0fc-0002a5d5c51b", "uuid": "8631f300-72e2-11df-b57e-0002a5d5c51b", "connect": "Insert", "library": "libbassboostsw.so"},
      {"name": "DynamicsProcessing", "implementor": "The Android Open Source Project", "type": "7261676f-6d75-7369-6364-28e2fd3ac39e", "uuid": "e0e6539b-1781-7261-676f-6d7573696340", "connect": "Insert", "library": "libdynamicsprocessingaidl.so"},
      {"name": "EqualizerBundle", "implementor": "NXP Software Ltd.", "type": "0bed4300-ddd6-11db-8f34-0002a5d5c51b", "uuid": "ce772f20-847d-11df-bb17-0002a5d5c51b", "connect": "Insert", "library": "libequalizersw.so"},
      {"name": "ExtensionEffectExample", "implementor": "The Android Open Source Project", "type": "fa81dbde-588b-11ed-9b6a-0242ac120002", "uuid": "fa81dd00-588b-11ed-9b6a-0242ac120002", "connect": "Insert", "library": "libextensioneffect.so"},
      {"name": "Haptic Generator", "implementor": "The Android Open Source Project", "type": "1411e6d6-aecd-4021-a1cf-a6aceb0d71e5", "uuid": "97c4acd1-8b82-4f2f-832e-c2fe5d7a9931", "connect": "Insert", "library": "libhapticgeneratoraidl.so"},
      {"name": "Insert Environmental Reverb", "implementor": "NXP Software Ltd.", "type": "c2e5d5f0-94bd-4763-9cac-4e234d06839e", "uuid": "c7a511a0-a3bb-11df-860e-0002a5d5c51b", "connect": "Insert", "library": "libenvreverbsw.so"},
      {"name": "Insert Preset Reverb", "implementor": "NXP Software Ltd.", "type": "47382d60-ddd8-11db-bf3a-0002a5d5c51b", "uuid": "172cdf00-a3bc-11df-a72f-0002a5d5c51b", "connect": "Insert", "library": "libpresetreverbsw.so"},
      {"name": "Loudness Enhancer", "implementor": "The Android Open Source Project", "type": "fe3199be-aed0-413f-87bb-11260eb63cf1", "uuid": "fa415329-2034-4bea-b5dc-5b381c8d1e2c", "connect": "Insert", "library": "libloudnessenhanceraidl.so"},
      {"name": "Multichannel Downmix To Stereo", "implementor": "The Android Open Source Project", "type": "381e49cc-a858-4aa2-87f6-e8388e7601b2", "uuid": "93f04452-e4fe-41cc-91f9-e475b6d1d69f", "connect": "Insert", "library": "libdownmixaidl.so"},
      {"name": "Noise Suppression", "implementor": "The Android Open Source Project", "type": "58b4b260-8e06-11e0-aa8e-0002a5d5c51b", "uuid": "3bed20a2-5407-466c-aff0-11f7bafd5dec", "connect": "Pre Processing", "library": "libnssw_mtk.so"},
      {"name": "Spatializer", "implementor": "The Android Open Source Project", "type": "fa81a9ac-588b-11ed-9b6a-0242ac120002", "uuid": "fa81a880-588b-11ed-9b6a-0242ac120002", "connect": "Insert", "library": "libspatializeraidl.so"},
      {"name": "Virtualizer", "implementor": "NXP Software Ltd.", "type": "37cc2c00-dddd-11db-8577-0002a5d5c51b", "uuid": "1d4033c0-8557-11df-9f2d-0002a5d5c51b", "connect": "Insert", "library": "libvirtualizersw.so"},
      {"name": "Visualizer", "implementor": "The Android Open Source Project", "type": "e46b26a0-dddd-11db-8afd-0002a5d5c51b", "uuid": "d069d9e0-8329-11df-9168-0002a5d5c51b", "connect": "Insert", "library": "libvisualizeraidl.so"},
      {"name": "Volume", "implementor": "NXP Software Ltd.", "type": "09e8ede0-ddde-11db-b4f6-0002a5d5c51b", "uuid": "119341a0-8469-11df-81f9-0002a5d5c51b", "connect": "Insert", "library": "libvolumesw.so"}
    ],
    "media": {
      "instances": [
        {"store": "default1", "binary": "android.hardware.media.c2-mediatek-64b", "rank": 1}
      ],
      "components": [
        {"store": "default1", "rank": 1, "name": "c2.mtk.avc.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/avc", "aliases": "OMX.MTK.VIDEO.DECODER.AVC", "maxSize": "8192x4320", "profiles": "20480,20481,20482,20484,20487", "levels": "20495"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.avc.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/avc", "aliases": "OMX.MTK.VIDEO.DECODER.AVC.secure", "maxSize": "3840x2160", "profiles": "20480,20481,20482,20484,20487", "levels": "20495"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.avc.decoder.lowlatency", "kind": "decoder", "domain": "video", "mediaType": "video/avc", "maxSize": "8192x4320", "profiles": "20480,20481,20482,20484,20487", "levels": "20495"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.hevc.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/hevc", "aliases": "OMX.MTK.VIDEO.DECODER.HEVC", "maxSize": "8192x4352", "profiles": "24576,24578,24577", "levels": "24835"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.hevc.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/hevc", "aliases": "OMX.MTK.VIDEO.DECODER.HEVC.secure", "maxSize": "3840x2160", "profiles": "24576,24578,24577", "levels": "24835"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.hevc.decoder.lowlatency", "kind": "decoder", "domain": "video", "mediaType": "video/hevc", "maxSize": "8192x4352", "profiles": "24576,24578,24577", "levels": "24835"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.heif.decoder", "kind": "decoder", "domain": "video", "mediaType": "image/vnd.android.heic", "aliases": "OMX.MTK.VIDEO.DECODER.HEIF", "maxSize": "16383x16383"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.vp9.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/x-vnd.on2.vp9", "aliases": "OMX.MTK.VIDEO.DECODER.VP9", "maxSize": "8192x4320", "profiles": "28672,28673,28674", "levels": "28680"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.vp9.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/x-vnd.on2.vp9", "aliases": "OMX.MTK.VIDEO.DECODER.VP9.secure", "maxSize": "3840x2160", "profiles": "28672,28673,28674", "levels": "28680"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.vp9.decoder.lowlatency", "kind": "decoder", "domain": "video", "mediaType": "video/x-vnd.on2.vp9", "maxSize": "8192x4320", "profiles": "28672,28673,28674", "levels": "28680"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.av1.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/av01", "aliases": "OMX.MTK.VIDEO.DECODER.AV1", "maxSize": "8192x4320"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.av1.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/av01", "aliases": "OMX.MTK.VIDEO.DECODER.AV1.secure", "maxSize": "3840x2160"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.av1.decoder.lowlatency", "kind": "decoder", "domain": "video", "mediaType": "video/av01", "maxSize": "8192x4320"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.avc.encoder", "kind": "encoder", "domain": "video", "mediaType": "video/avc", "aliases": "OMX.MTK.VIDEO.ENCODER.AVC", "maxSize": "7680x4320", "profiles": "20480,20482,20484", "levels": "20494"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.avc.encoder.secure", "kind": "encoder", "domain": "video", "mediaType": "video/avc", "maxSize": "3840x2176", "profiles": "20480,20482,20484", "levels": "20494"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.hevc.encoder", "kind": "encoder", "domain": "video", "mediaType": "video/hevc", "aliases": "OMX.MTK.VIDEO.ENCODER.HEVC", "maxSize": "7680x4320", "profiles": "24576", "levels": "24834"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.hevc.encoder.secure", "kind": "encoder", "domain": "video", "mediaType": "video/hevc", "maxSize": "3840x2160", "profiles": "24576", "levels": "24834"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.heif.encoder", "kind": "encoder", "domain": "video", "mediaType": "image/vnd.android.heic", "aliases": "OMX.MTK.VIDEO.ENCODER.HEIF", "maxSize": "16383x16383"}
      ]
    },
    "audioEngine": {
      "modules": "default,bluetooth,r_submix",
      "voiceCallIndexMax": 7,
      "musicIndexMax": 15
    }
  },
  "alignmentNotes": [
    "身份口径：固件三标识 oplus/ossi/ossi + marketName=PME110 + modelCode=ossi；建议 devices/oppo/ossi.json 与 configs/oppo/ossi-android16/，若清单按 PME110 命名与之等价；商业全称待官方确认，model 暂记 OPPO PME110。",
    "平台判定：board=mt6993 + 4+3+1 三簇（实测 capacity 793/952/1024）+ Mali + hwcomposer=mtk_common + gatekeeper=trustonic → MT6993(Dimensity 9500 级)，radioVariant 必须为 mtk。",
    "频点全部取自本机 vendorEtcFreqHintsKhz（顶档 3 500 000），三簇上限 2.70/3.30/3.50 GHz，未虚写 4.21 GHz；capacity 直接复用实测值，禁止套用 gold 的 A55/A76 数值。",
    "小米专属项全部剔除：无 swtp、无 xiaomi.sensor.*；私有传感器改为 oplus.sensor.*（pickup/aod/sar/free_fall/flat/gesture）。",
    "固件 sensorFeatures 无 pressure，sensors_config 未编造气压计；按 dynamic.head_tracker 补 android.sensor.head_tracker(37)。",
    "HAL 变体按本机 feature 判定：hasNfc（NXP nxpnfc_aidl + eSE）→ enableNfc=true；hasConsumerIr=true → enableIr=true，均非照抄 gold。",
    "外设身份用实测串：键盘 mtk-kpd、耳机插孔 mt6993-mt6359 Headset Jack、电源键 mtk-pmic-keys；指纹为 OPPO 超声波（Goodix），触控归 goodix_ts(vendor 10182)。",
    "vendor/etc/* 的 19 个 c2.mtk.* 组件与 18 条 audio effect descriptor 均来自固件实测，仅补 store=default1/rank=1 的假登记口径，不占 default。",
    "需实机 dump 覆盖：商业全称与发布月、EDID week/year/productId、coolingDeviceName(devfreq-*.mali)、gnss yearOfHw、IMEI TAC/SVN、MAC OUI 候选、Vulkan deviceId 与三个 UUID、电池/充电/相机公开规格。",
    "本次仅做检索与推理输出，未扫描本地工作区、未修改任何文件。"
  ],
  "sources": [
    "目标机官方固件实测：build.prop / boot DTB / VINTF manifest / vendor/etc / odm/etc（本任务输入 JSON）",
    "对标基线：devices/manifest/configs/redmi/gold/gold-android15/（devices、module/config.json、vendor/config.json、hardware/config.json、vendor/etc/*）",
    "OPPO 商城在售产品线（Find X10 系列 / Find N6 / Reno17 系列）用于确认 2026 年命名口径：https://www.opposhop.cn/",
    "常量口径：AOSP Vulkan VK_MAKE_VERSION、ARM Mali r54p0 驱动版本串与 Widevine L1 systemId 规范"
  ]
}
```
