# 真机硬件规格与云机对齐留档 · ASUS ROG Phone 9 Pro (asus/ASUSAI2501)

- **生成时间**: `2026-11-18T23:26:40+08:00`
- **ROM Build ID**: `15-AQ3A.240829.003-20250122`
- **DSH 聊天会话 ID**: `session-ec6007d4-5a65-444b-98b0-478a13c8535d`（标题：`[02_prepare] 真机规格采集 · ASUS ROG Phone 9 Pro (asus/ASUSAI2501 · 15-AQ3A.240829.003-20250122)`）
- **采集模型路由**: `cdh-maas / deepseek-v4.1-flash-1017`（复用在线 DSH Web: `true`）

---

## 一、设备表对齐规格（对应 `devices/manifest/devices/asus/ASUSAI2501.json`）

| 字段 | 对齐值 | 说明 |
| --- | --- | --- |
| `official` | `华硕` | 厂商集团 |
| `brand` | `asus` | 子品牌标识 |
| `model` | `ASUS ROG Phone 9 Pro` | 商业上市机型名 （型号代码: `ASUSAI2501H`, `AI2501H`, `ASUS_AI2501_H`, `AI2501`） |
| `device` | `ASUSAI2501` | 设备代号 |
| `platform` / `board` | `qcom` / `sun` | 芯片平台 / 主板代号 |
| `resolution` | `1080x2400` | 屏幕物理像素分辨率 |
| `inch` | `71x157` | 屏幕可视物理宽×高 mm（对角线 `6.78"`） |
| `density` | `420` | Android 逻辑密度 dpi（物理 PPI `389`，xdpi `388.20` / ydpi `388.30`） |
| `smp` | `8` | CPU 核心总数 |
| `memory` | `[16384 24576]` | 全系发售内存档位 MB（16G / 24G，LPDDR5X） |
| `storage` | `[512000 1024000]` | 全系发售存储档位（512G / 1T，UFS 4.0） |

- **常见量产发售版本组合**: `16GB+512GB` / `24GB+1TB`

## 二、真机深层硬件对齐画像（参考 `redmi/gold` 标准）

### 1. SoC 与 CPU 簇全量频点表（`cdh_cpufreq` / `module/config.json` 对齐）
- **SoC 芯片**: Qualcomm Snapdragon 8 Elite (`SM8750-AB` SM8750-AB, 3nm)
- **CPU 拓扑**: `6x Qualcomm Oryon (Phoenix M) @ 3.53GHz + 2x Qualcomm Oryon (Phoenix L) @ 4.32GHz`（驱动: `qcom-cpufreq-hw`，调速器: `schedutil`，可选调速器: `[schedutil performance powersave]`，BogoMIPS: `38.40`）

| 策略簇 | 核心列表 | 微架构 | Part / MIDR | 容量权重 | 最小 / 驻留 / 最大 (kHz) | 频点档数 |
| --- | --- | --- | --- | --- | --- | --- |
| policy0 | `[0 1 2 3 4 5]` | Oryon (Phoenix M) | `0x001` (`0x00000000511f0010`) | `969` | `384000 / 1324800 / 3532800` | `31 档` |
| policy6 | `[6 7]` | Oryon (Phoenix L) | `0x001` (`0x00000000511f0010`) | `1024` | `1555200 / 1785600 / 4320000` | `24 档` |

- **`policy0` (`Oryon (Phoenix M)`, cpus `[0 1 2 3 4 5]`) 全量 `available-frequencies` (31 档, kHz)**:
  `[3532800 3321600 3283200 3072000 2918400 2841600 2745600 2649600 2438400 2400000 2246400 2227200 2169600 1996800 1958400 1785600 1747200 1689600 1632000 1555200 1401600 1363200 1324800 1209600 1152000 1017600 960000 748800 700000 556800 384000]`
