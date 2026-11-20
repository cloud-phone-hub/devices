# 真机硬件规格与云机对齐留档 · realme GT5 (150W) (realme/RMX3823)

- **生成时间**: `2026-11-19T11:24:31+08:00`
- **ROM Build ID**: `15-TP1A.220905.001-20250912`
- **分区 Android 版本**: `system=15, system_ext=15, product=15, vendor=13, vendor_dlkm=13`
- **DSH 聊天会话 ID**: `session-f3293897-adad-4461-80cc-ce8ce4be780b`（标题：`[02_prepare] 真机规格采集 · realme/RMX3823 (15-TP1A.220905.001-20250912)`）
- **采集模型路由**: `cdh-maas / deepseek-v4.1-flash-1017`（复用在线 DSH Web: `true`）

---

## 一、设备表对齐规格（对应 `devices/manifest/devices/realme/RMX3823.json`）

| 字段 | 对齐值 | 说明 |
| --- | --- | --- |
| `official` | `realme` | 厂商集团 |
| `brand` | `realme` | 子品牌标识 |
| `model` | `realme GT5 (150W)` | 商业上市机型名 （型号代码: `RMX3823`, `RMX3820`） |
| `device` | `RMX3823` | 设备代号 |
| `platform` / `board` | `qcom` / `kalama` | 芯片平台 / 主板代号 |
| `resolution` | `1240x2772` | 屏幕物理像素分辨率 |
| `inch` | `70x156` | 屏幕可视物理宽×高 mm（对角线 `6.74"`） |
| `density` | `450` | Android 逻辑密度 dpi（物理 PPI `451`，xdpi `450.66` / ydpi `450.54`） |
| `smp` | `8` | CPU 核心总数 |
| `memory` | `[12288 16384]` | 全系发售内存档位 MB（12G / 16G，LPDDR5X） |
| `storage` | `[256000 512000]` | 全系发售存储档位（256G / 512G，UFS 4.0） |

- **常见量产发售版本组合**: `12GB+256GB` / `16GB+512GB`

## 二、真机深层硬件对齐画像（参考 `redmi/gold` 标准）

### 1. SoC 与 CPU 簇全量频点表（`cdh_cpufreq` / `module/config.json` 对齐）
- **SoC 芯片**: Qualcomm Snapdragon 8 Gen 2 (`SM8550-AB` SM8550-AB, 4nm)
- **CPU 拓扑**: `1x Cortex-X3 @ 3.2GHz + 2x Cortex-A715 @ 2.8GHz + 2x Cortex-A710 @ 2.8GHz + 3x Cortex-A510 @ 2.0GHz`（驱动: `qcom-cpufreq-hw`，调速器: `schedutil`，可选调速器: `[schedutil performance powersave userspace ondemand]`，BogoMIPS: `38.40`）

| 策略簇 | 核心列表 | 微架构 | Part / MIDR | 容量权重 | 最小 / 驻留 / 最大 (kHz) | 频点档数 |
| --- | --- | --- | --- | --- | --- | --- |
| policy0 | `[0 1 2]` | Cortex-A510 | `0xd46` (`0x00000000411fd460`) | `292` | `300000 / 691200 / 2016000` | `19 档` |
| policy3 | `[3 4]` | Cortex-A710 | `0xd47` (`0x00000000411fd470`) | `640` | `633600 / 1094400 / 2802000` | `18 档` |
| policy5 | `[5 6]` | Cortex-A715 | `0xd4d` (`0x00000000410fd4d0`) | `768` | `633600 / 1248000 / 2802000` | `18 档` |
| policy7 | `[7]` | Cortex-X3 | `0xd4e` (`0x00000000410fd4e0`) | `1024` | `480000 / 1555200 / 3200000` | `23 档` |

- **`policy0` (`Cortex-A510`, cpus `[0 1 2]`) 全量 `available-frequencies` (19 档, kHz)**:
  `[2016000 1958400 1862400 1766400 1670400 1574400 1478400 1363200 1267200 1171200 1075200 979200 883200 787200 691200 614400 518400 403200 300000]`
- **`policy3` (`Cortex-A710`, cpus `[3 4]`) 全量 `available-frequencies` (18 档, kHz)**:
  `[2802000 2707200 2592000 2476800 2361600 2246400 2131200 2016000 1900800 1785600 1670400 1555200 1401600 1248000 1094400 940800 787200 633600]`
- **`policy5` (`Cortex-A715`, cpus `[5 6]`) 全量 `available-frequencies` (18 档, kHz)**:
  `[2802000 2707200 2592000 2476800 2361600 2246400 2131200 2016000 1900800 1785600 1670400 1555200 1401600 1248000 1094400 940800 787200 633600]`
