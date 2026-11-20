# #280：`odm` 双卡 `DualSimOverlay` 自动回填与 `device build --refresh` 联动重打包 `build_vendor`

- 编号：`#280`
- 分组：`common`（引擎级 / 跨机型，适用全部声明双卡属性且依赖官方 `odm/overlay` 声明物理卡槽数的机型）
- 触发机型与复验样本：`itel/itel-A6612L`（`13-BP2A.250605.031.A3-20260823`）
- 状态：**已闭环**（2026-11-19，`itel-A6612L` 一次冷启动 `passed=true` 进入桌面）

## 1. 问题

`itel/itel-A6612L` 首启时，电话进程反复抛出 `java.lang.ArrayIndexOutOfBoundsException: length=1; index=1 at com.android.internal.telephony.uicc.UiccController.onGetIccCardStatusDone(UiccController.java:1137)`，连续崩溃后触发 `sys.powerctl='reboot,RescueParty'` 黑屏重启。此外，使用 `--refresh` 刷新 `vendor` / `odm` 属性或覆盖件时，`RefreshPackagingStages()` 若未包含 `build_vendor` 会遗漏重新打包。

## 2. 根因

1. `itel-A6612L` 在 `odm/etc/build.prop` 声明双卡（`dsds`），但 `07_build_odm` 未将官方 `odm/overlay/DualSimOverlay.apk` RRO 覆盖件回填至 `odm.img`（且未赋予正确的 `u:object_r:vendor_overlay_file:s0` SELinux 标签），导致系统框架读到的 `config_num_physical_slots` 仍为单卡（`length=1`），访问第 2 张卡槽（`index=1`）时数组越界并引发 `RescueParty`；
2. [pipeline.go](../../../../agent/modules/pipeline/internal/pipeline/pipeline.go) 的 `RefreshPackagingStages()` 需明确将 `build_vendor` 与 `build_odm` 纳入强制重打集合，确保 `--refresh` 同步生效。

## 3. 证据

- 失败现场日志：[log-fail.txt](log-fail.txt)（记录自 `verify-itel-A6612L-diag.log` 与 `verify-itel-A6612L-pass1.log` 的 `ArrayIndexOutOfBoundsException: length=1; index=1` 与 `reboot,RescueParty`）。
- 修复后实机通过日志：[log-pass.txt](log-pass.txt)（`itel-A6612L` `passed: true`、`bootanim: "stopped"`、`serviceCount: "325"`、`meanBrightness: 72.05`）。

## 4. 改动

1. [plan_props.go](../../../../agent/modules/pipeline/stages/07_build_odm/plan_props.go)（`L45`、`L348-L430+`）：新增 `backfillOfficialOdmDualSimOverlays`，检测到 `odm` 属性声明双卡/多卡时从官方 `odm` 解包目录或 `odm.img` 原样提取 `DualSimOverlay` RRO APK 至 `odm/overlay/`。
2. [pack.go](../../../../agent/modules/pipeline/stages/07_build_odm/pack.go)（`L45`、`L143`、`L152`）：将 `odm/overlay/` 目录及文件统一标注为 `odmOverlayFile`（`u:object_r:vendor_overlay_file:s0`），确保系统叠加层解析与 SELinux 校验通过。
3. [pipeline.go](../../../../agent/modules/pipeline/internal/pipeline/pipeline.go)（`L200-L211`）：在 `RefreshPackagingStages()` 中包含 `build_vendor` 等全套镜像打包阶段。

## 5. 验证

1. Go 单元测试与重打包构建通过；
2. 实机冷启动验收：`itel/itel-A6612L`（`13-BP2A.250605.031.A3-20260823`）`verify_boot.py` 冷启动一次通过（`passed=true`、`bootanim="stopped"`、`serviceCount="325"`、`meanBrightness=72.05`），未再触发 `UiccController` 越界与 `RescueParty`。

## 6. 遗留

- 当前 `itel-A6612L` 桌面弹出非致命 `Application Error: com.spreadtrum.ims` 对话框，不影响 `Launcher3QuickStep` 与开机主链路；后续涉及 VoLTE/IMS 时再按需单独完善。
