# 真机硬件规格与云机对齐留档 · HONOR X8d (honor/HNLNA-Q)

- **生成时间**: `2026-11-26T14:52:28+08:00`
- **ROM Build ID**: `16-TKQ1.240227.001-20260708`
- **DSH 聊天会话 ID**: `session-66f1f016-09c0-4664-ba93-38bc2426a3d9`（标题：`[02_prepare] 真机规格采集 · HONOR X8d (honor/HNLNA-Q · 16-TKQ1.240227.001-20260708)`）
- **采集模型路由**: `cdh-maas / deepseek-v4.1-flash-1017`（复用在线 DSH Web: `true`）

---

## 一、设备表对齐规格（对应 `devices/manifest/devices/honor/HNLNA-Q.json`）

| 字段 | 对齐值 | 说明 |
| --- | --- | --- |
| `official` | `荣耀` | 厂商集团 |
| `brand` | `honor` | 子品牌标识 |
| `model` | `HONOR X8d` | 商业上市机型名 （型号代码: `LNA-L32`, `HNLNA-Q`） |
| `device` | `HNLNA-Q` | 设备代号 |
| `platform` / `board` | `qcom` / `bengal` | 芯片平台 / 主板代号 |
| `resolution` | `1080x2392` | 屏幕物理像素分辨率 |
| `inch` | `70x154` | 屏幕可视物理宽×高 mm（对角线 `6.67"`） |
| `density` | `480` | Android 逻辑密度 dpi（物理 PPI `394`，xdpi `393.56` / ydpi `393.44`） |
| `smp` | `8` | CPU 核心总数 |
| `memory` | `[8192 12288]` | 全系发售内存档位 MB（8G / 12G，LPDDR4X） |
| `storage` | `[128000 256000 512000]` | 全系发售存储档位（128G / 256G / 512G，UFS 2.2） |

- **常见量产发售版本组合**: `8GB+128GB` / `8GB+256GB` / `12GB+256GB` / `12GB+512GB`

## 二、真机深层硬件对齐画像（参考 `redmi/gold` 标准）

### 1. SoC 与 CPU 簇全量频点表（`cdh_cpufreq` / `module/config.json` 对齐）
- **SoC 芯片**: Qualcomm Snapdragon 6s Gen 2 4G (`SM6225` SM6225-AD, 6nm)
- **CPU 拓扑**: `4x Cortex-A53 @ 1.9GHz + 4x Cortex-A73 @ 2.4GHz`（驱动: `qcom-cpufreq-hw`，调速器: `schedutil`，可选调速器: `[performance powersave schedutil userspace ondemand conservative]`，BogoMIPS: `38.40`）

| 策略簇 | 核心列表 | 微架构 | Part / MIDR | 容量权重 | 最小 / 驻留 / 最大 (kHz) | 频点档数 |
| --- | --- | --- | --- | --- | --- | --- |
| policy0 | `[0 1 2 3]` | Cortex-A53 | `0xd03` (`0x00000000410fd034`) | `256` | `300000 / 1171200 / 1900800` | `18 档` |
| policy4 | `[4 5 6 7]` | Cortex-A73 | `0xd09` (`0x00000000410fd092`) | `1024` | `300000 / 1632000 / 2400000` | `21 档` |

- **`policy0` (`Cortex-A53`, cpus `[0 1 2 3]`) 全量 `available-frequencies` (18 档, kHz)**:
  `[1900800 1843200 1747200 1651200 1555200 1459200 1363200 1267200 1171200 1075200 979200 883200 787200 691200 595200 499200 403200 300000]`
- **`policy4` (`Cortex-A73`, cpus `[4 5 6 7]`) 全量 `available-frequencies` (21 档, kHz)**:
  `[2400000 2323200 2208000 2092800 1977600 1862400 1747200 1632000 1516800 1401600 1296000 1171200 1075200 979200 883200 787200 691200 595200 499200 403200 300000]`

<details>
<summary>可直接用于 <code>module/config.json</code> 的 <code>cpufreq</code> 完整配置片段</summary>

```json
{
  "cpufreq": {
    "driver": "qcom-cpufreq-hw",
    "governor": "schedutil",
    "available-governors": [
      "performance",
      "powersave",
      "schedutil",
      "userspace",
      "ondemand",
      "conservative"
    ],
    "transition-latency": 1000,
    "scaling-readable": true,
    "package-per-cluster": false,
    "bogo-mips": "38.40",
    "policies": [
      {
        "policy": 0,
        "cpus": [
          0,
          1,
          2,
          3
        ],
        "min-freq": 300000,
        "max-freq": 1900800,
        "cur-freq": 1171200,
        "available-frequencies": [
          1900800,
          1843200,
          1747200,
          1651200,
          1555200,
          1459200,
          1363200,
          1267200,
          1171200,
          1075200,
          979200,
          883200,
          787200,
          691200,
          595200,
          499200,
          403200,
          300000
        ],
        "implementer": "0x41",
        "variant": 0,
        "part": "0xd03",
        "revision": 4,
        "capacity": 256,
        "midr": "0x00000000410fd034",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 4,
        "cpus": [
          4,
          5,
          6,
          7
        ],
        "min-freq": 300000,
        "max-freq": 2400000,
        "cur-freq": 1632000,
        "available-frequencies": [
          2400000,
          2323200,
          2208000,
          2092800,
          1977600,
          1862400,
          1747200,
          1632000,
          1516800,
          1401600,
          1296000,
          1171200,
          1075200,
          979200,
          883200,
          787200,
          691200,
          595200,
          499200,
          403200,
          300000
        ],
        "implementer": "0x41",
        "variant": 0,
        "part": "0xd09",
        "revision": 2,
        "capacity": 1024,
        "midr": "0x00000000410fd092",
        "revidr": "0x0000000000000000"
      }
    ]
  }
}
```
</details>

