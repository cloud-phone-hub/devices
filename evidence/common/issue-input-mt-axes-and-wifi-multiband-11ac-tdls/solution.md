# #189：`goodix_ts` 8 轴多点触控 `MotionRange` 对齐与 Wi-Fi 跨频 2.4G/5G 扫描 / `11ac` / TDLS 能力对齐

## 1. 问题

在 `gold`（`15-OS3.0.10.0.VNQCNXM`）输入外设与无线网络能力的深水区比对中，发现两项底层硬件能力特征差异：

1. **触摸屏 `goodix_ts` 的 `InputDevice.getMotionRanges()` 轴数不足**：
   - 真机 `goodix_ts` 上报 **8 个** `MotionRange`（`AXIS_X(0)`、`AXIS_Y(1)`、`AXIS_PRESSURE(2)`、`AXIS_SIZE(3)`、`AXIS_TOUCH_MAJOR(4)`、`AXIS_TOUCH_MINOR(5)`、`AXIS_TOOL_MAJOR(6)`、`AXIS_TOOL_MINOR(7)`）；
   - 云机 `virtio-input` 经 `cdh_input_identity` 伪装名称与 VID/PID 后，仅声明了基础 `ABS_MT_POSITION_X/Y`，导致 Android `InputReader` 只生成 **2 个** `MotionRange`（缺压力与接触面长短轴）。
2. **Wi-Fi 跨频扫描结果数、TDLS 与 Wi-Fi 标准代际不匹配**：
   - 真机（MT6833 天玑 6080 集成 Wi-Fi 5 基带）`WifiManager.getScanResults()` 同时扫出 2.4GHz 与 5GHz 双频段热点，`is5GHzBandSupported() = true`、`isTdlsSupported() = true`、`isWifiStandardSupported(WIFI_STANDARD_11AC) = true`、**`isWifiStandardSupported(WIFI_STANDARD_11AX) = false`**；
   - 云机 `mac80211_hwsim` 虚拟无线驱动默认只在当前关联的 2.4GHz 信道（2437 MHz）收到 1 个同信道 Beacon，5GHz 邻区（`5180`/`5220` MHz）扫描为空（`scanResultCount = 1`），且未置位 `WIPHY_FLAG_SUPPORTS_TDLS`、默认开启了 802.11ax HE 能力（`std11axSupported = true`，与 MT6833 硬件规格不符）。

## 2. 根因

1. **Android `MultiTouchInputMapper` 依赖底层 `evdev` `ABS_MT_*` 轴位图合成 8 项 `MotionRange`**：
   - 当内核 `input_dev` 未调用 `input_set_abs_params()` 设置 `ABS_MT_PRESSURE`、`ABS_MT_TOUCH_MAJOR`、`ABS_MT_TOUCH_MINOR`、`ABS_MT_WIDTH_MAJOR`、`ABS_MT_WIDTH_MINOR` 时，`MultiTouchInputMapper::configureMotionRanges()` 不会注册 `AXIS_PRESSURE`、`AXIS_SIZE`、`AXIS_TOUCH_*`、`AXIS_TOOL_*` 6 个维度。
2. **`0002-hwsim-fake-wifi.patch` 扫描注入与 `wiphy` 能力位限制**：
   - 原补丁注入伪造扫描帧时按单信道接收过滤，扫描请求扫过 5GHz 信道时未注入 `wifi-aps.json` 配置的 5GHz AP（`5180`/`5220` MHz）Beacon/ProbeResp；
   - `mac80211_hwsim` 创建 `hw->wiphy` 时默认未开启 `WIPHY_FLAG_SUPPORTS_TDLS`，且无条件注册 `sband->iftype_data` 的 `he_cap.has_he = true`（Wi-Fi 6 `802.11ax`），没有为仅支持到 Wi-Fi 5（`802.11ac`）的 SoC 提供屏蔽开关。

## 3. 证据

- 失败日志见 [`log-fail.txt`](log-fail.txt)：
  - 修复前 `goodix_ts` 仅有 `X`/`Y` 2 个轴；`network.wifilist` 仅返回 1 个 2.4GHz AP，`isTdlsSupported = false`，`std11axSupported = true`。
- 通过日志见 [`log-pass.txt`](log-pass.txt)：
  - 全新干净盘双机复测（`run-00014..00020`）中，`sensors_input.inputs` 实测 `goodix_ts` **`motionRangesCount = 8`**（`axis 0..7` 齐全，屏幕物理对角线 `2631.805`），7 个输入设备名称/描述符/sources 与真机一致，`noSuspiciousVirtualInput = true`。
  - `network.wifi` 与 `network.wifilist` 实测 `connected = true`（`TP-LINK_2464`，`74:39:89:0b:24:63`），**`scanResultCount = 4`**（同时涵盖 2.4GHz `2412`/`2437` MHz 与 5GHz `5180`/`5220` MHz 双频 4 个 AP），`is5GHzBandSupported = true`、`isTdlsSupported = true`、`std11acSupported = true`、**`std11axSupported = false`**，与 MT6833 真机 **100% 一致**。

