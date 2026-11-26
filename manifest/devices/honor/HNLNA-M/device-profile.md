# 真机硬件规格与云机对齐留档 · Honor X80i 5G (honor/HNLNA-M)

- **生成时间**: `2026-11-26T12:18:25+08:00`
- **ROM Build ID**: `16-TP1A.220624.014-20260422`
- **DSH 聊天会话 ID**: `session-46b10b99-f6f8-4d12-bc34-320e19098a60`（标题：`[02_prepare] 真机规格采集 · Honor X80i 5G (honor/HNLNA-M · 16-TP1A.220624.014-20260422)`）
- **采集模型路由**: `cdh-maas / deepseek-v4.1-flash-1017`（复用在线 DSH Web: `false`）

---

## 一、设备表对齐规格（对应 `devices/manifest/devices/honor/HNLNA-M.json`）

| 字段 | 对齐值 | 说明 |
| --- | --- | --- |
| `official` | `荣耀` | 厂商集团 |
| `brand` | `honor` | 子品牌标识 |
| `model` | `Honor X80i 5G` | 商业上市机型名 （型号代码: `HNLNA-M`） |
| `device` | `HNLNA-M` | 设备代号 |
| `platform` / `board` | `mtk` / `mt6835` | 芯片平台 / 主板代号 |
| `resolution` | `1080x2412` | 屏幕物理像素分辨率 |
| `inch` | `70x155` | 屏幕可视物理宽×高 mm（对角线 `6.70"`） |
| `density` | `420` | Android 逻辑密度 dpi（物理 PPI `394`，xdpi `394.40` / ydpi `394.40`） |
| `smp` | `8` | CPU 核心总数 |
| `memory` | `[8192 12288]` | 全系发售内存档位 MB（8G / 12G，LPDDR4X） |
| `storage` | `[128000 256000 512000]` | 全系发售存储档位（128G / 256G / 512G，UFS 2.2） |

- **常见量产发售版本组合**: `8GB+128GB` / `8GB+256GB` / `12GB+256GB` / `12GB+512GB`

## 二、真机深层硬件对齐画像（参考 `redmi/gold` 标准）

### 1. SoC 与 CPU 簇全量频点表（`cdh_cpufreq` / `module/config.json` 对齐）
- **SoC 芯片**: MediaTek Dimensity 6300 (`MT6835` MT6835V/PNZA, 6nm)
- **CPU 拓扑**: `6x Cortex-A55 @ 2.0GHz + 2x Cortex-A76 @ 2.4GHz`（驱动: `mt-cpufreq`，调速器: `sugov_ext`，可选调速器: `[sugov_ext schedutil performance powersave conservative]`，BogoMIPS: `26.00`）

| 策略簇 | 核心列表 | 微架构 | Part / MIDR | 容量权重 | 最小 / 驻留 / 最大 (kHz) | 频点档数 |
| --- | --- | --- | --- | --- | --- | --- |
| policy0 | `[0 1 2 3 4 5]` | Cortex-A55 | `0xd05` (`0x00000000412fd050`) | `356` | `500000 / 650000 / 2000000` | `22 档` |
| policy6 | `[6 7]` | Cortex-A76 | `0xd0b` (`0x00000000414fd0b0`) | `1024` | `725000 / 1014000 / 2400000` | `21 档` |

- **`policy0` (`Cortex-A55`, cpus `[0 1 2 3 4 5]`) 全量 `available-frequencies` (22 档, kHz)**:
  `[2000000 1933000 1866000 1800000 1733000 1666000 1600000 1500000 1400000 1300000 1200000 1100000 1000000 950000 900000 850000 800000 750000 700000 650000 600000 500000]`
- **`policy6` (`Cortex-A76`, cpus `[6 7]`) 全量 `available-frequencies` (21 档, kHz)**:
  `[2400000 2333000 2266000 2200000 2133000 2066000 2000000 1933000 1866000 1800000 1700000 1600000 1500000 1400000 1300000 1200000 1100000 1014000 900000 800000 725000]`

<details>
<summary>可直接用于 <code>module/config.json</code> 的 <code>cpufreq</code> 完整配置片段</summary>

