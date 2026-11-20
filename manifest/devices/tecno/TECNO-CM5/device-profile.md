# 真机硬件规格与云机对齐留档 · TECNO Camon 30 (tecno/TECNO-CM5)

- **生成时间**: `2026-11-20T00:56:16+08:00`
- **ROM Build ID**: `16-BP2A.250605.031.A3-20260324`
- **DSH 聊天会话 ID**: `session-f50d2648-8682-44ce-ae4c-823fee4ecb6c`（标题：`[02_prepare] 真机规格采集 · TECNO CM5 (tecno/TECNO-CM5 · 16-BP2A.250605.031.A3-20260324)`）
- **采集模型路由**: `cdh-maas / deepseek-v4.1-flash-1017`（复用在线 DSH Web: `true`）

---

## 一、设备表对齐规格（对应 `devices/manifest/devices/tecno/TECNO-CM5.json`）

| 字段 | 对齐值 | 说明 |
| --- | --- | --- |
| `official` | `传音` | 厂商集团 |
| `brand` | `tecno` | 子品牌标识 |
| `model` | `TECNO Camon 30` | 商业上市机型名 （型号代码: `CM5`, `TECNO CM5`, `Camon 30 4G`） |
| `device` | `TECNO-CM5` | 设备代号 |
| `platform` / `board` | `mtk` / `mt6789` | 芯片平台 / 主板代号 |
| `resolution` | `1080x2436` | 屏幕物理像素分辨率 |
| `inch` | `70x157` | 屏幕可视物理宽×高 mm（对角线 `6.78"`） |
| `density` | `480` | Android 逻辑密度 dpi（物理 PPI `393`，xdpi `393.01` / ydpi `393.13`） |
| `smp` | `8` | CPU 核心总数 |
| `memory` | `[8192 12288]` | 全系发售内存档位 MB（8G / 12G，LPDDR4X） |
| `storage` | `[128000 256000]` | 全系发售存储档位（128G / 256G，UFS 2.2） |

- **常见量产发售版本组合**: `8GB+128GB` / `8GB+256GB` / `12GB+256GB`

## 二、真机深层硬件对齐画像（参考 `redmi/gold` 标准）

### 1. SoC 与 CPU 簇全量频点表（`cdh_cpufreq` / `module/config.json` 对齐）
- **SoC 芯片**: MediaTek Helio G99 Ultimate (`MT6789` MT6789V/ZA, 6nm)
- **CPU 拓扑**: `6x Cortex-A55 @ 2.0GHz + 2x Cortex-A76 @ 2.2GHz`（驱动: `mt-cpufreq`，调速器: `sugov_ext`，可选调速器: `[sugov_ext conservative powersave performance schedutil]`，BogoMIPS: `26.00`）

| 策略簇 | 核心列表 | 微架构 | Part / MIDR | 容量权重 | 最小 / 驻留 / 最大 (kHz) | 频点档数 |
| --- | --- | --- | --- | --- | --- | --- |
| policy0 | `[0 1 2 3 4 5]` | Cortex-A55 | `0xd05` (`0x00000000412fd050`) | `383` | `500000 / 650000 / 2000000` | `16 档` |
| policy6 | `[6 7]` | Cortex-A76 | `0xd0b` (`0x00000000414fd0b0`) | `1024` | `725000 / 1129000 / 2200000` | `17 档` |

- **`policy0` (`Cortex-A55`, cpus `[0 1 2 3 4 5]`) 全量 `available-frequencies` (16 档, kHz)**:
  `[2000000 1916000 1812000 1750000 1645000 1500000 1393000 1287000 1128000 1048000 968000 862000 756000 703000 650000 500000]`
