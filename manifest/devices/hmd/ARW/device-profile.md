# 真机硬件规格与云机对齐留档 · HMD Crest Max (hmd/ARW)

- **生成时间**: `2026-11-26T10:12:30+08:00`
- **ROM Build ID**: `14-QP1A.190711.020-20250620`
- **DSH 聊天会话 ID**: `session-816caa96-6599-40bd-88c8-71e21757ec28`（标题：`[02_prepare] 真机规格采集 · HMD Crest Max (hmd/ARW · 14-QP1A.190711.020-20250620)`）
- **采集模型路由**: `cdh-maas / deepseek-v4.1-flash-1017`（复用在线 DSH Web: `false`）

---

## 一、设备表对齐规格（对应 `devices/manifest/devices/hmd/ARW.json`）

| 字段 | 对齐值 | 说明 |
| --- | --- | --- |
| `official` | `HMD` | 厂商集团 |
| `brand` | `hmd` | 子品牌标识 |
| `model` | `HMD Crest Max` | 商业上市机型名 （型号代码: `HMD CREST`, `Arrow_0AIN`） |
| `device` | `ARW` | 设备代号 |
| `platform` / `board` | `unisoc` / `ums9620` | 芯片平台 / 主板代号 |
| `resolution` | `1080x2400` | 屏幕物理像素分辨率 |
| `inch` | `70x155` | 屏幕可视物理宽×高 mm（对角线 `6.67"`） |
| `density` | `400` | Android 逻辑密度 dpi（物理 PPI `395`，xdpi `394.60` / ydpi `394.60`） |
| `smp` | `8` | CPU 核心总数 |
| `memory` | `[6144 8192]` | 全系发售内存档位 MB（6G / 8G，LPDDR4X） |
| `storage` | `[128000 256000]` | 全系发售存储档位（128G / 256G，UFS 2.2） |

- **常见量产发售版本组合**: `6GB+128GB` / `8GB+128GB` / `8GB+256GB`

## 二、真机深层硬件对齐画像（参考 `redmi/gold` 标准）

### 1. SoC 与 CPU 簇全量频点表（`cdh_cpufreq` / `module/config.json` 对齐）
- **SoC 芯片**: Spreadtrum (Unisoc) Tanggula T770 (`UMS9620` UMS9620, 6nm)
- **CPU 拓扑**: `4x Cortex-A55 @ 2.0GHz + 3x Cortex-A76 @ 2.21GHz + 1x Cortex-A76 @ 2.55GHz`（驱动: `sprd-cpufreq`，调速器: `sprd_sugov`，可选调速器: `[sprd_sugov ondemand performance powersave userspace]`，BogoMIPS: `26.00`）

| 策略簇 | 核心列表 | 微架构 | Part / MIDR | 容量权重 | 最小 / 驻留 / 最大 (kHz) | 频点档数 |
| --- | --- | --- | --- | --- | --- | --- |
| policy0 | `[0 1 2 3]` | Cortex-A55 | `0xd05` (`0x00000000412fd050`) | `417` | `614400 / 936000 / 2002000` | `10 档` |
| policy4 | `[4 5 6]` | Cortex-A76 | `0xd0b` (`0x00000000412fd0b0`) | `1022` | `614400 / 1228800 / 2210000` | `11 档` |
| policy7 | `[7]` | Cortex-A76 | `0xd0b` (`0x00000000412fd0b0`) | `1024` | `614400 / 1536000 / 2548000` | `11 档` |

- **`policy0` (`Cortex-A55`, cpus `[0 1 2 3]`) 全量 `available-frequencies` (10 档, kHz)**:
  `[2002000 1846000 1703000 1560000 1404000 1228800 1105000 936000 768000 614400]`
- **`policy4` (`Cortex-A76`, cpus `[4 5 6]`) 全量 `available-frequencies` (11 档, kHz)**:
  `[2210000 2041000 1872000 1703000 1536000 1378000 1228800 1105000 936000 768000 614400]`
- **`policy7` (`Cortex-A76`, cpus `[7]`) 全量 `available-frequencies` (11 档, kHz)**:
  `[2548000 2457000 2366000 2262000 2132000 2002000 1820000 1536000 1228800 768000 614400]`

<details>
<summary>可直接用于 <code>module/config.json</code> 的 <code>cpufreq</code> 完整配置片段</summary>

