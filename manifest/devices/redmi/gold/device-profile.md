# 真机硬件规格与云机对齐留档 · Redmi Note 13 5G (redmi/gold)

- **生成时间**: `2026-11-17T17:13:08+08:00`
- **ROM Build ID**: `15-OS3.0.11.0.VNQCNXM`
- **DSH 聊天会话 ID**: `session-21b38ad5-5ab3-409f-89ab-0062bac13c8a`（标题：`[02_prepare] 真机规格采集 · Redmi Note 13 5G (redmi/gold · 15-OS3.0.11.0.VNQCNXM)`）
- **采集模型路由**: `cdh-maas / deepseek-v4.1-flash-1017`（复用在线 DSH Web: `true`）

---

## 一、设备表对齐规格（对应 `devices/manifest/devices/redmi/gold.json`）

| 字段 | 对齐值 | 说明 |
| --- | --- | --- |
| `official` | `小米` | 厂商集团 |
| `brand` | `redmi` | 子品牌标识 |
| `model` | `Redmi Note 13 5G` | 商业上市机型名 （型号代码: `2312DRAABC`, `2312DRAABG`） |
| `device` | `gold` | 设备代号 |
| `platform` / `board` | `mtk` / `mt6833` | 芯片平台 / 主板代号 |
| `resolution` | `1080x2400` | 屏幕物理像素分辨率 |
| `inch` | `70x155` | 屏幕可视物理宽×高 mm（对角线 `6.67"`） |
| `density` | `480` | Android 逻辑密度 dpi（物理 PPI `395`，xdpi `394.40` / ydpi `393.40`） |
| `smp` | `8` | CPU 核心总数 |
| `memory` | `[6144 8192 12288]` | 全系发售内存档位 MB（6G / 8G / 12G，LPDDR4X） |
| `storage` | `[128000 256000]` | 全系发售存储档位（128G / 256G，UFS 2.2） |

- **常见量产发售版本组合**: `6GB+128GB` / `8GB+128GB` / `8GB+256GB` / `12GB+256GB`

## 二、真机深层硬件对齐画像（参考 `redmi/gold` 标准）

### 1. SoC 与 CPU 簇全量频点表（`cdh_cpufreq` / `module/config.json` 对齐）
- **SoC 芯片**: MediaTek Dimensity 6080 (`MT6833` MT6833V/PNZA, 6nm)
- **CPU 拓扑**: `6x Cortex-A55 @ 2.0GHz + 2x Cortex-A76 @ 2.4GHz`（驱动: `mt-cpufreq`，调速器: `sugov_ext`，可选调速器: `[sugov_ext performance powersave userspace]`，BogoMIPS: `26.00`）

| 策略簇 | 核心列表 | 微架构 | Part / MIDR | 容量权重 | 最小 / 驻留 / 最大 (kHz) | 频点档数 |
| --- | --- | --- | --- | --- | --- | --- |
| policy0 | `[0 1 2 3 4 5]` | Cortex-A55 | `0xd05` (`0x00000000412fd050`) | `405` | `500000 / 950000 / 2000000` | `16 档` |
| policy6 | `[6 7]` | Cortex-A76 | `0xd0b` (`0x00000000414fd0b0`) | `1024` | `725000 / 1274000 / 2400000` | `17 档` |

- **`policy0` (`Cortex-A55`, cpus `[0 1 2 3 4 5]`) 全量 `available-frequencies` (16 档, kHz)**:
  `[2000000 1916000 1812000 1750000 1645000 1500000 1390000 1280000 1115000 1032000 950000 840000 730000 675000 620000 500000]`
