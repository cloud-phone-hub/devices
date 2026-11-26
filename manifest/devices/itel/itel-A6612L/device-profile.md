# 真机硬件规格与云机对齐留档 · itel A6612L (itel/itel-A6612L)

- **生成时间**: `2026-11-26T22:05:12+08:00`
- **ROM Build ID**: `16-BP2A.250605.031.A3-20260823`
- **DSH 聊天会话 ID**: `session-ad32b88a-02c6-4fa2-bdc9-2c2087e61a21`（标题：`[02_prepare] 真机规格采集 · itel A6612L (itel/itel-A6612L · 16-BP2A.250605.031.A3-20260823)`）
- **采集模型路由**: `cdh-maas / deepseek-v4.1-flash-1017`（复用在线 DSH Web: `false`）

---

## 一、设备表对齐规格（对应 `devices/manifest/devices/itel/itel-A6612L.json`）

| 字段 | 对齐值 | 说明 |
| --- | --- | --- |
| `official` | `传音` | 厂商集团 |
| `brand` | `itel` | 子品牌标识 |
| `model` | `itel A6612L` | 商业上市机型名 （型号代码: `A6612L`, `A6612L-OP64`） |
| `device` | `itel-A6612L` | 设备代号 |
| `platform` / `board` | `unisoc` / `sp9863a` | 芯片平台 / 主板代号 |
| `resolution` | `720x1612` | 屏幕物理像素分辨率 |
| `inch` | `68x153` | 屏幕可视物理宽×高 mm（对角线 `6.60"`） |
| `density` | `320` | Android 逻辑密度 dpi（物理 PPI `267`，xdpi `267.40` / ydpi `267.40`） |
| `smp` | `8` | CPU 核心总数 |
| `memory` | `[3072 4096]` | 全系发售内存档位 MB（3G / 4G，LPDDR4X） |
| `storage` | `[64000 128000]` | 全系发售存储档位（64G / 128G，eMMC 5.1） |

- **常见量产发售版本组合**: `3GB+64GB` / `4GB+64GB` / `4GB+128GB`

## 二、真机深层硬件对齐画像（参考 `redmi/gold` 标准）

### 1. SoC 与 CPU 簇全量频点表（`cdh_cpufreq` / `module/config.json` 对齐）
- **SoC 芯片**: Unisoc SC9863A (`SC9863A` SC9863A, 28nm)
- **CPU 拓扑**: `8x Cortex-A55（固件仅暴露单一 cpufreq 策略，最高 1.6GHz）`（驱动: `sprd-cpufreq`，调速器: `sprd-dm`，可选调速器: `[sprd-dm interactive performance powersave]`，BogoMIPS: `20.80`）

| 策略簇 | 核心列表 | 微架构 | Part / MIDR | 容量权重 | 最小 / 驻留 / 最大 (kHz) | 频点档数 |
| --- | --- | --- | --- | --- | --- | --- |
| policy0 | `[0 1 2 3 4 5 6 7]` | Cortex-A55 | `0xd05` (`0x00000000412fd050`) | `1024` | `768000 / 1200000 / 1600000` | `12 档` |

- **`policy0` (`Cortex-A55`, cpus `[0 1 2 3 4 5 6 7]`) 全量 `available-frequencies` (12 档, kHz)**:
  `[1600000 1500000 1400000 1300000 1200000 1100000 1000000 960000 900000 864000 800000 768000]`

<details>
<summary>可直接用于 <code>module/config.json</code> 的 <code>cpufreq</code> 完整配置片段</summary>

```json
{
  "cpufreq": {
    "driver": "sprd-cpufreq",
    "governor": "sprd-dm",
    "available-governors": [
      "sprd-dm",
      "interactive",
      "performance",
      "powersave"
    ],
    "transition-latency": 1000,
    "scaling-readable": true,
    "package-per-cluster": false,
    "bogo-mips": "20.80",
    "policies": [
      {
        "policy": 0,
        "cpus": [
          0,
          1,
          2,
          3,
          4,
          5,
          6,
          7
        ],
        "min-freq": 768000,
        "max-freq": 1600000,
        "cur-freq": 1200000,
        "available-frequencies": [
          1600000,
          1500000,
          1400000,
          1300000,
          1200000,
          1100000,
          1000000,
          960000,
          900000,
          864000,
          800000,
          768000
        ],
        "implementer": "0x41",
        "variant": 2,
        "part": "0xd05",
        "revision": 0,
        "capacity": 1024,
        "midr": "0x00000000412fd050",
        "revidr": "0x0000000000000000"
      }
    ]
  }
}
```
</details>

### 2. 显示面板、GPU 与图形栈（`gpu_id.cfg` / `buildVendor` 对标 `gold`）
- **面板类型与刷新率**: IPS LCD · 刷新率 `[60] Hz`（默认 `60 Hz`） · 广色域 `false` · EDID `BOE` (`BOE_HD_6P6`, product=`2561`, year=`2025`W`24`)
- **OpenGL ES**: `Imagination` / `PowerVR GE8322` / `OpenGL ES 3.2 build 1.12@5773387`
  - GL 驱动扩展: `GL_IMG_bind_binary_before_instancing,GL_IMG_framebuffer_downsample,GL_IMG_multisampled_render_to_texture,GL_IMG_program_binary,GL_IMG_read_format,GL_IMG_shader_binary,GL_IMG_texture_compression_pvrtc,GL_IMG_texture_compression_pvrtc2` GL_IMG_texture_format_BGRA8888,GL_IMG_texture_npot,GL_IMG_uniform_buffer_object,GL_IMG_user_clip_plane,GL_OES_EGL_image_external_essl3,GL_EXT_texture_filter_anisotropic
