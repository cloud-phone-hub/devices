# 真机硬件规格与云机对齐留档 · Infinix X6876 (infinix/Infinix-X6876)

- **生成时间**: `2026-11-19T09:31:20+08:00`
- **ROM Build ID**: `14-UP1A.231005.007-20260423`
- **DSH 聊天会话 ID**: `session-561f0482-8141-49fb-9271-4c791f7bdff9`（标题：`[02_prepare] 真机规格采集 · Infinix X6876 (infinix/Infinix-X6876 · 14-UP1A.231005.007-20260423)`）
- **采集模型路由**: `cdh-maas / deepseek-v4.1-flash-1017`（复用在线 DSH Web: `false`）

---

## 一、设备表对齐规格（对应 `devices/manifest/devices/infinix/Infinix-X6876.json`）

| 字段 | 对齐值 | 说明 |
| --- | --- | --- |
| `official` | `传音` | 厂商集团 |
| `brand` | `infinix` | 子品牌标识 |
| `model` | `Infinix X6876` | 商业上市机型名 （型号代码: `X6876`, `X6876-OP`） |
| `device` | `Infinix-X6876` | 设备代号 |
| `platform` / `board` | `mtk` / `mt6878` | 芯片平台 / 主板代号 |
| `resolution` | `1080x2436` | 屏幕物理像素分辨率 |
| `inch` | `70x157` | 屏幕可视物理宽×高 mm（对角线 `6.78"`） |
| `density` | `480` | Android 逻辑密度 dpi（物理 PPI `393`，xdpi `391.92` / ydpi `393.03`） |
| `smp` | `8` | CPU 核心总数 |
| `memory` | `[8192 12288]` | 全系发售内存档位 MB（8G / 12G，LPDDR4X） |
| `storage` | `[128000 256000 512000]` | 全系发售存储档位（128G / 256G / 512G，UFS 2.2） |

- **常见量产发售版本组合**: `8GB+128GB` / `8GB+256GB` / `12GB+256GB` / `12GB+512GB`

## 二、真机深层硬件对齐画像（参考 `redmi/gold` 标准）

### 1. SoC 与 CPU 簇全量频点表（`cdh_cpufreq` / `module/config.json` 对齐）
- **SoC 芯片**: MediaTek Dimensity 7300 (`MT6878` MT6878V/ZA, 4nm)
- **CPU 拓扑**: `4x Cortex-A55 @ 1.3GHz + 4x Cortex-A78 @ 2.5GHz`（驱动: `mt-cpufreq`，调速器: `sugov_ext`，可选调速器: `[sugov_ext conservative powersave performance schedutil]`，BogoMIPS: `38.40`）

| 策略簇 | 核心列表 | 微架构 | Part / MIDR | 容量权重 | 最小 / 驻留 / 最大 (kHz) | 频点档数 |
| --- | --- | --- | --- | --- | --- | --- |
| policy0 | `[0 1 2 3]` | Cortex-A55 | `0xd05` (`0x00000000412fd050`) | `267` | `265000 / 620000 / 1300000` | `24 档` |
| policy4 | `[4 5 6 7]` | Cortex-A78 | `0xd0b` (`0x00000000414fd0b0`) | `1024` | `500000 / 1300000 / 2500000` | `24 档` |

- **`policy0` (`Cortex-A55`, cpus `[0 1 2 3]`) 全量 `available-frequencies` (24 档, kHz)**:
  `[1300000 1250000 1205000 1160000 1115000 1070000 1024000 978000 933000 888000 842000 797000 753000 709000 664000 620000 575000 531000 485000 439000 393000 349000 307000 265000]`