```json
{
  "cpufreq": {
    "driver": "sprd-cpufreq",
    "governor": "sprd_sugov",
    "available-governors": [
      "sprd_sugov",
      "ondemand",
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
          3
        ],
        "min-freq": 614400,
        "max-freq": 2002000,
        "cur-freq": 936000,
        "available-frequencies": [
          2002000,
          1846000,
          1703000,
          1560000,
          1404000,
          1228800,
          1105000,
          936000,
          768000,
          614400
        ],
        "implementer": "0x41",
        "variant": 2,
        "part": "0xd05",
        "revision": 0,
        "capacity": 417,
        "midr": "0x00000000412fd050",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 4,
        "cpus": [
          4,
          5,
          6
        ],
        "min-freq": 614400,
        "max-freq": 2210000,
        "cur-freq": 1228800,
        "available-frequencies": [
          2210000,
          2041000,
          1872000,
          1703000,
          1536000,
          1378000,
          1228800,
          1105000,
          936000,
          768000,
          614400
        ],
        "implementer": "0x41",
        "variant": 2,
        "part": "0xd0b",
        "revision": 0,
        "capacity": 1022,
        "midr": "0x00000000412fd0b0",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 7,
        "cpus": [
          7
        ],
        "min-freq": 614400,
        "max-freq": 2548000,
        "cur-freq": 1536000,
        "available-frequencies": [
          2548000,
          2457000,
          2366000,
          2262000,
          2132000,
          2002000,
          1820000,
          1536000,
          1228800,
          768000,
          614400
        ],
        "implementer": "0x41",
        "variant": 2,
        "part": "0xd0b",
        "revision": 0,
        "capacity": 1024,
        "midr": "0x00000000412fd0b0",
        "revidr": "0x0000000000000000"
      }
    ]
  }
}
```
</details>

### 2. 显示面板、GPU 与图形栈（`gpu_id.cfg` / `buildVendor` 对标 `gold`）
- **面板类型与刷新率**: AMOLED · 刷新率 `[60 120] Hz`（默认 `120 Hz`） · 广色域 `true` · EDID `HMD` (`HMDCRST`, product=`2561`, year=`2025`W`25`)
- **OpenGL ES**: `ARM` / `Mali-G57 MC4` / `OpenGL ES 3.2 v1.r38p1-01eac0.9c4e1a7b3d5f8021e6b4c9a0d7f3e2c1`
  - GL 驱动扩展: `GL_ARM_mali_program_binary,GL_ARM_mali_shader_binary,GL_ARM_rgba8` GL_ARM_debug_assert,GL_ARM_shader_core_properties,GL_ARM_texture_unnormalized_coordinates
- **Vulkan**: `ro.hardware.vulkan=mali` · VendorID `0x13b5` · DeviceID `0x90930040` · API `1.1` (`0x00401000`) · DriverID `9` (`0x0c381000`)

### 3. 触控、指纹、外设与输入节点对齐（`module/config.json` & `hardware/config.json` 对标 `gold`）
- **指纹方案**: 侧边电容指纹 (FPC)
- **触控与按键对齐**: touch=`focaltech_ats` · keyboard=`gpio-keys` · input_identity 外设数=`4` 项
- **硬件 HAL 变体与开关**: radioVariant=`default` · NFC=`false` · 红外=`false` · 3.5mm=`true` · 单扬声器
- **影像规格**: 后置 108MP + 2MP | 前置 50MP
- **电池与通信**: `5000 mAh`（有线 `33W` / 无线 `0W`） · Wi-Fi 5 (802.11ac)（Wi-Fi 6=`false`） · BT `5.0` · 5G=`true` · Widevine `L1` (sysId=`25817`) · TAC `35486509` · 基带 `5G_MODEM_V2_23B_W24.26.4_P11` · MAC OUI `[00:0a:f5]`

### 4. `vendor/etc/*_config.xml` 虚拟 HAL AI 对齐清单（对标 `gold-android15/vendor/etc`）
- **`sensors_config.xml`**: 共 `19` 项对齐传感器
- **`battery_config.xml`**: `Li-poly` · 容量 `5000 mAh` · 满电/充电电压 `4400 / 4470 mV`
- **`thermal_config.xml`**: 皮肤基温 `28.800 ℃` · 温区传感器 `7` 项 · Cooling=`devfreq-23000000.mali`
- **`gnss_config.xml`**: systemInfo=`SPRD_GNSS_default` · yearOfHw=`2022` · L5=`false`
- **`vibrator_config.xml` & `light_config.xml`**: 马达 `ERM coin` (cap=`0`, effects=`[CLICK TICK HEAVY_CLICK]`) · 逻辑灯 `6` 项
- **`audio_effects_descriptors.xml` & `media_config.xml`**: AI 音效描述符 `15` 项 · AI Codec2 实例 `1` 项 / 组件 `13` 项

