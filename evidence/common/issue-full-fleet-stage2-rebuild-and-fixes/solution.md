# #292 全仓第二阶段 29 版本全量重建、引擎/HAL/机型系统性排障与华为纯血鸿蒙实测取证

- **编号**：#292
- **分组**：`common/`（跨 A13~A17 五代底座、涉及引擎流水线 / HIDL 蓝牙 / 显示脚本 / 27 台设备硬件通道与 9 台机型配置）
- **日期**：2026-11-21
- **关联条目**：接续 [#291](../issue-vendor-lineage-full-fleet-align-and-grf-fixes/solution.md)（第一阶段 17 份历史未对齐模版 / 18 版对齐与重建），合拢全仓 39 台机型、42 份配置模版、47 个 ROM 版本的全量 `--refresh` 重建、Enforcing 冷启动体检与人工审图闭环。

---

## 一、问题

在 [#291](../issue-vendor-lineage-full-fleet-align-and-grf-fixes/solution.md) 完成全仓 17 份历史未对齐模版（18 个 ROM 版本）的 `vendor → Lineage` 对齐重建后，全仓剩余 **29 个 ROM 版本**（覆盖 `lineage-20.0`、`lineage-21.0`、`lineage-22.2`、`lineage-23.2`、`lineage-24.0` 五代底座，以及 2 款华为固件包）仍需按同一标准逐一执行 `--refresh` 全量重建、在独立测试 `host-daemon` 上以 `SELinux=Enforcing` 冷启动体检（查进程崩溃、重启循环、HAL 在线与 `boot_completed=1`）并人工审图确认亮屏进桌面。

全量拉通排查过程中，集中暴露出 **12 类横跨引擎流水线、通用 HAL、机型配置与固件体系的卡点**：

1. **Android 13 底座（`lineage-20.0`）蓝牙循环 `SIGABRT`**：`ASUS_AI2205` 等 A13 底座机型在 `bluetooth@1.1` 初始化期持续崩溃，表现为 `com.android.bluetooth` 启动即 `SIGABRT`。
2. **`symphony/MAX_10` 亮屏前 `SimSalesTracker` 越界崩溃黑屏**：首启时 `SimSalesTracker.readProInfo` 读 NVRAM 数组越界，阻塞正常亮屏进桌面。
3. **27 台新注册机型缺失蓝牙/触觉 HVC 通道**：`devices/manifest/devices/<vendor>/<device>.json` 的 `extra.hostHvc` 缺失 `host-hvc-bluetooth`（`hvc5`）与 `host-hvc-haptic`（`hvc6`），导致对应机型蓝牙 HCI RESET 超时 `SIGABRT` 且触觉后端缺失。
4. **`asus/ASUSAI2501` 与 `oppo/CPH2797` `com.android.bluetooth` LE Audio `SIGABRT`**：系统日志持续打印 `LE Audio Client requires Bluetooth Audio HAL V2.1 at least`，导致蓝牙进程循环崩溃。
5. **`blackview/XPLORE_X1` `art_exec` 触发 `dex2oat32` `Exec format error`**：纯 64 位云机环境尝试拉起 32 位 `dex2oat32` 持续报错。
6. **`oneplus/CPH2767`、`oppo/CPH2797`、`xiaomi/popsicle-android16` `vendor-cil-rewrite` 命中 0 次报错**：历史机型补丁与引擎通用 CIL 清洗器重复，构建期 `patch` 阶段报错。
7. **`google/oriole` Android 17（`lineage-24.0`）冷启动黑屏**：日志持续报 `gralloc-mapper is missing`，同时缺失 `nfc@1.0` 及 Tensor 平台属性。
8. **`oppo/CPH2797` v1 `keystore2` 循环 `SIGABRT` 引发重启循环**：官方 `/odm` 分区携带 3 份 NXP StrongBox VINTF 清单，云机无实体 StrongBox 芯片导致 `keystore2` 反复崩溃触发系统重启。
9. **`realme/RMX6688` v1 26 项 VINTF 冲突触发 `NULL VINTF MANIFEST`**：直通官方 `/odm` 场景下，引擎合成的 `cdh_vendor_placeholders.xml` 与官方 `/odm/etc/vintf/manifest/*.xml` 存在 26 项重复 FQInstance，直接导致 `vold` / `keystore2` 卡死无法启动。
10. **`realme/RMX6688` v2/v3 `com.android.nfc` 每 3 秒 `SIGSEGV`**：`libsn100nfc_nci.so` 中 `SN100HalHelper::halGetProperty` 空指针崩溃，原因是清单中 `NfcNci` 屏蔽路径写错。
11. **`tcl/ericanppro` v1 `SurfaceFlinger` 循环 `gralloc-mapper is missing`**：官方 `vendor_a.img` 为 ext4 格式且按需提取，导致引擎未能识别 `mapper.mediatek.so`，装配错配为 `mapper.minigbm.so` 并触发 SELinux 拒绝与黑屏。
12. **`huawei/HBN-AL00`（Pura 70 Pro）与 `huawei/HED-AL00`（Pura 80）停在 `build_kernel`**：两款固件包无法在 `03_build_kernel` 提取 Linux 内核版本，也无法在 Android 构建流水线继续。

---

## 二、根因

1. **A13 HIDL 蓝牙命令集不全**：`bluetooth_hidl_service.cpp` 返回的 Local Supported Commands 位图及指令分发缺少 `Read Encryption Key Size`（`0x1408`）、BQR VSC（`0xfd5e`）及通用厂商命令段（`0xfc00+`）的应答，A13 蓝牙协议栈严格校验响应后直接 `abort()`。
2. **联发科/传音系 `SimSalesTracker` NVRAM 读取假设**：`symphony/MAX_10` 读系统全局属性 `sales_nvram_flag` 未置位时会读取空/畸形 NVRAM 记录导致数组越界。
3. **`hostHvc` 缺少注册期默认补齐与运行期双层兜底**：早期 `02_prepare/register_parse.go` 生成设备清单未写入 `host-hvc-bluetooth`/`host-hvc-haptic`，`internal/device/db_bridge.go` 转运行时设备对象时也未自动兜底补齐。
4. **高版本 OEM 蓝牙默认启用 LE Audio 客户端**：`ASUSAI2501`（A15）与 `CPH2797`（A16）默认开启 LE Audio Profile，但云机蓝牙音频 HAL 仅提供经典蓝牙 Audio 路径，未提供 LE Audio HAL V2.1+，导致协议栈主动 `SIGABRT`。
5. **`dex2oat64` 未显式开启**：`blackview/XPLORE_X1` 未在 `vendor/config.json` 中显式设置 `dalvik.vm.dex2oat64.enabled=true`，导致 `art_exec` 尝试回退执行 32 位 `dex2oat32`。
6. **历史 `vendor-cil-rewrite` 规则已由引擎通用 CIL 清洗器接管**：重复规则匹配 0 次后按严格模式中断构建。
7. **`google/oriole` A17 清单缺图形分配器/NFC 声明**：缺少 `android.hardware.graphics.allocator@2.0`、`android.hardware.nfc@1.0` 及 Tensor 相关伪装属性。
8. **`oppo/CPH2797` 直通 ODM 暴露未遮蔽的 NXP StrongBox VINTF 清单**：`keystore2` 发现 StrongBox VINTF 声明后强制尝试连接对应 HAL，失败即 `SIGABRT`。
9. **`09_build_vendor` 占位 VINTF 合成器未扣除直通官方 ODM 实例**：`synthesizePlaceholderVintfManifest` 仅检查本机合成的 `/vendor/etc/vintf` 片段，未扫描直通官方 `/odm` 中未被 `init/*.rc` 遮蔽的 VINTF XML，导致同一 FQInstance 同时出现在 `/vendor` 与 `/odm`（`RMX6688` 实测 26 项冲突），VINTF 校验直接返回 `NULL`。
10. **`RMX6688` `NfcNci` 实际落点在 `/system_ext/app/NfcNci`**：原 `module/config.json` 错误屏蔽 `/system/priv-app/NfcNci`，导致 `/system_ext/app/NfcNci` 仍被拉起并调用 NXP SN100 私有库崩溃。
11. **`09_build_vendor` 探测官方 `vendor` mapper/gralloc 后缀未兼容 ext4 按需解包模式**：`detectOfficialMapperSuffix` 与 `detectOfficialGrallocTarget` 仅扫描宿主机已解包的 `super/unpack_vendor/lib64/hw` 等目录；但按 `#141` 约定，ext4 格式的官方 `vendor_a.img` 不做整树解包、只按需逐件提取，导致 `unpack_vendor/lib64/hw` 不存在，探测函数静默回退到 `minigbm` 而错失官方 `mapper.mediatek.so`。
12. **华为两款固件实为纯血 HarmonyOS NEXT（OpenHarmony 微内核），非 Android / 非 Linux 架构**：实测解包 `Huawei-Pura-70-PRO.zip`（`HBN-AL00`）与 `Huawei-Pura-80-HED-AL00.zip`（`HED-AL00`）确认：
    - `boot.img` 为华为 X.509 签名鸿蒙引导镜像（`Certificate, Version=3`），不含 Linux 内核 `Image` 与 `Linux version`；
    - `system_a.img` / `vendor_a.img` 根文件系统为 OpenHarmony 布局（`chip_prod`、`chipset`、`eng_chipset`、`patch_hw`、`.para` 配置、HDF 驱动框架），无任何 Android `init`/`servicemanager`/`zygote`/`*.rc`；
    - `ohos.para` 明确标明 `const.ohos.fullname=OpenHarmony-5.0.1.102`（`apiversion=13`，Beta2）与 `OpenHarmony-5.1.1.206`（`apiversion=19`，Release）。早期入库将其标记为 `vendor=15` 属脚本误识别。

---

## 三、证据

- **失败与异常实测日志**：[log-fail.txt](log-fail.txt)（包含 `ASUS_AI2205`、`symphony/MAX_10`、`asus/ASUSAI2501`、`blackview/XPLORE_X1`、`google/oriole` A17、`oppo/CPH2797`、`tcl/ericanppro` 的崩溃/冲突关键日志，以及 `huawei/HBN-AL00` 与 `huawei/HED-AL00` 实测 `file` / `dump.erofs` / `ohos.para` 鸿蒙证据）。
- **通过与回归日志**：[log-pass.txt](log-pass.txt)（包含全量 37 个 Go 包单测通过记录、全仓 39 设备 / 42 模版 / 45 套盘集核查摘要，以及第二阶段全部 27 个 Android ROM 版本的 Enforcing 冷启动体检摘录）。
- **代表性验收截图（均经人工审图确认亮屏及正确 UI/桌面）**：
  - [MAX_10_home.png](MAX_10_home.png)（`symphony/MAX_10` 亮屏桌面）
  - [ASUSAI2501_home.png](ASUSAI2501_home.png)（`asus/ASUSAI2501` 亮屏桌面，0 蓝牙崩溃）
  - [XPLORE_X1_pass.png](XPLORE_X1_pass.png)（`blackview/XPLORE_X1` v2 亮屏，0 `dex2oat32` 报错）
  - [oriole_17_pass.png](oriole_17_pass.png)（`google/oriole` Android 17 `lineage-24.0` Pixel 6 桌面）
  - [CPH2797_v2b_ui.png](CPH2797_v2b_ui.png) & [CPH2797_pass.png](CPH2797_pass.png)（`oppo/CPH2797` ColorOS 16 `Launcher`，0 `keystore2`/蓝牙崩溃）
  - [RMX6688_pass.png](RMX6688_pass.png)（`realme/RMX6688` Realme UI 6.0 桌面，0 VINTF 冲突/0 NFC 崩溃）
  - [ericanppro_pass.png](ericanppro_pass.png)（`tcl/ericanppro` TCL 锁屏与 `TclQuickstepLauncher`）
  - [gold_11_pass.png](gold_11_pass.png)（`redmi/gold` HyperOS 3.0.11.0 完整中文桌面）
  - [PD2454_pass.png](PD2454_pass.png)（`vivo/PD2454` OriginOS 5 欢迎页 1440×3168）
  - [TECNO-CM5_pass.png](TECNO-CM5_pass.png)（`tecno/TECNO-CM5` HiOS 16 锁屏与 `QuickstepLauncher`）
  - [popsicle_17_pass.png](popsicle_17_pass.png)（`xiaomi/popsicle` 跨 GRF `sys=17/ven=16` HyperOS 4 界面）

---

## 四、改动

### 1. 引擎与流水线级修复
1. **全仓注册与运行期 `hostHvc` 双层防呆**：
   - `agent/modules/pipeline/stages/02_prepare/register_parse.go`：新注册设备默认写入 `host-hvc-bluetooth: hvc5`、`host-hvc-haptic: hvc6`，新增回归单测 `TestBuildDeviceJSONIncludesBluetoothAndHapticHVC`（`register_test.go`）。
   - `agent/modules/pipeline/internal/device/db_bridge.go`：新增 `ensureHardwareHostHVC` 自动兜底补齐，新增回归单测 `TestToRuntimeDeviceAutoFillsBluetoothAndHapticHVC`（`db_test.go`）。
   - 全仓补齐 27 台既有设备 `devices/manifest/devices/*/*.json` 的 `host-hvc-bluetooth`/`host-hvc-haptic`（全仓 39/39 设备齐全）。
2. **`09_build_vendor` 直通官方 ODM VINTF 自动去重**：
   - `agent/modules/pipeline/stages/09_build_vendor/files_compat.go`：`synthesizePlaceholderVintfManifest` 新增收集直通官方 ODM 未被 `init/*.rc` 遮蔽的 VINTF XML 实例并去重，根治直通 ODM 机型重复 VINTF 冲突。
   - 新增白盒单测 `TestSynthesizePlaceholderVintfSkipsPassThroughOfficialOdm`（`files_test.go`）。
3. **`09_build_vendor` ext4 官方 `vendor_a.img` 回退直读探测 mapper/gralloc 后缀**：
   - `agent/modules/pipeline/stages/09_build_vendor/files_compat.go`：新增 `parseDebugfsLsP` 与 `listOfficialVendorDir`，`detectOfficialMapperSuffix` 与 `detectOfficialGrallocTarget` 在 `unpack_vendor` 无对应目录（ext4 按需提取场景）时自动回退调用 `debugfs -R "ls -p <dir>"` 直读官方镜像。
   - 新增白盒单测 `TestParseDebugfsLsPAndExt4MapperFallback`（`props_test.go`）。

### 2. 通用 HAL 与系统脚本修复
4. **Android 13 HIDL 蓝牙服务补全**：
   - `devices/hardware-hidl/` 中 `bluetooth_hidl_service.cpp` 补齐 `Read Encryption Key Size` 支持位图 + `0x1408` + BQR VSC `0xfd5e` + `0xfc00+` 通用应答并重编落盘，消除 A13 底座全员蓝牙 `SIGABRT`。
5. **`cph-setup-display` 预置 `sales_nvram_flag=1`**：
   - 同步更新 `out/devices/vendor/android{13..17}/bin/cph-setup-display`，首启预置 `settings put global sales_nvram_flag 1`，根治 `symphony/MAX_10` 越界黑屏。

### 3. 机型清单与配置修复
6. `devices/manifest/configs/google/oriole/oriole-android17/{hardware,vendor}/config.json`：补齐 `android.hardware.graphics.allocator@2.0`、`android.hardware.nfc@1.0` 及 Tensor 伪装属性。
7. `devices/manifest/configs/asus/ASUSAI2501/ASUSAI2501-android15/vendor/config.json` 与 `devices/manifest/configs/oppo/CPH2797/CPH2797-android16/vendor/config.json`：各补齐 11 条 LE Audio 禁用属性。
8. `devices/manifest/configs/blackview/XPLORE_X1/XPLORE_X1-android15/vendor/config.json`：补齐 `dalvik.vm.dex2oat64.enabled=true`。
9. `oneplus/CPH2767`、`oppo/CPH2797`、`xiaomi/popsicle-android16` `patch/config.json`：清理与引擎通用 CIL 清洗器重复的 `vendor-cil-rewrite`。
10. `devices/manifest/configs/oppo/CPH2797/CPH2797-android16/vendor/etc/init/init.virtio_compat.rc`：遮蔽 `/odm` 下 3 份 NXP StrongBox VINTF 清单。
11. `devices/manifest/configs/realme/RMX6688/RMX6688-android15/module/config.json`：修正 `NfcNci` 屏蔽路径 `/system/priv-app/NfcNci` → `/system_ext/app/NfcNci`。

---

## 五、验证

1. **全量 Go 单测验收**：
   ```bash
   cd /Volumes/work/cloud-phone-hub/agent && \
     TMPDIR=/Volumes/work/cloud-phone-hub/out/devices/tmp \
     GOCACHE=/Volumes/work/cloud-phone-hub/out/agent/gocache \
     go test -count=1 ./modules/pipeline/... ./modules/daemon/... ./internal/manifestview/...
   ```
   结果：**37 / 37 个有测试包全部 PASS（0 FAIL）**，新增 4 项单测全部通过。
2. **全仓静态对账与盘集完备性核查**：
   - 设备总数：**39 / 39** 均含 `host-hvc-bluetooth` / `host-hvc-haptic`（缺失 0）；
   - 配置模版总数：**42 / 42** 均严格符合 `vendor → Lineage` 对齐口径（错配 0）；
   - ROM 版本总数：**47**，其中 **45 / 45 个 Android 版本**在 `out/devices/dist/` 均具备完整 4 件套（`vda.img`、`patch.img`、`data.qcow2`、`device.json`），唯一缺失的 2 个为已实测确认为纯血 HarmonyOS NEXT 的 `huawei/HBN-AL00` 与 `huawei/HED-AL00`。
3. **第二阶段 29 版本逐项验收总表（27/27 Android Enforcing 亮屏审图 PASS + 2/2 华为纯血鸿蒙取证）**：

| 组别 / 底座 | 机型 | 版本 (`buildId`) | 状态 | 冷启动与体检摘要 |
| :--- | :--- | :--- | :--- | :--- |
| A · `lineage-20.0` (A13) | `asus/ASUS_AI2205` | `14-TKQ1.220928.001-20240229` | **PASS** | Enforcing 亮屏进桌面，蓝牙 `SIGABRT` 归零 |
| A · `lineage-20.0` (A13) | `blu/G1130` | `14-UP1A.231005.007-20260511` | **PASS** | Enforcing 零崩溃亮屏进桌面 |
| A · `lineage-20.0` (A13) | `hmd/ARW` | `14-QP1A.190711.020-20250620` | **PASS** | Enforcing 亮屏进桌面（见遗留 2） |
| A · `lineage-20.0` (A13) | `nubia/P616F05` | `14-TP1A.220624.014-20260324` | **PASS** | Enforcing 零崩溃亮屏进桌面 |
| A · `lineage-20.0` (A13) | `symphony/G27` | `14-TP1A.220624.014-20240929` | **PASS** | Enforcing 亮屏进桌面（见遗留 2） |
| A · `lineage-20.0` (A13) | `symphony/MAX_10` | `14-TP1A.220624.014-20250212` | **PASS** | 预置 `sales_nvram_flag=1` 后 Enforcing 亮屏进桌面 |
| A · `lineage-20.0` (A13) | `villaon/VILLAON-V671NP` | `16-TP1A.220624.014-20260713` | **PASS** | 跨 GRF `sys=16/ven=13` Enforcing 亮屏进桌面 |
| B · `lineage-21.0` (A14) | `google/oriole` | `14-AP2A.240905.003.F1-20240815` | **PASS** | Enforcing 零崩溃亮屏进 Pixel 桌面 |
| B · `lineage-22.2` (A15) | `google/oriole` | `15-BP1A.250505.005-20250326` | **PASS** | Enforcing 零崩溃亮屏进 Pixel 桌面 |
| B · `lineage-23.2` (A16) | `google/oriole` | `16-CP1A.260405.005-20260310` | **PASS** | 75s Enforcing 零崩溃亮屏进桌面 |
| B · `lineage-24.0` (A17) | `google/oriole` | `17-CP3A.261005.002.A1-20260905` | **PASS** | 补 `allocator@2.0`/`nfc@1.0`/Tensor 属性后 v2 67s 亮屏 |
| C · `lineage-22.2` (A15) | `asus/ASUSAI2501` | `15-AQ3A.240829.003-20250122` | **PASS** | 补 11 条 LE Audio 禁用属性后蓝牙崩溃归零 |
| C · `lineage-22.2` (A15) | `blackview/XPLORE_X1` | `15-AP3A.240905.015.A2-20251126` | **PASS** | 补 `dex2oat64` 后 v2 104s 零崩溃亮屏 |
| C · `lineage-22.2` (A15) | `blu/G1170` | `15-AP3A.240905.015.A2-20260209` | **PASS** | 35s Enforcing 零崩溃亮屏进桌面 |
| C · `lineage-22.2` (A15) | `honor/sun` | `16-AQ3A.250214.001-20260904` | **PASS** | 88s Enforcing (`sys=16/ven=15`) 亮屏进 MagicOS |
| C · `lineage-22.2` (A15) | `realme/RMX6688` | `15-AP3A.240617.008-20250825` | **PASS** | ODM VINTF 去重 + 修 `NfcNci` 屏蔽后 v4 100s 0 崩溃进 Realme UI 6.0 |
| C · `lineage-22.2` (A15) | `redmi/gold` | `15-OS3.0.10.0.VNQCNXM` | **PASS** | 184s Enforcing 0 崩溃亮屏进 HyperOS 3.0 中文桌面 |
| C · `lineage-22.2` (A15) | `redmi/gold` | `15-OS3.0.11.0.VNQCNXM` | **PASS** | 246s Enforcing 0 崩溃亮屏进 HyperOS 3.0.11.0 中文桌面 |
| C · `lineage-22.2` (A15) | `tcl/ericanppro` | `15-AP3A.240905.015.A2-20250702` | **PASS** | ext4 `debugfs` 回退探测 mapper 后 v2 98s 0 崩溃亮屏进 TCL 桌面 |
| C · `lineage-22.2` (A15) | `vivo/PD2454` | `15-AP3A.240905.015.A2-20250416` | **PASS** | 55s Enforcing 0 崩溃亮屏进 OriginOS 5 (1440×3168) |
| D · `lineage-23.2` (A16) | `honor/chora` | `16-BQ2A.250913.001-BP2A.250605.031.A3-20260804` | **PASS** | 120s Enforcing 零崩溃进桌面 |
| D · `lineage-23.2` (A16) | `oneplus/CPH2767` | `16-BP2A.250605.015-20260810` | **PASS** | 清理重复 CIL 规则，116s Enforcing 零崩溃亮屏 |
| D · `lineage-23.2` (A16) | `oppo/CPH2797` | `16-BP2A.250605.015-20260529` | **PASS** | 遮蔽 StrongBox + 禁用 LE Audio 后 v3 442s 0 崩溃进 ColorOS 16 |
| D · `lineage-23.2` (A16) | `redmagic/NX809J` | `16-BQ2A.250705.001-BP2A.250605.031.A3-20260527` | **PASS** | 76s Enforcing 零崩溃亮屏进 REDMAGIC OS 11 |
| D · `lineage-23.2` (A16) | `tecno/TECNO-CM5` | `16-BP2A.250605.031.A3-20260324` | **PASS** | 191s Enforcing 0 崩溃亮屏进 HiOS 16 |
| D · `lineage-23.2` (A16) | `xiaomi/popsicle` | `16-OS3.0.319.0.WPBCNXM` | **PASS** | 185s Enforcing 0 系统/HAL 崩溃亮屏进 HyperOS 欢迎页 |
| D · `lineage-23.2` (A16) | `xiaomi/popsicle` | `17-OS4.0.5.0.XPBCN` | **PASS** | 163s Enforcing (`sys=17/ven=16`) 0 崩溃进 HyperOS 4 与桌面 |
| E · 非 Android | `huawei/HBN-AL00` | `15-205.0.0.102_SP3` | **已取证（不适用）** | 实测 `OpenHarmony-5.0.1.102` (`apiversion=13`)，纯血 HarmonyOS NEXT 5.0 |
| E · 非 Android | `huawei/HED-AL00` | `15-205.1.0.212_SP2` | **已取证（不适用）** | 实测 `OpenHarmony-5.1.1.206` (`apiversion=19`)，纯血 HarmonyOS NEXT 5.1 |

---

## 六、遗留

1. **华为 2 款纯血 HarmonyOS NEXT 固件不适用于当前 Android/Lineage 流水线**：`huawei/HBN-AL00` 与 `huawei/HED-AL00` 均为 OpenHarmony 微内核系统（无 Linux 内核及 Android 运行时），若需云端仿真需单独规划 HarmonyOS / OpenHarmony 路线，或从 Android 机型清单中归档移除（注意本轮试跑 `HBN-AL00` 的 `02_prepare` 曾尝试按默认模版重写其 `configs/huawei/HBN-AL00/HBN-AL00-android15/*`，因本仓非 Git 仓库未自动回退，但模版 `vendor → Lineage` 关系仍保持一致）。
2. **2024 旧版 A14 `cm7.xml` VINTF 弹窗已知遗留**：`hmd/ARW`、`symphony/G27`、`symphony/MAX_10` 的 `cm7.xml` 要求内核内置 `CONFIG_PPP_MPPE=y`，而当前 5 颗内核将其编译为模块 `=m`，仅产生非致命弹窗提示，不影响亮屏与正常交互，后续统一内核配方迭代时可将 `CONFIG_PPP_MPPE` 调整为 `=y` 消除弹窗。
