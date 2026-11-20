# 真机硬件规格与云机对齐留档 · OPPO CPH2797 (oppo/CPH2797)

- **生成时间**: `2026-11-18T19:16:23+08:00`
- **ROM Build ID**: `16-BP2A.250605.015-20260529`
- **分区 Android 版本**: `system=16, system_ext=16, product=16, vendor=16, vendor_dlkm=16, odm_dlkm=16`
- **DSH 聊天会话 ID**: `session-ab23d997-14df-4851-a372-6649b5a122a2`（标题：`[02_prepare] 真机规格采集 · oppo/CPH2797 (16-BP2A.250605.015-20260529)`）
- **采集模型路由**: `cdh-maas / deepseek-v4.1-flash-1017`（复用在线 DSH Web: `true`）

---

## 一、设备表对齐规格（对应 `devices/manifest/devices/oppo/CPH2797.json`）

| 字段 | 对齐值 | 说明 |
| --- | --- | --- |
| `official` | `OPPO` | 厂商集团 |
| `brand` | `oppo` | 子品牌标识 |
| `model` | `OPPO CPH2797` | 商业上市机型名 （型号代码: `CPH2797`, `ossi`） |
| `device` | `CPH2797` | 设备代号 |
| `platform` / `board` | `mtk` / `mt6993` | 芯片平台 / 主板代号 |
| `resolution` | `1256x2760` | 屏幕物理像素分辨率 |
| `inch` | `69x152` | 屏幕可视物理宽×高 mm（对角线 `6.59"`） |
| `density` | `480` | Android 逻辑密度 dpi（物理 PPI `460`，xdpi `460.21` / ydpi `460.21`） |
| `smp` | `8` | CPU 核心总数 |
| `memory` | `[12288 16384]` | 全系发售内存档位 MB（12G / 16G，LPDDR5X） |
| `storage` | `[256000 512000 1024000]` | 全系发售存储档位（256G / 512G / 1T，UFS 4.1） |

- **常见量产发售版本组合**: `12GB+256GB` / `12GB+512GB` / `16GB+512GB` / `16GB+1TB`

## 二、真机深层硬件对齐画像（参考 `redmi/gold` 标准）

### 1. SoC 与 CPU 簇全量频点表（`cdh_cpufreq` / `module/config.json` 对齐）
- **SoC 芯片**: MediaTek Dimensity 9500 (`MT6993` MT6993V/ZA, 3nm)
- **CPU 拓扑**: `4x Cortex-C1 Pro @ 2.7GHz + 3x Cortex-C1 Premium @ 3.5GHz + 1x Cortex-C1 Ultra @ 4.21GHz`（驱动: `mt-cpufreq`，调速器: `sugov_ext`，可选调速器: `[sugov_ext conservative powersave performance schedutil]`，BogoMIPS: `26.00`）

| 策略簇 | 核心列表 | 微架构 | Part / MIDR | 容量权重 | 最小 / 驻留 / 最大 (kHz) | 频点档数 |
| --- | --- | --- | --- | --- | --- | --- |
| policy0 | `[0 1 2 3]` | Cortex-C1 Pro | `0xd8a` (`0x00000000410fd8a0`) | `793` | `400000 / 650000 / 2700000` | `19 档` |
| policy4 | `[4 5 6]` | Cortex-C1 Premium | `0xd8b` (`0x00000000410fd8b0`) | `952` | `600000 / 1100000 / 3500000` | `23 档` |
| policy7 | `[7]` | Cortex-C1 Ultra | `0xd8c` (`0x00000000410fd8c0`) | `1024` | `1000000 / 1200000 / 4210000` | `17 档` |

- **`policy0` (`Cortex-C1 Pro`, cpus `[0 1 2 3]`) 全量 `available-frequencies` (19 档, kHz)**:
  `[2700000 2600000 2500000 2400000 2200000 2000000 1800000 1700000 1600000 1500000 1400000 1200000 1100000 1000000 800000 700000 600000 500000 400000]`
- **`policy4` (`Cortex-C1 Premium`, cpus `[4 5 6]`) 全量 `available-frequencies` (23 档, kHz)**:
  `[3500000 3300000 3250000 3000000 2900000 2850000 2700000 2600000 2500000 2400000 2200000 2000000 1800000 1700000 1600000 1500000 1400000 1200000 1100000 1000000 800000 700000 600000]`
- **`policy7` (`Cortex-C1 Ultra`, cpus `[7]`) 全量 `available-frequencies` (17 档, kHz)**:
  `[4210000 4000000 3800000 3500000 3300000 3000000 2900000 2700000 2500000 2400000 2200000 2000000 1800000 1600000 1400000 1200000 1000000]`

<details>
<summary>可直接用于 <code>module/config.json</code> 的 <code>cpufreq</code> 完整配置片段</summary>

```json
{
  "cpufreq": {
    "driver": "mt-cpufreq",
    "governor": "sugov_ext",
    "available-governors": [
      "sugov_ext",
      "conservative",
      "powersave",
      "performance",
      "schedutil"
    ],
    "transition-latency": 1000,
    "scaling-readable": false,
    "package-per-cluster": false,
    "bogo-mips": "26.00",
    "policies": [
      {
        "policy": 0,
        "cpus": [
          0,
          1,
          2,
          3
        ],
        "min-freq": 400000,
        "max-freq": 2700000,
        "cur-freq": 650000,
        "available-frequencies": [
          2700000,
          2600000,
          2500000,
          2400000,
          2200000,
          2000000,
          1800000,
          1700000,
          1600000,
          1500000,
          1400000,
          1200000,
          1100000,
          1000000,
          800000,
          700000,
          600000,
          500000,
          400000
        ],
        "implementer": "0x41",
        "variant": 0,
        "part": "0xd8a",
        "revision": 0,
        "capacity": 793,
        "midr": "0x00000000410fd8a0",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 4,
        "cpus": [
          4,
          5,
          6
        ],
        "min-freq": 600000,
        "max-freq": 3500000,
        "cur-freq": 1100000,
        "available-frequencies": [
          3500000,
          3300000,
          3250000,
          3000000,
          2900000,
          2850000,
          2700000,
          2600000,
          2500000,
          2400000,
          2200000,
          2000000,
          1800000,
          1700000,
          1600000,
          1500000,
          1400000,
          1200000,
          1100000,
          1000000,
          800000,
          700000,
          600000
        ],
        "implementer": "0x41",
        "variant": 0,
        "part": "0xd8b",
        "revision": 0,
        "capacity": 952,
        "midr": "0x00000000410fd8b0",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 7,
        "cpus": [
          7
        ],
        "min-freq": 1000000,
        "max-freq": 4210000,
        "cur-freq": 1200000,
        "available-frequencies": [
          4210000,
          4000000,
          3800000,
          3500000,
          3300000,
          3000000,
          2900000,
          2700000,
          2500000,
          2400000,
          2200000,
          2000000,
          1800000,
          1600000,
          1400000,
          1200000,
          1000000
        ],
        "implementer": "0x41",
        "variant": 0,
        "part": "0xd8c",
        "revision": 0,
        "capacity": 1024,
        "midr": "0x00000000410fd8c0",
        "revidr": "0x0000000000000000"
      }
    ]
  }
}
```
</details>

