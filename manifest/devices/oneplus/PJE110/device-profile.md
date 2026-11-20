# 真机硬件规格与云机对齐留档 · PJE110 (oneplus/PJE110)

- **生成时间**: `2026-11-19T11:07:05+08:00`
- **ROM Build ID**: `13-TP1A.220905.001-20250926`
- **DSH 聊天会话 ID**: `measured-only`（标题：`[02_prepare] 真机规格采集 · oneplus/PJE110 (13-TP1A.220905.001-20250926)`）

---

## 一、设备表对齐规格（对应 `devices/manifest/devices/oneplus/PJE110.json`）

| 字段 | 对齐值 | 说明 |
| --- | --- | --- |
| `official` | `OPPO` | 厂商集团 |
| `brand` | `oneplus` | 子品牌标识 |
| `model` | `PJE110` | 商业上市机型名  |
| `device` | `PJE110` | 设备代号 |
| `platform` / `board` | `qcom` / `kalama` | 芯片平台 / 主板代号 |
| `resolution` | `1080x2400` | 屏幕物理像素分辨率 |
| `inch` | `70x155` | 屏幕可视物理宽×高 mm（对角线 `0.00"`） |
| `density` | `440` | Android 逻辑密度 dpi（物理 PPI `0`，xdpi `0.00` / ydpi `0.00`） |
| `smp` | `8` | CPU 核心总数 |
| `memory` | `[8192]` | 全系发售内存档位 MB（8G，-） |
| `storage` | `[128000 256000]` | 全系发售存储档位（128G / 256G，-） |

## 二、真机深层硬件对齐画像（参考 `redmi/gold` 标准）

### 1. SoC 与 CPU 簇全量频点表（`cdh_cpufreq` / `module/config.json` 对齐）
- **SoC 芯片**:   (`` , 0nm)
- **CPU 拓扑**: `-`（驱动: `-`，调速器: `-`，可选调速器: `[]`，BogoMIPS: `-`）

### 2. 显示面板、GPU 与图形栈（`gpu_id.cfg` / `buildVendor` 对标 `gold`）
- **面板类型与刷新率**: - · 刷新率 `[] Hz`（默认 `0 Hz`） · 广色域 `false` · EDID `-` (`-`, product=`0`, year=`0`W`0`)
- **OpenGL ES**: `-` / `-` / `-`
- **Vulkan**: `ro.hardware.vulkan=-` · VendorID `-` · DeviceID `-` · API `-` (`-`) · DriverID `0` (`-`)

### 3. 触控、指纹、外设与输入节点对齐（`module/config.json` & `hardware/config.json` 对标 `gold`）
- **指纹方案**: -
- **触控与按键对齐**: touch=`-` · keyboard=`-` · input_identity 外设数=`0` 项
- **硬件 HAL 变体与开关**: radioVariant=`-` · NFC=`false` · 红外=`false` · 3.5mm=`false` · -
- **影像规格**: 后置 - | 前置 -
- **电池与通信**: `0 mAh`（有线 `0W` / 无线 `0W`） · -（Wi-Fi 6=`false`） · BT `-` · 5G=`false` · Widevine `-` (sysId=`-`) · TAC `-` · MAC OUI `[]`

### 4. `vendor/etc/*_config.xml` 虚拟 HAL AI 对齐清单（对标 `gold-android15/vendor/etc`）
- **`sensors_config.xml`**: 共 `0` 项对齐传感器
- **`battery_config.xml`**: `Li-poly` · 容量 `0 mAh` · 满电/充电电压 `0 / 0 mV`
- **`thermal_config.xml`**: 皮肤基温 `0.000 ℃` · 温区传感器 `0` 项 · Cooling=`-`
- **`gnss_config.xml`**: systemInfo=`-` · yearOfHw=`0` · L5=`false`
- **`vibrator_config.xml` & `light_config.xml`**: 马达 `-` (cap=`0`, effects=`[]`) · 逻辑灯 `0` 项
- **`audio_effects_descriptors.xml` & `media_config.xml`**: AI 音效描述符 `0` 项 · AI Codec2 实例 `0` 项 / 组件 `0` 项