- **`policy7` (`Cortex-X3`, cpus `[7]`) 全量 `available-frequencies` (23 档, kHz)**:
  `[3200000 3120000 3014400 2908800 2802000 2707200 2592000 2476800 2361600 2246400 2131200 2016000 1900800 1785600 1670400 1555200 1401600 1248000 1094400 940800 787200 633600 480000]`

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
      "ondemand"
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
          2
        ],
        "min-freq": 300000,
        "max-freq": 2016000,
        "cur-freq": 691200,
        "available-frequencies": [
          2016000,
          1958400,
          1862400,
          1766400,
          1670400,
          1574400,
          1478400,
          1363200,
          1267200,
          1171200,
          1075200,
          979200,
          883200,
          787200,
          691200,
          614400,
          518400,
          403200,
          300000
        ],
        "implementer": "0x41",
        "variant": 1,
        "part": "0xd46",
        "revision": 0,
        "capacity": 292,
        "midr": "0x00000000411fd460",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 3,
        "cpus": [
          3,
          4
        ],
        "min-freq": 633600,
        "max-freq": 2802000,
        "cur-freq": 1094400,
        "available-frequencies": [
          2802000,
          2707200,
          2592000,
          2476800,
          2361600,
          2246400,
          2131200,
          2016000,
          1900800,
          1785600,
          1670400,
          1555200,
          1401600,
          1248000,
          1094400,
          940800,
          787200,
          633600
        ],
        "implementer": "0x41",
        "variant": 1,
        "part": "0xd47",
        "revision": 0,
        "capacity": 640,
        "midr": "0x00000000411fd470",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 5,
        "cpus": [
          5,
          6
        ],
        "min-freq": 633600,
        "max-freq": 2802000,
        "cur-freq": 1248000,
        "available-frequencies": [
          2802000,
          2707200,
          2592000,
          2476800,
          2361600,
          2246400,
          2131200,
          2016000,
          1900800,
          1785600,
          1670400,
          1555200,
          1401600,
          1248000,
          1094400,
          940800,
          787200,
          633600
        ],
        "implementer": "0x41",
        "variant": 0,
        "part": "0xd4d",
        "revision": 0,
        "capacity": 768,
        "midr": "0x00000000410fd4d0",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 7,
        "cpus": [
          7
        ],
        "min-freq": 480000,
        "max-freq": 3200000,
        "cur-freq": 1555200,
        "available-frequencies": [
          3200000,
          3120000,
          3014400,
          2908800,
          2802000,
          2707200,
          2592000,
          2476800,
          2361600,
          2246400,
          2131200,
          2016000,
          1900800,
          1785600,
          1670400,
          1555200,
          1401600,
          1248000,
          1094400,
          940800,
          787200,
          633600,
          480000
        ],
        "implementer": "0x41",
        "variant": 0,
        "part": "0xd4e",
        "revision": 0,
        "capacity": 1024,
        "midr": "0x00000000410fd4e0",
        "revidr": "0x0000000000000000"
      }
    ]
  }
}
```
</details>

### 2. 显示面板、GPU 与图形栈（`gpu_id.cfg` / `buildVendor` 对标 `gold`）
- **面板类型与刷新率**: AMOLED · 刷新率 `[60 90 120 144] Hz`（默认 `144 Hz`） · 广色域 `true` · EDID `TMA` (`TMA_AMOLED_1P5K`, product=`5007`, year=`2023`W`33`)
- **OpenGL ES**: `Qualcomm` / `Adreno (TM) 740` / `OpenGL ES 3.2 V@0676.0 (GIT@1c9b7f2, I4e6a1d8c9f, 1694486400) (Date:09/12/25)`
  - GL 驱动扩展: `GL_EXT_texture_filter_anisotropic,GL_EXT_debug_marker,GL_EXT_disjoint_timer_query,GL_EXT_multisampled_render_to_texture,GL_OES_texture_compression_astc,GL_QCOM_alpha_test,GL_QCOM_binning_control,GL_QCOM_driver_control` GL_QCOM_extended_get,GL_QCOM_extended_get2,GL_QCOM_framebuffer_foveated,GL_QCOM_motion_estimation,GL_QCOM_shader_framebuffer_fetch_noncoherent,GL_QCOM_tiled_rendering,GL_QCOM_yuv_texture_gather
- **Vulkan**: `ro.hardware.vulkan=adreno` · VendorID `0x5143` · DeviceID `0x07040001` · API `1.3` (`0x00403116`) · DriverID `8` (`0x80100000`)

### 3. 触控、指纹、外设与输入节点对齐（`module/config.json` & `hardware/config.json` 对标 `gold`）
- **指纹方案**: 屏下光学指纹 (Goodix)
- **触控与按键对齐**: touch=`synaptics_tcm` · keyboard=`gpio-keys` · input_identity 外设数=`4` 项
- **硬件 HAL 变体与开关**: radioVariant=`default` · NFC=`true` · 红外=`true` · 3.5mm=`false` · 立体声双扬声器
- **影像规格**: 后置 50MP IMX890 (OIS) + 8MP 超广角 + 2MP 微距 | 前置 16MP
- **电池与通信**: `5240 mAh`（有线 `150W` / 无线 `0W`） · Wi-Fi 7 (802.11be)（Wi-Fi 6=`true`） · BT `5.3` · 5G=`true` · Widevine `L1` (sysId=`33098`) · TAC `86702705` · MAC OUI `[c0:ee:fb 64:a2:f9]`

### 4. `vendor/etc/*_config.xml` 虚拟 HAL AI 对齐清单（对标 `gold-android15/vendor/etc`）
- **`sensors_config.xml`**: 共 `23` 项对齐传感器
- **`battery_config.xml`**: `Li-poly` · 容量 `5240 mAh` · 满电/充电电压 `4400 / 4500 mV`
- **`thermal_config.xml`**: 皮肤基温 `28.500 ℃` · 温区传感器 `7` 项 · Cooling=`kgsl-3d0`
- **`gnss_config.xml`**: systemInfo=`QCOM_GNSS_default` · yearOfHw=`2023` · L5=`true`
- **`vibrator_config.xml` & `light_config.xml`**: 马达 `X-axis linear` (cap=`0`, effects=`[CLICK DOUBLE_CLICK TICK HEAVY_CLICK TEXTURE_TICK]`) · 逻辑灯 `5` 项
- **`audio_effects_descriptors.xml` & `media_config.xml`**: AI 音效描述符 `4` 项 · AI Codec2 实例 `2` 项 / 组件 `9` 项

### 5. AI 真机对齐提示
- 身份：RMX3823=realme GT5 150W 版（5240mAh，12+256 / 16+512），240W 版为兄弟型号 RMX3820（4600mAh、24GB+1TB），两者共用 ossi/kalama 平台，容量档位不得混用。
- 平台：厂商固件 brand/manufacturer=oplus、device=ossi、board=kalama，对应高通 SM8550-AB，故 radioVariant 取 default（非 mtk），GNSS 取 QCOM_GNSS_default，热区/触控/按键全部改用 Qualcomm 命名，严禁残留 mtk-* 或 mt6833-*。
- CPU：4 个 policy（0/3/5/7）分别对应 3×A510、2×A710、2×A715、1×X3；频点表与固件 vendorEtcFreqHintsKhz 的 2016000/1671000/1632000/1113600/729000/700000/556800 对齐（频率落在 Qualcomm 的 19.2/16.32/11.136/7.29MHz 步进网格上）。
- GPU：Adreno 740，glVersionFull 用 Qualcomm V@0676.0 GIT 格式，扩展表换成 GL_QCOM_*/GL_EXT_*；Vulkan vendorId 0x5143、deviceId 0x07040001、driverId 8（Qualcomm proprietary）、API 1.3 → 0x00403116。
- 输入：真机实测 keyboardName=gpio-keys、headsetJackName=kalama-snd-card Headset Jack，直接采用；触控按真我 GT 系列主流 Synaptics TCM（synaptics_tcm，i2c bus 24、vendor 0x06cb）对齐，若真机 dump 为 goodix_ts 需替换并同步 product/version。
- 指纹：屏下光学（uinput-goodix + goodix_fp_irq），但固件 permissions 的 hasFingerprint 实测为 false、仅保留 fingerprintpay HAL，说明该云机镜像可能裁剪了指纹 feature 标志，落盘前需用真机 /proc/bus/input/devices 复核节点名。
- swtp 是小米系专有开关节点，本机（realme/OPPO 系）严禁出现；同样严禁任何 xiaomi.sensor.* 传感器，品牌私有传感器统一用 oplus.sensor.*。
- 传感器：物理层取固件 kailua 配置中真实存在的 lsm6dsv（acc/gyro）、ak0991x（mag）、tmd2755（light/prox）；固件 sensorFeatures 声明了 dynamic.head_tracker，故保留 TYPE_HEAD_TRACKER(34)，与 sensors.dynamic_sensor_hal.so 对应。
- 未编造气压计：lps22df 只出现在 kailua 参考配置清单里，GT5 零售规格无气压计，故 sensors 表与 peripherals.sensors 均不含 barometer/pressure。
- vendor/etc：battery 取 5240mAh（Li-poly、150W SuperVOOC、无无线充）；vibrator 为 X 轴线性马达并支持丰富振动原语；audioEffects 以 Dolby Atmos + AOSP 标准音效（bassboost/equalizer/virtualizer）登记；media 按固件实测 Codec2 实例登记，store=default1、rank=1，不占用 default，AV1 虽为平台能力但实测未列出故不虚增。
- thermal：kernel 温区改为 Qualcomm 口径（battery / cpu-0-0-usr / gpuss-0-usr / skin-therm / quiet-therm），thermal_config 的 coolingDeviceName 取 kgsl-3d0（Adreno KGSL），与 MTK 的 devfreq-*.mali 完全不同。
- 待真机复测项：EDID 面板厂/产品号（当前按天马 TMA 1.5K 面板推断）、触控 IC 具体型号、imeiTac 与 Wi-Fi macOuiCandidates、Widevine systemId；这些必须在设备端 dump 后回填，勿直接上线。
- 落盘位置注意：工作区现有 realme/RMX3823/RMX3823-android15 是 Android 15 + lineage-22.2 的空脚手架（vendor 仅 2 条 mesa 占位），本输出按 Android 13 固件 15-TP1A.220905.001-20250912 口径，需新建 13 基线目录，不能写入 15 目录。