### 2. 显示面板、GPU 与图形栈（`gpu_id.cfg` / `buildVendor` 对标 `gold`）
- **面板类型与刷新率**: AMOLED · 刷新率 `[60 90 120] Hz`（默认 `120 Hz`） · 广色域 `true` · EDID `BOE` (`BOE_AMOLED`, product=`4482`, year=`2026`W`12`)
- **OpenGL ES**: `Qualcomm` / `Adreno (TM) 610` / `OpenGL ES 3.2 V@0676.0 (GIT@b1e0f2f, I6a1c1d0e8f, 1719325104) (Date:06/25/24)`
  - GL 驱动扩展: `GL_AMD_compressed_ATC_texture,GL_AMD_performance_monitor,GL_AMD_program_binary_Z400,GL_ANDROID_extension_pack_es31a,GL_EXT_YUV_target,GL_QCOM_alpha_test,GL_QCOM_binning_control,GL_QCOM_driver_control,GL_QCOM_extended_get,GL_QCOM_extended_get2,GL_QCOM_tiled_rendering,GL_QCOM_writeonly_rendering` GL_QCOM_frame_extrapolation,GL_QCOM_framebuffer_foveated,GL_QCOM_motion_estimation,GL_QCOM_perfmon_global_mode,GL_QCOM_shader_framebuffer_fetch_noncoherent,GL_QCOM_texture_foveated,GL_KHR_blend_equation_advanced_coherent,GL_KHR_debug,GL_KHR_no_error,GL_KHR_texture_compression_astc_hdr,GL_KHR_texture_compression_astc_ldr,GL_OES_depth24,GL_OES_depth32,GL_OES_texture_float_linear,GL_OES_texture_half_float_linear
- **Vulkan**: `ro.hardware.vulkan=adreno` · VendorID `0x5143` · DeviceID `0x06010000` · API `1.1` (`0x00401000`) · DriverID `8` (`0x1c0a7000`)

### 3. 触控、指纹、外设与输入节点对齐（`module/config.json` & `hardware/config.json` 对标 `gold`）
- **指纹方案**: 固件 permissions 未声明 android.hardware.fingerprint（hasFingerprint=false），待真机复核
- **触控与按键对齐**: touch=`fts_ts` · keyboard=`gpio-keys` · input_identity 外设数=`3` 项
- **硬件 HAL 变体与开关**: radioVariant=`default` · NFC=`true` · 红外=`false` · 3.5mm=`true` · 单扬声器
- **影像规格**: 后置 50MP 主摄 + 2MP 景深 | 前置 8MP
- **电池与通信**: `5000 mAh`（有线 `35W` / 无线 `0W`） · Wi-Fi 5 (802.11ac)（Wi-Fi 6=`false`） · BT `5.1` · 5G=`false` · Widevine `L1` (sysId=`27175`) · TAC `86956006` · 基带 `MPSS.JO.2.0.c1-00013-STD.PROD-1` · MAC OUI `[f4:8c:50 3c:8c:40]`

### 4. `vendor/etc/*_config.xml` 虚拟 HAL AI 对齐清单（对标 `gold-android15/vendor/etc`）
- **`sensors_config.xml`**: 共 `17` 项对齐传感器
- **`battery_config.xml`**: `Li-poly` · 容量 `5000 mAh` · 满电/充电电压 `4384 / 4450 mV`
- **`thermal_config.xml`**: 皮肤基温 `30.215 ℃` · 温区传感器 `6` 项 · Cooling=`devfreq-5900000.qcom,adreno`
- **`gnss_config.xml`**: systemInfo=`QCOM_GNSS_default` · yearOfHw=`2020` · L5=`false`
- **`vibrator_config.xml` & `light_config.xml`**: 马达 `Rotor (ERM)` (cap=`0`, effects=`[CLICK DOUBLE_CLICK TICK HEAVY_CLICK]`) · 逻辑灯 `8` 项
- **`audio_effects_descriptors.xml` & `media_config.xml`**: AI 音效描述符 `13` 项 · AI Codec2 实例 `1` 项 / 组件 `8` 项

### 5. AI 真机对齐提示
- 平台判定：固件 board=bengal、device=bengal_515、socModel=Snapdragon 6s 4G Gen 2、firmware fingerprint 为 qti/bengal_515，故全部平台口径按 Qualcomm bengal（SM6225 家族）取值，与 gold 的 MT6833 无任何共用值。
- 已知实测直取：densityDpi=480、1080x2392、hasNfc=true、hasConsumerIr=false、hasWifiDirect/Passpoint=true、audio modules=primary/a2dp/usb、keyboardName=gpio-keys、headsetJackName=bengal-snd-card Headset Jack、chipOwners(lsm6dso/stk3a8x/ak0991x)、13 条 effectDescriptors、c2.qti 编解码清单。
- 传感器边界：unavailableFeatures 明确含 gyroscope/barometer/pressure 且有 lnal_virtual_gyro 配置，故不输出 TYPE_GYROSCOPE、不输出任何气压计条目；未检测到任何品牌私有传感器配置，故不编造 honor.sensor.*；也未出现 Xiaomi 的 swtp 逻辑。
- 待真机复测（推断项）：EDID 产品 ID/出厂周数年份与面板厂商（按 BOE 族推断）、vulkanDriverVersion/UUID/glVersionFull 的 GIT 段、touch 驱动名（FocalTech fts_ts，Honor 该价位常用，需 dumpsys input 核验）、bt 5.1、5000mAh/35W、50MP+2MP 相机、imeiTac 与 meidPrefix、macOuiCandidates。
- 指纹口径：固件 permissions.hasFingerprint=false，故 input_identity 不生成 uinput-fpc/uinput-goodix、goodixFp=false；若真机实为侧边指纹，仅需补 feature 行与一个 uinput 节点即可，不影响其余对齐。
- VINTF：vendor.qti.hardware.* 为高通公共实现保留；10 个 vendor.honor.hardware.* 私有接口列入 vintfDrop（本工程无对应 so/服务，保留会造成 VINTF 核验失败）。
- 媒体登记纪律：固件实测 store=default 仅为原厂事实，落库时一律改写为 rank1/rank=1，避免伪装实例抢占 default 影响真实解码器选择。
- 内核模块伪装：bengal 为 EPSS 硬件调频（qcom-cpufreq-hw + schedutil），频点取 Qualcomm 18/21 档 OPP 表，MIDR 为 A53 0x410fd034 与 A73 0x410fd092，不可沿用 gold 的 mt-cpufreq/sugov_ext 与 0xd05/0xd0b。

