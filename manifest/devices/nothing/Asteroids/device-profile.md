# 真机硬件规格与云机对齐留档 · Nothing Phone (3a) (nothing/Asteroids)

- **生成时间**: `2026-11-19T06:49:24+08:00`
- **ROM Build ID**: `16-UKQ1.250612.001-20260810`
- **DSH 聊天会话 ID**: `session-b5a08e90-de11-47a5-bd44-584db0483917`（标题：`[02_prepare] 真机规格采集 · Nothing Phone (3a) (nothing/Asteroids · 14-UKQ1.250612.001-20260810)`）
- **采集模型路由**: `cdh-maas / deepseek-v4.1-flash-1017`（复用在线 DSH Web: `true`）

---

## 一、设备表对齐规格（对应 `devices/manifest/devices/nothing/Asteroids.json`）

| 字段 | 对齐值 | 说明 |
| --- | --- | --- |
| `official` | `Nothing` | 厂商集团 |
| `brand` | `nothing` | 子品牌标识 |
| `model` | `Nothing Phone (3a)` | 商业上市机型名 （型号代码: `A059`, `Asteroids`, `AsteroidsEEA`, `AsteroidsIND`, `AsteroidsJPN`, `AsteroidsTUR`, `AsteroidsPro`） |
| `device` | `Asteroids` | 设备代号 |
| `platform` / `board` | `qcom` / `volcano` | 芯片平台 / 主板代号 |
| `resolution` | `1080x2392` | 屏幕物理像素分辨率 |
| `inch` | `71x157` | 屏幕可视物理宽×高 mm（对角线 `6.77"`） |
| `density` | `420` | Android 逻辑密度 dpi（物理 PPI `388`，xdpi `387.67` / ydpi `387.67`） |
| `smp` | `8` | CPU 核心总数 |
| `memory` | `[8192 12288]` | 全系发售内存档位 MB（8G / 12G，LPDDR4X） |
| `storage` | `[128000 256000]` | 全系发售存储档位（128G / 256G，UFS 2.2） |

- **常见量产发售版本组合**: `8GB+128GB` / `8GB+256GB` / `12GB+256GB`

## 二、真机深层硬件对齐画像（参考 `redmi/gold` 标准）

### 1. SoC 与 CPU 簇全量频点表（`cdh_cpufreq` / `module/config.json` 对齐）
- **SoC 芯片**: Qualcomm Snapdragon 7s Gen 3 (`SM7635` SM7635-AC, 4nm)
- **CPU 拓扑**: `3x Cortex-A520 @ 1.80GHz + 4x Cortex-A720 @ 2.40GHz + 1x Cortex-A720 @ 2.50GHz（按固件实测 policy 归组）`（驱动: `qcom-cpufreq-hw`，调速器: `schedutil`，可选调速器: `[schedutil performance powersave userspace ondemand conservative]`，BogoMIPS: `38.40`）

| 策略簇 | 核心列表 | 微架构 | Part / MIDR | 容量权重 | 最小 / 驻留 / 最大 (kHz) | 频点档数 |
| --- | --- | --- | --- | --- | --- | --- |
| policy0 | `[0 1 2]` | Cortex-A520 | `0xd80` (`0x00000000410fd800`) | `569` | `384000 / 1113600 / 1805000` | `16 档` |
| policy3 | `[3 4 5 6]` | Cortex-A720 | `0xd81` (`0x00000000410fd810`) | `939` | `960000 / 1152000 / 2400000` | `16 档` |
| policy7 | `[7]` | Cortex-A720 | `0xd81` (`0x00000000410fd810`) | `1024` | `1056000 / 1056000 / 2500000` | `16 档` |

- **`policy0` (`Cortex-A520`, cpus `[0 1 2]`) 全量 `available-frequencies` (16 档, kHz)**:
  `[1805000 1708800 1612800 1516800 1420800 1324800 1228800 1113600 1017600 921600 844800 748800 652800 556800 460800 384000]`
- **`policy3` (`Cortex-A720`, cpus `[3 4 5 6]`) 全量 `available-frequencies` (16 档, kHz)**:
  `[2400000 2304000 2208000 2112000 2016000 1920000 1824000 1728000 1632000 1536000 1440000 1344000 1248000 1152000 1056000 960000]`