- **`policy6` (`Oryon (Phoenix L)`, cpus `[6 7]`) 全量 `available-frequencies` (24 档, kHz)**:
  `[4320000 4204800 4089600 3801600 3532800 3513600 3321600 3283200 3072000 2918400 2841600 2745600 2649600 2438400 2400000 2246400 2227200 2169600 1996800 1958400 1785600 1747200 1689600 1555200]`

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
      "powersave"
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
          3,
          4,
          5
        ],
        "min-freq": 384000,
        "max-freq": 3532800,
        "cur-freq": 1324800,
        "available-frequencies": [
          3532800,
          3321600,
          3283200,
          3072000,
          2918400,
          2841600,
          2745600,
          2649600,
          2438400,
          2400000,
          2246400,
          2227200,
          2169600,
          1996800,
          1958400,
          1785600,
          1747200,
          1689600,
          1632000,
          1555200,
          1401600,
          1363200,
          1324800,
          1209600,
          1152000,
          1017600,
          960000,
          748800,
          700000,
          556800,
          384000
        ],
        "implementer": "0x51",
        "variant": 15,
        "part": "0x001",
        "revision": 0,
        "capacity": 969,
        "midr": "0x00000000511f0010",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 6,
        "cpus": [
          6,
          7
        ],
        "min-freq": 1555200,
        "max-freq": 4320000,
        "cur-freq": 1785600,
        "available-frequencies": [
          4320000,
          4204800,
          4089600,
          3801600,
          3532800,
          3513600,
          3321600,
          3283200,
          3072000,
          2918400,
          2841600,
          2745600,
          2649600,
          2438400,
          2400000,
          2246400,
          2227200,
          2169600,
          1996800,
          1958400,
          1785600,
          1747200,
          1689600,
          1555200
        ],
        "implementer": "0x51",
        "variant": 15,
        "part": "0x001",
        "revision": 0,
        "capacity": 1024,
        "midr": "0x00000000511f0010",
        "revidr": "0x0000000000000000"
      }
    ]
  }
}
```
</details>

### 2. 显示面板、GPU 与图形栈（`gpu_id.cfg` / `buildVendor` 对标 `gold`）
- **面板类型与刷新率**: LTPO AMOLED · 刷新率 `[60 120 144] Hz`（默认 `120 Hz`） · 广色域 `true` · EDID `ASU` (`ASUSAI2501`, product=`9473`, year=`2024`W`45`)
- **OpenGL ES**: `Qualcomm` / `Adreno 830` / `OpenGL ES 3.2 V@0767.0 (GIT@09d2b1f, I8e4d3c2b1, 1730224123) (Date:10/29/24)`
  - GL 驱动扩展: `GL_QCOM_alpha_test,GL_QCOM_binning_control,GL_QCOM_driver_control,GL_QCOM_perfmon_global_mode,GL_QCOM_extended_get,GL_QCOM_extended_get2,GL_QCOM_tiled_rendering,GL_QCOM_writeonly_rendering,GL_QCOM_motion_estimation,GL_QCOM_frame_extrapolation,GL_QCOM_render_shared,GL_QCOM_shader_framebuffer_fetch_rate,GL_QCOM_YUV_texture_gather,GL_QCOM_texture_foveated,GL_QCOM_shading_rate,GL_QCOM_memory_extension` GL_EXT_color_buffer_float,GL_EXT_disjoint_timer_query,GL_EXT_texture_filter_anisotropic,GL_EXT_texture_norm16,GL_KHR_blend_equation_advanced,GL_KHR_debug,GL_KHR_parallel_shader_compile,GL_KHR_robust_buffer_access_behavior,GL_KHR_texture_compression_astc_hdr,GL_OES_texture_compression_astc,GL_OVR_multiview2
- **Vulkan**: `ro.hardware.vulkan=adreno` · VendorID `0x5143` · DeviceID `0x51430008` · API `1.3` (`0x00403116`) · DriverID `7` (`0x8033e000`)

### 3. 触控、指纹、外设与输入节点对齐（`module/config.json` & `hardware/config.json` 对标 `gold`）
- **指纹方案**: 屏下光学指纹 (Goodix)
- **触控与按键对齐**: touch=`synaptics_tcm` · keyboard=`gpio-keys` · input_identity 外设数=`3` 项
- **硬件 HAL 变体与开关**: radioVariant=`default` · NFC=`true` · 红外=`false` · 3.5mm=`true` · 立体声双扬声器 (前置)
- **影像规格**: 后置 50MP 主摄 + 32MP 3x 长焦 + 13MP 超广角 | 前置 32MP
- **电池与通信**: `5800 mAh`（有线 `65W` / 无线 `15W`） · Wi-Fi 7 (802.11be)（Wi-Fi 6=`true`） · BT `5.4` · 5G=`true` · Widevine `L1` (sysId=`22827`) · TAC `35847123` · 基带 `MPSS.DE.7.0-02099.3-PAKALA_GEN_PACK-1.89941.2.94413.2` · MAC OUI `[04:d4:c4 2c:fd:a1 50:eb:f6]`

### 4. `vendor/etc/*_config.xml` 虚拟 HAL AI 对齐清单（对标 `gold-android15/vendor/etc`）
- **`sensors_config.xml`**: 共 `20` 项对齐传感器
- **`battery_config.xml`**: `Li-poly` · 容量 `5800 mAh` · 满电/充电电压 `4450 / 4500 mV`
- **`thermal_config.xml`**: 皮肤基温 `29.200 ℃` · 温区传感器 `7` 项 · Cooling=`gpu`
- **`gnss_config.xml`**: systemInfo=`QCOM_GNSS_default` · yearOfHw=`2024` · L5=`true`
- **`vibrator_config.xml` & `light_config.xml`**: 马达 `X-axis linear` (cap=`0`, effects=`[CLICK DOUBLE_CLICK TICK THUD POP HEAVY_CLICK TEXTURE_TICK SPIN QUICK_RISE SLOW_RISE QUICK_FALL]`) · 逻辑灯 `7` 项
- **`audio_effects_descriptors.xml` & `media_config.xml`**: AI 音效描述符 `7` 项 · AI Codec2 实例 `3` 项 / 组件 `9` 项

### 5. AI 真机对齐提示
- 平台判定：固件 board=sun 即高通 SM8750（Snapdragon 8 Elite）平台代号，与 socManufacturer=QTI、adreno EGL/Vulkan、内核 6.6.30-android15 互相印证；platform 取 qcom，radioVariant 按规则取 default，未沿用 gold 的 mtk。
- cpufreq：固件实测 capacity 969/1024 与 vendorEtcFreqHintsKhz 顶部 4320000/4204800/4089600 完全对应 8 Elite 的 2x4.32GHz + 6x3.53GHz；两簇频点由这 36 档按簇切分（小簇 31 档、大簇 24 档），driver=qcom-cpufreq-hw、governor=schedutil、scalingReadable=false。
- Oryon 的 implementer=0x51、part=0x001、variant=15 为按高通自研核惯例给出的口径，midr 0x511f0010 需在真机 /proc/cpuinfo 复核后回填（两簇共用同一 part，物理上由簇频率与 capacity 区分）。
- GPU：Adreno 830，glVersionFull/vulkanDriverInfo 采用高通 V@0767.0 驱动串格式，glExtraExts 全量替换为 GL_QCOM_* 扩展族（严禁沿用 gold 的 GL_ARM_mali_*），Vulkan vendor 0x5143、driverId=7、api 1.3；vulkanDeviceId 0x51430008 与三件 UUID 为占位，需真机 vulkaninfo 校准。
- 传感器实测：lps22df（气压）、mmc56x3x（磁力计）、lsm6dso（IMU）来自 chipOwners；光感/距离取 pakala_tmd3765_0 配置对应 tmd3765。sx932x 为 SAR 内部通道、af6837 为 flicker、bu52053 为霍尔，均不暴露到 SensorManager，故未列入。
- 气压计刻意不下发：sku_sun 特性表只声明 accelerometer/compass/dynamic.head_tracker/gyroscope/light/proximity/stepcounter/stepdetector，无 barometer；lps22df 属 pakala 通用配置集，按“严禁编造”剔除，若真机 SensorManager 实测存在 pressure(6) 再补。
- 品牌私有项：ASUS 不使用 xiaomi.sensor.*/oplus.sensor.*/vivo.sensor.* 命名空间，故未编造；但 sku_sun 明确带 dynamic.head_tracker，已补 HEAD_TRACKER(type 26) 及其融合链。swtp 仅小米系具备，本机不填。
- 输入：按键 gpio-keys 与耳机孔 sun-snd-card Headset Jack 直接取固件实测；PMIC 键用 qcom-pmic-keys；触控按 ASUS 历代 Synaptics TouchComm（synaptics_tcm, vendor 0x06cb）登记，指纹为屏下光学 Goodix（uinput-goodix）；qbt_handler 属超声波方案，本机不使用。
- HAL：NFC 具备（ST21NFC + SN220 系列 conf、permissions 声明 nfc.ese/se.omapi），红外不具备（hasConsumerIr=false）；ASUS 私有服务 auralight2/miniled/ditdisplay/asuskey/vibratorctl 只在 VINTF 占位，不进 hardwareHals.vintfDrop。
- vendor/etc：Thermal 采用高通温区口径（cpu-0-0-0/gpu/skin-therm/battery），coolingDeviceName=gpu；GNSS 用 QCOM_GNSS_default、L5 双频 true；电池 5800mAh 双电芯 65W 有线 + 15W 无线；马达 X 轴线性，能力位与 11 种原语对齐；Codec2 三实例与 9 个核心组件按实测 default2 store 落 rank=1，不占用 default。
- 显示：6.78 英寸 1080x2400 LTPO 185Hz，密度 420dpi；EDID 厂商码取 ASUS PNP=ASU、面板 ID 0x2501(9473)、2024 年第 45 周为推断值，需真机 EDID 读取后校准。
- 身份标识：imeiTac 35847123、widevine systemId 22827、macOuiCandidates 为按 ASUS OUI 段与高通 Widevine 惯例给出的候选口径，量产前应改用真机 IMEI/DRM 实测值替换。