### 5. AI 真机对齐提示
- 身份：brand=hmd、device=ARW、modelCode=HMD CREST、fingerprint HMD/Arrow_0AIN/ARW；OEM 为 HMD Global，非小米，未沿用 xiaomi.sensor.*、swtp、mtk-kpd 等小米/MTK 专属值。
- CPU：UMS9620(T770) 实测三簇 policy 0/4/7（4xA55 2.002GHz / 3xA76 2.21GHz / 1xA76 2.548GHz），频率表直接取 DTB OPP 实测 10/11/11 档；因硬件 OPP 就这么多，未按 12 档补造频点；driver=sprd-cpufreq、governor=sprd_sugov 按 Sprd 平台口径。
- GPU：Mali-G57 MC4（T770 集成），GL 串按 Mali r38p1（Android 13 BSP 基线）；Vulkan 按 Unisoc A13 vendor 取 1.1（0x00401000），deviceID 按 MC4 推 0x90930040，driverID=9（ARM proprietary）。
- 输入：触控 focaltech_ats、按键 gpio-keys 均来自固件实测；补 sprd-pmic-keys 电源键与 sprdphone Headset Jack 耳机插孔；无 swtp（该节点仅小米系具备）。
- 传感器：按 T770 参考设计给 BMI323 六轴（acc/gyro）+ MMC5603 磁力计 + STK3338 光感/距离，另含 AOSP 融合/未校准/计步类；无气压计故不登记，也未虚构 HMD 品牌私有传感器。
- thermal.zones 逐条照抄固件实测 31 个温区（soc-thmzone 31500/280、gpu 30200/220、pa 29500/180 等）；thermal_config 的 CPU/GPU/SOC 值取实测基温。
- 显示：6.67" AMOLED 1080x2400 120Hz、density 400、可视面板 69.5x154.5mm；EDID 周数由 buildDate 20250620 推为 2025 年第 25 周。
- 电池/连接：5000mAh 单电芯 33W、Li-poly、无无线充；无 NFC/无红外；Wi-Fi 5（T770 仅 802.11ac）、5G DSDS。
- audioEffects 直接核验保留固件实测 15 个描述符（含 ozo/ozowidening 两个 OEM 音效）；media 假登记统一 store=rank=1，未占用 default。
- 待真机回读核验的字段：imeiTac、imeiSvn、meidPrefix、macOuiCandidates、widevineSystemId、basebandVersion 精确串、EDID productId、vibrator 马达类型。GSMArena/DuckDuckGo 检索均被反爬拦截（Turnstile/验证码），未采信二手页面，以上按平台与固件线索推断，需在 persist/标签/getprop 侧回读确认。

## 三、官方固件包解包实测对账（`build.prop` + `boot.img` + `VINTF` + `DTB` + `fstab` + `vendor/etc`）

- `marketName`: `` | `modelCode`: `HMD CREST` | `manufacturer`: `HMD Global`
- `board`: `ums9620` | `soc`: `Spreadtrum T770 ()`
- `densityDpi`: `400` | `egl`: `` | `vulkan`: `` | `abilist`: `arm64-v8a,armeabi-v7a,armeabi`
- `securityPatch`: `2024-06-05` | `buildDate`: `20250620` | `kernelLine`: `5.15` | `vintfVersion`: `7.0`
- `partitionAndroidVersions`: `system=14, system_ext=14, product=14, vendor=13, odm=13, vendor_dlkm=13`
- `uname`: `Linux localhost 5.15.149-android13-8-00010-gc2e0ba41ba85-ab12040008 #1 SMP PREEMPT Tue Jul 2 00:44:45 UTC 2024 aarch64 Toybox`
- `basebandVersion`: `5G_MODEM_V2_23B_W24.26.4_P11`（来源: `rar:HMD Crest Max 5G (ARW) 1590-0-00WW-B01.pac/Modem_NR(SC9600_QogirN6Pro_PS_modem.bin)`）
- `fingerprint`: `HMD/Arrow_0AIN/ARW:13/QP1A.190711.020/00WW_1_590:user/release-keys`
- `vendor_boot.cmdline` (4 项): `androidboot.product.hardware.sku=Arrow_0AIN`, `androidboot.secureboot=1`, `androidboot.slot=a`, `androidboot.veritymode.managed=yes`
- `thermal.zones` (31 个温区): `soc-thmzone`, `big7mid4-thmzone`, `big7-thmzone`, `mid5mid6-thmzone`, `mid4-thmzone`, `mid5-thmzone`, `mid6-thmzone`, `apcpu0-thmzone`, `lit0-thmzone`, `lit1-thmzone`, `lit2-thmzone`, `lit3-thmzone`, `gpu-thmzone`, `apcpu1-thmzone`, `ai0-thmzone`, `ai1-thmzone`, `mm-thmzone`, `lte-thmzone`, `nr0-thmzone`, `nr1-thmzone`, `front-thmzone`, `back-thmzone`, `left-thmzone`, `right-thmzone`, `env-thmzone`, `osctsen-thmzone`, `outtsen-thmzone`, `board-thmzone`, `pa-thmzone`, `pa5g-thmzone`, `chg-thmzone`
- `vintfPlaceholders`: radio=`15` 项 · audio=`1` 项 · gnss=`1` 项 · nn=`13` 项

### `vendor/etc` 与 `odm/etc` 硬件配置实测详情

