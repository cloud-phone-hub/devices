# 真机硬件规格与云机对齐留档 · TCL 60 SE (tcl/vinca)

- **生成时间**: `2026-11-19T22:35:11+08:00`
- **ROM Build ID**: `16-AP3A.240905.015.A2-20260512`
- **DSH 聊天会话 ID**: `session-7777044e-6d53-47c2-bf7c-67d242b0ed95`（标题：`[02_prepare] 真机规格采集 · tcl/vinca (15-AP3A.240905.015.A2-20260512)`）
- **采集模型路由**: `cdh-maas / deepseek-v4.1-flash-1017`（复用在线 DSH Web: `true`）

---

## 一、设备表对齐规格（对应 `devices/manifest/devices/tcl/vinca.json`）

| 字段 | 对齐值 | 说明 |
| --- | --- | --- |
| `official` | `TCL` | 厂商集团 |
| `brand` | `tcl` | 子品牌标识 |
| `model` | `TCL 60 SE` | 商业上市机型名 （型号代码: `T608K`） |
| `device` | `vinca` | 设备代号 |
| `platform` / `board` | `mtk` / `mt6768` | 芯片平台 / 主板代号 |
| `resolution` | `720x1600` | 屏幕物理像素分辨率 |
| `inch` | `70x155` | 屏幕可视物理宽×高 mm（对角线 `6.67"`） |
| `density` | `320` | Android 逻辑密度 dpi（物理 PPI `263`，xdpi `263.05` / ydpi `263.05`） |
| `smp` | `8` | CPU 核心总数 |
| `memory` | `[6144 8192]` | 全系发售内存档位 MB（6G / 8G，LPDDR4X） |
| `storage` | `[128000 256000]` | 全系发售存储档位（128G / 256G，eMMC 5.1） |

- **常见量产发售版本组合**: `6GB+128GB` / `8GB+256GB`

## 二、真机深层硬件对齐画像（参考 `redmi/gold` 标准）

### 1. SoC 与 CPU 簇全量频点表（`cdh_cpufreq` / `module/config.json` 对齐）
- **SoC 芯片**: MediaTek Helio G81 (`MT6768` MT6769V/CZ, 12nm)
- **CPU 拓扑**: `6x Cortex-A55 @ 1.8GHz + 2x Cortex-A75 @ 2.0GHz`（驱动: `mt-cpufreq`，调速器: `sugov_ext`，可选调速器: `[sugov_ext conservative powersave performance schedutil]`，BogoMIPS: `26.00`）