### 2. 显示面板、GPU 与图形栈（`gpu_id.cfg` / `buildVendor` 对标 `gold`）
- **面板类型与刷新率**: LTPO AMOLED · 刷新率 `[60 90 120] Hz`（默认 `120 Hz`） · 广色域 `true` · EDID `BOE` (`BOE_OSSI`, product=`2591`, year=`2026`W`12`)
- **OpenGL ES**: `ARM` / `Mali-G1-Ultra MC12` / `OpenGL ES 3.2 v1.r54p0-01eac0.5f3a9c7e2b8d4610af5c3e8b2d7a90f1`
  - GL 驱动扩展: `GL_ARM_mali_program_binary,GL_ARM_mali_shader_binary,GL_ARM_rgba8,GL_EXT_buffer_storage,GL_EXT_geometry_shader,GL_EXT_shader_pixel_local_storage2,GL_KHR_blend_equation_advanced,GL_KHR_debug,GL_OES_depth24,GL_OES_texture_float` GL_ARM_debug_assert,GL_ARM_shader_core_properties,GL_ARM_texture_unnormalized_coordinates,GL_EXT_texture_cube_map_array,GL_EXT_shader_framebuffer_fetch,GL_KHR_shader_subgroup
- **Vulkan**: `ro.hardware.vulkan=mali` · VendorID `0x13b5` · DeviceID `0x90960010` · API `1.3` (`0x00403116`) · DriverID `9` (`0x0d801000`)

### 3. 触控、指纹、外设与输入节点对齐（`module/config.json` & `hardware/config.json` 对标 `gold`）
- **指纹方案**: 屏下超声波指纹 (Goodix)
- **触控与按键对齐**: touch=`synaptics_tcm` · keyboard=`mtk-kpd` · input_identity 外设数=`4` 项
- **硬件 HAL 变体与开关**: radioVariant=`mtk` · NFC=`true` · 红外=`false` · 3.5mm=`false` · 立体声双扬声器
- **影像规格**: 后置 50MP 主摄 + 50MP 超广角 + 50MP 潜望长焦(3x) | 前置 32MP
- **电池与通信**: `7025 mAh`（有线 `80W` / 无线 `50W`） · Wi-Fi 7 (802.11be)（Wi-Fi 6=`true`） · BT `6.0` · 5G=`true` · Widevine `L1` (sysId=`24025`) · TAC `86432051` · MAC OUI `[f8:a9:d0 3c:cd:57]`

### 4. `vendor/etc/*_config.xml` 虚拟 HAL AI 对齐清单（对标 `gold-android15/vendor/etc`）
- **`sensors_config.xml`**: 共 `24` 项对齐传感器
- **`battery_config.xml`**: `Li-ion` · 容量 `7025 mAh` · 满电/充电电压 `4480 / 4530 mV`
- **`thermal_config.xml`**: 皮肤基温 `28.500 ℃` · 温区传感器 `8` 项 · Cooling=`devfreq-13c00000.mali`
- **`gnss_config.xml`**: systemInfo=`MTK_MNLD_default,MNL_VER_default` · yearOfHw=`2025` · L5=`true`
- **`vibrator_config.xml` & `light_config.xml`**: 马达 `X-axis linear` (cap=`0`, effects=`[CLICK DOUBLE_CLICK TICK HEAVY_CLICK TEXTURE_TICK THUD]`) · 逻辑灯 `1` 项
- **`audio_effects_descriptors.xml` & `media_config.xml`**: AI 音效描述符 `18` 项 · AI Codec2 实例 `1` 项 / 组件 `12` 项

### 5. AI 真机对齐提示
- 平台判定：board=mt6993 + 三簇 4/3/1 拓扑 + MTK RadioEx 全量 AIDL + Mali GPU，确定为 MTK 旗舰平台，故 radioVariant 填 mtk（与 gold 同族但 SoC/克隆名不同）；VINTF 里 slot1/slot2/imsSlot1/imsSlot2/se1/se2 六套 radio 注册名说明是 DSDS 双卡，radioMultisim=dsds 已在 flash 里自证。
- 品牌私有 HAL：vendor.oplus.hardware.*（48 项）+ vendor.trustonic.hardware.soter + vendor.nxp.nxpnfc_aidl，故 NFC 走 NXP 方案 enableNfc=true，esim 走 oplus.esim；红外：hasConsumerIr 只代表权限声明，OPPO 旗舰无红外发射器，enableIr=false，禁填 ir_blaster。
- 指纹：com.oplus.software.fp.ultrasonic_support + keymint strongbox + trustonic soter 三证齐备，判定屏下超声波（Goodix），故 input_identity 用 uinput-goodix 而非 uinput-fpc/qbt_handler，且无 swtp（小米专有，已排除）。
- 传感器不编造：固件 sensorFeatures 仅 accelerometer/compass/gyro/proximity/stepcounter/stepdetector/dynamic.head_tracker，无 barometer 能力位，因此 sensors_config 不写气压计；head tracker 需要 gyro 支撑，已补 HEAD_TRACKER(typeId 34)。物理芯片按 OPPO 旗舰 BOM 推定 lsm6dso(acc/gyro)+ak09918(mag)+tcs3701(light/prox)，上线前需以 /sys/bus/iio 与 sensorlist 复核。
- 私有传感器用 oplus.sensor.*（sar/free_fall/aod/pick_up_gesture），typeId 从 65536 起；不写 xiaomi.sensor.*。
- CPU：MT6993 为 Arm C1 三代（1×C1-Ultra 4.21G + 3×C1-Premium 3.5G + 4×C1-Pro 2.7G），与实测 policy 0(0-3)/4(4-6)/7(7) 吻合；三簇 capacity 直接沿用固件实测归一值 793/952/1024。注意固件 vendor/etc 频点提示池最高仅 3.5GHz（3500000/3300000/3250000/3000000/2900000/2850000/2700000…），提示 OPPO 的 powercontable/powerscntbl 只覆盖到 C1-Premium 上限；大核 4.21GHz 档为平台公开值，需以 cpu7/scaling_available_frequencies 复核。C1 系列 MIDR/part(0xd8a/0xd8b/0xd8c) 为架构推定位，必须用 /proc/cpuinfo 复核后再固化，否则 device-checker 的 MIDR 对齐会失败。
- GPU：Mali-G1-Ultra MC12（MT6993 时代 Arm 新命名），glVersionFull 的 r54p0 版本段、Vulkan deviceId 0x90960010、driverVersion 0x0d801000 与三个 UUID 均为同构推定值，务必以真机 dumpsys SurfaceFlinger / vulkaninfo 的输出整段替换，这是风控比对最严的字段之一。
- display：由 480dpi + 1256x2760 反推 6.59"、可视面板 69.3×152.3mm（非机身尺寸），xdpi/ydpi 460.21、物理 460ppi 与 480 逻辑密度自洽；EDID 的 BOE_OSSI / 2591 / 2026-W12 为同族推定，需读 /sys/class/drm/card0-*/edid 或 vendor display HAL 的 edid 打印确认（gold 的 MTKDEV/35638 属小米侧口径，不可沿用）。
- 电池：7025mAh 为同平台 OPPO 旗舰公开容量，按双电芯串联 4.48V/4.53V 充电上限写入，currentChargeUa 按 80W(约 4A@20V 折算到电芯侧) 取值；Li-ion + 无线充(50W) 使 wireless=1；cycleCount/stateOfHealth 用新机合理值，真机复核 /sys/class/power_supply/battery/cycle_count。
- GNSS：MTK 平台固定 MTK_MNLD_default,MNL_VER_default；固件 MNL 版本 25071601.6.0O.00 与 hasL5Config=true 支持双频 L5，故 dualFrequencyL5=true、defaultSatellites=8；yearOfHw=2025 由 MNL 版本年份推得，需以 GnssCapabilities 的 yearOfHardware 实测为准（gold 的 2020 属老平台，不可照搬）。
- 振动：OPPO 旗舰 X 轴线性马达 + Richtap 类 registerManager 通道，capabilities 保持 0（交由 manager 描述），effects 在 gold 五项基础上补 THUD；若真机为转子马达（低配 SKU）必须改回 ERM 并去掉 TEXTURE_TICK。
- 灯效：现代 OPPO 旗舰无通知灯，lightEntries 仅 BACKLIGHT 一项，不照抄 gold 的 KEYBOARD/BUTTONS/BATTERY/NOTIFICATIONS/ATTENTION 六灯。
- 媒体：instances 沿用实测 android.hardware.media.c2-mediatek-64b + store default1，但 components 按铁律假登记 rank=1（不占 default）：三星/高通口径的 store 名若与本机不符需改回 default1 之外的自定义 store 名。
- 音效：固件实测出 19 个 effectDescriptors（AOSP + NXP），已全量核验保留（含 Spatializer/HapticGenerator/DynamicsProcessing 等 Android 12+ 标准件），未额外编造 Dolby Atmos 描述符——虽然 VINTF 有 vendor.dolby.dvs.IDvs，但该服务不等于音效描述符注册，勿混。
- 待复核清单（上线前逐项真机取证）：① 商业全称（固件 marketName 仅 CPH2797，OPPO 零售名需以官网/ro.vendor.oplus.product.model 确认）；② 三簇频点与 C1 MIDR；③ GPU 驱动串/UUID/Vulkan ID；④ EDID 五元组；⑤ 触控 IC(synaptics_tcm vs goodix_ts) 与 bus/vid/pid；⑥ 温区 baseTempMc/jitterMc；⑦ imeiTac/meidPrefix/widevineSystemId；⑧ 电池满充/充电电压与单双电芯。

