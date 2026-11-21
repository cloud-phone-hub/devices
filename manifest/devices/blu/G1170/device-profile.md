# 真机硬件规格与云机对齐留档 · BLU G75 (blu/G1170)

- **生成时间**: `2026-11-21T09:49:33+08:00`
- **ROM Build ID**: `15-AP3A.240905.015.A2-20260209`
- **DSH 聊天会话 ID**: `session-46fb30c8-21f6-4db6-9ba7-2fa2f530c00c`（标题：`[02_prepare] 真机规格采集 · BLU G75 (blu/G1170 · 15-AP3A.240905.015.A2-20260209)`）
- **采集模型路由**: `cdh-maas / deepseek-v4.1-flash-1017`（复用在线 DSH Web: `false`）

---

## 一、设备表对齐规格（对应 `devices/manifest/devices/blu/G1170.json`）

| 字段 | 对齐值 | 说明 |
| --- | --- | --- |
| `official` | `BLU Products` | 厂商集团 |
| `brand` | `blu` | 子品牌标识 |
| `model` | `BLU G75` | 商业上市机型名 （型号代码: `G75`, `G1170`） |
| `device` | `G1170` | 设备代号 |
| `platform` / `board` | `mtk` / `mt6768` | 芯片平台 / 主板代号 |
| `resolution` | `720x1600` | 屏幕物理像素分辨率 |
| `inch` | `72x159` | 屏幕可视物理宽×高 mm（对角线 `6.88"`） |
| `density` | `320` | Android 逻辑密度 dpi（物理 PPI `255`，xdpi `255.00` / ydpi `255.00`） |
| `smp` | `8` | CPU 核心总数 |
| `memory` | `[4096]` | 全系发售内存档位 MB（4G，LPDDR4X） |
| `storage` | `[256000]` | 全系发售存储档位（256G，eMMC 5.1） |

- **常见量产发售版本组合**: `4GB+256GB`

## 二、真机深层硬件对齐画像（参考 `redmi/gold` 标准）

### 1. SoC 与 CPU 簇全量频点表（`cdh_cpufreq` / `module/config.json` 对齐）
- **SoC 芯片**: MediaTek Helio G81 (`MT6769Z` MT6769ZV/ZA, 12nm)
- **CPU 拓扑**: `6x Cortex-A55 @ 1.8GHz + 2x Cortex-A75 @ 2.0GHz`（驱动: `mt-cpufreq`，调速器: `sugov_ext`，可选调速器: `[sugov_ext conservative powersave performance schedutil]`，BogoMIPS: `26.00`）

| 策略簇 | 核心列表 | 微架构 | Part / MIDR | 容量权重 | 最小 / 驻留 / 最大 (kHz) | 频点档数 |
| --- | --- | --- | --- | --- | --- | --- |
| policy0 | `[0 1 2 3 4 5]` | Cortex-A55 | `0xd05` (`0x00000000412fd050`) | `495` | `500000 / 774000 / 1800000` | `16 档` |
| policy6 | `[6 7]` | Cortex-A75 | `0xd0a` (`0x00000000412fd0a0`) | `1024` | `850000 / 1176000 / 2000000` | `16 档` |

- **`policy0` (`Cortex-A55`, cpus `[0 1 2 3 4 5]`) 全量 `available-frequencies` (16 档, kHz)**:
  `[1800000 1625000 1500000 1450000 1375000 1325000 1275000 1175000 1100000 1050000 999000 950000 900000 850000 774000 500000]`
- **`policy6` (`Cortex-A75`, cpus `[6 7]`) 全量 `available-frequencies` (16 档, kHz)**:
  `[2000000 1950000 1900000 1850000 1800000 1710000 1621000 1532000 1443000 1354000 1295000 1176000 1087000 998000 909000 850000]`

<details>
<summary>可直接用于 <code>module/config.json</code> 的 <code>cpufreq</code> 完整配置片段</summary>

```json
{
  "cpufreq": {
    "driver": "mt-cpufreq",
    "governor": "sugov_ext",
    "available-governors": [
      "sugov_ext",
      "conservative",
      "powersave",
      "performance",
      "schedutil"
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
          3,
          4,
          5
        ],
        "min-freq": 500000,
        "max-freq": 1800000,
        "cur-freq": 774000,
        "available-frequencies": [
          1800000,
          1625000,
          1500000,
          1450000,
          1375000,
          1325000,
          1275000,
          1175000,
          1100000,
          1050000,
          999000,
          950000,
          900000,
          850000,
          774000,
          500000
        ],
        "implementer": "0x41",
        "variant": 2,
        "part": "0xd05",
        "revision": 0,
        "capacity": 495,
        "midr": "0x00000000412fd050",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 6,
        "cpus": [
          6,
          7
        ],
        "min-freq": 850000,
        "max-freq": 2000000,
        "cur-freq": 1176000,
        "available-frequencies": [
          2000000,
          1950000,
          1900000,
          1850000,
          1800000,
          1710000,
          1621000,
          1532000,
          1443000,
          1354000,
          1295000,
          1176000,
          1087000,
          998000,
          909000,
          850000
        ],
        "implementer": "0x41",
        "variant": 2,
        "part": "0xd0a",
        "revision": 0,
        "capacity": 1024,
        "midr": "0x00000000412fd0a0",
        "revidr": "0x0000000000000000"
      }
    ]
  }
}
```
</details>