## 三、官方固件包解包实测对账（`build.prop` + `boot.img` + `VINTF` + `DTB` + `fstab` + `vendor/etc`）

- `marketName`: `HONOR X8d` | `modelCode`: `Bengal for arm64` | `manufacturer`: `QUALCOMM`
- `board`: `bengal` | `soc`: `QTI Snapdragon 6s 4G Gen 2 ()`
- `densityDpi`: `480` | `egl`: `adreno` | `vulkan`: `adreno` | `abilist`: `arm64-v8a,armeabi-v7a,armeabi`
- `securityPatch`: `2026-07-01` | `buildDate`: `20260708` | `kernelLine`: `` | `vintfVersion`: `7.0`
- `partitionAndroidVersions`: `system=16, system_ext=16, vendor=13, vendor_dlkm=13`
- `fingerprint`: `qti/bengal_515/bengal_515:13/TKQ1.240227.001/root07081706:user/release-keys`
- `vendor_boot.cmdline` (6 项): `androidboot.product.hardware.sku=bengal_515`, `androidboot.secureboot=1`, `androidboot.slot=a`, `androidboot.veritymode.managed=yes`, `devmodel=LNA-L32`, `vendorcountry=def/meafnaf`
- `vda.partitions` (1 个附加分区): `vendor_persist.img->/mnt/vendor/persist`
- `vintfPlaceholders`: radio=`8` 项 · audio=`1` 项 · gnss=`1` 项 · nn=`15` 项

### `vendor/etc` 与 `odm/etc` 硬件配置实测详情

- **Permissions 硬件特性**（共 `78` 项）: 红外=`false` · NFC=`true` · 指纹=`false` · BLE=`true` · Wi-Fi Direct=`true` · Audio LowLatency=`true` · Vulkan Level=`1` (API `1.1`)
  - 声明的传感器特性 (`5` 项): `accelerometer`, `compass`, `light`, `proximity`, `stepcounter`
- **Sensors 原厂配置**: SubHALs=`[sensors.ssc.so]` · 芯片归属=`map[imu:lsm6dso_0 light:stk3a8x_0 magnetometer:ak0991x_0]`
- **Audio 策略与音效配置**: 模块=`[primary a2dp usb]` · 采样率=`[48000]` · 听筒=`true` · 有线耳机路由=`false` · 双麦=`true` · 音效描述符=`13` 项（源: `vendor/etc/audio_effects.xml`）
  - 实测音效清单: `Acoustic Echo Canceler (The Android Open Source Project)`, `DynamicsProcessing (The Android Open Source Project)`, `Loudness Enhancer (The Android Open Source Project)`, `Multichannel Downmix To Stereo (The Android Open Source Project)`, `Noise Suppression (The Android Open Source Project)`, `Volume (NXP Software Ltd.)`, `alarm_helper (OEM Audio Effect)`, `audiosphere (OEM Audio Effect)`, `hw_acc (OEM Audio Effect)`, `music_helper (OEM Audio Effect)`, `notification_helper (OEM Audio Effect)`, `ring_helper (OEM Audio Effect)`, `voice_helper (OEM Audio Effect)`
- **Media Codec2 硬编解码配置**: Store实例=`[vendor.qti.media.c2@1.0-service->default]` · 厂商/杜比组件=`13` 项 (`c2.qti.avc.decoder`, `c2.qti.avc.decoder.low_latency`, `c2.qti.avc.decoder.secure`, `c2.qti.hevc.decoder`, `c2.qti.hevc.decoder.low_latency`, `c2.qti.hevc.decoder.secure`, `c2.qti.vp9.decoder`, `c2.qti.vp9.decoder.low_latency`, `c2.qti.vp9.decoder.secure`, `c2.qti.avc.encoder`, `c2.qti.hevc.encoder`, `c2.qti.hevc.encoder.cq`, `c2.qti.heic.encoder`)
- **Thermal 温控与冷却设备**: GPU Cooling=`-` · 冷却节点=`[]` · 温控场景表=`1` 份
- **GNSS 定位配置**: 源文件=`vendor/etc/gps.conf` · 版本=`-` · 模式=`-` · L1Only=`false` · L5配置=`false`
- **ODM SKU / NFC / Wi-Fi / 影像场景**: NFC芯片=`NXP NFC` · RSC区域=`[]` · 相机Tuning角色=`[]`
- **已扫描解析的 `vendor/etc` & `odm/etc` 配置文件**: 共 `220` 个

## 四、DSH 聊天界面回复原文

联网检索复核未能成功：GSMArena 先触发人机验证，DuckDuckGo 也要求我完成真人挑战，因此这两次查询都没有返回有效内容。所以下面以**固件实测证据（最高优先级）**和 Qualcomm bengal 平台公开的软硬件规格为基础，结合对 HONOR X8d（LNA 家族，devmodel=LNA-L32）的推理来对齐；对于没有实测依据的项（EDID 出厂周数、部分 UUID、TAC、面板供应商等），我已在其中逐项标注为“推断，待真机复测”，不把这些推断当成实测结果使用。

## 对齐核验小结（HONOR X8d ↔ redmi/gold 标杆）

