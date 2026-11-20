# 真机硬件规格与云机对齐留档 · Symphony MAX 10 (symphony/MAX_10)

- **生成时间**: `2026-11-20T17:54:57+08:00`
- **ROM Build ID**: `14-TP1A.220624.014-20250212`
- **分区 Android 版本**: `system=14, system_ext=14, product=14, vendor=13, odm=13, vendor_dlkm=13`
- **DSH 聊天会话 ID**: `session-a7323a14-c5ef-4440-9e76-0eb8bf1791e3`（标题：`[02_prepare] 真机规格采集 · MAX 10 (symphony/MAX_10 · 14-TP1A.220624.014-20250212)`）
- **采集模型路由**: `cdh-maas / deepseek-v4.1-flash-1017`（复用在线 DSH Web: `false`）

---

## 一、设备表对齐规格（对应 `devices/manifest/devices/symphony/MAX_10.json`）

| 字段 | 对齐值 | 说明 |
| --- | --- | --- |
| `official` | `Symphony` | 厂商集团 |
| `brand` | `symphony` | 子品牌标识 |
| `model` | `Symphony MAX 10` | 商业上市机型名 （型号代码: `MAX10`, `MAX 10`, `MAX_10`） |
| `device` | `MAX_10` | 设备代号 |
| `platform` / `board` | `unisoc` / `sp9863a` | 芯片平台 / 主板代号 |
| `resolution` | `720x1600` | 屏幕物理像素分辨率 |
| `inch` | `69x153` | 屏幕可视物理宽×高 mm（对角线 `6.60"`） |
| `density` | `320` | Android 逻辑密度 dpi（物理 PPI `266`，xdpi `265.80` / ydpi `265.80`） |
| `smp` | `8` | CPU 核心总数 |
| `memory` | `[3072 4096 6144]` | 全系发售内存档位 MB（3G / 4G / 6G，LPDDR4X） |
| `storage` | `[64000 128000]` | 全系发售存储档位（64G / 128G，eMMC 5.1） |

- **常见量产发售版本组合**: `3GB+64GB` / `4GB+64GB` / `4GB+128GB` / `6GB+128GB`

## 二、真机深层硬件对齐画像（参考 `redmi/gold` 标准）

### 1. SoC 与 CPU 簇全量频点表（`cdh_cpufreq` / `module/config.json` 对齐）
- **SoC 芯片**: Unisoc SC9863A (`SC9863A` SC9863A, 28nm)
- **CPU 拓扑**: `8x Cortex-A55 @ 1.8GHz（单簇）`（驱动: `sprd-cpufreq`，调速器: `sprd_dvfs`，可选调速器: `[sprd_dvfs performance powersave userspace]`，BogoMIPS: `26.00`）

| 策略簇 | 核心列表 | 微架构 | Part / MIDR | 容量权重 | 最小 / 驻留 / 最大 (kHz) | 频点档数 |
| --- | --- | --- | --- | --- | --- | --- |
| policy0 | `[0 1 2 3 4 5 6 7]` | Cortex-A55 | `0xd05` (`0x00000000412fd050`) | `1024` | `300000 / 1200000 / 1800000` | `17 档` |

- **`policy0` (`Cortex-A55`, cpus `[0 1 2 3 4 5 6 7]`) 全量 `available-frequencies` (17 档, kHz)**:
  `[1800000 1700000 1600000 1500000 1400000 1300000 1200000 1150000 1100000 1000000 900000 800000 700000 600000 500000 400000 300000]`

<details>
<summary>可直接用于 <code>module/config.json</code> 的 <code>cpufreq</code> 完整配置片段</summary>

```json
{
  "cpufreq": {
    "driver": "sprd-cpufreq",
    "governor": "sprd_dvfs",
    "available-governors": [
      "sprd_dvfs",
      "performance",
      "powersave",
      "userspace"
    ],
    "transition-latency": 1000,
    "scaling-readable": true,
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
          5,
          6,
          7
        ],
        "min-freq": 300000,
        "max-freq": 1800000,
        "cur-freq": 1200000,
        "available-frequencies": [
          1800000,
          1700000,
          1600000,
          1500000,
          1400000,
          1300000,
          1200000,
          1150000,
          1100000,
          1000000,
          900000,
          800000,
          700000,
          600000,
          500000,
          400000,
          300000
        ],
        "implementer": "0x41",
        "variant": 2,
        "part": "0xd05",
        "revision": 0,
        "capacity": 1024,
        "midr": "0x00000000412fd050",
        "revidr": "0x0000000000000000"
      }
    ]
  }
}
```
</details>

### 2. 显示面板、GPU 与图形栈（`gpu_id.cfg` / `buildVendor` 对标 `gold`）
- **面板类型与刷新率**: IPS LCD · 刷新率 `[60] Hz`（默认 `60 Hz`） · 广色域 `false` · EDID `SPR` (`ili9881c`, product=`39041`, year=`2024`W`38`)
- **OpenGL ES**: `Imagination Technologies` / `PowerVR Rogue GE8322` / `OpenGL ES 3.2 build 1.13@5776728`
  - GL 驱动扩展: `GL_IMG_bindless_texture,GL_IMG_framebuffer_downsample,GL_IMG_multisampled_render_to_texture,GL_IMG_program_binary,GL_IMG_read_format,GL_IMG_shader_binary,GL_IMG_texture_compression_pvrtc,GL_IMG_texture_compression_pvrtc2,GL_IMG_texture_format_BGRA8888,GL_IMG_uniform_buffer_object,GL_IMG_vertex_array_object` GL_EXT_debug_marker,GL_EXT_discard_framebuffer,GL_EXT_multisampled_render_to_texture,GL_EXT_robustness,GL_EXT_sRGB,GL_EXT_texture_filter_anisotropic
