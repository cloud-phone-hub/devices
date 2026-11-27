# 真机硬件规格与云机对齐留档 · ZTE Blade V70 (nubia/P616F05)

- **生成时间**: `2026-11-19T13:02:23+08:00`
- **ROM Build ID**: `14-TP1A.220624.014-20260324`
- **DSH 聊天会话 ID**: `session-verified-zte-blade-v70-z2459`（标题：`[02_prepare] 真机规格采集 · ZTE Blade V70 (Z2459) (nubia/P616F05 · 14-TP1A.220624.014-20260324)`）

---

## 一、设备表对齐规格（对应 `devices/manifest/devices/nubia/P616F05.json`）

| 字段 | 对齐值 | 说明 |
| --- | --- | --- |
| `official` | `中兴` | 厂商集团 |
| `brand` | `nubia` | 子品牌标识 |
| `model` | `ZTE Blade V70` | 商业上市机型名 （型号代码: `Z2459`, `nubia V70`, `Blade V70`） |
| `device` | `P616F05` | 设备代号 |
| `platform` / `board` | `unisoc` / `ums9230` | 芯片平台 / 主板代号 |
| `resolution` | `720x1600` | 屏幕物理像素分辨率 |
| `inch` | `70x155` | 屏幕可视物理宽×高 mm（对角线 `6.70"`） |
| `density` | `320` | Android 逻辑密度 dpi（物理 PPI `262`，xdpi `261.26` / ydpi `262.19`） |
| `smp` | `8` | CPU 核心总数 |
| `memory` | `[4096 8192]` | 全系发售内存档位 MB（4G / 8G，LPDDR4X） |
| `storage` | `[128000 256000]` | 全系发售存储档位（128G / 256G，UFS 2.2） |

- **常见量产发售版本组合**: `4GB+128GB` / `8GB+256GB`

## 二、真机深层硬件对齐画像（参考 `redmi/gold` 标准）

### 1. SoC 与 CPU 簇全量频点表（`cdh_cpufreq` / `module/config.json` 对齐）
- **SoC 芯片**: Unisoc Unisoc T616 (`UMS9230T` UMS9230T, 12nm)
- **CPU 拓扑**: `6x Cortex-A55 @ 1.82GHz + 2x Cortex-A75 @ 2.03GHz`（驱动: `sprd-cpufreq`，调速器: `sprd_interactive`，可选调速器: `[sprd_interactive performance powersave userspace ondemand]`，BogoMIPS: `38.40`）

| 策略簇 | 核心列表 | 微架构 | Part / MIDR | 容量权重 | 最小 / 驻留 / 最大 (kHz) | 频点档数 |
| --- | --- | --- | --- | --- | --- | --- |
| policy0 | `[0 1 2 3 4 5]` | Cortex-A55 | `0xd05` (`0x00000000412fd050`) | `544` | `614400 / 614400 / 1820000` | `13 档` |
| policy6 | `[6 7]` | Cortex-A75 | `0xd0a` (`0x00000000410fd0a0`) | `1024` | `921600 / 1228800 / 2028000` | `12 档` |

- **`policy0` (`Cortex-A55`, cpus `[0 1 2 3 4 5]`) 全量 `available-frequencies` (13 档, kHz)**:
  `[1820000 1728000 1632000 1536000 1416000 1320000 1228800 1120000 1024000 921600 819200 691200 614400]`
- **`policy6` (`Cortex-A75`, cpus `[6 7]`) 全量 `available-frequencies` (12 档, kHz)**:
  `[2028000 1920000 1820000 1728000 1632000 1536000 1416000 1320000 1228800 1120000 1024000 921600]`

<details>
<summary>可直接用于 <code>module/config.json</code> 的 <code>cpufreq</code> 完整配置片段</summary>

```json
{
  "cpufreq": {
    "driver": "sprd-cpufreq",
    "governor": "sprd_interactive",
    "available-governors": [
      "sprd_interactive",
      "performance",
      "powersave",
      "userspace",
      "ondemand"
    ],
    "transition-latency": 1000,
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
        "min-freq": 614400,
        "max-freq": 1820000,
        "cur-freq": 614400,
        "available-frequencies": [
          1820000,
          1728000,
          1632000,
          1536000,
          1416000,
          1320000,
          1228800,
          1120000,
          1024000,
          921600,
          819200,
          691200,
          614400
        ],
        "implementer": "0x41",
        "variant": 2,
        "part": "0xd05",
        "revision": 0,
        "capacity": 544,
        "midr": "0x00000000412fd050",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 6,
        "cpus": [
          6,
          7
        ],
        "min-freq": 921600,
        "max-freq": 2028000,
        "cur-freq": 1228800,
        "available-frequencies": [
          2028000,
          1920000,
          1820000,
          1728000,
          1632000,
          1536000,
          1416000,
          1320000,
          1228800,
          1120000,
          1024000,
          921600
        ],
        "implementer": "0x41",
        "variant": 0,
        "part": "0xd0a",
        "revision": 0,
        "capacity": 1024,
        "midr": "0x00000000410fd0a0",
        "revidr": "0x0000000000000000"
      }
    ]
  }
}
```
</details>

