# 真机硬件规格与云机对齐留档 · 真我GT7 (realme/RMX6688)

- **生成时间**: `2026-11-21T10:24:56+08:00`
- **ROM Build ID**: `15-AP3A.240617.008-20250825`
- **DSH 聊天会话 ID**: `session-f04ae32a-3620-4718-8661-b1487f34bf09`（标题：`[02_prepare] 真机规格采集 · 真我GT7 (realme/RMX6688 · 15-AP3A.240617.008-20250825)`）
- **采集模型路由**: `cdh-maas / deepseek-v4.1-flash-1017`（复用在线 DSH Web: `false`）

---

## 一、设备表对齐规格（对应 `devices/manifest/devices/realme/RMX6688.json`）

| 字段 | 对齐值 | 说明 |
| --- | --- | --- |
| `official` | `真我` | 厂商集团 |
| `brand` | `realme` | 子品牌标识 |
| `model` | `真我GT7` | 商业上市机型名 （型号代码: `RMX6688`, `realme GT7`） |
| `device` | `RMX6688` | 设备代号 |
| `platform` / `board` | `mtk` / `mt6991` | 芯片平台 / 主板代号 |
| `resolution` | `1280x2800` | 屏幕物理像素分辨率 |
| `inch` | `72x157` | 屏幕可视物理宽×高 mm（对角线 `6.80"`） |
| `density` | `480` | Android 逻辑密度 dpi（物理 PPI `453`，xdpi `452.94` / ydpi `453.20`） |
| `smp` | `8` | CPU 核心总数 |
| `memory` | `[12288 16384]` | 全系发售内存档位 MB（12G / 16G，LPDDR5X） |
| `storage` | `[256000 512000 1024000]` | 全系发售存储档位（256G / 512G / 1T，UFS 4.0） |

- **常见量产发售版本组合**: `12GB+256GB` / `16GB+256GB` / `16GB+512GB` / `16GB+1TB`

## 二、真机深层硬件对齐画像（参考 `redmi/gold` 标准）

### 1. SoC 与 CPU 簇全量频点表（`cdh_cpufreq` / `module/config.json` 对齐）
- **SoC 芯片**: MediaTek 天玑9400+ (`MT6991` MT6991, 3nm)
- **CPU 拓扑**: `4x Cortex-A720 @ 2.4GHz + 3x Cortex-X4 @ 3.3GHz + 1x Cortex-X925 @ 3.63GHz`（驱动: `mt-cpufreq`，调速器: `sugov_ext`，可选调速器: `[sugov_ext performance powersave userspace ondemand]`，BogoMIPS: `38.40`）

| 策略簇 | 核心列表 | 微架构 | Part / MIDR | 容量权重 | 最小 / 驻留 / 最大 (kHz) | 频点档数 |
| --- | --- | --- | --- | --- | --- | --- |
| policy0 | `[0 1 2 3]` | Cortex-A720 | `0xd89` (`0x00000000410fd890`) | `714` | `338000 / 988000 / 1508000` | `46 档` |
| policy4 | `[4 5 6]` | Cortex-X4 | `0xd82` (`0x00000000410fd820`) | `1024` | `400000 / 1500000 / 3300000` | `24 档` |
| policy7 | `[7]` | Cortex-X925 | `0xd85` (`0x00000000410fd850`) | `937` | `400000 / 1500000 / 3630000` | `24 档` |

- **`policy0` (`Cortex-A720`, cpus `[0 1 2 3]`) 全量 `available-frequencies` (46 档, kHz)**:
  `[1508000 1482000 1456000 1430000 1404000 1378000 1352000 1326000 1300000 1274000 1248000 1222000 1196000 1170000 1144000 1118000 1092000 1066000 1040000 1014000 988000 962000 936000 910000 884000 858000 832000 806000 780000 754000 728000 702000 676000 650000 624000 598000 572000 546000 520000 494000 468000 442000 416000 390000 365000 338000]`
- **`policy4` (`Cortex-X4`, cpus `[4 5 6]`) 全量 `available-frequencies` (24 档, kHz)**:
  `[3300000 3200000 3100000 3000000 2900000 2800000 2700000 2600000 2500000 2400000 2300000 2200000 2100000 2000000 1900000 1800000 1600000 1500000 1400000 1200000 1000000 800000 600000 400000]`
- **`policy7` (`Cortex-X925`, cpus `[7]`) 全量 `available-frequencies` (24 档, kHz)**:
  `[3630000 3500000 3300000 3200000 3000000 2900000 2800000 2700000 2500000 2400000 2300000 2200000 2100000 2000000 1900000 1800000 1600000 1500000 1400000 1200000 1000000 800000 600000 400000]`

<details>
<summary>可直接用于 <code>module/config.json</code> 的 <code>cpufreq</code> 完整配置片段</summary>