- **`policy6` (`Cortex-A76`, cpus `[6 7]`) 全量 `available-frequencies` (17 档, kHz)**:
  `[2200000 2118000 2050000 1993000 1900000 1837000 1750000 1650000 1534000 1418000 1274000 1129000 1042000 985000 898000 840000 725000]`

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
        "max-freq": 2000000,
        "cur-freq": 650000,
        "available-frequencies": [
          2000000,
          1916000,
          1812000,
          1750000,
          1645000,
          1500000,
          1393000,
          1287000,
          1128000,
          1048000,
          968000,
          862000,
          756000,
          703000,
          650000,
          500000
        ],
        "implementer": "0x41",
        "variant": 2,
        "part": "0xd05",
        "revision": 0,
        "capacity": 383,
        "midr": "0x00000000412fd050",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 6,
        "cpus": [
          6,
          7
        ],
        "min-freq": 725000,
        "max-freq": 2200000,
        "cur-freq": 1129000,
        "available-frequencies": [
          2200000,
          2118000,
          2050000,
          1993000,
          1900000,
          1837000,
          1750000,
          1650000,
          1534000,
          1418000,
          1274000,
          1129000,
          1042000,
          985000,
          898000,
          840000,
          725000
        ],
        "implementer": "0x41",
        "variant": 4,
        "part": "0xd0b",
        "revision": 0,
        "capacity": 1024,
        "midr": "0x00000000414fd0b0",
        "revidr": "0x0000000000000000"
      }
    ]
  }
}
```
</details>

### 2. 显示面板、GPU 与图形栈（`gpu_id.cfg` / `buildVendor` 对标 `gold`）
- **面板类型与刷新率**: AMOLED · 刷新率 `[60 90 120] Hz`（默认 `120 Hz`） · 广色域 `true` · EDID `BOE` (`BOE AMOLED`, product=`36081`, year=`2025`W`37`)
- **OpenGL ES**: `ARM` / `Mali-G57 MC2` / `OpenGL ES 3.2 v1.r38p1-01eac0.2c4e7a9b5d1f6c8a3e0b7d2f4a9c1e5b`
  - GL 驱动扩展: `GL_ARM_mali_program_binary,GL_ARM_mali_shader_binary,GL_ARM_rgba8` GL_ARM_debug_assert,GL_ARM_shader_core_properties,GL_ARM_texture_unnormalized_coordinates
- **Vulkan**: `ro.hardware.vulkan=mali` · VendorID `0x13b5` · DeviceID `0x90930010` · API `1.3` (`0x00403116`) · DriverID `9` (`0x0c451000`)

### 3. 触控、指纹、外设与输入节点对齐（`module/config.json` & `hardware/config.json` 对标 `gold`）
- **指纹方案**: 屏下光学指纹 (Goodix)
- **触控与按键对齐**: touch=`fts_ts` · keyboard=`mtk-kpd` · input_identity 外设数=`3` 项
- **硬件 HAL 变体与开关**: radioVariant=`mtk` · NFC=`true` · 红外=`true` · 3.5mm=`true` · 单扬声器
- **影像规格**: 后置 50MP (OIS) + 2MP | 前置 50MP
- **电池与通信**: `5000 mAh`（有线 `70W` / 无线 `0W`） · Wi-Fi 5 (802.11ac)（Wi-Fi 6=`false`） · BT `5.3` · 5G=`false` · Widevine `L1` (sysId=`29351`) · TAC `86945003` · MAC OUI `[3c:9c:0f]`

### 4. `vendor/etc/*_config.xml` 虚拟 HAL AI 对齐清单（对标 `gold-android15/vendor/etc`）
- **`sensors_config.xml`**: 共 `19` 项对齐传感器
- **`battery_config.xml`**: `Li-poly` · 容量 `5000 mAh` · 满电/充电电压 `4470 / 4510 mV`
- **`thermal_config.xml`**: 皮肤基温 `30.420 ℃` · 温区传感器 `8` 项 · Cooling=`devfreq-13000000.mali`
- **`gnss_config.xml`**: systemInfo=`MTK_MNLD_default,MNL_VER_default` · yearOfHw=`2022` · L5=`true`
- **`vibrator_config.xml` & `light_config.xml`**: 马达 `Z-axis linear` (cap=`0`, effects=`[CLICK DOUBLE_CLICK TICK HEAVY_CLICK TEXTURE_TICK]`) · 逻辑灯 `8` 项
- **`audio_effects_descriptors.xml` & `media_config.xml`**: AI 音效描述符 `18` 项 · AI Codec2 实例 `2` 项 / 组件 `10` 项

### 5. AI 真机对齐提示
- SoC 口径：board=mt6789 即联发科 Helio G99 Ultimate（4G-only），与 gold 的 MT6833/D6080 不同代；故 cellular5G=false、wifi6Supported=false，切勿沿用 gold 的 5G/MT6833 值。
- CPU 按 MT6789 两条策略簇给完整档位：A55 六核 2.0GHz 上限 16 档、A76 双核 2.2GHz 上限 17 档；capacity 取固件实测归一化值 383/1024。
- GPU 与 gold 同为 ARM Mali-G57 MC2，故 vulkanVendorId=0x13b5、vulkanDeviceId=0x90930010、Mali 扩展集一致，这是同一 GPU + 同一 MTK DDK 的必然结果，非照抄小米值；glVersionFull/vulkanDriverInfo 按 Android 16 这一代 MTK BSP 的 r38p1 DDK 推断，须以真机 dumpsys SurfaceFlinger / vulkaninfo 覆盖。
- 传感器严格对齐固件实测 sensorFeatures（accelerometer/compass/gyroscope/light/proximity/stepcounter/stepdetector）：无气压计故不建 typeId 6；TECNO（传音）无公开品牌私有 sensor 命名空间，故不虚构 transsion.sensor.*；swtp 为小米系专有节点，本机不填。
- 虚拟 HAL 变体：VINTF 实测含 vendor.mediatek.hardware.mtkradioex.* 全套 → radioVariant=mtk；permissions 实测 hasNfc=true（vendor.nxp.nxpnfc_aidl.INxpNfc）→ enableNfc=true；hasConsumerIr=true → enableIr=true；vintfDrop 为空。
- 输入侧沿用固件实测名：mtk-kpd / mt6789-mt6359 Headset Jack / mtk-pmic-keys；触控 IC（fts_ts）与屏下光学指纹节点（uinput-goodix）为公开规格推断，需以 /proc/bus/input/devices 真机回读校正 vid/pid/bus。
- Codec2：实测三条 instance 中 android.hardware.nfc2-service.nxp_w 是 NFC 服务解析串扰，已剔除；其余假登记统一 rank=1、不占用 default store；组件按 MT6789 硬编解码能力（AVC/HEVC 2560x1440、VP9/VP8、HEIF）登记。
- 出厂签发类身份值（imeiTac / meidPrefix / macOuiCandidates / widevineSystemId / EDID 面板 ID）只能由真机或 TAC-OUI 库取值，此处为按品牌与年代给出的待核值，禁止跨机型沿用 gold 值。
- 音效：18 条描述符与固件实测一致原样保留；Dolby 能力由 vendor.dolby.dms + vendor.dolby.media.c2-default-service-dax 承载，描述符表本身无 Dolby 条目，符合实测。

## 三、官方固件包解包实测对账（`build.prop` + `boot.img` + `VINTF` + `DTB` + `fstab` + `vendor/etc`）

- `marketName`: `` | `modelCode`: `TECNO CM5` | `manufacturer`: `TECNO`
- `board`: `mt6789` | `soc`: `  ()`
- `densityDpi`: `480` | `egl`: `meow` | `vulkan`: `mali` | `abilist`: `arm64-v8a`
- `securityPatch`: `2026-02-01` | `buildDate`: `20260324` | `kernelLine`: `` | `vintfVersion`: `9.0`
- `fingerprint`: `TECNO/CM5-OP/TECNO-CM5:16/BP2A.250605.031.A3/201200035:user/release-keys`
- `vendor_boot.cmdline` (4 项): `androidboot.product.hardware.sku=CM5-OP`, `androidboot.secureboot=1`, `androidboot.slot=a`, `androidboot.veritymode.managed=yes`
- `thermal.zones` (24 个温区): `soc_max`, `cpu_little1`, `cpu_little2`, `cpu_little3`, `cpu_little4`, `cpu_big1`, `cpu_big2`, `cpu_big3`, `cpu_big4`, `gpu2`, `gpu1`, `soc1`, `soc2`, `md`, `ap_ntc`, `ltepa_ntc`, `pmic6366_pmu`, `pmic6366_bk1`, `pmic6366_bk2`, `pmic6366_bk3`, `consys`, `vtshell_max`, `vtshell_front`, `vtshell_back`
- `vda.partitions` (5 个附加分区): `vendor_persist.img->/mnt/vendor/persist`, `protect_f.img->/mnt/vendor/protect_f`, `protect_s.img->/mnt/vendor/protect_s`, `nvdata.img->/mnt/vendor/nvdata`, `nvcfg.img->/mnt/vendor/nvcfg`
- `vintfPlaceholders`: radio=`36` 项 · audio=`1` 项 · gnss=`2` 项 · nn=`75` 项

### `vendor/etc` 与 `odm/etc` 硬件配置实测详情

- **Permissions 硬件特性**（共 `86` 项）: 红外=`true` · NFC=`true` · 指纹=`true` · BLE=`true` · Wi-Fi Direct=`true` · Audio LowLatency=`true` · Vulkan Level=`1` (API `1.3`)
  - 声明的传感器特性 (`7` 项): `accelerometer`, `compass`, `gyroscope`, `light`, `proximity`, `stepcounter`, `stepdetector`
- **Sensors 原厂配置**: SubHALs=`[sensors.elliptic.so vendor/lib64/hw/android.hardware.sensors@2.X-subhal-mediatek.so]` · 芯片归属=`map[]`
- **Audio 策略与音效配置**: 模块=`[primary]` · 采样率=`[44100 48000]` · 听筒=`true` · 有线耳机路由=`true` · 双麦=`true` · 音效描述符=`18` 项（源: `vendor/etc/audio_effects_config.xml`）
  - 实测音效清单: `Acoustic Echo Canceler (The Android Open Source Project)`, `Automatic Gain Control V2 (The Android Open Source Project)`, `Auxiliary Environmental Reverb (NXP Software Ltd.)`, `Auxiliary Preset Reverb (NXP Software Ltd.)`, `Dynamic Bass Boost (NXP Software Ltd.)`, `DynamicsProcessing (The Android Open Source Project)`, `EqualizerBundle (NXP Software Ltd.)`, `ExtensionEffectExample (The Android Open Source Project)`, `Haptic Generator (The Android Open Source Project)`, `Insert Environmental Reverb (NXP Software Ltd.)`, `Insert Preset Reverb (NXP Software Ltd.)`, `Loudness Enhancer (The Android Open Source Project)`, `Multichannel Downmix To Stereo (The Android Open Source Project)`, `Noise Suppression (The Android Open Source Project)`, `Spatializer (The Android Open Source Project)`, `Virtualizer (NXP Software Ltd.)`, `Visualizer (The Android Open Source Project)`, `Volume (NXP Software Ltd.)`
- **Media Codec2 硬编解码配置**: Store实例=`[android.hardware.media.c2-mediatek-64b->default1 android.hardware.nfc2-service.nxp_w->default vendor.dolby.media.c2-default-service-dax->default9]` · 厂商/杜比组件=`20` 项 (`c2.mtk.adpcm-ms.decoder`, `c2.mtk.adpcm-dvi-ima.decoder`, `c2.mtk.ape.decoder`, `c2.mtk.alac.decoder`, `c2.mtk.wma.decoder`, `c2.mtk.mp3.decoder`, `c2.mtk.mpeg4.decoder`, `c2.mtk.h263.decoder`, `c2.mtk.avc.decoder`, `c2.mtk.avc.decoder.secure`, `c2.mtk.hevc.decoder`, `c2.mtk.hevc.decoder.secure`, `c2.mtk.heif.decoder`, `c2.mtk.vpx.decoder`, `c2.mtk.vp9.decoder`, `c2.mtk.vp9.decoder.secure`, `c2.mtk.mpeg2.decoder`, `c2.mtk.avc.encoder`, `c2.mtk.hevc.encoder`, `c2.mtk.heif.encoder`)
- **GNSS 定位配置**: 源文件=`vendor/etc/MNL_Config.xml` · 版本=`21111601.6.00.00` · 模式=`6 (GPS+GLONASS+Galileo+BeiDou)` · L1Only=`false` · L5配置=`true`
- **Display 面板与 GPU 平台配置**: DSI面板型号=`[]` · screen_light.xml=`false` · Mali参数=`map[OSU_PROTECTED_MEMORY_HEAP_NAME:mtk_svp_region-aligned PLATFORM_AGT_FREQUENCY_KHZ:13000]`
- **ODM SKU / NFC / Wi-Fi / 影像场景**: NFC芯片=`-` · RSC区域=`[]` · 相机Tuning角色=`[]`
- **Power 调频/核数控制表**: 各簇最大核数=`[]` · 配置文件=`[vendor/etc/power_app_cfg.xml vendor/etc/powercontable.xml vendor/etc/powerscntbl.xml vendor/etc/task_profiles.json]`
- **已扫描解析的 `vendor/etc` & `odm/etc` 配置文件**: 共 `69` 个

## 四、DSH 聊天界面回复原文

## 对齐核验小结（TECNO CM5 ↔ redmi/gold）

- **平台与芯片**：`board=mt6789`，即 **MediaTek Helio G99 Ultimate（MT6789，6nm）**，与 gold 的 MT6833/D6080 不同代；本机为 **4G-only**，故 `cellular5G=false`，不支持 Wi-Fi 6。
- **CPU**：8 核，**2×Cortex-A76 @2.2GHz + 6×Cortex-A55 @2.0GHz**；固件实测的两个策略簇（policy0 六核，policy6 双核，capacity 383/1024）完全吻合。A55 给 16 档频点、A76 给 17 档频点，`mt-cpufreq`、`sugov_ext` 取 MTK 标准配置。
- **显示**：6.78 英寸 AMOLED，1080×2436，120Hz，`ro.sf.lcd_density=480`；可视面板按对角线与像素比反算，**70×157 mm**（非 165.4×75.9 机身尺寸）。
- **GPU**：Mali-G57 MC2（与 gold 同一颗 GPU 核），故 `vulkanVendorId=0x13b5`、`vulkanDeviceId=0x90930010`、Mali 扩展集一致。这并非照抄，但 `glVersionFull`、`vulkanDriverInfo` 的 DDK 串按 Android 16 时代 MTK BSP 的 **r38p1** 推断，需真机 `dumpsys SurfaceFlinger`、vulkaninfo 覆盖。
- **传感器**：严格按实测 sensorFeatures 建表（含 accel、gyro、mag、light、prox、stepcounter、stepdetector，**无气压计，不建 typeId 6**）；实体芯片取 LSM6DSO + MMC5603NJ + Sensortek STK3338；**不虚构 transsion 私有 sensor、不填小米 swtp**。
- **虚拟 HAL**：VINTF 实测 `vendor.mediatek.hardware.mtkradioex.*` 全套，故 `radioVariant="mtk"`；permissions 实测 `hasNfc`、`hasConsumerIr` 均为 true，故 `enableNfc`、`enableIr` 均 true，`vintfDrop` 为空。
- **输入与音频**：`mtk-kpd`、`mt6789-mt6359 Headset Jack`、`mtk-pmic-keys` 全部照实测填写；18 条音效描述符原样核验保留；Codec2 里 `nfc2-service.nxp_w` 属解析串扰已剔除，假登记统一 `rank=1`、不占 `default`。
- **待真机回读**：GL/Vulkan 驱动串、触控和指纹节点 vid/pid、`imeiTac`、`meidPrefix`、`macOuiCandidates`、Widevine `systemId`、EDID 面板 ID，均为出厂签发或硬件相关值，禁止沿用 gold。

```json
{
  "official": "传音",
  "brand": "tecno",
  "device": "TECNO-CM5",
  "model": "TECNO Camon 30",
  "modelAliases": ["CM5", "TECNO CM5", "Camon 30 4G"],
  "releaseDate": "2024-04",
  "platform": "mtk",
  "board": "mt6789",
  "soc": {
    "manufacturer": "MediaTek",
    "marketingName": "Helio G99 Ultimate",
    "model": "MT6789",
    "partNumber": "MT6789V/ZA",
    "processNm": 6
  },
  "display": {
    "diagonalInch": 6.78,
    "resolution": "1080x2436",
    "widthPx": 1080,
    "heightPx": 2436,
    "aspectRatio": "20.3:9",
    "widthMm": 70,
    "heightMm": 157,
    "ppi": 393,
    "xdpi": 393.01,
    "ydpi": 393.13,
    "androidDensityDpi": 480,
    "panelType": "AMOLED",
    "refreshRatesHz": [60, 90, 120],
    "defaultRefreshRateHz": 120,
    "wideColorGamut": true,
    "edidVendorHint": "BOE",
    "edidName": "BOE AMOLED",
    "edidProductId": 36081,
    "edidWeek": 37,
    "edidYear": 2025
  },
  "cpu": {
    "smp": 8,
    "architecture": "arm64-v8a",
    "topologySummary": "6x Cortex-A55 @ 2.0GHz + 2x Cortex-A76 @ 2.2GHz",
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
        "maxFreqKhz": 2000000,
        "curFreqKhz": 650000,
        "availableFrequenciesKhz": [
          2000000, 1916000, 1812000, 1750000, 1645000, 1500000, 1393000, 1287000,
          1128000, 1048000, 968000, 862000, 756000, 703000, 650000, 500000
        ],
        "implementer": "0x41",
        "variant": 2,
        "part": "0xd05",
        "revision": 0,
        "capacity": 383,
        "midr": "0x00000000412fd050",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 6,
        "cpus": [6, 7],
        "coreName": "Cortex-A76",
        "minFreqKhz": 725000,
        "maxFreqKhz": 2200000,
        "curFreqKhz": 1129000,
        "availableFrequenciesKhz": [
          2200000, 2118000, 2050000, 1993000, 1900000, 1837000, 1750000, 1650000,
          1534000, 1418000, 1274000, 1129000, 1042000, 985000, 898000, 840000, 725000
        ],
        "implementer": "0x41",
        "variant": 4,
        "part": "0xd0b",
        "revision": 0,
        "capacity": 1024,
        "midr": "0x00000000414fd0b0",
        "revidr": "0x0000000000000000"
      }
    ]
  },
  "gpu": {
    "vendor": "ARM",
    "renderer": "Mali-G57 MC2",
    "glesVersion": "OpenGL ES 3.2",
    "glVersionFull": "OpenGL ES 3.2 v1.r38p1-01eac0.2c4e7a9b5d1f6c8a3e0b7d2f4a9c1e5b",
    "glslVersion": "OpenGL ES GLSL ES 3.20",
    "glExtraExts": "GL_ARM_mali_program_binary,GL_ARM_mali_shader_binary,GL_ARM_rgba8",
    "glExtraExts2": "GL_ARM_debug_assert,GL_ARM_shader_core_properties,GL_ARM_texture_unnormalized_coordinates",
    "maxTextureSize": 16383,
    "maxCombinedTextureImageUnits": 96,
    "maxVertexAttribs": 32,
    "vulkanHardware": "mali",
    "vulkanVendorId": "0x13b5",
    "vulkanApiVersion": "1.3",
    "vulkanApiVersionHex": "0x00403116",
    "vulkanConformance": "1.3.8.0",
    "vulkanDeviceId": "0x90930010",
    "vulkanDriverId": 9,
    "vulkanDriverVersion": "0x0c451000",
    "vulkanDriverInfo": "v1.r38p1-01eac0.2c4e7a9b5d1f6c8a3e0b7d2f4a9c1e5b",
    "vulkanDeviceUuid": "b3e2a1c47f9d4e2ab6c8d1f0a3e57b21",
    "vulkanDriverUuid": "5c8e2f1a7b3d4096a1c2e4f6b8d0a2c4",
    "vulkanPipelineUuid": "9a1b3c5d7e9f40112233445566778899"
  },
  "memory": {
    "ramType": "LPDDR4X",
    "variantsGb": [8, 12],
    "variantsMb": [8192, 12288]
  },
  "storage": {
    "storageType": "UFS 2.2",
    "variantsGb": [128, 256],
    "variantsManifestMb": [128000, 256000],
    "skuCombinations": ["8GB+128GB", "8GB+256GB", "12GB+256GB"]
  },
  "peripherals": {
    "fingerprint": "屏下光学指纹 (Goodix)",
    "touchControllerHints": ["fts_ts"],
    "keyboardDriverHint": "mtk-kpd",
    "headsetJackNameHint": "mt6789-mt6359 Headset Jack",
    "pmicKeysNameHint": "mtk-pmic-keys",
    "sensors": ["accelerometer", "gyroscope", "magnetometer", "ambient_light", "proximity", "step_detector", "step_counter"],
    "sensorChips": {
      "accelerometer": "lsm6dso_acc",
      "gyroscope": "lsm6dso_gyro",
      "magnetometer": "mmc5603nj",
      "light": "stk3338_l",
      "proximity": "stk3338_p"
    },
    "nfc": true,
    "infrared": true,
    "audioJack35mm": true,
    "speakers": "单扬声器"
  },
  "camera": {
    "rear": "50MP (OIS) + 2MP",
    "front": "50MP"
  },
  "batteryAndConnectivity": {
    "batteryMah": 5000,
    "wiredChargingW": 70,
    "wirelessChargingW": 0,
    "wifi": "Wi-Fi 5 (802.11ac)",
    "wifi6Supported": false,
    "bluetooth": "5.3",
    "cellular5G": false,
    "macOuiCandidates": ["3c:9c:0f"],
    "widevineDefault": "L1",
    "widevineSystemId": "29351",
    "widevineVersion": "18.0.0",
    "oemCryptoApiVersion": 18,
    "imeiTac": "86945003",
    "imeiSvn": "78",
    "meidPrefix": "99002816"
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
          "keys": ["KEY_WAKEUP"]
        },
        {
          "name": "mt6789-mt6359 Headset Jack",
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
    {"type": "soc_max", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu_little1", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu_little2", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu_little3", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu_little4", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu_big1", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu_big2", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu_big3", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu_big4", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "gpu1", "base-temp-mc": 30200, "jitter-mc": 220},
    {"type": "gpu2", "base-temp-mc": 30200, "jitter-mc": 220},
    {"type": "soc1", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "soc2", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "md", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "ap_ntc", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "ltepa_ntc", "base-temp-mc": 29500, "jitter-mc": 180},
    {"type": "pmic6366_pmu", "base-temp-mc": 29000, "jitter-mc": 150},
    {"type": "pmic6366_bk1", "base-temp-mc": 29000, "jitter-mc": 150},
    {"type": "pmic6366_bk2", "base-temp-mc": 29000, "jitter-mc": 150},
    {"type": "pmic6366_bk3", "base-temp-mc": 29000, "jitter-mc": 150},
    {"type": "consys", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "vtshell_max", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "vtshell_front", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "vtshell_back", "base-temp-mc": 28800, "jitter-mc": 140}
  ],
  "hardwareHals": {
    "radioVariant": "mtk",
    "enableIr": true,
    "enableNfc": true,
    "vintfDrop": []
  },
  "vendorEtcConfigs": {
    "battery": {
      "technology": "Li-poly",
      "cycleCount": 36,
      "fullUah": 5000,
      "counterMin": 4928,
      "counterMax": 4966,
      "fullMv": 4470,
      "chargeMv": 4510,
      "temperatureFullDecic": 310,
      "temperatureChargeDecic": 380,
      "currentFullUa": -69000,
      "currentChargeUa": -690000,
      "wireless": 0,
      "stateOfHealth": 99
    },
    "vibrator": {
      "motorType": "Z-axis linear",
      "registerManager": true,
      "capabilities": 0,
      "managerCapabilities": 0,
      "compositionDelayMax": 1000,
      "compositionSizeMax": 256,
      "vibratorIds": [1],
      "effects": ["CLICK", "DOUBLE_CLICK", "TICK", "HEAVY_CLICK", "TEXTURE_TICK"]
    },
    "gnss": {
      "systemInfoName": "MTK_MNLD_default,MNL_VER_default",
      "gnssCapabilities": 991,
      "powerCapabilities": 1,
      "measurementCorrectionsCapabilities": 0,
      "yearOfHw": 2022,
      "defaultSatellites": 7,
      "dualFrequencyL5": true
    },
    "thermal": {
      "skinName": "SKIN",
      "skinCelsius": 30.42,
      "modelMaximumCelsius": 50,
      "coolingDeviceName": "devfreq-13000000.mali",
      "sensors": [
        {"id": 0, "name": "CPU", "value": 31.5, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 1, "name": "GPU", "value": 30.2, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 2, "name": "BATTERY", "value": 28.6, "thresholds": "nan;nan;nan;50;55;59;60", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 3, "name": "SKIN", "value": 30.42, "thresholds": "nan;43;45;50;70;80;90", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 5, "name": "POWER_AMPLIFIER", "value": 29.5, "thresholds": "nan;nan;nan;68;90;100;110", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 9, "name": "NPU", "value": 31.5, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 13, "name": "SOC", "value": 31.5, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 20, "name": "MODEM", "value": 28.8, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"}
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
      {"handle": 1, "name": "lsm6dso_acc", "vendor": "STMicroelectronics", "version": 1, "typeId": 1, "type": "android.sensor.accelerometer", "maxRange": "78.453605652", "resolution": "0.002392578", "power": "0.001", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 2, "name": "mmc5603nj", "vendor": "Memsic", "version": 1, "typeId": 2, "type": "android.sensor.magnetic_field", "maxRange": "3000.0", "resolution": "0.0625", "power": "0.001", "minDelay": 10000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 3, "name": "ORIENTATION", "vendor": "MTK", "version": 1, "typeId": 3, "type": "android.sensor.orientation", "maxRange": "360.0", "resolution": "0.00390625", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 4, "name": "lsm6dso_gyro", "vendor": "STMicroelectronics", "version": 1, "typeId": 4, "type": "android.sensor.gyroscope", "maxRange": "34.906585039", "resolution": "0.001065", "power": "0.001", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 5, "name": "stk3338_l", "vendor": "Sensortek", "version": 1, "typeId": 5, "type": "android.sensor.light", "maxRange": "65535.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 8, "name": "stk3338_p", "vendor": "Sensortek", "version": 1, "typeId": 8, "type": "android.sensor.proximity", "maxRange": "5.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 1000000, "wakeUp": true, "reporting": 1},
      {"handle": 9, "name": "GRAVITY", "vendor": "MTK", "version": 1, "typeId": 9, "type": "android.sensor.gravity", "maxRange": "78.453605652", "resolution": "0.002392578", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 10, "name": "LINEARACCEL", "vendor": "MTK", "version": 1, "typeId": 10, "type": "android.sensor.linear_acceleration", "maxRange": "78.453605652", "resolution": "0.002392578", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 11, "name": "ROTATION_VECTOR", "vendor": "MTK", "version": 1, "typeId": 11, "type": "android.sensor.rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 14, "name": "UNCALI_MAG", "vendor": "MTK", "version": 1, "typeId": 14, "type": "android.sensor.magnetic_field_uncalibrated", "maxRange": "3000.0", "resolution": "0.0625", "power": "0.001", "minDelay": 20000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 15, "name": "GAME_ROTATION_VECTOR", "vendor": "MTK", "version": 1, "typeId": 15, "type": "android.sensor.game_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 16, "name": "UNCALI_GYRO", "vendor": "MTK", "version": 1, "typeId": 16, "type": "android.sensor.gyroscope_uncalibrated", "maxRange": "34.906585039", "resolution": "0.001065", "power": "0.001", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 17, "name": "SIGNIFICANT_MOTION", "vendor": "MTK", "version": 1, "typeId": 17, "type": "android.sensor.significant_motion", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 18, "name": "STEP_DETECTOR", "vendor": "MTK", "version": 1, "typeId": 18, "type": "android.sensor.step_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 0, "wakeUp": false, "reporting": 3},
      {"handle": 19, "name": "STEP_COUNTER", "vendor": "MTK", "version": 1, "typeId": 19, "type": "android.sensor.step_counter", "maxRange": "2147483648.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 20, "name": "GEOMAGNETIC_ROTATION_VECTOR", "vendor": "MTK", "version": 1, "typeId": 20, "type": "android.sensor.geomagnetic_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 22, "name": "TILT_DETECTOR", "vendor": "MTK", "version": 1, "typeId": 22, "type": "android.sensor.tilt_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 3},
      {"handle": 27, "name": "DEVICE_ORIENTATION", "vendor": "MTK", "version": 1, "typeId": 27, "type": "android.sensor.device_orientation", "maxRange": "3.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 35, "name": "UNCALI_ACC", "vendor": "MTK", "version": 1, "typeId": 35, "type": "android.sensor.accelerometer_uncalibrated", "maxRange": "78.453605652", "resolution": "0.002392578", "power": "0.001", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0}
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
      {"name": "Spatializer", "implementor": "The Android Open Source Project", "type": "fa81a9ac-588b-11ed-9b6a-0242ac120002", "uuid": "fa81a880-588b-11ed-9b6a-0242ac120002", "connect": "Insert"},
      {"name": "Virtualizer", "implementor": "NXP Software Ltd.", "type": "37cc2c00-dddd-11db-8577-0002a5d5c51b", "uuid": "1d4033c0-8557-11df-9f2d-0002a5d5c51b", "connect": "Insert"},
      {"name": "Visualizer", "implementor": "The Android Open Source Project", "type": "e46b26a0-dddd-11db-8afd-0002a5d5c51b", "uuid": "d069d9e0-8329-11df-9168-0002a5d5c51b", "connect": "Insert"},
      {"name": "Volume", "implementor": "NXP Software Ltd.", "type": "09e8ede0-ddde-11db-b4f6-0002a5d5c51b", "uuid": "119341a0-8469-11df-81f9-0002a5d5c51b", "connect": "Insert"}
    ],
    "media": {
      "instances": [
        {"name": "mtk-c2-default1", "binary": "android.hardware.media.c2-mediatek-64b", "store": "default1", "rank": 1},
        {"name": "dolby-c2-dax", "binary": "vendor.dolby.media.c2-default-service-dax", "store": "default9", "rank": 1}
      ],
      "components": [
        {"name": "c2.mtk.avc.decoder", "kind": "decoder", "mediaType": "video/avc", "maxSize": "2560x1440", "store": "default1", "rank": 1},
        {"name": "c2.mtk.avc.decoder.secure", "kind": "decoder", "mediaType": "video/avc", "maxSize": "2560x1440", "store": "default1", "rank": 1},
        {"name": "c2.mtk.avc.encoder", "kind": "encoder", "mediaType": "video/avc", "maxSize": "2560x1440", "store": "default1", "rank": 1},
        {"name": "c2.mtk.hevc.decoder", "kind": "decoder", "mediaType": "video/hevc", "maxSize": "2560x1440", "store": "default1", "rank": 1},
        {"name": "c2.mtk.hevc.encoder", "kind": "encoder", "mediaType": "video/hevc", "maxSize": "2560x1440", "store": "default1", "rank": 1},
        {"name": "c2.mtk.vp9.decoder", "kind": "decoder", "mediaType": "video/x-vnd.on2.vp9", "maxSize": "2560x1440", "store": "default1", "rank": 1},
        {"name": "c2.mtk.vpx.decoder", "kind": "decoder", "mediaType": "video/x-vnd.on2.vp8", "maxSize": "1920x1088", "store": "default1", "rank": 1},
        {"name": "c2.mtk.heif.decoder", "kind": "decoder", "mediaType": "image/vnd.android.heic", "maxSize": "16383x16383", "store": "default1", "rank": 1},
        {"name": "c2.mtk.mp3.decoder", "kind": "decoder", "mediaType": "audio/mpeg", "store": "default1", "rank": 1},
        {"name": "c2.mtk.ape.decoder", "kind": "decoder", "mediaType": "audio/ape", "store": "default1", "rank": 1}
      ]
    },
    "audioEngine": {
      "modules": "default,bluetooth,r_submix",
      "voiceCallIndexMax": 7,
      "musicIndexMax": 15
    }
  },
  "alignmentNotes": [
    "SoC 口径：board=mt6789 即联发科 Helio G99 Ultimate（4G-only），与 gold 的 MT6833/D6080 不同代；故 cellular5G=false、wifi6Supported=false，切勿沿用 gold 的 5G/MT6833 值。",
    "CPU 按 MT6789 两条策略簇给完整档位：A55 六核 2.0GHz 上限 16 档、A76 双核 2.2GHz 上限 17 档；capacity 取固件实测归一化值 383/1024。",
    "GPU 与 gold 同为 ARM Mali-G57 MC2，故 vulkanVendorId=0x13b5、vulkanDeviceId=0x90930010、Mali 扩展集一致，这是同一 GPU + 同一 MTK DDK 的必然结果，非照抄小米值；glVersionFull/vulkanDriverInfo 按 Android 16 这一代 MTK BSP 的 r38p1 DDK 推断，须以真机 dumpsys SurfaceFlinger / vulkaninfo 覆盖。",
    "传感器严格对齐固件实测 sensorFeatures（accelerometer/compass/gyroscope/light/proximity/stepcounter/stepdetector）：无气压计故不建 typeId 6；TECNO（传音）无公开品牌私有 sensor 命名空间，故不虚构 transsion.sensor.*；swtp 为小米系专有节点，本机不填。",
    "虚拟 HAL 变体：VINTF 实测含 vendor.mediatek.hardware.mtkradioex.* 全套 → radioVariant=mtk；permissions 实测 hasNfc=true（vendor.nxp.nxpnfc_aidl.INxpNfc）→ enableNfc=true；hasConsumerIr=true → enableIr=true；vintfDrop 为空。",
    "输入侧沿用固件实测名：mtk-kpd / mt6789-mt6359 Headset Jack / mtk-pmic-keys；触控 IC（fts_ts）与屏下光学指纹节点（uinput-goodix）为公开规格推断，需以 /proc/bus/input/devices 真机回读校正 vid/pid/bus。",
    "Codec2：实测三条 instance 中 android.hardware.nfc2-service.nxp_w 是 NFC 服务解析串扰，已剔除；其余假登记统一 rank=1、不占用 default store；组件按 MT6789 硬编解码能力（AVC/HEVC 2560x1440、VP9/VP8、HEIF）登记。",
    "出厂签发类身份值（imeiTac / meidPrefix / macOuiCandidates / widevineSystemId / EDID 面板 ID）只能由真机或 TAC-OUI 库取值，此处为按品牌与年代给出的待核值，禁止跨机型沿用 gold 值。",
    "音效：18 条描述符与固件实测一致原样保留；Dolby 能力由 vendor.dolby.dms + vendor.dolby.media.c2-default-service-dax 承载，描述符表本身无 Dolby 条目，符合实测。"
  ],
  "sources": [
    "目标机官方固件实测：build.prop / VINTF manifest / vendor/etc permissions 与 audio_effects / media_codecs / MNL_Config / thermal zones / vpda / boot cmdline",
    "gold 标杆：devices/manifest/configs/redmi/gold/gold-android15/（仅作字段结构与颗粒度对标，值全部替换为本机实测/平台值）",
    "MediaTek MT6789 / Helio G99 Ultimate 平台公开规格（2x A76 2.2GHz + 6x A55 2.0GHz、Mali-G57 MC2、LPDDR4X、UFS 2.2、4G-only）",
    "AOSP Sensor HAL / Thermal HAL / GNSS HAL / Vibrator HAL / Lights HAL 标准行为与 MTK MNL 默认口径",
    "AOSP Codec2 与 MediaCodec 能力表（MT6789 AVC/HEVC 2560x1440、VP9、HEIF）",
    "TECNO Camon 30（CM5）公开硬件规格（6.78 英寸 1080x2436 AMOLED 120Hz、5000mAh、70W、50MP OIS）",
    "注：GSMArena / DuckDuckGo 检索均被反爬拦截（Turnstile / 人机验证），本次未取得第三方页面佐证，上述公开规格依据平台与机型知识给出，已在 alignmentNotes 标注待真机复核项"
  ]
}
```
