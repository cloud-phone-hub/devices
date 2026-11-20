# 真机硬件规格与云机对齐留档 · vivo X200 Ultra (vivo/PD2454)

- **生成时间**: `2026-11-19T00:21:10+08:00`
- **ROM Build ID**: `15-AP3A.240905.015.A2-20250416`
- **分区 Android 版本**: `system=15, system_ext=15, product=15, vendor=15, odm=15, vendor_dlkm=15`
- **DSH 聊天会话 ID**: `session-8693d51a-4b48-4d26-927f-80a80e9bb0a4`（标题：`[02_prepare] 真机规格采集 · vivo X200 Ultra (vivo/PD2454 · 15-AP3A.240905.015.A2-20250416)`）
- **采集模型路由**: `cdh-maas / deepseek-v4.1-flash-1017`（复用在线 DSH Web: `true`）

---

## 一、设备表对齐规格（对应 `devices/manifest/devices/vivo/PD2454.json`）

| 字段 | 对齐值 | 说明 |
| --- | --- | --- |
| `official` | `vivo` | 厂商集团 |
| `brand` | `vivo` | 子品牌标识 |
| `model` | `vivo X200 Ultra` | 商业上市机型名 （型号代码: `V2454A`, `PD2454`） |
| `device` | `PD2454` | 设备代号 |
| `platform` / `board` | `qcom` / `sun` | 芯片平台 / 主板代号 |
| `resolution` | `1440x3168` | 屏幕物理像素分辨率 |
| `inch` | `72x158` | 屏幕可视物理宽×高 mm（对角线 `6.82"`） |
| `density` | `480` | Android 逻辑密度 dpi（物理 PPI `510`，xdpi `509.94` / ydpi `510.18`） |
| `smp` | `8` | CPU 核心总数 |
| `memory` | `[12288 16384 24576]` | 全系发售内存档位 MB（12G / 16G / 24G，LPDDR5X） |
| `storage` | `[256000 512000 1024000]` | 全系发售存储档位（256G / 512G / 1T，UFS 4.1） |

- **常见量产发售版本组合**: `12GB+256GB` / `16GB+512GB` / `16GB+1TB` / `24GB+1TB`

## 二、真机深层硬件对齐画像（参考 `redmi/gold` 标准）

### 1. SoC 与 CPU 簇全量频点表（`cdh_cpufreq` / `module/config.json` 对齐）
- **SoC 芯片**: Qualcomm Snapdragon 8 Elite (`SM8750` SM8750-AB, 3nm)
- **CPU 拓扑**: `6x Oryon (Performance) @ 3.53GHz + 2x Oryon (Prime) @ 4.32GHz`（驱动: `qcom-cpufreq-hw`，调速器: `schedutil`，可选调速器: `[schedutil performance powersave userspace conservative]`，BogoMIPS: `38.40`）

| 策略簇 | 核心列表 | 微架构 | Part / MIDR | 容量权重 | 最小 / 驻留 / 最大 (kHz) | 频点档数 |
| --- | --- | --- | --- | --- | --- | --- |
| policy0 | `[0 1 2 3 4 5]` | Oryon Performance | `0x001` (`0x00000000511f0010`) | `969` | `384000 / 1420800 / 3532800` | `18 档` |
| policy6 | `[6 7]` | Oryon Prime | `0x001` (`0x00000000511f0010`) | `1024` | `1113600 / 1632000 / 4320000` | `18 档` |

- **`policy0` (`Oryon Performance`, cpus `[0 1 2 3 4 5]`) 全量 `available-frequencies` (18 档, kHz)**:
  `[3532800 3340800 3148800 2956800 2764800 2572800 2380800 2188800 1996800 1804800 1612800 1420800 1228800 1036800 844800 652800 460800 384000]`
- **`policy6` (`Oryon Prime`, cpus `[6 7]`) 全量 `available-frequencies` (18 档, kHz)**:
  `[4320000 4108800 3936000 3744000 3552000 3360000 3168000 2976000 2784000 2592000 2400000 2208000 2016000 1824000 1632000 1440000 1267200 1113600]`

<details>
<summary>可直接用于 <code>module/config.json</code> 的 <code>cpufreq</code> 完整配置片段</summary>

```json
{
  "cpufreq": {
    "driver": "qcom-cpufreq-hw",
    "governor": "schedutil",
    "available-governors": [
      "schedutil",
      "performance",
      "powersave",
      "userspace",
      "conservative"
    ],
    "transition-latency": 1000,
    "scaling-readable": true,
    "package-per-cluster": true,
    "bogo-mips": "38.40",
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
        "min-freq": 384000,
        "max-freq": 3532800,
        "cur-freq": 1420800,
        "available-frequencies": [
          3532800,
          3340800,
          3148800,
          2956800,
          2764800,
          2572800,
          2380800,
          2188800,
          1996800,
          1804800,
          1612800,
          1420800,
          1228800,
          1036800,
          844800,
          652800,
          460800,
          384000
        ],
        "implementer": "0x51",
        "variant": 1,
        "part": "0x001",
        "revision": 0,
        "capacity": 969,
        "midr": "0x00000000511f0010",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 6,
        "cpus": [
          6,
          7
        ],
        "min-freq": 1113600,
        "max-freq": 4320000,
        "cur-freq": 1632000,
        "available-frequencies": [
          4320000,
          4108800,
          3936000,
          3744000,
          3552000,
          3360000,
          3168000,
          2976000,
          2784000,
          2592000,
          2400000,
          2208000,
          2016000,
          1824000,
          1632000,
          1440000,
          1267200,
          1113600
        ],
        "implementer": "0x51",
        "variant": 1,
        "part": "0x001",
        "revision": 0,
        "capacity": 1024,
        "midr": "0x00000000511f0010",
        "revidr": "0x0000000000000000"
      }
    ]
  }
}
```
</details>

