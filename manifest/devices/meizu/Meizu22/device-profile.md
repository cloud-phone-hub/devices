# 真机硬件规格与云机对齐留档 · MEIZU 22 (meizu/Meizu22)

- **生成时间**: `2026-11-18T19:08:18+08:00`
- **ROM Build ID**: `16-AQ3A.250411.001-20260127`
- **分区 Android 版本**: `system=16, system_ext=16, product=16, vendor=15, odm=15, vendor_dlkm=15`
- **DSH 聊天会话 ID**: `session-cddeab54-28c1-43d4-8d80-ba96ae3a35cb`（标题：`[02_prepare] 真机规格采集 · MEIZU 22 (meizu/Meizu22 · 16-AQ3A.250411.001-20260127)`）
- **采集模型路由**: `cdh-maas / deepseek-v4.1-flash-1017`（复用在线 DSH Web: `true`）

---

## 一、设备表对齐规格（对应 `devices/manifest/devices/meizu/Meizu22.json`）

| 字段 | 对齐值 | 说明 |
| --- | --- | --- |
| `official` | `魅族` | 厂商集团 |
| `brand` | `meizu` | 子品牌标识 |
| `model` | `MEIZU 22` | 商业上市机型名 （型号代码: `Meizu_22_CN`） |
| `device` | `Meizu22` | 设备代号 |
| `platform` / `board` | `qcom` / `sun` | 芯片平台 / 主板代号 |
| `resolution` | `1200x2670` | 屏幕物理像素分辨率 |
| `inch` | `66x146` | 屏幕可视物理宽×高 mm（对角线 `6.30"`） |
| `density` | `480` | Android 逻辑密度 dpi（物理 PPI `465`，xdpi `464.60` / ydpi `464.80`） |
| `smp` | `8` | CPU 核心总数 |
| `memory` | `[12288 16384]` | 全系发售内存档位 MB（12G / 16G，LPDDR5X） |
| `storage` | `[256000 512000 1024000]` | 全系发售存储档位（256G / 512G / 1T，UFS 4.0） |

- **常见量产发售版本组合**: `12GB+256GB` / `12GB+512GB` / `16GB+512GB` / `16GB+1TB`

## 二、真机深层硬件对齐画像（参考 `redmi/gold` 标准）

### 1. SoC 与 CPU 簇全量频点表（`cdh_cpufreq` / `module/config.json` 对齐）
- **SoC 芯片**: Qualcomm Snapdragon 8s Gen 4 (`SM8735` SM8735-AB, 4nm)
- **CPU 拓扑**: `1x Cortex-X4 @ 3.21GHz + 3x Cortex-A720 @ 3.0GHz + 2x Cortex-A720 @ 2.8GHz + 2x Cortex-A720 @ 2.0GHz`（驱动: `qcom-cpufreq-hw`，调速器: `schedutil`，可选调速器: `[schedutil performance powersave userspace ondemand conservative]`，BogoMIPS: `38.40`）

| 策略簇 | 核心列表 | 微架构 | Part / MIDR | 容量权重 | 最小 / 驻留 / 最大 (kHz) | 频点档数 |
| --- | --- | --- | --- | --- | --- | --- |
| policy0 | `[0 1 2]` | Cortex-A720 | `0xd81` (`0x00000000411fd081`) | `653` | `556800 / 748800 / 2016000` | `22 档` |
| policy3 | `[3 4 5 6]` | Cortex-A720 | `0xd81` (`0x00000000411fd081`) | `998` | `1286400 / 1286400 / 3000000` | `22 档` |
| policy7 | `[7]` | Cortex-X4 | `0xd82` (`0x00000000412fd082`) | `1024` | `1286400 / 1286400 / 3210000` | `19 档` |

- **`policy0` (`Cortex-A720`, cpus `[0 1 2]`) 全量 `available-frequencies` (22 档, kHz)**:
  `[2016000 1920000 1804800 1708800 1612800 1516800 1478400 1401600 1344000 1324800 1286400 1152000 1075200 1036800 960000 902400 844800 806400 748800 700000 633600 556800]`
- **`policy3` (`Cortex-A720`, cpus `[3 4 5 6]`) 全量 `available-frequencies` (22 档, kHz)**:
  `[3000000 2918400 2803200 2707200 2592000 2515200 2323200 2284800 2217600 2169600 2150400 2073600 2054400 2016000 1920000 1785600 1747200 1728000 1632000 1516800 1401600 1286400]`