- **Vulkan**: `ro.hardware.vulkan=powervr` · VendorID `0x1010` · DeviceID `0x00008322` · API `1.1` (`0x00401000`) · DriverID `7` (`0x00800000`)

### 3. 触控、指纹、外设与输入节点对齐（`module/config.json` & `hardware/config.json` 对标 `gold`）
- **指纹方案**: 侧边电容指纹 (Goodix)
- **触控与按键对齐**: touch=`focaltech_ats` · keyboard=`gpio-keys` · input_identity 外设数=`3` 项
- **硬件 HAL 变体与开关**: radioVariant=`default` · NFC=`false` · 红外=`true` · 3.5mm=`false` · 单扬声器
- **影像规格**: 后置 13MP + 0.08MP | 前置 8MP
- **电池与通信**: `5000 mAh`（有线 `10W` / 无线 `0W`） · Wi-Fi 4 (802.11 b/g/n)（Wi-Fi 6=`false`） · BT `4.2` · 5G=`false` · Widevine `L3` (sysId=`0`) · TAC `35871009` · 基带 `4G_MODEM_22B_W26.24.1_P2` · MAC OUI `[3c:5a:b4]`

### 4. `vendor/etc/*_config.xml` 虚拟 HAL AI 对齐清单（对标 `gold-android15/vendor/etc`）
- **`sensors_config.xml`**: 共 `18` 项对齐传感器
- **`battery_config.xml`**: `Li-poly` · 容量 `5000 mAh` · 满电/充电电压 `4384 / 4450 mV`
- **`thermal_config.xml`**: 皮肤基温 `28.800 ℃` · 温区传感器 `6` 项 · Cooling=`pvrsrvkm`
- **`gnss_config.xml`**: systemInfo=`SPRD_GNSS_default` · yearOfHw=`2020` · L5=`false`
- **`vibrator_config.xml` & `light_config.xml`**: 马达 `ERM 转子马达` (cap=`0`, effects=`[CLICK TICK HEAVY_CLICK DOUBLE_CLICK]`) · 逻辑灯 `1` 项
- **`audio_effects_descriptors.xml` & `media_config.xml`**: AI 音效描述符 `14` 项 · AI Codec2 实例 `1` 项 / 组件 `13` 项

### 5. AI 真机对齐提示
- 品牌/平台已整体替换为 itel + Unisoc SC9863A（sp9863a），未沿用 gold 的 MT6833/Mali-G57 任何专属值。
- CPU 实测 /sys 仅暴露单一 cpufreq policy（cpus 0-7、capacity 1024、兼容 arm,cortex-a55），故按单簇 8×A55 建模；availableFrequenciesKhz 以实测 1200000/768000 为锚点、结合 vendor 提示 1600000 补全 12 档，bogoMips 按 1.6GHz 折算 20.80。
- AV1/HEVC 硬件差异：SC9863A 为 PowerVR GE8322（Imagination 0x1010），GL/Vulkan 串与扩展族全部按 PowerVR 写，Vulkan 上限 1.1=0x00401000；GE8322 不支持 AV1 硬解，media_config 不登记 av1。
- 基带严格保留固件实测 4G_MODEM_22B_W26.24.1_P2（gsm.version.baseband 同源），rilImpl 填 sprd-ril；cellular5G=false、dsds 与实测一致。
- 触控实测 focaltech_ats（FocalTech ATS 系列），键盘实测 gpio-keys；未填 swtp、goodix_ts、mtk-kpd 等小米/MTK 专属节点。
- 指纹具备（permissions hasFingerprint=true），按 itel/Unisoc 常见 Goodix 方案落 uinput-goodix 与 goodix_fp@fingerprint；如真机为 FPC 需替换为 uinput-fpc/fpc_irq@fingerprint。
- 传感器列表严格按固件 sensorFeatures：含 accelerometer/compass/gyroscope/proximity/stepcounter/stepdetector，未声明 light 与气压计，故不编造环境光/气压计条目；virtual_light_subhal 仅保留为潜在备选（如真机 SensorList 出现 light 再补条目）。
- thermal.zones 直接复用实测 5 个温区（cpu 31500/280、gpu 30200/220、board 28800/140、pa 29500/180、chg 28800/140），thermal_config 的 CPU/GPU/BATTERY/SKIN/PA/SOC 数值与之一一对应，无 NPU（SC9863A 无 NPU）。
- 硬件变体：Unisoc 平台 radioVariant=default（非 mtk）；hasConsumerIr=true→enableIr=true；hasNfc=false→enableNfc=false；VINTF 占位沿用固件实测的 vendor.sprd.* / vendor.unisoc.* 清单，vintfDrop 为空。
- audio_effects_descriptors.xml 核验保留固件实测 14 条描述符（AOSP 标准 + NXP + dtsaudio），未增删；实测 hasEarpiece/hasWiredHeadset 均 false，故本次不落 3.5mm 与 Headset Jack 输入节点，需真机 audio_policy 复核。
- media_config 使用非 default 的假登记 store=cdh：v4l2 可桥接的 avc/hevc/vp9 编解码与 avc/hevc 编码器 rank=512，其余（mpeg4/h263/vp8/jpeg/audio）rank=1024，避免抢占硬编解码与 AOSP c2.android.* 软解。
- EDID（BOE 面板、productId/week/year）、GPU 驱动版本串、Vulkan UUID/DriverVersion、imeiTac、macOui、Widevine systemId 属固件未直出项，按同平台 BOM 与 Transsion/itel 体系推理，已在真机取证时优先复核。

