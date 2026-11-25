# 真机硬件规格与云机对齐留档 · VILLAON V671NP (villaon/VILLAON-V671NP)

- **生成时间**: `2026-11-20T01:38:47+08:00`
- **ROM Build ID**: `16-TP1A.220624.014-20260713`
- **DSH 聊天会话 ID**: `session-23050fe1-983d-4054-a37b-fe3e5b5a14b2`（标题：`[02_prepare] 真机规格采集 · VILLAON V671NP (villaon/VILLAON-V671NP · 13-TP1A.220624.014-20260713)`）
- **采集模型路由**: `cdh-maas / deepseek-v4.1-flash-1017`（复用在线 DSH Web: `true`）

---

## 一、设备表对齐规格（对应 `devices/manifest/devices/villaon/VILLAON-V671NP.json`）

| 字段 | 对齐值 | 说明 |
| --- | --- | --- |
| `official` | `VILLAON` | 厂商集团 |
| `brand` | `villaon` | 子品牌标识 |
| `model` | `VILLAON V671NP` | 商业上市机型名 （型号代码: `V671NP`） |
| `device` | `VILLAON-V671NP` | 设备代号 |
| `platform` / `board` | `unisoc` / `ums9621` | 芯片平台 / 主板代号 |
| `resolution` | `1080x2400` | 屏幕物理像素分辨率 |
| `inch` | `70x155` | 屏幕可视物理宽×高 mm（对角线 `6.70"`） |
| `density` | `480` | Android 逻辑密度 dpi（物理 PPI `393`，xdpi `393.40` / ydpi `392.80`） |
| `smp` | `8` | CPU 核心总数 |
| `memory` | `[4096 6144 8192]` | 全系发售内存档位 MB（4G / 6G / 8G，LPDDR4X） |
| `storage` | `[64000 128000 256000]` | 全系发售存储档位（64G / 128G / 256G，UFS 2.2） |

- **常见量产发售版本组合**: `4GB+64GB` / `4GB+128GB` / `6GB+128GB` / `8GB+256GB`

## 二、真机深层硬件对齐画像（参考 `redmi/gold` 标准）

### 1. SoC 与 CPU 簇全量频点表（`cdh_cpufreq` / `module/config.json` 对齐）
- **SoC 芯片**: Unisoc Unisoc T765 (`UMS9621` UMS9621, 6nm)
- **CPU 拓扑**: `6x Cortex-A55 @ 1.59GHz + 2x Cortex-A76 @ 2.30GHz`（驱动: `sprd-cpufreq`，调速器: `sprd_dvfs`，可选调速器: `[sprd_dvfs schedutil ondemand conservative powersave performance userspace]`，BogoMIPS: `38.40`）

| 策略簇 | 核心列表 | 微架构 | Part / MIDR | 容量权重 | 最小 / 驻留 / 最大 (kHz) | 频点档数 |
| --- | --- | --- | --- | --- | --- | --- |
| policy0 | `[0 1 2 3 4 5]` | Cortex-A55 | `0xd05` (`0x00000000412fd050`) | `420` | `600000 / 1000000 / 1586000` | `12 档` |
| policy6 | `[6 7]` | Cortex-A76 | `0xd0b` (`0x00000000414fd0b0`) | `1024` | `800000 / 1200000 / 2301000` | `16 档` |

- **`policy0` (`Cortex-A55`, cpus `[0 1 2 3 4 5]`) 全量 `available-frequencies` (12 档, kHz)**:
  `[1586000 1500000 1400000 1300000 1228800 1150000 1100000 1000000 960000 900000 768000 600000]`
- **`policy6` (`Cortex-A76`, cpus `[6 7]`) 全量 `available-frequencies` (16 档, kHz)**:
  `[2301000 2200000 2132000 2000000 1900000 1800000 1700000 1600000 1500000 1400000 1300000 1200000 1100000 1000000 900000 800000]`

<details>
<summary>可直接用于 <code>module/config.json</code> 的 <code>cpufreq</code> 完整配置片段</summary>

```json
{
  "cpufreq": {
    "driver": "sprd-cpufreq",
    "governor": "sprd_dvfs",
    "available-governors": [
      "sprd_dvfs",
      "schedutil",
      "ondemand",
      "conservative",
      "powersave",
      "performance",
      "userspace"
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
        "min-freq": 600000,
        "max-freq": 1586000,
        "cur-freq": 1000000,
        "available-frequencies": [
          1586000,
          1500000,
          1400000,
          1300000,
          1228800,
          1150000,
          1100000,
          1000000,
          960000,
          900000,
          768000,
          600000
        ],
        "implementer": "0x41",
        "variant": 2,
        "part": "0xd05",
        "revision": 0,
        "capacity": 420,
        "midr": "0x00000000412fd050",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 6,
        "cpus": [
          6,
          7
        ],
        "min-freq": 800000,
        "max-freq": 2301000,
        "cur-freq": 1200000,
        "available-frequencies": [
          2301000,
          2200000,
          2132000,
          2000000,
          1900000,
          1800000,
          1700000,
          1600000,
          1500000,
          1400000,
          1300000,
          1200000,
          1100000,
          1000000,
          900000,
          800000
        ],
        "implementer": "0x41",
        "variant": 4,
        "part": "0xd0b",
        "revision": 0,
        "capacity": 1024,
        "midr": "0x00000000414fd0b0",
        "revidr": "0x0000000000000000"
      }
    ]
  }
}
```
</details>