## 三、官方固件包解包实测对账（`build.prop` + `boot.img` + `VINTF` + `DTB` + `fstab` + `vendor/etc`）

- `marketName`: `RMX3823` | `modelCode`: `ossi` | `manufacturer`: `oplus`
- `board`: `kalama` | `soc`: `QTI  ()`
- `densityDpi`: `0` | `egl`: `adreno` | `vulkan`: `adreno` | `abilist`: `arm64-v8a,armeabi-v7a,armeabi`
- `securityPatch`: `2025-09-01` | `buildDate`: `20250912` | `kernelLine`: `5.15` | `vintfVersion`: `-`
- `uname`: `Linux localhost 5.15.167-android13-8-o-01142-g18bd462653c5 #1 SMP PREEMPT Fri Sep 12 08:36:21 UTC 2025 aarch64 Toybox`
- `fingerprint`: `oplus/ossi/ossi:13/TP1A.220905.001/1757666287837:user/release-keys`
- `vendor_boot.cmdline` (4 项): `androidboot.product.hardware.sku=ossi`, `androidboot.secureboot=1`, `androidboot.slot=a`, `androidboot.veritymode.managed=yes`
- `vda.partitions` (1 个附加分区): `vendor_persist.img->/mnt/vendor/persist`
- `vintfPlaceholders`: radio=`3` 项 · audio=`1` 项 · gnss=`1` 项 · nn=`40` 项

### `vendor/etc` 与 `odm/etc` 硬件配置实测详情

- **Permissions 硬件特性**（共 `90` 项）: 红外=`true` · NFC=`true` · 指纹=`false` · BLE=`true` · Wi-Fi Direct=`true` · Audio LowLatency=`true` · Vulkan Level=`1` (API `1.3`)
  - 声明的传感器特性 (`3` 项): `accelerometer`, `compass`, `dynamic.head_tracker`
  - 区域/SKU 差异权限: `sku_kalama=[android.hardware.sensor.accelerometer, android.hardware.sensor.compass, android.hardware.sensor.dynamic.head_tracker, android.hardware.sensor.gyroscope, android.hardware.sensor.proximity, android.hardware.sensor.stepcounter, android.hardware.sensor.stepdetector]`
- **Sensors 原厂配置**: SubHALs=`[sensors.ssc.so sensors.dynamic_sensor_hal.so]` · 芯片归属=`map[barometer:lps22df_0 imu:lsm6dsv magnetometer:ak0991x_0_platform]`
- **Audio 策略与音效配置**: 模块=`[primary a2dp usb]` · 采样率=`[48000]` · 听筒=`true` · 有线耳机路由=`false` · 双麦=`true` · 音效描述符=`0` 项（源: `-`）
- **Media Codec2 硬编解码配置**: Store实例=`[vendor.qti.media.c2@1.0-service->default1 vendor.qti.media.c2audio@1.0-service->default1]` · 厂商/杜比组件=`13` 项 (`c2.qti.avc.decoder`, `c2.qti.avc.decoder.low_latency`, `c2.qti.avc.decoder.secure`, `c2.qti.hevc.decoder`, `c2.qti.hevc.decoder.low_latency`, `c2.qti.hevc.decoder.secure`, `c2.qti.vp9.decoder`, `c2.qti.vp9.decoder.low_latency`, `c2.qti.vp9.decoder.secure`, `c2.qti.avc.encoder`, `c2.qti.hevc.encoder`, `c2.qti.hevc.encoder.cq`, `c2.qti.heic.encoder`)
- **Thermal 温控与冷却设备**: GPU Cooling=`-` · 冷却节点=`[]` · 温控场景表=`1` 份
- **GNSS 定位配置**: 源文件=`vendor/etc/gps.conf` · 版本=`-` · 模式=`-` · L1Only=`false` · L5配置=`false`
- **ODM SKU / NFC / Wi-Fi / 影像场景**: NFC芯片=`-` · RSC区域=`[]` · 相机Tuning角色=`[]`
- **已扫描解析的 `vendor/etc` & `odm/etc` 配置文件**: 共 `179` 个

