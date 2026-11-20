# #275：官方 `/system/etc/vintf/manifest.xml` 权威版本钳制与 Android 15/16 内联 AIMapper 5.0 防误降级

- 编号：`#275`
- 分组：`common`（引擎级 / 跨机型，适用一切低版 `libvintf` 与 Android 15/16 GRF 混搭机型）
- 触发机型与复验样本：`asus/ASUS_AI2205`（`13-TKQ1.220928.001-20240229`，Android 14 `libvintf@7.0`）、`ulefone/GQ5011AF1`（`14-GQ5011AF1_EEA_V13-20260825`，Android 16）
- 状态：**已闭环**（2026-11-19，单元测试全绿 + `ASUS_AI2205` / `GQ5011AF1` A16 冷启动均 `passed=true`）

## 1. 问题

1. `asus/ASUS_AI2205` 在清单显式配置高于官方系统支持的 VINTF schema（`8.0`）时，冷启动出现：
   - `hwservicemanager` 与 `keystore2` 连续报 `getDeviceHalManifest: -22 VINTF parse error: Illformed file: /vendor/etc/vintf/manifest.xml: Unrecognized manifest.version 8.0 (libvintf@7.0)`；
   - 整个 Device VINTF Manifest 解析归零，`serviceCount` 为空，系统无法正常进入桌面（`meanBrightness=13.63` / `20.1`）。
2. Android 15/16 平台已将 `AIMapper` 静态内联进 `/system/lib64/libui.so`（符号 `AIMapper_loadIMapper`），官方系统目录下不再存在独立的 `libaimapper.so`；若仅凭 `!IsFile("libaimapper.so")` 判断并触发 `mapper@5.0` → `HIDL 4.0` 降级，会导致 Android 15/16（如 `GQ5011AF1` A16）误删/降级原生 AIMapper 5.0 声明。

## 2. 根因

1. **VINTF schema 缺少官方系统权威上限钳制**：[vintf.go](../../../../agent/modules/pipeline/stages/09_build_vendor/vintf.go) 此前未以官方 `/system/etc/vintf/manifest.xml` 根元素的 `version` 作为上限去钳制基线配置，导致低版本 `libvintf`（如 Android 13 / 14 QPR0 的 `libvintf@7.0`）读到 `version="8.0"` 即整份拒绝解析。
2. **AIMapper 5.0 探测口径过时**：Android 15/16 将 `libaimapper` 内联至 `libui.so` 而取消独立 `libaimapper.so` 文件；必须优先读取官方 framework `manifest.xml` 的 schema version（`<= 7.0` 方判定不支持 `<hal format="native"> mapper@5.0`），并在字节级探测 `libui.so` 是否包含 `AIMapper_loadIMapper`。

## 3. 证据

- 失败现场日志：[log-fail.txt](log-fail.txt)（摘录自 `verify-ASUS_AI2205-pass1.log` 与 `verify_pass2/ASUS_AI2205_13-TKQ1.220928.001-20240229.log` 中的 `Unrecognized manifest.version 8.0 (libvintf@7.0)` 报错及 `passed: false` 结果）。
- 修复后实机通过日志：[log-pass.txt](log-pass.txt)（`ASUS_AI2205` `passed: true`、`serviceCount: "299"`、`bootanim: "stopped"`；`GQ5011AF1` A16 `passed: true`、`serviceCount: "329"`、`Found mapper/mediatek in device VINTF manifest.`）。

## 4. 改动

1. [vintf.go](../../../../agent/modules/pipeline/stages/09_build_vendor/vintf.go)（`L96-L169`）：新增 `officialFrameworkVintfSchema` 从官方 `system` 解包树或镜像读取 `/system/etc/vintf/manifest.xml` 根元素 `version`，并在 `resolveVintfSchema` 中强制将超出官方上限的基线版本钳制至 `fwk`。
2. [files_compat.go](../../../../agent/modules/pipeline/stages/09_build_vendor/files_compat.go)（`L1373-L1442`）：更新 `officialSystemLacksAIMapper` 优先以 `officialFrameworkVintfSchema <= "7.0"` 为准、其次检测 `libui.so` 中 `AIMapper_loadIMapper` 与独立 `libaimapper.so`；`downgradeLegacyNativeMapperVintf` 在官方原生支持 AIMapper 5.0 时自动清理旧降级软链接 `vendor/lib64/hw/android.hardware.graphics.mapper@4.0-impl.so`。
3. [props_test.go](../../../../agent/modules/pipeline/stages/09_build_vendor/props_test.go)：补充 framework manifest 7.0 钳制与 AIMapper 5.0 防误降级单元测试。

## 5. 验证

1. Go 单元测试：`go test ./modules/pipeline/stages/09_build_vendor` 全部通过（含 `Test` 钳制与 AIMapper 5.0 保护断言）；
2. 实机冷启动复验：`asus/ASUS_AI2205` 通过 `verify_boot.py`（`passed=true`、`serviceCount="299"`、`QuickstepLauncher` 前台）；`ulefone/GQ5011AF1` Android 16 保留 `mapper@5.0/mediatek` 并通过冷启动验收（`passed=true`、`serviceCount="329"`、`meanBrightness=124.85`）。

## 6. 遗留

- 无。