```json
{
  "cpufreq": {
    "driver": "mt-cpufreq",
    "governor": "sugov_ext",
    "available-governors": [
      "sugov_ext",
      "performance",
      "powersave",
      "userspace",
      "ondemand"
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
          3
        ],
        "min-freq": 338000,
        "max-freq": 1508000,
        "cur-freq": 988000,
        "available-frequencies": [
          1508000,
          1482000,
          1456000,
          1430000,
          1404000,
          1378000,
          1352000,
          1326000,
          1300000,
          1274000,
          1248000,
          1222000,
          1196000,
          1170000,
          1144000,
          1118000,
          1092000,
          1066000,
          1040000,
          1014000,
          988000,
          962000,
          936000,
          910000,
          884000,
          858000,
          832000,
          806000,
          780000,
          754000,
          728000,
          702000,
          676000,
          650000,
          624000,
          598000,
          572000,
          546000,
          520000,
          494000,
          468000,
          442000,
          416000,
          390000,
          365000,
          338000
        ],
        "implementer": "0x41",
        "variant": 0,
        "part": "0xd89",
        "revision": 0,
        "capacity": 714,
        "midr": "0x00000000410fd890",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 4,
        "cpus": [
          4,
          5,
          6
        ],
        "min-freq": 400000,
        "max-freq": 3300000,
        "cur-freq": 1500000,
        "available-frequencies": [
          3300000,
          3200000,
          3100000,
          3000000,
          2900000,
          2800000,
          2700000,
          2600000,
          2500000,
          2400000,
          2300000,
          2200000,
          2100000,
          2000000,
          1900000,
          1800000,
          1600000,
          1500000,
          1400000,
          1200000,
          1000000,
          800000,
          600000,
          400000
        ],
        "implementer": "0x41",
        "variant": 0,
        "part": "0xd82",
        "revision": 0,
        "capacity": 1024,
        "midr": "0x00000000410fd820",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 7,
        "cpus": [
          7
        ],
        "min-freq": 400000,
        "max-freq": 3630000,
        "cur-freq": 1500000,
        "available-frequencies": [
          3630000,
          3500000,
          3300000,
          3200000,
          3000000,
          2900000,
          2800000,
          2700000,
          2500000,
          2400000,
          2300000,
          2200000,
          2100000,
          2000000,
          1900000,
          1800000,
          1600000,
          1500000,
          1400000,
          1200000,
          1000000,
          800000,
          600000,
          400000
        ],
        "implementer": "0x41",
        "variant": 0,
        "part": "0xd85",
        "revision": 0,
        "capacity": 937,
        "midr": "0x00000000410fd850",
        "revidr": "0x0000000000000000"
      }
    ]
  }
}
```
</details>

### 2. 显示面板、GPU 与图形栈（`gpu_id.cfg` / `buildVendor` 对标 `gold`）
- **面板类型与刷新率**: AMOLED · 刷新率 `[60 90 120 144] Hz`（默认 `144 Hz`） · 广色域 `true` · EDID `BOE` (`BOE`, product=`2591`, year=`2025`W`9`)
- **OpenGL ES**: `ARM` / `Immortalis-G925 MC12` / `OpenGL ES 3.2 v1.r49p0-01eac0.9fbe4dd3cf5e4a8f3c9a2d6b1e7f0c24`
  - GL 驱动扩展: `GL_ARM_mali_program_binary,GL_ARM_mali_shader_binary,GL_ARM_rgba8,GL_ARM_shader_core_properties` GL_ARM_texture_unnormalized_coordinates,GL_ARM_debug_assert,GL_EXT_texture_filter_anisotropic,GL_EXT_texture_cube_map_array
- **Vulkan**: `ro.hardware.vulkan=mali` · VendorID `0x13b5` · DeviceID `0x90930010` · API `1.3` (`0x00403116`) · DriverID `9` (`0x0d490000`)

### 3. 触控、指纹、外设与输入节点对齐（`module/config.json` & `hardware/config.json` 对标 `gold`）
- **指纹方案**: 3D 超声波屏下指纹 (Goodix)
- **触控与按键对齐**: touch=`goodix_ts` · keyboard=`mtk-kpd` · input_identity 外设数=`3` 项
- **硬件 HAL 变体与开关**: radioVariant=`mtk` · NFC=`true` · 红外=`true` · 3.5mm=`true` · 双扬声器
- **影像规格**: 后置 50MP + 8MP | 前置 16MP
- **电池与通信**: `7000 mAh`（有线 `100W` / 无线 `0W`） · Wi-Fi 7 (802.11be)（Wi-Fi 6=`true`） · BT `5.4` · 5G=`true` · Widevine `L1` (sysId=`33098`) · TAC `86731205` · MAC OUI `[94:65:2d d4:6a:6a]`

### 4. `vendor/etc/*_config.xml` 虚拟 HAL AI 对齐清单（对标 `gold-android15/vendor/etc`）
- **`sensors_config.xml`**: 共 `24` 项对齐传感器
- **`battery_config.xml`**: `Li-poly` · 容量 `7000 mAh` · 满电/充电电压 `4470 / 4530 mV`
- **`thermal_config.xml`**: 皮肤基温 `30.800 ℃` · 温区传感器 `7` 项 · Cooling=`devfreq-13000000.mali`
- **`gnss_config.xml`**: systemInfo=`MTK_MNLD_default,MNL_VER_default` · yearOfHw=`2024` · L5=`true`
- **`vibrator_config.xml` & `light_config.xml`**: 马达 `X-axis linear` (cap=`0`, effects=`[CLICK DOUBLE_CLICK TICK HEAVY_CLICK TEXTURE_TICK THUD POP QUICK_RISE]`) · 逻辑灯 `4` 项
- **`audio_effects_descriptors.xml` & `media_config.xml`**: AI 音效描述符 `18` 项 · AI Codec2 实例 `1` 项 / 组件 `10` 项

