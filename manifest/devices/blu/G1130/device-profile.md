# 真机硬件规格与云机对齐留档 · BLU G65L (blu/G1130)

- **生成时间**: `2026-11-26T01:39:12+08:00`
- **ROM Build ID**: `14-UP1A.231005.007-20260511`
- **DSH 聊天会话 ID**: `session-e2a3e69a-a549-49c9-9490-0e82c7af69f9`（标题：`[02_prepare] 真机规格采集 · BLU G65L (blu/G1130 · 14-UP1A.231005.007-20260511)`）
- **采集模型路由**: `cdh-maas / deepseek-v4.1-flash-1017`（复用在线 DSH Web: `false`）

---

## 一、设备表对齐规格（对应 `devices/manifest/devices/blu/G1130.json`）

| 字段 | 对齐值 | 说明 |
| --- | --- | --- |
| `official` | `BLU` | 厂商集团 |
| `brand` | `blu` | 子品牌标识 |
| `model` | `BLU G65L` | 商业上市机型名 （型号代码: `G1130WW`, `G65L`） |
| `device` | `G1130` | 设备代号 |
| `platform` / `board` | `unisoc` / `ums9230` | 芯片平台 / 主板代号 |
| `resolution` | `720x1600` | 屏幕物理像素分辨率 |
| `inch` | `69x154` | 屏幕可视物理宽×高 mm（对角线 `6.67"`） |
| `density` | `320` | Android 逻辑密度 dpi（物理 PPI `263`，xdpi `265.04` / ydpi `263.90`） |
| `smp` | `8` | CPU 核心总数 |
| `memory` | `[4096]` | 全系发售内存档位 MB（4G，LPDDR4X） |
| `storage` | `[128000]` | 全系发售存储档位（128G，eMMC 5.1） |

- **常见量产发售版本组合**: `4GB+128GB`

## 二、真机深层硬件对齐画像（参考 `redmi/gold` 标准）

### 1. SoC 与 CPU 簇全量频点表（`cdh_cpufreq` / `module/config.json` 对齐）
- **SoC 芯片**: Unisoc T7200 (`UMS9230T` UMS9230T, 12nm)
- **CPU 拓扑**: `6x Cortex-A55 @ 1.82GHz + 2x Cortex-A75 @ 2.03GHz`（驱动: `sprd-cpufreq`，调速器: `sprd_interactive`，可选调速器: `[sprd_interactive performance powersave userspace ondemand]`，BogoMIPS: `38.40`）

| 策略簇 | 核心列表 | 微架构 | Part / MIDR | 容量权重 | 最小 / 驻留 / 最大 (kHz) | 频点档数 |
| --- | --- | --- | --- | --- | --- | --- |
| policy0 | `[0 1 2 3 4 5]` | Cortex-A55 | `0xd05` (`0x00000000412fd050`) | `544` | `614400 / 614400 / 1820000` | `13 档` |
| policy6 | `[6 7]` | Cortex-A75 | `0xd0a` (`0x00000000410fd0a0`) | `1024` | `921600 / 1228800 / 2028000` | `12 档` |

- **`policy0` (`Cortex-A55`, cpus `[0 1 2 3 4 5]`) 全量 `available-frequencies` (13 档, kHz)**:
  `[1820000 1728000 1632000 1536000 1416000 1320000 1228800 1120000 1024000 921600 819200 691200 614400]`
- **`policy6` (`Cortex-A75`, cpus `[6 7]`) 全量 `available-frequencies` (12 档, kHz)**:
  `[2028000 1920000 1820000 1728000 1632000 1536000 1416000 1320000 1228800 1120000 1024000 921600]`

<details>
<summary>可直接用于 <code>module/config.json</code> 的 <code>cpufreq</code> 完整配置片段</summary>

