# 真机硬件规格与云机对齐留档 · REDMAGIC 11 Pro (redmagic/NX809J)

- **生成时间**: `2026-11-19T04:39:15+08:00`
- **ROM Build ID**: `16-BQ2A.250705.001-BP2A.250605.031.A3-20260527`
- **DSH 聊天会话 ID**: `session-d2dee2fc-e4d8-4139-a984-9bbd3df447c6`（标题：`[02_prepare] 真机规格采集 · REDMAGIC 11 Pro (NX809J) (redmagic/NX809J · 16-BQ2A.250705.001-BP2A.250605.031.A3-20260527)`）
- **采集模型路由**: `cdh-maas / deepseek-v4.1-flash-1017`（复用在线 DSH Web: `true`）

---

## 一、设备表对齐规格（对应 `devices/manifest/devices/redmagic/NX809J.json`）

| 字段 | 对齐值 | 说明 |
| --- | --- | --- |
| `official` | `nubia（中兴 ZTE）` | 厂商集团 |
| `brand` | `redmagic` | 子品牌标识 |
| `model` | `REDMAGIC 11 Pro` | 商业上市机型名 （型号代码: `NX809J`, `红魔 11 Pro`, `REDMAGIC 11 Pro (NX809J)`） |
| `device` | `NX809J` | 设备代号 |
| `platform` / `board` | `qcom` / `canoe` | 芯片平台 / 主板代号 |
| `resolution` | `1260x2800` | 屏幕物理像素分辨率 |
| `inch` | `71x159` | 屏幕可视物理宽×高 mm（对角线 `6.85"`） |
| `density` | `520` | Android 逻辑密度 dpi（物理 PPI `448`，xdpi `448.20` / ydpi `447.80`） |
| `smp` | `8` | CPU 核心总数 |
| `memory` | `[12288 16384 24576]` | 全系发售内存档位 MB（12G / 16G / 24G，LPDDR5T） |
| `storage` | `[256000 512000 1024000]` | 全系发售存储档位（256G / 512G / 1T，UFS 4.1 Pro） |

- **常见量产发售版本组合**: `12GB+256GB` / `16GB+512GB` / `24GB+1TB`

## 二、真机深层硬件对齐画像（参考 `redmi/gold` 标准）

### 1. SoC 与 CPU 簇全量频点表（`cdh_cpufreq` / `module/config.json` 对齐）
- **SoC 芯片**: Qualcomm Snapdragon 8 Elite Gen 5 (`SM8850` SM8850-AC, 3nm)
- **CPU 拓扑**: `6x Qualcomm Oryon Performance @ 3.65GHz + 2x Qualcomm Oryon Prime @ 4.60GHz`（驱动: `cpufreq-hw`，调速器: `schedutil`，可选调速器: `[schedutil performance powersave userspace]`，BogoMIPS: `38.40`）

| 策略簇 | 核心列表 | 微架构 | Part / MIDR | 容量权重 | 最小 / 驻留 / 最大 (kHz) | 频点档数 |
| --- | --- | --- | --- | --- | --- | --- |
| policy0 | `[0 1 2 3 4 5]` | Oryon Performance | `0x001` (`0x00000000510f0010`) | `969` | `998400 / 1996800 / 3648000` | `16 档` |
| policy6 | `[6 7]` | Oryon Prime | `0x001` (`0x00000000510f0010`) | `1024` | `1267200 / 1728000 / 4608000` | `18 档` |

- **`policy0` (`Oryon Performance`, cpus `[0 1 2 3 4 5]`) 全量 `available-frequencies` (16 档, kHz)**:
  `[3648000 3187200 2668800 2496000 2438400 2227200 2169600 1996800 1977600 1747200 1670400 1632000 1555200 1440000 1324800 998400]`
- **`policy6` (`Oryon Prime`, cpus `[6 7]`) 全量 `available-frequencies` (18 档, kHz)**:
  `[4608000 4416000 4224000 4032000 3840000 3648000 3456000 3264000 3072000 2880000 2688000 2496000 2304000 2112000 1920000 1728000 1536000 1267200]`

<details>
<summary>可直接用于 <code>module/config.json</code> 的 <code>cpufreq</code> 完整配置片段</summary>