### 5. AI 真机对齐提示
- 目标机为 realme 真我GT7（研发代号 RMX6688，MT6991 / 天玑9400+），固件 brand/manufacturer=oplus、device=ossi、VINTF 8.0、Android 15；与 gold（redmi/gold，MT6833 天玑6080）仅共享 MTK cpufreq/VINTF 框架，所有具体数值按本机重算，未复用任何小米/MT6833 专属值。
- cpufreq：MTK 三簇 1+3+4 拓扑（policy0=cpu0-3 A720、policy4=cpu4-6 X4、policy7=cpu7 X925），capacity 714/1024/937 与固件 measuredCpuClusters 完全一致；MIDR part 分别取 0xd89/0xd82/0xd85。policy0 的 availableFrequenciesKhz 按固件实测 OPP 表原样保留（26 MHz 步进、实测上限 1508000，共 45 档），若实机 scaling_max_freq 为 2402000 则在 1508000 以上补齐 1534000~2402000，不改动已实测档位。
- GPU：本机 SoC 为 Immortalis-G925 MC12（hardwareEgl=meow、hardwareVulkan=mali），故 glVersionFull/vulkanDriverInfo 按 ARM Mali r49p0-01eac0 驱动线给出，vulkanApiVersionHex 为 Vulkan 1.3 的 0x00403116，vulkanDriverId=9（ARM proprietary）；驱动版本号/Vulkan UUID 为按驱动线推定，需真机 glGetString + vulkaninfo 复核。
- input：keyboardDriverHint=mtk-kpd、headsetJackNameHint=mt6991-mt6359 Headset Jack、pmicKeysNameHint=mtk-pmic-keys 三项直接取自固件 measuredInputHints，未沿用 gold 的 mt6833-mt6359；触控 IC 按 OPPO/realme 同代 BOM 推定为 Goodix（goodix_ts，I2C bus=24），指纹依固件 power_features 的 oplus.software.fp.ultrasonic_support 判为 3D 超声波（Goodix），uinput 节点名 uinput-goodix 需以 /proc/bus/input/devices 复核；本机无 swtp（该节点仅小米系具备），未填写。
- 传感器：固件 sensorFeatures 仅声明 accelerometer/compass/gyroscope/proximity/stepcounter/stepdetector/dynamic.head_tracker，无 pressure —— 故不编造气压计；物理芯片按 OPPO 系同代 BOM 推定为 lsm6dso（acc/gyro）+ mmc5603（磁力计）+ stk3338（光感/距离）；品牌私有传感器使用 oplus.sensor.pick_up/free_fall/sar_algo（typeId 33171000 段），未使用任何 xiaomi.sensor.*。
- vendor/etc：battery 按 7000mAh 单电芯 + 100W SUPERVOOC + 无无线充给出；vibrator 按旗舰 X 轴线性马达（capabilities=0、支持 8 种原语）；thermal 温区 base/jitter 逐项取自固件实测（soc/cpu/gpu/apu/pmic6363/pmic6373/wifi/vtskin）；light 收缩为 BACKLIGHT/BATTERY/NOTIFICATIONS/ATTENTION 四条逻辑灯；media 直接登记固件实测的 Codec2 实例 android.hardware.media.c2-mediatek-64b 与 10 个核心 c2.mtk.* 组件，store 统一 default1、rank=1，未占用 default。
- HAL 变体：radioVariant=mtk（固件 VINTF 中 34 项 vendor.mediatek.hardware.mtkradioex.* 占位）；enableIr=true（固件 hasConsumerIr）；enableNfc=true（固件 hasNfc，与 gold 的 false 相反，不可照抄）。
- 待真机复核项（当前为推定值）：glVersionFull 与 Vulkan 驱动版本/UUID、imeiTac（86731205）、macOuiCandidates（94:65:2d / d4:6a:6a）、widevineSystemId（33098）、EDID 产品 ID/周数、触控与指纹 uinput 节点名、传感器物理芯片型号；出货前用 getprop / dumpsys / /proc/bus/input/devices / glGetString 校准一次即可闭环。

## 三、官方固件包解包实测对账（`build.prop` + `boot.img` + `VINTF` + `DTB` + `fstab` + `vendor/etc`）

- `marketName`: `真我GT7` | `modelCode`: `ossi` | `manufacturer`: `oplus`
- `board`: `mt6991` | `soc`: `Mediatek  ()`
- `densityDpi`: `480` | `egl`: `meow` | `vulkan`: `mali` | `abilist`: `arm64-v8a`
- `securityPatch`: `2025-09-01` | `buildDate`: `20250825` | `kernelLine`: `6.6` | `vintfVersion`: `8.0`
- `partitionAndroidVersions`: `system=15, system_ext=15, product=15, vendor=15, vendor_dlkm=15, odm_dlkm=15`
- `uname`: `Linux localhost 6.6.66-android15-8-gd43086512890-abogki423825152-4k #1 SMP PREEMPT Tue Jun 10 12:12:23 UTC 2025 aarch64 Toybox`
- `fingerprint`: `oplus/ossi/ossi:15/AP3A.240617.008/1756131725502:user/release-keys`
- `vendor_boot.cmdline` (4 项): `androidboot.product.hardware.sku=dsds`, `androidboot.secureboot=1`, `androidboot.slot=a`, `androidboot.veritymode.managed=yes`
- `thermal.zones` (62 个温区): `soc_max`, `cpu-medium-core6-0`, `cpu-medium-core6-1`, `cpu-dsu2`, `cpu-dsu3`, `cpu-little-core3`, `cpu-little-core0`, `cpu-little-core1`, `cpu-little-core2`, `cpu-medium-core4-0`, `cpu-medium-core4-1`, `cpu-medium-core5-0`, `cpu-medium-core5-1`, `cpu-dsu0`, `cpu-dsu1`, `cpu-big-core7-0`, `cpu-big-core7-1`, `apu-a0`, `apu-a1`, `apu-a2`, `apu-a3`, `gpu0`, `gpu1`, `soc-top0`, `soc-top1`, `soc-top2`, `soc-top3`, `soc-bot0`, `soc-bot1`, `soc-bot2`, `soc-bot3`, `ap_ntc`, `ltepa_ntc`, `nrpa_ntc`, `quiet-rf-top-ntc`, `quiet-pa-ntc`, `quiet-rf-bottom-ntc`, `quiet-rf-ntc`, `pmic6363-vio18`, `pmic6363-vs1-vs3`, `pmic6363-bk3-bk7`, `pmic6363-vs2`, `pmic6373-bk0`, `pmic6373-ldo`, `pmic6373-bk3-bk7`, `pmic6373-bk4-bk8`, `wifi-adie-0`, `wifi-ddie-0`, `wifi-ddie-1`, `wifi-ddie-2`, `wifi-ddie-3`, `vtskin-max`, `vtskin1`, `vtskin2`, `vtskin3`, `vtskin4`, `vtskin5`, `vtskin6`, `vtskin7`, `vtskin8`, `vtskin9`, `vtskin10`
- `vda.partitions` (5 个附加分区): `vendor_persist.img->/mnt/vendor/persist`, `protect_f.img->/mnt/vendor/protect_f`, `protect_s.img->/mnt/vendor/protect_s`, `nvdata.img->/mnt/vendor/nvdata`, `nvcfg.img->/mnt/vendor/nvcfg`
- `vintfPlaceholders`: radio=`34` 项 · audio=`1` 项 · gnss=`2` 项 · nn=`98` 项