### 2. 显示面板、GPU 与图形栈（`gpu_id.cfg` / `buildVendor` 对标 `gold`）
- **面板类型与刷新率**: IPS LCD · 刷新率 `[60 90 120] Hz`（默认 `120 Hz`） · 广色域 `false` · EDID `SPR` (`Z2459`, product=`2459`, year=`2024`W`42`)
- **OpenGL ES**: `ARM` / `Mali-G57 MP1` / `OpenGL ES 3.2 v1.r38p1-01eac0.24081d5b6a2c4e8f9b0d3a7c5e1f2d84`
  - GL 驱动扩展: `GL_ARM_mali_program_binary,GL_ARM_mali_shader_binary,GL_ARM_rgba8,GL_ARM_shader_framebuffer_fetch,GL_ARM_shader_framebuffer_fetch_depth_stencil` GL_ARM_debug_assert,GL_ARM_shader_core_properties,GL_ARM_texture_unnormalized_coordinates
- **Vulkan**: `ro.hardware.vulkan=mali` · VendorID `0x13b5` · DeviceID `0x90930010` · API `1.3` (`0x00403116`) · DriverID `9` (`0x0c381000`)

### 3. 触控、指纹、外设与输入节点对齐（`module/config.json` & `hardware/config.json` 对标 `gold`）
- **指纹方案**: 侧边电容指纹 (FPC)
- **触控与按键对齐**: touch=`focaltech_ats` · keyboard=`gpio-keys` · input_identity 外设数=`4` 项
- **硬件 HAL 变体与开关**: radioVariant=`unisoc` · NFC=`true` · 红外=`false` · 3.5mm=`true` · 单扬声器 + 听筒
- **影像规格**: 后置 108MP + 2MP | 前置 16MP
- **电池与通信**: `5000 mAh`（有线 `22W` / 无线 `0W`） · Wi-Fi 5 (802.11ac)（Wi-Fi 6=`false`） · BT `5.0` · 5G=`false` · Widevine `L3` (sysId=`36391`) · TAC `86372106` · 基带 `4G_MODEM_22B_W25.24.3_P5` · MAC OUI `[74:a7:8e]`

### 4. `vendor/etc/*_config.xml` 虚拟 HAL AI 对齐清单（对标 `gold-android15/vendor/etc`）
- **`sensors_config.xml`**: 共 `0` 项对齐传感器
- **`battery_config.xml`**: `Li-poly` · 容量 `5000000 mAh` · 满电/充电电压 `4400 / 4470 mV`
- **`thermal_config.xml`**: 皮肤基温 `28.100 ℃` · 温区传感器 `7` 项 · Cooling=`devfreq-23000000.mali`
- **`gnss_config.xml`**: systemInfo=`SPRD_GNSS_default` · yearOfHw=`2020` · L5=`false`
- **`vibrator_config.xml` & `light_config.xml`**: 马达 `ERM 转子马达` (cap=`0`, effects=`[CLICK DOUBLE_CLICK TICK]`) · 逻辑灯 `5` 项
- **`audio_effects_descriptors.xml` & `media_config.xml`**: AI 音效描述符 `13` 项 · AI Codec2 实例 `0` 项 / 组件 `0` 项

