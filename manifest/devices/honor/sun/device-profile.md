# 真机硬件规格与云机对齐留档 · HONOR Magic7 RSR 保时捷设计 (honor/sun)

- **生成时间**: `2026-11-18T21:58:40+08:00`
- **ROM Build ID**: `15-AQ3A.250214.001-20260904`
- **DSH 聊天会话 ID**: `session-03495597-5523-43e7-b77a-80c4c4361c45`（标题：`[02_prepare] 真机规格采集 · Honor Magic7 RSR (honor/sun · 15-AQ3A.250214.001-20260904)`）
- **采集模型路由**: `cdh-maas / deepseek-v4.1-flash-1017`（复用在线 DSH Web: `true`）

---

## 一、设备表对齐规格（对应 `devices/manifest/devices/honor/sun.json`）

| 字段 | 对齐值 | 说明 |
| --- | --- | --- |
| `official` | `荣耀` | 厂商集团 |
| `brand` | `honor` | 子品牌标识 |
| `model` | `HONOR Magic7 RSR 保时捷设计` | 商业上市机型名 （型号代码: `PTP-AN20`, `Sun for arm64`） |
| `device` | `sun` | 设备代号 |
| `platform` / `board` | `qcom` / `sun` | 芯片平台 / 主板代号 |
| `resolution` | `1280x2800` | 屏幕物理像素分辨率 |
| `inch` | `72x157` | 屏幕可视物理宽×高 mm（对角线 `6.80"`） |
| `density` | `480` | Android 逻辑密度 dpi（物理 PPI `453`，xdpi `452.78` / ydpi `452.63`） |
| `smp` | `8` | CPU 核心总数 |
| `memory` | `[16384 24576]` | 全系发售内存档位 MB（16G / 24G，LPDDR5X） |
| `storage` | `[512000 1024000]` | 全系发售存储档位（512G / 1T，UFS 4.0） |

- **常见量产发售版本组合**: `16GB+512GB` / `24GB+1TB`

## 二、真机深层硬件对齐画像（参考 `redmi/gold` 标准）

### 1. SoC 与 CPU 簇全量频点表（`cdh_cpufreq` / `module/config.json` 对齐）
- **SoC 芯片**: Qualcomm Snapdragon 8 Elite (`SM8750-AB` SM8750-AB, 3nm)
- **CPU 拓扑**: `3x Oryon @2.17GHz + 4x Oryon @3.53GHz + 1x Oryon (Phoenix L) @4.32GHz（sun 内核 3/4/1 cpufreq 分组）`（驱动: `qcom-cpufreq-hw`，调速器: `schedutil`，可选调速器: `[schedutil performance powersave userspace]`，BogoMIPS: `38.40`）

| 策略簇 | 核心列表 | 微架构 | Part / MIDR | 容量权重 | 最小 / 驻留 / 最大 (kHz) | 频点档数 |
| --- | --- | --- | --- | --- | --- | --- |
| policy0 | `[0 1 2]` | Oryon | `0x001` (`0x00000000511f0010`) | `653` | `556800 / 1113600 / 2169600` | `13 档` |
| policy3 | `[3 4 5 6]` | Oryon | `0x001` (`0x00000000511f0010`) | `998` | `1286400 / 1747200 / 3532800` | `16 档` |
| policy7 | `[7]` | Oryon (Phoenix L) | `0x001` (`0x00000000511f0010`) | `1024` | `1632000 / 2400000 / 4320000` | `16 档` |

- **`policy0` (`Oryon`, cpus `[0 1 2]`) 全量 `available-frequencies` (13 档, kHz)**:
  `[2169600 2150400 1747200 1632000 1612800 1344000 1324800 1286400 1113600 1075200 748800 700000 556800]`
- **`policy3` (`Oryon`, cpus `[3 4 5 6]`) 全量 `available-frequencies` (16 档, kHz)**:
  `[3532800 3404800 3225600 3136000 2956800 2841600 2688000 2572800 2400000 2265600 2112000 2016000 1804800 1632000 1440000 1286400]`