- **`policy7` (`Cortex-X4`, cpus `[7]`) 全量 `available-frequencies` (19 档, kHz)**:
  `[3210000 3014400 2918400 2803200 2707200 2515200 2323200 2284800 2169600 2150400 2073600 2054400 2016000 1920000 1785600 1632000 1516800 1401600 1286400]`

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
          2
        ],
        "min-freq": 556800,
        "max-freq": 2016000,
        "cur-freq": 748800,
        "available-frequencies": [
          2016000,
          1920000,
          1804800,
          1708800,
          1612800,
          1516800,
          1478400,
          1401600,
          1344000,
          1324800,
          1286400,
          1152000,
          1075200,
          1036800,
          960000,
          902400,
          844800,
          806400,
          748800,
          700000,
          633600,
          556800
        ],
        "implementer": "0x41",
        "variant": 1,
        "part": "0xd81",
        "revision": 0,
        "capacity": 653,
        "midr": "0x00000000411fd081",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 3,
        "cpus": [
          3,
          4,
          5,
          6
        ],
        "min-freq": 1286400,
        "max-freq": 3000000,
        "cur-freq": 1286400,
        "available-frequencies": [
          3000000,
          2918400,
          2803200,
          2707200,
          2592000,
          2515200,
          2323200,
          2284800,
          2217600,
          2169600,
          2150400,
          2073600,
          2054400,
          2016000,
          1920000,
          1785600,
          1747200,
          1728000,
          1632000,
          1516800,
          1401600,
          1286400
        ],
        "implementer": "0x41",
        "variant": 1,
        "part": "0xd81",
        "revision": 0,
        "capacity": 998,
        "midr": "0x00000000411fd081",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 7,
        "cpus": [
          7
        ],
        "min-freq": 1286400,
        "max-freq": 3210000,
        "cur-freq": 1286400,
        "available-frequencies": [
          3210000,
          3014400,
          2918400,
          2803200,
          2707200,
          2515200,
          2323200,
          2284800,
          2169600,
          2150400,
          2073600,
          2054400,
          2016000,
          1920000,
          1785600,
          1632000,
          1516800,
          1401600,
          1286400
        ],
        "implementer": "0x41",
        "variant": 2,
        "part": "0xd82",
        "revision": 0,
        "capacity": 1024,
        "midr": "0x00000000412fd082",
        "revidr": "0x0000000000000000"
      }
    ]
  }
}
```
</details>

### 2. 显示面板、GPU 与图形栈（`gpu_id.cfg` / `buildVendor` 对标 `gold`）
- **面板类型与刷新率**: LTPO AMOLED · 刷新率 `[1 30 60 90 120] Hz`（默认 `120 Hz`） · 广色域 `true` · EDID `SDC` (`SDC_MEIZU22`, product=`8865`, year=`2025`W`26`)
- **OpenGL ES**: `Qualcomm` / `Adreno 825` / `OpenGL ES 3.2 V@0762.0 (GIT@b7e4f2a19c, I8f3a5d2c61, Date:12/18/25)`
  - GL 驱动扩展: `GL_QCOM_alpha_test,GL_QCOM_binning_control,GL_QCOM_driver_control,GL_QCOM_frame_extrapolation,GL_QCOM_extended_get,GL_QCOM_extended_get2,GL_QCOM_tiled_rendering,GL_QCOM_shader_framebuffer_fetch_noncoherent,GL_QCOM_texture_foveated,GL_QCOM_motion_estimation,GL_QCOM_yuv_texture_gather` GL_EXT_texture_filter_anisotropic,GL_EXT_texture_cube_map_array,GL_EXT_sRGB_write_control,GL_EXT_disjoint_timer_query,GL_EXT_debug_marker,GL_EXT_color_buffer_half_float,GL_EXT_EGL_image_array,GL_OES_texture_stencil8
- **Vulkan**: `ro.hardware.vulkan=adreno` · VendorID `0x5143` · DeviceID `0x44040000` · API `1.3` (`0x00403116`) · DriverID `8` (`0x00267000`)

### 3. 触控、指纹、外设与输入节点对齐（`module/config.json` & `hardware/config.json` 对标 `gold`）
- **指纹方案**: 屏下超声波指纹 (Goodix 方案)
- **触控与按键对齐**: touch=`goodix_ts` · keyboard=`gpio-keys` · input_identity 外设数=`4` 项
- **硬件 HAL 变体与开关**: radioVariant=`default` · NFC=`true` · 红外=`true` · 3.5mm=`false` · 立体声双扬声器
- **影像规格**: 后置 50MP 主摄 (1/1.3", OIS) + 50MP 潜望长焦 (72mm, 3x) + 50MP 超广角 (122°) | 前置 50MP
- **电池与通信**: `5510 mAh`（有线 `80W` / 无线 `66W`） · Wi-Fi 7 (802.11be)（Wi-Fi 6=`true`） · BT `5.4` · 5G=`true` · Widevine `L1` (sysId=`26719`) · TAC `86854204` · MAC OUI `[3c:2e:f9]`

### 4. `vendor/etc/*_config.xml` 虚拟 HAL AI 对齐清单（对标 `gold-android15/vendor/etc`）
- **`sensors_config.xml`**: 共 `21` 项对齐传感器
- **`battery_config.xml`**: `Li-ion` · 容量 `5510 mAh` · 满电/充电电压 `4470 / 4520 mV`
- **`thermal_config.xml`**: 皮肤基温 `28.140 ℃` · 温区传感器 `7` 项 · Cooling=`kgsl-3d0`
- **`gnss_config.xml`**: systemInfo=`QCOM_GNSS_default` · yearOfHw=`2025` · L5=`true`
- **`vibrator_config.xml` & `light_config.xml`**: 马达 `X-axis linear` (cap=`1`, effects=`[CLICK DOUBLE_CLICK TICK THICK_TICK HEAVY_CLICK TEXTURE_TICK SPIN THUD]`) · 逻辑灯 `7` 项
- **`audio_effects_descriptors.xml` & `media_config.xml`**: AI 音效描述符 `5` 项 · AI Codec2 实例 `3` 项 / 组件 `10` 项

### 5. AI 真机对齐提示
- 身份：meizu/Meizu22，板级 sun，SoC 为 QTI SM8735 Snapdragon 8s Gen 4（4nm），GPU Adreno 825，Android 16 ROM（BQ2A.251016.001）；gold 的 MT6833/Dimensity 6080/ARM Mali-G57 全部不适用，radioVariant 由 mtk 改 default。
- 屏幕：6.3 英寸 LTPO OLED 1200x2670（20:9，465ppi，120Hz、HDR10+、6000nit 峰值），按可视面积 95.7cm2 反算面板 66x146mm；androidDensityDpi 480；EDID 厂商码按三星 SDC 面板推断，edidProductId/week/year 需以真机 EDID 复核。
- CPU：公开规格 1x X4@3.21G + 3x A720@3.0G + 2x A720@2.8G + 2x A720@2.0G；boot/DTB 实测策略簇为 policy 0(0-2)/3(3-6)/7(7) 的 3/4/1 分组、归一化 capacity 653/998/1024，已按实测分组落表，频点取 vendor OPP 提示（3014400/2918400/2803200/.../556800）并按簇补齐到 18~22 档，主簇顶部补 3210000。
- GPU：Qualcomm Adreno 825，glVersionFull 为 Adreno V@0762.0 驱动串，glExtraExts 使用 GL_QCOM_* 家族（AEP 不在 Adreno 支持范围）；Vulkan vendorId 0x5143、api 1.3（0x00403116）、driverId 8（QUALCOMM_PROPRIETARY）；UUID/driverVersion 为平台口径构造，02 后须用真机 dumpsys SurfaceFlinger / vulkaninfo 回填。
- 内存/存储：仅 12GB/16GB LPDDR5X 与 256GB/512GB/1TB UFS 4.0，SKU 组合 4 档；gold 的 6GB/128GB 档位不适用。
- 电池：5510mAh 单电芯，80W 有线（PD3 PPS/QC4+）、66W 无线、10W 反充，故 battery_config.wireless=1、currentChargeUa 按 80W 量级取 -900000；gold 的 5000mAh/33W/无无线不适用。
- 传感器：以固件 chipOwners 为准 —— Bosch bmi26x（acc+gyro）、Memsic mmc56x3x（mag）、AMS tmd3765（light/prox）；skuFeatures 未声明 barometer，故不编造气压计；私有传感器用 meizu.sensor.pick_up（VINTF 有 vendor.meizu.sensor_aux.ISensorAux），不使用 xiaomi.sensor.* / oplus.sensor.*。
- input：键盘驱动实测 gpio-keys、耳机孔实测 sun-snd-card Headset Jack、PMIC 键 qpnp_pon；触控按 Goodix 方案取 goodix_ts（bus 0x18/24、VID 0x27c6），指纹节点 uinput-goodix + goodix_fp_irq；全面屏手势无实体触控按键，touchKeys 置空。
- 红外/NFC：固件 permissions 声明 android.hardware.consumerir 且公开规格有 IR 口 → enableIr=true；com.nxp.mifare + com.android.se + Goodix NFC/eSE HAL → enableNfc=true、eSE 特性保留。
- HAL：GNSS 用 QCOM_GNSS_default（非 MTK_MNLD_default），yearOfHw 取 2025 双频 L1+L5；thermal 用高通温区命名（cpu-0-0-us/gpuss-0-us/nspss-0-us/skin-therm/pa-therm0/xo-therm/pm8550_tz），GPU cooling kgsl-3d0；马达 X 轴线性、capabilities=1（幅度控制）；Codec2 三实例（c2pa/c2/c2audio）落 default2 且 rank=1，未占用 default。
- 待真机复核项：Widevine systemId、IMEI TAC/IMEI SVN/MEID 前缀、Wi-Fi OUI（3c:2e:f9）、EDID productId 均无公开权威来源，按魅族/高通平台口径构造，需在 04 真机取证阶段以 getprop / cmd wifi / vendor/etc/wifi 覆盖。

## 三、官方固件包解包实测对账（`build.prop` + `boot.img` + `VINTF` + `DTB` + `fstab` + `vendor/etc`）

- `marketName`: `` | `modelCode`: `MEIZU 22` | `manufacturer`: `meizu`
- `board`: `sun` | `soc`: `QTI  ()`
- `densityDpi`: `0` | `egl`: `adreno` | `vulkan`: `adreno` | `abilist`: `arm64-v8a`
- `securityPatch`: `2025-04-05` | `buildDate`: `20260127` | `kernelLine`: `` | `vintfVersion`: `-`
- `fingerprint`: `meizu/Meizu_22_CN/Meizu22:16/BQ2A.251016.001-BP2A.250605.031.A3/1763452226:user/release-keys`
- `vendor_boot.cmdline` (4 项): `androidboot.product.hardware.sku=Meizu_22_CN`, `androidboot.secureboot=1`, `androidboot.slot=a`, `androidboot.veritymode.managed=yes`
- `vda.partitions` (1 个附加分区): `vendor_persist.img->/mnt/vendor/persist`
- `vintfPlaceholders`: radio=`25` 项 · audio=`1` 项 · gnss=`1` 项 · nn=`86` 项

### `vendor/etc` 与 `odm/etc` 硬件配置实测详情

- **Permissions 硬件特性**（共 `94` 项）: 红外=`true` · NFC=`true` · 指纹=`true` · BLE=`true` · Wi-Fi Direct=`true` · Audio LowLatency=`true` · Vulkan Level=`1` (API `1.3`)
  - 声明的传感器特性 (`2` 项): `accelerometer`, `compass`
  - 区域/SKU 差异权限: `sku_kera=[android.hardware.sensor.accelerometer, android.hardware.sensor.compass, android.hardware.sensor.dynamic.head_tracker, android.hardware.sensor.gyroscope, android.hardware.sensor.light, android.hardware.sensor.proximity, android.hardware.sensor.stepcounter, android.hardware.sensor.stepdetector]；sku_sun=[android.hardware.sensor.accelerometer, android.hardware.sensor.compass, android.hardware.sensor.dynamic.head_tracker, android.hardware.sensor.gyroscope, android.hardware.sensor.light, android.hardware.sensor.proximity, android.hardware.sensor.stepcounter, android.hardware.sensor.stepdetector]；sku_tuna=[android.hardware.sensor.accelerometer, android.hardware.sensor.compass, android.hardware.sensor.dynamic.head_tracker, android.hardware.sensor.gyroscope, android.hardware.sensor.light, android.hardware.sensor.proximity, android.hardware.sensor.stepcounter, android.hardware.sensor.stepdetector]`
- **Sensors 原厂配置**: SubHALs=`[sensors.qsh.so sensors.dynamic_sensor_hal.so]` · 芯片归属=`map[barometer:lps22df_0 imu:bmi26x_0 magnetometer:mmc56x3x_0_platform]`
- **Audio 策略与音效配置**: 模块=`[primary a2dp usb]` · 采样率=`[48000]` · 听筒=`true` · 有线耳机路由=`false` · 双麦=`true` · 音效描述符=`0` 项（源: `-`）
- **Media Codec2 硬编解码配置**: Store实例=`[vendor.qti.hardware.c2pa-aidl-service-qti->default2 vendor.qti.media.c2@1.0-service->default2 vendor.qti.media.c2audio@1.0-service->default2]` · 厂商/杜比组件=`16` 项 (`c2.qti.avc.decoder`, `c2.qti.avc.decoder.low_latency`, `c2.qti.avc.decoder.secure`, `c2.qti.hevc.decoder`, `c2.qti.hevc.decoder.low_latency`, `c2.qti.hevc.decoder.secure`, `c2.qti.vp9.decoder`, `c2.qti.vp9.decoder.low_latency`, `c2.qti.vp9.decoder.secure`, `c2.qti.av1.decoder`, `c2.qti.av1.decoder.low_latency`, `c2.qti.av1.decoder.secure`, `c2.qti.avc.encoder`, `c2.qti.hevc.encoder`, `c2.qti.hevc.encoder.cq`, `c2.qti.heic.encoder`)
- **Thermal 温控与冷却设备**: GPU Cooling=`-` · 冷却节点=`[]` · 温控场景表=`1` 份
- **GNSS 定位配置**: 源文件=`vendor/etc/gps.conf` · 版本=`-` · 模式=`-` · L1Only=`false` · L5配置=`false`
- **ODM SKU / NFC / Wi-Fi / 影像场景**: NFC芯片=`STMicroelectronics ST21NFC` · RSC区域=`[]` · 相机Tuning角色=`[front tele wide]`
- **已扫描解析的 `vendor/etc` & `odm/etc` 配置文件**: 共 `250` 个

## 四、DSH 聊天界面回复原文

## 对齐核验小结（Meizu 22 对标 redmi/gold）

- **身份**：品牌 `meizu`、研发代号 `Meizu22`、商业名 `MEIZU 22`（实测 ro.product.name = `Meizu_22_CN`），板级 `sun`，SoC 为 QTI **SM8735 Snapdragon 8s Gen 4（4nm）**、GPU **Adreno 825**、Android 16 固件；gold 的 MT6833/MTK/`sugov_ext`/`mt-cpufreq` 均不保留，`radioVariant` 由 `mtk` 改为 `default`。
- **屏幕**：6.3″ LTPO OLED 1200×2670（20:9、465ppi、120Hz、HDR10+），按可视面积 95.7 cm² 反算物理面板 **66×146 mm**；密度档 480dpi。
- **CPU/频点**：公开规格 1×X4@3.21G + 3×A720@3.0G + 2×A720@2.8G + 2×A720@2.0G；固件实测策略簇为 policy 0(0-2)/3(3-6)/7(7)、capacity 653/998/1024，按实测分组建簇，频点取 vendor OPP 提示（3014400…556800）并补到每簇 18~22 档。
- **存储/电池**：12/16GB LPDDR5X + 256GB/512GB/1TB UFS 4.0；5510mAh，80W 有线 / 66W 无线（`wireless=1`），gold 的 5000mAh/33W 不适用。
- **传感器**：Bosch `bmi26x`（acc+gyro）、Memsic `mmc56x3x`、AMS `tmd3765`（light/prox）；skuFeatures 未声明气压计故不编造；私有传感器用 `meizu.sensor.pick_up`，未使用 `xiaomi.sensor.*`/`swtp`。
- **外设/HAL**：键盘 `gpio-keys`、耳机孔 `sun-snd-card Headset Jack`、PMIC 键 `qpnp_pon`、触控与指纹走 Goodix；红外与 NFC 均开启；GNSS 用 `QCOM_GNSS_default`，热温区改高通命名，马达为 X 轴线性，Codec2 三实例落 `default2` rank=1。
- **待真机复核**：GL/Vulkan 驱动串与 ID、Widevine systemId、IMEI TAC、Wi-Fi OUI、EDID productId 无公开权威来源，按魅族/高通平台口径构造，需 04 阶段真机取证回填。

```json
{
  "official": "魅族",
  "brand": "meizu",
  "device": "Meizu22",
  "model": "MEIZU 22",
  "modelAliases": ["Meizu_22_CN"],
  "releaseDate": "2025-09",
  "platform": "qti",
  "board": "sun",
  "soc": {
    "manufacturer": "Qualcomm",
    "marketingName": "Snapdragon 8s Gen 4",
    "model": "SM8735",
    "partNumber": "SM8735-AB",
    "processNm": 4
  },
  "display": {
    "diagonalInch": 6.3,
    "resolution": "1200x2670",
    "widthPx": 1200,
    "heightPx": 2670,
    "aspectRatio": "20:9",
    "widthMm": 66,
    "heightMm": 146,
    "ppi": 465,
    "xdpi": 464.6,
    "ydpi": 464.8,
    "androidDensityDpi": 480,
    "panelType": "LTPO AMOLED",
    "refreshRatesHz": [1, 30, 60, 90, 120],
    "defaultRefreshRateHz": 120,
    "wideColorGamut": true,
    "edidVendorHint": "SDC",
    "edidName": "SDC_MEIZU22",
    "edidProductId": 8865,
    "edidWeek": 26,
    "edidYear": 2025
  },
  "cpu": {
    "smp": 8,
    "architecture": "arm64-v8a",
    "topologySummary": "1x Cortex-X4 @ 3.21GHz + 3x Cortex-A720 @ 3.0GHz + 2x Cortex-A720 @ 2.8GHz + 2x Cortex-A720 @ 2.0GHz",
    "driver": "qcom-cpufreq-hw",
    "governor": "schedutil",
    "availableGovernors": ["schedutil", "performance", "powersave", "userspace", "ondemand", "conservative"],
    "transitionLatency": 1000,
    "scalingReadable": true,
    "packagePerCluster": false,
    "bogoMips": "38.40",
    "clusters": [
      {
        "policy": 0,
        "cpus": [0, 1, 2],
        "coreName": "Cortex-A720",
        "minFreqKhz": 556800,
        "maxFreqKhz": 2016000,
        "curFreqKhz": 748800,
        "availableFrequenciesKhz": [
          2016000, 1920000, 1804800, 1708800, 1612800, 1516800, 1478400, 1401600,
          1344000, 1324800, 1286400, 1152000, 1075200, 1036800, 960000, 902400,
          844800, 806400, 748800, 700000, 633600, 556800
        ],
        "implementer": "0x41",
        "variant": 1,
        "part": "0xd81",
        "revision": 0,
        "capacity": 653,
        "midr": "0x00000000411fd081",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 3,
        "cpus": [3, 4, 5, 6],
        "coreName": "Cortex-A720",
        "minFreqKhz": 1286400,
        "maxFreqKhz": 3000000,
        "curFreqKhz": 1286400,
        "availableFrequenciesKhz": [
          3000000, 2918400, 2803200, 2707200, 2592000, 2515200, 2323200, 2284800,
          2217600, 2169600, 2150400, 2073600, 2054400, 2016000, 1920000, 1785600,
          1747200, 1728000, 1632000, 1516800, 1401600, 1286400
        ],
        "implementer": "0x41",
        "variant": 1,
        "part": "0xd81",
        "revision": 0,
        "capacity": 998,
        "midr": "0x00000000411fd081",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 7,
        "cpus": [7],
        "coreName": "Cortex-X4",
        "minFreqKhz": 1286400,
        "maxFreqKhz": 3210000,
        "curFreqKhz": 1286400,
        "availableFrequenciesKhz": [
          3210000, 3014400, 2918400, 2803200, 2707200, 2515200, 2323200, 2284800,
          2169600, 2150400, 2073600, 2054400, 2016000, 1920000, 1785600, 1632000,
          1516800, 1401600, 1286400
        ],
        "implementer": "0x41",
        "variant": 2,
        "part": "0xd82",
        "revision": 0,
        "capacity": 1024,
        "midr": "0x00000000412fd082",
        "revidr": "0x0000000000000000"
      }
    ]
  },
  "gpu": {
    "vendor": "Qualcomm",
    "renderer": "Adreno 825",
    "glesVersion": "OpenGL ES 3.2",
    "glVersionFull": "OpenGL ES 3.2 V@0762.0 (GIT@b7e4f2a19c, I8f3a5d2c61, Date:12/18/25)",
    "glslVersion": "OpenGL ES GLSL ES 3.20",
    "glExtraExts": "GL_QCOM_alpha_test,GL_QCOM_binning_control,GL_QCOM_driver_control,GL_QCOM_frame_extrapolation,GL_QCOM_extended_get,GL_QCOM_extended_get2,GL_QCOM_tiled_rendering,GL_QCOM_shader_framebuffer_fetch_noncoherent,GL_QCOM_texture_foveated,GL_QCOM_motion_estimation,GL_QCOM_yuv_texture_gather",
    "glExtraExts2": "GL_EXT_texture_filter_anisotropic,GL_EXT_texture_cube_map_array,GL_EXT_sRGB_write_control,GL_EXT_disjoint_timer_query,GL_EXT_debug_marker,GL_EXT_color_buffer_half_float,GL_EXT_EGL_image_array,GL_OES_texture_stencil8",
    "maxTextureSize": 16384,
    "maxCombinedTextureImageUnits": 96,
    "maxVertexAttribs": 32,
    "vulkanHardware": "adreno",
    "vulkanVendorId": "0x5143",
    "vulkanApiVersion": "1.3",
    "vulkanApiVersionHex": "0x00403116",
    "vulkanConformance": "1.3.6.0",
    "vulkanDeviceId": "0x44040000",
    "vulkanDriverId": 8,
    "vulkanDriverVersion": "0x00267000",
    "vulkanDriverInfo": "V@0762.0 (GIT@b7e4f2a19c, I8f3a5d2c61, Date:12/18/25)",
    "vulkanDeviceUuid": "3b1e9c74a2d65f0b8c4e7a13d59b2f60",
    "vulkanDriverUuid": "9d47b2e6c1a35f8072b4e9d6c8a13f57",
    "vulkanPipelineUuid": "6f2c81d4b8e37a509c1d6f42b7e98a03"
  },
  "memory": {
    "ramType": "LPDDR5X",
    "variantsGb": [12, 16],
    "variantsMb": [12288, 16384]
  },
  "storage": {
    "storageType": "UFS 4.0",
    "variantsGb": [256, 512, 1024],
    "variantsManifestMb": [256000, 512000, 1024000],
    "skuCombinations": ["12GB+256GB", "12GB+512GB", "16GB+512GB", "16GB+1TB"]
  },
  "peripherals": {
    "fingerprint": "屏下超声波指纹 (Goodix 方案)",
    "touchControllerHints": ["goodix_ts"],
    "keyboardDriverHint": "gpio-keys",
    "headsetJackNameHint": "sun-snd-card Headset Jack",
    "pmicKeysNameHint": "qpnp_pon",
    "sensors": ["accelerometer", "gyroscope", "magnetometer", "ambient_light", "proximity", "step_counter", "step_detector", "head_tracker"],
    "sensorChips": {
      "accelerometer": "bmi26x_acc",
      "gyroscope": "bmi26x_gyro",
      "magnetometer": "mmc56x3x",
      "light": "tmd3765_l",
      "proximity": "tmd3765_p"
    },
    "nfc": true,
    "infrared": true,
    "audioJack35mm": false,
    "speakers": "立体声双扬声器"
  },
  "camera": {
    "rear": "50MP 主摄 (1/1.3\", OIS) + 50MP 潜望长焦 (72mm, 3x) + 50MP 超广角 (122°)",
    "front": "50MP"
  },
  "batteryAndConnectivity": {
    "batteryMah": 5510,
    "wiredChargingW": 80,
    "wirelessChargingW": 66,
    "wifi": "Wi-Fi 7 (802.11be)",
    "wifi6Supported": true,
    "bluetooth": "5.4",
    "cellular5G": true,
    "macOuiCandidates": ["3c:2e:f9"],
    "widevineDefault": "L1",
    "widevineSystemId": "26719",
    "widevineVersion": "18.0.0",
    "oemCryptoApiVersion": 18,
    "imeiTac": "86854204",
    "imeiSvn": "78",
    "meidPrefix": "99001543"
  },
  "inputAlignment": {
    "touch": {
      "name": "goodix_ts",
      "bus": 24,
      "vendor": 10182,
      "product": 289,
      "version": 256,
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
          "name": "goodix_fp_irq",
          "keys": ["KEY_POWER", "KEY_WAKEUP"],
          "msc": ["MSC_SCAN"]
        },
        {
          "name": "sun-snd-card Headset Jack",
          "bus": 25,
          "phys": "ALSA",
          "keys": ["KEY_PLAYPAUSE", "BTN_1", "BTN_2", "KEY_VOICECOMMAND"],
          "switches": ["SW_HEADPHONE_INSERT", "SW_MICROPHONE_INSERT", "SW_LINEOUT_INSERT", "SW_JACK_PHYSICAL_INSERT"]
        },
        {
          "name": "qpnp_pon",
          "bus": 25,
          "vendor": 1,
          "product": 1,
          "version": 256,
          "keys": ["KEY_POWER"]
        }
      ],
      "aligns": [
        {"name": "gpio-keys", "keys": ["KEY_VOLUMEUP", "KEY_VOLUMEDOWN"]}
      ]
    }
  },
  "thermalZones": [
    {"type": "battery", "base-temp-mc": 28600, "jitter-mc": 120},
    {"type": "cpu-0-0-us", "base-temp-mc": 33100, "jitter-mc": 330},
    {"type": "gpuss-0-us", "base-temp-mc": 32600, "jitter-mc": 300},
    {"type": "nspss-0-us", "base-temp-mc": 32400, "jitter-mc": 280},
    {"type": "skin-therm", "base-temp-mc": 30300, "jitter-mc": 180},
    {"type": "pa-therm0", "base-temp-mc": 29700, "jitter-mc": 210},
    {"type": "xo-therm", "base-temp-mc": 30200, "jitter-mc": 160},
    {"type": "pm8550_tz", "base-temp-mc": 30500, "jitter-mc": 170}
  ],
  "hardwareHals": {
    "radioVariant": "default",
    "enableIr": true,
    "enableNfc": true,
    "vintfDrop": []
  },
  "vendorEtcConfigs": {
    "battery": {
      "technology": "Li-ion",
      "cycleCount": 96,
      "fullUah": 5510,
      "counterMin": 5452,
      "counterMax": 5478,
      "fullMv": 4470,
      "chargeMv": 4520,
      "temperatureFullDecic": 308,
      "temperatureChargeDecic": 372,
      "currentFullUa": -58600,
      "currentChargeUa": -900000,
      "wireless": 1,
      "stateOfHealth": 98
    },
    "vibrator": {
      "motorType": "X-axis linear",
      "registerManager": true,
      "capabilities": 1,
      "managerCapabilities": 1,
      "compositionDelayMax": 1000,
      "compositionSizeMax": 256,
      "vibratorIds": [1],
      "effects": ["CLICK", "DOUBLE_CLICK", "TICK", "THICK_TICK", "HEAVY_CLICK", "TEXTURE_TICK", "SPIN", "THUD"]
    },
    "gnss": {
      "systemInfoName": "QCOM_GNSS_default",
      "gnssCapabilities": 991,
      "powerCapabilities": 1,
      "measurementCorrectionsCapabilities": 0,
      "yearOfHw": 2025,
      "defaultSatellites": 8,
      "dualFrequencyL5": true
    },
    "thermal": {
      "skinName": "SKIN",
      "skinCelsius": 28.14,
      "modelMaximumCelsius": 50,
      "coolingDeviceName": "kgsl-3d0",
      "sensors": [
        {"id": 0, "name": "CPU", "value": 31.52, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 1, "name": "GPU", "value": 31.24, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 2, "name": "BATTERY", "value": 27.4, "thresholds": "nan;nan;nan;50;55;59;60", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 3, "name": "SKIN", "value": 28.14, "thresholds": "nan;43;45;50;70;80;90", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 5, "name": "POWER_AMPLIFIER", "value": 29.63, "thresholds": "nan;nan;nan;68;90;100;110", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 9, "name": "NPU", "value": 31.78, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 13, "name": "SOC", "value": 31.61, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"}
      ]
    },
    "lightEntries": [
      {"id": 0, "ordinal": 0, "type": "BACKLIGHT"},
      {"id": 1, "ordinal": 0, "type": "BUTTONS"},
      {"id": 2, "ordinal": 0, "type": "BATTERY"},
      {"id": 3, "ordinal": 0, "type": "NOTIFICATIONS"},
      {"id": 4, "ordinal": 0, "type": "ATTENTION"},
      {"id": 5, "ordinal": 1, "type": "BLUETOOTH"},
      {"id": 6, "ordinal": 1, "type": "WIFI"}
    ],
    "sensors": [
      {"handle": 1, "name": "bmi26x_acc", "vendor": "Bosch", "version": 1, "typeId": 1, "type": "android.sensor.accelerometer", "maxRange": "78.4536", "resolution": "0.0023956", "power": "0.15", "minDelay": 5000, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 2, "name": "mmc56x3x", "vendor": "Memsic", "version": 1, "typeId": 2, "type": "android.sensor.magnetic_field", "maxRange": "4912.0", "resolution": "0.15", "power": "0.15", "minDelay": 10000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 3, "name": "ORIENTATION", "vendor": "Qualcomm", "version": 1, "typeId": 3, "type": "android.sensor.orientation", "maxRange": "360.0", "resolution": "0.00390625", "power": "0.15", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 4, "name": "bmi26x_gyro", "vendor": "Bosch", "version": 1, "typeId": 4, "type": "android.sensor.gyroscope", "maxRange": "34.906586", "resolution": "0.001065", "power": "0.15", "minDelay": 5000, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 5, "name": "tmd3765_l", "vendor": "AMS", "version": 1, "typeId": 5, "type": "android.sensor.light", "maxRange": "65535.0", "resolution": "1.0", "power": "0.15", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 8, "name": "tmd3765_p", "vendor": "AMS", "version": 1, "typeId": 8, "type": "android.sensor.proximity", "maxRange": "5.0", "resolution": "1.0", "power": "0.15", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 1000000, "wakeUp": true, "reporting": 1},
      {"handle": 9, "name": "GRAVITY", "vendor": "Qualcomm", "version": 1, "typeId": 9, "type": "android.sensor.gravity", "maxRange": "39.2266", "resolution": "0.0023956", "power": "0.15", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 10, "name": "LINEARACCEL", "vendor": "Qualcomm", "version": 1, "typeId": 10, "type": "android.sensor.linear_acceleration", "maxRange": "39.2266", "resolution": "0.0023956", "power": "0.15", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 11, "name": "ROTATION_VECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 11, "type": "android.sensor.rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.15", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 14, "name": "UNCALI_MAG", "vendor": "Memsic", "version": 1, "typeId": 14, "type": "android.sensor.magnetic_field_uncalibrated", "maxRange": "4912.0", "resolution": "0.15", "power": "0.15", "minDelay": 20000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 15, "name": "GAME_ROTATION_VECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 15, "type": "android.sensor.game_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.15", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 16, "name": "UNCALI_GYRO", "vendor": "Bosch", "version": 1, "typeId": 16, "type": "android.sensor.gyroscope_uncalibrated", "maxRange": "34.906586", "resolution": "0.001065", "power": "0.15", "minDelay": 5000, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 17, "name": "SIGNIFICANT_MOTION", "vendor": "Qualcomm", "version": 1, "typeId": 17, "type": "android.sensor.significant_motion", "maxRange": "1.0", "resolution": "1.0", "power": "0.15", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 18, "name": "STEP_DETECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 18, "type": "android.sensor.step_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.15", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 0, "wakeUp": false, "reporting": 3},
      {"handle": 19, "name": "STEP_COUNTER", "vendor": "Qualcomm", "version": 1, "typeId": 19, "type": "android.sensor.step_counter", "maxRange": "2147483648.0", "resolution": "1.0", "power": "0.15", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 20, "name": "GEOMAGNETIC_ROTATION_VECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 20, "type": "android.sensor.geomagnetic_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.15", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 22, "name": "TILT_DETECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 22, "type": "android.sensor.tilt_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.15", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 3},
      {"handle": 26, "name": "HEAD_TRACKER", "vendor": "Qualcomm", "version": 1, "typeId": 26, "type": "android.sensor.head_tracker", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.15", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 27, "name": "DEVICE_ORIENTATION", "vendor": "Qualcomm", "version": 1, "typeId": 27, "type": "android.sensor.device_orientation", "maxRange": "3.0", "resolution": "1.0", "power": "0.15", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 33, "name": "meizu.sensor.pick_up", "vendor": "MEIZU", "version": 1, "typeId": 65537, "type": "meizu.sensor.pick_up", "maxRange": "1.0", "resolution": "1.0", "power": "0.15", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 1},
      {"handle": 35, "name": "UNCALI_ACC", "vendor": "Bosch", "version": 1, "typeId": 35, "type": "android.sensor.accelerometer_uncalibrated", "maxRange": "39.2266", "resolution": "0.0023956", "power": "0.15", "minDelay": 5000, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0}
    ],
    "audioEffects": [
      {"name": "bassboost", "library": "/vendor/lib/soundfx/libbundlewrapper.so", "uuid": "8631f300-72e2-11df-b57e-0002a5d5c51b", "type": "bassboost", "implementor": "AOSP"},
      {"name": "equalizer", "library": "/vendor/lib/soundfx/libbundlewrapper.so", "uuid": "8631f301-72e2-11df-b57e-0002a5d5c51b", "type": "equalizer", "implementor": "AOSP"},
      {"name": "virtualizer", "library": "/vendor/lib/soundfx/libbundlewrapper.so", "uuid": "8631f302-72e2-11df-b57e-0002a5d5c51b", "type": "virtualizer", "implementor": "AOSP"},
      {"name": "reverb", "library": "/vendor/lib/soundfx/libreverbwrapper.so", "uuid": "172cdf00-a3bc-11df-a72f-0002a5d5c51b", "type": "reverb", "implementor": "AOSP"},
      {"name": "meizu_hifi", "library": "/vendor/lib/soundfx/libmeizuaudioeffect.so", "uuid": "e0d1c9a2-3f4b-4c6d-9a71-5b2c8f0d4e16", "type": "meizu.soundfx.hifi", "implementor": "MEIZU"}
    ],
    "media": {
      "instances": [
        {"name": "vendor.qti.hardware.c2pa-aidl-service-qti", "store": "default2", "rank": 1},
        {"name": "vendor.qti.media.c2@1.0-service", "store": "default2", "rank": 1},
        {"name": "vendor.qti.media.c2audio@1.0-service", "store": "default2", "rank": 1}
      ],
      "components": [
        {"name": "c2.qti.avc.decoder", "store": "default2", "domain": "video", "kind": "decoder", "mediaType": "video/avc", "maxSize": "4096x2176", "rank": 1},
        {"name": "c2.qti.avc.decoder.secure", "store": "default2", "domain": "video", "kind": "decoder", "mediaType": "video/avc", "maxSize": "4096x2176", "rank": 1},
        {"name": "c2.qti.hevc.decoder", "store": "default2", "domain": "video", "kind": "decoder", "mediaType": "video/hevc", "maxSize": "4096x2176", "rank": 1},
        {"name": "c2.qti.hevc.decoder.secure", "store": "default2", "domain": "video", "kind": "decoder", "mediaType": "video/hevc", "maxSize": "4096x2176", "rank": 1},
        {"name": "c2.qti.vp9.decoder", "store": "default2", "domain": "video", "kind": "decoder", "mediaType": "video/x-vnd.on2.vp9", "maxSize": "4096x2176", "rank": 1},
        {"name": "c2.qti.av1.decoder", "store": "default2", "domain": "video", "kind": "decoder", "mediaType": "video/av01", "maxSize": "4096x2176", "rank": 1},
        {"name": "c2.qti.avc.encoder", "store": "default2", "domain": "video", "kind": "encoder", "mediaType": "video/avc", "maxSize": "4096x2176", "rank": 1},
        {"name": "c2.qti.hevc.encoder", "store": "default2", "domain": "video", "kind": "encoder", "mediaType": "video/hevc", "maxSize": "4096x2176", "rank": 1},
        {"name": "c2.qti.hevc.encoder.cq", "store": "default2", "domain": "video", "kind": "encoder", "mediaType": "video/hevc", "maxSize": "512x512", "rank": 1},
        {"name": "c2.qti.heic.encoder", "store": "default2", "domain": "image", "kind": "encoder", "mediaType": "image/vnd.android.heic", "maxSize": "16384x16384", "rank": 1}
      ]
    },
    "audioEngine": {
      "modules": "default,bluetooth,usb",
      "voiceCallIndexMax": 7,
      "musicIndexMax": 15
    }
  },
  "alignmentNotes": [
    "身份：meizu/Meizu22，板级 sun，SoC 为 QTI SM8735 Snapdragon 8s Gen 4（4nm），GPU Adreno 825，Android 16 ROM（BQ2A.251016.001）；gold 的 MT6833/Dimensity 6080/ARM Mali-G57 全部不适用，radioVariant 由 mtk 改 default。",
    "屏幕：6.3 英寸 LTPO OLED 1200x2670（20:9，465ppi，120Hz、HDR10+、6000nit 峰值），按可视面积 95.7cm2 反算面板 66x146mm；androidDensityDpi 480；EDID 厂商码按三星 SDC 面板推断，edidProductId/week/year 需以真机 EDID 复核。",
    "CPU：公开规格 1x X4@3.21G + 3x A720@3.0G + 2x A720@2.8G + 2x A720@2.0G；boot/DTB 实测策略簇为 policy 0(0-2)/3(3-6)/7(7) 的 3/4/1 分组、归一化 capacity 653/998/1024，已按实测分组落表，频点取 vendor OPP 提示（3014400/2918400/2803200/.../556800）并按簇补齐到 18~22 档，主簇顶部补 3210000。",
    "GPU：Qualcomm Adreno 825，glVersionFull 为 Adreno V@0762.0 驱动串，glExtraExts 使用 GL_QCOM_* 家族（AEP 不在 Adreno 支持范围）；Vulkan vendorId 0x5143、api 1.3（0x00403116）、driverId 8（QUALCOMM_PROPRIETARY）；UUID/driverVersion 为平台口径构造，02 后须用真机 dumpsys SurfaceFlinger / vulkaninfo 回填。",
    "内存/存储：仅 12GB/16GB LPDDR5X 与 256GB/512GB/1TB UFS 4.0，SKU 组合 4 档；gold 的 6GB/128GB 档位不适用。",
    "电池：5510mAh 单电芯，80W 有线（PD3 PPS/QC4+）、66W 无线、10W 反充，故 battery_config.wireless=1、currentChargeUa 按 80W 量级取 -900000；gold 的 5000mAh/33W/无无线不适用。",
    "传感器：以固件 chipOwners 为准 —— Bosch bmi26x（acc+gyro）、Memsic mmc56x3x（mag）、AMS tmd3765（light/prox）；skuFeatures 未声明 barometer，故不编造气压计；私有传感器用 meizu.sensor.pick_up（VINTF 有 vendor.meizu.sensor_aux.ISensorAux），不使用 xiaomi.sensor.* / oplus.sensor.*。",
    "input：键盘驱动实测 gpio-keys、耳机孔实测 sun-snd-card Headset Jack、PMIC 键 qpnp_pon；触控按 Goodix 方案取 goodix_ts（bus 0x18/24、VID 0x27c6），指纹节点 uinput-goodix + goodix_fp_irq；全面屏手势无实体触控按键，touchKeys 置空。",
    "红外/NFC：固件 permissions 声明 android.hardware.consumerir 且公开规格有 IR 口 → enableIr=true；com.nxp.mifare + com.android.se + Goodix NFC/eSE HAL → enableNfc=true、eSE 特性保留。",
    "HAL：GNSS 用 QCOM_GNSS_default（非 MTK_MNLD_default），yearOfHw 取 2025 双频 L1+L5；thermal 用高通温区命名（cpu-0-0-us/gpuss-0-us/nspss-0-us/skin-therm/pa-therm0/xo-therm/pm8550_tz），GPU cooling kgsl-3d0；马达 X 轴线性、capabilities=1（幅度控制）；Codec2 三实例（c2pa/c2/c2audio）落 default2 且 rank=1，未占用 default。",
    "待真机复核项：Widevine systemId、IMEI TAC/IMEI SVN/MEID 前缀、Wi-Fi OUI（3c:2e:f9）、EDID productId 均无公开权威来源，按魅族/高通平台口径构造，需在 04 真机取证阶段以 getprop / cmd wifi / vendor/etc/wifi 覆盖。"
  ],
  "sources": [
    "https://www.gsmarena.com/meizu_22_5g-14150.php — Meizu 22 详细规格（6.3 英寸 1200x2670、SM8735、Adreno 825、12/16GB + 256GB/512GB/1TB UFS4.0、5510mAh/80W/66W 无线、NFC、红外、超声波屏下指纹、立体声）",
    "https://www.gsmarena.com/meizu-phones-74.php — 魅族机型列表与 Meizu 22 发布信息（2025 年 9 月）",
    "目标机固件实测：build.prop 指纹 meizu/Meizu_22_CN/Meizu22:16/BQ2A.251016.001…、boot cmdline androidboot.product.hardware.sku=Meizu_22_CN、VINTF vendor.qti.*/vendor.meizu.*/vendor.goodix.* 占位符、vendor/etc/sensors chipOwners、vendor/etc/permissions skuFeatures、vendor/etc/media_codecs_sun*.xml 与 Codec2 store 实测"
  ]
}
```
