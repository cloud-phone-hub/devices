# #278：多行续行 `.rc` `LD_PRELOAD` 注入修复（`wpa_supplicant` `APersistableBundle_delete`）与残留硬件权限自动收口

- 编号：`#278`
- 分组：`common`（引擎级 / 跨机型）
- 触发机型与复验样本：`blu/G1130`（`13-UP1A.231005.007-20260511`）、`asus/ASUS_AI2205`、`itel/P616F05`、`ulefone/GQ5011AF1` A15/A16
- 状态：**已闭环**（2026-11-19，`09_build_vendor` 单测全绿 + 上述样本冷启动全部 `passed=true`）

## 1. 问题

1. `blu/G1130` 上 `/vendor/bin/hw/wpa_supplicant` 持续报 `CANNOT LINK EXECUTABLE "/vendor/bin/hw/wpa_supplicant": cannot locate symbol "APersistableBundle_delete"`，即使引擎尝试在 init `.rc` 注入 `LD_PRELOAD` 仍未生效；
2. `itel/P616F05` / `asus/ASUS_AI2205` 因官方声明了 NFC 特性却缺少 `vendor/etc/libnfc*.conf`，触发 `com.android.nfc` 在 `NfcConfig::loadConfig()` 处 `SIGABRT` 循环崩溃；而 `ulefone/GQ5011AF1` A15 官方自带 `Nfc_st.apk` (`libstnfc_nci_jni.so`)，在 `GKI_getbuf` 发生崩溃；
3. `ulefone/GQ5011AF1` A16 因 `vendor/etc/permissions` 残留 `android.hardware.fingerprint.xml` 且无对应 HAL 支撑，导致 `system_server` 抛 `FATAL EXCEPTION IN SYSTEM PROCESS: BiometricServiceRegistry` 循环崩溃。

## 2. 根因

1. **`service` 命令行行尾 `\` 续行破坏注入位置**：部分机型 `wpa_supplicant` `.rc` 中 `service wpa_supplicant ... \` 跨越多行，原注入逻辑紧跟 `service` 首行插入 `setenv LD_PRELOAD ...`，使其被 `init` 解析进命令行参数而非服务 option，导致 `libcdh_binder_ndk_compat.so` 未加载；
2. **NFC 配置与 ST NFC / 无 HAL 生物特征权限缺失或残留**：启用 `android.hardware.nfc`（或官方系统含 `NfcNci`）但缺 `libnfc*.conf` 时 `libnfc-nci.so` 直接 `Check` 失败中止；ST NFC (`Nfc_st`) 走专有 JNI 互斥锁路径；无指纹 HAL 时残留的 `android.hardware.fingerprint.xml` 诱发 `BiometricServiceRegistry` 崩溃。

## 3. 证据

- 失败现场日志：[log-fail.txt](log-fail.txt)（含 `G1130` `APersistableBundle_delete`、`P616F05` `NfcConfig::loadConfig()`、`GQ5011AF1` A15 `libstnfc_nci_jni.so GKI_getbuf`、`GQ5011AF1` A16 `BiometricServiceRegistry` 崩溃现场）。
- 修复后实机通过日志：[log-pass.txt](log-pass.txt)（`G1130`、`P616F05`、`GQ5011AF1` A16 均 `passed: true`）。

## 4. 改动

1. [files_compat.go](../../../../agent/modules/pipeline/stages/09_build_vendor/files_compat.go)（`L1042-L1076`）：改写 `injectWifiPreloadRC`，先剥离旧注入行，在定位 `service wpa_supplicant` / `hostapd` 后持续跳过行尾带 `\` 的续行再插入 `setenv LD_PRELOAD /vendor/lib64/libcdh_binder_ndk_compat.so`；
2. [files_compat.go](../../../../agent/modules/pipeline/stages/09_build_vendor/files_compat.go)（`L338-L416`）：新增 `backfillOfficialNfcConfigs` 自动从官方 `vendor/etc/` 原样回填 `libnfc*.conf`（若官方无配置则移除 NFC/Mifare 权限声明）；
3. [assemble.go](../../../../agent/modules/pipeline/stages/09_build_vendor/assemble.go)（`L726-L775+`）：`pruneUnsupportedHardwarePermissions` 统一收口清理无 HAL 支撑的 `android.hardware.fingerprint.xml` / `face.xml`、ST NFC（`officialSystemHasStNfc`）与展锐 TEE（`ifaa/soter/tui`）权限声明；
4. [props_test.go](../../../../agent/modules/pipeline/stages/09_build_vendor/props_test.go)：补充多行续行 `.rc` 注入与权限裁剪单测。

## 5. 验证

1. `09_build_vendor` 单元测试全绿；
2. `G1130`（`passed=true`、`serviceCount="297"`）、`P616F05`（`passed=true`、`serviceCount="321"`）、`ASUS_AI2205`（`passed=true`、`serviceCount="299"`）、`GQ5011AF1` A15/A16（`passed=true`）冷启动全部消除上述崩溃并正常进入桌面。

## 6. 遗留

- 无。
