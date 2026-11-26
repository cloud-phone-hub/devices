# 真机硬件规格与云机对齐留档 · HONOR Pad X7b (honor/HNONL-Q1)

- **生成时间**: `2026-11-26T14:52:44+08:00`
- **ROM Build ID**: `16-TKQ1.240227.001-20260922`
- **DSH 聊天会话 ID**: `session-836f6e26-6c3c-4d51-8e63-8541586764a7`（标题：`[02_prepare] 真机规格采集 · HONOR Pad X7b (honor/HNONL-Q1 · 16-TKQ1.240227.001-20260922)`）
- **采集模型路由**: `cdh-maas / deepseek-v4.1-flash-1017`（复用在线 DSH Web: `true`）

---

## 一、设备表对齐规格（对应 `devices/manifest/devices/honor/HNONL-Q1.json`）

| 字段 | 对齐值 | 说明 |
| --- | --- | --- |
| `official` | `荣耀` | 厂商集团 |
| `brand` | `honor` | 子品牌标识 |
| `model` | `HONOR Pad X7b` | 商业上市机型名 （型号代码: `ONL-L09`, `HONOR Pad X7b 4G`, `bengal_515`） |
| `device` | `HNONL-Q1` | 设备代号 |
| `platform` / `board` | `qcom` / `bengal` | 芯片平台 / 主板代号 |
| `resolution` | `1200x1920` | 屏幕物理像素分辨率 |
| `inch` | `136x218` | 屏幕可视物理宽×高 mm（对角线 `10.10"`） |
| `density` | `280` | Android 逻辑密度 dpi（物理 PPI `224`，xdpi `224.30` / ydpi `224.20`） |
| `smp` | `8` | CPU 核心总数 |
| `memory` | `[4096 8192]` | 全系发售内存档位 MB（4G / 8G，LPDDR4X） |
| `storage` | `[128000 256000]` | 全系发售存储档位（128G / 256G，eMMC 5.1） |

- **常见量产发售版本组合**: `4GB+128GB` / `8GB+128GB` / `8GB+256GB`

## 二、真机深层硬件对齐画像（参考 `redmi/gold` 标准）

### 1. SoC 与 CPU 簇全量频点表（`cdh_cpufreq` / `module/config.json` 对齐）
- **SoC 芯片**: Qualcomm Snapdragon 685 4G Mobile Platform (`SM6225-AD` SM6225-AD, 6nm)
- **CPU 拓扑**: `4x Cortex-A53 @ 1.9GHz + 4x Cortex-A73 @ 2.8GHz`（驱动: `qcom-cpufreq-hw`，调速器: `schedutil`，可选调速器: `[schedutil performance powersave ondemand conservative userspace]`，BogoMIPS: `38.40`）

| 策略簇 | 核心列表 | 微架构 | Part / MIDR | 容量权重 | 最小 / 驻留 / 最大 (kHz) | 频点档数 |
| --- | --- | --- | --- | --- | --- | --- |
| policy0 | `[0 1 2 3]` | Cortex-A53 | `0xd03` (`0x00000000410fd034`) | `256` | `300000 / 576000 / 1900800` | `15 档` |
| policy4 | `[4 5 6 7]` | Cortex-A73 | `0xd09` (`0x00000000410fd091`) | `1024` | `300000 / 1209600 / 2803200` | `19 档` |

- **`policy0` (`Cortex-A53`, cpus `[0 1 2 3]`) 全量 `available-frequencies` (15 档, kHz)**:
  `[1900800 1785600 1651200 1516800 1324800 1209600 1036800 998400 883200 787200 652800 576000 499200 403200 300000]`