- **Vulkan**: `ro.hardware.vulkan=powervr` · VendorID `0x1010` · DeviceID `0x00008322` · API `1.3` (`0x00403116`) · DriverID `6` (`0x000d0000`)

### 3. 触控、指纹、外设与输入节点对齐（`module/config.json` & `hardware/config.json` 对标 `gold`）
- **指纹方案**: 侧边电容指纹 (FPC)
- **触控与按键对齐**: touch=`focaltech_ats` · keyboard=`gpio-keys` · input_identity 外设数=`4` 项
- **硬件 HAL 变体与开关**: radioVariant=`default` · NFC=`false` · 红外=`false` · 3.5mm=`true` · 单扬声器
- **影像规格**: 后置 13MP + 2MP | 前置 8MP
- **电池与通信**: `5000 mAh`（有线 `10W` / 无线 `0W`） · Wi-Fi 5 (802.11ac)（Wi-Fi 6=`false`） · BT `5.0` · 5G=`false` · Widevine `L3` (sysId=`30251`) · TAC `35851009` · MAC OUI `[00:1e:3a]`

### 4. `vendor/etc/*_config.xml` 虚拟 HAL AI 对齐清单（对标 `gold-android15/vendor/etc`）
- **`sensors_config.xml`**: 共 `11` 项对齐传感器
- **`battery_config.xml`**: `Li-poly` · 容量 `5000 mAh` · 满电/充电电压 `4384 / 4400 mV`
- **`thermal_config.xml`**: 皮肤基温 `28.800 ℃` · 温区传感器 `6` 项 · Cooling=`gpu-cooling`
- **`gnss_config.xml`**: systemInfo=`UNISOC_GNSS_default,GNSS_VER_default` · yearOfHw=`2020` · L5=`false`
- **`vibrator_config.xml` & `light_config.xml`**: 马达 `ERM (转子马达)` (cap=`0`, effects=`[CLICK DOUBLE_CLICK TICK HEAVY_CLICK]`) · 逻辑灯 `6` 项
- **`audio_effects_descriptors.xml` & `media_config.xml`**: AI 音效描述符 `13` 项 · AI Codec2 实例 `1` 项 / 组件 `15` 项

### 5. AI 真机对齐提示
- 平台为 Unisoc（展锐）SC9863A，非 MTK：radioVariant 取 default，VINTF 保留实测 vendor.unisoc.hardware.radio.* 与 vendor.sprd.hardware.* 全部占位符，未改写成 mtk 分支。
- CPU 实测仅 policy0 单簇 0-7、capacity 1024，按单簇 A55 建模；最高频取 vendor/etc 频点线索的 1800000 kHz，17 档为展锐 DVFS 常见档位，未虚构 2.0GHz big 簇。
- GPU 为 Imagination PowerVR Rogue GE8322：GL 串按 PowerVR DDK 1.13 生成，扩展仅保留 IMG/EXT 系；Vulkan 依 permissions 实测 vulkanLevel=0、vulkanVersion=1.3 落 0x00403116，未套用 gold 的 Mali 值。
- 传感器严格按实测能力位 accelerometer/light/proximity/stepcounter/stepdetector 生成 11 项：物理三件套（bmi160_acc + stk3x1x 光感/距感）加加速度派生融合与计步项；未编造陀螺仪、磁力计、气压计，也没有 xiaomi.sensor.*/oplus.sensor.*/vivo.sensor.* 等品牌私有 sensor，且无 swtp。
- touch 用实测 focaltech_ats（FocalTech），按键 gpio-keys，PMIC 键 sprd-pmic-keys，声卡插孔按展锐 sprdphone 命名；指纹按 FPC 侧边电容方案（goodixFp=false），inputIdentity 未出现小米专属节点。
- thermal.zones 五项直接取实测 gpu/cpu/board/pa/chg 温区的 base-temp-mc 与 jitter-mc；thermal_config.xml 的 SKIN 基温与分级阈值按展锐低端平台常见区间（80/85/95/115℃ 结温、45/50/55/60℃ 电池）配置。
- 电池按 5000mAh 单电芯 Li-poly、4.384V 满电、10W 有线、无无线充；NFC 与红外均按实测 hasNfc=false、hasConsumerIr=false 关闭。
- gold 的 ro.vendor.cdh.display.edid_* / refresh_rates / wide_color_gamut 映射到 display.edid*、refreshRatesHz、wideColorGamut；glVersionFull/glExtraExts/vulkan*/widevine*/imeiTac/macOui 均已落到对应字段，未直接复制 gold 的 MT6833/Mali 专属串。
- 待真机复核项：EDID 面板 ID 与年份周（ili9881c 为推定面板）、imeiTac、Wi-Fi MAC OUI、Widevine systemId 为品牌级推定值；固件 hasEarpiece/hasWiredHeadset=false 与直板机形态矛盾，暂按保留 3.5mm 插孔与耳机插孔节点处理，需以真机 /proc/bus/input/devices、dumpsys media.audio_flinger、getprop ro.vendor.cdh.* 复核后固化。

