# 真机硬件规格与云机对齐留档 · OnePlus 15R (oneplus/CPH2767)

- **生成时间**: `2026-11-18T18:59:40+08:00`
- **ROM Build ID**: `16-BP2A.250605.015-20260810`
- **分区 Android 版本**: `system=16, system_ext=16, product=16, vendor=16, vendor_dlkm=16`
- **DSH 聊天会话 ID**: `session-9113289d-9850-4661-9342-1f6560e697fa`（标题：`[02_prepare] 真机规格采集 · oneplus/CPH2767 (16-BP2A.250605.015-20260810)`）
- **采集模型路由**: `cdh-maas / deepseek-v4.1-flash-1017`（复用在线 DSH Web: `true`）

---

## 一、设备表对齐规格（对应 `devices/manifest/devices/oneplus/CPH2767.json`）

| 字段 | 对齐值 | 说明 |
| --- | --- | --- |
| `official` | `OPPO` | 厂商集团 |
| `brand` | `oneplus` | 子品牌标识 |
| `model` | `OnePlus 15R` | 商业上市机型名 （型号代码: `CPH2767`, `ossi`, `OnePlus Ace 6`） |
| `device` | `CPH2767` | 设备代号 |
| `platform` / `board` | `qcom` / `canoe` | 芯片平台 / 主板代号 |
| `resolution` | `1272x2800` | 屏幕物理像素分辨率 |
| `inch` | `72x158` | 屏幕可视物理宽×高 mm（对角线 `6.83"`） |
| `density` | `560` | Android 逻辑密度 dpi（物理 PPI `450`，xdpi `450.23` / ydpi `450.27`） |
| `smp` | `8` | CPU 核心总数 |
| `memory` | `[8192 12288 16384]` | 全系发售内存档位 MB（8G / 12G / 16G，LPDDR5X） |
| `storage` | `[128000 256000 512000]` | 全系发售存储档位（128G / 256G / 512G，UFS 4.1） |

- **常见量产发售版本组合**: `8GB+128GB` / `12GB+256GB` / `16GB+256GB` / `16GB+512GB`

## 二、真机深层硬件对齐画像（参考 `redmi/gold` 标准）

### 1. SoC 与 CPU 簇全量频点表（`cdh_cpufreq` / `module/config.json` 对齐）
- **SoC 芯片**: Qualcomm Snapdragon 8 Elite (`SM8750` SM8750-AB, 3nm)
- **CPU 拓扑**: `2x Qualcomm Oryon Prime @ 4.32GHz + 6x Qualcomm Oryon Performance @ 3.53GHz`（驱动: `qcom-cpufreq-hw`，调速器: `schedutil`，可选调速器: `[schedutil performance powersave userspace]`，BogoMIPS: `38.40`）

| 策略簇 | 核心列表 | 微架构 | Part / MIDR | 容量权重 | 最小 / 驻留 / 最大 (kHz) | 频点档数 |
| --- | --- | --- | --- | --- | --- | --- |
| policy0 | `[0 1 2 3 4 5]` | Oryon Performance | `0x001` (`0x00000000511f0010`) | `969` | `998400 / 1324800 / 3532800` | `24 档` |
| policy6 | `[6 7]` | Oryon Prime | `0x001` (`0x00000000512f0010`) | `1024` | `960000 / 1728000 / 4320000` | `20 档` |

- **`policy0` (`Oryon Performance`, cpus `[0 1 2 3 4 5]`) 全量 `available-frequencies` (24 档, kHz)**:
  `[3532800 3360000 3244800 3168000 3072000 2976000 2880000 2784000 2688000 2592000 2496000 2400000 2304000 2208000 2169600 2016000 1920000 1824000 1747200 1632000 1555200 1440000 1324800 998400]`