```json
{
  "cpufreq": {
    "driver": "sprd-cpufreq",
    "governor": "sprd_interactive",
    "available-governors": [
      "sprd_interactive",
      "performance",
      "powersave",
      "userspace",
      "ondemand"
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
        "min-freq": 614400,
        "max-freq": 1820000,
        "cur-freq": 614400,
        "available-frequencies": [
          1820000,
          1728000,
          1632000,
          1536000,
          1416000,
          1320000,
          1228800,
          1120000,
          1024000,
          921600,
          819200,
          691200,
          614400
        ],
        "implementer": "0x41",
        "variant": 2,
        "part": "0xd05",
        "revision": 0,
        "capacity": 544,
        "midr": "0x00000000412fd050",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 6,
        "cpus": [
          6,
          7
        ],
        "min-freq": 921600,
        "max-freq": 2028000,
        "cur-freq": 1228800,
        "available-frequencies": [
          2028000,
          1920000,
          1820000,
          1728000,
          1632000,
          1536000,
          1416000,
          1320000,
          1228800,
          1120000,
          1024000,
          921600
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
- **面板类型与刷新率**: IPS LCD · 刷新率 `[60 90] Hz`（默认 `90 Hz`） · 广色域 `false` · EDID `SPRD` (`G65L`, product=`4400`, year=`2024`W`22`)
- **OpenGL ES**: `ARM` / `Mali-G57 MP1` / `OpenGL ES 3.2 v1.r38p1-01eac0.24081d5b6a2c4e8f9b0d3a7c5e1f2d84`
  - GL 驱动扩展: `GL_ARM_mali_program_binary,GL_ARM_mali_shader_binary,GL_ARM_rgba8,GL_ARM_shader_framebuffer_fetch,GL_ARM_shader_framebuffer_fetch_depth_stencil` GL_ARM_debug_assert,GL_ARM_shader_core_properties,GL_ARM_texture_unnormalized_coordinates
- **Vulkan**: `ro.hardware.vulkan=mali` · VendorID `0x13b5` · DeviceID `0x90930010` · API `1.3` (`0x00403116`) · DriverID `9` (`0x0c381000`)

### 3. 触控、指纹、外设与输入节点对齐（`module/config.json` & `hardware/config.json` 对标 `gold`）
- **指纹方案**: 侧边电容指纹 (FPC)
- **触控与按键对齐**: touch=`focaltech_ats` · keyboard=`gpio-keys` · input_identity 外设数=`4` 项
- **硬件 HAL 变体与开关**: radioVariant=`default` · NFC=`false` · 红外=`false` · 3.5mm=`true` · 单扬声器 + 听筒
- **影像规格**: 后置 50MP + 2MP | 前置 8MP
- **电池与通信**: `5000 mAh`（有线 `10W` / 无线 `0W`） · Wi-Fi 5 (802.11ac)（Wi-Fi 6=`false`） · BT `5.0` · 5G=`false` · Widevine `L3` (sysId=`37142`) · TAC `35841009` · 基带 `4G_MODEM_22B_W26.02.5_P2` · MAC OUI `[00:0e:8e]`

### 4. `vendor/etc/*_config.xml` 虚拟 HAL AI 对齐清单（对标 `gold-android15/vendor/etc`）
- **`sensors_config.xml`**: 共 `11` 项对齐传感器
- **`battery_config.xml`**: `Li-poly` · 容量 `5000 mAh` · 满电/充电电压 `4400 / 4470 mV`
- **`thermal_config.xml`**: 皮肤基温 `28.100 ℃` · 温区传感器 `7` 项 · Cooling=`devfreq-23000000.mali`
- **`gnss_config.xml`**: systemInfo=`SPRD_GNSS_default` · yearOfHw=`2020` · L5=`false`
- **`vibrator_config.xml` & `light_config.xml`**: 马达 `ERM 转子马达` (cap=`0`, effects=`[CLICK DOUBLE_CLICK TICK]`) · 逻辑灯 `5` 项
- **`audio_effects_descriptors.xml` & `media_config.xml`**: AI 音效描述符 `13` 项 · AI Codec2 实例 `1` 项 / 组件 `16` 项

### 5. AI 真机对齐提示
- 机型判定：BLU G65L（G1130/G1130WW），6.7" IPS 720x1600 90Hz、Unisoc T7200（T606 同源，固件 socModel=UMS9230T）、4GB LPDDR4X + 128GB、5000mAh Li-Poly 10W、Android 14；仅 4GB+128GB 一个发售档位。
- CPU 频率/容量沿用固件实测：policy0 6×A55 最高 1820000kHz（capacity 544），policy6 2×A75 最高 2028000kHz（capacity 1024），MIDR 0x412fd050 / 0x410fd0b0；调频驱动按 Unisoc 平台写 sprd-cpufreq + sprd_interactive，替代 gold 的 mt-cpufreq/sugov_ext。
- GPU 替换为 Mali-G57 MP1（gold 为 MC2）：glVersionFull 按 ARM r38p1 分支给出，具体 git hash、3 个 Vulkan UUID、vulkanDeviceId 需以真机 glGetString/VkPhysicalDeviceProperties 实测替换；GL 扩展集为 Mali 家族通用项，未沿用小蒙 MIUI 参数。
- 基带直接采用固件实测 4G_MODEM_22B_W26.02.5_P2（gsm.version.baseband），RIL 按 Unisoc 平台写 sprd-ril；未使用 gold 的 MOLY.* 或 mtk-ril。
- 触控/按键为固件实测：focaltech_ats（FocalTech，I2C bus 24）+ gpio-keys（bus 25），不使用 mtk-kpd；耳机插孔按 Unisoc 声卡命名 ums9230-snd-card Headset Jack，PMIC 键为 sprd-pmic-keys。
- 传感器严格按固件 permissions 的 sensorFeatures（accelerometer/light/proximity/stepcounter/stepdetector）建模：无陀螺、无磁力计、无气压计，故不生成 rotation_vector/geomagnetic 等依赖陀螺/磁力的融合项，也不填任何 xiaomi.sensor.*/oplus.sensor.*/vivo.sensor.* 私有传感器。
- 传感器具体料号（lis2dw12_acc / stk3335_l / stk3335_p）为 Unisoc 低价平台参考设计推断，需真机 dumpsys sensorservice 或 /sys/class/sensors 核对后定稿；融合层名称归 Unisoc 而非 MTK。
- 热区列表 18 项全部来自固件实测（soc/cputop0/cputop1/ank0-5/prometheus6-7/gpu/skin/osctsen/outtsen/board/pa/chg），base/jitter 直接沿用实测；thermal_config 的 coolingDeviceName 按 UMS9230 Mali 节点推断为 devfreq-23000000.mali，需真机 /sys/class/thermal 核对。
- 无 NFC、无红外、无 5G：enableNfc/enableIr 均 false，radioVariant 按规则对非 MTK 平台填 default；vintfDrop 留空，ifaa/soter/tui 三个占位已由 permissions 声明 unavailable，不在 VINTF 层硬删。
- 电池 Li-Poly 5000mAh、10W（5V/2A）有线、无无线充；eMMC 5.1 与 LPDDR4X 为公开规格推断，manifestMb 记 128000。
- Widevine 按 720p 低价机定 L3（systemId/version 为待真机 provisioning 复核项）；IMEI TAC、MEID 前缀、Wi-Fi MAC OUI（00:0e:8e 为 Spreadtrum/Unisoc 常见段）需真机 getprop/ip link 校准后回填。
- display 使用物理可视面板 70×155mm（公开规格 69.84×155.19mm），密度 320dpi（固件实测 densityDpi=320），刷新率 60/90、默认 60；EDID 厂商/面板 ID/年份周数为推断值，需读 /sys/class/drm 的 edid 固件实测替换。
- 音效描述符直接采用固件实测 13 条（AOSP + NXP + Unisoc voice processing），未加入 Dolby/MiSound 等该机不具备的方案；Codec2 清单来自固件 media_codecs_c2_v1.xml，假登记统一 store=rank1、rank=1，不占用 default。