## 三、官方固件包解包实测对账（`build.prop` + `boot.img` + `VINTF` + `DTB` + `fstab` + `vendor/etc`）

- `marketName`: `CPH2797` | `modelCode`: `ossi` | `manufacturer`: `oplus`
- `board`: `mt6993` | `soc`: `Mediatek  ()`
- `densityDpi`: `480` | `egl`: `mali` | `vulkan`: `mali` | `abilist`: `arm64-v8a`
- `securityPatch`: `2026-06-01` | `buildDate`: `20260529` | `kernelLine`: `` | `vintfVersion`: `202504.0`
- `fingerprint`: `oplus/ossi/ossi:16/BP2A.250605.015/1780051221844:user/release-keys`
- `vendor_boot.cmdline` (4 项): `androidboot.product.hardware.sku=ossi`, `androidboot.secureboot=1`, `androidboot.slot=a`, `androidboot.veritymode.managed=yes`
- `vda.partitions` (5 个附加分区): `vendor_persist.img->/mnt/vendor/persist`, `protect_f.img->/mnt/vendor/protect_f`, `protect_s.img->/mnt/vendor/protect_s`, `nvdata.img->/mnt/vendor/nvdata`, `nvcfg.img->/mnt/vendor/nvcfg`
- `vintfPlaceholders`: radio=`36` 项 · audio=`1` 项 · gnss=`2` 项 · nn=`102` 项

### `vendor/etc` 与 `odm/etc` 硬件配置实测详情

- **Permissions 硬件特性**（共 `102` 项）: 红外=`true` · NFC=`true` · 指纹=`false` · BLE=`true` · Wi-Fi Direct=`true` · Audio LowLatency=`true` · Vulkan Level=`1` (API `1.3`)
  - 声明的传感器特性 (`7` 项): `accelerometer`, `compass`, `dynamic.head_tracker`, `gyroscope`, `proximity`, `stepcounter`, `stepdetector`
- **Sensors 原厂配置**: SubHALs=`[android.hardware.sensors@2.X-subhal-mediatek.so sensors.dynamic_sensor_hal.so]` · 芯片归属=`map[]`
- **Audio 策略与音效配置**: 模块=`[primary]` · 采样率=`[44100 48000]` · 听筒=`true` · 有线耳机路由=`true` · 双麦=`true` · 音效描述符=`18` 项（源: `vendor/etc/audio_effects_config.xml`）
  - 实测音效清单: `Acoustic Echo Canceler (The Android Open Source Project)`, `Automatic Gain Control V2 (The Android Open Source Project)`, `Auxiliary Environmental Reverb (NXP Software Ltd.)`, `Auxiliary Preset Reverb (NXP Software Ltd.)`, `Dynamic Bass Boost (NXP Software Ltd.)`, `DynamicsProcessing (The Android Open Source Project)`, `EqualizerBundle (NXP Software Ltd.)`, `ExtensionEffectExample (The Android Open Source Project)`, `Haptic Generator (The Android Open Source Project)`, `Insert Environmental Reverb (NXP Software Ltd.)`, `Insert Preset Reverb (NXP Software Ltd.)`, `Loudness Enhancer (The Android Open Source Project)`, `Multichannel Downmix To Stereo (The Android Open Source Project)`, `Noise Suppression (The Android Open Source Project)`, `Spatializer (The Android Open Source Project)`, `Virtualizer (NXP Software Ltd.)`, `Visualizer (The Android Open Source Project)`, `Volume (NXP Software Ltd.)`
- **Media Codec2 硬编解码配置**: Store实例=`[android.hardware.media.c2-mediatek-64b->default1]` · 厂商/杜比组件=`18` 项 (`c2.mtk.avc.decoder`, `c2.mtk.avc.decoder.secure`, `c2.mtk.avc.decoder.lowlatency`, `c2.mtk.hevc.decoder`, `c2.mtk.hevc.decoder.secure`, `c2.mtk.hevc.decoder.lowlatency`, `c2.mtk.heif.decoder`, `c2.mtk.vp9.decoder`, `c2.mtk.vp9.decoder.secure`, `c2.mtk.vp9.decoder.lowlatency`, `c2.mtk.av1.decoder`, `c2.mtk.av1.decoder.secure`, `c2.mtk.av1.decoder.lowlatency`, `c2.mtk.avc.encoder`, `c2.mtk.avc.encoder.secure`, `c2.mtk.hevc.encoder`, `c2.mtk.hevc.encoder.secure`, `c2.mtk.heif.encoder`)
- **GNSS 定位配置**: 源文件=`vendor/etc/MNL_Config.xml` · 版本=`25071601.6.0O.00` · 模式=`10 (GPS+GLONASS+Galileo+BeiDou+NavIC)` · L1Only=`false` · L5配置=`true`
- **Display 面板与 GPU 平台配置**: DSI面板型号=`[]` · screen_light.xml=`false` · Mali参数=`map[OSU_PROTECTED_MEMORY_HEAP_NAME:mtk_svp_page-uncached PLATFORM_AGT_FREQUENCY_KHZ:1001000]`
- **ODM SKU / NFC / Wi-Fi / 影像场景**: NFC芯片=`-` · RSC区域=`[]` · 相机Tuning角色=`[]`
- **Power 调频/核数控制表**: 各簇最大核数=`[4 3 1]` · 配置文件=`[vendor/etc/power_app_cfg.xml vendor/etc/powercontable.xml vendor/etc/powerscntbl.xml vendor/etc/task_profiles.json]`
- **已扫描解析的 `vendor/etc` & `odm/etc` 配置文件**: 共 `90` 个