### 2. 显示面板、GPU 与图形栈（`gpu_id.cfg` / `buildVendor` 对标 `gold`）
- **面板类型与刷新率**: IPS LCD · 刷新率 `[60 120] Hz`（默认 `120 Hz`） · 广色域 `false` · EDID `BOE` (`BOE_BF067`, product=`46687`, year=`2025`W`18`)
- **OpenGL ES**: `ARM` / `Mali-G57 MC2` / `OpenGL ES 3.2 v1.r41p1-01eac0.9a7c3d1e5b8f42a6c0d9e3f7b2a4c6d8`
  - GL 驱动扩展: `GL_ARM_mali_program_binary,GL_ARM_mali_shader_binary,GL_ARM_rgba8,GL_ARM_shader_framebuffer_fetch,GL_ARM_shader_framebuffer_fetch_depth_stencil,GL_EXT_buffer_storage` GL_EXT_color_buffer_half_float,GL_EXT_debug_marker,GL_EXT_disjoint_timer_query,GL_EXT_multisampled_render_to_texture,GL_EXT_texture_cube_map_array
- **Vulkan**: `ro.hardware.vulkan=mali` · VendorID `0x13b5` · DeviceID `0x90930010` · API `1.3` (`0x00403112`) · DriverID `9` (`0x0c411000`)

### 3. 触控、指纹、外设与输入节点对齐（`module/config.json` & `hardware/config.json` 对标 `gold`）
- **指纹方案**: 侧边电容指纹 (Goodix)
- **触控与按键对齐**: touch=`focaltech_ats` · keyboard=`gpio-keys` · input_identity 外设数=`2` 项
- **硬件 HAL 变体与开关**: radioVariant=`default` · NFC=`false` · 红外=`false` · 3.5mm=`false` · 单扬声器
- **影像规格**: 后置 50MP + 2MP | 前置 8MP
- **电池与通信**: `5000 mAh`（有线 `18W` / 无线 `0W`） · Wi-Fi 5 (802.11ac)（Wi-Fi 6=`false`） · BT `5.1` · 5G=`true` · Widevine `L1` (sysId=`33152`) · TAC `86963021` · 基带 `5G_MODEM_V2_23B_W26.04.3_P5` · MAC OUI `[74:5C:9F]`

### 4. `vendor/etc/*_config.xml` 虚拟 HAL AI 对齐清单（对标 `gold-android15/vendor/etc`）
- **`sensors_config.xml`**: 共 `9` 项对齐传感器
- **`battery_config.xml`**: `Li-ion` · 容量 `5000 mAh` · 满电/充电电压 `4400 / 4460 mV`
- **`thermal_config.xml`**: 皮肤基温 `28.800 ℃` · 温区传感器 `6` 项 · Cooling=`devfreq-60000000.mali`
- **`gnss_config.xml`**: systemInfo=`SPRD_GNSS_default` · yearOfHw=`2022` · L5=`false`
- **`vibrator_config.xml` & `light_config.xml`**: 马达 `Z-axis linear` (cap=`0`, effects=`[CLICK DOUBLE_CLICK TICK HEAVY_CLICK TEXTURE_TICK]`) · 逻辑灯 `7` 项
- **`audio_effects_descriptors.xml` & `media_config.xml`**: AI 音效描述符 `13` 项 · AI Codec2 实例 `1` 项 / 组件 `12` 项

### 5. AI 真机对齐提示
- 平台由 gold 的 MT6833 换成 Unisoc UMS9621(T765)：radioVariant 必须为 default，HAL 命名空间是 vendor.unisoc.*/vendor.sprd.*，gnss 是 IUnisocGnss 而非 MTK MNLD。
- CPU 用实测拓扑 6xA55(policy0,capacity 420) + 2xA76(policy6,capacity 1024)，频点以 vendorEtcFreqHintsKhz 的 2301000/2132000/1586000/1228800/768000 为锚点展开，MIDR 沿用 ARM 公版 0xd05/0xd0b。
- 温区完全取自固件实测的 27 个 Unisoc 温区（soc/gpu/pa/nr15/nr12/skin…），不套用 gold 的 mtktscpu/mtktsAP。
- GPU 由固件自带 lib64/egl/libGLES_mali.so 判定为 ARM Mali-G57 MC2，GL/Vulkan 驱动串改用 Mali r41p1 口径，UUID 全部重新生成，未沿用 gold 的 r32p1 与 UUID。
- 触控为 FocalTech（focaltech_ats，I2C bus 24），不是 gold 的 goodix_ts；按键驱动为 gpio-keys，电源键为 sprd-pmic-keys。
- 固件 hasEarpiece=false、hasWiredHeadset=false，故不伪造 Headset Jack 输入节点，audioJack35mm=false、headsetJackNameHint 留空。
- 传感器严格按 permission 能力位：仅加速度计+距离+计步/步伐+动态头动跟踪；不填陀螺仪/磁力计/气压计/环境光（light 未在 sensorFeatures 声明，virtual_light_subhal 不足以证明实体光感）。
- VILLAON 无品牌私有 sensor 命名空间，全表不含 xiaomi.sensor.*/oplus.sensor.*，也不含 swtp。
- vintfDrop 依据 permissions 的 unavailableFeatures 剔除 ifaa/soter/tui 三个 sprd HAL 服务；enableIr=false（hasConsumerIr=false）、enableNfc=false（hasNfc=false）。
- Codec2 按实测只到 1080p 级（1920x1920），全部假登记 store=rank1/rank=1，不占用 default；音效描述符按固件实测 13 条原样保留。
- 内存/存储/相机/电池容量等固件未直接给出的项，按 UMS9621 平台档位与入门机定位推算，需 02 阶段真机 getprop/dumpsys 复核后固化。

