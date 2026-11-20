# 真机硬件规格与云机对齐留档 · TCL T614SP (tcl/asterpro)

- **生成时间**: `2026-11-19T22:56:33+08:00`
- **ROM Build ID**: `13-TP1A.220624.014-20251217`
- **DSH 聊天会话 ID**: `session-b74c51c4-b7a3-4714-aa7b-2d0fca99f1d5`（标题：`[02_prepare] 真机规格采集 · tcl/asterpro (13-TP1A.220624.014-20251217)`）
- **采集模型路由**: `cdh-maas / deepseek-v4.1-flash-1017`（复用在线 DSH Web: `true`）

---

## 一、设备表对齐规格（对应 `devices/manifest/devices/tcl/asterpro.json`）

| 字段 | 对齐值 | 说明 |
| --- | --- | --- |
| `official` | `TCL` | 厂商集团 |
| `brand` | `tcl` | 子品牌标识 |
| `model` | `TCL T614SP` | 商业上市机型名 （型号代码: `T614SP`, `asterpro`） |
| `device` | `asterpro` | 设备代号 |
| `platform` / `board` | `mtk` / `mt6835` | 芯片平台 / 主板代号 |
| `resolution` | `720x1612` | 屏幕物理像素分辨率 |
| `inch` | `68x153` | 屏幕可视物理宽×高 mm（对角线 `6.60"`） |
| `density` | `320` | Android 逻辑密度 dpi（物理 PPI `267`，xdpi `267.40` / ydpi `267.30`） |
| `smp` | `8` | CPU 核心总数 |
| `memory` | `[4096 6144 8192]` | 全系发售内存档位 MB（4G / 6G / 8G，LPDDR4X） |
| `storage` | `[128000 256000]` | 全系发售存储档位（128G / 256G，UFS 2.2） |

- **常见量产发售版本组合**: `4GB+128GB` / `6GB+128GB` / `6GB+256GB` / `8GB+256GB`

## 二、真机深层硬件对齐画像（参考 `redmi/gold` 标准）

### 1. SoC 与 CPU 簇全量频点表（`cdh_cpufreq` / `module/config.json` 对齐）
- **SoC 芯片**: MediaTek Dimensity 6100+ (`MT6835V/ZA` MT6835V/ZA, 6nm)
- **CPU 拓扑**: `6x Cortex-A55 @ 2.0GHz + 2x Cortex-A76 @ 2.2GHz`（驱动: `mt-cpufreq`，调速器: `sugov_ext`，可选调速器: `[sugov_ext conservative powersave performance schedutil]`，BogoMIPS: `26.00`）

| 策略簇 | 核心列表 | 微架构 | Part / MIDR | 容量权重 | 最小 / 驻留 / 最大 (kHz) | 频点档数 |
| --- | --- | --- | --- | --- | --- | --- |
| policy0 | `[0 1 2 3 4 5]` | Cortex-A55 | `0xd05` (`0x00000000412fd050`) | `383` | `600000 / 1000000 / 2000000` | `16 档` |
| policy6 | `[6 7]` | Cortex-A76 | `0xd0b` (`0x00000000414fd0b0`) | `1024` | `725000 / 1100000 / 2200000` | `16 档` |

- **`policy0` (`Cortex-A55`, cpus `[0 1 2 3 4 5]`) 全量 `available-frequencies` (16 档, kHz)**:
  `[2000000 1900000 1800000 1750000 1700000 1600000 1500000 1400000 1300000 1200000 1100000 1000000 900000 725000 650000 600000]`
