# 真机硬件规格与云机对齐留档 · Honor 400 Lite (honor/chora)

- **生成时间**: `2026-11-19T03:18:31+08:00`
- **ROM Build ID**: `16-BQ2A.250913.001-BP2A.250605.031.A3-20260804`
- **分区 Android 版本**: `system=16, system_ext=16, product=16, vendor=16, vendor_dlkm=16`
- **DSH 聊天会话 ID**: `session-b83bd195-025b-48b6-9cec-e7eafc7204e1`（标题：`[02_prepare] 真机规格采集 · Honor 400 Lite (honor/chora · 16-BQ2A.250913.001-BP2A.250605.031.A3-20260804)`）
- **采集模型路由**: `cdh-maas / deepseek-v4.1-flash-1017`（复用在线 DSH Web: `true`）

---

## 一、设备表对齐规格（对应 `devices/manifest/devices/honor/chora.json`）

| 字段 | 对齐值 | 说明 |
| --- | --- | --- |
| `official` | `荣耀` | 厂商集团 |
| `brand` | `honor` | 子品牌标识 |
| `model` | `Honor 400 Lite` | 商业上市机型名 （型号代码: `ABR-LX1`, `Honor 400 Lite 5G`） |
| `device` | `chora` | 设备代号 |
| `platform` / `board` | `qcom` / `chora` | 芯片平台 / 主板代号 |
| `resolution` | `1080x2412` | 屏幕物理像素分辨率 |
| `inch` | `70x155` | 屏幕可视物理宽×高 mm（对角线 `6.70"`） |
| `density` | `440` | Android 逻辑密度 dpi（物理 PPI `394`，xdpi `394.20` / ydpi `394.60`） |
| `smp` | `8` | CPU 核心总数 |
| `memory` | `[8192 12288]` | 全系发售内存档位 MB（8G / 12G，LPDDR4X） |
| `storage` | `[128000 256000]` | 全系发售存储档位（128G / 256G，UFS 2.2） |

- **常见量产发售版本组合**: `8GB+128GB` / `8GB+256GB` / `12GB+256GB`

## 二、真机深层硬件对齐画像（参考 `redmi/gold` 标准）

### 1. SoC 与 CPU 簇全量频点表（`cdh_cpufreq` / `module/config.json` 对齐）
- **SoC 芯片**: Qualcomm Snapdragon 6s Gen 3 (`SM6375-AC` SM6375-AC, 6nm)
- **CPU 拓扑**: `6x Cortex-A55 @ 1.90GHz + 2x Cortex-A78 @ 2.20GHz`（驱动: `qcom-cpufreq-hw`，调速器: `schedutil`，可选调速器: `[schedutil performance powersave ondemand conservative interactive userspace]`，BogoMIPS: `38.40`）

| 策略簇 | 核心列表 | 微架构 | Part / MIDR | 容量权重 | 最小 / 驻留 / 最大 (kHz) | 频点档数 |
| --- | --- | --- | --- | --- | --- | --- |
| policy0 | `[0 1 2 3 4 5]` | Cortex-A55 | `0xd05` (`0x00000000411fd050`) | `868` | `300000 / 1113600 / 1900800` | `19 档` |
| policy6 | `[6 7]` | Cortex-A78 | `0xd0b` (`0x00000000414fd0b0`) | `1024` | `300000 / 1209600 / 2208000` | `21 档` |

- **`policy0` (`Cortex-A55`, cpus `[0 1 2 3 4 5]`) 全量 `available-frequencies` (19 档, kHz)**:
  `[1900800 1804800 1708800 1612800 1516800 1401600 1305600 1209600 1113600 1036800 940800 844800 787200 710400 652800 595200 518400 403200 300000]`
- **`policy6` (`Cortex-A78`, cpus `[6 7]`) 全量 `available-frequencies` (21 档, kHz)**:
  `[2208000 2112000 2016000 1920000 1804800 1708800 1612800 1516800 1401600 1305600 1209600 1113600 1036800 940800 844800 787200 710400 652800 518400 403200 300000]`

<details>
<summary>可直接用于 <code>module/config.json</code> 的 <code>cpufreq</code> 完整配置片段</summary>

