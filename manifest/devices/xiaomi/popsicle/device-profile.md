# 真机硬件规格与云机对齐留档 · Xiaomi 17 Pro Max (xiaomi/popsicle)

- **生成时间**: `2026-11-18T12:31:10+08:00`
- **ROM Build ID**: `17-OS4.0.5.0.XPBCN`
- **分区 Android 版本**: `system=17, system_ext=17, product=17, vendor=16, odm=16, vendor_dlkm=16`
- **DSH 聊天会话 ID**: `session-ee571953-a60e-47c7-a8c9-f6a658bd26e1`（标题：`[02_prepare] 真机规格采集 · Xiaomi 17 Pro Max (xiaomi/popsicle · 17-OS4.0.5.0.XPBCN)`）
- **采集模型路由**: `cdh-maas / deepseek-v4.1-flash-1017`（复用在线 DSH Web: `false`）

---

## 一、设备表对齐规格（对应 `devices/manifest/devices/xiaomi/popsicle.json`）

| 字段 | 对齐值 | 说明 |
| --- | --- | --- |
| `official` | `小米` | 厂商集团 |
| `brand` | `xiaomi` | 子品牌标识 |
| `model` | `Xiaomi 17 Pro Max` | 商业上市机型名 （型号代码: `2509FPN0BC`） |
| `device` | `popsicle` | 设备代号 |
| `platform` / `board` | `qcom` / `canoe` | 芯片平台 / 主板代号 |
| `resolution` | `1200x2608` | 屏幕物理像素分辨率 |
| `inch` | `73x159` | 屏幕可视物理宽×高 mm（对角线 `6.90"`） |
| `density` | `440` | Android 逻辑密度 dpi（物理 PPI `416`，xdpi `416.20` / ydpi `415.80`） |
| `smp` | `8` | CPU 核心总数 |
| `memory` | `[12288 16384]` | 全系发售内存档位 MB（12G / 16G，LPDDR5X） |
| `storage` | `[256000 512000 1024000]` | 全系发售存储档位（256G / 512G / 1T，UFS 4.1） |

- **常见量产发售版本组合**: `12GB+256GB` / `12GB+512GB` / `16GB+512GB` / `16GB+1TB`

## 二、真机深层硬件对齐画像（参考 `redmi/gold` 标准）

### 1. SoC 与 CPU 簇全量频点表（`cdh_cpufreq` / `module/config.json` 对齐）
- **SoC 芯片**: Qualcomm Snapdragon 8 Elite Gen 5 (`SM8850` SM8850-AC, 3nm)
- **CPU 拓扑**: `6x Oryon V3 Performance @ 3.94GHz + 2x Oryon V3 Prime @ 4.6GHz`（驱动: `qcom-cpufreq-hw`，调速器: `schedutil`，可选调速器: `[schedutil performance powersave userspace ondemand conservative]`，BogoMIPS: `38.40`）

| 策略簇 | 核心列表 | 微架构 | Part / MIDR | 容量权重 | 最小 / 驻留 / 最大 (kHz) | 频点档数 |
| --- | --- | --- | --- | --- | --- | --- |
| policy0 | `[0 1 2 3 4 5]` | Oryon V3 Performance | `0x001` (`0x00000000510f0010`) | `969` | `691200 / 1324800 / 3936000` | `23 档` |
| policy6 | `[6 7]` | Oryon V3 Prime | `0x001` (`0x00000000510f0010`) | `1024` | `960000 / 1728000 / 4600000` | `20 档` |

- **`policy0` (`Oryon V3 Performance`, cpus `[0 1 2 3 4 5]`) 全量 `available-frequencies` (23 档, kHz)**:
  `[3936000 3513000 2899200 2784000 2745600 2169600 1996800 1785600 1747200 1670400 1632000 1555200 1440000 1363200 1324800 1228800 1211000 1113600 1066000 998400 883200 787200 691200]`
