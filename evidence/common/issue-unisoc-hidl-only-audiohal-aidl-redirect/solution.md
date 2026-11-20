# #276：展锐 Android 14 HIDL-only `libaudiohal.so` 自动识别与零修改 `libaudiohal@7.1.so -> libaudiohal.sys.so` 重定向桥接

- 编号：`#276`
- 分组：`common`（引擎级 / 跨机型，适用展锐 Android 14 QPR0 及同构 HIDL-only FactoryHal 机型）
- 触发机型与复验样本：`blu/G1130`（`13-UP1A.231005.007-20260511`）、`doogee/ARW`（`13-QP1A.190711.020-20250620`）、`itel/P616F05`（`13-TP1A.220624.014-20260324`）
- 状态：**已闭环**（2026-11-19，`G1130` / `ARW` / `P616F05` 均冷启动 `passed=true`）

## 1. 问题

部分展锐 Android 14（QPR0）厂商镜像的 `/system/lib64/libaudiohal.so` 编译时未开启 AIDL FactoryHal，仅支持 HIDL；而云机默认仅注入 AIDL 音频服务。冷启动时 `/system/bin/audioserver` 在 `AudioFlinger::AudioFlinger()` 内连续 `Fatal signal 11 (SIGSEGV), fault addr 0x0` 崩溃重启，无法加载主音频模块。

## 2. 根因

`libaudiohal.so` 内部的 `sAudioHALVersions` 查找表仅包含连续的 HIDL 5.0/4.0/6.0/7.0 常量枚举（字节模式 `0,5 / 0,4 / 0,6 / 0,7`），未包含 `Type::AIDL`，导致即使同分区内存在 `/system/lib64/libaudiohal@aidl.so` 也无法直接路由到云机 AIDL Audio HAL；且依据 [`AGENTS.md`](../../../../AGENTS.md) 第 8 条，禁止直接修改官方 `system.img`。

## 3. 证据

- 失败现场日志：[log-fail.txt](log-fail.txt)（摘录自 `verify_pass2/G1130_13-UP1A.231005.007-20260511.log`、`verify-ARW-r1.log` 中的 `audioserver` `SIGSEGV` at `AudioFlinger::AudioFlinger()` 崩溃栈）。
- 修复后实机通过日志：[log-pass.txt](log-pass.txt)（`ARW` 日志出现 `AudioFlinger: loadHwModule() Loaded primary audio interface, handle 10`；`G1130`、`P616F05` 最终冷启动均为 `passed: true`）。

## 4. 改动

1. [firmware_measured_extra.go](../../../../agent/modules/pipeline/stages/02_prepare/firmware_measured_extra.go)（`L635-L669`）：检测官方 `system/lib64/libaudiohal.so` 是否命中 HIDL-only `sAudioHALVersions` 字节特征，并在同镜像具备 `libaudiohal@aidl.so` 时，自动生成 `cdh_redirect` 文件重定向规则：`/system/lib64/libaudiohal@7.1.so -> /mnt/vendorpatch/vendor/lib64/libaudiohal.sys.so`。
2. [files_compat.go](../../../../agent/modules/pipeline/stages/09_build_vendor/files_compat.go)（`L789-L860`）：`backfillQSSIAudioHalAidl` 对 Android 13 QSSI 及命中该 HIDL-only 模式的机型，零字节修改提取官方 `libaudiohal@aidl.so` 到 `vendor/lib64/libaudiohal.sys.so`（赋予 `u:object_r:system_lib_file:s0`），并调用 `writeAudioQssiCompatVintfIfNeeded` 生成 `vendor/etc/vintf/manifest/audio_qssi_compat.xml` 声明 `android.hardware.audio@7.1::IDevicesFactory/default` 触发框架侧 `7.1 -> AIDL` 桥接。
3. [props_test.go](../../../../agent/modules/pipeline/stages/09_build_vendor/props_test.go)：补充 `backfillQSSIAudioHalAidl` 单元测试。

## 5. 验证

1. `09_build_vendor` 单测全部通过；
2. 实机冷启动验证：`blu/G1130`（`passed=true`、`serviceCount="297"`、`meanBrightness=59.2`）、`doogee/ARW`（`passed=true`、`serviceCount="299"`、`meanBrightness=87.62`）、`itel/P616F05`（`passed=true`、`serviceCount="321"`、`meanBrightness=138.78`）均无 `audioserver` 崩溃并顺利进入桌面。

## 6. 遗留

- 无。