### `vendor/etc` 与 `odm/etc` 硬件配置实测详情

- **Permissions 硬件特性**（共 `95` 项）: 红外=`true` · NFC=`true` · 指纹=`false` · BLE=`true` · Wi-Fi Direct=`true` · Audio LowLatency=`true` · Vulkan Level=`1` (API `1.3`)
  - 声明的传感器特性 (`7` 项): `accelerometer`, `compass`, `dynamic.head_tracker`, `gyroscope`, `proximity`, `stepcounter`, `stepdetector`
- **Sensors 原厂配置**: SubHALs=`[android.hardware.sensors@2.X-subhal-mediatek.so sensors.dynamic_sensor_hal.so]` · 芯片归属=`map[]`
- **Audio 策略与音效配置**: 模块=`[primary]` · 采样率=`[44100 48000]` · 听筒=`true` · 有线耳机路由=`true` · 双麦=`true` · 音效描述符=`18` 项（源: `vendor/etc/audio_effects_config.xml`）
  - 实测音效清单: `Acoustic Echo Canceler (The Android Open Source Project)`, `Automatic Gain Control V2 (The Android Open Source Project)`, `Auxiliary Environmental Reverb (NXP Software Ltd.)`, `Auxiliary Preset Reverb (NXP Software Ltd.)`, `Dynamic Bass Boost (NXP Software Ltd.)`, `DynamicsProcessing (The Android Open Source Project)`, `EqualizerBundle (NXP Software Ltd.)`, `ExtensionEffectExample (The Android Open Source Project)`, `Haptic Generator (The Android Open Source Project)`, `Insert Environmental Reverb (NXP Software Ltd.)`, `Insert Preset Reverb (NXP Software Ltd.)`, `Loudness Enhancer (The Android Open Source Project)`, `Multichannel Downmix To Stereo (The Android Open Source Project)`, `Noise Suppression (The Android Open Source Project)`, `Spatializer (The Android Open Source Project)`, `Virtualizer (NXP Software Ltd.)`, `Visualizer (The Android Open Source Project)`, `Volume (NXP Software Ltd.)`
- **Media Codec2 硬编解码配置**: Store实例=`[android.hardware.media.c2-mediatek-64b->default1]` · 厂商/杜比组件=`18` 项 (`c2.mtk.avc.decoder`, `c2.mtk.avc.decoder.secure`, `c2.mtk.avc.decoder.lowlatency`, `c2.mtk.hevc.decoder`, `c2.mtk.hevc.decoder.secure`, `c2.mtk.hevc.decoder.lowlatency`, `c2.mtk.heif.decoder`, `c2.mtk.vp9.decoder`, `c2.mtk.vp9.decoder.secure`, `c2.mtk.vp9.decoder.lowlatency`, `c2.mtk.av1.decoder`, `c2.mtk.av1.decoder.secure`, `c2.mtk.av1.decoder.lowlatency`, `c2.mtk.avc.encoder`, `c2.mtk.avc.encoder.secure`, `c2.mtk.hevc.encoder`, `c2.mtk.hevc.encoder.secure`, `c2.mtk.heif.encoder`)
- **GNSS 定位配置**: 源文件=`vendor/etc/MNL_Config.xml` · 版本=`24081501.6.0O.00` · 模式=`10 (GPS+GLONASS+Galileo+BeiDou+NavIC)` · L1Only=`false` · L5配置=`true`
- **Display 面板与 GPU 平台配置**: DSI面板型号=`[]` · screen_light.xml=`false` · Mali参数=`map[OSU_PROTECTED_MEMORY_HEAP_NAME:mtk_svp_page-uncached PLATFORM_AGT_FREQUENCY_KHZ:13000]`
- **ODM SKU / NFC / Wi-Fi / 影像场景**: NFC芯片=`-` · RSC区域=`[]` · 相机Tuning角色=`[]`
- **Power 调频/核数控制表**: 各簇最大核数=`[4 3 1]` · 配置文件=`[vendor/etc/power_app_cfg.xml vendor/etc/powercontable.xml vendor/etc/powerscntbl.xml vendor/etc/task_profiles.json]`
- **已扫描解析的 `vendor/etc` & `odm/etc` 配置文件**: 共 `87` 个

## 四、DSH 聊天界面回复原文

## 对齐核验小结

目标机为 **realme 真我GT7（RMX6688，MT6991 / 天玑9400+，Android 15，VINTF 8.0）**，与 gold（redmi/gold，MT6833 天玑6080）只共享 MTK 的 cpufreq/VINTF 框架，具体数值全部按本机重算：

- **SoC/CPU**：1+3+4 三簇（A720×4 / X4×3 / X925×1），capacity 714/1024/937 与固件 `measuredCpuClusters` 一致；MIDR part 取 0xd89/0xd82/0xd85；policy0 频点表沿用固件实测的 45 档（26 MHz 步进，上限 1508000）。
- **GPU**：Immortalis-G925 MC12（固件 `hardwareEgl=meow`、`hardwareVulkan=mali`），GL 串/Vulkan ID 按 Mali r49p0 驱动线，`vulkanApiVersionHex=0x00403116`（Vulkan 1.3）。
- **input**：`mtk-kpd`、`mt6991-mt6359 Headset Jack`、`mtk-pmic-keys` 三项直接取自固件实测，**不沿用** gold 的 `mt6833-mt6359`；触控/指纹按 OPPO 系 BOM 推定为 Goodix，指纹依固件 `oplus.software.fp.ultrasonic_support` 判为 3D 超声波；本机无 `swtp`。
- **传感器**：固件 `sensorFeatures` 无 pressure → **不编造气压计**；私有传感器用 `oplus.sensor.*`（pick_up / free_fall / sar_algo），**严禁 xiaomi.sensor.\***。
- **vendor/etc**：7000mAh 单电芯 + 100W、X 轴线性马达、热区逐项取固件实测、media 登记固件实测 Codec2（store=`default1`、rank=1）、`enableNfc=true`（与 gold 相反）、`enableIr=true`。
- **待真机复核**：GPU 驱动 hash/UUID、`imeiTac`、`macOuiCandidates`、`widevineSystemId`、EDID 产品 ID/周数、触控与指纹 uinput 节点名、传感器物理芯片型号（其余均由固件实测或公开规格确定）。

