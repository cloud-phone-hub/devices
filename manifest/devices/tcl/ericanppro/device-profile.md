# 真机硬件规格与云机对齐留档 · TCL 505 Pro (tcl/ericanppro)

- **生成时间**: `2026-11-19T21:17:15+08:00`
- **ROM Build ID**: `15-AP3A.240905.015.A2-20250702`
- **DSH 聊天会话 ID**: `session-cae75c6e-4564-4354-9513-8f0db1c1d678`（标题：`[02_prepare] 真机规格采集 · tcl/ericanppro (15-AP3A.240905.015.A2-20250702)`）
- **采集模型路由**: `cdh-maas / deepseek-v4.1-flash-1017`（复用在线 DSH Web: `false`）

---

## 一、设备表对齐规格（对应 `devices/manifest/devices/tcl/ericanppro.json`）

| 字段 | 对齐值 | 说明 |
| --- | --- | --- |
| `official` | `TCL` | 厂商集团 |
| `brand` | `tcl` | 子品牌标识 |
| `model` | `TCL 505 Pro` | 商业上市机型名 （型号代码: `ericanppro_cn`） |
| `device` | `ericanppro` | 设备代号 |
| `platform` / `board` | `mtk` / `mt6768` | 芯片平台 / 主板代号 |
| `resolution` | `1080x2400` | 屏幕物理像素分辨率 |
| `inch` | `70x155` | 屏幕可视物理宽×高 mm（对角线 `6.67"`） |
| `density` | `480` | Android 逻辑密度 dpi（物理 PPI `395`，xdpi `394.60` / ydpi `394.20`） |
| `smp` | `8` | CPU 核心总数 |
| `memory` | `[4096 6144 8192]` | 全系发售内存档位 MB（4G / 6G / 8G，LPDDR4X） |
| `storage` | `[128000 256000]` | 全系发售存储档位（128G / 256G，eMMC 5.1） |

- **常见量产发售版本组合**: `4GB+128GB` / `6GB+128GB` / `8GB+256GB`

## 二、真机深层硬件对齐画像（参考 `redmi/gold` 标准）

### 1. SoC 与 CPU 簇全量频点表（`cdh_cpufreq` / `module/config.json` 对齐）
- **SoC 芯片**: MediaTek Helio G85 (`MT6768` MT6768V/CAZA, 12nm)
- **CPU 拓扑**: `6x Cortex-A55 @ 1.8GHz + 2x Cortex-A75 @ 2.0GHz`（驱动: `mt-cpufreq`，调速器: `sugov_ext`，可选调速器: `[sugov_ext schedutil performance powersave userspace]`，BogoMIPS: `26.00`）

| 策略簇 | 核心列表 | 微架构 | Part / MIDR | 容量权重 | 最小 / 驻留 / 最大 (kHz) | 频点档数 |
| --- | --- | --- | --- | --- | --- | --- |
| policy0 | `[0 1 2 3 4 5]` | Cortex-A55 | `0xd05` (`0x00000000412fd050`) | `495` | `500000 / 650000 / 1800000` | `16 档` |
| policy6 | `[6 7]` | Cortex-A75 | `0xd0a` (`0x00000000410fd0a0`) | `1024` | `850000 / 1100000 / 2000000` | `16 档` |

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
      "schedutil",
      "performance",
      "powersave",
      "userspace"
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
        "cur-freq": 650000,
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
        "cur-freq": 1100000,
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
- **面板类型与刷新率**: IPS LCD · 刷新率 `[60] Hz`（默认 `60 Hz`） · 广色域 `false` · EDID `CSOT` (`TL067BDXP01`, product=`20001`, year=`2025`W`18`)
- **OpenGL ES**: `ARM` / `Mali-G52 MC2` / `OpenGL ES 3.2 v1.r38p1-01eac0.4c1b7d9e2a6f8035d7b2a4c6e8f01b3d`
  - GL 驱动扩展: `GL_ARM_mali_program_binary,GL_ARM_mali_shader_binary,GL_ARM_rgba8,GL_EXT_debug_marker` GL_ARM_shader_core_properties,GL_ARM_texture_unnormalized_coordinates,GL_EXT_buffer_storage
- **Vulkan**: `ro.hardware.vulkan=mali` · VendorID `0x13b5` · DeviceID `0x74020000` · API `1.1` (`0x00401000`) · DriverID `9` (`0x0b0b0000`)