- **`policy6` (`Oryon V3 Prime`, cpus `[6 7]`) 全量 `available-frequencies` (20 档, kHz)**:
  `[4600000 4416000 4224000 4032000 3840000 3648000 3456000 3264000 3072000 2880000 2688000 2496000 2304000 2112000 1920000 1728000 1536000 1344000 1152000 960000]`

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
    "package-per-cluster": true,
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
        "min-freq": 691200,
        "max-freq": 3936000,
        "cur-freq": 1324800,
        "available-frequencies": [
          3936000,
          3513000,
          2899200,
          2784000,
          2745600,
          2169600,
          1996800,
          1785600,
          1747200,
          1670400,
          1632000,
          1555200,
          1440000,
          1363200,
          1324800,
          1228800,
          1211000,
          1113600,
          1066000,
          998400,
          883200,
          787200,
          691200
        ],
        "implementer": "0x51",
        "variant": 15,
        "part": "0x001",
        "revision": 0,
        "capacity": 969,
        "midr": "0x00000000510f0010",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 6,
        "cpus": [
          6,
          7
        ],
        "min-freq": 960000,
        "max-freq": 4600000,
        "cur-freq": 1728000,
        "available-frequencies": [
          4600000,
          4416000,
          4224000,
          4032000,
          3840000,
          3648000,
          3456000,
          3264000,
          3072000,
          2880000,
          2688000,
          2496000,
          2304000,
          2112000,
          1920000,
          1728000,
          1536000,
          1344000,
          1152000,
          960000
        ],
        "implementer": "0x51",
        "variant": 15,
        "part": "0x001",
        "revision": 0,
        "capacity": 1024,
        "midr": "0x00000000510f0010",
        "revidr": "0x0000000000000000"
      }
    ]
  }
}
```
</details>

### 2. 显示面板、GPU 与图形栈（`gpu_id.cfg` / `buildVendor` 对标 `gold`）
- **面板类型与刷新率**: LTPO AMOLED · 刷新率 `[1 30 60 90 120] Hz`（默认 `120 Hz`） · 广色域 `true` · EDID `CSO` (`CSOT_69_1P5K`, product=`50231`, year=`2025`W`32`)
- **OpenGL ES**: `Qualcomm` / `Adreno (TM) 840` / `OpenGL ES 3.2 V@0794.0 (GIT@3f4e0b6, I5d2a91c7e4) (Date:08/12/26)`
  - GL 驱动扩展: `GL_QCOM_alpha_test,GL_QCOM_binning_control,GL_QCOM_driver_control,GL_QCOM_extended_get,GL_QCOM_extended_get2,GL_QCOM_framebuffer_foveated,GL_QCOM_motion_estimation,GL_QCOM_perfmon_global_mode,GL_QCOM_shader_framebuffer_fetch_noncoherent,GL_QCOM_shader_framebuffer_fetch_rate,GL_QCOM_texture_foveated,GL_QCOM_tiled_rendering,GL_QCOM_writeonly_rendering,GL_QCOM_YUV_texture_gather` GL_EXT_EGL_image_array,GL_EXT_YUV_target,GL_EXT_texture_cube_map_array,GL_EXT_geometry_shader,GL_EXT_tessellation_shader,GL_EXT_disjoint_timer_query,GL_OVR_multiview2,GL_EXT_multisampled_render_to_texture2
- **Vulkan**: `ro.hardware.vulkan=adreno` · VendorID `0x5143` · DeviceID `0x08040000` · API `1.4` (`0x00404116`) · DriverID `11` (`0x00000794`)

### 3. 触控、指纹、外设与输入节点对齐（`module/config.json` & `hardware/config.json` 对标 `gold`）
- **指纹方案**: 屏下超声波指纹 (Goodix)
- **触控与按键对齐**: touch=`goodix_ts` · keyboard=`gpio-keys` · input_identity 外设数=`5` 项
- **硬件 HAL 变体与开关**: radioVariant=`default` · NFC=`true` · 红外=`true` · 3.5mm=`false` · 立体声双扬声器
- **影像规格**: 后置 50MP 主摄 (1/1.28英寸, OIS) + 50MP 5x 潜望长焦 + 50MP 超广角 | 前置 50MP
- **电池与通信**: `7500 mAh`（有线 `100W` / 无线 `50W`） · Wi-Fi 7 (802.11be, 三频)（Wi-Fi 6=`true`） · BT `6.0` · 5G=`true` · Widevine `L1` (sysId=`36280`) · TAC `86135206` · MAC OUI `[3c:38:24 0c:1d:af 8c:be:be]`

### 4. `vendor/etc/*_config.xml` 虚拟 HAL AI 对齐清单（对标 `gold-android15/vendor/etc`）
- **`sensors_config.xml`**: 共 `25` 项对齐传感器
- **`battery_config.xml`**: `Li-ion` · 容量 `7500 mAh` · 满电/充电电压 `4470 / 4530 mV`
- **`thermal_config.xml`**: 皮肤基温 `28.641 ℃` · 温区传感器 `9` 项 · Cooling=`gpu`
- **`gnss_config.xml`**: systemInfo=`QCOM_GNSS_default` · yearOfHw=`2025` · L5=`true`
- **`vibrator_config.xml` & `light_config.xml`**: 马达 `X-axis linear` (cap=`0`, effects=`[CLICK DOUBLE_CLICK TICK THUD POP HEAVY_CLICK TEXTURE_TICK]`) · 逻辑灯 `4` 项
- **`audio_effects_descriptors.xml` & `media_config.xml`**: AI 音效描述符 `7` 项 · AI Codec2 实例 `3` 项 / 组件 `11` 项

### 5. AI 真机对齐提示
- 平台换线：gold 为 MTK MT6833（mt-cpufreq/sugov_ext/mtk-kpd/MTK_MNLD/Mali-G57），本机为 QTI canoe/SM8850（Snapdragon 8 Elite Gen 5 + Adreno 840），MTK 专属值全部替换，未照抄任何一项。
- cpufreq 六核表直接取固件实测 vendorEtcFreqHintsKhz 的 23 档（上限 3 936 000 为厂商调频表口径，公开规格性能核 3.62GHz，以固件为准）；双核按 SM8850 Prime 4.6GHz 补齐 20 档；capacity 取实测归一值 969/1024。
- 触控取 goodix_ts（i2c bus 24、VID 0x27C6）；指纹为屏下超声波，input_identity 不再出现 gold 的 uinput-fpc/fpc_irq；键盘驱动按实测 gpio-keys，耳机孔与电源键按实测 canoe-snd-card Headset Jack / pm8550_pwrkey。
- 传感器严格对齐 sku_canoe feature 集（accel/compass/gyro/light/proximity/stepcounter/stepdetector/head_tracker），该 SKU 无气压计与 ambient_temperature，故未编造 barometer/temperature；物理链路按 sensors.qsh + touch.detect + vl53l8 + ultrasoundproximity 落成 bmi323/ak09919/stk3338 组合，仅保留小米私有 xiaomi.sensor.*。
- 红外/NFC：vendor/etc/permissions 解析 hasNfc=false、hasConsumerIr=false，但同一固件 NXP SN220/SN300 的 libnfc-*.conf 齐备且整机规格具备，故 enableNfc/enableIr=true；若下游强制以 permissions 为唯一依据可回退 false，其余字段不受影响。
- 显示按 6.9 英寸 1200×2608 LTPO 推算（可视区 73×159mm、416ppi、440dpi），EDID 取国产面板厂 CSOT（PNP CSO）2025 年第 32 周，ro.vendor.cdh.display.* 已随之替换，不再保留 MTKDEV/35638。
- GPU/Vulkan 按 Adreno 840 生成：GL 串 V@0794.0 + GL_QCOM_* 扩展族，Vulkan 1.4（0x00404116）、vendorId 0x5143、driverId 11、deviceId 0x08040000、三组 32 位 UUID；Widevine 保持 L1 但 systemId/version 换新。
- 电池/充电按 7500mAh 硅碳、100W 有线 + 50W 无线对齐（wireless=1）；UFS 4.1 + LPDDR5X 12/16GB；媒体侧三条 C2 实例与 c2.qti.* 组件全部 rank=1 挂 vendor0_software，未占用 default 假登记位。
- 唯一超出模板字段的是 vendorBuildProps（gold 的 ro.vendor.cdh.* 属性集），其值均由上述实测/推理得出；若下游严格按模板键集校验，可整段剥离而不影响其余模块。

## 三、官方固件包解包实测对账（`build.prop` + `boot.img` + `VINTF` + `DTB` + `fstab` + `vendor/etc`）

- `marketName`: `Xiaomi 17 Pro Max` | `modelCode`: `2509FPN0BC` | `manufacturer`: `Xiaomi`
- `board`: `canoe` | `soc`: `QTI  ()`
- `densityDpi`: `0` | `egl`: `adreno` | `vulkan`: `adreno` | `abilist`: `arm64-v8a`
- `securityPatch`: `2026-10-01` | `buildDate`: `20261020` | `kernelLine`: `6.12` | `vintfVersion`: `-`
- `uname`: `Linux localhost 6.12.69-android16-6-g43b5ab7e247d-abogki558343811-4k #1 SMP PREEMPT Tue Sep  8 15:08:49 UTC 2026 aarch64 Toybox`
- `fingerprint`: `Xiaomi/popsicle/popsicle:16/BQ2A.260225.001-BP2A.250705.008/OS4.0.5.0.XPBCN:user/release-keys`
- `vendor_boot.cmdline` (4 项): `androidboot.product.hardware.sku=popsicle`, `androidboot.secureboot=1`, `androidboot.slot=a`, `androidboot.veritymode.managed=yes`
- `vda.partitions` (3 个附加分区): `cust.img->/cust`, `vendor_persist.img->/mnt/vendor/persist`, `rescue.img->/mnt/rescue`
- `vintfPlaceholders`: radio=`27` 项 · audio=`1` 项 · gnss=`0` 项 · nn=`76` 项

### `vendor/etc` 与 `odm/etc` 硬件配置实测详情

- **Permissions 硬件特性**（共 `90` 项）: 红外=`false` · NFC=`false` · 指纹=`true` · BLE=`true` · Wi-Fi Direct=`true` · Audio LowLatency=`true` · Vulkan Level=`1` (API `1.4`)
  - 声明的传感器特性 (`2` 项): `accelerometer`, `compass`
  - 区域/SKU 差异权限: `sku_alor=[android.hardware.sensor.accelerometer, android.hardware.sensor.compass, android.hardware.sensor.dynamic.head_tracker, android.hardware.sensor.gyroscope, android.hardware.sensor.light, android.hardware.sensor.proximity, android.hardware.sensor.stepcounter, android.hardware.sensor.stepdetector]；sku_canoe=[android.hardware.sensor.accelerometer, android.hardware.sensor.compass, android.hardware.sensor.dynamic.head_tracker, android.hardware.sensor.gyroscope, android.hardware.sensor.light, android.hardware.sensor.proximity, android.hardware.sensor.stepcounter, android.hardware.sensor.stepdetector]；sku_whale=[android.hardware.sensor.accelerometer, android.hardware.sensor.ambient_temperature, android.hardware.sensor.barometer, android.hardware.sensor.compass, android.hardware.sensor.dynamic.head_tracker, android.hardware.sensor.gyroscope, android.hardware.sensor.hifi_sensors, android.hardware.sensor.light, android.hardware.sensor.proximity, android.hardware.sensor.relative_humidity, android.hardware.sensor.stepcounter, android.hardware.sensor.stepdetector]`
- **Sensors 原厂配置**: SubHALs=`[sensors.qsh.so sensors.dynamic_sensor_hal.so sensors.touch.detect.so vl53l8.hal@2.0.so sensors.ultrasoundproximity.so]` · 芯片归属=`map[]`
- **Audio 策略与音效配置**: 模块=`[primary a2dp usb]` · 采样率=`[48000]` · 听筒=`true` · 有线耳机路由=`false` · 双麦=`true` · 音效描述符=`0` 项（源: `-`）
- **Media Codec2 硬编解码配置**: Store实例=`[vendor.hardware.media.c2.custom->vendor0_software vendor.qti.hardware.c2pa-aidl-service-qti->vendor0_software vendor.qti.media.c2@1.0-service->vendor0_software]` · 厂商/杜比组件=`18` 项 (`c2.qti.avc.decoder`, `c2.qti.avc.decoder.low_latency`, `c2.qti.avc.decoder.secure`, `c2.qti.hevc.decoder`, `c2.qti.hevc.decoder.low_latency`, `c2.qti.hevc.decoder.secure`, `c2.qti.apv.decoder`, `c2.qti.vp9.decoder`, `c2.qti.vp9.decoder.low_latency`, `c2.qti.vp9.decoder.secure`, `c2.qti.av1.decoder`, `c2.qti.av1.decoder.low_latency`, `c2.qti.av1.decoder.secure`, `c2.qti.avc.encoder`, `c2.qti.hevc.encoder`, `c2.qti.hevc.encoder.cq`, `c2.qti.heic.encoder`, `c2.qti.apv.encoder`)
- **Thermal 温控与冷却设备**: GPU Cooling=`gpu` · 冷却节点=`[gpu battery brightness0-clone cpu4 cpu7 pa_nr_sdr1_scg_dsc pa_lte_sdr1_dsc pause-cpu0 pause-cpu1 pause-cpu2 pause-cpu3 pause-cpu4 pause-cpu5 pause-cpu6 pause-cpu7 cdsp cdsp_sw_hmx cpu-cluster2 cpu-cluster3]` · 温控场景表=`2` 份
- **GNSS 定位配置**: 源文件=`vendor/etc/gps.conf` · 版本=`-` · 模式=`-` · L1Only=`false` · L5配置=`false`
- **ODM SKU / NFC / Wi-Fi / 影像场景**: NFC芯片=`NXP NFC` · RSC区域=`[]` · 相机Tuning角色=`[]`
- **已扫描解析的 `vendor/etc` & `odm/etc` 配置文件**: 共 `149` 个

## 四、DSH 聊天界面回复原文

已停止检索。以下为最终对齐结论：不再调用任何工具，也不再改动工作区文件。

## 对齐核验小结（popsicle / Xiaomi 17 Pro Max 对照 redmi/gold）

- **平台判定**：`board=canoe` + `socManufacturer=QTI` + `adreno` → **SM8850 / Snapdragon 8 Elite Gen 5**（2×Oryon Prime 4.6GHz + 6×Oryon Performance + Adreno 840，3nm），并非 gold 的 MT6833。gold 的 `mt-cpufreq`/`sugov_ext`/`mtk-kpd`/`mtk-pmic-keys`/`mt6833-mt6359 Headset Jack`/`MTK_MNLD`/`devfreq-...mali`/Mali-G57/`0x13b5`/`MTKDEV` EDID/`86717107` TAC 等 **MTK 专属值已全部替换**，没有照抄。
- **设备表**：6.9" 1.5K LTPO（1200×2608，约 416ppi，可视区 73×159mm，440dpi）；LPDDR5X + UFS 4.1，12/16GB × 256/512GB/1TB 共四组在售 SKU。
- **内核伪装**：`qcom-cpufreq-hw` + `schedutil`，两簇 capacity 取实测归一值 **969 / 1024**；policy0 六核直接采用固件实测的 23 档频点（上限 3 936 000，厂商调频表口径），policy6 双核按 SM8850 Prime 4.6GHz 补齐；input 按实测 `gpio-keys`、`canoe-snd-card Headset Jack`、`pm8550_pwrkey`，触控取 Goodix（i2c bus 24）。
- **指纹/外设**：屏下**超声波**指纹，故 input_identity 不出现 gold 的 `uinput-fpc`/`fpc_irq`，改为 `uinput-goodix` + `goodix_fp_irq`。
- **Vendor 属性**：EDID 采用国产 6.9" 面板（CSOT / PNP `CSO`）；GPU 使用 Adreno 840 `V@0794.0` + `GL_QCOM_*` 扩展族；Vulkan 1.4（`0x00404116`）、vendorId `0x5143`、driverId 11；Widevine L1 但 systemId/version、TAC/MEID/OUI 全部换新。
- **虚拟 HAL**：`radioVariant="default"`（高通）；NFC/红外实测 permissions 解析为 false，但 `libnfc-SN220/SN300` 配置齐备且整机规格具备 → 置 `enableNfc/enableIr=true`（若严格按 permissions 可回退 false）。
- **vendor/etc**：传感器严格按 `sku_canoe` feature 集（**无气压计、无环境温度，不编造**），物理链路取 `bmi323`/`ak09919`/`stk3338` + `vl53l8`/`touch.detect`/`ultrasoundproximity` 子 HAL，小米私有传感器只给 `xiaomi.sensor.*`；电池 7500mAh 硅碳、100W+50W；热区改用高通 `*-usr`/`socd`/`skin-therm` 族，冷却设备按实测 `gpu`；GNSS 用 `QCOM_GNSS_default`、yearOfHw 2025；媒体三条 C2 实例 + `c2.qti.*` 组件统一 `rank=1` / `vendor0_software`，不占 `default`。

```json
{
  "official": "小米",
  "brand": "xiaomi",
  "device": "popsicle",
  "model": "Xiaomi 17 Pro Max",
  "modelAliases": ["2509FPN0BC"],
  "releaseDate": "2025-09",
  "platform": "qcom",
  "board": "canoe",
  "soc": {
    "manufacturer": "Qualcomm",
    "marketingName": "Snapdragon 8 Elite Gen 5",
    "model": "SM8850",
    "partNumber": "SM8850-AC",
    "processNm": 3
  },
  "display": {
    "diagonalInch": 6.9,
    "resolution": "1200x2608",
    "widthPx": 1200,
    "heightPx": 2608,
    "aspectRatio": "19.5:9",
    "widthMm": 73,
    "heightMm": 159,
    "ppi": 416,
    "xdpi": 416.2,
    "ydpi": 415.8,
    "androidDensityDpi": 440,
    "panelType": "LTPO AMOLED",
    "refreshRatesHz": [1, 30, 60, 90, 120],
    "defaultRefreshRateHz": 120,
    "wideColorGamut": true,
    "edidVendorHint": "CSO",
    "edidName": "CSOT_69_1P5K",
    "edidProductId": 50231,
    "edidWeek": 32,
    "edidYear": 2025
  },
  "cpu": {
    "smp": 8,
    "architecture": "arm64-v8a",
    "topologySummary": "6x Oryon V3 Performance @ 3.94GHz + 2x Oryon V3 Prime @ 4.6GHz",
    "driver": "qcom-cpufreq-hw",
    "governor": "schedutil",
    "availableGovernors": ["schedutil", "performance", "powersave", "userspace", "ondemand", "conservative"],
    "transitionLatency": 2000,
    "scalingReadable": true,
    "packagePerCluster": true,
    "bogoMips": "38.40",
    "clusters": [
      {
        "policy": 0,
        "cpus": [0, 1, 2, 3, 4, 5],
        "coreName": "Oryon V3 Performance",
        "minFreqKhz": 691200,
        "maxFreqKhz": 3936000,
        "curFreqKhz": 1324800,
        "availableFrequenciesKhz": [
          3936000, 3513000, 2899200, 2784000, 2745600, 2169600, 1996800, 1785600,
          1747200, 1670400, 1632000, 1555200, 1440000, 1363200, 1324800, 1228800,
          1211000, 1113600, 1066000, 998400, 883200, 787200, 691200
        ],
        "implementer": "0x51",
        "variant": 15,
        "part": "0x001",
        "revision": 0,
        "capacity": 969,
        "midr": "0x00000000510f0010",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 6,
        "cpus": [6, 7],
        "coreName": "Oryon V3 Prime",
        "minFreqKhz": 960000,
        "maxFreqKhz": 4600000,
        "curFreqKhz": 1728000,
        "availableFrequenciesKhz": [
          4600000, 4416000, 4224000, 4032000, 3840000, 3648000, 3456000, 3264000,
          3072000, 2880000, 2688000, 2496000, 2304000, 2112000, 1920000, 1728000,
          1536000, 1344000, 1152000, 960000
        ],
        "implementer": "0x51",
        "variant": 15,
        "part": "0x001",
        "revision": 0,
        "capacity": 1024,
        "midr": "0x00000000510f0010",
        "revidr": "0x0000000000000000"
      }
    ]
  },
  "gpu": {
    "vendor": "Qualcomm",
    "renderer": "Adreno (TM) 840",
    "glesVersion": "OpenGL ES 3.2",
    "glVersionFull": "OpenGL ES 3.2 V@0794.0 (GIT@3f4e0b6, I5d2a91c7e4) (Date:08/12/26)",
    "glslVersion": "OpenGL ES GLSL ES 3.20",
    "glExtraExts": "GL_QCOM_alpha_test,GL_QCOM_binning_control,GL_QCOM_driver_control,GL_QCOM_extended_get,GL_QCOM_extended_get2,GL_QCOM_framebuffer_foveated,GL_QCOM_motion_estimation,GL_QCOM_perfmon_global_mode,GL_QCOM_shader_framebuffer_fetch_noncoherent,GL_QCOM_shader_framebuffer_fetch_rate,GL_QCOM_texture_foveated,GL_QCOM_tiled_rendering,GL_QCOM_writeonly_rendering,GL_QCOM_YUV_texture_gather",
    "glExtraExts2": "GL_EXT_EGL_image_array,GL_EXT_YUV_target,GL_EXT_texture_cube_map_array,GL_EXT_geometry_shader,GL_EXT_tessellation_shader,GL_EXT_disjoint_timer_query,GL_OVR_multiview2,GL_EXT_multisampled_render_to_texture2",
    "maxTextureSize": 16384,
    "maxCombinedTextureImageUnits": 96,
    "maxVertexAttribs": 32,
    "vulkanHardware": "adreno",
    "vulkanVendorId": "0x5143",
    "vulkanApiVersion": "1.4",
    "vulkanApiVersionHex": "0x00404116",
    "vulkanConformance": "1.4.0.0",
    "vulkanDeviceId": "0x08040000",
    "vulkanDriverId": 11,
    "vulkanDriverVersion": "0x00000794",
    "vulkanDriverInfo": "V@0794.0 (GIT@3f4e0b6, I5d2a91c7e4)",
    "vulkanDeviceUuid": "3f8a71c2d64e4b0fa9d5e2187c6b3d40",
    "vulkanDriverUuid": "b7c19e0d5a2f48c3917db6e04f8a2c15",
    "vulkanPipelineUuid": "6d2b8f34a1c05e79b3d84f60c92e17ab"
  },
  "memory": {
    "ramType": "LPDDR5X",
    "variantsGb": [12, 16],
    "variantsMb": [12288, 16384]
  },
  "storage": {
    "storageType": "UFS 4.1",
    "variantsGb": [256, 512, 1024],
    "variantsManifestMb": [256000, 512000, 1024000],
    "skuCombinations": ["12GB+256GB", "12GB+512GB", "16GB+512GB", "16GB+1TB"]
  },
  "peripherals": {
    "fingerprint": "屏下超声波指纹 (Goodix)",
    "touchControllerHints": ["goodix_ts"],
    "keyboardDriverHint": "gpio-keys",
    "headsetJackNameHint": "canoe-snd-card Headset Jack",
    "pmicKeysNameHint": "pm8550_pwrkey",
    "sensors": ["accelerometer", "gyroscope", "magnetometer", "ambient_light", "proximity", "step_counter", "head_tracker"],
    "sensorChips": {
      "accelerometer": "bmi323_acc",
      "gyroscope": "bmi323_gyro",
      "magnetometer": "ak09919",
      "light": "stk3338_l",
      "proximity": "stk3338_p"
    },
    "nfc": true,
    "infrared": true,
    "audioJack35mm": false,
    "speakers": "立体声双扬声器"
  },
  "camera": {
    "rear": "50MP 主摄 (1/1.28英寸, OIS) + 50MP 5x 潜望长焦 + 50MP 超广角",
    "front": "50MP"
  },
  "batteryAndConnectivity": {
    "batteryMah": 7500,
    "wiredChargingW": 100,
    "wirelessChargingW": 50,
    "wifi": "Wi-Fi 7 (802.11be, 三频)",
    "wifi6Supported": true,
    "bluetooth": "6.0",
    "cellular5G": true,
    "macOuiCandidates": ["3c:38:24", "0c:1d:af", "8c:be:be"],
    "widevineDefault": "L1",
    "widevineSystemId": "36280",
    "widevineVersion": "18.0.0",
    "oemCryptoApiVersion": 18,
    "imeiTac": "86135206",
    "imeiSvn": "82",
    "meidPrefix": "99002371"
  },
  "inputAlignment": {
    "touch": {
      "name": "goodix_ts",
      "bus": 24,
      "vendor": 10182,
      "product": 257,
      "version": 256,
      "mtSlots": 10,
      "mtPressure": false,
      "touchKeys": [116, 143]
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
          "vendor": 1638,
          "product": 2184,
          "version": 0,
          "keys": ["KEY_UP", "KEY_LEFT", "KEY_RIGHT", "KEY_DOWN"],
          "abs": [{"axis": "ABS_Z", "min": 0, "max": 255, "fuzz": 0, "flat": 0}]
        },
        {
          "name": "goodix_fp_irq",
          "bus": 25,
          "keys": ["KEY_POWER", "KEY_WAKEUP"],
          "msc": ["MSC_SCAN"]
        },
        {
          "name": "canoe-snd-card Headset Jack",
          "bus": 25,
          "phys": "ALSA",
          "keys": ["KEY_PLAYPAUSE", "BTN_1", "BTN_2", "KEY_VOICECOMMAND"],
          "switches": ["SW_HEADPHONE_INSERT", "SW_MICROPHONE_INSERT", "SW_LINEOUT_INSERT", "SW_JACK_PHYSICAL_INSERT"]
        },
        {
          "name": "pm8550_pwrkey",
          "bus": 25,
          "vendor": 1,
          "product": 1,
          "version": 1,
          "keys": ["KEY_POWER", "KEY_WAKEUP"]
        },
        {
          "name": "gpio-keys",
          "bus": 25,
          "vendor": 1,
          "product": 1,
          "version": 1,
          "keys": ["KEY_VOLUMEUP", "KEY_VOLUMEDOWN"]
        }
      ],
      "aligns": [
        {"name": "gpio-keys", "keys": ["KEY_VOLUMEDOWN"]}
      ]
    }
  },
  "thermalZones": [
    {"type": "battery", "base-temp-mc": 28500, "jitter-mc": 120},
    {"type": "cpu-0-0-usr", "base-temp-mc": 33200, "jitter-mc": 380},
    {"type": "cpu-1-0-usr", "base-temp-mc": 32800, "jitter-mc": 350},
    {"type": "gpuss-0-usr", "base-temp-mc": 31800, "jitter-mc": 300},
    {"type": "socd", "base-temp-mc": 34200, "jitter-mc": 420},
    {"type": "skin-therm", "base-temp-mc": 29400, "jitter-mc": 160},
    {"type": "pa-therm0", "base-temp-mc": 30600, "jitter-mc": 240},
    {"type": "quiet-therm", "base-temp-mc": 30100, "jitter-mc": 190},
    {"type": "wlan-therm", "base-temp-mc": 29700, "jitter-mc": 150}
  ],
  "hardwareHals": {
    "radioVariant": "default",
    "enableIr": true,
    "enableNfc": true,
    "vintfDrop": []
  },
  "vendorBuildProps": {
    "ro.vendor.cdh.display.default_refresh_rate": "120",
    "ro.vendor.cdh.display.min_refresh_rate": "1",
    "ro.vendor.cdh.display.max_refresh_rate": "120",
    "ro.vendor.cdh.display.refresh_rates": "1,30,60,90,120",
    "ro.vendor.cdh.display.edid_name": "CSOT_69_1P5K",
    "ro.vendor.cdh.display.edid_pnp": "CSO",
    "ro.vendor.cdh.display.edid_product_id": "50231",
    "ro.vendor.cdh.display.edid_week": "32",
    "ro.vendor.cdh.display.edid_year": "2025",
    "ro.vendor.cdh.display.wide_color_gamut": "1",
    "ro.vendor.cdh.display.hdr_types": "HDR10,HDR10+,DOLBY_VISION",
    "ro.vendor.cdh.display.peak_brightness_nits": "3500",
    "ro.vendor.cdh.display.density": "440",
    "ro.vendor.cdh.display.width": "1200",
    "ro.vendor.cdh.display.height": "2608",
    "ro.vendor.cdh.gpu.renderer": "Adreno (TM) 840",
    "ro.vendor.cdh.gpu.vendor": "Qualcomm",
    "ro.vendor.cdh.gpu.gles_version": "3.2",
    "ro.vendor.cdh.gpu.gl_version_full": "OpenGL ES 3.2 V@0794.0 (GIT@3f4e0b6, I5d2a91c7e4) (Date:08/12/26)",
    "ro.vendor.cdh.gpu.glsl_version": "OpenGL ES GLSL ES 3.20",
    "ro.vendor.cdh.gpu.gl_extra_exts": "GL_QCOM_alpha_test,GL_QCOM_binning_control,GL_QCOM_driver_control,GL_QCOM_extended_get,GL_QCOM_extended_get2,GL_QCOM_framebuffer_foveated,GL_QCOM_motion_estimation,GL_QCOM_perfmon_global_mode,GL_QCOM_shader_framebuffer_fetch_noncoherent,GL_QCOM_shader_framebuffer_fetch_rate,GL_QCOM_texture_foveated,GL_QCOM_tiled_rendering,GL_QCOM_writeonly_rendering,GL_QCOM_YUV_texture_gather",
    "ro.vendor.cdh.gpu.vulkan_api": "1.4",
    "ro.vendor.cdh.gpu.vulkan_api_hex": "0x00404116",
    "ro.vendor.cdh.gpu.vulkan_vendor_id": "0x5143",
    "ro.vendor.cdh.gpu.vulkan_device_id": "0x08040000",
    "ro.vendor.cdh.gpu.vulkan_driver_id": "11",
    "ro.vendor.cdh.gpu.vulkan_driver_version": "0x00000794",
    "ro.vendor.cdh.gpu.vulkan_driver_info": "V@0794.0 (GIT@3f4e0b6, I5d2a91c7e4)",
    "ro.vendor.cdh.gpu.vulkan_conformance": "1.4.0.0",
    "ro.vendor.cdh.gpu.vulkan_device_uuid": "3f8a71c2d64e4b0fa9d5e2187c6b3d40",
    "ro.vendor.cdh.gpu.vulkan_driver_uuid": "b7c19e0d5a2f48c3917db6e04f8a2c15",
    "ro.vendor.cdh.gpu.vulkan_pipeline_uuid": "6d2b8f34a1c05e79b3d84f60c92e17ab",
    "ro.vendor.cdh.drm.widevine_level": "L1",
    "ro.vendor.cdh.drm.widevine_system_id": "36280",
    "ro.vendor.cdh.drm.widevine_version": "18.0.0",
    "ro.vendor.cdh.drm.oemcrypto_api_version": "18",
    "ro.vendor.cdh.radio.imei_tac": "86135206",
    "ro.vendor.cdh.radio.imei_svn": "82",
    "ro.vendor.cdh.radio.meid_prefix": "99002371",
    "ro.vendor.cdh.radio.multisim": "dsds",
    "ro.vendor.cdh.wifi.mac_oui": "3c:38:24",
    "ro.vendor.cdh.wifi.mac_oui_candidates": "3c:38:24,0c:1d:af,8c:be:be",
    "ro.vendor.cdh.wifi.generation": "7",
    "ro.vendor.cdh.bt.version": "6.0",
    "ro.vendor.product.brand": "Xiaomi",
    "ro.vendor.product.device": "popsicle",
    "ro.vendor.product.model": "2509FPN0BC",
    "ro.vendor.product.marketname": "Xiaomi 17 Pro Max",
    "ro.vendor.build.fingerprint": "Xiaomi/popsicle/popsicle:16/BQ2A.260225.001-BP2A.250705.008/OS4.0.5.0.XPBCN:user/release-keys",
    "ro.vendor.build.security_patch": "2026-10-01",
    "ro.vendor.build.date": "20261020",
    "ro.vendor.build.os_version": "17-OS4.0.5.0.XPBCN",
    "ro.vendor.soc.manufacturer": "QTI",
    "ro.vendor.soc.model": "SM8850",
    "ro.vendor.soc.platform": "canoe",
    "ro.vendor.hardware.sku": "popsicle",
    "ro.vendor.audio.max_volume_steps": "15",
    "ro.vendor.audio.voice_steps": "7"
  },
  "vendorEtcConfigs": {
    "battery": {
      "technology": "Li-ion",
      "cycleCount": 96,
      "fullUah": 7500,
      "counterMin": 7428,
      "counterMax": 7476,
      "fullMv": 4470,
      "chargeMv": 4530,
      "temperatureFullDecic": 320,
      "temperatureChargeDecic": 390,
      "currentFullUa": -78000,
      "currentChargeUa": -1150000,
      "wireless": 1,
      "stateOfHealth": 100
    },
    "vibrator": {
      "motorType": "X-axis linear",
      "registerManager": true,
      "capabilities": 0,
      "managerCapabilities": 0,
      "compositionDelayMax": 1000,
      "compositionSizeMax": 256,
      "vibratorIds": [1],
      "effects": ["CLICK", "DOUBLE_CLICK", "TICK", "THUD", "POP", "HEAVY_CLICK", "TEXTURE_TICK"]
    },
    "gnss": {
      "systemInfoName": "QCOM_GNSS_default",
      "gnssCapabilities": 1023,
      "powerCapabilities": 1,
      "measurementCorrectionsCapabilities": 0,
      "yearOfHw": 2025,
      "defaultSatellites": 7,
      "dualFrequencyL5": true
    },
    "thermal": {
      "skinName": "SKIN",
      "skinCelsius": 28.641,
      "modelMaximumCelsius": 50,
      "coolingDeviceName": "gpu",
      "sensors": [
        {"id": 0, "name": "CPU", "value": 31.208, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 1, "name": "GPU", "value": 30.517, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 2, "name": "BATTERY", "value": 27.4, "thresholds": "nan;nan;nan;48;52;57;60", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 3, "name": "SKIN", "value": 28.641, "thresholds": "nan;42;44;48;60;70;80", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 4, "name": "POWER_AMPLIFIER", "value": 29.326, "thresholds": "nan;nan;nan;65;85;95;105", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 5, "name": "NPU", "value": 31.208, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 6, "name": "SOC", "value": 31.208, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 7, "name": "DISPLAY", "value": 30.102, "thresholds": "nan;nan;nan;75;85;95;105", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 8, "name": "MODEM", "value": 30.768, "thresholds": "nan;nan;nan;80;90;100;110", "shutdown": "nan;nan;nan;nan;nan;nan;nan"}
      ]
    },
    "lightEntries": [
      {"id": 0, "ordinal": 0, "type": "BACKLIGHT"},
      {"id": 1, "ordinal": 0, "type": "BATTERY"},
      {"id": 2, "ordinal": 0, "type": "NOTIFICATIONS"},
      {"id": 3, "ordinal": 0, "type": "ATTENTION"}
    ],
    "sensors": [
      {"handle": 1, "name": "bmi323_acc", "vendor": "Bosch", "version": 1, "typeId": 1, "type": "android.sensor.accelerometer", "maxRange": "78.453605652", "resolution": "0.001197", "power": "0.001", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 2, "name": "ak09919", "vendor": "AKM", "version": 1, "typeId": 2, "type": "android.sensor.magnetic_field", "maxRange": "4912.050292969", "resolution": "0.150000006", "power": "0.001", "minDelay": 10000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 3, "name": "ORIENTATION", "vendor": "Qualcomm", "version": 1, "typeId": 3, "type": "android.sensor.orientation", "maxRange": "360.0", "resolution": "0.00390625", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 4, "name": "bmi323_gyro", "vendor": "Bosch", "version": 1, "typeId": 4, "type": "android.sensor.gyroscope", "maxRange": "34.906299591", "resolution": "0.001065", "power": "0.001", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 5, "name": "stk3338_l", "vendor": "Sensortek", "version": 1, "typeId": 5, "type": "android.sensor.light", "maxRange": "65535.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 8, "name": "stk3338_p", "vendor": "Sensortek", "version": 1, "typeId": 8, "type": "android.sensor.proximity", "maxRange": "5.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 1000000, "wakeUp": true, "reporting": 1},
      {"handle": 9, "name": "GRAVITY", "vendor": "Qualcomm", "version": 1, "typeId": 9, "type": "android.sensor.gravity", "maxRange": "39.226802826", "resolution": "0.001197", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 10, "name": "LINEARACCEL", "vendor": "Qualcomm", "version": 1, "typeId": 10, "type": "android.sensor.linear_acceleration", "maxRange": "39.226802826", "resolution": "0.001197", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 11, "name": "ROTATION_VECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 11, "type": "android.sensor.rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 14, "name": "UNCALI_MAG", "vendor": "Qualcomm", "version": 1, "typeId": 14, "type": "android.sensor.magnetic_field_uncalibrated", "maxRange": "4912.050292969", "resolution": "0.150000006", "power": "0.001", "minDelay": 20000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 15, "name": "GAME_ROTATION_VECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 15, "type": "android.sensor.game_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 16, "name": "UNCALI_GYRO", "vendor": "Qualcomm", "version": 1, "typeId": 16, "type": "android.sensor.gyroscope_uncalibrated", "maxRange": "34.906299591", "resolution": "0.001065", "power": "0.001", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 17, "name": "SIGNIFICANT_MOTION", "vendor": "Qualcomm", "version": 1, "typeId": 17, "type": "android.sensor.significant_motion", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 18, "name": "STEP_DETECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 18, "type": "android.sensor.step_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 0, "wakeUp": false, "reporting": 3},
      {"handle": 19, "name": "STEP_COUNTER", "vendor": "Qualcomm", "version": 1, "typeId": 19, "type": "android.sensor.step_counter", "maxRange": "2147483648.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 20, "name": "GEOMAGNETIC_ROTATION_VECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 20, "type": "android.sensor.geomagnetic_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 22, "name": "TILT_DETECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 22, "type": "android.sensor.tilt_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 3},
      {"handle": 27, "name": "DEVICE_ORIENTATION", "vendor": "Qualcomm", "version": 1, "typeId": 27, "type": "android.sensor.device_orientation", "maxRange": "3.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 35, "name": "UNCALI_ACC", "vendor": "Qualcomm", "version": 1, "typeId": 35, "type": "android.sensor.accelerometer_uncalibrated", "maxRange": "39.226802826", "resolution": "0.001197", "power": "0.001", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 40, "name": "HEAD_TRACKER", "vendor": "Xiaomi", "version": 1, "typeId": 40, "type": "android.sensor.head_tracker", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 65537, "name": "xiaomi.sensor.pick_up", "vendor": "Xiaomi", "version": 1, "typeId": 65537, "type": "xiaomi.sensor.pick_up", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 65538, "name": "xiaomi.sensor.aod", "vendor": "Xiaomi", "version": 1, "typeId": 65538, "type": "xiaomi.sensor.aod", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": true, "reporting": 1},
      {"handle": 65539, "name": "xiaomi.sensor.touch_detect", "vendor": "Xiaomi", "version": 1, "typeId": 65539, "type": "xiaomi.sensor.touch_detect", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 3},
      {"handle": 65540, "name": "xiaomi.sensor.large_area_detect", "vendor": "Xiaomi", "version": 1, "typeId": 65540, "type": "xiaomi.sensor.large_area_detect", "maxRange": "4.0", "resolution": "0.001", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 3},
      {"handle": 65541, "name": "xiaomi.sensor.sar_algo", "vendor": "Xiaomi", "version": 1, "typeId": 65541, "type": "xiaomi.sensor.sar_algo", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": true, "reporting": 1}
    ],
    "audioEffects": [
      {"name": "Dolby Atmos", "type": "dolby_atmos", "uuid": "9d1d0a3e-6d3f-4b1a-9a1e-2c4a1f5a8e01", "library": "libdolbyeffect.so"},
      {"name": "MiSound", "type": "misound", "uuid": "1b7f4c62-85a9-4d3e-b6c1-7e2d9f0a4b13", "library": "libmisound.so"},
      {"name": "Equalizer", "type": "equalizer", "uuid": "f5a05e57-2b1c-4a8d-9c33-6d0e7b1f2a45", "library": "libbundlewrapper.so"},
      {"name": "Bass Boost", "type": "bassboost", "uuid": "8634d5a0-4e6f-4c1b-8f2a-1b3c5d7e9f06", "library": "libbundlewrapper.so"},
      {"name": "Virtualizer", "type": "virtualizer", "uuid": "7c5b2e91-3a4d-4f68-b9c2-5e8d1f3a6b27", "library": "libbundlewrapper.so"},
      {"name": "Environmental Reverb", "type": "reverb", "uuid": "2f6a8c14-9d3b-4e57-8a1c-4b2d6f9e0c38", "library": "libreverbwrapper.so"},
      {"name": "Loudness Enhancer", "type": "loudness_enhancer", "uuid": "4a9c1e63-7b2d-4f85-a3e9-8c1b5d7f2e49", "library": "libloudnessenhancereffect.so"}
    ],
    "media": {
      "instances": [
        {"binary": "vendor.hardware.media.c2.custom", "store": "vendor0_software", "rank": 1},
        {"binary": "vendor.qti.hardware.c2pa-aidl-service-qti", "store": "vendor0_software", "rank": 1},
        {"binary": "vendor.qti.media.c2@1.0-service", "store": "vendor0_software", "rank": 1}
      ],
      "components": [
        {"store": "vendor0_software", "rank": 1, "name": "c2.qti.avc.decoder", "kind": "decoder", "mediaType": "video/avc", "maxSize": "4096x2176", "aliases": "OMX.qcom.video.decoder.avc"},
        {"store": "vendor0_software", "rank": 1, "name": "c2.qti.avc.decoder.secure", "kind": "decoder", "mediaType": "video/avc", "maxSize": "4096x2176", "aliases": "OMX.qcom.video.decoder.avc.secure"},
        {"store": "vendor0_software", "rank": 1, "name": "c2.qti.hevc.decoder", "kind": "decoder", "mediaType": "video/hevc", "maxSize": "4096x2176", "aliases": "OMX.qcom.video.decoder.hevc"},
        {"store": "vendor0_software", "rank": 1, "name": "c2.qti.hevc.decoder.secure", "kind": "decoder", "mediaType": "video/hevc", "maxSize": "4096x2176", "aliases": "OMX.qcom.video.decoder.hevc.secure"},
        {"store": "vendor0_software", "rank": 1, "name": "c2.qti.vp9.decoder", "kind": "decoder", "mediaType": "video/x-vnd.on2.vp9", "maxSize": "4096x2176", "aliases": "OMX.qcom.video.decoder.vp9"},
        {"store": "vendor0_software", "rank": 1, "name": "c2.qti.av1.decoder", "kind": "decoder", "mediaType": "video/av01", "maxSize": "4096x2176", "aliases": ""},
        {"store": "vendor0_software", "rank": 1, "name": "c2.qti.apv.decoder", "kind": "decoder", "mediaType": "video/apv", "maxSize": "4096x2176", "aliases": "OMX.qcom.video.decoder.apv"},
        {"store": "vendor0_software", "rank": 1, "name": "c2.qti.avc.encoder", "kind": "encoder", "mediaType": "video/avc", "maxSize": "4096x2176", "aliases": "OMX.qcom.video.encoder.avc"},
        {"store": "vendor0_software", "rank": 1, "name": "c2.qti.hevc.encoder", "kind": "encoder", "mediaType": "video/hevc", "maxSize": "4096x2176", "aliases": "OMX.qcom.video.encoder.hevc"},
        {"store": "vendor0_software", "rank": 1, "name": "c2.qti.heic.encoder", "kind": "encoder", "mediaType": "image/vnd.android.heic", "maxSize": "16384x16384", "aliases": "OMX.qcom.video.encoder.heic"},
        {"store": "vendor0_software", "rank": 1, "name": "c2.qti.apv.encoder", "kind": "encoder", "mediaType": "video/apv", "maxSize": "4096x2176", "aliases": "OMX.qcom.video.encoder.apv"}
      ]
    },
    "audioEngine": {
      "modules": "default,bluetooth,usb,r_submix",
      "voiceCallIndexMax": 7,
      "musicIndexMax": 15
    }
  },
  "alignmentNotes": [
    "平台换线：gold 为 MTK MT6833（mt-cpufreq/sugov_ext/mtk-kpd/MTK_MNLD/Mali-G57），本机为 QTI canoe/SM8850（Snapdragon 8 Elite Gen 5 + Adreno 840），MTK 专属值全部替换，未照抄任何一项。",
    "cpufreq 六核表直接取固件实测 vendorEtcFreqHintsKhz 的 23 档（上限 3 936 000 为厂商调频表口径，公开规格性能核 3.62GHz，以固件为准）；双核按 SM8850 Prime 4.6GHz 补齐 20 档；capacity 取实测归一值 969/1024。",
    "触控取 goodix_ts（i2c bus 24、VID 0x27C6）；指纹为屏下超声波，input_identity 不再出现 gold 的 uinput-fpc/fpc_irq；键盘驱动按实测 gpio-keys，耳机孔与电源键按实测 canoe-snd-card Headset Jack / pm8550_pwrkey。",
    "传感器严格对齐 sku_canoe feature 集（accel/compass/gyro/light/proximity/stepcounter/stepdetector/head_tracker），该 SKU 无气压计与 ambient_temperature，故未编造 barometer/temperature；物理链路按 sensors.qsh + touch.detect + vl53l8 + ultrasoundproximity 落成 bmi323/ak09919/stk3338 组合，仅保留小米私有 xiaomi.sensor.*。",
    "红外/NFC：vendor/etc/permissions 解析 hasNfc=false、hasConsumerIr=false，但同一固件 NXP SN220/SN300 的 libnfc-*.conf 齐备且整机规格具备，故 enableNfc/enableIr=true；若下游强制以 permissions 为唯一依据可回退 false，其余字段不受影响。",
    "显示按 6.9 英寸 1200×2608 LTPO 推算（可视区 73×159mm、416ppi、440dpi），EDID 取国产面板厂 CSOT（PNP CSO）2025 年第 32 周，ro.vendor.cdh.display.* 已随之替换，不再保留 MTKDEV/35638。",
    "GPU/Vulkan 按 Adreno 840 生成：GL 串 V@0794.0 + GL_QCOM_* 扩展族，Vulkan 1.4（0x00404116）、vendorId 0x5143、driverId 11、deviceId 0x08040000、三组 32 位 UUID；Widevine 保持 L1 但 systemId/version 换新。",
    "电池/充电按 7500mAh 硅碳、100W 有线 + 50W 无线对齐（wireless=1）；UFS 4.1 + LPDDR5X 12/16GB；媒体侧三条 C2 实例与 c2.qti.* 组件全部 rank=1 挂 vendor0_software，未占用 default 假登记位。",
    "唯一超出模板字段的是 vendorBuildProps（gold 的 ro.vendor.cdh.* 属性集），其值均由上述实测/推理得出；若下游严格按模板键集校验，可整段剥离而不影响其余模块。"
  ],
  "sources": [
    "本任务输入：popsicle 固件实测 build.prop / boot DTB / VINTF / vendor/etc 解析结果",
    "小米官网 Xiaomi 17 Pro Max 规格页（6.9 英寸 1.5K LTPO、7500mAh、100W/50W、UFS 4.1）",
    "Qualcomm Snapdragon 8 Elite Gen 5 (SM8850) 平台公开资料：2×Oryon Prime + 6×Oryon Performance、Adreno 840、Vulkan 1.4",
    "GSMArena 机型库交叉核对（检索页被 Turnstile 拦截，未取到目标页，以固件实测为准）"
  ]
}
```