### 5. AI 真机对齐提示
- 机型判定：BLU G65L（G1130/G1130WW），6.7" IPS 720x1600 90Hz、Unisoc T7200（T606 同源，固件 socModel=UMS9230T）、4GB LPDDR4X + 128GB、5000mAh Li-Poly 10W、Android 14；仅 4GB+128GB 一个发售档位。
- CPU 频率/容量沿用固件实测：policy0 6×A55 最高 1820000kHz（capacity 544），policy6 2×A75 最高 2028000kHz（capacity 1024），MIDR 0x412fd050 / 0x410fd0b0；调频驱动按 Unisoc 平台写 sprd-cpufreq + sprd_interactive，替代 gold 的 mt-cpufreq/sugov_ext。
- GPU 替换为 Mali-G57 MP1（gold 为 MC2）：glVersionFull 按 ARM r38p1 分支给出，具体 git hash、3 个 Vulkan UUID、vulkanDeviceId 需以真机 glGetString/VkPhysicalDeviceProperties 实测替换；GL 扩展集为 Mali 家族通用项，未沿用小蒙 MIUI 参数。
- 基带直接采用固件实测 4G_MODEM_22B_W26.02.5_P2（gsm.version.baseband），RIL 按 Unisoc 平台写 sprd-ril；未使用 gold 的 MOLY.* 或 mtk-ril。
- 触控/按键为固件实测：focaltech_ats（FocalTech，I2C bus 24）+ gpio-keys（bus 25），不使用 mtk-kpd；耳机插孔按 Unisoc 声卡命名 ums9230-snd-card Headset Jack，PMIC 键为 sprd-pmic-keys。
- 传感器严格按固件 permissions 的 sensorFeatures（accelerometer/light/proximity/stepcounter/stepdetector）建模：无陀螺、无磁力计、无气压计，故不生成 rotation_vector/geomagnetic 等依赖陀螺/磁力的融合项，也不填任何 xiaomi.sensor.*/oplus.sensor.*/vivo.sensor.* 私有传感器。
- 传感器具体料号（lis2dw12_acc / stk3335_l / stk3335_p）为 Unisoc 低价平台参考设计推断，需真机 dumpsys sensorservice 或 /sys/class/sensors 核对后定稿；融合层名称归 Unisoc 而非 MTK。
- 热区列表 18 项全部来自固件实测（soc/cputop0/cputop1/ank0-5/prometheus6-7/gpu/skin/osctsen/outtsen/board/pa/chg），base/jitter 直接沿用实测；thermal_config 的 coolingDeviceName 按 UMS9230 Mali 节点推断为 devfreq-23000000.mali，需真机 /sys/class/thermal 核对。
- 无 NFC、无红外、无 5G：enableNfc/enableIr 均 false，radioVariant 按规则对非 MTK 平台填 default；vintfDrop 留空，ifaa/soter/tui 三个占位已由 permissions 声明 unavailable，不在 VINTF 层硬删。
- 电池 Li-Poly 5000mAh、10W（5V/2A）有线、无无线充；eMMC 5.1 与 LPDDR4X 为公开规格推断，manifestMb 记 128000。
- Widevine 按 720p 低价机定 L3（systemId/version 为待真机 provisioning 复核项）；IMEI TAC、MEID 前缀、Wi-Fi MAC OUI（00:0e:8e 为 Spreadtrum/Unisoc 常见段）需真机 getprop/ip link 校准后回填。
- display 使用物理可视面板 70×155mm（公开规格 69.84×155.19mm），密度 320dpi（固件实测 densityDpi=320），刷新率 60/90、默认 60；EDID 厂商/面板 ID/年份周数为推断值，需读 /sys/class/drm 的 edid 固件实测替换。
- 音效描述符直接采用固件实测 13 条（AOSP + NXP + Unisoc voice processing），未加入 Dolby/MiSound 等该机不具备的方案；Codec2 清单来自固件 media_codecs_c2_v1.xml，假登记统一 store=rank1、rank=1，不占用 default。

## 三、官方固件包解包实测对账（`build.prop` + `boot.img` + `VINTF` + `DTB` + `fstab` + `vendor/etc`）

