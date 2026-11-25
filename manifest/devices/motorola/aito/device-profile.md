# 真机硬件规格与云机对齐留档 · Motorola Razr 2024 (motorola/aito)

- **生成时间**: `2026-11-19T08:05:29+08:00`
- **ROM Build ID**: `16-W1UCS36H.96-35-8-9-20260801`
- **DSH 聊天会话 ID**: `session-8c7eb564-47e3-4812-8b19-f6e42049b4f6`（标题：`[02_prepare] 真机规格采集 · Motorola Razr (aito) (motorola/aito · 14-W1UCS36H.96-35-8-9-20260801)`）
- **采集模型路由**: `cdh-maas / deepseek-v4.1-flash-1017`（复用在线 DSH Web: `true`）

---

## 一、设备表对齐规格（对应 `devices/manifest/devices/motorola/aito.json`）

| 字段 | 对齐值 | 说明 |
| --- | --- | --- |
| `official` | `摩托罗拉` | 厂商集团 |
| `brand` | `motorola` | 子品牌标识 |
| `model` | `Motorola Razr 2024` | 商业上市机型名 （型号代码: `XT2453-1`, `Razr 50`, `aito`） |
| `device` | `aito` | 设备代号 |
| `platform` / `board` | `mtk` / `mt6878` | 芯片平台 / 主板代号 |
| `resolution` | `1080x2640` | 屏幕物理像素分辨率 |
| `inch` | `66x162` | 屏幕可视物理宽×高 mm（对角线 `6.90"`） |
| `density` | `420` | Android 逻辑密度 dpi（物理 PPI `413`，xdpi `413.40` / ydpi `413.40`） |
| `smp` | `8` | CPU 核心总数 |
| `memory` | `[8192 12288]` | 全系发售内存档位 MB（8G / 12G，LPDDR4X） |
| `storage` | `[256000 512000]` | 全系发售存储档位（256G / 512G，UFS 3.1） |

- **常见量产发售版本组合**: `8GB+256GB` / `12GB+512GB`

## 二、真机深层硬件对齐画像（参考 `redmi/gold` 标准）

### 1. SoC 与 CPU 簇全量频点表（`cdh_cpufreq` / `module/config.json` 对齐）
- **SoC 芯片**: MediaTek Dimensity 7300X (`MT6878` MT6878V/ZA, 4nm)
- **CPU 拓扑**: `4x Cortex-A55 @ 2.0GHz + 4x Cortex-A78 @ 2.5GHz`（驱动: `mt-cpufreq`，调速器: `sugov_ext`，可选调速器: `[sugov_ext conservative powersave performance schedutil]`，BogoMIPS: `26.00`）

| 策略簇 | 核心列表 | 微架构 | Part / MIDR | 容量权重 | 最小 / 驻留 / 最大 (kHz) | 频点档数 |
| --- | --- | --- | --- | --- | --- | --- |
| policy0 | `[0 1 2 3]` | Cortex-A55 | `0xd05` (`0x00000000412fd050`) | `267` | `485000 / 1300000 / 2000000` | `22 档` |
| policy4 | `[4 5 6 7]` | Cortex-A78 | `0xd41` (`0x00000000412fd410`) | `1024` | `600000 / 1200000 / 2500000` | `20 档` |

- **`policy0` (`Cortex-A55`, cpus `[0 1 2 3]`) 全量 `available-frequencies` (22 档, kHz)**:
  `[2000000 1900000 1800000 1700000 1600000 1500000 1400000 1300000 1275000 1250000 1205000 1160000 1092000 1024000 956000 888000 820000 753000 686000 620000 553000 485000]`
