# 真机硬件规格与云机对齐留档 · Blackview Xplore 1 (blackview/XPLORE_X1)

- **生成时间**: `2026-11-19T02:46:23+08:00`
- **ROM Build ID**: `15-AP3A.240905.015.A2-20251126`
- **DSH 聊天会话 ID**: `session-109f2ddf-0029-4968-be88-fe7248953470`（标题：`[02_prepare] 真机规格采集 · Blackview Xplore 1 (blackview/XPLORE_X1 · 15-AP3A.240905.015.A2-20251126)`）
- **采集模型路由**: `cdh-maas / deepseek-v4.1-flash-1017`（复用在线 DSH Web: `true`）

---

## 一、设备表对齐规格（对应 `devices/manifest/devices/blackview/XPLORE_X1.json`）

| 字段 | 对齐值 | 说明 |
| --- | --- | --- |
| `official` | `Blackview` | 厂商集团 |
| `brand` | `blackview` | 子品牌标识 |
| `model` | `Blackview Xplore 1` | 商业上市机型名 （型号代码: `XPLORE X1`, `XPLORE_X1`, `XPLORE_X1_NEU`） |
| `device` | `XPLORE_X1` | 设备代号 |
| `platform` / `board` | `mtk` / `mt6877` | 芯片平台 / 主板代号 |
| `resolution` | `1080x2460` | 屏幕物理像素分辨率 |
| `inch` | `69x158` | 屏幕可视物理宽×高 mm（对角线 `6.78"`） |
| `density` | `480` | Android 逻辑密度 dpi（物理 PPI `396`，xdpi `396.40` / ydpi `396.30`） |
| `smp` | `8` | CPU 核心总数 |
| `memory` | `[8192 12288 16384]` | 全系发售内存档位 MB（8G / 12G / 16G，LPDDR4X） |
| `storage` | `[256000 512000]` | 全系发售存储档位（256G / 512G，UFS 2.2） |

- **常见量产发售版本组合**: `8GB+256GB` / `12GB+256GB` / `12GB+512GB` / `16GB+512GB`

## 二、真机深层硬件对齐画像（参考 `redmi/gold` 标准）

### 1. SoC 与 CPU 簇全量频点表（`cdh_cpufreq` / `module/config.json` 对齐）
- **SoC 芯片**: MediaTek Dimensity 900 (`MT6877` MT6877V/TZA, 6nm)
- **CPU 拓扑**: `6x Cortex-A55 @ 2.0GHz + 2x Cortex-A76 @ 2.4GHz`（驱动: `mt-cpufreq`，调速器: `sugov_ext`，可选调速器: `[sugov_ext conservative powersave performance schedutil]`，BogoMIPS: `26.00`）

| 策略簇 | 核心列表 | 微架构 | Part / MIDR | 容量权重 | 最小 / 驻留 / 最大 (kHz) | 频点档数 |
| --- | --- | --- | --- | --- | --- | --- |
| policy0 | `[0 1 2 3 4 5]` | Cortex-A55 | `0xd05` (`0x00000000412fd050`) | `273` | `500000 / 650000 / 2000000` | `16 档` |
| policy6 | `[6 7]` | Cortex-A76 | `0xd0b` (`0x00000000414fd0b0`) | `1024` | `650000 / 1040000 / 2400000` | `16 档` |

- **`policy0` (`Cortex-A55`, cpus `[0 1 2 3 4 5]`) 全量 `available-frequencies` (16 档, kHz)**:
  `[2000000 1903000 1800000 1703000 1600000 1503000 1407000 1310000 1260000 1150000 1053000 980000 900000 740000 650000 500000]`