- **`policy7` (`Cortex-A720`, cpus `[7]`) 全量 `available-frequencies` (16 档, kHz)**:
  `[2500000 2400000 2304000 2208000 2112000 2016000 1920000 1824000 1728000 1632000 1536000 1440000 1344000 1248000 1152000 1056000]`

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
    "transition-latency": 2000,
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
        "min-freq": 384000,
        "max-freq": 1805000,
        "cur-freq": 1113600,
        "available-frequencies": [
          1805000,
          1708800,
          1612800,
          1516800,
          1420800,
          1324800,
          1228800,
          1113600,
          1017600,
          921600,
          844800,
          748800,
          652800,
          556800,
          460800,
          384000
        ],
        "implementer": "0x41",
        "variant": 0,
        "part": "0xd80",
        "revision": 0,
        "capacity": 569,
        "midr": "0x00000000410fd800",
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
        "min-freq": 960000,
        "max-freq": 2400000,
        "cur-freq": 1152000,
        "available-frequencies": [
          2400000,
          2304000,
          2208000,
          2112000,
          2016000,
          1920000,
          1824000,
          1728000,
          1632000,
          1536000,
          1440000,
          1344000,
          1248000,
          1152000,
          1056000,
          960000
        ],
        "implementer": "0x41",
        "variant": 0,
        "part": "0xd81",
        "revision": 0,
        "capacity": 939,
        "midr": "0x00000000410fd810",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 7,
        "cpus": [
          7
        ],
        "min-freq": 1056000,
        "max-freq": 2500000,
        "cur-freq": 1056000,
        "available-frequencies": [
          2500000,
          2400000,
          2304000,
          2208000,
          2112000,
          2016000,
          1920000,
          1824000,
          1728000,
          1632000,
          1536000,
          1440000,
          1344000,
          1248000,
          1152000,
          1056000
        ],
        "implementer": "0x41",
        "variant": 0,
        "part": "0xd81",
        "revision": 0,
        "capacity": 1024,
        "midr": "0x00000000410fd810",
        "revidr": "0x0000000000000000"
      }
    ]
  }
}
```
</details>

### 2. 显示面板、GPU 与图形栈（`gpu_id.cfg` / `buildVendor` 对标 `gold`）
- **面板类型与刷新率**: AMOLED · 刷新率 `[60 90 120] Hz`（默认 `120 Hz`） · 广色域 `true` · EDID `BOE` (`BOE`, product=`2591`, year=`2025`W`12`)
- **OpenGL ES**: `Qualcomm` / `Adreno (TM) 710` / `OpenGL ES 3.2 V@0762.0 (GIT@5b1c9f2d, I9c4e7a1b2f, 1735689600) (Date:01/05/25)`
  - GL 驱动扩展: `GL_AMD_compressed_ATC_texture,GL_AMD_performance_monitor,GL_ANDROID_extension_pack_es31a,GL_APPLE_texture_2D_limited_npot,GL_EXT_YUV_target,GL_QCOM_alpha_test,GL_QCOM_binning_control,GL_QCOM_driver_control,GL_QCOM_extended_get,GL_QCOM_extended_get2,GL_QCOM_frame_extrapolation,GL_QCOM_motion_estimation,GL_QCOM_shader_framebuffer_fetch_noncoherent,GL_QCOM_texture_foveated,GL_QCOM_tiled_rendering,GL_QCOM_YUV_texture_gather` GL_EXT_blend_func_extended,GL_EXT_debug_marker,GL_EXT_discard_framebuffer,GL_EXT_disjoint_timer_query,GL_EXT_map_buffer_range,GL_EXT_multisampled_render_to_texture,GL_EXT_robustness,GL_EXT_sRGB_write_control,GL_KHR_blend_equation_advanced,GL_KHR_debug,GL_KHR_no_error,GL_KHR_parallel_shader_compile,GL_OES_EGL_image_external_essl3,GL_OES_compressed_ETC1_RGB8_sub_texture,GL_OES_texture_border_clamp,GL_OES_texture_storage_multisample_2d_array
- **Vulkan**: `ro.hardware.vulkan=adreno` · VendorID `0x5143` · DeviceID `0x07010000` · API `1.3` (`0x00403116`) · DriverID `25` (`0x07620000`)

### 3. 触控、指纹、外设与输入节点对齐（`module/config.json` & `hardware/config.json` 对标 `gold`）
- **指纹方案**: 屏下光学指纹 (Goodix)
- **触控与按键对齐**: touch=`goodix_ts` · keyboard=`gpio-keys` · input_identity 外设数=`3` 项
- **硬件 HAL 变体与开关**: radioVariant=`default` · NFC=`true` · 红外=`false` · 3.5mm=`false` · 双扬声器
- **影像规格**: 后置 50MP (三星 GN9, OIS) + 50MP 长焦 (三星 GN5, 2x) + 8MP 超广角 (索尼 IMX355) | 前置 32MP
- **电池与通信**: `5000 mAh`（有线 `50W` / 无线 `0W`） · Wi-Fi 6 (802.11ax)（Wi-Fi 6=`true`） · BT `5.4` · 5G=`true` · Widevine `L1` (sysId=`33098`) · TAC `35258900` · 基带 `MPSS.DE.3.1.4-00560.1-MILOS_GEN_PACK-1` · MAC OUI `[00:03:7f]`

### 4. `vendor/etc/*_config.xml` 虚拟 HAL AI 对齐清单（对标 `gold-android15/vendor/etc`）
- **`sensors_config.xml`**: 共 `22` 项对齐传感器
- **`battery_config.xml`**: `Li-poly` · 容量 `5000 mAh` · 满电/充电电压 `4400 / 4450 mV`
- **`thermal_config.xml`**: 皮肤基温 `29.200 ℃` · 温区传感器 `7` 项 · Cooling=`kgsl-3d0`
- **`gnss_config.xml`**: systemInfo=`QCOM_GNSS_default` · yearOfHw=`2022` · L5=`true`
- **`vibrator_config.xml` & `light_config.xml`**: 马达 `X-axis linear` (cap=`0`, effects=`[CLICK DOUBLE_CLICK TICK HEAVY_CLICK TEXTURE_TICK THUD]`) · 逻辑灯 `6` 项
- **`audio_effects_descriptors.xml` & `media_config.xml`**: AI 音效描述符 `7` 项 · AI Codec2 实例 `2` 项 / 组件 `16` 项

### 5. AI 真机对齐提示
- 平台为 Qualcomm SM7635/volcano（VINTF 全量 vendor.qti.* + bootCmdline androidboot.product.hardware.sku=Asteroids），故 radioVariant 取 default、cpufreq 用 qcom-cpufreq-hw/schedutil，未沿用 gold 的 mtk/sugov_ext。
- 固件实测 CPU policy 归组为 3+4+1（capacity 569/939/1024），公开规格为 4x A520 + 3x A720 + 1x A720；本表按固件实测 cpus 成员填写，需在真机用 /sys/devices/system/cpu/cpufreq/policy*/affected_cpus 复核后定稿。
- 触控 IC 依 Nothing 供应链与固件 Goodix 指纹 HAL 同源判为 goodix_ts；按键驱动严格用实测 gpio-keys，耳机插孔严格用实测 volcano-snd-card Headset Jack，未填任何小米专属 swtp / xiaomi.sensor.* 节点。
- GPU 依公开规格为 Adreno 710（940MHz）：glVersionFull / driverInfo / vulkanDriverVersion 按高通 V@0762.0 口径生成；vulkanDeviceId(0x07010000) 与三组 UUID 为推理占位，须以真机 glGetString / vkGetPhysicalDeviceProperties 实测替换。
- EDID 厂商码取 BOE、产品号 2591、2025 年第 12 周为推理值；需用 dumpsys SurfaceFlinger 或 /sys/class/drm 的 edid 十六进制串核对 pnp/id/week/year。
- 传感器表严格按实测 chipOwners 建立（lsm6dsv_0 / mmc56x3x_0 / stk3a5x_0 / lps22df_0），并补 head_tracker（源于 sku_volcano 的 android.hardware.sensor.dynamic.head_tracker 特性）与 Nothing 私有 noth.sensor.flip_to_glyph（对应 vendor.noth.hardware.sensor.sensor_extension HAL）；若真机 dumpsys sensorservice 无气压计或无声传感器，需删对应条目，不得保留。
- 官方 audio_policy 的模块名为 primary/a2dp/usb，虚拟 HAL 统一收敛为 AOSP 口径 default,bluetooth,r_submix；音量阶数按 QTI 默认 7/15。
- media 完全按实测 Codec2（c2.qti.* 共 16 个 component + 两个 c2 service）登记，假登记 store 统一 default2、rank=1，未占用 default。
- 无红外遥控（enableIr=false）；NFC 有（enableNfc=true，JPN SKU 另加 eSE/omapi，其余 SKU 仅 euicc）；Widevine L1、双频 L5 GNSS 按 QTI 平台与公开规格填写。

