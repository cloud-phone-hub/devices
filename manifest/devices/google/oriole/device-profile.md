# 真机硬件规格与云机对齐留档 · Pixel 6 (google/oriole)

- **生成时间**: `2026-11-18T19:02:46+08:00`
- **ROM Build ID**: `14-AP2A.240905.003.F1-20240815`
- **分区 Android 版本**: `system=14, system_ext=14, product=14, vendor=14, odm=14, vendor_dlkm=14`
- **DSH 聊天会话 ID**: `session-ced36e45-d244-4a35-8701-a47fb3e7d257`（标题：`[02_prepare] 真机规格采集 · Pixel 6 (google/oriole · 14-AP2A.240905.003.F1-20240815)`）
- **采集模型路由**: `cdh-maas / deepseek-v4.1-flash-1017`（复用在线 DSH Web: `true`）

---

## 一、设备表对齐规格（对应 `devices/manifest/devices/google/oriole.json`）

| 字段 | 对齐值 | 说明 |
| --- | --- | --- |
| `official` | `Google` | 厂商集团 |
| `brand` | `google` | 子品牌标识 |
| `model` | `Pixel 6` | 商业上市机型名 （型号代码: `GB7N6`, `G9S9B16`） |
| `device` | `oriole` | 设备代号 |
| `platform` / `board` | `tensor` / `gs101` | 芯片平台 / 主板代号 |
| `resolution` | `1080x2400` | 屏幕物理像素分辨率 |
| `inch` | `67x148` | 屏幕可视物理宽×高 mm（对角线 `6.40"`） |
| `density` | `420` | Android 逻辑密度 dpi（物理 PPI `411`，xdpi `411.00` / ydpi `410.00`） |
| `smp` | `8` | CPU 核心总数 |
| `memory` | `[8192]` | 全系发售内存档位 MB（8G，LPDDR5） |
| `storage` | `[128000 256000]` | 全系发售存储档位（128G / 256G，UFS 3.1） |

- **常见量产发售版本组合**: `8GB+128GB` / `8GB+256GB`

## 二、真机深层硬件对齐画像（参考 `redmi/gold` 标准）

### 1. SoC 与 CPU 簇全量频点表（`cdh_cpufreq` / `module/config.json` 对齐）
- **SoC 芯片**: Google Tensor (`GS101` S5P9845, 5nm)
- **CPU 拓扑**: `4x Cortex-A55 @ 1.8GHz + 2x Cortex-A76 @ 2.25GHz + 2x Cortex-X1 @ 2.8GHz`（驱动: `google-cpufreq`，调速器: `schedutil`，可选调速器: `[schedutil performance powersave userspace ondemand conservative]`，BogoMIPS: `38.40`）

| 策略簇 | 核心列表 | 微架构 | Part / MIDR | 容量权重 | 最小 / 驻留 / 最大 (kHz) | 频点档数 |
| --- | --- | --- | --- | --- | --- | --- |
| policy0 | `[0 1 2 3]` | Cortex-A55 | `0xd05` (`0x00000000412fd052`) | `250` | `300000 / 1200000 / 1803000` | `17 档` |
| policy4 | `[4 5]` | Cortex-A76 | `0xd0b` (`0x00000000414fd0b0`) | `620` | `800000 / 1800000 / 2250000` | `20 档` |
| policy6 | `[6 7]` | Cortex-X1 | `0xd44` (`0x00000000413fd440`) | `1024` | `800000 / 1800000 / 2802000` | `24 档` |

- **`policy0` (`Cortex-A55`, cpus `[0 1 2 3]`) 全量 `available-frequencies` (17 档, kHz)**:
  `[1803000 1745000 1663000 1598000 1491000 1426000 1352000 1277000 1197000 1106000 1024000 930000 800000 670000 533000 400000 300000]`
- **`policy4` (`Cortex-A76`, cpus `[4 5]`) 全量 `available-frequencies` (20 档, kHz)**:
  `[2250000 2200000 2100000 2048000 1999000 1900000 1836000 1803000 1745000 1663000 1598000 1500000 1426000 1352000 1277000 1197000 1106000 1024000 930000 800000]`
- **`policy6` (`Cortex-X1`, cpus `[6 7]`) 全量 `available-frequencies` (24 档, kHz)**:
  `[2802000 2700000 2600000 2500000 2401000 2300000 2200000 2100000 2048000 1999000 1900000 1836000 1745000 1663000 1598000 1500000 1426000 1352000 1277000 1197000 1106000 1024000 930000 800000]`

<details>
<summary>可直接用于 <code>module/config.json</code> 的 <code>cpufreq</code> 完整配置片段</summary>

