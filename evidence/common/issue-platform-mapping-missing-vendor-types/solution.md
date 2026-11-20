# `#284`：自动补齐官方 `mapping/<vers>.cil` 引用的厂商缺失 SELinux 类型声明

- 编号：`#284`
- 分组：`common`（引擎级：`09_build_vendor` 跨分区 SELinux CIL 对账，影响官方 `system` / `system_ext` / `product` 的 `mapping/*.cil` 引用仅在官方 `vendor_sepolicy.cil` 声明类型的全部机型）
- 触发机型与复验样本：`tcl/asterpro`（`TCL-T521.zip`，`13-TP1A.220624.014-20251217`，TCL T614SP，GRF Android 15 底座 / `lineage-22.2`，官方 `vendor` + `system_ext` 策略版本 `33.0`）
- 状态：**已闭环**（2026-11-19，`09_build_vendor` 单测全绿；`tcl/asterpro` 增量重打包后一次通过冷启动验收 `passed=true`，`serviceCount=336`，`meanBrightness=91.14`，VNC 截图 `720x1612` 正常渲染）

## 1. 问题

`tcl/asterpro` 首轮全量构建后干净盘冷启动至 `t=1.56s` 时 `init` 致命重启：
- `init: /system/bin/secilc: Failed to resolve typeattributeset statement at /system_ext/etc/selinux/mapping/33.0.cil:182`
- `init: /system/bin/secilc exited with status 254` → `InitFatalReboot: signal 6`。

## 2. 根因

1. 官方 `system_ext_a.img:/etc/selinux/mapping/33.0.cil` 第 182 行包含 `(typeattributeset oqc_mark_33_0 (oqc_mark))`，期望策略树中已声明基类型 `oqc_mark`；
2. 厂商固件中 `(type oqc_mark)` 与 `(roletype object_r oqc_mark)` 声明在官方 `vendor_sepolicy.cil`（第 4026–4027 行），而不在 `system_ext_sepolicy.cil` 或 `plat_pub_versioned.cil`；
3. `09_build_vendor` 使用 Lineage `vendor_sepolicy.cil` 为基底重组 `vendor`，既有同步逻辑仅同步属性（`syncOfficialPublicVendorProperties`）、服务（`syncOfficialVendorServices`）及 `property_contexts` / `seapp_contexts` 缺失类型（`reconcilePlatformMissingContextTypesCIL`），未检查官方 `platCils`（含 `mapping/<vers>.cil`）中 `(typeattributeset <attr> (<members...>))` 引用的裸类型名，导致 `oqc_mark` 缺失使 `secilc` 在编译期直接失败。

## 3. 证据

- 失败现场日志：[log-fail.txt](log-fail.txt)（`init` 调用 `secilc` 在 `/system_ext/etc/selinux/mapping/33.0.cil:182` 退出 254，全量 CIL 审计确认唯一缺失符号为官方 `vendor_sepolicy.cil` 中的 `oqc_mark`）。
- 修复后通过日志：[log-pass.txt](log-pass.txt)（`09_build_vendor` 自动输出 `补齐官方 mapping 映射引用未声明类型 1 项`，冷启动 `bootCompleted=true`、`bootanim="stopped"`、`serviceCount="336"`、`roHardwareEgl="meow"`、`roHardwareVulkan="mali"`、`meanBrightness=91.14`）。

## 4. 改动

1. [sepolicy_cil.go](../../../../agent/modules/pipeline/stages/09_build_vendor/sepolicy_cil.go)：
   - 新增 `cilTypeAliasDeclRe`（L2092）与 `reconcilePlatformMissingMappingTypesCIL(vendorCil, pubCil string, platCils ...string) (string, int)`（L2098）：汇总 `pubCil`、`vendorCil` 与 `platCils` 中已声明的 `type` / `typeattribute` / `typealias`，扫描 `platCils` 全部 `(typeattributeset <attr> (<members...>))` 语句（跳过 `all/not/and/or/xor` 操作符），在 `vendor_sepolicy.cil` 末尾自动补齐缺失类型的 `(type <t>)` 与 `(roletype object_r <t>)`；
   - 在 `reconcilePlatformGenfscon` 末尾接入该对账步骤（L1149）。
2. [sepolicy_test.go](../../../../agent/modules/pipeline/stages/09_build_vendor/sepolicy_test.go)（L850）：新增 `TestReconcilePlatformMissingMappingTypesCIL`，验证缺失 `mapping` 类型补齐、已声明类型/CIL 操作符跳过及重复执行幂等性。

## 5. 验证

1. `cd /Volumes/work/cloud-phone-hub/agent && TMPDIR=/Volumes/work/cloud-phone-hub/out/devices/tmp GOCACHE=/Volumes/work/cloud-phone-hub/out/agent/gocache /opt/homebrew/bin/go test -count=1 ./modules/pipeline/stages/09_build_vendor` 通过（`ok 1.276s`），并重编 `out/agent/dist/cdh-pipeline`；
2. 增量重建 `tcl/asterpro`（`-from build_vendor -skip build_vendor_dlkm -skip pack_super`），`vendor-log.txt` 记录 `补齐官方 mapping 映射引用未声明类型 1 项`；
3. `verify_boot.py --match asterpro/13-TP1A.220624.014-20251217 --timeout 280` 冷启动 `passed=true`（`bootanim="stopped"`、`serviceCount="336"`、`meanBrightness=91.14`、`com.tcl.android.launcher/.uioverrides.TclQuickstepLauncher` / `NotificationShade`），人工 `read_file` 审阅 `720x1612` PNG 截图全屏梯度壁纸渲染正常。

## 6. 遗留

- 无。