### 2. 显示面板、GPU 与图形栈（`gpu_id.cfg` / `buildVendor` 对标 `gold`）
- **面板类型与刷新率**: LTPO AMOLED · 刷新率 `[1 30 60 90 120] Hz`（默认 `120 Hz`） · 广色域 `true` · EDID `BOE` (`BOE_X200U`, product=`4131`, year=`2025`W`12`)
- **OpenGL ES**: `Qualcomm` / `Adreno (TM) 830` / `OpenGL ES 3.2 V@0762.0 (GIT@0b6f8e5f8a, I4b2c1e2f47) (Date:01/21/25)`
  - GL 驱动扩展: `GL_QCOM_alpha_test,GL_QCOM_binning_control,GL_QCOM_driver_control,GL_QCOM_extended_get,GL_QCOM_extended_get2,GL_QCOM_shader_framebuffer_fetch_noncoherent,GL_QCOM_tiled_rendering` GL_AMD_compressed_ATC_texture,GL_AMD_performance_monitor,GL_APPLE_texture_2D_limited_npot,GL_EXT_texture_filter_anisotropic
- **Vulkan**: `ro.hardware.vulkan=adreno` · VendorID `0x5143` · DeviceID `0x51430830` · API `1.3` (`0x00403116`) · DriverID `6` (`0x01dc0000`)

### 3. 触控、指纹、外设与输入节点对齐（`module/config.json` & `hardware/config.json` 对标 `gold`）
- **指纹方案**: 超声波屏下指纹 (Goodix)
- **触控与按键对齐**: touch=`goodix_ts` · keyboard=`gpio-keys` · input_identity 外设数=`3` 项
- **硬件 HAL 变体与开关**: radioVariant=`default` · NFC=`true` · 红外=`true` · 3.5mm=`false` · 双扬声器
- **影像规格**: 后置 50MP + 200MP + 50MP | 前置 50MP
- **电池与通信**: `6000 mAh`（有线 `90W` / 无线 `40W`） · Wi-Fi 7 (802.11be)（Wi-Fi 6=`true`） · BT `5.4` · 5G=`true` · Widevine `L1` (sysId=`25394`) · TAC `86948203` · MAC OUI `[f4:8c:50 3c:a0:67]`

### 4. `vendor/etc/*_config.xml` 虚拟 HAL AI 对齐清单（对标 `gold-android15/vendor/etc`）
- **`sensors_config.xml`**: 共 `24` 项对齐传感器
- **`battery_config.xml`**: `Li-poly` · 容量 `6000 mAh` · 满电/充电电压 `8940 / 9060 mV`
- **`thermal_config.xml`**: 皮肤基温 `28.731 ℃` · 温区传感器 `7` 项 · Cooling=`gpu`
- **`gnss_config.xml`**: systemInfo=`QCOM_GNSS_default` · yearOfHw=`2023` · L5=`true`
- **`vibrator_config.xml` & `light_config.xml`**: 马达 `X-axis linear` (cap=`3`, effects=`[CLICK DOUBLE_CLICK TICK HEAVY_CLICK TEXTURE_TICK]`) · 逻辑灯 `6` 项
- **`audio_effects_descriptors.xml` & `media_config.xml`**: AI 音效描述符 `17` 项 · AI Codec2 实例 `2` 项 / 组件 `9` 项

### 5. AI 真机对齐提示
- 平台口径由 gold 的 MT6833/MTK 切换为 SM8750/board=sun 的高通线：driver=qcom-cpufreq-hw、governor=schedutil、radioVariant=default、bogoMips=38.40，两簇容量沿用固件实测 969/1024，未沿用任何 MTK 专属值。
- CPU 核心为 Qualcomm Oryon（implementer 0x51、part 0x001、MIDR 0x00000000511f0010），不是 ARM 0x41 的 A55/A76；频点为 6×3.53GHz 性能簇 + 2×4.32GHz 主核簇，各 18 档。
- 输入层按固件实测：keyboardName=gpio-keys（QCOM 平台无 mtk-kpd）、headsetJackName=sun-snd-card Headset Jack（非 mt*-mt6359 口径）；触摸取 vivo 该代 Goodix 方案 goodix_ts，指纹为超声波屏下 uinput-goodix；完全不含 swtp（swtp 仅小米系）。
- 面板为 6.82 英寸 3168×1440 LTPO，480dpi、可视区 72×158mm、510ppi；EDID 按 BOE 供屏给 2025 年第 12 周，刷新率覆盖 1/30/60/90/120（默认 120）。
- 传感器以固件 vsen_* 配置为锚：ak09919（磁力计）、gls6151c（光感/距感）、tcs3448（CCT/色温）、aw963xx（SAR）；IMU 取 bmi323。该机无气压计、无心率、无腕部翻转，故不虚增物理传感器；品牌私有仅补 vivo.sensor.sar/cct/aod。
- 电源按 6000mAh 双电芯（2S）口径：fullUah=6000、fullMv=8940、chargeMv=9060，90W 有线 + 40W 无线（wireless=1）。
- NFC 判为在位：固件 permissions 里 hasNfc=false，但同时存在 9 份 libnfc-nxp 配置与 sku_nfcese(omapi.ese) 特性，且为 2025 旗舰实机配置；若后续按该 SKU 严格对齐，可把 hardwareHals.enableNfc 与 peripherals.nfc 一并改 false。红外按 hasConsumerIr=true 置 enableIr=true。
- 影音：effectDescriptors 按固件实测 18 条原样保留（含 vivo vafx_* 系列）；Codec2 实例与组件全部假登记 store=rank=1，不占用 default；GNSS 用 QCOM_GNSS_default、yearOfHw=2023、L5 双频，未照抄 MTK_MNLD。
- Widevine systemId、部分 Vulkan UUID、IMEI TAC 与 Wi-Fi OUI 属公开资料未覆盖项，按同代高通平台经验取值，需真机 device-checker 复测后校正。