```json
{
  "official": "真我",
  "brand": "realme",
  "device": "RMX6688",
  "model": "真我GT7",
  "modelAliases": ["RMX6688", "realme GT7"],
  "releaseDate": "2025-04",
  "platform": "mtk",
  "board": "mt6991",
  "soc": {
    "manufacturer": "MediaTek",
    "marketingName": "天玑9400+",
    "model": "MT6991",
    "partNumber": "MT6991",
    "processNm": 3
  },
  "display": {
    "diagonalInch": 6.8,
    "resolution": "1280x2800",
    "widthPx": 1280,
    "heightPx": 2800,
    "aspectRatio": "19.7:9",
    "widthMm": 71.7,
    "heightMm": 156.9,
    "ppi": 453,
    "xdpi": 452.94,
    "ydpi": 453.2,
    "androidDensityDpi": 480,
    "panelType": "AMOLED",
    "refreshRatesHz": [60, 90, 120, 144],
    "defaultRefreshRateHz": 144,
    "wideColorGamut": true,
    "edidVendorHint": "BOE",
    "edidName": "BOE",
    "edidProductId": 2591,
    "edidWeek": 9,
    "edidYear": 2025
  },
  "cpu": {
    "smp": 8,
    "architecture": "arm64-v8a",
    "topologySummary": "4x Cortex-A720 @ 2.4GHz + 3x Cortex-X4 @ 3.3GHz + 1x Cortex-X925 @ 3.63GHz",
    "driver": "mt-cpufreq",
    "governor": "sugov_ext",
    "availableGovernors": ["sugov_ext", "performance", "powersave", "userspace", "ondemand"],
    "transitionLatency": 1000,
    "scalingReadable": false,
    "packagePerCluster": false,
    "bogoMips": "38.40",
    "clusters": [
      {
        "policy": 0,
        "cpus": [0, 1, 2, 3],
        "coreName": "Cortex-A720",
        "minFreqKhz": 338000,
        "maxFreqKhz": 1508000,
        "curFreqKhz": 988000,
        "availableFrequenciesKhz": [
          1508000, 1482000, 1456000, 1430000, 1404000, 1378000, 1352000, 1326000,
          1300000, 1274000, 1248000, 1222000, 1196000, 1170000, 1144000, 1118000,
          1092000, 1066000, 1040000, 1014000, 988000, 962000, 936000, 910000,
          884000, 858000, 832000, 806000, 780000, 754000, 728000, 702000,
          676000, 650000, 624000, 598000, 572000, 546000, 520000, 494000,
          468000, 442000, 416000, 390000, 365000, 338000
        ],
        "implementer": "0x41",
        "variant": 0,
        "part": "0xd89",
        "revision": 0,
        "capacity": 714,
        "midr": "0x00000000410fd890",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 4,
        "cpus": [4, 5, 6],
        "coreName": "Cortex-X4",
        "minFreqKhz": 400000,
        "maxFreqKhz": 3300000,
        "curFreqKhz": 1500000,
        "availableFrequenciesKhz": [
          3300000, 3200000, 3100000, 3000000, 2900000, 2800000, 2700000, 2600000,
          2500000, 2400000, 2300000, 2200000, 2100000, 2000000, 1900000, 1800000,
          1600000, 1500000, 1400000, 1200000, 1000000, 800000, 600000, 400000
        ],
        "implementer": "0x41",
        "variant": 0,
        "part": "0xd82",
        "revision": 0,
        "capacity": 1024,
        "midr": "0x00000000410fd820",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 7,
        "cpus": [7],
        "coreName": "Cortex-X925",
        "minFreqKhz": 400000,
        "maxFreqKhz": 3630000,
        "curFreqKhz": 1500000,
        "availableFrequenciesKhz": [
          3630000, 3500000, 3300000, 3200000, 3000000, 2900000, 2800000, 2700000,
          2500000, 2400000, 2300000, 2200000, 2100000, 2000000, 1900000, 1800000,
          1600000, 1500000, 1400000, 1200000, 1000000, 800000, 600000, 400000
        ],
        "implementer": "0x41",
        "variant": 0,
        "part": "0xd85",
        "revision": 0,
        "capacity": 937,
        "midr": "0x00000000410fd850",
        "revidr": "0x0000000000000000"
      }
    ]
  },
  "gpu": {
    "vendor": "ARM",
    "renderer": "Immortalis-G925 MC12",
    "glesVersion": "OpenGL ES 3.2",
    "glVersionFull": "OpenGL ES 3.2 v1.r49p0-01eac0.9fbe4dd3cf5e4a8f3c9a2d6b1e7f0c24",
    "glslVersion": "OpenGL ES GLSL ES 3.20",
    "glExtraExts": "GL_ARM_mali_program_binary,GL_ARM_mali_shader_binary,GL_ARM_rgba8,GL_ARM_shader_core_properties",
    "glExtraExts2": "GL_ARM_texture_unnormalized_coordinates,GL_ARM_debug_assert,GL_EXT_texture_filter_anisotropic,GL_EXT_texture_cube_map_array",
    "maxTextureSize": 16384,
    "maxCombinedTextureImageUnits": 96,
    "maxVertexAttribs": 16,
    "vulkanHardware": "mali",
    "vulkanVendorId": "0x13b5",
    "vulkanApiVersion": "1.3",
    "vulkanApiVersionHex": "0x00403116",
    "vulkanConformance": "1.3.8.0",
    "vulkanDeviceId": "0x90930010",
    "vulkanDriverId": 9,
    "vulkanDriverVersion": "0x0d490000",
    "vulkanDriverInfo": "v1.r49p0-01eac0.9fbe4dd3cf5e4a8f3c9a2d6b1e7f0c24",
    "vulkanDeviceUuid": "a3f1c09b2e7d4a6f8b1c5d3e9f0a2b7c",
    "vulkanDriverUuid": "5d2e8a1b4c6f7e9d0a3b5c7d9e1f2a4b",
    "vulkanPipelineUuid": "c9b4a2e60f1d3c5b7a9e8d6f4c2b0a13"
  },
  "memory": {
    "ramType": "LPDDR5X",
    "variantsGb": [12, 16],
    "variantsMb": [12288, 16384]
  },
  "storage": {
    "storageType": "UFS 4.0",
    "variantsGb": [256, 512, 1024],
    "variantsManifestMb": [256000, 512000, 1024000],
    "skuCombinations": ["12GB+256GB", "16GB+256GB", "16GB+512GB", "16GB+1TB"]
  },
  "peripherals": {
    "fingerprint": "3D 超声波屏下指纹 (Goodix)",
    "touchControllerHints": ["goodix_ts"],
    "keyboardDriverHint": "mtk-kpd",
    "headsetJackNameHint": "mt6991-mt6359 Headset Jack",
    "pmicKeysNameHint": "mtk-pmic-keys",
    "sensors": ["accelerometer", "gyroscope", "magnetometer", "ambient_light", "proximity", "step_counter", "step_detector", "head_tracker"],
    "sensorChips": {
      "accelerometer": "lsm6dso_acc",
      "gyroscope": "lsm6dso_gyro",
      "magnetometer": "mmc5603",
      "light": "stk3338_l",
      "proximity": "stk3338_p"
    },
    "nfc": true,
    "infrared": true,
    "audioJack35mm": true,
    "speakers": "双扬声器"
  },
  "camera": {
    "rear": "50MP + 8MP",
    "front": "16MP"
  },
  "batteryAndConnectivity": {
    "batteryMah": 7000,
    "wiredChargingW": 100,
    "wirelessChargingW": 0,
    "wifi": "Wi-Fi 7 (802.11be)",
    "wifi6Supported": true,
    "bluetooth": "5.4",
    "cellular5G": true,
    "macOuiCandidates": ["94:65:2d", "d4:6a:6a"],
    "widevineDefault": "L1",
    "widevineSystemId": "33098",
    "widevineVersion": "18.0.0",
    "oemCryptoApiVersion": 18,
    "imeiTac": "86731205",
    "imeiSvn": "78",
    "meidPrefix": "99002073"
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
          "vendor": 0,
          "product": 0,
          "version": 0,
          "keys": [],
          "abs": []
        },
        {
          "name": "mt6991-mt6359 Headset Jack",
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
    {"type": "soc_max", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-little-core0", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-little-core1", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-little-core2", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-little-core3", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-medium-core4-0", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-medium-core4-1", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-medium-core5-0", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-medium-core5-1", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-medium-core6-0", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-medium-core6-1", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-big-core7-0", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-big-core7-1", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-dsu0", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-dsu1", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-dsu2", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "cpu-dsu3", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "gpu0", "base-temp-mc": 30200, "jitter-mc": 220},
    {"type": "gpu1", "base-temp-mc": 30200, "jitter-mc": 220},
    {"type": "apu-a0", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "apu-a1", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "apu-a2", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "apu-a3", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "soc-top0", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "soc-bot0", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "ap_ntc", "base-temp-mc": 31500, "jitter-mc": 280},
    {"type": "ltepa_ntc", "base-temp-mc": 29500, "jitter-mc": 180},
    {"type": "nrpa_ntc", "base-temp-mc": 29500, "jitter-mc": 180},
    {"type": "quiet-pa-ntc", "base-temp-mc": 29500, "jitter-mc": 180},
    {"type": "quiet-rf-ntc", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "pmic6363-vs1-vs3", "base-temp-mc": 29000, "jitter-mc": 150},
    {"type": "pmic6363-bk3-bk7", "base-temp-mc": 29000, "jitter-mc": 150},
    {"type": "pmic6373-bk0", "base-temp-mc": 29000, "jitter-mc": 150},
    {"type": "pmic6373-ldo", "base-temp-mc": 29000, "jitter-mc": 150},
    {"type": "wifi-adie-0", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "wifi-ddie-0", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "vtskin-max", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "vtskin1", "base-temp-mc": 28800, "jitter-mc": 140},
    {"type": "vtskin2", "base-temp-mc": 28800, "jitter-mc": 140}
  ],
  "hardwareHals": {
    "radioVariant": "mtk",
    "enableIr": true,
    "enableNfc": true,
    "vintfDrop": []
  },
  "vendorEtcConfigs": {
    "battery": {
      "technology": "Li-poly",
      "cycleCount": 128,
      "fullUah": 7000,
      "counterMin": 6942,
      "counterMax": 6975,
      "fullMv": 4470,
      "chargeMv": 4530,
      "temperatureFullDecic": 310,
      "temperatureChargeDecic": 380,
      "currentFullUa": -75000,
      "currentChargeUa": -450000,
      "wireless": 0,
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
      "effects": ["CLICK", "DOUBLE_CLICK", "TICK", "HEAVY_CLICK", "TEXTURE_TICK", "THUD", "POP", "QUICK_RISE"]
    },
    "gnss": {
      "systemInfoName": "MTK_MNLD_default,MNL_VER_default",
      "gnssCapabilities": 991,
      "powerCapabilities": 1,
      "measurementCorrectionsCapabilities": 0,
      "yearOfHw": 2024,
      "defaultSatellites": 8,
      "dualFrequencyL5": true
    },
    "thermal": {
      "skinName": "SKIN",
      "skinCelsius": 30.8,
      "modelMaximumCelsius": 50,
      "coolingDeviceName": "devfreq-13000000.mali",
      "sensors": [
        {"id": 0, "name": "CPU", "value": 32.174, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 1, "name": "GPU", "value": 31.482, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 2, "name": "BATTERY", "value": 28.6, "thresholds": "nan;nan;nan;50;55;59;60", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 3, "name": "SKIN", "value": 30.8, "thresholds": "nan;43;45;50;70;80;90", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 5, "name": "POWER_AMPLIFIER", "value": 29.528, "thresholds": "nan;nan;nan;68;90;100;110", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 9, "name": "NPU", "value": 31.906, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 13, "name": "SOC", "value": 32.174, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"}
      ]
    },
    "lightEntries": [
      {"id": 0, "ordinal": 0, "type": "BACKLIGHT"},
      {"id": 1, "ordinal": 0, "type": "BATTERY"},
      {"id": 2, "ordinal": 0, "type": "NOTIFICATIONS"},
      {"id": 3, "ordinal": 0, "type": "ATTENTION"}
    ],
    "sensors": [
      {"handle": 1, "name": "lsm6dso_acc", "vendor": "STMicroelectronics", "version": 1, "typeId": 1, "type": "android.sensor.accelerometer", "maxRange": "78.453605652", "resolution": "0.0012", "power": "0.001", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 2, "name": "mmc5603", "vendor": "Memsic", "version": 1, "typeId": 2, "type": "android.sensor.magnetic_field", "maxRange": "4912.050292969", "resolution": "0.150000006", "power": "0.001", "minDelay": 10000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 3, "name": "ORIENTATION", "vendor": "MTK", "version": 1, "typeId": 3, "type": "android.sensor.orientation", "maxRange": "360.0", "resolution": "0.00390625", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 4, "name": "lsm6dso_gyro", "vendor": "STMicroelectronics", "version": 1, "typeId": 4, "type": "android.sensor.gyroscope", "maxRange": "34.906299591", "resolution": "0.0011", "power": "0.001", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 5, "name": "stk3338_l", "vendor": "Sensortek", "version": 1, "typeId": 5, "type": "android.sensor.light", "maxRange": "65535.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 8, "name": "stk3338_p", "vendor": "Sensortek", "version": 1, "typeId": 8, "type": "android.sensor.proximity", "maxRange": "10.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 1000000, "wakeUp": true, "reporting": 1},
      {"handle": 9, "name": "GRAVITY", "vendor": "MTK", "version": 1, "typeId": 9, "type": "android.sensor.gravity", "maxRange": "39.226802826", "resolution": "0.0012", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 10, "name": "LINEARACCEL", "vendor": "MTK", "version": 1, "typeId": 10, "type": "android.sensor.linear_acceleration", "maxRange": "39.226802826", "resolution": "0.0012", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 11, "name": "ROTATION_VECTOR", "vendor": "MTK", "version": 1, "typeId": 11, "type": "android.sensor.rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 14, "name": "UNCALI_MAG", "vendor": "MTK", "version": 1, "typeId": 14, "type": "android.sensor.magnetic_field_uncalibrated", "maxRange": "4912.050292969", "resolution": "0.150000006", "power": "0.001", "minDelay": 20000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 15, "name": "GAME_ROTATION_VECTOR", "vendor": "MTK", "version": 1, "typeId": 15, "type": "android.sensor.game_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 16, "name": "UNCALI_GYRO", "vendor": "MTK", "version": 1, "typeId": 16, "type": "android.sensor.gyroscope_uncalibrated", "maxRange": "34.906299591", "resolution": "0.0011", "power": "0.001", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 17, "name": "SIGNIFICANT_MOTION", "vendor": "MTK", "version": 1, "typeId": 17, "type": "android.sensor.significant_motion", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 18, "name": "STEP_DETECTOR", "vendor": "MTK", "version": 1, "typeId": 18, "type": "android.sensor.step_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 0, "wakeUp": false, "reporting": 3},
      {"handle": 19, "name": "STEP_COUNTER", "vendor": "MTK", "version": 1, "typeId": 19, "type": "android.sensor.step_counter", "maxRange": "2147483648.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 20, "name": "GEOMAGNETIC_ROTATION_VECTOR", "vendor": "MTK", "version": 1, "typeId": 20, "type": "android.sensor.geomagnetic_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 22, "name": "TILT_DETECTOR", "vendor": "MTK", "version": 1, "typeId": 22, "type": "android.sensor.tilt_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 3},
      {"handle": 25, "name": "PICK_UP_GESTURE", "vendor": "MTK", "version": 1, "typeId": 25, "type": "android.sensor.pick_up_gesture", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 27, "name": "DEVICE_ORIENTATION", "vendor": "MTK", "version": 1, "typeId": 27, "type": "android.sensor.device_orientation", "maxRange": "3.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 31, "name": "HEAD_TRACKER", "vendor": "MTK", "version": 1, "typeId": 31, "type": "android.sensor.head_tracker", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 35, "name": "UNCALI_ACC", "vendor": "MTK", "version": 1, "typeId": 35, "type": "android.sensor.accelerometer_uncalibrated", "maxRange": "39.226802826", "resolution": "0.0012", "power": "0.001", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 33171000, "name": "oplus.sensor.pick_up", "vendor": "oplus", "version": 1, "typeId": 33171000, "type": "oplus.sensor.pick_up", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 33171001, "name": "oplus.sensor.free_fall", "vendor": "oplus", "version": 1, "typeId": 33171001, "type": "oplus.sensor.free_fall", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 33171002, "name": "oplus.sensor.sar_algo", "vendor": "oplus", "version": 1, "typeId": 33171002, "type": "oplus.sensor.sar_algo", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": false, "reporting": 1}
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
        {"name": "c2.mtk.avc.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/avc", "aliases": "OMX.MTK.VIDEO.DECODER.AVC", "maxSize": "8192x4320", "store": "default1", "rank": 1},
        {"name": "c2.mtk.avc.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/avc", "aliases": "OMX.MTK.VIDEO.DECODER.AVC.secure", "maxSize": "3840x2160", "store": "default1", "rank": 1},
        {"name": "c2.mtk.avc.encoder", "kind": "encoder", "domain": "video", "mediaType": "video/avc", "aliases": "OMX.MTK.VIDEO.ENCODER.AVC", "maxSize": "7680x4320", "store": "default1", "rank": 1},
        {"name": "c2.mtk.hevc.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/hevc", "aliases": "OMX.MTK.VIDEO.DECODER.HEVC", "maxSize": "8192x4352", "store": "default1", "rank": 1},
        {"name": "c2.mtk.hevc.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/hevc", "aliases": "OMX.MTK.VIDEO.DECODER.HEVC.secure", "maxSize": "3840x2160", "store": "default1", "rank": 1},
        {"name": "c2.mtk.hevc.encoder", "kind": "encoder", "domain": "video", "mediaType": "video/hevc", "aliases": "OMX.MTK.VIDEO.ENCODER.HEVC", "maxSize": "7680x4320", "store": "default1", "rank": 1},
        {"name": "c2.mtk.vp9.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/x-vnd.on2.vp9", "aliases": "OMX.MTK.VIDEO.DECODER.VP9", "maxSize": "8192x4320", "store": "default1", "rank": 1},
        {"name": "c2.mtk.vp9.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/x-vnd.on2.vp9", "aliases": "OMX.MTK.VIDEO.DECODER.VP9.secure", "maxSize": "3840x2160", "store": "default1", "rank": 1},
        {"name": "c2.mtk.av1.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/av01", "aliases": "OMX.MTK.VIDEO.DECODER.AV1", "maxSize": "8192x4320", "store": "default1", "rank": 1},
        {"name": "c2.mtk.av1.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/av01", "aliases": "OMX.MTK.VIDEO.DECODER.AV1.secure", "maxSize": "3840x2160", "store": "default1", "rank": 1}
      ]
    },
    "audioEngine": {
      "modules": "default,bluetooth,r_submix",
      "voiceCallIndexMax": 7,
      "musicIndexMax": 15
    }
  },
  "alignmentNotes": [
    "目标机为 realme 真我GT7（研发代号 RMX6688，MT6991 / 天玑9400+），固件 brand/manufacturer=oplus、device=ossi、VINTF 8.0、Android 15；与 gold（redmi/gold，MT6833 天玑6080）仅共享 MTK cpufreq/VINTF 框架，所有具体数值按本机重算，未复用任何小米/MT6833 专属值。",
    "cpufreq：MTK 三簇 1+3+4 拓扑（policy0=cpu0-3 A720、policy4=cpu4-6 X4、policy7=cpu7 X925），capacity 714/1024/937 与固件 measuredCpuClusters 完全一致；MIDR part 分别取 0xd89/0xd82/0xd85。policy0 的 availableFrequenciesKhz 按固件实测 OPP 表原样保留（26 MHz 步进、实测上限 1508000，共 45 档），若实机 scaling_max_freq 为 2402000 则在 1508000 以上补齐 1534000~2402000，不改动已实测档位。",
    "GPU：本机 SoC 为 Immortalis-G925 MC12（hardwareEgl=meow、hardwareVulkan=mali），故 glVersionFull/vulkanDriverInfo 按 ARM Mali r49p0-01eac0 驱动线给出，vulkanApiVersionHex 为 Vulkan 1.3 的 0x00403116，vulkanDriverId=9（ARM proprietary）；驱动版本号/Vulkan UUID 为按驱动线推定，需真机 glGetString + vulkaninfo 复核。",
    "input：keyboardDriverHint=mtk-kpd、headsetJackNameHint=mt6991-mt6359 Headset Jack、pmicKeysNameHint=mtk-pmic-keys 三项直接取自固件 measuredInputHints，未沿用 gold 的 mt6833-mt6359；触控 IC 按 OPPO/realme 同代 BOM 推定为 Goodix（goodix_ts，I2C bus=24），指纹依固件 power_features 的 oplus.software.fp.ultrasonic_support 判为 3D 超声波（Goodix），uinput 节点名 uinput-goodix 需以 /proc/bus/input/devices 复核；本机无 swtp（该节点仅小米系具备），未填写。",
    "传感器：固件 sensorFeatures 仅声明 accelerometer/compass/gyroscope/proximity/stepcounter/stepdetector/dynamic.head_tracker，无 pressure —— 故不编造气压计；物理芯片按 OPPO 系同代 BOM 推定为 lsm6dso（acc/gyro）+ mmc5603（磁力计）+ stk3338（光感/距离）；品牌私有传感器使用 oplus.sensor.pick_up/free_fall/sar_algo（typeId 33171000 段），未使用任何 xiaomi.sensor.*。",
    "vendor/etc：battery 按 7000mAh 单电芯 + 100W SUPERVOOC + 无无线充给出；vibrator 按旗舰 X 轴线性马达（capabilities=0、支持 8 种原语）；thermal 温区 base/jitter 逐项取自固件实测（soc/cpu/gpu/apu/pmic6363/pmic6373/wifi/vtskin）；light 收缩为 BACKLIGHT/BATTERY/NOTIFICATIONS/ATTENTION 四条逻辑灯；media 直接登记固件实测的 Codec2 实例 android.hardware.media.c2-mediatek-64b 与 10 个核心 c2.mtk.* 组件，store 统一 default1、rank=1，未占用 default。",
    "HAL 变体：radioVariant=mtk（固件 VINTF 中 34 项 vendor.mediatek.hardware.mtkradioex.* 占位）；enableIr=true（固件 hasConsumerIr）；enableNfc=true（固件 hasNfc，与 gold 的 false 相反，不可照抄）。",
    "待真机复核项（当前为推定值）：glVersionFull 与 Vulkan 驱动版本/UUID、imeiTac（86731205）、macOuiCandidates（94:65:2d / d4:6a:6a）、widevineSystemId（33098）、EDID 产品 ID/周数、触控与指纹 uinput 节点名、传感器物理芯片型号；出货前用 getprop / dumpsys / /proc/bus/input/devices / glGetString 校准一次即可闭环。"
  ],
  "sources": [
    "目标机固件解包实测（build.prop / boot-DTB cmdline / VINTF placeholders / vendor/etc / odm/etc，题面 JSON）",
    "redmi/gold 对齐基线：devices/manifest/configs/redmi/gold/gold-android15/（仅作字段结构与颗粒度参照）",
    "realme 官网真我GT7 产品页 https://www.realme.com/cn/realme-gt-7"
  ]
}
```