## 三、官方固件包解包实测对账（`build.prop` + `boot.img` + `VINTF` + `DTB` + `fstab` + `vendor/etc`）

- `marketName`: `PJE110` | `modelCode`: `ossi` | `manufacturer`: `oplus`
- `board`: `kalama` | `soc`: `QTI  ()`
- `densityDpi`: `0` | `egl`: `adreno` | `vulkan`: `adreno` | `abilist`: `arm64-v8a,armeabi-v7a,armeabi`
- `securityPatch`: `2025-10-01` | `buildDate`: `20250926` | `kernelLine`: `5.15` | `vintfVersion`: `-`
- `uname`: `Linux localhost 5.15.167-android13-8-o-01144-gdc8278c1c5f9 #1 SMP PREEMPT Wed Sep 3 15:30:49 UTC 2025 aarch64 Toybox`
- `fingerprint`: `oplus/ossi/ossi:13/TP1A.220905.001/1758858667782:user/release-keys`
- `vendor_boot.cmdline` (4 项): `androidboot.product.hardware.sku=ossi`, `androidboot.secureboot=1`, `androidboot.slot=a`, `androidboot.veritymode.managed=yes`
- `vda.partitions` (1 个附加分区): `vendor_persist.img->/mnt/vendor/persist`
- `vintfPlaceholders`: radio=`3` 项 · audio=`1` 项 · gnss=`1` 项 · nn=`41` 项

### `vendor/etc` 与 `odm/etc` 硬件配置实测详情

- **Permissions 硬件特性**（共 `92` 项）: 红外=`true` · NFC=`true` · 指纹=`false` · BLE=`true` · Wi-Fi Direct=`true` · Audio LowLatency=`true` · Vulkan Level=`1` (API `1.3`)
  - 声明的传感器特性 (`3` 项): `accelerometer`, `compass`, `dynamic.head_tracker`
  - 区域/SKU 差异权限: `sku_kalama=[android.hardware.sensor.accelerometer, android.hardware.sensor.compass, android.hardware.sensor.dynamic.head_tracker, android.hardware.sensor.gyroscope, android.hardware.sensor.proximity, android.hardware.sensor.stepcounter, android.hardware.sensor.stepdetector]`
- **Sensors 原厂配置**: SubHALs=`[sensors.ssc.so sensors.dynamic_sensor_hal.so]` · 芯片归属=`map[barometer:lps22df_0 imu:lsm6dsv magnetometer:ak0991x_0_platform]`
- **Audio 策略与音效配置**: 模块=`[primary a2dp usb]` · 采样率=`[48000]` · 听筒=`true` · 有线耳机路由=`false` · 双麦=`true` · 音效描述符=`0` 项（源: `-`）
- **Media Codec2 硬编解码配置**: Store实例=`[vendor.qti.media.c2@1.0-service->default1 vendor.qti.media.c2audio@1.0-service->default1]` · 厂商/杜比组件=`13` 项 (`c2.qti.avc.decoder`, `c2.qti.avc.decoder.low_latency`, `c2.qti.avc.decoder.secure`, `c2.qti.hevc.decoder`, `c2.qti.hevc.decoder.low_latency`, `c2.qti.hevc.decoder.secure`, `c2.qti.vp9.decoder`, `c2.qti.vp9.decoder.low_latency`, `c2.qti.vp9.decoder.secure`, `c2.qti.avc.encoder`, `c2.qti.hevc.encoder`, `c2.qti.hevc.encoder.cq`, `c2.qti.heic.encoder`)
- **Thermal 温控与冷却设备**: GPU Cooling=`-` · 冷却节点=`[]` · 温控场景表=`1` 份
- **GNSS 定位配置**: 源文件=`vendor/etc/gps.conf` · 版本=`-` · 模式=`-` · L1Only=`false` · L5配置=`false`
- **ODM SKU / NFC / Wi-Fi / 影像场景**: NFC芯片=`-` · RSC区域=`[]` · 相机Tuning角色=`[]`
- **已扫描解析的 `vendor/etc` & `odm/etc` 配置文件**: 共 `181` 个

## 四、DSH 聊天界面回复原文

CDH_PREPARE_MEASURED_ONLY=1