### 2. 显示面板、GPU 与图形栈（`gpu_id.cfg` / `buildVendor` 对标 `gold`）
- **面板类型与刷新率**: IPS LCD · 刷新率 `[60 120] Hz`（默认 `120 Hz`） · 广色域 `false` · EDID `BOE` (`BOE`, product=`2603`, year=`2025`W`42`)
- **OpenGL ES**: `ARM` / `Mali-G52 MC2` / `OpenGL ES 3.2 v1.r32p1-01eac0.7f3ab19c2d5e08a4ff61b37e0c9d2548`
  - GL 驱动扩展: `GL_ARM_mali_program_binary,GL_ARM_mali_shader_binary,GL_ARM_rgba8` GL_ARM_debug_assert,GL_ARM_shader_core_properties,GL_ARM_texture_unnormalized_coordinates
- **Vulkan**: `ro.hardware.vulkan=mali` · VendorID `0x13b5` · DeviceID `0x70000002` · API `1.3` (`0x00403116`) · DriverID `9` (`0x0c401000`)

### 3. 触控、指纹、外设与输入节点对齐（`module/config.json` & `hardware/config.json` 对标 `gold`）
- **指纹方案**: 侧边电容指纹 (Goodix)
- **触控与按键对齐**: touch=`fts_ts` · keyboard=`mtk-kpd` · input_identity 外设数=`3` 项
- **硬件 HAL 变体与开关**: radioVariant=`mtk` · NFC=`false` · 红外=`false` · 3.5mm=`true` · 双扬声器
- **影像规格**: 后置 50MP (AF) | 前置 13MP
- **电池与通信**: `6000 mAh`（有线 `18W` / 无线 `0W`） · Wi-Fi 5 (802.11ac)（Wi-Fi 6=`false`） · BT `5.0` · 5G=`false` · Widevine `L3` (sysId=`27141`) · TAC `35874005` · MAC OUI `[00:0c:43]`

### 4. `vendor/etc/*_config.xml` 虚拟 HAL AI 对齐清单（对标 `gold-android15/vendor/etc`）
- **`sensors_config.xml`**: 共 `15` 项对齐传感器
- **`battery_config.xml`**: `Li-poly` · 容量 `6000 mAh` · 满电/充电电压 `4400 / 4470 mV`
- **`thermal_config.xml`**: 皮肤基温 `28.600 ℃` · 温区传感器 `6` 项 · Cooling=`devfreq-13000000.mali`
- **`gnss_config.xml`**: systemInfo=`MTK_MNLD_default,MNL_VER_default` · yearOfHw=`2020` · L5=`true`
- **`vibrator_config.xml` & `light_config.xml`**: 马达 `ERM 转子马达` (cap=`0`, effects=`[CLICK DOUBLE_CLICK TICK HEAVY_CLICK]`) · 逻辑灯 `8` 项
- **`audio_effects_descriptors.xml` & `media_config.xml`**: AI 音效描述符 `17` 项 · AI Codec2 实例 `1` 项 / 组件 `12` 项

### 5. AI 真机对齐提示
- 平台替换：gold 为 MT6833/Dimensity 6080 + Mali-G57 MC2，本机为 MT6769Z/Helio G81 + Mali-G52 MC2（板号沿用厂商 mt6768），GPU 节点地址因此为 13000000.mali（coolingDeviceName=devfreq-13000000.mali）。
- CPUFreq 全部取本机 DTB OPP：policy0（A55，part 0xd05，capacity 495）16 档止于 1800000kHz，policy6（A75，part 0xd0a，capacity 1024）16 档止于 2000000kHz；公开规格写 A55 1.7GHz，以固件 OPP 表 1.8GHz 为准。vendor/etc 里的 3000000/2400000 等提示频率与本机 OPP 无交集，不采用。
- 屏幕：6.88" IPS 720x1600 120Hz、可视区 71.7x159.4mm、320dpi；AMOLED 专属项（wideColorGamut、edidName=MTKDEV 等）一律不沿用，wideColorGamut=false，刷新率仅保留 60/120。
- 传感器：固件 permissions 的 sensorFeatures=accelerometer/compass/light/proximity/stepcounter/stepdetector，公开规格明确无陀螺仪、无气压计，故物理层只给 accel/mag/light/prox 四颗，融合层不含 ROTATION_VECTOR/GAME_ROTATION_VECTOR/UNCALI_GYRO；BLU 为 AOSP 直出，绝对不填 xiaomi.sensor.*/oplus/vivo/samsung 私有传感器，也不填 swtp（swtp 仅小米系）。
- 外设身份：mtk-kpd、mt6768-mt6359 Headset Jack、mtk-pmic-keys 三项为固件实测直接落库；指纹为侧边 Goodix 电容（VINTF 含 vendor.goodix...IGoodixFingerprintDaemon），节点取 uinput-goodix；触控按 MTK G81 低成本板最常见的 Focaltech fts_ts（bus 24）填写，需 adb shell getevent -pl 复核。
- GPU/Vulkan：glVersionFull 与 vulkanDriverInfo 按 MTK Android15 Mali DDK r32p1 基线给出（构建 hash 为占位，须以真机 glGetString 为准）；vulkanDeviceId 0x70000002、maxTextureSize 8192、maxVertexAttribs 16 为 G52 推断，非 gold 的 G57 值。
- Vendor 身份属性：6000mAh 低端机无 L1 认证，Widevine 定 L3（systemId 为推断占位）；EDID 厂商码/面板 ID/年份周数、IMEI TAC（35874005）、Wi-Fi OUI（00:0c:43，MTK 连接芯片池）均为 AI 推断占位，需真机回采后替换。
- 电池/马达：6000mAh Li-poly 单电芯、18W（9V/2A）有线、无无线充；ERM 转子马达 capabilities=0，振动原语收窄为 CLICK/DOUBLE_CLICK/TICK/HEAVY_CLICK（gold 的 X 轴线性马达能力位不适用）。
- GNSS：MTK MNLD（固件 MNL_Config 版本 20050701.6.00.00）→ systemInfoName=MTK_MNLD_default,MNL_VER_default、yearOfHw=2020；dualFrequencyL5=true 依固件 hasL5Config 保留。
- 虚拟 HAL 配置：radioVariant=mtk（MTK radio HAL 全套实测）、enableIr=false（hasConsumerIr=false）、enableNfc=false（hasNfc=false 且规格无 NFC）；音效 17 条 descriptor 与 Codec2 default1 组件（rank=1）全部取自固件实测，未占用 default store。
- 存储/内存：LPDDR4X 4GB（1800MHz）+ 256GB；厂商宣传的 14GB 为 4GB 物理 + 10GB 虚拟内存扩展，不得写进 RAM 档位。

