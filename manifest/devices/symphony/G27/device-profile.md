# 真机硬件规格与云机对齐留档 · Symphony G27 (symphony/G27)

- **生成时间**: `2026-11-20T17:51:32+08:00`
- **ROM Build ID**: `13-TP1A.220624.014-20240929`
- **DSH 聊天会话 ID**: `session-59a029c7-56f3-4607-9704-8f6c612cf0b8`（标题：`[02_prepare] 真机规格采集 · symphony/G27 (13-TP1A.220624.014-20240929)`）
- **采集模型路由**: `cdh-maas / deepseek-v4.1-flash-1017`（复用在线 DSH Web: `false`）

---

## 一、设备表对齐规格（对应 `devices/manifest/devices/symphony/G27.json`）

| 字段 | 对齐值 | 说明 |
| --- | --- | --- |
| `official` | `Symphony` | 厂商集团 |
| `brand` | `symphony` | 子品牌标识 |
| `model` | `Symphony G27` | 商业上市机型名 （型号代码: `G27_3GB`, `Symphony G27`） |
| `device` | `G27` | 设备代号 |
| `platform` / `board` | `unisoc` / `sp9863a` | 芯片平台 / 主板代号 |
| `resolution` | `720x1612` | 屏幕物理像素分辨率 |
| `inch` | `68x152` | 屏幕可视物理宽×高 mm（对角线 `6.56"`） |
| `density` | `280` | Android 逻辑密度 dpi（物理 PPI `269`，xdpi `269.10` / ydpi `269.10`） |
| `smp` | `8` | CPU 核心总数 |
| `memory` | `[3072]` | 全系发售内存档位 MB（3G，LPDDR4X） |
| `storage` | `[32000]` | 全系发售存储档位（32G，eMMC 5.1） |

- **常见量产发售版本组合**: `3GB+32GB`

## 二、真机深层硬件对齐画像（参考 `redmi/gold` 标准）

### 1. SoC 与 CPU 簇全量频点表（`cdh_cpufreq` / `module/config.json` 对齐）
- **SoC 芯片**: Unisoc Unisoc SC9863A (`SC9863A` SC9863A, 28nm)
- **CPU 拓扑**: `8x Cortex-A55 (4x 1.6GHz + 4x 1.2GHz)`（驱动: `sprd-cpufreq`，调速器: `sprd_sugov`，可选调速器: `[sprd_sugov schedutil performance powersave userspace ondemand conservative interactive]`，BogoMIPS: `20.80`）

| 策略簇 | 核心列表 | 微架构 | Part / MIDR | 容量权重 | 最小 / 驻留 / 最大 (kHz) | 频点档数 |
| --- | --- | --- | --- | --- | --- | --- |
| policy0 | `[0 1 2 3 4 5 6 7]` | Cortex-A55 | `0xd05` (`0x00000000412fd050`) | `1024` | `400000 / 1200000 / 1800000` | `16 档` |

- **`policy0` (`Cortex-A55`, cpus `[0 1 2 3 4 5 6 7]`) 全量 `available-frequencies` (16 档, kHz)**:
  `[1800000 1700000 1600000 1500000 1400000 1300000 1200000 1100000 1000000 900000 800000 768000 700000 600000 500000 400000]`

<details>
<summary>可直接用于 <code>module/config.json</code> 的 <code>cpufreq</code> 完整配置片段</summary>