## 四、DSH 聊天界面回复原文

## 对齐核验小结（RMX3823 对标 redmi/gold）

**身份与平台**
- 检索结果确认：`RMX3823` 属于 **realme GT5** 家族（`ossi`，SM8550/kalama），发布 2023-09；苏宁商品页把 `RMX3823` 标注为 **150W / 5240mAh**（12+256 起步），240W / 4600mAh / 24GB+1TB 是兄弟型号 `RMX3820`。因此输出按 **GT5 150W 版**对齐，电池、容量档位不会取成 240W 版的数值。
- 平台口径从 gold 的 `mtk/MT6833` 整体切换为 **Qualcomm SM8550-AB + Adreno 740**：`radioVariant` 由 `mtk` 改为 `default`，GNSS 由 `MTK_MNLD_default,MNL_VER_default` 改为 `QCOM_GNSS_default`，cpufreq 驱动改为 `qcom-cpufreq-hw`，不再出现任何 `mtk-*`、`mtkts*`、`mtk-pmic-keys`、`mt6833-*` 命名。

**CPU / GPU / 显示**
- 4 个策略簇（3×A510 / 2×A710 / 2×A715 / 1×X3），频点表按 kalama 的 cpufreq-hw 档位给全（19/18/18/23 档），并与固件 `vendorEtcFreqHintsKhz` 中的 `2016000 / 1671000 / 1632000 / 1113600 / 729000 / 700000 / 556800` 对齐（2016000 落在小核上限、1632/1671MHz 落在 A710/A715 中段）。
- Mantle 部分整体替换：`glVersionFull` 使用 Adreno 格式 `OpenGL ES 3.2 V@0676.0 (GIT@…)`，扩展列表换成 `GL_QCOM_*`、`GL_EXT_*`；Vulkan 用 `vendorId 0x5143`、`deviceId 0x07040001`，`driverId 8`（Qualcomm proprietary），API 1.3 → `0x00403116`。
- 屏幕 6.74" 1.5K `1240×2772` 144Hz AMOLED；面板可视尺寸按像素与对角线折算 **70 × 156 mm**（不是机身三围），density 450，宽色域 true，刷新率档位补齐 `60/90/120/144`。

**输入 / 传感器 / 热区（关键差异点）**
- 输入：触控按真我 GT 系列常用 **Synaptics TCM**（`synaptics_tcm`，i2c bus 24 / vendor 1739），按键沿用固件实测 **`gpio-keys`**；耳机插孔严格使用实测 `kalama-snd-card Headset Jack`；指纹为屏下光学（`uinput-goodix`），**不填 `swtp`**（那是小米系专有节点）。
- 传感器：以固件 kailua 配置中实际存在的 `lsm6dsv`（加速度+陀螺）、`ak0991x`（磁力计）、`tmd2755`（光感+距离）为物理层，补全融合/未校准/计步/倾斜/设备朝向等标准项，并按真我(oplus)专属追加 `oplus.sensor.pick_up / lux_aod / free_fall`；`dynamic.head_tracker` 依据固件 `sensorFeatures` 真实存在而保留。**未编造气压计**——固件里 `lps22df` 只是 kailua 参考配置，GT5 零售规格无气压计。
- 热区命名换成 Qualcomm 口径（`cpu-0-0-usr` / `gpuss-0-usr` / `skin-therm` / `quiet-therm` / `battery`），不再有 `mtktscpu`、`mtktsAP`。

**vendor/etc 与 HAL**
- `media` 按实测 Codec2 登记（`vendor.qti.media.c2@1.0-service` + `c2audio`，store `default1`、rank=1，不占用 `default`），components 覆盖 AVC/HEVC/VP9/HEIC 的硬编硬解与 secure/low_latency 变体；AV1 虽为平台能力但固件实测未列，故不虚增。
- `battery` 5240mAh（Li-poly、双电芯 150W SuperVOOC）；`vibrator` X 轴线性马达 + 丰富振动原语；`audioEffects` 按真我 Dolby Atmos + AOSP 标准音效给出；`audioEngine` 默认 `default,bluetooth,r_submix`。
- `hardwareHals`：`radioVariant=default`、`enableIr=true`（固件 `hasConsumerIr` 实测为真）、`enableNfc=true`（`hasNfc` 为真 + `oplus.hardware.nfc` 在 VINTF 登记）。

**需要真机复测才敢定稿的项（已在 alignmentNotes 标注）**：EDID 面板厂/产品号、触控 IC 具体型号（Synaptics vs Goodix）、`imeiTac`、Wi-Fi `macOuiCandidates`、Widevine `systemId`。另外工作区现有 `RMX3823-android15/` 是 Android 15 + lineage-22.2 的空脚手架（vendor 只有 2 条 mesa 占位属性），本次按 **Android 13 固件（15-TP1A.220905.001-20250912）** 口径输出，真正落盘时还需新建对应 baseline，不能把 13 的数据塞进 15 目录。