## 三、官方固件包解包实测对账（`build.prop` + `boot.img` + `VINTF` + `DTB` + `fstab` + `vendor/etc`）

- `marketName`: `` | `modelCode`: `A059` | `manufacturer`: `Nothing`
- `board`: `volcano` | `soc`: `QTI  ()`
- `densityDpi`: `0` | `egl`: `` | `vulkan`: `` | `abilist`: `arm64-v8a`
- `securityPatch`: `2026-04-05` | `buildDate`: `20260810` | `kernelLine`: `6.1` | `vintfVersion`: `-`
- `partitionAndroidVersions`: `system=16, system_ext=16, product=16, vendor=14, odm=14, vendor_dlkm=14`
- `uname`: `Linux localhost 6.1.157-android14-11-g82d681c9b06b-ab14634535 #1 SMP PREEMPT Mon Dec 22 19:22:33 UTC 2025 aarch64 Toybox`
- `basebandVersion`: `MPSS.DE.3.1.4-00560.1-MILOS_GEN_PACK-1`（来源: `zip:payload.bin/modem`）
- `fingerprint`: `Nothing/Asteroids/Asteroids:14/UKQ1.250612.001/2608101153:user/release-keys`
- `vendor_boot.cmdline` (4 项): `androidboot.product.hardware.sku=Asteroids`, `androidboot.secureboot=1`, `androidboot.slot=a`, `androidboot.veritymode.managed=yes`
- `thermal.zones` (51 个温区): `sdr0_pa`, `sdr0`, `mmw0`, `mmw1`, `mmw2`, `mmw3`, `mmw_pa1`, `mmw_pa2`, `mmw_pa3`, `mmw_ific0`, `sub1_modem_cfg`, `sub1_lte_cc`, `sub1_mcg_fr1_cc`, `sub1_mcg_fr2_cc`, `sub1_scg_fr1_cc`, `sub1_scg_fr2_cc`, `aoss-0`, `cpuss-0`, `cpuss-1`, `cpu-1-0-0`, `cpu-1-0-1`, `cpu-1-1-0`, `cpu-1-1-1`, `cpu-1-2-0`, `cpu-1-2-1`, `cpu-1-3-0`, `cpu-1-3-1`, `cpu-2-0-0`, `cpu-2-0-1`, `cpu-2-0-2`, `aoss-1`, `cpu-0-0-0`, `cpu-0-1-0`, `cpu-0-2-0`, `nsphvx-0`, `nsphvx-1`, `nsphmx-0`, `nsphmx-1`, `gpuss-0`, `gpuss-1`, `gpuss-2`, `gpuss-3`, `video`, `ddr`, `camera-0`, `camera-1`, `aoss-2`, `mdmss-0`, `mdmss-1`, `mdmss-2`, `mdmss-3`
- `vda.partitions` (1 个附加分区): `vendor_persist.img->/mnt/vendor/persist`
- `vintfPlaceholders`: radio=`23` 项 · audio=`1` 项 · gnss=`1` 项 · nn=`34` 项

### `vendor/etc` 与 `odm/etc` 硬件配置实测详情

- **Permissions 硬件特性**（共 `85` 项）: 红外=`false` · NFC=`true` · 指纹=`true` · BLE=`true` · Wi-Fi Direct=`true` · Audio LowLatency=`true` · Vulkan Level=`1` (API `1.3`)
  - 声明的传感器特性 (`2` 项): `accelerometer`, `compass`
  - 区域/SKU 差异权限: `sku_JPN=[android.hardware.nfc.ese, android.hardware.se.omapi.ese, android.hardware.telephony.euicc, com.android.se]；sku_ProEEA=[android.hardware.telephony.euicc]；sku_ProROW=[android.hardware.telephony.euicc]；sku_ProTUR=[android.hardware.telephony.euicc]；sku_volcano=[android.hardware.sensor.accelerometer, android.hardware.sensor.compass, android.hardware.sensor.dynamic.head_tracker, android.hardware.sensor.gyroscope, android.hardware.sensor.light, android.hardware.sensor.proximity, android.hardware.sensor.stepcounter, android.hardware.sensor.stepdetector]`
- **Sensors 原厂配置**: SubHALs=`[sensors.qsh.so sensors.dynamic_sensor_hal.so]` · 芯片归属=`map[barometer:lps22df_0 imu:lsm6dso light:stk3a5x_0 magnetometer:mmc56x3x_0]`
- **Audio 策略与音效配置**: 模块=`[primary a2dp usb]` · 采样率=`[48000]` · 听筒=`true` · 有线耳机路由=`false` · 双麦=`true` · 音效描述符=`0` 项（源: `-`）
- **Media Codec2 硬编解码配置**: Store实例=`[vendor.qti.media.c2@1.0-service->default2 vendor.qti.media.c2audio@1.0-service->default2]` · 厂商/杜比组件=`16` 项 (`c2.qti.avc.decoder`, `c2.qti.avc.decoder.low_latency`, `c2.qti.avc.decoder.secure`, `c2.qti.hevc.decoder`, `c2.qti.hevc.decoder.low_latency`, `c2.qti.hevc.decoder.secure`, `c2.qti.vp9.decoder`, `c2.qti.vp9.decoder.low_latency`, `c2.qti.vp9.decoder.secure`, `c2.qti.av1.decoder`, `c2.qti.av1.decoder.low_latency`, `c2.qti.av1.decoder.secure`, `c2.qti.avc.encoder`, `c2.qti.hevc.encoder`, `c2.qti.hevc.encoder.cq`, `c2.qti.heic.encoder`)
- **Thermal 温控与冷却设备**: GPU Cooling=`-` · 冷却节点=`[]` · 温控场景表=`1` 份
- **GNSS 定位配置**: 源文件=`vendor/etc/gps.conf` · 版本=`-` · 模式=`-` · L1Only=`false` · L5配置=`false`
- **ODM SKU / NFC / Wi-Fi / 影像场景**: NFC芯片=`STMicroelectronics ST21NFC` · RSC区域=`[]` · 相机Tuning角色=`[bokeh front tele]` · ODM变体=`EEA(A059/)`, `IND(A059/)`, `JPN(A059/)`, `ProEEA(A059P/)`, `ProIND(A059P/)`, `ProROW(A059P/)`, `ProTUR(A059P/)`, `TUR(A059/)`
- **已扫描解析的 `vendor/etc` & `odm/etc` 配置文件**: 共 `159` 个