## 三、官方固件包解包实测对账（`build.prop` + `boot.img` + `VINTF` + `DTB` + `fstab` + `vendor/etc`）

- `marketName`: `` | `modelCode`: `G75` | `manufacturer`: `BLU`
- `board`: `mt6768` | `soc`: `  ()`
- `densityDpi`: `320` | `egl`: `meow` | `vulkan`: `mali` | `abilist`: `arm64-v8a`
- `securityPatch`: `2026-02-05` | `buildDate`: `20260209` | `kernelLine`: `` | `vintfVersion`: `8.0`
- `partitionAndroidVersions`: `system=15, system_ext=15, product=15, vendor=15, odm=15, vendor_dlkm=15, odm_dlkm=15`
- `fingerprint`: `BLU/G75/G1170:15/AP3A.240905.015.A2/1770610308:user/release-keys`
- `vendor_boot.cmdline` (4 项): `androidboot.product.hardware.sku=G75`, `androidboot.secureboot=1`, `androidboot.slot=a`, `androidboot.veritymode.managed=yes`
- `vda.partitions` (5 个附加分区): `vendor_persist.img->/mnt/vendor/persist`, `protect_f.img->/mnt/vendor/protect_f`, `protect_s.img->/mnt/vendor/protect_s`, `nvdata.img->/mnt/vendor/nvdata`, `nvcfg.img->/mnt/vendor/nvcfg`
- `vintfPlaceholders`: radio=`34` 项 · audio=`1` 项 · gnss=`1` 项 · nn=`50` 项

### `vendor/etc` 与 `odm/etc` 硬件配置实测详情

- **Permissions 硬件特性**（共 `79` 项）: 红外=`false` · NFC=`false` · 指纹=`true` · BLE=`true` · Wi-Fi Direct=`true` · Audio LowLatency=`true` · Vulkan Level=`1` (API `1.3`)
  - 声明的传感器特性 (`6` 项): `accelerometer`, `compass`, `light`, `proximity`, `stepcounter`, `stepdetector`
- **Sensors 原厂配置**: SubHALs=`[android.hardware.sensors@2.X-subhal-mediatek.so]` · 芯片归属=`map[]`
- **Audio 策略与音效配置**: 模块=`[primary]` · 采样率=`[44100 48000]` · 听筒=`true` · 有线耳机路由=`true` · 双麦=`true` · 音效描述符=`17` 项（源: `vendor/etc/audio_effects_config.xml`）
  - 实测音效清单: `Acoustic Echo Canceler (The Android Open Source Project)`, `Automatic Gain Control V2 (The Android Open Source Project)`, `Auxiliary Environmental Reverb (NXP Software Ltd.)`, `Auxiliary Preset Reverb (NXP Software Ltd.)`, `Dynamic Bass Boost (NXP Software Ltd.)`, `DynamicsProcessing (The Android Open Source Project)`, `EqualizerBundle (NXP Software Ltd.)`, `ExtensionEffectExample (The Android Open Source Project)`, `Haptic Generator (The Android Open Source Project)`, `Insert Environmental Reverb (NXP Software Ltd.)`, `Insert Preset Reverb (NXP Software Ltd.)`, `Loudness Enhancer (The Android Open Source Project)`, `Multichannel Downmix To Stereo (The Android Open Source Project)`, `Noise Suppression (The Android Open Source Project)`, `Virtualizer (NXP Software Ltd.)`, `Visualizer (The Android Open Source Project)`, `Volume (NXP Software Ltd.)`
- **Media Codec2 硬编解码配置**: Store实例=`[android.hardware.media.c2-mediatek-64b->default1]` · 厂商/杜比组件=`18` 项 (`c2.mtk.adpcm-ms.decoder`, `c2.mtk.adpcm-dvi-ima.decoder`, `c2.mtk.ape.decoder`, `c2.mtk.alac.decoder`, `c2.mtk.wma.decoder`, `c2.mtk.mp3.decoder`, `c2.mtk.mpeg4.decoder`, `c2.mtk.h263.decoder`, `c2.mtk.avc.decoder`, `c2.mtk.avc.decoder.secure`, `c2.mtk.hevc.decoder`, `c2.mtk.vpx.decoder`, `c2.mtk.vp9.decoder`, `c2.mtk.vp9.decoder.secure`, `c2.mtk.mpeg2.decoder`, `c2.mtk.avc.encoder`, `c2.mtk.avc.encoder.secure`, `c2.mtk.hevc.encoder`)
- **GNSS 定位配置**: 源文件=`vendor/etc/MNL_Config.xml` · 版本=`20050701.6.00.00` · 模式=`6 (GPS+GLONASS+Galileo+BeiDou)` · L1Only=`false` · L5配置=`true`
- **Display 面板与 GPU 平台配置**: DSI面板型号=`[]` · screen_light.xml=`false` · Mali参数=`map[OSU_PROTECTED_MEMORY_HEAP_NAME:mtk_svp_region PLATFORM_AGT_FREQUENCY_KHZ:13000]`
- **ODM SKU / NFC / Wi-Fi / 影像场景**: NFC芯片=`-` · RSC区域=`[]` · 相机Tuning角色=`[]`
- **Power 调频/核数控制表**: 各簇最大核数=`[4 3]` · 配置文件=`[vendor/etc/power_app_cfg.xml vendor/etc/powercontable.xml vendor/etc/powerscntbl.xml vendor/etc/task_profiles.json]`
- **已扫描解析的 `vendor/etc` & `odm/etc` 配置文件**: 共 `61` 个