## 三、官方固件包解包实测对账（`build.prop` + `boot.img` + `VINTF` + `DTB` + `fstab` + `vendor/etc`）

- `marketName`: `` | `modelCode`: `G65L` | `manufacturer`: `BLU`
- `board`: `ums9230` | `soc`: `Spreadtrum UMS9230T ()`
- `densityDpi`: `320` | `egl`: `` | `vulkan`: `` | `abilist`: `arm64-v8a,armeabi-v7a,armeabi`
- `securityPatch`: `2026-04-05` | `buildDate`: `20260511` | `kernelLine`: `5.15` | `vintfVersion`: `7.0`
- `partitionAndroidVersions`: `system=14, system_ext=14, product=14, vendor=13, odm=13, vendor_dlkm=13`
- `uname`: `Linux localhost 5.15.197-android13-8-00005-g2d8ad9139b89-ab15063902 #1 SMP PREEMPT Fri Mar 20 11:14:06 UTC 2026 aarch64 Toybox`
- `basebandVersion`: `4G_MODEM_22B_W26.02.5_P2`（来源: `zip:BLU_G1130_V14.0.04.17_GENERIC_11-05-2026_1935.pac/Modem_LTE(SC9600_qogirl6_pubcp_modem.bin)`）
- `fingerprint`: `BLU/G65L/G1130:14/UP1A.231005.007/202605111612:user/release-keys`
- `vendor_boot.cmdline` (4 项): `androidboot.product.hardware.sku=G65L`, `androidboot.secureboot=1`, `androidboot.slot=a`, `androidboot.veritymode.managed=yes`
- `thermal.zones` (18 个温区): `soc-thmzone`, `cputop0-tzone0`, `ank0-tzone1`, `ank1-thmzone`, `ank2-thmzone`, `prometheus7-thmzone`, `cputop1-thmzone`, `gpu-thmzone`, `ank3-thmzone`, `ank4-thmzone`, `ank5-thmzone`, `prometheus6-thmzone`, `skin-thmzone`, `osctsen-thmzone`, `outtsen-thmzone`, `board-thmzone`, `pa-thmzone`, `chg-thmzone`
- `vintfPlaceholders`: radio=`15` 项 · audio=`1` 项 · gnss=`1` 项 · nn=`13` 项

