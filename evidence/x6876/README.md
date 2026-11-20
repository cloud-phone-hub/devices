# x6876：Infinix Note 50 Pro+ 5G / X6876（`infinix/Infinix-X6876` / `mt6878` / GRF vendor=14, system=16）

1 条机型条目：#273 记录 `infinix/Infinix-X6876`（`14-UP1A.231005.007-20260423`，`lineage-23.2` + `kernels/6.1`）在借助通用 `IComposerExt` AIDL 协议桩与 GRF `mapper/minigbm*` 标签恢复图形链（[../common/issue-mtk-a16-composer-ext-aidl-stub-and-prepare-fallback/solution.md](../common/issue-mtk-a16-composer-ext-aidl-stub-and-prepare-fallback/solution.md)）后，定位并消除官方 `/system/etc/init/hw/init.rc:1232-1248` 三道传音防破解校验钩子（`Trigger_Rpmb`、`Trigger_P7`、`Trigger_keyfilesvfy`）被官方 `/odm/etc/build.prop` 跨层置 `1` 触发 `reboot,recovery` 的问题，实现干净盘冷启动进入 `QuickstepLauncher` 桌面闭环。

## 条目

| # | 问题标识 | 说明 | 状态 |
| :--- | :--- | :--- | :--- |
| 273 | [issue-x6876-a16-tran-vfy-boot-odm-override](issue-x6876-a16-tran-vfy-boot-odm-override/solution.md) | 剖析 `/system/etc/init/hw/init.rc:1232-1248` 与 `unpack_odm/etc/build.prop` 实证传音在 `sys.boot_completed=1` 挂载三道 `vfy_boot` 校验（`ro.vendor.tran.rpmb.support=1` → `Trigger_Rpmb`、`ro.vendor.tran.anti.crack.p7.support=1` → `Trigger_P7`、`ro.odm.tr_keyfilevfy.support=1` → `Trigger_keyfilesvfy`），仅在 `vendor/config.json` 置 `0` 会被后装载的 `/odm/etc/build.prop` 盖回 `1`；在 `Infinix-X6876-android16/odm/config.json` 全量置 `"0"` 后消除重启循环 | **已闭环**（2026-11-19，实例 1）：`verify_boot.py` `passed=true`（`bootCompleted=true`、`postBootCompleted=1`、`bootanim=stopped`、`surfaceFlingerState=running`），进入 `com.transsion.launcher3/.../QuickstepLauncher` 桌面（`meanBrightness=56.88`、`peak=235`） |