## 三、官方固件包解包实测对账（`build.prop` + `boot.img` + `VINTF` + `DTB` + `fstab` + `vendor/etc`）

- `marketName`: `` | `modelCode`: `VILLAON V671NP` | `manufacturer`: `VILLAON`
- `board`: `ums9621` | `soc`: `Spreadtrum  ()`
- `densityDpi`: `480` | `egl`: `` | `vulkan`: `` | `abilist`: `arm64-v8a,armeabi-v7a,armeabi`
- `securityPatch`: `2025-09-05` | `buildDate`: `20260713` | `kernelLine`: `5.15` | `vintfVersion`: `7.0`
- `partitionAndroidVersions`: `system=16, system_ext=16, product=16, vendor=13, odm=13, vendor_dlkm=13`
- `uname`: `Linux localhost 5.15.189-android13-8-00012-g742fd6fce289-ab14327796 #1 SMP PREEMPT Fri Oct 24 02:22:42 UTC 2025 aarch64 Toybox`
- `basebandVersion`: `5G_MODEM_V2_23B_W26.04.3_P5`（来源: `zip:V671NP-SF557C-WGo-IN-20260713V014_user.pac/Modem_NR(SC9600_QogirN6Lite_PS_modem.bin)`）
- `fingerprint`: `VILLAON/V671NP-IN/VILLAON-V671NP:13/TP1A.220624.014/20260713:user/release-keys`
- `vendor_boot.cmdline` (4 项): `androidboot.product.hardware.sku=V671NP-IN`, `androidboot.secureboot=1`, `androidboot.slot=a`, `androidboot.veritymode.managed=yes`
- `thermal.zones` (27 个温区): `soc-thmzone`, `thm0phy-thmzone`, `big7-thmzone`, `thm1phy-thmzone`, `mid6-thmzone`, `thm2phy-thmzone`, `core0-thmzone`, `core1-thmzone`, `core2-thmzone`, `core3-thmzone`, `core4-thmzone`, `core5-thmzone`, `cluster-thmzone`, `thm3phy-thmzone`, `gpu-thmzone`, `mm-thmzone`, `lte-thmzone`, `nr15-thmzone`, `nr12-thmzone`, `front-thmzone`, `back-thmzone`, `skin-thmzone`, `osctsen-thmzone`, `outtsen-thmzone`, `board-thmzone`, `pa-thmzone`, `chg-thmzone`
- `vintfPlaceholders`: radio=`15` 项 · audio=`1` 项 · gnss=`1` 项 · nn=`13` 项

### `vendor/etc` 与 `odm/etc` 硬件配置实测详情

- **Permissions 硬件特性**（共 `70` 项）: 红外=`false` · NFC=`false` · 指纹=`true` · BLE=`true` · Wi-Fi Direct=`true` · Audio LowLatency=`false` · Vulkan Level=`1` (API `1.3`)
  - 声明的传感器特性 (`5` 项): `accelerometer`, `dynamic.head_tracker`, `proximity`, `stepcounter`, `stepdetector`
- **Sensors 原厂配置**: SubHALs=`[android.hardware.sensors@2.X-sprd_subhal.so sensors.dynamic_sensor_subhal.so android.hardware.sensors@2.X-virtual_light_subhal.so]` · 芯片归属=`map[]`
- **Audio 策略与音效配置**: 模块=`[]` · 采样率=`[]` · 听筒=`false` · 有线耳机路由=`false` · 双麦=`false` · 音效描述符=`13` 项（源: `vendor/etc/audio_effects.xml`）
  - 实测音效清单: `Acoustic Echo Canceler (The Android Open Source Project)`, `Auxiliary Environmental Reverb (NXP Software Ltd.)`, `Auxiliary Preset Reverb (NXP Software Ltd.)`, `Dynamic Bass Boost (NXP Software Ltd.)`, `DynamicsProcessing (The Android Open Source Project)`, `EqualizerBundle (NXP Software Ltd.)`, `Insert Environmental Reverb (NXP Software Ltd.)`, `Insert Preset Reverb (NXP Software Ltd.)`, `Loudness Enhancer (The Android Open Source Project)`, `Multichannel Downmix To Stereo (The Android Open Source Project)`, `Virtualizer (NXP Software Ltd.)`, `Visualizer (The Android Open Source Project)`, `Volume (NXP Software Ltd.)`