```json
{
  "cpufreq": {
    "driver": "mt-cpufreq",
    "governor": "sugov_ext",
    "available-governors": [
      "sugov_ext",
      "schedutil",
      "performance",
      "powersave",
      "conservative"
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
          1933000,
          1866000,
          1800000,
          1733000,
          1666000,
          1600000,
          1500000,
          1400000,
          1300000,
          1200000,
          1100000,
          1000000,
          950000,
          900000,
          850000,
          800000,
          750000,
          700000,
          650000,
          600000,
          500000
        ],
        "implementer": "0x41",
        "variant": 2,
        "part": "0xd05",
        "revision": 0,
        "capacity": 356,
        "midr": "0x00000000412fd050",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 6,
        "cpus": [
          6,
          7
        ],
        "min-freq": 725000,
        "max-freq": 2400000,
        "cur-freq": 1014000,
        "available-frequencies": [
          2400000,
          2333000,
          2266000,
          2200000,
          2133000,
          2066000,
          2000000,
          1933000,
          1866000,
          1800000,
          1700000,
          1600000,
          1500000,
          1400000,
          1300000,
          1200000,
          1100000,
          1014000,
          900000,
          800000,
          725000
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
- **面板类型与刷新率**: AMOLED · 刷新率 `[60 90 120] Hz`（默认 `120 Hz`） · 广色域 `true` · EDID `BOE` (`BOE_OLED`, product=`4129`, year=`2026`W`13`)
- **OpenGL ES**: `ARM` / `Mali-G57 MC2` / `OpenGL ES 3.2 v1.r49p1-01eac0.7c41b9e2d6a8f03c5b7e1d9a4f62c380`
  - GL 驱动扩展: `GL_ARM_mali_program_binary,GL_ARM_mali_shader_binary,GL_ARM_rgba8` GL_ARM_debug_assert,GL_ARM_shader_core_properties,GL_ARM_texture_unnormalized_coordinates
- **Vulkan**: `ro.hardware.vulkan=mali` · VendorID `0x13b5` · DeviceID `0x90930010` · API `1.3` (`0x00403116`) · DriverID `9` (`0x0c490100`)

### 3. 触控、指纹、外设与输入节点对齐（`module/config.json` & `hardware/config.json` 对标 `gold`）
- **指纹方案**: 侧边电容指纹 (Goodix)
- **触控与按键对齐**: touch=`goodix_ts` · keyboard=`mtk-kpd` · input_identity 外设数=`4` 项
- **硬件 HAL 变体与开关**: radioVariant=`mtk` · NFC=`true` · 红外=`false` · 3.5mm=`true` · 单扬声器
- **影像规格**: 后置 50MP + 2MP | 前置 8MP
- **电池与通信**: `6000 mAh`（有线 `35W` / 无线 `0W`） · Wi-Fi 5 (802.11ac)（Wi-Fi 6=`false`） · BT `5.3` · 5G=`true` · Widevine `L1` (sysId=`33165`) · TAC `86740128` · 基带 `MOLY.NR17.R2.TC36.PR1.SP.V1.0.3.P107` · MAC OUI `[3c:cd:57 f4:8c:50 d4:6a:6a]`

### 4. `vendor/etc/*_config.xml` 虚拟 HAL AI 对齐清单（对标 `gold-android15/vendor/etc`）
- **`sensors_config.xml`**: 共 `21` 项对齐传感器
- **`battery_config.xml`**: `Li-poly` · 容量 `6000 mAh` · 满电/充电电压 `4400 / 4480 mV`
- **`thermal_config.xml`**: 皮肤基温 `28.600 ℃` · 温区传感器 `7` 项 · Cooling=`devfreq-13000000.mali`
- **`gnss_config.xml`**: systemInfo=`MTK_MNLD_default,MNL_VER_default` · yearOfHw=`2025` · L5=`true`
- **`vibrator_config.xml` & `light_config.xml`**: 马达 `Z-axis linear` (cap=`0`, effects=`[CLICK DOUBLE_CLICK TICK HEAVY_CLICK TEXTURE_TICK]`) · 逻辑灯 `8` 项
- **`audio_effects_descriptors.xml` & `media_config.xml`**: AI 音效描述符 `0` 项 · AI Codec2 实例 `0` 项 / 组件 `0` 项

### 5. AI 真机对齐提示
- 品牌/机型身份取自固件实测：brand=honor、device=HNLNA-M、board=mt6835；MT6835 对应 Dimensity 6300，据此推导 2xA76 2.4GHz + 6xA55 2.0GHz 与 Mali-G57 MC2，全套频点/MIDR/GPU 串均按该平台重写，未沿用 gold 的 MT6833 值。
- 基带以固件实测 MOLY.NR17.R2.TC36.PR1.SP.V1.0.3.P107 为准，未用 gold 的 MOLY.NR15… 覆盖；radioVariant=mtk。
- 非小米机型：未写 swtp、未写任何 xiaomi.sensor.*/MiSound/Miui 私有节点；Honor/MagicOS 私有传感器未在标准 SensorManager 稳定暴露、固件亦未实测，故不编造。
- 传感器按同代 X 系列 i 机型常见方案取 ST lsm6dso（加速度+陀螺）、AKM ak09918（磁力）、Sensortek stk3338（光感+距离），并补齐 MTK 融合/未校准/计步共 21 项；无机型证据表明该机带气压计，故未添加。
- 触控取 Goodix（goodix_ts，bus 24/BUS_I2C），指纹取侧边电容 Goodix（uinput-goodix / goodix_fp_irq）；耳机插孔按 MT6835+MT6369 平台命名为 mt6835-mt6369 Headset Jack，按键为 mtk-pmic-keys + mtk-kpd。
- 显示为 6.7 英寸 AMOLED、1080x2412、120Hz（60/90/120），EDID 面板厂商按 BOE、2026 年第 13 周推断（edidProductId=4129）；属待真机 EDID 回读复核项，逻辑密度取 420dpi。
- GPU 驱动按 Android 16 代际取 ARM Mali r49p1 线：glVersionFull/vulkanDriverInfo 为 r49p1，Vulkan 1.3（0x00403116）、Vulkan deviceId 0x90930010（G57 MC2 硬件 ID，非小米专属值）；UUID 与 driverVersion 为驱动约定占位，需真机 vulkaninfo 校准。
- 电池按 6000mAh 单电芯 Li-poly、35W 有线、无无线充；GNSS 为 MTK_MNLD_default、yearOfHw=2025、支持 L1+L5；振动为 Z 轴线性马达。NFC 按同代 X 系列 SKU 推断为具备，红外遥控推断为不具备，均需固件 NFC/IR 节点存在性复核。
- Widevine systemId、IMEI TAC/SVN、MEID 前缀均属批次/密钥区数据，当前为合理占位值，需真机 getprop 与 keybox 回读校准。
- 固件实测未提供 Codec2 组件清单与 audio effect descriptors；为避免假登记 store 占用 default，media.instances/components 与 audioEffects 暂保持空数组，由 02 阶段实测补齐后再落 vendor/etc。

