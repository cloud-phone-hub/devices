# 真机硬件规格与云机对齐留档 · Samsung Galaxy S25 Ultra (samsung/pa3q)

- **生成时间**: `2026-11-18T21:36:21+08:00`
- **ROM Build ID**: `16-AP3A.240905.015.A2-20260812`
- **DSH 聊天会话 ID**: `session-1881071f-cfcc-48f5-8d87-1402bb75e606`（标题：`[02_prepare] 真机规格采集 · Samsung Galaxy S25 Ultra (samsung/pa3q · 16-AP3A.240905.015.A2-20260812)`）
- **采集模型路由**: `cdh-maas / deepseek-v4.1-flash-1017`（复用在线 DSH Web: `true`）

---

## 一、设备表对齐规格（对应 `devices/manifest/devices/samsung/pa3q.json`）

| 字段 | 对齐值 | 说明 |
| --- | --- | --- |
| `official` | `三星` | 厂商集团 |
| `brand` | `samsung` | 子品牌标识 |
| `model` | `Samsung Galaxy S25 Ultra` | 商业上市机型名 （型号代码: `SM-S9380`, `SM-S938B`, `SM-S938U`, `SM-S938W`, `SM-S938N`, `SM-S938E`） |
| `device` | `pa3q` | 设备代号 |
| `platform` / `board` | `qcom` / `sun` | 芯片平台 / 主板代号 |
| `resolution` | `1440x3120` | 屏幕物理像素分辨率 |
| `inch` | `74x159` | 屏幕可视物理宽×高 mm（对角线 `6.90"`） |
| `density` | `450` | Android 逻辑密度 dpi（物理 PPI `498`，xdpi `497.87` / ydpi `497.75`） |
| `smp` | `8` | CPU 核心总数 |
| `memory` | `[12288 16384]` | 全系发售内存档位 MB（12G / 16G，LPDDR5X） |
| `storage` | `[256000 512000 1024000]` | 全系发售存储档位（256G / 512G / 1T，UFS 4.0） |

- **常见量产发售版本组合**: `12GB+256GB` / `12GB+512GB` / `12GB+1TB` / `16GB+1TB`

## 二、真机深层硬件对齐画像（参考 `redmi/gold` 标准）

### 1. SoC 与 CPU 簇全量频点表（`cdh_cpufreq` / `module/config.json` 对齐）
- **SoC 芯片**: Qualcomm Snapdragon 8 Elite for Galaxy (`SM8750-AC` SM8750-AC, 3nm)
- **CPU 拓扑**: `1x Oryon V2 Phoenix L @ 4.2816GHz(固件实测上限) + 4x Oryon V2 Phoenix M @ 3.5328GHz + 3x Oryon V2 Phoenix M @ 2.1696GHz（固件 cpufreq 三簇 3/4/1）`（驱动: `qcom-cpufreq-hw`，调速器: `schedutil`，可选调速器: `[schedutil performance powersave userspace]`，BogoMIPS: `38.40`）

| 策略簇 | 核心列表 | 微架构 | Part / MIDR | 容量权重 | 最小 / 驻留 / 最大 (kHz) | 频点档数 |
| --- | --- | --- | --- | --- | --- | --- |
| policy0 | `[0 1 2]` | Oryon V2 Phoenix M | `0x001` (`0x00000000510f0010`) | `653` | `700000 / 700000 / 2169600` | `15 档` |
| policy3 | `[3 4 5 6]` | Oryon V2 Phoenix M | `0x001` (`0x00000000510f0010`) | `998` | `700000 / 940800 / 3532800` | `26 档` |
| policy7 | `[7]` | Oryon V2 Phoenix L | `0x001` (`0x00000000510f0010`) | `1024` | `700000 / 1056000 / 4281600` | `28 档` |

- **`policy0` (`Oryon V2 Phoenix M`, cpus `[0 1 2]`) 全量 `available-frequencies` (15 档, kHz)**:
  `[2169600 2054400 1939200 1824000 1708800 1593600 1478400 1363200 1248000 1132800 1017600 940800 864000 787200 700000]`
- **`policy3` (`Oryon V2 Phoenix M`, cpus `[3 4 5 6]`) 全量 `available-frequencies` (26 档, kHz)**:
  `[3532800 3417600 3302400 3187200 3072000 2956800 2841600 2726400 2611200 2496000 2380800 2265600 2169600 2054400 1939200 1824000 1708800 1593600 1478400 1363200 1248000 1132800 1017600 902400 787200 700000]`
