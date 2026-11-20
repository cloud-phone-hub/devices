# #279：展锐 `vendor.sprd.hardware.miscdata@1.0-service` 与荣耀 MagicOS `hwfactoryinterface` 自动原样回填及 SELinux CIL 绑定

- 编号：`#279`
- 分组：`common`（引擎级 / 跨机型，适用展锐 `miscdata@1.0` 与荣耀 MagicOS `hwfactoryinterface` 全系机型）
- 触发机型与复验样本：`doogee/ARW`（`13-QP1A.190711.020-20250620`）、`honor/HNLNA-M`（`13-TP1A.220624.014-20260422`）、`honor/bengal_515`（`20260708`、`20260922`）
- 状态：**已闭环**（2026-11-19，`09_build_vendor` 单测全绿 + 上述样本冷启动全部 `passed=true`）

## 1. 问题

1. 展锐平台（`doogee/ARW`）在系统服务启动早期报 `NoSuchElementException at vendor.sprd.hardware.miscdata.V1_0.IMiscdata.getService(IMiscdata.java:74)`，引起 `system_server` 启动中断；
2. 荣耀 MagicOS（`honor/bengal_515` 等）`system_server` 中的 `FilteredPreloadAppManager$LoadFilteredPreloadRunnable` 反复报 `NoSuchElementException at vendor.honor.hardware.hwfactoryinterface.V1_1.IHwFactoryInterface.getService` 导致反复崩溃重启。

## 2. 根因

`09_build_vendor` 装配 `vendor.img` 时未将官方 `vendor` 专有出厂服务（展锐 `miscdata@1.0-service`、荣耀 `hwfactoryinterface` `-service` / `@1.1-service`）及其运行期共享库、init `.rc`、VINTF 片段与 SELinux 文件/服务/hwservice 上下文同步回填并合入 `vendor_sepolicy.cil`。即使静态文件补齐，若缺 `vendor_sepolicy.cil` 跨分区授权、`vendor_hwservice_contexts` 或 MagicOS 8+ 预探测所需的 `vendor_service_contexts` 绑定，服务仍无法注册或被 `system_server` 访问。

## 3. 证据

- 失败现场日志：[log-fail.txt](log-fail.txt)（含 `ARW` `IMiscdata.getService` 异常栈与 `bengal_515` `IHwFactoryInterface.getService` / `FilteredPreloadAppManager` 崩溃栈）。
- 修复后实机通过日志：[log-pass.txt](log-pass.txt)（`ARW`、`HNLNA-M`、`bengal_515` 均 `passed: true`）。

## 4. 改动

1. [files_compat.go](../../../../agent/modules/pipeline/stages/09_build_vendor/files_compat.go)（`L57-L280`）：新增 `backfillHonorFactoryInterface` 与 `backfillUnisocMiscdataHal`，自动探测并从官方 `vendor` 原样提取对应二进制、共享库、`.rc`、VINTF `.xml`，并补齐 `vendor_file_contexts`、`vendor_hwservice_contexts` 与 `vendor_service_contexts`。
2. [sepolicy_cil.go](../../../../agent/modules/pipeline/stages/09_build_vendor/sepolicy_cil.go)（`L1816+` 及周边）：新增 `reconcileHonorVendorHalBindingsCIL` 与 `reconcileUnisocMiscdataBindingsCIL`，动态声明缺失域/类型并生成 `hwservice_manager`、`servicemanager` 及 `binder`/`prop` 最小跨分区授权，受 `platCils` 与版本后缀校验约束以防破坏 `neverallow`。
3. [props_test.go](../../../../agent/modules/pipeline/stages/09_build_vendor/props_test.go)：补充 `miscdata` CIL 等单元测试。

## 5. 验证

1. `09_build_vendor` Go 单测全绿；
2. 实机冷启动验收：`doogee/ARW`（`passed=true`、`serviceCount="299"`、`meanBrightness=87.62`）、`honor/HNLNA-M`（`passed=true`、`meanBrightness=198.34`）、`honor/bengal_515` `20260708`（`passed=true`、`serviceCount="380"`、`meanBrightness=81.87`）均消除对应崩溃，一次通过。

## 6. 遗留

- 无。