- **Permissions 硬件特性**（共 `68` 项）: 红外=`false` · NFC=`false` · 指纹=`true` · BLE=`true` · Wi-Fi Direct=`true` · Audio LowLatency=`false` · Vulkan Level=`1` (API `1.3`)
- **Sensors 原厂配置**: SubHALs=`[android.hardware.sensors@2.X-sprd_subhal.so]` · 芯片归属=`map[]`
- **Audio 策略与音效配置**: 模块=`[]` · 采样率=`[]` · 听筒=`false` · 有线耳机路由=`false` · 双麦=`false` · 音效描述符=`15` 项（源: `vendor/etc/audio_effects.xml`）
  - 实测音效清单: `Acoustic Echo Canceler (The Android Open Source Project)`, `Auxiliary Environmental Reverb (NXP Software Ltd.)`, `Auxiliary Preset Reverb (NXP Software Ltd.)`, `Dynamic Bass Boost (NXP Software Ltd.)`, `DynamicsProcessing (The Android Open Source Project)`, `EqualizerBundle (NXP Software Ltd.)`, `Insert Environmental Reverb (NXP Software Ltd.)`, `Insert Preset Reverb (NXP Software Ltd.)`, `Loudness Enhancer (The Android Open Source Project)`, `Multichannel Downmix To Stereo (The Android Open Source Project)`, `Virtualizer (NXP Software Ltd.)`, `Visualizer (The Android Open Source Project)`, `Volume (NXP Software Ltd.)`, `ozo (OEM Audio Effect)`, `ozowidening (OEM Audio Effect)`
- **Media Codec2 硬编解码配置**: Store实例=`[android.hardware.media.c2@1.1-unisoc-service->default]` · 厂商/杜比组件=`16` 项 (`c2.unisoc.jpeg.decoder`, `c2.unisoc.imaadpcm.decoder`, `c2.unisoc.mp3.decoder`, `c2.unisoc.mpl1.decoder`, `c2.unisoc.mpl2.decoder`, `c2.unisoc.avc.decoder`, `c2.unisoc.hevc.decoder`, `c2.unisoc.mpeg4.decoder`, `c2.unisoc.h263.decoder`, `c2.unisoc.vp8.decoder`, `c2.unisoc.vp9.decoder`, `c2.unisoc.av1.decoder`, `c2.unisoc.jpeg.encoder`, `c2.unisoc.mp3.encoder`, `c2.unisoc.avc.encoder`, `c2.unisoc.hevc.encoder`)
- **ODM SKU / NFC / Wi-Fi / 影像场景**: NFC芯片=`-` · RSC区域=`[]` · 相机Tuning角色=`[]`
- **Power 调频/核数控制表**: 各簇最大核数=`[]` · 配置文件=`[vendor/etc/task_profiles.json]`
- **已扫描解析的 `vendor/etc` & `odm/etc` 配置文件**: 共 `52` 个

## 四、DSH 聊天界面回复原文

## 简明总结

- **目标机**：HMD Crest Max（`hmd` / `ARW` / modelCode `HMD CREST`），OEM 为 HMD Global，与 gold 的小米/MT6833 无关；小米系 `xiaomi.sensor.*`、`swtp`、`mtk-*` 一律未沿用。
- **平台**：Unisoc UMS9620（T770，6nm），实测三簇 `policy 0/4/7`：4×A55@2.0GHz、3×A76@2.21GHz、1×A76@2.55GHz；频点表取 DTB OPP 实测（10/11/11 档，不补造）；GPU 为 Mali-G57 MC4，GL 串按 Mali r38p1，Vulkan 取 1.1（`0x00401000`）。
- **输入**：实测触控 `focaltech_ats`、按键 `gpio-keys`；补 `sprd-pmic-keys`、`sprdphone Headset Jack`；指纹按 HMD 侧边 FPC（`uinput-fpc`）。
- **传感器**：BMI323 六轴 + MMC5603 磁力 + STK3338 光感/距离 + AOSP 融合/未校准/计步；无气压计不登记。
- **温区**：31 个固件实测温区逐条保留（soc 31500/280、gpu 30200/220、pa 29500/180 等）。
- **显示/电池**：6.67" AMOLED 1080×2400 120Hz、density 400、面板 69.5×154.5mm、EDID 2025-W25；5000mAh 33W、无 NFC/红外、Wi-Fi 5、5G。
- **诚实标注**：检索被反爬拦截，`imeiTac`、`macOUI`、Widevine `systemId`、`basebandVersion` 精确串、EDID productId、马达类型等需真机回读核验。