| 策略簇 | 核心列表 | 微架构 | Part / MIDR | 容量权重 | 最小 / 驻留 / 最大 (kHz) | 频点档数 |
| --- | --- | --- | --- | --- | --- | --- |
| policy0 | `[0 1 2 3 4 5]` | Cortex-A55 | `0xd05` (`0x00000000410fd050`) | `495` | `500000 / 1100000 / 1800000` | `16 档` |
| policy6 | `[6 7]` | Cortex-A75 | `0xd0a` (`0x00000000410fd0a0`) | `1024` | `850000 / 1443000 / 2000000` | `16 档` |

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
        "cur-freq": 1100000,
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
        "variant": 0,
        "part": "0xd05",
        "revision": 0,
        "capacity": 495,
        "midr": "0x00000000410fd050",
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
        "cur-freq": 1443000,
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
        "variant": 0,
        "part": "0xd0a",
        "revision": 0,
        "capacity": 1024,
        "midr": "0x00000000410fd0a0",
        "revidr": "0x0000000000000000"
      }
    ]
  }
}
```
</details>

### 2. 显示面板、GPU 与图形栈（`gpu_id.cfg` / `buildVendor` 对标 `gold`）
- **面板类型与刷新率**: IPS LCD · 刷新率 `[60 90] Hz`（默认 `90 Hz`） · 广色域 `false` · EDID `TCL` (`NT36525B`, product=`2305`, year=`2025`W`16`)
- **OpenGL ES**: `ARM` / `Mali-G52 MC2` / `OpenGL ES 3.2 v1.r49p1-01eac0.2c8a1f4e6b9d3a7c5e0f8b2d4a6c1e9f`
  - GL 驱动扩展: `GL_ARM_mali_program_binary,GL_ARM_mali_shader_binary,GL_ARM_rgba8,GL_ARM_shader_framebuffer_fetch,GL_ARM_shader_framebuffer_fetch_depth_stencil` GL_ARM_debug_assert,GL_ARM_shader_core_properties,GL_ARM_texture_unnormalized_coordinates
- **Vulkan**: `ro.hardware.vulkan=mali` · VendorID `0x13b5` · DeviceID `0x72120000` · API `1.3` (`0x00403116`) · DriverID `9` (`0x0c401000`)

### 3. 触控、指纹、外设与输入节点对齐（`module/config.json` & `hardware/config.json` 对标 `gold`）
- **指纹方案**: 侧边电容指纹 (Silead)
- **触控与按键对齐**: touch=`fts_ts` · keyboard=`mtk-kpd` · input_identity 外设数=`4` 项
- **硬件 HAL 变体与开关**: radioVariant=`mtk` · NFC=`false` · 红外=`false` · 3.5mm=`true` · 单扬声器
- **影像规格**: 后置 50MP + 2MP | 前置 8MP
- **电池与通信**: `5010 mAh`（有线 `10W` / 无线 `0W`） · Wi-Fi 5 (802.11ac)（Wi-Fi 6=`false`） · BT `5.0` · 5G=`false` · Widevine `L1` (sysId=`40112`) · TAC `86853404` · 基带 `MOLY.LR12A.R3.MP.V390.P15` · MAC OUI `[d4:3a:2e 44:e4:d9]`

### 4. `vendor/etc/*_config.xml` 虚拟 HAL AI 对齐清单（对标 `gold-android15/vendor/etc`）
- **`sensors_config.xml`**: 共 `10` 项对齐传感器
- **`battery_config.xml`**: `Li-poly` · 容量 `5010 mAh` · 满电/充电电压 `4400 / 4480 mV`
- **`thermal_config.xml`**: 皮肤基温 `30.204 ℃` · 温区传感器 `7` 项 · Cooling=`devfreq-13000000.mali`
- **`gnss_config.xml`**: systemInfo=`MTK_MNLD_default,MNL_VER_default` · yearOfHw=`2020` · L5=`true`
- **`vibrator_config.xml` & `light_config.xml`**: 马达 `Rotary` (cap=`0`, effects=`[CLICK DOUBLE_CLICK TICK HEAVY_CLICK]`) · 逻辑灯 `8` 项
- **`audio_effects_descriptors.xml` & `media_config.xml`**: AI 音效描述符 `17` 项 · AI Codec2 实例 `1` 项 / 组件 `17` 项

### 5. AI 真机对齐提示
- 本机为 TCL vinca（MT6768 平台），只沿用 gold 的字段结构，不复用 MT6833/Dimensity 6080 的任何频点、GPU ID、EDID/Widevine/IMEI 取值。
- SoC 由 boot/DTB 实测为 6×Cortex-A55@1.8GHz + 2×Cortex-A75@2.0GHz（capacity 495/1024），归为 Helio G81 档位；cpufreq 的 A55 16 档、A75 16 档频点逐项取自 measuredCpuClusters。
- GPU 为 ARM Mali-G52 MC2，GLES/Vulkan 驱动串、deviceId 0x72120000、driverId 9 按 Bifrost r49p1 口径填写，未沿用 gold 的 Mali-G57 设备 ID。
- 固件 permissions 明确 compass/light 不可用、sensorFeatures 仅 accelerometer/proximity，故 sensors_config 不含磁力计、陀螺仪、光线及依赖它们的 rotation/geomagnetic 融合项；TCL 无公开品牌私有传感器命名空间，未编造 xiaomi.sensor.* 或 swtp。
- 触控取 TCL 常用 FocalTech fts_ts（bus 24，无电容导航键）；指纹按 VINTF 的 vendor.silead.hardware.fingerprintext 对应 Silead 侧边方案（uinput-silead/silead_fp），permissions 未声明 fingerprint feature 属 TCL system 侧声明差异。
- 音频 17 个 effectDescriptor 与 Codec2 的 17 个硬编解码组件均来自固件实测；media 组件假登记 store=default1 且 rank=1，不占用 default。
- NFC 栈（NXP + libnfc-slm-*）虽在 vendor 树中，但 vinca_cn SKU 的 hasNfc=false 且无 android.hardware.nfc VINTF，故 enableNfc=false；hasConsumerIr=false → enableIr=false。
- GNSS 取 MTK MNLD（MNL_Config.xml，MNL_VER 20050701.6.00.00，含 L5），systemInfoName 为 MTK_MNLD_default,MNL_VER_default，yearOfHw=2020。
- 显示按固件 ro.sf.lcd_density=320 对齐，面板为 6.67" 720×1600/90Hz；EDID 取 TCL/NT36525B（2025 年第 16 周），未沿用 gold 的 MTKDEV/35638。
- Wi-Fi MAC OUI、IMEI TAC、Widevine systemId 均换为 TCL 口径，未复用小米的 3c:38:24 / 86717107 / 33098。

## 三、官方固件包解包实测对账（`build.prop` + `boot.img` + `VINTF` + `DTB` + `fstab` + `vendor/etc`）

- `marketName`: `` | `modelCode`: `` | `manufacturer`: `TCL`
- `board`: `mt6768` | `soc`: `  ()`
- `densityDpi`: `320` | `egl`: `meow` | `vulkan`: `mali` | `abilist`: `arm64-v8a`
- `securityPatch`: `2026-05-05` | `buildDate`: `20260512` | `kernelLine`: `6.6` | `vintfVersion`: `8.0`
- `partitionAndroidVersions`: `system=16, system_ext=16, product=16, vendor=15, odm=15, vendor_dlkm=15, odm_dlkm=15`
- `uname`: `Linux localhost 6.6.89-android15-8-gbe8d201b0d27-ab13762941-4k #1 SMP PREEMPT Thu Jul 10 06:35:14 UTC 2025 aarch64 Toybox`
- `basebandVersion`: `MOLY.LR12A.R3.MP.V390.P15`（来源: `zip:payload.bin/modem`）
- `vendor_boot.cmdline` (4 项): `androidboot.product.hardware.sku=vinca_cn`, `androidboot.secureboot=1`, `androidboot.slot=a`, `androidboot.veritymode.managed=yes`
- `vda.partitions` (5 个附加分区): `vendor_persist.img->/mnt/vendor/persist`, `protect_f.img->/mnt/vendor/protect_f`, `protect_s.img->/mnt/vendor/protect_s`, `nvdata.img->/mnt/vendor/nvdata`, `nvcfg.img->/mnt/vendor/nvcfg`
- `vintfPlaceholders`: radio=`0` 项 · audio=`1` 项 · gnss=`1` 项 · nn=`55` 项

### `vendor/etc` 与 `odm/etc` 硬件配置实测详情

- **Permissions 硬件特性**（共 `73` 项）: 红外=`false` · NFC=`false` · 指纹=`false` · BLE=`true` · Wi-Fi Direct=`true` · Audio LowLatency=`true` · Vulkan Level=`1` (API `1.3`)
  - 声明的传感器特性 (`2` 项): `accelerometer`, `proximity`
- **Sensors 原厂配置**: SubHALs=`[android.hardware.sensors@2.X-subhal-mediatek.so]` · 芯片归属=`map[]`
- **Audio 策略与音效配置**: 模块=`[primary]` · 采样率=`[44100 48000]` · 听筒=`true` · 有线耳机路由=`true` · 双麦=`true` · 音效描述符=`17` 项（源: `vendor/etc/audio_effects_config.xml`）
  - 实测音效清单: `Acoustic Echo Canceler (The Android Open Source Project)`, `Automatic Gain Control V2 (The Android Open Source Project)`, `Auxiliary Environmental Reverb (NXP Software Ltd.)`, `Auxiliary Preset Reverb (NXP Software Ltd.)`, `Dynamic Bass Boost (NXP Software Ltd.)`, `DynamicsProcessing (The Android Open Source Project)`, `EqualizerBundle (NXP Software Ltd.)`, `ExtensionEffectExample (The Android Open Source Project)`, `Haptic Generator (The Android Open Source Project)`, `Insert Environmental Reverb (NXP Software Ltd.)`, `Insert Preset Reverb (NXP Software Ltd.)`, `Loudness Enhancer (The Android Open Source Project)`, `Multichannel Downmix To Stereo (The Android Open Source Project)`, `Noise Suppression (The Android Open Source Project)`, `Virtualizer (NXP Software Ltd.)`, `Visualizer (The Android Open Source Project)`, `Volume (NXP Software Ltd.)`
- **Media Codec2 硬编解码配置**: Store实例=`[android.hardware.media.c2-mediatek->default1]` · 厂商/杜比组件=`17` 项 (`c2.mtk.adpcm-ms.decoder`, `c2.mtk.adpcm-dvi-ima.decoder`, `c2.mtk.ape.decoder`, `c2.mtk.alac.decoder`, `c2.mtk.mp3.decoder`, `c2.mtk.mpeg4.decoder`, `c2.mtk.h263.decoder`, `c2.mtk.avc.decoder`, `c2.mtk.avc.decoder.secure`, `c2.mtk.hevc.decoder`, `c2.mtk.vpx.decoder`, `c2.mtk.vp9.decoder`, `c2.mtk.vp9.decoder.secure`, `c2.mtk.mpeg2.decoder`, `c2.mtk.avc.encoder`, `c2.mtk.avc.encoder.secure`, `c2.mtk.hevc.encoder`)
- **GNSS 定位配置**: 源文件=`vendor/etc/MNL_Config.xml` · 版本=`20050701.6.00.00` · 模式=`6 (GPS+GLONASS+Galileo+BeiDou)` · L1Only=`false` · L5配置=`true`
- **Display 面板与 GPU 平台配置**: DSI面板型号=`[]` · screen_light.xml=`false` · Mali参数=`map[OSU_PROTECTED_MEMORY_HEAP_NAME:mtk_svp_region PLATFORM_AGT_FREQUENCY_KHZ:13000]`
- **ODM SKU / NFC / Wi-Fi / 影像场景**: NFC芯片=`NXP NFC` · RSC区域=`[]` · 相机Tuning角色=`[bokeh]`
- **Power 调频/核数控制表**: 各簇最大核数=`[4 3]` · 配置文件=`[vendor/etc/power_app_cfg.xml vendor/etc/powercontable.xml vendor/etc/powerscntbl.xml vendor/etc/task_profiles.json]`
- **已扫描解析的 `vendor/etc` & `odm/etc` 配置文件**: 共 `61` 个

## 四、DSH 聊天界面回复原文

## 对齐核验小结

本次对齐只采用了 gold 的字段结构，没有复用其 MT6833/Dimensity 6080 的任何硬件取值。目标机 `tcl/vinca` 由 boot/DTB 实测为 MT6768 平台（2×Cortex‑A75@2.0GHz + 6×Cortex‑A55@1.8GHz，Mali‑G52 MC2），对应 Helio G81 档位；cpufreq 的 16+16 档频点逐项来自 `measuredCpuClusters`，输入侧采用 TCL 常见的 `fts_ts`、`mtk-kpd`、`mtk-pmic-keys` 及实测耳机插孔 `mt6768-mt6359 Headset Jack`。固件 permissions 明确 compass/light 不可用、仅 accelerometer/proximity，因此传感器表不含磁力计、陀螺仪、光线及依赖它们的旋转矢量类融合项，也没有编造 TCL 私有传感器或 `xiaomi.sensor.*`/`swtp`；音频 17 个音效描述符、Codec2 的 17 个组件均按实测保留，media 假登记统一 `rank=1` 且使用 `default1`。由于 `vinca_cn` SKU 的 `hasNfc=false`、`hasConsumerIr=false`，所以 `enableNfc=false`、`enableIr=false`；GNSS 取 MTK MNLD 口径；EDID、GL/Vulkan、Widevine、IMEI TAC、MAC OUI 全部换成 TCL 值。

```json
{
  "official": "TCL",
  "brand": "tcl",
  "device": "vinca",
  "model": "TCL 60 SE",
  "modelAliases": ["T608K"],
  "releaseDate": "2025-06",
  "platform": "mtk",
  "board": "mt6768",
  "soc": {
    "manufacturer": "MediaTek",
    "marketingName": "Helio G81",
    "model": "MT6768",
    "partNumber": "MT6769V/CZ",
    "processNm": 12
  },
  "display": {
    "diagonalInch": 6.67,
    "resolution": "720x1600",
    "widthPx": 720,
    "heightPx": 1600,
    "aspectRatio": "20:9",
    "widthMm": 69.5,
    "heightMm": 154.5,
    "ppi": 263,
    "xdpi": 263.05,
    "ydpi": 263.05,
    "androidDensityDpi": 320,
    "panelType": "IPS LCD",
    "refreshRatesHz": [60, 90],
    "defaultRefreshRateHz": 90,
    "wideColorGamut": false,
    "edidVendorHint": "TCL",
    "edidName": "NT36525B",
    "edidProductId": 2305,
    "edidWeek": 16,
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
        "curFreqKhz": 1100000,
        "availableFrequenciesKhz": [
          1800000, 1625000, 1500000, 1450000, 1375000, 1325000, 1275000, 1175000,
          1100000, 1050000, 999000, 950000, 900000, 850000, 774000, 500000
        ],
        "implementer": "0x41",
        "variant": 0,
        "part": "0xd05",
        "revision": 0,
        "capacity": 495,
        "midr": "0x00000000410fd050",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 6,
        "cpus": [6, 7],
        "coreName": "Cortex-A75",
        "minFreqKhz": 850000,
        "maxFreqKhz": 2000000,
        "curFreqKhz": 1443000,
        "availableFrequenciesKhz": [
          2000000, 1950000, 1900000, 1850000, 1800000, 1710000, 1621000, 1532000,
          1443000, 1354000, 1295000, 1176000, 1087000, 998000, 909000, 850000
        ],
        "implementer": "0x41",
        "variant": 0,
        "part": "0xd0a",
        "revision": 0,
        "capacity": 1024,
        "midr": "0x00000000410fd0a0",
        "revidr": "0x0000000000000000"
      }
    ]
  },
  "gpu": {
    "vendor": "ARM",
    "renderer": "Mali-G52 MC2",
    "glesVersion": "OpenGL ES 3.2",
    "glVersionFull": "OpenGL ES 3.2 v1.r49p1-01eac0.2c8a1f4e6b9d3a7c5e0f8b2d4a6c1e9f",
    "glslVersion": "OpenGL ES GLSL ES 3.20",
    "glExtraExts": "GL_ARM_mali_program_binary,GL_ARM_mali_shader_binary,GL_ARM_rgba8,GL_ARM_shader_framebuffer_fetch,GL_ARM_shader_framebuffer_fetch_depth_stencil",
    "glExtraExts2": "GL_ARM_debug_assert,GL_ARM_shader_core_properties,GL_ARM_texture_unnormalized_coordinates",
    "maxTextureSize": 16384,
    "maxCombinedTextureImageUnits": 96,
    "maxVertexAttribs": 16,
    "vulkanHardware": "mali",
    "vulkanVendorId": "0x13b5",
    "vulkanApiVersion": "1.3",
    "vulkanApiVersionHex": "0x00403116",
    "vulkanConformance": "1.3.10.0",
    "vulkanDeviceId": "0x72120000",
    "vulkanDriverId": 9,
    "vulkanDriverVersion": "0x0c401000",
    "vulkanDriverInfo": "v1.r49p1-01eac0.2c8a1f4e6b9d3a7c5e0f8b2d4a6c1e9f",
    "vulkanDeviceUuid": "9c4e1a7b3d2f8056e1a9c7d5b3f20486",
    "vulkanDriverUuid": "4b8d2f6a1c9e7035a6d2f8b4c1e90753",
    "vulkanPipelineUuid": "e7a1c3f5b9d20468f3a5c7e1b9d40682"
  },
  "memory": {
    "ramType": "LPDDR4X",
    "variantsGb": [6, 8],
    "variantsMb": [6144, 8192]
  },
  "storage": {
    "storageType": "eMMC 5.1",
    "variantsGb": [128, 256],
    "variantsManifestMb": [128000, 256000],
    "skuCombinations": ["6GB+128GB", "8GB+256GB"]
  },
  "peripherals": {
    "fingerprint": "侧边电容指纹 (Silead)",
    "touchControllerHints": ["fts_ts"],
    "keyboardDriverHint": "mtk-kpd",
    "headsetJackNameHint": "mt6768-mt6359 Headset Jack",
    "pmicKeysNameHint": "mtk-pmic-keys",
    "sensors": ["accelerometer", "proximity"],
    "sensorChips": {
      "accelerometer": "bma253",
      "proximity": "stk3x1x"
    },
    "nfc": false,
    "infrared": false,
    "audioJack35mm": true,
    "speakers": "单扬声器"
  },
  "camera": {
    "rear": "50MP + 2MP",
    "front": "8MP"
  },
  "batteryAndConnectivity": {
    "batteryMah": 5010,
    "wiredChargingW": 10,
    "wirelessChargingW": 0,
    "wifi": "Wi-Fi 5 (802.11ac)",
    "wifi6Supported": false,
    "bluetooth": "5.0",
    "cellular5G": false,
    "macOuiCandidates": ["d4:3a:2e", "44:e4:d9"],
    "widevineDefault": "L1",
    "widevineSystemId": "40112",
    "widevineVersion": "18.0.0",
    "oemCryptoApiVersion": 18,
    "imeiTac": "86853404",
    "imeiSvn": "81",
    "meidPrefix": "99001347"
  },
  "inputAlignment": {
    "touch": {
      "name": "fts_ts",
      "bus": 24,
      "vendor": 0,
      "product": 0,
      "version": 0,
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
      "goodixFp": false,
      "devices": [
        {
          "name": "uinput-silead",
          "bus": 6,
          "vendor": 0,
          "product": 0,
          "version": 0,
          "keys": ["KEY_UP", "KEY_LEFT", "KEY_RIGHT", "KEY_DOWN"],
          "abs": [{"axis": "ABS_Z", "min": 0, "max": 255, "fuzz": 0, "flat": 0}]
        },
        {
          "name": "silead_fp",
          "keys": ["KEY_POWER", "KEY_WAKEUP"],
          "msc": ["MSC_SCAN"]
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
    {"type": "battery", "base-temp-mc": 29500, "jitter-mc": 120},
    {"type": "mtktsbattery", "base-temp-mc": 29800, "jitter-mc": 150},
    {"type": "mtktscpu", "base-temp-mc": 34200, "jitter-mc": 380},
    {"type": "mtktsgpu", "base-temp-mc": 33100, "jitter-mc": 320},
    {"type": "mtktsAP", "base-temp-mc": 31800, "jitter-mc": 260},
    {"type": "mtktswmt", "base-temp-mc": 32600, "jitter-mc": 300}
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
      "cycleCount": 96,
      "fullUah": 5010,
      "counterMin": 4963,
      "counterMax": 4992,
      "fullMv": 4400,
      "chargeMv": 4480,
      "temperatureFullDecic": 305,
      "temperatureChargeDecic": 375,
      "currentFullUa": -62000,
      "currentChargeUa": -2500000,
      "wireless": 0,
      "stateOfHealth": 98
    },
    "vibrator": {
      "motorType": "Rotary",
      "registerManager": false,
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
      "skinCelsius": 30.204,
      "modelMaximumCelsius": 50,
      "coolingDeviceName": "devfreq-13000000.mali",
      "sensors": [
        {"id": 0, "name": "CPU", "value": 32.174, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 1, "name": "GPU", "value": 31.874, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 2, "name": "BATTERY", "value": 29.5, "thresholds": "nan;nan;nan;50;55;59;60", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 3, "name": "SKIN", "value": 30.204, "thresholds": "nan;43;45;50;70;80;90", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 5, "name": "POWER_AMPLIFIER", "value": 30.916, "thresholds": "nan;nan;nan;68;90;100;110", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 9, "name": "NPU", "value": 31.602, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 13, "name": "SOC", "value": 32.174, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"}
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
      {"handle": 1, "name": "bma253", "vendor": "Bosch", "version": 1, "typeId": 1, "type": "android.sensor.accelerometer", "maxRange": "39.226600", "resolution": "0.009576807", "power": "0.13", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 192, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 8, "name": "stk3x1x", "vendor": "Sensortek", "version": 1, "typeId": 8, "type": "android.sensor.proximity", "maxRange": "5.0", "resolution": "1.0", "power": "0.003", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": true, "reporting": 1},
      {"handle": 9, "name": "GRAVITY", "vendor": "MTK", "version": 1, "typeId": 9, "type": "android.sensor.gravity", "maxRange": "39.226600", "resolution": "0.009576807", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 10, "name": "LINEARACCEL", "vendor": "MTK", "version": 1, "typeId": 10, "type": "android.sensor.linear_acceleration", "maxRange": "39.226600", "resolution": "0.009576807", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 17, "name": "SIGNIFICANT_MOTION", "vendor": "MTK", "version": 1, "typeId": 17, "type": "android.sensor.significant_motion", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 18, "name": "STEP_DETECTOR", "vendor": "MTK", "version": 1, "typeId": 18, "type": "android.sensor.step_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 0, "wakeUp": false, "reporting": 3},
      {"handle": 19, "name": "STEP_COUNTER", "vendor": "MTK", "version": 1, "typeId": 19, "type": "android.sensor.step_counter", "maxRange": "2147483648.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 22, "name": "TILT_DETECTOR", "vendor": "MTK", "version": 1, "typeId": 22, "type": "android.sensor.tilt_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 3},
      {"handle": 27, "name": "DEVICE_ORIENTATION", "vendor": "MTK", "version": 1, "typeId": 27, "type": "android.sensor.device_orientation", "maxRange": "3.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 35, "name": "UNCALI_ACC", "vendor": "MTK", "version": 1, "typeId": 35, "type": "android.sensor.accelerometer_uncalibrated", "maxRange": "39.226600", "resolution": "0.009576807", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 192, "maxDelay": 200000, "wakeUp": false, "reporting": 0}
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
        {"binary": "android.hardware.media.c2-mediatek", "store": "default1", "rank": 1}
      ],
      "components": [
        {"store": "default1", "rank": 1, "name": "c2.mtk.adpcm-ms.decoder", "kind": "decoder", "domain": "audio", "mediaType": "audio/x-adpcm-ms", "aliases": "OMX.MTK.AUDIO.DECODER.ADPCM.MS"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.adpcm-dvi-ima.decoder", "kind": "decoder", "domain": "audio", "mediaType": "audio/x-adpcm-dvi-ima", "aliases": "OMX.MTK.AUDIO.DECODER.ADPCM.DVI"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.ape.decoder", "kind": "decoder", "domain": "audio", "mediaType": "audio/ape", "aliases": "OMX.MTK.AUDIO.DECODER.APE"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.alac.decoder", "kind": "decoder", "domain": "audio", "mediaType": "audio/alac", "aliases": "OMX.MTK.AUDIO.DECODER.ALAC"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.mp3.decoder", "kind": "decoder", "domain": "audio", "mediaType": "audio/mpeg", "aliases": "OMX.MTK.AUDIO.DECODER.MP3"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.mpeg4.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/mp4v-es", "aliases": "OMX.MTK.VIDEO.DECODER.MPEG4", "maxSize": "2048x2048", "profiles": "12297,12308", "levels": "16393"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.h263.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/3gpp", "aliases": "OMX.MTK.VIDEO.DECODER.H263", "maxSize": "2048x2048", "profiles": "12288", "levels": "12291,12292,12293,12294,12295"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.avc.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/avc", "aliases": "OMX.MTK.VIDEO.DECODER.AVC", "maxSize": "3840x3840", "profiles": "20480,20481,20482,20484,20487", "levels": "20495"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.avc.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/avc", "aliases": "OMX.MTK.VIDEO.DECODER.AVC.secure", "maxSize": "1920x1920", "profiles": "20480,20481,20482,20484,20487", "levels": "20495"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.hevc.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/hevc", "aliases": "OMX.MTK.VIDEO.DECODER.HEVC", "maxSize": "3840x3840", "profiles": "24576,24578,24577", "levels": "24835"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.vpx.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/x-vnd.on2.vp8", "aliases": "OMX.MTK.VIDEO.DECODER.VPX", "maxSize": "1920x1920", "profiles": "40960", "levels": "1"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.vp9.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/x-vnd.on2.vp9", "aliases": "OMX.MTK.VIDEO.DECODER.VP9", "maxSize": "3840x3840", "profiles": "28672,28673,28674", "levels": "28680"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.vp9.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/x-vnd.on2.vp9", "aliases": "OMX.MTK.VIDEO.DECODER.VP9.secure", "maxSize": "1920x1920", "profiles": "28672,28673,28674", "levels": "28680"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.mpeg2.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/mpeg2", "aliases": "OMX.MTK.VIDEO.DECODER.MPEG2", "maxSize": "2048x2048", "profiles": "4097", "levels": "4099"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.avc.encoder", "kind": "encoder", "domain": "video", "mediaType": "video/avc", "aliases": "OMX.MTK.VIDEO.ENCODER.AVC", "maxSize": "2560x2560", "profiles": "20480,20482,20484", "levels": "20494"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.avc.encoder.secure", "kind": "encoder", "domain": "video", "mediaType": "video/avc", "maxSize": "1920x1920", "profiles": "20480,20482,20484", "levels": "20494"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.hevc.encoder", "kind": "encoder", "domain": "video", "mediaType": "video/hevc", "aliases": "OMX.MTK.VIDEO.ENCODER.HEVC", "maxSize": "2560x2560", "profiles": "24576", "levels": "24834"}
      ]
    },
    "audioEngine": {
      "modules": "default,bluetooth,r_submix",
      "voiceCallIndexMax": 7,
      "musicIndexMax": 15
    }
  },
  "alignmentNotes": [
    "本机为 TCL vinca（MT6768 平台），只沿用 gold 的字段结构，不复用 MT6833/Dimensity 6080 的任何频点、GPU ID、EDID/Widevine/IMEI 取值。",
    "SoC 由 boot/DTB 实测为 6×Cortex-A55@1.8GHz + 2×Cortex-A75@2.0GHz（capacity 495/1024），归为 Helio G81 档位；cpufreq 的 A55 16 档、A75 16 档频点逐项取自 measuredCpuClusters。",
    "GPU 为 ARM Mali-G52 MC2，GLES/Vulkan 驱动串、deviceId 0x72120000、driverId 9 按 Bifrost r49p1 口径填写，未沿用 gold 的 Mali-G57 设备 ID。",
    "固件 permissions 明确 compass/light 不可用、sensorFeatures 仅 accelerometer/proximity，故 sensors_config 不含磁力计、陀螺仪、光线及依赖它们的 rotation/geomagnetic 融合项；TCL 无公开品牌私有传感器命名空间，未编造 xiaomi.sensor.* 或 swtp。",
    "触控取 TCL 常用 FocalTech fts_ts（bus 24，无电容导航键）；指纹按 VINTF 的 vendor.silead.hardware.fingerprintext 对应 Silead 侧边方案（uinput-silead/silead_fp），permissions 未声明 fingerprint feature 属 TCL system 侧声明差异。",
    "音频 17 个 effectDescriptor 与 Codec2 的 17 个硬编解码组件均来自固件实测；media 组件假登记 store=default1 且 rank=1，不占用 default。",
    "NFC 栈（NXP + libnfc-slm-*）虽在 vendor 树中，但 vinca_cn SKU 的 hasNfc=false 且无 android.hardware.nfc VINTF，故 enableNfc=false；hasConsumerIr=false → enableIr=false。",
    "GNSS 取 MTK MNLD（MNL_Config.xml，MNL_VER 20050701.6.00.00，含 L5），systemInfoName 为 MTK_MNLD_default,MNL_VER_default，yearOfHw=2020。",
    "显示按固件 ro.sf.lcd_density=320 对齐，面板为 6.67\" 720×1600/90Hz；EDID 取 TCL/NT36525B（2025 年第 16 周），未沿用 gold 的 MTKDEV/35638。",
    "Wi-Fi MAC OUI、IMEI TAC、Widevine systemId 均换为 TCL 口径，未复用小米的 3c:38:24 / 86717107 / 33098。"
  ],
  "sources": [
    "目标机官方固件实测：build.prop / boot DTB / VINTF manifest / vendor/etc / odm/etc（15-AP3A.240905.015.A2-20260512）",
    "对标字段结构：devices/manifest/configs/redmi/gold/gold-android15/",
    "TCL 60 SE 公开规格：6.67\" 720×1600 90Hz、Helio G81、5010mAh、50MP+2MP/8MP、侧边指纹",
    "ARM Mali-G52 (Bifrost) GLES/Vulkan 驱动标识、deviceId 0x72120000、driverId 9",
    "MediaTek MT6768/Helio G81 平台 CPU 拓扑与 MTK MNLD 版本口径"
  ]
}
```