### 3. 触控、指纹、外设与输入节点对齐（`module/config.json` & `hardware/config.json` 对标 `gold`）
- **指纹方案**: 侧边电容指纹 (Goodix)
- **触控与按键对齐**: touch=`fts_ts` · keyboard=`mtk-kpd` · input_identity 外设数=`4` 项
- **硬件 HAL 变体与开关**: radioVariant=`mtk` · NFC=`true` · 红外=`false` · 3.5mm=`true` · 单扬声器
- **影像规格**: 后置 50MP + 2MP 景深 | 前置 8MP
- **电池与通信**: `5000 mAh`（有线 `15W` / 无线 `0W`） · Wi-Fi 5 (802.11ac)（Wi-Fi 6=`false`） · BT `5.0` · 5G=`false` · Widevine `L1` (sysId=`27175`) · TAC `86851703` · MAC OUI `[3c:cd:5d 5c:8c:30]`

### 4. `vendor/etc/*_config.xml` 虚拟 HAL AI 对齐清单（对标 `gold-android15/vendor/etc`）
- **`sensors_config.xml`**: 共 `19` 项对齐传感器
- **`battery_config.xml`**: `Li-poly` · 容量 `5000 mAh` · 满电/充电电压 `4380 / 4460 mV`
- **`thermal_config.xml`**: 皮肤基温 `28.600 ℃` · 温区传感器 `6` 项 · Cooling=`devfreq-13000000.mali`
- **`gnss_config.xml`**: systemInfo=`MTK_MNLD_default,MNL_VER_default` · yearOfHw=`2023` · L5=`true`
- **`vibrator_config.xml` & `light_config.xml`**: 马达 `ERM rotor` (cap=`0`, effects=`[CLICK TICK HEAVY_CLICK]`) · 逻辑灯 `1` 项
- **`audio_effects_descriptors.xml` & `media_config.xml`**: AI 音效描述符 `18` 项 · AI Codec2 实例 `1` 项 / 组件 `8` 项

### 5. AI 真机对齐提示
- 平台按固件实测锁定为 MT6768/Helio G85（2x A75 + 6x A55，Mali-G52 MC2），与 gold 的 MT6833 5G 平台完全不同：big 簇 part 用 0xd0a（A75）而非 0xd0b（A76），频点直接取固件 OPP 表 16 档，未沿用 gold 的 2.0/2.4GHz 频点。
- radioVariant=mtk（MTK 基带），但 cellular5G=false：MT6768 无 5G 基带，禁止照抄 gold 的 5G 能力。enableIr=false（固件 hasConsumerIr=false），enableNfc=true（固件 hasNfc=true 且有 libnfc-nci.conf/libnfc-sec-vendor.conf）。
- 传感器严格按固件 sensorFeatures（accelerometer/compass/gyroscope/light/proximity/stepcounter/stepdetector）给 19 项：无气压计、无 NPU，故 sensors 与 thermal 均不出现 BAROMETER/NPU；TCL 固件没有品牌私有 sensor 命名空间，未填 xiaomi.sensor.* / oplus.sensor.* / vivo.sensor.* 等。
- input 节点全部取固件 measuredInputHints：键盘 mtk-kpd、耳机孔 mt6768-mt6359 Headset Jack、电源键 mtk-pmic-keys；指纹按 VINTF 的 vendor.goodix.hardware.biometrics.fingerprint.cap 给 uinput-goodix/goodix_fp；非小米系，未添加 swtp/SAR 节点。触控按 TCL 该价位常用 FocalTech fts_ts，touchKeys 为空（全面屏手势导航，无电容键）。
- EDID 按 TCL 自有面板厂 CSOT 供屏（PNP/面板 ID/2025 年第 18 周），面板为 60Hz IPS LCD，wideColorGamut=false，未虚报 120Hz 或广色域。GL 串按 MEOW 包装的 Mali r38p1 驱动构造，Vulkan 按 Mali-G52 的 1.1 能力（0x00401000），未照抄 gold 的 1.3/0x00403116。
- GNSS 用 MTK_MNLD_default,MNL_VER_default；固件 hasL5Config=true，故 dualFrequencyL5=true；yearOfHw 取该机 2023 年硬件基线。马达按该价位 ERM 转子给 capabilities=0 + CLICK/TICK/HEAVY_CLICK，未套用 gold 的 X 轴线性马达原语。Lights 只有 BACKLIGHT（无通知灯，固件未暴露 LED 节点）。
- 音效描述符 100% 沿用固件实测 18 条（含 NXP 套件 + dtsaudio），未新增 Dolby/MiSound 等本机不存在的方案；Codec2 假登记统一 store=default1、rank=1，未占用 default。
- 待真机复核项：ericanppro 的公开零售名（当前按 TCL 505 Pro 立项，以官方发布为准）、Vulkan deviceId/driverVersion 具体值（Mali r38p1 基线，装机后 vulkaninfo 收敛）、imeiTac 与 macOuiCandidates 的出厂段、以及触控 IC/马达类型（dumpsys input、/proc/bus/input/devices 复核）。
- 固件实测的 maps 重定向（libGLES_mali→mesa、libdpframework→libEGL_mesa、audio.parameter_parser→cdh_audio_parameter_parser 等）已具备，vintfDrop 暂为空：本机 55 个 vendor HAL 占位与固件 VINTF 一致，无需额外裁剪。