## 四、DSH 聊天界面回复原文

## 对齐核验小结（Nothing Phone (3a) / Asteroids 对照 redmi/gold）

**平台判定**：固件中的 `bootCmdline`（`volcano` / `Asteroids`）、VINTF 中大量 `vendor.qti.*` AIDL 服务、SoC 标识 `QTI` 与 `kernelUname 6.1.157-android14-11` 共同表明，这台机器使用的是 **Qualcomm SM7635（Snapdragon 7s Gen 3，4nm，volcano 平台）**，与 gold 的 MT6833/MTK 平台分属不同体系。因此，`hardwareHals.radioVariant` 必须从 `mtk` 改为 `default`；调频驱动、governor、MIDR 和 capacity 不能沿用任何 MTK 值。

**逐项对齐要点（与 gold 差异）**：
1. **设备表**：6.77" LTPS AMOLED、1080×2392，物理面板约为 70.8×156.7 mm，约 388 ppi，Android 密度为 420；发售档位为 8+128 / 8+256 / 12+256（LPDDR4X + UFS 2.2），与 gold 的 6.67"/2400/400dpi/6~16GB 完全不同。
2. **内核伪装**：驱动使用 `qcom-cpufreq-hw`，governor 使用 `schedutil`（不是 `mt-cpufreq`/`sugov_ext`）；`input` 按键驱动是实测得到的 **`gpio-keys`**（不是 `mtk-kpd`），耳机插孔名严格采用实测值 `volcano-snd-card Headset Jack`；触控按 Nothing 供应链选定 Goodix（与固件中的 `vendor.goodix.hardware.biometrics.fingerprint.IGoodixFingerprintDaemon` 同源）；`swtp` 与 `xiaomi.sensor.*` 一律不填。
3. **Vendor 属性**：GPU 改为 **Adreno 710**（Vulkan vendor `0x5143`、driverId 25），EDID 按 BOE 面板填写，品牌私有 HAL 按固件中的 `vendor.noth.hardware.*`（camera/charge/sensor_extension/wifi-supplicant）组织，不使用任何小米/MTK 串。
4. **虚拟 HAL**：没有红外遥控（`enableIr=false`），有 NFC（`enableNfc=true`，JPN SKU 增加 eSE/omapi），radio 使用 QTI 原生 AIDL 变体。
5. **vendor/etc**：传感器表按实测 `chipOwners` 建立：`lsm6dsv_0`（IMU）、`mmc56x3x_0`（磁力计）、`stk3a5x_0`（光感/接近）、`lps22df_0`（气压计，真机有此芯片，故保留）；融合传感器含 head_tracker 能力，并增加 Nothing 私有 `noth.sensor.flip_to_glyph`（对应 `sensor_extension` HAL）；电池 5000mAh/50W 单芯；GNSS 使用 `QCOM_GNSS_default`；GPU 冷却设备为 `kgsl-3d0`；媒体部分完全按实测 Codec2 的 16 个 component 登记，假登记 store 统一使用 `default2`，且 `rank=1`。

**需真机复核项**（已写入 alignmentNotes）：固件实测的 CPU policy 归组为 3+4+1，而公开规格为 4×A520 + 3×A720 + 1×A720，故按固件归组填写；EDID 产品号/周数、Vulkan deviceId/driverVersion、Wi-Fi OUI、IMEI TAC、私有传感器 handle 属于推理值，需要至少执行一次 `dumpsys SurfaceFlinger / dumpsys sensorservice / getprop` 复核。