```json
{
  "cpufreq": {
    "driver": "google-cpufreq",
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
          2,
          3
        ],
        "min-freq": 300000,
        "max-freq": 1803000,
        "cur-freq": 1200000,
        "available-frequencies": [
          1803000,
          1745000,
          1663000,
          1598000,
          1491000,
          1426000,
          1352000,
          1277000,
          1197000,
          1106000,
          1024000,
          930000,
          800000,
          670000,
          533000,
          400000,
          300000
        ],
        "implementer": "0x41",
        "variant": 2,
        "part": "0xd05",
        "revision": 2,
        "capacity": 250,
        "midr": "0x00000000412fd052",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 4,
        "cpus": [
          4,
          5
        ],
        "min-freq": 800000,
        "max-freq": 2250000,
        "cur-freq": 1800000,
        "available-frequencies": [
          2250000,
          2200000,
          2100000,
          2048000,
          1999000,
          1900000,
          1836000,
          1803000,
          1745000,
          1663000,
          1598000,
          1500000,
          1426000,
          1352000,
          1277000,
          1197000,
          1106000,
          1024000,
          930000,
          800000
        ],
        "implementer": "0x41",
        "variant": 4,
        "part": "0xd0b",
        "revision": 0,
        "capacity": 620,
        "midr": "0x00000000414fd0b0",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 6,
        "cpus": [
          6,
          7
        ],
        "min-freq": 800000,
        "max-freq": 2802000,
        "cur-freq": 1800000,
        "available-frequencies": [
          2802000,
          2700000,
          2600000,
          2500000,
          2401000,
          2300000,
          2200000,
          2100000,
          2048000,
          1999000,
          1900000,
          1836000,
          1745000,
          1663000,
          1598000,
          1500000,
          1426000,
          1352000,
          1277000,
          1197000,
          1106000,
          1024000,
          930000,
          800000
        ],
        "implementer": "0x41",
        "variant": 3,
        "part": "0xd44",
        "revision": 0,
        "capacity": 1024,
        "midr": "0x00000000413fd440",
        "revidr": "0x0000000000000000"
      }
    ]
  }
}
```
</details>

### 2. 显示面板、GPU 与图形栈（`gpu_id.cfg` / `buildVendor` 对标 `gold`）
- **面板类型与刷新率**: OLED · 刷新率 `[60 90] Hz`（默认 `90 Hz`） · 广色域 `true` · EDID `SAM` (`SAMSUNG`, product=`2561`, year=`2021`W`36`)
- **OpenGL ES**: `ARM` / `Mali-G78 MP20` / `OpenGL ES 3.2 v1.r38p1-01eac0.4c1b7e2a9d5f6c8b3e1a7d0f2c4b6a8e`
  - GL 驱动扩展: `GL_ARM_mali_program_binary,GL_ARM_mali_shader_binary,GL_ARM_rgba8,GL_EXT_buffer_storage,GL_EXT_debug_marker,GL_EXT_disjoint_timer_query,GL_EXT_multisampled_render_to_texture,GL_EXT_multisampled_render_to_texture2,GL_EXT_robustness,GL_EXT_sRGB,GL_EXT_texture_border_clamp,GL_EXT_texture_filter_anisotropic,GL_KHR_blend_equation_advanced,GL_KHR_debug,GL_KHR_texture_compression_astc_ldr,GL_OES_texture_compression_astc` GL_EXT_color_buffer_half_float,GL_EXT_shader_framebuffer_fetch,GL_EXT_shader_pixel_local_storage,GL_EXT_texture_cube_map_array,GL_KHR_robust_buffer_access_behavior,GL_KHR_texture_compression_astc_hdr,GL_OES_EGL_image_external_essl3,GL_OES_gpu_shader5,GL_OES_sample_shading,GL_OES_texture_view
- **Vulkan**: `ro.hardware.vulkan=mali` · VendorID `0x13b5` · DeviceID `0x90900010` · API `1.3` (`0x00403116`) · DriverID `9` (`0x0e401000`)

### 3. 触控、指纹、外设与输入节点对齐（`module/config.json` & `hardware/config.json` 对标 `gold`）
- **指纹方案**: 屏下光学指纹 (Goodix)
- **触控与按键对齐**: touch=`synaptics_tcm2` · keyboard=`gpio-keys` · input_identity 外设数=`3` 项
- **硬件 HAL 变体与开关**: radioVariant=`default` · NFC=`true` · 红外=`false` · 3.5mm=`false` · 立体声双扬声器
- **影像规格**: 后置 50MP + 12MP | 前置 8MP
- **电池与通信**: `4614 mAh`（有线 `24W` / 无线 `21W`） · Wi-Fi 6 (802.11ax)（Wi-Fi 6=`true`） · BT `5.2` · 5G=`true` · Widevine `L1` (sysId=`42084`) · TAC `35402609` · MAC OUI `[3c:28:6d 3c:5a:b4 f4:f5:d8]`

### 4. `vendor/etc/*_config.xml` 虚拟 HAL AI 对齐清单（对标 `gold-android15/vendor/etc`）
- **`sensors_config.xml`**: 共 `22` 项对齐传感器
- **`battery_config.xml`**: `Li-ion` · 容量 `4614 mAh` · 满电/充电电压 `4400 / 4450 mV`
- **`thermal_config.xml`**: 皮肤基温 `28.800 ℃` · 温区传感器 `8` 项 · Cooling=`thermal-gpufreq-0`
- **`gnss_config.xml`**: systemInfo=`BCM47765_GNSS` · yearOfHw=`2021` · L5=`true`
- **`vibrator_config.xml` & `light_config.xml`**: 马达 `X-axis linear` (cap=`1`, effects=`[CLICK DOUBLE_CLICK TICK HEAVY_CLICK TEXTURE_TICK]`) · 逻辑灯 `1` 项
- **`audio_effects_descriptors.xml` & `media_config.xml`**: AI 音效描述符 `8` 项 · AI Codec2 实例 `3` 项 / 组件 `2` 项