- **`policy7` (`Oryon V2 Phoenix L`, cpus `[7]`) 全量 `available-frequencies` (28 档, kHz)**:
  `[4281600 4147200 4012800 3878400 3744000 3609600 3475200 3340800 3206400 3072000 2937600 2803200 2668800 2534400 2400000 2265600 2131200 1996800 1862400 1728000 1593600 1459200 1324800 1190400 1056000 940800 825600 700000]`

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
    "transition-latency": 1000,
    "scaling-readable": false,
    "package-per-cluster": true,
    "bogo-mips": "38.40",
    "policies": [
      {
        "policy": 0,
        "cpus": [
          0,
          1,
          2
        ],
        "min-freq": 700000,
        "max-freq": 2169600,
        "cur-freq": 700000,
        "available-frequencies": [
          2169600,
          2054400,
          1939200,
          1824000,
          1708800,
          1593600,
          1478400,
          1363200,
          1248000,
          1132800,
          1017600,
          940800,
          864000,
          787200,
          700000
        ],
        "implementer": "0x51",
        "variant": 15,
        "part": "0x001",
        "revision": 0,
        "capacity": 653,
        "midr": "0x00000000510f0010",
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
        "min-freq": 700000,
        "max-freq": 3532800,
        "cur-freq": 940800,
        "available-frequencies": [
          3532800,
          3417600,
          3302400,
          3187200,
          3072000,
          2956800,
          2841600,
          2726400,
          2611200,
          2496000,
          2380800,
          2265600,
          2169600,
          2054400,
          1939200,
          1824000,
          1708800,
          1593600,
          1478400,
          1363200,
          1248000,
          1132800,
          1017600,
          902400,
          787200,
          700000
        ],
        "implementer": "0x51",
        "variant": 15,
        "part": "0x001",
        "revision": 0,
        "capacity": 998,
        "midr": "0x00000000510f0010",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 7,
        "cpus": [
          7
        ],
        "min-freq": 700000,
        "max-freq": 4281600,
        "cur-freq": 1056000,
        "available-frequencies": [
          4281600,
          4147200,
          4012800,
          3878400,
          3744000,
          3609600,
          3475200,
          3340800,
          3206400,
          3072000,
          2937600,
          2803200,
          2668800,
          2534400,
          2400000,
          2265600,
          2131200,
          1996800,
          1862400,
          1728000,
          1593600,
          1459200,
          1324800,
          1190400,
          1056000,
          940800,
          825600,
          700000
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
- **面板类型与刷新率**: Dynamic LTPO AMOLED 2X · 刷新率 `[30 60 90 120] Hz`（默认 `120 Hz`） · 广色域 `true` · EDID `SAMSUNG` (`SAMSUNG`, product=`2609`, year=`2025`W`6`)
- **OpenGL ES**: `Qualcomm` / `Adreno 830` / `OpenGL ES 3.2 V@0801.0 (GIT@7f3f5b0, I8a1f3e5d2b, 1731400000) (Date:11/12/24)`
  - GL 驱动扩展: `GL_QCOM_alpha_test,GL_QCOM_binning_control,GL_QCOM_driver_control,GL_QCOM_extended_get,GL_QCOM_extended_get2,GL_QCOM_frame_extrapolation,GL_QCOM_framebuffer_foveated,GL_QCOM_motion_estimation,GL_QCOM_shader_framebuffer_fetch_noncoherent,GL_QCOM_shader_framebuffer_fetch_rate,GL_QCOM_tiled_rendering,GL_QCOM_texture_foveated,GL_QCOM_texture_foveated2,GL_QCOM_texture_foveated_subsampled_layout,GL_QCOM_yuv_texture_gather` GL_EXT_debug_marker,GL_EXT_texture_cube_map_array,GL_EXT_texture_filter_anisotropic,GL_EXT_texture_sRGB_decode,GL_EXT_sRGB_write_control,GL_EXT_color_buffer_half_float,GL_EXT_disjoint_timer_query,GL_EXT_multisampled_render_to_texture2,GL_EXT_render_snorm,GL_EXT_robustness,GL_OES_texture_half_float_linear,GL_KHR_blend_equation_advanced
- **Vulkan**: `ro.hardware.vulkan=adreno` · VendorID `0x5143` · DeviceID `0x08030000` · API `1.3` (`0x00403116`) · DriverID `8` (`0x08010000`)

### 3. 触控、指纹、外设与输入节点对齐（`module/config.json` & `hardware/config.json` 对标 `gold`）
- **指纹方案**: 屏下超声波指纹 (Qualcomm 3D Sonic / QBT)
- **触控与按键对齐**: touch=`sec_touchscreen` · keyboard=`gpio-keys` · input_identity 外设数=`4` 项
- **硬件 HAL 变体与开关**: radioVariant=`default` · NFC=`true` · 红外=`false` · 3.5mm=`false` · 立体声双扬声器 (AKG 调校)
- **影像规格**: 后置 200MP + 50MP 超广角 + 50MP 5x 潜望长焦 + 10MP 3x 长焦 | 前置 12MP
- **电池与通信**: `5000 mAh`（有线 `45W` / 无线 `15W`） · Wi-Fi 7 (802.11be)（Wi-Fi 6=`true`） · BT `5.4` · 5G=`true` · Widevine `L1` (sysId=`20346`) · TAC `35693811` · 基带 `S9380ZCSDCZH1` · MAC OUI `[5c:0a:5b 8c:77:12 e4:7c:f9]`

### 4. `vendor/etc/*_config.xml` 虚拟 HAL AI 对齐清单（对标 `gold-android15/vendor/etc`）
- **`sensors_config.xml`**: 共 `22` 项对齐传感器
- **`battery_config.xml`**: `Li-ion` · 容量 `5000 mAh` · 满电/充电电压 `4470 / 4520 mV`
- **`thermal_config.xml`**: 皮肤基温 `30.500 ℃` · 温区传感器 `7` 项 · Cooling=`devfreq-3d00000.qcom,kgsl-3d0`
- **`gnss_config.xml`**: systemInfo=`QCOM_GNSS_default` · yearOfHw=`2025` · L5=`true`
- **`vibrator_config.xml` & `light_config.xml`**: 马达 `X-axis linear` (cap=`31`, effects=`[CLICK DOUBLE_CLICK TICK THUD POP HEAVY_CLICK TEXTURE_TICK QUICK_RISE SLOW_RISE QUICK_FALL]`) · 逻辑灯 `8` 项
- **`audio_effects_descriptors.xml` & `media_config.xml`**: AI 音效描述符 `12` 项 · AI Codec2 实例 `3` 项 / 组件 `21` 项

### 5. AI 真机对齐提示
- 平台口径整体由 gold 的 MediaTek 换成 Qualcomm sun：cpufreq driver=qcom-cpufreq-hw、governor=schedutil、gnss=QCOM_GNSS_default、radioVariant=default；gold 的 mt-cpufreq/sugov_ext/mtk-kpd/MTK_MNLD/mtk-pmic-keys 一律不得带入。
- cpu.clusters 严格按固件实测三簇（policy0=0-2 容量653、policy3=3-6 容量998、policy7=7 容量1024），而非官方营销的 2×4.47+6×3.53；prime 的 maxFreqKhz 取 vendorEtcFreqHintsKhz 实测上限 4281600，不得写成 4470000。
- 各簇频点以实测提示 4281600/3532800/2169600/1747200/1632000/1324800/700000 为锚点铺满等差档位，保证 device-checker 读 scaling_available_frequencies 时首档=实测上限、末档=700000。
- GPU 由 Mali-G57 换成 Adreno 830：glExtraExts 全量替换为 GL_QCOM_*（不再含任何 GL_ARM_mali_*），vulkanVendorId=0x5143、vulkanDriverId=8（VK_DRIVER_ID_QUALCOMM_PROPRIETARY，gold 的 9 是 ARM），vulkanHardware=adreno。
- display：6.9 英寸 1440x3120 19.5:9，有效显示区由 116.9cm² 反算得 73.5×159.2mm（非机身 162.8×77.6mm），逻辑密度沿用固件实测 450dpi；EDID 换三星 SAM 口径（0x4C2D），productId/周数属出厂烧录值，02 阶段建议 dumpsys display 回读后按批次覆盖。
- input：触控 IC 为三星自家 sec_touchscreen（不是 goodix/synaptics/fts），按键沿用实测 gpio-keys（不是 mtk-kpd），无 swtp 节点（swtp 仅小米系）；指纹为屏下超声波，节点为 qbt_handler/uinput-qbt，goodixFp=false。
- 耳机插孔名沿用实测 sun-snd-card Headset Jack；本机无 3.5mm 孔（audioJack35mm=false），故耳机节点仅作 USB-C/蓝牙路由占位，PMIC 电源键用 Qualcomm 口径 pmic-keys。
- sensors 按固件 sensors_config 四颗实体芯片建表：lsm6dsv(acc/gyro)、ak0991x(mag)、stk3afx(light/prox)、lps22df(baro)，并补 sensors.grip.so / sensors.flicker.so 对应的 samsung.sensor.grip 与 samsung.sensor.flicker；严禁出现 xiaomi.sensor.*/oplus.sensor.*/vivo.sensor.*。
- battery：5000mAh Li-ion 单电芯、满电 4470mV、充电 4520mV、45W 有线 + 15W 无线（wireless=1，gold 的 0 不适用）；vibrator 由 gold 的 Z 轴换成 X 轴线性马达，capabilities 打开振幅/合成/PWLE 位（31）。
- media：按固件实测 3 个 Codec2 service 建 instances，21 个硬编解码器按 firmware 的 store（default1，杜比 AC4 为 default9）登记且 rank 统一 1，绝不占用真实 default store；AudioEffects 直接采用固件提取的 12 条 NXP+AOSP 描述符（三星 Dolby/SoundAlive 由 Samsung 效果库另行接管，不下沉到 AOSP 描述符表）。
- hardwareHals：enableNfc=true（实测 libnfc-nxp.conf + com.nxp.mifare + NXP NFC 芯片）、enableIr=false（S25 Ultra 无红外，gold 的 true 为小米专属）；vintf 占位按实测 vendor.qti.* + vendor.samsung.* 保留，radioVariant 用 default 而非 mtk。
- 待真机回读校准：imeiTac/imeiSvn/meidPrefix、widevineSystemId、edidProductId/edidWeek、vulkanDeviceId 与 GPU 驱动 build 号属厂商按批次/固件烧录值，本表给同平台同代合理值，建议 02 阶段以 getprop ro.serialno/gsm.sim.operator、dumpsys media.drm、dumpsys SurfaceFlinger 回读后覆盖。

## 三、官方固件包解包实测对账（`build.prop` + `boot.img` + `VINTF` + `DTB` + `fstab` + `vendor/etc`）

- `marketName`: `` | `modelCode`: `SM-S9380` | `manufacturer`: `samsung`
- `board`: `sun` | `soc`: `QTI  ()`
- `densityDpi`: `450` | `egl`: `adreno` | `vulkan`: `adreno` | `abilist`: `arm64-v8a`
- `securityPatch`: `2026-08-05` | `buildDate`: `20260812` | `kernelLine`: `` | `vintfVersion`: `-`
- `partitionAndroidVersions`: `system=16, system_ext=16, product=16, vendor=15, odm=15, vendor_dlkm=15`
- `basebandVersion`: `S9380ZCSDCZH1`（来源: `zip:CP_S9380ZCSDCZH1_CP35846090_MQB113296591_REV00_user_low_ship_MULTI_CERT.tar.md5`）
- `fingerprint`: `samsung/pa3qzcx/pa3q:15/AP3A.240905.015.A2/S9380ZCSDCZH1:user/release-keys`
- `vendor_boot.cmdline` (4 项): `androidboot.product.hardware.sku=pa3qzcx`, `androidboot.secureboot=1`, `androidboot.slot=a`, `androidboot.veritymode.managed=yes`
- `vda.partitions` (1 个附加分区): `vendor_persist.img->/mnt/vendor/persist`
- `vintfPlaceholders`: radio=`17` 项 · audio=`0` 项 · gnss=`2` 项 · nn=`62` 项

### `vendor/etc` 与 `odm/etc` 硬件配置实测详情

- **Permissions 硬件特性**（共 `97` 项）: 红外=`false` · NFC=`true` · 指纹=`false` · BLE=`true` · Wi-Fi Direct=`true` · Audio LowLatency=`true` · Vulkan Level=`1` (API `1.3`)
  - 声明的传感器特性 (`7` 项): `accelerometer`, `barometer`, `compass`, `gyroscope`, `light`, `stepcounter`, `stepdetector`
- **Sensors 原厂配置**: SubHALs=`[sensors.qsh.so sensors.flicker.so sensors.grip.so]` · 芯片归属=`map[barometer:lps22df_0 imu:lsm6dsv_0 light:stk3afx_0_platform magnetometer:ak0991x_0]`
- **Audio 策略与音效配置**: 模块=`[]` · 采样率=`[]` · 听筒=`false` · 有线耳机路由=`false` · 双麦=`false` · 音效描述符=`12` 项（源: `vendor/etc/audio_effects.xml`）
  - 实测音效清单: `Auxiliary Environmental Reverb (NXP Software Ltd.)`, `Auxiliary Preset Reverb (NXP Software Ltd.)`, `Dynamic Bass Boost (NXP Software Ltd.)`, `DynamicsProcessing (The Android Open Source Project)`, `EqualizerBundle (NXP Software Ltd.)`, `Insert Environmental Reverb (NXP Software Ltd.)`, `Insert Preset Reverb (NXP Software Ltd.)`, `Loudness Enhancer (The Android Open Source Project)`, `Multichannel Downmix To Stereo (The Android Open Source Project)`, `Virtualizer (NXP Software Ltd.)`, `Visualizer (The Android Open Source Project)`, `Volume (NXP Software Ltd.)`
- **Media Codec2 硬编解码配置**: Store实例=`[samsung.software.media.c2@1.0-service->default1 vendor.qti.hardware.c2pa-aidl-service-qti->default1 vendor.qti.media.c2@1.0-service->default1]` · 厂商/杜比组件=`21` 项 (`c2.dolby.ac4.decoder`, `c2.qti.avc.decoder`, `c2.qti.avc.decoder.low_latency`, `c2.qti.avc.decoder.secure`, `c2.qti.hevc.decoder`, `c2.qti.hevc.decoder.low_latency`, `c2.qti.hevc.decoder.secure`, `c2.qti.vp9.decoder`, `c2.qti.vp9.decoder.low_latency`, `c2.qti.vp9.decoder.secure`, `c2.qti.av1.decoder`, `c2.qti.av1.decoder.low_latency`, `c2.qti.av1.decoder.secure`, `c2.qti.avc.encoder`, `c2.qti.avc.encoder.secure`, `c2.qti.hevc.encoder`, `c2.qti.hevc.encoder.secure`, `c2.qti.hevc.encoder.hdr`, `c2.qti.hevc.encoder.cq`, `c2.qti.mvhevc.encoder`, `c2.qti.heic.encoder`)
- **Thermal 温控与冷却设备**: GPU Cooling=`-` · 冷却节点=`[]` · 温控场景表=`1` 份
- **GNSS 定位配置**: 源文件=`vendor/etc/gps.conf` · 版本=`-` · 模式=`-` · L1Only=`false` · L5配置=`false`
- **ODM SKU / NFC / Wi-Fi / 影像场景**: NFC芯片=`NXP NFC` · RSC区域=`[]` · 相机Tuning角色=`[]`
- **Power 调频/核数控制表**: 各簇最大核数=`[]` · 配置文件=`[vendor/etc/task_profiles.json]`
- **已扫描解析的 `vendor/etc` & `odm/etc` 配置文件**: 共 `113` 个

## 四、DSH 聊天界面回复原文

## 对齐核验小结（三星 pa3q 对照 redmi/gold 基准机型）

- **身份与平台**：`samsung/pa3q`（SM-S9380，Galaxy S25 Ultra 国行），board `sun`，平台从 gold 的 **MTK 全系换成 Qualcomm**——`cpufreq` 驱动从 `mt-cpufreq` 换成 `qcom-cpufreq-hw`、`governor` 从 `sugov_ext` 换成 `schedutil`、`gnss.systemInfoName` 从 `MTK_MNLD_default` 换成 `QCOM_GNSS_default`、`radioVariant` 从 `mtk` 换成 `default`（gold 的 17 条 `vendor.samsung/MTK` 类 vintf 占位改由实测 `vendor.qti.*` + `vendor.samsung.*` 原样保留）。
- **SoC**：SM8750-AC Snapdragon 8 Elite for Galaxy（3nm），营销 2×Oryon V2 Phoenix L 4.47GHz + 6×Oryon V2 Phoenix M 3.53GHz；但**固件 cpufreq 实测是三簇 3/4/1**（policy0=0-2、policy3=3-6、policy7=7），且能承受的最高频率只有 **4281600kHz**，因此 `clusters` 严格按实测三簇 + `vendorEtcFreqHintsKhz` 里出现的 3532800/2169600/1747200/1632000/1324800/700000 铺满各簇频点，**不照搬营销 4.47GHz**。
- **GPU**：Adreno 830（1200MHz），`vulkanHardware` 从 `mali` 换成 `adreno`，`vulkanVendorId` 从 `0x13b5` 换成 `0x5143`（Qualcomm），`vulkanDriverId` 从 9（ARM）换成 **8（VK_DRIVER_ID_QUALCOMM_PROPRIETARY）**，API 仍为 1.3 → `0x00403116`；GL 扩展换成 Adreno 的 `GL_QCOM_*` 系列（不再有 `GL_ARM_mali_*`）。
- **屏幕**：6.9" LTPO AMOLED 2X，1440×3120（19.5:9，498ppi，有效显示区 ≈ 73.5×159.2mm，由 116.9cm² 反算），逻辑密度沿用固件实测 **450dpi**；EDID 由 `MTK/MTKDEV/0x8B36` 换成三星 `SAM/0x4C2D` 口径（productId `0x0A31`，2025 年第 6 周），刷新率按 LTPO 1–120 档给 30/60/90/120，默认 120。
- **输入与外设**：触控由 `goodix_ts` 换成三星自家 **`sec_touchscreen`**（bus 24/I2C，vendor 0x04E8），按键驱动沿用实测 **`gpio-keys`**（不是 `mtk-kpd`），指纹节点的标示从 `goodixFp/uinput-fpc` 换成 **电容式改为屏下超声波 `qbt_handler` + `uinput-qbt`**，耳机插孔沿用实测 **`sun-snd-card Headset Jack`**；**三星无 `swtp`**，故 `inputIdentity` 不填 swtp（gold 的 `mtk-pmic-keys` 同步换成 `pmic-keys`）。
- **传感器**：按固件 `sensors_config` 的四颗实体芯片（`lsm6dsv` IMU、`ak0991x` 磁力、`stk3afx` 光感/距感、`lps22df` 气压）建传感器表，再补标准融合/未校准/计步/倾斜 + 三星私有 `samsung.sensor.grip`（对应 `sensors.grip.so`）与 `samsung.sensor.flicker`（对应 `sensors.flicker.so`）；**不出现任何 `xiaomi.sensor.*`**。
- **硬件 HAL 与 vendor/etc**：`enableIr=false`（S25U 无红外）、`enableNfc=true`（实测 `libnfc-nxp.conf` + NXP NFC）、`enableIr` 与 gold 相反；`battery` 5000mAh Li-ion/4470mV/45W+15W 无线（`wireless=1`）、`vibrator` 由 Z 轴换成 **X 轴线性马达**、`media` 按实测 3 个 Codec2 service 与 21 个硬编解码器登记（store 沿用 `default1`/`default9`，rank=1）。
- **待真机回读校准项**：`imeiTac`/`imeiSvn`/`meidPrefix`、`widevineSystemId`、`edidProductId`/`edidWeek`、`vulkanDeviceId`/驱动串号段属于厂商按批次烧录值，已给同代同平台合理值，建议 02 阶段用 `getprop`+`dumpsys` 回读后覆盖（§注记已列明）。

```json
{
  "official": "三星",
  "brand": "samsung",
  "device": "pa3q",
  "model": "Samsung Galaxy S25 Ultra",
  "modelAliases": ["SM-S9380", "SM-S938B", "SM-S938U", "SM-S938W", "SM-S938N", "SM-S938E"],
  "releaseDate": "2025-02",
  "platform": "qcom",
  "board": "sun",
  "soc": {
    "manufacturer": "Qualcomm",
    "marketingName": "Snapdragon 8 Elite for Galaxy",
    "model": "SM8750-AC",
    "partNumber": "SM8750-AC",
    "processNm": 3
  },
  "display": {
    "diagonalInch": 6.9,
    "resolution": "1440x3120",
    "widthPx": 1440,
    "heightPx": 3120,
    "aspectRatio": "19.5:9",
    "widthMm": 73.5,
    "heightMm": 159.2,
    "ppi": 498,
    "xdpi": 497.87,
    "ydpi": 497.75,
    "androidDensityDpi": 450,
    "panelType": "Dynamic LTPO AMOLED 2X",
    "refreshRatesHz": [30, 60, 90, 120],
    "defaultRefreshRateHz": 120,
    "wideColorGamut": true,
    "edidVendorHint": "SAMSUNG",
    "edidName": "SAMSUNG",
    "edidProductId": 2609,
    "edidWeek": 6,
    "edidYear": 2025
  },
  "cpu": {
    "smp": 8,
    "architecture": "arm64-v8a",
    "topologySummary": "1x Oryon V2 Phoenix L @ 4.2816GHz(固件实测上限) + 4x Oryon V2 Phoenix M @ 3.5328GHz + 3x Oryon V2 Phoenix M @ 2.1696GHz（固件 cpufreq 三簇 3/4/1）",
    "driver": "qcom-cpufreq-hw",
    "governor": "schedutil",
    "availableGovernors": ["schedutil", "performance", "powersave", "userspace"],
    "transitionLatency": 1000,
    "scalingReadable": false,
    "packagePerCluster": true,
    "bogoMips": "38.40",
    "clusters": [
      {
        "policy": 0,
        "cpus": [0, 1, 2],
        "coreName": "Oryon V2 Phoenix M",
        "minFreqKhz": 700000,
        "maxFreqKhz": 2169600,
        "curFreqKhz": 700000,
        "availableFrequenciesKhz": [
          2169600, 2054400, 1939200, 1824000, 1708800, 1593600, 1478400, 1363200,
          1248000, 1132800, 1017600, 940800, 864000, 787200, 700000
        ],
        "implementer": "0x51",
        "variant": 15,
        "part": "0x001",
        "revision": 0,
        "capacity": 653,
        "midr": "0x00000000510f0010",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 3,
        "cpus": [3, 4, 5, 6],
        "coreName": "Oryon V2 Phoenix M",
        "minFreqKhz": 700000,
        "maxFreqKhz": 3532800,
        "curFreqKhz": 940800,
        "availableFrequenciesKhz": [
          3532800, 3417600, 3302400, 3187200, 3072000, 2956800, 2841600, 2726400,
          2611200, 2496000, 2380800, 2265600, 2169600, 2054400, 1939200, 1824000,
          1708800, 1593600, 1478400, 1363200, 1248000, 1132800, 1017600, 902400,
          787200, 700000
        ],
        "implementer": "0x51",
        "variant": 15,
        "part": "0x001",
        "revision": 0,
        "capacity": 998,
        "midr": "0x00000000510f0010",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 7,
        "cpus": [7],
        "coreName": "Oryon V2 Phoenix L",
        "minFreqKhz": 700000,
        "maxFreqKhz": 4281600,
        "curFreqKhz": 1056000,
        "availableFrequenciesKhz": [
          4281600, 4147200, 4012800, 3878400, 3744000, 3609600, 3475200, 3340800,
          3206400, 3072000, 2937600, 2803200, 2668800, 2534400, 2400000, 2265600,
          2131200, 1996800, 1862400, 1728000, 1593600, 1459200, 1324800, 1190400,
          1056000, 940800, 825600, 700000
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
    "renderer": "Adreno 830",
    "glesVersion": "OpenGL ES 3.2",
    "glVersionFull": "OpenGL ES 3.2 V@0801.0 (GIT@7f3f5b0, I8a1f3e5d2b, 1731400000) (Date:11/12/24)",
    "glslVersion": "OpenGL ES GLSL ES 3.20",
    "glExtraExts": "GL_QCOM_alpha_test,GL_QCOM_binning_control,GL_QCOM_driver_control,GL_QCOM_extended_get,GL_QCOM_extended_get2,GL_QCOM_frame_extrapolation,GL_QCOM_framebuffer_foveated,GL_QCOM_motion_estimation,GL_QCOM_shader_framebuffer_fetch_noncoherent,GL_QCOM_shader_framebuffer_fetch_rate,GL_QCOM_tiled_rendering,GL_QCOM_texture_foveated,GL_QCOM_texture_foveated2,GL_QCOM_texture_foveated_subsampled_layout,GL_QCOM_yuv_texture_gather",
    "glExtraExts2": "GL_EXT_debug_marker,GL_EXT_texture_cube_map_array,GL_EXT_texture_filter_anisotropic,GL_EXT_texture_sRGB_decode,GL_EXT_sRGB_write_control,GL_EXT_color_buffer_half_float,GL_EXT_disjoint_timer_query,GL_EXT_multisampled_render_to_texture2,GL_EXT_render_snorm,GL_EXT_robustness,GL_OES_texture_half_float_linear,GL_KHR_blend_equation_advanced",
    "maxTextureSize": 16384,
    "maxCombinedTextureImageUnits": 96,
    "maxVertexAttribs": 32,
    "vulkanHardware": "adreno",
    "vulkanVendorId": "0x5143",
    "vulkanApiVersion": "1.3",
    "vulkanApiVersionHex": "0x00403116",
    "vulkanConformance": "1.3.5.0",
    "vulkanDeviceId": "0x08030000",
    "vulkanDriverId": 8,
    "vulkanDriverVersion": "0x08010000",
    "vulkanDriverInfo": "V@0801.0 (GIT@7f3f5b0, I8a1f3e5d2b, 1731400000)",
    "vulkanDeviceUuid": "3ad1f0c95b7e42d8a6c40f1b9e2d7c58",
    "vulkanDriverUuid": "b7c2e91d4a6f3805c1d92e7b40a8f36d",
    "vulkanPipelineUuid": "0f4a8c2e6b1d39a75c8e2f4b60d3a91c"
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
    "skuCombinations": ["12GB+256GB", "12GB+512GB", "12GB+1TB", "16GB+1TB"]
  },
  "peripherals": {
    "fingerprint": "屏下超声波指纹 (Qualcomm 3D Sonic / QBT)",
    "touchControllerHints": ["sec_touchscreen", "sec_ts"],
    "keyboardDriverHint": "gpio-keys",
    "headsetJackNameHint": "sun-snd-card Headset Jack",
    "pmicKeysNameHint": "pmic-keys",
    "sensors": ["accelerometer", "gyroscope", "magnetometer", "barometer", "ambient_light", "proximity", "step_counter", "step_detector", "significant_motion", "grip"],
    "sensorChips": {
      "accelerometer": "lsm6dsv_acc",
      "gyroscope": "lsm6dsv_gyro",
      "magnetometer": "ak0991x",
      "light": "stk3afx_l",
      "proximity": "stk3afx_p",
      "barometer": "lps22df",
      "grip": "samsung_grip"
    },
    "nfc": true,
    "infrared": false,
    "audioJack35mm": false,
    "speakers": "立体声双扬声器 (AKG 调校)"
  },
  "camera": {
    "rear": "200MP + 50MP 超广角 + 50MP 5x 潜望长焦 + 10MP 3x 长焦",
    "front": "12MP"
  },
  "batteryAndConnectivity": {
    "batteryMah": 5000,
    "wiredChargingW": 45,
    "wirelessChargingW": 15,
    "wifi": "Wi-Fi 7 (802.11be)",
    "wifi6Supported": true,
    "bluetooth": "5.4",
    "cellular5G": true,
    "macOuiCandidates": ["5c:0a:5b", "8c:77:12", "e4:7c:f9"],
    "widevineDefault": "L1",
    "widevineSystemId": "20346",
    "widevineVersion": "19.0.0",
    "oemCryptoApiVersion": 19,
    "imeiTac": "35693811",
    "imeiSvn": "78",
    "meidPrefix": "99001346"
  },
  "inputAlignment": {
    "touch": {
      "name": "sec_touchscreen",
      "bus": 24,
      "vendor": 1256,
      "product": 1,
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
      "goodixFp": false,
      "devices": [
        {
          "name": "qbt_handler",
          "bus": 25,
          "vendor": 1,
          "product": 1,
          "version": 1,
          "keys": ["KEY_POWER", "KEY_WAKEUP"],
          "msc": ["MSC_SCAN"]
        },
        {
          "name": "uinput-qbt",
          "bus": 6,
          "vendor": 1638,
          "product": 2184,
          "version": 0,
          "keys": ["KEY_UP", "KEY_LEFT", "KEY_RIGHT", "KEY_DOWN"],
          "abs": [{"axis": "ABS_Z", "min": 0, "max": 255, "fuzz": 0, "flat": 0}]
        },
        {
          "name": "sun-snd-card Headset Jack",
          "bus": 25,
          "phys": "ALSA",
          "keys": ["KEY_PLAYPAUSE", "BTN_1", "BTN_2", "KEY_VOICECOMMAND"],
          "switches": ["SW_HEADPHONE_INSERT", "SW_MICROPHONE_INSERT", "SW_LINEOUT_INSERT", "SW_JACK_PHYSICAL_INSERT"]
        },
        {
          "name": "pmic-keys",
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
    {"type": "battery", "base-temp-mc": 28500, "jitter-mc": 100},
    {"type": "cpuss-0", "base-temp-mc": 34200, "jitter-mc": 350},
    {"type": "cpuss-1", "base-temp-mc": 34000, "jitter-mc": 330},
    {"type": "gpuss-0", "base-temp-mc": 33200, "jitter-mc": 320},
    {"type": "nspss-0", "base-temp-mc": 33000, "jitter-mc": 300},
    {"type": "socd-0", "base-temp-mc": 34500, "jitter-mc": 400}
  ],
  "hardwareHals": {
    "radioVariant": "default",
    "enableIr": false,
    "enableNfc": true,
    "vintfDrop": []
  },
  "vendorEtcConfigs": {
    "battery": {
      "technology": "Li-ion",
      "cycleCount": 128,
      "fullUah": 5000,
      "counterMin": 4890,
      "counterMax": 4945,
      "fullMv": 4470,
      "chargeMv": 4520,
      "temperatureFullDecic": 300,
      "temperatureChargeDecic": 380,
      "currentFullUa": -68000,
      "currentChargeUa": -450000,
      "wireless": 1,
      "stateOfHealth": 99
    },
    "vibrator": {
      "motorType": "X-axis linear",
      "registerManager": true,
      "capabilities": 31,
      "managerCapabilities": 31,
      "compositionDelayMax": 1000,
      "compositionSizeMax": 256,
      "vibratorIds": [1],
      "effects": ["CLICK", "DOUBLE_CLICK", "TICK", "THUD", "POP", "HEAVY_CLICK", "TEXTURE_TICK", "QUICK_RISE", "SLOW_RISE", "QUICK_FALL"]
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
      "skinCelsius": 30.5,
      "modelMaximumCelsius": 50,
      "coolingDeviceName": "devfreq-3d00000.qcom,kgsl-3d0",
      "sensors": [
        {"id": 0, "name": "CPU", "value": 34.2, "thresholds": "nan;nan;nan;90;95;100;110", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 1, "name": "GPU", "value": 33.2, "thresholds": "nan;nan;nan;90;95;100;110", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 2, "name": "BATTERY", "value": 28.5, "thresholds": "nan;nan;nan;45;50;55;60", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 3, "name": "SKIN", "value": 30.5, "thresholds": "nan;40;43;46;50;55;60", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 5, "name": "POWER_AMPLIFIER", "value": 31.5, "thresholds": "nan;nan;nan;60;70;80;90", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 9, "name": "NPU", "value": 33.0, "thresholds": "nan;nan;nan;90;95;100;110", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 13, "name": "SOC", "value": 34.5, "thresholds": "nan;nan;nan;95;100;105;115", "shutdown": "nan;nan;nan;nan;nan;nan;nan"}
      ]
    },
    "lightEntries": [
      {"id": 0, "ordinal": 0, "type": "BACKLIGHT"},
      {"id": 1, "ordinal": 0, "type": "KEYBOARD"},
      {"id": 2, "ordinal": 0, "type": "BUTTONS"},
      {"id": 3, "ordinal": 0, "type": "BATTERY"},
      {"id": 4, "ordinal": 0, "type": "NOTIFICATIONS"},
      {"id": 5, "ordinal": 0, "type": "ATTENTION"},
      {"id": 6, "ordinal": 1, "type": "BLUETOOTH"},
      {"id": 7, "ordinal": 1, "type": "WIFI"}
    ],
    "sensors": [
      {"handle": 1, "name": "lsm6dsv_acc", "vendor": "STMicroelectronics", "version": 1, "typeId": 1, "type": "android.sensor.accelerometer", "maxRange": "78.453605652", "resolution": "0.0023956299", "power": "0.145", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 2, "name": "ak0991x", "vendor": "AKM", "version": 1, "typeId": 2, "type": "android.sensor.magnetic_field", "maxRange": "4912.050292969", "resolution": "0.150000006", "power": "0.28", "minDelay": 10000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 3, "name": "ORIENTATION", "vendor": "Samsung", "version": 1, "typeId": 3, "type": "android.sensor.orientation", "maxRange": "360.0", "resolution": "0.00390625", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 4, "name": "lsm6dsv_gyro", "vendor": "STMicroelectronics", "version": 1, "typeId": 4, "type": "android.sensor.gyroscope", "maxRange": "34.906299591", "resolution": "0.001065", "power": "0.75", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 5, "name": "stk3afx_l", "vendor": "Sensortek", "version": 1, "typeId": 5, "type": "android.sensor.light", "maxRange": "65535.0", "resolution": "1.0", "power": "0.101", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 6, "name": "lps22df", "vendor": "STMicroelectronics", "version": 1, "typeId": 6, "type": "android.sensor.pressure", "maxRange": "1100.0", "resolution": "0.01", "power": "0.06", "minDelay": 10000, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 8, "name": "stk3afx_p", "vendor": "Sensortek", "version": 1, "typeId": 8, "type": "android.sensor.proximity", "maxRange": "8.0", "resolution": "1.0", "power": "0.15", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 1000000, "wakeUp": true, "reporting": 1},
      {"handle": 9, "name": "GRAVITY", "vendor": "Samsung", "version": 1, "typeId": 9, "type": "android.sensor.gravity", "maxRange": "39.226802826", "resolution": "0.0023956299", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 10, "name": "LINEARACCEL", "vendor": "Samsung", "version": 1, "typeId": 10, "type": "android.sensor.linear_acceleration", "maxRange": "39.226802826", "resolution": "0.0023956299", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 11, "name": "ROTATION_VECTOR", "vendor": "Samsung", "version": 1, "typeId": 11, "type": "android.sensor.rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 14, "name": "UNCALI_MAG", "vendor": "Samsung", "version": 1, "typeId": 14, "type": "android.sensor.magnetic_field_uncalibrated", "maxRange": "4912.050292969", "resolution": "0.150000006", "power": "0.001", "minDelay": 20000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 15, "name": "GAME_ROTATION_VECTOR", "vendor": "Samsung", "version": 1, "typeId": 15, "type": "android.sensor.game_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 16, "name": "UNCALI_GYRO", "vendor": "Samsung", "version": 1, "typeId": 16, "type": "android.sensor.gyroscope_uncalibrated", "maxRange": "34.906299591", "resolution": "0.001065", "power": "0.001", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 17, "name": "SIGNIFICANT_MOTION", "vendor": "Samsung", "version": 1, "typeId": 17, "type": "android.sensor.significant_motion", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 18, "name": "STEP_DETECTOR", "vendor": "Samsung", "version": 1, "typeId": 18, "type": "android.sensor.step_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 0, "wakeUp": false, "reporting": 3},
      {"handle": 19, "name": "STEP_COUNTER", "vendor": "Samsung", "version": 1, "typeId": 19, "type": "android.sensor.step_counter", "maxRange": "2147483648.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 20, "name": "GEOMAGNETIC_ROTATION_VECTOR", "vendor": "Samsung", "version": 1, "typeId": 20, "type": "android.sensor.geomagnetic_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 22, "name": "TILT_DETECTOR", "vendor": "Samsung", "version": 1, "typeId": 22, "type": "android.sensor.tilt_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 3},
      {"handle": 27, "name": "DEVICE_ORIENTATION", "vendor": "Samsung", "version": 1, "typeId": 27, "type": "android.sensor.device_orientation", "maxRange": "3.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 35, "name": "UNCALI_ACC", "vendor": "Samsung", "version": 1, "typeId": 35, "type": "android.sensor.accelerometer_uncalibrated", "maxRange": "39.226802826", "resolution": "0.0023956299", "power": "0.001", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 65537, "name": "samsung.sensor.grip", "vendor": "Samsung", "version": 1, "typeId": 65537, "type": "samsung.sensor.grip", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": true, "reporting": 1},
      {"handle": 65538, "name": "samsung.sensor.flicker", "vendor": "Samsung", "version": 1, "typeId": 65538, "type": "samsung.sensor.flicker", "maxRange": "65535.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1}
    ],
    "audioEffects": [
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
        {"name": "samsung.software.media.c2@1.0-service", "store": "default1", "rank": 1},
        {"name": "vendor.qti.hardware.c2pa-aidl-service-qti", "store": "default1", "rank": 1},
        {"name": "vendor.qti.media.c2@1.0-service", "store": "default1", "rank": 1}
      ],
      "components": [
        {"store": "default9", "name": "c2.dolby.ac4.decoder", "kind": "decoder", "domain": "audio", "mediaType": "audio/ac4", "aliases": "OMX.dolby.ac4.decoder", "rank": 1},
        {"store": "default1", "name": "c2.qti.avc.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/avc", "aliases": "OMX.qcom.video.decoder.avc", "maxSize": "8192x8192", "profiles": "20480,20481,20482,20484,20487", "levels": "20495", "rank": 1},
        {"store": "default1", "name": "c2.qti.avc.decoder.low_latency", "kind": "decoder", "domain": "video", "mediaType": "video/avc", "aliases": "OMX.qcom.video.decoder.avc.low_latency", "maxSize": "8192x8192", "profiles": "20480,20481,20482,20484,20487", "levels": "20495", "rank": 1},
        {"store": "default1", "name": "c2.qti.avc.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/avc", "aliases": "OMX.qcom.video.decoder.avc.secure", "maxSize": "4096x4096", "profiles": "20480,20481,20482,20484,20487", "levels": "20495", "rank": 1},
        {"store": "default1", "name": "c2.qti.hevc.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/hevc", "aliases": "OMX.qcom.video.decoder.hevc", "maxSize": "8192x8192", "profiles": "24576,24578,24577", "levels": "24835", "rank": 1},
        {"store": "default1", "name": "c2.qti.hevc.decoder.low_latency", "kind": "decoder", "domain": "video", "mediaType": "video/hevc", "aliases": "OMX.qcom.video.decoder.hevc.low_latency", "maxSize": "8192x8192", "profiles": "24576,24578,24577", "levels": "24835", "rank": 1},
        {"store": "default1", "name": "c2.qti.hevc.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/hevc", "aliases": "OMX.qcom.video.decoder.hevc.secure", "maxSize": "4096x4096", "profiles": "24576,24578,24577", "levels": "24835", "rank": 1},
        {"store": "default1", "name": "c2.qti.vp9.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/x-vnd.on2.vp9", "aliases": "OMX.qcom.video.decoder.vp9", "maxSize": "4096x4096", "profiles": "28672,28673,28674", "levels": "28680", "rank": 1},
        {"store": "default1", "name": "c2.qti.vp9.decoder.low_latency", "kind": "decoder", "domain": "video", "mediaType": "video/x-vnd.on2.vp9", "aliases": "OMX.qcom.video.decoder.vp9.low_latency", "maxSize": "4096x4096", "profiles": "28672,28673,28674", "levels": "28680", "rank": 1},
        {"store": "default1", "name": "c2.qti.vp9.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/x-vnd.on2.vp9", "aliases": "OMX.qcom.video.decoder.vp9.secure", "maxSize": "4096x4096", "profiles": "28672,28673,28674", "levels": "28680", "rank": 1},
        {"store": "default1", "name": "c2.qti.av1.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/av01", "maxSize": "8192x8192", "rank": 1},
        {"store": "default1", "name": "c2.qti.av1.decoder.low_latency", "kind": "decoder", "domain": "video", "mediaType": "video/av01", "maxSize": "8192x8192", "rank": 1},
        {"store": "default1", "name": "c2.qti.av1.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/av01", "maxSize": "4096x4096", "rank": 1},
        {"store": "default1", "name": "c2.qti.avc.encoder", "kind": "encoder", "domain": "video", "mediaType": "video/avc", "aliases": "OMX.qcom.video.encoder.avc", "maxSize": "8192x8192", "profiles": "20480,20482,20484", "levels": "20494", "rank": 1},
        {"store": "default1", "name": "c2.qti.avc.encoder.secure", "kind": "encoder", "domain": "video", "mediaType": "wfd/avc", "aliases": "OMX.qcom.video.encoder.avc.secure", "maxSize": "8192x8192", "rank": 1},
        {"store": "default1", "name": "c2.qti.hevc.encoder", "kind": "encoder", "domain": "video", "mediaType": "video/hevc", "aliases": "OMX.qcom.video.encoder.hevc", "maxSize": "8192x8192", "profiles": "24576", "levels": "24834", "rank": 1},
        {"store": "default1", "name": "c2.qti.hevc.encoder.secure", "kind": "encoder", "domain": "video", "mediaType": "wfd/hevc", "aliases": "OMX.qcom.video.encoder.hevc.secure", "maxSize": "8192x8192", "rank": 1},
        {"store": "default1", "name": "c2.qti.hevc.encoder.hdr", "kind": "encoder", "domain": "video", "mediaType": "video/hevc", "maxSize": "4096x2176", "profiles": "24576", "levels": "24834", "rank": 1},
        {"store": "default1", "name": "c2.qti.hevc.encoder.cq", "kind": "encoder", "domain": "video", "mediaType": "video/hevc", "aliases": "OMX.qcom.video.encoder.hevc.cq", "maxSize": "512x512", "profiles": "24576", "levels": "24834", "rank": 1},
        {"store": "default1", "name": "c2.qti.mvhevc.encoder", "kind": "encoder", "domain": "video", "mediaType": "video/x-mvhevc", "maxSize": "4096x4096", "rank": 1},
        {"store": "default1", "name": "c2.qti.heic.encoder", "kind": "encoder", "domain": "video", "mediaType": "image/vnd.android.heic", "aliases": "OMX.qcom.video.encoder.heic", "maxSize": "16384x16384", "rank": 1}
      ]
    },
    "audioEngine": {
      "modules": "default,bluetooth,r_submix,usb",
      "voiceCallIndexMax": 7,
      "musicIndexMax": 15
    }
  },
  "alignmentNotes": [
    "平台口径整体由 gold 的 MediaTek 换成 Qualcomm sun：cpufreq driver=qcom-cpufreq-hw、governor=schedutil、gnss=QCOM_GNSS_default、radioVariant=default；gold 的 mt-cpufreq/sugov_ext/mtk-kpd/MTK_MNLD/mtk-pmic-keys 一律不得带入。",
    "cpu.clusters 严格按固件实测三簇（policy0=0-2 容量653、policy3=3-6 容量998、policy7=7 容量1024），而非官方营销的 2×4.47+6×3.53；prime 的 maxFreqKhz 取 vendorEtcFreqHintsKhz 实测上限 4281600，不得写成 4470000。",
    "各簇频点以实测提示 4281600/3532800/2169600/1747200/1632000/1324800/700000 为锚点铺满等差档位，保证 device-checker 读 scaling_available_frequencies 时首档=实测上限、末档=700000。",
    "GPU 由 Mali-G57 换成 Adreno 830：glExtraExts 全量替换为 GL_QCOM_*（不再含任何 GL_ARM_mali_*），vulkanVendorId=0x5143、vulkanDriverId=8（VK_DRIVER_ID_QUALCOMM_PROPRIETARY，gold 的 9 是 ARM），vulkanHardware=adreno。",
    "display：6.9 英寸 1440x3120 19.5:9，有效显示区由 116.9cm² 反算得 73.5×159.2mm（非机身 162.8×77.6mm），逻辑密度沿用固件实测 450dpi；EDID 换三星 SAM 口径（0x4C2D），productId/周数属出厂烧录值，02 阶段建议 dumpsys display 回读后按批次覆盖。",
    "input：触控 IC 为三星自家 sec_touchscreen（不是 goodix/synaptics/fts），按键沿用实测 gpio-keys（不是 mtk-kpd），无 swtp 节点（swtp 仅小米系）；指纹为屏下超声波，节点为 qbt_handler/uinput-qbt，goodixFp=false。",
    "耳机插孔名沿用实测 sun-snd-card Headset Jack；本机无 3.5mm 孔（audioJack35mm=false），故耳机节点仅作 USB-C/蓝牙路由占位，PMIC 电源键用 Qualcomm 口径 pmic-keys。",
    "sensors 按固件 sensors_config 四颗实体芯片建表：lsm6dsv(acc/gyro)、ak0991x(mag)、stk3afx(light/prox)、lps22df(baro)，并补 sensors.grip.so / sensors.flicker.so 对应的 samsung.sensor.grip 与 samsung.sensor.flicker；严禁出现 xiaomi.sensor.*/oplus.sensor.*/vivo.sensor.*。",
    "battery：5000mAh Li-ion 单电芯、满电 4470mV、充电 4520mV、45W 有线 + 15W 无线（wireless=1，gold 的 0 不适用）；vibrator 由 gold 的 Z 轴换成 X 轴线性马达，capabilities 打开振幅/合成/PWLE 位（31）。",
    "media：按固件实测 3 个 Codec2 service 建 instances，21 个硬编解码器按 firmware 的 store（default1，杜比 AC4 为 default9）登记且 rank 统一 1，绝不占用真实 default store；AudioEffects 直接采用固件提取的 12 条 NXP+AOSP 描述符（三星 Dolby/SoundAlive 由 Samsung 效果库另行接管，不下沉到 AOSP 描述符表）。",
    "hardwareHals：enableNfc=true（实测 libnfc-nxp.conf + com.nxp.mifare + NXP NFC 芯片）、enableIr=false（S25 Ultra 无红外，gold 的 true 为小米专属）；vintf 占位按实测 vendor.qti.* + vendor.samsung.* 保留，radioVariant 用 default 而非 mtk。",
    "待真机回读校准：imeiTac/imeiSvn/meidPrefix、widevineSystemId、edidProductId/edidWeek、vulkanDeviceId 与 GPU 驱动 build 号属厂商按批次/固件烧录值，本表给同平台同代合理值，建议 02 阶段以 getprop ro.serialno/gsm.sim.operator、dumpsys media.drm、dumpsys SurfaceFlinger 回读后覆盖。"
  ],
  "sources": [
    "https://www.gsmarena.com/samsung_galaxy_s25_ultra-13322.php",
    "本机官方固件解包实测：build.prop / boot DTB / VINTF manifest / vendor/etc / odm/etc（见任务输入 JSON）",
    "Gold 标杆结构：devices/manifest/configs/redmi/gold/gold-android15/（仅字段结构与颗粒度对标，硬件值全部替换）",
    "Khronos Vulkan driver ID 注册表（VK_DRIVER_ID_QUALCOMM_PROPRIETARY = 8，VK_DRIVER_ID_ARM_PROPRIETARY = 9）"
  ]
}
```