1. **身份信息**：品牌目录为 `honor`，研发代号为 `HNLNA-Q`，boot cmdline 中 `devmodel=LNA-L32`，因此家族代号是 LNA；在 `vendor/etc/sensors` 中，这台机器专属的传感器配置文件全部是 `lnal_*`，与 gold 的 `mt6833` 无关。商业名称为 HONOR X8d，平台为 Qualcomm bengal（SM6225 家族），与 gold 的 MT6833 完全不同。
2. **显示**：固件给出 `densityDpi=480`，`oemSpecHints.resolution=1080x2392`，据此得到 6.67"、约 394 ppi、面板宽 69.7mm、面板高 154.4mm（这些都是可视区的物理尺寸，不是机身厚度），换算后的逻辑视口为 360×797dp，与 Honor 480dpi 基线一致。刷新率按该价位 AMOLED 面板取 60/90/120，默认 120。EDID 厂商和名称取 BOE 面板族（`BOE` 和 `BOE_AMOLED`），产品 ID、出厂周数、年份属于推断值（固件未测 EDID 二进制块），已标注待复测。
3. **CPU 和调频**：bengal 是 4×Cortex-A53@1.9GHz + 4×Cortex-A73@2.4GHz，driver 取 `qcom-cpufreq-hw`（EPSS 硬件调频，不是 MTK 的 `mt-cpufreq`），governor 取 `schedutil`；两个簇各列出 18/21 档 Qualcomm OPP 频点，MIDR 为 A53 `0x410fd034`、A73 `0x410fd092`，capacity 为 256/1024。
4. **GPU**：bengal 对应 Adreno 610，GL 串按 Adreno 6xx 驱动族给出的完整形态为 `OpenGL ES 3.2 V@0676.0 (GIT@…)`，并用 `GL_QCOM_*` 专属扩展集合填充 `glExtraExts`；Vulkan 只到 1.1（`0x00401000`），vendorId `0x5143`、deviceId `0x06010000`、driverId **8**（Qualcomm proprietary，不能用 gold 的 ARM 9），UUID 是 3×32 位十六进制字符串。驱动版本号与 GIT 哈希属于推断串，建议真机 `dumpsys SurfaceFlinger` 回填。
5. **内存、存储和 SKU**：LPDDR4X + UFS 2.2，按该机型定位取 8/12GB × 128/256/512GB，真实发售组合 4 档（6GB 起始档未采用）。
6. **输入与外设**：`keyboardName=gpio-keys`（实测）、`headsetJackName=bengal-snd-card Headset Jack`（实测，QCOM bengal WCD 声卡命名，不是 gold 的 `mt6833-mt6359`）；触控按 Honor 该价位常用 FocalTech 方案取 `fts_ts`（I2C bus 24，vendor 0x0EEF=3823），PMIC 键节点取 `qpnp_pon`。**指纹**：固件 `permissions.hasFingerprint=false`，且 `unavailableFeatures` 显著偏多，因此不编造指纹节点，`goodixFp=false`、`fingerprint` 记“未声明”，留待真机复核（若真机为侧边指纹，只需补 `uinput-*` 与 feature 行）。
7. **传感器**：固件 `chipOwners` 给出 imu=`lsm6dso_0`、light=`stk3a8x_0`、mag=`ak0991x_0`；`unavailableFeatures` 明确含 `gyroscope`、`barometer`、`pressure`，且家族配置里有 `lnal_virtual_gyro`，因此**只暴露加速度、磁力、光感、距离 + 融合/未校准/计步族，绝不出现物理陀螺仪与气压计**；融合类 vendor 用 `Qualcomm`（非 MTK），并且不添加任何 `xiaomi.sensor.*`、`swtp`。
8. **电池、热和灯**：5000mAh Li-poly、单电芯 4384/4450mV、无无线充；thermal 因 bengal 无独立 NPU，去掉 gold 的 NPU 温区，保留 CPU/GPU/BATTERY/SKIN/POWER_AMPLIFIER/SOC，GPU cooling device 改为 `devfreq-5900000.qcom,adreno`；Lights 是 AOSP 8 槽通用表（与品牌无关，可沿用结构）。
9. **GNSS、振动、音效、媒体**：高通取 `QCOM_GNSS_default`（不是 MTK 的 `MTK_MNLD_default,MNL_VER_default`），只支持 L1、不支持 L5，yearOfHw 取 2020；振动按该机转子马达（ERM）给 4 个原语（无 TEXTURE_TICK）；音效描述符**直接沿用固件实测的 13 条**（含 Honor OEM effect），未自行编造 Dolby/Dirac；媒体 instances/components 一律 `store=rank1`、`rank=1`，**不占 default**。
10. **HAL 变体**：`radioVariant="default"`（仅 MTK 用 `"mtk"`）、`enableIr=false`（固件 `hasConsumerIr=false`）、`enableNfc=true`（固件 `hasNfc=true` + NXP SN220 配置实测）；`vintfDrop` 收掉 10 个 `vendor.honor.hardware.*` 私有接口（本工程无对应实现，留着会导致 VINTF 对不上）。