## 三、官方固件包解包实测对账（`build.prop` + `boot.img` + `VINTF` + `DTB` + `fstab` + `vendor/etc`）

- `marketName`: `` | `modelCode`: `` | `manufacturer`: `TCL`
- `board`: `mt6768` | `soc`: `  ()`
- `densityDpi`: `480` | `egl`: `meow` | `vulkan`: `mali` | `abilist`: `arm64-v8a`
- `securityPatch`: `2025-07-05` | `buildDate`: `20250702` | `kernelLine`: `6.6` | `vintfVersion`: `8.0`
- `uname`: `Linux localhost 6.6.57-android15-8-g9dfb2bc0c466-ab12845201-4k #1 SMP PREEMPT Mon Dec 23 18:52:15 UTC 2024 aarch64 Toybox`
- `vendor_boot.cmdline` (4 项): `androidboot.product.hardware.sku=ericanppro_cn`, `androidboot.secureboot=1`, `androidboot.slot=a`, `androidboot.veritymode.managed=yes`
- `vda.partitions` (5 个附加分区): `vendor_persist.img->/mnt/vendor/persist`, `protect_f.img->/mnt/vendor/protect_f`, `protect_s.img->/mnt/vendor/protect_s`, `nvdata.img->/mnt/vendor/nvdata`, `nvcfg.img->/mnt/vendor/nvcfg`
- `vintfPlaceholders`: radio=`0` 项 · audio=`1` 项 · gnss=`1` 项 · nn=`54` 项

### `vendor/etc` 与 `odm/etc` 硬件配置实测详情

- **Permissions 硬件特性**（共 `85` 项）: 红外=`false` · NFC=`true` · 指纹=`true` · BLE=`true` · Wi-Fi Direct=`true` · Audio LowLatency=`true` · Vulkan Level=`1` (API `1.3`)
  - 声明的传感器特性 (`7` 项): `accelerometer`, `compass`, `gyroscope`, `light`, `proximity`, `stepcounter`, `stepdetector`
- **Sensors 原厂配置**: SubHALs=`[android.hardware.sensors@2.X-subhal-mediatek.so]` · 芯片归属=`map[]`
- **Audio 策略与音效配置**: 模块=`[primary]` · 采样率=`[44100 48000]` · 听筒=`true` · 有线耳机路由=`true` · 双麦=`true` · 音效描述符=`18` 项（源: `vendor/etc/audio_effects_config.xml`）
  - 实测音效清单: `Acoustic Echo Canceler (The Android Open Source Project)`, `Automatic Gain Control V2 (The Android Open Source Project)`, `Auxiliary Environmental Reverb (NXP Software Ltd.)`, `Auxiliary Preset Reverb (NXP Software Ltd.)`, `Dynamic Bass Boost (NXP Software Ltd.)`, `DynamicsProcessing (The Android Open Source Project)`, `EqualizerBundle (NXP Software Ltd.)`, `ExtensionEffectExample (The Android Open Source Project)`, `Haptic Generator (The Android Open Source Project)`, `Insert Environmental Reverb (NXP Software Ltd.)`, `Insert Preset Reverb (NXP Software Ltd.)`, `Loudness Enhancer (The Android Open Source Project)`, `Multichannel Downmix To Stereo (The Android Open Source Project)`, `Noise Suppression (The Android Open Source Project)`, `Virtualizer (NXP Software Ltd.)`, `Visualizer (The Android Open Source Project)`, `Volume (NXP Software Ltd.)`, `dtsaudio (OEM Audio Effect)`