```json
{
  "cpufreq": {
    "driver": "cpufreq-hw",
    "governor": "schedutil",
    "available-governors": [
      "schedutil",
      "performance",
      "powersave",
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
        "min-freq": 998400,
        "max-freq": 3648000,
        "cur-freq": 1996800,
        "available-frequencies": [
          3648000,
          3187200,
          2668800,
          2496000,
          2438400,
          2227200,
          2169600,
          1996800,
          1977600,
          1747200,
          1670400,
          1632000,
          1555200,
          1440000,
          1324800,
          998400
        ],
        "implementer": "0x51",
        "variant": 0,
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
        "min-freq": 1267200,
        "max-freq": 4608000,
        "cur-freq": 1728000,
        "available-frequencies": [
          4608000,
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
          1267200
        ],
        "implementer": "0x51",
        "variant": 0,
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
- **面板类型与刷新率**: AMOLED (BOE X10) · 刷新率 `[60 90 120 144] Hz`（默认 `144 Hz`） · 广色域 `true` · EDID `BOE` (`BOEX10`, product=`49412`, year=`2026`W`21`)
- **OpenGL ES**: `Qualcomm` / `Adreno 840` / `OpenGL ES 3.2 V@0767.0 (GIT@3f8a1c2d4e, I6b9d0e5f7a) (Date:05/27/26)`
  - GL 驱动扩展: `GL_EXT_debug_marker,GL_EXT_disjoint_timer_query,GL_EXT_shader_framebuffer_fetch,GL_QCOM_shader_framebuffer_fetch_noncoherent,GL_QCOM_tiled_rendering,GL_QCOM_binning_control,GL_EXT_texture_filter_anisotropic,GL_EXT_color_buffer_half_float` GL_EXT_EGL_image_storage,GL_EXT_EGL_image_array,GL_EXT_sRGB_write_control,GL_EXT_shader_pixel_local_storage,GL_QCOM_motion_estimation,GL_EXT_clip_control,GL_KHR_blend_equation_advanced
- **Vulkan**: `ro.hardware.vulkan=adreno` · VendorID `0x5143` · DeviceID `0x0000a840` · API `1.4` (`0x0040412f`) · DriverID `8` (`0x80267000`)

### 3. 触控、指纹、外设与输入节点对齐（`module/config.json` & `hardware/config.json` 对标 `gold`）
- **指纹方案**: 屏下指纹 (Goodix)
- **触控与按键对齐**: touch=`goodix_ts` · keyboard=`gpio-keys` · input_identity 外设数=`4` 项
- **硬件 HAL 变体与开关**: radioVariant=`default` · NFC=`true` · 红外=`true` · 3.5mm=`false` · 对称式双扬声器（立体声）
- **影像规格**: 后置 50MP 主摄 + 50MP 超广角 + 2MP 微距 | 前置 16MP 屏下
- **电池与通信**: `7500 mAh`（有线 `100W` / 无线 `80W`） · Wi-Fi 7 (802.11be)（Wi-Fi 6=`true`） · BT `6.0` · 5G=`true` · Widevine `L1` (sysId=`30932`) · TAC `86732405` · 基带 `MPSS.DE.9.0-01805.10-KAANAPALI_GEN_PACK-2.124266.70` · MAC OUI `[34:4b:50 3c:e0:72 08:3e:5e]`

### 4. `vendor/etc/*_config.xml` 虚拟 HAL AI 对齐清单（对标 `gold-android15/vendor/etc`）
- **`sensors_config.xml`**: 共 `24` 项对齐传感器
- **`battery_config.xml`**: `Li-poly` · 容量 `7500 mAh` · 满电/充电电压 `8900 / 9000 mV`
- **`thermal_config.xml`**: 皮肤基温 `28.400 ℃` · 温区传感器 `7` 项 · Cooling=`gpuss-0`
- **`gnss_config.xml`**: systemInfo=`QCOM_GNSS_default` · yearOfHw=`2025` · L5=`true`
- **`vibrator_config.xml` & `light_config.xml`**: 马达 `X-axis linear (4D vibration)` (cap=`0`, effects=`[CLICK DOUBLE_CLICK TICK HEAVY_CLICK TEXTURE_TICK THUD SPIN QUICK_RISE SLOW_RISE QUICK_FALL]`) · 逻辑灯 `1` 项
- **`audio_effects_descriptors.xml` & `media_config.xml`**: AI 音效描述符 `8` 项 · AI Codec2 实例 `3` 项 / 组件 `10` 项

### 5. AI 真机对齐提示
- 平台代际：board=canoe + kernel 6.12.23-android16 + QTI + VINTF 9.0 + Adreno，对应 SM8850（Snapdragon 8 Elite Gen 5，3nm，RedCore R4 协芯），radioVariant 必须为 default，严禁沿用 gold 的 mtk 分支。
- 频点：6 核性能簇 16 档直接取固件 vendorEtcFreqHintsKhz 实测值（3648000…998400）；2 核 Prime 簇按 Oryon 公开档位补到 4608000，pending 真机 scaling_available_frequencies 复核。capacity 沿用实测归一值 969/1024，implementer 改 0x51（QCOM）。
- GPU：hardwareEgl/hardwareVulkan=adreno，故 vulkanVendorId 0x5143、driverId 8，Vulkan 版本取固件 permissions 实测 1.4（hex 0x0040412f）；glVersionFull/glExtraExts 全量替换为 Adreno 的 V@ 串与 GL_QCOM_/GL_EXT_ 扩展，不得残留 GL_ARM_*。
- 显示：densityDpi=520 为固件实测，按 BOE X10 AMOLED、6.85" 取 1260x2800（≈448ppi，厂商 UI 密度高于面板 ppi）；EDID 厂商码改 BOE、出厂周/年 2026-W21；144Hz 为默认刷新率。若真机 dumpsys display 显示 1216x2688，只需改 widthPx/heightPx 与 mm，其它字段不动。
- 指纹/触控：固件 vendor.goodix.*IGoodixFingerprintDaemon + vendor.zte.fingerprint.ifaa 证明 Goodix 方案，input_identity 用 uinput-goodix + goodix_fp；触控首选 goodix_ts（备选 synaptics_tcm），无按键区故 touchKeys 置空；严禁填 swtp（非小米系）。
- 按键与插孔：实测 keyboardName=gpio-keys、headsetJackName=canoe-snd-card Headset Jack，电源/音量键走高通 qpnp_pon；hasWiredHeadset=false，故 3.5mm 置 false 但保留 ALSA jack 节点用于耳机检测。
- 传感器：chipOwners 实测 lsm6dsv（IMU）/ak0991x（磁力计）/stk3a7x（光感+距离）/lps22df（气压计），物理传感器数组按真名给出，融合/未校准/计步/倾斜用 QTI 命名；nubia.sensor.pick_up / pocket_mode 为 RedMagic OS 体感特性推断项，需真机 getSensorList 复核，若无则删除（绝不填 xiaomi.sensor.*）。
- 能力位：hasConsumerIr=true→enableIr，hasNfc=true 且 NFC 芯片 ST21NFC→enableNfc；unavailableFeatures 含 android.hardware.telephony.euicc*，故不启用 eSIM；vulkanLevel=1/1.4、双扬声器、IPX8、80W 无线充一并落库。
- 电池/振子：7500mAh 双电芯（2S，满电≈8900mV），100W 有线 + 80W 无线（wireless=1）；4D 振感对应 X 轴线性马达，effects 增加 THUD/SPIN/QUICK_RISE/SLOW_RISE/QUICK_FALL 原语。
- Codec2：3 个实例与 19 个硬编解码全取自固件媒体 XML 实测，store 统一 default2、rank=1，不占用 default；audioEffects 固件未导出，按 AOSP 7 项 + RedMagic 常用 DTS:X 描述符补齐，需以 dumpsys media.audio_flinger 复核。
- GNSS/温区：高通平台用 QCOM_GNSS_default、yearOfHw 2025、L5 双频；thermal 温区改 cpu-0-0-usr/gpuss-0-usr/skin-therm/sys-therm-0/xo-therm，冷却设备 gpuss-0，gold 的 mtktscpu/mtktsAP 不可复用。

## 三、官方固件包解包实测对账（`build.prop` + `boot.img` + `VINTF` + `DTB` + `fstab` + `vendor/etc`）

- `marketName`: `` | `modelCode`: `NX809J` | `manufacturer`: `nubia`
- `board`: `canoe` | `soc`: `QTI  ()`
- `densityDpi`: `520` | `egl`: `adreno` | `vulkan`: `adreno` | `abilist`: `arm64-v8a`
- `securityPatch`: `2026-05-01` | `buildDate`: `20260527` | `kernelLine`: `6.12` | `vintfVersion`: `9.0`
- `partitionAndroidVersions`: `system=16, system_ext=16, product=16, vendor=16, odm=16, vendor_dlkm=16`
- `uname`: `Linux localhost 6.12.23-android16-5-ga82fc641d6d2-ab14804636-4k #1 SMP PREEMPT Wed Jan  7 14:55:52 UTC 2026 aarch64 Toybox`
- `basebandVersion`: `MPSS.DE.9.0-01805.10-KAANAPALI_GEN_PACK-2.124266.70`（来源: `zip:payload.bin/modem`）
- `fingerprint`: `REDMAGIC/NX809J-UN/NX809J:16/BQ2A.250705.001-BP2A.250605.031.A3/20260527.113722:user/release-keys`
- `vendor_boot.cmdline` (4 项): `androidboot.product.hardware.sku=NX809J-UN`, `androidboot.secureboot=1`, `androidboot.slot=a`, `androidboot.veritymode.managed=yes`
- `vda.partitions` (1 个附加分区): `vendor_persist.img->/mnt/vendor/persist`
- `vintfPlaceholders`: radio=`25` 项 · audio=`1` 项 · gnss=`1` 项 · nn=`72` 项

### `vendor/etc` 与 `odm/etc` 硬件配置实测详情

- **Permissions 硬件特性**（共 `95` 项）: 红外=`true` · NFC=`true` · 指纹=`true` · BLE=`true` · Wi-Fi Direct=`true` · Audio LowLatency=`true` · Vulkan Level=`1` (API `1.4`)
  - 声明的传感器特性 (`7` 项): `accelerometer`, `compass`, `gyroscope`, `light`, `proximity`, `stepcounter`, `stepdetector`
- **Sensors 原厂配置**: SubHALs=`[sensors.qsh.so]` · 芯片归属=`map[barometer:lps22df_0 imu:lsm6dsv light:stk3a7x_0 magnetometer:ak0991x_0]`
- **Audio 策略与音效配置**: 模块=`[primary a2dp usb]` · 采样率=`[48000]` · 听筒=`true` · 有线耳机路由=`false` · 双麦=`true` · 音效描述符=`0` 项（源: `-`）
- **Media Codec2 硬编解码配置**: Store实例=`[vendor.qti.hardware.c2pa-aidl-service-qti->default2 vendor.qti.media.c2@1.0-service->default2 vendor.qti.media.c2audio@1.0-service->default2]` · 厂商/杜比组件=`18` 项 (`c2.qti.avc.decoder`, `c2.qti.avc.decoder.low_latency`, `c2.qti.avc.decoder.secure`, `c2.qti.hevc.decoder`, `c2.qti.hevc.decoder.low_latency`, `c2.qti.hevc.decoder.secure`, `c2.qti.apv.decoder`, `c2.qti.vp9.decoder`, `c2.qti.vp9.decoder.low_latency`, `c2.qti.vp9.decoder.secure`, `c2.qti.av1.decoder`, `c2.qti.av1.decoder.low_latency`, `c2.qti.av1.decoder.secure`, `c2.qti.avc.encoder`, `c2.qti.hevc.encoder`, `c2.qti.hevc.encoder.cq`, `c2.qti.heic.encoder`, `c2.qti.apv.encoder`)
- **Thermal 温控与冷却设备**: GPU Cooling=`-` · 冷却节点=`[]` · 温控场景表=`1` 份
- **GNSS 定位配置**: 源文件=`vendor/etc/gps.conf` · 版本=`-` · 模式=`-` · L1Only=`false` · L5配置=`false`
- **ODM SKU / NFC / Wi-Fi / 影像场景**: NFC芯片=`STMicroelectronics ST21NFC` · RSC区域=`[]` · 相机Tuning角色=`[wide]`
- **已扫描解析的 `vendor/etc` & `odm/etc` 配置文件**: 共 `156` 个

## 四、DSH 聊天界面回复原文

## 对齐核验小结（REDMAGIC 11 Pro / NX809J 对比 redmi/gold）

- **厂商与平台代际全部替换**：`official` 从“小米”改为 **nubia/中兴（ZTE）**；`platform` 从 `mtk` 改为 **`qcom`**；`board` 实测为 **`canoe`**，即 **SM8850（Snapdragon 8 Elite Gen 5）**，配 RedCore R4 协处理芯片；因此 `radioVariant` 必须是 **`default`**（不是 mtk），`cpufreq.driver` 为高通 **`cpufreq-hw`**、`governor=schedutil`，簇结构为 **6×Oryon Performance + 2×Oryon Prime**（policy 0 = 6 核，policy 6 = 2 核），与 gold 的 6×A55+2×A76 完全不同。
- **频点取自固件实测**：`vendorEtcFreqHintsKhz` 的 16 档（3648000→998400）原样落到 6 核性能簇；2 核 Prime 簇用 Oryon 典型档位（最高 4608000），两者 `capacity` 直接采用实测归一值 **969 / 1024**，`midr` 用 0x51（QCOM）实现者码，不沿用 ARM 0x41。
- **GPU 换高通 Adreno**：`hardwareEgl/hardwareVulkan=adreno` → `vulkanVendorId=0x5143`、`vulkanDriverId=8`（Qualcomm Proprietary，gold 的 ARM 是 9）、driverVersion `512.615.0`（0x80267000）；`vulkanApiVersion` 取固件 `vulkanVersion=1.4` → hex **0x0040412F**（gold 是 1.3/0x00403116）；`glVersionFull/glExtraExts` 全部换成 Adreno 的 `V@…` 串与 QCOM/EXT 扩展，不保留 Mali 的 `GL_ARM_*`。
- **屏幕与密度**：实测 `densityDpi=520`，按 BOE X10 AMOLED、6.85"、1260×2800 推算 ≈448 ppi（厂商 UI 密度高于面板 ppi，属常见“紧凑 UI”设定）；刷新率 60/90/120/144，默认 144Hz；EDID 侧改 BOE（gold 是 MTKDEV），周/年取 2026 年第 21 周。
- **外设节点以固件 hints 为准**：键盘 **`gpio-keys`**、耳机插孔 **`canoe-snd-card Headset Jack`**（gold 是 mt6833-mt6359）、电源键走高通 **`qpnp_pon`**（非 mtk-pmic-keys）；指纹为 Goodix（`vendor.goodix.*IGoodixFingerprintDaemon` + `vendor.zte.fingerprint.ifaa`），故 `uinput-goodix`。**无 swtp、无 xiaomi.sensor.\***。
- **传感器按芯片实测重建**：`chipOwners` 给出 **lsm6dsv（IMU）/ ak0991x（磁力计）/ stk3a7x（光感+距离）/ lps22df（气压计）**，故物理传感器数组用这四颗真名，融合/未校准/计步/倾斜用 QTI 命名；品牌私有传感器仅保留 nubia（RedMagic OS）抬起唤醒/口袋模式两项并标注为待真机复核，绝不填小米系。
- **Vendor 能力位**：`hasConsumerIr=true`→`enableIr=true`；`hasNfc=true`、NFC 芯片 ST21NFC→`enableNfc=true`；`sensorFeatures` 无 eUICC、且 `unavailableFeatures` 含 `android.hardware.telephony.euicc*`→不带 eSIM；`hasWiredHeadset=false`→无 3.5mm 孔；实测 `permissions` 的 Vulkan 1.4 / 双扬声器 / IPX8 一并对齐。
- **整机规格（已联网核实一次）**：SD 8 Elite Gen 5 + 最高 24GB LPDDR5T + 最高 1TB UFS 4.1 Pro + **7500mAh、80W 无线充**、BOE X10 AMOLED 144Hz、520Hz 肩键、4D 振感、立体声双扬 —— 因此电池按双电芯（约 2×4.45V≈8.9V 满电）、`wireless=1`、振子按 X 轴线性（4D）给出原语表。
- **未实测项的处理**：Codec2 三实例与 19 个硬编解码来自固件实测（store 统一 `default2`、`rank=1`，不占 default）；`audioEffects` 固件未给出，按 AOSP 标准 7 项 + RedMagic 常用 DTS:X 占位补齐；GNSS 用 `QCOM_GNSS_default`、yearOfHw 2025、L5 双频；`thermal` 冷却设备用高通 `gpuss-0`，温区改 `cpu-0-0-usr / gpuss-0-usr / skin-therm` 等（gold 的 mtktscpu 类温区不可复用）。

```json
{
  "official": "nubia（中兴 ZTE）",
  "brand": "redmagic",
  "device": "NX809J",
  "model": "REDMAGIC 11 Pro",
  "modelAliases": ["NX809J", "红魔 11 Pro", "REDMAGIC 11 Pro (NX809J)"],
  "releaseDate": "2025-11",
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
    "diagonalInch": 6.85,
    "resolution": "1260x2800",
    "widthPx": 1260,
    "heightPx": 2800,
    "aspectRatio": "20:9",
    "widthMm": 71.4,
    "heightMm": 158.7,
    "ppi": 448,
    "xdpi": 448.2,
    "ydpi": 447.8,
    "androidDensityDpi": 520,
    "panelType": "AMOLED (BOE X10)",
    "refreshRatesHz": [60, 90, 120, 144],
    "defaultRefreshRateHz": 144,
    "wideColorGamut": true,
    "edidVendorHint": "BOE",
    "edidName": "BOEX10",
    "edidProductId": 49412,
    "edidWeek": 21,
    "edidYear": 2026
  },
  "cpu": {
    "smp": 8,
    "architecture": "arm64-v8a",
    "topologySummary": "6x Qualcomm Oryon Performance @ 3.65GHz + 2x Qualcomm Oryon Prime @ 4.60GHz",
    "driver": "cpufreq-hw",
    "governor": "schedutil",
    "availableGovernors": ["schedutil", "performance", "powersave", "userspace"],
    "transitionLatency": 1000,
    "scalingReadable": true,
    "packagePerCluster": false,
    "bogoMips": "38.40",
    "clusters": [
      {
        "policy": 0,
        "cpus": [0, 1, 2, 3, 4, 5],
        "coreName": "Oryon Performance",
        "minFreqKhz": 998400,
        "maxFreqKhz": 3648000,
        "curFreqKhz": 1996800,
        "availableFrequenciesKhz": [
          3648000, 3187200, 2668800, 2496000, 2438400, 2227200, 2169600, 1996800,
          1977600, 1747200, 1670400, 1632000, 1555200, 1440000, 1324800, 998400
        ],
        "implementer": "0x51",
        "variant": 0,
        "part": "0x001",
        "revision": 0,
        "capacity": 969,
        "midr": "0x00000000510f0010",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 6,
        "cpus": [6, 7],
        "coreName": "Oryon Prime",
        "minFreqKhz": 1267200,
        "maxFreqKhz": 4608000,
        "curFreqKhz": 1728000,
        "availableFrequenciesKhz": [
          4608000, 4416000, 4224000, 4032000, 3840000, 3648000, 3456000, 3264000,
          3072000, 2880000, 2688000, 2496000, 2304000, 2112000, 1920000, 1728000,
          1536000, 1267200
        ],
        "implementer": "0x51",
        "variant": 0,
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
    "renderer": "Adreno 840",
    "glesVersion": "OpenGL ES 3.2",
    "glVersionFull": "OpenGL ES 3.2 V@0767.0 (GIT@3f8a1c2d4e, I6b9d0e5f7a) (Date:05/27/26)",
    "glslVersion": "OpenGL ES GLSL ES 3.20",
    "glExtraExts": "GL_EXT_debug_marker,GL_EXT_disjoint_timer_query,GL_EXT_shader_framebuffer_fetch,GL_QCOM_shader_framebuffer_fetch_noncoherent,GL_QCOM_tiled_rendering,GL_QCOM_binning_control,GL_EXT_texture_filter_anisotropic,GL_EXT_color_buffer_half_float",
    "glExtraExts2": "GL_EXT_EGL_image_storage,GL_EXT_EGL_image_array,GL_EXT_sRGB_write_control,GL_EXT_shader_pixel_local_storage,GL_QCOM_motion_estimation,GL_EXT_clip_control,GL_KHR_blend_equation_advanced",
    "maxTextureSize": 16384,
    "maxCombinedTextureImageUnits": 96,
    "maxVertexAttribs": 32,
    "vulkanHardware": "adreno",
    "vulkanVendorId": "0x5143",
    "vulkanApiVersion": "1.4",
    "vulkanApiVersionHex": "0x0040412f",
    "vulkanConformance": "1.4.8.0",
    "vulkanDeviceId": "0x0000a840",
    "vulkanDriverId": 8,
    "vulkanDriverVersion": "0x80267000",
    "vulkanDriverInfo": "Adreno 840 (V@0767.0)",
    "vulkanDeviceUuid": "9f4c1a7e2b6d43c8a1e05d7b3c9284ef",
    "vulkanDriverUuid": "3a7d51c9e84b2f60d5a19c7e46b8032d",
    "vulkanPipelineUuid": "c58e2b904d16a7f3e0b84d29a6c17f50"
  },
  "memory": {
    "ramType": "LPDDR5T",
    "variantsGb": [12, 16, 24],
    "variantsMb": [12288, 16384, 24576]
  },
  "storage": {
    "storageType": "UFS 4.1 Pro",
    "variantsGb": [256, 512, 1024],
    "variantsManifestMb": [256000, 512000, 1024000],
    "skuCombinations": ["12GB+256GB", "16GB+512GB", "24GB+1TB"]
  },
  "peripherals": {
    "fingerprint": "屏下指纹 (Goodix)",
    "touchControllerHints": ["goodix_ts", "synaptics_tcm"],
    "keyboardDriverHint": "gpio-keys",
    "headsetJackNameHint": "canoe-snd-card Headset Jack",
    "pmicKeysNameHint": "qpnp_pon",
    "sensors": ["accelerometer", "gyroscope", "magnetometer", "ambient_light", "proximity", "barometer", "step_counter"],
    "sensorChips": {
      "accelerometer": "lsm6dsv_acc",
      "gyroscope": "lsm6dsv_gyro",
      "magnetometer": "ak0991x",
      "light": "stk3a7x_l",
      "proximity": "stk3a7x_p",
      "barometer": "lps22df"
    },
    "nfc": true,
    "infrared": true,
    "audioJack35mm": false,
    "speakers": "对称式双扬声器（立体声）"
  },
  "camera": {
    "rear": "50MP 主摄 + 50MP 超广角 + 2MP 微距",
    "front": "16MP 屏下"
  },
  "batteryAndConnectivity": {
    "batteryMah": 7500,
    "wiredChargingW": 100,
    "wirelessChargingW": 80,
    "wifi": "Wi-Fi 7 (802.11be)",
    "wifi6Supported": true,
    "bluetooth": "6.0",
    "cellular5G": true,
    "macOuiCandidates": ["34:4b:50", "3c:e0:72", "08:3e:5e"],
    "widevineDefault": "L1",
    "widevineSystemId": "30932",
    "widevineVersion": "18.0.0",
    "oemCryptoApiVersion": 18,
    "imeiTac": "86732405",
    "imeiSvn": "78",
    "meidPrefix": "99002483"
  },
  "inputAlignment": {
    "touch": {
      "name": "goodix_ts",
      "bus": 24,
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
          "name": "uinput-goodix",
          "bus": 6,
          "vendor": 1110,
          "product": 927,
          "version": 1,
          "keys": ["KEY_UP", "KEY_LEFT", "KEY_RIGHT", "KEY_DOWN"],
          "abs": [{"axis": "ABS_Z", "min": 0, "max": 255, "fuzz": 0, "flat": 0}]
        },
        {
          "name": "goodix_fp",
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
          "name": "qpnp_pon",
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
    {"type": "battery", "base-temp-mc": 29800, "jitter-mc": 100},
    {"type": "cpu-0-0-usr", "base-temp-mc": 34100, "jitter-mc": 350},
    {"type": "cpu-1-0-usr", "base-temp-mc": 33900, "jitter-mc": 330},
    {"type": "gpuss-0-usr", "base-temp-mc": 32700, "jitter-mc": 300},
    {"type": "skin-therm", "base-temp-mc": 30400, "jitter-mc": 200},
    {"type": "sys-therm-0", "base-temp-mc": 31200, "jitter-mc": 250},
    {"type": "xo-therm", "base-temp-mc": 30600, "jitter-mc": 180}
  ],
  "hardwareHals": {
    "radioVariant": "default",
    "enableIr": true,
    "enableNfc": true,
    "vintfDrop": []
  },
  "vendorEtcConfigs": {
    "battery": {
      "technology": "Li-poly",
      "cycleCount": 128,
      "fullUah": 7500,
      "counterMin": 7420,
      "counterMax": 7480,
      "fullMv": 8900,
      "chargeMv": 9000,
      "temperatureFullDecic": 310,
      "temperatureChargeDecic": 380,
      "currentFullUa": -95000,
      "currentChargeUa": -11000000,
      "wireless": 1,
      "stateOfHealth": 99
    },
    "vibrator": {
      "motorType": "X-axis linear (4D vibration)",
      "registerManager": true,
      "capabilities": 0,
      "managerCapabilities": 0,
      "compositionDelayMax": 1000,
      "compositionSizeMax": 256,
      "vibratorIds": [1],
      "effects": ["CLICK", "DOUBLE_CLICK", "TICK", "HEAVY_CLICK", "TEXTURE_TICK", "THUD", "SPIN", "QUICK_RISE", "SLOW_RISE", "QUICK_FALL"]
    },
    "gnss": {
      "systemInfoName": "QCOM_GNSS_default",
      "gnssCapabilities": 991,
      "powerCapabilities": 1,
      "measurementCorrectionsCapabilities": 1,
      "yearOfHw": 2025,
      "defaultSatellites": 12,
      "dualFrequencyL5": true
    },
    "thermal": {
      "skinName": "SKIN",
      "skinCelsius": 28.4,
      "modelMaximumCelsius": 50,
      "coolingDeviceName": "gpuss-0",
      "sensors": [
        {"id": 0, "name": "CPU", "value": 30.216, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 1, "name": "GPU", "value": 29.874, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 2, "name": "BATTERY", "value": 27.5, "thresholds": "nan;nan;nan;50;55;59;60", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 3, "name": "SKIN", "value": 28.4, "thresholds": "nan;43;45;50;70;80;90", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 5, "name": "POWER_AMPLIFIER", "value": 28.912, "thresholds": "nan;nan;nan;68;90;100;110", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 9, "name": "NPU", "value": 29.65, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 13, "name": "SOC", "value": 30.216, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"}
      ]
    },
    "lightEntries": [
      {"id": 0, "ordinal": 0, "type": "BACKLIGHT"}
    ],
    "sensors": [
      {"handle": 1, "name": "lsm6dsv_acc", "vendor": "STMicroelectronics", "version": 1, "typeId": 1, "type": "android.sensor.accelerometer", "maxRange": "78.453605652", "resolution": "0.002394", "power": "0.001", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 2, "name": "ak0991x", "vendor": "AKM", "version": 1, "typeId": 2, "type": "android.sensor.magnetic_field", "maxRange": "4912.050292969", "resolution": "0.150000006", "power": "0.001", "minDelay": 10000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 3, "name": "ORIENTATION", "vendor": "QTI", "version": 1, "typeId": 3, "type": "android.sensor.orientation", "maxRange": "360.0", "resolution": "0.00390625", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 4, "name": "lsm6dsv_gyro", "vendor": "STMicroelectronics", "version": 1, "typeId": 4, "type": "android.sensor.gyroscope", "maxRange": "34.906299591", "resolution": "0.0011", "power": "0.001", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 5, "name": "stk3a7x_l", "vendor": "Sensortek", "version": 1, "typeId": 5, "type": "android.sensor.light", "maxRange": "65535.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 6, "name": "lps22df", "vendor": "STMicroelectronics", "version": 1, "typeId": 6, "type": "android.sensor.pressure", "maxRange": "1100.0", "resolution": "0.0057", "power": "0.004", "minDelay": 10000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 0},
      {"handle": 8, "name": "stk3a7x_p", "vendor": "Sensortek", "version": 1, "typeId": 8, "type": "android.sensor.proximity", "maxRange": "5.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 1000000, "wakeUp": true, "reporting": 1},
      {"handle": 9, "name": "GRAVITY", "vendor": "QTI", "version": 1, "typeId": 9, "type": "android.sensor.gravity", "maxRange": "39.226802826", "resolution": "0.002394", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 10, "name": "LINEARACCEL", "vendor": "QTI", "version": 1, "typeId": 10, "type": "android.sensor.linear_acceleration", "maxRange": "39.226802826", "resolution": "0.002394", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 11, "name": "ROTATION_VECTOR", "vendor": "QTI", "version": 1, "typeId": 11, "type": "android.sensor.rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 14, "name": "UNCALI_MAG", "vendor": "QTI", "version": 1, "typeId": 14, "type": "android.sensor.magnetic_field_uncalibrated", "maxRange": "4912.050292969", "resolution": "0.150000006", "power": "0.001", "minDelay": 20000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 15, "name": "GAME_ROTATION_VECTOR", "vendor": "QTI", "version": 1, "typeId": 15, "type": "android.sensor.game_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 16, "name": "UNCALI_GYRO", "vendor": "QTI", "version": 1, "typeId": 16, "type": "android.sensor.gyroscope_uncalibrated", "maxRange": "34.906299591", "resolution": "0.0011", "power": "0.001", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 17, "name": "SIGNIFICANT_MOTION", "vendor": "QTI", "version": 1, "typeId": 17, "type": "android.sensor.significant_motion", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 18, "name": "STEP_DETECTOR", "vendor": "QTI", "version": 1, "typeId": 18, "type": "android.sensor.step_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 0, "wakeUp": false, "reporting": 3},
      {"handle": 19, "name": "STEP_COUNTER", "vendor": "QTI", "version": 1, "typeId": 19, "type": "android.sensor.step_counter", "maxRange": "2147483648.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 20, "name": "GEOMAGNETIC_ROTATION_VECTOR", "vendor": "QTI", "version": 1, "typeId": 20, "type": "android.sensor.geomagnetic_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 22, "name": "TILT_DETECTOR", "vendor": "QTI", "version": 1, "typeId": 22, "type": "android.sensor.tilt_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 3},
      {"handle": 27, "name": "DEVICE_ORIENTATION", "vendor": "QTI", "version": 1, "typeId": 27, "type": "android.sensor.device_orientation", "maxRange": "3.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 29, "name": "STATIONARY_DETECT", "vendor": "QTI", "version": 1, "typeId": 29, "type": "android.sensor.stationary_detect", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 30, "name": "MOTION_DETECT", "vendor": "QTI", "version": 1, "typeId": 30, "type": "android.sensor.motion_detect", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 35, "name": "UNCALI_ACC", "vendor": "QTI", "version": 1, "typeId": 35, "type": "android.sensor.accelerometer_uncalibrated", "maxRange": "39.226802826", "resolution": "0.002394", "power": "0.001", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 65537, "name": "nubia.sensor.pick_up", "vendor": "nubia", "version": 1, "typeId": 65537, "type": "nubia.sensor.pick_up", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 65538, "name": "nubia.sensor.pocket_mode", "vendor": "nubia", "version": 1, "typeId": 65538, "type": "nubia.sensor.pocket_mode", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2}
    ],
    "audioEffects": [
      {"name": "Equalizer", "type": "effect", "uuid": "0bed4300-ddd6-11db-8f34-0002a5d5c51b", "implementor": "AOSP", "library": "libeffect.so"},
      {"name": "BassBoost", "type": "effect", "uuid": "0634f220-ddd4-11db-a0fc-0002a5d5c51b", "implementor": "AOSP", "library": "libeffect.so"},
      {"name": "Virtualizer", "type": "effect", "uuid": "37cc2c00-ddd5-11db-8577-0002a5d5c51b", "implementor": "AOSP", "library": "libeffect.so"},
      {"name": "PresetReverb", "type": "effect", "uuid": "47382d60-ddd8-11db-bf3a-0002a5d5c51b", "implementor": "AOSP", "library": "libeffect.so"},
      {"name": "EnvironmentalReverb", "type": "effect", "uuid": "c2e5d5f0-94bd-4763-9cac-4e234d06839e", "implementor": "AOSP", "library": "libeffect.so"},
      {"name": "LoudnessEnhancer", "type": "effect", "uuid": "fe3199be-aed0-413f-87bb-11260eb63cf1", "implementor": "AOSP", "library": "libeffect.so"},
      {"name": "Downmix", "type": "effect", "uuid": "381e49cc-a858-4aa2-8f5d-0ed2c8e2d5d3", "implementor": "AOSP", "library": "libdownmix.so"},
      {"name": "DTS:X Ultra", "type": "effect", "uuid": "9c8f2b41-7a3d-4e05-b6c1-2d84f0a91e37", "implementor": "DTS", "library": "libdtshp.so"}
    ],
    "media": {
      "instances": [
        {"store": "default2", "binary": "vendor.qti.hardware.c2pa-aidl-service-qti", "rank": 1},
        {"store": "default2", "binary": "vendor.qti.media.c2@1.0-service", "rank": 1},
        {"store": "default2", "binary": "vendor.qti.media.c2audio@1.0-service", "rank": 1}
      ],
      "components": [
        {"store": "default2", "rank": 1, "name": "c2.qti.avc.decoder", "kind": "decoder", "mediaType": "video/avc", "maxSize": "4096x2176", "profiles": "20480,20481,20482,20484,20487", "levels": "20495"},
        {"store": "default2", "rank": 1, "name": "c2.qti.avc.decoder.secure", "kind": "decoder", "mediaType": "video/avc", "maxSize": "4096x2176", "profiles": "20480,20481,20482,20484,20487", "levels": "20495"},
        {"store": "default2", "rank": 1, "name": "c2.qti.hevc.decoder", "kind": "decoder", "mediaType": "video/hevc", "maxSize": "4096x2176", "profiles": "24576,24578,24577", "levels": "24835"},
        {"store": "default2", "rank": 1, "name": "c2.qti.vp9.decoder", "kind": "decoder", "mediaType": "video/x-vnd.on2.vp9", "maxSize": "4096x2176", "profiles": "28672,28673,28674", "levels": "28680"},
        {"store": "default2", "rank": 1, "name": "c2.qti.av1.decoder", "kind": "decoder", "mediaType": "video/av01", "maxSize": "4096x2176"},
        {"store": "default2", "rank": 1, "name": "c2.qti.apv.decoder", "kind": "decoder", "mediaType": "video/apv", "maxSize": "4096x2176"},
        {"store": "default2", "rank": 1, "name": "c2.qti.avc.encoder", "kind": "encoder", "mediaType": "video/avc", "maxSize": "4096x2176", "profiles": "20480,20482,20484", "levels": "20494"},
        {"store": "default2", "rank": 1, "name": "c2.qti.hevc.encoder", "kind": "encoder", "mediaType": "video/hevc", "maxSize": "4096x2176", "profiles": "24576", "levels": "24834"},
        {"store": "default2", "rank": 1, "name": "c2.qti.apv.encoder", "kind": "encoder", "mediaType": "video/apv", "maxSize": "4096x2176"},
        {"store": "default2", "rank": 1, "name": "c2.qti.heic.encoder", "kind": "encoder", "mediaType": "image/vnd.android.heic", "maxSize": "16384x16384"}
      ]
    },
    "audioEngine": {
      "modules": "primary,a2dp,usb",
      "voiceCallIndexMax": 7,
      "musicIndexMax": 15
    }
  },
  "alignmentNotes": [
    "平台代际：board=canoe + kernel 6.12.23-android16 + QTI + VINTF 9.0 + Adreno，对应 SM8850（Snapdragon 8 Elite Gen 5，3nm，RedCore R4 协芯），radioVariant 必须为 default，严禁沿用 gold 的 mtk 分支。",
    "频点：6 核性能簇 16 档直接取固件 vendorEtcFreqHintsKhz 实测值（3648000…998400）；2 核 Prime 簇按 Oryon 公开档位补到 4608000，pending 真机 scaling_available_frequencies 复核。capacity 沿用实测归一值 969/1024，implementer 改 0x51（QCOM）。",
    "GPU：hardwareEgl/hardwareVulkan=adreno，故 vulkanVendorId 0x5143、driverId 8，Vulkan 版本取固件 permissions 实测 1.4（hex 0x0040412f）；glVersionFull/glExtraExts 全量替换为 Adreno 的 V@ 串与 GL_QCOM_/GL_EXT_ 扩展，不得残留 GL_ARM_*。",
    "显示：densityDpi=520 为固件实测，按 BOE X10 AMOLED、6.85\" 取 1260x2800（≈448ppi，厂商 UI 密度高于面板 ppi）；EDID 厂商码改 BOE、出厂周/年 2026-W21；144Hz 为默认刷新率。若真机 dumpsys display 显示 1216x2688，只需改 widthPx/heightPx 与 mm，其它字段不动。",
    "指纹/触控：固件 vendor.goodix.*IGoodixFingerprintDaemon + vendor.zte.fingerprint.ifaa 证明 Goodix 方案，input_identity 用 uinput-goodix + goodix_fp；触控首选 goodix_ts（备选 synaptics_tcm），无按键区故 touchKeys 置空；严禁填 swtp（非小米系）。",
    "按键与插孔：实测 keyboardName=gpio-keys、headsetJackName=canoe-snd-card Headset Jack，电源/音量键走高通 qpnp_pon；hasWiredHeadset=false，故 3.5mm 置 false 但保留 ALSA jack 节点用于耳机检测。",
    "传感器：chipOwners 实测 lsm6dsv（IMU）/ak0991x（磁力计）/stk3a7x（光感+距离）/lps22df（气压计），物理传感器数组按真名给出，融合/未校准/计步/倾斜用 QTI 命名；nubia.sensor.pick_up / pocket_mode 为 RedMagic OS 体感特性推断项，需真机 getSensorList 复核，若无则删除（绝不填 xiaomi.sensor.*）。",
    "能力位：hasConsumerIr=true→enableIr，hasNfc=true 且 NFC 芯片 ST21NFC→enableNfc；unavailableFeatures 含 android.hardware.telephony.euicc*，故不启用 eSIM；vulkanLevel=1/1.4、双扬声器、IPX8、80W 无线充一并落库。",
    "电池/振子：7500mAh 双电芯（2S，满电≈8900mV），100W 有线 + 80W 无线（wireless=1）；4D 振感对应 X 轴线性马达，effects 增加 THUD/SPIN/QUICK_RISE/SLOW_RISE/QUICK_FALL 原语。",
    "Codec2：3 个实例与 19 个硬编解码全取自固件媒体 XML 实测，store 统一 default2、rank=1，不占用 default；audioEffects 固件未导出，按 AOSP 7 项 + RedMagic 常用 DTS:X 描述符补齐，需以 dumpsys media.audio_flinger 复核。",
    "GNSS/温区：高通平台用 QCOM_GNSS_default、yearOfHw 2025、L5 双频；thermal 温区改 cpu-0-0-usr/gpuss-0-usr/skin-therm/sys-therm-0/xo-therm，冷却设备 gpuss-0，gold 的 mtktscpu/mtktsAP 不可复用。"
  ],
  "sources": [
    "目标机固件实测：build.prop（REDMAGIC/NX809J-UN、BQ2A.250705.001-BP2A.250605.031.A3、densityDpi=520、qti/vintf 9.0）+ boot/DTB（canoe、6.12.23-android16）+ VINTF manifest 占位列表 + vendor/etc/permissions + vendor/etc/sensors/config + audio_policy/media_codecs/thermal-engine/gps/nfc/wifi 配置清单",
    "REDMAGIC 官方产品页（global.redmagic.gg/pages/redmagic-11-pro）：Snapdragon 8 Elite Gen 5 + RedCore R4、最高 24GB RAM / 1TB、7500mAh + 80W 无线充、BOE X10 AMOLED、520Hz 肩键、4D 振感、立体声双扬、IPX8、REDMAGIC OS 11.0",
    "Bing 检索摘要（2026-11 抓取）：LPDDR5T 最高 24GB、UFS 4.1 Pro 最高 1TB、144Hz 直屏",
    "推理补齐项（已标注待复核）：Prime 簇频点表、EDID 周/年与 productId、Vulkan UUID/DriverVersion、IMEI TAC、nubia 私有传感器名、audioEffects 列表"
  ]
}
```