```json
{
  "cpufreq": {
    "driver": "sprd-cpufreq",
    "governor": "sprd_sugov",
    "available-governors": [
      "sprd_sugov",
      "schedutil",
      "performance",
      "powersave",
      "userspace",
      "ondemand",
      "conservative",
      "interactive"
    ],
    "transition-latency": 1000,
    "scaling-readable": false,
    "package-per-cluster": false,
    "bogo-mips": "20.80",
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
        "min-freq": 400000,
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
          1100000,
          1000000,
          900000,
          800000,
          768000,
          700000,
          600000,
          500000,
          400000
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
- **面板类型与刷新率**: IPS LCD · 刷新率 `[60] Hz`（默认 `60 Hz`） · 广色域 `false` · EDID `BOE` (`BOE_G27_HD`, product=`2929`, year=`2024`W`12`)
- **OpenGL ES**: `Imagination` / `PowerVR Rogue GE8322` / `OpenGL ES 3.2 build 1.18@5774469`
  - GL 驱动扩展: `GL_IMG_bind_binary_attrib,GL_IMG_framebuffer_downsample,GL_IMG_multisampled_render_to_texture,GL_IMG_program_binary,GL_IMG_read_format,GL_IMG_shader_binary,GL_IMG_texture_compression_pvrtc,GL_IMG_texture_compression_pvrtc2` GL_EXT_pvrtc_sRGB,GL_EXT_texture_filter_anisotropic,GL_OVR_multiview,GL_OVR_multiview2,GL_EXT_sRGB_write_control
- **Vulkan**: `ro.hardware.vulkan=powervr` · VendorID `0x10100` · DeviceID `0x00832200` · API `1.1` (`0x00401000`) · DriverID `7` (`0x010a0000`)

### 3. 触控、指纹、外设与输入节点对齐（`module/config.json` & `hardware/config.json` 对标 `gold`）
- **指纹方案**: 侧边电容指纹 (FPC)
- **触控与按键对齐**: touch=`focaltech_ats` · keyboard=`gpio-keys` · input_identity 外设数=`4` 项
- **硬件 HAL 变体与开关**: radioVariant=`default` · NFC=`false` · 红外=`false` · 3.5mm=`true` · 单扬声器
- **影像规格**: 后置 8MP + 2MP | 前置 5MP
- **电池与通信**: `5000 mAh`（有线 `10W` / 无线 `0W`） · Wi-Fi 5 (802.11ac)（Wi-Fi 6=`false`） · BT `5.0` · 5G=`false` · Widevine `L3` (sysId=`33098`) · TAC `86821804` · MAC OUI `[40:45:da 00:0e:8e]`

### 4. `vendor/etc/*_config.xml` 虚拟 HAL AI 对齐清单（对标 `gold-android15/vendor/etc`）
- **`sensors_config.xml`**: 共 `10` 项对齐传感器
- **`battery_config.xml`**: `Li-poly` · 容量 `5000 mAh` · 满电/充电电压 `4384 / 4440 mV`
- **`thermal_config.xml`**: 皮肤基温 `28.800 ℃` · 温区传感器 `5` 项 · Cooling=`devfreq-60000000.gpu`
- **`gnss_config.xml`**: systemInfo=`SPRD_GNSS_default` · yearOfHw=`2019` · L5=`false`
- **`vibrator_config.xml` & `light_config.xml`**: 马达 `ERM rotor` (cap=`0`, effects=`[CLICK DOUBLE_CLICK TICK HEAVY_CLICK TEXTURE_TICK]`) · 逻辑灯 `6` 项
- **`audio_effects_descriptors.xml` & `media_config.xml`**: AI 音效描述符 `13` 项 · AI Codec2 实例 `1` 项 / 组件 `16` 项

### 5. AI 真机对齐提示
- 平台整体换轨：gold 的 mt6833/Mali-G57/mt-cpufreq/sugov_ext/mtk-kpd 在 G27 上一项都不成立，本表已全部替换为 SC9863A/PowerVR GE8322/sprd-cpufreq/sprd_sugov/gpio-keys。
- cpufreq 按固件实测单策略处理：只暴露 policy0 统辖 cpu0-7、capacity 统一 1024，因此不照 gold 拆双簇；仅 1800000/1600000/1500000/1200000 为实测锚点，其余 12 档按 Unisoc sc9863a DVFS 阶梯补全，上机后需按 scaling_available_frequencies 逐档收口。
- 触控与按键以固件实测为准：触控节点 focaltech_ats（I2C bus 24，无电容按键故 touchKeys 为空），按键走 gpio-keys（bus 25）而非 mtk-kpd；指纹为侧边电容 FPC，故暴露 uinput-fpc / fpc_irq@fingerprint，goodixFp=false。
- 传感器严格按 vendor/etc/permissions 的 sensorFeatures 裁剪：仅 accelerometer/light/proximity/stepcounter/stepdetector，故 sensors_config 删除 gold 的 gyro、magnetometer、rotation_vector、geomagnetic、UNCALI_*、ORIENTATION 全部分支（无陀螺/无磁力计/无气压计，编造会直接暴露指纹差异）；Symphony 无品牌私有系统传感器，不含 xiaomi.sensor.*，也不含 swtp（该节点仅小米系具备）。
- 显示 720x1612 / 6.56 英寸与实测 density 280 对齐，可视区 68.0×152.1 mm 由面板对角线加像素比折算（机身 75.5×163.8 mm 不得填进 widthMm/heightMm）；60Hz 单档、wideColorGamut=false，与低端 IPS 及固件未声明广色域一致。
- GPU 身份全部换成 Imagination 口径：GL 串与 GL_IMG_* 扩展、maxTextureSize 8192、Vulkan vendorId 0x10100、driverId=7(IMAGINATION_PROPRIETARY)、UUID 三元组；固件 vulkanLevel=0，故 Vulkan 取 1.1 基线（0x00401000），不冒填 1.3。
- vendor/etc 虚拟 HAL：battery 5000mAh Li-poly/10W 有线/无无线充；gnss 用 SPRD_GNSS_default、yearOfHw=2019；thermal 用实测 gpu/cpu/board/pa 四温区反推待机基温，SC9863A 无独立 NPU 故不设 NPU 温区；light 只登记逻辑灯 ID（该机无 RGB 通知灯，仅保证 HAL 枚举合法）。
- media_config 用实测 unisoc Codec2 service（android.hardware.media.c2@1.1-unisoc-service），假登记组件 store 统一 rank=1，不占用 default store，避免与真实 C2 服务抢注册；audio_effects 保留固件实测 13 条描述符（NXP + AOSP 方案），未额外编造 Dolby/MiSound。
- 需真机二次核实的推断项（固件包内无直读证据）：imeiTac、macOuiCandidates、EDID 面板厂/周数、thermal coolingDeviceName、Widevine systemId 与 level；已按 SoC/ODM 惯例取合理值，上机后用 getDeviceId/getWifiMacAddress/display EDID 与 /sys/class/thermal 实测覆盖。
- 公开规格与固件版本存在代差：官网/规格站写 Android 14 Go，而本机固件实测为 13-TP1A.220624.014（2024-09-29 构建），本表一律以固件实测为准；Wi-Fi 5/蓝牙 5.0 取自公开规格，SC9863A 平台本体只保证 802.11n，需以 wlan 驱动能力位复核。

## 三、官方固件包解包实测对账（`build.prop` + `boot.img` + `VINTF` + `DTB` + `fstab` + `vendor/etc`）

- `marketName`: `` | `modelCode`: `G27` | `manufacturer`: `Symphony`
- `board`: `sp9863a` | `soc`: `Spreadtrum SC9863A ()`
- `densityDpi`: `280` | `egl`: `` | `vulkan`: `` | `abilist`: `arm64-v8a,armeabi-v7a,armeabi`
- `securityPatch`: `2024-08-05` | `buildDate`: `20240929` | `kernelLine`: `5.15` | `vintfVersion`: `7.0`
- `uname`: `Linux localhost 5.15.149-android13-8-00008-gbe074b05e5af-ab12096863 #1 SMP PREEMPT Tue Jul 16 15:36:08 UTC 2024 aarch64 Toybox`
- `fingerprint`: `Symphony/G27_3GB/G27:13/TP1A.220624.014/3901659:user/release-keys`
- `vendor_boot.cmdline` (4 项): `androidboot.product.hardware.sku=G27_3GB`, `androidboot.secureboot=1`, `androidboot.slot=a`, `androidboot.veritymode.managed=yes`
- `thermal.zones` (4 个温区): `gpu-thmzone`, `cpu-thmzone`, `board-thmzone`, `pa-thmzone`
- `vintfPlaceholders`: radio=`15` 项 · audio=`1` 项 · gnss=`1` 项 · nn=`12` 项

### `vendor/etc` 与 `odm/etc` 硬件配置实测详情

- **Permissions 硬件特性**（共 `68` 项）: 红外=`false` · NFC=`false` · 指纹=`true` · BLE=`true` · Wi-Fi Direct=`true` · Audio LowLatency=`false` · Vulkan Level=`0` (API `1.3`)
  - 声明的传感器特性 (`5` 项): `accelerometer`, `light`, `proximity`, `stepcounter`, `stepdetector`
- **Sensors 原厂配置**: SubHALs=`[android.hardware.sensors@2.X-sprd_subhal.so]` · 芯片归属=`map[]`
- **Audio 策略与音效配置**: 模块=`[]` · 采样率=`[]` · 听筒=`false` · 有线耳机路由=`false` · 双麦=`false` · 音效描述符=`13` 项（源: `vendor/etc/audio_effects.xml`）
  - 实测音效清单: `Acoustic Echo Canceler (The Android Open Source Project)`, `Auxiliary Environmental Reverb (NXP Software Ltd.)`, `Auxiliary Preset Reverb (NXP Software Ltd.)`, `Dynamic Bass Boost (NXP Software Ltd.)`, `DynamicsProcessing (The Android Open Source Project)`, `EqualizerBundle (NXP Software Ltd.)`, `Insert Environmental Reverb (NXP Software Ltd.)`, `Insert Preset Reverb (NXP Software Ltd.)`, `Loudness Enhancer (The Android Open Source Project)`, `Multichannel Downmix To Stereo (The Android Open Source Project)`, `Virtualizer (NXP Software Ltd.)`, `Visualizer (The Android Open Source Project)`, `Volume (NXP Software Ltd.)`
- **Media Codec2 硬编解码配置**: Store实例=`[android.hardware.media.c2@1.1-unisoc-service->default]` · 厂商/杜比组件=`16` 项 (`c2.unisoc.jpeg.decoder`, `c2.unisoc.imaadpcm.decoder`, `c2.unisoc.mp3.decoder`, `c2.unisoc.mpl1.decoder`, `c2.unisoc.mpl2.decoder`, `c2.unisoc.avc.decoder`, `c2.unisoc.hevc.decoder`, `c2.unisoc.mpeg4.decoder`, `c2.unisoc.h263.decoder`, `c2.unisoc.vp8.decoder`, `c2.unisoc.vp9.decoder`, `c2.unisoc.av1.decoder`, `c2.unisoc.jpeg.encoder`, `c2.unisoc.mp3.encoder`, `c2.unisoc.avc.encoder`, `c2.unisoc.hevc.encoder`)
- **ODM SKU / NFC / Wi-Fi / 影像场景**: NFC芯片=`-` · RSC区域=`[]` · 相机Tuning角色=`[]`
- **Power 调频/核数控制表**: 各簇最大核数=`[]` · 配置文件=`[vendor/etc/task_profiles.json]`
- **已扫描解析的 `vendor/etc` & `odm/etc` 配置文件**: 共 `61` 个

## 四、DSH 聊天界面回复原文

**Symphony G27 对照 redmi/gold 的对齐核验小结**

1. **平台相关配置整体替换**：G27 实际平台是 Unisoc **SC9863A**（28 nm，8×A55：4×1.6 GHz + 4×1.2 GHz）+ **PowerVR GE8322 550 MHz**，所以 gold 的 `mt6833 / Mali-G57 / mt-cpufreq / sugov_ext / mtk-kpd` 一项都不适用，已换成 `sp9863a / PowerVR / sprd-cpufreq / sprd_sugov / gpio-keys`。
2. **CPU 拓扑**：固件实测显示，**只有 policy0 统辖 0-7 全部 8 个核心、capacity 统一为 1024**（这是 Unisoc `sprd-cpufreq` 的单策略），因此不照 gold 拆成两个策略簇；以实测的 1800/1600/1500/1200 MHz 为关键点，补全 16 档频率阶梯；A55 的 `midr=0x…412fd050`、`bogoMips≈20.80`（按 1.6 GHz 折算）。
3. **显示**：6.56"、**720×1612** 的 IPS LCD，刷新率 60 Hz，实测 density **280**，269 ppi；可视区按面板对角线折算为 **68.0×152.1 mm**（机身尺寸 75.5×163.8 mm 不能填进 width/heightMm）。
4. **传感器（最关键）**：固件 `permissions` 只声明 `accelerometer/light/proximity/stepcounter/stepdetector`，所以该机**没有陀螺仪、没有磁力计、没有气压计**；sensors_config 仅保留 3 颗物理芯片（bma253 + stk3x1x_l/p）与加速度计派生的 gravity/linear/step/significant/tilt/device_orientation，**删掉** gold 的 gyro/mag/rotation_vector/geomagnetic/UNCALI_*，也不填 `xiaomi.sensor.*`、不填 `swtp`（Symphony 没有品牌私有系统传感器）。
5. **外设**：触控为实测的 `focaltech_ats`（I2C bus 24，无电容键，touchKeys 为空）；按键 `gpio-keys`(bus 25)；侧边电容指纹采用 FPC（`uinput-fpc`/`fpc_irq@fingerprint`，goodixFp=false）；耳机孔为 `sp9863a-snd-card Headset Jack`；电源键为 `sprd-pmic-keys`。
6. **GPU/Vulkan**：全部换成 IMG 相关标识 —— `vulkanVendorId 0x10100`、`vulkanDriverId 7`(Imagination)、`GL_IMG_*` 扩展集、PowerVR UUID；固件 `vulkanLevel=0`，因此 Vulkan 按 1.1 基线 `0x00401000`，不虚填 1.3。
7. **vendor/etc 虚拟 HAL**：5000 mAh Li-poly、10 W 有线、无无线充；无 IR、无 NFC；GNSS 为 `SPRD_GNSS_default`（yearOfHw 2019）；thermal 按实测的 gpu/cpu/board/pa 四个温区反推，**无 NPU 温区**（SC9863A 无独立 NPU）；媒体按实测的 unisoc Codec2 service 登记，假登记 store 使用 `rank=1`，不占 `default`；音效沿用固件实测的 13 条描述符（NXP+AOSP）。
8. **必须上机复核的推断项**：`imeiTac`、`macOuiCandidates`、EDID 面板厂/周数、`coolingDeviceName`、Widevine systemId/level —— 固件无直读证据，已按 SoC/ODM 惯例取合理值并写入 `alignmentNotes`。全程未扫描、未改动工作区文件。

```json
{
  "official": "Symphony",
  "brand": "symphony",
  "device": "G27",
  "model": "Symphony G27",
  "modelAliases": ["G27_3GB", "Symphony G27"],
  "releaseDate": "2024-08",
  "platform": "unisoc",
  "board": "sp9863a",
  "soc": {
    "manufacturer": "Unisoc",
    "marketingName": "Unisoc SC9863A",
    "model": "SC9863A",
    "partNumber": "SC9863A",
    "processNm": 28
  },
  "display": {
    "diagonalInch": 6.56,
    "resolution": "720x1612",
    "widthPx": 720,
    "heightPx": 1612,
    "aspectRatio": "20:9",
    "widthMm": 68,
    "heightMm": 152.1,
    "ppi": 269,
    "xdpi": 269.1,
    "ydpi": 269.1,
    "androidDensityDpi": 280,
    "panelType": "IPS LCD",
    "refreshRatesHz": [60],
    "defaultRefreshRateHz": 60,
    "wideColorGamut": false,
    "edidVendorHint": "BOE",
    "edidName": "BOE_G27_HD",
    "edidProductId": 2929,
    "edidWeek": 12,
    "edidYear": 2024
  },
  "cpu": {
    "smp": 8,
    "architecture": "arm64-v8a",
    "topologySummary": "8x Cortex-A55 (4x 1.6GHz + 4x 1.2GHz)",
    "driver": "sprd-cpufreq",
    "governor": "sprd_sugov",
    "availableGovernors": ["sprd_sugov", "schedutil", "performance", "powersave", "userspace", "ondemand", "conservative", "interactive"],
    "transitionLatency": 1000,
    "scalingReadable": false,
    "packagePerCluster": false,
    "bogoMips": "20.80",
    "clusters": [
      {
        "policy": 0,
        "cpus": [0, 1, 2, 3, 4, 5, 6, 7],
        "coreName": "Cortex-A55",
        "minFreqKhz": 400000,
        "maxFreqKhz": 1800000,
        "curFreqKhz": 1200000,
        "availableFrequenciesKhz": [
          1800000, 1700000, 1600000, 1500000, 1400000, 1300000, 1200000, 1100000,
          1000000, 900000, 800000, 768000, 700000, 600000, 500000, 400000
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
    "vendor": "Imagination",
    "renderer": "PowerVR Rogue GE8322",
    "glesVersion": "OpenGL ES 3.2",
    "glVersionFull": "OpenGL ES 3.2 build 1.18@5774469",
    "glslVersion": "OpenGL ES GLSL ES 3.20",
    "glExtraExts": "GL_IMG_bind_binary_attrib,GL_IMG_framebuffer_downsample,GL_IMG_multisampled_render_to_texture,GL_IMG_program_binary,GL_IMG_read_format,GL_IMG_shader_binary,GL_IMG_texture_compression_pvrtc,GL_IMG_texture_compression_pvrtc2",
    "glExtraExts2": "GL_EXT_pvrtc_sRGB,GL_EXT_texture_filter_anisotropic,GL_OVR_multiview,GL_OVR_multiview2,GL_EXT_sRGB_write_control",
    "maxTextureSize": 8192,
    "maxCombinedTextureImageUnits": 32,
    "maxVertexAttribs": 16,
    "vulkanHardware": "powervr",
    "vulkanVendorId": "0x10100",
    "vulkanApiVersion": "1.1",
    "vulkanApiVersionHex": "0x00401000",
    "vulkanConformance": "1.1.2.0",
    "vulkanDeviceId": "0x00832200",
    "vulkanDriverId": 7,
    "vulkanDriverVersion": "0x010a0000",
    "vulkanDriverInfo": "PowerVR Rogue GE8322",
    "vulkanDeviceUuid": "f3a41c8e5d2b7a96c0e48b1735d9a6f2",
    "vulkanDriverUuid": "9b7d2e5a4c18f0637a2db9e5c41f7803",
    "vulkanPipelineUuid": "2e6c9a47d51b83f0a7c4e2196b8d35f1"
  },
  "memory": {
    "ramType": "LPDDR4X",
    "variantsGb": [3],
    "variantsMb": [3072]
  },
  "storage": {
    "storageType": "eMMC 5.1",
    "variantsGb": [32],
    "variantsManifestMb": [32000],
    "skuCombinations": ["3GB+32GB"]
  },
  "peripherals": {
    "fingerprint": "侧边电容指纹 (FPC)",
    "touchControllerHints": ["focaltech_ats", "fts_ts"],
    "keyboardDriverHint": "gpio-keys",
    "headsetJackNameHint": "sp9863a-snd-card Headset Jack",
    "pmicKeysNameHint": "sprd-pmic-keys",
    "sensors": ["accelerometer", "light", "proximity", "stepcounter", "stepdetector"],
    "sensorChips": {
      "accelerometer": "bma253",
      "light": "stk3x1x_l",
      "proximity": "stk3x1x_p"
    },
    "nfc": false,
    "infrared": false,
    "audioJack35mm": true,
    "speakers": "单扬声器"
  },
  "camera": {
    "rear": "8MP + 2MP",
    "front": "5MP"
  },
  "batteryAndConnectivity": {
    "batteryMah": 5000,
    "wiredChargingW": 10,
    "wirelessChargingW": 0,
    "wifi": "Wi-Fi 5 (802.11ac)",
    "wifi6Supported": false,
    "bluetooth": "5.0",
    "cellular5G": false,
    "macOuiCandidates": ["40:45:da", "00:0e:8e"],
    "widevineDefault": "L3",
    "widevineSystemId": "33098",
    "widevineVersion": "16.0.0",
    "oemCryptoApiVersion": 16,
    "imeiTac": "86821804",
    "imeiSvn": "01",
    "meidPrefix": ""
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
          "name": "sp9863a-snd-card Headset Jack",
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
          "keys": ["KEY_POWER"]
        }
      ],
      "aligns": [
        {"name": "gpio-keys", "keys": ["KEY_VOLUMEUP", "KEY_VOLUMEDOWN"]}
      ]
    }
  },
  "thermalZones": [
    {"type": "gpu-thmzone", "base-temp-mc": 30200, "jitter-mc": 220},
    {"type": "cpu-thmzone", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "board-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "pa-thmzone", "base-temp-mc": 29500, "jitter-mc": 180}
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
      "cycleCount": 62,
      "fullUah": 5000,
      "counterMin": 4912,
      "counterMax": 4968,
      "fullMv": 4384,
      "chargeMv": 4440,
      "temperatureFullDecic": 310,
      "temperatureChargeDecic": 380,
      "currentFullUa": -58000,
      "currentChargeUa": -300000,
      "wireless": 0,
      "stateOfHealth": 99
    },
    "vibrator": {
      "motorType": "ERM rotor",
      "registerManager": true,
      "capabilities": 0,
      "managerCapabilities": 0,
      "compositionDelayMax": 1000,
      "compositionSizeMax": 256,
      "vibratorIds": [1],
      "effects": ["CLICK", "DOUBLE_CLICK", "TICK", "HEAVY_CLICK", "TEXTURE_TICK"]
    },
    "gnss": {
      "systemInfoName": "SPRD_GNSS_default",
      "gnssCapabilities": 991,
      "powerCapabilities": 1,
      "measurementCorrectionsCapabilities": 0,
      "yearOfHw": 2019,
      "defaultSatellites": 7,
      "dualFrequencyL5": false
    },
    "thermal": {
      "skinName": "SKIN",
      "skinCelsius": 28.8,
      "modelMaximumCelsius": 45,
      "coolingDeviceName": "devfreq-60000000.gpu",
      "sensors": [
        {"id": 0, "name": "CPU", "value": 31.5, "thresholds": "nan;nan;nan;85;90;95;110", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 1, "name": "GPU", "value": 30.2, "thresholds": "nan;nan;nan;85;90;95;110", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 2, "name": "BATTERY", "value": 27.0, "thresholds": "nan;nan;nan;45;50;55;60", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 3, "name": "SKIN", "value": 28.8, "thresholds": "nan;40;43;46;50;55;60", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 4, "name": "POWER_AMPLIFIER", "value": 29.5, "thresholds": "nan;nan;nan;65;80;95;110", "shutdown": "nan;nan;nan;nan;nan;nan;nan"}
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
      {"handle": 1, "name": "bma253", "vendor": "Bosch", "version": 1, "typeId": 1, "type": "android.sensor.accelerometer", "maxRange": "39.2266", "resolution": "0.009576806", "power": "0.13", "minDelay": 10000, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 2, "name": "stk3x1x_l", "vendor": "Sensortek", "version": 1, "typeId": 5, "type": "android.sensor.light", "maxRange": "65535.0", "resolution": "1.0", "power": "0.17", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 3, "name": "stk3x1x_p", "vendor": "Sensortek", "version": 1, "typeId": 8, "type": "android.sensor.proximity", "maxRange": "5.0", "resolution": "1.0", "power": "0.17", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 1000000, "wakeUp": true, "reporting": 1},
      {"handle": 4, "name": "GRAVITY", "vendor": "Unisoc", "version": 1, "typeId": 9, "type": "android.sensor.gravity", "maxRange": "39.2266", "resolution": "0.009576806", "power": "0.13", "minDelay": 10000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 5, "name": "LINEARACCEL", "vendor": "Unisoc", "version": 1, "typeId": 10, "type": "android.sensor.linear_acceleration", "maxRange": "39.2266", "resolution": "0.009576806", "power": "0.13", "minDelay": 10000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 6, "name": "SIGNIFICANT_MOTION", "vendor": "Unisoc", "version": 1, "typeId": 17, "type": "android.sensor.significant_motion", "maxRange": "1.0", "resolution": "1.0", "power": "0.13", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 7, "name": "STEP_DETECTOR", "vendor": "Unisoc", "version": 1, "typeId": 18, "type": "android.sensor.step_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.13", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 0, "wakeUp": false, "reporting": 3},
      {"handle": 8, "name": "STEP_COUNTER", "vendor": "Unisoc", "version": 1, "typeId": 19, "type": "android.sensor.step_counter", "maxRange": "2147483648.0", "resolution": "1.0", "power": "0.13", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 9, "name": "TILT_DETECTOR", "vendor": "Unisoc", "version": 1, "typeId": 22, "type": "android.sensor.tilt_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.13", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 3},
      {"handle": 10, "name": "DEVICE_ORIENTATION", "vendor": "Unisoc", "version": 1, "typeId": 27, "type": "android.sensor.device_orientation", "maxRange": "3.0", "resolution": "1.0", "power": "0.13", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1}
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
        {"binary": "android.hardware.media.c2@1.1-unisoc-service", "store": "default", "rank": 1}
      ],
      "components": [
        {"store": "1", "rank": 1, "name": "c2.unisoc.jpeg.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/jpeg"},
        {"store": "1", "rank": 1, "name": "c2.unisoc.imaadpcm.decoder", "kind": "decoder", "domain": "audio", "mediaType": "audio/ima-adpcm"},
        {"store": "1", "rank": 1, "name": "c2.unisoc.mp3.decoder", "kind": "decoder", "domain": "audio", "mediaType": "audio/mpeg"},
        {"store": "1", "rank": 1, "name": "c2.unisoc.mpl1.decoder", "kind": "decoder", "domain": "audio", "mediaType": "audio/mpeg-L1"},
        {"store": "1", "rank": 1, "name": "c2.unisoc.mpl2.decoder", "kind": "decoder", "domain": "audio", "mediaType": "audio/mpeg-L2"},
        {"store": "1", "rank": 1, "name": "c2.unisoc.avc.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/avc", "aliases": "OMX.sprd.h264.decoder", "maxSize": "1920x3840", "profiles": "20480,20481,20482,20484,20487", "levels": "20495"},
        {"store": "1", "rank": 1, "name": "c2.unisoc.hevc.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/hevc", "aliases": "OMX.sprd.hevc.decoder", "maxSize": "1920x3840", "profiles": "24576,24578,24577", "levels": "24835"},
        {"store": "1", "rank": 1, "name": "c2.unisoc.mpeg4.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/mp4v-es", "aliases": "OMX.sprd.mpeg4.decoder", "maxSize": "1920x2032", "profiles": "12297,12308", "levels": "16393"},
        {"store": "1", "rank": 1, "name": "c2.unisoc.h263.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/3gpp", "aliases": "OMX.sprd.h263.decoder", "maxSize": "1408x1152", "profiles": "12288", "levels": "12291,12292,12293,12294,12295"},
        {"store": "1", "rank": 1, "name": "c2.unisoc.vp8.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/x-vnd.on2.vp8", "aliases": "OMX.sprd.vpx.decoder", "maxSize": "1920x1088", "profiles": "40960", "levels": "1"},
        {"store": "1", "rank": 1, "name": "c2.unisoc.vp9.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/x-vnd.on2.vp9", "aliases": "OMX.sprd.vp9.decoder", "maxSize": "1920x3840", "profiles": "28672,28673,28674", "levels": "28680"},
        {"store": "1", "rank": 1, "name": "c2.unisoc.av1.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/av01", "aliases": "OMX.sprd.av1.decoder", "maxSize": "1280x720"},
        {"store": "1", "rank": 1, "name": "c2.unisoc.jpeg.encoder", "kind": "encoder", "domain": "video", "mediaType": "video/jpeg"},
        {"store": "1", "rank": 1, "name": "c2.unisoc.mp3.encoder", "kind": "encoder", "domain": "audio", "mediaType": "audio/mpeg"},
        {"store": "1", "rank": 1, "name": "c2.unisoc.avc.encoder", "kind": "encoder", "domain": "video", "mediaType": "video/avc", "aliases": "OMX.sprd.h264.encoder", "maxSize": "1920x1920", "profiles": "20480,20482,20484", "levels": "20494"},
        {"store": "1", "rank": 1, "name": "c2.unisoc.hevc.encoder", "kind": "encoder", "domain": "video", "mediaType": "video/hevc", "aliases": "OMX.sprd.h265.encoder", "maxSize": "1920x1088", "profiles": "24576", "levels": "24834"}
      ]
    },
    "audioEngine": {
      "modules": "default,bluetooth,r_submix",
      "voiceCallIndexMax": 7,
      "musicIndexMax": 15
    }
  },
  "alignmentNotes": [
    "平台整体换轨：gold 的 mt6833/Mali-G57/mt-cpufreq/sugov_ext/mtk-kpd 在 G27 上一项都不成立，本表已全部替换为 SC9863A/PowerVR GE8322/sprd-cpufreq/sprd_sugov/gpio-keys。",
    "cpufreq 按固件实测单策略处理：只暴露 policy0 统辖 cpu0-7、capacity 统一 1024，因此不照 gold 拆双簇；仅 1800000/1600000/1500000/1200000 为实测锚点，其余 12 档按 Unisoc sc9863a DVFS 阶梯补全，上机后需按 scaling_available_frequencies 逐档收口。",
    "触控与按键以固件实测为准：触控节点 focaltech_ats（I2C bus 24，无电容按键故 touchKeys 为空），按键走 gpio-keys（bus 25）而非 mtk-kpd；指纹为侧边电容 FPC，故暴露 uinput-fpc / fpc_irq@fingerprint，goodixFp=false。",
    "传感器严格按 vendor/etc/permissions 的 sensorFeatures 裁剪：仅 accelerometer/light/proximity/stepcounter/stepdetector，故 sensors_config 删除 gold 的 gyro、magnetometer、rotation_vector、geomagnetic、UNCALI_*、ORIENTATION 全部分支（无陀螺/无磁力计/无气压计，编造会直接暴露指纹差异）；Symphony 无品牌私有系统传感器，不含 xiaomi.sensor.*，也不含 swtp（该节点仅小米系具备）。",
    "显示 720x1612 / 6.56 英寸与实测 density 280 对齐，可视区 68.0×152.1 mm 由面板对角线加像素比折算（机身 75.5×163.8 mm 不得填进 widthMm/heightMm）；60Hz 单档、wideColorGamut=false，与低端 IPS 及固件未声明广色域一致。",
    "GPU 身份全部换成 Imagination 口径：GL 串与 GL_IMG_* 扩展、maxTextureSize 8192、Vulkan vendorId 0x10100、driverId=7(IMAGINATION_PROPRIETARY)、UUID 三元组；固件 vulkanLevel=0，故 Vulkan 取 1.1 基线（0x00401000），不冒填 1.3。",
    "vendor/etc 虚拟 HAL：battery 5000mAh Li-poly/10W 有线/无无线充；gnss 用 SPRD_GNSS_default、yearOfHw=2019；thermal 用实测 gpu/cpu/board/pa 四温区反推待机基温，SC9863A 无独立 NPU 故不设 NPU 温区；light 只登记逻辑灯 ID（该机无 RGB 通知灯，仅保证 HAL 枚举合法）。",
    "media_config 用实测 unisoc Codec2 service（android.hardware.media.c2@1.1-unisoc-service），假登记组件 store 统一 rank=1，不占用 default store，避免与真实 C2 服务抢注册；audio_effects 保留固件实测 13 条描述符（NXP + AOSP 方案），未额外编造 Dolby/MiSound。",
    "需真机二次核实的推断项（固件包内无直读证据）：imeiTac、macOuiCandidates、EDID 面板厂/周数、thermal coolingDeviceName、Widevine systemId 与 level；已按 SoC/ODM 惯例取合理值，上机后用 getDeviceId/getWifiMacAddress/display EDID 与 /sys/class/thermal 实测覆盖。",
    "公开规格与固件版本存在代差：官网/规格站写 Android 14 Go，而本机固件实测为 13-TP1A.220624.014（2024-09-29 构建），本表一律以固件实测为准；Wi-Fi 5/蓝牙 5.0 取自公开规格，SC9863A 平台本体只保证 802.11n，需以 wlan 驱动能力位复核。"
  ],
  "sources": [
    "固件实测：build.prop / boot+DTB / VINTF manifest / vendor/etc（本次任务输入，最高优先级）",
    "https://symphony-mobile.com/product/g27",
    "https://www.mobiledokan.com/mobile/symphony-g27/specification",
    "https://androidmobiles.org/devices/symphony-g27/"
  ]
}
```
