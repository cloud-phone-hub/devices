# #258 跨分区 SELinux `genfscon` 规则冲突（`system_ext` `sysfs_hwthermal` 与模块 `sysfs_thermal`）

- 编号：`#258`
- 分组：`common`（引擎级修复：所有自带 `/class/thermal` 等 `genfscon` 的官方 `system` / `system_ext` / `product` 固件 + `09_build_vendor`）
- 首现机型：`honor/sun` (`15-AQ3A.250214.001-20260904`)
- 状态：**已闭环**（2026-11-18）

## 1. 问题

`honor/sun` 构建完成冷启动时，`init` 一阶段调用 `/system/bin/secilc` 编译 SELinux 策略失败（`Problems processing genfscon rules` / `exit 255`），触发 `InitFatalReboot: signal 6` 在引导前 ~1.8s 无限重启，无法进入 Android。

## 2. 根因

1. `devices/hardware/android.hardware.thermal/sepolicy/vendor_sepolicy.cil` 为所有开启 `android.hardware.thermal` 的机型硬编码注入两条 `genfscon`：
   - `(genfscon sysfs "/class/thermal" (u object_r sysfs_thermal ((s0) (s0))))`
   - `(genfscon sysfs "/devices/virtual/thermal" (u object_r sysfs_thermal ((s0) (s0))))`
2. 荣耀 MagicOS 官方 `/system_ext/etc/selinux/system_ext_sepolicy.cil` 第 269 行已占用同路径规则 `(genfscon sysfs "/class/thermal" (u object_r sysfs_hwthermal ((s0) (s0))))`（官方 `vendor_sepolicy.cil` 本身只写了 `/devices/virtual/thermal` 而避开 `/class/thermal`）。
3. 由于 `AGENTS.md` 第 8 条禁止修改官方 `system_ext.img`，`secilc` 合并编译 `system_ext` 与 `vendor` CIL 时遇到同 `(fstype, path)` 的两份 `genfscon` 直接报 `Problems processing genfscon rules` 失败退出；且若仅删去冲突 `genfscon` 而不克隆 `allow` 授权，`hal_thermal_default` 将失去对平台实际类型 `sysfs_hwthermal_202404` 的只读权限。

## 3. 证据

- 失败日志：[log-fail.txt](log-fail.txt)（`qemu.log` 首次启动 `secilc exited with status 255` 与两分区重复规则位置）
- 通过日志：[log-pass.txt](log-pass.txt)（`09_build_vendor` 单测、原厂 `/system/bin/secilc` 离线过编 `1.1M` 二进制策略、`vendor-log.txt` 冲突消解行及进入 `UniHomeLauncher` 实测）

## 4. 改动

1. [agent/modules/pipeline/stages/09_build_vendor/sepolicy_cil.go](../../../../agent/modules/pipeline/stages/09_build_vendor/sepolicy_cil.go)：
   - 新增 `reconcilePlatformGenfsconConflictsCIL(vendorCil, pubCil, versSuffix string, platCils ...string)`：解析平台 CIL 中已占用的 `(fs, path) -> platType`，从 `vendor_sepolicy.cil` 剔除同 `(fs, path)` 冲突行；当双方类型名不同且目标类型（优先 `<platType>_<versSuffix>`，否则 `<platType>`）在 `plat_pub_versioned.cil` 或 `vendor_sepolicy.cil` 作用域内已声明时，自动将原 `(allow ... <venType> ...)` 规则克隆授权到平台类型并去重追加（幂等）。
   - 新增 `reconcilePlatformGenfscon(plan *Plan, stdout io.Writer)`：按官方 `system` / `system_ext` / `product` 分区抽取实际生效的 CIL（仅加载匹配 `genfs_labels_version.txt` 的 `plat_sepolicy_genfs_<ver>.cil`）并写回 `vendor_sepolicy.cil`。
2. [agent/modules/pipeline/stages/09_build_vendor/sepolicy.go](../../../../agent/modules/pipeline/stages/09_build_vendor/sepolicy.go)：
   - 调整 `applySepolicyPatches` 使无论模块/机型 `sepolicy/` 片段是否存在，收口阶段都会执行 `reconcilePlatformGenfscon`。
3. [agent/modules/pipeline/stages/09_build_vendor/sepolicy_test.go](../../../../agent/modules/pipeline/stages/09_build_vendor/sepolicy_test.go)：
   - 同目录新增单测 `TestReconcilePlatformGenfsconConflictsCIL` 验证冲突剔除、授权克隆、未导出私有类型防悬空规则与幂等性。

## 5. 验证

1. `cd /Volumes/work/cloud-phone-hub/agent && TMPDIR=/Volumes/work/cloud-phone-hub/out/agent/tmp go test -v -count=1 ./modules/pipeline/stages/09_build_vendor/...` 全部通过（`1.274s`，`TestReconcilePlatformGenfsconConflictsCIL` PASS）。
2. 重编 `out/agent/dist/cdh-pipeline` 后重打 `honor/sun`（`15-AQ3A.250214.001-20260904`），`vendor-log.txt` 记录 `消解跨分区 genfscon 冲突 1 条（补齐平台类型别名授权 3 条）`。
3. 实机冷启动直接通过一阶段 `secilc` 并进入 Android 桌面（`UniHomeLauncher`、`bootanim=stopped`）。

## 6. 遗留

- 无引擎级遗留；`honor/sun` 进入桌面后紧接着遇到的 CHR 进程空指针触发 `RescueParty` 问题已拆至 [../../sun/issue-sun-chr-chipsettype-rescueparty/solution.md](../../sun/issue-sun-chr-chipsettype-rescueparty/solution.md)（`#259`）。