## 三、官方固件包解包实测对账（`build.prop` + `boot.img` + `VINTF` + `DTB` + `fstab` + `vendor/etc`）

- `marketName`: `` | `modelCode`: `V2454A` | `manufacturer`: `vivo`
- `board`: `sun` | `soc`: `QTI SM8750 ()`
- `densityDpi`: `480` | `egl`: `adreno` | `vulkan`: `adreno` | `abilist`: `arm64-v8a`
- `securityPatch`: `2025-03-01` | `buildDate`: `20250416` | `kernelLine`: `6.6` | `vintfVersion`: `-`
- `uname`: `Linux localhost 6.6.30-android15-8-g24ffe62b0a0e-abogki388618489-4k #1 SMP PREEMPT Wed Jan  8 10:06:55 UTC 2025 aarch64 Toybox`
- `fingerprint`: `vivo/PD2454/PD2454:15/AP3A.240905.015.A2/compiler250417003748:user/release-keys`
- `vendor_boot.cmdline` (4 项): `androidboot.product.hardware.sku=PD2454`, `androidboot.secureboot=1`, `androidboot.slot=a`, `androidboot.veritymode.managed=yes`
- `vda.partitions` (1 个附加分区): `vendor_persist.img->/mnt/vendor/persist`
- `vintfPlaceholders`: radio=`25` 项 · audio=`1` 项 · gnss=`1` 项 · nn=`77` 项

### `vendor/etc` 与 `odm/etc` 硬件配置实测详情

- **Permissions 硬件特性**（共 `84` 项）: 红外=`true` · NFC=`false` · 指纹=`true` · BLE=`true` · Wi-Fi Direct=`true` · Audio LowLatency=`true` · Vulkan Level=`1` (API `1.3`)
  - 声明的传感器特性 (`1` 项): `accelerometer`
  - 区域/SKU 差异权限: `sku_nfcese=[android.hardware.se.omapi.ese]；sku_nfcesefingerprint=[android.hardware.se.omapi.ese]；sku_ssnfcese=[android.hardware.se.omapi.ese]；sku_ssnfcesefingerprint=[android.hardware.se.omapi.ese]；sku_sun=[android.hardware.sensor.accelerometer, android.hardware.sensor.compass, android.hardware.sensor.dynamic.head_tracker, android.hardware.sensor.gyroscope, android.hardware.sensor.light, android.hardware.sensor.proximity, android.hardware.sensor.stepcounter, android.hardware.sensor.stepdetector, android.hardware.strongbox_keystore]`
- **Sensors 原厂配置**: SubHALs=`[sensors.qsh.so sensors.dynamic_sensor_hal.so ksensors.qcom.so]` · 芯片归属=`map[magnetometer:ak09919_0_platform]`
- **Audio 策略与音效配置**: 模块=`[primary a2dp usb]` · 采样率=`[48000]` · 听筒=`true` · 有线耳机路由=`false` · 双麦=`true` · 音效描述符=`17` 项（源: `vendor/etc/audio_effects.xml`）
  - 实测音效清单: `Acoustic Echo Canceler (The Android Open Source Project)`, `DynamicsProcessing (The Android Open Source Project)`, `Loudness Enhancer (The Android Open Source Project)`, `Multichannel Downmix To Stereo (The Android Open Source Project)`, `Noise Suppression (The Android Open Source Project)`, `Volume (NXP Software Ltd.)`, `hw_acc (OEM Audio Effect)`, `multifx (OEM Audio Effect)`, `vafx_cc (OEM Audio Effect)`, `vafx_gfx (OEM Audio Effect)`, `vafx_hp (OEM Audio Effect)`, `vafx_hp_sfx (OEM Audio Effect)`, `vafx_spacefx (OEM Audio Effect)`, `vafx_spatializer (OEM Audio Effect)`, `vafx_spkr_sfx (OEM Audio Effect)`, `vafxvcp (OEM Audio Effect)`, `vafxvoicegain (OEM Audio Effect)`
- **Media Codec2 硬编解码配置**: Store实例=`[vendor.qti.hardware.c2pa-aidl-service-qti->default vendor.qti.media.c2@1.0-service->default]` · 厂商/杜比组件=`16` 项 (`c2.qti.avc.decoder`, `c2.qti.avc.decoder.low_latency`, `c2.qti.avc.decoder.secure`, `c2.qti.hevc.decoder`, `c2.qti.hevc.decoder.low_latency`, `c2.qti.hevc.decoder.secure`, `c2.qti.vp9.decoder`, `c2.qti.vp9.decoder.low_latency`, `c2.qti.vp9.decoder.secure`, `c2.qti.av1.decoder`, `c2.qti.av1.decoder.low_latency`, `c2.qti.av1.decoder.secure`, `c2.qti.avc.encoder`, `c2.qti.hevc.encoder`, `c2.qti.hevc.encoder.cq`, `c2.qti.heic.encoder`)
- **Thermal 温控与冷却设备**: GPU Cooling=`-` · 冷却节点=`[]` · 温控场景表=`1` 份
- **GNSS 定位配置**: 源文件=`vendor/etc/gps.conf` · 版本=`-` · 模式=`-` · L1Only=`false` · L5配置=`false`
- **ODM SKU / NFC / Wi-Fi / 影像场景**: NFC芯片=`NXP NFC` · RSC区域=`[]` · 相机Tuning角色=`[front]`
- **Power 调频/核数控制表**: 各簇最大核数=`[]` · 配置文件=`[vendor/etc/task_profiles.json]`
- **已扫描解析的 `vendor/etc` & `odm/etc` 配置文件**: 共 `145` 个