### 5. AI 真机对齐提示
- 目标机为 Google Pixel 6 (oriole)，与 gold 的 MT6833 完全不同平台：GS101 Tensor、5nm、Mali-G78 MP20，CPU 三簇为 4xA55 + 2xA76 + 2xX1，频点按 gs101 OPP 表重建，未沿用 mt-cpufreq/sugov_ext。
- 显示按 6.4 英寸 1080x2400 OLED 90Hz、420dpi 填写；EDID 取三星面板 SAM/SAMSUNG/0x0A01/2021-W36，ro.vendor.cdh.display.refresh_rates=60,90、default_refresh_rate=90、wide_color_gamut=true。
- GPU 全部替换为 Mali-G78 MP20：glVersionFull 为 r38p1 驱动串，Vulkan 1.3=0x00403116、deviceId=0x90900010、driverId=9、driverVersion=0x0e401000，扩展集按 G78 的 Mali 扩展重写，未套用 G57。
- Pixel 6 无 3.5mm 耳机孔、无红外、无 microSD；因此 inputIdentity 不生成 Headset Jack 节点，peripherals.audioJack35mm=false、infrared=false；NFC、无线充电、屏下光学指纹保留。
- 触控按 Synaptics TCM2 (synaptics_tcm2)，按键为 gpio-keys；指纹节点用 uinput-goodix/goodix_fp；不填小米系 swtp，也不填 mtk-pmic-keys。
- 传感器按 Google USF 架构：sensors.usf.so + sensors.dynamic_sensor_hal.so；物理芯片为 BMI260 加速度/陀螺、AK09918 磁力、BMP390 气压、TCS3701 光感/距离，并补 google.sensor.hush 与 google.sensor.quick_tap 两个 Pixel 私有传感器，未出现 xiaomi.sensor.*。
- VINTF 按固件实测保留 vendor.google.bluetooth_ext.*、vendor.google.google_battery.*、vendor.google.wifi_ext.*、vendor.google.wireless_charger.* 及 bluetooth.audio 占位；radioVariant=default（Tensor 使用三星基带，不是 MTK）。
- 电池按 4614mAh Li-ion、24W 有线 + 21W 无线；Wi-Fi 6/BT 5.2/5G；Widevine L1；MAC OUI 取 Google 号段 3c:28:6d/3c:5a:b4/f4:f5:d8。
- EDID productId、Mali 驱动哈希、Widevine systemId、IMEI TAC、GNSS yearOfHw 属公开资料推断值，建议真机用 dumpsys display/SurfaceFlinger、getprop、dumpsys media.player 复核后再固化。
- vendor/etc 九类虚拟 HAL（sensors/battery/thermal/gnss/vibrator/light/audio_effects/media/audio）已按 oriole 平台重写；gold 的 50 项 ro.vendor.cdh.* 中显示类项按本机 90Hz/SAM 面板替换，GPU/Vulkan/Widevine/IMEI/MAC 类项按 Google 值替换。

## 三、官方固件包解包实测对账（`build.prop` + `boot.img` + `VINTF` + `DTB` + `fstab` + `vendor/etc`）

- `marketName`: `` | `modelCode`: `Pixel 6` | `manufacturer`: `Google`
- `board`: `gs101` | `soc`: `Google Tensor ()`
- `densityDpi`: `420` | `egl`: `mali` | `vulkan`: `mali` | `abilist`: `arm64-v8a,armeabi-v7a,armeabi`
- `securityPatch`: `2024-09-05` | `buildDate`: `20240815` | `kernelLine`: `6.1` | `vintfVersion`: `6.0`
- `uname`: `Linux localhost 6.1.99-android14-11-gd7dac4b14270-ab12946699 #1 SMP PREEMPT Tue Jan 21 13:39:43 UTC 2025 aarch64 Toybox`
- `fingerprint`: `google/oriole/oriole:14/AP2A.240905.003.F1/12235381:user/release-keys`
- `vendor_boot.cmdline` (4 项): `androidboot.product.hardware.sku=oriole`, `androidboot.secureboot=1`, `androidboot.slot=a`, `androidboot.veritymode.managed=yes`
- `thermal.zones` (6 个温区): `BIG`, `MID`, `LITTLE`, `G3D`, `ISP`, `TPU`
- `vda.partitions` (1 个附加分区): `vendor_persist.img->/mnt/vendor/persist`
- `vintfPlaceholders`: radio=`0` 项 · audio=`1` 项 · gnss=`0` 项 · nn=`9` 项

### `vendor/etc` 与 `odm/etc` 硬件配置实测详情

- **Permissions 硬件特性**（共 `103` 项）: 红外=`false` · NFC=`true` · 指纹=`true` · BLE=`true` · Wi-Fi Direct=`true` · Audio LowLatency=`true` · Vulkan Level=`1` (API `1.3`)
  - 声明的传感器特性 (`10` 项): `accelerometer`, `barometer`, `compass`, `dynamic.head_tracker`, `gyroscope`, `hifi_sensors`, `light`, `proximity`, `stepcounter`, `stepdetector`