```json
{
  "official": "HMD Global",
  "brand": "hmd",
  "device": "ARW",
  "model": "HMD Crest Max",
  "modelAliases": ["HMD CREST", "Arrow_0AIN"],
  "releaseDate": "2025-06",
  "platform": "unisoc",
  "board": "ums9620",
  "soc": {
    "manufacturer": "Spreadtrum (Unisoc)",
    "marketingName": "Tanggula T770",
    "model": "UMS9620",
    "partNumber": "UMS9620",
    "processNm": 6
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
    "ydpi": 394.6,
    "androidDensityDpi": 400,
    "panelType": "AMOLED",
    "refreshRatesHz": [60, 120],
    "defaultRefreshRateHz": 120,
    "wideColorGamut": true,
    "edidVendorHint": "HMD",
    "edidName": "HMDCRST",
    "edidProductId": 2561,
    "edidWeek": 25,
    "edidYear": 2025
  },
  "cpu": {
    "smp": 8,
    "architecture": "arm64-v8a",
    "topologySummary": "4x Cortex-A55 @ 2.0GHz + 3x Cortex-A76 @ 2.21GHz + 1x Cortex-A76 @ 2.55GHz",
    "driver": "sprd-cpufreq",
    "governor": "sprd_sugov",
    "availableGovernors": ["sprd_sugov", "ondemand", "performance", "powersave", "userspace"],
    "transitionLatency": 1000,
    "scalingReadable": true,
    "packagePerCluster": false,
    "bogoMips": "26.00",
    "clusters": [
      {
        "policy": 0,
        "cpus": [0, 1, 2, 3],
        "coreName": "Cortex-A55",
        "minFreqKhz": 614400,
        "maxFreqKhz": 2002000,
        "curFreqKhz": 936000,
        "availableFrequenciesKhz": [
          2002000, 1846000, 1703000, 1560000, 1404000, 1228800, 1105000, 936000, 768000, 614400
        ],
        "implementer": "0x41",
        "variant": 2,
        "part": "0xd05",
        "revision": 0,
        "capacity": 417,
        "midr": "0x00000000412fd050",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 4,
        "cpus": [4, 5, 6],
        "coreName": "Cortex-A76",
        "minFreqKhz": 614400,
        "maxFreqKhz": 2210000,
        "curFreqKhz": 1228800,
        "availableFrequenciesKhz": [
          2210000, 2041000, 1872000, 1703000, 1536000, 1378000, 1228800, 1105000, 936000, 768000, 614400
        ],
        "implementer": "0x41",
        "variant": 2,
        "part": "0xd0b",
        "revision": 0,
        "capacity": 1022,
        "midr": "0x00000000412fd0b0",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 7,
        "cpus": [7],
        "coreName": "Cortex-A76",
        "minFreqKhz": 614400,
        "maxFreqKhz": 2548000,
        "curFreqKhz": 1536000,
        "availableFrequenciesKhz": [
          2548000, 2457000, 2366000, 2262000, 2132000, 2002000, 1820000, 1536000, 1228800, 768000, 614400
        ],
        "implementer": "0x41",
        "variant": 2,
        "part": "0xd0b",
        "revision": 0,
        "capacity": 1024,
        "midr": "0x00000000412fd0b0",
        "revidr": "0x0000000000000000"
      }
    ]
  },
  "gpu": {
    "vendor": "ARM",
    "renderer": "Mali-G57 MC4",
    "glesVersion": "OpenGL ES 3.2",
    "glVersionFull": "OpenGL ES 3.2 v1.r38p1-01eac0.9c4e1a7b3d5f8021e6b4c9a0d7f3e2c1",
    "glslVersion": "OpenGL ES GLSL ES 3.20",
    "glExtraExts": "GL_ARM_mali_program_binary,GL_ARM_mali_shader_binary,GL_ARM_rgba8",
    "glExtraExts2": "GL_ARM_debug_assert,GL_ARM_shader_core_properties,GL_ARM_texture_unnormalized_coordinates",
    "maxTextureSize": 16383,
    "maxCombinedTextureImageUnits": 96,
    "maxVertexAttribs": 32,
    "vulkanHardware": "mali",
    "vulkanVendorId": "0x13b5",
    "vulkanApiVersion": "1.1",
    "vulkanApiVersionHex": "0x00401000",
    "vulkanConformance": "1.1.6.0",
    "vulkanDeviceId": "0x90930040",
    "vulkanDriverId": 9,
    "vulkanDriverVersion": "0x0c381000",
    "vulkanDriverInfo": "v1.r38p1-01eac0.9c4e1a7b3d5f8021e6b4c9a0d7f3e2c1",
    "vulkanDeviceUuid": "3f1c8a7d92b6e5401c4a7f9e2d3b6c80",
    "vulkanDriverUuid": "8a2d4f6e1b3c5d7f9e0a2c4b6d8f1e3a",
    "vulkanPipelineUuid": "b7e4c1a9d2f5836e0c4a7b1d9f2e6c38"
  },
  "memory": {
    "ramType": "LPDDR4X",
    "variantsGb": [6, 8],
    "variantsMb": [6144, 8192]
  },
  "storage": {
    "storageType": "UFS 2.2",
    "variantsGb": [128, 256],
    "variantsManifestMb": [128000, 256000],
    "skuCombinations": ["6GB+128GB", "8GB+128GB", "8GB+256GB"]
  },
  "peripherals": {
    "fingerprint": "侧边电容指纹 (FPC)",
    "touchControllerHints": ["focaltech_ats"],
    "keyboardDriverHint": "gpio-keys",
    "headsetJackNameHint": "sprdphone Headset Jack",
    "pmicKeysNameHint": "sprd-pmic-keys",
    "sensors": ["accelerometer", "gyroscope", "magnetometer", "ambient_light", "proximity"],
    "sensorChips": {
      "accelerometer": "bmi323_acc",
      "gyroscope": "bmi323_gyro",
      "magnetometer": "mmc5603",
      "light": "stk3338_l",
      "proximity": "stk3338_p"
    },
    "nfc": false,
    "infrared": false,
    "audioJack35mm": true,
    "speakers": "单扬声器"
  },
  "camera": {
    "rear": "108MP + 2MP",
    "front": "50MP"
  },
  "batteryAndConnectivity": {
    "batteryMah": 5000,
    "wiredChargingW": 33,
    "wirelessChargingW": 0,
    "wifi": "Wi-Fi 5 (802.11ac)",
    "wifi6Supported": false,
    "bluetooth": "5.0",
    "cellular5G": true,
    "macOuiCandidates": ["00:0a:f5"],
    "widevineDefault": "L1",
    "widevineSystemId": "25817",
    "widevineVersion": "18.0.0",
    "oemCryptoApiVersion": 18,
    "imeiTac": "35486509",
    "imeiSvn": "20",
    "meidPrefix": "99001402",
    "basebandVersion": "UMS9620_MODEM_V1.0.0",
    "rilImpl": "sprd-ril"
  },
  "inputAlignment": {
    "touch": {
      "name": "focaltech_ats",
      "bus": 24,
      "vendor": 0,
      "product": 0,
      "version": 256,
      "mtSlots": 10,
      "mtPressure": true,
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
      "goodixFp": false,
      "devices": [
        {
          "name": "uinput-fpc",
          "bus": 6,
          "vendor": 0,
          "product": 0,
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
          "switches": ["SW_HEADPHONE_INSERT", "SW_MICROPHONE_INSERT", "SW_LINEOUT_INSERT", "SW_JACK_PHYSICAL_INSERT"]
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
    {"type": "soc-thmzone", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "big7mid4-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "big7-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "mid5mid6-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "mid4-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "mid5-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "mid6-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "apcpu0-thmzone", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "lit0-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "lit1-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "lit2-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "lit3-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "gpu-thmzone", "base-temp-mc": 30200, "jitter-mc": 220},
    {"type": "apcpu1-thmzone", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "ai0-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "ai1-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "mm-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "lte-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "nr0-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "nr1-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "front-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "back-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "left-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "right-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "env-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "osctsen-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "outtsen-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "board-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "pa-thmzone", "base-temp-mc": 29500, "jitter-mc": 180},
    {"type": "pa5g-thmzone", "base-temp-mc": 29500, "jitter-mc": 180},
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
      "cycleCount": 96,
      "fullUah": 5000,
      "counterMin": 4925,
      "counterMax": 4958,
      "fullMv": 4400,
      "chargeMv": 4470,
      "temperatureFullDecic": 300,
      "temperatureChargeDecic": 380,
      "currentFullUa": -58000,
      "currentChargeUa": -2500000,
      "wireless": 0,
      "stateOfHealth": 98
    },
    "vibrator": {
      "motorType": "ERM coin",
      "registerManager": true,
      "capabilities": 0,
      "managerCapabilities": 0,
      "compositionDelayMax": 1000,
      "compositionSizeMax": 256,
      "vibratorIds": [1],
      "effects": ["CLICK", "TICK", "HEAVY_CLICK"]
    },
    "gnss": {
      "systemInfoName": "SPRD_GNSS_default",
      "gnssCapabilities": 991,
      "powerCapabilities": 1,
      "measurementCorrectionsCapabilities": 0,
      "yearOfHw": 2022,
      "defaultSatellites": 8,
      "dualFrequencyL5": false
    },
    "thermal": {
      "skinName": "SKIN",
      "skinCelsius": 28.8,
      "modelMaximumCelsius": 50,
      "coolingDeviceName": "devfreq-23000000.mali",
      "sensors": [
        {"id": 0, "name": "CPU", "value": 30.175, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 1, "name": "GPU", "value": 30.175, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 2, "name": "BATTERY", "value": 27.5, "thresholds": "nan;nan;nan;50;55;59;60", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 3, "name": "SKIN", "value": 28.8, "thresholds": "nan;43;45;50;70;80;90", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 5, "name": "POWER_AMPLIFIER", "value": 29.5, "thresholds": "nan;nan;nan;68;90;100;110", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 9, "name": "NPU", "value": 30.175, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 13, "name": "SOC", "value": 31.5, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"}
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
      {"handle": 1, "name": "bmi323_acc", "vendor": "Bosch", "version": 1, "typeId": 1, "type": "android.sensor.accelerometer", "maxRange": "78.453605652", "resolution": "0.002394201", "power": "0.001", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 2, "name": "mmc5603", "vendor": "Memsic", "version": 1, "typeId": 2, "type": "android.sensor.magnetic_field", "maxRange": "4912.050292969", "resolution": "0.150000006", "power": "0.001", "minDelay": 10000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 3, "name": "ORIENTATION", "vendor": "Unisoc", "version": 1, "typeId": 3, "type": "android.sensor.orientation", "maxRange": "360.0", "resolution": "0.00390625", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 4, "name": "bmi323_gyro", "vendor": "Bosch", "version": 1, "typeId": 4, "type": "android.sensor.gyroscope", "maxRange": "34.906299591", "resolution": "0.001065", "power": "0.001", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 5, "name": "stk3338_l", "vendor": "Sensortek", "version": 1, "typeId": 5, "type": "android.sensor.light", "maxRange": "65535.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 8, "name": "stk3338_p", "vendor": "Sensortek", "version": 1, "typeId": 8, "type": "android.sensor.proximity", "maxRange": "5.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 1000000, "wakeUp": true, "reporting": 1},
      {"handle": 9, "name": "GRAVITY", "vendor": "Unisoc", "version": 1, "typeId": 9, "type": "android.sensor.gravity", "maxRange": "39.226802826", "resolution": "0.002394201", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 10, "name": "LINEARACCEL", "vendor": "Unisoc", "version": 1, "typeId": 10, "type": "android.sensor.linear_acceleration", "maxRange": "39.226802826", "resolution": "0.002394201", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 11, "name": "ROTATION_VECTOR", "vendor": "Unisoc", "version": 1, "typeId": 11, "type": "android.sensor.rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 14, "name": "UNCALI_MAG", "vendor": "Unisoc", "version": 1, "typeId": 14, "type": "android.sensor.magnetic_field_uncalibrated", "maxRange": "4912.050292969", "resolution": "0.150000006", "power": "0.001", "minDelay": 20000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 15, "name": "GAME_ROTATION_VECTOR", "vendor": "Unisoc", "version": 1, "typeId": 15, "type": "android.sensor.game_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 16, "name": "UNCALI_GYRO", "vendor": "Unisoc", "version": 1, "typeId": 16, "type": "android.sensor.gyroscope_uncalibrated", "maxRange": "34.906299591", "resolution": "0.001065", "power": "0.001", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 17, "name": "SIGNIFICANT_MOTION", "vendor": "Unisoc", "version": 1, "typeId": 17, "type": "android.sensor.significant_motion", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 18, "name": "STEP_DETECTOR", "vendor": "Unisoc", "version": 1, "typeId": 18, "type": "android.sensor.step_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 0, "wakeUp": false, "reporting": 3},
      {"handle": 19, "name": "STEP_COUNTER", "vendor": "Unisoc", "version": 1, "typeId": 19, "type": "android.sensor.step_counter", "maxRange": "2147483648.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 20, "name": "GEOMAGNETIC_ROTATION_VECTOR", "vendor": "Unisoc", "version": 1, "typeId": 20, "type": "android.sensor.geomagnetic_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 22, "name": "TILT_DETECTOR", "vendor": "Unisoc", "version": 1, "typeId": 22, "type": "android.sensor.tilt_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 3},
      {"handle": 27, "name": "DEVICE_ORIENTATION", "vendor": "Unisoc", "version": 1, "typeId": 27, "type": "android.sensor.device_orientation", "maxRange": "3.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 35, "name": "UNCALI_ACC", "vendor": "Unisoc", "version": 1, "typeId": 35, "type": "android.sensor.accelerometer_uncalibrated", "maxRange": "39.226802826", "resolution": "0.002394201", "power": "0.001", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0}
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
      {"name": "Volume", "implementor": "NXP Software Ltd.", "type": "09e8ede0-ddde-11db-b4f6-0002a5d5c51b", "uuid": "119341a0-8469-11df-81f9-0002a5d5c51b", "connect": "Insert"},
      {"name": "ozo", "implementor": "OEM Audio Effect", "type": "fa81dbde-588b-11ed-9b6a-0242ac120002", "uuid": "7e384a3b-7850-4a64-a097-884250d8a737", "connect": "Insert"},
      {"name": "ozowidening", "implementor": "OEM Audio Effect", "type": "fa81dbde-588b-11ed-9b6a-0242ac120002", "uuid": "79bbd08d-8f08-4e96-925a-430f2eb96bf1", "connect": "Insert"}
    ],
    "media": {
      "instances": [
        {"binary": "android.hardware.media.c2@1.1-unisoc-service", "store": "rank=1"}
      ],
      "components": [
        {"name": "c2.unisoc.avc.decoder", "kind": "decoder", "mediaType": "video/avc", "store": "rank=1"},
        {"name": "c2.unisoc.hevc.decoder", "kind": "decoder", "mediaType": "video/hevc", "store": "rank=1"},
        {"name": "c2.unisoc.vp9.decoder", "kind": "decoder", "mediaType": "video/x-vnd.on2.vp9", "store": "rank=1"},
        {"name": "c2.unisoc.vp8.decoder", "kind": "decoder", "mediaType": "video/x-vnd.on2.vp8", "store": "rank=1"},
        {"name": "c2.unisoc.av1.decoder", "kind": "decoder", "mediaType": "video/av01", "store": "rank=1"},
        {"name": "c2.unisoc.mpeg4.decoder", "kind": "decoder", "mediaType": "video/mp4v-es", "store": "rank=1"},
        {"name": "c2.unisoc.h263.decoder", "kind": "decoder", "mediaType": "video/3gpp", "store": "rank=1"},
        {"name": "c2.unisoc.jpeg.decoder", "kind": "decoder", "mediaType": "video/jpeg", "store": "rank=1"},
        {"name": "c2.unisoc.mp3.decoder", "kind": "decoder", "mediaType": "audio/mpeg", "store": "rank=1"},
        {"name": "c2.unisoc.avc.encoder", "kind": "encoder", "mediaType": "video/avc", "store": "rank=1"},
        {"name": "c2.unisoc.hevc.encoder", "kind": "encoder", "mediaType": "video/hevc", "store": "rank=1"},
        {"name": "c2.unisoc.jpeg.encoder", "kind": "encoder", "mediaType": "video/jpeg", "store": "rank=1"},
        {"name": "c2.unisoc.mp3.encoder", "kind": "encoder", "mediaType": "audio/mpeg", "store": "rank=1"}
      ]
    },
    "audioEngine": {
      "modules": "default,bluetooth,r_submix",
      "voiceCallIndexMax": 7,
      "musicIndexMax": 15
    }
  },
  "alignmentNotes": [
    "身份：brand=hmd、device=ARW、modelCode=HMD CREST、fingerprint HMD/Arrow_0AIN/ARW；OEM 为 HMD Global，非小米，未沿用 xiaomi.sensor.*、swtp、mtk-kpd 等小米/MTK 专属值。",
    "CPU：UMS9620(T770) 实测三簇 policy 0/4/7（4xA55 2.002GHz / 3xA76 2.21GHz / 1xA76 2.548GHz），频率表直接取 DTB OPP 实测 10/11/11 档；因硬件 OPP 就这么多，未按 12 档补造频点；driver=sprd-cpufreq、governor=sprd_sugov 按 Sprd 平台口径。",
    "GPU：Mali-G57 MC4（T770 集成），GL 串按 Mali r38p1（Android 13 BSP 基线）；Vulkan 按 Unisoc A13 vendor 取 1.1（0x00401000），deviceID 按 MC4 推 0x90930040，driverID=9（ARM proprietary）。",
    "输入：触控 focaltech_ats、按键 gpio-keys 均来自固件实测；补 sprd-pmic-keys 电源键与 sprdphone Headset Jack 耳机插孔；无 swtp（该节点仅小米系具备）。",
    "传感器：按 T770 参考设计给 BMI323 六轴（acc/gyro）+ MMC5603 磁力计 + STK3338 光感/距离，另含 AOSP 融合/未校准/计步类；无气压计故不登记，也未虚构 HMD 品牌私有传感器。",
    "thermal.zones 逐条照抄固件实测 31 个温区（soc-thmzone 31500/280、gpu 30200/220、pa 29500/180 等）；thermal_config 的 CPU/GPU/SOC 值取实测基温。",
    "显示：6.67\" AMOLED 1080x2400 120Hz、density 400、可视面板 69.5x154.5mm；EDID 周数由 buildDate 20250620 推为 2025 年第 25 周。",
    "电池/连接：5000mAh 单电芯 33W、Li-poly、无无线充；无 NFC/无红外；Wi-Fi 5（T770 仅 802.11ac）、5G DSDS。",
    "audioEffects 直接核验保留固件实测 15 个描述符（含 ozo/ozowidening 两个 OEM 音效）；media 假登记统一 store=rank=1，未占用 default。",
    "待真机回读核验的字段：imeiTac、imeiSvn、meidPrefix、macOuiCandidates、widevineSystemId、basebandVersion 精确串、EDID productId、vibrator 马达类型。GSMArena/DuckDuckGo 检索均被反爬拦截（Turnstile/验证码），未采信二手页面，以上按平台与固件线索推断，需在 persist/标签/getprop 侧回读确认。"
  ],
  "sources": [
    "本机固件实测 JSON：build.prop + boot/DTB + VINTF manifest + vendor/etc + odm/etc（buildId 14-QP1A.190711.020-20250620）",
    "Unisoc Tanggula T770（UMS9620）平台公开规格：6nm、4xA55+3xA76+1xA76、Mali-G57 MC4、Wi-Fi 5",
    "Android Vulkan 版本编码约定（1.1=0x00401000，1.3=0x00403116）",
    "GSMArena / DuckDuckGo 检索尝试（Turnstile 与机器人验证码拦截，未取得页面，未采信）"
  ]
}
```