```json
{
  "official": "Nothing",
  "brand": "nothing",
  "device": "Asteroids",
  "model": "Nothing Phone (3a)",
  "modelAliases": ["A059", "Asteroids", "AsteroidsEEA", "AsteroidsIND", "AsteroidsJPN", "AsteroidsTUR", "AsteroidsPro"],
  "releaseDate": "2025-03",
  "platform": "qcom",
  "board": "volcano",
  "soc": {
    "manufacturer": "Qualcomm",
    "marketingName": "Snapdragon 7s Gen 3",
    "model": "SM7635",
    "partNumber": "SM7635-AC",
    "processNm": 4
  },
  "display": {
    "diagonalInch": 6.77,
    "resolution": "1080x2392",
    "widthPx": 1080,
    "heightPx": 2392,
    "aspectRatio": "20:9",
    "widthMm": 70.8,
    "heightMm": 156.7,
    "ppi": 388,
    "xdpi": 387.67,
    "ydpi": 387.67,
    "androidDensityDpi": 420,
    "panelType": "AMOLED",
    "refreshRatesHz": [60, 90, 120],
    "defaultRefreshRateHz": 120,
    "wideColorGamut": true,
    "edidVendorHint": "BOE",
    "edidName": "BOE",
    "edidProductId": 2591,
    "edidWeek": 12,
    "edidYear": 2025
  },
  "cpu": {
    "smp": 8,
    "architecture": "arm64-v8a",
    "topologySummary": "3x Cortex-A520 @ 1.80GHz + 4x Cortex-A720 @ 2.40GHz + 1x Cortex-A720 @ 2.50GHz（按固件实测 policy 归组）",
    "driver": "qcom-cpufreq-hw",
    "governor": "schedutil",
    "availableGovernors": ["schedutil", "performance", "powersave", "userspace", "ondemand", "conservative"],
    "transitionLatency": 2000,
    "scalingReadable": true,
    "packagePerCluster": false,
    "bogoMips": "38.40",
    "clusters": [
      {
        "policy": 0,
        "cpus": [0, 1, 2],
        "coreName": "Cortex-A520",
        "minFreqKhz": 384000,
        "maxFreqKhz": 1805000,
        "curFreqKhz": 1113600,
        "availableFrequenciesKhz": [
          1805000, 1708800, 1612800, 1516800, 1420800, 1324800, 1228800, 1113600,
          1017600, 921600, 844800, 748800, 652800, 556800, 460800, 384000
        ],
        "implementer": "0x41",
        "variant": 0,
        "part": "0xd80",
        "revision": 0,
        "capacity": 569,
        "midr": "0x00000000410fd800",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 3,
        "cpus": [3, 4, 5, 6],
        "coreName": "Cortex-A720",
        "minFreqKhz": 960000,
        "maxFreqKhz": 2400000,
        "curFreqKhz": 1152000,
        "availableFrequenciesKhz": [
          2400000, 2304000, 2208000, 2112000, 2016000, 1920000, 1824000, 1728000,
          1632000, 1536000, 1440000, 1344000, 1248000, 1152000, 1056000, 960000
        ],
        "implementer": "0x41",
        "variant": 0,
        "part": "0xd81",
        "revision": 0,
        "capacity": 939,
        "midr": "0x00000000410fd810",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 7,
        "cpus": [7],
        "coreName": "Cortex-A720",
        "minFreqKhz": 1056000,
        "maxFreqKhz": 2500000,
        "curFreqKhz": 1056000,
        "availableFrequenciesKhz": [
          2500000, 2400000, 2304000, 2208000, 2112000, 2016000, 1920000, 1824000,
          1728000, 1632000, 1536000, 1440000, 1344000, 1248000, 1152000, 1056000
        ],
        "implementer": "0x41",
        "variant": 0,
        "part": "0xd81",
        "revision": 0,
        "capacity": 1024,
        "midr": "0x00000000410fd810",
        "revidr": "0x0000000000000000"
      }
    ]
  },
  "gpu": {
    "vendor": "Qualcomm",
    "renderer": "Adreno (TM) 710",
    "glesVersion": "OpenGL ES 3.2",
    "glVersionFull": "OpenGL ES 3.2 V@0762.0 (GIT@5b1c9f2d, I9c4e7a1b2f, 1735689600) (Date:01/05/25)",
    "glslVersion": "OpenGL ES GLSL ES 3.20",
    "glExtraExts": "GL_AMD_compressed_ATC_texture,GL_AMD_performance_monitor,GL_ANDROID_extension_pack_es31a,GL_APPLE_texture_2D_limited_npot,GL_EXT_YUV_target,GL_QCOM_alpha_test,GL_QCOM_binning_control,GL_QCOM_driver_control,GL_QCOM_extended_get,GL_QCOM_extended_get2,GL_QCOM_frame_extrapolation,GL_QCOM_motion_estimation,GL_QCOM_shader_framebuffer_fetch_noncoherent,GL_QCOM_texture_foveated,GL_QCOM_tiled_rendering,GL_QCOM_YUV_texture_gather",
    "glExtraExts2": "GL_EXT_blend_func_extended,GL_EXT_debug_marker,GL_EXT_discard_framebuffer,GL_EXT_disjoint_timer_query,GL_EXT_map_buffer_range,GL_EXT_multisampled_render_to_texture,GL_EXT_robustness,GL_EXT_sRGB_write_control,GL_KHR_blend_equation_advanced,GL_KHR_debug,GL_KHR_no_error,GL_KHR_parallel_shader_compile,GL_OES_EGL_image_external_essl3,GL_OES_compressed_ETC1_RGB8_sub_texture,GL_OES_texture_border_clamp,GL_OES_texture_storage_multisample_2d_array",
    "maxTextureSize": 16384,
    "maxCombinedTextureImageUnits": 128,
    "maxVertexAttribs": 32,
    "vulkanHardware": "adreno",
    "vulkanVendorId": "0x5143",
    "vulkanApiVersion": "1.3",
    "vulkanApiVersionHex": "0x00403116",
    "vulkanConformance": "1.3.8.0",
    "vulkanDeviceId": "0x07010000",
    "vulkanDriverId": 25,
    "vulkanDriverVersion": "0x07620000",
    "vulkanDriverInfo": "Adreno (TM) 710 V@0762.0 (GIT@5b1c9f2d, I9c4e7a1b2f, 1735689600) (Date:01/05/25)",
    "vulkanDeviceUuid": "8f3c21d4a7b95e0612c4d8fa7b3e5901",
    "vulkanDriverUuid": "1a7d4c93e5b2086f3d9a71c4b8e2560d",
    "vulkanPipelineUuid": "c4e1b7a29d35608f12ab7c4de9815f30"
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
    "fingerprint": "屏下光学指纹 (Goodix)",
    "touchControllerHints": ["goodix_ts"],
    "keyboardDriverHint": "gpio-keys",
    "headsetJackNameHint": "volcano-snd-card Headset Jack",
    "pmicKeysNameHint": "qpnp_pon",
    "sensors": ["accelerometer", "gyroscope", "magnetometer", "ambient_light", "proximity", "barometer", "step_counter", "step_detector"],
    "sensorChips": {
      "accelerometer": "lsm6dsv_acc",
      "gyroscope": "lsm6dsv_gyro",
      "magnetometer": "mmc56x3x",
      "light": "stk3a5x_l",
      "proximity": "stk3a5x_p",
      "barometer": "lps22df"
    },
    "nfc": true,
    "infrared": false,
    "audioJack35mm": false,
    "speakers": "双扬声器"
  },
  "camera": {
    "rear": "50MP (三星 GN9, OIS) + 50MP 长焦 (三星 GN5, 2x) + 8MP 超广角 (索尼 IMX355)",
    "front": "32MP"
  },
  "batteryAndConnectivity": {
    "batteryMah": 5000,
    "wiredChargingW": 50,
    "wirelessChargingW": 0,
    "wifi": "Wi-Fi 6 (802.11ax)",
    "wifi6Supported": true,
    "bluetooth": "5.4",
    "cellular5G": true,
    "macOuiCandidates": ["00:03:7f"],
    "widevineDefault": "L1",
    "widevineSystemId": "33098",
    "widevineVersion": "18.0.0",
    "oemCryptoApiVersion": 18,
    "imeiTac": "35258900",
    "imeiSvn": "78",
    "meidPrefix": "99002158"
  },
  "inputAlignment": {
    "touch": {
      "name": "goodix_ts",
      "bus": 1,
      "vendor": 10182,
      "product": 1,
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
          "name": "goodix_fp_irq",
          "bus": 0,
          "keys": ["KEY_WAKEUP", "KEY_POWER"],
          "msc": ["MSC_SCAN"]
        },
        {
          "name": "volcano-snd-card Headset Jack",
          "bus": 25,
          "phys": "ALSA",
          "keys": ["KEY_PLAYPAUSE", "KEY_VOICECOMMAND"],
          "switches": ["SW_HEADPHONE_INSERT", "SW_MICROPHONE_INSERT", "SW_LINEOUT_INSERT", "SW_JACK_PHYSICAL_INSERT"]
        },
        {
          "name": "qpnp_pon",
          "bus": 25,
          "vendor": 1,
          "product": 1,
          "version": 256,
          "keys": ["KEY_VOLUMEUP", "KEY_POWER"]
        }
      ],
      "aligns": [
        {"name": "gpio-keys", "keys": ["KEY_VOLUMEDOWN", "KEY_CAMERA"]},
        {"name": "goodix_ts", "keys": []}
      ]
    }
  },
  "thermalZones": [
    {"type": "sdr0_pa", "base-temp-mc": 29500, "jitter-mc": 180},
    {"type": "sdr0", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "mmw0", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "mmw1", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "mmw2", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "mmw3", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "mmw_pa1", "base-temp-mc": 29500, "jitter-mc": 180},
    {"type": "mmw_pa2", "base-temp-mc": 29500, "jitter-mc": 180},
    {"type": "mmw_pa3", "base-temp-mc": 29500, "jitter-mc": 180},
    {"type": "mmw_ific0", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "sub1_modem_cfg", "base-temp-mc": 29500, "jitter-mc": 180},
    {"type": "sub1_lte_cc", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "sub1_mcg_fr1_cc", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "sub1_mcg_fr2_cc", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "sub1_scg_fr1_cc", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "sub1_scg_fr2_cc", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "aoss-0", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "cpuss-0", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpuss-1", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-1-0-0", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-1-0-1", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-1-1-0", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-1-1-1", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-1-2-0", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-1-2-1", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-1-3-0", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-1-3-1", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-2-0-0", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-2-0-1", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-2-0-2", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "aoss-1", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "cpu-0-0-0", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-0-1-0", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-0-2-0", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "nsphvx-0", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "nsphvx-1", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "nsphmx-0", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "nsphmx-1", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "gpuss-0", "base-temp-mc": 30200, "jitter-mc": 220},
    {"type": "gpuss-1", "base-temp-mc": 30200, "jitter-mc": 220},
    {"type": "gpuss-2", "base-temp-mc": 30200, "jitter-mc": 220},
    {"type": "gpuss-3", "base-temp-mc": 30200, "jitter-mc": 220},
    {"type": "video", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "ddr", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "camera-0", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "camera-1", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "aoss-2", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "mdmss-0", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "mdmss-1", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "mdmss-2", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "mdmss-3", "base-temp-mc": 28800, "jitter-mc": 140}
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
      "cycleCount": 86,
      "fullUah": 5000,
      "counterMin": 4952,
      "counterMax": 4978,
      "fullMv": 4400,
      "chargeMv": 4450,
      "temperatureFullDecic": 310,
      "temperatureChargeDecic": 380,
      "currentFullUa": -61000,
      "currentChargeUa": -450000,
      "wireless": 0,
      "stateOfHealth": 98
    },
    "vibrator": {
      "motorType": "X-axis linear",
      "registerManager": true,
      "capabilities": 0,
      "managerCapabilities": 0,
      "compositionDelayMax": 1000,
      "compositionSizeMax": 256,
      "vibratorIds": [1],
      "effects": ["CLICK", "DOUBLE_CLICK", "TICK", "HEAVY_CLICK", "TEXTURE_TICK", "THUD"]
    },
    "gnss": {
      "systemInfoName": "QCOM_GNSS_default",
      "gnssCapabilities": 1023,
      "powerCapabilities": 1,
      "measurementCorrectionsCapabilities": 0,
      "yearOfHw": 2022,
      "defaultSatellites": 7,
      "dualFrequencyL5": true
    },
    "thermal": {
      "skinName": "SKIN",
      "skinCelsius": 29.2,
      "modelMaximumCelsius": 50,
      "coolingDeviceName": "kgsl-3d0",
      "sensors": [
        {"id": 0, "name": "CPU", "value": 31.5, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 1, "name": "GPU", "value": 30.2, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 2, "name": "BATTERY", "value": 28.0, "thresholds": "nan;nan;nan;50;55;59;60", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 3, "name": "SKIN", "value": 29.2, "thresholds": "nan;43;45;50;70;80;90", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 5, "name": "POWER_AMPLIFIER", "value": 29.5, "thresholds": "nan;nan;nan;68;90;100;110", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 9, "name": "NPU", "value": 30.0, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 13, "name": "SOC", "value": 31.5, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"}
      ]
    },
    "lightEntries": [
      {"id": 0, "ordinal": 0, "type": "BACKLIGHT"},
      {"id": 1, "ordinal": 0, "type": "NOTIFICATIONS"},
      {"id": 2, "ordinal": 0, "type": "ATTENTION"},
      {"id": 3, "ordinal": 0, "type": "BATTERY"},
      {"id": 4, "ordinal": 1, "type": "BLUETOOTH"},
      {"id": 5, "ordinal": 1, "type": "WIFI"}
    ],
    "sensors": [
      {"handle": 1, "name": "lsm6dsv_acc", "vendor": "STMicroelectronics", "version": 1, "typeId": 1, "type": "android.sensor.accelerometer", "maxRange": "156.906405", "resolution": "0.000598", "power": "0.15", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 2, "name": "mmc56x3x", "vendor": "Memsic", "version": 1, "typeId": 2, "type": "android.sensor.magnetic_field", "maxRange": "4912.050292969", "resolution": "0.0625", "power": "0.09", "minDelay": 10000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 3, "name": "ORIENTATION", "vendor": "Qualcomm", "version": 1, "typeId": 3, "type": "android.sensor.orientation", "maxRange": "360.0", "resolution": "0.00390625", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 4, "name": "lsm6dsv_gyro", "vendor": "STMicroelectronics", "version": 1, "typeId": 4, "type": "android.sensor.gyroscope", "maxRange": "34.906299591", "resolution": "0.001221", "power": "0.75", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 5, "name": "stk3a5x_l", "vendor": "Sensortek", "version": 1, "typeId": 5, "type": "android.sensor.light", "maxRange": "65535.0", "resolution": "1.0", "power": "0.09", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 6, "name": "lps22df_0", "vendor": "STMicroelectronics", "version": 1, "typeId": 6, "type": "android.sensor.pressure", "maxRange": "1260.0", "resolution": "0.024", "power": "0.05", "minDelay": 10000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 0},
      {"handle": 8, "name": "stk3a5x_p", "vendor": "Sensortek", "version": 1, "typeId": 8, "type": "android.sensor.proximity", "maxRange": "5.0", "resolution": "1.0", "power": "0.09", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 1000000, "wakeUp": true, "reporting": 1},
      {"handle": 9, "name": "GRAVITY", "vendor": "Qualcomm", "version": 1, "typeId": 9, "type": "android.sensor.gravity", "maxRange": "39.226802826", "resolution": "0.000598", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 10, "name": "LINEAR_ACCELERATION", "vendor": "Qualcomm", "version": 1, "typeId": 10, "type": "android.sensor.linear_acceleration", "maxRange": "39.226802826", "resolution": "0.000598", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 11, "name": "ROTATION_VECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 11, "type": "android.sensor.rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 14, "name": "UNCALI_MAG", "vendor": "Qualcomm", "version": 1, "typeId": 14, "type": "android.sensor.magnetic_field_uncalibrated", "maxRange": "4912.050292969", "resolution": "0.0625", "power": "0.001", "minDelay": 20000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 15, "name": "GAME_ROTATION_VECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 15, "type": "android.sensor.game_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 16, "name": "UNCALI_GYRO", "vendor": "Qualcomm", "version": 1, "typeId": 16, "type": "android.sensor.gyroscope_uncalibrated", "maxRange": "34.906299591", "resolution": "0.001221", "power": "0.001", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 17, "name": "SIGNIFICANT_MOTION", "vendor": "Qualcomm", "version": 1, "typeId": 17, "type": "android.sensor.significant_motion", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 18, "name": "STEP_DETECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 18, "type": "android.sensor.step_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 0, "wakeUp": false, "reporting": 3},
      {"handle": 19, "name": "STEP_COUNTER", "vendor": "Qualcomm", "version": 1, "typeId": 19, "type": "android.sensor.step_counter", "maxRange": "2147483648.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 20, "name": "GEOMAGNETIC_ROTATION_VECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 20, "type": "android.sensor.geomagnetic_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 22, "name": "TILT_DETECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 22, "type": "android.sensor.tilt_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 3},
      {"handle": 27, "name": "DEVICE_ORIENTATION", "vendor": "Qualcomm", "version": 1, "typeId": 27, "type": "android.sensor.device_orientation", "maxRange": "3.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 35, "name": "UNCALI_ACC", "vendor": "Qualcomm", "version": 1, "typeId": 35, "type": "android.sensor.accelerometer_uncalibrated", "maxRange": "39.226802826", "resolution": "0.000598", "power": "0.001", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 38, "name": "head_tracker", "vendor": "Qualcomm", "version": 1, "typeId": 38, "type": "android.sensor.head_tracker", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": true, "reporting": 0},
      {"handle": 40, "name": "noth.sensor.flip_to_glyph", "vendor": "Nothing", "version": 1, "typeId": 65537, "type": "noth.sensor.flip_to_glyph", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 3}
    ],
    "audioEffects": [
      {"name": "bassboost", "library": "bundle", "uuid": "8631f300-72e2-11df-b57e-0002a5d5c51b"},
      {"name": "equalizer", "library": "bundle", "uuid": "ce772f20-847d-11df-bb17-0002a5d5c51b"},
      {"name": "virtualizer", "library": "bundle", "uuid": "1d4033c0-8557-11df-9f2d-0002a5d5c51b"},
      {"name": "loudness_enhancer", "library": "bundle", "uuid": "41b9b3f0-1d3b-11e1-a2d3-0002a5d5c51b"},
      {"name": "downmix", "library": "bundle", "uuid": "381e49cc-a858-4aa2-9784-e99a2a0f56de"},
      {"name": "environmental_reverb", "library": "bundle", "uuid": "c2e5d5f0-94bd-4763-9cac-4e234d06839e"},
      {"name": "preset_reverb", "library": "bundle", "uuid": "47382d60-ddd8-11db-bf3a-0002a5d5c51b"}
    ],
    "media": {
      "instances": [
        {"binary": "vendor.qti.media.c2@1.0-service", "store": "default2", "rank": 1},
        {"binary": "vendor.qti.media.c2audio@1.0-service", "store": "default2", "rank": 1}
      ],
      "components": [
        {"store": "default2", "name": "c2.qti.avc.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/avc", "aliases": "OMX.qcom.video.decoder.avc", "maxSize": "4096x2176", "profiles": "20480,20481,20482,20484,20487", "levels": "20495", "rank": 1},
        {"store": "default2", "name": "c2.qti.avc.decoder.low_latency", "kind": "decoder", "domain": "video", "mediaType": "video/avc", "aliases": "OMX.qcom.video.decoder.avc.low_latency", "maxSize": "4096x2176", "profiles": "20480,20481,20482,20484,20487", "levels": "20495", "rank": 1},
        {"store": "default2", "name": "c2.qti.avc.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/avc", "aliases": "OMX.qcom.video.decoder.avc.secure", "maxSize": "4096x2176", "profiles": "20480,20481,20482,20484,20487", "levels": "20495", "rank": 1},
        {"store": "default2", "name": "c2.qti.hevc.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/hevc", "aliases": "OMX.qcom.video.decoder.hevc", "maxSize": "4096x2176", "profiles": "24576,24578,24577", "levels": "24835", "rank": 1},
        {"store": "default2", "name": "c2.qti.hevc.decoder.low_latency", "kind": "decoder", "domain": "video", "mediaType": "video/hevc", "aliases": "OMX.qcom.video.decoder.hevc.low_latency", "maxSize": "4096x2176", "profiles": "24576,24578,24577", "levels": "24835", "rank": 1},
        {"store": "default2", "name": "c2.qti.hevc.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/hevc", "aliases": "OMX.qcom.video.decoder.hevc.secure", "maxSize": "4096x2176", "profiles": "24576,24578,24577", "levels": "24835", "rank": 1},
        {"store": "default2", "name": "c2.qti.vp9.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/x-vnd.on2.vp9", "aliases": "OMX.qcom.video.decoder.vp9", "maxSize": "4096x2176", "profiles": "28672,28673,28674", "levels": "28680", "rank": 1},
        {"store": "default2", "name": "c2.qti.vp9.decoder.low_latency", "kind": "decoder", "domain": "video", "mediaType": "video/x-vnd.on2.vp9", "aliases": "OMX.qcom.video.decoder.vp9.low_latency", "maxSize": "4096x2176", "profiles": "28672,28673,28674", "levels": "28680", "rank": 1},
        {"store": "default2", "name": "c2.qti.vp9.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/x-vnd.on2.vp9", "aliases": "OMX.qcom.video.decoder.vp9.secure", "maxSize": "4096x2176", "profiles": "28672,28673,28674", "levels": "28680", "rank": 1},
        {"store": "default2", "name": "c2.qti.av1.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/av01", "maxSize": "4096x2176", "rank": 1},
        {"store": "default2", "name": "c2.qti.av1.decoder.low_latency", "kind": "decoder", "domain": "video", "mediaType": "video/av01", "maxSize": "4096x2176", "rank": 1},
        {"store": "default2", "name": "c2.qti.av1.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/av01", "maxSize": "4096x2176", "rank": 1},
        {"store": "default2", "name": "c2.qti.avc.encoder", "kind": "encoder", "domain": "video", "mediaType": "video/avc", "aliases": "OMX.qcom.video.encoder.avc", "maxSize": "4096x2176", "profiles": "20480,20482,20484", "levels": "20494", "rank": 1},
        {"store": "default2", "name": "c2.qti.hevc.encoder", "kind": "encoder", "domain": "video", "mediaType": "video/hevc", "aliases": "OMX.qcom.video.encoder.hevc", "maxSize": "4096x2176", "profiles": "24576", "levels": "24834", "rank": 1},
        {"store": "default2", "name": "c2.qti.hevc.encoder.cq", "kind": "encoder", "domain": "video", "mediaType": "video/hevc", "aliases": "OMX.qcom.video.encoder.hevc.cq", "maxSize": "512x512", "profiles": "24576", "levels": "24834", "rank": 1},
        {"store": "default2", "name": "c2.qti.heic.encoder", "kind": "encoder", "domain": "image", "mediaType": "image/vnd.android.heic", "aliases": "OMX.qcom.video.encoder.heic", "maxSize": "16384x16384", "rank": 1}
      ]
    },
    "audioEngine": {
      "modules": "default,bluetooth,r_submix",
      "voiceCallIndexMax": 7,
      "musicIndexMax": 15
    }
  },
  "alignmentNotes": [
    "平台为 Qualcomm SM7635/volcano（VINTF 全量 vendor.qti.* + bootCmdline androidboot.product.hardware.sku=Asteroids），故 radioVariant 取 default、cpufreq 用 qcom-cpufreq-hw/schedutil，未沿用 gold 的 mtk/sugov_ext。",
    "固件实测 CPU policy 归组为 3+4+1（capacity 569/939/1024），公开规格为 4x A520 + 3x A720 + 1x A720；本表按固件实测 cpus 成员填写，需在真机用 /sys/devices/system/cpu/cpufreq/policy*/affected_cpus 复核后定稿。",
    "触控 IC 依 Nothing 供应链与固件 Goodix 指纹 HAL 同源判为 goodix_ts；按键驱动严格用实测 gpio-keys，耳机插孔严格用实测 volcano-snd-card Headset Jack，未填任何小米专属 swtp / xiaomi.sensor.* 节点。",
    "GPU 依公开规格为 Adreno 710（940MHz）：glVersionFull / driverInfo / vulkanDriverVersion 按高通 V@0762.0 口径生成；vulkanDeviceId(0x07010000) 与三组 UUID 为推理占位，须以真机 glGetString / vkGetPhysicalDeviceProperties 实测替换。",
    "EDID 厂商码取 BOE、产品号 2591、2025 年第 12 周为推理值；需用 dumpsys SurfaceFlinger 或 /sys/class/drm 的 edid 十六进制串核对 pnp/id/week/year。",
    "传感器表严格按实测 chipOwners 建立（lsm6dsv_0 / mmc56x3x_0 / stk3a5x_0 / lps22df_0），并补 head_tracker（源于 sku_volcano 的 android.hardware.sensor.dynamic.head_tracker 特性）与 Nothing 私有 noth.sensor.flip_to_glyph（对应 vendor.noth.hardware.sensor.sensor_extension HAL）；若真机 dumpsys sensorservice 无气压计或无声传感器，需删对应条目，不得保留。",
    "官方 audio_policy 的模块名为 primary/a2dp/usb，虚拟 HAL 统一收敛为 AOSP 口径 default,bluetooth,r_submix；音量阶数按 QTI 默认 7/15。",
    "media 完全按实测 Codec2（c2.qti.* 共 16 个 component + 两个 c2 service）登记，假登记 store 统一 default2、rank=1，未占用 default。",
    "无红外遥控（enableIr=false）；NFC 有（enableNfc=true，JPN SKU 另加 eSE/omapi，其余 SKU 仅 euicc）；Widevine L1、双频 L5 GNSS 按 QTI 平台与公开规格填写。"
  ],
  "sources": [
    "目标机固件实测：build.prop / boot DTB / VINTF manifest / vendor/etc / odm/etc（brand=Nothing, device=Asteroids, board=volcano, SoC=QTI, kernel 6.1.157-android14-11）",
    "https://en.wikipedia.org/wiki/Nothing_Phone_3a/3a_Pro （2025-03-04 发布、6.77\" 1080x2392 LTPS AMOLED 120Hz、SM7635 Snapdragon 7s Gen 3 4nm、Adreno 710、LPDDR4X + UFS 2.2、8/12GB、128/256GB、5000mAh 50W、Wi-Fi 6 / BT 5.4、A059/A059p）",
    "对标基线：devices/manifest/configs/redmi/gold/gold-android15/（字段结构与颗粒度）",
    "Qualcomm 平台惯例：qcom-cpufreq-hw/schedutil、qpnp_pon 按键节点、kgsl-3d0 GPU 冷却设备、QCOM_GNSS_default、Vulkan driverID 25"
  ]
}
```