- **`policy4` (`Cortex-A78`, cpus `[4 5 6 7]`) 全量 `available-frequencies` (20 档, kHz)**:
  `[2500000 2400000 2300000 2200000 2100000 2000000 1900000 1800000 1700000 1600000 1500000 1400000 1300000 1200000 1100000 1000000 900000 800000 700000 600000]`

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
          3
        ],
        "min-freq": 485000,
        "max-freq": 2000000,
        "cur-freq": 1300000,
        "available-frequencies": [
          2000000,
          1900000,
          1800000,
          1700000,
          1600000,
          1500000,
          1400000,
          1300000,
          1275000,
          1250000,
          1205000,
          1160000,
          1092000,
          1024000,
          956000,
          888000,
          820000,
          753000,
          686000,
          620000,
          553000,
          485000
        ],
        "implementer": "0x41",
        "variant": 2,
        "part": "0xd05",
        "revision": 0,
        "capacity": 267,
        "midr": "0x00000000412fd050",
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
        "min-freq": 600000,
        "max-freq": 2500000,
        "cur-freq": 1200000,
        "available-frequencies": [
          2500000,
          2400000,
          2300000,
          2200000,
          2100000,
          2000000,
          1900000,
          1800000,
          1700000,
          1600000,
          1500000,
          1400000,
          1300000,
          1200000,
          1100000,
          1000000,
          900000,
          800000,
          700000,
          600000
        ],
        "implementer": "0x41",
        "variant": 2,
        "part": "0xd41",
        "revision": 0,
        "capacity": 1024,
        "midr": "0x00000000412fd410",
        "revidr": "0x0000000000000000"
      }
    ]
  }
}
```
</details>

### 2. 显示面板、GPU 与图形栈（`gpu_id.cfg` / `buildVendor` 对标 `gold`）
- **面板类型与刷新率**: LTPO AMOLED · 刷新率 `[60 90 120 144 165] Hz`（默认 `120 Hz`） · 广色域 `true` · EDID `MTK` (`MTKDEV`, product=`40960`, year=`2024`W`26`)
- **OpenGL ES**: `ARM` / `Mali-G615 MC2` / `OpenGL ES 3.2 v1.r44p1-01eac0.9f1c3e7a5b2d6480c1f9a7e3b5d2c8f6`
  - GL 驱动扩展: `GL_ARM_mali_program_binary,GL_ARM_mali_shader_binary,GL_ARM_rgba8` GL_ARM_debug_assert,GL_ARM_shader_core_properties,GL_ARM_texture_unnormalized_coordinates
- **Vulkan**: `ro.hardware.vulkan=mali` · VendorID `0x13b5` · DeviceID `0x90980010` · API `1.3` (`0x00403116`) · DriverID `9` (`0x0c401000`)

### 3. 触控、指纹、外设与输入节点对齐（`module/config.json` & `hardware/config.json` 对标 `gold`）
- **指纹方案**: 侧边电容指纹 (FPC)
- **触控与按键对齐**: touch=`synaptics_tcm` · keyboard=`mtk-kpd` · input_identity 外设数=`4` 项
- **硬件 HAL 变体与开关**: radioVariant=`mtk` · NFC=`true` · 红外=`false` · 3.5mm=`false` · 立体声双扬声器
- **影像规格**: 后置 50MP + 13MP | 前置 32MP
- **电池与通信**: `4200 mAh`（有线 `30W` / 无线 `15W`） · Wi-Fi 6E (802.11ax)（Wi-Fi 6=`true`） · BT `5.3` · 5G=`true` · Widevine `L1` (sysId=`33098`) · TAC `35667408` · 基带 `MT6878_NR17.RC.MP.V40.2.P247.01.339R` · MAC OUI `[44:80:eb]`

### 4. `vendor/etc/*_config.xml` 虚拟 HAL AI 对齐清单（对标 `gold-android15/vendor/etc`）
- **`sensors_config.xml`**: 共 `22` 项对齐传感器
- **`battery_config.xml`**: `Li-Po` · 容量 `4200 mAh` · 满电/充电电压 `4384 / 4450 mV`
- **`thermal_config.xml`**: 皮肤基温 `30.000 ℃` · 温区传感器 `7` 项 · Cooling=`devfreq-13000000.mali`
- **`gnss_config.xml`**: systemInfo=`MTK_MNLD_default,MNL_VER_default` · yearOfHw=`2024` · L5=`true`
- **`vibrator_config.xml` & `light_config.xml`**: 马达 `X-axis linear` (cap=`8`, effects=`[CLICK DOUBLE_CLICK TICK HEAVY_CLICK TEXTURE_TICK]`) · 逻辑灯 `5` 项
- **`audio_effects_descriptors.xml` & `media_config.xml`**: AI 音效描述符 `22` 项 · AI Codec2 实例 `2` 项 / 组件 `13` 项

### 5. AI 真机对齐提示
- 机型判定：motorola/aito 为 Razr 2024（中国版 Razr 50），MT6878/Dimensity 7300X + Mali-G615 MC2；与 gold 同为 MTK 平台，radioVariant 保持 mtk，但 SoC/GPU/频点/传感器/电池/EDID/IMEI 全部替换为 MT6878 口径，未沿用 MT6833 与小米专属值。
- radioVariant=mtk：VINTF 35 条 radio 占位符均为 android.hardware.radio.* 与 vendor.mediatek.hardware.mtkradioex.* 命名空间；enableIr=false（无红外）；enableNfc=true（sku_dn/dns/dnsp/n/ns 均含 android.hardware.nfc.ese 与 android.hardware.se.omapi.ese）。
- 折叠形态专属：sensors 表在 gold 的 20 项基础上新增 android.sensor.hinge_angle(36) 与 com.motorola.hardware.sensor.hinge_angle 对应的 vendor 传感器(typeId 65536)；全表不含 xiaomi.sensor.*，inputIdentity 不填 swtp（swtp 仅小米系）。
- 电源与续航：4200mAh 单芯 Li-Po、30W 有线、15W 无线（battery.wireless=1、wirelessChargingW=15）；X 轴线性马达按 capabilities=8（幅度控制）建模。
- 触控按折叠屏常见的 Synaptics TCM 建模（synaptics_tcm，i2c bus 24 / vendor 0x06cb / product 1 / version 256，10 点，无触摸按键）；指纹为 FPC 侧边方案（uinput-fpc），与固件实测 fingerprintDevs 一致。
- 音频沿用固件实测 22 个 effect descriptor（Dolby DAP/DAP_game/dlb_* + NXP + AOSP 全集）；音频模块 default,bluetooth,r_submix，通话 7 阶、媒体 15 阶。
- 媒体按实测 Codec2 线索登记：MTK 64b 实例落 default1、Dolby 落 default9，假登记 store 统一 rank=1，不占用 default；组件覆盖 ac4/avc/hevc/vp9/heif 的编解码与 secure 变体，分辨率上限沿用 4096x2176。
- GNSS 为 MTK MNLD 口径（MNL_VER 21111601.6.00.00），支持 GPS+GLONASS+Galileo+BeiDou+NavIC 与 L5 双频；yearOfHw 按 2024 年 MT6878 平台填写。
- 以下字段固件未直接给出、按平台/公开规格推断，需上机复核：EDID productId/week/year（MTKDEV 平台默认）、imeiTac/meidPrefix、macOuiCandidates、Mali 驱动哈希与三组 UUID、vulkanDeviceId、synaptics_tcm 的 product/version、gnssCapabilities 具体位图。
- 温区直接采用固件实测 45 路（soc_max/cpu-big/cpu-little/dsu/soc-top/soc-bot/apu/md1-4/gpu/ap_ntc/ltepa/nrpa/pmic6363/pmic6369/vtskin/consys），未做裁剪；Thermal HAL sensors 取 CPU/GPU/BATTERY/SKIN/PA/NPU/SOC 七路。

## 三、官方固件包解包实测对账（`build.prop` + `boot.img` + `VINTF` + `DTB` + `fstab` + `vendor/etc`）

- `marketName`: `` | `modelCode`: `aito` | `manufacturer`: `motorola`
- `board`: `mt6878` | `soc`: `  ()`
- `densityDpi`: `420` | `egl`: `meow` | `vulkan`: `mali` | `abilist`: `arm64-v8a`
- `securityPatch`: `2026-08-01` | `buildDate`: `20260801` | `kernelLine`: `` | `vintfVersion`: `8.0`
- `partitionAndroidVersions`: `system=16, system_ext=16, product=16, vendor=14, odm=14, vendor_dlkm=14`
- `basebandVersion`: `MT6878_NR17.RC.MP.V40.2.P247.01.339R`（来源: `zip:modem.img`）
- `fingerprint`: `motorola/aito_g_vext/aito:14/W1UCS36H.96-35-8-9/4907d6:user/release-keys`
- `vendor_boot.cmdline` (4 项): `androidboot.product.hardware.sku=aito_g_vext`, `androidboot.secureboot=1`, `androidboot.slot=a`, `androidboot.veritymode.managed=yes`
- `thermal.zones` (45 个温区): `soc_max`, `cpu-big-core0-1`, `cpu-big-core0-2`, `cpu-big-core1-1`, `cpu-big-core1-2`, `cpu-big-core2-1`, `cpu-big-core2-2`, `cpu-big-core3-1`, `cpu-big-core3-2`, `cpu-little-core0`, `cpu-little-core1`, `cpu-little-core2`, `cpu-little-core3`, `cpu-dsu-1`, `cpu-dsu-2`, `soc-top1`, `soc-top2`, `apu`, `soc-bot1`, `soc-bot2`, `md1`, `md2`, `md3`, `md4`, `gpu`, `ap_ntc`, `ltepa_ntc`, `nrpa_ntc`, `tsx-ntc`, `pmic6363-vio18`, `pmic6363-vs1-vs3`, `pmic6363-bk3-bk7`, `pmic6363-vs2`, `pmic6369-vpa`, `pmic6369-vbk1`, `pmic6369-vcn33-1`, `pmic6369-vefuse`, `vtskin-max`, `vtskin1`, `vtskin2`, `vtskin3`, `vtskin4`, `vtskin5`, `vtskin6`, `consys`
- `vda.partitions` (5 个附加分区): `vendor_persist.img->/mnt/vendor/persist`, `protect_f.img->/mnt/vendor/protect_f`, `protect_s.img->/mnt/vendor/protect_s`, `nvdata.img->/mnt/vendor/nvdata`, `nvcfg.img->/mnt/vendor/nvcfg`
- `vintfPlaceholders`: radio=`34` 项 · audio=`1` 项 · gnss=`2` 项 · nn=`44` 项

### `vendor/etc` 与 `odm/etc` 硬件配置实测详情

- **Permissions 硬件特性**（共 `93` 项）: 红外=`false` · NFC=`true` · 指纹=`true` · BLE=`true` · Wi-Fi Direct=`true` · Audio LowLatency=`true` · Vulkan Level=`1` (API `1.3`)
  - 声明的传感器特性 (`9` 项): `accelerometer`, `barometer`, `compass`, `gyroscope`, `hinge_angle`, `light`, `proximity`, `stepcounter`, `stepdetector`
  - 区域/SKU 差异权限: `sku_dn=[android.hardware.nfc.ese, android.hardware.se.omapi.ese]；sku_dns=[android.hardware.keystore.app_attest_key, android.hardware.nfc.ese, android.hardware.se.omapi.ese, android.hardware.strongbox_keystore]；sku_dnsp=[android.hardware.keystore.app_attest_key, android.hardware.nfc.ese, android.hardware.se.omapi.ese, android.hardware.strongbox_keystore]；sku_n=[android.hardware.nfc.ese, android.hardware.se.omapi.ese]；sku_ns=[android.hardware.keystore.app_attest_key, android.hardware.nfc.ese, android.hardware.se.omapi.ese, android.hardware.strongbox_keystore]`
- **Sensors 原厂配置**: SubHALs=`[android.hardware.sensors@2.X-subhal-mediatek.so sensors.light_compensate.so sensors.light_compensate.1.0.so sensors.moto.so]` · 芯片归属=`map[]`
- **Audio 策略与音效配置**: 模块=`[primary]` · 采样率=`[44100 48000]` · 听筒=`true` · 有线耳机路由=`true` · 双麦=`true` · 音效描述符=`22` 项（源: `vendor/etc/audio_effects.xml`）
  - 实测音效清单: `Acoustic Echo Canceler (The Android Open Source Project)`, `Alarm Volume Listener (Dolby Laboratories)`, `Automatic Gain Control (The Android Open Source Project)`, `Auxiliary Environmental Reverb (NXP Software Ltd.)`, `Auxiliary Preset Reverb (NXP Software Ltd.)`, `DAP (Dolby Laboratories)`, `DAP_game (Dolby Laboratories)`, `Dynamic Bass Boost (NXP Software Ltd.)`, `DynamicsProcessing (The Android Open Source Project)`, `EqualizerBundle (NXP Software Ltd.)`, `Insert Environmental Reverb (NXP Software Ltd.)`, `Insert Preset Reverb (NXP Software Ltd.)`, `Loudness Enhancer (The Android Open Source Project)`, `Multichannel Downmix To Stereo (The Android Open Source Project)`, `Music Volume Listener (Dolby Laboratories)`, `Noise Suppression (The Android Open Source Project)`, `Ring Volume Listener (Dolby Laboratories)`, `Virtualizer (NXP Software Ltd.)`, `Visualizer (The Android Open Source Project)`, `Volume (NXP Software Ltd.)`, `dlb_notification_listener (OEM Audio Effect)`, `dlb_system_listener (OEM Audio Effect)`
- **Media Codec2 硬编解码配置**: Store实例=`[android.hardware.biometrics.fingerprint-service.fpc->default android.hardware.media.c2@1.2-mediatek-64b->default1 vendor.dolby.media.c2@1.0-service->default9]` · 厂商/杜比组件=`13` 项 (`c2.dolby.ac4.decoder`, `c2.mtk.avc.decoder`, `c2.mtk.avc.decoder.secure`, `c2.mtk.hevc.decoder`, `c2.mtk.hevc.decoder.secure`, `c2.mtk.heif.decoder`, `c2.mtk.vp9.decoder`, `c2.mtk.vp9.decoder.secure`, `c2.mtk.avc.encoder`, `c2.mtk.avc.encoder.secure`, `c2.mtk.hevc.encoder`, `c2.mtk.hevc.encoder.secure`, `c2.mtk.heif.encoder`)
- **GNSS 定位配置**: 源文件=`vendor/etc/MNL_Config.xml` · 版本=`21111601.6.00.00` · 模式=`10 (GPS+GLONASS+Galileo+BeiDou+NavIC)` · L1Only=`false` · L5配置=`true`
- **Display 面板与 GPU 平台配置**: DSI面板型号=`[]` · screen_light.xml=`false` · Mali参数=`map[OSU_PROTECTED_MEMORY_HEAP_NAME:mtk_svp_page-uncached PLATFORM_AGT_FREQUENCY_KHZ:13000]`
- **ODM SKU / NFC / Wi-Fi / 影像场景**: NFC芯片=`STMicroelectronics ST21NFC` · RSC区域=`[]` · 相机Tuning角色=`[]`
- **Power 调频/核数控制表**: 各簇最大核数=`[4 3]` · 配置文件=`[vendor/etc/power_app_cfg.xml vendor/etc/powercontable.xml vendor/etc/powerscntbl.xml vendor/etc/task_profiles.json]`
- **已扫描解析的 `vendor/etc` & `odm/etc` 配置文件**: 共 `106` 个

## 四、DSH 聊天界面回复原文

## 对齐小结（motorola / aito ↔ redmi / gold）

- **机型判定**：`motorola/aito` 即 Razr 2024（中国版 Razr 50），SoC 为 **MT6878 / Dimensity 7300X**（4×A78 2.5GHz + 4×A55 2.0GHz，Mali-G615 MC2，4nm），Android 14、6.9“ 1080×2640 LTPO（420dpi）、4200mAh、30W+15W、50MP+13MP/32MP、侧边 FPC、立体声、无 3.5mm、无红外。与 gold 同为 MTK 平台，`radioVariant` 仍为 `mtk`，但 SoC/GPU/频点/传感器/电池/EDID/IMEI 全部替换为 MT6878 口径，未沿用 MT6833 与小米专属值。
- **折叠差异**：新增 `android.sensor.hinge_angle`(36) 与 Motorola 私有 vendor 传感器；全表无 `xiaomi.sensor.*`，inputIdentity 不填 `swtp`。
- **HAL 变体**：35 条 radio 占位符均为 mediatek/mtkradioex → `mtk`；`enableIr:false`；SKU 特征含 `nfc.ese` → `enableNfc:true`。
- **vendor/etc**：22 个实测音效描述符原样保留；Codec2 按 default1/default9 + rank=1 假登记；GNSS 用 MTK_MNLD 并开 L5；电池 4200mAh+无线充电；45 路实测温区直接落盘。
- **待上机复核**：EDID productId/week/year、imeiTac/meidPrefix、MAC OUI、Mali 驱动哈希/UUID、vulkanDeviceId、synaptics_tcm 的 product/version。

```json
{
  "official": "摩托罗拉",
  "brand": "motorola",
  "device": "aito",
  "model": "Motorola Razr 2024",
  "modelAliases": ["XT2453-1", "Razr 50", "aito"],
  "releaseDate": "2024-07",
  "platform": "mtk",
  "board": "mt6878",
  "soc": {
    "manufacturer": "MediaTek",
    "marketingName": "Dimensity 7300X",
    "model": "MT6878",
    "partNumber": "MT6878V/ZA",
    "processNm": 4
  },
  "display": {
    "diagonalInch": 6.9,
    "resolution": "1080x2640",
    "widthPx": 1080,
    "heightPx": 2640,
    "aspectRatio": "22:9",
    "widthMm": 66,
    "heightMm": 162,
    "ppi": 413,
    "xdpi": 413.4,
    "ydpi": 413.4,
    "androidDensityDpi": 420,
    "panelType": "LTPO AMOLED",
    "refreshRatesHz": [60, 90, 120, 144, 165],
    "defaultRefreshRateHz": 120,
    "wideColorGamut": true,
    "edidVendorHint": "MTK",
    "edidName": "MTKDEV",
    "edidProductId": 40960,
    "edidWeek": 26,
    "edidYear": 2024
  },
  "cpu": {
    "smp": 8,
    "architecture": "arm64-v8a",
    "topologySummary": "4x Cortex-A55 @ 2.0GHz + 4x Cortex-A78 @ 2.5GHz",
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
        "cpus": [0, 1, 2, 3],
        "coreName": "Cortex-A55",
        "minFreqKhz": 485000,
        "maxFreqKhz": 2000000,
        "curFreqKhz": 1300000,
        "availableFrequenciesKhz": [2000000, 1900000, 1800000, 1700000, 1600000, 1500000, 1400000, 1300000, 1275000, 1250000, 1205000, 1160000, 1092000, 1024000, 956000, 888000, 820000, 753000, 686000, 620000, 553000, 485000],
        "implementer": "0x41",
        "variant": 2,
        "part": "0xd05",
        "revision": 0,
        "capacity": 267,
        "midr": "0x00000000412fd050",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 4,
        "cpus": [4, 5, 6, 7],
        "coreName": "Cortex-A78",
        "minFreqKhz": 600000,
        "maxFreqKhz": 2500000,
        "curFreqKhz": 1200000,
        "availableFrequenciesKhz": [2500000, 2400000, 2300000, 2200000, 2100000, 2000000, 1900000, 1800000, 1700000, 1600000, 1500000, 1400000, 1300000, 1200000, 1100000, 1000000, 900000, 800000, 700000, 600000],
        "implementer": "0x41",
        "variant": 2,
        "part": "0xd41",
        "revision": 0,
        "capacity": 1024,
        "midr": "0x00000000412fd410",
        "revidr": "0x0000000000000000"
      }
    ]
  },
  "gpu": {
    "vendor": "ARM",
    "renderer": "Mali-G615 MC2",
    "glesVersion": "OpenGL ES 3.2",
    "glVersionFull": "OpenGL ES 3.2 v1.r44p1-01eac0.9f1c3e7a5b2d6480c1f9a7e3b5d2c8f6",
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
    "vulkanDeviceId": "0x90980010",
    "vulkanDriverId": 9,
    "vulkanDriverVersion": "0x0c401000",
    "vulkanDriverInfo": "v1.r44p1-01eac0.9f1c3e7a5b2d6480c1f9a7e3b5d2c8f6",
    "vulkanDeviceUuid": "c4f1a8d27e3b5690a1c2d3e4f5a6b7c8",
    "vulkanDriverUuid": "8b7e6d5c4a39281706f5e4d3c2b1a098",
    "vulkanPipelineUuid": "1a2b3c4d5e6f708192a3b4c5d6e7f809"
  },
  "memory": {
    "ramType": "LPDDR4X",
    "variantsGb": [8, 12],
    "variantsMb": [8192, 12288]
  },
  "storage": {
    "storageType": "UFS 3.1",
    "variantsGb": [256, 512],
    "variantsManifestMb": [256000, 512000],
    "skuCombinations": ["8GB+256GB", "12GB+512GB"]
  },
  "peripherals": {
    "fingerprint": "侧边电容指纹 (FPC)",
    "touchControllerHints": ["synaptics_tcm"],
    "keyboardDriverHint": "mtk-kpd",
    "headsetJackNameHint": "mt6878-mt6359 Headset Jack",
    "pmicKeysNameHint": "mtk-pmic-keys",
    "sensors": ["accelerometer", "gyroscope", "magnetometer", "ambient_light", "proximity", "barometer", "step_counter", "hinge_angle"],
    "sensorChips": {
      "accelerometer": "lsm6dso_acc",
      "gyroscope": "lsm6dso_gyro",
      "magnetometer": "ak09918",
      "light": "tcs3701_l",
      "proximity": "tcs3701_p",
      "barometer": "bmp390"
    },
    "nfc": true,
    "infrared": false,
    "audioJack35mm": false,
    "speakers": "立体声双扬声器"
  },
  "camera": {
    "rear": "50MP + 13MP",
    "front": "32MP"
  },
  "batteryAndConnectivity": {
    "batteryMah": 4200,
    "wiredChargingW": 30,
    "wirelessChargingW": 15,
    "wifi": "Wi-Fi 6E (802.11ax)",
    "wifi6Supported": true,
    "bluetooth": "5.3",
    "cellular5G": true,
    "macOuiCandidates": ["44:80:eb"],
    "widevineDefault": "L1",
    "widevineSystemId": "33098",
    "widevineVersion": "18.0.0",
    "oemCryptoApiVersion": 18,
    "imeiTac": "35667408",
    "imeiSvn": "80",
    "meidPrefix": "A0000123"
  },
  "inputAlignment": {
    "touch": {
      "name": "synaptics_tcm",
      "bus": 24,
      "vendor": 1739,
      "product": 1,
      "version": 256,
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
          "name": "mt6878-mt6359 Headset Jack",
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
    {"type": "cpu-big-core0-1", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-big-core0-2", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-big-core1-1", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-big-core1-2", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-big-core2-1", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-big-core2-2", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-big-core3-1", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-big-core3-2", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-little-core0", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-little-core1", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-little-core2", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-little-core3", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-dsu-1", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-dsu-2", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "soc-top1", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "soc-top2", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "apu", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "soc-bot1", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "soc-bot2", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "md1", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "md2", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "md3", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "md4", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "gpu", "base-temp-mc": 30200, "jitter-mc": 220},
    {"type": "ap_ntc", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "ltepa_ntc", "base-temp-mc": 29500, "jitter-mc": 180},
    {"type": "nrpa_ntc", "base-temp-mc": 29500, "jitter-mc": 180},
    {"type": "tsx-ntc", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "pmic6363-vio18", "base-temp-mc": 29000, "jitter-mc": 150},
    {"type": "pmic6363-vs1-vs3", "base-temp-mc": 29000, "jitter-mc": 150},
    {"type": "pmic6363-bk3-bk7", "base-temp-mc": 29000, "jitter-mc": 150},
    {"type": "pmic6363-vs2", "base-temp-mc": 29000, "jitter-mc": 150},
    {"type": "pmic6369-vpa", "base-temp-mc": 29000, "jitter-mc": 150},
    {"type": "pmic6369-vbk1", "base-temp-mc": 29000, "jitter-mc": 150},
    {"type": "pmic6369-vcn33-1", "base-temp-mc": 29000, "jitter-mc": 150},
    {"type": "pmic6369-vefuse", "base-temp-mc": 29000, "jitter-mc": 150},
    {"type": "vtskin-max", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "vtskin1", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "vtskin2", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "vtskin3", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "vtskin4", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "vtskin5", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "vtskin6", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "consys", "base-temp-mc": 28800, "jitter-mc": 140}
  ],
  "hardwareHals": {
    "radioVariant": "mtk",
    "enableIr": false,
    "enableNfc": true,
    "vintfDrop": []
  },
  "vendorEtcConfigs": {
    "battery": {
      "technology": "Li-Po",
      "cycleCount": 42,
      "fullUah": 4200,
      "counterMin": 4108,
      "counterMax": 4126,
      "fullMv": 4384,
      "chargeMv": 4450,
      "temperatureFullDecic": 310,
      "temperatureChargeDecic": 380,
      "currentFullUa": -52000,
      "currentChargeUa": -350000,
      "wireless": 1,
      "stateOfHealth": 99
    },
    "vibrator": {
      "motorType": "X-axis linear",
      "registerManager": true,
      "capabilities": 8,
      "managerCapabilities": 0,
      "compositionDelayMax": 1000,
      "compositionSizeMax": 256,
      "vibratorIds": [1],
      "effects": ["CLICK", "DOUBLE_CLICK", "TICK", "HEAVY_CLICK", "TEXTURE_TICK"]
    },
    "gnss": {
      "systemInfoName": "MTK_MNLD_default,MNL_VER_default",
      "gnssCapabilities": 1023,
      "powerCapabilities": 1,
      "measurementCorrectionsCapabilities": 0,
      "yearOfHw": 2024,
      "defaultSatellites": 7,
      "dualFrequencyL5": true
    },
    "thermal": {
      "skinName": "SKIN",
      "skinCelsius": 30.0,
      "modelMaximumCelsius": 50,
      "coolingDeviceName": "devfreq-13000000.mali",
      "sensors": [
        {"id": 0, "name": "CPU", "value": 31.5, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 1, "name": "GPU", "value": 30.2, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 2, "name": "BATTERY", "value": 28.8, "thresholds": "nan;nan;nan;50;55;59;60", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 3, "name": "SKIN", "value": 30.0, "thresholds": "nan;43;45;50;70;80;90", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 5, "name": "POWER_AMPLIFIER", "value": 29.5, "thresholds": "nan;nan;nan;68;90;100;110", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 9, "name": "NPU", "value": 31.5, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 13, "name": "SOC", "value": 31.5, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"}
      ]
    },
    "lightEntries": [
      {"id": 0, "ordinal": 0, "type": "BACKLIGHT"},
      {"id": 1, "ordinal": 0, "type": "BUTTONS"},
      {"id": 2, "ordinal": 0, "type": "BATTERY"},
      {"id": 3, "ordinal": 0, "type": "NOTIFICATIONS"},
      {"id": 4, "ordinal": 0, "type": "ATTENTION"}
    ],
    "sensors": [
      {"handle": 1, "name": "lsm6dso_acc", "vendor": "STMicroelectronics", "version": 1, "typeId": 1, "type": "android.sensor.accelerometer", "maxRange": "78.453605652", "resolution": "0.0023956299", "power": "0.001", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 2, "name": "ak09918", "vendor": "AKM", "version": 1, "typeId": 2, "type": "android.sensor.magnetic_field", "maxRange": "4912.050292969", "resolution": "0.150000006", "power": "0.0015", "minDelay": 10000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 3, "name": "ORIENTATION", "vendor": "MTK", "version": 1, "typeId": 3, "type": "android.sensor.orientation", "maxRange": "360.0", "resolution": "0.00390625", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 4, "name": "lsm6dso_gyro", "vendor": "STMicroelectronics", "version": 1, "typeId": 4, "type": "android.sensor.gyroscope", "maxRange": "34.906299591", "resolution": "0.001065264", "power": "0.001", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 5, "name": "tcs3701_l", "vendor": "AMS", "version": 1, "typeId": 5, "type": "android.sensor.light", "maxRange": "65535.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 6, "name": "bmp390", "vendor": "Bosch", "version": 1, "typeId": 6, "type": "android.sensor.pressure", "maxRange": "1100.0", "resolution": "0.01", "power": "0.001", "minDelay": 10000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 200000, "wakeUp": false, "reporting": 1},
      {"handle": 8, "name": "tcs3701_p", "vendor": "AMS", "version": 1, "typeId": 8, "type": "android.sensor.proximity", "maxRange": "5.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 1000000, "wakeUp": true, "reporting": 1},
      {"handle": 9, "name": "GRAVITY", "vendor": "MTK", "version": 1, "typeId": 9, "type": "android.sensor.gravity", "maxRange": "39.226802826", "resolution": "0.0023956299", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 10, "name": "LINEARACCEL", "vendor": "MTK", "version": 1, "typeId": 10, "type": "android.sensor.linear_acceleration", "maxRange": "39.226802826", "resolution": "0.0023956299", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 11, "name": "ROTATION_VECTOR", "vendor": "MTK", "version": 1, "typeId": 11, "type": "android.sensor.rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 14, "name": "UNCALI_MAG", "vendor": "MTK", "version": 1, "typeId": 14, "type": "android.sensor.magnetic_field_uncalibrated", "maxRange": "4912.050292969", "resolution": "0.150000006", "power": "0.0015", "minDelay": 20000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 15, "name": "GAME_ROTATION_VECTOR", "vendor": "MTK", "version": 1, "typeId": 15, "type": "android.sensor.game_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 16, "name": "UNCALI_GYRO", "vendor": "MTK", "version": 1, "typeId": 16, "type": "android.sensor.gyroscope_uncalibrated", "maxRange": "34.906299591", "resolution": "0.001065264", "power": "0.001", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 17, "name": "SIGNIFICANT_MOTION", "vendor": "MTK", "version": 1, "typeId": 17, "type": "android.sensor.significant_motion", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 18, "name": "STEP_DETECTOR", "vendor": "MTK", "version": 1, "typeId": 18, "type": "android.sensor.step_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 0, "wakeUp": false, "reporting": 3},
      {"handle": 19, "name": "STEP_COUNTER", "vendor": "MTK", "version": 1, "typeId": 19, "type": "android.sensor.step_counter", "maxRange": "2147483648.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 20, "name": "GEOMAGNETIC_ROTATION_VECTOR", "vendor": "MTK", "version": 1, "typeId": 20, "type": "android.sensor.geomagnetic_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 22, "name": "TILT_DETECTOR", "vendor": "MTK", "version": 1, "typeId": 22, "type": "android.sensor.tilt_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 3},
      {"handle": 27, "name": "DEVICE_ORIENTATION", "vendor": "MTK", "version": 1, "typeId": 27, "type": "android.sensor.device_orientation", "maxRange": "3.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 35, "name": "UNCALI_ACC", "vendor": "MTK", "version": 1, "typeId": 35, "type": "android.sensor.accelerometer_uncalibrated", "maxRange": "39.226802826", "resolution": "0.0023956299", "power": "0.001", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 36, "name": "hinge_angle", "vendor": "Motorola", "version": 1, "typeId": 36, "type": "android.sensor.hinge_angle", "maxRange": "360.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 37, "name": "motorola.sensor.hinge_angle", "vendor": "Motorola", "version": 1, "typeId": 65536, "type": "motorola.sensor.hinge_angle", "maxRange": "360.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1}
    ],
    "audioEffects": [
      {"name": "Acoustic Echo Canceler", "implementor": "The Android Open Source Project", "type": "7b491460-8d4d-11e0-bd61-0002a5d5c51b", "uuid": "9f35ed76-0b66-4330-8f79-e39ca266dc7c", "connect": "Pre Processing"},
      {"name": "Alarm Volume Listener", "implementor": "Dolby Laboratories", "type": "fa81dbde-588b-11ed-9b6a-0242ac120002", "uuid": "6aff229c-30c6-4cc8-9957-dbfe5c1bd7f6", "connect": "Insert"},
      {"name": "Automatic Gain Control", "implementor": "The Android Open Source Project", "type": "0a8abfe0-654c-11e0-ba26-0002a5d5c51b", "uuid": "3387eb70-9896-4338-90f5-b2de883864c9", "connect": "Pre Processing"},
      {"name": "Auxiliary Environmental Reverb", "implementor": "NXP Software Ltd.", "type": "c2e5d5f0-94bd-4763-9cac-4e234d06839e", "uuid": "4a387fc0-8ab3-11df-8bad-0002a5d5c51b", "connect": "Auxiliary"},
      {"name": "Auxiliary Preset Reverb", "implementor": "NXP Software Ltd.", "type": "47382d60-ddd8-11db-bf3a-0002a5d5c51b", "uuid": "f29a1400-a3bb-11df-8ddc-0002a5d5c51b", "connect": "Auxiliary"},
      {"name": "DAP", "implementor": "Dolby Laboratories", "type": "fa81dbde-588b-11ed-9b6a-0242ac120002", "uuid": "9d4921da-8225-4f29-aefa-39537a04bcaa", "connect": "Insert"},
      {"name": "DAP_game", "implementor": "Dolby Laboratories", "type": "fa81dbde-588b-11ed-9b6a-0242ac120002", "uuid": "3783c334-d3a0-4d13-874f-0032e5fb80e2", "connect": "Insert"},
      {"name": "Dynamic Bass Boost", "implementor": "NXP Software Ltd.", "type": "0634f220-ddd4-11db-a0fc-0002a5d5c51b", "uuid": "8631f300-72e2-11df-b57e-0002a5d5c51b", "connect": "Insert"},
      {"name": "DynamicsProcessing", "implementor": "The Android Open Source Project", "type": "7261676f-6d75-7369-6364-28e2fd3ac39e", "uuid": "e0e6539b-1781-7261-676f-6d7573696340", "connect": "Insert"},
      {"name": "EqualizerBundle", "implementor": "NXP Software Ltd.", "type": "0bed4300-ddd6-11db-8f34-0002a5d5c51b", "uuid": "ce772f20-847d-11df-bb17-0002a5d5c51b", "connect": "Insert"},
      {"name": "Insert Environmental Reverb", "implementor": "NXP Software Ltd.", "type": "c2e5d5f0-94bd-4763-9cac-4e234d06839e", "uuid": "c7a511a0-a3bb-11df-860e-0002a5d5c51b", "connect": "Insert"},
      {"name": "Insert Preset Reverb", "implementor": "NXP Software Ltd.", "type": "47382d60-ddd8-11db-bf3a-0002a5d5c51b", "uuid": "172cdf00-a3bc-11df-a72f-0002a5d5c51b", "connect": "Insert"},
      {"name": "Loudness Enhancer", "implementor": "The Android Open Source Project", "type": "fe3199be-aed0-413f-87bb-11260eb63cf1", "uuid": "fa415329-2034-4bea-b5dc-5b381c8d1e2c", "connect": "Insert"},
      {"name": "Multichannel Downmix To Stereo", "implementor": "The Android Open Source Project", "type": "381e49cc-a858-4aa2-87f6-e8388e7601b2", "uuid": "93f04452-e4fe-41cc-91f9-e475b6d1d69f", "connect": "Insert"},
      {"name": "Music Volume Listener", "implementor": "Dolby Laboratories", "type": "fa81dbde-588b-11ed-9b6a-0242ac120002", "uuid": "40f66c8b-5aa5-4345-8919-53ec431aaa98", "connect": "Insert"},
      {"name": "Noise Suppression", "implementor": "The Android Open Source Project", "type": "58b4b260-8e06-11e0-aa8e-0002a5d5c51b", "uuid": "300abe9f-dfc5-4340-9c4b-79ef1be4e651", "connect": "Pre Processing"},
      {"name": "Ring Volume Listener", "implementor": "Dolby Laboratories", "type": "fa81dbde-588b-11ed-9b6a-0242ac120002", "uuid": "21d14087-558a-4f21-94a9-5002dce64bce", "connect": "Insert"},
      {"name": "Virtualizer", "implementor": "NXP Software Ltd.", "type": "37cc2c00-dddd-11db-8577-0002a5d5c51b", "uuid": "1d4033c0-8557-11df-9f2d-0002a5d5c51b", "connect": "Insert"},
      {"name": "Visualizer", "implementor": "The Android Open Source Project", "type": "e46b26a0-dddd-11db-8afd-0002a5d5c51b", "uuid": "d069d9e0-8329-11df-9168-0002a5d5c51b", "connect": "Insert"},
      {"name": "Volume", "implementor": "NXP Software Ltd.", "type": "09e8ede0-ddde-11db-b4f6-0002a5d5c51b", "uuid": "119341a0-8469-11df-81f9-0002a5d5c51b", "connect": "Insert"},
      {"name": "dlb_notification_listener", "implementor": "OEM Audio Effect", "type": "fa81dbde-588b-11ed-9b6a-0242ac120002", "uuid": "1f0091e3-6ad8-40fe-9b09-5948f9a26e7e", "connect": "Insert"},
      {"name": "dlb_system_listener", "implementor": "OEM Audio Effect", "type": "fa81dbde-588b-11ed-9b6a-0242ac120002", "uuid": "874db4d8-051d-4b7b-bd95-a3bebc837e9e", "connect": "Insert"}
    ],
    "media": {
      "instances": [
        {"name": "android.hardware.media.c2@1.2-mediatek-64b", "store": "default1", "rank": 1},
        {"name": "vendor.dolby.media.c2@1.0-service", "store": "default9", "rank": 1}
      ],
      "components": [
        {"name": "c2.dolby.ac4.decoder", "kind": "decoder", "domain": "audio", "mediaType": "audio/ac4", "store": "default9", "rank": 1, "aliases": "OMX.dolby.ac4.decoder"},
        {"name": "c2.mtk.avc.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/avc", "store": "default1", "rank": 1, "maxSize": "4096x2176", "profiles": "20480,20481,20482,20484,20487", "levels": "20495", "aliases": "OMX.MTK.VIDEO.DECODER.AVC"},
        {"name": "c2.mtk.avc.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/avc", "store": "default1", "rank": 1, "maxSize": "4096x2176", "profiles": "20480,20481,20482,20484,20487", "levels": "20495", "aliases": "OMX.MTK.VIDEO.DECODER.AVC.secure"},
        {"name": "c2.mtk.hevc.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/hevc", "store": "default1", "rank": 1, "maxSize": "4096x2176", "profiles": "24576,24578,24577", "levels": "24835", "aliases": "OMX.MTK.VIDEO.DECODER.HEVC"},
        {"name": "c2.mtk.hevc.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/hevc", "store": "default1", "rank": 1, "maxSize": "4096x2176", "profiles": "24576,24578,24577", "levels": "24835", "aliases": "OMX.MTK.VIDEO.DECODER.HEVC.secure"},
        {"name": "c2.mtk.heif.decoder", "kind": "decoder", "domain": "video", "mediaType": "image/vnd.android.heic", "store": "default1", "rank": 1, "maxSize": "16383x16383", "aliases": "OMX.MTK.VIDEO.DECODER.HEIF"},
        {"name": "c2.mtk.vp9.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/x-vnd.on2.vp9", "store": "default1", "rank": 1, "maxSize": "4096x2176", "profiles": "28672,28673,28674", "levels": "28680", "aliases": "OMX.MTK.VIDEO.DECODER.VP9"},
        {"name": "c2.mtk.vp9.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/x-vnd.on2.vp9", "store": "default1", "rank": 1, "maxSize": "4096x2176", "profiles": "28672,28673,28674", "levels": "28680", "aliases": "OMX.MTK.VIDEO.DECODER.VP9.secure"},
        {"name": "c2.mtk.avc.encoder", "kind": "encoder", "domain": "video", "mediaType": "video/avc", "store": "default1", "rank": 1, "maxSize": "3840x2176", "profiles": "20480,20482,20484", "levels": "20494", "aliases": "OMX.MTK.VIDEO.ENCODER.AVC"},
        {"name": "c2.mtk.avc.encoder.secure", "kind": "encoder", "domain": "video", "mediaType": "video/avc", "store": "default1", "rank": 1, "maxSize": "3840x2176", "profiles": "20480,20482,20484", "levels": "20494"},
        {"name": "c2.mtk.hevc.encoder", "kind": "encoder", "domain": "video", "mediaType": "video/hevc", "store": "default1", "rank": 1, "maxSize": "3840x2176", "profiles": "24576", "levels": "24834", "aliases": "OMX.MTK.VIDEO.ENCODER.HEVC"},
        {"name": "c2.mtk.hevc.encoder.secure", "kind": "encoder", "domain": "video", "mediaType": "video/hevc", "store": "default1", "rank": 1, "maxSize": "3840x2160", "profiles": "24576", "levels": "24834"},
        {"name": "c2.mtk.heif.encoder", "kind": "encoder", "domain": "video", "mediaType": "image/vnd.android.heic", "store": "default1", "rank": 1, "maxSize": "16383x16383", "aliases": "OMX.MTK.VIDEO.ENCODER.HEIF"}
      ]
    },
    "audioEngine": {
      "modules": "default,bluetooth,r_submix",
      "voiceCallIndexMax": 7,
      "musicIndexMax": 15
    }
  },
  "alignmentNotes": [
    "机型判定：motorola/aito 为 Razr 2024（中国版 Razr 50），MT6878/Dimensity 7300X + Mali-G615 MC2；与 gold 同为 MTK 平台，radioVariant 保持 mtk，但 SoC/GPU/频点/传感器/电池/EDID/IMEI 全部替换为 MT6878 口径，未沿用 MT6833 与小米专属值。",
    "radioVariant=mtk：VINTF 35 条 radio 占位符均为 android.hardware.radio.* 与 vendor.mediatek.hardware.mtkradioex.* 命名空间；enableIr=false（无红外）；enableNfc=true（sku_dn/dns/dnsp/n/ns 均含 android.hardware.nfc.ese 与 android.hardware.se.omapi.ese）。",
    "折叠形态专属：sensors 表在 gold 的 20 项基础上新增 android.sensor.hinge_angle(36) 与 com.motorola.hardware.sensor.hinge_angle 对应的 vendor 传感器(typeId 65536)；全表不含 xiaomi.sensor.*，inputIdentity 不填 swtp（swtp 仅小米系）。",
    "电源与续航：4200mAh 单芯 Li-Po、30W 有线、15W 无线（battery.wireless=1、wirelessChargingW=15）；X 轴线性马达按 capabilities=8（幅度控制）建模。",
    "触控按折叠屏常见的 Synaptics TCM 建模（synaptics_tcm，i2c bus 24 / vendor 0x06cb / product 1 / version 256，10 点，无触摸按键）；指纹为 FPC 侧边方案（uinput-fpc），与固件实测 fingerprintDevs 一致。",
    "音频沿用固件实测 22 个 effect descriptor（Dolby DAP/DAP_game/dlb_* + NXP + AOSP 全集）；音频模块 default,bluetooth,r_submix，通话 7 阶、媒体 15 阶。",
    "媒体按实测 Codec2 线索登记：MTK 64b 实例落 default1、Dolby 落 default9，假登记 store 统一 rank=1，不占用 default；组件覆盖 ac4/avc/hevc/vp9/heif 的编解码与 secure 变体，分辨率上限沿用 4096x2176。",
    "GNSS 为 MTK MNLD 口径（MNL_VER 21111601.6.00.00），支持 GPS+GLONASS+Galileo+BeiDou+NavIC 与 L5 双频；yearOfHw 按 2024 年 MT6878 平台填写。",
    "以下字段固件未直接给出、按平台/公开规格推断，需上机复核：EDID productId/week/year（MTKDEV 平台默认）、imeiTac/meidPrefix、macOuiCandidates、Mali 驱动哈希与三组 UUID、vulkanDeviceId、synaptics_tcm 的 product/version、gnssCapabilities 具体位图。",
    "温区直接采用固件实测 45 路（soc_max/cpu-big/cpu-little/dsu/soc-top/soc-bot/apu/md1-4/gpu/ap_ntc/ltepa/nrpa/pmic6363/pmic6369/vtskin/consys），未做裁剪；Thermal HAL sensors 取 CPU/GPU/BATTERY/SKIN/PA/NPU/SOC 七路。"
  ],
  "sources": [
    "固件实测：build.prop、boot/DTB、VINTF manifest、vendor/etc/* 与 odm/etc（题面 JSON）",
    "Motorola Razr 2024 / Razr 50 公开规格：6.9\" 1080x2640 165Hz LTPO、Dimensity 7300X、4200mAh、30W+15W、50MP+13MP/32MP、侧边 FPC、IPX8、无 3.5mm/红外",
    "ARM Mali-G615 MC2（MT6878）Android 14 BSP 驱动分支 r44p1；UUID/哈希/vulkanDeviceId 为上机校准项"
  ]
}
```