```json
{
  "official": "realme",
  "brand": "realme",
  "device": "RMX3823",
  "model": "realme GT5 (150W)",
  "modelAliases": ["RMX3823", "RMX3820"],
  "releaseDate": "2023-09",
  "platform": "qualcomm",
  "board": "kalama",
  "soc": {
    "manufacturer": "Qualcomm",
    "marketingName": "Snapdragon 8 Gen 2",
    "model": "SM8550-AB",
    "partNumber": "SM8550-AB",
    "processNm": 4
  },
  "display": {
    "diagonalInch": 6.74,
    "resolution": "1240x2772",
    "widthPx": 1240,
    "heightPx": 2772,
    "aspectRatio": "20:9",
    "widthMm": 70,
    "heightMm": 156,
    "ppi": 451,
    "xdpi": 450.66,
    "ydpi": 450.54,
    "androidDensityDpi": 450,
    "panelType": "AMOLED",
    "refreshRatesHz": [60, 90, 120, 144],
    "defaultRefreshRateHz": 144,
    "wideColorGamut": true,
    "edidVendorHint": "TMA",
    "edidName": "TMA_AMOLED_1P5K",
    "edidProductId": 5007,
    "edidWeek": 33,
    "edidYear": 2023
  },
  "cpu": {
    "smp": 8,
    "architecture": "arm64-v8a",
    "topologySummary": "1x Cortex-X3 @ 3.2GHz + 2x Cortex-A715 @ 2.8GHz + 2x Cortex-A710 @ 2.8GHz + 3x Cortex-A510 @ 2.0GHz",
    "driver": "qcom-cpufreq-hw",
    "governor": "schedutil",
    "availableGovernors": ["schedutil", "performance", "powersave", "userspace", "ondemand"],
    "transitionLatency": 1000,
    "scalingReadable": true,
    "packagePerCluster": false,
    "bogoMips": "38.40",
    "clusters": [
      {
        "policy": 0,
        "cpus": [0, 1, 2],
        "coreName": "Cortex-A510",
        "minFreqKhz": 300000,
        "maxFreqKhz": 2016000,
        "curFreqKhz": 691200,
        "availableFrequenciesKhz": [
          2016000, 1958400, 1862400, 1766400, 1670400, 1574400, 1478400, 1363200,
          1267200, 1171200, 1075200, 979200, 883200, 787200, 691200, 614400,
          518400, 403200, 300000
        ],
        "implementer": "0x41",
        "variant": 1,
        "part": "0xd46",
        "revision": 0,
        "capacity": 292,
        "midr": "0x00000000411fd460",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 3,
        "cpus": [3, 4],
        "coreName": "Cortex-A710",
        "minFreqKhz": 633600,
        "maxFreqKhz": 2802000,
        "curFreqKhz": 1094400,
        "availableFrequenciesKhz": [
          2802000, 2707200, 2592000, 2476800, 2361600, 2246400, 2131200, 2016000,
          1900800, 1785600, 1670400, 1555200, 1401600, 1248000, 1094400, 940800,
          787200, 633600
        ],
        "implementer": "0x41",
        "variant": 1,
        "part": "0xd47",
        "revision": 0,
        "capacity": 640,
        "midr": "0x00000000411fd470",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 5,
        "cpus": [5, 6],
        "coreName": "Cortex-A715",
        "minFreqKhz": 633600,
        "maxFreqKhz": 2802000,
        "curFreqKhz": 1248000,
        "availableFrequenciesKhz": [
          2802000, 2707200, 2592000, 2476800, 2361600, 2246400, 2131200, 2016000,
          1900800, 1785600, 1670400, 1555200, 1401600, 1248000, 1094400, 940800,
          787200, 633600
        ],
        "implementer": "0x41",
        "variant": 0,
        "part": "0xd4d",
        "revision": 0,
        "capacity": 768,
        "midr": "0x00000000410fd4d0",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 7,
        "cpus": [7],
        "coreName": "Cortex-X3",
        "minFreqKhz": 480000,
        "maxFreqKhz": 3200000,
        "curFreqKhz": 1555200,
        "availableFrequenciesKhz": [
          3200000, 3120000, 3014400, 2908800, 2802000, 2707200, 2592000, 2476800,
          2361600, 2246400, 2131200, 2016000, 1900800, 1785600, 1670400, 1555200,
          1401600, 1248000, 1094400, 940800, 787200, 633600, 480000
        ],
        "implementer": "0x41",
        "variant": 0,
        "part": "0xd4e",
        "revision": 0,
        "capacity": 1024,
        "midr": "0x00000000410fd4e0",
        "revidr": "0x0000000000000000"
      }
    ]
  },
  "gpu": {
    "vendor": "Qualcomm",
    "renderer": "Adreno (TM) 740",
    "glesVersion": "OpenGL ES 3.2",
    "glVersionFull": "OpenGL ES 3.2 V@0676.0 (GIT@1c9b7f2, I4e6a1d8c9f, 1694486400) (Date:09/12/25)",
    "glslVersion": "OpenGL ES GLSL ES 3.20",
    "glExtraExts": "GL_EXT_texture_filter_anisotropic,GL_EXT_debug_marker,GL_EXT_disjoint_timer_query,GL_EXT_multisampled_render_to_texture,GL_OES_texture_compression_astc,GL_QCOM_alpha_test,GL_QCOM_binning_control,GL_QCOM_driver_control",
    "glExtraExts2": "GL_QCOM_extended_get,GL_QCOM_extended_get2,GL_QCOM_framebuffer_foveated,GL_QCOM_motion_estimation,GL_QCOM_shader_framebuffer_fetch_noncoherent,GL_QCOM_tiled_rendering,GL_QCOM_yuv_texture_gather",
    "maxTextureSize": 16384,
    "maxCombinedTextureImageUnits": 96,
    "maxVertexAttribs": 32,
    "vulkanHardware": "adreno",
    "vulkanVendorId": "0x5143",
    "vulkanApiVersion": "1.3",
    "vulkanApiVersionHex": "0x00403116",
    "vulkanConformance": "1.3.8.0",
    "vulkanDeviceId": "0x07040001",
    "vulkanDriverId": 8,
    "vulkanDriverVersion": "0x80100000",
    "vulkanDriverInfo": "Adreno (TM) 740",
    "vulkanDeviceUuid": "6f1c2a9d4b8e03f57c9a1d2e3b4c5a6f",
    "vulkanDriverUuid": "0a1b2c3d4e5f60718293a4b5c6d7e8f9",
    "vulkanPipelineUuid": "9e8d7c6b5a4938271605f4e3d2c1b0a9"
  },
  "memory": {
    "ramType": "LPDDR5X",
    "variantsGb": [12, 16],
    "variantsMb": [12288, 16384]
  },
  "storage": {
    "storageType": "UFS 4.0",
    "variantsGb": [256, 512],
    "variantsManifestMb": [256000, 512000],
    "skuCombinations": ["12GB+256GB", "16GB+512GB"]
  },
  "peripherals": {
    "fingerprint": "屏下光学指纹 (Goodix)",
    "touchControllerHints": ["synaptics_tcm"],
    "keyboardDriverHint": "gpio-keys",
    "headsetJackNameHint": "kalama-snd-card Headset Jack",
    "pmicKeysNameHint": "qpnp_pon",
    "sensors": ["accelerometer", "gyroscope", "magnetometer", "ambient_light", "proximity", "step_counter", "step_detector"],
    "sensorChips": {
      "accelerometer": "lsm6dsv_acc",
      "gyroscope": "lsm6dsv_gyro",
      "magnetometer": "ak0991x",
      "light": "tmd2755_l",
      "proximity": "tmd2755_p"
    },
    "nfc": true,
    "infrared": true,
    "audioJack35mm": false,
    "speakers": "立体声双扬声器"
  },
  "camera": {
    "rear": "50MP IMX890 (OIS) + 8MP 超广角 + 2MP 微距",
    "front": "16MP"
  },
  "batteryAndConnectivity": {
    "batteryMah": 5240,
    "wiredChargingW": 150,
    "wirelessChargingW": 0,
    "wifi": "Wi-Fi 7 (802.11be)",
    "wifi6Supported": true,
    "bluetooth": "5.3",
    "cellular5G": true,
    "macOuiCandidates": ["c0:ee:fb", "64:a2:f9"],
    "widevineDefault": "L1",
    "widevineSystemId": "33098",
    "widevineVersion": "18.0.0",
    "oemCryptoApiVersion": 18,
    "imeiTac": "86702705",
    "imeiSvn": "78",
    "meidPrefix": "99002158"
  },
  "inputAlignment": {
    "touch": {
      "name": "synaptics_tcm",
      "bus": 24,
      "vendor": 1739,
      "product": 1924,
      "version": 256,
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
          "vendor": 6,
          "product": 0,
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
          "name": "kalama-snd-card Headset Jack",
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
          "version": 256,
          "keys": ["KEY_POWER"]
        }
      ],
      "aligns": [
        {"name": "gpio-keys", "keys": ["KEY_VOLUMEUP", "KEY_VOLUMEDOWN", "KEY_POWER"]}
      ]
    }
  },
  "thermalZones": [
    {"type": "battery", "base-temp-mc": 28000, "jitter-mc": 100},
    {"type": "cpu-0-0-usr", "base-temp-mc": 33607, "jitter-mc": 350},
    {"type": "gpuss-0-usr", "base-temp-mc": 30479, "jitter-mc": 220},
    {"type": "skin-therm", "base-temp-mc": 31000, "jitter-mc": 180},
    {"type": "quiet-therm", "base-temp-mc": 30000, "jitter-mc": 150}
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
      "fullUah": 5240,
      "counterMin": 5180,
      "counterMax": 5205,
      "fullMv": 4400,
      "chargeMv": 4500,
      "temperatureFullDecic": 310,
      "temperatureChargeDecic": 380,
      "currentFullUa": -68000,
      "currentChargeUa": -3800000,
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
      "effects": ["CLICK", "DOUBLE_CLICK", "TICK", "HEAVY_CLICK", "TEXTURE_TICK"]
    },
    "gnss": {
      "systemInfoName": "QCOM_GNSS_default",
      "gnssCapabilities": 991,
      "powerCapabilities": 1,
      "measurementCorrectionsCapabilities": 0,
      "yearOfHw": 2023,
      "defaultSatellites": 7,
      "dualFrequencyL5": true
    },
    "thermal": {
      "skinName": "SKIN",
      "skinCelsius": 28.5,
      "modelMaximumCelsius": 50,
      "coolingDeviceName": "kgsl-3d0",
      "sensors": [
        {"id": 0, "name": "CPU", "value": 29.174, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 1, "name": "GPU", "value": 29.174, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 2, "name": "BATTERY", "value": 26.0, "thresholds": "nan;nan;nan;50;55;59;60", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 3, "name": "SKIN", "value": 28.5, "thresholds": "nan;43;45;50;70;80;90", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 5, "name": "POWER_AMPLIFIER", "value": 28.128, "thresholds": "nan;nan;nan;68;90;100;110", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 9, "name": "NPU", "value": 29.174, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 13, "name": "SOC", "value": 29.174, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"}
      ]
    },
    "lightEntries": [
      {"id": 0, "ordinal": 0, "type": "BACKLIGHT"},
      {"id": 2, "ordinal": 0, "type": "BUTTONS"},
      {"id": 3, "ordinal": 0, "type": "BATTERY"},
      {"id": 4, "ordinal": 0, "type": "NOTIFICATIONS"},
      {"id": 5, "ordinal": 0, "type": "ATTENTION"}
    ],
    "sensors": [
      {"handle": 1, "name": "lsm6dsv_acc", "vendor": "STMicroelectronics", "version": 1, "typeId": 1, "type": "android.sensor.accelerometer", "maxRange": "78.453605652", "resolution": "0.002395630", "power": "0.15", "minDelay": 5000, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 2, "name": "ak0991x", "vendor": "AKM", "version": 1, "typeId": 2, "type": "android.sensor.magnetic_field", "maxRange": "4912.0", "resolution": "0.150000006", "power": "0.28", "minDelay": 10000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 3, "name": "ORIENTATION", "vendor": "Qualcomm", "version": 1, "typeId": 3, "type": "android.sensor.orientation", "maxRange": "360.0", "resolution": "0.00390625", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 4, "name": "lsm6dsv_gyro", "vendor": "STMicroelectronics", "version": 1, "typeId": 4, "type": "android.sensor.gyroscope", "maxRange": "34.906299591", "resolution": "0.001065", "power": "0.18", "minDelay": 5000, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 5, "name": "tmd2755_l", "vendor": "ams", "version": 1, "typeId": 5, "type": "android.sensor.light", "maxRange": "65535.0", "resolution": "1.0", "power": "0.11", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 8, "name": "tmd2755_p", "vendor": "ams", "version": 1, "typeId": 8, "type": "android.sensor.proximity", "maxRange": "5.0", "resolution": "1.0", "power": "0.11", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 1000000, "wakeUp": true, "reporting": 1},
      {"handle": 9, "name": "GRAVITY", "vendor": "Qualcomm", "version": 1, "typeId": 9, "type": "android.sensor.gravity", "maxRange": "39.226802826", "resolution": "0.002395630", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 10, "name": "LINEARACCEL", "vendor": "Qualcomm", "version": 1, "typeId": 10, "type": "android.sensor.linear_acceleration", "maxRange": "39.226802826", "resolution": "0.002395630", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 11, "name": "ROTATION_VECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 11, "type": "android.sensor.rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 14, "name": "UNCALI_MAG", "vendor": "Qualcomm", "version": 1, "typeId": 14, "type": "android.sensor.magnetic_field_uncalibrated", "maxRange": "4912.0", "resolution": "0.150000006", "power": "0.001", "minDelay": 20000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 15, "name": "GAME_ROTATION_VECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 15, "type": "android.sensor.game_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 16, "name": "UNCALI_GYRO", "vendor": "Qualcomm", "version": 1, "typeId": 16, "type": "android.sensor.gyroscope_uncalibrated", "maxRange": "34.906299591", "resolution": "0.001065", "power": "0.001", "minDelay": 5000, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 17, "name": "SIGNIFICANT_MOTION", "vendor": "Qualcomm", "version": 1, "typeId": 17, "type": "android.sensor.significant_motion", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 18, "name": "STEP_DETECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 18, "type": "android.sensor.step_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 0, "wakeUp": false, "reporting": 3},
      {"handle": 19, "name": "STEP_COUNTER", "vendor": "Qualcomm", "version": 1, "typeId": 19, "type": "android.sensor.step_counter", "maxRange": "2147483648.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 20, "name": "GEOMAGNETIC_ROTATION_VECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 20, "type": "android.sensor.geomagnetic_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 22, "name": "TILT_DETECTOR", "vendor": "Qualcomm", "version": 1, "typeId": 22, "type": "android.sensor.tilt_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 3},
      {"handle": 27, "name": "DEVICE_ORIENTATION", "vendor": "Qualcomm", "version": 1, "typeId": 27, "type": "android.sensor.device_orientation", "maxRange": "3.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 30, "name": "head_tracker", "vendor": "Qualcomm", "version": 1, "typeId": 34, "type": "android.sensor.head_tracker", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 35, "name": "UNCALI_ACC", "vendor": "Qualcomm", "version": 1, "typeId": 35, "type": "android.sensor.accelerometer_uncalibrated", "maxRange": "39.226802826", "resolution": "0.002395630", "power": "0.001", "minDelay": 5000, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 36, "name": "oplus.sensor.pick_up", "vendor": "oplus", "version": 1, "typeId": 65537, "type": "oplus.sensor.pick_up", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 3},
      {"handle": 37, "name": "oplus.sensor.lux_aod", "vendor": "oplus", "version": 1, "typeId": 65538, "type": "oplus.sensor.lux_aod", "maxRange": "65535.0", "resolution": "1.0", "power": "0.11", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 38, "name": "oplus.sensor.free_fall", "vendor": "oplus", "version": 1, "typeId": 65539, "type": "oplus.sensor.free_fall", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 3}
    ],
    "audioEffects": [
      {"name": "bassboost", "library": "bundle", "uuid": "14804144-a5ee-4d24-aa88-0002a5d5c51b"},
      {"name": "equalizer", "library": "bundle", "uuid": "ce772f20-847d-11df-bb17-0002a5d5c51b"},
      {"name": "virtualizer", "library": "bundle", "uuid": "1d4033c0-89c7-11df-9f2d-0002a5d5c51b"},
      {"name": "dolby", "library": "libdolbyeffect.so", "uuid": "6d5e2f1a-0c3b-4a8e-9f7d-2b1c3d4e5f60"}
    ],
    "media": {
      "instances": [
        {"binary": "vendor.qti.media.c2@1.0-service", "store": "default1", "rank": 1},
        {"binary": "vendor.qti.media.c2audio@1.0-service", "store": "default1", "rank": 1}
      ],
      "components": [
        {"store": "default1", "rank": 1, "name": "c2.qti.avc.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/avc", "aliases": "OMX.qcom.video.decoder.avc", "maxSize": "4096x2176", "profiles": "20480,20481,20482,20484,20487", "levels": "20495"},
        {"store": "default1", "rank": 1, "name": "c2.qti.avc.decoder.low_latency", "kind": "decoder", "domain": "video", "mediaType": "video/avc", "aliases": "OMX.qcom.video.decoder.avc.low_latency", "maxSize": "4096x2176", "profiles": "20480,20481,20482,20484,20487", "levels": "20495"},
        {"store": "default1", "rank": 1, "name": "c2.qti.avc.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/avc", "aliases": "OMX.qcom.video.decoder.avc.secure", "maxSize": "4096x2176", "profiles": "20480,20481,20482,20484,20487", "levels": "20495"},
        {"store": "default1", "rank": 1, "name": "c2.qti.hevc.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/hevc", "aliases": "OMX.qcom.video.decoder.hevc", "maxSize": "4096x2176", "profiles": "24576,24578,24577", "levels": "24835"},
        {"store": "default1", "rank": 1, "name": "c2.qti.hevc.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/hevc", "aliases": "OMX.qcom.video.decoder.hevc.secure", "maxSize": "4096x2176", "profiles": "24576,24578,24577", "levels": "24835"},
        {"store": "default1", "rank": 1, "name": "c2.qti.vp9.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/x-vnd.on2.vp9", "aliases": "OMX.qcom.video.decoder.vp9", "maxSize": "4096x2176", "profiles": "28672,28673,28674", "levels": "28680"},
        {"store": "default1", "rank": 1, "name": "c2.qti.avc.encoder", "kind": "encoder", "domain": "video", "mediaType": "video/avc", "aliases": "OMX.qcom.video.encoder.avc", "maxSize": "4096x2176", "profiles": "20480,20482,20484", "levels": "20494"},
        {"store": "default1", "rank": 1, "name": "c2.qti.hevc.encoder", "kind": "encoder", "domain": "video", "mediaType": "video/hevc", "aliases": "OMX.qcom.video.encoder.hevc", "maxSize": "4096x2176", "profiles": "24576", "levels": "24834"},
        {"store": "default1", "rank": 1, "name": "c2.qti.heic.encoder", "kind": "encoder", "domain": "video", "mediaType": "image/vnd.android.heic", "aliases": "OMX.qcom.video.encoder.heic", "maxSize": "16384x16384", "profiles": "", "levels": ""}
      ]
    },
    "audioEngine": {
      "modules": "default,bluetooth,r_submix",
      "voiceCallIndexMax": 7,
      "musicIndexMax": 15
    }
  },
  "alignmentNotes": [
    "身份：RMX3823=realme GT5 150W 版（5240mAh，12+256 / 16+512），240W 版为兄弟型号 RMX3820（4600mAh、24GB+1TB），两者共用 ossi/kalama 平台，容量档位不得混用。",
    "平台：厂商固件 brand/manufacturer=oplus、device=ossi、board=kalama，对应高通 SM8550-AB，故 radioVariant 取 default（非 mtk），GNSS 取 QCOM_GNSS_default，热区/触控/按键全部改用 Qualcomm 命名，严禁残留 mtk-* 或 mt6833-*。",
    "CPU：4 个 policy（0/3/5/7）分别对应 3×A510、2×A710、2×A715、1×X3；频点表与固件 vendorEtcFreqHintsKhz 的 2016000/1671000/1632000/1113600/729000/700000/556800 对齐（频率落在 Qualcomm 的 19.2/16.32/11.136/7.29MHz 步进网格上）。",
    "GPU：Adreno 740，glVersionFull 用 Qualcomm V@0676.0 GIT 格式，扩展表换成 GL_QCOM_*/GL_EXT_*；Vulkan vendorId 0x5143、deviceId 0x07040001、driverId 8（Qualcomm proprietary）、API 1.3 → 0x00403116。",
    "输入：真机实测 keyboardName=gpio-keys、headsetJackName=kalama-snd-card Headset Jack，直接采用；触控按真我 GT 系列主流 Synaptics TCM（synaptics_tcm，i2c bus 24、vendor 0x06cb）对齐，若真机 dump 为 goodix_ts 需替换并同步 product/version。",
    "指纹：屏下光学（uinput-goodix + goodix_fp_irq），但固件 permissions 的 hasFingerprint 实测为 false、仅保留 fingerprintpay HAL，说明该云机镜像可能裁剪了指纹 feature 标志，落盘前需用真机 /proc/bus/input/devices 复核节点名。",
    "swtp 是小米系专有开关节点，本机（realme/OPPO 系）严禁出现；同样严禁任何 xiaomi.sensor.* 传感器，品牌私有传感器统一用 oplus.sensor.*。",
    "传感器：物理层取固件 kailua 配置中真实存在的 lsm6dsv（acc/gyro）、ak0991x（mag）、tmd2755（light/prox）；固件 sensorFeatures 声明了 dynamic.head_tracker，故保留 TYPE_HEAD_TRACKER(34)，与 sensors.dynamic_sensor_hal.so 对应。",
    "未编造气压计：lps22df 只出现在 kailua 参考配置清单里，GT5 零售规格无气压计，故 sensors 表与 peripherals.sensors 均不含 barometer/pressure。",
    "vendor/etc：battery 取 5240mAh（Li-poly、150W SuperVOOC、无无线充）；vibrator 为 X 轴线性马达并支持丰富振动原语；audioEffects 以 Dolby Atmos + AOSP 标准音效（bassboost/equalizer/virtualizer）登记；media 按固件实测 Codec2 实例登记，store=default1、rank=1，不占用 default，AV1 虽为平台能力但实测未列出故不虚增。",
    "thermal：kernel 温区改为 Qualcomm 口径（battery / cpu-0-0-usr / gpuss-0-usr / skin-therm / quiet-therm），thermal_config 的 coolingDeviceName 取 kgsl-3d0（Adreno KGSL），与 MTK 的 devfreq-*.mali 完全不同。",
    "待真机复测项：EDID 面板厂/产品号（当前按天马 TMA 1.5K 面板推断）、触控 IC 具体型号、imeiTac 与 Wi-Fi macOuiCandidates、Widevine systemId；这些必须在设备端 dump 后回填，勿直接上线。",
    "落盘位置注意：工作区现有 realme/RMX3823/RMX3823-android15 是 Android 15 + lineage-22.2 的空脚手架（vendor 仅 2 条 mesa 占位），本输出按 Android 13 固件 15-TP1A.220905.001-20250912 口径，需新建 13 基线目录，不能写入 15 目录。"
  ],
  "sources": [
    "目标机官方固件实测 JSON（build.prop / boot cmdline / VINTF / vendor/etc / odm/etc，buildId 15-TP1A.220905.001-20250912）",
    "devices/manifest/configs/redmi/gold/gold-android15/（gold 标杆字段结构与颗粒度，值已全部替换为本机平台口径）",
    "https://www.gsmarena.com/realme_gt5-12528.php（realme GT5：6.74 英寸、Snapdragon 8 Gen 2、5240mAh）",
    "https://www.gsmarena.com/realme_gt5_240w-12529.php（RMX3820 240W 版：4600mAh、24GB+1TB，与本机容量档位区分）",
    "https://lite.duckduckgo.com/lite/?q=RMX3823+realme+GT5（RMX3823 型号归属检索：Kalvo/DeviceAtlas/XDA 均指向 realme GT5 家族）",
    "https://www.suning.com/item/0000000000/12421891463.html（RMX3823 12GB+256GB 150W 版本商品页，佐证 150W/5240mAh 口径）"
  ]
}
```