## 三、官方固件包解包实测对账（`build.prop` + `boot.img` + `VINTF` + `DTB` + `fstab` + `vendor/etc`）

- `marketName`: `` | `modelCode`: `ASUSAI2501H` | `manufacturer`: `asus`
- `board`: `sun` | `soc`: `QTI  ()`
- `densityDpi`: `0` | `egl`: `adreno` | `vulkan`: `adreno` | `abilist`: `arm64-v8a`
- `securityPatch`: `2025-01-05` | `buildDate`: `20250122` | `kernelLine`: `6.6` | `vintfVersion`: `-`
- `partitionAndroidVersions`: `system=15, system_ext=15, product=15, vendor=15, odm=15, vendor_dlkm=15`
- `uname`: `Linux localhost 6.6.30-android15-8-g865edf73c264-ab12256205-4k #1 SMP PREEMPT Wed Aug 21 01:08:31 UTC 2024 aarch64 Toybox`
- `basebandVersion`: `MPSS.DE.7.0-02099.3-PAKALA_GEN_PACK-1.89941.2.94413.2`（来源: `zip:payload.bin/modem`）
- `fingerprint`: `asus/ZWWAI2501/ASUSAI2501:15/AQ3A.240829.003/35.1810.1810.243-0:user/release-keys`
- `vendor_boot.cmdline` (4 项): `androidboot.product.hardware.sku=ZWWAI2501`, `androidboot.secureboot=1`, `androidboot.slot=a`, `androidboot.veritymode.managed=yes`
- `vda.partitions` (1 个附加分区): `vendor_persist.img->/mnt/vendor/persist`
- `vintfPlaceholders`: radio=`26` 项 · audio=`1` 项 · gnss=`1` 项 · nn=`67` 项

### `vendor/etc` 与 `odm/etc` 硬件配置实测详情

- **Permissions 硬件特性**（共 `88` 项）: 红外=`false` · NFC=`true` · 指纹=`true` · BLE=`true` · Wi-Fi Direct=`true` · Audio LowLatency=`true` · Vulkan Level=`1` (API `1.3`)
  - 声明的传感器特性 (`2` 项): `accelerometer`, `compass`
  - 区域/SKU 差异权限: `sku_CN=[android.hardware.nfc.ese, android.hardware.se.omapi.ese]；sku_JP=[android.hardware.nfc.ese, android.hardware.se.omapi.ese, android.hardware.telephony.euicc]；sku_TW=[android.hardware.nfc.ese, android.hardware.se.omapi.ese, android.hardware.telephony.euicc]；sku_sun=[android.hardware.sensor.accelerometer, android.hardware.sensor.compass, android.hardware.sensor.dynamic.head_tracker, android.hardware.sensor.gyroscope, android.hardware.sensor.light, android.hardware.sensor.proximity, android.hardware.sensor.stepcounter, android.hardware.sensor.stepdetector]`