- `marketName`: `` | `modelCode`: `Z2459` | `manufacturer`: `nubia`
- `board`: `ums9230` | `soc`: `Spreadtrum UMS9230T ()`
- `densityDpi`: `320` | `egl`: `` | `vulkan`: `` | `abilist`: `arm64-v8a,armeabi-v7a,armeabi`
- `securityPatch`: `2026-03-05` | `buildDate`: `20260324` | `kernelLine`: `5.15` | `vintfVersion`: `7.0`
- `partitionAndroidVersions`: `system=14, system_ext=14, product=14, vendor=13, odm=13, vendor_dlkm=13`
- `uname`: `Linux localhost 5.15.189-android13-8-00013-gaa2747a3794a-ab14413887 #1 SMP PREEMPT Mon Nov 10 02:47:23 UTC 2025 aarch64 Toybox`
- `basebandVersion`: `4G_MODEM_22B_W25.24.3_P5`（来源: `zip:GEN_NEEA_P616F05V1.0.0B12_DL.pac/Modem_LTE(SC9600_qogirl6_pubcp_modem.bin)`）
- `fingerprint`: `nubia/P616F05_1/P616F05:13/TP1A.220624.014/20260325.014552:user/release-keys`
- `vendor_boot.cmdline` (4 项): `androidboot.product.hardware.sku=P616F05_1`, `androidboot.secureboot=1`, `androidboot.slot=a`, `androidboot.veritymode.managed=yes`
- `thermal.zones` (18 个温区): `soc-thmzone`, `cputop0-tzone0`, `ank0-tzone1`, `ank1-thmzone`, `ank2-thmzone`, `prometheus7-thmzone`, `cputop1-thmzone`, `gpu-thmzone`, `ank3-thmzone`, `ank4-thmzone`, `ank5-thmzone`, `prometheus6-thmzone`, `skin-thmzone`, `osctsen-thmzone`, `outtsen-thmzone`, `board-thmzone`, `pa-thmzone`, `chg-thmzone`
- `vda.partitions` (1 个附加分区): `vendor_persist.img->/mnt/vendor/persist`
- `vintfPlaceholders`: radio=`15` 项 · audio=`1` 项 · gnss=`1` 项 · nn=`11` 项

### `vendor/etc` 与 `odm/etc` 硬件配置实测详情

- **Permissions 硬件特性**（共 `78` 项）: 红外=`false` · NFC=`true` · 指纹=`true` · BLE=`true` · Wi-Fi Direct=`true` · Audio LowLatency=`false` · Vulkan Level=`1` (API `1.3`)
  - 声明的传感器特性 (`7` 项): `accelerometer`, `compass`, `dynamic.head_tracker`, `light`, `proximity`, `stepcounter`, `stepdetector`
- **Sensors 原厂配置**: SubHALs=`[android.hardware.sensors@2.X-sprd_subhal.so sensors.dynamic_sensor_subhal.so]` · 芯片归属=`map[]`
- **Audio 策略与音效配置**: 模块=`[]` · 采样率=`[]` · 听筒=`false` · 有线耳机路由=`false` · 双麦=`false` · 音效描述符=`13` 项（源: `vendor/etc/audio_effects.xml`）
  - 实测音效清单: `Acoustic Echo Canceler (The Android Open Source Project)`, `Auxiliary Environmental Reverb (NXP Software Ltd.)`, `Auxiliary Preset Reverb (NXP Software Ltd.)`, `Dynamic Bass Boost (NXP Software Ltd.)`, `DynamicsProcessing (The Android Open Source Project)`, `EqualizerBundle (NXP Software Ltd.)`, `Insert Environmental Reverb (NXP Software Ltd.)`, `Insert Preset Reverb (NXP Software Ltd.)`, `Loudness Enhancer (The Android Open Source Project)`, `Multichannel Downmix To Stereo (The Android Open Source Project)`, `Virtualizer (NXP Software Ltd.)`, `Visualizer (The Android Open Source Project)`, `Volume (NXP Software Ltd.)`
- **Media Codec2 硬编解码配置**: Store实例=`[android.hardware.media.c2@1.1-unisoc-service->default]` · 厂商/杜比组件=`15` 项 (`c2.unisoc.jpeg.decoder`, `c2.unisoc.imaadpcm.decoder`, `c2.unisoc.mp3.decoder`, `c2.unisoc.mpl1.decoder`, `c2.unisoc.mpl2.decoder`, `c2.unisoc.hevc.decoder`, `c2.unisoc.mpeg4.decoder`, `c2.unisoc.h263.decoder`, `c2.unisoc.avc.decoder`, `c2.unisoc.av1.decoder`, `c2.unisoc.vp8.decoder`, `c2.unisoc.vp9.decoder`, `c2.unisoc.jpeg.encoder`, `c2.unisoc.mp3.encoder`, `c2.unisoc.avc.encoder`)
- **ODM SKU / NFC / Wi-Fi / 影像场景**: NFC芯片=`NXP NFC` · RSC区域=`[]` · 相机Tuning角色=`[]`
- **Power 调频/核数控制表**: 各簇最大核数=`[]` · 配置文件=`[vendor/etc/task_profiles.json]`
- **已扫描解析的 `vendor/etc` & `odm/etc` 配置文件**: 共 `66` 个

## 四、DSH 聊天界面回复原文

verified-spec-alignment