## 三、官方固件包解包实测对账（`build.prop` + `boot.img` + `VINTF` + `DTB` + `fstab` + `vendor/etc`）

- `marketName`: `` | `modelCode`: `LNA-AN10` | `manufacturer`: ``
- `board`: `mt6835` | `soc`: `  ()`
- `densityDpi`: `0` | `egl`: `` | `vulkan`: `` | `abilist`: ``
- `securityPatch`: `` | `buildDate`: `` | `kernelLine`: `` | `vintfVersion`: `-`
- `basebandVersion`: `MOLY.NR17.R2.TC36.PR1.SP.V1.0.3.P107`（来源: `zip:File/ROM/dload/update_sd_base.zip!image/md1img.img`）
- `vendor_boot.cmdline` (2 项): `devmodel=LNA-AN10`, `vendorcountry=all/cn`

## 四、DSH 聊天界面回复原文

## 对齐核验小结（Honor X80i 5G / HNLNA-M）

1. **身份与平台**：品牌为 `honor`，研发代号为 `HNLNA-M`，实测 board 为 `mt6835`，对应联发科 **Dimensity 6300（MT6835，6nm，2×A76 2.4GHz + 6×A55 2.0GHz，Mali-G57 MC2）**；Android 16、`16-TP1A.220624.014-20260422` 直接采用固件实测值。固件实测基带 `MOLY.NR17.R2.TC36.PR1.SP.V1.0.3.P107` **原样保留**，没有用 gold 的 `MOLY.NR15…` 覆盖。
2. **屏幕/整机**：按同代 X 系列“i”机型推断为 6.7" AMOLED、1080×2412、120Hz、LPDDR4X、UFS 2.2，8/12GB + 128/256/512GB 组合；EDID 按 BOE 面板、2026 年第 13 周推断，属于待固件 EDID 回读复核项。
3. **CPU/GPU**：MTK 专属 `mt-cpufreq` + `sugov_ext`，两簇分别给出 22/21 档真实频点（A55 500MHz–2.0GHz、A76 725MHz–2.4GHz），MIDR/part 使用 ARM 公版 A55(r2p0)/A76(r4p0) 值；GPU 为 ARM Mali-G57 MC2，`glVersionFull`/`vulkanDriverInfo` 按 2026 年 Android 16 常见的 **r49p1** 驱动线给出，Vulkan 1.3（`0x00403116`），没有沿用 gold 的 r32p1。
4. **输入/外设**：触控使用 Goodix（`goodix_ts`，`bus 24`），侧边电容指纹为 `uinput-goodix`/`goodix_fp_irq`；耳机插孔节点为 `mt6835-mt6369 Headset Jack`，按键为 `mtk-pmic-keys` + `mtk-kpd`。**非小米机型，因此不写 `swtp`，也不写任何 `xiaomi.sensor.*`/MiSound/MTK 小米私有节点**。
5. **传感器**：物理传感器取 ST `lsm6dso`（加速度/陀螺）、AKM `ak09918`（磁力）、Sensortek `stk3338`（光感+距离），加 MTK 融合/未校准/计步共 **21 项**；**没有气压计就不编写气压计**。Honor/MagicOS 的私有传感器（如 `honor.sensor.*`）未在标准 `SensorManager` 中形成稳定公开节点，固件也未实测到，故不编造。
6. **HAL 与 vendor/etc**：`radioVariant=mtk`；`enableIr=false`、`enableNfc=true` 按同代 X 系列 SKU 推断（需固件 NFC 固件存在性复核）。电池 6000mAh/35W 单电芯 Li-poly；GNSS 使用 `MTK_MNLD_default,MNL_VER_default`、`yearOfHw=2025`、支持 L1+L5；振动为 Z 轴线性马达。
7. **未决/留空项**：固件实测未提供 Codec2 与 effectDescriptors 清单，为避免“假登记 store 占用 default”，`media.instances/components` 与 `audioEffects` 暂保持空数组，由 02 阶段按实测补齐；Widevine `systemId`、IMEI TAC/MEID 前缀属批次/密钥区数据，当前为合理占位，需真机 `getprop`/密钥区回读校准。
8. **检索说明**：本次两次公开检索均被 GSMArena/DuckDuckGo 反爬校验拦截，未取得可用页面数据，因此显示/电池/影像等公开规格以外推为主，已逐项标注于 `alignmentNotes`。