## 三、官方固件包解包实测对账（`build.prop` + `boot.img` + `VINTF` + `DTB` + `fstab` + `vendor/etc`）

- `marketName`: `` | `modelCode`: `MAX 10` | `manufacturer`: `Symphony`
- `board`: `sp9863a` | `soc`: `Spreadtrum SC9863A ()`
- `densityDpi`: `320` | `egl`: `` | `vulkan`: `` | `abilist`: `arm64-v8a,armeabi-v7a,armeabi`
- `securityPatch`: `2024-12-05` | `buildDate`: `20250212` | `kernelLine`: `5.15` | `vintfVersion`: `7.0`
- `uname`: `Linux localhost 5.15.149-android13-8-00008-gbe074b05e5af-ab12096863 #1 SMP PREEMPT Tue Jul 16 15:36:08 UTC 2024 aarch64 Toybox`
- `fingerprint`: `Symphony/MAX10/MAX_10:14/UP1A.231005.007/1739350789:user/release-keys`
- `vendor_boot.cmdline` (4 项): `androidboot.product.hardware.sku=MAX10`, `androidboot.secureboot=1`, `androidboot.slot=a`, `androidboot.veritymode.managed=yes`
- `thermal.zones` (5 个温区): `gpu-thmzone`, `cpu-thmzone`, `board-thmzone`, `pa-thmzone`, `chg-thmzone`
- `vintfPlaceholders`: radio=`15` 项 · audio=`1` 项 · gnss=`1` 项 · nn=`12` 项

### `vendor/etc` 与 `odm/etc` 硬件配置实测详情

- **Permissions 硬件特性**（共 `68` 项）: 红外=`false` · NFC=`false` · 指纹=`true` · BLE=`true` · Wi-Fi Direct=`true` · Audio LowLatency=`false` · Vulkan Level=`0` (API `1.3`)
  - 声明的传感器特性 (`5` 项): `accelerometer`, `light`, `proximity`, `stepcounter`, `stepdetector`
- **Sensors 原厂配置**: SubHALs=`[android.hardware.sensors@2.X-sprd_subhal.so]` · 芯片归属=`map[]`
- **Audio 策略与音效配置**: 模块=`[]` · 采样率=`[]` · 听筒=`false` · 有线耳机路由=`false` · 双麦=`false` · 音效描述符=`13` 项（源: `vendor/etc/audio_effects.xml`）
  - 实测音效清单: `Acoustic Echo Canceler (The Android Open Source Project)`, `Auxiliary Environmental Reverb (NXP Software Ltd.)`, `Auxiliary Preset Reverb (NXP Software Ltd.)`, `Dynamic Bass Boost (NXP Software Ltd.)`, `DynamicsProcessing (The Android Open Source Project)`, `EqualizerBundle (NXP Software Ltd.)`, `Insert Environmental Reverb (NXP Software Ltd.)`, `Insert Preset Reverb (NXP Software Ltd.)`, `Loudness Enhancer (The Android Open Source Project)`, `Multichannel Downmix To Stereo (The Android Open Source Project)`, `Virtualizer (NXP Software Ltd.)`, `Visualizer (The Android Open Source Project)`, `Volume (NXP Software Ltd.)`
- **Media Codec2 硬编解码配置**: Store实例=`[android.hardware.media.c2@1.1-unisoc-service->default]` · 厂商/杜比组件=`15` 项 (`c2.unisoc.jpeg.decoder`, `c2.unisoc.imaadpcm.decoder`, `c2.unisoc.mp3.decoder`, `c2.unisoc.mpl1.decoder`, `c2.unisoc.mpl2.decoder`, `c2.unisoc.avc.decoder`, `c2.unisoc.hevc.decoder`, `c2.unisoc.mpeg4.decoder`, `c2.unisoc.h263.decoder`, `c2.unisoc.vp8.decoder`, `c2.unisoc.vp9.decoder`, `c2.unisoc.jpeg.encoder`, `c2.unisoc.mp3.encoder`, `c2.unisoc.avc.encoder`, `c2.unisoc.hevc.encoder`)
- **ODM SKU / NFC / Wi-Fi / 影像场景**: NFC芯片=`-` · RSC区域=`[]` · 相机Tuning角色=`[]`
- **Power 调频/核数控制表**: 各簇最大核数=`[]` · 配置文件=`[vendor/etc/task_profiles.json]`
- **已扫描解析的 `vendor/etc` & `odm/etc` 配置文件**: 共 `61` 个

## 四、DSH 聊天界面回复原文

## 简明总结