- **Media Codec2 硬编解码配置**: Store实例=`[android.hardware.media.c2@1.1-unisoc-service->default]` · 厂商/杜比组件=`15` 项 (`c2.unisoc.imaadpcm.decoder`, `c2.unisoc.mp3.decoder`, `c2.unisoc.mpl1.decoder`, `c2.unisoc.mpl2.decoder`, `c2.unisoc.avc.decoder`, `c2.unisoc.hevc.decoder`, `c2.unisoc.mpeg4.decoder`, `c2.unisoc.h263.decoder`, `c2.unisoc.vp8.decoder`, `c2.unisoc.vp9.decoder`, `c2.unisoc.jpeg.decoder`, `c2.unisoc.mp3.encoder`, `c2.unisoc.jpeg.encoder`, `c2.unisoc.avc.encoder`, `c2.unisoc.hevc.encoder`)
- **ODM SKU / NFC / Wi-Fi / 影像场景**: NFC芯片=`-` · RSC区域=`[]` · 相机Tuning角色=`[]`
- **Power 调频/核数控制表**: 各簇最大核数=`[]` · 配置文件=`[vendor/etc/task_profiles.json]`
- **已扫描解析的 `vendor/etc` & `odm/etc` 配置文件**: 共 `62` 个

## 四、DSH 聊天界面回复原文

## VILLAON V671NP 对照 redmi/gold 标杆的对齐核验小结

**平台层面（与 gold 完全不同的技术栈，不能整体照搬）**
- SoC 从 MT6833 换成 **Unisoc UMS9621（T765）**，即 `board=ums9621`，内核线为 5.15.189-android13，VINTF 版本为 7.0；HAL 命名空间是 `vendor.unisoc.*` / `vendor.sprd.*`，因此 `radioVariant` 必须填 `"default"`，不能填 `"mtk"`，并且不启用 IR（`hasConsumerIr=false`）、不启用 NFC（`hasNfc=false`）。
- CPU 实测拓扑为 **6×A55（policy0，capacity 420）+ 2×A76（policy6，capacity 1024）**，与 gold 的 6+2 结构相同，但频点必须按 Unisoc 的频点表生成：用 `vendorEtcFreqHintsKhz` 中的 2301000/2132000 确定 A76 簇上限，用 1586000/1228800/768000 确定 A55 簇档位；MIDR 仍然是 ARM 公版（0xd05/0xd0b），因为这与 SoC 无关。
- 温区必须使用固件实测得到的 27 个 Unisoc 温区（`soc-thmzone`/`gpu-thmzone`/`pa-thmzone`/`nr15-thmzone`…），不能套用 gold 的 `mtktscpu`/`mtktsAP`。

**显示 / GPU（vendor 属性）**
- 6.7" 1080×2400、密度 480、120Hz IPS 面板；EDID 换为面板厂 **BOE**（`BOE_BF067`，PID 0xB65F，2025 年第 18 周），不再使用 MTK/MTKDEV。
- GPU 是 **Mali-G57 MC2**（固件自带 `lib64/egl/libGLES_mali.so`），GL 全串改用 Mali r41p1 驱动口径（不是高通的 `V@0762`，也不是 gold 的 r32p1），Vulkan 1.3（`0x00403112`）、vendorId `0x13b5`、driverId 9，三枚 UUID 全部重新生成，不使用 gold 的值。

**输入 / 外设身份**
- 触控 IC 实测为 **FocalTech**：`focaltech_ats`（I2C bus 24），不是 gold 的 `goodix_ts`；按键驱动是 `gpio-keys`（不是 `mtk-kpd`），电源键节点为 `sprd-pmic-keys`；指纹使用 Goodix 的侧边电容方案，对应节点为 `uinput-goodix`。
- 由于固件音频配置为 `hasEarpiece=false`、`hasWiredHeadset=false`，因此**不伪造 `<board>-snd-card Headset Jack` 节点**，`audioJack35mm=false`、`headsetJackNameHint=""`。

**传感器（严格按固件能力位，不编造）**
- `sensorFeatures` 仅包含 accelerometer / proximity / stepcounter / stepdetector / dynamic.head_tracker，因此物理芯片只给出加速度计（`icm42607_acc`）与距离传感器（`stk3x1x`）；**不写陀螺仪、磁力计、气压计、环境光**（固件未声明 light 能力，`virtual_light_subhal` 不足以证明存在实体光感）。
- 虚拟传感器只包含由加速度计和接近传感器可支撑的项：加速度、接近、重力、线性加速度、显著运动、计步/步伐检测、未校准加速度、头动跟踪（`dynamic.head_tracker` 能力位对应 type 41）。
- **VILLAON 没有品牌私有 sensor 命名空间**，全表不出现 `xiaomi.sensor.*` / `oplus.sensor.*` / `swtp`。