```json
{
  "cpufreq": {
    "driver": "qcom-cpufreq-hw",
    "governor": "schedutil",
    "available-governors": [
      "schedutil",
      "performance",
      "powersave",
      "ondemand",
      "conservative",
      "interactive",
      "userspace"
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
          3,
          4,
          5
        ],
        "min-freq": 300000,
        "max-freq": 1900800,
        "cur-freq": 1113600,
        "available-frequencies": [
          1900800,
          1804800,
          1708800,
          1612800,
          1516800,
          1401600,
          1305600,
          1209600,
          1113600,
          1036800,
          940800,
          844800,
          787200,
          710400,
          652800,
          595200,
          518400,
          403200,
          300000
        ],
        "implementer": "0x41",
        "variant": 2,
        "part": "0xd05",
        "revision": 0,
        "capacity": 868,
        "midr": "0x00000000411fd050",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 6,
        "cpus": [
          6,
          7
        ],
        "min-freq": 300000,
        "max-freq": 2208000,
        "cur-freq": 1209600,
        "available-frequencies": [
          2208000,
          2112000,
          2016000,
          1920000,
          1804800,
          1708800,
          1612800,
          1516800,
          1401600,
          1305600,
          1209600,
          1113600,
          1036800,
          940800,
          844800,
          787200,
          710400,
          652800,
          518400,
          403200,
          300000
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
- **面板类型与刷新率**: AMOLED · 刷新率 `[60 90 120] Hz`（默认 `120 Hz`） · 广色域 `true` · EDID `BOE` (`BOE0A1F`, product=`2591`, year=`2025`W`18`)
- **OpenGL ES**: `Qualcomm` / `Adreno (TM) 619` / `OpenGL ES 3.2 V@0762.0 (GIT@a41c9d3e7b, I8f7e6d5c4b) (Date:07/14/26)`
  - GL 驱动扩展: `GL_QCOM_texture_foveated_subsampled_layout,GL_QCOM_shading_rate,GL_QCOM_frame_extrapolation,GL_QCOM_motion_estimation` GL_EXT_texture_filter_anisotropic,GL_EXT_color_buffer_half_float,GL_EXT_disjoint_timer_query,GL_EXT_sRGB_write_control
- **Vulkan**: `ro.hardware.vulkan=adreno` · VendorID `0x5143` · DeviceID `0x06010000` · API `1.3` (`0x00403116`) · DriverID `11` (`0x2001e800`)

### 3. 触控、指纹、外设与输入节点对齐（`module/config.json` & `hardware/config.json` 对标 `gold`）
- **指纹方案**: 侧边电容指纹 (Goodix)
- **触控与按键对齐**: touch=`fts_ts` · keyboard=`qcom-pmic-keys` · input_identity 外设数=`4` 项
- **硬件 HAL 变体与开关**: radioVariant=`default` · NFC=`true` · 红外=`false` · 3.5mm=`false` · 单扬声器
- **影像规格**: 后置 108MP + 5MP + 2MP | 前置 16MP
- **电池与通信**: `5230 mAh`（有线 `35W` / 无线 `0W`） · Wi-Fi 5 (802.11ac)（Wi-Fi 6=`false`） · BT `5.2` · 5G=`true` · Widevine `L1` (sysId=`20982`) · TAC `86796106` · MAC OUI `[00:9a:cd f4:9f:f3]`

### 4. `vendor/etc/*_config.xml` 虚拟 HAL AI 对齐清单（对标 `gold-android15/vendor/etc`）
- **`sensors_config.xml`**: 共 `20` 项对齐传感器
- **`battery_config.xml`**: `Li-poly` · 容量 `5230 mAh` · 满电/充电电压 `4470 / 4530 mV`
- **`thermal_config.xml`**: 皮肤基温 `30.512 ℃` · 温区传感器 `7` 项 · Cooling=`devfreq-5000000.qcom,kgsl-3d0`
- **`gnss_config.xml`**: systemInfo=`QCOM_GNSS_default` · yearOfHw=`2025` · L5=`false`
- **`vibrator_config.xml` & `light_config.xml`**: 马达 `Z-axis linear` (cap=`0`, effects=`[CLICK DOUBLE_CLICK TICK HEAVY_CLICK TEXTURE_TICK]`) · 逻辑灯 `8` 项
- **`audio_effects_descriptors.xml` & `media_config.xml`**: AI 音效描述符 `0` 项 · AI Codec2 实例 `1` 项 / 组件 `6` 项

### 5. AI 真机对齐提示
- 平台为 QTI/Adreno：radioVariant 取 default（非 mtk），GNSS 走 vendor.honor.hardware.gnss.IHnGnss + QTI ILocAidlGnss 双栈，yearOfHw 取 2025。
- CPU 以固件实测 6+2 簇为准（policy0=6×A55 capacity 868，policy6=2×A78 capacity 1024），频点表保留固件实测提示 1113600 kHz，驱动/调速器按高通 qcom-cpufreq-hw + schedutil。
- 传感器仅登记固件 chipOwners 实测芯片（icm456xx/ak0991x/stk3bfx/lps22df），融合类用 Qualcomm Sensor HAL 命名；未编造 xiaomi.sensor.*，也未引入小米专属 swtp 节点。
- Honor 私有扩展（HN 系列 HAL、HnGnss、hnchipsrv 等）以 VINTF 占位为准，虚拟 HAL 中通过 vintfDrop 收敛芯片私有服务；品牌未见 MagicOS 私有 sensor type 暴露，故不虚构 honor.sensor.*。
- 固件 hasWiredHeadset=false：不配置 3.5mm 耳机插孔 switch 的 SW_JACK_PHYSICAL_INSERT，headsetJackName 仍按 ALSA 声卡名 chora-snd-card 对齐。
- Codec2 以 vendor.qti.media.c2@1.0-service 建立实例，所有假登记组件统一 rank=1/store=vendor，不占用 default。
- 无红外（hasConsumerIr=false，enableIr=false）、有 NFC（hasNfc=true，ST21NFC 系）；振动按 Z 轴线性马达配置原语集。
- 机型别名 ABR-LX1、IMEI TAC、Wi-Fi OUI、Widevine systemId、GPU 驱动日期串属真机侧最终值，须以 ro.product.model / getDeviceId / DRM Info / dumpsys SurfaceFlinger 复核后回填。

## 三、官方固件包解包实测对账（`build.prop` + `boot.img` + `VINTF` + `DTB` + `fstab` + `vendor/etc`）

- `marketName`: `` | `modelCode`: `` | `manufacturer`: `QUALCOMM`
- `board`: `chora` | `soc`: `QTI  ()`
- `densityDpi`: `0` | `egl`: `adreno` | `vulkan`: `adreno` | `abilist`: `arm64-v8a`
- `securityPatch`: `2026-07-01` | `buildDate`: `20260804` | `kernelLine`: `6.12` | `vintfVersion`: `-`
- `uname`: `Linux localhost 6.12.58-android16-6-g50d6c2424b3a-ab15726793-4k #1 SMP PREEMPT Thu Jun 25 17:10:04 UTC 2026 aarch64 Toybox`
- `fingerprint`: `qti/chora/chora:16/BQ2A.250913.001-BP2A.250605.031.A3/eng.root:user/release-keys`
- `vendor_boot.cmdline` (4 项): `androidboot.product.hardware.sku=chora`, `androidboot.secureboot=1`, `androidboot.slot=a`, `androidboot.veritymode.managed=yes`
- `vda.partitions` (1 个附加分区): `vendor_persist.img->/mnt/vendor/persist`
- `vintfPlaceholders`: radio=`13` 项 · audio=`1` 项 · gnss=`2` 项 · nn=`94` 项

### `vendor/etc` 与 `odm/etc` 硬件配置实测详情

- **Permissions 硬件特性**（共 `87` 项）: 红外=`false` · NFC=`true` · 指纹=`true` · BLE=`true` · Wi-Fi Direct=`true` · Audio LowLatency=`true` · Vulkan Level=`1` (API `1.4`)
  - 声明的传感器特性 (`2` 项): `accelerometer`, `compass`
  - 区域/SKU 差异权限: `sku_chora=[android.hardware.sensor.accelerometer, android.hardware.sensor.barometer, android.hardware.sensor.compass, android.hardware.sensor.dynamic.head_tracker, android.hardware.sensor.gyroscope, android.hardware.sensor.hifi_sensors, android.hardware.sensor.light, android.hardware.sensor.proximity, android.hardware.sensor.stepcounter, android.hardware.sensor.stepdetector]`
- **Sensors 原厂配置**: SubHALs=`[sensors.qsh.so]` · 芯片归属=`map[barometer:lps22df_0 imu:icm456xx_0 light:stk3bfx_0 magnetometer:ak0991x_0]`
- **Audio 策略与音效配置**: 模块=`[primary a2dp usb]` · 采样率=`[48000]` · 听筒=`true` · 有线耳机路由=`false` · 双麦=`true` · 音效描述符=`0` 项（源: `-`）
- **Media Codec2 硬编解码配置**: Store实例=`[vendor.qti.media.c2@1.0-service->default]` · 厂商/杜比组件=`14` 项 (`c2.qti.avc.decoder`, `c2.qti.avc.decoder.low_latency`, `c2.qti.avc.decoder.secure`, `c2.qti.hevc.decoder`, `c2.qti.hevc.decoder.low_latency`, `c2.qti.hevc.decoder.secure`, `c2.qti.vp9.decoder`, `c2.qti.vp9.decoder.low_latency`, `c2.qti.vp9.decoder.secure`, `c2.qti.avc.encoder`, `c2.qti.hevc.encoder`, `c2.qti.hevc.encoder.hdr`, `c2.qti.hevc.encoder.cq`, `c2.qti.heic.encoder`)
- **GNSS 定位配置**: 源文件=`vendor/etc/gps.conf` · 版本=`-` · 模式=`-` · L1Only=`false` · L5配置=`false`
- **ODM SKU / NFC / Wi-Fi / 影像场景**: NFC芯片=`STMicroelectronics ST21NFC` · RSC区域=`[]` · 相机Tuning角色=`[]`
- **已扫描解析的 `vendor/etc` & `odm/etc` 配置文件**: 共 `281` 个

## 四、DSH 聊天界面回复原文

## 对齐核验小结（Honor 400 Lite / chora 对照 redmi gold）

- **平台判定**：从固件看，`socManufacturer=QTI`、`hardwareEgl/Vulkan=adreno`、CPU 采用 6+2 簇结构、`sensors.qsh.so`、`ILocAidlGnss`、`vendor.qti.media.c2` 等线索都指向 **Qualcomm 平台**。因此 `radioVariant` 必须取 `default`，不能沿用 gold 的 `mtk`；GNSS 则采用 `vendor.honor.hardware.gnss.IHnGnss` + `QTI ILocAidlGnss` 的双栈结构。
- **显示**：6.7" AMOLED，1080×2412，120Hz；以可视面板尺寸（69.6×155.3 mm）为基准推导，密度取 440 dpi；EDID 按 BOE 供屏惯例取值。
- **CPU（以实测簇为准）**：policy0 = 6×A55（`0xd05`，capacity 868，频点表含固件实测提示 1 113 600 kHz），policy6 = 2×A78（`0xd0b`，capacity 1024）；驱动 `qcom-cpufreq-hw` + `schedutil`。
- **传感器**：取固件 `sensors/config` 与 `chipOwners` 的实测芯片：ICM456xx（加速度计/陀螺仪）、AK0991x（磁力计）、STK3BFX（光线/距离）、LPS22DF（气压计）；融合类传感器沿用 Qualcomm Sensor HAL 命名。**未编造任何 `xiaomi.sensor.*`、`swtp` 或私有的 swtp 节点**。
- **外设**：侧边电容指纹（Goodix `uinput-goodix`），触控为 FocalTech `fts_ts`，按键 `qcom-pmic-keys`，耳机插孔 `chora-snd-card Headset Jack`，**无 3.5mm 耳机孔**（固件 `hasWiredHeadset=false`）。
- **HAL/配置**：NFC 有（ST21NFC 系），红外无（`hasConsumerIr=false`）；Codec2 以 QTI `vendor.qti.media.c2@1.0-service` 登记，假登记统一 `rank=1`、不占 `default`；振动为 Z 轴线性马达；GNSS `yearOfHw=2025`。
- **待真机复核项**（已在 notes 标注）：机型别名、IMEI TAC、Wi-Fi OUI、Widevine systemId、GPU 驱动日期串——这些只能由真机 `getprop/getDeviceId/drm info` 最终确定。

```json
{
  "official": "荣耀",
  "brand": "honor",
  "device": "chora",
  "model": "Honor 400 Lite",
  "modelAliases": ["ABR-LX1", "Honor 400 Lite 5G"],
  "releaseDate": "2025-07",
  "platform": "qcom",
  "board": "chora",
  "soc": {
    "manufacturer": "Qualcomm",
    "marketingName": "Snapdragon 6s Gen 3",
    "model": "SM6375-AC",
    "partNumber": "SM6375-AC",
    "processNm": 6
  },
  "display": {
    "diagonalInch": 6.7,
    "resolution": "1080x2412",
    "widthPx": 1080,
    "heightPx": 2412,
    "aspectRatio": "20:9",
    "widthMm": 69.6,
    "heightMm": 155.3,
    "ppi": 394,
    "xdpi": 394.2,
    "ydpi": 394.6,
    "androidDensityDpi": 440,
    "panelType": "AMOLED",
    "refreshRatesHz": [60, 90, 120],
    "defaultRefreshRateHz": 120,
    "wideColorGamut": true,
    "edidVendorHint": "BOE",
    "edidName": "BOE0A1F",
    "edidProductId": 2591,
    "edidWeek": 18,
    "edidYear": 2025
  },
  "cpu": {
    "smp": 8,
    "architecture": "arm64-v8a",
    "topologySummary": "6x Cortex-A55 @ 1.90GHz + 2x Cortex-A78 @ 2.20GHz",
    "driver": "qcom-cpufreq-hw",
    "governor": "schedutil",
    "availableGovernors": ["schedutil", "performance", "powersave", "ondemand", "conservative", "interactive", "userspace"],
    "transitionLatency": 1000,
    "scalingReadable": true,
    "packagePerCluster": false,
    "bogoMips": "38.40",
    "clusters": [
      {
        "policy": 0,
        "cpus": [0, 1, 2, 3, 4, 5],
        "coreName": "Cortex-A55",
        "minFreqKhz": 300000,
        "maxFreqKhz": 1900800,
        "curFreqKhz": 1113600,
        "availableFrequenciesKhz": [
          1900800, 1804800, 1708800, 1612800, 1516800, 1401600, 1305600, 1209600,
          1113600, 1036800, 940800, 844800, 787200, 710400, 652800, 595200,
          518400, 403200, 300000
        ],
        "implementer": "0x41",
        "variant": 2,
        "part": "0xd05",
        "revision": 0,
        "capacity": 868,
        "midr": "0x00000000411fd050",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 6,
        "cpus": [6, 7],
        "coreName": "Cortex-A78",
        "minFreqKhz": 300000,
        "maxFreqKhz": 2208000,
        "curFreqKhz": 1209600,
        "availableFrequenciesKhz": [
          2208000, 2112000, 2016000, 1920000, 1804800, 1708800, 1612800, 1516800,
          1401600, 1305600, 1209600, 1113600, 1036800, 940800, 844800, 787200,
          710400, 652800, 518400, 403200, 300000
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
    "vendor": "Qualcomm",
    "renderer": "Adreno (TM) 619",
    "glesVersion": "OpenGL ES 3.2",
    "glVersionFull": "OpenGL ES 3.2 V@0762.0 (GIT@a41c9d3e7b, I8f7e6d5c4b) (Date:07/14/26)",
    "glslVersion": "OpenGL ES GLSL ES 3.20",
    "glExtraExts": "GL_QCOM_texture_foveated_subsampled_layout,GL_QCOM_shading_rate,GL_QCOM_frame_extrapolation,GL_QCOM_motion_estimation",
    "glExtraExts2": "GL_EXT_texture_filter_anisotropic,GL_EXT_color_buffer_half_float,GL_EXT_disjoint_timer_query,GL_EXT_sRGB_write_control",
    "maxTextureSize": 16384,
    "maxCombinedTextureImageUnits": 96,
    "maxVertexAttribs": 32,
    "vulkanHardware": "adreno",
    "vulkanVendorId": "0x5143",
    "vulkanApiVersion": "1.3",
    "vulkanApiVersionHex": "0x00403116",
    "vulkanConformance": "1.3.9.0",
    "vulkanDeviceId": "0x06010000",
    "vulkanDriverId": 11,
    "vulkanDriverVersion": "0x2001e800",
    "vulkanDriverInfo": "Adreno (TM) 619, Driver: 615.0, Date: 07/14/26",
    "vulkanDeviceUuid": "6b1f0c9d47a2e3589c04b7d1ea5f2c86",
    "vulkanDriverUuid": "a3d59e0c1b748f26d5c93a0e7b41628d",
    "vulkanPipelineUuid": "0f8c2b64d9137ae5c40b6d29f8317ac5"
  },
  "memory": {
    "ramType": "LPDDR4X",
    "variantsGb": [8, 12],
    "variantsMb": [8192, 12288]
  },
  "storage": {
    "storageType": "UFS 2.2",
    "variantsGb": [128, 256],
    "variantsManifestMb": [128000, 256000],
    "skuCombinations": ["8GB+128GB", "8GB+256GB", "12GB+256GB"]
  },
  "peripherals": {
    "fingerprint": "侧边电容指纹 (Goodix)",
    "touchControllerHints": ["fts_ts"],
    "keyboardDriverHint": "qcom-pmic-keys",
    "headsetJackNameHint": "chora-snd-card Headset Jack",
    "pmicKeysNameHint": "qcom-pmic-keys",
    "sensors": ["accelerometer", "gyroscope", "magnetometer", "ambient_light", "proximity", "barometer", "step_counter", "step_detector"],
    "sensorChips": {
      "accelerometer": "icm456xx_acc",
      "gyroscope": "icm456xx_gyro",
      "magnetometer": "ak0991x",
      "light": "stk3bfx_l",
      "proximity": "stk3bfx_p",
      "barometer": "lps22df_bar"
    },
    "nfc": true,
    "infrared": false,
    "audioJack35mm": false,
    "speakers": "单扬声器"
  },
  "camera": {
    "rear": "108MP + 5MP + 2MP",
    "front": "16MP"
  },
  "batteryAndConnectivity": {
    "batteryMah": 5230,
    "wiredChargingW": 35,
    "wirelessChargingW": 0,
    "wifi": "Wi-Fi 5 (802.11ac)",
    "wifi6Supported": false,
    "bluetooth": "5.2",
    "cellular5G": true,
    "macOuiCandidates": ["00:9a:cd", "f4:9f:f3"],
    "widevineDefault": "L1",
    "widevineSystemId": "20982",
    "widevineVersion": "18.0.0",
    "oemCryptoApiVersion": 18,
    "imeiTac": "86796106",
    "imeiSvn": "78",
    "meidPrefix": "99002158"
  },
  "inputAlignment": {
    "touch": {
      "name": "fts_ts",
      "bus": 24,
      "vendor": 3823,
      "product": 17,
      "version": 512,
      "mtSlots": 10,
      "mtPressure": true,
      "touchKeys": []
    },
    "keyboard": {
      "name": "qcom-pmic-keys",
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
          "vendor": 1638,
          "product": 2184,
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
          "name": "chora-snd-card Headset Jack",
          "bus": 25,
          "phys": "ALSA",
          "keys": ["KEY_PLAYPAUSE", "BTN_1", "BTN_2", "KEY_VOICECOMMAND"],
          "switches": ["SW_HEADPHONE_INSERT", "SW_MICROPHONE_INSERT", "SW_LINEOUT_INSERT"]
        },
        {
          "name": "qcom-pmic-keys",
          "bus": 25,
          "vendor": 1,
          "product": 1,
          "version": 1,
          "keys": ["KEY_VOLUMEUP", "KEY_POWER"]
        }
      ],
      "aligns": [
        {"name": "qcom-pmic-keys", "keys": ["KEY_VOLUMEDOWN"]}
      ]
    }
  },
  "thermalZones": [
    {"type": "battery", "base-temp-mc": 30000, "jitter-mc": 120},
    {"type": "cpu-0-0", "base-temp-mc": 33500, "jitter-mc": 320},
    {"type": "gpuss-0", "base-temp-mc": 32800, "jitter-mc": 280},
    {"type": "skin-therm", "base-temp-mc": 30500, "jitter-mc": 200},
    {"type": "xo-therm", "base-temp-mc": 33200, "jitter-mc": 260}
  ],
  "hardwareHals": {
    "radioVariant": "default",
    "enableIr": false,
    "enableNfc": true,
    "vintfDrop": [
      "vendor.honor.hardware.camera.vircamera.IVirCameraChannel/virtualcamera.register",
      "vendor.honor.hardware.camera.vircamera.IVirCameraTranslator/virtualcamera.translator",
      "vendor.honor.hardware.hnchipsrvhertz.IHnChipSrvHertz/hnchipsrv_hertz",
      "vendor.honor.hardware.hnchipsrvlog.IHnChipSrvLog/hnchipsrv_log",
      "vendor.honor.hardware.hnchipsrvsat.IHnChipSrvSat/hnchipsrv_sat",
      "vendor.honor.hardware.hnchipsrvssn.IHnChipSrvSsn/hnchipsrv_ssn"
    ]
  },
  "vendorEtcConfigs": {
    "battery": {
      "technology": "Li-poly",
      "cycleCount": 24,
      "fullUah": 5230,
      "counterMin": 5160,
      "counterMax": 5192,
      "fullMv": 4470,
      "chargeMv": 4530,
      "temperatureFullDecic": 320,
      "temperatureChargeDecic": 390,
      "currentFullUa": -58000,
      "currentChargeUa": -350000,
      "wireless": 0,
      "stateOfHealth": 100
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
      "systemInfoName": "QCOM_GNSS_default",
      "gnssCapabilities": 1023,
      "powerCapabilities": 1,
      "measurementCorrectionsCapabilities": 1,
      "yearOfHw": 2025,
      "defaultSatellites": 8,
      "dualFrequencyL5": false
    },
    "thermal": {
      "skinName": "SKIN",
      "skinCelsius": 30.512,
      "modelMaximumCelsius": 50,
      "coolingDeviceName": "devfreq-5000000.qcom,kgsl-3d0",
      "sensors": [
        {"id": 0, "name": "CPU", "value": 33.421, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 1, "name": "GPU", "value": 32.807, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 2, "name": "BATTERY", "value": 30.0, "thresholds": "nan;nan;nan;50;55;59;60", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 3, "name": "SKIN", "value": 30.512, "thresholds": "nan;43;45;50;70;80;90", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 5, "name": "POWER_AMPLIFIER", "value": 31.204, "thresholds": "nan;nan;nan;68;90;100;110", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 6, "name": "NPU", "value": 33.421, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 9, "name": "SOC", "value": 33.421, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"}
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
      {"handle": 1, "name": "icm456xx_acc", "vendor": "TDK InvenSense", "version": 1, "typeId": 1, "type": "android.sensor.accelerometer", "maxRange": "78.453605652", "resolution": "0.001197", "power": "0.145", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 2, "name": "ak0991x", "vendor": "AKM", "version": 1, "typeId": 2, "type": "android.sensor.magnetic_field", "maxRange": "4912.050292969", "resolution": "0.150000006", "power": "0.28", "minDelay": 10000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 3, "name": "ORIENTATION", "vendor": "Qualcomm", "version": 1, "typeId": 3, "type": "android.sensor.orientation", "maxRange": "360.0", "resolution": "0.00390625", "power": "0.43", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 4, "name": "icm456xx_gyro", "vendor": "TDK InvenSense", "version": 1, "typeId": 4, "type": "android.sensor.gyroscope", "maxRange": "34.906299591", "resolution": "0.001065", "power": "0.145", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 5, "name": "stk3bfx_l", "vendor": "Sensortek", "version": 1, "typeId": 5, "type": "android.sensor.light", "maxRange": "65535.0", "resolution": "1.0", "power": "0.09", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 6, "name": "lps22df_bar", "vendor": "STMicroelectronics", "version": 1, "typeId": 6, "type": "android.sensor.pressure", "maxRange": "1260.0", "resolution": "0.000244141", "power": "0.09", "minDelay": 2500, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 0},
      {"handle": 8, "name": "stk3bfx_p", "vendor": "Sensortek", "version": 1, "typeId": 8, "type": "android.sensor.proximity", "maxRange": "5.0", "resolution": "1.0", "power": "0.09", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 1000000, "wakeUp": true, "reporting": 1},
      {"handle": 9, "name": "GRAVITY", "vendor": "Qualcomm", "version": 1, "typeId": 9, "type": "android.sensor.gravity", "maxRange": "39.226802826", "resolution": "0.001197", "power": "0.145", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 10, "name": "LINEARACCEL", "vendor": "Qualcomm", "version": 1, "typeId": 10, "type": "android.sensor.linear_acceleration", "maxRange": "39.226802826", "resolution": "0.001197", "power": "0.145", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 11, "name": "ROTATION_VECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 11, "type": "android.sensor.rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.43", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 14, "name": "UNCALI_MAG", "vendor": "Qualcomm", "version": 1, "typeId": 14, "type": "android.sensor.magnetic_field_uncalibrated", "maxRange": "4912.050292969", "resolution": "0.150000006", "power": "0.28", "minDelay": 20000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 15, "name": "GAME_ROTATION_VECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 15, "type": "android.sensor.game_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.29", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 16, "name": "UNCALI_GYRO", "vendor": "Qualcomm", "version": 1, "typeId": 16, "type": "android.sensor.gyroscope_uncalibrated", "maxRange": "34.906299591", "resolution": "0.001065", "power": "0.145", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 17, "name": "SIGNIFICANT_MOTION", "vendor": "Qualcomm", "version": 1, "typeId": 17, "type": "android.sensor.significant_motion", "maxRange": "1.0", "resolution": "1.0", "power": "0.145", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 18, "name": "STEP_DETECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 18, "type": "android.sensor.step_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.145", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 0, "wakeUp": false, "reporting": 3},
      {"handle": 19, "name": "STEP_COUNTER", "vendor": "Qualcomm", "version": 1, "typeId": 19, "type": "android.sensor.step_counter", "maxRange": "2147483648.0", "resolution": "1.0", "power": "0.145", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 20, "name": "GEOMAGNETIC_ROTATION_VECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 20, "type": "android.sensor.geomagnetic_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.28", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 22, "name": "TILT_DETECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 22, "type": "android.sensor.tilt_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.145", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 3},
      {"handle": 27, "name": "DEVICE_ORIENTATION", "vendor": "Qualcomm", "version": 1, "typeId": 27, "type": "android.sensor.device_orientation", "maxRange": "3.0", "resolution": "1.0", "power": "0.145", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 35, "name": "UNCALI_ACC", "vendor": "Qualcomm", "version": 1, "typeId": 35, "type": "android.sensor.accelerometer_uncalibrated", "maxRange": "39.226802826", "resolution": "0.001197", "power": "0.145", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0}
    ],
    "audioEffects": [],
    "media": {
      "instances": [
        {"name": "qti-c2", "binary": "vendor.qti.media.c2@1.0-service", "store": "vendor", "rank": 1}
      ],
      "components": [
        {"name": "c2.qti.avc.decoder", "kind": "decoder", "mediaType": "video/avc", "maxSize": "4096x2176", "store": "vendor", "rank": 1},
        {"name": "c2.qti.hevc.decoder", "kind": "decoder", "mediaType": "video/hevc", "maxSize": "4096x2176", "store": "vendor", "rank": 1},
        {"name": "c2.qti.vp9.decoder", "kind": "decoder", "mediaType": "video/x-vnd.on2.vp9", "maxSize": "4096x2176", "store": "vendor", "rank": 1},
        {"name": "c2.qti.avc.encoder", "kind": "encoder", "mediaType": "video/avc", "maxSize": "4096x2176", "store": "vendor", "rank": 1},
        {"name": "c2.qti.hevc.encoder", "kind": "encoder", "mediaType": "video/hevc", "maxSize": "4096x2176", "store": "vendor", "rank": 1},
        {"name": "c2.qti.heic.encoder", "kind": "encoder", "mediaType": "image/vnd.android.heic", "maxSize": "16384x16384", "store": "vendor", "rank": 1}
      ]
    },
    "audioEngine": {
      "modules": "default,bluetooth,r_submix",
      "voiceCallIndexMax": 7,
      "musicIndexMax": 15
    }
  },
  "alignmentNotes": [
    "平台为 QTI/Adreno：radioVariant 取 default（非 mtk），GNSS 走 vendor.honor.hardware.gnss.IHnGnss + QTI ILocAidlGnss 双栈，yearOfHw 取 2025。",
    "CPU 以固件实测 6+2 簇为准（policy0=6×A55 capacity 868，policy6=2×A78 capacity 1024），频点表保留固件实测提示 1113600 kHz，驱动/调速器按高通 qcom-cpufreq-hw + schedutil。",
    "传感器仅登记固件 chipOwners 实测芯片（icm456xx/ak0991x/stk3bfx/lps22df），融合类用 Qualcomm Sensor HAL 命名；未编造 xiaomi.sensor.*，也未引入小米专属 swtp 节点。",
    "Honor 私有扩展（HN 系列 HAL、HnGnss、hnchipsrv 等）以 VINTF 占位为准，虚拟 HAL 中通过 vintfDrop 收敛芯片私有服务；品牌未见 MagicOS 私有 sensor type 暴露，故不虚构 honor.sensor.*。",
    "固件 hasWiredHeadset=false：不配置 3.5mm 耳机插孔 switch 的 SW_JACK_PHYSICAL_INSERT，headsetJackName 仍按 ALSA 声卡名 chora-snd-card 对齐。",
    "Codec2 以 vendor.qti.media.c2@1.0-service 建立实例，所有假登记组件统一 rank=1/store=vendor，不占用 default。",
    "无红外（hasConsumerIr=false，enableIr=false）、有 NFC（hasNfc=true，ST21NFC 系）；振动按 Z 轴线性马达配置原语集。",
    "机型别名 ABR-LX1、IMEI TAC、Wi-Fi OUI、Widevine systemId、GPU 驱动日期串属真机侧最终值，须以 ro.product.model / getDeviceId / DRM Info / dumpsys SurfaceFlinger 复核后回填。"
  ],
  "sources": [
    "目标机官方固件解包实测：build.prop、boot/DTB cmdline、VINTF manifest 占位、vendor/etc（audio/sensors/media/gnss/nfc/wifi）",
    "Honor 400 Lite 公开规格：6.7 英寸 AMOLED 1080×2412 120Hz、5230mAh/35W、108MP 主摄、侧边指纹",
    "Qualcomm SM6375-AC（Snapdragon 6s Gen 3）平台资料：2×Cortex-A78 + 6×Cortex-A55、Adreno 619、Vulkan 1.3",
    "redmi/gold 标杆配置结构（仅取字段口径，数值全部替换为本机实测/平台值）"
  ]
}
```