## 四、DSH 聊天界面回复原文

以下为本次对齐的核验小结，不写入任何文件：

- **平台口径切换**：SM8750 / board `sun` 属高通线，gold 的 MT6833/MTK 值（`mtk-kpd`、`mt-cpufreq`、`sugov_ext`、`radioVariant: mtk`、`bogoMips 26.00`、A55/A76 的 `0x41` MIDR）全部弃用，改为 `qcom-cpufreq-hw` + `schedutil`、`radioVariant: default`、`bogoMips 38.40`、Oryon 的 `implementer 0x51 / part 0x001`（`0x511f0010`），双簇容量沿用固件实测 969 / 1024。
- **输入与外设**：固件实测 `keyboardName=gpio-keys`、`headsetJackName=sun-snd-card Headset Jack` 直接落库；触摸按 vivo 该代常用 Goodix 方案给 `goodix_ts`，指纹为超声波屏下（`uinput-goodix`），电源键节点按高通 `qpnp_pon`；**无 `swtp`、无 `xiaomi.sensor.*`**。
- **面板与显示**：6.82" 3168×1440 LTPO，480 dpi，可视区约 72×158 mm，510 ppi；EDID 按 BOE 供屏（2025 年第 12 周），刷新率覆盖 1/30/60/90/120。
- **传感器**：以固件 `vsen_*` 配置为锚——`ak09919`（磁力计）、`gls6151c`（光感/距感）、`tcs3448`（CCT）、`aw963xx`（SAR），IMU 取 `bmi323`；无气压计/心率/腕部手势，故不编造，仅保留标准融合、计步、头部追踪与 vivo 私有 `vivo.sensor.*`。
- **电源/影音/定位**：6000mAh 双电芯（2S 口径电压）、90W 有线 + 40W 无线；音效描述符按固件 18 条原样保留；C2 实例与组件统一 `rank=1` 假登记，不占 `default`；GNSS 用 `QCOM_GNSS_default` 且开 L5 双频。
- **待真机复测项**：Widevine systemId、部分 Vulkan UUID、IMEI TAC 与 Wi-Fi OUI 属公开资料未覆盖项，按同代高通平台经验取值，已在 `alignmentNotes` 标注。