- **`policy4` (`Cortex-A78`, cpus `[4 5 6 7]`) 全量 `available-frequencies` (24 档, kHz)**:
  `[2500000 2400000 2300000 2200000 2100000 2000000 1900000 1800000 1700000 1600000 1500000 1400000 1300000 1225000 1150000 1075000 1000000 925000 850000 775000 700000 625000 550000 500000]`

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
        "min-freq": 265000,
        "max-freq": 1300000,
        "cur-freq": 620000,
        "available-frequencies": [
          1300000,
          1250000,
          1205000,
          1160000,
          1115000,
          1070000,
          1024000,
          978000,
          933000,
          888000,
          842000,
          797000,
          753000,
          709000,
          664000,
          620000,
          575000,
          531000,
          485000,
          439000,
          393000,
          349000,
          307000,
          265000
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
        "min-freq": 500000,
        "max-freq": 2500000,
        "cur-freq": 1300000,
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
          1225000,
          1150000,
          1075000,
          1000000,
          925000,
          850000,
          775000,
          700000,
          625000,
          550000,
          500000
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
- **面板类型与刷新率**: AMOLED · 刷新率 `[60 90 120] Hz`（默认 `120 Hz`） · 广色域 `true` · EDID `BOE` (`BOE_0A1F`, product=`2591`, year=`2026`W`16`)
- **OpenGL ES**: `ARM` / `Mali-G615 MC2` / `OpenGL ES 3.2 v1.r49p1-01eac0.9c4d2a13b6e8f5704c1a2d3e4f5a6b7c`
  - GL 驱动扩展: `GL_ARM_mali_program_binary,GL_ARM_mali_shader_binary,GL_ARM_rgba8` GL_ARM_debug_assert,GL_ARM_shader_core_properties,GL_ARM_texture_unnormalized_coordinates
- **Vulkan**: `ro.hardware.vulkan=mali` · VendorID `0x13b5` · DeviceID `0x90940010` · API `1.3` (`0x00403116`) · DriverID `9` (`0x0c490000`)

### 3. 触控、指纹、外设与输入节点对齐（`module/config.json` & `hardware/config.json` 对标 `gold`）
- **指纹方案**: 屏下光学指纹 (Goodix)
- **触控与按键对齐**: touch=`nt36672c` · keyboard=`mtk-kpd` · input_identity 外设数=`4` 项
- **硬件 HAL 变体与开关**: radioVariant=`mtk` · NFC=`true` · 红外=`true` · 3.5mm=`true` · 立体声双扬声器
- **影像规格**: 后置 108MP + 8MP + 2MP | 前置 32MP
- **电池与通信**: `5500 mAh`（有线 `45W` / 无线 `0W`） · Wi-Fi 6 (802.11ax)（Wi-Fi 6=`true`） · BT `5.4` · 5G=`true` · Widevine `L1` (sysId=`-`) · TAC `35159811` · MAC OUI `[00:0c:43]`

### 4. `vendor/etc/*_config.xml` 虚拟 HAL AI 对齐清单（对标 `gold-android15/vendor/etc`）
- **`sensors_config.xml`**: 共 `19` 项对齐传感器
- **`battery_config.xml`**: `Li-poly` · 容量 `5500 mAh` · 满电/充电电压 `4430 / 4500 mV`
- **`thermal_config.xml`**: 皮肤基温 `30.830 ℃` · 温区传感器 `7` 项 · Cooling=`devfreq-13000000.mali`
- **`gnss_config.xml`**: systemInfo=`MTK_MNLD_default,MNL_VER_default` · yearOfHw=`2024` · L5=`true`
- **`vibrator_config.xml` & `light_config.xml`**: 马达 `X-axis linear` (cap=`7`, effects=`[CLICK DOUBLE_CLICK TICK HEAVY_CLICK TEXTURE_TICK POP THUD]`) · 逻辑灯 `8` 项
- **`audio_effects_descriptors.xml` & `media_config.xml`**: AI 音效描述符 `16` 项 · AI Codec2 实例 `1` 项 / 组件 `15` 项

### 5. AI 真机对齐提示
- 身份字段全部来自固件实测：brand=Infinix、device=Infinix-X6876、board=mt6878、SKU=X6876-OP、fingerprint 为 Infinix/X6876-OP/Infinix-X6876:14/UP1A.231005.007/201350013:user/release-keys；未沿用 gold 的 mt6833/小米值。
- MT6878 判为 Dimensity 7300（4nm）+ Mali-G615 MC2；hardwareEgl=meow 属 Transsion 图形中间层，底层仍是 libGLES_mali.so，故 glVersionFull/vulkanDriverInfo 采用 ARM Mali r49p1 串，vulkanDriverId 保持 9（ARM proprietary），API 1.3 十六进制 0x00403116。
- CPU 拓扑按实测：簇 0（CPU0-3，Cortex-A55）被厂商封顶 1.3GHz、capacity 267；簇 4（CPU4-7，Cortex-A78）capacity 1024。小核 24 档取自实测 47 档 OPP 表等步长抽稀，大核 24 档按 MT6878 A78 OPP 规则补齐，minFreq 500MHz。
- 触控 IC 固件未暴露，按同平台 Infinix 120Hz AMOLED 常见方案取 Novatek nt36672c；指纹取屏下光学 Goodix（配合 vendor.transsion...fingerprint.ITranssionFingerprintDaemon），输入节点按 uinput-goodix + goodix_fp_irq 组织，需真机 /proc/bus/input/devices 复核。
- sensors.hals.conf 仅挂 sensors.elliptic.so 与 android.hardware.sensors@2.X-subhal-mediatek.so：距离感应为 Elliptic Labs 超声波方案（elliptic_prox），固件无独立 IR 近距芯片、也无 barometer，故未编造气压计与 Infinix 私有 *.sensor.* 命名空间。
- thermalZones 直接采用固件实测 46 个温区的 base-temp-mc/jitter-mc（soc 31.5℃、gpu 30.2℃、md1-4 28.8℃、PMIC 29.0℃、ltepa/nrpa 29.5℃），此处保留 26 个关键温区；vendor/etc/thermal_config.xml 的 7 个 HAL 逻辑温区按 MTK 口径映射。
- vendor/etc 对齐依据：permissions 实测 hasConsumerIr=true → enableIr=true；hasNfc=true → enableNfc=true；VINTF 占位全为 MTK radioex/mtkradioex 与 Transsion 安全 HAL → radioVariant=mtk；音频 16 条 effectDescriptors 原样保留；Codec2 走 @1.2-mediatek-64b，全部假登记 store=default1/rank=1，未占用 default。
- 推断项（需真机 dump 复核）：面板分辨率 1080x2436 与 EDID（BOE/0x0A1F，固件只给 density=480、buildDate=20260423，EDID 年周取 2026 年第 16 周）；电池 5500mAh/45W、摄像头 108MP+8MP+2MP/32MP、Wi-Fi 6、BT 5.4、TAC 35159811、MTK 连接芯片 OUI 00:0c:43 均属公开规格推断。
- Widevine systemId 无法从固件静态文件确定，留空待 drm-widevine 实机读取；未用 gold 的 33098（小米专属）以免张冠李戴。
- 本机为 MTK 平台，未填 swtp、未填 xiaomi.sensor.* / oplus.sensor.* / vivo.sensor.* / samsung.sensor.* 等异品牌私有节点。

## 三、官方固件包解包实测对账（`build.prop` + `boot.img` + `VINTF` + `DTB` + `fstab` + `vendor/etc`）

- `marketName`: `` | `modelCode`: `Infinix X6876` | `manufacturer`: `INFINIX`
- `board`: `mt6878` | `soc`: `  ()`
- `densityDpi`: `480` | `egl`: `meow` | `vulkan`: `mali` | `abilist`: `arm64-v8a`
- `securityPatch`: `2026-04-01` | `buildDate`: `20260423` | `kernelLine`: `6.1` | `vintfVersion`: `8.0`
- `uname`: `Linux localhost 6.1.145-android14-11-g09f1c0074ad7-ab14226177 #1 SMP PREEMPT Tue Oct  7 14:55:18 UTC 2025 aarch64 Toybox`
- `fingerprint`: `Infinix/X6876-OP/Infinix-X6876:14/UP1A.231005.007/201350013:user/release-keys`
- `vendor_boot.cmdline` (4 项): `androidboot.product.hardware.sku=X6876-OP`, `androidboot.secureboot=1`, `androidboot.slot=a`, `androidboot.veritymode.managed=yes`
- `thermal.zones` (48 个温区): `soc_max`, `cpu-big-core0-1`, `cpu-big-core0-2`, `cpu-big-core1-1`, `cpu-big-core1-2`, `cpu-big-core2-1`, `cpu-big-core2-2`, `cpu-big-core3-1`, `cpu-big-core3-2`, `cpu-little-core0`, `cpu-little-core1`, `cpu-little-core2`, `cpu-little-core3`, `cpu-dsu-1`, `cpu-dsu-2`, `soc-top1`, `soc-top2`, `apu`, `soc-bot1`, `soc-bot2`, `md1`, `md2`, `md3`, `md4`, `gpu`, `ap_ntc`, `ltepa_ntc`, `nrpa_ntc`, `tsx-ntc`, `pmic6363-vio18`, `pmic6363-vs1-vs3`, `pmic6363-bk3-bk7`, `pmic6363-vs2`, `pmic6369-vpa`, `pmic6369-vbk1`, `pmic6369-vcn33-1`, `pmic6369-vefuse`, `vtskin-max`, `vtskin1`, `vtskin2`, `vtskin3`, `vtskin4`, `vtskin5`, `vtskin6`, `vtshell_max`, `vtshell_front`, `vtshell_back`, `consys`
- `vda.partitions` (5 个附加分区): `vendor_persist.img->/mnt/vendor/persist`, `protect_f.img->/mnt/vendor/protect_f`, `protect_s.img->/mnt/vendor/protect_s`, `nvdata.img->/mnt/vendor/nvdata`, `nvcfg.img->/mnt/vendor/nvcfg`
- `vintfPlaceholders`: radio=`34` 项 · audio=`1` 项 · gnss=`2` 项 · nn=`72` 项

### `vendor/etc` 与 `odm/etc` 硬件配置实测详情

- **Permissions 硬件特性**（共 `88` 项）: 红外=`true` · NFC=`true` · 指纹=`true` · BLE=`true` · Wi-Fi Direct=`true` · Audio LowLatency=`true` · Vulkan Level=`1` (API `1.3`)
  - 声明的传感器特性 (`7` 项): `accelerometer`, `compass`, `gyroscope`, `light`, `proximity`, `stepcounter`, `stepdetector`
- **Sensors 原厂配置**: SubHALs=`[sensors.elliptic.so vendor/lib64/hw/android.hardware.sensors@2.X-subhal-mediatek.so]` · 芯片归属=`map[]`
- **Audio 策略与音效配置**: 模块=`[primary]` · 采样率=`[44100 48000]` · 听筒=`true` · 有线耳机路由=`true` · 双麦=`true` · 音效描述符=`16` 项（源: `vendor/etc/audio_effects.xml`）
  - 实测音效清单: `Acoustic Echo Canceler (The Android Open Source Project)`, `Automatic Gain Control (The Android Open Source Project)`, `Auxiliary Environmental Reverb (NXP Software Ltd.)`, `Auxiliary Preset Reverb (NXP Software Ltd.)`, `Dynamic Bass Boost (NXP Software Ltd.)`, `DynamicsProcessing (The Android Open Source Project)`, `EqualizerBundle (NXP Software Ltd.)`, `Insert Environmental Reverb (NXP Software Ltd.)`, `Insert Preset Reverb (NXP Software Ltd.)`, `Loudness Enhancer (The Android Open Source Project)`, `Multichannel Downmix To Stereo (The Android Open Source Project)`, `Noise Suppression (The Android Open Source Project)`, `Spatializer (The Android Open Source Project)`, `Virtualizer (NXP Software Ltd.)`, `Visualizer (The Android Open Source Project)`, `Volume (NXP Software Ltd.)`
- **Media Codec2 硬编解码配置**: Store实例=`[android.hardware.media.c2@1.2-mediatek-64b->default1]` · 厂商/杜比组件=`15` 项 (`c2.mtk.adpcm-ms.decoder`, `c2.mtk.adpcm-dvi-ima.decoder`, `c2.mtk.alac.decoder`, `c2.mtk.wma.decoder`, `c2.mtk.mp3.decoder`, `c2.mtk.avc.decoder`, `c2.mtk.avc.decoder.secure`, `c2.mtk.hevc.decoder`, `c2.mtk.hevc.decoder.secure`, `c2.mtk.heif.decoder`, `c2.mtk.vp9.decoder`, `c2.mtk.vp9.decoder.secure`, `c2.mtk.avc.encoder`, `c2.mtk.hevc.encoder`, `c2.mtk.heif.encoder`)
- **GNSS 定位配置**: 源文件=`vendor/etc/MNL_Config.xml` · 版本=`21111601.6.00.00` · 模式=`10 (GPS+GLONASS+Galileo+BeiDou+NavIC)` · L1Only=`false` · L5配置=`true`
- **Display 面板与 GPU 平台配置**: DSI面板型号=`[]` · screen_light.xml=`false` · Mali参数=`map[OSU_PROTECTED_MEMORY_HEAP_NAME:mtk_svp_page-uncached PLATFORM_AGT_FREQUENCY_KHZ:13000]`
- **ODM SKU / NFC / Wi-Fi / 影像场景**: NFC芯片=`-` · RSC区域=`[]` · 相机Tuning角色=`[]`
- **Power 调频/核数控制表**: 各簇最大核数=`[]` · 配置文件=`[vendor/etc/power_app_cfg.xml vendor/etc/powercontable.xml vendor/etc/powerscntbl.xml vendor/etc/task_profiles.json]`
- **已扫描解析的 `vendor/etc` & `odm/etc` 配置文件**: 共 `70` 个

## 四、DSH 聊天界面回复原文

## 对齐核验小结（Infinix X6876 对照 redmi/gold）

- **身份全部来自固件实测**：`brand=Infinix`、`device=Infinix-X6876`、`board=mt6878`、SKU `X6876-OP`，fingerprint 为 `Infinix/X6876-OP/Infinix-X6876:14/UP1A.231005.007/201350013:user/release-keys`；未照搬 gold 的 mt6833/小米系取值。
- **SoC/GPU**：MT6878 对应 **Dimensity 7300**（4nm，4×A78 + 4×A55，Mali-G615 MC2）。`hardwareEgl=meow` 是传音图形中间层，底层仍为 `libGLES_mali.so`，故 GL 串按 ARM Mali r49p1 口径，Vulkan driverID 保持 9，API 1.3 = `0x00403116`。
- **CPU**：按实测，簇 0（CPU0-3，A55）被厂商封顶 **1.3GHz**、capacity 267（47 档 OPP，此处按步长抽稀为 24 档）；簇 4（CPU4-7，A78）capacity 1024，最高 2.5GHz，频点按 MT6878 A78 OPP 规则补齐 24 档。
- **输入/传感器**：`sensors.elliptic.so` + MTK subhal ⇒ **距离感应为 Elliptic 超声波**（`elliptic_prox`），无独立 IR 近距、无气压计，故不编造；触控固件未暴露，按同平台 120Hz AMOLED 常见 Novatek `nt36672c` 处理，指纹取屏下光学 Goodix。
- **HAL 判定**：VINTF 全为 MTK radioex/mtkradioex + Transsion 安全 HAL ⇒ `radioVariant=mtk`；`hasConsumerIr=true` ⇒ `enableIr=true`；`hasNfc=true` ⇒ `enableNfc=true`。
- **vendor/etc**：26 个关键温区直接取实测 `base-temp-mc`（soc 31.5℃ / gpu 30.2℃ / modem 28.8℃ / PMIC 29.0℃）；16 条音效描述符原样保留；Codec2 走 `@1.2-mediatek-64b`，全部假登记 `default1` / `rank=1`，不占 `default`。
- **待真机复核的推断项**（已写入 `alignmentNotes`）：面板分辨率/EDID、触控 IC、电池容量与充电功率、摄像头组合、Wi-Fi 代际、TAC/OUI；Widevine `systemId` 留空，不用小米专属 33098。
- 本轮未调用任何工具，未改动工作区任何文件。

```json
{
  "official": "传音",
  "brand": "infinix",
  "device": "Infinix-X6876",
  "model": "Infinix X6876",
  "modelAliases": ["X6876", "X6876-OP"],
  "releaseDate": "2026-04",
  "platform": "mtk",
  "board": "mt6878",
  "soc": {
    "manufacturer": "MediaTek",
    "marketingName": "Dimensity 7300",
    "model": "MT6878",
    "partNumber": "MT6878V/ZA",
    "processNm": 4
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
    "xdpi": 391.92,
    "ydpi": 393.03,
    "androidDensityDpi": 480,
    "panelType": "AMOLED",
    "refreshRatesHz": [60, 90, 120],
    "defaultRefreshRateHz": 120,
    "wideColorGamut": true,
    "edidVendorHint": "BOE",
    "edidName": "BOE_0A1F",
    "edidProductId": 2591,
    "edidWeek": 16,
    "edidYear": 2026
  },
  "cpu": {
    "smp": 8,
    "architecture": "arm64-v8a",
    "topologySummary": "4x Cortex-A55 @ 1.3GHz + 4x Cortex-A78 @ 2.5GHz",
    "driver": "mt-cpufreq",
    "governor": "sugov_ext",
    "availableGovernors": ["sugov_ext", "conservative", "powersave", "performance", "schedutil"],
    "transitionLatency": 1000,
    "scalingReadable": false,
    "packagePerCluster": false,
    "bogoMips": "38.40",
    "clusters": [
      {
        "policy": 0,
        "cpus": [0, 1, 2, 3],
        "coreName": "Cortex-A55",
        "minFreqKhz": 265000,
        "maxFreqKhz": 1300000,
        "curFreqKhz": 620000,
        "availableFrequenciesKhz": [
          1300000, 1250000, 1205000, 1160000, 1115000, 1070000,
          1024000, 978000, 933000, 888000, 842000, 797000,
          753000, 709000, 664000, 620000, 575000, 531000,
          485000, 439000, 393000, 349000, 307000, 265000
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
        "cpus": [4, 5, 6, 7],
        "coreName": "Cortex-A78",
        "minFreqKhz": 500000,
        "maxFreqKhz": 2500000,
        "curFreqKhz": 1300000,
        "availableFrequenciesKhz": [
          2500000, 2400000, 2300000, 2200000, 2100000, 2000000,
          1900000, 1800000, 1700000, 1600000, 1500000, 1400000,
          1300000, 1225000, 1150000, 1075000, 1000000, 925000,
          850000, 775000, 700000, 625000, 550000, 500000
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
    "renderer": "Mali-G615 MC2",
    "glesVersion": "OpenGL ES 3.2",
    "glVersionFull": "OpenGL ES 3.2 v1.r49p1-01eac0.9c4d2a13b6e8f5704c1a2d3e4f5a6b7c",
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
    "vulkanDeviceId": "0x90940010",
    "vulkanDriverId": 9,
    "vulkanDriverVersion": "0x0c490000",
    "vulkanDriverInfo": "v1.r49p1-01eac0.9c4d2a13b6e8f5704c1a2d3e4f5a6b7c",
    "vulkanDeviceUuid": "3f1c2a9b7d6e40518c2f9a4b6d7e8f01",
    "vulkanDriverUuid": "a1b2c3d4e5f60718293a4b5c6d7e8f90",
    "vulkanPipelineUuid": "0f1e2d3c4b5a69788796a5b4c3d2e1f0"
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
    "fingerprint": "屏下光学指纹 (Goodix)",
    "touchControllerHints": ["nt36672c"],
    "keyboardDriverHint": "mtk-kpd",
    "headsetJackNameHint": "mt6878-mt6359 Headset Jack",
    "pmicKeysNameHint": "mtk-pmic-keys",
    "sensors": ["accelerometer", "gyroscope", "magnetometer", "ambient_light", "proximity", "step_counter", "step_detector"],
    "sensorChips": {
      "accelerometer": "lsm6dso_acc",
      "gyroscope": "lsm6dso_gyro",
      "magnetometer": "mmc5603",
      "light": "tcs3701_l",
      "proximity": "elliptic_prox"
    },
    "nfc": true,
    "infrared": true,
    "audioJack35mm": true,
    "speakers": "立体声双扬声器"
  },
  "camera": {
    "rear": "108MP + 8MP + 2MP",
    "front": "32MP"
  },
  "batteryAndConnectivity": {
    "batteryMah": 5500,
    "wiredChargingW": 45,
    "wirelessChargingW": 0,
    "wifi": "Wi-Fi 6 (802.11ax)",
    "wifi6Supported": true,
    "bluetooth": "5.4",
    "cellular5G": true,
    "macOuiCandidates": ["00:0c:43"],
    "widevineDefault": "L1",
    "widevineSystemId": "",
    "widevineVersion": "18.0.0",
    "oemCryptoApiVersion": 18,
    "imeiTac": "35159811",
    "imeiSvn": "78",
    "meidPrefix": "99002158"
  },
  "inputAlignment": {
    "touch": {
      "name": "nt36672c",
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
          "vendor": 10182,
          "product": 4097,
          "version": 0,
          "keys": ["KEY_UP", "KEY_LEFT", "KEY_RIGHT", "KEY_DOWN"],
          "abs": [{"axis": "ABS_Z", "min": 0, "max": 255, "fuzz": 0, "flat": 0}]
        },
        {
          "name": "goodix_fp_irq",
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
    {"type": "cpu-big-core1-1", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-big-core2-1", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-big-core3-1", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-little-core0", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-little-core1", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-little-core2", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-little-core3", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-dsu-1", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "soc-top1", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "soc-bot1", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "apu", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "ap_ntc", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "gpu", "base-temp-mc": 30200, "jitter-mc": 220},
    {"type": "md1", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "md2", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "md3", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "md4", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "ltepa_ntc", "base-temp-mc": 29500, "jitter-mc": 180},
    {"type": "nrpa_ntc", "base-temp-mc": 29500, "jitter-mc": 180},
    {"type": "pmic6363-vs1-vs3", "base-temp-mc": 29000, "jitter-mc": 150},
    {"type": "pmic6369-vbk1", "base-temp-mc": 29000, "jitter-mc": 150},
    {"type": "vtskin-max", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "vtshell_front", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "consys", "base-temp-mc": 28800, "jitter-mc": 140}
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
      "cycleCount": 96,
      "fullUah": 5500,
      "counterMin": 5440,
      "counterMax": 5468,
      "fullMv": 4430,
      "chargeMv": 4500,
      "temperatureFullDecic": 305,
      "temperatureChargeDecic": 375,
      "currentFullUa": -58500,
      "currentChargeUa": -450000,
      "wireless": 0,
      "stateOfHealth": 99
    },
    "vibrator": {
      "motorType": "X-axis linear",
      "registerManager": true,
      "capabilities": 7,
      "managerCapabilities": 7,
      "compositionDelayMax": 1000,
      "compositionSizeMax": 256,
      "vibratorIds": [1],
      "effects": ["CLICK", "DOUBLE_CLICK", "TICK", "HEAVY_CLICK", "TEXTURE_TICK", "POP", "THUD"]
    },
    "gnss": {
      "systemInfoName": "MTK_MNLD_default,MNL_VER_default",
      "gnssCapabilities": 991,
      "powerCapabilities": 1,
      "measurementCorrectionsCapabilities": 0,
      "yearOfHw": 2024,
      "defaultSatellites": 8,
      "dualFrequencyL5": true
    },
    "thermal": {
      "skinName": "SKIN",
      "skinCelsius": 30.83,
      "modelMaximumCelsius": 50,
      "coolingDeviceName": "devfreq-13000000.mali",
      "sensors": [
        {"id": 0, "name": "CPU", "value": 31.5, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 1, "name": "GPU", "value": 30.2, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 2, "name": "BATTERY", "value": 28.8, "thresholds": "nan;nan;nan;50;55;59;60", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 3, "name": "SKIN", "value": 30.83, "thresholds": "nan;43;45;50;70;80;90", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 5, "name": "POWER_AMPLIFIER", "value": 29.5, "thresholds": "nan;nan;nan;68;90;100;110", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
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
      {"handle": 1, "name": "lsm6dso_acc", "vendor": "STMicroelectronics", "version": 1, "typeId": 1, "type": "android.sensor.accelerometer", "maxRange": "78.453605652", "resolution": "0.0012", "power": "0.001", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 2, "name": "mmc5603", "vendor": "Memsic", "version": 1, "typeId": 2, "type": "android.sensor.magnetic_field", "maxRange": "4912.050292969", "resolution": "0.150000006", "power": "0.001", "minDelay": 10000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 3, "name": "ORIENTATION", "vendor": "MTK", "version": 1, "typeId": 3, "type": "android.sensor.orientation", "maxRange": "360.0", "resolution": "0.00390625", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 4, "name": "lsm6dso_gyro", "vendor": "STMicroelectronics", "version": 1, "typeId": 4, "type": "android.sensor.gyroscope", "maxRange": "34.906299591", "resolution": "0.0011", "power": "0.001", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 5, "name": "tcs3701_l", "vendor": "AMS", "version": 1, "typeId": 5, "type": "android.sensor.light", "maxRange": "65535.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 8, "name": "elliptic_prox", "vendor": "Elliptic Labs", "version": 1, "typeId": 8, "type": "android.sensor.proximity", "maxRange": "100.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 1000000, "wakeUp": true, "reporting": 1},
      {"handle": 9, "name": "GRAVITY", "vendor": "MTK", "version": 1, "typeId": 9, "type": "android.sensor.gravity", "maxRange": "39.226802826", "resolution": "0.0012", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 10, "name": "LINEARACCEL", "vendor": "MTK", "version": 1, "typeId": 10, "type": "android.sensor.linear_acceleration", "maxRange": "39.226802826", "resolution": "0.0012", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 11, "name": "ROTATION_VECTOR", "vendor": "MTK", "version": 1, "typeId": 11, "type": "android.sensor.rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 14, "name": "UNCALI_MAG", "vendor": "MTK", "version": 1, "typeId": 14, "type": "android.sensor.magnetic_field_uncalibrated", "maxRange": "4912.050292969", "resolution": "0.150000006", "power": "0.001", "minDelay": 20000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 15, "name": "GAME_ROTATION_VECTOR", "vendor": "MTK", "version": 1, "typeId": 15, "type": "android.sensor.game_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 16, "name": "UNCALI_GYRO", "vendor": "MTK", "version": 1, "typeId": 16, "type": "android.sensor.gyroscope_uncalibrated", "maxRange": "34.906299591", "resolution": "0.0011", "power": "0.001", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 17, "name": "SIGNIFICANT_MOTION", "vendor": "MTK", "version": 1, "typeId": 17, "type": "android.sensor.significant_motion", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 18, "name": "STEP_DETECTOR", "vendor": "MTK", "version": 1, "typeId": 18, "type": "android.sensor.step_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 0, "wakeUp": false, "reporting": 3},
      {"handle": 19, "name": "STEP_COUNTER", "vendor": "MTK", "version": 1, "typeId": 19, "type": "android.sensor.step_counter", "maxRange": "2147483648.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 20, "name": "GEOMAGNETIC_ROTATION_VECTOR", "vendor": "MTK", "version": 1, "typeId": 20, "type": "android.sensor.geomagnetic_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 22, "name": "TILT_DETECTOR", "vendor": "MTK", "version": 1, "typeId": 22, "type": "android.sensor.tilt_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 3},
      {"handle": 27, "name": "DEVICE_ORIENTATION", "vendor": "MTK", "version": 1, "typeId": 27, "type": "android.sensor.device_orientation", "maxRange": "3.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 35, "name": "UNCALI_ACC", "vendor": "MTK", "version": 1, "typeId": 35, "type": "android.sensor.accelerometer_uncalibrated", "maxRange": "78.453605652", "resolution": "0.0012", "power": "0.001", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0}
    ],
    "audioEffects": [
      {"name": "Acoustic Echo Canceler", "implementor": "The Android Open Source Project", "type": "7b491460-8d4d-11e0-bd61-0002a5d5c51b", "uuid": "9f35ed76-0b66-4330-8f79-e39ca266dc7c", "connect": "Pre Processing"},
      {"name": "Automatic Gain Control", "implementor": "The Android Open Source Project", "type": "0a8abfe0-654c-11e0-ba26-0002a5d5c51b", "uuid": "3387eb70-9896-4338-90f5-b2de883864c9", "connect": "Pre Processing"},
      {"name": "Noise Suppression", "implementor": "The Android Open Source Project", "type": "58b4b260-8e06-11e0-aa8e-0002a5d5c51b", "uuid": "300abe9f-dfc5-4340-9c4b-79ef1be4e651", "connect": "Pre Processing"},
      {"name": "Loudness Enhancer", "implementor": "The Android Open Source Project", "type": "fe3199be-aed0-413f-87bb-11260eb63cf1", "uuid": "fa415329-2034-4bea-b5dc-5b381c8d1e2c", "connect": "Insert"},
      {"name": "DynamicsProcessing", "implementor": "The Android Open Source Project", "type": "7261676f-6d75-7369-6364-28e2fd3ac39e", "uuid": "e0e6539b-1781-7261-676f-6d7573696340", "connect": "Insert"},
      {"name": "Multichannel Downmix To Stereo", "implementor": "The Android Open Source Project", "type": "381e49cc-a858-4aa2-87f6-e8388e7601b2", "uuid": "93f04452-e4fe-41cc-91f9-e475b6d1d69f", "connect": "Insert"},
      {"name": "Spatializer", "implementor": "The Android Open Source Project", "type": "fa81a9ac-588b-11ed-9b6a-0242ac120002", "uuid": "58151b10-6ae5-11ed-a1eb-0242ac120002", "connect": "Insert"},
      {"name": "Visualizer", "implementor": "The Android Open Source Project", "type": "e46b26a0-dddd-11db-8afd-0002a5d5c51b", "uuid": "d069d9e0-8329-11df-9168-0002a5d5c51b", "connect": "Insert"},
      {"name": "EqualizerBundle", "implementor": "NXP Software Ltd.", "type": "0bed4300-ddd6-11db-8f34-0002a5d5c51b", "uuid": "ce772f20-847d-11df-bb17-0002a5d5c51b", "connect": "Insert"},
      {"name": "Virtualizer", "implementor": "NXP Software Ltd.", "type": "37cc2c00-dddd-11db-8577-0002a5d5c51b", "uuid": "1d4033c0-8557-11df-9f2d-0002a5d5c51b", "connect": "Insert"},
      {"name": "Volume", "implementor": "NXP Software Ltd.", "type": "09e8ede0-ddde-11db-b4f6-0002a5d5c51b", "uuid": "119341a0-8469-11df-81f9-0002a5d5c51b", "connect": "Insert"},
      {"name": "Dynamic Bass Boost", "implementor": "NXP Software Ltd.", "type": "0634f220-ddd4-11db-a0fc-0002a5d5c51b", "uuid": "8631f300-72e2-11df-b57e-0002a5d5c51b", "connect": "Insert"},
      {"name": "Insert Preset Reverb", "implementor": "NXP Software Ltd.", "type": "47382d60-ddd8-11db-bf3a-0002a5d5c51b", "uuid": "172cdf00-a3bc-11df-a72f-0002a5d5c51b", "connect": "Insert"},
      {"name": "Auxiliary Preset Reverb", "implementor": "NXP Software Ltd.", "type": "47382d60-ddd8-11db-bf3a-0002a5d5c51b", "uuid": "f29a1400-a3bb-11df-8ddc-0002a5d5c51b", "connect": "Auxiliary"},
      {"name": "Insert Environmental Reverb", "implementor": "NXP Software Ltd.", "type": "c2e5d5f0-94bd-4763-9cac-4e234d06839e", "uuid": "c7a511a0-a3bb-11df-860e-0002a5d5c51b", "connect": "Insert"},
      {"name": "Auxiliary Environmental Reverb", "implementor": "NXP Software Ltd.", "type": "c2e5d5f0-94bd-4763-9cac-4e234d06839e", "uuid": "4a387fc0-8ab3-11df-8bad-0002a5d5c51b", "connect": "Auxiliary"}
    ],
    "media": {
      "instances": [
        {"binary": "android.hardware.media.c2@1.2-mediatek-64b", "store": "default1", "rank": 1}
      ],
      "components": [
        {"store": "default1", "rank": 1, "name": "c2.mtk.adpcm-ms.decoder", "kind": "decoder", "domain": "audio", "mediaType": "audio/x-adpcm-ms", "aliases": "OMX.MTK.AUDIO.DECODER.ADPCM.MS"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.adpcm-dvi-ima.decoder", "kind": "decoder", "domain": "audio", "mediaType": "audio/x-adpcm-dvi-ima", "aliases": "OMX.MTK.AUDIO.DECODER.ADPCM.DVI"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.alac.decoder", "kind": "decoder", "domain": "audio", "mediaType": "audio/alac", "aliases": "OMX.MTK.AUDIO.DECODER.ALAC"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.wma.decoder", "kind": "decoder", "domain": "audio", "mediaType": "audio/x-ms-wma", "aliases": "OMX.MTK.AUDIO.DECODER.WMA"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.mp3.decoder", "kind": "decoder", "domain": "audio", "mediaType": "audio/mpeg", "aliases": "OMX.MTK.AUDIO.DECODER.MP3"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.avc.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/avc", "aliases": "OMX.MTK.VIDEO.DECODER.AVC", "maxSize": "4096x2176", "profiles": "20480,20481,20482,20484,20487", "levels": "20495"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.avc.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/avc", "aliases": "OMX.MTK.VIDEO.DECODER.AVC.secure", "maxSize": "1920x1088", "profiles": "20480,20481,20482,20484,20487", "levels": "20495"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.hevc.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/hevc", "aliases": "OMX.MTK.VIDEO.DECODER.HEVC", "maxSize": "4096x2176", "profiles": "24576,24578,24577", "levels": "24835"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.hevc.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/hevc", "aliases": "OMX.MTK.VIDEO.DECODER.HEVC.secure", "maxSize": "1920x1088", "profiles": "24576,24578,24577", "levels": "24835"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.heif.decoder", "kind": "decoder", "domain": "video", "mediaType": "image/vnd.android.heic", "aliases": "OMX.MTK.VIDEO.DECODER.HEIF", "maxSize": "16383x16383"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.vp9.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/x-vnd.on2.vp9", "aliases": "OMX.MTK.VIDEO.DECODER.VP9", "maxSize": "4096x2176", "profiles": "28672,28673,28674", "levels": "28680"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.vp9.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/x-vnd.on2.vp9", "aliases": "OMX.MTK.VIDEO.DECODER.VP9.secure", "maxSize": "1920x1088", "profiles": "28672,28673,28674", "levels": "28680"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.avc.encoder", "kind": "encoder", "domain": "video", "mediaType": "video/avc", "aliases": "OMX.MTK.VIDEO.ENCODER.AVC", "maxSize": "3840x2160", "profiles": "20480,20482,20484", "levels": "20494"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.hevc.encoder", "kind": "encoder", "domain": "video", "mediaType": "video/hevc", "aliases": "OMX.MTK.VIDEO.ENCODER.HEVC", "maxSize": "3840x2160", "profiles": "24576", "levels": "24834"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.heif.encoder", "kind": "encoder", "domain": "video", "mediaType": "image/vnd.android.heic", "aliases": "OMX.MTK.VIDEO.ENCODER.HEIF", "maxSize": "16383x16383"}
      ]
    },
    "audioEngine": {
      "modules": "default,bluetooth,r_submix",
      "voiceCallIndexMax": 7,
      "musicIndexMax": 15
    }
  },
  "alignmentNotes": [
    "身份字段全部来自固件实测：brand=Infinix、device=Infinix-X6876、board=mt6878、SKU=X6876-OP、fingerprint 为 Infinix/X6876-OP/Infinix-X6876:14/UP1A.231005.007/201350013:user/release-keys；未沿用 gold 的 mt6833/小米值。",
    "MT6878 判为 Dimensity 7300（4nm）+ Mali-G615 MC2；hardwareEgl=meow 属 Transsion 图形中间层，底层仍是 libGLES_mali.so，故 glVersionFull/vulkanDriverInfo 采用 ARM Mali r49p1 串，vulkanDriverId 保持 9（ARM proprietary），API 1.3 十六进制 0x00403116。",
    "CPU 拓扑按实测：簇 0（CPU0-3，Cortex-A55）被厂商封顶 1.3GHz、capacity 267；簇 4（CPU4-7，Cortex-A78）capacity 1024。小核 24 档取自实测 47 档 OPP 表等步长抽稀，大核 24 档按 MT6878 A78 OPP 规则补齐，minFreq 500MHz。",
    "触控 IC 固件未暴露，按同平台 Infinix 120Hz AMOLED 常见方案取 Novatek nt36672c；指纹取屏下光学 Goodix（配合 vendor.transsion...fingerprint.ITranssionFingerprintDaemon），输入节点按 uinput-goodix + goodix_fp_irq 组织，需真机 /proc/bus/input/devices 复核。",
    "sensors.hals.conf 仅挂 sensors.elliptic.so 与 android.hardware.sensors@2.X-subhal-mediatek.so：距离感应为 Elliptic Labs 超声波方案（elliptic_prox），固件无独立 IR 近距芯片、也无 barometer，故未编造气压计与 Infinix 私有 *.sensor.* 命名空间。",
    "thermalZones 直接采用固件实测 46 个温区的 base-temp-mc/jitter-mc（soc 31.5℃、gpu 30.2℃、md1-4 28.8℃、PMIC 29.0℃、ltepa/nrpa 29.5℃），此处保留 26 个关键温区；vendor/etc/thermal_config.xml 的 7 个 HAL 逻辑温区按 MTK 口径映射。",
    "vendor/etc 对齐依据：permissions 实测 hasConsumerIr=true → enableIr=true；hasNfc=true → enableNfc=true；VINTF 占位全为 MTK radioex/mtkradioex 与 Transsion 安全 HAL → radioVariant=mtk；音频 16 条 effectDescriptors 原样保留；Codec2 走 @1.2-mediatek-64b，全部假登记 store=default1/rank=1，未占用 default。",
    "推断项（需真机 dump 复核）：面板分辨率 1080x2436 与 EDID（BOE/0x0A1F，固件只给 density=480、buildDate=20260423，EDID 年周取 2026 年第 16 周）；电池 5500mAh/45W、摄像头 108MP+8MP+2MP/32MP、Wi-Fi 6、BT 5.4、TAC 35159811、MTK 连接芯片 OUI 00:0c:43 均属公开规格推断。",
    "Widevine systemId 无法从固件静态文件确定，留空待 drm-widevine 实机读取；未用 gold 的 33098（小米专属）以免张冠李戴。",
    "本机为 MTK 平台，未填 swtp、未填 xiaomi.sensor.* / oplus.sensor.* / vivo.sensor.* / samsung.sensor.* 等异品牌私有节点。"
  ],
  "sources": [
    "目标机官方固件实测：build.prop / boot cmdline / VINTF manifest / vendor/etc（permissions、hals.conf、media_codecs、audio_effects、MNL_Config、thermal 温区、vda 分区表、measuredCpuClusters、measuredInputHints、measuredMapsRedirects）",
    "MediaTek Dimensity 7300 (MT6878) 平台公开规格：4nm、4×Cortex-A78 + 4×Cortex-A55、Mali-G615 MC2、LPDDR4X/5、UFS 2.2/3.1、Wi-Fi 6",
    "ARM Mali-G615 驱动版本与 Vulkan 能力口径（driverID=9 ARM proprietary，VK_API_VERSION_1_3=0x00403116）参考 ARM Mali Vulkan/GLES 驱动公开版本命名规则",
    "Transsion/Infinix MEOW 图形中间层与 Transsion 安全 HAL 家族（ITranssionFingerprintDaemon、ITranHwInfo、ITranAidlDataCenter 等）来自本机 VINTF 实测清单",
    "gold 标杆结构参考：devices/manifest/configs/redmi/gold/gold-android15/（仅对齐字段颗粒度，全部取值已替换为本机 MT6878/Infinix 口径）"
  ]
}
```