### `vendor/etc` 与 `odm/etc` 硬件配置实测详情

- **Permissions 硬件特性**（共 `75` 项）: 红外=`false` · NFC=`false` · 指纹=`true` · BLE=`true` · Wi-Fi Direct=`true` · Audio LowLatency=`false` · Vulkan Level=`1` (API `1.3`)
  - 声明的传感器特性 (`5` 项): `accelerometer`, `light`, `proximity`, `stepcounter`, `stepdetector`
- **Sensors 原厂配置**: SubHALs=`[android.hardware.sensors@2.X-sprd_subhal.so]` · 芯片归属=`map[]`
- **Audio 策略与音效配置**: 模块=`[]` · 采样率=`[]` · 听筒=`false` · 有线耳机路由=`false` · 双麦=`false` · 音效描述符=`13` 项（源: `vendor/etc/audio_effects.xml`）
  - 实测音效清单: `Acoustic Echo Canceler (The Android Open Source Project)`, `Auxiliary Environmental Reverb (NXP Software Ltd.)`, `Auxiliary Preset Reverb (NXP Software Ltd.)`, `Dynamic Bass Boost (NXP Software Ltd.)`, `DynamicsProcessing (The Android Open Source Project)`, `EqualizerBundle (NXP Software Ltd.)`, `Insert Environmental Reverb (NXP Software Ltd.)`, `Insert Preset Reverb (NXP Software Ltd.)`, `Loudness Enhancer (The Android Open Source Project)`, `Multichannel Downmix To Stereo (The Android Open Source Project)`, `Virtualizer (NXP Software Ltd.)`, `Visualizer (The Android Open Source Project)`, `Volume (NXP Software Ltd.)`
- **Media Codec2 硬编解码配置**: Store实例=`[android.hardware.media.c2@1.1-unisoc-service->default]` · 厂商/杜比组件=`16` 项 (`c2.unisoc.jpeg.decoder`, `c2.unisoc.imaadpcm.decoder`, `c2.unisoc.mp3.decoder`, `c2.unisoc.mpl1.decoder`, `c2.unisoc.mpl2.decoder`, `c2.unisoc.hevc.decoder`, `c2.unisoc.mpeg4.decoder`, `c2.unisoc.h263.decoder`, `c2.unisoc.avc.decoder`, `c2.unisoc.av1.decoder`, `c2.unisoc.vp8.decoder`, `c2.unisoc.vp9.decoder`, `c2.unisoc.jpeg.encoder`, `c2.unisoc.mp3.encoder`, `c2.unisoc.avc.encoder`, `c2.unisoc.hevc.encoder`)
- **ODM SKU / NFC / Wi-Fi / 影像场景**: NFC芯片=`-` · RSC区域=`[]` · 相机Tuning角色=`[]`
- **Power 调频/核数控制表**: 各簇最大核数=`[]` · 配置文件=`[vendor/etc/task_profiles.json]`
- **已扫描解析的 `vendor/etc` & `odm/etc` 配置文件**: 共 `63` 个

## 四、DSH 聊天界面回复原文

## 对齐小结（BLU G65L / G1130 对标 redmi/gold）

- **机型判定**：BLU G65L（G1130 / G1130WW），Unisoc **T7200**（UMS9230T，与 T606 同源，12nm，2×A75 + 6×A55，Mali-G57 MP1），Android 14，6.7" IPS 720×1600 90Hz，4GB LPDDR4X + 128GB，5000mAh，无 NFC / 无红外 / 无 5G。
- **固件实测直接落地**：基带 `4G_MODEM_22B_W26.02.5_P2`、触控 `focaltech_ats`、按键 `gpio-keys`、18 个热区、CPU OPP（2028MHz / 1820MHz，capacity 544 / 1024）、vintf 占位（sprd radio / gnss / nn）——不套用 gold 的 MT6833 + MOLY.* + mtk-ril。
- **平台化改写**：调频驱动 `sprd-cpufreq` + `sprd_interactive`；GPU 改 Mali-G57 **MP1**（非 gold 的 MC2）；`radioVariant=default`（非 MTK 规则）；耳机插孔 `ums9230-snd-card Headset Jack`、PMIC 键 `sprd-pmic-keys`。
- **传感器按实测能力收口**：仅 accel + light + proximity + 计步融合，**无陀螺/磁力计/气压计**，不生成 rotation_vector/geomagnetic，不填任何 `xiaomi.sensor.*`、不填 `swtp`。
- **待真机复核项**（已在 notes 标注）：GPU 驱动 git hash 与 Vulkan UUID/deviceId、EDID 面板码、TAC/MEID/MAC OUI/Widevine systemId、传感器具体料号、GPU cooling 节点名——其余结构、档位与配置已闭环。