## 四、DSH 聊天界面回复原文

**简明总结（BLU G75 / G1170 对照 gold 标杆）**

- 平台：MT6769Z/Helio G81（12nm）+ Mali-G52 MC2，板号沿用厂商 `mt6768`；频点、MIDR、capacity 全部取自本机 DTB（A55 六核 16 档止于 1.8GHz / A75 双核 16 档止于 2.0GHz），不沿用 gold 的 MT6833 任何档位。
- 屏幕：6.88" IPS 720×1600 120Hz、可视区 71.7×159.4mm、320dpi；AMOLED/宽色域/1080p/400dpi 等 gold 专属值全部作废。
- 传感器：固件 sensorFeatures 与公开规格一致——有 accel/compass/light/prox/计步，**无陀螺仪、无气压计**；BLU 为 AOSP 直出，不含任何品牌私有传感器，也不填 `swtp`。
- 外设：`mtk-kpd`、`mt6768-mt6359 Headset Jack`、`mtk-pmic-keys` 三项固件实测直落；侧边 Goodix 指纹 → `uinput-goodix`；触控按 Focaltech `fts_ts` 填写（待 `getevent -pl` 复核）。
- HAL 变体：`radioVariant=mtk`、`enableIr=false`、`enableNfc=false`；4G only、Wi-Fi 5 双频、BT 5.0、双扬声器 + 3.5mm。
- 电池/马达/GNSS：6000mAh Li-poly 单电芯 18W、无无线充；ERM 转子马达 capabilities=0；MTK MNLD（MNL 20050701.6.00.00）→ `yearOfHw=2020`、保留 L5。
- 固件实测原样保留：17 条音效描述符、Codec2 `default1` 组件（rank=1）、音频 device/port 表、`audio_param`/`media_codecs*` 替换项。
- 待真机回采的推断项：触控 IC、四颗传感器型号、Mali 构建 hash/Vulkan UUID、EDID 三元组、IMEI TAC、Wi-Fi OUI、Widevine systemId（已在 notes 标注）。