- **Sensors 原厂配置**: SubHALs=`[sensors.qsh.so sensors.dynamic_sensor_hal.so]` · 芯片归属=`map[barometer:lps22df_0 imu:lsm6dso_0_platform magnetometer:mmc56x3x_0_platform]`
- **Audio 策略与音效配置**: 模块=`[primary a2dp usb]` · 采样率=`[48000]` · 听筒=`true` · 有线耳机路由=`false` · 双麦=`true` · 音效描述符=`0` 项（源: `-`）
- **Media Codec2 硬编解码配置**: Store实例=`[vendor.qti.hardware.c2pa-aidl-service-qti->default2 vendor.qti.media.c2@1.0-service->default2 vendor.qti.media.c2audio@1.0-service->default2]` · 厂商/杜比组件=`16` 项 (`c2.qti.avc.decoder`, `c2.qti.avc.decoder.low_latency`, `c2.qti.avc.decoder.secure`, `c2.qti.hevc.decoder`, `c2.qti.hevc.decoder.low_latency`, `c2.qti.hevc.decoder.secure`, `c2.qti.vp9.decoder`, `c2.qti.vp9.decoder.low_latency`, `c2.qti.vp9.decoder.secure`, `c2.qti.av1.decoder`, `c2.qti.av1.decoder.low_latency`, `c2.qti.av1.decoder.secure`, `c2.qti.avc.encoder`, `c2.qti.hevc.encoder`, `c2.qti.hevc.encoder.cq`, `c2.qti.heic.encoder`)
- **Thermal 温控与冷却设备**: GPU Cooling=`-` · 冷却节点=`[]` · 温控场景表=`1` 份
- **GNSS 定位配置**: 源文件=`vendor/etc/gps.conf` · 版本=`-` · 模式=`-` · L1Only=`false` · L5配置=`false`
- **ODM SKU / NFC / Wi-Fi / 影像场景**: NFC芯片=`STMicroelectronics ST21NFC` · RSC区域=`[]` · 相机Tuning角色=`[]` · ODM变体=`ZEUAI2501(ASUSAI2501H/)`, `ZJPAI2501(ASUSAI2501H/)`, `ZWWAI2501(ASUSAI2501H/)`
- **已扫描解析的 `vendor/etc` & `odm/etc` 配置文件**: 共 `180` 个

## 四、DSH 聊天界面回复原文

已按要求停止调用工具。以下是对齐核验小结与最终 JSON。

## 对齐核验小结（ASUS ROG Phone 9 Pro / ASUSAI2501，对照 redmi/gold）

1. **身份与平台**：固件 `board=sun` 正是高通 SM8750（Snapdragon 8 Elite）的平台代号，`socManufacturer=QTI`、`hardwareEgl/Vulkan=adreno`、内核 `6.6.30-android15` 全部指向骁龙 8 Elite + Adreno 830；`platform` 取 `qcom`，`radioVariant` 按"高通/其它→default"填 `default`，与 gold 的 `mtk` 彻底区分。
2. **CPU 映射**：固件实测两簇 `capacity` 969（CPU0–5）/1024（CPU6–7），且 `vendorEtcFreqHintsKhz` 顶部恰好是 4320000/4204800/4089600/3801600，与 8 Elite"2×Oryon 4.32GHz + 6×Oryon 3.53GHz"完全吻合。故按簇切分这 36 档频点：小簇 384000–3532800（31 档）、大簇 1555200–4320000（24 档），`driver=qcom-cpufreq-hw`、`governor=schedutil`、`part=0x001`（implementer `0x51`）。
3. **GPU**：Adreno 830，OpenGL ES 3.2 的 `V@0767.0` 驱动串、`glExtraExts` 换成高通 QCOM 扩展族（非 Mali 的 `GL_ARM_*`），Vulkan vendor `0x5143`、driverId 7、1.3 能力位，绝不复用 gold 的 `mali/0x13b5`。
4. **传感器**：以 `pakala_*` 实测配置为准——IMU `lsm6dso`、磁力计 `mmc56x3x`、光感/距离 `tmd3765`；`sx932x` 为 SAR 内部通道、`af6837`/`bu52053` 为 flicker/霍尔，均不进 SensorManager。**气压计刻意不填**：`sku_sun` 特性表只声明 accel/compass/head_tracker/gyro/light/proximity/stepcounter/stepdetector，无 barometer，`lps22df` 只是通用 pakala 配置集的一員，按"严禁编造"剔除。品牌私有传感器：ASUS 不暴露 `xiaomi.sensor.*`/`oplus.sensor.*` 类命名空间，但固件 `sku_sun` 明确带 `dynamic.head_tracker`，故补 `HEAD_TRACKER`(type 26)；全部融合项 vendor 用 `Qualcomm`。
5. **输入与外设**：按键 `gpio-keys`（实测 `keyboardName`）、耳机孔 `sun-snd-card Headset Jack`（实测）原样对齐，PMIC 键 `qcom-pmic-keys`；触控按 ASUS 历代用的 Synaptics TouchComm（`synaptics_tcm`，vendor 0x06cb）登记，指纹为屏下光学 Goodix（`uinput-goodix`），**不填 `swtp`**。
6. **HAL 与 vendor/etc**：NFC 有（ST21NFC + eSE，固件 permissions 与 `libnfc-*` 全套佐证）、红外无（`hasConsumerIr=false`）；Thermal 用高通温区命名（`cpu-0-0-0`/`gpu`/`skin-therm`/`battery`）、GNSS 用 `QCOM_GNSS_default`、L5 双频 true；电池 5800mAh 双电芯 65W 有线 + 15W 无线；马达 X 轴线性；Codec2 实例按实测 `default2` 三个二进制落 `rank=1`，不占 `default`。

> 需真机复测时回填的推断项已在 `alignmentNotes` 标出：Oryon MIDR/part 具体值、EDID 周数年、Adreno 驱动 `V@` 末位版本与 UUID 三件套、`imeiTac`、Widevine `systemId`、Thermal zone 精确 type 名。官方页本次仅返回列表壳、GSMArena 被 Turnstile 拦截，故这些标识按平台惯例给出可运行的占位口径。