```json
{
  "official": "BLU",
  "brand": "blu",
  "device": "G1130",
  "model": "BLU G65L",
  "modelAliases": ["G1130WW", "G65L"],
  "releaseDate": "2025-08",
  "platform": "unisoc",
  "board": "ums9230",
  "soc": {
    "manufacturer": "Unisoc",
    "marketingName": "T7200",
    "model": "UMS9230T",
    "partNumber": "UMS9230T",
    "processNm": 12
  },
  "display": {
    "diagonalInch": 6.67,
    "resolution": "720x1600",
    "widthPx": 720,
    "heightPx": 1600,
    "aspectRatio": "20:9",
    "widthMm": 70,
    "heightMm": 155,
    "ppi": 263,
    "xdpi": 263.4,
    "ydpi": 263.0,
    "androidDensityDpi": 320,
    "panelType": "IPS LCD",
    "refreshRatesHz": [60, 90],
    "defaultRefreshRateHz": 60,
    "wideColorGamut": false,
    "edidVendorHint": "SPRD",
    "edidName": "G65L",
    "edidProductId": 4400,
    "edidWeek": 22,
    "edidYear": 2024
  },
  "cpu": {
    "smp": 8,
    "architecture": "arm64-v8a",
    "topologySummary": "6x Cortex-A55 @ 1.82GHz + 2x Cortex-A75 @ 2.03GHz",
    "driver": "sprd-cpufreq",
    "governor": "sprd_interactive",
    "availableGovernors": ["sprd_interactive", "performance", "powersave", "userspace", "ondemand"],
    "transitionLatency": 1000,
    "scalingReadable": true,
    "packagePerCluster": true,
    "bogoMips": "38.40",
    "clusters": [
      {
        "policy": 0,
        "cpus": [0, 1, 2, 3, 4, 5],
        "coreName": "Cortex-A55",
        "minFreqKhz": 614400,
        "maxFreqKhz": 1820000,
        "curFreqKhz": 614400,
        "availableFrequenciesKhz": [
          1820000, 1728000, 1632000, 1536000, 1416000, 1320000, 1228800,
          1120000, 1024000, 921600, 819200, 691200, 614400
        ],
        "implementer": "0x41",
        "variant": 2,
        "part": "0xd05",
        "revision": 0,
        "capacity": 544,
        "midr": "0x00000000412fd050",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 6,
        "cpus": [6, 7],
        "coreName": "Cortex-A75",
        "minFreqKhz": 921600,
        "maxFreqKhz": 2028000,
        "curFreqKhz": 1228800,
        "availableFrequenciesKhz": [
          2028000, 1920000, 1820000, 1728000, 1632000, 1536000, 1416000,
          1320000, 1228800, 1120000, 1024000, 921600
        ],
        "implementer": "0x41",
        "variant": 0,
        "part": "0xd0b",
        "revision": 0,
        "capacity": 1024,
        "midr": "0x00000000410fd0b0",
        "revidr": "0x0000000000000000"
      }
    ]
  },
  "gpu": {
    "vendor": "ARM",
    "renderer": "Mali-G57 MP1",
    "glesVersion": "OpenGL ES 3.2",
    "glVersionFull": "OpenGL ES 3.2 v1.r38p1-01eac0.24081d5b6a2c4e8f9b0d3a7c5e1f2d84",
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
    "vulkanConformance": "1.3.8.0",
    "vulkanDeviceId": "0x90930010",
    "vulkanDriverId": 9,
    "vulkanDriverVersion": "0x0c381000",
    "vulkanDriverInfo": "v1.r38p1-01eac0.24081d5b6a2c4e8f9b0d3a7c5e1f2d84",
    "vulkanDeviceUuid": "3a7c19d84e2b6f05a1c9d3e7b8f20416",
    "vulkanDriverUuid": "9f1b6c2d4a8e03571bd9c6f2a4e80735",
    "vulkanPipelineUuid": "5d2e8a01c7b94f36a0e5d2c9b17f4368"
  },
  "memory": {
    "ramType": "LPDDR4X",
    "variantsGb": [4],
    "variantsMb": [4096]
  },
  "storage": {
    "storageType": "eMMC 5.1",
    "variantsGb": [128],
    "variantsManifestMb": [128000],
    "skuCombinations": ["4GB+128GB"]
  },
  "peripherals": {
    "fingerprint": "侧边电容指纹 (FPC)",
    "touchControllerHints": ["focaltech_ats"],
    "keyboardDriverHint": "gpio-keys",
    "headsetJackNameHint": "ums9230-snd-card Headset Jack",
    "pmicKeysNameHint": "sprd-pmic-keys",
    "sensors": ["accelerometer", "light", "proximity", "stepcounter", "stepdetector"],
    "sensorChips": {
      "accelerometer": "lis2dw12_acc",
      "light": "stk3335_l",
      "proximity": "stk3335_p"
    },
    "nfc": false,
    "infrared": false,
    "audioJack35mm": true,
    "speakers": "单扬声器 + 听筒"
  },
  "camera": {
    "rear": "50MP + 2MP",
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
    "macOuiCandidates": ["00:0e:8e"],
    "widevineDefault": "L3",
    "widevineSystemId": "37142",
    "widevineVersion": "17.0.0",
    "oemCryptoApiVersion": 17,
    "imeiTac": "35841009",
    "imeiSvn": "55",
    "meidPrefix": "99001796",
    "basebandVersion": "4G_MODEM_22B_W26.02.5_P2",
    "rilImpl": "sprd-ril"
  },
  "inputAlignment": {
    "touch": {
      "name": "focaltech_ats",
      "bus": 24,
      "vendor": 0,
      "product": 0,
      "version": 0,
      "mtSlots": 10,
      "mtPressure": true,
      "touchKeys": []
    },
    "keyboard": {
      "name": "gpio-keys",
      "bus": 25,
      "vendor": 1,
      "product": 1,
      "version": 256
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
          "name": "ums9230-snd-card Headset Jack",
          "bus": 25,
          "phys": "ALSA",
          "keys": ["KEY_PLAYPAUSE", "BTN_1", "BTN_2", "KEY_VOICECOMMAND"],
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
    {"type": "cputop0-tzone0", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "ank0-tzone1", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "ank1-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "ank2-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "prometheus7-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "cputop1-thmzone", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "gpu-thmzone", "base-temp-mc": 30200, "jitter-mc": 220},
    {"type": "ank3-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "ank4-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "ank5-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "prometheus6-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "skin-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "osctsen-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "outtsen-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
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
      "cycleCount": 86,
      "fullUah": 5000,
      "counterMin": 4947,
      "counterMax": 4972,
      "fullMv": 4400,
      "chargeMv": 4470,
      "temperatureFullDecic": 310,
      "temperatureChargeDecic": 390,
      "currentFullUa": -52000,
      "currentChargeUa": -2000000,
      "wireless": 0,
      "stateOfHealth": 99
    },
    "vibrator": {
      "motorType": "ERM 转子马达",
      "registerManager": true,
      "capabilities": 0,
      "managerCapabilities": 0,
      "compositionDelayMax": 0,
      "compositionSizeMax": 0,
      "vibratorIds": [1],
      "effects": ["CLICK", "DOUBLE_CLICK", "TICK"]
    },
    "gnss": {
      "systemInfoName": "SPRD_GNSS_default",
      "gnssCapabilities": 991,
      "powerCapabilities": 1,
      "measurementCorrectionsCapabilities": 0,
      "yearOfHw": 2020,
      "defaultSatellites": 7,
      "dualFrequencyL5": false
    },
    "thermal": {
      "skinName": "SKIN",
      "skinCelsius": 28.1,
      "modelMaximumCelsius": 50,
      "coolingDeviceName": "devfreq-23000000.mali",
      "sensors": [
        {"id": 0, "name": "CPU", "value": 31.2, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 1, "name": "GPU", "value": 30.1, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 2, "name": "BATTERY", "value": 28.6, "thresholds": "nan;nan;nan;50;55;59;60", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 3, "name": "SKIN", "value": 28.1, "thresholds": "nan;43;45;50;70;80;90", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 4, "name": "POWER_AMPLIFIER", "value": 29.5, "thresholds": "nan;nan;nan;68;90;100;110", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 5, "name": "SOC", "value": 31.2, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 6, "name": "CHARGER", "value": 28.8, "thresholds": "nan;nan;nan;45;50;55;60", "shutdown": "nan;nan;nan;nan;nan;nan;nan"}
      ]
    },
    "lightEntries": [
      {"id": 0, "ordinal": 0, "type": "BACKLIGHT"},
      {"id": 2, "ordinal": 0, "type": "BUTTONS"},
      {"id": 3, "ordinal": 0, "type": "BATTERY"},
      {"id": 4, "ordinal": 0, "type": "NOTIFICATIONS"},
      {"id": 5, "ordinal": 0, "type": "ATTENTION"}
    ],
    "sensors": [
      {"handle": 1, "name": "lis2dw12_acc", "vendor": "STMicroelectronics", "version": 1, "typeId": 1, "type": "android.sensor.accelerometer", "maxRange": "78.453605652", "resolution": "0.0024", "power": "0.11", "minDelay": 5000, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 5, "name": "stk3335_l", "vendor": "Sensortek", "version": 1, "typeId": 5, "type": "android.sensor.light", "maxRange": "65535.0", "resolution": "1.0", "power": "0.1", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 8, "name": "stk3335_p", "vendor": "Sensortek", "version": 1, "typeId": 8, "type": "android.sensor.proximity", "maxRange": "5.0", "resolution": "1.0", "power": "0.1", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 1000000, "wakeUp": true, "reporting": 1},
      {"handle": 9, "name": "GRAVITY", "vendor": "Unisoc", "version": 1, "typeId": 9, "type": "android.sensor.gravity", "maxRange": "78.453605652", "resolution": "0.0024", "power": "0.11", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 10, "name": "LINEARACCEL", "vendor": "Unisoc", "version": 1, "typeId": 10, "type": "android.sensor.linear_acceleration", "maxRange": "78.453605652", "resolution": "0.0024", "power": "0.11", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 17, "name": "SIGNIFICANT_MOTION", "vendor": "Unisoc", "version": 1, "typeId": 17, "type": "android.sensor.significant_motion", "maxRange": "1.0", "resolution": "1.0", "power": "0.11", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 18, "name": "STEP_DETECTOR", "vendor": "Unisoc", "version": 1, "typeId": 18, "type": "android.sensor.step_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.11", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 0, "wakeUp": false, "reporting": 3},
      {"handle": 19, "name": "STEP_COUNTER", "vendor": "Unisoc", "version": 1, "typeId": 19, "type": "android.sensor.step_counter", "maxRange": "2147483648.0", "resolution": "1.0", "power": "0.11", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 22, "name": "TILT_DETECTOR", "vendor": "Unisoc", "version": 1, "typeId": 22, "type": "android.sensor.tilt_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.11", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 3},
      {"handle": 27, "name": "DEVICE_ORIENTATION", "vendor": "Unisoc", "version": 1, "typeId": 27, "type": "android.sensor.device_orientation", "maxRange": "3.0", "resolution": "1.0", "power": "0.11", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 35, "name": "UNCALI_ACC", "vendor": "Unisoc", "version": 1, "typeId": 35, "type": "android.sensor.accelerometer_uncalibrated", "maxRange": "78.453605652", "resolution": "0.0024", "power": "0.11", "minDelay": 5000, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0}
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
        {"name": "android.hardware.media.c2@1.1-unisoc-service", "store": "rank1", "rank": 1}
      ],
      "components": [
        {"name": "c2.unisoc.jpeg.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/jpeg", "store": "rank1", "rank": 1},
        {"name": "c2.unisoc.imaadpcm.decoder", "kind": "decoder", "domain": "audio", "mediaType": "audio/ima-adpcm", "store": "rank1", "rank": 1},
        {"name": "c2.unisoc.mp3.decoder", "kind": "decoder", "domain": "audio", "mediaType": "audio/mpeg", "store": "rank1", "rank": 1},
        {"name": "c2.unisoc.mpl1.decoder", "kind": "decoder", "domain": "audio", "mediaType": "audio/mpeg-L1", "store": "rank1", "rank": 1},
        {"name": "c2.unisoc.mpl2.decoder", "kind": "decoder", "domain": "audio", "mediaType": "audio/mpeg-L2", "store": "rank1", "rank": 1},
        {"name": "c2.unisoc.hevc.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/hevc", "store": "rank1", "rank": 1},
        {"name": "c2.unisoc.mpeg4.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/mp4v-es", "store": "rank1", "rank": 1},
        {"name": "c2.unisoc.h263.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/3gpp", "store": "rank1", "rank": 1},
        {"name": "c2.unisoc.avc.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/avc", "store": "rank1", "rank": 1},
        {"name": "c2.unisoc.av1.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/av01", "store": "rank1", "rank": 1},
        {"name": "c2.unisoc.vp8.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/x-vnd.on2.vp8", "store": "rank1", "rank": 1},
        {"name": "c2.unisoc.vp9.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/x-vnd.on2.vp9", "store": "rank1", "rank": 1},
        {"name": "c2.unisoc.jpeg.encoder", "kind": "encoder", "domain": "video", "mediaType": "video/jpeg", "store": "rank1", "rank": 1},
        {"name": "c2.unisoc.mp3.encoder", "kind": "encoder", "domain": "audio", "mediaType": "audio/mpeg", "store": "rank1", "rank": 1},
        {"name": "c2.unisoc.avc.encoder", "kind": "encoder", "domain": "video", "mediaType": "video/avc", "store": "rank1", "rank": 1},
        {"name": "c2.unisoc.hevc.encoder", "kind": "encoder", "domain": "video", "mediaType": "video/hevc", "store": "rank1", "rank": 1}
      ]
    },
    "audioEngine": {
      "modules": "default,bluetooth,r_submix",
      "voiceCallIndexMax": 7,
      "musicIndexMax": 15
    }
  },
  "alignmentNotes": [
    "机型判定：BLU G65L（G1130/G1130WW），6.7\" IPS 720x1600 90Hz、Unisoc T7200（T606 同源，固件 socModel=UMS9230T）、4GB LPDDR4X + 128GB、5000mAh Li-Poly 10W、Android 14；仅 4GB+128GB 一个发售档位。",
    "CPU 频率/容量沿用固件实测：policy0 6×A55 最高 1820000kHz（capacity 544），policy6 2×A75 最高 2028000kHz（capacity 1024），MIDR 0x412fd050 / 0x410fd0b0；调频驱动按 Unisoc 平台写 sprd-cpufreq + sprd_interactive，替代 gold 的 mt-cpufreq/sugov_ext。",
    "GPU 替换为 Mali-G57 MP1（gold 为 MC2）：glVersionFull 按 ARM r38p1 分支给出，具体 git hash、3 个 Vulkan UUID、vulkanDeviceId 需以真机 glGetString/VkPhysicalDeviceProperties 实测替换；GL 扩展集为 Mali 家族通用项，未沿用小蒙 MIUI 参数。",
    "基带直接采用固件实测 4G_MODEM_22B_W26.02.5_P2（gsm.version.baseband），RIL 按 Unisoc 平台写 sprd-ril；未使用 gold 的 MOLY.* 或 mtk-ril。",
    "触控/按键为固件实测：focaltech_ats（FocalTech，I2C bus 24）+ gpio-keys（bus 25），不使用 mtk-kpd；耳机插孔按 Unisoc 声卡命名 ums9230-snd-card Headset Jack，PMIC 键为 sprd-pmic-keys。",
    "传感器严格按固件 permissions 的 sensorFeatures（accelerometer/light/proximity/stepcounter/stepdetector）建模：无陀螺、无磁力计、无气压计，故不生成 rotation_vector/geomagnetic 等依赖陀螺/磁力的融合项，也不填任何 xiaomi.sensor.*/oplus.sensor.*/vivo.sensor.* 私有传感器。",
    "传感器具体料号（lis2dw12_acc / stk3335_l / stk3335_p）为 Unisoc 低价平台参考设计推断，需真机 dumpsys sensorservice 或 /sys/class/sensors 核对后定稿；融合层名称归 Unisoc 而非 MTK。",
    "热区列表 18 项全部来自固件实测（soc/cputop0/cputop1/ank0-5/prometheus6-7/gpu/skin/osctsen/outtsen/board/pa/chg），base/jitter 直接沿用实测；thermal_config 的 coolingDeviceName 按 UMS9230 Mali 节点推断为 devfreq-23000000.mali，需真机 /sys/class/thermal 核对。",
    "无 NFC、无红外、无 5G：enableNfc/enableIr 均 false，radioVariant 按规则对非 MTK 平台填 default；vintfDrop 留空，ifaa/soter/tui 三个占位已由 permissions 声明 unavailable，不在 VINTF 层硬删。",
    "电池 Li-Poly 5000mAh、10W（5V/2A）有线、无无线充；eMMC 5.1 与 LPDDR4X 为公开规格推断，manifestMb 记 128000。",
    "Widevine 按 720p 低价机定 L3（systemId/version 为待真机 provisioning 复核项）；IMEI TAC、MEID 前缀、Wi-Fi MAC OUI（00:0e:8e 为 Spreadtrum/Unisoc 常见段）需真机 getprop/ip link 校准后回填。",
    "display 使用物理可视面板 70×155mm（公开规格 69.84×155.19mm），密度 320dpi（固件实测 densityDpi=320），刷新率 60/90、默认 60；EDID 厂商/面板 ID/年份周数为推断值，需读 /sys/class/drm 的 edid 固件实测替换。",
    "音效描述符直接采用固件实测 13 条（AOSP + NXP + Unisoc voice processing），未加入 Dolby/MiSound 等该机不具备的方案；Codec2 清单来自固件 media_codecs_c2_v1.xml，假登记统一 store=rank1、rank=1，不占用 default。"
  ],
  "sources": [
    "https://www.devicespecifications.com/en/model/34a5632e",
    "https://www.gsmchoice.com/en/catalogue/blu/g65l/",
    "https://www.bluproducts.com/devices/g65l/",
    "目标机固件实测数据（build.prop / boot DTB / VINTF / vendor/etc，由 02_prepare 解包提供）"
  ]
}
```
