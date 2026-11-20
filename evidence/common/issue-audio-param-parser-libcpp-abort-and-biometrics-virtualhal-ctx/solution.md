# #274：`cdh_audio_parameter_parser` 跨代 `__libcpp_verbose_abort` ABI 自包含与生物识别 `virtualhal` 服务上下文自动绑定

- 编号：`#274`
- 分组：`common`（引擎级 / 跨机型，适用全部重定向 `parameter_parser.service` 的 MTK Android 14/15 机型与启用 `biometrics` `virtualhal` 的机型）
- 触发机型与复验样本：`realme/RMX5112`（`13-TP1A.220905.001-20251025`，`851_sign_RMX5112_11_A_OTA_0520_all_0Q8VwS_10010111.zip`，GRF `vendor=13 / system=15`，`lineage-22.2` + `6.6`）
- 状态：**已闭环**（2026-11-19，单元测试全绿 + `realme/RMX5112` 实机冷启动 `passed=true` 直入桌面）

## 1. 问题

`realme/RMX5112`（ColorOS 15 / MTK 平台）在补齐 `soundtrigger3@1.0`、`biometrics`（`fakefingerprint`）与 `power@1.0` 后推进至 `serviceCount=248`，但卡死在开机动画无法进入桌面：
1. `/system_ext/bin/hw/android.hardware.audio.parameter_parser.service` 经 `cdh_redirect` 重定向至 `/mnt/vendorpatch/vendor/bin/hw/cdh_audio_parameter_parser` 后，每次拉起均立即被动态链接器中止，报 `CANNOT LINK EXECUTABLE ... cannot locate symbol "_ZNSt3__122__libcpp_verbose_abortEPKcz" referenced by ".../parameter_parser.service"`，致使 `audioserver` / `system_server` 等待 `IHalAdapterVendorExtension/default` 超时；
2. `vendor.fingerprint-virtualhal` 与 `vendor.face-virtualhal` 注册 `android.hardware.biometrics.{fingerprint,face}.virtualhal.IVirtualHal/virtual` 时，被 `servicemanager` 报 `SELinux: avc: denied { add } ... tcontext=u:object_r:default_android_service:s0` 拒绝。

## 2. 根因

1. **System Linker Namespace 跨代 `libc++.so` 符号差异**：`cdh_audio_parameter_parser` 被重定向替代 `/system_ext/bin/hw/android.hardware.audio.parameter_parser.service` 时，按文件路径归入 **`system` linker namespace**，运行期直接动态链接官方 `/system/lib64/libc++.so`（Android 15 ColorOS 15 基于 LLVM 17 构建，尚未导出 LLVM 18+ / NDK r29 头文件内联代码引用的 `std::__1::__libcpp_verbose_abort(const char*, ...)`）。
2. **`virtualhal` 未入低版本平台 `plat_service_contexts`**：Android 14/15 官方 `/system/etc/selinux/plat_service_contexts` 尚未内置 `android.hardware.biometrics.{fingerprint,face}.virtualhal.IVirtualHal/virtual` 条目，服务名回退匹配兜底项 `default_android_service`（而 AOSP `domain.te` `neverallow * default_android_service:service_manager add`），必须在官方平台未声明时于 `vendor_service_contexts` 显式归入 `hal_fingerprint_service` / `hal_face_service`。

## 3. 证据

- 失败现场日志：[log-fail.txt](log-fail.txt)（记录 `_ZNSt3__122__libcpp_verbose_abortEPKcz` 链接失败与 `IVirtualHal/virtual` 落入 `default_android_service` 被拒）。
- 修复后实机冷启动通过日志：[log-pass.txt](log-pass.txt)（`serviceCount=395`、`bootCompleted=true`、`bootanim=stopped`、`meanBrightness=194.04`）。

## 4. 改动

1. [ParameterParserMain.cpp](../../../../devices/hardware/android.hardware.audio/src/ParameterParserMain.cpp)：按 LLVM `libc++` `_LIBCPP_OVERRIDABLE_FUNC_VIS` 规范在 `std::__1` 命名空间提供进程内强定义 `[[noreturn]] void __libcpp_verbose_abort(const char* format, ...) noexcept`，消除对外部 `/system/lib64/libc++.so` 该符号的 `UND` 导入；
2. [files_compat.go](../../../../agent/modules/pipeline/stages/09_build_vendor/files_compat.go)：新增 `backfillBiometricsVirtualHalContexts`，当装配树携带 `fingerprint-virtualhal-service` 或 `face-virtualhal-service` 时，幂等补齐 `vendor_file_contexts` 执行域标签，并在检查官方 `plat_service_contexts` 未包含对应键时向 `vendor_service_contexts` 写入 `hal_fingerprint_service` / `hal_face_service` 映射。

## 5. 验证

1. 编译与单元测试：`02_prepare` 与 `09_build_vendor` Go 单测全绿，`android.hardware.audio` 在 Android 15 / 16 目标下编译通过；
2. 实机冷启动验收：`realme/RMX5112`（`13-TP1A.220905.001-20251025`）刷新 `patch.img`（`sha256 eef9f05e39a0`）后执行 `verify_boot.py`，一次冷启动通过（`passed=true`，`serviceCount=395`，进入 `com.android.launcher/.Launcher` 桌面，VNC `1088×2400` `meanBrightness=194.04`）。

## 6. 遗留

- ColorOS 锁屏首帧偶现「充电电压过高」提示横幅（不影响 `Launcher` 焦点与触控交互），后续可在 `android.hardware.health` 针对 Oplus `charger` 扩展属性进一步对齐电压上报单位（mV vs µV）。