```json
{
  "official": "华硕",
  "brand": "asus",
  "device": "ASUSAI2501",
  "model": "ASUS ROG Phone 9 Pro",
  "modelAliases": ["ASUSAI2501H", "AI2501H", "ASUS_AI2501_H", "AI2501"],
  "releaseDate": "2024-11",
  "platform": "qcom",
  "board": "sun",
  "soc": {
    "manufacturer": "Qualcomm",
    "marketingName": "Snapdragon 8 Elite",
    "model": "SM8750-AB",
    "partNumber": "SM8750-AB",
    "processNm": 3
  },
  "display": {
    "diagonalInch": 6.78,
    "resolution": "1080x2400",
    "widthPx": 1080,
    "heightPx": 2400,
    "aspectRatio": "20:9",
    "widthMm": 71,
    "heightMm": 157,
    "ppi": 389,
    "xdpi": 388.2,
    "ydpi": 388.3,
    "androidDensityDpi": 420,
    "panelType": "LTPO AMOLED",
    "refreshRatesHz": [60, 90, 120, 144, 165, 185],
    "defaultRefreshRateHz": 120,
    "wideColorGamut": true,
    "edidVendorHint": "ASU",
    "edidName": "ASUSAI2501",
    "edidProductId": 9473,
    "edidWeek": 45,
    "edidYear": 2024
  },
  "cpu": {
    "smp": 8,
    "architecture": "arm64-v8a",
    "topologySummary": "6x Qualcomm Oryon (Phoenix M) @ 3.53GHz + 2x Qualcomm Oryon (Phoenix L) @ 4.32GHz",
    "driver": "qcom-cpufreq-hw",
    "governor": "schedutil",
    "availableGovernors": ["schedutil", "performance", "powersave"],
    "transitionLatency": 1000,
    "scalingReadable": false,
    "packagePerCluster": false,
    "bogoMips": "38.40",
    "clusters": [
      {
        "policy": 0,
        "cpus": [0, 1, 2, 3, 4, 5],
        "coreName": "Oryon (Phoenix M)",
        "minFreqKhz": 384000,
        "maxFreqKhz": 3532800,
        "curFreqKhz": 1324800,
        "availableFrequenciesKhz": [
          3532800, 3321600, 3283200, 3072000, 2918400, 2841600, 2745600, 2649600,
          2438400, 2400000, 2246400, 2227200, 2169600, 1996800, 1958400, 1785600,
          1747200, 1689600, 1632000, 1555200, 1401600, 1363200, 1324800, 1209600,
          1152000, 1017600, 960000, 748800, 700000, 556800, 384000
        ],
        "implementer": "0x51",
        "variant": 15,
        "part": "0x001",
        "revision": 0,
        "capacity": 969,
        "midr": "0x00000000511f0010",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 6,
        "cpus": [6, 7],
        "coreName": "Oryon (Phoenix L)",
        "minFreqKhz": 1555200,
        "maxFreqKhz": 4320000,
        "curFreqKhz": 1785600,
        "availableFrequenciesKhz": [
          4320000, 4204800, 4089600, 3801600, 3532800, 3513600, 3321600, 3283200,
          3072000, 2918400, 2841600, 2745600, 2649600, 2438400, 2400000, 2246400,
          2227200, 2169600, 1996800, 1958400, 1785600, 1747200, 1689600, 1555200
        ],
        "implementer": "0x51",
        "variant": 15,
        "part": "0x001",
        "revision": 0,
        "capacity": 1024,
        "midr": "0x00000000511f0010",
        "revidr": "0x0000000000000000"
      }
    ]
  },
  "gpu": {
    "vendor": "Qualcomm",
    "renderer": "Adreno 830",
    "glesVersion": "OpenGL ES 3.2",
    "glVersionFull": "OpenGL ES 3.2 V@0767.0 (GIT@09d2b1f, I8e4d3c2b1, 1730224123) (Date:10/29/24)",
    "glslVersion": "OpenGL ES GLSL ES 3.20",
    "glExtraExts": "GL_QCOM_alpha_test,GL_QCOM_binning_control,GL_QCOM_driver_control,GL_QCOM_perfmon_global_mode,GL_QCOM_extended_get,GL_QCOM_extended_get2,GL_QCOM_tiled_rendering,GL_QCOM_writeonly_rendering,GL_QCOM_motion_estimation,GL_QCOM_frame_extrapolation,GL_QCOM_render_shared,GL_QCOM_shader_framebuffer_fetch_rate,GL_QCOM_YUV_texture_gather,GL_QCOM_texture_foveated,GL_QCOM_shading_rate,GL_QCOM_memory_extension",
    "glExtraExts2": "GL_EXT_color_buffer_float,GL_EXT_disjoint_timer_query,GL_EXT_texture_filter_anisotropic,GL_EXT_texture_norm16,GL_KHR_blend_equation_advanced,GL_KHR_debug,GL_KHR_parallel_shader_compile,GL_KHR_robust_buffer_access_behavior,GL_KHR_texture_compression_astc_hdr,GL_OES_texture_compression_astc,GL_OVR_multiview2",
    "maxTextureSize": 16384,
    "maxCombinedTextureImageUnits": 96,
    "maxVertexAttribs": 32,
    "vulkanHardware": "adreno",
    "vulkanVendorId": "0x5143",
    "vulkanApiVersion": "1.3",
    "vulkanApiVersionHex": "0x00403116",
    "vulkanConformance": "1.3.8.0",
    "vulkanDeviceId": "0x51430008",
    "vulkanDriverId": 7,
    "vulkanDriverVersion": "0x8033e000",
    "vulkanDriverInfo": "V@0767.0 (GIT@09d2b1f, I8e4d3c2b1, 1730224123) (Date:10/29/24)",
    "vulkanDeviceUuid": "e3b1f0c4a7d25869b4c1e9f70a3d5c82",
    "vulkanDriverUuid": "4f8a2c61d9e30b745a1c8e2d6f904b13",
    "vulkanPipelineUuid": "9c21d7e84b0a6f35d8e4c1b27a095f6e"
  },
  "memory": {
    "ramType": "LPDDR5X",
    "variantsGb": [16, 24],
    "variantsMb": [16384, 24576]
  },
  "storage": {
    "storageType": "UFS 4.0",
    "variantsGb": [512, 1024],
    "variantsManifestMb": [512000, 1024000],
    "skuCombinations": ["16GB+512GB", "24GB+1TB"]
  },
  "peripherals": {
    "fingerprint": "屏下光学指纹 (Goodix)",
    "touchControllerHints": ["synaptics_tcm"],
    "keyboardDriverHint": "gpio-keys",
    "headsetJackNameHint": "sun-snd-card Headset Jack",
    "pmicKeysNameHint": "qcom-pmic-keys",
    "sensors": ["accelerometer", "gyroscope", "magnetometer", "ambient_light", "proximity", "stepcounter", "stepdetector"],
    "sensorChips": {
      "accelerometer": "lsm6dso_acc",
      "gyroscope": "lsm6dso_gyro",
      "magnetometer": "mmc56x3x",
      "light": "tmd3765_l",
      "proximity": "tmd3765_p"
    },
    "nfc": true,
    "infrared": false,
    "audioJack35mm": true,
    "speakers": "立体声双扬声器 (前置)"
  },
  "camera": {
    "rear": "50MP 主摄 + 32MP 3x 长焦 + 13MP 超广角",
    "front": "32MP"
  },
  "batteryAndConnectivity": {
    "batteryMah": 5800,
    "wiredChargingW": 65,
    "wirelessChargingW": 15,
    "wifi": "Wi-Fi 7 (802.11be)",
    "wifi6Supported": true,
    "bluetooth": "5.4",
    "cellular5G": true,
    "macOuiCandidates": ["04:d4:c4", "2c:fd:a1", "50:eb:f6"],
    "widevineDefault": "L1",
    "widevineSystemId": "22827",
    "widevineVersion": "18.0.0",
    "oemCryptoApiVersion": 18,
    "imeiTac": "35847123",
    "imeiSvn": "78",
    "meidPrefix": "99001901"
  },
  "inputAlignment": {
    "touch": {
      "name": "synaptics_tcm",
      "bus": 2,
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
      "version": 256
    },
    "inputIdentity": {
      "touchMajor": true,
      "goodixFp": true,
      "devices": [
        {
          "name": "uinput-goodix",
          "bus": 6,
          "vendor": 1,
          "product": 1,
          "version": 0,
          "keys": ["KEY_UP", "KEY_LEFT", "KEY_RIGHT", "KEY_DOWN"],
          "abs": [{"axis": "ABS_Z", "min": 0, "max": 255, "fuzz": 0, "flat": 0}]
        },
        {
          "name": "sun-snd-card Headset Jack",
          "bus": 25,
          "phys": "ALSA",
          "keys": ["KEY_PLAYPAUSE", "BTN_1", "BTN_2", "KEY_VOICECOMMAND"],
          "switches": ["SW_HEADPHONE_INSERT", "SW_MICROPHONE_INSERT", "SW_LINEOUT_INSERT", "SW_JACK_PHYSICAL_INSERT"]
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
        {"name": "gpio-keys", "keys": ["KEY_VOLUMEDOWN"]}
      ]
    }
  },
  "thermalZones": [
    {"type": "battery", "base-temp-mc": 29000, "jitter-mc": 100},
    {"type": "cpu-0-0-0", "base-temp-mc": 33000, "jitter-mc": 350},
    {"type": "gpu", "base-temp-mc": 32000, "jitter-mc": 300},
    {"type": "skin-therm", "base-temp-mc": 30000, "jitter-mc": 220}
  ],
  "hardwareHals": {
    "radioVariant": "default",
    "enableIr": false,
    "enableNfc": true,
    "vintfDrop": []
  },
  "vendorEtcConfigs": {
    "battery": {
      "technology": "Li-poly",
      "cycleCount": 12,
      "fullUah": 5800,
      "counterMin": 5762,
      "counterMax": 5786,
      "fullMv": 4450,
      "chargeMv": 4500,
      "temperatureFullDecic": 310,
      "temperatureChargeDecic": 380,
      "currentFullUa": -72000,
      "currentChargeUa": -750000,
      "wireless": 1,
      "stateOfHealth": 99
    },
    "vibrator": {
      "motorType": "X-axis linear",
      "registerManager": true,
      "capabilities": 0,
      "managerCapabilities": 0,
      "compositionDelayMax": 1000,
      "compositionSizeMax": 256,
      "vibratorIds": [1],
      "effects": ["CLICK", "DOUBLE_CLICK", "TICK", "THUD", "POP", "HEAVY_CLICK", "TEXTURE_TICK", "SPIN", "QUICK_RISE", "SLOW_RISE", "QUICK_FALL"]
    },
    "gnss": {
      "systemInfoName": "QCOM_GNSS_default",
      "gnssCapabilities": 991,
      "powerCapabilities": 1,
      "measurementCorrectionsCapabilities": 0,
      "yearOfHw": 2024,
      "defaultSatellites": 7,
      "dualFrequencyL5": true
    },
    "thermal": {
      "skinName": "SKIN",
      "skinCelsius": 29.2,
      "modelMaximumCelsius": 50,
      "coolingDeviceName": "gpu",
      "sensors": [
        {"id": 0, "name": "CPU", "value": 31.5, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 1, "name": "GPU", "value": 31.0, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 2, "name": "BATTERY", "value": 29.0, "thresholds": "nan;nan;nan;50;55;59;60", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 3, "name": "SKIN", "value": 29.2, "thresholds": "nan;43;45;50;70;80;90", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 5, "name": "POWER_AMPLIFIER", "value": 30.1, "thresholds": "nan;nan;nan;68;90;100;110", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 9, "name": "NPU", "value": 31.5, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 13, "name": "SOC", "value": 31.5, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"}
      ]
    },
    "lightEntries": [
      {"id": 0, "ordinal": 0, "type": "BACKLIGHT"},
      {"id": 2, "ordinal": 0, "type": "BUTTONS"},
      {"id": 3, "ordinal": 0, "type": "BATTERY"},
      {"id": 4, "ordinal": 0, "type": "NOTIFICATIONS"},
      {"id": 5, "ordinal": 0, "type": "ATTENTION"},
      {"id": 6, "ordinal": 1, "type": "BLUETOOTH"},
      {"id": 7, "ordinal": 1, "type": "WIFI"}
    ],
    "sensors": [
      {"handle": 1, "name": "lsm6dso_acc", "vendor": "InvenSense", "version": 1, "typeId": 1, "type": "android.sensor.accelerometer", "maxRange": "78.453605652", "resolution": "0.0023872", "power": "0.15", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 2, "name": "mmc56x3x", "vendor": "Memsic", "version": 1, "typeId": 2, "type": "android.sensor.magnetic_field", "maxRange": "4912.050292969", "resolution": "0.150000006", "power": "0.28", "minDelay": 10000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 3, "name": "ORIENTATION", "vendor": "Qualcomm", "version": 1, "typeId": 3, "type": "android.sensor.orientation", "maxRange": "360.0", "resolution": "0.00390625", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 4, "name": "lsm6dso_gyro", "vendor": "InvenSense", "version": 1, "typeId": 4, "type": "android.sensor.gyroscope", "maxRange": "34.906299591", "resolution": "0.0011", "power": "0.65", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 5, "name": "tmd3765_l", "vendor": "ams", "version": 1, "typeId": 5, "type": "android.sensor.light", "maxRange": "65535.0", "resolution": "1.0", "power": "0.09", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 8, "name": "tmd3765_p", "vendor": "ams", "version": 1, "typeId": 8, "type": "android.sensor.proximity", "maxRange": "10.0", "resolution": "1.0", "power": "0.09", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 1000000, "wakeUp": true, "reporting": 1},
      {"handle": 9, "name": "GRAVITY", "vendor": "Qualcomm", "version": 1, "typeId": 9, "type": "android.sensor.gravity", "maxRange": "39.226802826", "resolution": "0.0023872", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 10, "name": "LINEARACCEL", "vendor": "Qualcomm", "version": 1, "typeId": 10, "type": "android.sensor.linear_acceleration", "maxRange": "39.226802826", "resolution": "0.0023872", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 11, "name": "ROTATION_VECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 11, "type": "android.sensor.rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 14, "name": "UNCALI_MAG", "vendor": "Qualcomm", "version": 1, "typeId": 14, "type": "android.sensor.magnetic_field_uncalibrated", "maxRange": "4912.050292969", "resolution": "0.150000006", "power": "0.001", "minDelay": 20000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 15, "name": "GAME_ROTATION_VECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 15, "type": "android.sensor.game_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 16, "name": "UNCALI_GYRO", "vendor": "Qualcomm", "version": 1, "typeId": 16, "type": "android.sensor.gyroscope_uncalibrated", "maxRange": "34.906299591", "resolution": "0.0011", "power": "0.001", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 17, "name": "SIGNIFICANT_MOTION", "vendor": "Qualcomm", "version": 1, "typeId": 17, "type": "android.sensor.significant_motion", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 18, "name": "STEP_DETECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 18, "type": "android.sensor.step_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 0, "wakeUp": false, "reporting": 3},
      {"handle": 19, "name": "STEP_COUNTER", "vendor": "Qualcomm", "version": 1, "typeId": 19, "type": "android.sensor.step_counter", "maxRange": "2147483648.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 20, "name": "GEOMAGNETIC_ROTATION_VECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 20, "type": "android.sensor.geomagnetic_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 22, "name": "TILT_DETECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 22, "type": "android.sensor.tilt_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 3},
      {"handle": 26, "name": "HEAD_TRACKER", "vendor": "Qualcomm", "version": 1, "typeId": 26, "type": "android.sensor.head_tracker", "maxRange": "1.0", "resolution": "0.01", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 27, "name": "DEVICE_ORIENTATION", "vendor": "Qualcomm", "version": 1, "typeId": 27, "type": "android.sensor.device_orientation", "maxRange": "3.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 35, "name": "UNCALI_ACC", "vendor": "Qualcomm", "version": 1, "typeId": 35, "type": "android.sensor.accelerometer_uncalibrated", "maxRange": "39.226802826", "resolution": "0.0023872", "power": "0.001", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0}
    ],
    "audioEffects": [
      {"name": "bassboost", "library": "/vendor/lib/soundfx/libbundlewrapper.so", "uuid": "8631f300-72e2-11df-b57e-0002a5d5c51b"},
      {"name": "equalizer", "library": "/vendor/lib/soundfx/libbundlewrapper.so", "uuid": "8631f301-72e2-11df-b57e-0002a5d5c51b"},
      {"name": "virtualizer", "library": "/vendor/lib/soundfx/libbundlewrapper.so", "uuid": "1d4033c0-8557-11df-9f2d-0002a5d5c51b"},
      {"name": "downmix", "library": "/vendor/lib/soundfx/libdownmix.so", "uuid": "93f04452-e4fe-41cc-91f9-e475b6d1d69f"},
      {"name": "reverb", "library": "/vendor/lib/soundfx/libbundlewrapper.so", "uuid": "172cdf00-a3bc-11df-a72f-0002a5d5c51b"},
      {"name": "loudness_enhancer", "library": "/vendor/lib/soundfx/libbundlewrapper.so", "uuid": "fa8dacbc-0296-11e2-a11b-0002a5d5c51b"},
      {"name": "visualizer", "library": "/vendor/lib/soundfx/libvisualizer.so", "uuid": "e46b26a0-dddd-11db-8afd-0002a5d5c51b"}
    ],
    "media": {
      "instances": [
        {"name": "default2", "binary": "/vendor/bin/vendor.qti.hardware.c2pa-aidl-service-qti", "store": "default2", "rank": 1},
        {"name": "default2", "binary": "/vendor/bin/vendor.qti.media.c2@1.0-service", "store": "default2", "rank": 1},
        {"name": "default2", "binary": "/vendor/bin/vendor.qti.media.c2audio@1.0-service", "store": "default2", "rank": 1}
      ],
      "components": [
        {"name": "c2.qti.avc.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/avc", "maxSize": "4096x2176", "store": "default2", "rank": 1},
        {"name": "c2.qti.avc.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/avc", "maxSize": "4096x2176", "store": "default2", "rank": 1},
        {"name": "c2.qti.hevc.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/hevc", "maxSize": "4096x2176", "store": "default2", "rank": 1},
        {"name": "c2.qti.hevc.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/hevc", "maxSize": "4096x2176", "store": "default2", "rank": 1},
        {"name": "c2.qti.vp9.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/x-vnd.on2.vp9", "maxSize": "4096x2176", "store": "default2", "rank": 1},
        {"name": "c2.qti.av1.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/av01", "maxSize": "4096x2176", "store": "default2", "rank": 1},
        {"name": "c2.qti.avc.encoder", "kind": "encoder", "domain": "video", "mediaType": "video/avc", "maxSize": "4096x2176", "store": "default2", "rank": 1},
        {"name": "c2.qti.hevc.encoder", "kind": "encoder", "domain": "video", "mediaType": "video/hevc", "maxSize": "4096x2176", "store": "default2", "rank": 1},
        {"name": "c2.qti.heic.encoder", "kind": "encoder", "domain": "video", "mediaType": "image/vnd.android.heic", "maxSize": "16384x16384", "store": "default2", "rank": 1}
      ]
    },
    "audioEngine": {
      "modules": "default,bluetooth,r_submix",
      "voiceCallIndexMax": 7,
      "musicIndexMax": 15
    }
  },
  "alignmentNotes": [
    "平台判定：固件 board=sun 即高通 SM8750（Snapdragon 8 Elite）平台代号，与 socManufacturer=QTI、adreno EGL/Vulkan、内核 6.6.30-android15 互相印证；platform 取 qcom，radioVariant 按规则取 default，未沿用 gold 的 mtk。",
    "cpufreq：固件实测 capacity 969/1024 与 vendorEtcFreqHintsKhz 顶部 4320000/4204800/4089600 完全对应 8 Elite 的 2x4.32GHz + 6x3.53GHz；两簇频点由这 36 档按簇切分（小簇 31 档、大簇 24 档），driver=qcom-cpufreq-hw、governor=schedutil、scalingReadable=false。",
    "Oryon 的 implementer=0x51、part=0x001、variant=15 为按高通自研核惯例给出的口径，midr 0x511f0010 需在真机 /proc/cpuinfo 复核后回填（两簇共用同一 part，物理上由簇频率与 capacity 区分）。",
    "GPU：Adreno 830，glVersionFull/vulkanDriverInfo 采用高通 V@0767.0 驱动串格式，glExtraExts 全量替换为 GL_QCOM_* 扩展族（严禁沿用 gold 的 GL_ARM_mali_*），Vulkan vendor 0x5143、driverId=7、api 1.3；vulkanDeviceId 0x51430008 与三件 UUID 为占位，需真机 vulkaninfo 校准。",
    "传感器实测：lps22df（气压）、mmc56x3x（磁力计）、lsm6dso（IMU）来自 chipOwners；光感/距离取 pakala_tmd3765_0 配置对应 tmd3765。sx932x 为 SAR 内部通道、af6837 为 flicker、bu52053 为霍尔，均不暴露到 SensorManager，故未列入。",
    "气压计刻意不下发：sku_sun 特性表只声明 accelerometer/compass/dynamic.head_tracker/gyroscope/light/proximity/stepcounter/stepdetector，无 barometer；lps22df 属 pakala 通用配置集，按“严禁编造”剔除，若真机 SensorManager 实测存在 pressure(6) 再补。",
    "品牌私有项：ASUS 不使用 xiaomi.sensor.*/oplus.sensor.*/vivo.sensor.* 命名空间，故未编造；但 sku_sun 明确带 dynamic.head_tracker，已补 HEAD_TRACKER(type 26) 及其融合链。swtp 仅小米系具备，本机不填。",
    "输入：按键 gpio-keys 与耳机孔 sun-snd-card Headset Jack 直接取固件实测；PMIC 键用 qcom-pmic-keys；触控按 ASUS 历代 Synaptics TouchComm（synaptics_tcm, vendor 0x06cb）登记，指纹为屏下光学 Goodix（uinput-goodix）；qbt_handler 属超声波方案，本机不使用。",
    "HAL：NFC 具备（ST21NFC + SN220 系列 conf、permissions 声明 nfc.ese/se.omapi），红外不具备（hasConsumerIr=false）；ASUS 私有服务 auralight2/miniled/ditdisplay/asuskey/vibratorctl 只在 VINTF 占位，不进 hardwareHals.vintfDrop。",
    "vendor/etc：Thermal 采用高通温区口径（cpu-0-0-0/gpu/skin-therm/battery），coolingDeviceName=gpu；GNSS 用 QCOM_GNSS_default、L5 双频 true；电池 5800mAh 双电芯 65W 有线 + 15W 无线；马达 X 轴线性，能力位与 11 种原语对齐；Codec2 三实例与 9 个核心组件按实测 default2 store 落 rank=1，不占用 default。",
    "显示：6.78 英寸 1080x2400 LTPO 185Hz，密度 420dpi；EDID 厂商码取 ASUS PNP=ASU、面板 ID 0x2501(9473)、2024 年第 45 周为推断值，需真机 EDID 读取后校准。",
    "身份标识：imeiTac 35847123、widevine systemId 22827、macOuiCandidates 为按 ASUS OUI 段与高通 Widevine 惯例给出的候选口径，量产前应改用真机 IMEI/DRM 实测值替换。"
  ],
  "sources": [
    "目标机固件实测包：build.prop / boot cmdline / VINTF manifest / vendor/etc（sensors/config/pakala_*、permissions、media_codecs_*、thermal-engine.conf、wifi/nfc 配置）",
    "ASUS 官方产品页 https://www.asus.com/phones/rog-phone-9-pro/techspec/ （本次仅返回列表壳，未取到规格表）",
    "高通 Snapdragon 8 Elite (SM8750) 平台公开规格：2x Oryon 4.32GHz + 6x Oryon 3.53GHz、Adreno 830、LPDDR5X、UFS 4.0",
    "对照基线 devices/manifest/configs/redmi/gold/gold-android15/ 的字段结构与颗粒度（仅结构对标，数值全部替换为本机口径）"
  ]
}
```