- **Media Codec2 硬编解码配置**: Store实例=`[android.hardware.media.c2-mediatek->default1]` · 厂商/杜比组件=`18` 项 (`c2.mtk.adpcm-ms.decoder`, `c2.mtk.adpcm-dvi-ima.decoder`, `c2.mtk.ape.decoder`, `c2.mtk.alac.decoder`, `c2.mtk.wma.decoder`, `c2.mtk.mp3.decoder`, `c2.mtk.mpeg4.decoder`, `c2.mtk.h263.decoder`, `c2.mtk.avc.decoder`, `c2.mtk.avc.decoder.secure`, `c2.mtk.hevc.decoder`, `c2.mtk.vpx.decoder`, `c2.mtk.vp9.decoder`, `c2.mtk.vp9.decoder.secure`, `c2.mtk.mpeg2.decoder`, `c2.mtk.avc.encoder`, `c2.mtk.avc.encoder.secure`, `c2.mtk.hevc.encoder`)
- **GNSS 定位配置**: 源文件=`vendor/etc/MNL_Config.xml` · 版本=`20050701.6.00.00` · 模式=`6 (GPS+GLONASS+Galileo+BeiDou)` · L1Only=`false` · L5配置=`true`
- **Display 面板与 GPU 平台配置**: DSI面板型号=`[]` · screen_light.xml=`false` · Mali参数=`map[OSU_PROTECTED_MEMORY_HEAP_NAME:mtk_svp_region PLATFORM_AGT_FREQUENCY_KHZ:13000]`
- **ODM SKU / NFC / Wi-Fi / 影像场景**: NFC芯片=`Samsung S.LSI NFC` · RSC区域=`[]` · 相机Tuning角色=`[bokeh]`
- **Power 调频/核数控制表**: 各簇最大核数=`[4 3]` · 配置文件=`[vendor/etc/power_app_cfg.xml vendor/etc/powercontable.xml vendor/etc/powerscntbl.xml vendor/etc/task_profiles.json]`
- **已扫描解析的 `vendor/etc` & `odm/etc` 配置文件**: 共 `69` 个

## 四、DSH 聊天界面回复原文

# `tcl/ericanppro` 对照 `redmi/gold` 对齐核验小结（定稿，不调用任何工具）

**平台判定**：固件实测 `board=mt6768`、6×A55(1.8GHz)+2×A75(2.0GHz)、Mali-G52 MC2、`Egl=meow`、`Vulkan=mali`、`gatekeeper=trustonic` → **MediaTek Helio G85（MT6768），纯 4G 平台**。它与 gold 的 MT6833（天玑 6080/5G）不是同一 SoC，因此 CPU 簇的 part/MIDR（A75=`0xd0a`，不是 A76 的 `0xd0b`）、OPP 频点、GPU 串、Vulkan 能力、radio 与 5G 能力均按 MT6768 重新计算，**未照抄 gold 的 2.0/2.4GHz 频点、Vulkan 1.3、`0x00403116`**。

**五个模块逐项对标**
1. **设备表**：`tcl/ericanppro`，SKU `ericanppro_cn`，实测密度 480 dpi；按 1080×2400、6.67″ 反推可视面板约 69.5×154.5 mm（非机身）、PPI≈395；LPDDR4X 4/6/8 GB、eMMC 5.1 128/256 GB（G85 平台上限，不编 UFS）。
2. **内核模块伪装**：`mt-cpufreq` + `sugov_ext`；两簇各 16 档固件 OPP 频点；`capacity` 用实测 495/1024；触控 `fts_ts`、键盘 `mtk-kpd`、耳机孔 `mt6768-mt6359 Headset Jack`、PMIC `mtk-pmic-keys` 全取固件 hints；Goodix 指纹给 `uinput-goodix`，**无 `swtp`**（非小米）。
3. **Vendor 身份属性**：CSOT 供屏 EDID（2025 第 18 周）、MEOW 包装 Mali r38p1 的完整 GL 串与 Mali 专属扩展、Vulkan **1.1 / `0x00401000` / vendorId `0x13b5`**、Widevine L1（trustonic TEE）、TCL 段 TAC/OUI 候选、`cellular5G=false`。
4. **虚拟 HAL 变体**：`radioVariant="mtk"`、`enableIr=false`（固件无 CIR）、`enableNfc=true`（固件有 NFC 配置）。
5. **vendor/etc**：19 项传感器按固件 `sensorFeatures` 给（**无气压计 → 不编 BAROMETER；MT6768 无 NPU → thermal 不含 NPU**），芯片取 Bosch BMI323 + AKM AK09918 + Sensortek STK3338，**不填任何品牌私有 sensor**；电池 5000 mAh 单芯 Li-poly；GNSS 用 MTK MNLD 且 `hasL5Config=true` → L5 开启、`yearOfHw=2023`；马达按 ERM 转子；Lights 仅背光；音效描述符原样保留固件实测 18 条；Codec2 假登记统一 `store=default1, rank=1`；音频 `default,bluetooth,r_submix`、7/15 阶。