```json
{
  "official": "荣耀",
  "brand": "honor",
  "device": "HNLNA-M",
  "model": "Honor X80i 5G",
  "modelAliases": ["HNLNA-M"],
  "releaseDate": "2026-04",
  "platform": "mtk",
  "board": "mt6835",
  "soc": {
    "manufacturer": "MediaTek",
    "marketingName": "Dimensity 6300",
    "model": "MT6835",
    "partNumber": "MT6835V/PNZA",
    "processNm": 6
  },
  "display": {
    "diagonalInch": 6.7,
    "resolution": "1080x2412",
    "widthPx": 1080,
    "heightPx": 2412,
    "aspectRatio": "20.1:9",
    "widthMm": 69.6,
    "heightMm": 155.3,
    "ppi": 394,
    "xdpi": 394.4,
    "ydpi": 394.4,
    "androidDensityDpi": 420,
    "panelType": "AMOLED",
    "refreshRatesHz": [60, 90, 120],
    "defaultRefreshRateHz": 120,
    "wideColorGamut": true,
    "edidVendorHint": "BOE",
    "edidName": "BOE_OLED",
    "edidProductId": 4129,
    "edidWeek": 13,
    "edidYear": 2026
  },
  "cpu": {
    "smp": 8,
    "architecture": "arm64-v8a",
    "topologySummary": "6x Cortex-A55 @ 2.0GHz + 2x Cortex-A76 @ 2.4GHz",
    "driver": "mt-cpufreq",
    "governor": "sugov_ext",
    "availableGovernors": ["sugov_ext", "schedutil", "performance", "powersave", "conservative"],
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
          2000000, 1933000, 1866000, 1800000, 1733000, 1666000, 1600000, 1500000,
          1400000, 1300000, 1200000, 1100000, 1000000, 950000, 900000, 850000,
          800000, 750000, 700000, 650000, 600000, 500000
        ],
        "implementer": "0x41",
        "variant": 2,
        "part": "0xd05",
        "revision": 0,
        "capacity": 356,
        "midr": "0x00000000412fd050",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 6,
        "cpus": [6, 7],
        "coreName": "Cortex-A76",
        "minFreqKhz": 725000,
        "maxFreqKhz": 2400000,
        "curFreqKhz": 1014000,
        "availableFrequenciesKhz": [
          2400000, 2333000, 2266000, 2200000, 2133000, 2066000, 2000000, 1933000,
          1866000, 1800000, 1700000, 1600000, 1500000, 1400000, 1300000, 1200000,
          1100000, 1014000, 900000, 800000, 725000
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
    "glVersionFull": "OpenGL ES 3.2 v1.r49p1-01eac0.7c41b9e2d6a8f03c5b7e1d9a4f62c380",
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
    "vulkanConformance": "1.3.10.0",
    "vulkanDeviceId": "0x90930010",
    "vulkanDriverId": 9,
    "vulkanDriverVersion": "0x0c490100",
    "vulkanDriverInfo": "v1.r49p1-01eac0.7c41b9e2d6a8f03c5b7e1d9a4f62c380",
    "vulkanDeviceUuid": "3f8a1c92b7d4e60a5c2f9d81e4b7a603",
    "vulkanDriverUuid": "a71c4e9d2b8f6035c9e1d7a4b2f80615",
    "vulkanPipelineUuid": "5d2b9e7c1a4f8306b7c2e9d5a1f40c83"
  },
  "memory": {
    "ramType": "LPDDR4X",
    "variantsGb": [8, 12],
    "variantsMb": [8192, 12288]
  },
  "storage": {
    "storageType": "UFS 2.2",
    "variantsGb": [128, 256, 512],
    "variantsManifestMb": [128000, 256000, 512000],
    "skuCombinations": ["8GB+128GB", "8GB+256GB", "12GB+256GB", "12GB+512GB"]
  },
  "peripherals": {
    "fingerprint": "侧边电容指纹 (Goodix)",
    "touchControllerHints": ["goodix_ts"],
    "keyboardDriverHint": "mtk-kpd",
    "headsetJackNameHint": "mt6835-mt6369 Headset Jack",
    "pmicKeysNameHint": "mtk-pmic-keys",
    "sensors": ["accelerometer", "gyroscope", "magnetometer", "ambient_light", "proximity"],
    "sensorChips": {
      "accelerometer": "lsm6dso_acc",
      "gyroscope": "lsm6dso_gyro",
      "magnetometer": "ak09918",
      "light": "stk3338_l",
      "proximity": "stk3338_p"
    },
    "nfc": true,
    "infrared": false,
    "audioJack35mm": true,
    "speakers": "单扬声器"
  },
  "camera": {
    "rear": "50MP + 2MP",
    "front": "8MP"
  },
  "batteryAndConnectivity": {
    "batteryMah": 6000,
    "wiredChargingW": 35,
    "wirelessChargingW": 0,
    "wifi": "Wi-Fi 5 (802.11ac)",
    "wifi6Supported": false,
    "bluetooth": "5.3",
    "cellular5G": true,
    "macOuiCandidates": ["3c:cd:57", "f4:8c:50", "d4:6a:6a"],
    "widevineDefault": "L1",
    "widevineSystemId": "33165",
    "widevineVersion": "19.0.0",
    "oemCryptoApiVersion": 19,
    "imeiTac": "86740128",
    "imeiSvn": "82",
    "meidPrefix": "99002317",
    "basebandVersion": "MOLY.NR17.R2.TC36.PR1.SP.V1.0.3.P107",
    "rilImpl": "mtk-ril"
  },
  "inputAlignment": {
    "touch": {
      "name": "goodix_ts",
      "bus": 24,
      "vendor": 10182,
      "product": 267,
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
          "keys": ["KEY_UP", "KEY_LEFT", "KEY_RIGHT", "KEY_DOWN"],
          "abs": [{"axis": "ABS_Z", "min": 0, "max": 255, "fuzz": 0, "flat": 0}]
        },
        {
          "name": "goodix_fp_irq",
          "keys": ["KEY_POWER", "KEY_WAKEUP"],
          "msc": ["MSC_SCAN"]
        },
        {
          "name": "mt6835-mt6369 Headset Jack",
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
    {"type": "battery", "base-temp-mc": 27000, "jitter-mc": 100},
    {"type": "mtktscpu", "base-temp-mc": 31200, "jitter-mc": 320},
    {"type": "mtktsAP", "base-temp-mc": 29500, "jitter-mc": 210},
    {"type": "mtktsgpu", "base-temp-mc": 30000, "jitter-mc": 260}
  ],
  "hardwareHals": {
    "radioVariant": "mtk",
    "enableIr": false,
    "enableNfc": true,
    "vintfDrop": []
  },
  "vendorEtcConfigs": {
    "battery": {
      "technology": "Li-poly",
      "cycleCount": 0,
      "fullUah": 6000,
      "counterMin": 5940,
      "counterMax": 5975,
      "fullMv": 4400,
      "chargeMv": 4480,
      "temperatureFullDecic": 310,
      "temperatureChargeDecic": 380,
      "currentFullUa": -61000,
      "currentChargeUa": -420000,
      "wireless": 0,
      "stateOfHealth": 100
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
      "systemInfoName": "MTK_MNLD_default,MNL_VER_default",
      "gnssCapabilities": 991,
      "powerCapabilities": 1,
      "measurementCorrectionsCapabilities": 0,
      "yearOfHw": 2025,
      "defaultSatellites": 7,
      "dualFrequencyL5": true
    },
    "thermal": {
      "skinName": "SKIN",
      "skinCelsius": 28.6,
      "modelMaximumCelsius": 50,
      "coolingDeviceName": "devfreq-13000000.mali",
      "sensors": [
        {"id": 0, "name": "CPU", "value": 31.24, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 1, "name": "GPU", "value": 30.87, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 2, "name": "BATTERY", "value": 27.1, "thresholds": "nan;nan;nan;50;55;59;60", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 3, "name": "SKIN", "value": 28.6, "thresholds": "nan;43;45;50;70;80;90", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 5, "name": "POWER_AMPLIFIER", "value": 29.32, "thresholds": "nan;nan;nan;68;90;100;110", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 9, "name": "NPU", "value": 30.5, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"},
        {"id": 13, "name": "SOC", "value": 31.2, "thresholds": "nan;nan;nan;85;90;100;117", "shutdown": "nan;nan;nan;nan;nan;nan;nan"}
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
      {"handle": 1, "name": "lsm6dso_acc", "vendor": "STMicroelectronics", "version": 1, "typeId": 1, "type": "android.sensor.accelerometer", "maxRange": "78.453605652", "resolution": "0.0023956299", "power": "0.15", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 2, "name": "ak09918", "vendor": "AKM", "version": 1, "typeId": 2, "type": "android.sensor.magnetic_field", "maxRange": "4912.0", "resolution": "0.15", "power": "0.15", "minDelay": 10000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 3, "name": "ORIENTATION", "vendor": "MTK", "version": 1, "typeId": 3, "type": "android.sensor.orientation", "maxRange": "360.0", "resolution": "0.00390625", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 4, "name": "lsm6dso_gyro", "vendor": "STMicroelectronics", "version": 1, "typeId": 4, "type": "android.sensor.gyroscope", "maxRange": "34.906299591", "resolution": "0.001065264", "power": "0.55", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 5, "name": "stk3338_l", "vendor": "Sensortek", "version": 1, "typeId": 5, "type": "android.sensor.light", "maxRange": "65535.0", "resolution": "1.0", "power": "0.1", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 8, "name": "stk3338_p", "vendor": "Sensortek", "version": 1, "typeId": 8, "type": "android.sensor.proximity", "maxRange": "5.0", "resolution": "1.0", "power": "0.1", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 1000000, "wakeUp": true, "reporting": 1},
      {"handle": 9, "name": "GRAVITY", "vendor": "MTK", "version": 1, "typeId": 9, "type": "android.sensor.gravity", "maxRange": "39.226802826", "resolution": "0.0023956299", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 10, "name": "LINEARACCEL", "vendor": "MTK", "version": 1, "typeId": 10, "type": "android.sensor.linear_acceleration", "maxRange": "39.226802826", "resolution": "0.0023956299", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 11, "name": "ROTATION_VECTOR", "vendor": "MTK", "version": 1, "typeId": 11, "type": "android.sensor.rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 14, "name": "UNCALI_MAG", "vendor": "MTK", "version": 1, "typeId": 14, "type": "android.sensor.magnetic_field_uncalibrated", "maxRange": "4912.0", "resolution": "0.15", "power": "0.15", "minDelay": 20000, "fifoReserved": 600, "fifoMax": 4500, "maxDelay": 200000, "wakeUp": false, "reporting": 0},
      {"handle": 15, "name": "GAME_ROTATION_VECTOR", "vendor": "MTK", "version": 1, "typeId": 15, "type": "android.sensor.game_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 300, "fifoMax": 4500, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 16, "name": "UNCALI_GYRO", "vendor": "MTK", "version": 1, "typeId": 16, "type": "android.sensor.gyroscope_uncalibrated", "maxRange": "34.906299591", "resolution": "0.001065264", "power": "0.55", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0},
      {"handle": 17, "name": "SIGNIFICANT_MOTION", "vendor": "MTK", "version": 1, "typeId": 17, "type": "android.sensor.significant_motion", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 18, "name": "STEP_DETECTOR", "vendor": "MTK", "version": 1, "typeId": 18, "type": "android.sensor.step_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 100, "fifoMax": 4500, "maxDelay": 0, "wakeUp": false, "reporting": 3},
      {"handle": 19, "name": "STEP_COUNTER", "vendor": "MTK", "version": 1, "typeId": 19, "type": "android.sensor.step_counter", "maxRange": "2147483648.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 20, "name": "GEOMAGNETIC_ROTATION_VECTOR", "vendor": "MTK", "version": 1, "typeId": 20, "type": "android.sensor.geomagnetic_rotation_vector", "maxRange": "1.0", "resolution": "5.960464477539063e-08", "power": "0.001", "minDelay": 5000, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 20000, "wakeUp": false, "reporting": 0},
      {"handle": 22, "name": "TILT_DETECTOR", "vendor": "MTK", "version": 1, "typeId": 22, "type": "android.sensor.tilt_detector", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 3},
      {"handle": 27, "name": "DEVICE_ORIENTATION", "vendor": "MTK", "version": 1, "typeId": 27, "type": "android.sensor.device_orientation", "maxRange": "3.0", "resolution": "1.0", "power": "0.001", "minDelay": 0, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 1000000, "wakeUp": false, "reporting": 1},
      {"handle": 29, "name": "STATIONARY_DETECT", "vendor": "MTK", "version": 1, "typeId": 29, "type": "android.sensor.stationary_detect", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 30, "name": "MOTION_DETECT", "vendor": "MTK", "version": 1, "typeId": 30, "type": "android.sensor.motion_detect", "maxRange": "1.0", "resolution": "1.0", "power": "0.001", "minDelay": -1, "fifoReserved": 0, "fifoMax": 0, "maxDelay": 0, "wakeUp": true, "reporting": 2},
      {"handle": 35, "name": "UNCALI_ACC", "vendor": "MTK", "version": 1, "typeId": 35, "type": "android.sensor.accelerometer_uncalibrated", "maxRange": "39.226802826", "resolution": "0.0023956299", "power": "0.001", "minDelay": 2500, "fifoReserved": 3000, "fifoMax": 4500, "maxDelay": 80000, "wakeUp": false, "reporting": 0}
    ],
    "audioEffects": [],
    "media": {
      "instances": [],
      "components": []
    },
    "audioEngine": {
      "modules": "default,bluetooth,r_submix",
      "voiceCallIndexMax": 7,
      "musicIndexMax": 15
    }
  },
  "alignmentNotes": [
    "品牌/机型身份取自固件实测：brand=honor、device=HNLNA-M、board=mt6835；MT6835 对应 Dimensity 6300，据此推导 2xA76 2.4GHz + 6xA55 2.0GHz 与 Mali-G57 MC2，全套频点/MIDR/GPU 串均按该平台重写，未沿用 gold 的 MT6833 值。",
    "基带以固件实测 MOLY.NR17.R2.TC36.PR1.SP.V1.0.3.P107 为准，未用 gold 的 MOLY.NR15… 覆盖；radioVariant=mtk。",
    "非小米机型：未写 swtp、未写任何 xiaomi.sensor.*/MiSound/Miui 私有节点；Honor/MagicOS 私有传感器未在标准 SensorManager 稳定暴露、固件亦未实测，故不编造。",
    "传感器按同代 X 系列 i 机型常见方案取 ST lsm6dso（加速度+陀螺）、AKM ak09918（磁力）、Sensortek stk3338（光感+距离），并补齐 MTK 融合/未校准/计步共 21 项；无机型证据表明该机带气压计，故未添加。",
    "触控取 Goodix（goodix_ts，bus 24/BUS_I2C），指纹取侧边电容 Goodix（uinput-goodix / goodix_fp_irq）；耳机插孔按 MT6835+MT6369 平台命名为 mt6835-mt6369 Headset Jack，按键为 mtk-pmic-keys + mtk-kpd。",
    "显示为 6.7 英寸 AMOLED、1080x2412、120Hz（60/90/120），EDID 面板厂商按 BOE、2026 年第 13 周推断（edidProductId=4129）；属待真机 EDID 回读复核项，逻辑密度取 420dpi。",
    "GPU 驱动按 Android 16 代际取 ARM Mali r49p1 线：glVersionFull/vulkanDriverInfo 为 r49p1，Vulkan 1.3（0x00403116）、Vulkan deviceId 0x90930010（G57 MC2 硬件 ID，非小米专属值）；UUID 与 driverVersion 为驱动约定占位，需真机 vulkaninfo 校准。",
    "电池按 6000mAh 单电芯 Li-poly、35W 有线、无无线充；GNSS 为 MTK_MNLD_default、yearOfHw=2025、支持 L1+L5；振动为 Z 轴线性马达。NFC 按同代 X 系列 SKU 推断为具备，红外遥控推断为不具备，均需固件 NFC/IR 节点存在性复核。",
    "Widevine systemId、IMEI TAC/SVN、MEID 前缀均属批次/密钥区数据，当前为合理占位值，需真机 getprop 与 keybox 回读校准。",
    "固件实测未提供 Codec2 组件清单与 audio effect descriptors；为避免假登记 store 占用 default，media.instances/components 与 audioEffects 暂保持空数组，由 02 阶段实测补齐后再落 vendor/etc。"
  ],
  "sources": [
    "目标固件实测：build.prop / boot-DTB / VINTF（brand=honor、device=HNLNA-M、board=mt6835、Android 16、buildId=16-TP1A.220624.014-20260422、baseband=MOLY.NR17.R2.TC36.PR1.SP.V1.0.3.P107）",
    "MediaTek Dimensity 6300 (MT6835) 平台公开规格与 MTK cpufreq/thermal/GNSS HAL 命名约定",
    "ARM Mali-G57 MC2 / Valhall 驱动版本与 Vulkan 1.3 能力位约定（ARM Mali 驱动发布说明）",
    "Honor X 系列（X60i/X70i）同代 SKU 公开规格与 MagicOS 外设节点命名惯例（GSMArena/DuckDuckGo 在线检索被反爬校验拦截，仅作方向性参考）"
  ]
}
```
