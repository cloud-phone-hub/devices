# `#285`：Gzip `vendor_boot` 解压、`cdh_redirect` 逆向优先排序与 Android 12 (`31.0`) GRF 跨代兼容收口

- 编号：`#285`
- 分组：`common`（引擎级：`internal/fsimg`、`02_prepare`、`05_build_boot`、`06_build_vendor_boot` 与 `09_build_vendor` 通用兼容机制，覆盖 gzip ramdisk、跨分区 `file` 重定向逆向覆盖、`composer@2.x` `system` 回退、`vibrator` VINTF 契约一致及 `31.0` SELinux 映射补齐）
- 触发机型与复验样本：`alps/mt6765`（`TCL-T610.zip`，`12-SP1A.210812.016-20250306`，Android 14 `UP1A` 系统 + Android 12 `31.0` vendor / kernel `5.10` / `lineage-21.0`）
- 状态：**已闭环**（2026-11-19/20，`fsimg`、`02_prepare`、`05_build_boot`、`06_build_vendor_boot`、`09_build_vendor` 全部 Go 单测全绿；`alps/mt6765` 干净盘冷启动 ~87s 完成，`passed=true`，`serviceCount=330`，`meanBrightness=124.06`，SetupWizard 开机向导渲染正常）

## 1. 问题

在制作与验证 Batch C #4 `alps/mt6765`（`TCL-T610.zip`）期间先后出现五类阻塞：
1. `06_build_vendor_boot` 构建期报错 `vendor_boot.img 的 platform fragment 内没有可读文件`；
2. `audioserver` 因 `library "libaudiohal@7.1.so" ("/vendor/lib64/libaudiohal.sys.so") ... is not accessible for the namespace "(default)"` `SIGSEGV` 崩溃；
3. `vendor.hwcomposer-2-4` 报 `CANNOT LINK EXECUTABLE ... library "android.hardware.graphics.composer@2.2.so" not found` 连退 4 次；
4. `surfaceflinger` 访问 `/sys/devices/pci0000:00/0000:00:03.0` 被拒（`tcontext=u:object_r:sysfs_gpu:s0`）致 `eglInitialize` 失败崩溃；`vendor-dolby-media-c2-hal` 注册 `hal_codec2_service` 被拒；`suspend_blocker` 访问 `hal_system_suspend_service` 被拒；`init` 创建 `/dev/socket/netdiag` 被拒；
5. 第二轮冷启动仍卡在 `SystemServer` 阶段，串口每秒循环打印 `servicemanager: Since 'android.hardware.vibrator.IVibratorManager/default' could not be found ... init: service 'vendor.vibrator-default' requested start, but it is already running`（`serviceCount=166`、`meanBrightness=0.03`）。

## 2. 根因

1. **Gzip `vendor_boot` 未识别**：`TCL-T610` 官方 `vendor_boot.img` 的 `platform` fragment（`type=1, name=""`）使用 `gzip`（`1f 8b 08 00`）压缩，原 `06_build_vendor_boot/fstab.go` 与 `fragment.go` 及 `05_build_boot/ramdisk.go` 仅判断 LZ4 / 明文 cpio。
2. **跨分区 `file` 重定向逆向覆盖顺序错误**：`02_prepare` 仅对 `From == To` 的自映射规则调用 `insertSelfMapBeforeParentDir`，跨分区 `file` 规则 `/system/lib64/libaudiohal@7.1.so -> /mnt/vendorpatch/vendor/lib64/libaudiohal.sys.so` 排在了 `dir /vendor/lib64 -> /mnt/vendorpatch/vendor/lib64` 之后；Bionic linker 调用 `readlink("/proc/self/fd/...")` 触发 `cdh_redirect` 逆向还原时，先命中 `dir /vendor/lib64` 而错还原为 `/vendor/lib64/libaudiohal.sys.so`，遭 `(default)` linker namespace 拦截。
3. **`composer@2.4` 运行期接口库仅查官方 `vendor`**：Android 12 GRF 官方 `vendor` 仅带 `@2.1.so`，`@2.{2,3,4}.so` 位于官方 `system_a.img:/system/lib64/`。
4. **Android 12 (`31.0`) 与 Android 14 系统跨代 SELinux 映射缺口**：官方 `mapping/31.0.cil` 早于 Android 13，缺少 `sysfs_gpu_31_0 -> sysfs_gpu` 与 `hal_system_suspend_service_31_0 -> hal_system_suspend_service` 映射（不可全量盲绑所有 `_31_0`，因 `plat_sepolicy.cil` 对部分 `hal_*_service` 存在 `neverallow`）；Android 14 QPR0 `plat_sepolicy.cil` 缺失 AIDL `hal_codec2_service`；Lineage 重组 `vendor` 时未同步 MTK `netdiag_socket` 类型与 `/dev/socket/netdiag(/.*)?` 标签。
5. **`vibrator_config.xml` 与 VINTF 契约脱节**：`02_prepare` 生成 `register-manager="false"`，而 `android.hardware.vibrator` VINTF 与 `.rc` 始终声明 `IVibratorManager/default`；`vibrator_service.cpp` 读到 `false` 后跳过注册，导致 Android 14 `SystemServer` 启动 `VibratorManagerService` 时无限阻塞 `waitForService`。