```json
{
  "official": "BLU Products",
  "brand": "blu",
  "device": "G1170",
  "model": "BLU G75",
  "modelAliases": ["G75", "G1170"],
  "releaseDate": "2025-11",
  "platform": "mtk",
  "board": "mt6768",
  "soc": {
    "manufacturer": "MediaTek",
    "marketingName": "Helio G81",
    "model": "MT6769Z",
    "partNumber": "MT6769ZV/ZA",
    "processNm": 12
  },
  "display": {
    "diagonalInch": 6.88,
    "resolution": "720x1600",
    "widthPx": 720,
    "heightPx": 1600,
    "aspectRatio": "20:9",
    "widthMm": 71.7,
    "heightMm": 159.4,
    "ppi": 255,
    "xdpi": 255.0,
    "ydpi": 255.0,
    "androidDensityDpi": 320,
    "panelType": "IPS LCD",
    "refreshRatesHz": [60, 120],
    "defaultRefreshRateHz": 120,
    "wideColorGamut": false,
    "edidVendorHint": "BOE",
    "edidName": "BOE",
    "edidProductId": 2603,
    "edidWeek": 42,
    "edidYear": 2025
  },
  "cpu": {
    "smp": 8,
    "architecture": "arm64-v8a",
    "topologySummary": "6x Cortex-A55 @ 1.8GHz + 2x Cortex-A75 @ 2.0GHz",
    "driver": "mt-cpufreq",
    "governor": "sugov_ext",
    "availableGovernors": ["sugov_ext", "conservative", "powersave", "performance", "schedutil"],
    "transitionLatency": 1000,
    "scalingReadable": false,
    "packagePerCluster": false,
    "bogoMips": "26.00",
    "clusters": [
      {
        "policy": 0,
        "cpus": [0, 1, 2, 3, 4, 5],
        "coreName": "Cortex-A55",
        "minFreqKhz": 500000,
        "maxFreqKhz": 1800000,
        "curFreqKhz": 774000,
        "availableFrequenciesKhz": [
          1800000, 1625000, 1500000, 1450000, 1375000, 1325000, 1275000, 1175000,
          1100000, 1050000, 999000, 950000, 900000, 850000, 774000, 500000
        ],
        "implementer": "0x41",
        "variant": 2,
        "part": "0xd05",
        "revision": 0,
        "capacity": 495,
        "midr": "0x00000000412fd050",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 6,
        "cpus": [6, 7],
        "coreName": "Cortex-A75",
        "minFreqKhz": 850000,
        "maxFreqKhz": 2000000,
        "curFreqKhz": 1176000,
        "availableFrequenciesKhz": [
          2000000, 1950000, 1900000, 1850000, 1800000, 1710000, 1621000, 1532000,
          1443000, 1354000, 1295000, 1176000, 1087000, 998000, 909000, 850000
        ],
        "implementer": "0x41",
        "variant": 2,
        "part": "0xd0a",
        "revision": 0,
        "capacity": 1024,
        "midr": "0x00000000412fd0a0",
        "revidr": "0x0000000000000000"
      }
    ]
  },
  "gpu": {
    "vendor": "ARM",
    "renderer": "Mali-G52 MC2",
    "glesVersion": "OpenGL ES 3.2",
    "glVersionFull": "OpenGL ES 3.2 v1.r32p1-01eac0.7f3ab19c2d5e08a4ff61b37e0c9d2548",
    "glslVersion": "OpenGL ES GLSL ES 3.20",
    "glExtraExts": "GL_ARM_mali_program_binary,GL_ARM_mali_shader_binary,GL_ARM_rgba8",
    "glExtraExts2": "GL_ARM_debug_assert,GL_ARM_shader_core_properties,GL_ARM_texture_unnormalized_coordinates",
    "maxTextureSize": 8192,
    "maxCombinedTextureImageUnits": 96,
    "maxVertexAttribs": 16,
    "vulkanHardware": "mali",
    "vulkanVendorId": "0x13b5",
    "vulkanApiVersion": "1.3",
    "vulkanApiVersionHex": "0x00403116",
    "vulkanConformance": "1.3.8.0",
    "vulkanDeviceId": "0x70000002",
    "vulkanDriverId": 9,
    "vulkanDriverVersion": "0x0c401000",
    "vulkanDriverInfo": "v1.r32p1-01eac0.7f3ab19c2d5e08a4ff61b37e0c9d2548",
    "vulkanDeviceUuid": "5f1c9d0a7b3e42d18c6fa2e5b7d90c34",
    "vulkanDriverUuid": "a3d7e21c5b90462f8e17c4d6b0a9f253",
    "vulkanPipelineUuid": "9c4b7e10d2a8453fb6e09c7d1a3f5b82"
  },
  "memory": {
    "ramType": "LPDDR4X",
    "variantsGb": [4],
    "variantsMb": [4096]
  },
  "storage": {
    "storageType": "eMMC 5.1",
    "variantsGb": [256],
    "variantsManifestMb": [256000],
    "skuCombinations": ["4GB+256GB"]
  },
  "peripherals": {
    "fingerprint": "侧边电容指纹 (Goodix)",
    "touchControllerHints": ["fts_ts"],
    "keyboardDriverHint": "mtk-kpd",
    "headsetJackNameHint": "mt6768-mt6359 Headset Jack",
    "pmicKeysNameHint": "mtk-pmic-keys",
    "sensors": ["accelerometer", "magnetometer", "ambient_light", "proximity", "stepcounter", "stepdetector"],
    "sensorChips": {
      "accelerometer": "bma422_acc",
      "magnetometer": "mmc5603",
      "light": "stk333x_l",
      "proximity": "stk333x_p"
    },
    "nfc": false,
    "infrared": false,
    "audioJack35mm": true,
    "speakers": "双扬声器"
  },
  "camera": {
    "rear": "50MP (AF)",
    "front": "13MP"
  },
  "batteryAndConnectivity": {
    "batteryMah": 6000,
    "wiredChargingW": 18,
    "wirelessChargingW": 0,
    "wifi": "Wi-Fi 5 (802.11ac)",
    "wifi6Supported": false,
    "bluetooth": "5.0",
    "cellular5G": false,
    "macOuiCandidates": ["00:0c:43"],
    "widevineDefault": "L3",
    "widevineSystemId": "27141",
    "widevineVersion": "18.0.0",
    "oemCryptoApiVersion": 18,
    "imeiTac": "35874005",
    "imeiSvn": "71",
    "meidPrefix": "99002477"
  },
  "inputAlignment": {
    "touch": {
      "name": "fts_ts",
      "bus": 24,
      "vendor": 0,
      "product": 0,
      "version": 0,
      "mtSlots": 10,
      "mtPressure": true,
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
          "vendor": 0,
          "product": 0,
          "version": 0,
          "keys": ["KEY_UP", "KEY_LEFT", "KEY_RIGHT", "KEY_DOWN"],
          "abs": [{"axis": "ABS_Z", "min": 0, "max": 255, "fuzz": 0, "flat": 0}]
        },
        {
          "name": "mt6768-mt6359 Headset Jack",
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
    {"type": "battery", "base-temp-mc": 27500, "jitter-mc": 100},
    {"type": "mtktscpu", "base-temp-mc": 31500, "jitter-mc": 320},
    {"type": "mtktsAP", "base-temp-mc": 29800, "jitter-mc": 210},
    {"type": "mtktsgpu", "base-temp-mc": 30200, "jitter-mc": 260},
    {"type": "mtktswmt", "base-temp-mc": 29500, "jitter-mc": 180}
  ],
  "hardwareHals": {
    "radioVariant": "mtk",
    "enableIr": false,
    "enableNfc": false,
    "vintfDrop": []
  },
  "vendorEtcConfigs": {
    "battery": {
      "technology": "Li-poly",
      "cycleCount": 12,
      "fullUah": 6000,
      "counterMin": 5940,
      "counterMax": 5968,
      "fullMv": 4400,
      "chargeMv": 4470,
      "temperatureFullDecic": 300,
      "temperatureChargeDecic": 380,
      "currentFullUa": -65000,
      "currentChargeUa": -450000,
      "wireless": 0,
      "stateOfHealth": 100
    },
    "vibrator": {
      "motorType": "ERM 转子马达",
      "registerManager": true,
      "capabilities": 0,
      "managerCapabilities": 0,
      "compositionDelayMax": 1000,
      "compositionSizeMax": 256,
      "vibratorIds": [1],
      "effects": ["CLICK", "DOUBLE_CLICK", "TICK", "HEAVY_CLICK"]
    },
    "gnss": {
      "systemInfoName": "MTK_MNLD_default,MNL_VER_default",
      "gnssCapabilities": 991,
      "powerCapabilities": 1,
      "measurementCorrectionsCapabilities": 0,
      "yearOfHw": 2020,
      "defaultSatellites": 7,
      "dualFrequencyL5": true
    },
    "thermal": {
      "skinName": "SKIN",
      "skinCelsius": 28.6,
      "modelMaximumCelsius": 50,
      "coolingDeviceName": "devfreq-13000000.mali",
      "sensors": [
        {"id": 0, "name": "CPU", "value": 31.2, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 1, "name": "GPU", "value": 31.0, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 2, "name": "BATTERY", "value": 28.4, "thresholds": "nan;nan;nan;50;55;59;60", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 3, "name": "SKIN", "value": 28.6, "thresholds": "nan;43;45;50;70;80;90", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 5, "name": "POWER_AMPLIFIER", "value": 29.0, "thresholds": "nan;nan;nan;68;90;100;110", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 13, "name": "SOC", "value": 31.2, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"}
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
      {"handle": 1, "name": "bma422_acc", "vendor": "Bosch Sensortec", "version": 1, "typeId": 1, "type": "android.sensor.accelerometer", "maxRange": "78.453605652", "resolution": "0.002395630", "power": "0.15", "minDelay": 5000, "fifoReserved": 600, "fifoMax": 3000, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 2, "name": "mmc5603", "vendor": "Memsic", "version": 1, "typeId": 2, "type": "android.sensor.magnetic_field", "maxRange": "4912.050292969", "resolution": "0.0625", "power": "0.28", "minDelay": 10000, "fifoReserved": 600, "fifoMax": 3000, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 3, "name": "ORIENTATION", "vendor": "MTK", "version": 1, "typeId": 3, "type": "android.sensor.orientation", "maxRange": "360.0", "resolution": "0.00390625", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 5, "name": "stk333x_l", "vendor": "Sensortek", "version": 1, "typeId": 5, "type": "android.sensor.light", "maxRange": "65535.0", "resolution": "1.0", "power": "0.10", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 8, "name": "stk333x_p", "vendor": "Sensortek", "version": 1, "typeId": 8, "type": "android.sensor.proximity", "maxRange": "5.0", "resolution": "1.0", "power": "0.10", "minDelay": 0, "fifoReserved": 100, "fifoMax": 3000, "maxDelay": 1000000, "wakeUp": true, "reporting": 1},
      {"handle": 9, "name": "GRAVITY", "vendor": "MTK", "version": 1, "typeId": 9, "type": "android.sensor.gravity", "maxRange": "78.453605652", "resolution": "0.002395630", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 10, "name": "LINEARACCEL", "vendor": "MTK", "version": 1, "typeId": 10, "type": "android.sensor.linear_acceleration", "maxRange": "78.453605652", "resolution": "0.002395630", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 14, "name": "UNCALI_MAG", "vendor": "MTK", "version": 1, "typeId": 14, "type": "android.sensor.magnetic_field_uncalibrated", "maxRange": "4912.050292969", "resolution": "0.0625", "power": "0.001", "minDelay": 20000, "fifoReserved": 600, "fifoMax": 3000, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 17, "name": "SIGNIFICANT_MOTION", "vendor": "MTK", "version": 1, "typeId": 17, "type": "android.sensor.significant_motion", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 18, "name": "STEP_DETECTOR", "vendor": "MTK", "version": 1, "typeId": 18, "type": "android.sensor.step_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 100, "fifoMax": 3000, "maxDelay": 0, "wakeUp": false, "reporting": 3},
      {"handle": 19, "name": "STEP_COUNTER", "vendor": "MTK", "version": 1, "typeId": 19, "type": "android.sensor.step_counter", "maxRange": "2147483648.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 20, "name": "GEOMAGNETIC_ROTATION_VECTOR", "vendor": "MTK", "version": 1, "typeId": 20, "type": "android.sensor.geomagnetic_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 22, "name": "TILT_DETECTOR", "vendor": "MTK", "version": 1, "typeId": 22, "type": "android.sensor.tilt_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 3},
      {"handle": 27, "name": "DEVICE_ORIENTATION", "vendor": "MTK", "version": 1, "typeId": 27, "type": "android.sensor.device_orientation", "maxRange": "3.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 35, "name": "UNCALI_ACC", "vendor": "MTK", "version": 1, "typeId": 35, "type": "android.sensor.accelerometer_uncalibrated", "maxRange": "78.453605652", "resolution": "0.002395630", "power": "0.001", "minDelay": 5000, "fifoReserved": 600, "fifoMax": 3000, "maxDelay": 80000, "wakeUp": false, "reporting": 0}
    ],
    "audioEffects": [
      {"name": "Acoustic Echo Canceler", "implementor": "The Android Open Source Project", "type": "7b491460-8d4d-11e0-bd61-0002a5d5c51b", "uuid": "595f5152-3049-46b2-a2bc-7b15c65f4d4e", "connect": "Pre Processing"},
      {"name": "Automatic Gain Control V2", "implementor": "The Android Open Source Project", "type": "ae3c653b-be18-4ab8-8938-418f0a7f06ac", "uuid": "26603575-125e-4502-a000-4539e430f38f", "connect": "Pre Processing"},
      {"name": "Auxiliary Environmental Reverb", "implementor": "NXP Software Ltd.", "type": "c2e5d5f0-94bd-4763-9cac-4e234d06839e", "uuid": "4a387fc0-8ab3-11df-8bad-0002a5d5c51b", "connect": "Auxiliary"},
      {"name": "Auxiliary Preset Reverb", "implementor": "NXP Software Ltd.", "type": "47382d60-ddd8-11db-bf3a-0002a5d5c51b", "uuid": "f29a1400-a3bb-11df-8ddc-0002a5d5c51b", "connect": "Auxiliary"},
      {"name": "Dynamic Bass Boost", "implementor": "NXP Software Ltd.", "type": "0634f220-ddd4-11db-a0fc-0002a5d5c51b", "uuid": "8631f300-72e2-11df-b57e-0002a5d5c51b", "connect": "Insert"},
      {"name": "DynamicsProcessing", "implementor": "The Android Open Source Project", "type": "7261676f-6d75-7369-6364-28e2fd3ac39e", "uuid": "e0e6539b-1781-7261-676f-6d7573696340", "connect": "Insert"},
      {"name": "EqualizerBundle", "implementor": "NXP Software Ltd.", "type": "0bed4300-ddd6-11db-8f34-0002a5d5c51b", "uuid": "ce772f20-847d-11df-bb17-0002a5d5c51b", "connect": "Insert"},
      {"name": "ExtensionEffectExample", "implementor": "The Android Open Source Project", "type": "fa81dbde-588b-11ed-9b6a-0242ac120002", "uuid": "fa81dd00-588b-11ed-9b6a-0242ac120002", "connect": "Insert"},
      {"name": "Haptic Generator", "implementor": "The Android Open Source Project", "type": "1411e6d6-aecd-4021-a1cf-a6aceb0d71e5", "uuid": "97c4acd1-8b82-4f2f-832e-c2fe5d7a9931", "connect": "Insert"},
      {"name": "Insert Environmental Reverb", "implementor": "NXP Software Ltd.", "type": "c2e5d5f0-94bd-4763-9cac-4e234d06839e", "uuid": "c7a511a0-a3bb-11df-860e-0002a5d5c51b", "connect": "Insert"},
      {"name": "Insert Preset Reverb", "implementor": "NXP Software Ltd.", "type": "47382d60-ddd8-11db-bf3a-0002a5d5c51b", "uuid": "172cdf00-a3bc-11df-a72f-0002a5d5c51b", "connect": "Insert"},
      {"name": "Loudness Enhancer", "implementor": "The Android Open Source Project", "type": "fe3199be-aed0-413f-87bb-11260eb63cf1", "uuid": "fa415329-2034-4bea-b5dc-5b381c8d1e2c", "connect": "Insert"},
      {"name": "Multichannel Downmix To Stereo", "implementor": "The Android Open Source Project", "type": "381e49cc-a858-4aa2-87f6-e8388e7601b2", "uuid": "93f04452-e4fe-41cc-91f9-e475b6d1d69f", "connect": "Insert"},
      {"name": "Noise Suppression", "implementor": "The Android Open Source Project", "type": "58b4b260-8e06-11e0-aa8e-0002a5d5c51b", "uuid": "3bed20a2-5407-466c-aff0-11f7bafd5dec", "connect": "Pre Processing"},
      {"name": "Virtualizer", "implementor": "NXP Software Ltd.", "type": "37cc2c00-dddd-11db-8577-0002a5d5c51b", "uuid": "1d4033c0-8557-11df-9f2d-0002a5d5c51b", "connect": "Insert"},
      {"name": "Visualizer", "implementor": "The Android Open Source Project", "type": "e46b26a0-dddd-11db-8afd-0002a5d5c51b", "uuid": "d069d9e0-8329-11df-9168-0002a5d5c51b", "connect": "Insert"},
      {"name": "Volume", "implementor": "NXP Software Ltd.", "type": "09e8ede0-ddde-11db-b4f6-0002a5d5c51b", "uuid": "119341a0-8469-11df-81f9-0002a5d5c51b", "connect": "Insert"}
    ],
    "media": {
      "instances": [
        {"binary": "android.hardware.media.c2-mediatek-64b", "store": "default1", "rank": 1}
      ],
      "components": [
        {"name": "c2.mtk.avc.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/avc", "aliases": "OMX.MTK.VIDEO.DECODER.AVC", "store": "default1", "rank": 1},
        {"name": "c2.mtk.avc.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/avc", "aliases": "OMX.MTK.VIDEO.DECODER.AVC.secure", "store": "default1", "rank": 1},
        {"name": "c2.mtk.hevc.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/hevc", "aliases": "OMX.MTK.VIDEO.DECODER.HEVC", "store": "default1", "rank": 1},
        {"name": "c2.mtk.vp9.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/x-vnd.on2.vp9", "aliases": "OMX.MTK.VIDEO.DECODER.VP9", "store": "default1", "rank": 1},
        {"name": "c2.mtk.vpx.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/x-vnd.on2.vp8", "aliases": "OMX.MTK.VIDEO.DECODER.VPX", "store": "default1", "rank": 1},
        {"name": "c2.mtk.mpeg4.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/mp4v-es", "aliases": "OMX.MTK.VIDEO.DECODER.MPEG4", "store": "default1", "rank": 1},
        {"name": "c2.mtk.h263.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/3gpp", "aliases": "OMX.MTK.VIDEO.DECODER.H263", "store": "default1", "rank": 1},
        {"name": "c2.mtk.avc.encoder", "kind": "encoder", "domain": "video", "mediaType": "video/avc", "aliases": "OMX.MTK.VIDEO.ENCODER.AVC", "store": "default1", "rank": 1},
        {"name": "c2.mtk.hevc.encoder", "kind": "encoder", "domain": "video", "mediaType": "video/hevc", "aliases": "OMX.MTK.VIDEO.ENCODER.HEVC", "store": "default1", "rank": 1},
        {"name": "c2.mtk.mp3.decoder", "kind": "decoder", "domain": "audio", "mediaType": "audio/mpeg", "aliases": "OMX.MTK.AUDIO.DECODER.MP3", "store": "default1", "rank": 1},
        {"name": "c2.mtk.ape.decoder", "kind": "decoder", "domain": "audio", "mediaType": "audio/ape", "aliases": "OMX.MTK.AUDIO.DECODER.APE", "store": "default1", "rank": 1},
        {"name": "c2.mtk.alac.decoder", "kind": "decoder", "domain": "audio", "mediaType": "audio/alac", "aliases": "OMX.MTK.AUDIO.DECODER.ALAC", "store": "default1", "rank": 1}
      ]
    },
    "audioEngine": {
      "modules": "default,bluetooth,r_submix",
      "voiceCallIndexMax": 7,
      "musicIndexMax": 15
    }
  },
  "alignmentNotes": [
    "平台替换：gold 为 MT6833/Dimensity 6080 + Mali-G57 MC2，本机为 MT6769Z/Helio G81 + Mali-G52 MC2（板号沿用厂商 mt6768），GPU 节点地址因此为 13000000.mali（coolingDeviceName=devfreq-13000000.mali）。",
    "CPUFreq 全部取本机 DTB OPP：policy0（A55，part 0xd05，capacity 495）16 档止于 1800000kHz，policy6（A75，part 0xd0a，capacity 1024）16 档止于 2000000kHz；公开规格写 A55 1.7GHz，以固件 OPP 表 1.8GHz 为准。vendor/etc 里的 3000000/2400000 等提示频率与本机 OPP 无交集，不采用。",
    "屏幕：6.88\" IPS 720x1600 120Hz、可视区 71.7x159.4mm、320dpi；AMOLED 专属项（wideColorGamut、edidName=MTKDEV 等）一律不沿用，wideColorGamut=false，刷新率仅保留 60/120。",
    "传感器：固件 permissions 的 sensorFeatures=accelerometer/compass/light/proximity/stepcounter/stepdetector，公开规格明确无陀螺仪、无气压计，故物理层只给 accel/mag/light/prox 四颗，融合层不含 ROTATION_VECTOR/GAME_ROTATION_VECTOR/UNCALI_GYRO；BLU 为 AOSP 直出，绝对不填 xiaomi.sensor.*/oplus/vivo/samsung 私有传感器，也不填 swtp（swtp 仅小米系）。",
    "外设身份：mtk-kpd、mt6768-mt6359 Headset Jack、mtk-pmic-keys 三项为固件实测直接落库；指纹为侧边 Goodix 电容（VINTF 含 vendor.goodix...IGoodixFingerprintDaemon），节点取 uinput-goodix；触控按 MTK G81 低成本板最常见的 Focaltech fts_ts（bus 24）填写，需 adb shell getevent -pl 复核。",
    "GPU/Vulkan：glVersionFull 与 vulkanDriverInfo 按 MTK Android15 Mali DDK r32p1 基线给出（构建 hash 为占位，须以真机 glGetString 为准）；vulkanDeviceId 0x70000002、maxTextureSize 8192、maxVertexAttribs 16 为 G52 推断，非 gold 的 G57 值。",
    "Vendor 身份属性：6000mAh 低端机无 L1 认证，Widevine 定 L3（systemId 为推断占位）；EDID 厂商码/面板 ID/年份周数、IMEI TAC（35874005）、Wi-Fi OUI（00:0c:43，MTK 连接芯片池）均为 AI 推断占位，需真机回采后替换。",
    "电池/马达：6000mAh Li-poly 单电芯、18W（9V/2A）有线、无无线充；ERM 转子马达 capabilities=0，振动原语收窄为 CLICK/DOUBLE_CLICK/TICK/HEAVY_CLICK（gold 的 X 轴线性马达能力位不适用）。",
    "GNSS：MTK MNLD（固件 MNL_Config 版本 20050701.6.00.00）→ systemInfoName=MTK_MNLD_default,MNL_VER_default、yearOfHw=2020；dualFrequencyL5=true 依固件 hasL5Config 保留。",
    "虚拟 HAL 配置：radioVariant=mtk（MTK radio HAL 全套实测）、enableIr=false（hasConsumerIr=false）、enableNfc=false（hasNfc=false 且规格无 NFC）；音效 17 条 descriptor 与 Codec2 default1 组件（rank=1）全部取自固件实测，未占用 default store。",
    "存储/内存：LPDDR4X 4GB（1800MHz）+ 256GB；厂商宣传的 14GB 为 4GB 物理 + 10GB 虚拟内存扩展，不得写进 RAM 档位。"
  ],
  "sources": [
    "目标机固件实测 JSON（build.prop / boot-DTB / VINTF / vendor/etc / odm/etc，2026-02-09 构建）",
    "https://www.gsmchoice.com/en/catalogue/blu/g75/",
    "https://www.devicespecifications.com/en/model/ef5664fb",
    "https://www.bluproducts.com/devices/g75/salesguide/g75-sg.pdf",
    "redmi/gold 黄金标杆字段结构与颗粒度参考（devices/manifest/configs/redmi/gold/gold-android15/）"
  ]
}
```