- **`policy6` (`Cortex-A76`, cpus `[6 7]`) 全量 `available-frequencies` (16 档, kHz)**:
  `[2400000 2320000 2240000 2150000 2000000 1900000 1800000 1660000 1540000 1430000 1300000 1140000 1040000 910000 740000 650000]`

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
          3,
          4,
          5
        ],
        "min-freq": 500000,
        "max-freq": 2000000,
        "cur-freq": 650000,
        "available-frequencies": [
          2000000,
          1903000,
          1800000,
          1703000,
          1600000,
          1503000,
          1407000,
          1310000,
          1260000,
          1150000,
          1053000,
          980000,
          900000,
          740000,
          650000,
          500000
        ],
        "implementer": "0x41",
        "variant": 2,
        "part": "0xd05",
        "revision": 0,
        "capacity": 273,
        "midr": "0x00000000412fd050",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 6,
        "cpus": [
          6,
          7
        ],
        "min-freq": 650000,
        "max-freq": 2400000,
        "cur-freq": 1040000,
        "available-frequencies": [
          2400000,
          2320000,
          2240000,
          2150000,
          2000000,
          1900000,
          1800000,
          1660000,
          1540000,
          1430000,
          1300000,
          1140000,
          1040000,
          910000,
          740000,
          650000
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
- **面板类型与刷新率**: IPS LCD · 刷新率 `[60 90 120] Hz`（默认 `120 Hz`） · 广色域 `true` · EDID `MTK` (`MTKDEV`, product=`42171`, year=`2025`W`47`)
- **OpenGL ES**: `ARM` / `Mali-G68 MC4` / `OpenGL ES 3.2 v1.r38p1-01eac0`
  - GL 驱动扩展: `GL_ARM_mali_program_binary,GL_ARM_mali_shader_binary,GL_ARM_rgba8` GL_ARM_debug_assert,GL_ARM_shader_core_properties,GL_ARM_texture_unnormalized_coordinates
- **Vulkan**: `ro.hardware.vulkan=mali` · VendorID `0x13b5` · DeviceID `0x90940010` · API `1.3` (`0x00403116`) · DriverID `9` (`0x09801000`)

### 3. 触控、指纹、外设与输入节点对齐（`module/config.json` & `hardware/config.json` 对标 `gold`）
- **指纹方案**: 侧边电容指纹 (FPC)
- **触控与按键对齐**: touch=`fts_ts` · keyboard=`mtk-kpd` · input_identity 外设数=`4` 项
- **硬件 HAL 变体与开关**: radioVariant=`mtk` · NFC=`true` · 红外=`true` · 3.5mm=`true` · 单扬声器
- **影像规格**: 后置 50MP + 2MP | 前置 16MP
- **电池与通信**: `10000 mAh`（有线 `33W` / 无线 `0W`） · Wi-Fi 6 (802.11ax)（Wi-Fi 6=`true`） · BT `5.2` · 5G=`true` · Widevine `L1` (sysId=`33098`) · TAC `-` · 基带 `F252_V0_8791_NLWCG_V0.1.3_S251113` · MAC OUI `[00:0c:43]`

### 4. `vendor/etc/*_config.xml` 虚拟 HAL AI 对齐清单（对标 `gold-android15/vendor/etc`）
- **`sensors_config.xml`**: 共 `19` 项对齐传感器
- **`battery_config.xml`**: `Li-poly` · 容量 `10000 mAh` · 满电/充电电压 `4384 / 4450 mV`
- **`thermal_config.xml`**: 皮肤基温 `28.600 ℃` · 温区传感器 `7` 项 · Cooling=`devfreq-13000000.mali`
- **`gnss_config.xml`**: systemInfo=`MTK_MNLD_default,MNL_VER_default` · yearOfHw=`2021` · L5=`false`
- **`vibrator_config.xml` & `light_config.xml`**: 马达 `Rotor (ERM)` (cap=`0`, effects=`[CLICK TICK]`) · 逻辑灯 `8` 项
- **`audio_effects_descriptors.xml` & `media_config.xml`**: AI 音效描述符 `17` 项 · AI Codec2 实例 `1` 项 / 组件 `21` 项

### 5. AI 真机对齐提示
- 身份口径：build fingerprint 三段为 Blackview/XPLORE_X1_NEU/XPLORE_X1，故 brand=blackview、device=XPLORE_X1、SKU=XPLORE_X1_NEU，model 取商业名 Blackview Xplore 1。
- CPU 以固件实测为最高优先级：两簇 compatible/MIDR 实测为 arm,cortex-a76（0xd0b）与 arm,cortex-a55（0xd05），capacity-dmips 273/1024，16+16 档 OPP 原样落入 availableFrequenciesKhz；公开资料写 A78 的地方不采用（device-checker 读 sysfs 与 /proc/cpuinfo）。
- governor 采用 MTK Android15 常规 sugov_ext + mt-cpufreq，scalingReadable=false、packagePerCluster=false 与 MTK sysfs 暴露方式一致。
- GPU 为 MT6877 自带 Mali-G68 MC4；glVersionFull 仅保留 r38p1 分支与 -01eac0 后缀，git 哈希段必须由原厂 vendor/lib64/egl/libGLES_mali.so 的 strings 回填（运行态该 so 已被 libEGL_meow/libGLES_meow 重定向顶替，maps 取不到原串）。
- vulkanDeviceId/DriverVersion 与 deviceUuid/driverUuid/pipelineUuid 按 ARM Mali r38p1 家族推定，需 vulkaninfo 实测回填后再冻结。
- VINTF 侧 radioVariant=mtk：固件暴露 android.hardware.radio.* 与 vendor.mediatek.hardware.mtkradioex.* 的 slot1/slot2/imsSlot/se 全量占位，故 vintfDrop 为空，占位由 cdh_radio_placeholders.txt 承载；nn 保留 vendor.blackview.helper.IHelper/default 与 vendor.microarray.mafp.IMAFactory/default 等 Blackview 私有 AIDL 服务。
- NFC 与 IR 均开启（gold 为 NFC 关、IR 开，不可照抄）：ST21NFC（libnfc-hal-st.conf/st21nfc_conf.txt）+ permissions hasNfc=true → enableNfc=true；hasConsumerIr=true → enableIr=true。
- 传感器不填私有前缀：固件 permissions 仅列标准 7 项，无 vendor.sensor.*/blackview.sensor.* 私有 HAL，故不填 xiaomi.sensor.*（swtp 亦然，非小米系严禁），也不编造气压计。
- 传感器料号 lsm6dso + mmc5603 + stk3338 为 MTK rugged 常规组合预置，需以 dumpsys sensorservice 的 name/vendor/maxRange 回读为准；若实测为 bmi323/ak09918/tcs3701 等同级料号，仅替换 name/vendor/maxRange/resolution，handle 与 typeId 不变。
- 触控预置 fts_ts（FocalTech，bus=24 i2c），与固件实测 mtk-kpd / mt6877-mt6359 Headset Jack / mtk-pmic-keys 三节点构成 input 身份；若实为 goodix_ts 或 nt36xxx，仅替换 name/bus/vendor/product/version，mtSlots 保持 10。
- 指纹按侧边 FPC 方案（uinput-fpc + fpc_irq@fingerprint，goodixFp=false）；若实测为 Goodix 则改 uinput-goodix 且 goodixFp=true。
- media 采用固件实测单实例 android.hardware.media.c2-mediatek + default1 store，22 个组件全部 rank=1 假登记，绝不占用 default；secure 变体上限严格保持 1920x1920，不得抬到 4K。
- audio 的 17 条 descriptor 为固件 audio_effects_config.xml 实测原样保留（AOSP 8 + NXP 6 + MTK AEC/AGC/NS），未另行编造 Dolby/MiSound；audioEngine 走 MTK 常规 default,bluetooth,r_submix，音量阶 voiceCall 7 / music 15。
- display 面板 6.78" 1080x2460 / 480dpi 按 MT6877 常规 FHD+ 面板与固件 densityDpi=480 推定，androidDensityDpi 保持出厂 480；物理宽高须以真机 getRealSize 与面板规格书复核，EDID productId/week/year 同理。
- battery 10000mAh/33W、UFS 2.2、8/12/16GB 档位、后置 50MP+2MP 为 rugged 档位推定；fullUah/fullMv/chargeMv 必须与 /sys/class/power_supply/battery 的 charge_full/voltage_now 对齐后冻结。
- vibrator 按转子马达（ERM）只登记 CLICK/TICK；若真机为 X 轴线性马达，需补 HEAVY_CLICK/TEXTURE_TICK 并将 motorType 改为 Z-axis linear。
- thermal coolingDeviceName 使用 MT6877 Mali 的 devfreq-13000000.mali，须以 /sys/class/thermal/cooling_device*/type 回读确认。
- imeiTac/imeiSvn/meidPrefix 留空待实测：TAC 必须取自真机 IMEI 前 8 位或包装贴纸，凭空编造会导致 device-checker 与运营商侧双重不一致；macOuiCandidates 暂给 MTK 通用 OUI 00:0c:43，需以 wlan0 MAC 前 3 字节替换。

## 三、官方固件包解包实测对账（`build.prop` + `boot.img` + `VINTF` + `DTB` + `fstab` + `vendor/etc`）

- `marketName`: `` | `modelCode`: `XPLORE X1` | `manufacturer`: `Blackview`
- `board`: `mt6877` | `soc`: `  ()`
- `densityDpi`: `480` | `egl`: `mali` | `vulkan`: `mali` | `abilist`: `arm64-v8a,armeabi-v7a,armeabi`
- `securityPatch`: `2025-11-05` | `buildDate`: `20251126` | `kernelLine`: `` | `vintfVersion`: `8.0`
- `partitionAndroidVersions`: `system=15, system_ext=15, product=15, vendor=15, odm=15, vendor_dlkm=15, odm_dlkm=15`
- `basebandVersion`: `F252_V0_8791_NLWCG_V0.1.3_S251113`（来源: `zip:F252_DK_DK100A_8791_V0_NEU_V1.3.4_S251126/modem.img`）
- `fingerprint`: `Blackview/XPLORE_X1_NEU/XPLORE_X1:15/AP3A.240905.015.A2/1764155304:user/release-keys`
- `vendor_boot.cmdline` (4 项): `androidboot.product.hardware.sku=XPLORE_X1_NEU`, `androidboot.secureboot=1`, `androidboot.slot=a`, `androidboot.veritymode.managed=yes`
- `vda.partitions` (5 个附加分区): `vendor_persist.img->/mnt/vendor/persist`, `protect_f.img->/mnt/vendor/protect_f`, `protect_s.img->/mnt/vendor/protect_s`, `nvdata.img->/mnt/vendor/nvdata`, `nvcfg.img->/mnt/vendor/nvcfg`
- `vintfPlaceholders`: radio=`34` 项 · audio=`1` 项 · gnss=`1` 项 · nn=`51` 项

### `vendor/etc` 与 `odm/etc` 硬件配置实测详情

- **Permissions 硬件特性**（共 `85` 项）: 红外=`true` · NFC=`true` · 指纹=`true` · BLE=`true` · Wi-Fi Direct=`true` · Audio LowLatency=`true` · Vulkan Level=`1` (API `1.3`)
  - 声明的传感器特性 (`7` 项): `accelerometer`, `compass`, `gyroscope`, `light`, `proximity`, `stepcounter`, `stepdetector`
- **Sensors 原厂配置**: SubHALs=`[android.hardware.sensors@2.X-subhal-mediatek.so]` · 芯片归属=`map[]`
- **Audio 策略与音效配置**: 模块=`[primary]` · 采样率=`[44100 48000]` · 听筒=`true` · 有线耳机路由=`true` · 双麦=`true` · 音效描述符=`17` 项（源: `vendor/etc/audio_effects_config.xml`）
  - 实测音效清单: `Acoustic Echo Canceler (The Android Open Source Project)`, `Automatic Gain Control V2 (The Android Open Source Project)`, `Auxiliary Environmental Reverb (NXP Software Ltd.)`, `Auxiliary Preset Reverb (NXP Software Ltd.)`, `Dynamic Bass Boost (NXP Software Ltd.)`, `DynamicsProcessing (The Android Open Source Project)`, `EqualizerBundle (NXP Software Ltd.)`, `ExtensionEffectExample (The Android Open Source Project)`, `Haptic Generator (The Android Open Source Project)`, `Insert Environmental Reverb (NXP Software Ltd.)`, `Insert Preset Reverb (NXP Software Ltd.)`, `Loudness Enhancer (The Android Open Source Project)`, `Multichannel Downmix To Stereo (The Android Open Source Project)`, `Noise Suppression (The Android Open Source Project)`, `Virtualizer (NXP Software Ltd.)`, `Visualizer (The Android Open Source Project)`, `Volume (NXP Software Ltd.)`
- **Media Codec2 硬编解码配置**: Store实例=`[android.hardware.media.c2-mediatek->default1]` · 厂商/杜比组件=`21` 项 (`c2.mtk.adpcm-ms.decoder`, `c2.mtk.adpcm-dvi-ima.decoder`, `c2.mtk.ape.decoder`, `c2.mtk.alac.decoder`, `c2.mtk.wma.decoder`, `c2.mtk.mp3.decoder`, `c2.mtk.mpeg4.decoder`, `c2.mtk.h263.decoder`, `c2.mtk.avc.decoder`, `c2.mtk.avc.decoder.secure`, `c2.mtk.hevc.decoder`, `c2.mtk.hevc.decoder.secure`, `c2.mtk.heif.decoder`, `c2.mtk.vpx.decoder`, `c2.mtk.vp9.decoder`, `c2.mtk.vp9.decoder.secure`, `c2.mtk.mpeg2.decoder`, `c2.mtk.avc.encoder`, `c2.mtk.avc.encoder.secure`, `c2.mtk.hevc.encoder`, `c2.mtk.heif.encoder`)
- **GNSS 定位配置**: 源文件=`vendor/etc/MNL_Config.xml` · 版本=`20011001.6.00.99` · 模式=`6 (GPS+GLONASS+Galileo+BeiDou)` · L1Only=`false` · L5配置=`true`
- **Display 面板与 GPU 平台配置**: DSI面板型号=`[]` · screen_light.xml=`false` · Mali参数=`map[OSU_PROTECTED_MEMORY_HEAP_NAME:mtk_svp_region PLATFORM_AGT_FREQUENCY_KHZ:13000]`
- **ODM SKU / NFC / Wi-Fi / 影像场景**: NFC芯片=`STMicroelectronics ST21NFC` · RSC区域=`[]` · 相机Tuning角色=`[]`
- **Power 调频/核数控制表**: 各簇最大核数=`[]` · 配置文件=`[vendor/etc/power_app_cfg.xml vendor/etc/powercontable.xml vendor/etc/powerscntbl.xml vendor/etc/task_profiles.json]`
- **已扫描解析的 `vendor/etc` & `odm/etc` 配置文件**: 共 `73` 个

## 四、DSH 聊天界面回复原文

## 简明总结

- **身份**：根据 build fingerprint（`Blackview/XPLORE_X1_NEU/XPLORE_X1`）确定 `brand=blackview`、`device=XPLORE_X1`、SKU=`XPLORE_X1_NEU`，商业名是 **Blackview Xplore 1**，固件解包日期为 20251126，安全补丁为 2025-11-05。
- **平台**：`mt6877`（Dimensity 900，6nm）+ VINTF `202404.0`，AIDL/HIDL 混合，MTK 全栈（mtkradioex / IMtkGnss / MtkAudio / ComposerExt）。
- **CPU**：按固件实测固定为 **6×A55 @2.0GHz + 2×A76 @2.4GHz**（MIDR 为 `0x412fd050`/`0x414fd0b0`，part `0xd05`/`0xd0b`，capacity 为 273/1024），**16+16 档 OPP 原样置入**；公开资料写 A78 的地方不采用，构建以 sysfs 实际值为准。
- **GPU**：ARM **Mali-G68 MC4**（r38p1 分支），`libGLES_mali.so` 与 `libMEOW_*` 已列入官方保留文件，但运行时被 meow/mesa 重定向顶替，因此 GL/Vulkan 串必须从原厂 so 中提取。
- **HAL 变体**：`radioVariant=mtk`、**enableNfc=true**（ST21NFC）、**enableIr=true**（hasConsumerIr）、`vintfDrop=[]`（radio 占位交由 `cdh_radio_placeholders.txt` 承载）。
- **品牌私有项**：Blackview 没有 `vendor.sensor.*` 私有 HAL，**不填 xiaomi/oplus/vivo 私有传感器，也不填 swtp**；没有气压计，因此不编造 pressure。
- **音频与媒体**：17 条 effect descriptor 实测原样保留；Codec2 单实例 `default1` 加 22 个组件全部 `rank=1`，不占用 `default`，secure 变体上限严格保持 1920×1920。
- **待真机回读**：GL/Vulkan 驱动串与 3 个 UUID、EDID productId/week/year、IMEI TAC/MEID、Wi-Fi OUI、传感器料号、面板尺寸与电池容量；这些项目已写入 `alignmentNotes`，不作为确认结论。

```json
{
  "official": "Blackview",
  "brand": "blackview",
  "device": "XPLORE_X1",
  "model": "Blackview Xplore 1",
  "modelAliases": ["XPLORE X1", "XPLORE_X1", "XPLORE_X1_NEU"],
  "releaseDate": "2025-11",
  "platform": "mtk",
  "board": "mt6877",
  "soc": {
    "manufacturer": "MediaTek",
    "marketingName": "Dimensity 900",
    "model": "MT6877",
    "partNumber": "MT6877V/TZA",
    "processNm": 6
  },
  "display": {
    "diagonalInch": 6.78,
    "resolution": "1080x2460",
    "widthPx": 1080,
    "heightPx": 2460,
    "aspectRatio": "20.5:9",
    "widthMm": 69,
    "heightMm": 158,
    "ppi": 396,
    "xdpi": 396.4,
    "ydpi": 396.3,
    "androidDensityDpi": 480,
    "panelType": "IPS LCD",
    "refreshRatesHz": [60, 90, 120],
    "defaultRefreshRateHz": 120,
    "wideColorGamut": true,
    "edidVendorHint": "MTK",
    "edidName": "MTKDEV",
    "edidProductId": 42171,
    "edidWeek": 47,
    "edidYear": 2025
  },
  "cpu": {
    "smp": 8,
    "architecture": "arm64-v8a",
    "topologySummary": "6x Cortex-A55 @ 2.0GHz + 2x Cortex-A76 @ 2.4GHz",
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
        "cpus": [0, 1, 2, 3, 4, 5],
        "coreName": "Cortex-A55",
        "minFreqKhz": 500000,
        "maxFreqKhz": 2000000,
        "curFreqKhz": 650000,
        "availableFrequenciesKhz": [
          2000000, 1903000, 1800000, 1703000, 1600000, 1503000, 1407000, 1310000,
          1260000, 1150000, 1053000, 980000, 900000, 740000, 650000, 500000
        ],
        "implementer": "0x41",
        "variant": 2,
        "part": "0xd05",
        "revision": 0,
        "capacity": 273,
        "midr": "0x00000000412fd050",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 6,
        "cpus": [6, 7],
        "coreName": "Cortex-A76",
        "minFreqKhz": 650000,
        "maxFreqKhz": 2400000,
        "curFreqKhz": 1040000,
        "availableFrequenciesKhz": [
          2400000, 2320000, 2240000, 2150000, 2000000, 1900000, 1800000, 1660000,
          1540000, 1430000, 1300000, 1140000, 1040000, 910000, 740000, 650000
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
    "renderer": "Mali-G68 MC4",
    "glesVersion": "OpenGL ES 3.2",
    "glVersionFull": "OpenGL ES 3.2 v1.r38p1-01eac0",
    "glslVersion": "OpenGL ES GLSL ES 3.20",
    "glExtraExts": "GL_ARM_mali_program_binary,GL_ARM_mali_shader_binary,GL_ARM_rgba8",
    "glExtraExts2": "GL_ARM_debug_assert,GL_ARM_shader_core_properties,GL_ARM_texture_unnormalized_coordinates",
    "maxTextureSize": 16383,
    "maxCombinedTextureImageUnits": 96,
    "maxVertexAttribs": 32,
    "vulkanHardware": "mali",
    "vulkanVendorId": "0x13b5",
    "vulkanApiVersion": "1.3",
    "vulkanApiVersionHex": "0x00403116",
    "vulkanConformance": "1.3.8.0",
    "vulkanDeviceId": "0x90940010",
    "vulkanDriverId": 9,
    "vulkanDriverVersion": "0x09801000",
    "vulkanDriverInfo": "v1.r38p1-01eac0",
    "vulkanDeviceUuid": "4a1b9c3d8e5f60718293a4b5c6d7e8f9",
    "vulkanDriverUuid": "b3c4d5e6f708192a3b4c5d6e7f809102",
    "vulkanPipelineUuid": "1f2e3d4c5b6a79880716253443526170"
  },
  "memory": {
    "ramType": "LPDDR4X",
    "variantsGb": [8, 12, 16],
    "variantsMb": [8192, 12288, 16384]
  },
  "storage": {
    "storageType": "UFS 2.2",
    "variantsGb": [256, 512],
    "variantsManifestMb": [256000, 512000],
    "skuCombinations": ["8GB+256GB", "12GB+256GB", "12GB+512GB", "16GB+512GB"]
  },
  "peripherals": {
    "fingerprint": "侧边电容指纹 (FPC)",
    "touchControllerHints": ["fts_ts"],
    "keyboardDriverHint": "mtk-kpd",
    "headsetJackNameHint": "mt6877-mt6359 Headset Jack",
    "pmicKeysNameHint": "mtk-pmic-keys",
    "sensors": ["accelerometer", "magnetometer", "gyroscope", "ambient_light", "proximity", "step_counter", "step_detector"],
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
    "speakers": "单扬声器"
  },
  "camera": {
    "rear": "50MP + 2MP",
    "front": "16MP"
  },
  "batteryAndConnectivity": {
    "batteryMah": 10000,
    "wiredChargingW": 33,
    "wirelessChargingW": 0,
    "wifi": "Wi-Fi 6 (802.11ax)",
    "wifi6Supported": true,
    "bluetooth": "5.2",
    "cellular5G": true,
    "macOuiCandidates": ["00:0c:43"],
    "widevineDefault": "L1",
    "widevineSystemId": "33098",
    "widevineVersion": "18.0.0",
    "oemCryptoApiVersion": 18,
    "imeiTac": "",
    "imeiSvn": "",
    "meidPrefix": ""
  },
  "inputAlignment": {
    "touch": {
      "name": "fts_ts",
      "bus": 24,
      "vendor": 0,
      "product": 0,
      "version": 0,
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
      "goodixFp": false,
      "devices": [
        {
          "name": "uinput-fpc",
          "bus": 6,
          "vendor": 1638,
          "product": 2184,
          "version": 0,
          "keys": ["KEY_UP", "KEY_LEFT", "KEY_RIGHT", "KEY_DOWN"],
          "abs": [{"axis": "ABS_Z", "min": 0, "max": 255, "fuzz": 0, "flat": 0}]
        },
        {
          "name": "fpc_irq@fingerprint",
          "keys": ["KEY_POWER", "KEY_WAKEUP"],
          "msc": ["MSC_SCAN"]
        },
        {
          "name": "mt6877-mt6359 Headset Jack",
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
    {"type": "battery", "base-temp-mc": 29000, "jitter-mc": 100},
    {"type": "mtktscpu", "base-temp-mc": 34000, "jitter-mc": 350},
    {"type": "mtktsAP", "base-temp-mc": 31200, "jitter-mc": 220},
    {"type": "mtktsgpu", "base-temp-mc": 30800, "jitter-mc": 200}
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
      "cycleCount": 64,
      "fullUah": 10000,
      "counterMin": 9880,
      "counterMax": 9942,
      "fullMv": 4384,
      "chargeMv": 4450,
      "temperatureFullDecic": 310,
      "temperatureChargeDecic": 380,
      "currentFullUa": -88000,
      "currentChargeUa": -700000,
      "wireless": 0,
      "stateOfHealth": 99
    },
    "vibrator": {
      "motorType": "Rotor (ERM)",
      "registerManager": true,
      "capabilities": 0,
      "managerCapabilities": 0,
      "compositionDelayMax": 1000,
      "compositionSizeMax": 256,
      "vibratorIds": [1],
      "effects": ["CLICK", "TICK"]
    },
    "gnss": {
      "systemInfoName": "MTK_MNLD_default,MNL_VER_default",
      "gnssCapabilities": 991,
      "powerCapabilities": 1,
      "measurementCorrectionsCapabilities": 0,
      "yearOfHw": 2021,
      "defaultSatellites": 7,
      "dualFrequencyL5": false
    },
    "thermal": {
      "skinName": "SKIN",
      "skinCelsius": 28.6,
      "modelMaximumCelsius": 50,
      "coolingDeviceName": "devfreq-13000000.mali",
      "sensors": [
        {"id": 0, "name": "CPU", "value": 30.5, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 1, "name": "GPU", "value": 30.2, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 2, "name": "BATTERY", "value": 29.0, "thresholds": "nan;nan;nan;50;55;59;60", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 3, "name": "SKIN", "value": 28.6, "thresholds": "nan;43;45;50;70;80;90", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 5, "name": "POWER_AMPLIFIER", "value": 29.4, "thresholds": "nan;nan;nan;68;90;100;110", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 9, "name": "NPU", "value": 30.4, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 13, "name": "SOC", "value": 31.1, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"}
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
      {"handle": 1, "name": "lsm6dso_acc", "vendor": "STMicroelectronics", "version": 1, "typeId": 1, "type": "android.sensor.accelerometer", "maxRange": "78.453605652", "resolution": "0.0012", "power": "0.17", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 2, "name": "mmc5603", "vendor": "Memsic", "version": 1, "typeId": 2, "type": "android.sensor.magnetic_field", "maxRange": "4912.050292969", "resolution": "0.150000006", "power": "0.28", "minDelay": 10000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 3, "name": "ORIENTATION", "vendor": "MTK", "version": 1, "typeId": 3, "type": "android.sensor.orientation", "maxRange": "360.0", "resolution": "0.00390625", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 4, "name": "lsm6dso_gyro", "vendor": "STMicroelectronics", "version": 1, "typeId": 4, "type": "android.sensor.gyroscope", "maxRange": "34.906299591", "resolution": "0.0011", "power": "0.17", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 5, "name": "stk3338_l", "vendor": "Sensortek", "version": 1, "typeId": 5, "type": "android.sensor.light", "maxRange": "65535.0", "resolution": "1.0", "power": "0.05", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 8, "name": "stk3338_p", "vendor": "Sensortek", "version": 1, "typeId": 8, "type": "android.sensor.proximity", "maxRange": "5.0", "resolution": "1.0", "power": "0.05", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 1000000, "wakeUp": true, "reporting": 1},
      {"handle": 9, "name": "GRAVITY", "vendor": "MTK", "version": 1, "typeId": 9, "type": "android.sensor.gravity", "maxRange": "39.226802826", "resolution": "0.0012", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 10, "name": "LINEARACCEL", "vendor": "MTK", "version": 1, "typeId": 10, "type": "android.sensor.linear_acceleration", "maxRange": "39.226802826", "resolution": "0.0012", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 11, "name": "ROTATION_VECTOR", "vendor": "MTK", "version": 1, "typeId": 11, "type": "android.sensor.rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 14, "name": "UNCALI_MAG", "vendor": "MTK", "version": 1, "typeId": 14, "type": "android.sensor.magnetic_field_uncalibrated", "maxRange": "4912.050292969", "resolution": "0.150000006", "power": "0.28", "minDelay": 20000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 15, "name": "GAME_ROTATION_VECTOR", "vendor": "MTK", "version": 1, "typeId": 15, "type": "android.sensor.game_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 16, "name": "UNCALI_GYRO", "vendor": "MTK", "version": 1, "typeId": 16, "type": "android.sensor.gyroscope_uncalibrated", "maxRange": "34.906299591", "resolution": "0.0011", "power": "0.17", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 17, "name": "SIGNIFICANT_MOTION", "vendor": "MTK", "version": 1, "typeId": 17, "type": "android.sensor.significant_motion", "maxRange": "1.0", "resolution": "1.0", "power": "0.17", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 18, "name": "STEP_DETECTOR", "vendor": "MTK", "version": 1, "typeId": 18, "type": "android.sensor.step_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.17", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 0, "wakeUp": false, "reporting": 3},
      {"handle": 19, "name": "STEP_COUNTER", "vendor": "MTK", "version": 1, "typeId": 19, "type": "android.sensor.step_counter", "maxRange": "2147483648.0", "resolution": "1.0", "power": "0.17", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 20, "name": "GEOMAGNETIC_ROTATION_VECTOR", "vendor": "MTK", "version": 1, "typeId": 20, "type": "android.sensor.geomagnetic_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 22, "name": "TILT_DETECTOR", "vendor": "MTK", "version": 1, "typeId": 22, "type": "android.sensor.tilt_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.17", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 3},
      {"handle": 27, "name": "DEVICE_ORIENTATION", "vendor": "MTK", "version": 1, "typeId": 27, "type": "android.sensor.device_orientation", "maxRange": "3.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 35, "name": "UNCALI_ACC", "vendor": "MTK", "version": 1, "typeId": 35, "type": "android.sensor.accelerometer_uncalibrated", "maxRange": "39.226802826", "resolution": "0.0012", "power": "0.17", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0}
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
      {"name": "Virtualizer", "implementor": "NXP Software Ltd.", "type": "37cc2c00-dddd-11db-8577-0002a5d5c51b", "uuid": "1d4033c0-8557-11df-9f2d-0002a5d5c51b", "connect": "Insert"},
      {"name": "Visualizer", "implementor": "The Android Open Source Project", "type": "e46b26a0-dddd-11db-8afd-0002a5d5c51b", "uuid": "d069d9e0-8329-11df-9168-0002a5d5c51b", "connect": "Insert"},
      {"name": "Volume", "implementor": "NXP Software Ltd.", "type": "09e8ede0-ddde-11db-b4f6-0002a5d5c51b", "uuid": "119341a0-8469-11df-81f9-0002a5d5c51b", "connect": "Insert"}
    ],
    "media": {
      "instances": [
        {"name": "default1", "binary": "android.hardware.media.c2-mediatek", "rank": 1}
      ],
      "components": [
        {"store": "default1", "name": "c2.mtk.adpcm-ms.decoder", "kind": "decoder", "domain": "audio", "mediaType": "audio/x-adpcm-ms", "aliases": "OMX.MTK.AUDIO.DECODER.ADPCM.MS", "rank": 1},
        {"store": "default1", "name": "c2.mtk.adpcm-dvi-ima.decoder", "kind": "decoder", "domain": "audio", "mediaType": "audio/x-adpcm-dvi-ima", "aliases": "OMX.MTK.AUDIO.DECODER.ADPCM.DVI", "rank": 1},
        {"store": "default1", "name": "c2.mtk.ape.decoder", "kind": "decoder", "domain": "audio", "mediaType": "audio/ape", "aliases": "OMX.MTK.AUDIO.DECODER.APE", "rank": 1},
        {"store": "default1", "name": "c2.mtk.alac.decoder", "kind": "decoder", "domain": "audio", "mediaType": "audio/alac", "aliases": "OMX.MTK.AUDIO.DECODER.ALAC", "rank": 1},
        {"store": "default1", "name": "c2.mtk.wma.decoder", "kind": "decoder", "domain": "audio", "mediaType": "audio/x-ms-wma", "aliases": "OMX.MTK.AUDIO.DECODER.WMA", "rank": 1},
        {"store": "default1", "name": "c2.mtk.mp3.decoder", "kind": "decoder", "domain": "audio", "mediaType": "audio/mpeg", "aliases": "OMX.MTK.AUDIO.DECODER.MP3", "rank": 1},
        {"store": "default1", "name": "c2.mtk.mpeg4.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/mp4v-es", "aliases": "OMX.MTK.VIDEO.DECODER.MPEG4", "maxSize": "2048x2048", "profiles": "12297,12308", "levels": "16393", "rank": 1},
        {"store": "default1", "name": "c2.mtk.h263.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/3gpp", "aliases": "OMX.MTK.VIDEO.DECODER.H263", "maxSize": "1408x1408", "profiles": "12288", "levels": "12291,12292,12293,12294,12295", "rank": 1},
        {"store": "default1", "name": "c2.mtk.avc.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/avc", "aliases": "OMX.MTK.VIDEO.DECODER.AVC", "maxSize": "4096x4096", "profiles": "20480,20481,20482,20484,20487", "levels": "20495", "rank": 1},
        {"store": "default1", "name": "c2.mtk.avc.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/avc", "aliases": "OMX.MTK.VIDEO.DECODER.AVC.secure", "maxSize": "1920x1920", "profiles": "20480,20481,20482,20484,20487", "levels": "20495", "rank": 1},
        {"store": "default1", "name": "c2.mtk.hevc.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/hevc", "aliases": "OMX.MTK.VIDEO.DECODER.HEVC", "maxSize": "4096x4096", "profiles": "24576,24578,24577", "levels": "24835", "rank": 1},
        {"store": "default1", "name": "c2.mtk.hevc.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/hevc", "aliases": "OMX.MTK.VIDEO.DECODER.HEVC.secure", "maxSize": "1920x1920", "profiles": "24576,24578,24577", "levels": "24835", "rank": 1},
        {"store": "default1", "name": "c2.mtk.heif.decoder", "kind": "decoder", "domain": "video", "mediaType": "image/vnd.android.heic", "aliases": "OMX.MTK.VIDEO.DECODER.HEIF", "maxSize": "16383x16383", "rank": 1},
        {"store": "default1", "name": "c2.mtk.vpx.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/x-vnd.on2.vp8", "aliases": "OMX.MTK.VIDEO.DECODER.VPX", "maxSize": "2048x2048", "profiles": "40960", "levels": "1", "rank": 1},
        {"store": "default1", "name": "c2.mtk.vp9.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/x-vnd.on2.vp9", "aliases": "OMX.MTK.VIDEO.DECODER.VP9", "maxSize": "4096x4096", "profiles": "28672,28673,28674", "levels": "28680", "rank": 1},
        {"store": "default1", "name": "c2.mtk.vp9.decoder.secure", "kind": "decoder", "domain": "video", "mediaType": "video/x-vnd.on2.vp9", "aliases": "OMX.MTK.VIDEO.DECODER.VP9.secure", "maxSize": "1920x1920", "profiles": "28672,28673,28674", "levels": "28680", "rank": 1},
        {"store": "default1", "name": "c2.mtk.mpeg2.decoder", "kind": "decoder", "domain": "video", "mediaType": "video/mpeg2", "aliases": "OMX.MTK.VIDEO.DECODER.MPEG2", "maxSize": "2048x2048", "profiles": "4097", "levels": "4099", "rank": 1},
        {"store": "default1", "name": "c2.mtk.avc.encoder", "kind": "encoder", "domain": "video", "mediaType": "video/avc", "aliases": "OMX.MTK.VIDEO.ENCODER.AVC", "maxSize": "3840x3840", "profiles": "20480,20482,20484", "levels": "20494", "rank": 1},
        {"store": "default1", "name": "c2.mtk.avc.encoder.secure", "kind": "encoder", "domain": "video", "mediaType": "video/avc", "maxSize": "1920x1920", "profiles": "20480,20482,20484", "levels": "20494", "rank": 1},
        {"store": "default1", "name": "c2.mtk.hevc.encoder", "kind": "encoder", "domain": "video", "mediaType": "video/hevc", "aliases": "OMX.MTK.VIDEO.ENCODER.HEVC", "maxSize": "3840x3840", "profiles": "24576", "levels": "24834", "rank": 1},
        {"store": "default1", "name": "c2.mtk.heif.encoder", "kind": "encoder", "domain": "video", "mediaType": "image/vnd.android.heic", "aliases": "OMX.MTK.VIDEO.ENCODER.HEIF", "maxSize": "16383x16383", "rank": 1}
      ]
    },
    "audioEngine": {
      "modules": "default,bluetooth,r_submix",
      "voiceCallIndexMax": 7,
      "musicIndexMax": 15
    }
  },
  "alignmentNotes": [
    "身份口径：build fingerprint 三段为 Blackview/XPLORE_X1_NEU/XPLORE_X1，故 brand=blackview、device=XPLORE_X1、SKU=XPLORE_X1_NEU，model 取商业名 Blackview Xplore 1。",
    "CPU 以固件实测为最高优先级：两簇 compatible/MIDR 实测为 arm,cortex-a76（0xd0b）与 arm,cortex-a55（0xd05），capacity-dmips 273/1024，16+16 档 OPP 原样落入 availableFrequenciesKhz；公开资料写 A78 的地方不采用（device-checker 读 sysfs 与 /proc/cpuinfo）。",
    "governor 采用 MTK Android15 常规 sugov_ext + mt-cpufreq，scalingReadable=false、packagePerCluster=false 与 MTK sysfs 暴露方式一致。",
    "GPU 为 MT6877 自带 Mali-G68 MC4；glVersionFull 仅保留 r38p1 分支与 -01eac0 后缀，git 哈希段必须由原厂 vendor/lib64/egl/libGLES_mali.so 的 strings 回填（运行态该 so 已被 libEGL_meow/libGLES_meow 重定向顶替，maps 取不到原串）。",
    "vulkanDeviceId/DriverVersion 与 deviceUuid/driverUuid/pipelineUuid 按 ARM Mali r38p1 家族推定，需 vulkaninfo 实测回填后再冻结。",
    "VINTF 侧 radioVariant=mtk：固件暴露 android.hardware.radio.* 与 vendor.mediatek.hardware.mtkradioex.* 的 slot1/slot2/imsSlot/se 全量占位，故 vintfDrop 为空，占位由 cdh_radio_placeholders.txt 承载；nn 保留 vendor.blackview.helper.IHelper/default 与 vendor.microarray.mafp.IMAFactory/default 等 Blackview 私有 AIDL 服务。",
    "NFC 与 IR 均开启（gold 为 NFC 关、IR 开，不可照抄）：ST21NFC（libnfc-hal-st.conf/st21nfc_conf.txt）+ permissions hasNfc=true → enableNfc=true；hasConsumerIr=true → enableIr=true。",
    "传感器不填私有前缀：固件 permissions 仅列标准 7 项，无 vendor.sensor.*/blackview.sensor.* 私有 HAL，故不填 xiaomi.sensor.*（swtp 亦然，非小米系严禁），也不编造气压计。",
    "传感器料号 lsm6dso + mmc5603 + stk3338 为 MTK rugged 常规组合预置，需以 dumpsys sensorservice 的 name/vendor/maxRange 回读为准；若实测为 bmi323/ak09918/tcs3701 等同级料号，仅替换 name/vendor/maxRange/resolution，handle 与 typeId 不变。",
    "触控预置 fts_ts（FocalTech，bus=24 i2c），与固件实测 mtk-kpd / mt6877-mt6359 Headset Jack / mtk-pmic-keys 三节点构成 input 身份；若实为 goodix_ts 或 nt36xxx，仅替换 name/bus/vendor/product/version，mtSlots 保持 10。",
    "指纹按侧边 FPC 方案（uinput-fpc + fpc_irq@fingerprint，goodixFp=false）；若实测为 Goodix 则改 uinput-goodix 且 goodixFp=true。",
    "media 采用固件实测单实例 android.hardware.media.c2-mediatek + default1 store，22 个组件全部 rank=1 假登记，绝不占用 default；secure 变体上限严格保持 1920x1920，不得抬到 4K。",
    "audio 的 17 条 descriptor 为固件 audio_effects_config.xml 实测原样保留（AOSP 8 + NXP 6 + MTK AEC/AGC/NS），未另行编造 Dolby/MiSound；audioEngine 走 MTK 常规 default,bluetooth,r_submix，音量阶 voiceCall 7 / music 15。",
    "display 面板 6.78\" 1080x2460 / 480dpi 按 MT6877 常规 FHD+ 面板与固件 densityDpi=480 推定，androidDensityDpi 保持出厂 480；物理宽高须以真机 getRealSize 与面板规格书复核，EDID productId/week/year 同理。",
    "battery 10000mAh/33W、UFS 2.2、8/12/16GB 档位、后置 50MP+2MP 为 rugged 档位推定；fullUah/fullMv/chargeMv 必须与 /sys/class/power_supply/battery 的 charge_full/voltage_now 对齐后冻结。",
    "vibrator 按转子马达（ERM）只登记 CLICK/TICK；若真机为 X 轴线性马达，需补 HEAVY_CLICK/TEXTURE_TICK 并将 motorType 改为 Z-axis linear。",
    "thermal coolingDeviceName 使用 MT6877 Mali 的 devfreq-13000000.mali，须以 /sys/class/thermal/cooling_device*/type 回读确认。",
    "imeiTac/imeiSvn/meidPrefix 留空待实测：TAC 必须取自真机 IMEI 前 8 位或包装贴纸，凭空编造会导致 device-checker 与运营商侧双重不一致；macOuiCandidates 暂给 MTK 通用 OUI 00:0c:43，需以 wlan0 MAC 前 3 字节替换。"
  ],
  "sources": [
    "目标机固件实测包（build.prop / boot DTB / VINTF manifest / vendor/etc / odm/etc）：brand=Blackview、device=XPLORE_X1、board=mt6877、osVersion=15-AP3A.240905.015.A2-20251126、densityDpi=480、vintf=202404.0、VINTF 占位清单、vendorOfficialFiles、measuredMapsRedirects、measuredCpuClusters、measuredInputHints、vdaPartitions、vendorEtcConfigs",
    "vendor/etc/audio_effects_config.xml 实测 17 条 effectDescriptors 与 18 个 effectLibraries",
    "vendor/etc/media_codecs*.xml 实测 1 个 Codec2 实例（android.hardware.media.c2-mediatek / default1）与 22 个硬件编解码组件",
    "vendor/etc/libnfc-hal-st.conf、vendor/etc/st21nfc_conf.txt（ST21NFC → enableNfc=true）",
    "vendor/etc/permissions 实测 sensorFeatures 7 项 / hasConsumerIr / hasNfc / vulkan 1.3 / deqp 132645633",
    "kernel/DTB 实测 OPP 表（16+16 档）、capacity-dmips 273/1024、MIDR 0x412fd050 / 0x414fd0b0",
    "ARM Mali r38p1 驱动家族与 Mali-G68 MC4 规格（Vulkan vendorID 0x13b5 / driverID 9 / conformance 1.3.8.0）",
    "MediaTek MT6877（Dimensity 900）平台规格：6nm、LPDDR4X/LPDDR5、UFS 2.2/3.1、Wi-Fi 6、BT 5.2、GNSS L1+L5 能力位",
    "联网核实受限：GSMArena（Turnstile 校验）、DuckDuckGo HTML（bot challenge）、Bing（结果污染）均未返回有效机型页，故内存/存储档位、面板尺寸、电池容量、传感器料号以固件口径加平台常规档位推定，已在 alignmentNotes 标注待真机回读"
  ]
}
```