- **`policy6` (`Cortex-A76`, cpus `[6 7]`) 全量 `available-frequencies` (17 档, kHz)**:
  `[2400000 2203000 2087000 1995000 1903000 1788000 1719000 1650000 1534000 1418000 1274000 1129000 1042000 985000 898000 840000 725000]`

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
      "userspace"
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
        "cur-freq": 950000,
        "available-frequencies": [
          2000000,
          1916000,
          1812000,
          1750000,
          1645000,
          1500000,
          1390000,
          1280000,
          1115000,
          1032000,
          950000,
          840000,
          730000,
          675000,
          620000,
          500000
        ],
        "implementer": "0x41",
        "variant": 2,
        "part": "0xd05",
        "revision": 0,
        "capacity": 405,
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
        "cur-freq": 1274000,
        "available-frequencies": [
          2400000,
          2203000,
          2087000,
          1995000,
          1903000,
          1788000,
          1719000,
          1650000,
          1534000,
          1418000,
          1274000,
          1129000,
          1042000,
          985000,
          898000,
          840000,
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

### 2. 显示面板、GPU 与图形栈（`gpu_id.cfg` / `buildVendor` 参考）
- **面板类型与刷新率**: AMOLED · 刷新率 `[60 90 120] Hz`（默认 `120 Hz`） · 广色域 `true` · EDID 厂商提示 `MTK`
- **OpenGL ES**: `ARM` / `Mali-G57 MC2` / `OpenGL ES 3.2`
- **Vulkan**: `ro.hardware.vulkan=mali` · VendorID `0x13b5` · API `1.1`

### 3. 触控、指纹、物理传感器与外设（`input_identity` / Sensors HAL 参考）
- **指纹方案**: 侧边电容指纹 (FPC / Goodix)
- **常见触控 IC 驱动名**: `goodix_ts`, `fts_ts`
- **物理传感器清单**: `accelerometer`, `gyroscope`, `magnetometer`, `ambient_light`, `proximity`, `ir_blaster`
- **NFC / 红外遥控 / 3.5mm 耳机孔 / 扬声器**: NFC=`false` · 红外=`true` · 3.5mm=`true` · 单扬声器
- **影像规格**: 后置 100MP + 2MP | 前置 16MP
- **电池与通信**: `5000 mAh`（有线 `33W` / 无线 `0W`） · Wi-Fi 5 (802.11ac)（Wi-Fi 6=`false`） · BT `5.3` · 5G=`true` · Widevine `L1` · MAC OUI `[3c:38:24 64:cc:2e 78:11:dc 28:6c:07]`

### 4. AI 真机对齐提示
- 本机为 gold 的国内（CN）SKU：固件 VNQCNXM、型号 2312DRAABC；海外孪生 2312DRAABG（2024-01 上市）为 108MP+8MP+2MP 三摄且带 NFC，其余硬件同源，跨 ROM 对齐时勿混用相机/NFC 字段。
- 大簇频点是本任务最大坑点：vendor_boot/dtbo FDT 实测大簇最高仅 2203000 kHz（天玑 700 档低频底座），而天玑 6080 公开最高 OPP 为 2400000 kHz，vendor/etc 电源/温度表亦出现 2400000。故取 DTB 16 档保序 + 顶部补 2400000 = 17 档；2306000 在 DTB 与 vendor 频点线索中均不存在，不编造，避免 cdh_cpufreq 暴露非原厂频点。
- 小簇 16 档严格按 DTB 原表（2000000…500000），未采用部分第三方站点的 1287000/968000/756000 变体；vendorEtcFreqHintsKhz 中 1287000/1162000/1075000 属于温控/GPU/CCI 等旁路线索，不作为 CPU 簇 OPP 直接写入。
- EDID 合成：物理可视区约 69.6 × 154.6 mm，整数化取 widthMm=70、heightMm=155；屏占比高（机身为 161.1 × 74.95 × 7.6 mm，重约 173.5 g），EDID 只描述显示区，不要用机身尺寸。
- densityDpi=480 与物理 PPI≈395 不矛盾：HyperOS 在 1080×2400 上做逻辑放大，xdpi/ydpi 必须用物理值（约 394），否则 QEMU 显示比例与字体发虚。
- GPU 侧 ro.hardware.egl=meow、ro.hardware.vulkan=mali；Vulkan VendorID 0x13b5（ARM）。Mali-G57 MC2 属 Valhall，规范上限 Vulkan 1.1，禁止按 1.3 上报（否则部分需要 Vulkan 1.3 特性的 App 会走到未实现路径）。gpu_id.cfg 的 GL_RENDERER 精确串为 "Mali-G57 MC2"，不要写成 "Mali-G57" 或 "Mali-G57 MC2 (MT6833)"。
- 通信：天玑 6080 仅 Wi-Fi 5（2.4/5GHz 802.11ac），无 Wi-Fi 6/6E；蓝牙官方标称 5.3（SoC 参考设计为 BT 5.1/5.2 档），云机上报取 5.3。5G 为 Sub-6 双卡，无毫米波。
- 国内版无 NFC（NFC 仅海外部分区域 SKU），红外遥控国内版标配；若伪装目标为海外 ROM，需同步把 nfc 置 true 并调整相机为 108MP 三摄。
- 存储/内存只有 128GB/256GB 与 6/8/12GB 四档组合，不要生成 512GB/1TB 或 16GB SKU（该机不存在，manifest 多出容量会在 02 校验阶段与真机 dump 对不上）。
- kernelUnamne 实测 6.6.118-android15-8-…，与清单 kernel="6.6"、lineage-22.2 内核线同代，无需借用登记；GPU/CPU 频点与内核代际无关，可直接复用本表。

## 三、官方固件包解包实测对账（`build.prop` + `boot.img`）

- `marketName`: `` | `modelCode`: `gold` | `manufacturer`: `Xiaomi`
- `board`: `mt6833` | `soc`: `  ()`
- `densityDpi`: `480` | `egl`: `meow` | `vulkan`: `mali` | `abilist`: `arm64-v8a,armeabi-v7a,armeabi`
- `securityPatch`: `2026-02-01` | `buildDate`: `20260924` | `kernelLine`: `6.6`
- `uname`: `Linux localhost 6.6.118-android15-8-ge56cf6b09cca-ab15511674-4k #1 SMP PREEMPT Thu May 28 07:48:46 UTC 2026 aarch64 Toybox`
- `fingerprint`: `Redmi/vnd_gold/gold:15/AP3A.240905.015.A2/OS3.0.11.0.VNQCNXM:user/release-keys`

## 四、DSH 聊天界面回复原文

## 真机规格核验小结（Redmi Note 13 5G / `gold`）

- **身份**：小米集团旗下 Redmi（红米）的国内机型，研发代号 `gold`，国内型号 `2312DRAABC`，海外孪生型号 `2312DRAABG`；2023-09 在国内发布并开售，海外版 2024-01 上市。`15-OS3.0.11.0.VNQCNXM` 属于**国内（CN）固件**，因此下列参数以国内版为准，海外差异在 `alignmentNotes` 中单独标注。
- **屏幕**：屏幕为 6.67 英寸柔性直屏，像素分辨率为 1080×2400，20:9，刷新率为 60/90/120Hz。实测 PPI≈395；物理可视区约 69.6×154.6 mm，按整数口径记为 70×155 mm 供 EDID 合成。固件 `densityDpi=480` 是 HyperOS 的**逻辑密度上采样**结果，并非物理密度，因此不能拿它反算 xdpi/ydpi。
- **SoC**：联发科天玑 6080（MT6833，6nm），2×Cortex-A76@2.4GHz + 6×Cortex-A55@2.0GHz，Mali-G57 MC2。
- **频点对齐的关键结论（重要）**：本包 DTB 实测大簇**只到 2203000 kHz**（这是天玑 700 级别的低频基础配置，不含 2.4GHz 高频段），而天玑 6080 的大核公开最高频是 2400000 kHz，`vendorEtcFreqHintsKhz` 中也恰好出现 `2400000`。因此大簇采用「DTB 全量中低频段 + 补 2400000 最高档」= 17 档；**不凭空编造 2306000**：实测 DTB 与 vendor 温度/电源表中都没有该频点，强行写入反而会让 `cdh_cpufreq` 上报一个厂商内核从不驻留的频点。小簇 16 档完全采用 DTB 原表，把 500000 到 2000000 的完整频点全部保留。
- **内存/存储**：LPDDR4X + UFS 2.2，国内发售 6/8/12GB RAM × 128/256GB ROM（无 16GB、无 512GB、无 1TB）。
- **易错点**：国内版为 **1 亿像素主摄 + 2MP 景深**；海外版才是 108MP 三摄；国内版**无 NFC**（海外版有）、**有红外遥控**；仅 Wi-Fi 5，不支持 Wi-Fi 6；Vulkan 按 Mali-G57 的 Valhall 架构能力上报 **1.1**，不要写成 1.3。

```json
{
  "official": "小米",
  "brand": "redmi",
  "device": "gold",
  "model": "Redmi Note 13 5G",
  "modelAliases": ["2312DRAABC", "2312DRAABG"],
  "releaseDate": "2023-09",
  "platform": "mtk",
  "board": "mt6833",
  "soc": {
    "manufacturer": "MediaTek",
    "marketingName": "Dimensity 6080",
    "model": "MT6833",
    "partNumber": "MT6833V/PNZA",
    "processNm": 6
  },
  "display": {
    "diagonalInch": 6.67,
    "resolution": "1080x2400",
    "widthPx": 1080,
    "heightPx": 2400,
    "aspectRatio": "20:9",
    "widthMm": 70,
    "heightMm": 155,
    "ppi": 395,
    "xdpi": 394.4,
    "ydpi": 393.4,
    "androidDensityDpi": 480,
    "panelType": "AMOLED",
    "refreshRatesHz": [60, 90, 120],
    "defaultRefreshRateHz": 120,
    "wideColorGamut": true,
    "edidVendorHint": "MTK"
  },
  "cpu": {
    "smp": 8,
    "architecture": "arm64-v8a",
    "topologySummary": "6x Cortex-A55 @ 2.0GHz + 2x Cortex-A76 @ 2.4GHz",
    "driver": "mt-cpufreq",
    "governor": "sugov_ext",
    "availableGovernors": ["sugov_ext", "performance", "powersave", "userspace"],
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
        "curFreqKhz": 950000,
        "availableFrequenciesKhz": [
          2000000, 1916000, 1812000, 1750000, 1645000, 1500000, 1390000, 1280000,
          1115000, 1032000, 950000, 840000, 730000, 675000, 620000, 500000
        ],
        "implementer": "0x41",
        "variant": 2,
        "part": "0xd05",
        "revision": 0,
        "capacity": 405,
        "midr": "0x00000000412fd050",
        "revidr": "0x0000000000000000"
      },
      {
        "policy": 6,
        "cpus": [6, 7],
        "coreName": "Cortex-A76",
        "minFreqKhz": 725000,
        "maxFreqKhz": 2400000,
        "curFreqKhz": 1274000,
        "availableFrequenciesKhz": [
          2400000, 2203000, 2087000, 1995000, 1903000, 1788000, 1719000, 1650000,
          1534000, 1418000, 1274000, 1129000, 1042000, 985000, 898000, 840000, 725000
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
    "vulkanHardware": "mali",
    "vulkanVendorId": "0x13b5",
    "vulkanApiVersion": "1.1"
  },
  "memory": {
    "ramType": "LPDDR4X",
    "variantsGb": [6, 8, 12],
    "variantsMb": [6144, 8192, 12288]
  },
  "storage": {
    "storageType": "UFS 2.2",
    "variantsGb": [128, 256],
    "variantsManifestMb": [128000, 256000],
    "skuCombinations": ["6GB+128GB", "8GB+128GB", "8GB+256GB", "12GB+256GB"]
  },
  "peripherals": {
    "fingerprint": "侧边电容指纹 (FPC / Goodix)",
    "touchControllerHints": ["goodix_ts", "fts_ts"],
    "sensors": ["accelerometer", "gyroscope", "magnetometer", "ambient_light", "proximity", "ir_blaster"],
    "nfc": false,
    "infrared": true,
    "audioJack35mm": true,
    "speakers": "单扬声器"
  },
  "camera": {
    "rear": "100MP + 2MP",
    "front": "16MP"
  },
  "batteryAndConnectivity": {
    "batteryMah": 5000,
    "wiredChargingW": 33,
    "wirelessChargingW": 0,
    "wifi": "Wi-Fi 5 (802.11ac)",
    "wifi6Supported": false,
    "bluetooth": "5.3",
    "cellular5G": true,
    "macOuiCandidates": ["3c:38:24", "64:cc:2e", "78:11:dc", "28:6c:07"],
    "widevineDefault": "L1"
  },
  "alignmentNotes": [
    "本机为 gold 的国内（CN）SKU：固件 VNQCNXM、型号 2312DRAABC；海外孪生 2312DRAABG（2024-01 上市）为 108MP+8MP+2MP 三摄且带 NFC，其余硬件同源，跨 ROM 对齐时勿混用相机/NFC 字段。",
    "大簇频点是本任务最大坑点：vendor_boot/dtbo FDT 实测大簇最高仅 2203000 kHz（天玑 700 档低频底座），而天玑 6080 公开最高 OPP 为 2400000 kHz，vendor/etc 电源/温度表亦出现 2400000。故取 DTB 16 档保序 + 顶部补 2400000 = 17 档；2306000 在 DTB 与 vendor 频点线索中均不存在，不编造，避免 cdh_cpufreq 暴露非原厂频点。",
    "小簇 16 档严格按 DTB 原表（2000000…500000），未采用部分第三方站点的 1287000/968000/756000 变体；vendorEtcFreqHintsKhz 中 1287000/1162000/1075000 属于温控/GPU/CCI 等旁路线索，不作为 CPU 簇 OPP 直接写入。",
    "EDID 合成：物理可视区约 69.6 × 154.6 mm，整数化取 widthMm=70、heightMm=155；屏占比高（机身为 161.1 × 74.95 × 7.6 mm，重约 173.5 g），EDID 只描述显示区，不要用机身尺寸。",
    "densityDpi=480 与物理 PPI≈395 不矛盾：HyperOS 在 1080×2400 上做逻辑放大，xdpi/ydpi 必须用物理值（约 394），否则 QEMU 显示比例与字体发虚。",
    "GPU 侧 ro.hardware.egl=meow、ro.hardware.vulkan=mali；Vulkan VendorID 0x13b5（ARM）。Mali-G57 MC2 属 Valhall，规范上限 Vulkan 1.1，禁止按 1.3 上报（否则部分需要 Vulkan 1.3 特性的 App 会走到未实现路径）。gpu_id.cfg 的 GL_RENDERER 精确串为 \"Mali-G57 MC2\"，不要写成 \"Mali-G57\" 或 \"Mali-G57 MC2 (MT6833)\"。",
    "通信：天玑 6080 仅 Wi-Fi 5（2.4/5GHz 802.11ac），无 Wi-Fi 6/6E；蓝牙官方标称 5.3（SoC 参考设计为 BT 5.1/5.2 档），云机上报取 5.3。5G 为 Sub-6 双卡，无毫米波。",
    "国内版无 NFC（NFC 仅海外部分区域 SKU），红外遥控国内版标配；若伪装目标为海外 ROM，需同步把 nfc 置 true 并调整相机为 108MP 三摄。",
    "存储/内存只有 128GB/256GB 与 6/8/12GB 四档组合，不要生成 512GB/1TB 或 16GB SKU（该机不存在，manifest 多出容量会在 02 校验阶段与真机 dump 对不上）。",
    "kernelUnamne 实测 6.6.118-android15-8-…，与清单 kernel=\"6.6\"、lineage-22.2 内核线同代，无需借用登记；GPU/CPU 频点与内核代际无关，可直接复用本表。"
  ],
  "sources": [
    "小米官网 Redmi Note 13 5G 规格页 https://www.mi.com/redmi-note-13-5g/specs（页面为动态渲染，本次仅取到导航壳，参数以公开规格记忆交叉核对）",
    "MediaTek Dimensity 6080 产品规格（6nm、2×A76@2.4GHz + 6×A55@2.0GHz、Mali-G57 MC2、Wi-Fi 5）",
    "本包实测：vendor_boot.img / dtbo.img FDT 的 measuredCpuClusters，以及 vendor/etc 的 vendorEtcFreqHintsKhz",
    "GSMArena Redmi Note 13 5G 参数页（本次抓取被 Turnstile 人机校验拦截，仅作交叉参考，未采信其单页数值）"
  ]
}
```

**两处需要你决定的判断口径**（如果不一致，我改一版即可）：① 大簇“补 2400000”还是“严格照 DTB 停在 2203000”；② 相机按国内 100MP 还是按海外 108MP 三摄。当前 JSON 选的是「CN 口径 + 补最高档」。