- **平台**：采用 Spreadtrum/Unisoc **SC9863A（sp9863a，28nm，8×Cortex-A55，最高 1.8GHz）**，不是 MTK；因此 `radioVariant` 取 `"default"`，VINTF 沿用实测中 15 项 `vendor.unisoc.hardware.radio.*` 以及 `vendor.sprd.hardware.*` 占位符。
- **GPU**：**Imagination PowerVR Rogue GE8322**（不是 Mali/Adreno）；GL 字符串按 PowerVR DDK 1.13 生成；Vulkan 根据实测 `vulkanLevel=0`、`vulkanVersion=1.3`，取 `0x00403116`。
- **传感器**：实测能力只有加速度、光线、距离、计步检测和计步计数，所以只输出 11 项（`bmi160_acc` + `stk3x1x_l/_p` + 加速度派生融合与计步），**未编造陀螺仪、磁力计、气压计，也没有任何品牌私有 sensor 与 `swtp`**。
- **外设**：触摸使用实测的 `focaltech_ats`（FocalTech），按键为 `gpio-keys`，PMIC 键为 `sprd-pmic-keys`，声卡插孔为 `sprdphone Headset Jack`，指纹按 FPC 侧边电容方案（`goodixFp=false`）。
- **显示、电池、散热**：320dpi 加 6.6″，因此为 720×1600 IPS 60Hz；电池 5000mAh Li-poly、10W 有线、无无线充；五项内核温区直接采用实测的 `base-temp-mc` 与 `jitter-mc`；NFC、红外均按实测 `false`。
- **gold 的 50 项 `ro.vendor.*` 映射**：`cdh.display.edid_* / refresh_rates / wide_color_gamut` 映射到 `display.*`；`glVersionFull / glExtraExts / vulkan*` 映射到 `gpu.*`；`widevine* / imeiTac / macOuiCandidates` 映射到 `batteryAndConnectivity.*`。
- **待复核项**：EDID 面板 ID/年份周、`imeiTac`、Wi-Fi OUI、Widevine `systemId` 为品牌级推定值；固件 `hasEarpiece/hasWiredHeadset=false` 与直板机形态矛盾（疑似配置未解析），暂保留 3.5mm 耳机孔。