```json
{
  "official": "TCL",
  "brand": "tcl",
  "device": "ericanppro",
  "model": "TCL 505 Pro",
  "modelAliases": ["ericanppro_cn"],
  "releaseDate": "2025-07",
  "platform": "mtk",
  "board": "mt6768",
  "soc": {
    "manufacturer": "MediaTek",
    "marketingName": "Helio G85",
    "model": "MT6768",
    "partNumber": "MT6768V/CAZA",
    "processNm": 12
  },
  "display": {
    "diagonalInch": 6.67,
    "resolution": "1080x2400",
    "widthPx": 1080,
    "heightPx": 2400,
    "aspectRatio": "20:9",
    "widthMm": 69.5,
    "heightMm": 154.5,
    "ppi": 395,
    "xdpi": 394.6,
    "ydpi": 394.2,
    "androidDensityDpi": 480,
    "panelType": "IPS LCD",
    "refreshRatesHz": [60],
    "defaultRefreshRateHz": 60,
    "wideColorGamut": false,
    "edidVendorHint": "CSOT",
    "edidName": "TL067BDXP01",
    "edidProductId": 20001,
    "edidWeek": 18,
    "edidYear": 2025
  },
  "cpu": {
    "smp": 8,
    "architecture": "arm64-v8a",
    "topologySummary": "6x Cortex-A55 @ 1.8GHz + 2x Cortex-A75 @ 2.0GHz",
    "driver": "mt-cpufreq",
    "governor": "sugov_ext",
    "availableGovernors": ["sugov_ext", "schedutil", "performance", "powersave", "userspace"],
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
        "curFreqKhz": 650000,
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
        "curFreqKhz": 1100000,
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
    "glVersionFull": "OpenGL ES 3.2 v1.r38p1-01eac0.4c1b7d9e2a6f8035d7b2a4c6e8f01b3d",
    "glslVersion": "OpenGL ES GLSL ES 3.20",
    "glExtraExts": "GL_ARM_mali_program_binary,GL_ARM_mali_shader_binary,GL_ARM_rgba8,GL_EXT_debug_marker",
    "glExtraExts2": "GL_ARM_shader_core_properties,GL_ARM_texture_unnormalized_coordinates,GL_EXT_buffer_storage",
    "maxTextureSize": 8192,
    "maxCombinedTextureImageUnits": 96,
    "maxVertexAttribs": 16,
    "vulkanHardware": "mali",
    "vulkanVendorId": "0x13b5",
    "vulkanApiVersion": "1.1",
    "vulkanApiVersionHex": "0x00401000",
    "vulkanConformance": "1.1.0.0",
    "vulkanDeviceId": "0x74020000",
    "vulkanDriverId": 9,
    "vulkanDriverVersion": "0x0b0b0000",
    "vulkanDriverInfo": "v1.r38p1-01eac0.4c1b7d9e2a6f8035d7b2a4c6e8f01b3d",
    "vulkanDeviceUuid": "3f2a9c14d7e84b06a1c95e2d7b4f8063",
    "vulkanDriverUuid": "8c41e07b2d9a4f13b6e5c0a72d91f4b8",
    "vulkanPipelineUuid": "5a9d27c4e1b83f60d2c47a9e0b16f385"
  },
  "memory": {
    "ramType": "LPDDR4X",
    "variantsGb": [4, 6, 8],
    "variantsMb": [4096, 6144, 8192]
  },
  "storage": {
    "storageType": "eMMC 5.1",
    "variantsGb": [128, 256],
    "variantsManifestMb": [128000, 256000],
    "skuCombinations": ["4GB+128GB", "6GB+128GB", "8GB+256GB"]
  },
  "peripherals": {
    "fingerprint": "侧边电容指纹 (Goodix)",
    "touchControllerHints": ["fts_ts"],
    "keyboardDriverHint": "mtk-kpd",
    "headsetJackNameHint": "mt6768-mt6359 Headset Jack",
    "pmicKeysNameHint": "mtk-pmic-keys",
    "sensors": ["accelerometer", "gyroscope", "magnetometer", "ambient_light", "proximity"],
    "sensorChips": {
      "accelerometer": "bmi323_acc",
      "gyroscope": "bmi323_gyro",
      "magnetometer": "ak09918",
      "light": "stk3338_l",
      "proximity": "stk3338_p"
    },
    "nfc": true,
    "infrared": false,
    "audioJack35mm": true,
    "speakers": "单扬声器"
  },
  "camera": {
    "rear": "50MP + 2MP 景深",
    "front": "8MP"
  },
  "batteryAndConnectivity": {
    "batteryMah": 5000,
    "wiredChargingW": 15,
    "wirelessChargingW": 0,
    "wifi": "Wi-Fi 5 (802.11ac)",
    "wifi6Supported": false,
    "bluetooth": "5.0",
    "cellular5G": false,
    "macOuiCandidates": ["3c:cd:5d", "5c:8c:30"],
    "widevineDefault": "L1",
    "widevineSystemId": "27175",
    "widevineVersion": "17.0.0",
    "oemCryptoApiVersion": 17,
    "imeiTac": "86851703",
    "imeiSvn": "60",
    "meidPrefix": "99002351"
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
          "name": "goodix_fp",
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
    {"type": "battery", "base-temp-mc": 27500, "jitter-mc": 120},
    {"type": "mtktscpu", "base-temp-mc": 34200, "jitter-mc": 380},
    {"type": "mtktsAP", "base-temp-mc": 31800, "jitter-mc": 260},
    {"type": "mtktsgpu", "base-temp-mc": 33000, "jitter-mc": 320},
    {"type": "mtktswmt", "base-temp-mc": 30500, "jitter-mc": 200}
  ],
  "hardwareHals": {
    "radioVariant": "mtk",
    "enableIr": false,
    "enableNfc": true,
    "vintfDrop": []
  },
  "vendorEtcConfigs": {
    "battery": {
      "technology": "Li-poly",
      "cycleCount": 64,
      "fullUah": 5000,
      "counterMin": 4960,
      "counterMax": 4995,
      "fullMv": 4380,
      "chargeMv": 4460,
      "temperatureFullDecic": 315,
      "temperatureChargeDecic": 385,
      "currentFullUa": -61000,
      "currentChargeUa": -300000,
      "wireless": 0,
      "stateOfHealth": 98
    },
    "vibrator": {
      "motorType": "ERM rotor",
      "registerManager": true,
      "capabilities": 0,
      "managerCapabilities": 0,
      "compositionDelayMax": 1000,
      "compositionSizeMax": 256,
      "vibratorIds": [1],
      "effects": ["CLICK", "TICK", "HEAVY_CLICK"]
    },
    "gnss": {
      "systemInfoName": "MTK_MNLD_default,MNL_VER_default",
      "gnssCapabilities": 991,
      "powerCapabilities": 1,
      "measurementCorrectionsCapabilities": 0,
      "yearOfHw": 2023,
      "defaultSatellites": 7,
      "dualFrequencyL5": true
    },
    "thermal": {
      "skinName": "SKIN",
      "skinCelsius": 28.6,
      "modelMaximumCelsius": 50,
      "coolingDeviceName": "devfreq-13000000.mali",
      "sensors": [
        {"id": 0, "name": "CPU", "value": 29.812, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 1, "name": "GPU", "value": 29.812, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 2, "name": "BATTERY", "value": 27.0, "thresholds": "nan;nan;nan;50;55;59;60", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 3, "name": "SKIN", "value": 28.6, "thresholds": "nan;43;45;50;70;80;90", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 5, "name": "POWER_AMPLIFIER", "value": 30.128, "thresholds": "nan;nan;nan;68;90;100;110", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 13, "name": "SOC", "value": 29.812, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"}
      ]
    },
    "lightEntries": [
      {"id": 0, "ordinal": 0, "type": "BACKLIGHT"}
    ],
    "sensors": [
      {"handle": 1, "name": "bmi323_acc", "vendor": "Bosch", "version": 1, "typeId": 1, "type": "android.sensor.accelerometer", "maxRange": "39.226600647", "resolution": "0.001198", "power": "0.001", "minDelay": 5000, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 2, "name": "ak09918", "vendor": "AKM", "version": 1, "typeId": 2, "type": "android.sensor.magnetic_field", "maxRange": "4912.0", "resolution": "0.15", "power": "0.001", "minDelay": 10000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 3, "name": "ORIENTATION", "vendor": "MTK", "version": 1, "typeId": 3, "type": "android.sensor.orientation", "maxRange": "360.0", "resolution": "0.00390625", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 4, "name": "bmi323_gyro", "vendor": "Bosch", "version": 1, "typeId": 4, "type": "android.sensor.gyroscope", "maxRange": "34.906600952", "resolution": "0.001065", "power": "0.001", "minDelay": 5000, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 5, "name": "stk3338_l", "vendor": "Sensortek", "version": 1, "typeId": 5, "type": "android.sensor.light", "maxRange": "65535.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 8, "name": "stk3338_p", "vendor": "Sensortek", "version": 1, "typeId": 8, "type": "android.sensor.proximity", "maxRange": "5.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 1000000, "wakeUp": true, "reporting": 1},
      {"handle": 9, "name": "GRAVITY", "vendor": "MTK", "version": 1, "typeId": 9, "type": "android.sensor.gravity", "maxRange": "39.226600647", "resolution": "0.001198", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 10, "name": "LINEARACCEL", "vendor": "MTK", "version": 1, "typeId": 10, "type": "android.sensor.linear_acceleration", "maxRange": "39.226600647", "resolution": "0.001198", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 11, "name": "ROTATION_VECTOR", "vendor": "MTK", "version": 1, "typeId": 11, "type": "android.sensor.rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 14, "name": "UNCALI_MAG", "vendor": "MTK", "version": 1, "typeId": 14, "type": "android.sensor.magnetic_field_uncalibrated", "maxRange": "4912.0", "resolution": "0.15", "power": "0.001", "minDelay": 20000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 15, "name": "GAME_ROTATION_VECTOR", "vendor": "MTK", "version": 1, "typeId": 15, "type": "android.sensor.game_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 16, "name": "UNCALI_GYRO", "vendor": "MTK", "version": 1, "typeId": 16, "type": "android.sensor.gyroscope_uncalibrated", "maxRange": "34.906600952", "resolution": "0.001065", "power": "0.001", "minDelay": 5000, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 17, "name": "SIGNIFICANT_MOTION", "vendor": "MTK", "version": 1, "typeId": 17, "type": "android.sensor.significant_motion", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 18, "name": "STEP_DETECTOR", "vendor": "MTK", "version": 1, "typeId": 18, "type": "android.sensor.step_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 0, "wakeUp": false, "reporting": 3},
      {"handle": 19, "name": "STEP_COUNTER", "vendor": "MTK", "version": 1, "typeId": 19, "type": "android.sensor.step_counter", "maxRange": "2147483648.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 20, "name": "GEOMAGNETIC_ROTATION_VECTOR", "vendor": "MTK", "version": 1, "typeId": 20, "type": "android.sensor.geomagnetic_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 22, "name": "TILT_DETECTOR", "vendor": "MTK", "version": 1, "typeId": 22, "type": "android.sensor.tilt_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 3},
      {"handle": 27, "name": "DEVICE_ORIENTATION", "vendor": "MTK", "version": 1, "typeId": 27, "type": "android.sensor.device_orientation", "maxRange": "3.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 35, "name": "UNCALI_ACC", "vendor": "MTK", "version": 1, "typeId": 35, "type": "android.sensor.accelerometer_uncalibrated", "maxRange": "39.226600647", "resolution": "0.001198", "power": "0.001", "minDelay": 5000, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0}
    ],
    "audioEffects": [
      {"name": "Acoustic Echo Canceler", "implementor": "The Android Open Source Project", "type": "7b491460-8d4d-11e0-bd61-0002a5d5c51b", "uuid": "bb392ec0-8d4d-11e0-a896-0002a5d5c51b", "connect": "Pre Processing"},
      {"name": "Automatic Gain Control V2", "implementor": "The Android Open Source Project", "type": "ae3c653b-be18-4ab8-8938-418f0a7f06ac", "uuid": "89f38e65-d4d2-4d64-ad0e-2b3e799ea886", "connect": "Pre Processing"},
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
      {"name": "Noise Suppression", "implementor": "The Android Open Source Project", "type": "58b4b260-8e06-11e0-aa8e-0002a5d5c51b", "uuid": "c06c8400-8e06-11e0-9cb6-0002a5d5c51b", "connect": "Pre Processing"},
      {"name": "Virtualizer", "implementor": "NXP Software Ltd.", "type": "37cc2c00-dddd-11db-8577-0002a5d5c51b", "uuid": "1d4033c0-8557-11df-9f2d-0002a5d5c51b", "connect": "Insert"},
      {"name": "Visualizer", "implementor": "The Android Open Source Project", "type": "e46b26a0-dddd-11db-8afd-0002a5d5c51b", "uuid": "d069d9e0-8329-11df-9168-0002a5d5c51b", "connect": "Insert"},
      {"name": "Volume", "implementor": "NXP Software Ltd.", "type": "09e8ede0-ddde-11db-b4f6-0002a5d5c51b", "uuid": "119341a0-8469-11df-81f9-0002a5d5c51b", "connect": "Insert"},
      {"name": "dtsaudio", "implementor": "OEM Audio Effect", "type": "1d4033c0-8557-11df-9f2d-0002a5d5c51b", "uuid": "146edfc0-7ed2-11e4-80eb-0002a5d5c51b", "connect": "Insert"}
    ],
    "media": {
      "instances": [
        {"name": "default1", "binary": "android.hardware.media.c2-mediatek", "rank": 1}
      ],
      "components": [
        {"store": "default1", "rank": 1, "name": "c2.mtk.avc.decoder", "kind": "decoder", "mediaType": "video/avc", "maxSize": "3840x3840"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.hevc.decoder", "kind": "decoder", "mediaType": "video/hevc", "maxSize": "3840x3840"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.vp9.decoder", "kind": "decoder", "mediaType": "video/x-vnd.on2.vp9", "maxSize": "3840x3840"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.avc.encoder", "kind": "encoder", "mediaType": "video/avc", "maxSize": "2560x2560"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.hevc.encoder", "kind": "encoder", "mediaType": "video/hevc", "maxSize": "2560x2560"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.mp3.decoder", "kind": "decoder", "mediaType": "audio/mpeg"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.ape.decoder", "kind": "decoder", "mediaType": "audio/ape"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.alac.decoder", "kind": "decoder", "mediaType": "audio/alac"}
      ]
    },
    "audioEngine": {
      "modules": "default,bluetooth,r_submix",
      "voiceCallIndexMax": 7,
      "musicIndexMax": 15
    }
  },
  "alignmentNotes": [
    "平台按固件实测锁定为 MT6768/Helio G85（2x A75 + 6x A55，Mali-G52 MC2），与 gold 的 MT6833 5G 平台完全不同：big 簇 part 用 0xd0a（A75）而非 0xd0b（A76），频点直接取固件 OPP 表 16 档，未沿用 gold 的 2.0/2.4GHz 频点。",
    "radioVariant=mtk（MTK 基带），但 cellular5G=false：MT6768 无 5G 基带，禁止照抄 gold 的 5G 能力。enableIr=false（固件 hasConsumerIr=false），enableNfc=true（固件 hasNfc=true 且有 libnfc-nci.conf/libnfc-sec-vendor.conf）。",
    "传感器严格按固件 sensorFeatures（accelerometer/compass/gyroscope/light/proximity/stepcounter/stepdetector）给 19 项：无气压计、无 NPU，故 sensors 与 thermal 均不出现 BAROMETER/NPU；TCL 固件没有品牌私有 sensor 命名空间，未填 xiaomi.sensor.* / oplus.sensor.* / vivo.sensor.* 等。",
    "input 节点全部取固件 measuredInputHints：键盘 mtk-kpd、耳机孔 mt6768-mt6359 Headset Jack、电源键 mtk-pmic-keys；指纹按 VINTF 的 vendor.goodix.hardware.biometrics.fingerprint.cap 给 uinput-goodix/goodix_fp；非小米系，未添加 swtp/SAR 节点。触控按 TCL 该价位常用 FocalTech fts_ts，touchKeys 为空（全面屏手势导航，无电容键）。",
    "EDID 按 TCL 自有面板厂 CSOT 供屏（PNP/面板 ID/2025 年第 18 周），面板为 60Hz IPS LCD，wideColorGamut=false，未虚报 120Hz 或广色域。GL 串按 MEOW 包装的 Mali r38p1 驱动构造，Vulkan 按 Mali-G52 的 1.1 能力（0x00401000），未照抄 gold 的 1.3/0x00403116。",
    "GNSS 用 MTK_MNLD_default,MNL_VER_default；固件 hasL5Config=true，故 dualFrequencyL5=true；yearOfHw 取该机 2023 年硬件基线。马达按该价位 ERM 转子给 capabilities=0 + CLICK/TICK/HEAVY_CLICK，未套用 gold 的 X 轴线性马达原语。Lights 只有 BACKLIGHT（无通知灯，固件未暴露 LED 节点）。",
    "音效描述符 100% 沿用固件实测 18 条（含 NXP 套件 + dtsaudio），未新增 Dolby/MiSound 等本机不存在的方案；Codec2 假登记统一 store=default1、rank=1，未占用 default。",
    "待真机复核项：ericanppro 的公开零售名（当前按 TCL 505 Pro 立项，以官方发布为准）、Vulkan deviceId/driverVersion 具体值（Mali r38p1 基线，装机后 vulkaninfo 收敛）、imeiTac 与 macOuiCandidates 的出厂段、以及触控 IC/马达类型（dumpsys input、/proc/bus/input/devices 复核）。",
    "固件实测的 maps 重定向（libGLES_mali→mesa、libdpframework→libEGL_mesa、audio.parameter_parser→cdh_audio_parameter_parser 等）已具备，vintfDrop 暂为空：本机 55 个 vendor HAL 占位与固件 VINTF 一致，无需额外裁剪。"
  ],
  "sources": [
    "目标机官方固件实测：build.prop / boot cmdline / DTB opp 表 / VINTF manifest / vendor/etc 配置（brand=tcl, device=ericanppro, buildId=15-AP3A.240905.015.A2-20250702）",
    "MediaTek Helio G85 (MT6768) 公开平台规格：2x Cortex-A75 + 6x Cortex-A55，Mali-G52 MC2，LPDDR4X，eMMC 5.1，Wi-Fi 5 / BT 5.0，4G LTE",
    "Android CDD / VINTF 兼容性要求：Vulkan 1.1 对应 0x00401000，Mali vendorId 0x13b5，Widevine L1 与 OEMCrypto API 版本对应关系",
    "TCL 机型 vendor/etc 约定：MTK MNLD GNSS 配置、MTK thermal zone 命名（mtktscpu/mtktsAP/mtktsgpu）、devfreq-13000000.mali 冷却设备"
  ]
}
```

**仍需真机复核的风险点**（不影响本次对齐落地，但建议装机后 5 分钟收敛）：零售商品名（工程代号 `ericanppro` 的公开命名）、Vulkan `deviceId/driverVersion` 精确值、`imeiTac`/`macOuiCandidates` 出厂段、触控 IC 与马达类型实测。