## 4. 改动

1. **`cdh_input_identity` 补齐多点触控 8 轴参数**：
   - [`devices/modules/input_identity/cdh_input_identity.c`](../../../modules/input_identity/cdh_input_identity.c)：在接管目标触摸屏 `input_dev` 时，除 `ABS_MT_POSITION_X/Y` 与 `ABS_MT_SLOT` / `ABS_MT_TRACKING_ID` 外，同步通过 `input_set_abs_params()` 注册 `ABS_MT_TOUCH_MAJOR`（`0..255`）、`ABS_MT_TOUCH_MINOR`（`0..255`）、`ABS_MT_WIDTH_MAJOR`（`0..255`）、`ABS_MT_WIDTH_MINOR`（`0..255`）、`ABS_MT_PRESSURE`（`0..255`）与 `ABS_MT_ORIENTATION`（`0..1`），使 Android `InputReader` 完整构建 `axis 0..7` 共 8 个 `MotionRange`。
2. **`mac80211_hwsim` 跨频段 2.4G/5G 扫描注入、TDLS 标志与可选屏蔽 `11ax` HE**：
   - [`devices/kernels/patches/lineage-22-6.6/0002-hwsim-fake-wifi.patch`](../../../kernels/patches/lineage-22-6.6/0002-hwsim-fake-wifi.patch)：
     - 支持在软件扫描期间按每个虚拟 AP 自身的频点（`2412`/`2437`/`5180`/`5220` MHz 等）匹配对应 `ieee80211_channel` 注入跨频段 Beacon/ProbeResp 帧；
     - 为 `hw->wiphy->flags` 置上 `WIPHY_FLAG_SUPPORTS_TDLS`；
     - 新增模块参数 `disable_he`（`bool`），置 `1` 时剥离 2.4GHz/5GHz band 的 `he_cap.has_he`，使上层识别最高标准为 `802.11ac`（Wi-Fi 5）。
3. **Wi-Fi 启动脚本、HAL 与机型配置联动**：
   - [`engine/vendor/hardware/android.service.daemon/vendor/bin/cph-netmgr-setup`](../../../vendor/hardware/android.service.daemon/vendor/bin/cph-netmgr-setup) 与 [`cph-wifi-bootstrap`](../../../vendor/hardware/android.service.daemon/vendor/bin/cph-wifi-bootstrap)：读取 `ro.vendor.cdh.wifi.disable_he` 动态传递或写入 `/sys/module/mac80211_hwsim/parameters/disable_he`，并在初始连接后触发全频段扫描缓存预热。
   - [`engine/vendor/hardware/android.hardware.wifi/src/wifi_service.cpp`](../../../vendor/hardware/android.hardware.wifi/src/wifi_service.cpp)：同步对齐芯片与 STA 特性标志位（含 5GHz 与 TDLS 能力位）。
   - [`devices/manifest/configs/redmi/gold/gold-android15.json`](../../../manifest/configs/redmi/gold/gold-android15.json)：在 `build-prop-sets` 中声明 `"ro.vendor.cdh.wifi.disable_he": "1"`。

## 5. 验证

1. 重跑 `02_build_kernel`（`6.6-cdh-eceba5903006`）→ `03_build_modules` → `04..11` 全链路构建，其中 `08_build_vendor_dlkm`（`da16d2bb9d0f`）清空 99 个基座 `.ko`、回读复核无残留 `.ko`、12 个自研/in-tree 模块依赖闭包成立并全量重建五份索引。
2. 全新干净盘冷启动实例 `1`，`boot_check.py` `[pass]`，`cmd wifi list-scan-results` 稳定返回 2.4GHz + 5GHz 全部 4 个 AP。
3. 双机复测 `hardware.inputs`、`aligner_xiaomi_gold.sensors_input`（`D1..D4` 全绿）、`network.wifi`、`network.wifilist`、`aligner_xiaomi_gold.network_telephony_bt_gnss`（`G1..G4` 全绿）全部通过。

## 6. 遗留

- 支持 Wi-Fi 6/7 的高端 SoC 机型（如骁龙 8 Elite 的 `15r`、`popsicle`、`sun`）不设置 `ro.vendor.cdh.wifi.disable_he`（默认 `0`），自动保持 `std11axSupported = true`；仅 Wi-Fi 5 的中端机型在清单中配 `"ro.vendor.cdh.wifi.disable_he": "1"` 即可复用。