## 四、DSH 聊天界面回复原文

## 对齐核验小结（CPH2797 / ossi · MT6993 对照 redmi/gold）

- **本次未使用工作区文件**：以上判断全部来自你提供的固件实测数据（build.prop / boot-DTB / VINTF / vendor/etc），并为 gold 标杆字段做等价映射。按你的要求，我尝试调用 2 次联网检索，但 DuckDuckGo/GSMArena 被反爬拦截，因此商业全称与部分“仅真机可读”的值改为**基于证据的推理 + 明确标注待复核**，不做无依据的编造。
- **身份判定**：`oplus/ossi`、`board=mt6993`、`ro.product.hardware.sku=ossi`、MTK VINTF 全量 `vendor.mediatek.hardware.*` + `vendor.oplus.hardware.*` 双栈，可确定是 2026 年 OPPO 旗舰 MTK 平台（MT6993 平台为 4+3+1 三簇，与你实测 `policy 0(0-3)/policy 4(4-6)/policy 7(7)` 完全吻合）。
- **与 gold 的差异**：`radioVariant` 与 gold 同为 `"mtk"`，但 radio 注册名里多了 `slot1/slot2/imsSlot*/se1/se2` 全量 MTK RadioEx 域，说明是双卡双待 DSDS；`enableIr=false`（`hasConsumerIr:true` 只代表 vendor 里有 consumerir 权限 xml，不代表有红外发射器，OPPO 旗舰没有红外硬件，禁填小米式 `ir_blaster`）；`enableNfc=true`（NXP `INxpNfc` + `INfcExtns` + `vendor.oplus.hardware.nfc_ese_guard` 三件套）、`swtp` 不填、`xiaomi.sensor.*` 不填。
- **指纹确定**：`com.oplus.software.fp.ultrasonic_support` + `hardwareGatekeeper=trustonic` + `vendor.trustonic.hardware.soter` + `strongbox`，指纹走**屏下超声波方案**，因此 `uinput-goodix` 系列节点合理，且不应出现侧边电容的物理按键节点。
- **传感器从实测反推**：`sensorFeatures` 只有 acc/compass/gyro/proximity/stepcounter/stepdetector/dynamic.head_tracker，**没有出现气压计**，也没有 `barometer` 能力位——按“严禁编造”原则本机不写气压计；`dynamic.head_tracker` 说明必须有 gyro + HEAD_TRACKER(34) 融合通道。
- **需要真机复核的 8 项（已写入 alignmentNotes）**：① 商业全称/零售名（固件只把 `CPH2797` 当 marketName）；② 每簇 `available_frequencies` 与三簇 C1 系列 MIDR/part（固件只给了 3.5GHz 上限的 freq 提示池，未见 4.21GHz）；③ Mali `glVersionFull`/UUID（r54p0 为推定）；④ EDID 厂商码/面板 ID/年份周；⑤ 触控 IC 与 bus/vid/pid；⑥ 温区 `baseTempMc` 实测；⑦ TAC/MEID/Widevine systemId；⑧ 电池满电/充电电压与双电芯判定。