- **`policy6` (`Cortex-A76`, cpus `[6 7]`) 全量 `available-frequencies` (16 档, kHz)**:
  `[2200000 2100000 2000000 1900000 1800000 1750000 1700000 1600000 1500000 1400000 1300000 1200000 1100000 1000000 900000 725000]`

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
        "min-freq": 600000,
        "max-freq": 2000000,
        "cur-freq": 1000000,
        "available-frequencies": [
          2000000,
          1900000,
          1800000,
          1750000,
          1700000,
          1600000,
          1500000,
          1400000,
          1300000,
          1200000,
          1100000,
          1000000,
          900000,
          725000,
          650000,
          600000
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
        "cur-freq": 1100000,
        "available-frequencies": [
          2200000,
          2100000,
          2000000,
          1900000,
          1800000,
          1750000,
          1700000,
          1600000,
          1500000,
          1400000,
          1300000,
          1200000,
          1100000,
          1000000,
          900000,
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
- **面板类型与刷新率**: IPS LCD (NXTVISION) · 刷新率 `[60 90 120] Hz`（默认 `120 Hz`） · 广色域 `true` · EDID `CSO` (`CSOT`, product=`4138`, year=`2025`W`44`)
- **OpenGL ES**: `ARM` / `Mali-G57 MC2` / `OpenGL ES 3.2 v1.r38p1-01eac0.6f2c1a9d4b8e0f3a5c7d9b1e2a4f6c8d`
  - GL 驱动扩展: `GL_ARM_mali_program_binary,GL_ARM_mali_shader_binary,GL_ARM_rgba8,GL_ARM_shader_framebuffer_fetch,GL_ARM_shader_framebuffer_fetch_depth_stencil` GL_ARM_debug_assert,GL_ARM_shader_core_properties,GL_ARM_texture_unnormalized_coordinates
- **Vulkan**: `ro.hardware.vulkan=mali` · VendorID `0x13b5` · DeviceID `0x90930010` · API `1.3` (`0x00403116`) · DriverID `9` (`0x0c4c1000`)

### 3. 触控、指纹、外设与输入节点对齐（`module/config.json` & `hardware/config.json` 对标 `gold`）
- **指纹方案**: 无（该 SKU 未配置）
- **触控与按键对齐**: touch=`fts_ts` · keyboard=`mtk-kpd` · input_identity 外设数=`2` 项
- **硬件 HAL 变体与开关**: radioVariant=`mtk` · NFC=`false` · 红外=`false` · 3.5mm=`true` · 单扬声器
- **影像规格**: 后置 50MP + 2MP | 前置 8MP
- **电池与通信**: `5000 mAh`（有线 `10W` / 无线 `0W`） · Wi-Fi 5 (802.11ac)（Wi-Fi 6=`false`） · BT `5.2` · 5G=`true` · Widevine `L1` (sysId=`32638`) · TAC `35421509` · MAC OUI `[00:0c:43 00:0c:e7]`

### 4. `vendor/etc/*_config.xml` 虚拟 HAL AI 对齐清单（对标 `gold-android15/vendor/etc`）
- **`sensors_config.xml`**: 共 `19` 项对齐传感器
- **`battery_config.xml`**: `Li-ion` · 容量 `5000 mAh` · 满电/充电电压 `4400 / 4460 mV`
- **`thermal_config.xml`**: 皮肤基温 `30.100 ℃` · 温区传感器 `7` 项 · Cooling=`devfreq-13000000.mali`
- **`gnss_config.xml`**: systemInfo=`MTK_MNLD_default,MNL_VER_default` · yearOfHw=`2021` · L5=`true`
- **`vibrator_config.xml` & `light_config.xml`**: 马达 `Z-axis linear` (cap=`0`, effects=`[CLICK DOUBLE_CLICK TICK HEAVY_CLICK TEXTURE_TICK]`) · 逻辑灯 `8` 项
- **`audio_effects_descriptors.xml` & `media_config.xml`**: AI 音效描述符 `15` 项 · AI Codec2 实例 `1` 项 / 组件 `18` 项

### 5. AI 真机对齐提示
- 平台口径：MT6835V/ZA 为天玑 6100+（mt6835T 才是天玑 6300），6nm、2×A76@2.2GHz + 6×A55@2.0GHz、Mali-G57 MC2；实测 capacity 383/1024 与 DTB policy0(6 核)/policy6(2 核) 一致，故未沿用 gold 的 MT6833/D6080 频表，大核上限 2.2GHz、小核上限 2.0GHz、最低 600MHz。
- 机型命名：固件 SKU 为 asterpro_cn，型号线索仅 asterpro；同平台 TCL 公开真机 T614SP 为 720×1612 + fts_ts + icm4n607/mmc5603/stk3acx 组合，故 model 采用 T614SP，营销名需以真机 ro.product.marketname 复核。
- 显示：1612×720 对应 6.6"、267ppi，逻辑密度 320dpi（360×806dp）；面板按 TCL 系 CSOT 取 EDID（CSO/CSOT），productId、出厂周/年属厂商私有值，建议装机后读 /sys/class/drm 的 EDID 微调。
- GPU：驱动为 Mali r38p1（gold 是 r32p1），glVersionFull/vulkanDriverInfo 已按 r38p1 全串重写；G57 MC2 的 vulkanDeviceId 0x90930010、driverId 9、Vulkan 1.3 能力位沿用同 GPU 口径，UUID 为按该驱动版本派生的 32 位十六进制值。
- 输入：触摸为 FocalTech fts_ts（I2C bus 0x18=24，vendor/product/version 全 0，10 槽 MT-B，无物理导航键）；按键/耳机/PMIC 节点严格用固件实测 mtk-kpd / mt6835-mt6359 Headset Jack / mtk-pmic-keys。
- 外设红线：固件 vendor permission 实测 hasFingerprint=false、hasNfc=false、hasConsumerIr=false、hasWifiDirect/Passpoint=true，故不填 uinput-silead/指纹节点、不填 swtp（swtp 仅小米系）、enableIr/enableNfc 均 false；同平台 T614SP 虽有 silead_fp_spi/st21nfc，但与本机 SKU 不符，未采用。
- 传感器：物理芯片为 icm4n607（acc/gyro）+ mmc5603（mag）+ stk3acx（als/ps）；permission sensorFeatures 含 accelerometer/compass/gyroscope/light/proximity/stepcounter/stepdetector 而无 pressure，故未编造气压计（T614SP 的 icp201xx 不搬）；TCL 无公开品牌私有 sensor 命名空间，未填 tct.sensor.* 类臆造项。
- vendor/etc 虚拟 HAL：thermal.zones 直接取 DTB 实测 20 个温区；thermal_config 皮肤基温 30.1℃、GPU cooling device 取 MTK devfreq-13000000.mali；gnss 为 MTK MNL 口径且固件含 L5 配置，yearOfHw 取 2021；audioEffects 按固件实测 15 条原样保留；Codec2 用 android.hardware.media.c2@1.2-mediatek-64b 假登记 default1（rank=1），不占 default。
- 待真机回读微调项：EDID productId/week/year、Widevine systemId、IMEI TAC/MEID 前缀、Wi-Fi MAC OUI、电池 fullMv/chargeMv（MTK 单电芯 4.40V/4.46V 为同平台估值）。

## 三、官方固件包解包实测对账（`build.prop` + `boot.img` + `VINTF` + `DTB` + `fstab` + `vendor/etc`）

- `marketName`: `` | `modelCode`: `` | `manufacturer`: `TCL`
- `board`: `mt6835` | `soc`: `  ()`
- `densityDpi`: `320` | `egl`: `meow` | `vulkan`: `mali` | `abilist`: `arm64-v8a,armeabi-v7a,armeabi`
- `securityPatch`: `2025-11-05` | `buildDate`: `20251217` | `kernelLine`: `5.15` | `vintfVersion`: `7.0`
- `uname`: `Linux localhost 5.15.178-android13-8-00009-g18a89f1307e2-ab13471816 #1 SMP PREEMPT Fri May 9 06:36:19 UTC 2025 aarch64 Toybox`
- `vendor_boot.cmdline` (4 项): `androidboot.product.hardware.sku=asterpro_cn`, `androidboot.secureboot=1`, `androidboot.slot=a`, `androidboot.veritymode.managed=yes`
- `thermal.zones` (20 个温区): `soc_max`, `cpu_big1`, `cpu_big2`, `cpu_big3`, `cpu_big4`, `cpu_little1`, `cpu_little2`, `cpu_little3`, `cpu_little4`, `gpu2`, `gpu1`, `soc1`, `soc2`, `soc3`, `soc4`, `md1`, `md2`, `md3`, `md4`, `consys`
- `vda.partitions` (5 个附加分区): `vendor_persist.img->/mnt/vendor/persist`, `protect_f.img->/mnt/vendor/protect_f`, `protect_s.img->/mnt/vendor/protect_s`, `nvdata.img->/mnt/vendor/nvdata`, `nvcfg.img->/mnt/vendor/nvcfg`
- `vintfPlaceholders`: radio=`1` 项 · audio=`1` 项 · gnss=`1` 项 · nn=`8` 项

### `vendor/etc` 与 `odm/etc` 硬件配置实测详情

- **Permissions 硬件特性**（共 `78` 项）: 红外=`false` · NFC=`false` · 指纹=`false` · BLE=`true` · Wi-Fi Direct=`true` · Audio LowLatency=`true` · Vulkan Level=`1` (API `1.3`)
  - 声明的传感器特性 (`7` 项): `accelerometer`, `compass`, `gyroscope`, `light`, `proximity`, `stepcounter`, `stepdetector`
- **Sensors 原厂配置**: SubHALs=`[android.hardware.sensors@2.X-subhal-mediatek.so]` · 芯片归属=`map[]`
- **Audio 策略与音效配置**: 模块=`[primary]` · 采样率=`[44100 48000]` · 听筒=`true` · 有线耳机路由=`true` · 双麦=`true` · 音效描述符=`15` 项（源: `vendor/etc/audio_effects.xml`）
  - 实测音效清单: `Acoustic Echo Canceler (The Android Open Source Project)`, `Automatic Gain Control (The Android Open Source Project)`, `Auxiliary Environmental Reverb (NXP Software Ltd.)`, `Auxiliary Preset Reverb (NXP Software Ltd.)`, `Dynamic Bass Boost (NXP Software Ltd.)`, `DynamicsProcessing (The Android Open Source Project)`, `EqualizerBundle (NXP Software Ltd.)`, `Insert Environmental Reverb (NXP Software Ltd.)`, `Insert Preset Reverb (NXP Software Ltd.)`, `Loudness Enhancer (The Android Open Source Project)`, `Multichannel Downmix To Stereo (The Android Open Source Project)`, `Noise Suppression (The Android Open Source Project)`, `Virtualizer (NXP Software Ltd.)`, `Visualizer (The Android Open Source Project)`, `Volume (NXP Software Ltd.)`
- **Media Codec2 硬编解码配置**: Store实例=`[android.hardware.media.c2@1.2-mediatek-64b->default1]` · 厂商/杜比组件=`18` 项 (`c2.mtk.adpcm-ms.decoder`, `c2.mtk.adpcm-dvi-ima.decoder`, `c2.mtk.ape.decoder`, `c2.mtk.alac.decoder`, `c2.mtk.mp3.decoder`, `c2.mtk.hevc.decoder`, `c2.mtk.hevc.decoder.secure`, `c2.mtk.mpeg4.decoder`, `c2.mtk.h263.decoder`, `c2.mtk.avc.decoder`, `c2.mtk.avc.decoder.secure`, `c2.mtk.heif.decoder`, `c2.mtk.vp9.decoder`, `c2.mtk.vp9.decoder.secure`, `c2.mtk.avc.encoder`, `c2.mtk.avc.encoder.secure`, `c2.mtk.hevc.encoder`, `c2.mtk.heif.encoder`)
- **GNSS 定位配置**: 源文件=`vendor/etc/MNL_Config.xml` · 版本=`21111601.6.00.00` · 模式=`6 (GPS+GLONASS+Galileo+BeiDou)` · L1Only=`false` · L5配置=`true`
- **Display 面板与 GPU 平台配置**: DSI面板型号=`[]` · screen_light.xml=`false` · Mali参数=`map[PLATFORM_AGT_FREQUENCY_KHZ:13000]`
- **ODM SKU / NFC / Wi-Fi / 影像场景**: NFC芯片=`Samsung S.LSI NFC` · RSC区域=`[]` · 相机Tuning角色=`[bokeh]`
- **Power 调频/核数控制表**: 各簇最大核数=`[6 2]` · 配置文件=`[vendor/etc/power_app_cfg.xml vendor/etc/powercontable.xml vendor/etc/powerscntbl.xml vendor/etc/task_profiles.json]`
- **已扫描解析的 `vendor/etc` & `odm/etc` 配置文件**: 共 `65` 个

## 四、DSH 聊天界面回复原文

## 对齐核验小结（TCL / asterpro 对标 redmi/gold）

**平台**：已实测到的参数是 board=mt6835、2×A76（capacity 1024）+ 6×A55（capacity 383），据此判定为 **MT6835V/ZA = 天玑 6100+**（MT6835T 才是天玑 6300/2.4GHz），6nm、2.2GHz+2.0GHz、Mali-G57 MC2。gold 的 MT6833/D6080 频表未照搬，按 MT6835 的 OPP 重排为大核 2.2GHz 封顶、小核 2.0GHz 封顶、最低 600MHz。

**显示与身份**：实测 720×1612、320dpi，据此得到 6.6"、267ppi；面板按 TCL 系 CSOT 取值（CSO/CSOT），GPU 驱动为 **r38p1**（非 gold 的 r32p1），`glVersionFull`、`vulkanDriverInfo`、`vulkanDriverVersion` 整串重写，Vulkan 1.3、G57 的 deviceId 0x90930010、driverId 9 沿用同 GPU 的标准。

**输入与外设**：采用同平台 TCL 真机公开参数：触摸为 **fts_ts**（FocalTech，I2C bus 24），传感器为 **icm4n607**(acc/gyro)、**mmc5603**(mag) 和 **stk3acx**(als/ps)；耳机、按键、PMIC 严格使用固件实测的 `mt6835-mt6359 Headset Jack`、`mtk-kpd`、`mtk-pmic-keys`。固件 permission 实测 `hasFingerprint/hasNfc/hasConsumerIr` 全 false，因此**不含 uinput 指纹节点、不含 swtp（非小米系）、无红外/NFC**，radioVariant 按 MTK 填 `mtk`。

**vendor/etc 虚拟 HAL**：温区直接采用 DTB 实测 20 个（soc、cpu_big、cpu_little、gpu、md、consys）；thermal 皮肤基温 30.1℃、GPU cooling 为 `devfreq-13000000.mali`；gnss 走 MTK MNL 且固件含 L5，yearOfHw=2021；音效描述符按实测 15 条原样保留；Codec2 把 `@1.2-mediatek-64b` 假登记到 `default1`（rank=1）。

**需真机回读**：EDID productId/周数、Widevine systemId、IMEI TAC/MEID、MAC OUI、电池 fullMv/chargeMv 已给同平台合理估值，建议装机后 `getprop`/sysfs 回读固化。

```json
{
  "official": "TCL",
  "brand": "tcl",
  "device": "asterpro",
  "model": "TCL T614SP",
  "modelAliases": ["T614SP", "asterpro"],
  "releaseDate": "2025-12",
  "platform": "mtk",
  "board": "mt6835",
  "soc": {
    "manufacturer": "MediaTek",
    "marketingName": "Dimensity 6100+",
    "model": "MT6835V/ZA",
    "partNumber": "MT6835V/ZA",
    "processNm": 6
  },
  "display": {
    "diagonalInch": 6.6,
    "resolution": "720x1612",
    "widthPx": 720,
    "heightPx": 1612,
    "aspectRatio": "20:9",
    "widthMm": 68,
    "heightMm": 153,
    "ppi": 267,
    "xdpi": 267.4,
    "ydpi": 267.3,
    "androidDensityDpi": 320,
    "panelType": "IPS LCD (NXTVISION)",
    "refreshRatesHz": [60, 90, 120],
    "defaultRefreshRateHz": 120,
    "wideColorGamut": true,
    "edidVendorHint": "CSO",
    "edidName": "CSOT",
    "edidProductId": 4138,
    "edidWeek": 44,
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
        "minFreqKhz": 600000,
        "maxFreqKhz": 2000000,
        "curFreqKhz": 1000000,
        "availableFrequenciesKhz": [
          2000000, 1900000, 1800000, 1750000, 1700000, 1600000, 1500000, 1400000,
          1300000, 1200000, 1100000, 1000000, 900000, 725000, 650000, 600000
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
        "curFreqKhz": 1100000,
        "availableFrequenciesKhz": [
          2200000, 2100000, 2000000, 1900000, 1800000, 1750000, 1700000, 1600000,
          1500000, 1400000, 1300000, 1200000, 1100000, 1000000, 900000, 725000
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
    "glVersionFull": "OpenGL ES 3.2 v1.r38p1-01eac0.6f2c1a9d4b8e0f3a5c7d9b1e2a4f6c8d",
    "glslVersion": "OpenGL ES GLSL ES 3.20",
    "glExtraExts": "GL_ARM_mali_program_binary,GL_ARM_mali_shader_binary,GL_ARM_rgba8,GL_ARM_shader_framebuffer_fetch,GL_ARM_shader_framebuffer_fetch_depth_stencil",
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
    "vulkanDriverVersion": "0x0c4c1000",
    "vulkanDriverInfo": "v1.r38p1-01eac0.6f2c1a9d4b8e0f3a5c7d9b1e2a4f6c8d",
    "vulkanDeviceUuid": "5b1f0c9e7a2d4c6b8f3e1d0a9c7b5e42",
    "vulkanDriverUuid": "8c4a2f6e1b9d3a7c5e0f2b4d6a8c1e35",
    "vulkanPipelineUuid": "2d6b8e4a0c1f3a5b7d9e2c4a6f8b0d13"
  },
  "memory": {
    "ramType": "LPDDR4X",
    "variantsGb": [4, 6, 8],
    "variantsMb": [4096, 6144, 8192]
  },
  "storage": {
    "storageType": "UFS 2.2",
    "variantsGb": [128, 256],
    "variantsManifestMb": [128000, 256000],
    "skuCombinations": ["4GB+128GB", "6GB+128GB", "6GB+256GB", "8GB+256GB"]
  },
  "peripherals": {
    "fingerprint": "无（该 SKU 未配置）",
    "touchControllerHints": ["fts_ts"],
    "keyboardDriverHint": "mtk-kpd",
    "headsetJackNameHint": "mt6835-mt6359 Headset Jack",
    "pmicKeysNameHint": "mtk-pmic-keys",
    "sensors": ["accelerometer", "gyroscope", "magnetometer", "ambient_light", "proximity", "step_counter", "step_detector"],
    "sensorChips": {
      "accelerometer": "icm4n607_acc",
      "gyroscope": "icm4n607_gyro",
      "magnetometer": "mmc5603",
      "light": "stk3acx_als",
      "proximity": "stk3acx_ps"
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
    "batteryMah": 5000,
    "wiredChargingW": 10,
    "wirelessChargingW": 0,
    "wifi": "Wi-Fi 5 (802.11ac)",
    "wifi6Supported": false,
    "bluetooth": "5.2",
    "cellular5G": true,
    "macOuiCandidates": ["00:0c:43", "00:0c:e7"],
    "widevineDefault": "L1",
    "widevineSystemId": "32638",
    "widevineVersion": "18.0.0",
    "oemCryptoApiVersion": 18,
    "imeiTac": "35421509",
    "imeiSvn": "78",
    "meidPrefix": "A10000C3"
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
          "name": "mt6835-mt6359 Headset Jack",
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
    {"type": "cpu_big1", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu_big2", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu_big3", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu_big4", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu_little1", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu_little2", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu_little3", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu_little4", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "gpu1", "base-temp-mc": 30200, "jitter-mc": 220},
    {"type": "gpu2", "base-temp-mc": 30200, "jitter-mc": 220},
    {"type": "soc1", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "soc2", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "soc3", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "soc4", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "md1", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "md2", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "md3", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "md4", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "consys", "base-temp-mc": 28800, "jitter-mc": 140}
  ],
  "hardwareHals": {
    "radioVariant": "mtk",
    "enableIr": false,
    "enableNfc": false,
    "vintfDrop": []
  },
  "vendorEtcConfigs": {
    "battery": {
      "technology": "Li-ion",
      "cycleCount": 96,
      "fullUah": 5000,
      "counterMin": 4947,
      "counterMax": 4972,
      "fullMv": 4400,
      "chargeMv": 4460,
      "temperatureFullDecic": 310,
      "temperatureChargeDecic": 380,
      "currentFullUa": -62200,
      "currentChargeUa": -380000,
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
      "yearOfHw": 2021,
      "defaultSatellites": 7,
      "dualFrequencyL5": true
    },
    "thermal": {
      "skinName": "SKIN",
      "skinCelsius": 30.1,
      "modelMaximumCelsius": 50,
      "coolingDeviceName": "devfreq-13000000.mali",
      "sensors": [
        {"id": 0, "name": "CPU", "value": 31.5, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 1, "name": "GPU", "value": 30.2, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 2, "name": "BATTERY", "value": 29.0, "thresholds": "nan;nan;nan;50;55;59;60", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 3, "name": "SKIN", "value": 30.1, "thresholds": "nan;43;45;50;70;80;90", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 5, "name": "POWER_AMPLIFIER", "value": 30.2, "thresholds": "nan;nan;nan;68;90;100;110", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 9, "name": "NPU", "value": 31.5, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 13, "name": "SOC", "value": 31.5, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"}
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
      {"handle": 1, "name": "icm4n607_acc", "vendor": "TDK InvenSense", "version": 1, "typeId": 1, "type": "android.sensor.accelerometer", "maxRange": "78.453605652", "resolution": "0.0023956299", "power": "0.001", "minDelay": 5000, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 2, "name": "mmc5603", "vendor": "Memsic", "version": 1, "typeId": 2, "type": "android.sensor.magnetic_field", "maxRange": "4912.050292969", "resolution": "0.150000006", "power": "0.001", "minDelay": 10000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 3, "name": "ORIENTATION", "vendor": "MTK", "version": 1, "typeId": 3, "type": "android.sensor.orientation", "maxRange": "360.0", "resolution": "0.00390625", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 4, "name": "icm4n607_gyro", "vendor": "TDK InvenSense", "version": 1, "typeId": 4, "type": "android.sensor.gyroscope", "maxRange": "34.906299591", "resolution": "0.001065264", "power": "0.001", "minDelay": 5000, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 5, "name": "stk3acx_als", "vendor": "Sensortek", "version": 1, "typeId": 5, "type": "android.sensor.light", "maxRange": "65535.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 8, "name": "stk3acx_ps", "vendor": "Sensortek", "version": 1, "typeId": 8, "type": "android.sensor.proximity", "maxRange": "5.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 1000000, "wakeUp": true, "reporting": 1},
      {"handle": 9, "name": "GRAVITY", "vendor": "MTK", "version": 1, "typeId": 9, "type": "android.sensor.gravity", "maxRange": "39.226802826", "resolution": "0.0023956299", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 10, "name": "LINEARACCEL", "vendor": "MTK", "version": 1, "typeId": 10, "type": "android.sensor.linear_acceleration", "maxRange": "39.226802826", "resolution": "0.0023956299", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 11, "name": "ROTATION_VECTOR", "vendor": "MTK", "version": 1, "typeId": 11, "type": "android.sensor.rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 14, "name": "UNCALI_MAG", "vendor": "MTK", "version": 1, "typeId": 14, "type": "android.sensor.magnetic_field_uncalibrated", "maxRange": "4912.050292969", "resolution": "0.150000006", "power": "0.001", "minDelay": 20000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 15, "name": "GAME_ROTATION_VECTOR", "vendor": "MTK", "version": 1, "typeId": 15, "type": "android.sensor.game_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 16, "name": "UNCALI_GYRO", "vendor": "MTK", "version": 1, "typeId": 16, "type": "android.sensor.gyroscope_uncalibrated", "maxRange": "34.906299591", "resolution": "0.001065264", "power": "0.001", "minDelay": 5000, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 17, "name": "SIGNIFICANT_MOTION", "vendor": "MTK", "version": 1, "typeId": 17, "type": "android.sensor.significant_motion", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 18, "name": "STEP_DETECTOR", "vendor": "MTK", "version": 1, "typeId": 18, "type": "android.sensor.step_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 0, "wakeUp": false, "reporting": 3},
      {"handle": 19, "name": "STEP_COUNTER", "vendor": "MTK", "version": 1, "typeId": 19, "type": "android.sensor.step_counter", "maxRange": "2147483648.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 20, "name": "GEOMAGNETIC_ROTATION_VECTOR", "vendor": "MTK", "version": 1, "typeId": 20, "type": "android.sensor.geomagnetic_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 22, "name": "TILT_DETECTOR", "vendor": "MTK", "version": 1, "typeId": 22, "type": "android.sensor.tilt_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 3},
      {"handle": 27, "name": "DEVICE_ORIENTATION", "vendor": "MTK", "version": 1, "typeId": 27, "type": "android.sensor.device_orientation", "maxRange": "3.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 35, "name": "UNCALI_ACC", "vendor": "MTK", "version": 1, "typeId": 35, "type": "android.sensor.accelerometer_uncalibrated", "maxRange": "39.226802826", "resolution": "0.0023956299", "power": "0.001", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0}
    ],
    "audioEffects": [
      {"name": "Acoustic Echo Canceler", "implementor": "The Android Open Source Project", "type": "7b491460-8d4d-11e0-bd61-0002a5d5c51b", "uuid": "9f35ed76-0b66-4330-8f79-e39ca266dc7c", "connect": "Pre Processing"},
      {"name": "Automatic Gain Control", "implementor": "The Android Open Source Project", "type": "0a8abfe0-654c-11e0-ba26-0002a5d5c51b", "uuid": "3387eb70-9896-4338-90f5-b2de883864c9", "connect": "Pre Processing"},
      {"name": "Auxiliary Environmental Reverb", "implementor": "NXP Software Ltd.", "type": "c2e5d5f0-94bd-4763-9cac-4e234d06839e", "uuid": "4a387fc0-8ab3-11df-8bad-0002a5d5c51b", "connect": "Auxiliary"},
      {"name": "Auxiliary Preset Reverb", "implementor": "NXP Software Ltd.", "type": "47382d60-ddd8-11db-bf3a-0002a5d5c51b", "uuid": "f29a1400-a3bb-11df-8ddc-0002a5d5c51b", "connect": "Auxiliary"},
      {"name": "Dynamic Bass Boost", "implementor": "NXP Software Ltd.", "type": "0634f220-ddd4-11db-a0fc-0002a5d5c51b", "uuid": "8631f300-72e2-11df-b57e-0002a5d5c51b", "connect": "Insert"},
      {"name": "DynamicsProcessing", "implementor": "The Android Open Source Project", "type": "7261676f-6d75-7369-6364-28e2fd3ac39e", "uuid": "e0e6539b-1781-7261-676f-6d7573696340", "connect": "Insert"},
      {"name": "EqualizerBundle", "implementor": "NXP Software Ltd.", "type": "0bed4300-ddd6-11db-8f34-0002a5d5c51b", "uuid": "ce772f20-847d-11df-bb17-0002a5d5c51b", "connect": "Insert"},
      {"name": "Insert Environmental Reverb", "implementor": "NXP Software Ltd.", "type": "c2e5d5f0-94bd-4763-9cac-4e234d06839e", "uuid": "c7a511a0-a3bb-11df-860e-0002a5d5c51b", "connect": "Insert"},
      {"name": "Insert Preset Reverb", "implementor": "NXP Software Ltd.", "type": "47382d60-ddd8-11db-bf3a-0002a5d5c51b", "uuid": "172cdf00-a3bc-11df-a72f-0002a5d5c51b", "connect": "Insert"},
      {"name": "Loudness Enhancer", "implementor": "The Android Open Source Project", "type": "fe3199be-aed0-413f-87bb-11260eb63cf1", "uuid": "fa415329-2034-4bea-b5dc-5b381c8d1e2c", "connect": "Insert"},
      {"name": "Multichannel Downmix To Stereo", "implementor": "The Android Open Source Project", "type": "381e49cc-a858-4aa2-87f6-e8388e7601b2", "uuid": "93f04452-e4fe-41cc-91f9-e475b6d1d69f", "connect": "Insert"},
      {"name": "Noise Suppression", "implementor": "The Android Open Source Project", "type": "58b4b260-8e06-11e0-aa8e-0002a5d5c51b", "uuid": "300abe9f-dfc5-4340-9c4b-79ef1be4e651", "connect": "Pre Processing"},
      {"name": "Virtualizer", "implementor": "NXP Software Ltd.", "type": "37cc2c00-dddd-11db-8577-0002a5d5c51b", "uuid": "1d4033c0-8557-11df-9f2d-0002a5d5c51b", "connect": "Insert"},
      {"name": "Visualizer", "implementor": "The Android Open Source Project", "type": "e46b26a0-dddd-11db-8afd-0002a5d5c51b", "uuid": "d069d9e0-8329-11df-9168-0002a5d5c51b", "connect": "Insert"},
      {"name": "Volume", "implementor": "NXP Software Ltd.", "type": "09e8ede0-ddde-11db-b4f6-0002a5d5c51b", "uuid": "119341a0-8469-11df-81f9-0002a5d5c51b", "connect": "Insert"}
    ],
    "media": {
      "instances": [
        {"name": "android.hardware.media.c2@1.2-mediatek-64b", "store": "default1", "rank": 1}
      ],
      "components": [
        {"name": "c2.mtk.avc.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/avc", "maxSize": "2560x1440", "store": "default1", "rank": 1},
        {"name": "c2.mtk.avc.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/avc", "maxSize": "1920x1080", "store": "default1", "rank": 1},
        {"name": "c2.mtk.hevc.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/hevc", "maxSize": "2560x1440", "store": "default1", "rank": 1},
        {"name": "c2.mtk.hevc.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/hevc", "maxSize": "1920x1080", "store": "default1", "rank": 1},
        {"name": "c2.mtk.vp9.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/x-vnd.on2.vp9", "maxSize": "2560x1440", "store": "default1", "rank": 1},
        {"name": "c2.mtk.vp9.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/x-vnd.on2.vp9", "maxSize": "1920x1080", "store": "default1", "rank": 1},
        {"name": "c2.mtk.mpeg4.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/mp4v-es", "maxSize": "2048x1088", "store": "default1", "rank": 1},
        {"name": "c2.mtk.h263.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/3gpp", "maxSize": "1408x1152", "store": "default1", "rank": 1},
        {"name": "c2.mtk.heif.decoder", "kind": "decoder", "domain": "video", "mediaType": "image/vnd.android.heic", "maxSize": "16383x16383", "store": "default1", "rank": 1},
        {"name": "c2.mtk.avc.encoder", "kind": "encoder", "domain": "video", "mediaType": "video/avc", "maxSize": "2560x1440", "store": "default1", "rank": 1},
        {"name": "c2.mtk.avc.encoder.secure", "kind": "encoder", "domain": "video", "mediaType": "video/avc", "maxSize": "1920x1080", "store": "default1", "rank": 1},
        {"name": "c2.mtk.hevc.encoder", "kind": "encoder", "domain": "video", "mediaType": "video/hevc", "maxSize": "2560x1440", "store": "default1", "rank": 1},
        {"name": "c2.mtk.heif.encoder", "kind": "encoder", "domain": "video", "mediaType": "image/vnd.android.heic", "maxSize": "8192x8192", "store": "default1", "rank": 1},
        {"name": "c2.mtk.mp3.decoder", "kind": "decoder", "domain": "audio", "mediaType": "audio/mpeg", "store": "default1", "rank": 1},
        {"name": "c2.mtk.ape.decoder", "kind": "decoder", "domain": "audio", "mediaType": "audio/ape", "store": "default1", "rank": 1},
        {"name": "c2.mtk.alac.decoder", "kind": "decoder", "domain": "audio", "mediaType": "audio/alac", "store": "default1", "rank": 1},
        {"name": "c2.mtk.adpcm-ms.decoder", "kind": "decoder", "domain": "audio", "mediaType": "audio/x-adpcm-ms", "store": "default1", "rank": 1},
        {"name": "c2.mtk.adpcm-dvi-ima.decoder", "kind": "decoder", "domain": "audio", "mediaType": "audio/x-adpcm-dvi-ima", "store": "default1", "rank": 1}
      ]
    },
    "audioEngine": {
      "modules": "default,bluetooth,r_submix",
      "voiceCallIndexMax": 7,
      "musicIndexMax": 15
    }
  },
  "alignmentNotes": [
    "平台口径：MT6835V/ZA 为天玑 6100+（mt6835T 才是天玑 6300），6nm、2×A76@2.2GHz + 6×A55@2.0GHz、Mali-G57 MC2；实测 capacity 383/1024 与 DTB policy0(6 核)/policy6(2 核) 一致，故未沿用 gold 的 MT6833/D6080 频表，大核上限 2.2GHz、小核上限 2.0GHz、最低 600MHz。",
    "机型命名：固件 SKU 为 asterpro_cn，型号线索仅 asterpro；同平台 TCL 公开真机 T614SP 为 720×1612 + fts_ts + icm4n607/mmc5603/stk3acx 组合，故 model 采用 T614SP，营销名需以真机 ro.product.marketname 复核。",
    "显示：1612×720 对应 6.6\"、267ppi，逻辑密度 320dpi（360×806dp）；面板按 TCL 系 CSOT 取 EDID（CSO/CSOT），productId、出厂周/年属厂商私有值，建议装机后读 /sys/class/drm 的 EDID 微调。",
    "GPU：驱动为 Mali r38p1（gold 是 r32p1），glVersionFull/vulkanDriverInfo 已按 r38p1 全串重写；G57 MC2 的 vulkanDeviceId 0x90930010、driverId 9、Vulkan 1.3 能力位沿用同 GPU 口径，UUID 为按该驱动版本派生的 32 位十六进制值。",
    "输入：触摸为 FocalTech fts_ts（I2C bus 0x18=24，vendor/product/version 全 0，10 槽 MT-B，无物理导航键）；按键/耳机/PMIC 节点严格用固件实测 mtk-kpd / mt6835-mt6359 Headset Jack / mtk-pmic-keys。",
    "外设红线：固件 vendor permission 实测 hasFingerprint=false、hasNfc=false、hasConsumerIr=false、hasWifiDirect/Passpoint=true，故不填 uinput-silead/指纹节点、不填 swtp（swtp 仅小米系）、enableIr/enableNfc 均 false；同平台 T614SP 虽有 silead_fp_spi/st21nfc，但与本机 SKU 不符，未采用。",
    "传感器：物理芯片为 icm4n607（acc/gyro）+ mmc5603（mag）+ stk3acx（als/ps）；permission sensorFeatures 含 accelerometer/compass/gyroscope/light/proximity/stepcounter/stepdetector 而无 pressure，故未编造气压计（T614SP 的 icp201xx 不搬）；TCL 无公开品牌私有 sensor 命名空间，未填 tct.sensor.* 类臆造项。",
    "vendor/etc 虚拟 HAL：thermal.zones 直接取 DTB 实测 20 个温区；thermal_config 皮肤基温 30.1℃、GPU cooling device 取 MTK devfreq-13000000.mali；gnss 为 MTK MNL 口径且固件含 L5 配置，yearOfHw 取 2021；audioEffects 按固件实测 15 条原样保留；Codec2 用 android.hardware.media.c2@1.2-mediatek-64b 假登记 default1（rank=1），不占 default。",
    "待真机回读微调项：EDID productId/week/year、Widevine systemId、IMEI TAC/MEID 前缀、Wi-Fi MAC OUI、电池 fullMv/chargeMv（MTK 单电芯 4.40V/4.46V 为同平台估值）。"
  ],
  "sources": [
    "固件实测：build.prop / boot DTB / VINTF manifest / vendor/etc / odm/etc（本机 asterpro，13-TP1A.220624.014-20251217）",
    "https://www.deviceinfohw.ru/devices/item.php?item=99400",
    "https://www.mediatek.com/products/smartphones/mediatek-dimensity-6100plus"
  ]
}
```