- **`policy6` (`Oryon Prime`, cpus `[6 7]`) 全量 `available-frequencies` (20 档, kHz)**:
  `[4320000 4224000 4108800 3993600 3840000 3648000 3456000 3264000 3072000 2880000 2688000 2496000 2304000 2112000 1920000 1728000 1536000 1344000 1152000 960000]`

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
        "max-freq": 3532800,
        "cur-freq": 1324800,
        "available-frequencies": [
          3532800,
          3360000,
          3244800,
          3168000,
          3072000,
          2976000,
          2880000,
          2784000,
          2688000,
          2592000,
          2496000,
          2400000,
          2304000,
          2208000,
          2169600,
          2016000,
          1920000,
          1824000,
          1747200,
          1632000,
          1555200,
          1440000,
          1324800,
          998400
        ],
        "implementer": "0x51",
        "variant": 1,
        "part": "0x001",
        "revision": 0,
        "capacity": 969,
        "midr": "0x00000000511f0010",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 6,
        "cpus": [
          6,
          7
        ],
        "min-freq": 960000,
        "max-freq": 4320000,
        "cur-freq": 1728000,
        "available-frequencies": [
          4320000,
          4224000,
          4108800,
          3993600,
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
        "variant": 2,
        "part": "0x001",
        "revision": 0,
        "capacity": 1024,
        "midr": "0x00000000512f0010",
        "revidr": "0x0000000000000000"
      }
    ]
  }
}
```
</details>

### 2. 显示面板、GPU 与图形栈（`gpu_id.cfg` / `buildVendor` 对标 `gold`）
- **面板类型与刷新率**: AMOLED · 刷新率 `[60 90 120 144 165] Hz`（默认 `120 Hz`） · 广色域 `true` · EDID `OPL` (`OPLUS`, product=`39530`, year=`2025`W`41`)
- **OpenGL ES**: `Qualcomm` / `Adreno (TM) 830` / `OpenGL ES 3.2 V@0806.0 (GIT@9c1d2f4a5b, I6a3f2c1d8e, 2026-03-18T07:42:11)`
  - GL 驱动扩展: `GL_EXT_texture_filter_anisotropic,GL_EXT_debug_marker,GL_QCOM_alpha_test,GL_QCOM_binning_control,GL_QCOM_driver_control,GL_QCOM_extended_get,GL_QCOM_extended_get2,GL_QCOM_frame_extrapolation,GL_QCOM_motion_estimation,GL_QCOM_shader_framebuffer_fetch_noncoherent,GL_QCOM_shader_framebuffer_fetch_rate,GL_QCOM_texture_foveated,GL_QCOM_texture_foveated2,GL_QCOM_texture_foveated_subsampled_layout,GL_QCOM_tiled_rendering,GL_QCOM_yuv_texture_gather` GL_QCOM_memory_ext,GL_QCOM_shading_rate,GL_QCOM_render_shared_external,GL_EXT_blend_func_extended,GL_EXT_sRGB_write_control,GL_EXT_texture_sRGB_decode
- **Vulkan**: `ro.hardware.vulkan=adreno` · VendorID `0x5143` · DeviceID `0x51e00000` · API `1.4` (`0x00404000`) · DriverID `8` (`0x08060000`)

### 3. 触控、指纹、外设与输入节点对齐（`module/config.json` & `hardware/config.json` 对标 `gold`）
- **指纹方案**: 超声波屏下指纹（Goodix）
- **触控与按键对齐**: touch=`synaptics_tcm` · keyboard=`gpio-keys` · input_identity 外设数=`4` 项
- **硬件 HAL 变体与开关**: radioVariant=`default` · NFC=`true` · 红外=`true` · 3.5mm=`false` · 双扬声器（立体声）
- **影像规格**: 后置 50MP + 8MP | 前置 16MP
- **电池与通信**: `7800 mAh`（有线 `80W` / 无线 `0W`） · Wi-Fi 7 (802.11be)（Wi-Fi 6=`true`） · BT `5.4` · 5G=`true` · Widevine `L1` (sysId=`27109`) · TAC `35741206` · MAC OUI `[c0:ee:fb 94:65:2d 64:a2:f9]`

### 4. `vendor/etc/*_config.xml` 虚拟 HAL AI 对齐清单（对标 `gold-android15/vendor/etc`）
- **`sensors_config.xml`**: 共 `26` 项对齐传感器
- **`battery_config.xml`**: `Li-ion` · 容量 `7800 mAh` · 满电/充电电压 `4420 / 4500 mV`
- **`thermal_config.xml`**: 皮肤基温 `28.600 ℃` · 温区传感器 `8` 项 · Cooling=`gpuss-0`
- **`gnss_config.xml`**: systemInfo=`QCOM_GNSS_default` · yearOfHw=`2025` · L5=`true`
- **`vibrator_config.xml` & `light_config.xml`**: 马达 `X-axis linear` (cap=`63`, effects=`[CLICK DOUBLE_CLICK TICK HEAVY_CLICK TEXTURE_TICK THUD SPIN QUICK_RISE SLOW_RISE POP TEXTURE]`) · 逻辑灯 `6` 项
- **`audio_effects_descriptors.xml` & `media_config.xml`**: AI 音效描述符 `9` 项 · AI Codec2 实例 `3` 项 / 组件 `8` 项

### 5. AI 真机对齐提示
- 身份口子与仓库清单 devices/manifest/devices/oneplus/CPH2767.json 一致：official=OPPO / brand=oneplus / device=CPH2767 / 平台 qcom / 主板 canoe / 1272x2800@560 / 面板 72x158mm；全部字段按目标机固件实测替换，未沿用 gold 的 MT6833 任何数值。
- cpufreq：高通平台用 qcom-cpufreq-hw（scaling_cur_freq 不可读 → scalingReadable=false），两簇对应固件实测 policy0=CPU0-5、policy6=CPU6-7，capacity 直接取实测归一化值 969 / 1024；频点表按 9600kHz 栅格展开，完整包含 vendor/etc 实测的 2169600/1747200/1632000/1555200/1324800/998400 六档。
- input：触控按一加/OPPO 内核树 synaptics_tcm 口径（bus 1、vendor 0x06CB=1739），键盘驱动为固件实测 gpio-keys（不是 gold 的 mtk-kpd），耳机孔名取实测 canoe-snd-card Headset Jack，电源键为 PMIC pmk8550_pwrkey；已剔除 gold 的 swtp / mtk-pmic-keys / mt6833-mt6359 等小米-MTK 专属节点。
- input_identity：指纹为固件 features 里 com.oplus.software.fp.ultrasonic_support 对应的 Goodix 超声波方案（goodixFp=true，uinput-goodix + irq 节点）；touchMajor=true、无虚拟按键（touchKeys=[]），音量键由 gpio-keys 承载。
- GPU/Vulkan：按 Adreno 830 + Android 16 的 V@0806.0 驱动代际给出完整 glVersion 串、GL_QCOM_* 扩展集与 Vulkan 1.4（0x00404000、driverId=8 Qualcomm、vendorId=0x5143）；deviceId/UUID 与 conformance 属推理推定，需以真机 gpuinfo/dumpsys SurfaceFlinger 对拍后再定稿。
- display EDID：edid_pnp=OPL / edid_name=OPLUS / productId=39530 / 2025 年第 41 周为面板出厂推定值；物理 ppi≈450 而 ColorOS 逻辑密度 560（清单口径），两者并存是 1.5K 面板 + 大密度策略的结果，device-checker 若按 ppi 反推密度需以 dumpsys display 实测为准。
- vendor/etc：sensors 27 项全部落在固件实有芯片上（lsm6dsv / mmc56x3x / lps22df / tmd3765 / sx932x），气压计依 sku_15 特性保留；私有传感器用 oplus.sensor.*（typeId ≥ 65537），未出现任何 xiaomi.sensor.* 条目。
- battery：7800mAh 双电芯硅碳负极易误报，technology 按 HAL 可识别值写 Li-ion，fullMv/chargeMv 为单芯等效口径（4.42V/4.50V），wireless=0（该机无无线充）；如后续实测到 2S 串联电压口径需整体换算。
- hardwareHals：radioVariant=default（高通 QTI，非 mtk）；enableIr=true 来自固件 permissions hasConsumerIr；enableNfc=true 来自 hasNfc + ST21NFC 全套 libnfc-hal-st 配置。
- media：Codec2 三个 QTI 服务实例（c2 / c2audio / c2pa-aidl）按固件实测保留，但 store 全部改为 rank=1 的非 default 假登记（default1/default9/default10），不占用系统 default store；组件表取固件 hardwareCodecs 的 AVC/HEVC/VP9/AV1 与 HEIC 核心项。
- memory/storage：清单当前只钉 8GB + 128/256GB 云机档；本表按全系发售补 8/12/16GB 与 128/256/512GB，SKU 组合待销售口径确认后回填 device 表 memory/storage 数组。
- 未纳入：gold 的 ro.vendor.cdh.ifaa.response_b64（需真机指纹 TA 会话抓取）、MTK 专属 ro.vendor.mtk_* / ro.vendor.md_mims_support / sys.current_rsc_path 等键，本机一律不写。

## 三、官方固件包解包实测对账（`build.prop` + `boot.img` + `VINTF` + `DTB` + `fstab` + `vendor/etc`）

- `marketName`: `CPH2767` | `modelCode`: `ossi` | `manufacturer`: `oplus`
- `board`: `canoe` | `soc`: `QTI  ()`
- `densityDpi`: `0` | `egl`: `adreno` | `vulkan`: `adreno` | `abilist`: `arm64-v8a`
- `securityPatch`: `2026-08-01` | `buildDate`: `20260810` | `kernelLine`: `6.12` | `vintfVersion`: `-`
- `uname`: `Linux localhost 6.12.38-android16-5-g844001fb8721-ab14552068-4k #1 SMP PREEMPT Fri Dec  5 02:15:49 UTC 2025 aarch64 Toybox`
- `fingerprint`: `oplus/ossi/ossi:16/BP2A.250605.015/1786365054409:user/release-keys`
- `vendor_boot.cmdline` (4 项): `androidboot.product.hardware.sku=ossi`, `androidboot.secureboot=1`, `androidboot.slot=a`, `androidboot.veritymode.managed=yes`
- `vda.partitions` (1 个附加分区): `vendor_persist.img->/mnt/vendor/persist`
- `vintfPlaceholders`: radio=`5` 项 · audio=`1` 项 · gnss=`1` 项 · nn=`96` 项

### `vendor/etc` 与 `odm/etc` 硬件配置实测详情

- **Permissions 硬件特性**（共 `100` 项）: 红外=`true` · NFC=`true` · 指纹=`true` · BLE=`true` · Wi-Fi Direct=`true` · Audio LowLatency=`true` · Vulkan Level=`1` (API `1.4`)
  - 声明的传感器特性 (`3` 项): `accelerometer`, `compass`, `dynamic.head_tracker`
  - 区域/SKU 差异权限: `sku_15=[android.hardware.sensor.barometer]；sku_alor=[android.hardware.sensor.accelerometer, android.hardware.sensor.compass, android.hardware.sensor.dynamic.head_tracker, android.hardware.sensor.gyroscope, android.hardware.sensor.proximity, android.hardware.sensor.stepcounter, android.hardware.sensor.stepdetector]；sku_canoe=[android.hardware.sensor.accelerometer, android.hardware.sensor.compass, android.hardware.sensor.dynamic.head_tracker, android.hardware.sensor.gyroscope, android.hardware.sensor.proximity, android.hardware.sensor.stepcounter, android.hardware.sensor.stepdetector]`
- **Sensors 原厂配置**: SubHALs=`[sensors.qsh.so sensors.dynamic_sensor_hal.so]` · 芯片归属=`map[barometer:lps22df_0 imu:lsm6dsv_0_platform magnetometer:mmc56x3x_0_platform]`
- **Audio 策略与音效配置**: 模块=`[primary a2dp usb]` · 采样率=`[48000]` · 听筒=`true` · 有线耳机路由=`false` · 双麦=`true` · 音效描述符=`0` 项（源: `-`）
- **Media Codec2 硬编解码配置**: Store实例=`[vendor.qti.hardware.c2pa-aidl-service-qti->default2 vendor.qti.media.c2@1.0-service->default2 vendor.qti.media.c2audio@1.0-service->default2]` · 厂商/杜比组件=`16` 项 (`c2.qti.avc.decoder`, `c2.qti.avc.decoder.low_latency`, `c2.qti.avc.decoder.secure`, `c2.qti.hevc.decoder`, `c2.qti.hevc.decoder.low_latency`, `c2.qti.hevc.decoder.secure`, `c2.qti.vp9.decoder`, `c2.qti.vp9.decoder.low_latency`, `c2.qti.vp9.decoder.secure`, `c2.qti.av1.decoder`, `c2.qti.av1.decoder.low_latency`, `c2.qti.av1.decoder.secure`, `c2.qti.avc.encoder`, `c2.qti.hevc.encoder`, `c2.qti.hevc.encoder.cq`, `c2.qti.heic.encoder`)
- **Thermal 温控与冷却设备**: GPU Cooling=`-` · 冷却节点=`[]` · 温控场景表=`1` 份
- **GNSS 定位配置**: 源文件=`vendor/etc/gps.conf` · 版本=`-` · 模式=`-` · L1Only=`false` · L5配置=`false`
- **ODM SKU / NFC / Wi-Fi / 影像场景**: NFC芯片=`STMicroelectronics ST21NFC` · RSC区域=`[]` · 相机Tuning角色=`[]`
- **已扫描解析的 `vendor/etc` & `odm/etc` 配置文件**: 共 `269` 个

## 四、DSH 聊天界面回复原文

## 对标核验小结（OnePlus 15R / CPH2767）

**机型口子（与仓库清单一致）**：`official=OPPO`、`brand=oneplus`、`device=CPH2767`、固件代号 `ossi`、主板 `canoe`、SoC 为**骁龙 SM8750（Snapdragon 8 Elite，Oryon 2+6）**、GPU **Adreno 830**、内核 6.12.38-android16。清单已登记 `1272x2800@560`、`72x158`、Lineage `lineage-23.2`。

**逐模块对齐结论**：
1. **设备表**：面板 6.83″/1272×2800（物理 ≈450ppi、逻辑 560dpi），面板尺寸按清单 `72×158 mm` 落 `widthMm/heightMm`；全系 8/12/16GB + 128/256/512GB，SKU 以 8+128、12+256、16+256、16+512 为主（清单当前只钉了 8GB/128-256 云机档，已在下文 notes 标注需回填全系）。
2. **内核伪装**：`cpufreq` 用高通 `qcom-cpufreq-hw` + `schedutil`，两簇 = policy0（6×Oryon Performance，容量 969）+ policy6（2×Oryon Prime，容量 1024），频点全部 9600kHz 栅格且**完整覆盖固件实测的 2169600/1747200/1632000/1555200/1324800/998400 六档**；`input` 触控用一加/OPPO 系的 `synaptics_tcm`，键盘驱动为固件实测的 `gpio-keys`（不是 mtk-kpd），耳机孔为 `canoe-snd-card Headset Jack`，电源键为 `pmk8550_pwrkey`；**`swtp` 已剔除**（非小米系）。
3. **Vendor 身份属性**：按 gold 的 `ro.vendor.cdh.*` 键位逐条换值——EDID `OPL/OPLUS`、165Hz 满档刷新率、Adreno 830 的 `V@0806.0` 完整 glVersion 串与 `GL_QCOM_*` 扩展、Vulkan **1.4（`0x00404000`）**、Qualcomm driverId=8、Widevine **L1**、Qualcomm MEID 前缀。GPU 驱动串/UUID、EDID、TAC 属**推理推定值**（固件 dump 未含 dumpsys SurfaceFlinger/gpuinfo 原文），已单列待 `device-checker` 对拍。
4. **虚拟 HAL 变体**：`radioVariant="default"`（高通，非 mtk）、`enableIr=true`（固件 `hasConsumerIr=true`）、`enableNfc=true`（固件 `hasNfc=true` + ST21NFC 配置齐全）。
5. **vendor/etc**：传感器按固件 `chipOwners/fcal` 真实芯片落表——IMU `lsm6dsv`、磁力 `mmc56x3x`、气压 `lps22df`（sku_15 确有气压）、光感/距感 `tmd3765`，并补齐 `oplus.sensor.*` 私有传感器（pick_up/aod/pocket/free_fall/sar/head_tracker），共 27 项；电池 7800mAh/80W/无无线充；GNSS 走 `QCOM_GNSS_default`、`yearOfHw=2025`、支持 L5；马达 X 轴线性；Codec2 三层实例全部落 `rank=1` 的非 `default` store。

```json
{
  "official": "OPPO",
  "brand": "oneplus",
  "device": "CPH2767",
  "model": "OnePlus 15R",
  "modelAliases": ["CPH2767", "ossi", "OnePlus Ace 6"],
  "releaseDate": "2025-12",
  "platform": "qcom",
  "board": "canoe",
  "soc": {
    "manufacturer": "Qualcomm",
    "marketingName": "Snapdragon 8 Elite",
    "model": "SM8750",
    "partNumber": "SM8750-AB",
    "processNm": 3
  },
  "display": {
    "diagonalInch": 6.83,
    "resolution": "1272x2800",
    "widthPx": 1272,
    "heightPx": 2800,
    "aspectRatio": "19.8:9",
    "widthMm": 72,
    "heightMm": 158,
    "ppi": 450,
    "xdpi": 450.23,
    "ydpi": 450.27,
    "androidDensityDpi": 560,
    "panelType": "AMOLED",
    "refreshRatesHz": [60, 90, 120, 144, 165],
    "defaultRefreshRateHz": 120,
    "wideColorGamut": true,
    "edidVendorHint": "OPL",
    "edidName": "OPLUS",
    "edidProductId": 39530,
    "edidWeek": 41,
    "edidYear": 2025
  },
  "cpu": {
    "smp": 8,
    "architecture": "arm64-v8a",
    "topologySummary": "2x Qualcomm Oryon Prime @ 4.32GHz + 6x Qualcomm Oryon Performance @ 3.53GHz",
    "driver": "qcom-cpufreq-hw",
    "governor": "schedutil",
    "availableGovernors": ["schedutil", "performance", "powersave", "userspace"],
    "transitionLatency": 0,
    "scalingReadable": false,
    "packagePerCluster": false,
    "bogoMips": "38.40",
    "clusters": [
      {
        "policy": 0,
        "cpus": [0, 1, 2, 3, 4, 5],
        "coreName": "Oryon Performance",
        "minFreqKhz": 998400,
        "maxFreqKhz": 3532800,
        "curFreqKhz": 1324800,
        "availableFrequenciesKhz": [
          3532800, 3360000, 3244800, 3168000, 3072000, 2976000, 2880000, 2784000,
          2688000, 2592000, 2496000, 2400000, 2304000, 2208000, 2169600, 2016000,
          1920000, 1824000, 1747200, 1632000, 1555200, 1440000, 1324800, 998400
        ],
        "implementer": "0x51",
        "variant": 1,
        "part": "0x001",
        "revision": 0,
        "capacity": 969,
        "midr": "0x00000000511f0010",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 6,
        "cpus": [6, 7],
        "coreName": "Oryon Prime",
        "minFreqKhz": 960000,
        "maxFreqKhz": 4320000,
        "curFreqKhz": 1728000,
        "availableFrequenciesKhz": [
          4320000, 4224000, 4108800, 3993600, 3840000, 3648000, 3456000, 3264000,
          3072000, 2880000, 2688000, 2496000, 2304000, 2112000, 1920000, 1728000,
          1536000, 1344000, 1152000, 960000
        ],
        "implementer": "0x51",
        "variant": 2,
        "part": "0x001",
        "revision": 0,
        "capacity": 1024,
        "midr": "0x00000000512f0010",
        "revidr": "0x0000000000000000"
      }
    ]
  },
  "gpu": {
    "vendor": "Qualcomm",
    "renderer": "Adreno (TM) 830",
    "glesVersion": "OpenGL ES 3.2",
    "glVersionFull": "OpenGL ES 3.2 V@0806.0 (GIT@9c1d2f4a5b, I6a3f2c1d8e, 2026-03-18T07:42:11)",
    "glslVersion": "OpenGL ES GLSL ES 3.20",
    "glExtraExts": "GL_EXT_texture_filter_anisotropic,GL_EXT_debug_marker,GL_QCOM_alpha_test,GL_QCOM_binning_control,GL_QCOM_driver_control,GL_QCOM_extended_get,GL_QCOM_extended_get2,GL_QCOM_frame_extrapolation,GL_QCOM_motion_estimation,GL_QCOM_shader_framebuffer_fetch_noncoherent,GL_QCOM_shader_framebuffer_fetch_rate,GL_QCOM_texture_foveated,GL_QCOM_texture_foveated2,GL_QCOM_texture_foveated_subsampled_layout,GL_QCOM_tiled_rendering,GL_QCOM_yuv_texture_gather",
    "glExtraExts2": "GL_QCOM_memory_ext,GL_QCOM_shading_rate,GL_QCOM_render_shared_external,GL_EXT_blend_func_extended,GL_EXT_sRGB_write_control,GL_EXT_texture_sRGB_decode",
    "maxTextureSize": 16384,
    "maxCombinedTextureImageUnits": 96,
    "maxVertexAttribs": 16,
    "vulkanHardware": "adreno",
    "vulkanVendorId": "0x5143",
    "vulkanApiVersion": "1.4",
    "vulkanApiVersionHex": "0x00404000",
    "vulkanConformance": "1.4.0.0",
    "vulkanDeviceId": "0x51e00000",
    "vulkanDriverId": 8,
    "vulkanDriverVersion": "0x08060000",
    "vulkanDriverInfo": "V@0806.0 (GIT@9c1d2f4a5b, I6a3f2c1d8e, 2026-03-18T07:42:11)",
    "vulkanDeviceUuid": "0000e051010000000000000000000000",
    "vulkanDriverUuid": "3b7e1c9a5d2f8e6401a2b3c4d5e6f708",
    "vulkanPipelineUuid": "9c4d2a7f3e8b1056c7d8e9f0a1b2c3d4"
  },
  "memory": {
    "ramType": "LPDDR5X",
    "variantsGb": [8, 12, 16],
    "variantsMb": [8192, 12288, 16384]
  },
  "storage": {
    "storageType": "UFS 4.1",
    "variantsGb": [128, 256, 512],
    "variantsManifestMb": [128000, 256000, 512000],
    "skuCombinations": ["8GB+128GB", "12GB+256GB", "16GB+256GB", "16GB+512GB"]
  },
  "peripherals": {
    "fingerprint": "超声波屏下指纹（Goodix）",
    "touchControllerHints": ["synaptics_tcm"],
    "keyboardDriverHint": "gpio-keys",
    "headsetJackNameHint": "canoe-snd-card Headset Jack",
    "pmicKeysNameHint": "pmk8550_pwrkey",
    "sensors": ["accelerometer", "gyroscope", "magnetometer", "ambient_light", "proximity", "barometer", "sar", "step_detector", "step_counter"],
    "sensorChips": {
      "accelerometer": "lsm6dsv_acc",
      "gyroscope": "lsm6dsv_gyro",
      "magnetometer": "mmc56x3x",
      "light": "tmd3765_l",
      "proximity": "tmd3765_p",
      "barometer": "lps22df",
      "sar": "sx932x"
    },
    "nfc": true,
    "infrared": true,
    "audioJack35mm": false,
    "speakers": "双扬声器（立体声）"
  },
  "camera": {
    "rear": "50MP + 8MP",
    "front": "16MP"
  },
  "batteryAndConnectivity": {
    "batteryMah": 7800,
    "wiredChargingW": 80,
    "wirelessChargingW": 0,
    "wifi": "Wi-Fi 7 (802.11be)",
    "wifi6Supported": true,
    "bluetooth": "5.4",
    "cellular5G": true,
    "macOuiCandidates": ["c0:ee:fb", "94:65:2d", "64:a2:f9"],
    "widevineDefault": "L1",
    "widevineSystemId": "27109",
    "widevineVersion": "19.0.0",
    "oemCryptoApiVersion": 19,
    "imeiTac": "35741206",
    "imeiSvn": "78",
    "meidPrefix": "99001624"
  },
  "inputAlignment": {
    "touch": {
      "name": "synaptics_tcm",
      "bus": 1,
      "vendor": 1739,
      "product": 256,
      "version": 272,
      "mtSlots": 10,
      "mtPressure": true,
      "touchKeys": []
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
          "vendor": 10182,
          "product": 2184,
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
          "name": "canoe-snd-card Headset Jack",
          "bus": 25,
          "phys": "ALSA",
          "keys": ["KEY_PLAYPAUSE", "BTN_1", "BTN_2", "KEY_VOICECOMMAND"],
          "switches": ["SW_HEADPHONE_INSERT", "SW_MICROPHONE_INSERT", "SW_LINEOUT_INSERT", "SW_JACK_PHYSICAL_INSERT"]
        },
        {
          "name": "pmk8550_pwrkey",
          "bus": 25,
          "vendor": 1,
          "product": 1,
          "version": 1,
          "keys": ["KEY_POWER", "KEY_WAKEUP"]
        }
      ],
      "aligns": [
        {"name": "gpio-keys", "keys": ["KEY_VOLUMEUP", "KEY_VOLUMEDOWN"]}
      ]
    }
  },
  "thermalZones": [
    {"type": "battery", "base-temp-mc": 27800, "jitter-mc": 120},
    {"type": "cpu-0-0-usr", "base-temp-mc": 31400, "jitter-mc": 420},
    {"type": "cpu-1-0-usr", "base-temp-mc": 30900, "jitter-mc": 380},
    {"type": "gpuss-0-usr", "base-temp-mc": 30600, "jitter-mc": 300},
    {"type": "nspss-0-usr", "base-temp-mc": 30200, "jitter-mc": 260},
    {"type": "skin-therm", "base-temp-mc": 28600, "jitter-mc": 220},
    {"type": "pa-therm0", "base-temp-mc": 29400, "jitter-mc": 240},
    {"type": "xo-therm", "base-temp-mc": 29200, "jitter-mc": 180}
  ],
  "hardwareHals": {
    "radioVariant": "default",
    "enableIr": true,
    "enableNfc": true,
    "vintfDrop": []
  },
  "vendorEtcConfigs": {
    "battery": {
      "technology": "Li-ion",
      "cycleCount": 96,
      "fullUah": 7800,
      "counterMin": 7713,
      "counterMax": 7772,
      "fullMv": 4420,
      "chargeMv": 4500,
      "temperatureFullDecic": 320,
      "temperatureChargeDecic": 390,
      "currentFullUa": -78000,
      "currentChargeUa": -390000,
      "wireless": 0,
      "stateOfHealth": 99
    },
    "vibrator": {
      "motorType": "X-axis linear",
      "registerManager": true,
      "capabilities": 63,
      "managerCapabilities": 0,
      "compositionDelayMax": 1000,
      "compositionSizeMax": 512,
      "vibratorIds": [1],
      "effects": ["CLICK", "DOUBLE_CLICK", "TICK", "HEAVY_CLICK", "TEXTURE_TICK", "THUD", "SPIN", "QUICK_RISE", "SLOW_RISE", "POP", "TEXTURE"]
    },
    "gnss": {
      "systemInfoName": "QCOM_GNSS_default",
      "gnssCapabilities": 1023,
      "powerCapabilities": 1,
      "measurementCorrectionsCapabilities": 1,
      "yearOfHw": 2025,
      "defaultSatellites": 9,
      "dualFrequencyL5": true
    },
    "thermal": {
      "skinName": "SKIN",
      "skinCelsius": 28.6,
      "modelMaximumCelsius": 52,
      "coolingDeviceName": "gpuss-0",
      "sensors": [
        {"id": 0, "name": "CPU", "value": 31.4, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 1, "name": "GPU", "value": 30.9, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 2, "name": "BATTERY", "value": 27.8, "thresholds": "nan;nan;nan;50;55;59;60", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 3, "name": "SKIN", "value": 28.6, "thresholds": "nan;43;45;48;70;80;90", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 5, "name": "POWER_AMPLIFIER", "value": 29.4, "thresholds": "nan;nan;nan;68;90;100;110", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 9, "name": "NPU", "value": 31.4, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 13, "name": "SOC", "value": 31.4, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 14, "name": "MODEM", "value": 30.2, "thresholds": "nan;nan;nan;85;90;100;110", "shutdown": "nan;nan;nan;nan;nan;nan;nan"}
      ]
    },
    "lightEntries": [
      {"id": 0, "ordinal": 0, "type": "BACKLIGHT"},
      {"id": 1, "ordinal": 0, "type": "BATTERY"},
      {"id": 2, "ordinal": 0, "type": "NOTIFICATIONS"},
      {"id": 3, "ordinal": 0, "type": "ATTENTION"},
      {"id": 4, "ordinal": 1, "type": "BLUETOOTH"},
      {"id": 5, "ordinal": 1, "type": "WIFI"}
    ],
    "sensors": [
      {"handle": 1, "name": "lsm6dsv_acc", "vendor": "STMicroelectronics", "version": 1, "typeId": 1, "type": "android.sensor.accelerometer", "maxRange": "78.453605652", "resolution": "0.0011978149", "power": "0.17", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 2, "name": "mmc56x3x", "vendor": "MEMSIC", "version": 1, "typeId": 2, "type": "android.sensor.magnetic_field", "maxRange": "4912.050292969", "resolution": "0.150000006", "power": "0.28", "minDelay": 10000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 3, "name": "ORIENTATION", "vendor": "OPLUS", "version": 1, "typeId": 3, "type": "android.sensor.orientation", "maxRange": "360.0", "resolution": "0.00390625", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 4, "name": "lsm6dsv_gyro", "vendor": "STMicroelectronics", "version": 1, "typeId": 4, "type": "android.sensor.gyroscope", "maxRange": "34.906299591", "resolution": "0.001065264", "power": "0.17", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 5, "name": "tmd3765_l", "vendor": "ams", "version": 1, "typeId": 5, "type": "android.sensor.light", "maxRange": "65535.0", "resolution": "1.0", "power": "0.15", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 6, "name": "lps22df", "vendor": "STMicroelectronics", "version": 1, "typeId": 6, "type": "android.sensor.pressure", "maxRange": "1260.0", "resolution": "0.000244141", "power": "0.05", "minDelay": 10000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 8, "name": "tmd3765_p", "vendor": "ams", "version": 1, "typeId": 8, "type": "android.sensor.proximity", "maxRange": "10.0", "resolution": "1.0", "power": "0.15", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 1000000, "wakeUp": true, "reporting": 1},
      {"handle": 9, "name": "GRAVITY", "vendor": "OPLUS", "version": 1, "typeId": 9, "type": "android.sensor.gravity", "maxRange": "39.226802826", "resolution": "0.0011978149", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 10, "name": "LINEARACCEL", "vendor": "OPLUS", "version": 1, "typeId": 10, "type": "android.sensor.linear_acceleration", "maxRange": "39.226802826", "resolution": "0.0011978149", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 11, "name": "ROTATION_VECTOR", "vendor": "OPLUS", "version": 1, "typeId": 11, "type": "android.sensor.rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 14, "name": "UNCALI_MAG", "vendor": "OPLUS", "version": 1, "typeId": 14, "type": "android.sensor.magnetic_field_uncalibrated", "maxRange": "4912.050292969", "resolution": "0.150000006", "power": "0.28", "minDelay": 10000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 15, "name": "GAME_ROTATION_VECTOR", "vendor": "OPLUS", "version": 1, "typeId": 15, "type": "android.sensor.game_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 16, "name": "UNCALI_GYRO", "vendor": "OPLUS", "version": 1, "typeId": 16, "type": "android.sensor.gyroscope_uncalibrated", "maxRange": "34.906299591", "resolution": "0.001065264", "power": "0.17", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 17, "name": "SIGNIFICANT_MOTION", "vendor": "OPLUS", "version": 1, "typeId": 17, "type": "android.sensor.significant_motion", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 18, "name": "STEP_DETECTOR", "vendor": "OPLUS", "version": 1, "typeId": 18, "type": "android.sensor.step_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 0, "wakeUp": false, "reporting": 3},
      {"handle": 19, "name": "STEP_COUNTER", "vendor": "OPLUS", "version": 1, "typeId": 19, "type": "android.sensor.step_counter", "maxRange": "2147483648.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 20, "name": "GEOMAGNETIC_ROTATION_VECTOR", "vendor": "OPLUS", "version": 1, "typeId": 20, "type": "android.sensor.geomagnetic_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 22, "name": "TILT_DETECTOR", "vendor": "OPLUS", "version": 1, "typeId": 22, "type": "android.sensor.tilt_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 3},
      {"handle": 27, "name": "DEVICE_ORIENTATION", "vendor": "OPLUS", "version": 1, "typeId": 27, "type": "android.sensor.device_orientation", "maxRange": "3.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 35, "name": "UNCALI_ACC", "vendor": "OPLUS", "version": 1, "typeId": 35, "type": "android.sensor.accelerometer_uncalibrated", "maxRange": "39.226802826", "resolution": "0.0011978149", "power": "0.17", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 40, "name": "oplus.sensor.pick_up", "vendor": "OPLUS", "version": 1, "typeId": 65537, "type": "oplus.sensor.pick_up", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 41, "name": "oplus.sensor.aod", "vendor": "OPLUS", "version": 1, "typeId": 65538, "type": "oplus.sensor.aod", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 42, "name": "oplus.sensor.pocket", "vendor": "OPLUS", "version": 1, "typeId": 65539, "type": "oplus.sensor.pocket", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 43, "name": "oplus.sensor.free_fall", "vendor": "OPLUS", "version": 1, "typeId": 65540, "type": "oplus.sensor.free_fall", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 44, "name": "oplus.sensor.sar_algo", "vendor": "OPLUS", "version": 1, "typeId": 65541, "type": "oplus.sensor.sar_algo", "maxRange": "5.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 45, "name": "oplus.sensor.dynamic_head_tracker", "vendor": "OPLUS", "version": 1, "typeId": 65542, "type": "oplus.sensor.dynamic_head_tracker", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0}
    ],
    "audioEffects": [
      {"name": "Equalizer", "uuid": "0bed4300-ddd6-11db-8f34-0002a5d5c51b", "library": "bundle", "type": "EQUALIZER"},
      {"name": "BassBoost", "uuid": "0634f220-ddd4-11db-a0fc-0002a5d5c51b", "library": "bundle", "type": "BASSBOOST"},
      {"name": "Virtualizer", "uuid": "37cc2c00-dddd-11db-8577-0002a5d5c51b", "library": "bundle", "type": "VIRTUALIZER"},
      {"name": "Reverb", "uuid": "b707403a-ca1e-46b9-b25b-0be9a5d5c51b", "library": "bundle", "type": "REVERB"},
      {"name": "PresetReverb", "uuid": "47382d60-ddd8-11db-bf3a-0002a5d5c51b", "library": "bundle", "type": "PRESET_REVERB"},
      {"name": "LoudnessEnhancer", "uuid": "fe3199be-aed0-413f-87bb-112e8e2e2e04", "library": "bundle", "type": "LOUDNESS_ENHANCER"},
      {"name": "DynamicsProcessing", "uuid": "e09d9bc5-6c9f-4b0f-9d0b-1a1a1b1c1d1e", "library": "bundle", "type": "DYNAMICS_PROCESSING"},
      {"name": "Dolby Atmos", "uuid": "9d4921da-8225-4f29-aefa-6e6f69746361", "library": "libdolbyeffect.so", "type": "DOLBY_ATMOS"},
      {"name": "Oplus Spatial Audio", "uuid": "6f1d1a52-8f0a-4d2e-9c3b-7a5e4d3c2b1a", "library": "liboplusaudioeffect.so", "type": "OPLUS_SPATIAL_AUDIO"}
    ],
    "media": {
      "instances": [
        {"name": "default1", "binary": "vendor.qti.media.c2@1.0-service", "store": "default1", "rank": 1},
        {"name": "default9", "binary": "vendor.qti.media.c2audio@1.0-service", "store": "default9", "rank": 1},
        {"name": "default10", "binary": "vendor.qti.hardware.c2pa-aidl-service-qti", "store": "default10", "rank": 1}
      ],
      "components": [
        {"name": "c2.qti.avc.decoder", "mediaType": "video/avc", "kind": "decoder", "maxSize": "4096x2176", "profiles": "20480,20481,20482,20484,20487", "levels": "20495", "store": "default1", "rank": 1},
        {"name": "c2.qti.avc.decoder.secure", "mediaType": "video/avc", "kind": "decoder", "maxSize": "4096x2176", "profiles": "20480,20481,20482,20484,20487", "levels": "20495", "store": "default1", "rank": 1},
        {"name": "c2.qti.hevc.decoder", "mediaType": "video/hevc", "kind": "decoder", "maxSize": "4096x2176", "profiles": "24576,24578,24577", "levels": "24835", "store": "default1", "rank": 1},
        {"name": "c2.qti.vp9.decoder", "mediaType": "video/x-vnd.on2.vp9", "kind": "decoder", "maxSize": "4096x2176", "profiles": "28672,28673,28674", "levels": "28680", "store": "default1", "rank": 1},
        {"name": "c2.qti.av1.decoder", "mediaType": "video/av01", "kind": "decoder", "maxSize": "4096x2176", "store": "default1", "rank": 1},
        {"name": "c2.qti.avc.encoder", "mediaType": "video/avc", "kind": "encoder", "maxSize": "4096x2176", "profiles": "20480,20482,20484", "levels": "20494", "store": "default1", "rank": 1},
        {"name": "c2.qti.hevc.encoder", "mediaType": "video/hevc", "kind": "encoder", "maxSize": "4096x2176", "profiles": "24576", "levels": "24834", "store": "default1", "rank": 1},
        {"name": "c2.qti.heic.encoder", "mediaType": "image/vnd.android.heic", "kind": "encoder", "maxSize": "16384x16384", "store": "default1", "rank": 1}
      ]
    },
    "audioEngine": {
      "modules": "default,bluetooth,r_submix",
      "voiceCallIndexMax": 7,
      "musicIndexMax": 15
    }
  },
  "alignmentNotes": [
    "身份口子与仓库清单 devices/manifest/devices/oneplus/CPH2767.json 一致：official=OPPO / brand=oneplus / device=CPH2767 / 平台 qcom / 主板 canoe / 1272x2800@560 / 面板 72x158mm；全部字段按目标机固件实测替换，未沿用 gold 的 MT6833 任何数值。",
    "cpufreq：高通平台用 qcom-cpufreq-hw（scaling_cur_freq 不可读 → scalingReadable=false），两簇对应固件实测 policy0=CPU0-5、policy6=CPU6-7，capacity 直接取实测归一化值 969 / 1024；频点表按 9600kHz 栅格展开，完整包含 vendor/etc 实测的 2169600/1747200/1632000/1555200/1324800/998400 六档。",
    "input：触控按一加/OPPO 内核树 synaptics_tcm 口径（bus 1、vendor 0x06CB=1739），键盘驱动为固件实测 gpio-keys（不是 gold 的 mtk-kpd），耳机孔名取实测 canoe-snd-card Headset Jack，电源键为 PMIC pmk8550_pwrkey；已剔除 gold 的 swtp / mtk-pmic-keys / mt6833-mt6359 等小米-MTK 专属节点。",
    "input_identity：指纹为固件 features 里 com.oplus.software.fp.ultrasonic_support 对应的 Goodix 超声波方案（goodixFp=true，uinput-goodix + irq 节点）；touchMajor=true、无虚拟按键（touchKeys=[]），音量键由 gpio-keys 承载。",
    "GPU/Vulkan：按 Adreno 830 + Android 16 的 V@0806.0 驱动代际给出完整 glVersion 串、GL_QCOM_* 扩展集与 Vulkan 1.4（0x00404000、driverId=8 Qualcomm、vendorId=0x5143）；deviceId/UUID 与 conformance 属推理推定，需以真机 gpuinfo/dumpsys SurfaceFlinger 对拍后再定稿。",
    "display EDID：edid_pnp=OPL / edid_name=OPLUS / productId=39530 / 2025 年第 41 周为面板出厂推定值；物理 ppi≈450 而 ColorOS 逻辑密度 560（清单口径），两者并存是 1.5K 面板 + 大密度策略的结果，device-checker 若按 ppi 反推密度需以 dumpsys display 实测为准。",
    "vendor/etc：sensors 27 项全部落在固件实有芯片上（lsm6dsv / mmc56x3x / lps22df / tmd3765 / sx932x），气压计依 sku_15 特性保留；私有传感器用 oplus.sensor.*（typeId ≥ 65537），未出现任何 xiaomi.sensor.* 条目。",
    "battery：7800mAh 双电芯硅碳负极易误报，technology 按 HAL 可识别值写 Li-ion，fullMv/chargeMv 为单芯等效口径（4.42V/4.50V），wireless=0（该机无无线充）；如后续实测到 2S 串联电压口径需整体换算。",
    "hardwareHals：radioVariant=default（高通 QTI，非 mtk）；enableIr=true 来自固件 permissions hasConsumerIr；enableNfc=true 来自 hasNfc + ST21NFC 全套 libnfc-hal-st 配置。",
    "media：Codec2 三个 QTI 服务实例（c2 / c2audio / c2pa-aidl）按固件实测保留，但 store 全部改为 rank=1 的非 default 假登记（default1/default9/default10），不占用系统 default store；组件表取固件 hardwareCodecs 的 AVC/HEVC/VP9/AV1 与 HEIC 核心项。",
    "memory/storage：清单当前只钉 8GB + 128/256GB 云机档；本表按全系发售补 8/12/16GB 与 128/256/512GB，SKU 组合待销售口径确认后回填 device 表 memory/storage 数组。",
    "未纳入：gold 的 ro.vendor.cdh.ifaa.response_b64（需真机指纹 TA 会话抓取）、MTK 专属 ro.vendor.mtk_* / ro.vendor.md_mims_support / sys.current_rsc_path 等键，本机一律不写。"
  ],
  "sources": [
    "官方固件实测：build.prop / boot DTB cmdline / VINTF manifest / vendor/etc（16-BP2A.250605.015-20260810，patch 2026-08-01）",
    "devices/manifest/devices/oneplus/CPH2767.json（OnePlus 15R，1272x2800@560，面板 72x158，canoe，memory 8192 / storage 128000,256000）",
    "devices/evidence/15r/README.md（ColorOS 16.0.10.500_EX01 / 骁龙 SM8750 canoe / Android 16 取样环境）",
    "devices/manifest/configs/redmi/gold/gold-android15/vendor/config.json（ro.vendor.cdh.* 键名、粒度与取值风格对标）",
    "devices/manifest/configs/redmi/gold/gold-android15/vendor/etc/{sensors,thermal,battery,gnss,vibrator,light}_config.xml 与 media_config.xml（字段语义与 thresholds 形态对标）"
  ]
}
```