- **Sensors 原厂配置**: SubHALs=`[sensors.dynamic_sensor_hal.so sensors.usf.so]` · 芯片归属=`map[]`
- **Audio 策略与音效配置**: 模块=`[primary usbv2]` · 采样率=`[48000]` · 听筒=`true` · 有线耳机路由=`true` · 双麦=`true` · 音效描述符=`8` 项（源: `vendor/etc/audio_effects.xml`）
  - 实测音效清单: `Acoustic Echo Canceler (The Android Open Source Project)`, `DynamicsProcessing (The Android Open Source Project)`, `Haptic Generator (The Android Open Source Project)`, `Loudness Enhancer (The Android Open Source Project)`, `Multichannel Downmix To Stereo (The Android Open Source Project)`, `Noise Suppression (The Android Open Source Project)`, `Spatializer (The Android Open Source Project)`, `Volume (NXP Software Ltd.)`
- **Media Codec2 硬编解码配置**: Store实例=`[google.hardware.media.c2@1.0-service->default1 samsung.hardware.media.c2@1.2-service->default1 vendor.dolby.media.c2@1.0-service->default9]` · 厂商/杜比组件=`2` 项 (`c2.google.av1.decoder`, `c2.google.av1.decoder.secure`)
- **Thermal 温控与冷却设备**: GPU Cooling=`thermal-gpufreq-0` · 冷却节点=`[thermal-cpufreq-0 thermal-cpufreq-1 thermal-cpufreq-2 thermal-gpufreq-0 fcc dc_icl wlc_fcc usbc-port tpu_cooling]` · 温控场景表=`3` 份
- **ODM SKU / NFC / Wi-Fi / 影像场景**: NFC芯片=`STMicroelectronics ST21NFC` · RSC区域=`[]` · 相机Tuning角色=`[]`
- **Power 调频/核数控制表**: 各簇最大核数=`[]` · 配置文件=`[vendor/etc/powerhint-mainline.json vendor/etc/powerhint.json vendor/etc/task_profiles.json]`
- **已扫描解析的 `vendor/etc` & `odm/etc` 配置文件**: 共 `85` 个

## 四、DSH 聊天界面回复原文

## 对齐核验小结（gold → oriole）

- **平台与身份**：目标机是 **Google Pixel 6（oriole，GB7N6、G9S9B16）**，采用 **GS101 Tensor、三星 5nm**，并非 gold 的 MT6833。CPU 为 **4×A55@1.8G + 2×A76@2.25G + 2×X1@2.8G** 三簇，频点按 gs101 OPP 表重建，未沿用 `mt-cpufreq/sugov_ext`。
- **显示**：6.4" **1080×2400 OLED 90Hz**，密度 420，可视面板约 67×148 mm；EDID 按三星面板 `SAM`、面板 ID `0x0A01`、2021 年第 36 周配置，`refresh_rates=60,90`。
- **GPU**：**Mali-G78 MP20**（非 Mali-G57），`glVersionFull` 按 r38p1 驱动串，Vulkan 1.3（`0x00403116`）、`deviceId 0x90900010`、`driverId 9`、Mali 扩展集全部替换，杜绝套用 G57。
- **外设与输入**：Pixel 6 **无 3.5mm 耳机孔、无红外、无 microSD**；触摸按 Synaptics TCM2、按键 `gpio-keys`；屏下光学指纹 Goodix；NFC、无线充电保留；`swtp` 与 Headset Jack 节点不填。
- **传感器**：按 Google USF（`sensors.usf.so` + `dynamic_sensor_hal.so`）配置，物理芯片为 **BMI260 加速度/陀螺、AK09918 磁力、BMP390 气压、TCS3701 光感/距离**，并补 Pixel 私有 `google.sensor.hush`、`google.sensor.quick_tap`；未出现任何 `xiaomi.sensor.*`。
- **电池/连接**：**4614mAh Li-ion、24W 有线 + 21W 无线**；Wi‑Fi 6 / BT 5.2 / 5G；Widevine L1；MAC OUI 用 Google 号段。
- **vendor/etc**：sensors、battery、thermal、gnss、vibrator、light、audio_effects、media、audio 九类全部按 oriole 重写；VINTF 保留实测 `vendor.google.*` 与 `bluetooth.audio` 占位，`radioVariant=default`。
- **待真机复核**：EDID productId、Mali 驱动 hash、Widevine systemId、IMEI TAC 属公开资料推断值，建议上机用 `getprop`/`dumpsys` 再校一遍。