```json
{
  "official": "Symphony",
  "brand": "symphony",
  "device": "MAX_10",
  "model": "Symphony MAX 10",
  "modelAliases": ["MAX10", "MAX 10", "MAX_10"],
  "releaseDate": "2025-02",
  "platform": "unisoc",
  "board": "sp9863a",
  "soc": {
    "manufacturer": "Unisoc",
    "marketingName": "SC9863A",
    "model": "SC9863A",
    "partNumber": "SC9863A",
    "processNm": 28
  },
  "display": {
    "diagonalInch": 6.6,
    "resolution": "720x1600",
    "widthPx": 720,
    "heightPx": 1600,
    "aspectRatio": "20:9",
    "widthMm": 69,
    "heightMm": 153,
    "ppi": 266,
    "xdpi": 265.8,
    "ydpi": 265.8,
    "androidDensityDpi": 320,
    "panelType": "IPS LCD",
    "refreshRatesHz": [60],
    "defaultRefreshRateHz": 60,
    "wideColorGamut": false,
    "edidVendorHint": "SPR",
    "edidName": "ili9881c",
    "edidProductId": 39041,
    "edidWeek": 38,
    "edidYear": 2024
  },
  "cpu": {
    "smp": 8,
    "architecture": "arm64-v8a",
    "topologySummary": "8x Cortex-A55 @ 1.8GHz（单簇）",
    "driver": "sprd-cpufreq",
    "governor": "sprd_dvfs",
    "availableGovernors": ["sprd_dvfs", "performance", "powersave", "userspace"],
    "transitionLatency": 1000,
    "scalingReadable": true,
    "packagePerCluster": false,
    "bogoMips": "26.00",
    "clusters": [
      {
        "policy": 0,
        "cpus": [0, 1, 2, 3, 4, 5, 6, 7],
        "coreName": "Cortex-A55",
        "minFreqKhz": 300000,
        "maxFreqKhz": 1800000,
        "curFreqKhz": 1200000,
        "availableFrequenciesKhz": [
          1800000, 1700000, 1600000, 1500000, 1400000, 1300000, 1200000,
          1150000, 1100000, 1000000, 900000, 800000, 700000, 600000,
          500000, 400000, 300000
        ],
        "implementer": "0x41",
        "variant": 2,
        "part": "0xd05",
        "revision": 0,
        "capacity": 1024,
        "midr": "0x00000000412fd050",
        "revidr": "0x0000000000000000"
      }
    ]
  },
  "gpu": {
    "vendor": "Imagination Technologies",
    "renderer": "PowerVR Rogue GE8322",
    "glesVersion": "OpenGL ES 3.2",
    "glVersionFull": "OpenGL ES 3.2 build 1.13@5776728",
    "glslVersion": "OpenGL ES GLSL ES 3.20",
    "glExtraExts": "GL_IMG_bindless_texture,GL_IMG_framebuffer_downsample,GL_IMG_multisampled_render_to_texture,GL_IMG_program_binary,GL_IMG_read_format,GL_IMG_shader_binary,GL_IMG_texture_compression_pvrtc,GL_IMG_texture_compression_pvrtc2,GL_IMG_texture_format_BGRA8888,GL_IMG_uniform_buffer_object,GL_IMG_vertex_array_object",
    "glExtraExts2": "GL_EXT_debug_marker,GL_EXT_discard_framebuffer,GL_EXT_multisampled_render_to_texture,GL_EXT_robustness,GL_EXT_sRGB,GL_EXT_texture_filter_anisotropic",
    "maxTextureSize": 8192,
    "maxCombinedTextureImageUnits": 32,
    "maxVertexAttribs": 16,
    "vulkanHardware": "powervr",
    "vulkanVendorId": "0x1010",
    "vulkanApiVersion": "1.3",
    "vulkanApiVersionHex": "0x00403116",
    "vulkanConformance": "1.3.0.0",
    "vulkanDeviceId": "0x00008322",
    "vulkanDriverId": 6,
    "vulkanDriverVersion": "0x000d0000",
    "vulkanDriverInfo": "PowerVR Rogue GE8322 1.13@5776728",
    "vulkanDeviceUuid": "93bd2f7c1a4e6d0853b9c1f2a4d6e809",
    "vulkanDriverUuid": "b7c1d3e5a9f02468135b7d9f1c3e5a70",
    "vulkanPipelineUuid": "4f2a6c8e0b1d3579a1c3e5f7092b4d6f"
  },
  "memory": {
    "ramType": "LPDDR4X",
    "variantsGb": [3, 4, 6],
    "variantsMb": [3072, 4096, 6144]
  },
  "storage": {
    "storageType": "eMMC 5.1",
    "variantsGb": [64, 128],
    "variantsManifestMb": [64000, 128000],
    "skuCombinations": ["3GB+64GB", "4GB+64GB", "4GB+128GB", "6GB+128GB"]
  },
  "peripherals": {
    "fingerprint": "侧边电容指纹 (FPC)",
    "touchControllerHints": ["focaltech_ats"],
    "keyboardDriverHint": "gpio-keys",
    "headsetJackNameHint": "sprdphone Headset Jack",
    "pmicKeysNameHint": "sprd-pmic-keys",
    "sensors": ["accelerometer", "light", "proximity", "stepcounter", "stepdetector"],
    "sensorChips": {
      "accelerometer": "bmi160_acc",
      "light": "stk3x1x_l",
      "proximity": "stk3x1x_p"
    },
    "nfc": false,
    "infrared": false,
    "audioJack35mm": true,
    "speakers": "单扬声器"
  },
  "camera": {
    "rear": "13MP + 2MP",
    "front": "8MP"
  },
  "batteryAndConnectivity": {
    "batteryMah": 5000,
    "wiredChargingW": 10,
    "wirelessChargingW": 0,
    "wifi": "Wi-Fi 5 (802.11ac)",
    "wifi6Supported": false,
    "bluetooth": "5.0",
    "cellular5G": false,
    "macOuiCandidates": ["00:1e:3a"],
    "widevineDefault": "L3",
    "widevineSystemId": "30251",
    "widevineVersion": "18.0.0",
    "oemCryptoApiVersion": 18,
    "imeiTac": "35851009",
    "imeiSvn": "80",
    "meidPrefix": "99002158"
  },
  "inputAlignment": {
    "touch": {
      "name": "focaltech_ats",
      "bus": 24,
      "vendor": 0,
      "product": 0,
      "version": 0,
      "mtSlots": 10,
      "mtPressure": false,
      "touchKeys": []
    },
    "keyboard": {
      "name": "gpio-keys",
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
          "name": "uinput-fpc",
          "bus": 6,
          "vendor": 1638,
          "product": 2184,
          "version": 0,
          "keys": ["KEY_UP", "KEY_LEFT", "KEY_RIGHT", "KEY_DOWN"],
          "abs": [{"axis": "ABS_Z", "min": 0, "max": 255, "fuzz": 0, "flat": 0}]
        },
        {
          "name": "fpc_irq@fingerprint",
          "keys": ["KEY_POWER", "KEY_WAKEUP"],
          "msc": ["MSC_SCAN"]
        },
        {
          "name": "sprdphone Headset Jack",
          "bus": 25,
          "phys": "ALSA",
          "keys": ["KEY_PLAYPAUSE", "KEY_VOICECOMMAND"],
          "switches": ["SW_HEADPHONE_INSERT", "SW_MICROPHONE_INSERT"]
        },
        {
          "name": "sprd-pmic-keys",
          "bus": 25,
          "vendor": 1,
          "product": 1,
          "version": 1,
          "keys": ["KEY_VOLUMEUP", "KEY_POWER"]
        }
      ],
      "aligns": [
        {"name": "gpio-keys", "keys": ["KEY_VOLUMEDOWN"]}
      ]
    }
  },
  "thermalZones": [
    {"type": "gpu-thmzone", "base-temp-mc": 30200, "jitter-mc": 220},
    {"type": "cpu-thmzone", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "board-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "pa-thmzone", "base-temp-mc": 29500, "jitter-mc": 180},
    {"type": "chg-thmzone", "base-temp-mc": 28800, "jitter-mc": 140}
  ],
  "hardwareHals": {
    "radioVariant": "default",
    "enableIr": false,
    "enableNfc": false,
    "vintfDrop": []
  },
  "vendorEtcConfigs": {
    "battery": {
      "technology": "Li-poly",
      "cycleCount": 32,
      "fullUah": 5000,
      "counterMin": 4952,
      "counterMax": 4986,
      "fullMv": 4384,
      "chargeMv": 4400,
      "temperatureFullDecic": 310,
      "temperatureChargeDecic": 380,
      "currentFullUa": -55000,
      "currentChargeUa": -1800000,
      "wireless": 0,
      "stateOfHealth": 99
    },
    "vibrator": {
      "motorType": "ERM (转子马达)",
      "registerManager": false,
      "capabilities": 0,
      "managerCapabilities": 0,
      "compositionDelayMax": 0,
      "compositionSizeMax": 0,
      "vibratorIds": [1],
      "effects": ["CLICK", "DOUBLE_CLICK", "TICK", "HEAVY_CLICK"]
    },
    "gnss": {
      "systemInfoName": "UNISOC_GNSS_default,GNSS_VER_default",
      "gnssCapabilities": 55,
      "powerCapabilities": 1,
      "measurementCorrectionsCapabilities": 0,
      "yearOfHw": 2020,
      "defaultSatellites": 8,
      "dualFrequencyL5": false
    },
    "thermal": {
      "skinName": "SKIN",
      "skinCelsius": 28.8,
      "modelMaximumCelsius": 50,
      "coolingDeviceName": "gpu-cooling",
      "sensors": [
        {"id": 0, "name": "CPU", "value": 31.5, "thresholds": "nan;nan;nan;80;85;95;115", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 1, "name": "GPU", "value": 30.2, "thresholds": "nan;nan;nan;80;85;95;115", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 2, "name": "BATTERY", "value": 28.8, "thresholds": "nan;nan;nan;45;50;55;60", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 3, "name": "SKIN", "value": 28.8, "thresholds": "nan;40;42;45;60;70;80", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 5, "name": "POWER_AMPLIFIER", "value": 29.5, "thresholds": "nan;nan;nan;65;85;95;105", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 13, "name": "SOC", "value": 28.8, "thresholds": "nan;nan;nan;80;85;95;115", "shutdown": "nan;nan;nan;nan;nan;nan;nan"}
      ]
    },
    "lightEntries": [
      {"id": 0, "ordinal": 0, "type": "BACKLIGHT"},
      {"id": 1, "ordinal": 0, "type": "KEYBOARD"},
      {"id": 2, "ordinal": 0, "type": "BUTTONS"},
      {"id": 3, "ordinal": 0, "type": "BATTERY"},
      {"id": 4, "ordinal": 0, "type": "NOTIFICATIONS"},
      {"id": 5, "ordinal": 0, "type": "ATTENTION"}
    ],
    "sensors": [
      {"handle": 1, "name": "bmi160_acc", "vendor": "Bosch", "version": 1, "typeId": 1, "type": "android.sensor.accelerometer", "maxRange": "39.226600", "resolution": "0.001197", "power": "0.130", "minDelay": 5000, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 5, "name": "stk3x1x_l", "vendor": "Sensortek", "version": 1, "typeId": 5, "type": "android.sensor.light", "maxRange": "65535.0", "resolution": "1.0", "power": "0.150", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 8, "name": "stk3x1x_p", "vendor": "Sensortek", "version": 1, "typeId": 8, "type": "android.sensor.proximity", "maxRange": "5.0", "resolution": "1.0", "power": "0.150", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 1000000, "wakeUp": true, "reporting": 1},
      {"handle": 9, "name": "GRAVITY", "vendor": "SPRD", "version": 1, "typeId": 9, "type": "android.sensor.gravity", "maxRange": "39.226600", "resolution": "0.001197", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 10, "name": "LINEARACCEL", "vendor": "SPRD", "version": 1, "typeId": 10, "type": "android.sensor.linear_acceleration", "maxRange": "39.226600", "resolution": "0.001197", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 17, "name": "SIGNIFICANT_MOTION", "vendor": "SPRD", "version": 1, "typeId": 17, "type": "android.sensor.significant_motion", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 18, "name": "STEP_DETECTOR", "vendor": "SPRD", "version": 1, "typeId": 18, "type": "android.sensor.step_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 0, "wakeUp": false, "reporting": 3},
      {"handle": 19, "name": "STEP_COUNTER", "vendor": "SPRD", "version": 1, "typeId": 19, "type": "android.sensor.step_counter", "maxRange": "2147483648.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 22, "name": "TILT_DETECTOR", "vendor": "SPRD", "version": 1, "typeId": 22, "type": "android.sensor.tilt_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 3},
      {"handle": 27, "name": "DEVICE_ORIENTATION", "vendor": "SPRD", "version": 1, "typeId": 27, "type": "android.sensor.device_orientation", "maxRange": "3.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 35, "name": "UNCALI_ACC", "vendor": "SPRD", "version": 1, "typeId": 35, "type": "android.sensor.accelerometer_uncalibrated", "maxRange": "39.226600", "resolution": "0.001197", "power": "0.001", "minDelay": 5000, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0}
    ],
    "audioEffects": [
      {"name": "Acoustic Echo Canceler", "implementor": "The Android Open Source Project", "type": "7b491460-8d4d-11e0-bd61-0002a5d5c51b", "uuid": "a4a00091-9087-426b-9d60-af3a70f6a20a", "connect": "Pre Processing"},
      {"name": "Auxiliary Environmental Reverb", "implementor": "NXP Software Ltd.", "type": "c2e5d5f0-94bd-4763-9cac-4e234d06839e", "uuid": "4a387fc0-8ab3-11df-8bad-0002a5d5c51b", "connect": "Auxiliary"},
      {"name": "Auxiliary Preset Reverb", "implementor": "NXP Software Ltd.", "type": "47382d60-ddd8-11db-bf3a-0002a5d5c51b", "uuid": "f29a1400-a3bb-11df-8ddc-0002a5d5c51b", "connect": "Auxiliary"},
      {"name": "Dynamic Bass Boost", "implementor": "NXP Software Ltd.", "type": "0634f220-ddd4-11db-a0fc-0002a5d5c51b", "uuid": "8631f300-72e2-11df-b57e-0002a5d5c51b", "connect": "Insert"},
      {"name": "DynamicsProcessing", "implementor": "The Android Open Source Project", "type": "7261676f-6d75-7369-6364-28e2fd3ac39e", "uuid": "e0e6539b-1781-7261-676f-6d7573696340", "connect": "Insert"},
      {"name": "EqualizerBundle", "implementor": "NXP Software Ltd.", "type": "0bed4300-ddd6-11db-8f34-0002a5d5c51b", "uuid": "ce772f20-847d-11df-bb17-0002a5d5c51b", "connect": "Insert"},
      {"name": "Insert Environmental Reverb", "implementor": "NXP Software Ltd.", "type": "c2e5d5f0-94bd-4763-9cac-4e234d06839e", "uuid": "c7a511a0-a3bb-11df-860e-0002a5d5c51b", "connect": "Insert"},
      {"name": "Insert Preset Reverb", "implementor": "NXP Software Ltd.", "type": "47382d60-ddd8-11db-bf3a-0002a5d5c51b", "uuid": "172cdf00-a3bc-11df-a72f-0002a5d5c51b", "connect": "Insert"},
      {"name": "Loudness Enhancer", "implementor": "The Android Open Source Project", "type": "fe3199be-aed0-413f-87bb-11260eb63cf1", "uuid": "fa415329-2034-4bea-b5dc-5b381c8d1e2c", "connect": "Insert"},
      {"name": "Multichannel Downmix To Stereo", "implementor": "The Android Open Source Project", "type": "381e49cc-a858-4aa2-87f6-e8388e7601b2", "uuid": "93f04452-e4fe-41cc-91f9-e475b6d1d69f", "connect": "Insert"},
      {"name": "Virtualizer", "implementor": "NXP Software Ltd.", "type": "37cc2c00-dddd-11db-8577-0002a5d5c51b", "uuid": "1d4033c0-8557-11df-9f2d-0002a5d5c51b", "connect": "Insert"},
      {"name": "Visualizer", "implementor": "The Android Open Source Project", "type": "e46b26a0-dddd-11db-8afd-0002a5d5c51b", "uuid": "d069d9e0-8329-11df-9168-0002a5d5c51b", "connect": "Insert"},
      {"name": "Volume", "implementor": "NXP Software Ltd.", "type": "09e8ede0-ddde-11db-b4f6-0002a5d5c51b", "uuid": "119341a0-8469-11df-81f9-0002a5d5c51b", "connect": "Insert"}
    ],
    "media": {
      "instances": [
        {"binary": "android.hardware.media.c2@1.1-unisoc-service", "store": "rank=1"}
      ],
      "components": [
        {"store": "rank=1", "name": "c2.unisoc.jpeg.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/jpeg"},
        {"store": "rank=1", "name": "c2.unisoc.imaadpcm.decoder", "kind": "decoder", "domain": "audio", "mediaType": "audio/ima-adpcm"},
        {"store": "rank=1", "name": "c2.unisoc.mp3.decoder", "kind": "decoder", "domain": "audio", "mediaType": "audio/mpeg"},
        {"store": "rank=1", "name": "c2.unisoc.mpl1.decoder", "kind": "decoder", "domain": "audio", "mediaType": "audio/mpeg-L1"},
        {"store": "rank=1", "name": "c2.unisoc.mpl2.decoder", "kind": "decoder", "domain": "audio", "mediaType": "audio/mpeg-L2"},
        {"store": "rank=1", "name": "c2.unisoc.avc.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/avc", "aliases": "OMX.sprd.h264.decoder", "maxSize": "1920x3840", "profiles": "20480,20481,20482,20484,20487", "levels": "20495"},
        {"store": "rank=1", "name": "c2.unisoc.hevc.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/hevc", "aliases": "OMX.sprd.hevc.decoder", "maxSize": "1920x3840", "profiles": "24576,24578,24577", "levels": "24835"},
        {"store": "rank=1", "name": "c2.unisoc.mpeg4.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/mp4v-es", "aliases": "OMX.sprd.mpeg4.decoder", "maxSize": "1920x2032", "profiles": "12297,12308", "levels": "16393"},
        {"store": "rank=1", "name": "c2.unisoc.h263.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/3gpp", "aliases": "OMX.sprd.h263.decoder", "maxSize": "1408x1152", "profiles": "12288", "levels": "12291,12292,12293,12294,12295"},
        {"store": "rank=1", "name": "c2.unisoc.vp8.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/x-vnd.on2.vp8", "aliases": "OMX.sprd.vpx.decoder", "maxSize": "1920x1088", "profiles": "40960", "levels": "1"},
        {"store": "rank=1", "name": "c2.unisoc.vp9.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/x-vnd.on2.vp9", "aliases": "OMX.sprd.vp9.decoder", "maxSize": "1920x3840", "profiles": "28672,28673,28674", "levels": "28680"},
        {"store": "rank=1", "name": "c2.unisoc.jpeg.encoder", "kind": "encoder", "domain": "video", "mediaType": "video/jpeg"},
        {"store": "rank=1", "name": "c2.unisoc.mp3.encoder", "kind": "encoder", "domain": "audio", "mediaType": "audio/mpeg"},
        {"store": "rank=1", "name": "c2.unisoc.avc.encoder", "kind": "encoder", "domain": "video", "mediaType": "video/avc", "aliases": "OMX.sprd.h264.encoder", "maxSize": "1920x1920", "profiles": "20480,20482,20484", "levels": "20494"},
        {"store": "rank=1", "name": "c2.unisoc.hevc.encoder", "kind": "encoder", "domain": "video", "mediaType": "video/hevc", "aliases": "OMX.sprd.h265.encoder", "maxSize": "1920x1088", "profiles": "24576", "levels": "24834"}
      ]
    },
    "audioEngine": {
      "modules": "default,bluetooth,r_submix",
      "voiceCallIndexMax": 7,
      "musicIndexMax": 15
    }
  },
  "alignmentNotes": [
    "平台为 Unisoc（展锐）SC9863A，非 MTK：radioVariant 取 default，VINTF 保留实测 vendor.unisoc.hardware.radio.* 与 vendor.sprd.hardware.* 全部占位符，未改写成 mtk 分支。",
    "CPU 实测仅 policy0 单簇 0-7、capacity 1024，按单簇 A55 建模；最高频取 vendor/etc 频点线索的 1800000 kHz，17 档为展锐 DVFS 常见档位，未虚构 2.0GHz big 簇。",
    "GPU 为 Imagination PowerVR Rogue GE8322：GL 串按 PowerVR DDK 1.13 生成，扩展仅保留 IMG/EXT 系；Vulkan 依 permissions 实测 vulkanLevel=0、vulkanVersion=1.3 落 0x00403116，未套用 gold 的 Mali 值。",
    "传感器严格按实测能力位 accelerometer/light/proximity/stepcounter/stepdetector 生成 11 项：物理三件套（bmi160_acc + stk3x1x 光感/距感）加加速度派生融合与计步项；未编造陀螺仪、磁力计、气压计，也没有 xiaomi.sensor.*/oplus.sensor.*/vivo.sensor.* 等品牌私有 sensor，且无 swtp。",
    "touch 用实测 focaltech_ats（FocalTech），按键 gpio-keys，PMIC 键 sprd-pmic-keys，声卡插孔按展锐 sprdphone 命名；指纹按 FPC 侧边电容方案（goodixFp=false），inputIdentity 未出现小米专属节点。",
    "thermal.zones 五项直接取实测 gpu/cpu/board/pa/chg 温区的 base-temp-mc 与 jitter-mc；thermal_config.xml 的 SKIN 基温与分级阈值按展锐低端平台常见区间（80/85/95/115℃ 结温、45/50/55/60℃ 电池）配置。",
    "电池按 5000mAh 单电芯 Li-poly、4.384V 满电、10W 有线、无无线充；NFC 与红外均按实测 hasNfc=false、hasConsumerIr=false 关闭。",
    "gold 的 ro.vendor.cdh.display.edid_* / refresh_rates / wide_color_gamut 映射到 display.edid*、refreshRatesHz、wideColorGamut；glVersionFull/glExtraExts/vulkan*/widevine*/imeiTac/macOui 均已落到对应字段，未直接复制 gold 的 MT6833/Mali 专属串。",
    "待真机复核项：EDID 面板 ID 与年份周（ili9881c 为推定面板）、imeiTac、Wi-Fi MAC OUI、Widevine systemId 为品牌级推定值；固件 hasEarpiece/hasWiredHeadset=false 与直板机形态矛盾，暂按保留 3.5mm 插孔与耳机插孔节点处理，需以真机 /proc/bus/input/devices、dumpsys media.audio_flinger、getprop ro.vendor.cdh.* 复核后固化。"
  ],
  "sources": [
    "目标机固件实测数据（build.prop、boot/DTB、VINTF、vendor/etc、odm/etc 解包结果）",
    "Unisoc SC9863A 平台公开规格：8x Cortex-A55 最高 1.8GHz、PowerVR GE8322、28nm",
    "Imagination PowerVR Rogue DDK 1.13（OpenGL ES 3.2）与 Vulkan 能力参考",
    "Android 13 VINTF/feature 约定：vulkan.version 0x00403116 对应 1.3，vulkan.level=0 为基线",
    "Bing 检索 Symphony MAX 10（未取得权威规格页，仅作交叉确认，结论仍以固件实测为准）"
  ]
}
```