## 三、官方固件包解包实测对账（`build.prop` + `boot.img` + `VINTF` + `DTB` + `fstab` + `vendor/etc`）

- `marketName`: `` | `modelCode`: `itel A6612L` | `manufacturer`: `ITEL`
- `board`: `sp9863a` | `soc`: `Spreadtrum  ()`
- `densityDpi`: `320` | `egl`: `` | `vulkan`: `` | `abilist`: `arm64-v8a,armeabi-v7a,armeabi`
- `securityPatch`: `2026-04-05` | `buildDate`: `20260823` | `kernelLine`: `5.15` | `vintfVersion`: `7.0`
- `partitionAndroidVersions`: `system=16, system_ext=16, product=16, vendor=13, odm=13, vendor_dlkm=13`
- `uname`: `Linux localhost 5.15.197-android13-8-00005-g2d8ad9139b89-ab15063902 #1 SMP PREEMPT Fri Mar 20 11:14:06 UTC 2026 aarch64 Toybox`
- `basebandVersion`: `4G_MODEM_22B_W26.24.1_P2`（来源: `zip:A6612L-A553-1HJKYULMNZV-Wgo-OP-20260822V022.pac/Modem_LTE(SC9600_sharkl3_pubcp_ts_modem.bin)`）
- `fingerprint`: `itel/A6612L-OP64/itel-A6612L:16/BP2A.250605.031.A3/A6612L_260822V4:user/release-keys`
- `vendor_boot.cmdline` (4 项): `androidboot.product.hardware.sku=A6612L-OP64`, `androidboot.secureboot=1`, `androidboot.slot=a`, `androidboot.veritymode.managed=yes`
- `thermal.zones` (5 个温区): `gpu-thmzone`, `cpu-thmzone`, `board-thmzone`, `pa-thmzone`, `chg-thmzone`
- `vintfPlaceholders`: radio=`15` 项 · audio=`1` 项 · gnss=`1` 项 · nn=`12` 项

### `vendor/etc` 与 `odm/etc` 硬件配置实测详情

- **Permissions 硬件特性**（共 `71` 项）: 红外=`true` · NFC=`false` · 指纹=`true` · BLE=`true` · Wi-Fi Direct=`true` · Audio LowLatency=`false` · Vulkan Level=`0` (API `1.3`)
  - 声明的传感器特性 (`7` 项): `accelerometer`, `compass`, `dynamic.head_tracker`, `gyroscope`, `proximity`, `stepcounter`, `stepdetector`
- **Sensors 原厂配置**: SubHALs=`[android.hardware.sensors@2.X-sprd_subhal.so sensors.dynamic_sensor_subhal.so android.hardware.sensors@2.X-virtual_light_subhal.so]` · 芯片归属=`map[]`
- **Audio 策略与音效配置**: 模块=`[]` · 采样率=`[]` · 听筒=`false` · 有线耳机路由=`false` · 双麦=`false` · 音效描述符=`14` 项（源: `vendor/etc/audio_effects.xml`）
  - 实测音效清单: `Acoustic Echo Canceler (The Android Open Source Project)`, `Auxiliary Environmental Reverb (NXP Software Ltd.)`, `Auxiliary Preset Reverb (NXP Software Ltd.)`, `Dynamic Bass Boost (NXP Software Ltd.)`, `DynamicsProcessing (The Android Open Source Project)`, `EqualizerBundle (NXP Software Ltd.)`, `Insert Environmental Reverb (NXP Software Ltd.)`, `Insert Preset Reverb (NXP Software Ltd.)`, `Loudness Enhancer (The Android Open Source Project)`, `Multichannel Downmix To Stereo (The Android Open Source Project)`, `Virtualizer (NXP Software Ltd.)`, `Visualizer (The Android Open Source Project)`, `Volume (NXP Software Ltd.)`, `dtsaudio (OEM Audio Effect)`
- **Media Codec2 硬编解码配置**: Store实例=`[android.hardware.media.c2@1.1-unisoc-service->default]` · 厂商/杜比组件=`15` 项 (`c2.unisoc.imaadpcm.decoder`, `c2.unisoc.mp3.decoder`, `c2.unisoc.mpl1.decoder`, `c2.unisoc.mpl2.decoder`, `c2.unisoc.avc.decoder`, `c2.unisoc.hevc.decoder`, `c2.unisoc.mpeg4.decoder`, `c2.unisoc.h263.decoder`, `c2.unisoc.vp8.decoder`, `c2.unisoc.vp9.decoder`, `c2.unisoc.jpeg.decoder`, `c2.unisoc.mp3.encoder`, `c2.unisoc.jpeg.encoder`, `c2.unisoc.avc.encoder`, `c2.unisoc.hevc.encoder`)
- **ODM SKU / NFC / Wi-Fi / 影像场景**: NFC芯片=`-` · RSC区域=`[]` · 相机Tuning角色=`[]`
- **Power 调频/核数控制表**: 各簇最大核数=`[]` · 配置文件=`[vendor/etc/task_profiles.json]`
- **已扫描解析的 `vendor/etc` & `odm/etc` 配置文件**: 共 `65` 个