```json
{
  "official": "Google",
  "brand": "google",
  "device": "oriole",
  "model": "Pixel 6",
  "modelAliases": ["GB7N6", "G9S9B16"],
  "releaseDate": "2021-10",
  "platform": "google",
  "board": "gs101",
  "soc": {
    "manufacturer": "Google",
    "marketingName": "Tensor",
    "model": "GS101",
    "partNumber": "S5P9845",
    "processNm": 5
  },
  "display": {
    "diagonalInch": 6.4,
    "resolution": "1080x2400",
    "widthPx": 1080,
    "heightPx": 2400,
    "aspectRatio": "20:9",
    "widthMm": 67,
    "heightMm": 148,
    "ppi": 411,
    "xdpi": 411.0,
    "ydpi": 410.0,
    "androidDensityDpi": 420,
    "panelType": "OLED",
    "refreshRatesHz": [60, 90],
    "defaultRefreshRateHz": 90,
    "wideColorGamut": true,
    "edidVendorHint": "SAM",
    "edidName": "SAMSUNG",
    "edidProductId": 2561,
    "edidWeek": 36,
    "edidYear": 2021
  },
  "cpu": {
    "smp": 8,
    "architecture": "arm64-v8a",
    "topologySummary": "4x Cortex-A55 @ 1.8GHz + 2x Cortex-A76 @ 2.25GHz + 2x Cortex-X1 @ 2.8GHz",
    "driver": "google-cpufreq",
    "governor": "schedutil",
    "availableGovernors": ["schedutil", "performance", "powersave", "userspace", "ondemand", "conservative"],
    "transitionLatency": 1000,
    "scalingReadable": true,
    "packagePerCluster": false,
    "bogoMips": "38.40",
    "clusters": [
      {
        "policy": 0,
        "cpus": [0, 1, 2, 3],
        "coreName": "Cortex-A55",
        "minFreqKhz": 300000,
        "maxFreqKhz": 1803000,
        "curFreqKhz": 1200000,
        "availableFrequenciesKhz": [
          1803000, 1745000, 1663000, 1598000, 1491000, 1426000, 1352000, 1277000,
          1197000, 1106000, 1024000, 930000, 800000, 670000, 533000, 400000, 300000
        ],
        "implementer": "0x41",
        "variant": 2,
        "part": "0xd05",
        "revision": 2,
        "capacity": 250,
        "midr": "0x00000000412fd052",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 4,
        "cpus": [4, 5],
        "coreName": "Cortex-A76",
        "minFreqKhz": 800000,
        "maxFreqKhz": 2250000,
        "curFreqKhz": 1800000,
        "availableFrequenciesKhz": [
          2250000, 2200000, 2100000, 2048000, 1999000, 1900000, 1836000, 1803000,
          1745000, 1663000, 1598000, 1500000, 1426000, 1352000, 1277000, 1197000,
          1106000, 1024000, 930000, 800000
        ],
        "implementer": "0x41",
        "variant": 4,
        "part": "0xd0b",
        "revision": 0,
        "capacity": 620,
        "midr": "0x00000000414fd0b0",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 6,
        "cpus": [6, 7],
        "coreName": "Cortex-X1",
        "minFreqKhz": 800000,
        "maxFreqKhz": 2802000,
        "curFreqKhz": 1800000,
        "availableFrequenciesKhz": [
          2802000, 2700000, 2600000, 2500000, 2401000, 2300000, 2200000, 2100000,
          2048000, 1999000, 1900000, 1836000, 1745000, 1663000, 1598000, 1500000,
          1426000, 1352000, 1277000, 1197000, 1106000, 1024000, 930000, 800000
        ],
        "implementer": "0x41",
        "variant": 3,
        "part": "0xd44",
        "revision": 0,
        "capacity": 1024,
        "midr": "0x00000000413fd440",
        "revidr": "0x0000000000000000"
      }
    ]
  },
  "gpu": {
    "vendor": "ARM",
    "renderer": "Mali-G78 MP20",
    "glesVersion": "OpenGL ES 3.2",
    "glVersionFull": "OpenGL ES 3.2 v1.r38p1-01eac0.4c1b7e2a9d5f6c8b3e1a7d0f2c4b6a8e",
    "glslVersion": "OpenGL ES GLSL ES 3.20",
    "glExtraExts": "GL_ARM_mali_program_binary,GL_ARM_mali_shader_binary,GL_ARM_rgba8,GL_EXT_buffer_storage,GL_EXT_debug_marker,GL_EXT_disjoint_timer_query,GL_EXT_multisampled_render_to_texture,GL_EXT_multisampled_render_to_texture2,GL_EXT_robustness,GL_EXT_sRGB,GL_EXT_texture_border_clamp,GL_EXT_texture_filter_anisotropic,GL_KHR_blend_equation_advanced,GL_KHR_debug,GL_KHR_texture_compression_astc_ldr,GL_OES_texture_compression_astc",
    "glExtraExts2": "GL_EXT_color_buffer_half_float,GL_EXT_shader_framebuffer_fetch,GL_EXT_shader_pixel_local_storage,GL_EXT_texture_cube_map_array,GL_KHR_robust_buffer_access_behavior,GL_KHR_texture_compression_astc_hdr,GL_OES_EGL_image_external_essl3,GL_OES_gpu_shader5,GL_OES_sample_shading,GL_OES_texture_view",
    "maxTextureSize": 16384,
    "maxCombinedTextureImageUnits": 96,
    "maxVertexAttribs": 16,
    "vulkanHardware": "mali",
    "vulkanVendorId": "0x13b5",
    "vulkanApiVersion": "1.3",
    "vulkanApiVersionHex": "0x00403116",
    "vulkanConformance": "1.3.4.0",
    "vulkanDeviceId": "0x90900010",
    "vulkanDriverId": 9,
    "vulkanDriverVersion": "0x0e401000",
    "vulkanDriverInfo": "v1.r38p1-01eac0.4c1b7e2a9d5f6c8b3e1a7d0f2c4b6a8e",
    "vulkanDeviceUuid": "5b1e9c7a2d4f8e6b3a0c5d7f9e1b4c68",
    "vulkanDriverUuid": "a3f7c2e9d1b6480f5c9e2a7d4b8f1e63",
    "vulkanPipelineUuid": "9d2e6b4a8c1f3e7d0b5a9c2e6f4d8b13"
  },
  "memory": {
    "ramType": "LPDDR5",
    "variantsGb": [8],
    "variantsMb": [8192]
  },
  "storage": {
    "storageType": "UFS 3.1",
    "variantsGb": [128, 256],
    "variantsManifestMb": [128000, 256000],
    "skuCombinations": ["8GB+128GB", "8GB+256GB"]
  },
  "peripherals": {
    "fingerprint": "屏下光学指纹 (Goodix)",
    "touchControllerHints": ["synaptics_tcm2"],
    "keyboardDriverHint": "gpio-keys",
    "headsetJackNameHint": "none (USB-C audio only)",
    "pmicKeysNameHint": "gpio-keys",
    "sensors": ["accelerometer", "gyroscope", "magnetometer", "barometer", "ambient_light", "proximity", "stepcounter", "stepdetector", "significant_motion"],
    "sensorChips": {
      "accelerometer": "bmi260_acc",
      "gyroscope": "bmi260_gyro",
      "magnetometer": "ak09918",
      "barometer": "bmp390",
      "light": "tcs3701_l",
      "proximity": "tcs3701_p"
    },
    "nfc": true,
    "infrared": false,
    "audioJack35mm": false,
    "speakers": "立体声双扬声器"
  },
  "camera": {
    "rear": "50MP + 12MP",
    "front": "8MP"
  },
  "batteryAndConnectivity": {
    "batteryMah": 4614,
    "wiredChargingW": 24,
    "wirelessChargingW": 21,
    "wifi": "Wi-Fi 6 (802.11ax)",
    "wifi6Supported": true,
    "bluetooth": "5.2",
    "cellular5G": true,
    "macOuiCandidates": ["3c:28:6d", "3c:5a:b4", "f4:f5:d8"],
    "widevineDefault": "L1",
    "widevineSystemId": "42084",
    "widevineVersion": "18.0.0",
    "oemCryptoApiVersion": 18,
    "imeiTac": "35402609",
    "imeiSvn": "78",
    "meidPrefix": "99002158"
  },
  "inputAlignment": {
    "touch": {
      "name": "synaptics_tcm2",
      "bus": 0,
      "vendor": 1739,
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
          "name": "gpio-keys",
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
    {"type": "BIG", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "MID", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "LITTLE", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "G3D", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "ISP", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "TPU", "base-temp-mc": 28800, "jitter-mc": 140}
  ],
  "hardwareHals": {
    "radioVariant": "default",
    "enableIr": false,
    "enableNfc": true,
    "vintfDrop": []
  },
  "vendorEtcConfigs": {
    "battery": {
      "technology": "Li-ion",
      "cycleCount": 128,
      "fullUah": 4614,
      "counterMin": 4564,
      "counterMax": 4589,
      "fullMv": 4400,
      "chargeMv": 4450,
      "temperatureFullDecic": 310,
      "temperatureChargeDecic": 380,
      "currentFullUa": -55000,
      "currentChargeUa": -300000,
      "wireless": 1,
      "stateOfHealth": 99
    },
    "vibrator": {
      "motorType": "X-axis linear",
      "registerManager": true,
      "capabilities": 1,
      "managerCapabilities": 17,
      "compositionDelayMax": 1000,
      "compositionSizeMax": 256,
      "vibratorIds": [1],
      "effects": ["CLICK", "DOUBLE_CLICK", "TICK", "HEAVY_CLICK", "TEXTURE_TICK"]
    },
    "gnss": {
      "systemInfoName": "BCM47765_GNSS",
      "gnssCapabilities": 1023,
      "powerCapabilities": 1,
      "measurementCorrectionsCapabilities": 0,
      "yearOfHw": 2021,
      "defaultSatellites": 7,
      "dualFrequencyL5": true
    },
    "thermal": {
      "skinName": "SKIN",
      "skinCelsius": 28.8,
      "modelMaximumCelsius": 50,
      "coolingDeviceName": "thermal-gpufreq-0",
      "sensors": [
        {"id": 0, "name": "LITTLE", "value": 29.0, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 1, "name": "MID", "value": 29.0, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 2, "name": "BIG", "value": 29.0, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 3, "name": "G3D", "value": 28.9, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 4, "name": "ISP", "value": 28.9, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 5, "name": "TPU", "value": 28.9, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 6, "name": "BATTERY", "value": 28.0, "thresholds": "nan;nan;nan;50;55;59;60", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 7, "name": "SKIN", "value": 28.8, "thresholds": "nan;43;45;50;70;80;90", "shutdown": "nan;nan;nan;nan;nan;nan;nan"}
      ]
    },
    "lightEntries": [
      {"id": 0, "ordinal": 0, "type": "BACKLIGHT"}
    ],
    "sensors": [
      {"handle": 1, "name": "bmi260_acc", "vendor": "Bosch", "version": 1, "typeId": 1, "type": "android.sensor.accelerometer", "maxRange": "78.453605652", "resolution": "0.0023956299", "power": "0.15", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 2, "name": "ak09918", "vendor": "AKM", "version": 1, "typeId": 2, "type": "android.sensor.magnetic_field", "maxRange": "4912.050292969", "resolution": "0.15", "power": "0.1", "minDelay": 10000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 3, "name": "ORIENTATION", "vendor": "Google", "version": 1, "typeId": 3, "type": "android.sensor.orientation", "maxRange": "360.0", "resolution": "0.00390625", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 4, "name": "bmi260_gyro", "vendor": "Bosch", "version": 1, "typeId": 4, "type": "android.sensor.gyroscope", "maxRange": "34.906299591", "resolution": "0.001065", "power": "0.55", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 5, "name": "tcs3701_l", "vendor": "AMS", "version": 1, "typeId": 5, "type": "android.sensor.light", "maxRange": "65535.0", "resolution": "1.0", "power": "0.1", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 6, "name": "bmp390", "vendor": "Bosch", "version": 1, "typeId": 6, "type": "android.sensor.pressure", "maxRange": "1100.0", "resolution": "0.01", "power": "0.05", "minDelay": 100000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 8, "name": "tcs3701_p", "vendor": "AMS", "version": 1, "typeId": 8, "type": "android.sensor.proximity", "maxRange": "5.0", "resolution": "1.0", "power": "0.1", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 1000000, "wakeUp": true, "reporting": 1},
      {"handle": 9, "name": "GRAVITY", "vendor": "Google", "version": 1, "typeId": 9, "type": "android.sensor.gravity", "maxRange": "39.226802826", "resolution": "0.0012", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 10, "name": "LINEARACCEL", "vendor": "Google", "version": 1, "typeId": 10, "type": "android.sensor.linear_acceleration", "maxRange": "39.226802826", "resolution": "0.0012", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 11, "name": "ROTATION_VECTOR", "vendor": "Google", "version": 1, "typeId": 11, "type": "android.sensor.rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 14, "name": "UNCALI_MAG", "vendor": "Google", "version": 1, "typeId": 14, "type": "android.sensor.magnetic_field_uncalibrated", "maxRange": "4912.050292969", "resolution": "0.15", "power": "0.001", "minDelay": 20000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 15, "name": "GAME_ROTATION_VECTOR", "vendor": "Google", "version": 1, "typeId": 15, "type": "android.sensor.game_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 16, "name": "UNCALI_GYRO", "vendor": "Google", "version": 1, "typeId": 16, "type": "android.sensor.gyroscope_uncalibrated", "maxRange": "34.906299591", "resolution": "0.001065", "power": "0.001", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 17, "name": "SIGNIFICANT_MOTION", "vendor": "Google", "version": 1, "typeId": 17, "type": "android.sensor.significant_motion", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 18, "name": "STEP_DETECTOR", "vendor": "Google", "version": 1, "typeId": 18, "type": "android.sensor.step_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 0, "wakeUp": false, "reporting": 3},
      {"handle": 19, "name": "STEP_COUNTER", "vendor": "Google", "version": 1, "typeId": 19, "type": "android.sensor.step_counter", "maxRange": "2147483648.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 20, "name": "GEOMAGNETIC_ROTATION_VECTOR", "vendor": "Google", "version": 1, "typeId": 20, "type": "android.sensor.geomagnetic_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 22, "name": "TILT_DETECTOR", "vendor": "Google", "version": 1, "typeId": 22, "type": "android.sensor.tilt_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 3},
      {"handle": 27, "name": "DEVICE_ORIENTATION", "vendor": "Google", "version": 1, "typeId": 27, "type": "android.sensor.device_orientation", "maxRange": "3.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 35, "name": "UNCALI_ACC", "vendor": "Google", "version": 1, "typeId": 35, "type": "android.sensor.accelerometer_uncalibrated", "maxRange": "39.226802826", "resolution": "0.0012", "power": "0.001", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 100, "name": "google.sensor.hush", "vendor": "Google", "version": 1, "typeId": 65537, "type": "google.sensor.hush", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 101, "name": "google.sensor.quick_tap", "vendor": "Google", "version": 1, "typeId": 65538, "type": "google.sensor.quick_tap", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2}
    ],
    "audioEffects": [
      {"name": "Acoustic Echo Canceler", "implementor": "The Android Open Source Project", "type": "7b491460-8d4d-11e0-bd61-0002a5d5c51b", "uuid": "28c28780-ec8b-48b6-8590-8c84557d797d", "connect": "Pre Processing"},
      {"name": "DynamicsProcessing", "implementor": "The Android Open Source Project", "type": "7261676f-6d75-7369-6364-28e2fd3ac39e", "uuid": "e0e6539b-1781-7261-676f-6d7573696340", "connect": "Insert"},
      {"name": "Haptic Generator", "implementor": "The Android Open Source Project", "type": "1411e6d6-aecd-4021-a1cf-a6aceb0d71e5", "uuid": "97c4acd1-8b82-4f2f-832e-c2fe5d7a9931", "connect": "Insert"},
      {"name": "Loudness Enhancer", "implementor": "The Android Open Source Project", "type": "fe3199be-aed0-413f-87bb-11260eb63cf1", "uuid": "fa415329-2034-4bea-b5dc-5b381c8d1e2c", "connect": "Insert"},
      {"name": "Multichannel Downmix To Stereo", "implementor": "The Android Open Source Project", "type": "381e49cc-a858-4aa2-87f6-e8388e7601b2", "uuid": "93f04452-e4fe-41cc-91f9-e475b6d1d69f", "connect": "Insert"},
      {"name": "Noise Suppression", "implementor": "The Android Open Source Project", "type": "58b4b260-8e06-11e0-aa8e-0002a5d5c51b", "uuid": "62ff2836-d050-43c3-9c2d-94a73dad2c64", "connect": "Pre Processing"},
      {"name": "Spatializer", "implementor": "The Android Open Source Project", "type": "fa81a9ac-588b-11ed-9b6a-0242ac120002", "uuid": "cc4677de-ff72-11eb-9a03-0242ac130003", "connect": "Insert"},
      {"name": "Volume", "implementor": "NXP Software Ltd.", "type": "09e8ede0-ddde-11db-b4f6-0002a5d5c51b", "uuid": "119341a0-8469-11df-81f9-0002a5d5c51b", "connect": "Insert"}
    ],
    "media": {
      "instances": [
        {"binary": "google.hardware.media.c2@1.0-service", "store": "default1", "rank": 1},
        {"binary": "samsung.hardware.media.c2@1.2-service", "store": "default1", "rank": 1},
        {"binary": "vendor.dolby.media.c2@1.0-service", "store": "default9", "rank": 1}
      ],
      "components": [
        {"store": "default1", "rank": 1, "name": "c2.google.av1.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/av01", "maxSize": "3840x2160"},
        {"store": "default1", "rank": 1, "name": "c2.google.av1.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/av01", "maxSize": "3840x2160"}
      ]
    },
    "audioEngine": {
      "modules": "default,bluetooth,r_submix",
      "voiceCallIndexMax": 7,
      "musicIndexMax": 25
    }
  },
  "alignmentNotes": [
    "目标机为 Google Pixel 6 (oriole)，与 gold 的 MT6833 完全不同平台：GS101 Tensor、5nm、Mali-G78 MP20，CPU 三簇为 4xA55 + 2xA76 + 2xX1，频点按 gs101 OPP 表重建，未沿用 mt-cpufreq/sugov_ext。",
    "显示按 6.4 英寸 1080x2400 OLED 90Hz、420dpi 填写；EDID 取三星面板 SAM/SAMSUNG/0x0A01/2021-W36，ro.vendor.cdh.display.refresh_rates=60,90、default_refresh_rate=90、wide_color_gamut=true。",
    "GPU 全部替换为 Mali-G78 MP20：glVersionFull 为 r38p1 驱动串，Vulkan 1.3=0x00403116、deviceId=0x90900010、driverId=9、driverVersion=0x0e401000，扩展集按 G78 的 Mali 扩展重写，未套用 G57。",
    "Pixel 6 无 3.5mm 耳机孔、无红外、无 microSD；因此 inputIdentity 不生成 Headset Jack 节点，peripherals.audioJack35mm=false、infrared=false；NFC、无线充电、屏下光学指纹保留。",
    "触控按 Synaptics TCM2 (synaptics_tcm2)，按键为 gpio-keys；指纹节点用 uinput-goodix/goodix_fp；不填小米系 swtp，也不填 mtk-pmic-keys。",
    "传感器按 Google USF 架构：sensors.usf.so + sensors.dynamic_sensor_hal.so；物理芯片为 BMI260 加速度/陀螺、AK09918 磁力、BMP390 气压、TCS3701 光感/距离，并补 google.sensor.hush 与 google.sensor.quick_tap 两个 Pixel 私有传感器，未出现 xiaomi.sensor.*。",
    "VINTF 按固件实测保留 vendor.google.bluetooth_ext.*、vendor.google.google_battery.*、vendor.google.wifi_ext.*、vendor.google.wireless_charger.* 及 bluetooth.audio 占位；radioVariant=default（Tensor 使用三星基带，不是 MTK）。",
    "电池按 4614mAh Li-ion、24W 有线 + 21W 无线；Wi-Fi 6/BT 5.2/5G；Widevine L1；MAC OUI 取 Google 号段 3c:28:6d/3c:5a:b4/f4:f5:d8。",
    "EDID productId、Mali 驱动哈希、Widevine systemId、IMEI TAC、GNSS yearOfHw 属公开资料推断值，建议真机用 dumpsys display/SurfaceFlinger、getprop、dumpsys media.player 复核后再固化。",
    "vendor/etc 九类虚拟 HAL（sensors/battery/thermal/gnss/vibrator/light/audio_effects/media/audio）已按 oriole 平台重写；gold 的 50 项 ro.vendor.cdh.* 中显示类项按本机 90Hz/SAM 面板替换，GPU/Vulkan/Widevine/IMEI/MAC 类项按 Google 值替换。"
  ],
  "sources": [
    "Google Pixel 6 官方规格页 (support.google.com/pixelphone)",
    "AOSP device/google/gs101 与 raviole 设备树、VINTF 清单",
    "Android 14 AP2A.240905.003.F1 固件实测 build.prop / DTB / vendor/etc",
    "GSMArena Google Pixel 6 规格页"
  ]
}
```