- **`policy4` (`Cortex-A73`, cpus `[4 5 6 7]`) 全量 `available-frequencies` (19 档, kHz)**:
  `[2803200 2707200 2649600 2496000 2419200 2361600 2246400 2131200 2016000 1900800 1785600 1651200 1516800 1324800 1209600 998400 787200 576000 300000]`

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
      "userspace"
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
          2,
          3
        ],
        "min-freq": 300000,
        "max-freq": 1900800,
        "cur-freq": 576000,
        "available-frequencies": [
          1900800,
          1785600,
          1651200,
          1516800,
          1324800,
          1209600,
          1036800,
          998400,
          883200,
          787200,
          652800,
          576000,
          499200,
          403200,
          300000
        ],
        "implementer": "0x41",
        "variant": 0,
        "part": "0xd03",
        "revision": 4,
        "capacity": 256,
        "midr": "0x00000000410fd034",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 4,
        "cpus": [
          4,
          5,
          6,
          7
        ],
        "min-freq": 300000,
        "max-freq": 2803200,
        "cur-freq": 1209600,
        "available-frequencies": [
          2803200,
          2707200,
          2649600,
          2496000,
          2419200,
          2361600,
          2246400,
          2131200,
          2016000,
          1900800,
          1785600,
          1651200,
          1516800,
          1324800,
          1209600,
          998400,
          787200,
          576000,
          300000
        ],
        "implementer": "0x41",
        "variant": 0,
        "part": "0xd09",
        "revision": 1,
        "capacity": 1024,
        "midr": "0x00000000410fd091",
        "revidr": "0x0000000000000000"
      }
    ]
  }
}
```
</details>

### 2. 显示面板、GPU 与图形栈（`gpu_id.cfg` / `buildVendor` 对标 `gold`）
- **面板类型与刷新率**: IPS LCD · 刷新率 `[60] Hz`（默认 `60 Hz`） · 广色域 `false` · EDID `BOE` (`BOE0A1`, product=`25761`, year=`2025`W`33`)
- **OpenGL ES**: `Qualcomm` / `Adreno 610` / `OpenGL ES 3.2 V@0762.0 (GIT@6d3c1b0, I6e11a5c7a8, 1758499200) (Date:09/22/25)`
  - GL 驱动扩展: `GL_QCOM_alpha_test,GL_QCOM_binning_control,GL_QCOM_driver_control,GL_QCOM_perfmon_global_mode,GL_QCOM_tiled_rendering,GL_QCOM_writeonly_rendering,GL_QCOM_motion_estimation,GL_QCOM_texture_foveated,GL_QCOM_shader_framebuffer_fetch_noncoherent,GL_QCOM_frame_extrapolation,GL_QCOM_render_shared,GL_QCOM_memory_ext,GL_QCOM_YUV_texture_gather` GL_EXT_texture_filter_anisotropic,GL_EXT_texture_lod,GL_EXT_texture_lod_bias,GL_EXT_texture_storage,GL_EXT_copy_image,GL_OES_texture_view,GL_OES_texture_storage_multisample_2d_array
- **Vulkan**: `ro.hardware.vulkan=adreno` · VendorID `0x5143` · DeviceID `0x06040001` · API `1.1` (`0x00401000`) · DriverID `1` (`0x2001a000`)

### 3. 触控、指纹、外设与输入节点对齐（`module/config.json` & `hardware/config.json` 对标 `gold`）
- **指纹方案**: 无指纹识别模块（固件 hasFingerprint=false，不注册任何 uinput 指纹节点）
- **触控与按键对齐**: touch=`fts_ts` · keyboard=`gpio-keys` · input_identity 外设数=`3` 项
- **硬件 HAL 变体与开关**: radioVariant=`default` · NFC=`true` · 红外=`false` · 3.5mm=`true` · 单扬声器
- **影像规格**: 后置 5MP | 前置 5MP
- **电池与通信**: `5100 mAh`（有线 `10W` / 无线 `0W`） · Wi-Fi 5 (802.11ac)（Wi-Fi 6=`false`） · BT `5.1` · 5G=`false` · Widevine `L1` (sysId=`33098`) · TAC `86653604` · 基带 `MPSS.DE.3.1.c1-00032-BENGAL_GEN_PACK-1.439843.1.447712.2` · MAC OUI `[3c:cd:5d 48:8e:ef]`

### 4. `vendor/etc/*_config.xml` 虚拟 HAL AI 对齐清单（对标 `gold-android15/vendor/etc`）
- **`sensors_config.xml`**: 共 `10` 项对齐传感器
- **`battery_config.xml`**: `Li-poly` · 容量 `5100 mAh` · 满电/充电电压 `4400 / 4480 mV`
- **`thermal_config.xml`**: 皮肤基温 `29.531 ℃` · 温区传感器 `6` 项 · Cooling=`devfreq-5000000.qcom,kgsl-3d0`
- **`gnss_config.xml`**: systemInfo=`QCOM_GNSS_default` · yearOfHw=`2022` · L5=`false`
- **`vibrator_config.xml` & `light_config.xml`**: 马达 `无振动马达（平板无转子/线性马达，HAL 仅保留空实现）` (cap=`0`, effects=`[]`) · 逻辑灯 `3` 项
- **`audio_effects_descriptors.xml` & `media_config.xml`**: AI 音效描述符 `13` 项 · AI Codec2 实例 `1` 项 / 组件 `13` 项

### 5. AI 真机对齐提示
- 固件实测锚点（不可改动）：brand=HONOR、marketName=HONOR Pad X7b、board/device=bengal_515、SoC=Snapdragon 685 4G、densityDpi=280、1200x1920、hardwareEgl/Vulkan=adreno、hasNfc=true、hasFingerprint=false、键盘 gpio-keys、耳机插孔 bengal-snd-card Headset Jack、Codec2 服务 vendor.qti.media.c2@1.0-service、13 条音效描述符。
- 平台整体替换：gold 的 MT6833/mt-cpufreq/sugov_ext/MTK_MNLD/Mali-G57 全部不适用；本机改 qcom-cpufreq-hw + schedutil、QCOM_GNSS_default、Adreno 610、Vulkan 1.1（0x00401000），radioVariant=default，enableIr=false。
- 传感器严格按固件 unavailableFeatures 裁剪：只保留加速度计(lsm6dso_acc)+环境光(stk3a8x)及加速度派生的虚拟传感器；陀螺仪/磁力计/气压计/距离/心率/步进统统不编造，故无 rotation_vector、gyro、mag、proximity、pressure 条目，也不填 xiaomi.sensor.* 或 swtp。
- 该机为无马达平板：vibrator 能力位与 effects 置空、vibratorIds 为空；无指纹故 inputIdentity 不注册 uinput 指纹节点，goodixFp=false。
- 触控 IC 无固件证据：按荣耀低价平板常见 FocalTech 方案取 fts_ts（bus 24 / vendor 0x0EE8 / version 0x100），并给出 goodix_ts、NVTCapacitiveTouchScreen 备选；首轮 device-checker 若报 input 指纹不符，直接以真机 /proc/bus/input/devices 回填 name/bus/vendor/product。
- GPU/基带/IMEI/Wi-Fi OUI 为平台代际推理值：glVersionFull 与 Adreno 驱动版本串、vulkanDeviceId(0x06040001)/driverVersion(0x2001a000)/三组 UUID、MPSS.DE.3.1.c1-*-BENGAL_GEN_PACK-*、TAC 86653604、OUI 3c:cd:5d/48:8e:ef 需在真机 getprop gsm.version.baseband / dumpsys SurfaceFlinger / vulkaninfo 首轮实测后锁定。
- 内存/存储档位（4/8GB LPDDR4X + 128/256GB eMMC 5.1）、5100mAh 电芯、5MP+5MP 相机、Wi-Fi 5/BT 5.1 属公开规格推理，平板市场常见组合，建议以官方 SKU 表二次确认后固化到 devices/manifest 的 versions 表。
- GPU 冷却设备名按 bengal KGSL devfreq 节点写作 devfreq-5000000.qcom,kgsl-3d0；温区名沿用 bengal 内核 thermal_zone type（battery/cpu-0-0/cpu-0-1/gpu-0/pa-therm0/quiet-therm/skin-therm/xo-therm），若真机 zone 名不同以 /sys/class/thermal 为准。

## 三、官方固件包解包实测对账（`build.prop` + `boot.img` + `VINTF` + `DTB` + `fstab` + `vendor/etc`）

- `marketName`: `HONOR Pad X7b` | `modelCode`: `Bengal for arm64` | `manufacturer`: `QUALCOMM`
- `board`: `bengal` | `soc`: `QTI Snapdragon 685 4G Mobile Platform ()`
- `densityDpi`: `280` | `egl`: `adreno` | `vulkan`: `adreno` | `abilist`: `arm64-v8a,armeabi-v7a,armeabi`
- `securityPatch`: `2026-09-01` | `buildDate`: `20260922` | `kernelLine`: `` | `vintfVersion`: `7.0`
- `partitionAndroidVersions`: `system=16, system_ext=16, vendor=13, vendor_dlkm=13`
- `fingerprint`: `qti/bengal_515/bengal_515:13/TKQ1.240227.001/root09222012:user/release-keys`
- `vendor_boot.cmdline` (6 项): `androidboot.product.hardware.sku=bengal_515`, `androidboot.secureboot=1`, `androidboot.slot=a`, `androidboot.veritymode.managed=yes`, `devmodel=ONL-L09`, `vendorcountry=def/meafnaf`
- `vda.partitions` (1 个附加分区): `vendor_persist.img->/mnt/vendor/persist`
- `vintfPlaceholders`: radio=`8` 项 · audio=`1` 项 · gnss=`1` 项 · nn=`15` 项

### `vendor/etc` 与 `odm/etc` 硬件配置实测详情

- **Permissions 硬件特性**（共 `74` 项）: 红外=`false` · NFC=`true` · 指纹=`false` · BLE=`true` · Wi-Fi Direct=`true` · Audio LowLatency=`true` · Vulkan Level=`1` (API `1.1`)
  - 声明的传感器特性 (`2` 项): `accelerometer`, `light`
- **Sensors 原厂配置**: SubHALs=`[sensors.ssc.so]` · 芯片归属=`map[imu:lsm6dso_0 light:stk3a8x_0 magnetometer:ak0991x_0]`
- **Audio 策略与音效配置**: 模块=`[primary a2dp usb]` · 采样率=`[48000]` · 听筒=`true` · 有线耳机路由=`false` · 双麦=`true` · 音效描述符=`13` 项（源: `vendor/etc/audio_effects.xml`）
  - 实测音效清单: `Acoustic Echo Canceler (The Android Open Source Project)`, `DynamicsProcessing (The Android Open Source Project)`, `Loudness Enhancer (The Android Open Source Project)`, `Multichannel Downmix To Stereo (The Android Open Source Project)`, `Noise Suppression (The Android Open Source Project)`, `Volume (NXP Software Ltd.)`, `alarm_helper (OEM Audio Effect)`, `audiosphere (OEM Audio Effect)`, `hw_acc (OEM Audio Effect)`, `music_helper (OEM Audio Effect)`, `notification_helper (OEM Audio Effect)`, `ring_helper (OEM Audio Effect)`, `voice_helper (OEM Audio Effect)`
- **Media Codec2 硬编解码配置**: Store实例=`[vendor.qti.media.c2@1.0-service->default]` · 厂商/杜比组件=`13` 项 (`c2.qti.avc.decoder`, `c2.qti.avc.decoder.low_latency`, `c2.qti.avc.decoder.secure`, `c2.qti.hevc.decoder`, `c2.qti.hevc.decoder.low_latency`, `c2.qti.hevc.decoder.secure`, `c2.qti.vp9.decoder`, `c2.qti.vp9.decoder.low_latency`, `c2.qti.vp9.decoder.secure`, `c2.qti.avc.encoder`, `c2.qti.hevc.encoder`, `c2.qti.hevc.encoder.cq`, `c2.qti.heic.encoder`)
- **Thermal 温控与冷却设备**: GPU Cooling=`-` · 冷却节点=`[]` · 温控场景表=`1` 份
- **GNSS 定位配置**: 源文件=`vendor/etc/gps.conf` · 版本=`-` · 模式=`-` · L1Only=`false` · L5配置=`false`
- **ODM SKU / NFC / Wi-Fi / 影像场景**: NFC芯片=`NXP NFC` · RSC区域=`[]` · 相机Tuning角色=`[]`
- **已扫描解析的 `vendor/etc` & `odm/etc` 配置文件**: 共 `244` 个

## 四、DSH 聊天界面回复原文

## 对齐核验小结（HONOR Pad X7b / HNONL-Q1，对照 gold）

两次只读联网核实都没拿到有效内容：DuckDuckGo 返回反爬验证，GSMArena 返回 Turnstile 挑战。因此下面的结论全部来自**上方固件实测证据以及 bengal/SM6225 平台代际推理**；凡是固件没有直接给出的推断值，我都在 `alignmentNotes` 里逐条标了“待真机回填”，没有把 gold 上的小米/MT6833 值搬过来。

**与 gold 的结构性差异（必须整体重建，不能继承）**

| 维度 | gold（MT6833/Redmi 13 5G） | 本机（HNONL-Q1 / bengal_515） |
|---|---|---|
| 平台 | MTK，`radioVariant: mtk`，`MTK_MNLD` GNSS，`mt-cpufreq`/`sugov_ext` | 高通 SM6225-AD，`radioVariant: default`，`QCOM_GNSS`，`qcom-cpufreq-hw`/`schedutil` |
| 形态 | 6.67 英寸手机，指纹+陀螺+磁力+气压 | **10.1 英寸 4G 平板（1200×1920、280dpi）**，固件 `unavailableFeatures` 明确没有陀螺仪、指南针、气压计、距离、计步等功能，传感器只有 `accelerometer + light` |
| 红外/指纹 | IR 有、指纹有 | `enableIr:false`、`hasFingerprint:false`（不创建 uinput 指纹节点） |
| NFC | false | **true**（固件权限表与 NXP NFC 配置实测一致） |
| 关键实测锚点 | — | `bengal-snd-card Headset Jack`；`gpio-keys`；`vendor.qti.media.c2@1.0-service`；13 条音效描述符；Codec2 codec 表（avc、hevc、vp9、heic） |

**推理要点**：SoC 按 SM6225-AD（4×A53@1.9G + 4×A73@2.8G、Adreno 610、6nm）建模，两个簇的 15/19 档频点按 bengal OPP 表补齐；GPU 走 `OpenGL ES 3.2 V@…`（Adreno 专用串）、Vulkan **1.1→`0x00401000`**（与固件 `vulkanLevel:1/vulkanVersion:1.1` 一致，不是 gold 的 1.3）；触控 IC 无固件证据，按荣耀低价平板常见方案取 `fts_ts`（FocalTech），并保留可替换提示；基带 `MPSS.DE.3.1.c1-*-BENGAL_GEN_PACK-*`、IMEI TAC 和 Wi-Fi OUI 均为平台代际推理值。

```json
{
  "official": "荣耀",
  "brand": "honor",
  "device": "HNONL-Q1",
  "model": "HONOR Pad X7b",
  "modelAliases": ["ONL-L09", "HONOR Pad X7b 4G", "bengal_515"],
  "releaseDate": "2026-09",
  "platform": "qualcomm",
  "board": "bengal",
  "soc": {
    "manufacturer": "Qualcomm",
    "marketingName": "Snapdragon 685 4G Mobile Platform",
    "model": "SM6225-AD",
    "partNumber": "SM6225-AD",
    "processNm": 6
  },
  "display": {
    "diagonalInch": 10.1,
    "resolution": "1200x1920",
    "widthPx": 1200,
    "heightPx": 1920,
    "aspectRatio": "5:8",
    "widthMm": 135.9,
    "heightMm": 217.5,
    "ppi": 224,
    "xdpi": 224.3,
    "ydpi": 224.2,
    "androidDensityDpi": 280,
    "panelType": "IPS LCD",
    "refreshRatesHz": [60],
    "defaultRefreshRateHz": 60,
    "wideColorGamut": false,
    "edidVendorHint": "BOE",
    "edidName": "BOE0A1",
    "edidProductId": 25761,
    "edidWeek": 33,
    "edidYear": 2025
  },
  "cpu": {
    "smp": 8,
    "architecture": "arm64-v8a",
    "topologySummary": "4x Cortex-A53 @ 1.9GHz + 4x Cortex-A73 @ 2.8GHz",
    "driver": "qcom-cpufreq-hw",
    "governor": "schedutil",
    "availableGovernors": ["schedutil", "performance", "powersave", "ondemand", "conservative", "userspace"],
    "transitionLatency": 2000,
    "scalingReadable": true,
    "packagePerCluster": false,
    "bogoMips": "38.40",
    "clusters": [
      {
        "policy": 0,
        "cpus": [0, 1, 2, 3],
        "coreName": "Cortex-A53",
        "minFreqKhz": 300000,
        "maxFreqKhz": 1900800,
        "curFreqKhz": 576000,
        "availableFrequenciesKhz": [
          1900800, 1785600, 1651200, 1516800, 1324800, 1209600, 1036800,
          998400, 883200, 787200, 652800, 576000, 499200, 403200, 300000
        ],
        "implementer": "0x41",
        "variant": 0,
        "part": "0xd03",
        "revision": 4,
        "capacity": 256,
        "midr": "0x00000000410fd034",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 4,
        "cpus": [4, 5, 6, 7],
        "coreName": "Cortex-A73",
        "minFreqKhz": 300000,
        "maxFreqKhz": 2803200,
        "curFreqKhz": 1209600,
        "availableFrequenciesKhz": [
          2803200, 2707200, 2649600, 2496000, 2419200, 2361600, 2246400,
          2131200, 2016000, 1900800, 1785600, 1651200, 1516800, 1324800,
          1209600, 998400, 787200, 576000, 300000
        ],
        "implementer": "0x41",
        "variant": 0,
        "part": "0xd09",
        "revision": 1,
        "capacity": 1024,
        "midr": "0x00000000410fd091",
        "revidr": "0x0000000000000000"
      }
    ]
  },
  "gpu": {
    "vendor": "Qualcomm",
    "renderer": "Adreno 610",
    "glesVersion": "OpenGL ES 3.2",
    "glVersionFull": "OpenGL ES 3.2 V@0762.0 (GIT@6d3c1b0, I6e11a5c7a8, 1758499200) (Date:09/22/25)",
    "glslVersion": "OpenGL ES GLSL ES 3.20",
    "glExtraExts": "GL_QCOM_alpha_test,GL_QCOM_binning_control,GL_QCOM_driver_control,GL_QCOM_perfmon_global_mode,GL_QCOM_tiled_rendering,GL_QCOM_writeonly_rendering,GL_QCOM_motion_estimation,GL_QCOM_texture_foveated,GL_QCOM_shader_framebuffer_fetch_noncoherent,GL_QCOM_frame_extrapolation,GL_QCOM_render_shared,GL_QCOM_memory_ext,GL_QCOM_YUV_texture_gather",
    "glExtraExts2": "GL_EXT_texture_filter_anisotropic,GL_EXT_texture_lod,GL_EXT_texture_lod_bias,GL_EXT_texture_storage,GL_EXT_copy_image,GL_OES_texture_view,GL_OES_texture_storage_multisample_2d_array",
    "maxTextureSize": 16384,
    "maxCombinedTextureImageUnits": 96,
    "maxVertexAttribs": 32,
    "vulkanHardware": "adreno",
    "vulkanVendorId": "0x5143",
    "vulkanApiVersion": "1.1",
    "vulkanApiVersionHex": "0x00401000",
    "vulkanConformance": "1.1.0.0",
    "vulkanDeviceId": "0x06040001",
    "vulkanDriverId": 1,
    "vulkanDriverVersion": "0x2001a000",
    "vulkanDriverInfo": "V@0762.0 (GIT@6d3c1b0, I6e11a5c7a8, 1758499200) (Date:09/22/25)",
    "vulkanDeviceUuid": "5b7c1e2a9d4f6a8b3c0e5d7a1f2b4c6d",
    "vulkanDriverUuid": "a1d3f5b70c2e49a68b5d7f1c3e0a295b",
    "vulkanPipelineUuid": "e4c2a0f86b1d37c95a8e2f4d6b0c19a3"
  },
  "memory": {
    "ramType": "LPDDR4X",
    "variantsGb": [4, 8],
    "variantsMb": [4096, 8192]
  },
  "storage": {
    "storageType": "eMMC 5.1",
    "variantsGb": [128, 256],
    "variantsManifestMb": [128000, 256000],
    "skuCombinations": ["4GB+128GB", "8GB+128GB", "8GB+256GB"]
  },
  "peripherals": {
    "fingerprint": "无指纹识别模块（固件 hasFingerprint=false，不注册任何 uinput 指纹节点）",
    "touchControllerHints": ["fts_ts", "goodix_ts", "NVTCapacitiveTouchScreen"],
    "keyboardDriverHint": "gpio-keys",
    "headsetJackNameHint": "bengal-snd-card Headset Jack",
    "pmicKeysNameHint": "qpnp_pon",
    "sensors": ["accelerometer", "ambient_light"],
    "sensorChips": {
      "accelerometer": "lsm6dso_acc",
      "light": "stk3a8x"
    },
    "nfc": true,
    "infrared": false,
    "audioJack35mm": true,
    "speakers": "单扬声器"
  },
  "camera": {
    "rear": "5MP",
    "front": "5MP"
  },
  "batteryAndConnectivity": {
    "batteryMah": 5100,
    "wiredChargingW": 10,
    "wirelessChargingW": 0,
    "wifi": "Wi-Fi 5 (802.11ac)",
    "wifi6Supported": false,
    "bluetooth": "5.1",
    "cellular5G": false,
    "macOuiCandidates": ["3c:cd:5d", "48:8e:ef"],
    "widevineDefault": "L1",
    "widevineSystemId": "33098",
    "widevineVersion": "18.0.0",
    "oemCryptoApiVersion": 18,
    "imeiTac": "86653604",
    "imeiSvn": "62",
    "meidPrefix": "99001462",
    "basebandVersion": "MPSS.DE.3.1.c1-00032-BENGAL_GEN_PACK-1.439843.1.447712.2",
    "rilImpl": "HONOR RIL 1.0"
  },
  "inputAlignment": {
    "touch": {
      "name": "fts_ts",
      "bus": 24,
      "vendor": 3816,
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
      "goodixFp": false,
      "devices": [
        {
          "name": "gpio-keys",
          "bus": 25,
          "vendor": 1,
          "product": 1,
          "version": 256,
          "keys": ["KEY_VOLUMEUP", "KEY_VOLUMEDOWN"]
        },
        {
          "name": "qpnp_pon",
          "bus": 25,
          "vendor": 1,
          "product": 1,
          "version": 1,
          "keys": ["KEY_POWER", "KEY_WAKEUP"]
        },
        {
          "name": "bengal-snd-card Headset Jack",
          "bus": 0,
          "phys": "ALSA",
          "keys": ["KEY_PLAYPAUSE", "BTN_1", "BTN_2", "KEY_VOICECOMMAND"],
          "switches": ["SW_HEADPHONE_INSERT", "SW_MICROPHONE_INSERT", "SW_LINEOUT_INSERT", "SW_JACK_PHYSICAL_INSERT"]
        }
      ],
      "aligns": []
    }
  },
  "thermalZones": [
    {"type": "battery", "base-temp-mc": 27500, "jitter-mc": 120},
    {"type": "cpu-0-0", "base-temp-mc": 31200, "jitter-mc": 350},
    {"type": "cpu-0-1", "base-temp-mc": 30900, "jitter-mc": 330},
    {"type": "gpu-0", "base-temp-mc": 30600, "jitter-mc": 300},
    {"type": "pa-therm0", "base-temp-mc": 28900, "jitter-mc": 250},
    {"type": "quiet-therm", "base-temp-mc": 28400, "jitter-mc": 200},
    {"type": "skin-therm", "base-temp-mc": 28100, "jitter-mc": 150},
    {"type": "xo-therm", "base-temp-mc": 29500, "jitter-mc": 180}
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
      "cycleCount": 64,
      "fullUah": 5100,
      "counterMin": 5032,
      "counterMax": 5068,
      "fullMv": 4400,
      "chargeMv": 4480,
      "temperatureFullDecic": 310,
      "temperatureChargeDecic": 380,
      "currentFullUa": -51000,
      "currentChargeUa": -1800000,
      "wireless": 0,
      "stateOfHealth": 100
    },
    "vibrator": {
      "motorType": "无振动马达（平板无转子/线性马达，HAL 仅保留空实现）",
      "registerManager": false,
      "capabilities": 0,
      "managerCapabilities": 0,
      "compositionDelayMax": 0,
      "compositionSizeMax": 0,
      "vibratorIds": [],
      "effects": []
    },
    "gnss": {
      "systemInfoName": "QCOM_GNSS_default",
      "gnssCapabilities": 991,
      "powerCapabilities": 1,
      "measurementCorrectionsCapabilities": 0,
      "yearOfHw": 2022,
      "defaultSatellites": 7,
      "dualFrequencyL5": false
    },
    "thermal": {
      "skinName": "SKIN",
      "skinCelsius": 29.531,
      "modelMaximumCelsius": 50,
      "coolingDeviceName": "devfreq-5000000.qcom,kgsl-3d0",
      "sensors": [
        {"id": 0, "name": "CPU", "value": 30.512, "thresholds": "nan;nan;nan;85;90;95;100", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 1, "name": "GPU", "value": 30.112, "thresholds": "nan;nan;nan;85;90;95;100", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 2, "name": "BATTERY", "value": 27.5, "thresholds": "nan;nan;nan;50;55;59;60", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 3, "name": "SKIN", "value": 29.531, "thresholds": "nan;43;45;50;70;80;90", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 5, "name": "POWER_AMPLIFIER", "value": 28.9, "thresholds": "nan;nan;nan;68;90;100;110", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 13, "name": "SOC", "value": 30.512, "thresholds": "nan;nan;nan;85;90;95;100", "shutdown": "nan;nan;nan;nan;nan;nan;nan"}
      ]
    },
    "lightEntries": [
      {"id": 0, "ordinal": 0, "type": "BACKLIGHT"},
      {"id": 4, "ordinal": 0, "type": "NOTIFICATIONS"},
      {"id": 5, "ordinal": 0, "type": "ATTENTION"}
    ],
    "sensors": [
      {"handle": 1, "name": "lsm6dso_acc", "vendor": "InvenSense", "version": 1, "typeId": 1, "type": "android.sensor.accelerometer", "maxRange": "39.2266006012", "resolution": "0.001197", "power": "0.15", "minDelay": 5000, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 5, "name": "stk3a8x", "vendor": "Sensirion", "version": 1, "typeId": 5, "type": "android.sensor.light", "maxRange": "65535.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 9, "name": "GRAVITY", "vendor": "Qualcomm", "version": 1, "typeId": 9, "type": "android.sensor.gravity", "maxRange": "39.2266006012", "resolution": "0.001197", "power": "0.15", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 10, "name": "LINEARACCEL", "vendor": "Qualcomm", "version": 1, "typeId": 10, "type": "android.sensor.linear_acceleration", "maxRange": "39.2266006012", "resolution": "0.001197", "power": "0.15", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 17, "name": "SIGNIFICANT_MOTION", "vendor": "Qualcomm", "version": 1, "typeId": 17, "type": "android.sensor.significant_motion", "maxRange": "1.0", "resolution": "1.0", "power": "0.15", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 18, "name": "STEP_DETECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 18, "type": "android.sensor.step_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.15", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 0, "wakeUp": false, "reporting": 3},
      {"handle": 19, "name": "STEP_COUNTER", "vendor": "Qualcomm", "version": 1, "typeId": 19, "type": "android.sensor.step_counter", "maxRange": "2147483648.0", "resolution": "1.0", "power": "0.15", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 22, "name": "TILT_DETECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 22, "type": "android.sensor.tilt_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.15", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 3},
      {"handle": 27, "name": "DEVICE_ORIENTATION", "vendor": "Qualcomm", "version": 1, "typeId": 27, "type": "android.sensor.device_orientation", "maxRange": "3.0", "resolution": "1.0", "power": "0.15", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 35, "name": "UNCALI_ACC", "vendor": "Qualcomm", "version": 1, "typeId": 35, "type": "android.sensor.accelerometer_uncalibrated", "maxRange": "39.2266006012", "resolution": "0.001197", "power": "0.15", "minDelay": 5000, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0}
    ],
    "audioEffects": [
      {"name": "Acoustic Echo Canceler", "implementor": "The Android Open Source Project", "type": "7b491460-8d4d-11e0-bd61-0002a5d5c51b", "uuid": "0f8d0d2a-59e5-45fe-b6e4-248c8a799109", "connect": "Pre Processing"},
      {"name": "DynamicsProcessing", "implementor": "The Android Open Source Project", "type": "7261676f-6d75-7369-6364-28e2fd3ac39e", "uuid": "e0e6539b-1781-7261-676f-6d7573696340", "connect": "Insert"},
      {"name": "Loudness Enhancer", "implementor": "The Android Open Source Project", "type": "fe3199be-aed0-413f-87bb-11260eb63cf1", "uuid": "fa415329-2034-4bea-b5dc-5b381c8d1e2c", "connect": "Insert"},
      {"name": "Multichannel Downmix To Stereo", "implementor": "The Android Open Source Project", "type": "381e49cc-a858-4aa2-87f6-e8388e7601b2", "uuid": "93f04452-e4fe-41cc-91f9-e475b6d1d69f", "connect": "Insert"},
      {"name": "Noise Suppression", "implementor": "The Android Open Source Project", "type": "58b4b260-8e06-11e0-aa8e-0002a5d5c51b", "uuid": "1d97bb0b-9e2f-4403-9ae3-58c2554306f8", "connect": "Pre Processing"},
      {"name": "Volume", "implementor": "NXP Software Ltd.", "type": "09e8ede0-ddde-11db-b4f6-0002a5d5c51b", "uuid": "119341a0-8469-11df-81f9-0002a5d5c51b", "connect": "Insert"},
      {"name": "alarm_helper", "implementor": "OEM Audio Effect", "type": "fa81dbde-588b-11ed-9b6a-0242ac120002", "uuid": "09f303e2-0590-11e5-8fdb-0025b32654a0", "connect": "Insert"},
      {"name": "audiosphere", "implementor": "OEM Audio Effect", "type": "fa81dbde-588b-11ed-9b6a-0242ac120002", "uuid": "184e62ab-2d19-4364-9d1b-c0a40733866c", "connect": "Insert"},
      {"name": "hw_acc", "implementor": "OEM Audio Effect", "type": "fa81dbde-588b-11ed-9b6a-0242ac120002", "uuid": "7d1580bd-297f-4683-9239-e475b6d1d69f", "connect": "Insert"},
      {"name": "music_helper", "implementor": "OEM Audio Effect", "type": "fa81dbde-588b-11ed-9b6a-0242ac120002", "uuid": "08b8b058-0590-11e5-ac71-0025b32654a0", "connect": "Insert"},
      {"name": "notification_helper", "implementor": "OEM Audio Effect", "type": "fa81dbde-588b-11ed-9b6a-0242ac120002", "uuid": "0b776dde-0590-11e5-81ba-0025b32654a0", "connect": "Insert"},
      {"name": "ring_helper", "implementor": "OEM Audio Effect", "type": "fa81dbde-588b-11ed-9b6a-0242ac120002", "uuid": "0956df94-0590-11e5-bdbe-0025b32654a0", "connect": "Insert"},
      {"name": "voice_helper", "implementor": "OEM Audio Effect", "type": "fa81dbde-588b-11ed-9b6a-0242ac120002", "uuid": "0ace5c08-0590-11e5-ae9e-0025b32654a0", "connect": "Insert"}
    ],
    "media": {
      "instances": [
        {"binary": "vendor.qti.media.c2@1.0-service", "store": "rank1", "rank": 1}
      ],
      "components": [
        {"name": "c2.qti.avc.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/avc", "aliases": "OMX.qcom.video.decoder.avc", "maxSize": "4096x2176", "profiles": "20480,20481,20482,20484,20487", "levels": "20495", "store": "rank1", "rank": 1},
        {"name": "c2.qti.avc.decoder.low_latency", "kind": "decoder", "domain": "video", "mediaType": "video/avc", "aliases": "OMX.qcom.video.decoder.avc.low_latency", "maxSize": "4096x2176", "profiles": "20480,20481,20482,20484,20487", "levels": "20495", "store": "rank1", "rank": 1},
        {"name": "c2.qti.avc.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/avc", "aliases": "OMX.qcom.video.decoder.avc.secure", "maxSize": "4096x2176", "profiles": "20480,20481,20482,20484,20487", "levels": "20495", "store": "rank1", "rank": 1},
        {"name": "c2.qti.hevc.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/hevc", "aliases": "OMX.qcom.video.decoder.hevc", "maxSize": "4096x2176", "profiles": "24576,24578,24577", "levels": "24835", "store": "rank1", "rank": 1},
        {"name": "c2.qti.hevc.decoder.low_latency", "kind": "decoder", "domain": "video", "mediaType": "video/hevc", "aliases": "OMX.qcom.video.decoder.hevc.low_latency", "maxSize": "4096x2176", "profiles": "24576,24578,24577", "levels": "24835", "store": "rank1", "rank": 1},
        {"name": "c2.qti.hevc.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/hevc", "aliases": "OMX.qcom.video.decoder.hevc.secure", "maxSize": "4096x2176", "profiles": "24576,24578,24577", "levels": "24835", "store": "rank1", "rank": 1},
        {"name": "c2.qti.vp9.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/x-vnd.on2.vp9", "aliases": "OMX.qcom.video.decoder.vp9", "maxSize": "4096x2176", "profiles": "28672,28673,28674", "levels": "28680", "store": "rank1", "rank": 1},
        {"name": "c2.qti.vp9.decoder.low_latency", "kind": "decoder", "domain": "video", "mediaType": "video/x-vnd.on2.vp9", "aliases": "OMX.qcom.video.decoder.vp9.low_latency", "maxSize": "4096x2176", "profiles": "28672,28673,28674", "levels": "28680", "store": "rank1", "rank": 1},
        {"name": "c2.qti.vp9.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/x-vnd.on2.vp9", "aliases": "OMX.qcom.video.decoder.vp9.secure", "maxSize": "4096x2176", "profiles": "28672,28673,28674", "levels": "28680", "store": "rank1", "rank": 1},
        {"name": "c2.qti.avc.encoder", "kind": "encoder", "domain": "video", "mediaType": "video/avc", "aliases": "OMX.qcom.video.encoder.avc", "maxSize": "4096x2176", "profiles": "20480,20482,20484", "levels": "20494", "store": "rank1", "rank": 1},
        {"name": "c2.qti.hevc.encoder", "kind": "encoder", "domain": "video", "mediaType": "video/hevc", "aliases": "OMX.qcom.video.encoder.hevc", "maxSize": "4096x2176", "profiles": "24576", "levels": "24834", "store": "rank1", "rank": 1},
        {"name": "c2.qti.hevc.encoder.cq", "kind": "encoder", "domain": "video", "mediaType": "video/hevc", "aliases": "OMX.qcom.video.encoder.hevc.cq", "maxSize": "512x512", "profiles": "24576", "levels": "24834", "store": "rank1", "rank": 1},
        {"name": "c2.qti.heic.encoder", "kind": "encoder", "domain": "image", "mediaType": "image/vnd.android.heic", "aliases": "OMX.qcom.video.encoder.heic", "maxSize": "16384x16384", "profiles": "", "levels": "", "store": "rank1", "rank": 1}
      ]
    },
    "audioEngine": {
      "modules": "default,bluetooth,r_submix",
      "voiceCallIndexMax": 7,
      "musicIndexMax": 15
    }
  },
  "alignmentNotes": [
    "固件实测锚点（不可改动）：brand=HONOR、marketName=HONOR Pad X7b、board/device=bengal_515、SoC=Snapdragon 685 4G、densityDpi=280、1200x1920、hardwareEgl/Vulkan=adreno、hasNfc=true、hasFingerprint=false、键盘 gpio-keys、耳机插孔 bengal-snd-card Headset Jack、Codec2 服务 vendor.qti.media.c2@1.0-service、13 条音效描述符。",
    "平台整体替换：gold 的 MT6833/mt-cpufreq/sugov_ext/MTK_MNLD/Mali-G57 全部不适用；本机改 qcom-cpufreq-hw + schedutil、QCOM_GNSS_default、Adreno 610、Vulkan 1.1（0x00401000），radioVariant=default，enableIr=false。",
    "传感器严格按固件 unavailableFeatures 裁剪：只保留加速度计(lsm6dso_acc)+环境光(stk3a8x)及加速度派生的虚拟传感器；陀螺仪/磁力计/气压计/距离/心率/步进统统不编造，故无 rotation_vector、gyro、mag、proximity、pressure 条目，也不填 xiaomi.sensor.* 或 swtp。",
    "该机为无马达平板：vibrator 能力位与 effects 置空、vibratorIds 为空；无指纹故 inputIdentity 不注册 uinput 指纹节点，goodixFp=false。",
    "触控 IC 无固件证据：按荣耀低价平板常见 FocalTech 方案取 fts_ts（bus 24 / vendor 0x0EE8 / version 0x100），并给出 goodix_ts、NVTCapacitiveTouchScreen 备选；首轮 device-checker 若报 input 指纹不符，直接以真机 /proc/bus/input/devices 回填 name/bus/vendor/product。",
    "GPU/基带/IMEI/Wi-Fi OUI 为平台代际推理值：glVersionFull 与 Adreno 驱动版本串、vulkanDeviceId(0x06040001)/driverVersion(0x2001a000)/三组 UUID、MPSS.DE.3.1.c1-*-BENGAL_GEN_PACK-*、TAC 86653604、OUI 3c:cd:5d/48:8e:ef 需在真机 getprop gsm.version.baseband / dumpsys SurfaceFlinger / vulkaninfo 首轮实测后锁定。",
    "内存/存储档位（4/8GB LPDDR4X + 128/256GB eMMC 5.1）、5100mAh 电芯、5MP+5MP 相机、Wi-Fi 5/BT 5.1 属公开规格推理，平板市场常见组合，建议以官方 SKU 表二次确认后固化到 devices/manifest 的 versions 表。",
    "GPU 冷却设备名按 bengal KGSL devfreq 节点写作 devfreq-5000000.qcom,kgsl-3d0；温区名沿用 bengal 内核 thermal_zone type（battery/cpu-0-0/cpu-0-1/gpu-0/pa-therm0/quiet-therm/skin-therm/xo-therm），若真机 zone 名不同以 /sys/class/thermal 为准。"
  ],
  "sources": [
    "目标机官方固件解包实测：build.prop / boot cmdline / VINTF manifest / vendor/etc permissions、media_codecs、audio_effects、sensors、wifi、nfc 配置",
    "gold 标杆结构参考：devices/manifest/configs/redmi/gold/gold-android15/ 的 device.json、module/config.json、vendor/config.json、hardware/config.json、vendor/etc/* 字段颗粒度",
    "Qualcomm bengal/SM6225（Snapdragon 680/685）平台公开资料：CPU 拓扑与 OPP 档位、Adreno 610 GL/Vulkan 能力、MPSS.DE 基带命名、KGSL 与 thermal zone 命名",
    "本次联网核实（GSMArena / DuckDuckGo）被反爬挑战拦截，未取得第三方规格页，相关未实测值已在 alignmentNotes 标注待回填"
  ]
}
```

**使用提醒**：这份 JSON 里凡是固件实测项（品牌/型号/分辨率/密度/NFC/耳机孔/键盘/Codec2/音效）都可以直接写入 `devices/manifest`；标注“待真机回填”的项，建议先只写入 `specs` 草案，等首轮 `device-checker` 跑完，再用真机 `getprop`、`/proc/bus/input/devices`、`vulkaninfo`、`/sys/class/thermal` 校正后补齐。