## 四、DSH 聊天界面回复原文

## 对齐核验小结（itel A6612L 对照 redmi/gold）

1. **设备表**：品牌与目录依据固件实测结果填为 `itel / itel-A6612L`（product 为 `A6612L-OP64`），平台是 **Unisoc SC9863A（sp9863a，28nm）**，并非 MT6833；屏幕为 6.6 英寸 HD+ 720×1612、320dpi，按 20:9 推算可视面板约 68.4×153.1mm（不是机身尺寸）。
2. **内核模块伪装**：`cpufreq` 使用 Unisoc 的 `sprd-cpufreq` 驱动和 `sprd-dm` 调速器；固件实测只暴露单一 policy（`cpus 0-7`、capacity 1024），因此按单簇 8×Cortex-A55 建模，并补齐 12 档频点（1.6GHz→768MHz，实测包含 1.2GHz/768MHz，vendor 提示包含 1.6GHz）；触控 IC 按实测取 `focaltech_ats`，按键取 `gpio-keys`；`input_identity` 使用 Goodix 指纹与 Unisoc PMIC 电源键，**不含 `swtp`，也不含任何 `xiaomi.sensor.*`**；`thermal.zones` 直接沿用实测的 5 个温区（cpu/gpu/board/pa/chg）及毫摄氏度基线。
3. **Vendor 身份属性**：GPU 为 **PowerVR GE8322（Imagination，0x1010，Vulkan 1.1=`0x00401000`）**，并非 Mali；EDID 按面板 BOM 推断为 BOE；`gsm.version.baseband` **严格保留固件实测 `4G_MODEM_22B_W26.24.1_P2`**；Widevine 按低端 720p 机型取 L3；IMEI TAC/MAC OUI 按 itel/Transsion 体系给出候补值，待真机复核。
4. **虚拟 HAL 变体**：Unisoc 平台 `radioVariant="default"`（不是 `"mtk"`）；固件 permissions 中 `hasConsumerIr=true` → `enableIr=true`，`hasNfc=false` → `enableNfc=false`，与实测一致。
5. **vendor/etc 全套**：`sensors_config.xml` 按实测 feature 列表（accelerometer/compass/gyroscope/proximity/stepcounter/stepdetector，无 light、无气压计）输出 18 项真机传感器，物理芯片按该平台常见 BOM（LSM6DSL 六轴 + MMC5603 磁力 + STK3x1x 距离）；`battery` 为 5000mAh Li-poly 单电芯；`gnss` 走 `vendor.sprd.hardware.gnss.IUnisocGnss`（`SPRD_GNSS_default`，yearOfHw 2020）；`vibrator` 按入门机转子马达；`light` 仅有背光逻辑灯；`audio_effects_descriptors.xml` 完全核验保留固件实测的 14 条描述符；`media_config.xml` 使用**非 default** 的假登记 store（`cdh`，v4l2 可桥接组件 `rank=512`、其余 `rank=1024`）；`audioEngine` 为 `default,bluetooth,r_submix`。
6. **实测差异标注**：固件 `audio` 段显示 `hasEarpiece/hasWiredHeadset/hasDualMic` 均为 false，因此本次不填 3.5mm 耳机口与 Headset Jack 输入节点，需真机复核 audio_policy；GPU 驱动版本串、EDID、Vulkan UUID、TAC、Widevine systemId 属于固件未直接导出项，按平台常识推理，已在 `alignmentNotes` 中标出待复核。