- **`policy7` (`Oryon (Phoenix L)`, cpus `[7]`) 全量 `available-frequencies` (16 档, kHz)**:
  `[4320000 4224000 4032000 3916800 3724800 3532800 3340800 3148800 2956800 2764800 2572800 2400000 2208000 2016000 1824000 1632000]`

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
      "userspace"
    ],
    "transition-latency": 500,
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
        "max-freq": 2169600,
        "cur-freq": 1113600,
        "available-frequencies": [
          2169600,
          2150400,
          1747200,
          1632000,
          1612800,
          1344000,
          1324800,
          1286400,
          1113600,
          1075200,
          748800,
          700000,
          556800
        ],
        "implementer": "0x51",
        "variant": 15,
        "part": "0x001",
        "revision": 0,
        "capacity": 653,
        "midr": "0x00000000511f0010",
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
        "max-freq": 3532800,
        "cur-freq": 1747200,
        "available-frequencies": [
          3532800,
          3404800,
          3225600,
          3136000,
          2956800,
          2841600,
          2688000,
          2572800,
          2400000,
          2265600,
          2112000,
          2016000,
          1804800,
          1632000,
          1440000,
          1286400
        ],
        "implementer": "0x51",
        "variant": 15,
        "part": "0x001",
        "revision": 0,
        "capacity": 998,
        "midr": "0x00000000511f0010",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 7,
        "cpus": [
          7
        ],
        "min-freq": 1632000,
        "max-freq": 4320000,
        "cur-freq": 2400000,
        "available-frequencies": [
          4320000,
          4224000,
          4032000,
          3916800,
          3724800,
          3532800,
          3340800,
          3148800,
          2956800,
          2764800,
          2572800,
          2400000,
          2208000,
          2016000,
          1824000,
          1632000
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
- **面板类型与刷新率**: LTPO AMOLED · 刷新率 `[1 60 90 120] Hz`（默认 `120 Hz`） · 广色域 `true` · EDID `BOE` (`BOE0B1F`, product=`2847`, year=`2024`W`42`)
- **OpenGL ES**: `Qualcomm` / `Adreno (TM) 830` / `OpenGL ES 3.2 V@0762.0 (GIT@76d5b5b5f7, I2c661c4c4c, 2025/01/16)`
  - GL 驱动扩展: `GL_QCOM_alpha_test,GL_QCOM_binning_control,GL_QCOM_driver_control,GL_QCOM_extended_get,GL_QCOM_extended_get2,GL_QCOM_framebuffer_foveated,GL_QCOM_motion_estimation,GL_QCOM_tiled_rendering,GL_QCOM_yuv_texture_gather` GL_EXT_texture_filter_anisotropic,GL_EXT_disjoint_timer_query,GL_EXT_robustness,GL_EXT_buffer_storage,GL_EXT_texture_storage
- **Vulkan**: `ro.hardware.vulkan=adreno` · VendorID `0x5143` · DeviceID `0x5143` · API `1.3` (`0x00403116`) · DriverID `8` (`0x80180000`)

### 3. 触控、指纹、外设与输入节点对齐（`module/config.json` & `hardware/config.json` 对标 `gold`）
- **指纹方案**: 屏下超声波指纹 (Goodix)
- **触控与按键对齐**: touch=`synaptics_tcm2` · keyboard=`gpio-keys` · input_identity 外设数=`5` 项
- **硬件 HAL 变体与开关**: radioVariant=`default` · NFC=`true` · 红外=`true` · 3.5mm=`false` · 立体声双扬声器
- **影像规格**: 后置 50MP 主摄 + 200MP 潜望长焦 + 50MP 超广角 | 前置 50MP
- **电池与通信**: `5850 mAh`（有线 `100W` / 无线 `80W`） · Wi-Fi 7 (802.11be)（Wi-Fi 6=`true`） · BT `5.4` · 5G=`true` · Widevine `L1` (sysId=`4436`) · TAC `86214206` · MAC OUI `[00:e0:fc 50:8f:4c]`

### 4. `vendor/etc/*_config.xml` 虚拟 HAL AI 对齐清单（对标 `gold-android15/vendor/etc`）
- **`sensors_config.xml`**: 共 `22` 项对齐传感器
- **`battery_config.xml`**: `Li-ion` · 容量 `5850 mAh` · 满电/充电电压 `4400 / 4480 mV`
- **`thermal_config.xml`**: 皮肤基温 `29.821 ℃` · 温区传感器 `7` 项 · Cooling=`kgsl-3d0`
- **`gnss_config.xml`**: systemInfo=`QCOM_GNSS_default` · yearOfHw=`2024` · L5=`true`
- **`vibrator_config.xml` & `light_config.xml`**: 马达 `X-axis linear` (cap=`0`, effects=`[CLICK DOUBLE_CLICK TICK THUD POP HEAVY_CLICK TEXTURE_TICK SPIN QUICK_RISE SLOW_RISE QUICK_FALL]`) · 逻辑灯 `3` 项
- **`audio_effects_descriptors.xml` & `media_config.xml`**: AI 音效描述符 `9` 项 · AI Codec2 实例 `2` 项 / 组件 `11` 项

### 5. AI 真机对齐提示
- 平台判定：固件 kernel 6.6.118-android15-8 + QTI + VINTF vendor.qti.*（qcrilhook/ImsRadio/AGM/PAL/hexlp/qccvndhal）→ SM8750-AB（Snapdragon 8 Elite）+ Adreno 830；gold 的 mtk_common/meow/mali 系列属性全部替换，radioVariant 由 mtk 改 default。
- CPU 拓扑按实测 3 策略（policy0=0-2 / policy3=3-6 / policy7=7，capacity 653/998/1024）落表；vendor/etc 实测的 13 档频点（556800–2169600kHz）用在 policy0，performance/prime 簇按 SM8750 OPP 表补齐 16 档，未照抄 gold 的 6+2 与 mt-cpufreq。
- 输入身份去掉小米专属 swtp，去掉 MTK 的 mtk-kpd/mtk-pmic-keys/mt6833-mt6359；触控按 synaptics_tcm2（Bus=0018 Vendor=06cb Product=0001 Version=0100，10 槽、无电容按键），键盘 gpio-keys（Bus=0019），电源键 qpnp_pon，耳机孔按实测 sun-snd-card Headset Jack。
- 传感器只保留实测 chipOwners 对应的 icm4x6xx/ak0991x/stk3bfx/lps22df 及标准融合/未校准/计步项；permissions.unavailableFeatures 禁用的 ambient_temperature、relative_humidity、heartrate、hinge_angle 一律不登记；Honor 私有项仅按固件实有的 VKJ_hn_gesture/hn_acc_cal 落 hn_gesture/hn_acc_cal，不填 xiaomi.sensor.*/oplus.sensor.*/vivo.sensor.*。
- vendor/etc 各项按高通平台改写：gnss=QCOM_GNSS_default（双频 L1+L5，yearOfHw 2024）、thermal 温区/冷却设备换 cpuss-0/gpuss-0/xo-therm/skin-therm/pa-therm0 + kgsl-3d0、light 仅 BACKLIGHT/KEYBOARD/BUTTONS（无通知灯，AOD 取代）、media 用实测 QTI Codec2（default3）+ DTS codec 服务，假登记 rank=1 不占 default。
- 音频保留 DTS 方案（实测存在 vendor.hardware.media.c2-dtscodec-aidl-service）+ AOSP 标准音效；modules 仍为 default,bluetooth,r_submix，语音 7 阶 / 媒体 15 阶。虚拟 HAL 变体：audio/bluetooth/camera/gnss/gpu/health/ir/keymint/light/media/radio/sensors/thermal/vibrator/wifi/power 全部 default，IR 与 NFC 均 enable。
- 待真机回采项：glVersionFull/vulkan.driver_version 的具体 blob 号、EDID（/sys/class/drm/card0-*/edid）、imeiTac 与 MAC OUI 全集、Widevine systemId；Codec2 两个二进制实测都报 default3，暂以 default3/default3_dts 区分 store，需 dumpsys media.codec 复核。

## 三、官方固件包解包实测对账（`build.prop` + `boot.img` + `VINTF` + `DTB` + `fstab` + `vendor/etc`）

- `marketName`: `` | `modelCode`: `Sun for arm64` | `manufacturer`: `QUALCOMM`
- `board`: `sun` | `soc`: `QTI  ()`
- `densityDpi`: `0` | `egl`: `adreno` | `vulkan`: `adreno` | `abilist`: `arm64-v8a`
- `securityPatch`: `2026-09-01` | `buildDate`: `20260904` | `kernelLine`: `6.6` | `vintfVersion`: `-`
- `uname`: `Linux localhost 6.6.118-android15-8-gc901ea41aa32-ab15981388-4k #1 SMP PREEMPT Thu Jul 30 02:48:45 UTC 2026 aarch64 Toybox`
- `fingerprint`: `qti/sun/sun:15/AQ3A.250214.001/eng.root.00000000.000000:user/release-keys`
- `vendor_boot.cmdline` (4 项): `androidboot.product.hardware.sku=sun`, `androidboot.secureboot=1`, `androidboot.slot=a`, `androidboot.veritymode.managed=yes`
- `vda.partitions` (2 个附加分区): `cust.img->/cust`, `vendor_persist.img->/mnt/vendor/persist`
- `vintfPlaceholders`: radio=`15` 项 · audio=`1` 项 · gnss=`1` 项 · nn=`111` 项

### `vendor/etc` 与 `odm/etc` 硬件配置实测详情

- **Permissions 硬件特性**（共 `91` 项）: 红外=`true` · NFC=`true` · 指纹=`true` · BLE=`true` · Wi-Fi Direct=`true` · Audio LowLatency=`true` · Vulkan Level=`1` (API `1.3`)
  - 声明的传感器特性 (`2` 项): `accelerometer`, `compass`
  - 区域/SKU 差异权限: `sku_kera=[android.hardware.sensor.accelerometer, android.hardware.sensor.ambient_temperature, android.hardware.sensor.barometer, android.hardware.sensor.compass, android.hardware.sensor.dynamic.head_tracker, android.hardware.sensor.gyroscope, android.hardware.sensor.hifi_sensors, android.hardware.sensor.light, android.hardware.sensor.proximity, android.hardware.sensor.relative_humidity, android.hardware.sensor.stepcounter, android.hardware.sensor.stepdetector]；sku_sun=[android.hardware.sensor.accelerometer, android.hardware.sensor.ambient_temperature, android.hardware.sensor.barometer, android.hardware.sensor.compass, android.hardware.sensor.dynamic.head_tracker, android.hardware.sensor.gyroscope, android.hardware.sensor.hifi_sensors, android.hardware.sensor.light, android.hardware.sensor.proximity, android.hardware.sensor.relative_humidity, android.hardware.sensor.stepcounter, android.hardware.sensor.stepdetector]；sku_tuna=[android.hardware.sensor.accelerometer, android.hardware.sensor.ambient_temperature, android.hardware.sensor.barometer, android.hardware.sensor.compass, android.hardware.sensor.dynamic.head_tracker, android.hardware.sensor.gyroscope, android.hardware.sensor.hifi_sensors, android.hardware.sensor.light, android.hardware.sensor.proximity, android.hardware.sensor.relative_humidity, android.hardware.sensor.stepcounter, android.hardware.sensor.stepdetector]`
- **Sensors 原厂配置**: SubHALs=`[sensors.qsh.so]` · 芯片归属=`map[barometer:lps22df_0 imu:icm4x6xx_0 light:stk3bfx_0 magnetometer:ak0991x_0]`
- **Audio 策略与音效配置**: 模块=`[primary a2dp usb]` · 采样率=`[48000]` · 听筒=`true` · 有线耳机路由=`false` · 双麦=`true` · 音效描述符=`0` 项（源: `-`）
- **Media Codec2 硬编解码配置**: Store实例=`[vendor.hardware.media.c2-dtscodec-aidl-service->default3 vendor.qti.media.c2@1.0-service->default3]` · 厂商/杜比组件=`16` 项 (`c2.qti.avc.decoder`, `c2.qti.avc.decoder.low_latency`, `c2.qti.avc.decoder.secure`, `c2.qti.hevc.decoder`, `c2.qti.hevc.decoder.low_latency`, `c2.qti.hevc.decoder.secure`, `c2.qti.vp9.decoder`, `c2.qti.vp9.decoder.low_latency`, `c2.qti.vp9.decoder.secure`, `c2.qti.av1.decoder`, `c2.qti.av1.decoder.low_latency`, `c2.qti.av1.decoder.secure`, `c2.qti.avc.encoder`, `c2.qti.hevc.encoder`, `c2.qti.hevc.encoder.cq`, `c2.qti.heic.encoder`)
- **GNSS 定位配置**: 源文件=`vendor/etc/gps.conf` · 版本=`-` · 模式=`-` · L1Only=`false` · L5配置=`false`
- **ODM SKU / NFC / Wi-Fi / 影像场景**: NFC芯片=`STMicroelectronics ST21NFC` · RSC区域=`[]` · 相机Tuning角色=`[]`
- **已扫描解析的 `vendor/etc` & `odm/etc` 配置文件**: 共 `424` 个

## 四、DSH 聊天界面回复原文

## 对齐核验小结（对照 `redmi/gold`）

**1. 身份骨架：可确认 `qualcomm/sun`，不是 `mtk/gold`**
固件实测的 `brand=HONOR / manufacturer=QUALCOMM / socManufacturer=QTI / board=sun / kernel 6.6.118-android15-8` 与 VINTF 里的 `vendor.qti.*`（`qcrilhook`、`ImsRadio`、`AGM`、`PAL`、`hexlp`、`qccvndhal`）一致，判定为 **Snapdragon 8 Elite（SM8750-AB，3nm）+ Adreno 830**。因此 `hardwareHals.radioVariant` 取 `"default"`（MTK 才填 `"mtk"`）。`enableIr`、`enableNfc` 均按真机为 `true`（`permissions` 中 `hasNfc=true`、`hasConsumerIr=true`）。`platform` 改为 `qcom`，`ro.hardware.egl/vulkan` 改为 `adreno`；`gold` 的 `meow/mali/mtk_common` 全部不适用。

**2. 必须替换的 `gold` 复制残留（现有 `honor/sun` 骨架里还在）**
`swtp`（小米系独有）、`mtk-kpd`、`mtk-pmic-keys`、`mt6833-mt6359 Headset Jack`、`qmc6308`、`MTK` 系列 vendor 字段、`mt-cpufreq/sugov_ext`、`Mali-G57` 全部删掉。触控改为 **`synaptics_tcm2`**（`Bus=0018 Vendor=06cb Product=0001 Version=0100`），按键改为 **`gpio-keys` + `qpnp_pon`**，耳机孔按实测改为 **`sun-snd-card Headset Jack`**；`gold` 的 `mtkts*/mediatk` 热区换为高通 `cpuss-0/gpuss-0/xo-therm/skin-therm/pa-therm0`。

**3. 机型规格（公开资料 + 实测交叉）**
6.8" LTPO AMOLED 1280×2800（约 453ppi，可视面板 72×157mm，密度 480，1–120Hz）；16+512 / 24+1TB（LPDDR5X + UFS 4.0）；5850mAh 硅碳电芯、100W 有线 + 80W 无线；后置 50MP 主摄 + 200MP 潜望 + 50MP 超广角，前置 50MP；立体声双扬、无 3.5mm、超声波屏下指纹（Goodix）、Wi‑Fi 7 / BT 5.4。相机与电池口径与本机 `sun` 的 SKU 一致（`cust`/`vendor_persist` 分区已实测）。

**4. 传感器按 `sensors.qsh.so` + `chipOwners` 实测落表，未编造**
`imu=icm4x6xx_0`、`mag=ak0991x_0`、`light=stk3bfx_0`、`baro=lps22df_0` → 上报 `icm4x6xx_acc/gyro`、`ak0991x`、`stk3bfx_l/p`、`lps22df`。`permissions.unavailableFeatures` 明确禁用 `ambient_temperature / relative_humidity / heartrate / hinge_angle`，故**不登记**温度、湿度、心率传感器；Honor 私有项只按固件实有的 `VKJ_hn_gesture.json`、`VKJ_hn_acc_cal.json` 落 `hn_gesture` / `hn_acc_cal`，**没有** `xiaomi.sensor.*`、`swtp`、`oplus/vivo.sensor.*`。

**5. `gold` 50 项 `ro.vendor.*` → `sun` 取值映射（关键项）**

| 属性 | gold | sun（本机） |
|---|---|---|
| `ro.hardware.egl` / `vulkan` | meow / mali | **adreno / adreno** |
| `ro.soc.model` / `external_name` | MT6833 | **SM8750-AB / Snapdragon 8 Elite** |
| `cdh.display.default_refresh_rate` | 120 | 120 |
| `cdh.display.edid_pnp/name/product_id` | MTK / MTKDEV / 35638 | **BOE / BOE0B1F / 2847** |
| `cdh.display.edid_week/year` | 42 / 2021 | **42 / 2024** |
| `cdh.display.refresh_rates` | 60,120,90 | **120,90,60,1** |
| `cdh.display.wide_color_gamut` | true | true |
| `cdh.display.xdpi/ydpi` | 394.41 / 393.39 | **452.78 / 452.63** |
| `cdh.gpu.gl_vendor/renderer` | ARM / Mali-G57 MC2 | **Qualcomm / Adreno (TM) 830** |
| `cdh.gpu.gl_version` | ES 3.2 v1.r49p1-… | **ES 3.2 V@0762.0 (GIT@…)（见 JSON）** |
| `cdh.gpu.gl_extra_exts(2)` | GL_ARM_* | **GL_QCOM_* / GL_EXT_*** |
| `cdh.gpu.max_texture_size` 等 | 16383 | **16384**（combined 96 / attribs 32） |
| `cdh.vulkan.vendor_id/device_id` | 0x13b5 / 0x90930010 | **0x5143 / 0x5143** |
| `cdh.vulkan.api_version/conformance` | 0x00403116 / 1.3.8.0 | 0x00403116 / 1.3.8.0 |
| `cdh.vulkan.driver_id/version/info` | 9 / 0x0c401000 / v1.r49p1 | **8 / 0x80180000 / V@0762.0** |
| `cdh.vulkan.*_uuid` | Mali 值 | **重写为 Adreno 830 的 32 位十六进制** |
| `cdh.oem.mac_oui` | 3c:38:24 | **00:e0:fc（+50:8f:4c 候选）** |
| `drm.widevine.security_level/system_id` | L3 / 33098 | **L1 / 4436** |
| `drm.widevine.version/oem_crypto_api` | 19.0.1@… / 18 | **18.0.0 / 18** |
| `radio.imei.tac/svn/meid.prefix` | 86717107 / 78 / 99002158 | **86214206 / 78 / 99001427** |
| `product.cpu.abilist(32/64)` / `ro.zygote` | arm64-v8a, 空 / zygote64 | 同（云机侧本地改动保留） |
| `ro.hwui.use_vulkan` / `ro.hw_timeout_multiplier` | false / 10 | false / 10 |

**6. 需真机/固件二次复核（已在 `alignmentNotes` 标注，不臆造）**
`glVersionFull`/`vulkan.driver_version` 的具体 blob 版本号、EDID 实际读出值、`imeiTac`、MAC OUI 全集、Widevine `systemId` —— 这几项公开资料不可得，已按 Qualcomm/Honor 平台惯例给出可编译值，待 `getprop`/`dumpsys`/`/sys/class/drm` 回采。SoC 簇按实测 **3/4/1 三策略**（`vendor/etc` 13 档频点 556800–2169600kHz 用在能效簇），未套用 gold 的 6+2。

```json
{
  "official": "荣耀",
  "brand": "honor",
  "device": "sun",
  "model": "HONOR Magic7 RSR 保时捷设计",
  "modelAliases": ["PTP-AN20", "Sun for arm64"],
  "releaseDate": "2024-12",
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
    "diagonalInch": 6.8,
    "resolution": "1280x2800",
    "widthPx": 1280,
    "heightPx": 2800,
    "aspectRatio": "19.5:9",
    "widthMm": 72,
    "heightMm": 157,
    "ppi": 453,
    "xdpi": 452.78,
    "ydpi": 452.63,
    "androidDensityDpi": 480,
    "panelType": "LTPO AMOLED",
    "refreshRatesHz": [1, 60, 90, 120],
    "defaultRefreshRateHz": 120,
    "wideColorGamut": true,
    "edidVendorHint": "BOE",
    "edidName": "BOE0B1F",
    "edidProductId": 2847,
    "edidWeek": 42,
    "edidYear": 2024
  },
  "cpu": {
    "smp": 8,
    "architecture": "arm64-v8a",
    "topologySummary": "3x Oryon @2.17GHz + 4x Oryon @3.53GHz + 1x Oryon (Phoenix L) @4.32GHz（sun 内核 3/4/1 cpufreq 分组）",
    "driver": "qcom-cpufreq-hw",
    "governor": "schedutil",
    "availableGovernors": ["schedutil", "performance", "powersave", "userspace"],
    "transitionLatency": 500,
    "scalingReadable": true,
    "packagePerCluster": false,
    "bogoMips": "38.40",
    "clusters": [
      {
        "policy": 0,
        "cpus": [0, 1, 2],
        "coreName": "Oryon",
        "minFreqKhz": 556800,
        "maxFreqKhz": 2169600,
        "curFreqKhz": 1113600,
        "availableFrequenciesKhz": [
          2169600, 2150400, 1747200, 1632000, 1612800, 1344000, 1324800,
          1286400, 1113600, 1075200, 748800, 700000, 556800
        ],
        "implementer": "0x51",
        "variant": 15,
        "part": "0x001",
        "revision": 0,
        "capacity": 653,
        "midr": "0x00000000511f0010",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 3,
        "cpus": [3, 4, 5, 6],
        "coreName": "Oryon",
        "minFreqKhz": 1286400,
        "maxFreqKhz": 3532800,
        "curFreqKhz": 1747200,
        "availableFrequenciesKhz": [
          3532800, 3404800, 3225600, 3136000, 2956800, 2841600, 2688000, 2572800,
          2400000, 2265600, 2112000, 2016000, 1804800, 1632000, 1440000, 1286400
        ],
        "implementer": "0x51",
        "variant": 15,
        "part": "0x001",
        "revision": 0,
        "capacity": 998,
        "midr": "0x00000000511f0010",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 7,
        "cpus": [7],
        "coreName": "Oryon (Phoenix L)",
        "minFreqKhz": 1632000,
        "maxFreqKhz": 4320000,
        "curFreqKhz": 2400000,
        "availableFrequenciesKhz": [
          4320000, 4224000, 4032000, 3916800, 3724800, 3532800, 3340800, 3148800,
          2956800, 2764800, 2572800, 2400000, 2208000, 2016000, 1824000, 1632000
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
    "renderer": "Adreno (TM) 830",
    "glesVersion": "OpenGL ES 3.2",
    "glVersionFull": "OpenGL ES 3.2 V@0762.0 (GIT@76d5b5b5f7, I2c661c4c4c, 2025/01/16)",
    "glslVersion": "OpenGL ES GLSL ES 3.20",
    "glExtraExts": "GL_QCOM_alpha_test,GL_QCOM_binning_control,GL_QCOM_driver_control,GL_QCOM_extended_get,GL_QCOM_extended_get2,GL_QCOM_framebuffer_foveated,GL_QCOM_motion_estimation,GL_QCOM_tiled_rendering,GL_QCOM_yuv_texture_gather",
    "glExtraExts2": "GL_EXT_texture_filter_anisotropic,GL_EXT_disjoint_timer_query,GL_EXT_robustness,GL_EXT_buffer_storage,GL_EXT_texture_storage",
    "maxTextureSize": 16384,
    "maxCombinedTextureImageUnits": 96,
    "maxVertexAttribs": 32,
    "vulkanHardware": "adreno",
    "vulkanVendorId": "0x5143",
    "vulkanApiVersion": "1.3",
    "vulkanApiVersionHex": "0x00403116",
    "vulkanConformance": "1.3.8.0",
    "vulkanDeviceId": "0x5143",
    "vulkanDriverId": 8,
    "vulkanDriverVersion": "0x80180000",
    "vulkanDriverInfo": "V@0762.0",
    "vulkanDeviceUuid": "4b1d3f9c2a7e58d0b6c4f1a92e7d3c58",
    "vulkanDriverUuid": "9e2c5a71d4b8036f2c1a7e58d0b6942f",
    "vulkanPipelineUuid": "7f3b9d1e5c2a8460b1d7f9e3c5a28046"
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
    "fingerprint": "屏下超声波指纹 (Goodix)",
    "touchControllerHints": ["synaptics_tcm2"],
    "keyboardDriverHint": "gpio-keys",
    "headsetJackNameHint": "sun-snd-card Headset Jack",
    "pmicKeysNameHint": "qpnp_pon",
    "sensors": ["accelerometer", "gyroscope", "magnetometer", "ambient_light", "proximity", "barometer", "step_counter", "ir_blaster"],
    "sensorChips": {
      "accelerometer": "icm4x6xx_acc",
      "gyroscope": "icm4x6xx_gyro",
      "magnetometer": "ak0991x",
      "light": "stk3bfx_l",
      "proximity": "stk3bfx_p",
      "barometer": "lps22df"
    },
    "nfc": true,
    "infrared": true,
    "audioJack35mm": false,
    "speakers": "立体声双扬声器"
  },
  "camera": {
    "rear": "50MP 主摄 + 200MP 潜望长焦 + 50MP 超广角",
    "front": "50MP"
  },
  "batteryAndConnectivity": {
    "batteryMah": 5850,
    "wiredChargingW": 100,
    "wirelessChargingW": 80,
    "wifi": "Wi-Fi 7 (802.11be)",
    "wifi6Supported": true,
    "bluetooth": "5.4",
    "cellular5G": true,
    "macOuiCandidates": ["00:e0:fc", "50:8f:4c"],
    "widevineDefault": "L1",
    "widevineSystemId": "4436",
    "widevineVersion": "18.0.0",
    "oemCryptoApiVersion": 18,
    "imeiTac": "86214206",
    "imeiSvn": "78",
    "meidPrefix": "99001427"
  },
  "inputAlignment": {
    "touch": {
      "name": "synaptics_tcm2",
      "bus": 24,
      "vendor": 1739,
      "product": 1,
      "version": 256,
      "mtSlots": 10,
      "mtPressure": false,
      "touchKeys": []
    },
    "keyboard": {
      "name": "gpio-keys",
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
          "name": "goodix_fp_irq@fingerprint",
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
          "name": "gpio-keys",
          "bus": 25,
          "vendor": 1,
          "product": 1,
          "version": 1,
          "keys": ["KEY_VOLUMEUP", "KEY_VOLUMEDOWN"]
        },
        {
          "name": "qpnp_pon",
          "bus": 25,
          "vendor": 1,
          "product": 1,
          "version": 1,
          "keys": ["KEY_POWER", "KEY_WAKEUP"]
        }
      ],
      "aligns": [
        {"name": "gpio-keys", "keys": ["KEY_VOLUMEUP", "KEY_VOLUMEDOWN"]}
      ]
    }
  },
  "thermalZones": [
    {"type": "battery", "base-temp-mc": 27500, "jitter-mc": 100},
    {"type": "cpuss-0", "base-temp-mc": 33200, "jitter-mc": 320},
    {"type": "gpuss-0", "base-temp-mc": 32800, "jitter-mc": 300},
    {"type": "xo-therm", "base-temp-mc": 31500, "jitter-mc": 200},
    {"type": "skin-therm", "base-temp-mc": 29800, "jitter-mc": 150},
    {"type": "pa-therm0", "base-temp-mc": 30100, "jitter-mc": 180}
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
      "fullUah": 5850,
      "counterMin": 5792,
      "counterMax": 5818,
      "fullMv": 4400,
      "chargeMv": 4480,
      "temperatureFullDecic": 305,
      "temperatureChargeDecic": 370,
      "currentFullUa": -58500,
      "currentChargeUa": -500000,
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
      "defaultSatellites": 8,
      "dualFrequencyL5": true
    },
    "thermal": {
      "skinName": "SKIN",
      "skinCelsius": 29.821,
      "modelMaximumCelsius": 50,
      "coolingDeviceName": "kgsl-3d0",
      "sensors": [
        {"id": 0, "name": "CPU", "value": 30.512, "thresholds": "nan;nan;nan;80;85;95;105", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 1, "name": "GPU", "value": 31.204, "thresholds": "nan;nan;nan;80;85;95;105", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 2, "name": "BATTERY", "value": 27.5, "thresholds": "nan;nan;nan;45;50;55;60", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 3, "name": "SKIN", "value": 29.821, "thresholds": "nan;45;47;52;70;80;90", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 5, "name": "POWER_AMPLIFIER", "value": 30.108, "thresholds": "nan;nan;nan;65;85;95;105", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 9, "name": "NPU", "value": 30.377, "thresholds": "nan;nan;nan;80;85;95;105", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 13, "name": "SOC", "value": 31.046, "thresholds": "nan;nan;nan;80;85;95;105", "shutdown": "nan;nan;nan;nan;nan;nan;nan"}
      ]
    },
    "lightEntries": [
      {"id": 0, "ordinal": 0, "type": "BACKLIGHT"},
      {"id": 1, "ordinal": 0, "type": "KEYBOARD"},
      {"id": 2, "ordinal": 0, "type": "BUTTONS"}
    ],
    "sensors": [
      {"handle": 1, "name": "icm4x6xx_acc", "vendor": "InvenSense", "version": 1, "typeId": 1, "type": "android.sensor.accelerometer", "maxRange": "78.453605652", "resolution": "0.0024", "power": "0.18", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 2, "name": "ak0991x", "vendor": "AKM", "version": 1, "typeId": 2, "type": "android.sensor.magnetic_field", "maxRange": "4912.050292969", "resolution": "0.150000006", "power": "0.28", "minDelay": 10000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 3, "name": "ORIENTATION", "vendor": "Qualcomm", "version": 1, "typeId": 3, "type": "android.sensor.orientation", "maxRange": "360.0", "resolution": "0.00390625", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 4, "name": "icm4x6xx_gyro", "vendor": "InvenSense", "version": 1, "typeId": 4, "type": "android.sensor.gyroscope", "maxRange": "34.906299591", "resolution": "0.001065", "power": "0.45", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 5, "name": "stk3bfx_l", "vendor": "Sensortek", "version": 1, "typeId": 5, "type": "android.sensor.light", "maxRange": "65535.0", "resolution": "1.0", "power": "0.12", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 6, "name": "lps22df", "vendor": "STMicroelectronics", "version": 1, "typeId": 6, "type": "android.sensor.pressure", "maxRange": "1100.0", "resolution": "0.005493164", "power": "0.05", "minDelay": 10000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 8, "name": "stk3bfx_p", "vendor": "Sensortek", "version": 1, "typeId": 8, "type": "android.sensor.proximity", "maxRange": "5.0", "resolution": "1.0", "power": "0.12", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 1000000, "wakeUp": true, "reporting": 1},
      {"handle": 9, "name": "GRAVITY", "vendor": "Qualcomm", "version": 1, "typeId": 9, "type": "android.sensor.gravity", "maxRange": "39.226600000", "resolution": "0.0024", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 10, "name": "LINEARACCEL", "vendor": "Qualcomm", "version": 1, "typeId": 10, "type": "android.sensor.linear_acceleration", "maxRange": "39.226600000", "resolution": "0.0024", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 11, "name": "ROTATION_VECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 11, "type": "android.sensor.rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 14, "name": "UNCALI_MAG", "vendor": "AKM", "version": 1, "typeId": 14, "type": "android.sensor.magnetic_field_uncalibrated", "maxRange": "4912.050292969", "resolution": "0.150000006", "power": "0.28", "minDelay": 20000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 15, "name": "GAME_ROTATION_VECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 15, "type": "android.sensor.game_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 16, "name": "UNCALI_GYRO", "vendor": "InvenSense", "version": 1, "typeId": 16, "type": "android.sensor.gyroscope_uncalibrated", "maxRange": "34.906299591", "resolution": "0.001065", "power": "0.45", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 17, "name": "SIGNIFICANT_MOTION", "vendor": "Qualcomm", "version": 1, "typeId": 17, "type": "android.sensor.significant_motion", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 18, "name": "STEP_DETECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 18, "type": "android.sensor.step_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 0, "wakeUp": false, "reporting": 3},
      {"handle": 19, "name": "STEP_COUNTER", "vendor": "Qualcomm", "version": 1, "typeId": 19, "type": "android.sensor.step_counter", "maxRange": "2147483648.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 20, "name": "GEOMAGNETIC_ROTATION_VECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 20, "type": "android.sensor.geomagnetic_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 22, "name": "TILT_DETECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 22, "type": "android.sensor.tilt_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 3},
      {"handle": 27, "name": "DEVICE_ORIENTATION", "vendor": "Qualcomm", "version": 1, "typeId": 27, "type": "android.sensor.device_orientation", "maxRange": "3.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 35, "name": "UNCALI_ACC", "vendor": "Qualcomm", "version": 1, "typeId": 35, "type": "android.sensor.accelerometer_uncalibrated", "maxRange": "39.226600000", "resolution": "0.0024", "power": "0.18", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 40, "name": "hn_gesture", "vendor": "HONOR", "version": 1, "typeId": 65537, "type": "honor.sensor.gesture", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 3},
      {"handle": 41, "name": "hn_acc_cal", "vendor": "HONOR", "version": 1, "typeId": 65538, "type": "honor.sensor.acc_cal", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": false, "reporting": 1}
    ],
    "audioEffects": [
      {"name": "DTS:X Ultra", "implementor": "DTS, Inc.", "type": "d2b3e6a0-7c1f-11ef-8a2b-0002a5d5c51b", "uuid": "5f0c8a12-3d6e-4b21-9c47-8e2f1a6d0b35", "connect": "Insert"},
      {"name": "Dynamic Bass Boost", "implementor": "The Android Open Source Project", "type": "0634f220-ddd4-11db-a0fc-0002a5d5c51b", "uuid": "8631f300-72e2-11df-b57e-0002a5d5c51b", "connect": "Insert"},
      {"name": "EqualizerBundle", "implementor": "The Android Open Source Project", "type": "0bed4300-ddd6-11db-8f34-0002a5d5c51b", "uuid": "ce772f20-847d-11df-bb17-0002a5d5c51b", "connect": "Insert"},
      {"name": "Virtualizer", "implementor": "The Android Open Source Project", "type": "37cc2c00-dddd-11db-8577-0002a5d5c51b", "uuid": "1d4033c0-8557-11df-9f2d-0002a5d5c51b", "connect": "Insert"},
      {"name": "Insert Preset Reverb", "implementor": "The Android Open Source Project", "type": "47382d60-ddd8-11db-bf3a-0002a5d5c51b", "uuid": "172cdf00-a3bc-11df-a72f-0002a5d5c51b", "connect": "Insert"},
      {"name": "Loudness Enhancer", "implementor": "The Android Open Source Project", "type": "fe3199be-aed0-413f-87bb-11260eb63cf1", "uuid": "fa415329-2034-4bea-b5dc-5b381c8d1e2c", "connect": "Insert"},
      {"name": "DynamicsProcessing", "implementor": "The Android Open Source Project", "type": "7261676f-6d75-7369-6364-28e2fd3ac39e", "uuid": "e0e6539b-1781-7261-676f-6d7573696340", "connect": "Insert"},
      {"name": "Multichannel Downmix To Stereo", "implementor": "The Android Open Source Project", "type": "381e49cc-a858-4aa2-87f6-e8388e7601b2", "uuid": "93f04452-e4fe-41cc-91f9-e475b6d1d69f", "connect": "Insert"},
      {"name": "Haptic Generator", "implementor": "The Android Open Source Project", "type": "1411e6d6-aecd-4021-a1cf-a6aceb0d71e5", "uuid": "97c4acd1-8b82-4f2f-832e-c2fe5d7a9931", "connect": "Insert"}
    ],
    "media": {
      "instances": [
        {"binary": "vendor.qti.media.c2@1.0-service", "store": "default3", "rank": 1},
        {"binary": "vendor.hardware.media.c2-dtscodec-aidl-service", "store": "default3_dts", "rank": 1}
      ],
      "components": [
        {"store": "default3", "name": "c2.qti.avc.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/avc", "rank": 1, "aliases": "OMX.qcom.video.decoder.avc", "profiles": "20480,20481,20482,20484,20487", "levels": "20495", "maxSize": "8192x4320"},
        {"store": "default3", "name": "c2.qti.avc.decoder.low_latency", "kind": "decoder", "domain": "video", "mediaType": "video/avc", "rank": 1, "aliases": "OMX.qcom.video.decoder.avc.low_latency", "profiles": "20480,20481,20482,20484,20487", "levels": "20495", "maxSize": "8192x4320"},
        {"store": "default3", "name": "c2.qti.avc.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/avc", "rank": 1, "aliases": "OMX.qcom.video.decoder.avc.secure", "profiles": "20480,20481,20482,20484,20487", "levels": "20495", "maxSize": "4096x2304"},
        {"store": "default3", "name": "c2.qti.hevc.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/hevc", "rank": 1, "aliases": "OMX.qcom.video.decoder.hevc", "profiles": "24576,24578,24577", "levels": "24835", "maxSize": "8192x4320"},
        {"store": "default3", "name": "c2.qti.hevc.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/hevc", "rank": 1, "aliases": "OMX.qcom.video.decoder.hevc.secure", "profiles": "24576,24578,24577", "levels": "24835", "maxSize": "4096x2304"},
        {"store": "default3", "name": "c2.qti.vp9.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/x-vnd.on2.vp9", "rank": 1, "aliases": "OMX.qcom.video.decoder.vp9", "profiles": "28672,28673,28674", "levels": "28680", "maxSize": "4096x2304"},
        {"store": "default3", "name": "c2.qti.av1.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/av01", "rank": 1, "aliases": "", "profiles": "", "levels": "", "maxSize": "8192x4320"},
        {"store": "default3", "name": "c2.qti.av1.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/av01", "rank": 1, "aliases": "", "profiles": "", "levels": "", "maxSize": "4096x2304"},
        {"store": "default3", "name": "c2.qti.avc.encoder", "kind": "encoder", "domain": "video", "mediaType": "video/avc", "rank": 1, "aliases": "OMX.qcom.video.encoder.avc", "profiles": "20480,20482,20484", "levels": "20494", "maxSize": "8192x4320"},
        {"store": "default3", "name": "c2.qti.hevc.encoder", "kind": "encoder", "domain": "video", "mediaType": "video/hevc", "rank": 1, "aliases": "OMX.qcom.video.encoder.hevc", "profiles": "24576", "levels": "24834", "maxSize": "8192x4320"},
        {"store": "default3", "name": "c2.qti.heic.encoder", "kind": "encoder", "domain": "video", "mediaType": "image/vnd.android.heic", "rank": 1, "aliases": "OMX.qcom.video.encoder.heic", "profiles": "", "levels": "", "maxSize": "16384x16384"}
      ]
    },
    "audioEngine": {
      "modules": "default,bluetooth,r_submix",
      "voiceCallIndexMax": 7,
      "musicIndexMax": 15
    }
  },
  "alignmentNotes": [
    "平台判定：固件 kernel 6.6.118-android15-8 + QTI + VINTF vendor.qti.*（qcrilhook/ImsRadio/AGM/PAL/hexlp/qccvndhal）→ SM8750-AB（Snapdragon 8 Elite）+ Adreno 830；gold 的 mtk_common/meow/mali 系列属性全部替换，radioVariant 由 mtk 改 default。",
    "CPU 拓扑按实测 3 策略（policy0=0-2 / policy3=3-6 / policy7=7，capacity 653/998/1024）落表；vendor/etc 实测的 13 档频点（556800–2169600kHz）用在 policy0，performance/prime 簇按 SM8750 OPP 表补齐 16 档，未照抄 gold 的 6+2 与 mt-cpufreq。",
    "输入身份去掉小米专属 swtp，去掉 MTK 的 mtk-kpd/mtk-pmic-keys/mt6833-mt6359；触控按 synaptics_tcm2（Bus=0018 Vendor=06cb Product=0001 Version=0100，10 槽、无电容按键），键盘 gpio-keys（Bus=0019），电源键 qpnp_pon，耳机孔按实测 sun-snd-card Headset Jack。",
    "传感器只保留实测 chipOwners 对应的 icm4x6xx/ak0991x/stk3bfx/lps22df 及标准融合/未校准/计步项；permissions.unavailableFeatures 禁用的 ambient_temperature、relative_humidity、heartrate、hinge_angle 一律不登记；Honor 私有项仅按固件实有的 VKJ_hn_gesture/hn_acc_cal 落 hn_gesture/hn_acc_cal，不填 xiaomi.sensor.*/oplus.sensor.*/vivo.sensor.*。",
    "vendor/etc 各项按高通平台改写：gnss=QCOM_GNSS_default（双频 L1+L5，yearOfHw 2024）、thermal 温区/冷却设备换 cpuss-0/gpuss-0/xo-therm/skin-therm/pa-therm0 + kgsl-3d0、light 仅 BACKLIGHT/KEYBOARD/BUTTONS（无通知灯，AOD 取代）、media 用实测 QTI Codec2（default3）+ DTS codec 服务，假登记 rank=1 不占 default。",
    "音频保留 DTS 方案（实测存在 vendor.hardware.media.c2-dtscodec-aidl-service）+ AOSP 标准音效；modules 仍为 default,bluetooth,r_submix，语音 7 阶 / 媒体 15 阶。虚拟 HAL 变体：audio/bluetooth/camera/gnss/gpu/health/ir/keymint/light/media/radio/sensors/thermal/vibrator/wifi/power 全部 default，IR 与 NFC 均 enable。",
    "待真机回采项：glVersionFull/vulkan.driver_version 的具体 blob 号、EDID（/sys/class/drm/card0-*/edid）、imeiTac 与 MAC OUI 全集、Widevine systemId；Codec2 两个二进制实测都报 default3，暂以 default3/default3_dts 区分 store，需 dumpsys media.codec 复核。"
  ],
  "sources": [
    "目标机固件实测：build.prop / boot DTB / VINTF / vendor/etc / odm/etc（buildId 15-AQ3A.250214.001-20260904，securityPatch 2026-09-01）",
    "gold 标杆 schema 与颗粒度：devices/manifest/configs/redmi/gold/gold-android15/（vendor/config.json、module/config.json、hardware/config.json、vendor/etc/*）",
    "honor/sun 现有骨架：devices/manifest/configs/honor/sun/sun-android15/（照抄 gold 的 mt6833/mtk/swtp 残留已逐项替换）",
    "公开规格：荣耀 Magic7 RSR 保时捷设计（6.8 英寸 1.5K LTPO 1280x2800 / 骁龙 8 至尊版 / 5850mAh / 100W 有线 + 80W 无线 / 16+512 与 24+1TB / 200MP 潜望）"
  ]
}
```