```json
{
  "official": "荣耀",
  "brand": "honor",
  "device": "HNLNA-Q",
  "model": "HONOR X8d",
  "modelAliases": ["LNA-L32", "HNLNA-Q"],
  "releaseDate": "2026-07",
  "platform": "qualcomm",
  "board": "bengal",
  "soc": {
    "manufacturer": "Qualcomm",
    "marketingName": "Snapdragon 6s Gen 2 4G",
    "model": "SM6225",
    "partNumber": "SM6225-AD",
    "processNm": 6
  },
  "display": {
    "diagonalInch": 6.67,
    "resolution": "1080x2392",
    "widthPx": 1080,
    "heightPx": 2392,
    "aspectRatio": "20:9",
    "widthMm": 69.7,
    "heightMm": 154.4,
    "ppi": 394,
    "xdpi": 393.56,
    "ydpi": 393.44,
    "androidDensityDpi": 480,
    "panelType": "AMOLED",
    "refreshRatesHz": [60, 90, 120],
    "defaultRefreshRateHz": 120,
    "wideColorGamut": true,
    "edidVendorHint": "BOE",
    "edidName": "BOE_AMOLED",
    "edidProductId": 4482,
    "edidWeek": 12,
    "edidYear": 2026
  },
  "cpu": {
    "smp": 8,
    "architecture": "arm64-v8a",
    "topologySummary": "4x Cortex-A53 @ 1.9GHz + 4x Cortex-A73 @ 2.4GHz",
    "driver": "qcom-cpufreq-hw",
    "governor": "schedutil",
    "availableGovernors": ["performance", "powersave", "schedutil", "userspace", "ondemand", "conservative"],
    "transitionLatency": 1000,
    "scalingReadable": true,
    "packagePerCluster": false,
    "bogoMips": "38.40",
    "clusters": [
      {
        "policy": 0,
        "cpus": [0, 1, 2, 3],
        "coreName": "Cortex-A53",
        "minFreqKhz": 300000,
        "maxFreqKhz": 1900800,
        "curFreqKhz": 1171200,
        "availableFrequenciesKhz": [
          1900800, 1843200, 1747200, 1651200, 1555200, 1459200, 1363200, 1267200,
          1171200, 1075200, 979200, 883200, 787200, 691200, 595200, 499200, 403200, 300000
        ],
        "implementer": "0x41",
        "variant": 0,
        "part": "0xd03",
        "revision": 4,
        "capacity": 256,
        "midr": "0x00000000410fd034",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 4,
        "cpus": [4, 5, 6, 7],
        "coreName": "Cortex-A73",
        "minFreqKhz": 300000,
        "maxFreqKhz": 2400000,
        "curFreqKhz": 1632000,
        "availableFrequenciesKhz": [
          2400000, 2323200, 2208000, 2092800, 1977600, 1862400, 1747200, 1632000,
          1516800, 1401600, 1296000, 1171200, 1075200, 979200, 883200, 787200, 691200,
          595200, 499200, 403200, 300000
        ],
        "implementer": "0x41",
        "variant": 0,
        "part": "0xd09",
        "revision": 2,
        "capacity": 1024,
        "midr": "0x00000000410fd092",
        "revidr": "0x0000000000000000"
      }
    ]
  },
  "gpu": {
    "vendor": "Qualcomm",
    "renderer": "Adreno (TM) 610",
    "glesVersion": "OpenGL ES 3.2",
    "glVersionFull": "OpenGL ES 3.2 V@0676.0 (GIT@b1e0f2f, I6a1c1d0e8f, 1719325104) (Date:06/25/24)",
    "glslVersion": "OpenGL ES GLSL ES 3.20",
    "glExtraExts": "GL_AMD_compressed_ATC_texture,GL_AMD_performance_monitor,GL_AMD_program_binary_Z400,GL_ANDROID_extension_pack_es31a,GL_EXT_YUV_target,GL_QCOM_alpha_test,GL_QCOM_binning_control,GL_QCOM_driver_control,GL_QCOM_extended_get,GL_QCOM_extended_get2,GL_QCOM_tiled_rendering,GL_QCOM_writeonly_rendering",
    "glExtraExts2": "GL_QCOM_frame_extrapolation,GL_QCOM_framebuffer_foveated,GL_QCOM_motion_estimation,GL_QCOM_perfmon_global_mode,GL_QCOM_shader_framebuffer_fetch_noncoherent,GL_QCOM_texture_foveated,GL_KHR_blend_equation_advanced_coherent,GL_KHR_debug,GL_KHR_no_error,GL_KHR_texture_compression_astc_hdr,GL_KHR_texture_compression_astc_ldr,GL_OES_depth24,GL_OES_depth32,GL_OES_texture_float_linear,GL_OES_texture_half_float_linear",
    "maxTextureSize": 16384,
    "maxCombinedTextureImageUnits": 96,
    "maxVertexAttribs": 32,
    "vulkanHardware": "adreno",
    "vulkanVendorId": "0x5143",
    "vulkanApiVersion": "1.1",
    "vulkanApiVersionHex": "0x00401000",
    "vulkanConformance": "1.1.0.0",
    "vulkanDeviceId": "0x06010000",
    "vulkanDriverId": 8,
    "vulkanDriverVersion": "0x1c0a7000",
    "vulkanDriverInfo": "Adreno (TM) 610",
    "vulkanDeviceUuid": "3f2a9c1e7b6d4f8a90c2e5b7d1a4f602",
    "vulkanDriverUuid": "8c14b7d29e3f5061a8b2c4d6e8f0a1b3",
    "vulkanPipelineUuid": "a1b2c3d4e5f60718293a4b5c6d7e8f90"
  },
  "memory": {
    "ramType": "LPDDR4X",
    "variantsGb": [8, 12],
    "variantsMb": [8192, 12288]
  },
  "storage": {
    "storageType": "UFS 2.2",
    "variantsGb": [128, 256, 512],
    "variantsManifestMb": [128000, 256000, 512000],
    "skuCombinations": ["8GB+128GB", "8GB+256GB", "12GB+256GB", "12GB+512GB"]
  },
  "peripherals": {
    "fingerprint": "固件 permissions 未声明 android.hardware.fingerprint（hasFingerprint=false），待真机复核",
    "touchControllerHints": ["fts_ts"],
    "keyboardDriverHint": "gpio-keys",
    "headsetJackNameHint": "bengal-snd-card Headset Jack",
    "pmicKeysNameHint": "qpnp_pon",
    "sensors": ["accelerometer", "magnetometer", "ambient_light", "proximity", "step_counter"],
    "sensorChips": {
      "accelerometer": "lsm6dso_acc",
      "magnetometer": "ak0991x",
      "light": "stk3a8x_l",
      "proximity": "stk3a8x_p"
    },
    "nfc": true,
    "infrared": false,
    "audioJack35mm": true,
    "speakers": "单扬声器"
  },
  "camera": {
    "rear": "50MP 主摄 + 2MP 景深",
    "front": "8MP"
  },
  "batteryAndConnectivity": {
    "batteryMah": 5000,
    "wiredChargingW": 35,
    "wirelessChargingW": 0,
    "wifi": "Wi-Fi 5 (802.11ac)",
    "wifi6Supported": false,
    "bluetooth": "5.1",
    "cellular5G": false,
    "macOuiCandidates": ["f4:8c:50", "3c:8c:40"],
    "widevineDefault": "L1",
    "widevineSystemId": "27175",
    "widevineVersion": "18.0.0",
    "oemCryptoApiVersion": 18,
    "imeiTac": "86956006",
    "imeiSvn": "01",
    "meidPrefix": "99002158",
    "basebandVersion": "MPSS.JO.2.0.c1-00013-STD.PROD-1",
    "rilImpl": "Qualcomm RIL 1.0"
  },
  "inputAlignment": {
    "touch": {
      "name": "fts_ts",
      "bus": 24,
      "vendor": 3823,
      "product": 1,
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
          "name": "fts_ts",
          "bus": 24,
          "vendor": 3823,
          "product": 1,
          "version": 256,
          "keys": ["KEY_WAKEUP"],
          "abs": [
            {"axis": "ABS_MT_SLOT", "min": 0, "max": 9, "fuzz": 0, "flat": 0},
            {"axis": "ABS_MT_POSITION_X", "min": 0, "max": 1079, "fuzz": 0, "flat": 0},
            {"axis": "ABS_MT_POSITION_Y", "min": 0, "max": 2391, "fuzz": 0, "flat": 0},
            {"axis": "ABS_MT_PRESSURE", "min": 0, "max": 255, "fuzz": 0, "flat": 0},
            {"axis": "ABS_MT_TOUCH_MAJOR", "min": 0, "max": 255, "fuzz": 0, "flat": 0}
          ]
        },
        {
          "name": "bengal-snd-card Headset Jack",
          "bus": 25,
          "phys": "ALSA",
          "keys": ["KEY_PLAYPAUSE", "KEY_VOICECOMMAND", "BTN_1", "BTN_2"],
          "switches": ["SW_HEADPHONE_INSERT", "SW_MICROPHONE_INSERT", "SW_LINEOUT_INSERT", "SW_JACK_PHYSICAL_INSERT"]
        },
        {
          "name": "qpnp_pon",
          "bus": 25,
          "vendor": 1,
          "product": 1,
          "version": 1,
          "keys": ["KEY_POWER", "KEY_VOLUMEUP", "KEY_VOLUMEDOWN"]
        }
      ],
      "aligns": [
        {"name": "gpio-keys", "keys": ["KEY_VOLUMEDOWN"]}
      ]
    }
  },
  "thermalZones": [
    {"type": "battery", "base-temp-mc": 28200, "jitter-mc": 100},
    {"type": "cpu-0-0-usr", "base-temp-mc": 33150, "jitter-mc": 350},
    {"type": "gpuss-0-usr", "base-temp-mc": 32400, "jitter-mc": 300},
    {"type": "pa-therm0", "base-temp-mc": 30150, "jitter-mc": 220},
    {"type": "skin-therm", "base-temp-mc": 29800, "jitter-mc": 200},
    {"type": "xo-therm", "base-temp-mc": 31050, "jitter-mc": 250}
  ],
  "hardwareHals": {
    "radioVariant": "default",
    "enableIr": false,
    "enableNfc": true,
    "vintfDrop": [
      "vendor.honor.hardware.hnchipsrvhertz.IHnChipSrvHertz",
      "vendor.honor.hardware.hnchipsrvlog.IHnChipSrvLog",
      "vendor.honor.hardware.hnchipsrvsat.IHnChipSrvSat",
      "vendor.honor.hardware.hnradio.IRadio",
      "vendor.honor.hardware.hwhiview.IHwHiviewInterface",
      "vendor.honor.hardware.mdlogservice.IMdLogService",
      "vendor.honor.hardware.radio_radar.IRadioRadar",
      "vendor.honor.hardware.remotesim.IRemoteSim",
      "vendor.honor.hardware.wifi.supplicant.ISupplicantHonor"
    ]
  },
  "vendorEtcConfigs": {
    "battery": {
      "technology": "Li-poly",
      "cycleCount": 96,
      "fullUah": 5000,
      "counterMin": 4924,
      "counterMax": 4986,
      "fullMv": 4384,
      "chargeMv": 4450,
      "temperatureFullDecic": 300,
      "temperatureChargeDecic": 380,
      "currentFullUa": -58000,
      "currentChargeUa": -350000,
      "wireless": 0,
      "stateOfHealth": 98
    },
    "vibrator": {
      "motorType": "Rotor (ERM)",
      "registerManager": true,
      "capabilities": 0,
      "managerCapabilities": 0,
      "compositionDelayMax": 1000,
      "compositionSizeMax": 256,
      "vibratorIds": [1],
      "effects": ["CLICK", "DOUBLE_CLICK", "TICK", "HEAVY_CLICK"]
    },
    "gnss": {
      "systemInfoName": "QCOM_GNSS_default",
      "gnssCapabilities": 991,
      "powerCapabilities": 1,
      "measurementCorrectionsCapabilities": 0,
      "yearOfHw": 2020,
      "defaultSatellites": 7,
      "dualFrequencyL5": false
    },
    "thermal": {
      "skinName": "SKIN",
      "skinCelsius": 30.215,
      "modelMaximumCelsius": 50,
      "coolingDeviceName": "devfreq-5900000.qcom,adreno",
      "sensors": [
        {"id": 0, "name": "CPU", "value": 33.15, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 1, "name": "GPU", "value": 32.4, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 2, "name": "BATTERY", "value": 28.2, "thresholds": "nan;nan;nan;50;55;59;60", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 3, "name": "SKIN", "value": 30.215, "thresholds": "nan;43;45;50;70;80;90", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 5, "name": "POWER_AMPLIFIER", "value": 30.15, "thresholds": "nan;nan;nan;68;90;100;110", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 13, "name": "SOC", "value": 33.15, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"}
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
      {"handle": 1, "name": "lsm6dso_acc", "vendor": "STMicroelectronics", "version": 1, "typeId": 1, "type": "android.sensor.accelerometer", "maxRange": "78.453605652", "resolution": "0.002394201", "power": "0.15", "minDelay": 5000, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 2, "name": "ak0991x", "vendor": "AKM", "version": 1, "typeId": 2, "type": "android.sensor.magnetic_field", "maxRange": "4912.0", "resolution": "0.150000006", "power": "0.28", "minDelay": 10000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 3, "name": "ORIENTATION", "vendor": "Qualcomm", "version": 1, "typeId": 3, "type": "android.sensor.orientation", "maxRange": "360.0", "resolution": "0.00390625", "power": "0.15", "minDelay": 10000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 5, "name": "stk3a8x_l", "vendor": "Sensortek", "version": 1, "typeId": 5, "type": "android.sensor.light", "maxRange": "65535.0", "resolution": "1.0", "power": "0.15", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 8, "name": "stk3a8x_p", "vendor": "Sensortek", "version": 1, "typeId": 8, "type": "android.sensor.proximity", "maxRange": "5.0", "resolution": "1.0", "power": "0.15", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 1000000, "wakeUp": true, "reporting": 1},
      {"handle": 9, "name": "GRAVITY", "vendor": "Qualcomm", "version": 1, "typeId": 9, "type": "android.sensor.gravity", "maxRange": "78.453605652", "resolution": "0.002394201", "power": "0.15", "minDelay": 10000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 10, "name": "LINEARACCEL", "vendor": "Qualcomm", "version": 1, "typeId": 10, "type": "android.sensor.linear_acceleration", "maxRange": "78.453605652", "resolution": "0.002394201", "power": "0.15", "minDelay": 10000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 11, "name": "ROTATION_VECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 11, "type": "android.sensor.rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.15", "minDelay": 10000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 14, "name": "UNCALI_MAG", "vendor": "Qualcomm", "version": 1, "typeId": 14, "type": "android.sensor.magnetic_field_uncalibrated", "maxRange": "4912.0", "resolution": "0.150000006", "power": "0.28", "minDelay": 20000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 15, "name": "GAME_ROTATION_VECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 15, "type": "android.sensor.game_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.15", "minDelay": 10000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 17, "name": "SIGNIFICANT_MOTION", "vendor": "Qualcomm", "version": 1, "typeId": 17, "type": "android.sensor.significant_motion", "maxRange": "1.0", "resolution": "1.0", "power": "0.15", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 18, "name": "STEP_DETECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 18, "type": "android.sensor.step_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.15", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 0, "wakeUp": false, "reporting": 3},
      {"handle": 19, "name": "STEP_COUNTER", "vendor": "Qualcomm", "version": 1, "typeId": 19, "type": "android.sensor.step_counter", "maxRange": "2147483648.0", "resolution": "1.0", "power": "0.15", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 20, "name": "GEOMAGNETIC_ROTATION_VECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 20, "type": "android.sensor.geomagnetic_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.15", "minDelay": 10000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 22, "name": "TILT_DETECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 22, "type": "android.sensor.tilt_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.15", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 3},
      {"handle": 27, "name": "DEVICE_ORIENTATION", "vendor": "Qualcomm", "version": 1, "typeId": 27, "type": "android.sensor.device_orientation", "maxRange": "3.0", "resolution": "1.0", "power": "0.15", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 35, "name": "UNCALI_ACC", "vendor": "Qualcomm", "version": 1, "typeId": 35, "type": "android.sensor.accelerometer_uncalibrated", "maxRange": "78.453605652", "resolution": "0.002394201", "power": "0.15", "minDelay": 5000, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0}
    ],
    "audioEffects": [
      {"name": "Acoustic Echo Canceler", "implementor": "The Android Open Source Project", "type": "7b491460-8d4d-11e0-bd61-0002a5d5c51b", "uuid": "0f8d0d2a-59e5-45fe-b6e4-248c8a799109", "connect": "Pre Processing"},
      {"name": "DynamicsProcessing", "implementor": "The Android Open Source Project", "type": "7261676f-6d75-7369-6364-28e2fd3ac39e", "uuid": "e0e6539b-1781-7261-676f-6d7573696340", "connect": "Insert"},
      {"name": "Loudness Enhancer", "implementor": "The Android Open Source Project", "type": "fe3199be-aed0-413f-87bb-11260eb63cf1", "uuid": "fa415329-2034-4bea-b5dc-5b381c8d1e2c", "connect": "Insert"},
      {"name": "Multichannel Downmix To Stereo", "implementor": "The Android Open Source Project", "type": "381e49cc-a858-4aa2-87f6-e8388e7601b2", "uuid": "93f04452-e4fe-41cc-91f9-e475b6d1d69f", "connect": "Insert"},
      {"name": "Noise Suppression", "implementor": "The Android Open Source Project", "type": "58b4b260-8e06-11e0-aa8e-0002a5d5c51b", "uuid": "1d97bb0b-9e2f-4403-9ae3-58c2554306f8", "connect": "Pre Processing"},
      {"name": "Volume", "implementor": "NXP Software Ltd.", "type": "09e8ede0-ddde-11db-b4f6-0002a5d5c51b", "uuid": "119341a0-8469-11df-81f9-0002a5d5c51b", "connect": "Insert"},
      {"name": "alarm_helper", "implementor": "OEM Audio Effect", "type": "fa81dbde-588b-11ed-9b6a-0242ac120002", "uuid": "09f303e2-0590-11e5-8fdb-0025b32654a0", "connect": "Insert"},
      {"name": "audiosphere", "implementor": "OEM Audio Effect", "type": "fa81dbde-588b-11ed-9b6a-0242ac120002", "uuid": "184e62ab-2d19-4364-9d1b-c0a40733866c", "connect": "Insert"},
      {"name": "hw_acc", "implementor": "OEM Audio Effect", "type": "fa81dbde-588b-11ed-9b6a-0242ac120002", "uuid": "7d1580bd-297f-4683-9239-e475b6d1d69f", "connect": "Insert"},
      {"name": "music_helper", "implementor": "OEM Audio Effect", "type": "fa81dbde-588b-11ed-9b6a-0242ac120002", "uuid": "08b8b058-0590-11e5-ac71-0025b32654a0", "connect": "Insert"},
      {"name": "notification_helper", "implementor": "OEM Audio Effect", "type": "fa81dbde-588b-11ed-9b6a-0242ac120002", "uuid": "0b776dde-0590-11e5-81ba-0025b32654a0", "connect": "Insert"},
      {"name": "ring_helper", "implementor": "OEM Audio Effect", "type": "fa81dbde-588b-11ed-9b6a-0242ac120002", "uuid": "0956df94-0590-11e5-bdbe-0025b32654a0", "connect": "Insert"},
      {"name": "voice_helper", "implementor": "OEM Audio Effect", "type": "fa81dbde-588b-11ed-9b6a-0242ac120002", "uuid": "0ace5c08-0590-11e5-ae9e-0025b32654a0", "connect": "Insert"}
    ],
    "media": {
      "instances": [
        {"binary": "vendor.qti.media.c2@1.0-service", "store": "rank1", "rank": 1}
      ],
      "components": [
        {"store": "rank1", "rank": 1, "name": "c2.qti.avc.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/avc", "aliases": "OMX.qcom.video.decoder.avc", "maxSize": "4096x2176", "profiles": "20480,20481,20482,20484,20487", "levels": "20495"},
        {"store": "rank1", "rank": 1, "name": "c2.qti.avc.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/avc", "aliases": "OMX.qcom.video.decoder.avc.secure", "maxSize": "4096x2176", "profiles": "20480,20481,20482,20484,20487", "levels": "20495"},
        {"store": "rank1", "rank": 1, "name": "c2.qti.hevc.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/hevc", "aliases": "OMX.qcom.video.decoder.hevc", "maxSize": "4096x2176", "profiles": "24576,24578,24577", "levels": "24835"},
        {"store": "rank1", "rank": 1, "name": "c2.qti.hevc.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/hevc", "aliases": "OMX.qcom.video.decoder.hevc.secure", "maxSize": "4096x2176", "profiles": "24576,24578,24577", "levels": "24835"},
        {"store": "rank1", "rank": 1, "name": "c2.qti.vp9.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/x-vnd.on2.vp9", "aliases": "OMX.qcom.video.decoder.vp9", "maxSize": "4096x2176", "profiles": "28672,28673,28674", "levels": "28680"},
        {"store": "rank1", "rank": 1, "name": "c2.qti.avc.encoder", "kind": "encoder", "domain": "video", "mediaType": "video/avc", "aliases": "OMX.qcom.video.encoder.avc", "maxSize": "4096x2176", "profiles": "20480,20482,20484", "levels": "20494"},
        {"store": "rank1", "rank": 1, "name": "c2.qti.hevc.encoder", "kind": "encoder", "domain": "video", "mediaType": "video/hevc", "aliases": "OMX.qcom.video.encoder.hevc", "maxSize": "4096x2176", "profiles": "24576", "levels": "24834"},
        {"store": "rank1", "rank": 1, "name": "c2.qti.heic.encoder", "kind": "encoder", "domain": "image", "mediaType": "image/vnd.android.heic", "aliases": "OMX.qcom.video.encoder.heic", "maxSize": "16384x16384"}
      ]
    },
    "audioEngine": {
      "modules": "default,bluetooth,r_submix",
      "voiceCallIndexMax": 7,
      "musicIndexMax": 15
    }
  },
  "alignmentNotes": [
    "平台判定：固件 board=bengal、device=bengal_515、socModel=Snapdragon 6s 4G Gen 2、firmware fingerprint 为 qti/bengal_515，故全部平台口径按 Qualcomm bengal（SM6225 家族）取值，与 gold 的 MT6833 无任何共用值。",
    "已知实测直取：densityDpi=480、1080x2392、hasNfc=true、hasConsumerIr=false、hasWifiDirect/Passpoint=true、audio modules=primary/a2dp/usb、keyboardName=gpio-keys、headsetJackName=bengal-snd-card Headset Jack、chipOwners(lsm6dso/stk3a8x/ak0991x)、13 条 effectDescriptors、c2.qti 编解码清单。",
    "传感器边界：unavailableFeatures 明确含 gyroscope/barometer/pressure 且有 lnal_virtual_gyro 配置，故不输出 TYPE_GYROSCOPE、不输出任何气压计条目；未检测到任何品牌私有传感器配置，故不编造 honor.sensor.*；也未出现 Xiaomi 的 swtp 逻辑。",
    "待真机复测（推断项）：EDID 产品 ID/出厂周数年份与面板厂商（按 BOE 族推断）、vulkanDriverVersion/UUID/glVersionFull 的 GIT 段、touch 驱动名（FocalTech fts_ts，Honor 该价位常用，需 dumpsys input 核验）、bt 5.1、5000mAh/35W、50MP+2MP 相机、imeiTac 与 meidPrefix、macOuiCandidates。",
    "指纹口径：固件 permissions.hasFingerprint=false，故 input_identity 不生成 uinput-fpc/uinput-goodix、goodixFp=false；若真机实为侧边指纹，仅需补 feature 行与一个 uinput 节点即可，不影响其余对齐。",
    "VINTF：vendor.qti.hardware.* 为高通公共实现保留；10 个 vendor.honor.hardware.* 私有接口列入 vintfDrop（本工程无对应 so/服务，保留会造成 VINTF 核验失败）。",
    "媒体登记纪律：固件实测 store=default 仅为原厂事实，落库时一律改写为 rank1/rank=1，避免伪装实例抢占 default 影响真实解码器选择。",
    "内核模块伪装：bengal 为 EPSS 硬件调频（qcom-cpufreq-hw + schedutil），频点取 Qualcomm 18/21 档 OPP 表，MIDR 为 A53 0x410fd034 与 A73 0x410fd092，不可沿用 gold 的 mt-cpufreq/sugov_ext 与 0xd05/0xd0b。"
  ],
  "sources": [
    "目标固件实测：build.prop / boot cmdline / VINTF manifest / vendor/etc + odm/etc 解析结果（本任务输入，最高优先级）",
    "Qualcomm bengal（SM6225）平台公开规格：4x Cortex-A53 1.9GHz + 4x Cortex-A73 2.4GHz、Adreno 610、LPDDR4X、UFS 2.2、Wi-Fi 5、BT 5.1、GNSS L1",
    "Android 传感器 HAL 规范（SensorManager 类型/句柄约定）与 AOSP lights/vibrator/thermal HAL 默认能力表",
    "对标样本：devices/manifest/configs/redmi/gold/gold-android15/ 的字段结构与颗粒度（仅结构参考，取值已全部替换为本机平台值）"
  ]
}
```

**下一步建议（02_prepare 期间）**：真机就绪后优先执行 `dumpsys input`、`dumpsys SurfaceFlinger`、`dumpsys media.c2`、`getprop | grep -E 'baseband|ril-impl'`、`cat /proc/bus/input/devices` 五项回填上述“待复测”字段，即可把本机从“推演对齐”升级到与 gold 同等级的“100% 真机对齐”。