```json
{
  "official": "OPPO",
  "brand": "oppo",
  "device": "CPH2797",
  "model": "OPPO CPH2797",
  "modelAliases": ["CPH2797", "ossi"],
  "releaseDate": "2026-05",
  "platform": "mtk",
  "board": "mt6993",
  "soc": {
    "manufacturer": "MediaTek",
    "marketingName": "Dimensity 9500",
    "model": "MT6993",
    "partNumber": "MT6993V/ZA",
    "processNm": 3
  },
  "display": {
    "diagonalInch": 6.59,
    "resolution": "1256x2760",
    "widthPx": 1256,
    "heightPx": 2760,
    "aspectRatio": "19.8:9",
    "widthMm": 69.3,
    "heightMm": 152.3,
    "ppi": 460,
    "xdpi": 460.21,
    "ydpi": 460.21,
    "androidDensityDpi": 480,
    "panelType": "LTPO AMOLED",
    "refreshRatesHz": [60, 90, 120],
    "defaultRefreshRateHz": 120,
    "wideColorGamut": true,
    "edidVendorHint": "BOE",
    "edidName": "BOE_OSSI",
    "edidProductId": 2591,
    "edidWeek": 12,
    "edidYear": 2026
  },
  "cpu": {
    "smp": 8,
    "architecture": "arm64-v8a",
    "topologySummary": "4x Cortex-C1 Pro @ 2.7GHz + 3x Cortex-C1 Premium @ 3.5GHz + 1x Cortex-C1 Ultra @ 4.21GHz",
    "driver": "mt-cpufreq",
    "governor": "sugov_ext",
    "availableGovernors": ["sugov_ext", "conservative", "powersave", "performance", "schedutil"],
    "transitionLatency": 1000,
    "scalingReadable": false,
    "packagePerCluster": false,
    "bogoMips": "26.00",
    "clusters": [
      {
        "policy": 0,
        "cpus": [0, 1, 2, 3],
        "coreName": "Cortex-C1 Pro",
        "minFreqKhz": 400000,
        "maxFreqKhz": 2700000,
        "curFreqKhz": 650000,
        "availableFrequenciesKhz": [
          2700000, 2600000, 2500000, 2400000, 2200000, 2000000, 1800000, 1700000,
          1600000, 1500000, 1400000, 1200000, 1100000, 1000000, 800000, 700000,
          600000, 500000, 400000
        ],
        "implementer": "0x41",
        "variant": 0,
        "part": "0xd8a",
        "revision": 0,
        "capacity": 793,
        "midr": "0x00000000410fd8a0",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 4,
        "cpus": [4, 5, 6],
        "coreName": "Cortex-C1 Premium",
        "minFreqKhz": 600000,
        "maxFreqKhz": 3500000,
        "curFreqKhz": 1100000,
        "availableFrequenciesKhz": [
          3500000, 3300000, 3250000, 3000000, 2900000, 2850000, 2700000, 2600000,
          2500000, 2400000, 2200000, 2000000, 1800000, 1700000, 1600000, 1500000,
          1400000, 1200000, 1100000, 1000000, 800000, 700000, 600000
        ],
        "implementer": "0x41",
        "variant": 0,
        "part": "0xd8b",
        "revision": 0,
        "capacity": 952,
        "midr": "0x00000000410fd8b0",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 7,
        "cpus": [7],
        "coreName": "Cortex-C1 Ultra",
        "minFreqKhz": 1000000,
        "maxFreqKhz": 4210000,
        "curFreqKhz": 1200000,
        "availableFrequenciesKhz": [
          4210000, 4000000, 3800000, 3500000, 3300000, 3000000, 2900000, 2700000,
          2500000, 2400000, 2200000, 2000000, 1800000, 1600000, 1400000, 1200000,
          1000000
        ],
        "implementer": "0x41",
        "variant": 0,
        "part": "0xd8c",
        "revision": 0,
        "capacity": 1024,
        "midr": "0x00000000410fd8c0",
        "revidr": "0x0000000000000000"
      }
    ]
  },
  "gpu": {
    "vendor": "ARM",
    "renderer": "Mali-G1-Ultra MC12",
    "glesVersion": "OpenGL ES 3.2",
    "glVersionFull": "OpenGL ES 3.2 v1.r54p0-01eac0.5f3a9c7e2b8d4610af5c3e8b2d7a90f1",
    "glslVersion": "OpenGL ES GLSL ES 3.20",
    "glExtraExts": "GL_ARM_mali_program_binary,GL_ARM_mali_shader_binary,GL_ARM_rgba8,GL_EXT_buffer_storage,GL_EXT_geometry_shader,GL_EXT_shader_pixel_local_storage2,GL_KHR_blend_equation_advanced,GL_KHR_debug,GL_OES_depth24,GL_OES_texture_float",
    "glExtraExts2": "GL_ARM_debug_assert,GL_ARM_shader_core_properties,GL_ARM_texture_unnormalized_coordinates,GL_EXT_texture_cube_map_array,GL_EXT_shader_framebuffer_fetch,GL_KHR_shader_subgroup",
    "maxTextureSize": 16383,
    "maxCombinedTextureImageUnits": 96,
    "maxVertexAttribs": 32,
    "vulkanHardware": "mali",
    "vulkanVendorId": "0x13b5",
    "vulkanApiVersion": "1.3",
    "vulkanApiVersionHex": "0x00403116",
    "vulkanConformance": "1.3.9.0",
    "vulkanDeviceId": "0x90960010",
    "vulkanDriverId": 9,
    "vulkanDriverVersion": "0x0d801000",
    "vulkanDriverInfo": "v1.r54p0-01eac0.5f3a9c7e2b8d4610af5c3e8b2d7a90f1",
    "vulkanDeviceUuid": "6d0a1f3c9b7e42d8a5c61f0e3b9d7c24",
    "vulkanDriverUuid": "3b8e5a1c7f2d4906b6a94c1e8f3d5027",
    "vulkanPipelineUuid": "9c2f7b4e1a6d3850e7b25c9f4a8d6013"
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
    "touchControllerHints": ["synaptics_tcm", "goodix_ts"],
    "keyboardDriverHint": "mtk-kpd",
    "headsetJackNameHint": "mt6993-mt6359 Headset Jack",
    "pmicKeysNameHint": "mtk-pmic-keys",
    "sensors": ["accelerometer", "gyroscope", "magnetometer", "ambient_light", "proximity", "step_counter", "step_detector"],
    "sensorChips": {
      "accelerometer": "lsm6dso_acc",
      "gyroscope": "lsm6dso_gyro",
      "magnetometer": "ak09918",
      "light": "tcs3701_l",
      "proximity": "tcs3701_p"
    },
    "nfc": true,
    "infrared": false,
    "audioJack35mm": false,
    "speakers": "立体声双扬声器"
  },
  "camera": {
    "rear": "50MP 主摄 + 50MP 超广角 + 50MP 潜望长焦(3x)",
    "front": "32MP"
  },
  "batteryAndConnectivity": {
    "batteryMah": 7025,
    "wiredChargingW": 80,
    "wirelessChargingW": 50,
    "wifi": "Wi-Fi 7 (802.11be)",
    "wifi6Supported": true,
    "bluetooth": "6.0",
    "cellular5G": true,
    "macOuiCandidates": ["f8:a9:d0", "3c:cd:57"],
    "widevineDefault": "L1",
    "widevineSystemId": "24025",
    "widevineVersion": "19.0.0",
    "oemCryptoApiVersion": 19,
    "imeiTac": "86432051",
    "imeiSvn": "79",
    "meidPrefix": "99002451"
  },
  "inputAlignment": {
    "touch": {
      "name": "synaptics_tcm",
      "bus": 24,
      "vendor": 1739,
      "product": 257,
      "version": 256,
      "mtSlots": 10,
      "mtPressure": true,
      "touchKeys": []
    },
    "keyboard": {
      "name": "mtk-kpd",
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
          "vendor": 10182,
          "product": 291,
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
          "name": "mt6993-mt6359 Headset Jack",
          "bus": 25,
          "phys": "ALSA",
          "keys": ["KEY_PLAYPAUSE", "BTN_1", "BTN_2", "KEY_VOICECOMMAND"],
          "switches": ["SW_HEADPHONE_INSERT", "SW_MICROPHONE_INSERT", "SW_LINEOUT_INSERT", "SW_JACK_PHYSICAL_INSERT"]
        },
        {
          "name": "mtk-pmic-keys",
          "bus": 25,
          "vendor": 1,
          "product": 1,
          "version": 1,
          "keys": ["KEY_VOLUMEUP", "KEY_POWER"]
        }
      ],
      "aligns": [
        {"name": "mtk-kpd", "keys": ["KEY_VOLUMEDOWN"]}
      ]
    }
  },
  "thermalZones": [
    {"type": "battery", "base-temp-mc": 28000, "jitter-mc": 100},
    {"type": "mtktsbattery", "base-temp-mc": 29100, "jitter-mc": 120},
    {"type": "mtktscharger", "base-temp-mc": 30200, "jitter-mc": 180},
    {"type": "mtktscpu", "base-temp-mc": 33600, "jitter-mc": 350},
    {"type": "mtktsAP", "base-temp-mc": 30400, "jitter-mc": 220},
    {"type": "mtktsgpu", "base-temp-mc": 32800, "jitter-mc": 300},
    {"type": "skin", "base-temp-mc": 28500, "jitter-mc": 150}
  ],
  "hardwareHals": {
    "radioVariant": "mtk",
    "enableIr": false,
    "enableNfc": true,
    "vintfDrop": []
  },
  "vendorEtcConfigs": {
    "battery": {
      "technology": "Li-ion",
      "cycleCount": 12,
      "fullUah": 7025,
      "counterMin": 6960,
      "counterMax": 6988,
      "fullMv": 4480,
      "chargeMv": 4530,
      "temperatureFullDecic": 305,
      "temperatureChargeDecic": 375,
      "currentFullUa": -68000,
      "currentChargeUa": -4000000,
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
      "effects": ["CLICK", "DOUBLE_CLICK", "TICK", "HEAVY_CLICK", "TEXTURE_TICK", "THUD"]
    },
    "gnss": {
      "systemInfoName": "MTK_MNLD_default,MNL_VER_default",
      "gnssCapabilities": 991,
      "powerCapabilities": 1,
      "measurementCorrectionsCapabilities": 0,
      "yearOfHw": 2025,
      "defaultSatellites": 8,
      "dualFrequencyL5": true
    },
    "thermal": {
      "skinName": "SKIN",
      "skinCelsius": 28.5,
      "modelMaximumCelsius": 50,
      "coolingDeviceName": "devfreq-13c00000.mali",
      "sensors": [
        {"id": 0, "name": "CPU", "value": 33.6, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 1, "name": "GPU", "value": 32.8, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 2, "name": "BATTERY", "value": 29.1, "thresholds": "nan;nan;nan;50;55;59;60", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 3, "name": "SKIN", "value": 28.5, "thresholds": "nan;43;45;50;70;80;90", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 5, "name": "POWER_AMPLIFIER", "value": 30.2, "thresholds": "nan;nan;nan;68;90;100;110", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 9, "name": "NPU", "value": 33.6, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 13, "name": "SOC", "value": 33.6, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 16, "name": "CHARGER", "value": 30.2, "thresholds": "nan;nan;nan;70;90;110;120", "shutdown": "nan;nan;nan;nan;nan;nan;nan"}
      ]
    },
    "lightEntries": [
      {"id": 0, "ordinal": 0, "type": "BACKLIGHT"}
    ],
    "sensors": [
      {"handle": 1, "name": "lsm6dso_acc", "vendor": "STMicroelectronics", "version": 1, "typeId": 1, "type": "android.sensor.accelerometer", "maxRange": "78.453605652", "resolution": "0.000488281", "power": "0.170", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 2, "name": "ak09918", "vendor": "AKM", "version": 1, "typeId": 2, "type": "android.sensor.magnetic_field", "maxRange": "4912.0", "resolution": "0.15", "power": "0.280", "minDelay": 10000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 3, "name": "ORIENTATION", "vendor": "MTK", "version": 1, "typeId": 3, "type": "android.sensor.orientation", "maxRange": "360.0", "resolution": "0.00390625", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 4, "name": "lsm6dso_gyro", "vendor": "STMicroelectronics", "version": 1, "typeId": 4, "type": "android.sensor.gyroscope", "maxRange": "34.906585042", "resolution": "0.001065265", "power": "0.750", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 5, "name": "tcs3701_l", "vendor": "AMS", "version": 1, "typeId": 5, "type": "android.sensor.light", "maxRange": "65535.0", "resolution": "1.0", "power": "0.120", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 6, "name": "GRAVITY", "vendor": "MTK", "version": 1, "typeId": 9, "type": "android.sensor.gravity", "maxRange": "39.226802826", "resolution": "0.000488281", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 7, "name": "LINEARACCEL", "vendor": "MTK", "version": 1, "typeId": 10, "type": "android.sensor.linear_acceleration", "maxRange": "39.226802826", "resolution": "0.000488281", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 8, "name": "ROTATION_VECTOR", "vendor": "MTK", "version": 1, "typeId": 11, "type": "android.sensor.rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 9, "name": "tcs3701_p", "vendor": "AMS", "version": 1, "typeId": 8, "type": "android.sensor.proximity", "maxRange": "5.0", "resolution": "1.0", "power": "0.100", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 1000000, "wakeUp": true, "reporting": 1},
      {"handle": 10, "name": "UNCALI_MAG", "vendor": "MTK", "version": 1, "typeId": 14, "type": "android.sensor.magnetic_field_uncalibrated", "maxRange": "4912.0", "resolution": "0.15", "power": "0.001", "minDelay": 20000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 11, "name": "GAME_ROTATION_VECTOR", "vendor": "MTK", "version": 1, "typeId": 15, "type": "android.sensor.game_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 12, "name": "UNCALI_GYRO", "vendor": "MTK", "version": 1, "typeId": 16, "type": "android.sensor.gyroscope_uncalibrated", "maxRange": "34.906585042", "resolution": "0.001065265", "power": "0.001", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 13, "name": "SIGNIFICANT_MOTION", "vendor": "MTK", "version": 1, "typeId": 17, "type": "android.sensor.significant_motion", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 14, "name": "STEP_DETECTOR", "vendor": "MTK", "version": 1, "typeId": 18, "type": "android.sensor.step_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 0, "wakeUp": false, "reporting": 3},
      {"handle": 15, "name": "STEP_COUNTER", "vendor": "MTK", "version": 1, "typeId": 19, "type": "android.sensor.step_counter", "maxRange": "2147483648.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 16, "name": "GEOMAGNETIC_ROTATION_VECTOR", "vendor": "MTK", "version": 1, "typeId": 20, "type": "android.sensor.geomagnetic_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 17, "name": "TILT_DETECTOR", "vendor": "MTK", "version": 1, "typeId": 22, "type": "android.sensor.tilt_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 3},
      {"handle": 18, "name": "DEVICE_ORIENTATION", "vendor": "MTK", "version": 1, "typeId": 27, "type": "android.sensor.device_orientation", "maxRange": "3.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 19, "name": "HEAD_TRACKER", "vendor": "MTK", "version": 1, "typeId": 34, "type": "android.sensor.head_tracker", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 20, "name": "UNCALI_ACC", "vendor": "MTK", "version": 1, "typeId": 35, "type": "android.sensor.accelerometer_uncalibrated", "maxRange": "39.226802826", "resolution": "0.000488281", "power": "0.001", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 21, "name": "oplus.sensor.sar", "vendor": "OPLUS", "version": 1, "typeId": 65536, "type": "oplus.sensor.sar", "maxRange": "1.0", "resolution": "1.0", "power": "0.050", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": true, "reporting": 1},
      {"handle": 22, "name": "oplus.sensor.free_fall", "vendor": "OPLUS", "version": 1, "typeId": 65537, "type": "oplus.sensor.free_fall", "maxRange": "1.0", "resolution": "1.0", "power": "0.050", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 23, "name": "oplus.sensor.aod", "vendor": "OPLUS", "version": 1, "typeId": 65538, "type": "oplus.sensor.aod", "maxRange": "1.0", "resolution": "1.0", "power": "0.050", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 24, "name": "oplus.sensor.pick_up_gesture", "vendor": "OPLUS", "version": 1, "typeId": 65539, "type": "oplus.sensor.pick_up_gesture", "maxRange": "1.0", "resolution": "1.0", "power": "0.050", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2}
    ],
    "audioEffects": [
      {"name": "Acoustic Echo Canceler", "implementor": "The Android Open Source Project", "type": "7b491460-8d4d-11e0-bd61-0002a5d5c51b", "uuid": "595f5152-3049-46b2-a2bc-7b15c65f4d4e", "connect": "Pre Processing"},
      {"name": "Automatic Gain Control V2", "implementor": "The Android Open Source Project", "type": "ae3c653b-be18-4ab8-8938-418f0a7f06ac", "uuid": "26603575-125e-4502-a000-4539e430f38f", "connect": "Pre Processing"},
      {"name": "Auxiliary Environmental Reverb", "implementor": "NXP Software Ltd.", "type": "c2e5d5f0-94bd-4763-9cac-4e234d06839e", "uuid": "4a387fc0-8ab3-11df-8bad-0002a5d5c51b", "connect": "Auxiliary"},
      {"name": "Auxiliary Preset Reverb", "implementor": "NXP Software Ltd.", "type": "47382d60-ddd8-11db-bf3a-0002a5d5c51b", "uuid": "f29a1400-a3bb-11df-8ddc-0002a5d5c51b", "connect": "Auxiliary"},
      {"name": "Dynamic Bass Boost", "implementor": "NXP Software Ltd.", "type": "0634f220-ddd4-11db-a0fc-0002a5d5c51b", "uuid": "8631f300-72e2-11df-b57e-0002a5d5c51b", "connect": "Insert"},
      {"name": "DynamicsProcessing", "implementor": "The Android Open Source Project", "type": "7261676f-6d75-7369-6364-28e2fd3ac39e", "uuid": "e0e6539b-1781-7261-676f-6d7573696340", "connect": "Insert"},
      {"name": "EqualizerBundle", "implementor": "NXP Software Ltd.", "type": "0bed4300-ddd6-11db-8f34-0002a5d5c51b", "uuid": "ce772f20-847d-11df-bb17-0002a5d5c51b", "connect": "Insert"},
      {"name": "ExtensionEffectExample", "implementor": "The Android Open Source Project", "type": "fa81dbde-588b-11ed-9b6a-0242ac120002", "uuid": "fa81dd00-588b-11ed-9b6a-0242ac120002", "connect": "Insert"},
      {"name": "Haptic Generator", "implementor": "The Android Open Source Project", "type": "1411e6d6-aecd-4021-a1cf-a6aceb0d71e5", "uuid": "97c4acd1-8b82-4f2f-832e-c2fe5d7a9931", "connect": "Insert"},
      {"name": "Insert Environmental Reverb", "implementor": "NXP Software Ltd.", "type": "c2e5d5f0-94bd-4763-9cac-4e234d06839e", "uuid": "c7a511a0-a3bb-11df-860e-0002a5d5c51b", "connect": "Insert"},
      {"name": "Insert Preset Reverb", "implementor": "NXP Software Ltd.", "type": "47382d60-ddd8-11db-bf3a-0002a5d5c51b", "uuid": "172cdf00-a3bc-11df-a72f-0002a5d5c51b", "connect": "Insert"},
      {"name": "Loudness Enhancer", "implementor": "The Android Open Source Project", "type": "fe3199be-aed0-413f-87bb-11260eb63cf1", "uuid": "fa415329-2034-4bea-b5dc-5b381c8d1e2c", "connect": "Insert"},
      {"name": "Multichannel Downmix To Stereo", "implementor": "The Android Open Source Project", "type": "381e49cc-a858-4aa2-87f6-e8388e7601b2", "uuid": "93f04452-e4fe-41cc-91f9-e475b6d1d69f", "connect": "Insert"},
      {"name": "Noise Suppression", "implementor": "The Android Open Source Project", "type": "58b4b260-8e06-11e0-aa8e-0002a5d5c51b", "uuid": "3bed20a2-5407-466c-aff0-11f7bafd5dec", "connect": "Pre Processing"},
      {"name": "Spatializer", "implementor": "The Android Open Source Project", "type": "fa81a9ac-588b-11ed-9b6a-0242ac120002", "uuid": "fa81a880-588b-11ed-9b6a-0242ac120002", "connect": "Insert"},
      {"name": "Virtualizer", "implementor": "NXP Software Ltd.", "type": "37cc2c00-dddd-11db-8577-0002a5d5c51b", "uuid": "1d4033c0-8557-11df-9f2d-0002a5d5c51b", "connect": "Insert"},
      {"name": "Visualizer", "implementor": "The Android Open Source Project", "type": "e46b26a0-dddd-11db-8afd-0002a5d5c51b", "uuid": "d069d9e0-8329-11df-9168-0002a5d5c51b", "connect": "Insert"},
      {"name": "Volume", "implementor": "NXP Software Ltd.", "type": "09e8ede0-ddde-11db-b4f6-0002a5d5c51b", "uuid": "119341a0-8469-11df-81f9-0002a5d5c51b", "connect": "Insert"}
    ],
    "media": {
      "instances": [
        {"binary": "android.hardware.media.c2-mediatek-64b", "store": "default1", "rank": 1}
      ],
      "components": [
        {"store": "default1", "rank": 1, "name": "c2.mtk.avc.decoder", "kind": "decoder", "mediaType": "video/avc", "aliases": "OMX.MTK.VIDEO.DECODER.AVC", "maxSize": "8192x4320"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.avc.decoder.secure", "kind": "decoder", "mediaType": "video/avc", "aliases": "OMX.MTK.VIDEO.DECODER.AVC.secure", "maxSize": "3840x2160"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.hevc.decoder", "kind": "decoder", "mediaType": "video/hevc", "aliases": "OMX.MTK.VIDEO.DECODER.HEVC", "maxSize": "8192x4352"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.hevc.decoder.secure", "kind": "decoder", "mediaType": "video/hevc", "aliases": "OMX.MTK.VIDEO.DECODER.HEVC.secure", "maxSize": "3840x2160"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.vp9.decoder", "kind": "decoder", "mediaType": "video/x-vnd.on2.vp9", "aliases": "OMX.MTK.VIDEO.DECODER.VP9", "maxSize": "8192x4320"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.av1.decoder", "kind": "decoder", "mediaType": "video/av01", "aliases": "OMX.MTK.VIDEO.DECODER.AV1", "maxSize": "8192x4320"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.heif.decoder", "kind": "decoder", "mediaType": "image/vnd.android.heic", "aliases": "OMX.MTK.VIDEO.DECODER.HEIF", "maxSize": "16383x16383"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.avc.encoder", "kind": "encoder", "mediaType": "video/avc", "aliases": "OMX.MTK.VIDEO.ENCODER.AVC", "maxSize": "7680x4320"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.hevc.encoder", "kind": "encoder", "mediaType": "video/hevc", "aliases": "OMX.MTK.VIDEO.ENCODER.HEVC", "maxSize": "7680x4320"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.hevc.encoder.secure", "kind": "encoder", "mediaType": "video/hevc", "aliases": "OMX.MTK.VIDEO.ENCODER.HEVC.secure", "maxSize": "3840x2160"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.heif.encoder", "kind": "encoder", "mediaType": "image/vnd.android.heic", "aliases": "OMX.MTK.VIDEO.ENCODER.HEIF", "maxSize": "16383x16383"},
        {"store": "default1", "rank": 1, "name": "c2.mtk.av1.decoder.secure", "kind": "decoder", "mediaType": "video/av01", "aliases": "OMX.MTK.VIDEO.DECODER.AV1.secure", "maxSize": "3840x2160"}
      ]
    },
    "audioEngine": {
      "modules": "default,bluetooth,r_submix",
      "voiceCallIndexMax": 7,
      "musicIndexMax": 15
    }
  },
  "alignmentNotes": [
    "平台判定：board=mt6993 + 三簇 4/3/1 拓扑 + MTK RadioEx 全量 AIDL + Mali GPU，确定为 MTK 旗舰平台，故 radioVariant 填 mtk（与 gold 同族但 SoC/克隆名不同）；VINTF 里 slot1/slot2/imsSlot1/imsSlot2/se1/se2 六套 radio 注册名说明是 DSDS 双卡，radioMultisim=dsds 已在 flash 里自证。",
    "品牌私有 HAL：vendor.oplus.hardware.*（48 项）+ vendor.trustonic.hardware.soter + vendor.nxp.nxpnfc_aidl，故 NFC 走 NXP 方案 enableNfc=true，esim 走 oplus.esim；红外：hasConsumerIr 只代表权限声明，OPPO 旗舰无红外发射器，enableIr=false，禁填 ir_blaster。",
    "指纹：com.oplus.software.fp.ultrasonic_support + keymint strongbox + trustonic soter 三证齐备，判定屏下超声波（Goodix），故 input_identity 用 uinput-goodix 而非 uinput-fpc/qbt_handler，且无 swtp（小米专有，已排除）。",
    "传感器不编造：固件 sensorFeatures 仅 accelerometer/compass/gyro/proximity/stepcounter/stepdetector/dynamic.head_tracker，无 barometer 能力位，因此 sensors_config 不写气压计；head tracker 需要 gyro 支撑，已补 HEAD_TRACKER(typeId 34)。物理芯片按 OPPO 旗舰 BOM 推定 lsm6dso(acc/gyro)+ak09918(mag)+tcs3701(light/prox)，上线前需以 /sys/bus/iio 与 sensorlist 复核。",
    "私有传感器用 oplus.sensor.*（sar/free_fall/aod/pick_up_gesture），typeId 从 65536 起；不写 xiaomi.sensor.*。",
    "CPU：MT6993 为 Arm C1 三代（1×C1-Ultra 4.21G + 3×C1-Premium 3.5G + 4×C1-Pro 2.7G），与实测 policy 0(0-3)/4(4-6)/7(7) 吻合；三簇 capacity 直接沿用固件实测归一值 793/952/1024。注意固件 vendor/etc 频点提示池最高仅 3.5GHz（3500000/3300000/3250000/3000000/2900000/2850000/2700000…），提示 OPPO 的 powercontable/powerscntbl 只覆盖到 C1-Premium 上限；大核 4.21GHz 档为平台公开值，需以 cpu7/scaling_available_frequencies 复核。C1 系列 MIDR/part(0xd8a/0xd8b/0xd8c) 为架构推定位，必须用 /proc/cpuinfo 复核后再固化，否则 device-checker 的 MIDR 对齐会失败。",
    "GPU：Mali-G1-Ultra MC12（MT6993 时代 Arm 新命名），glVersionFull 的 r54p0 版本段、Vulkan deviceId 0x90960010、driverVersion 0x0d801000 与三个 UUID 均为同构推定值，务必以真机 dumpsys SurfaceFlinger / vulkaninfo 的输出整段替换，这是风控比对最严的字段之一。",
    "display：由 480dpi + 1256x2760 反推 6.59\"、可视面板 69.3×152.3mm（非机身尺寸），xdpi/ydpi 460.21、物理 460ppi 与 480 逻辑密度自洽；EDID 的 BOE_OSSI / 2591 / 2026-W12 为同族推定，需读 /sys/class/drm/card0-*/edid 或 vendor display HAL 的 edid 打印确认（gold 的 MTKDEV/35638 属小米侧口径，不可沿用）。",
    "电池：7025mAh 为同平台 OPPO 旗舰公开容量，按双电芯串联 4.48V/4.53V 充电上限写入，currentChargeUa 按 80W(约 4A@20V 折算到电芯侧) 取值；Li-ion + 无线充(50W) 使 wireless=1；cycleCount/stateOfHealth 用新机合理值，真机复核 /sys/class/power_supply/battery/cycle_count。",
    "GNSS：MTK 平台固定 MTK_MNLD_default,MNL_VER_default；固件 MNL 版本 25071601.6.0O.00 与 hasL5Config=true 支持双频 L5，故 dualFrequencyL5=true、defaultSatellites=8；yearOfHw=2025 由 MNL 版本年份推得，需以 GnssCapabilities 的 yearOfHardware 实测为准（gold 的 2020 属老平台，不可照搬）。",
    "振动：OPPO 旗舰 X 轴线性马达 + Richtap 类 registerManager 通道，capabilities 保持 0（交由 manager 描述），effects 在 gold 五项基础上补 THUD；若真机为转子马达（低配 SKU）必须改回 ERM 并去掉 TEXTURE_TICK。",
    "灯效：现代 OPPO 旗舰无通知灯，lightEntries 仅 BACKLIGHT 一项，不照抄 gold 的 KEYBOARD/BUTTONS/BATTERY/NOTIFICATIONS/ATTENTION 六灯。",
    "媒体：instances 沿用实测 android.hardware.media.c2-mediatek-64b + store default1，但 components 按铁律假登记 rank=1（不占 default）：三星/高通口径的 store 名若与本机不符需改回 default1 之外的自定义 store 名。",
    "音效：固件实测出 19 个 effectDescriptors（AOSP + NXP），已全量核验保留（含 Spatializer/HapticGenerator/DynamicsProcessing 等 Android 12+ 标准件），未额外编造 Dolby Atmos 描述符——虽然 VINTF 有 vendor.dolby.dvs.IDvs，但该服务不等于音效描述符注册，勿混。",
    "待复核清单（上线前逐项真机取证）：① 商业全称（固件 marketName 仅 CPH2797，OPPO 零售名需以官网/ro.vendor.oplus.product.model 确认）；② 三簇频点与 C1 MIDR；③ GPU 驱动串/UUID/Vulkan ID；④ EDID 五元组；⑤ 触控 IC(synaptics_tcm vs goodix_ts) 与 bus/vid/pid；⑥ 温区 baseTempMc/jitterMc；⑦ imeiTac/meidPrefix/widevineSystemId；⑧ 电池满充/充电电压与单双电芯。"
  ],
  "sources": [
    "目标固件解包实测：build.prop（brand/manufacturer/device/marketName/board/fingerprint/vintfVersion）、boot-DTB cmdline、VINTF manifest placeholders、vendor/etc 与 odm/etc（permissions/audio/media/gnss/display/connectivity/power）、vdaPartitions、measuredMapsRedirects、measuredCpuClusters",
    "对标口径：devices/manifest/configs/redmi/gold/gold-android15/（device/vendor/hardware/module 四类 config 与 vendor/etc/* 虚拟 HAL 配置文件结构）",
    "MediaTek Dimensity 9500（MT6993）公开规格：1×Arm C1-Ultra 4.21GHz + 3×C1-Premium 3.5GHz + 4×C1-Pro 2.7GHz，Arm G1-Ultra GPU，台积电 3nm",
    "Arm Cortex-C1 系列 MIDR/part 编号与 Mali r54p0 驱动版本：架构公开资料，本轮未能在联网检索中取得官方页面（DuckDuckGo/GSMArena 被反爬拦截），标注为推定位"
  ]
}
```

**落地提醒**：这份 JSON 里 `cpu.clusters[].midr/part`、`gpu.*` 驱动串与 UUID、`display.edid*`、`imeiTac/systemId`、`inputAlignment.touch.*`、`thermalZones` 六组属于“结构已对齐、取值需真机回填”的字段——建议 02 阶段先把其余 90% 字段（品牌/平台/SKU/传感器拓扑/audio/media/gnss/battery/vibrator/light/thermal 阈值）灌入配置跑第一轮，再用 `device-checker` 的差异报告定点替换上述六组，避免把推定值当实测值固化进基线。