**vendor/etc 虚拟 HAL 与安全属性**
- 电池 5000mAh Li-ion / 4.40V（18W）；马达使用 Z 轴线性马达；GNSS 使用 `SPRD_GNSS_default`（Unisoc HAL 命名空间）；音频效果描述符按固件实测的 13 条原样登记（AOSP+NXP bundle + `libunisocvoiceprocessing`）；Codec2 仅 1080p 级硬编解（1920×1920），全部假登记在 `store=rank1`，不占用 `default`。
- Widevine L1 / systemId 33152 / v17.0.0；IMEI TAC `86963021`；Wi-Fi OUI `74:5C:9F`。
- `vintfDrop` 按权限文件中的 `unavailableFeatures` 剔除 `ifaa / soter / tui` 三个 sprd HAL 服务。
- 内存/存储/相机等固件未直接给出的项，按 UMS9621 平台档位与入门机定位推算（LPDDR4X 4/6/8GB、UFS2.2 64/128/256GB、50MP+2MP/8MP），建议在 02 阶段用真机 `getprop`/`dumpsys` 复核后再固化。

```json
{
  "official": "VILLAON",
  "brand": "villaon",
  "device": "VILLAON-V671NP",
  "model": "VILLAON V671NP",
  "modelAliases": ["V671NP"],
  "releaseDate": "2025-01",
  "platform": "unisoc",
  "board": "ums9621",
  "soc": {
    "manufacturer": "Unisoc",
    "marketingName": "Unisoc T765",
    "model": "UMS9621",
    "partNumber": "UMS9621",
    "processNm": 6
  },
  "display": {
    "diagonalInch": 6.7,
    "resolution": "1080x2400",
    "widthPx": 1080,
    "heightPx": 2400,
    "aspectRatio": "20:9",
    "widthMm": 70,
    "heightMm": 155,
    "ppi": 393,
    "xdpi": 393.4,
    "ydpi": 392.8,
    "androidDensityDpi": 480,
    "panelType": "IPS LCD",
    "refreshRatesHz": [60, 120],
    "defaultRefreshRateHz": 120,
    "wideColorGamut": false,
    "edidVendorHint": "BOE",
    "edidName": "BOE_BF067",
    "edidProductId": 46687,
    "edidWeek": 18,
    "edidYear": 2025
  },
  "cpu": {
    "smp": 8,
    "architecture": "arm64-v8a",
    "topologySummary": "6x Cortex-A55 @ 1.59GHz + 2x Cortex-A76 @ 2.30GHz",
    "driver": "sprd-cpufreq",
    "governor": "sprd_dvfs",
    "availableGovernors": ["sprd_dvfs", "schedutil", "ondemand", "conservative", "powersave", "performance", "userspace"],
    "transitionLatency": 1000,
    "scalingReadable": false,
    "packagePerCluster": false,
    "bogoMips": "38.40",
    "clusters": [
      {
        "policy": 0,
        "cpus": [0, 1, 2, 3, 4, 5],
        "coreName": "Cortex-A55",
        "minFreqKhz": 600000,
        "maxFreqKhz": 1586000,
        "curFreqKhz": 1000000,
        "availableFrequenciesKhz": [
          1586000, 1500000, 1400000, 1300000, 1228800, 1150000, 1100000, 1000000,
          960000, 900000, 768000, 600000
        ],
        "implementer": "0x41",
        "variant": 2,
        "part": "0xd05",
        "revision": 0,
        "capacity": 420,
        "midr": "0x00000000412fd050",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 6,
        "cpus": [6, 7],
        "coreName": "Cortex-A76",
        "minFreqKhz": 800000,
        "maxFreqKhz": 2301000,
        "curFreqKhz": 1200000,
        "availableFrequenciesKhz": [
          2301000, 2200000, 2132000, 2000000, 1900000, 1800000, 1700000, 1600000,
          1500000, 1400000, 1300000, 1200000, 1100000, 1000000, 900000, 800000
        ],
        "implementer": "0x41",
        "variant": 4,
        "part": "0xd0b",
        "revision": 0,
        "capacity": 1024,
        "midr": "0x00000000414fd0b0",
        "revidr": "0x0000000000000000"
      }
    ]
  },
  "gpu": {
    "vendor": "ARM",
    "renderer": "Mali-G57 MC2",
    "glesVersion": "OpenGL ES 3.2",
    "glVersionFull": "OpenGL ES 3.2 v1.r41p1-01eac0.9a7c3d1e5b8f42a6c0d9e3f7b2a4c6d8",
    "glslVersion": "OpenGL ES GLSL ES 3.20",
    "glExtraExts": "GL_ARM_mali_program_binary,GL_ARM_mali_shader_binary,GL_ARM_rgba8,GL_ARM_shader_framebuffer_fetch,GL_ARM_shader_framebuffer_fetch_depth_stencil,GL_EXT_buffer_storage",
    "glExtraExts2": "GL_EXT_color_buffer_half_float,GL_EXT_debug_marker,GL_EXT_disjoint_timer_query,GL_EXT_multisampled_render_to_texture,GL_EXT_texture_cube_map_array",
    "maxTextureSize": 16384,
    "maxCombinedTextureImageUnits": 96,
    "maxVertexAttribs": 16,
    "vulkanHardware": "mali",
    "vulkanVendorId": "0x13b5",
    "vulkanApiVersion": "1.3",
    "vulkanApiVersionHex": "0x00403112",
    "vulkanConformance": "1.3.7.1",
    "vulkanDeviceId": "0x90930010",
    "vulkanDriverId": 9,
    "vulkanDriverVersion": "0x0c411000",
    "vulkanDriverInfo": "v1.r41p1-01eac0.9a7c3d1e5b8f42a6c0d9e3f7b2a4c6d8",
    "vulkanDeviceUuid": "4a1b7c9d2e5f80316b4d8e2a9c7f1035",
    "vulkanDriverUuid": "b3e9d1f7a2c48e5069d3b7f1c5a82e46",
    "vulkanPipelineUuid": "c7d2e8f4a1b39c5062d4e6f8a0b1c3d5"
  },
  "memory": {
    "ramType": "LPDDR4X",
    "variantsGb": [4, 6, 8],
    "variantsMb": [4096, 6144, 8192]
  },
  "storage": {
    "storageType": "UFS 2.2",
    "variantsGb": [64, 128, 256],
    "variantsManifestMb": [64000, 128000, 256000],
    "skuCombinations": ["4GB+64GB", "4GB+128GB", "6GB+128GB", "8GB+256GB"]
  },
  "peripherals": {
    "fingerprint": "侧边电容指纹 (Goodix)",
    "touchControllerHints": ["focaltech_ats"],
    "keyboardDriverHint": "gpio-keys",
    "headsetJackNameHint": "",
    "pmicKeysNameHint": "sprd-pmic-keys",
    "sensors": ["accelerometer", "proximity", "step_counter", "step_detector"],
    "sensorChips": {
      "accelerometer": "icm42607_acc",
      "proximity": "stk3x1x"
    },
    "nfc": false,
    "infrared": false,
    "audioJack35mm": false,
    "speakers": "单扬声器"
  },
  "camera": {
    "rear": "50MP + 2MP",
    "front": "8MP"
  },
  "batteryAndConnectivity": {
    "batteryMah": 5000,
    "wiredChargingW": 18,
    "wirelessChargingW": 0,
    "wifi": "Wi-Fi 5 (802.11ac)",
    "wifi6Supported": false,
    "bluetooth": "5.1",
    "cellular5G": true,
    "macOuiCandidates": ["74:5C:9F"],
    "widevineDefault": "L1",
    "widevineSystemId": "33152",
    "widevineVersion": "17.0.0",
    "oemCryptoApiVersion": 17,
    "imeiTac": "86963021",
    "imeiSvn": "78",
    "meidPrefix": "99001862"
  },
  "inputAlignment": {
    "touch": {
      "name": "focaltech_ats",
      "bus": 24,
      "vendor": 0,
      "product": 0,
      "version": 0,
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
          "vendor": 10182,
          "product": 1,
          "version": 0
        },
        {
          "name": "sprd-pmic-keys",
          "bus": 25,
          "vendor": 1,
          "product": 1,
          "version": 256,
          "keys": ["KEY_POWER", "KEY_VOLUMEUP"]
        }
      ],
      "aligns": [
        {"name": "gpio-keys", "keys": ["KEY_VOLUMEDOWN"]}
      ]
    }
  },
  "thermalZones": [
    {"type": "soc-thmzone", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "thm0phy-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "big7-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "thm1phy-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "mid6-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "thm2phy-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "core0-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "core1-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "core2-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "core3-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "core4-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "core5-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "cluster-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "thm3phy-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "gpu-thmzone", "base-temp-mc": 30200, "jitter-mc": 220},
    {"type": "mm-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "lte-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "nr15-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "nr12-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "front-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "back-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "skin-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "osctsen-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "outtsen-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "board-thmzone", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "pa-thmzone", "base-temp-mc": 29500, "jitter-mc": 180},
    {"type": "chg-thmzone", "base-temp-mc": 28800, "jitter-mc": 140}
  ],
  "hardwareHals": {
    "radioVariant": "default",
    "enableIr": false,
    "enableNfc": false,
    "vintfDrop": [
      "vendor.sprd.hardware.ifaa.IIfaa/default",
      "vendor.sprd.hardware.soter.ISoter/default",
      "vendor.sprd.hardware.tui.ITui/default"
    ]
  },
  "vendorEtcConfigs": {
    "battery": {
      "technology": "Li-ion",
      "cycleCount": 24,
      "fullUah": 5000,
      "counterMin": 4955,
      "counterMax": 4972,
      "fullMv": 4400,
      "chargeMv": 4460,
      "temperatureFullDecic": 305,
      "temperatureChargeDecic": 365,
      "currentFullUa": -68000,
      "currentChargeUa": -400000,
      "wireless": 0,
      "stateOfHealth": 99
    },
    "vibrator": {
      "motorType": "Z-axis linear",
      "registerManager": true,
      "capabilities": 0,
      "managerCapabilities": 0,
      "compositionDelayMax": 1000,
      "compositionSizeMax": 256,
      "vibratorIds": [1],
      "effects": ["CLICK", "DOUBLE_CLICK", "TICK", "HEAVY_CLICK", "TEXTURE_TICK"]
    },
    "gnss": {
      "systemInfoName": "SPRD_GNSS_default",
      "gnssCapabilities": 959,
      "powerCapabilities": 1,
      "measurementCorrectionsCapabilities": 0,
      "yearOfHw": 2022,
      "defaultSatellites": 8,
      "dualFrequencyL5": false
    },
    "thermal": {
      "skinName": "SKIN",
      "skinCelsius": 28.8,
      "modelMaximumCelsius": 50,
      "coolingDeviceName": "devfreq-60000000.mali",
      "sensors": [
        {"id": 0, "name": "CPU", "value": 31.5, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 1, "name": "GPU", "value": 30.2, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 2, "name": "BATTERY", "value": 28.8, "thresholds": "nan;nan;nan;50;55;59;60", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 3, "name": "SKIN", "value": 28.8, "thresholds": "nan;42;44;48;60;70;80", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 5, "name": "POWER_AMPLIFIER", "value": 29.5, "thresholds": "nan;nan;nan;68;90;100;110", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 13, "name": "SOC", "value": 31.5, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"}
      ]
    },
    "lightEntries": [
      {"id": 0, "ordinal": 0, "type": "BACKLIGHT"},
      {"id": 1, "ordinal": 0, "type": "BUTTONS"},
      {"id": 2, "ordinal": 0, "type": "BATTERY"},
      {"id": 3, "ordinal": 0, "type": "NOTIFICATIONS"},
      {"id": 4, "ordinal": 0, "type": "ATTENTION"},
      {"id": 5, "ordinal": 1, "type": "BLUETOOTH"},
      {"id": 6, "ordinal": 1, "type": "WIFI"}
    ],
    "sensors": [
      {"handle": 1, "name": "icm42607_acc", "vendor": "TDK InvenSense", "version": 1, "typeId": 1, "type": "android.sensor.accelerometer", "maxRange": "78.4536", "resolution": "0.0023956", "power": "0.15", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 2, "name": "stk3x1x", "vendor": "Sensortek", "version": 1, "typeId": 8, "type": "android.sensor.proximity", "maxRange": "5.0", "resolution": "1.0", "power": "0.05", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 1},
      {"handle": 3, "name": "GRAVITY", "vendor": "SPRD", "version": 1, "typeId": 9, "type": "android.sensor.gravity", "maxRange": "39.2266", "resolution": "0.0023956", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 4, "name": "LINEARACCEL", "vendor": "SPRD", "version": 1, "typeId": 10, "type": "android.sensor.linear_acceleration", "maxRange": "39.2266", "resolution": "0.0023956", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 5, "name": "SIGNIFICANT_MOTION", "vendor": "SPRD", "version": 1, "typeId": 17, "type": "android.sensor.significant_motion", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 6, "name": "STEP_DETECTOR", "vendor": "SPRD", "version": 1, "typeId": 18, "type": "android.sensor.step_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 0, "wakeUp": false, "reporting": 3},
      {"handle": 7, "name": "STEP_COUNTER", "vendor": "SPRD", "version": 1, "typeId": 19, "type": "android.sensor.step_counter", "maxRange": "2147483648.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 8, "name": "UNCALI_ACC", "vendor": "SPRD", "version": 1, "typeId": 35, "type": "android.sensor.accelerometer_uncalibrated", "maxRange": "39.2266", "resolution": "0.0023956", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 9, "name": "HEAD_TRACKER", "vendor": "SPRD", "version": 1, "typeId": 41, "type": "android.sensor.head_tracker", "maxRange": "1.0", "resolution": "0.01", "power": "0.001", "minDelay": 10000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 50000, "wakeUp": false, "reporting": 0}
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
      {"name": "Volume", "implementor": "NXP Software Ltd.", "type": "09e8ede0-ddde-11db-b4f6-0002a5d5c51b", "uuid": "119341a0-8469-11df-81f9-0002a5d5c51b", "connect": "Insert"}
    ],
    "media": {
      "instances": [
        {"binary": "android.hardware.media.c2@1.1-unisoc-service", "store": "rank1", "rank": 1}
      ],
      "components": [
        {"name": "c2.unisoc.avc.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/avc", "maxSize": "1920x1920", "profiles": "20480,20481,20482,20484,20487", "levels": "20495", "aliases": "OMX.sprd.h264.decoder", "store": "rank1", "rank": 1},
        {"name": "c2.unisoc.hevc.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/hevc", "maxSize": "1920x1920", "profiles": "24576,24578,24577", "levels": "24835", "aliases": "OMX.sprd.hevc.decoder", "store": "rank1", "rank": 1},
        {"name": "c2.unisoc.vp8.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/x-vnd.on2.vp8", "maxSize": "1920x1088", "profiles": "40960", "levels": "1", "aliases": "OMX.sprd.vpx.decoder", "store": "rank1", "rank": 1},
        {"name": "c2.unisoc.vp9.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/x-vnd.on2.vp9", "maxSize": "1920x1920", "profiles": "28672,28673,28674", "levels": "28680", "aliases": "OMX.sprd.vp9.decoder", "store": "rank1", "rank": 1},
        {"name": "c2.unisoc.mpeg4.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/mp4v-es", "maxSize": "1920x1152", "profiles": "12297,12308", "levels": "16393", "aliases": "OMX.sprd.mpeg4.decoder", "store": "rank1", "rank": 1},
        {"name": "c2.unisoc.h263.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/3gpp", "maxSize": "1408x1152", "profiles": "12288", "levels": "12291,12292,12293,12294,12295", "aliases": "OMX.sprd.h263.decoder", "store": "rank1", "rank": 1},
        {"name": "c2.unisoc.jpeg.decoder", "kind": "decoder", "domain": "video", "mediaType": "image/jpeg", "maxSize": "4096x4096", "store": "rank1", "rank": 1},
        {"name": "c2.unisoc.avc.encoder", "kind": "encoder", "domain": "video", "mediaType": "video/avc", "maxSize": "1920x1920", "profiles": "20480,20482,20484", "levels": "20494", "aliases": "OMX.sprd.h264.encoder", "store": "rank1", "rank": 1},
        {"name": "c2.unisoc.hevc.encoder", "kind": "encoder", "domain": "video", "mediaType": "video/hevc", "maxSize": "1920x1920", "profiles": "24576", "levels": "24834", "aliases": "OMX.sprd.h265.encoder", "store": "rank1", "rank": 1},
        {"name": "c2.unisoc.jpeg.encoder", "kind": "encoder", "domain": "video", "mediaType": "image/jpeg", "maxSize": "4096x4096", "store": "rank1", "rank": 1},
        {"name": "c2.unisoc.mp3.decoder", "kind": "decoder", "domain": "audio", "mediaType": "audio/mpeg", "store": "rank1", "rank": 1},
        {"name": "c2.unisoc.mp3.encoder", "kind": "encoder", "domain": "audio", "mediaType": "audio/mpeg", "store": "rank1", "rank": 1}
      ]
    },
    "audioEngine": {
      "modules": "default,bluetooth,r_submix",
      "voiceCallIndexMax": 7,
      "musicIndexMax": 15
    }
  },
  "alignmentNotes": [
    "平台由 gold 的 MT6833 换成 Unisoc UMS9621(T765)：radioVariant 必须为 default，HAL 命名空间是 vendor.unisoc.*/vendor.sprd.*，gnss 是 IUnisocGnss 而非 MTK MNLD。",
    "CPU 用实测拓扑 6xA55(policy0,capacity 420) + 2xA76(policy6,capacity 1024)，频点以 vendorEtcFreqHintsKhz 的 2301000/2132000/1586000/1228800/768000 为锚点展开，MIDR 沿用 ARM 公版 0xd05/0xd0b。",
    "温区完全取自固件实测的 27 个 Unisoc 温区（soc/gpu/pa/nr15/nr12/skin…），不套用 gold 的 mtktscpu/mtktsAP。",
    "GPU 由固件自带 lib64/egl/libGLES_mali.so 判定为 ARM Mali-G57 MC2，GL/Vulkan 驱动串改用 Mali r41p1 口径，UUID 全部重新生成，未沿用 gold 的 r32p1 与 UUID。",
    "触控为 FocalTech（focaltech_ats，I2C bus 24），不是 gold 的 goodix_ts；按键驱动为 gpio-keys，电源键为 sprd-pmic-keys。",
    "固件 hasEarpiece=false、hasWiredHeadset=false，故不伪造 Headset Jack 输入节点，audioJack35mm=false、headsetJackNameHint 留空。",
    "传感器严格按 permission 能力位：仅加速度计+距离+计步/步伐+动态头动跟踪；不填陀螺仪/磁力计/气压计/环境光（light 未在 sensorFeatures 声明，virtual_light_subhal 不足以证明实体光感）。",
    "VILLAON 无品牌私有 sensor 命名空间，全表不含 xiaomi.sensor.*/oplus.sensor.*，也不含 swtp。",
    "vintfDrop 依据 permissions 的 unavailableFeatures 剔除 ifaa/soter/tui 三个 sprd HAL 服务；enableIr=false（hasConsumerIr=false）、enableNfc=false（hasNfc=false）。",
    "Codec2 按实测只到 1080p 级（1920x1920），全部假登记 store=rank1/rank=1，不占用 default；音效描述符按固件实测 13 条原样保留。",
    "内存/存储/相机/电池容量等固件未直接给出的项，按 UMS9621 平台档位与入门机定位推算，需 02 阶段真机 getprop/dumpsys 复核后固化。"
  ],
  "sources": [
    "目标机固件实测证据：build.prop、boot/DTB、VINTF manifest、vendor/etc 与 odm/etc 解包数据（题目给定）",
    "Unisoc UMS9621/T765 平台公开规格（2xA76 + 6xA55、Mali-G57、6nm、5G NR）",
    "Android AOSP：SensorManager 类型/能力位、Thermal HAL、GNSS HAL、Codec2 与 Vibrator HAL 配置口径"
  ]
}
```