## 3. 证据

- 失败现场日志：[log-fail.txt](log-fail.txt)（包含 `06_build_vendor_boot` 报错、官方 `vendor_boot.img` gzip 头信息、`fail1` 原生崩溃与 `fail2` `IVibratorManager/default` 阻塞现场）。
- 修复通过日志：[log-pass.txt](log-pass.txt)（5 包 Go 单测输出、`composer@2.{2,3,4}.so` 从官方 `system` 回填日志、14 条跨代 SELinux 兼容绑定日志，以及 `verify_boot.py` `passed=true`、`serviceCount="330"`、`meanBrightness=124.06` 报告）。

## 4. 改动

1. [lz4.go](../../../../agent/modules/pipeline/internal/fsimg/lz4.go)（`L18–L115`）及 [lz4_test.go](../../../../agent/modules/pipeline/internal/fsimg/lz4_test.go)、[ramdisk.go](../../../../agent/modules/pipeline/stages/05_build_boot/ramdisk.go)、[fstab.go](../../../../agent/modules/pipeline/stages/06_build_vendor_boot/fstab.go)、[fragment.go](../../../../agent/modules/pipeline/stages/06_build_vendor_boot/fragment.go)、[vendor_boot_test.go](../../../../agent/modules/pipeline/stages/06_build_vendor_boot/vendor_boot_test.go)：统一抽取 `IsGzipCompressed`、`DecompressGzip`（支持多段拼接及尾随 0 填充）、`CompressGzip`、`DecompressRamdiskWithTools`、`CompressRamdiskWithTools`，打通 `cpio / lz4 / gzip` 统一解压与重压并补单测。
2. [apply_profile_configs.go](../../../../agent/modules/pipeline/stages/02_prepare/apply_profile_configs.go)（`L552–L670`、`L718–L750`、`L1243`）及 [apply_profile_test.go](../../../../agent/modules/pipeline/stages/02_prepare/apply_profile_test.go)：
   - 将 `insertSelfMapBeforeParentDir` 升级为 `insertFileRuleBeforeCoveringDir`（同时检测正向 `From` 覆盖与逆向 `To` 覆盖），新增 `normalizeFileRedirectOrder` 纠正存量规则序；
   - 将 `etc/vibrator_config.xml` 的 `register-manager` 固定为 `true`，确保与 `android.hardware.vibrator` VINTF manifest 一致。
3. [files_compat.go](../../../../agent/modules/pipeline/stages/09_build_vendor/files_compat.go)（`L27–L120`、`L412–L440`）及 [props_test.go](../../../../agent/modules/pipeline/stages/09_build_vendor/props_test.go)：`backfillComposerHidlLibs` 增加 `unpack_system` 与官方 `system` 镜像回退；新增 `alignVibratorManagerCompat` 在 `09_build_vendor` 兜底对齐 `IVibratorManager` VINTF 与 `vibrator_config.xml`。
4. [sepolicy_cil.go](../../../../agent/modules/pipeline/stages/09_build_vendor/sepolicy_cil.go)（`L1156`、`L2161–L2289`）及 [sepolicy_test.go](../../../../agent/modules/pipeline/stages/09_build_vendor/sepolicy_test.go)：新增 `reconcileLegacyCompatBindingsCIL`，安全补齐 `sysfs_gpu` / `hal_system_suspend_service` 跨代映射、`vendor_service_contexts` 未声明服务类型（如 `hal_codec2_service`）以及 MTK `netdiag_socket` 规则与 `/dev/socket/netdiag(/.*)?` 标签。
5. 机型配置修正：[mt6765/module/config.json](../../../../devices/manifest/configs/alps/mt6765/mt6765-android14/module/config.json) 重排 `libaudiohal@7.1.so` 规则位置，[mt6765/vendor/etc/vibrator_config.xml](../../../../devices/manifest/configs/alps/mt6765/mt6765-android14/vendor/etc/vibrator_config.xml) 设为 `register-manager="true"`。

## 5. 验证

1. `internal/fsimg` (`ok 0.446s`)、`05_build_boot` (`ok 0.601s`)、`06_build_vendor_boot` (`ok 3.039s`)、`02_prepare` (`ok 1.377s`)、`09_build_vendor` (`ok 1.503s`) 全部 `-count=1` 单测通过，重编 `out/agent/dist/cdh-pipeline`；
2. 重建 `alps/mt6765`（`task-1431` + `task-1487`），`verify_boot.py --match mt6765/12-SP1A.210812.016-20250306 --timeout 320` 干净冷启动 ~87s 完成，`passed=true`、`bootanim="stopped"`、`roHardwareEgl="meow"`、`roHardwareVulkan="powervr"`、`serviceCount="330"`、`meanBrightness=124.06`、`surfaceFlingerState="running"`，人工审阅 VNC PNG 截图正常显示 Android 14 `简体中文（中国）` SetupWizard 界面。

## 6. 遗留

- 无。