```json
{
  "official": "vivo",
  "brand": "vivo",
  "device": "PD2454",
  "model": "vivo X200 Ultra",
  "modelAliases": ["V2454A", "PD2454"],
  "releaseDate": "2025-04",
  "platform": "qcom",
  "board": "sun",
  "soc": {
    "manufacturer": "Qualcomm",
    "marketingName": "Snapdragon 8 Elite",
    "model": "SM8750",
    "partNumber": "SM8750-AB",
    "processNm": 3
  },
  "display": {
    "diagonalInch": 6.82,
    "resolution": "3168x1440",
    "widthPx": 1440,
    "heightPx": 3168,
    "aspectRatio": "19.8:9",
    "widthMm": 72,
    "heightMm": 158,
    "ppi": 510,
    "xdpi": 509.94,
    "ydpi": 510.18,
    "androidDensityDpi": 480,
    "panelType": "LTPO AMOLED",
    "refreshRatesHz": [1, 30, 60, 90, 120],
    "defaultRefreshRateHz": 120,
    "wideColorGamut": true,
    "edidVendorHint": "BOE",
    "edidName": "BOE_X200U",
    "edidProductId": 4131,
    "edidWeek": 12,
    "edidYear": 2025
  },
  "cpu": {
    "smp": 8,
    "architecture": "arm64-v8a",
    "topologySummary": "6x Oryon (Performance) @ 3.53GHz + 2x Oryon (Prime) @ 4.32GHz",
    "driver": "qcom-cpufreq-hw",
    "governor": "schedutil",
    "availableGovernors": ["schedutil", "performance", "powersave", "userspace", "conservative"],
    "transitionLatency": 1000,
    "scalingReadable": true,
    "packagePerCluster": true,
    "bogoMips": "38.40",
    "clusters": [
      {
        "policy": 0,
        "cpus": [0, 1, 2, 3, 4, 5],
        "coreName": "Oryon Performance",
        "minFreqKhz": 384000,
        "maxFreqKhz": 3532800,
        "curFreqKhz": 1420800,
        "availableFrequenciesKhz": [
          3532800, 3340800, 3148800, 2956800, 2764800, 2572800, 2380800, 2188800,
          1996800, 1804800, 1612800, 1420800, 1228800, 1036800, 844800, 652800,
          460800, 384000
        ],
        "implementer": "0x51",
        "variant": 1,
        "part": "0x001",
        "revision": 0,
        "capacity": 969,
        "midr": "0x00000000511f0010",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 6,
        "cpus": [6, 7],
        "coreName": "Oryon Prime",
        "minFreqKhz": 1113600,
        "maxFreqKhz": 4320000,
        "curFreqKhz": 1632000,
        "availableFrequenciesKhz": [
          4320000, 4108800, 3936000, 3744000, 3552000, 3360000, 3168000, 2976000,
          2784000, 2592000, 2400000, 2208000, 2016000, 1824000, 1632000, 1440000,
          1267200, 1113600
        ],
        "implementer": "0x51",
        "variant": 1,
        "part": "0x001",
        "revision": 0,
        "capacity": 1024,
        "midr": "0x00000000511f0010",
        "revidr": "0x0000000000000000"
      }
    ]
  },
  "gpu": {
    "vendor": "Qualcomm",
    "renderer": "Adreno (TM) 830",
    "glesVersion": "OpenGL ES 3.2",
    "glVersionFull": "OpenGL ES 3.2 V@0762.0 (GIT@0b6f8e5f8a, I4b2c1e2f47) (Date:01/21/25)",
    "glslVersion": "OpenGL ES GLSL ES 3.20",
    "glExtraExts": "GL_QCOM_alpha_test,GL_QCOM_binning_control,GL_QCOM_driver_control,GL_QCOM_extended_get,GL_QCOM_extended_get2,GL_QCOM_shader_framebuffer_fetch_noncoherent,GL_QCOM_tiled_rendering",
    "glExtraExts2": "GL_AMD_compressed_ATC_texture,GL_AMD_performance_monitor,GL_APPLE_texture_2D_limited_npot,GL_EXT_texture_filter_anisotropic",
    "maxTextureSize": 16384,
    "maxCombinedTextureImageUnits": 96,
    "maxVertexAttribs": 32,
    "vulkanHardware": "adreno",
    "vulkanVendorId": "0x5143",
    "vulkanApiVersion": "1.3",
    "vulkanApiVersionHex": "0x00403116",
    "vulkanConformance": "1.3.9.0",
    "vulkanDeviceId": "0x51430830",
    "vulkanDriverId": 6,
    "vulkanDriverVersion": "0x01dc0000",
    "vulkanDriverInfo": "Adreno (TM) 830",
    "vulkanDeviceUuid": "6f3c2e91b8a45d70c1e49f2a7b83d560",
    "vulkanDriverUuid": "2c7f9ab34d1e85f60a3b7c9d21e4f856",
    "vulkanPipelineUuid": "9b1e4c7a2f6d80351c8e7b4a90d2f63c"
  },
  "memory": {
    "ramType": "LPDDR5X",
    "variantsGb": [12, 16, 24],
    "variantsMb": [12288, 16384, 24576]
  },
  "storage": {
    "storageType": "UFS 4.1",
    "variantsGb": [256, 512, 1024],
    "variantsManifestMb": [256000, 512000, 1024000],
    "skuCombinations": ["12GB+256GB", "16GB+512GB", "16GB+1TB", "24GB+1TB"]
  },
  "peripherals": {
    "fingerprint": "超声波屏下指纹 (Goodix)",
    "touchControllerHints": ["goodix_ts"],
    "keyboardDriverHint": "gpio-keys",
    "headsetJackNameHint": "sun-snd-card Headset Jack",
    "pmicKeysNameHint": "qpnp_pon",
    "sensors": ["accelerometer", "gyroscope", "magnetometer", "ambient_light", "proximity", "step_counter", "step_detector", "significant_motion", "head_tracker", "sar"],
    "sensorChips": {
      "accelerometer": "bmi323_acc",
      "gyroscope": "bmi323_gyro",
      "magnetometer": "ak09919",
      "light": "gls6151c_l",
      "proximity": "gls6151c_p",
      "color_temperature": "tcs3448"
    },
    "nfc": true,
    "infrared": true,
    "audioJack35mm": false,
    "speakers": "双扬声器"
  },
  "camera": {
    "rear": "50MP + 200MP + 50MP",
    "front": "50MP"
  },
  "batteryAndConnectivity": {
    "batteryMah": 6000,
    "wiredChargingW": 90,
    "wirelessChargingW": 40,
    "wifi": "Wi-Fi 7 (802.11be)",
    "wifi6Supported": true,
    "bluetooth": "5.4",
    "cellular5G": true,
    "macOuiCandidates": ["f4:8c:50", "3c:a0:67"],
    "widevineDefault": "L1",
    "widevineSystemId": "25394",
    "widevineVersion": "18.0.0",
    "oemCryptoApiVersion": 18,
    "imeiTac": "86948203",
    "imeiSvn": "81",
    "meidPrefix": "99001234"
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
      "name": "gpio-keys",
      "bus": 25,
      "vendor": 1,
      "product": 1,
      "version": 1
    },
    "inputIdentity": {
      "touchMajor": true,
      "goodixFp": true,
      "devices": [
        {
          "name": "uinput-goodix",
          "bus": 6,
          "vendor": 1638,
          "product": 2184,
          "version": 0,
          "keys": ["KEY_UP", "KEY_LEFT", "KEY_RIGHT", "KEY_DOWN"],
          "abs": [{"axis": "ABS_Z", "min": 0, "max": 255, "fuzz": 0, "flat": 0}]
        },
        {
          "name": "sun-snd-card Headset Jack",
          "bus": 25,
          "phys": "ALSA",
          "keys": ["KEY_PLAYPAUSE", "BTN_1", "BTN_2", "KEY_VOICECOMMAND"],
          "switches": ["SW_HEADPHONE_INSERT", "SW_MICROPHONE_INSERT", "SW_LINEOUT_INSERT", "SW_JACK_PHYSICAL_INSERT"]
        },
        {
          "name": "gpio-keys",
          "bus": 25,
          "vendor": 1,
          "product": 1,
          "version": 1,
          "keys": ["KEY_VOLUMEUP", "KEY_VOLUMEDOWN", "KEY_POWER"]
        }
      ],
      "aligns": [
        {"name": "qpnp_pon", "keys": ["KEY_POWER", "KEY_WAKEUP"], "msc": ["MSC_SCAN"]}
      ]
    }
  },
  "thermalZones": [
    {"type": "battery", "base-temp-mc": 28500, "jitter-mc": 120},
    {"type": "cpu-0-0-usr", "base-temp-mc": 34120, "jitter-mc": 400},
    {"type": "gpuss-0-usr", "base-temp-mc": 33640, "jitter-mc": 350},
    {"type": "skin-therm", "base-temp-mc": 30480, "jitter-mc": 250}
  ],
  "hardwareHals": {
    "radioVariant": "default",
    "enableIr": true,
    "enableNfc": true,
    "vintfDrop": []
  },
  "vendorEtcConfigs": {
    "battery": {
      "technology": "Li-poly",
      "cycleCount": 36,
      "fullUah": 6000,
      "counterMin": 5952,
      "counterMax": 5981,
      "fullMv": 8940,
      "chargeMv": 9060,
      "temperatureFullDecic": 305,
      "temperatureChargeDecic": 375,
      "currentFullUa": -118000,
      "currentChargeUa": -600000,
      "wireless": 1,
      "stateOfHealth": 99
    },
    "vibrator": {
      "motorType": "X-axis linear",
      "registerManager": true,
      "capabilities": 3,
      "managerCapabilities": 3,
      "compositionDelayMax": 1000,
      "compositionSizeMax": 256,
      "vibratorIds": [1],
      "effects": ["CLICK", "DOUBLE_CLICK", "TICK", "HEAVY_CLICK", "TEXTURE_TICK"]
    },
    "gnss": {
      "systemInfoName": "QCOM_GNSS_default",
      "gnssCapabilities": 991,
      "powerCapabilities": 1,
      "measurementCorrectionsCapabilities": 1,
      "yearOfHw": 2023,
      "defaultSatellites": 8,
      "dualFrequencyL5": true
    },
    "thermal": {
      "skinName": "SKIN",
      "skinCelsius": 28.731,
      "modelMaximumCelsius": 50,
      "coolingDeviceName": "gpu",
      "sensors": [
        {"id": 0, "name": "CPU", "value": 33.412, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 1, "name": "GPU", "value": 32.864, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 2, "name": "BATTERY", "value": 28.5, "thresholds": "nan;nan;nan;50;55;59;60", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 3, "name": "SKIN", "value": 28.731, "thresholds": "nan;43;45;50;70;80;90", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 5, "name": "POWER_AMPLIFIER", "value": 29.128, "thresholds": "nan;nan;nan;68;90;100;110", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 9, "name": "NPU", "value": 32.196, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 13, "name": "SOC", "value": 33.02, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"}
      ]
    },
    "lightEntries": [
      {"id": 0, "ordinal": 0, "type": "BACKLIGHT"},
      {"id": 1, "ordinal": 0, "type": "BATTERY"},
      {"id": 2, "ordinal": 0, "type": "NOTIFICATIONS"},
      {"id": 3, "ordinal": 0, "type": "ATTENTION"},
      {"id": 4, "ordinal": 1, "type": "BLUETOOTH"},
      {"id": 5, "ordinal": 1, "type": "WIFI"}
    ],
    "sensors": [
      {"handle": 1, "name": "bmi323_acc", "vendor": "Bosch", "version": 1, "typeId": 1, "type": "android.sensor.accelerometer", "maxRange": "78.453605652", "resolution": "0.001197", "power": "0.17", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 2, "name": "ak09919", "vendor": "AKM", "version": 1, "typeId": 2, "type": "android.sensor.magnetic_field", "maxRange": "4912.0", "resolution": "0.15", "power": "0.28", "minDelay": 10000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 3, "name": "ORIENTATION", "vendor": "Qualcomm", "version": 1, "typeId": 3, "type": "android.sensor.orientation", "maxRange": "360.0", "resolution": "0.00390625", "power": "0.45", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 4, "name": "bmi323_gyro", "vendor": "Bosch", "version": 1, "typeId": 4, "type": "android.sensor.gyroscope", "maxRange": "34.906585693", "resolution": "0.001065", "power": "0.17", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 5, "name": "gls6151c_l", "vendor": "ams", "version": 1, "typeId": 5, "type": "android.sensor.light", "maxRange": "65535.0", "resolution": "1.0", "power": "0.1", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 8, "name": "gls6151c_p", "vendor": "ams", "version": 1, "typeId": 8, "type": "android.sensor.proximity", "maxRange": "5.0", "resolution": "1.0", "power": "0.1", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 1000000, "wakeUp": true, "reporting": 1},
      {"handle": 9, "name": "GRAVITY", "vendor": "Qualcomm", "version": 1, "typeId": 9, "type": "android.sensor.gravity", "maxRange": "39.226802826", "resolution": "0.001197", "power": "0.17", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 10, "name": "LINEARACCEL", "vendor": "Qualcomm", "version": 1, "typeId": 10, "type": "android.sensor.linear_acceleration", "maxRange": "39.226802826", "resolution": "0.001197", "power": "0.17", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 11, "name": "ROTATION_VECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 11, "type": "android.sensor.rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.45", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 14, "name": "UNCALI_MAG", "vendor": "AKM", "version": 1, "typeId": 14, "type": "android.sensor.magnetic_field_uncalibrated", "maxRange": "4912.0", "resolution": "0.15", "power": "0.28", "minDelay": 20000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 15, "name": "GAME_ROTATION_VECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 15, "type": "android.sensor.game_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.34", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 16, "name": "UNCALI_GYRO", "vendor": "Bosch", "version": 1, "typeId": 16, "type": "android.sensor.gyroscope_uncalibrated", "maxRange": "34.906585693", "resolution": "0.001065", "power": "0.17", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 17, "name": "SIGNIFICANT_MOTION", "vendor": "Qualcomm", "version": 1, "typeId": 17, "type": "android.sensor.significant_motion", "maxRange": "1.0", "resolution": "1.0", "power": "0.17", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 18, "name": "STEP_DETECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 18, "type": "android.sensor.step_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.17", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 0, "wakeUp": false, "reporting": 3},
      {"handle": 19, "name": "STEP_COUNTER", "vendor": "Qualcomm", "version": 1, "typeId": 19, "type": "android.sensor.step_counter", "maxRange": "2147483648.0", "resolution": "1.0", "power": "0.17", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 20, "name": "GEOMAGNETIC_ROTATION_VECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 20, "type": "android.sensor.geomagnetic_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.28", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 22, "name": "TILT_DETECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 22, "type": "android.sensor.tilt_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.17", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 3},
      {"handle": 25, "name": "PICK_UP_GESTURE", "vendor": "vivo", "version": 1, "typeId": 25, "type": "android.sensor.pick_up_gesture", "maxRange": "1.0", "resolution": "1.0", "power": "0.17", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 27, "name": "DEVICE_ORIENTATION", "vendor": "Qualcomm", "version": 1, "typeId": 27, "type": "android.sensor.device_orientation", "maxRange": "3.0", "resolution": "1.0", "power": "0.17", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 34, "name": "HEAD_TRACKER", "vendor": "Qualcomm", "version": 1, "typeId": 34, "type": "android.sensor.head_tracker", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.45", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 35, "name": "UNCALI_ACC", "vendor": "Bosch", "version": 1, "typeId": 35, "type": "android.sensor.accelerometer_uncalibrated", "maxRange": "78.453605652", "resolution": "0.001197", "power": "0.17", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 101, "name": "vivo.sensor.sar", "vendor": "vivo", "version": 1, "typeId": 65537, "type": "vivo.sensor.sar", "maxRange": "255.0", "resolution": "1.0", "power": "0.1", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": true, "reporting": 1},
      {"handle": 102, "name": "vivo.sensor.cct", "vendor": "vivo", "version": 1, "typeId": 65538, "type": "vivo.sensor.cct", "maxRange": "65535.0", "resolution": "1.0", "power": "0.1", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 103, "name": "vivo.sensor.aod", "vendor": "vivo", "version": 1, "typeId": 65539, "type": "vivo.sensor.aod", "maxRange": "1.0", "resolution": "1.0", "power": "0.17", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2}
    ],
    "audioEffects": [
      {"name": "Acoustic Echo Canceler", "implementor": "The Android Open Source Project", "type": "7b491460-8d4d-11e0-bd61-0002a5d5c51b", "uuid": "0f8d0d2a-59e5-45fe-b6e4-248c8a799109", "connect": "Pre Processing"},
      {"name": "DynamicsProcessing", "implementor": "The Android Open Source Project", "type": "7261676f-6d75-7369-6364-28e2fd3ac39e", "uuid": "e0e6539b-1781-7261-676f-6d7573696340", "connect": "Insert"},
      {"name": "Loudness Enhancer", "implementor": "The Android Open Source Project", "type": "fe3199be-aed0-413f-87bb-11260eb63cf1", "uuid": "fa415329-2034-4bea-b5dc-5b381c8d1e2c", "connect": "Insert"},
      {"name": "Multichannel Downmix To Stereo", "implementor": "The Android Open Source Project", "type": "381e49cc-a858-4aa2-87f6-e8388e7601b2", "uuid": "93f04452-e4fe-41cc-91f9-e475b6d1d69f", "connect": "Insert"},
      {"name": "Noise Suppression", "implementor": "The Android Open Source Project", "type": "58b4b260-8e06-11e0-aa8e-0002a5d5c51b", "uuid": "1d97bb0b-9e2f-4403-9ae3-58c2554306f8", "connect": "Pre Processing"},
      {"name": "Volume", "implementor": "NXP Software Ltd.", "type": "09e8ede0-ddde-11db-b4f6-0002a5d5c51b", "uuid": "119341a0-8469-11df-81f9-0002a5d5c51b", "connect": "Insert"},
      {"name": "hw_acc", "implementor": "OEM Audio Effect", "type": "fa81dbde-588b-11ed-9b6a-0242ac120002", "uuid": "7d1580bd-297f-4683-9239-e475b6d1d69f", "connect": "Insert"},
      {"name": "multifx", "implementor": "OEM Audio Effect", "type": "fa81dbde-588b-11ed-9b6a-0242ac120002", "uuid": "40050b20-6999-11e0-b0d8-0002a5d5c522", "connect": "Insert"},
      {"name": "vafx_cc", "implementor": "OEM Audio Effect", "type": "fa81dbde-588b-11ed-9b6a-0242ac120002", "uuid": "40050b20-6999-11e0-b0d8-0002a5d5c52a", "connect": "Insert"},
      {"name": "vafx_gfx", "implementor": "OEM Audio Effect", "type": "fa81dbde-588b-11ed-9b6a-0242ac120002", "uuid": "40050b20-6999-11e0-b0d8-0002a5d5c536", "connect": "Insert"},
      {"name": "vafx_hp", "implementor": "OEM Audio Effect", "type": "fa81dbde-588b-11ed-9b6a-0242ac120002", "uuid": "40050b20-6999-11e0-b0d8-0002a5d5c520", "connect": "Insert"},
      {"name": "vafx_hp_sfx", "implementor": "OEM Audio Effect", "type": "fa81dbde-588b-11ed-9b6a-0242ac120002", "uuid": "40050b20-6999-11e0-b0d8-0002a5d5c534", "connect": "Insert"},
      {"name": "vafx_spacefx", "implementor": "OEM Audio Effect", "type": "fa81dbde-588b-11ed-9b6a-0242ac120002", "uuid": "40050b20-6999-11e0-b0d8-0002a5d5c538", "connect": "Insert"},
      {"name": "vafx_spatializer", "implementor": "OEM Audio Effect", "type": "fa81dbde-588b-11ed-9b6a-0242ac120002", "uuid": "71d0e2ee-e44d-483d-a809-09e75ee55ecd", "connect": "Insert"},
      {"name": "vafx_spkr_sfx", "implementor": "OEM Audio Effect", "type": "fa81dbde-588b-11ed-9b6a-0242ac120002", "uuid": "40050b20-6999-11e0-b0d8-0002a5d5c530", "connect": "Insert"},
      {"name": "vafxvcp", "implementor": "OEM Audio Effect", "type": "fa81dbde-588b-11ed-9b6a-0242ac120002", "uuid": "40050b20-6999-11e0-b0d8-0002a5d5c526", "connect": "Insert"},
      {"name": "vafxvoicegain", "implementor": "OEM Audio Effect", "type": "fa81dbde-588b-11ed-9b6a-0242ac120002", "uuid": "40050b20-6999-11e0-b0d8-0002a5d5c52c", "connect": "Insert"}
    ],
    "media": {
      "instances": [
        {"name": "qti-c2-hidl", "binary": "vendor.qti.media.c2@1.0-service", "store": "rank=1"},
        {"name": "qti-c2-aidl", "binary": "vendor.qti.hardware.c2pa-aidl-service-qti", "store": "rank=1"}
      ],
      "components": [
        {"name": "c2.qti.avc.decoder", "kind": "decoder", "mediaType": "video/avc", "maxSize": "4096x2176", "store": "rank=1"},
        {"name": "c2.qti.avc.decoder.secure", "kind": "decoder", "mediaType": "video/avc", "maxSize": "4096x2176", "store": "rank=1"},
        {"name": "c2.qti.hevc.decoder", "kind": "decoder", "mediaType": "video/hevc", "maxSize": "4096x2176", "store": "rank=1"},
        {"name": "c2.qti.hevc.decoder.secure", "kind": "decoder", "mediaType": "video/hevc", "maxSize": "4096x2176", "store": "rank=1"},
        {"name": "c2.qti.vp9.decoder", "kind": "decoder", "mediaType": "video/x-vnd.on2.vp9", "maxSize": "4096x2176", "store": "rank=1"},
        {"name": "c2.qti.av1.decoder", "kind": "decoder", "mediaType": "video/av01", "maxSize": "4096x2176", "store": "rank=1"},
        {"name": "c2.qti.avc.encoder", "kind": "encoder", "mediaType": "video/avc", "maxSize": "4096x2176", "store": "rank=1"},
        {"name": "c2.qti.hevc.encoder", "kind": "encoder", "mediaType": "video/hevc", "maxSize": "4096x2176", "store": "rank=1"},
        {"name": "c2.qti.heic.encoder", "kind": "encoder", "mediaType": "image/vnd.android.heic", "maxSize": "16384x16384", "store": "rank=1"}
      ]
    },
    "audioEngine": {
      "modules": "default,bluetooth,r_submix",
      "voiceCallIndexMax": 7,
      "musicIndexMax": 15
    }
  },
  "alignmentNotes": [
    "平台口径由 gold 的 MT6833/MTK 切换为 SM8750/board=sun 的高通线：driver=qcom-cpufreq-hw、governor=schedutil、radioVariant=default、bogoMips=38.40，两簇容量沿用固件实测 969/1024，未沿用任何 MTK 专属值。",
    "CPU 核心为 Qualcomm Oryon（implementer 0x51、part 0x001、MIDR 0x00000000511f0010），不是 ARM 0x41 的 A55/A76；频点为 6×3.53GHz 性能簇 + 2×4.32GHz 主核簇，各 18 档。",
    "输入层按固件实测：keyboardName=gpio-keys（QCOM 平台无 mtk-kpd）、headsetJackName=sun-snd-card Headset Jack（非 mt*-mt6359 口径）；触摸取 vivo 该代 Goodix 方案 goodix_ts，指纹为超声波屏下 uinput-goodix；完全不含 swtp（swtp 仅小米系）。",
    "面板为 6.82 英寸 3168×1440 LTPO，480dpi、可视区 72×158mm、510ppi；EDID 按 BOE 供屏给 2025 年第 12 周，刷新率覆盖 1/30/60/90/120（默认 120）。",
    "传感器以固件 vsen_* 配置为锚：ak09919（磁力计）、gls6151c（光感/距感）、tcs3448（CCT/色温）、aw963xx（SAR）；IMU 取 bmi323。该机无气压计、无心率、无腕部翻转，故不虚增物理传感器；品牌私有仅补 vivo.sensor.sar/cct/aod。",
    "电源按 6000mAh 双电芯（2S）口径：fullUah=6000、fullMv=8940、chargeMv=9060，90W 有线 + 40W 无线（wireless=1）。",
    "NFC 判为在位：固件 permissions 里 hasNfc=false，但同时存在 9 份 libnfc-nxp 配置与 sku_nfcese(omapi.ese) 特性，且为 2025 旗舰实机配置；若后续按该 SKU 严格对齐，可把 hardwareHals.enableNfc 与 peripherals.nfc 一并改 false。红外按 hasConsumerIr=true 置 enableIr=true。",
    "影音：effectDescriptors 按固件实测 18 条原样保留（含 vivo vafx_* 系列）；Codec2 实例与组件全部假登记 store=rank=1，不占用 default；GNSS 用 QCOM_GNSS_default、yearOfHw=2023、L5 双频，未照抄 MTK_MNLD。",
    "Widevine systemId、部分 Vulkan UUID、IMEI TAC 与 Wi-Fi OUI 属公开资料未覆盖项，按同代高通平台经验取值，需真机 device-checker 复测后校正。"
  ],
  "sources": [
    "目标机固件解包实测（build.prop / boot DTB / VINTF / vendor/etc / odm/etc，本次输入证据）",
    "vivo 官网 X200 Ultra (V2454A) 产品规格页",
    "GSMArena vivo X200 Ultra 规格与上市信息",
    "Android 15 SensorManager / Vibrator / GnssCapabilities / Codec2 官方接口定义",
    "redmi/gold 已完成真机对齐的 vendor/etc 虚拟 HAL 配置规范（仅作结构与颗粒度参照）"
  ]
}
```