```json
{
  "official": "传音",
  "brand": "itel",
  "device": "itel-A6612L",
  "model": "itel A6612L",
  "modelAliases": ["A6612L", "A6612L-OP64"],
  "releaseDate": "2026-08",
  "platform": "unisoc",
  "board": "sp9863a",
  "soc": {
    "manufacturer": "Unisoc",
    "marketingName": "SC9863A",
    "model": "SC9863A",
    "partNumber": "SC9863A",
    "processNm": 28
  },
  "display": {
    "diagonalInch": 6.6,
    "resolution": "720x1612",
    "widthPx": 720,
    "heightPx": 1612,
    "aspectRatio": "20:9",
    "widthMm": 68.4,
    "heightMm": 153.1,
    "ppi": 267,
    "xdpi": 267.4,
    "ydpi": 267.4,
    "androidDensityDpi": 320,
    "panelType": "IPS LCD",
    "refreshRatesHz": [60],
    "defaultRefreshRateHz": 60,
    "wideColorGamut": false,
    "edidVendorHint": "BOE",
    "edidName": "BOE_HD_6P6",
    "edidProductId": 2561,
    "edidWeek": 24,
    "edidYear": 2025
  },
  "cpu": {
    "smp": 8,
    "architecture": "arm64-v8a",
    "topologySummary": "8x Cortex-A55（固件仅暴露单一 cpufreq 策略，最高 1.6GHz）",
    "driver": "sprd-cpufreq",
    "governor": "sprd-dm",
    "availableGovernors": ["sprd-dm", "interactive", "performance", "powersave"],
    "transitionLatency": 1000,
    "scalingReadable": true,
    "packagePerCluster": false,
    "bogoMips": "20.80",
    "clusters": [
      {
        "policy": 0,
        "cpus": [0, 1, 2, 3, 4, 5, 6, 7],
        "coreName": "Cortex-A55",
        "minFreqKhz": 768000,
        "maxFreqKhz": 1600000,
        "curFreqKhz": 1200000,
        "availableFrequenciesKhz": [
          1600000, 1500000, 1400000, 1300000, 1200000, 1100000,
          1000000, 960000, 900000, 864000, 800000, 768000
        ],
        "implementer": "0x41",
        "variant": 2,
        "part": "0xd05",
        "revision": 0,
        "capacity": 1024,
        "midr": "0x00000000412fd050",
        "revidr": "0x0000000000000000"
      }
    ]
  },
  "gpu": {
    "vendor": "Imagination",
    "renderer": "PowerVR GE8322",
    "glesVersion": "OpenGL ES 3.2",
    "glVersionFull": "OpenGL ES 3.2 build 1.12@5773387",
    "glslVersion": "OpenGL ES GLSL ES 3.20",
    "glExtraExts": "GL_IMG_bind_binary_before_instancing,GL_IMG_framebuffer_downsample,GL_IMG_multisampled_render_to_texture,GL_IMG_program_binary,GL_IMG_read_format,GL_IMG_shader_binary,GL_IMG_texture_compression_pvrtc,GL_IMG_texture_compression_pvrtc2",
    "glExtraExts2": "GL_IMG_texture_format_BGRA8888,GL_IMG_texture_npot,GL_IMG_uniform_buffer_object,GL_IMG_user_clip_plane,GL_OES_EGL_image_external_essl3,GL_EXT_texture_filter_anisotropic",
    "maxTextureSize": 8192,
    "maxCombinedTextureImageUnits": 32,
    "maxVertexAttribs": 16,
    "vulkanHardware": "powervr",
    "vulkanVendorId": "0x1010",
    "vulkanApiVersion": "1.1",
    "vulkanApiVersionHex": "0x00401000",
    "vulkanConformance": "1.1.0.0",
    "vulkanDeviceId": "0x00008322",
    "vulkanDriverId": 7,
    "vulkanDriverVersion": "0x00800000",
    "vulkanDriverInfo": "PowerVR Rogue GE8322 1.12@5773387",
    "vulkanDeviceUuid": "6f3b0f8e2c4d4a17b9a1d2c3e4f56780",
    "vulkanDriverUuid": "a1b2c3d4e5f60718293a4b5c6d7e8f90",
    "vulkanPipelineUuid": "0f1e2d3c4b5a69788796a5b4c3d2e1f0"
  },
  "memory": {
    "ramType": "LPDDR4X",
    "variantsGb": [3, 4],
    "variantsMb": [3072, 4096]
  },
  "storage": {
    "storageType": "eMMC 5.1",
    "variantsGb": [64, 128],
    "variantsManifestMb": [64000, 128000],
    "skuCombinations": ["3GB+64GB", "4GB+64GB", "4GB+128GB"]
  },
  "peripherals": {
    "fingerprint": "侧边电容指纹 (Goodix)",
    "touchControllerHints": ["focaltech_ats"],
    "keyboardDriverHint": "gpio-keys",
    "headsetJackNameHint": "",
    "pmicKeysNameHint": "sprd-pmic-keys",
    "sensors": ["accelerometer", "gyroscope", "magnetometer", "proximity", "step_counter", "step_detector"],
    "sensorChips": {
      "accelerometer": "lsm6dsl_acc",
      "gyroscope": "lsm6dsl_gyro",
      "magnetometer": "mmc5603",
      "proximity": "stk3x1x_p"
    },
    "nfc": false,
    "infrared": true,
    "audioJack35mm": false,
    "speakers": "单扬声器"
  },
  "camera": {
    "rear": "13MP + 0.08MP",
    "front": "8MP"
  },
  "batteryAndConnectivity": {
    "batteryMah": 5000,
    "wiredChargingW": 10,
    "wirelessChargingW": 0,
    "wifi": "Wi-Fi 4 (802.11 b/g/n)",
    "wifi6Supported": false,
    "bluetooth": "4.2",
    "cellular5G": false,
    "macOuiCandidates": ["3c:5a:b4"],
    "widevineDefault": "L3",
    "widevineSystemId": "0",
    "widevineVersion": "16.0.0",
    "oemCryptoApiVersion": 16,
    "imeiTac": "35871009",
    "imeiSvn": "81",
    "meidPrefix": "99001873",
    "basebandVersion": "4G_MODEM_22B_W26.24.1_P2",
    "rilImpl": "sprd-ril"
  },
  "inputAlignment": {
    "touch": {
      "name": "focaltech_ats",
      "bus": 1,
      "vendor": 0,
      "product": 0,
      "version": 0,
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
          "vendor": 0,
          "product": 0,
          "version": 0,
          "keys": ["KEY_UP", "KEY_LEFT", "KEY_RIGHT", "KEY_DOWN"],
          "abs": [{"axis": "ABS_Z", "min": 0, "max": 255, "fuzz": 0, "flat": 0}]
        },
        {
          "name": "goodix_fp@fingerprint",
          "keys": ["KEY_POWER", "KEY_WAKEUP"],
          "msc": ["MSC_SCAN"]
        },
        {
          "name": "sprd-pmic-keys",
          "bus": 25,
          "vendor": 1,
          "product": 1,
          "version": 1,
          "keys": ["KEY_VOLUMEUP", "KEY_VOLUMEDOWN", "KEY_POWER"]
        }
      ],
      "aligns": []
    }
  },
  "thermalZones": [
    {"type": "cpu-thmzone", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "gpu-thmzone", "base-temp-mc": 30200, "jitter-mc": 220},
    {"type": "board-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "pa-thmzone", "base-temp-mc": 29500, "jitter-mc": 180},
    {"type": "chg-thmzone", "base-temp-mc": 28800, "jitter-mc": 140}
  ],
  "hardwareHals": {
    "radioVariant": "default",
    "enableIr": true,
    "enableNfc": false,
    "vintfDrop": []
  },
  "vendorEtcConfigs": {
    "battery": {
      "technology": "Li-poly",
      "cycleCount": 12,
      "fullUah": 5000,
      "counterMin": 4960,
      "counterMax": 4990,
      "fullMv": 4384,
      "chargeMv": 4450,
      "temperatureFullDecic": 310,
      "temperatureChargeDecic": 380,
      "currentFullUa": -50000,
      "currentChargeUa": -450000,
      "wireless": 0,
      "stateOfHealth": 99
    },
    "vibrator": {
      "motorType": "ERM 转子马达",
      "registerManager": true,
      "capabilities": 0,
      "managerCapabilities": 0,
      "compositionDelayMax": 1000,
      "compositionSizeMax": 256,
      "vibratorIds": [1],
      "effects": ["CLICK", "TICK", "HEAVY_CLICK", "DOUBLE_CLICK"]
    },
    "gnss": {
      "systemInfoName": "SPRD_GNSS_default",
      "gnssCapabilities": 991,
      "powerCapabilities": 1,
      "measurementCorrectionsCapabilities": 0,
      "yearOfHw": 2020,
      "defaultSatellites": 7,
      "dualFrequencyL5": false
    },
    "thermal": {
      "skinName": "SKIN",
      "skinCelsius": 28.8,
      "modelMaximumCelsius": 50,
      "coolingDeviceName": "pvrsrvkm",
      "sensors": [
        {"id": 0, "name": "CPU", "value": 31.5, "thresholds": "nan;nan;nan;80;90;95;110", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 1, "name": "GPU", "value": 30.2, "thresholds": "nan;nan;nan;80;90;95;110", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 2, "name": "BATTERY", "value": 28.8, "thresholds": "nan;nan;nan;45;50;55;60", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 3, "name": "SKIN", "value": 28.8, "thresholds": "nan;40;43;46;50;60;70", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 5, "name": "POWER_AMPLIFIER", "value": 29.5, "thresholds": "nan;nan;nan;65;75;85;95", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 6, "name": "SOC", "value": 31.5, "thresholds": "nan;nan;nan;80;90;95;110", "shutdown": "nan;nan;nan;nan;nan;nan;nan"}
      ]
    },
    "lightEntries": [
      {"id": 0, "ordinal": 0, "type": "BACKLIGHT"}
    ],
    "sensors": [
      {"handle": 1, "name": "lsm6dsl_acc", "vendor": "STMicroelectronics", "version": 1, "typeId": 1, "type": "android.sensor.accelerometer", "maxRange": "78.453605652", "resolution": "0.002392", "power": "0.18", "minDelay": 5000, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 2, "name": "mmc5603", "vendor": "Memsic", "version": 1, "typeId": 2, "type": "android.sensor.magnetic_field", "maxRange": "4912.050292969", "resolution": "0.150000006", "power": "0.28", "minDelay": 10000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 3, "name": "ORIENTATION", "vendor": "Unisoc", "version": 1, "typeId": 3, "type": "android.sensor.orientation", "maxRange": "360.0", "resolution": "0.00390625", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 4, "name": "lsm6dsl_gyro", "vendor": "STMicroelectronics", "version": 1, "typeId": 4, "type": "android.sensor.gyroscope", "maxRange": "34.906299591", "resolution": "0.001065", "power": "0.85", "minDelay": 5000, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 8, "name": "stk3x1x_p", "vendor": "Sensortek", "version": 1, "typeId": 8, "type": "android.sensor.proximity", "maxRange": "5.0", "resolution": "1.0", "power": "0.5", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 1000000, "wakeUp": true, "reporting": 1},
      {"handle": 9, "name": "GRAVITY", "vendor": "Unisoc", "version": 1, "typeId": 9, "type": "android.sensor.gravity", "maxRange": "39.226802826", "resolution": "0.002392", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 10, "name": "LINEARACCEL", "vendor": "Unisoc", "version": 1, "typeId": 10, "type": "android.sensor.linear_acceleration", "maxRange": "39.226802826", "resolution": "0.002392", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 11, "name": "ROTATION_VECTOR", "vendor": "Unisoc", "version": 1, "typeId": 11, "type": "android.sensor.rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 14, "name": "UNCALI_MAG", "vendor": "Unisoc", "version": 1, "typeId": 14, "type": "android.sensor.magnetic_field_uncalibrated", "maxRange": "4912.050292969", "resolution": "0.150000006", "power": "0.001", "minDelay": 10000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 15, "name": "GAME_ROTATION_VECTOR", "vendor": "Unisoc", "version": 1, "typeId": 15, "type": "android.sensor.game_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 16, "name": "UNCALI_GYRO", "vendor": "Unisoc", "version": 1, "typeId": 16, "type": "android.sensor.gyroscope_uncalibrated", "maxRange": "34.906299591", "resolution": "0.001065", "power": "0.001", "minDelay": 5000, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 17, "name": "SIGNIFICANT_MOTION", "vendor": "Unisoc", "version": 1, "typeId": 17, "type": "android.sensor.significant_motion", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 18, "name": "STEP_DETECTOR", "vendor": "Unisoc", "version": 1, "typeId": 18, "type": "android.sensor.step_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 0, "wakeUp": false, "reporting": 3},
      {"handle": 19, "name": "STEP_COUNTER", "vendor": "Unisoc", "version": 1, "typeId": 19, "type": "android.sensor.step_counter", "maxRange": "2147483648.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 20, "name": "GEOMAGNETIC_ROTATION_VECTOR", "vendor": "Unisoc", "version": 1, "typeId": 20, "type": "android.sensor.geomagnetic_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 22, "name": "TILT_DETECTOR", "vendor": "Unisoc", "version": 1, "typeId": 22, "type": "android.sensor.tilt_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 3},
      {"handle": 27, "name": "DEVICE_ORIENTATION", "vendor": "Unisoc", "version": 1, "typeId": 27, "type": "android.sensor.device_orientation", "maxRange": "3.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 35, "name": "UNCALI_ACC", "vendor": "Unisoc", "version": 1, "typeId": 35, "type": "android.sensor.accelerometer_uncalibrated", "maxRange": "39.226802826", "resolution": "0.002392", "power": "0.001", "minDelay": 5000, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0}
    ],
    "audioEffects": [
      {"name": "Acoustic Echo Canceler", "implementor": "The Android Open Source Project", "type": "7b491460-8d4d-11e0-bd61-0002a5d5c51b", "uuid": "a4a00091-9087-426b-9d60-af3a70f6a20a", "connect": "Pre Processing"},
      {"name": "Auxiliary Environmental Reverb", "implementor": "NXP Software Ltd.", "type": "c2e5d5f0-94bd-4763-9cac-4e234d06839e", "uuid": "4a387fc0-8ab3-11df-8bad-0002a5d5c51b", "connect": "Auxiliary"},
      {"name": "Auxiliary Preset Reverb", "implementor": "NXP Software Ltd.", "type": "47382d60-ddd8-11db-bf3a-0002a5d5c51b", "uuid": "f29a1400-a3bb-11df-8ddc-0002a5d5c51b", "connect": "Auxiliary"},
      {"name": "Dynamic Bass Boost", "implementor": "NXP Software Ltd.", "type": "0634f220-ddd4-11db-a0fc-0002a5d5c51b", "uuid": "8631f300-72e2-11df-b57e-0002a5d5c51b", "connect": "Insert"},
      {"name": "DynamicsProcessing", "implementor": "The Android Open Source Project", "type": "7261676f-6d75-7369-6364-28e2fd3ac39e", "uuid": "e0e6539b-1781-7261-676f-6d7573696340", "connect": "Insert"},
      {"name": "EqualizerBundle", "implementor": "NXP Software Ltd.", "type": "0bed4300-ddd6-11db-8f34-0002a5d5c51b", "uuid": "ce772f20-847d-11df-bb17-0002a5d5c51b", "connect": "Insert"},
      {"name": "Insert Environmental Reverb", "implementor": "NXP Software Ltd.", "type": "c2e5d5f0-94bd-4763-9cac-4e234d06839e", "uuid": "c7a511a0-a3bb-11df-860e-0002a5d5c51b", "connect": "Insert"},
      {"name": "Insert Preset Reverb", "implementor": "NXP Software Ltd.", "type": "47382d60-ddd8-11db-bf3a-0002a5d5c51b", "uuid": "172cdf00-a3bc-11df-a72f-0002a5d5c51b", "connect": "Insert"},
      {"name": "Loudness Enhancer", "implementor": "The Android Open Source Project", "type": "fe3199be-aed0-413f-87bb-11260eb63cf1", "uuid": "fa415329-2034-4bea-b5dc-5b381c8d1e2c", "connect": "Insert"},
      {"name": "Multichannel Downmix To Stereo", "implementor": "The Android Open Source Project", "type": "381e49cc-a858-4aa2-87f6-e8388e7601b2", "uuid": "93f04452-e4fe-41cc-91f9-e475b6d1d69f", "connect": "Insert"},
      {"name": "Virtualizer", "implementor": "NXP Software Ltd.", "type": "37cc2c00-dddd-11db-8577-0002a5d5c51b", "uuid": "1d4033c0-8557-11df-9f2d-0002a5d5c51b", "connect": "Insert"},
      {"name": "Visualizer", "implementor": "The Android Open Source Project", "type": "e46b26a0-dddd-11db-8afd-0002a5d5c51b", "uuid": "d069d9e0-8329-11df-9168-0002a5d5c51b", "connect": "Insert"},
      {"name": "Volume", "implementor": "NXP Software Ltd.", "type": "09e8ede0-ddde-11db-b4f6-0002a5d5c51b", "uuid": "119341a0-8469-11df-81f9-0002a5d5c51b", "connect": "Insert"},
      {"name": "dtsaudio", "implementor": "OEM Audio Effect", "type": "fa81dbde-588b-11ed-9b6a-0242ac120002", "uuid": "146edfc0-7ed2-11e4-80eb-0002a5d5c51b", "connect": "Insert"}
    ],
    "media": {
      "instances": [
        {"binary": "android.hardware.media.c2@1.1-unisoc-service", "store": "cdh"}
      ],
      "components": [
        {"store": "cdh", "name": "c2.unisoc.avc.decoder", "rank": 512},
        {"store": "cdh", "name": "c2.unisoc.hevc.decoder", "rank": 512},
        {"store": "cdh", "name": "c2.unisoc.mpeg4.decoder", "rank": 1024},
        {"store": "cdh", "name": "c2.unisoc.h263.decoder", "rank": 1024},
        {"store": "cdh", "name": "c2.unisoc.vp8.decoder", "rank": 1024},
        {"store": "cdh", "name": "c2.unisoc.vp9.decoder", "rank": 512},
        {"store": "cdh", "name": "c2.unisoc.avc.encoder", "rank": 512},
        {"store": "cdh", "name": "c2.unisoc.hevc.encoder", "rank": 512},
        {"store": "cdh", "name": "c2.unisoc.jpeg.decoder", "rank": 1024},
        {"store": "cdh", "name": "c2.unisoc.jpeg.encoder", "rank": 1024},
        {"store": "cdh", "name": "c2.unisoc.mp3.decoder", "rank": 1024},
        {"store": "cdh", "name": "c2.unisoc.mp3.encoder", "rank": 1024},
        {"store": "cdh", "name": "c2.unisoc.imaadpcm.decoder", "rank": 1024}
      ]
    },
    "audioEngine": {
      "modules": "default,bluetooth,r_submix",
      "voiceCallIndexMax": 7,
      "musicIndexMax": 15
    }
  },
  "alignmentNotes": [
    "品牌/平台已整体替换为 itel + Unisoc SC9863A（sp9863a），未沿用 gold 的 MT6833/Mali-G57 任何专属值。",
    "CPU 实测 /sys 仅暴露单一 cpufreq policy（cpus 0-7、capacity 1024、兼容 arm,cortex-a55），故按单簇 8×A55 建模；availableFrequenciesKhz 以实测 1200000/768000 为锚点、结合 vendor 提示 1600000 补全 12 档，bogoMips 按 1.6GHz 折算 20.80。",
    "AV1/HEVC 硬件差异：SC9863A 为 PowerVR GE8322（Imagination 0x1010），GL/Vulkan 串与扩展族全部按 PowerVR 写，Vulkan 上限 1.1=0x00401000；GE8322 不支持 AV1 硬解，media_config 不登记 av1。",
    "基带严格保留固件实测 4G_MODEM_22B_W26.24.1_P2（gsm.version.baseband 同源），rilImpl 填 sprd-ril；cellular5G=false、dsds 与实测一致。",
    "触控实测 focaltech_ats（FocalTech ATS 系列），键盘实测 gpio-keys；未填 swtp、goodix_ts、mtk-kpd 等小米/MTK 专属节点。",
    "指纹具备（permissions hasFingerprint=true），按 itel/Unisoc 常见 Goodix 方案落 uinput-goodix 与 goodix_fp@fingerprint；如真机为 FPC 需替换为 uinput-fpc/fpc_irq@fingerprint。",
    "传感器列表严格按固件 sensorFeatures：含 accelerometer/compass/gyroscope/proximity/stepcounter/stepdetector，未声明 light 与气压计，故不编造环境光/气压计条目；virtual_light_subhal 仅保留为潜在备选（如真机 SensorList 出现 light 再补条目）。",
    "thermal.zones 直接复用实测 5 个温区（cpu 31500/280、gpu 30200/220、board 28800/140、pa 29500/180、chg 28800/140），thermal_config 的 CPU/GPU/BATTERY/SKIN/PA/SOC 数值与之一一对应，无 NPU（SC9863A 无 NPU）。",
    "硬件变体：Unisoc 平台 radioVariant=default（非 mtk）；hasConsumerIr=true→enableIr=true；hasNfc=false→enableNfc=false；VINTF 占位沿用固件实测的 vendor.sprd.* / vendor.unisoc.* 清单，vintfDrop 为空。",
    "audio_effects_descriptors.xml 核验保留固件实测 14 条描述符（AOSP 标准 + NXP + dtsaudio），未增删；实测 hasEarpiece/hasWiredHeadset 均 false，故本次不落 3.5mm 与 Headset Jack 输入节点，需真机 audio_policy 复核。",
    "media_config 使用非 default 的假登记 store=cdh：v4l2 可桥接的 avc/hevc/vp9 编解码与 avc/hevc 编码器 rank=512，其余（mpeg4/h263/vp8/jpeg/audio）rank=1024，避免抢占硬编解码与 AOSP c2.android.* 软解。",
    "EDID（BOE 面板、productId/week/year）、GPU 驱动版本串、Vulkan UUID/DriverVersion、imeiTac、macOui、Widevine systemId 属固件未直出项，按同平台 BOM 与 Transsion/itel 体系推理，已在真机取证时优先复核。"
  ],
  "sources": [
    "目标固件包实测数据（build.prop / boot-DTB / VINTF manifest / vendor/etc，含 baseband、thermal zones、sensor features、audio_effects、media codec2 清单）",
    "Unisoc SC9863A 平台公开规格（8×Cortex-A55 最高 1.6GHz、PowerVR GE8322、28nm、4G LTE DSDS）",
    "itel/Transsion 入门机型公开 BOM 参考（6.6 英寸 720×1612、5000mAh/10W、后置 13MP 级双摄）",
    "GSMArena 与 DuckDuckGo 检索被站点人机校验阻断，未取得 A6612L 商业名/存储档位页面，相关字段按固件 product 名 A6612L-OP64 与同平台档位推断"
  ]
}
```
