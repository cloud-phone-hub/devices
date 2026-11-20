# #273：`infinix/Infinix-X6876` Android 16 传音三道 `vfy_boot` 防破解钩子被 `/odm/etc/build.prop` 跨层盖回致 `reboot,recovery`

- **设备/机型**：`infinix/Infinix-X6876`（Infinix X6876，`mt6878`，版本 `14-UP1A.231005.007-20260423`，GRF `vendor=14 / system=16`，`lineage-23.2` + `kernels/6.1`）
- **状态**：`已闭环`

## 问题

在完成 `02_prepare` 配置对齐并通过通用 `IComposerExt` AIDL 协议桩（见 [`#272`](../../common/issue-mtk-a16-composer-ext-aidl-stub-and-prepare-fallback/solution.md)）恢复 `SurfaceFlinger` 后，`infinix/Infinix-X6876` 在干净盘冷启动约 86~99 s 刚抵达 `sys.boot_completed=1` 并渲染出 `com.transsion.launcher3` 的瞬间，立即触发 `init: Rebooting into recovery`（`reboot,recovery`）：
1. 首轮仅在 `vendor/config.json` 置 `"ro.vendor.tran.rpmb.support": "0"` 时，99.59 s 仍触发 `[VFY_BOOT] Rebooting into recovery, reason: Trigger_Rpmb`；
2. 次轮在 `odm/config.json` 置 `"ro.vendor.tran.rpmb.support": "0"` 后，到达 `sys.boot_completed=1` 仍触发第二道 `reboot,recovery`。

## 根因

1. **`/odm/etc/build.prop` 跨层后读覆盖 `vendor/build.prop`**：
   - Android `property_service` 按 `system → system_ext → product → vendor → vendor_dlkm → odm` 顺序装载属性，`/odm/etc/build.prop` 晚于 `/vendor/build.prop` 加载；
   - 实测官方 `unpack_odm/etc/build.prop` 原生携带：
     - 第 96 行：`ro.vendor.tran.rpmb.support=1`
     - 第 97 行：`ro.vendor.tran.rpmb.factory.enable.support=1`
     - 第 181 行：`ro.odm.tr_keyfilevfy.support=0`
     - 第 198 行：`ro.vendor.tran.anti.crack.p7.support=1`
   - 因此仅在 `vendor/config.json` 收口会被 `odm` 层直接盖回 `"1"`。
2. **官方 `/system/etc/init/hw/init.rc` 连续埋设三道独立的 `vfy_boot` 校验动作**：
   - 使用 `fsck.erofs --path=/system/etc/init/hw/init.rc` 抽出官方原件核查第 1231~1249 行：
     ```rc
     on property:sys.boot_completed=1 && property:ro.boot.mode=normal && property:ro.vendor.tran.rpmb.support=1
         vfy_boot Trigger_Rpmb /vendor/bin/verify_rpmb

     on property:sys.boot_completed=1 && property:ro.boot.mode=normal && property:ro.vendor.tran.anti.crack.p7.support=1
         vfy_boot Trigger_P7 /vendor/bin/verify_para

     on property:sys.boot_completed=1 && property:ro.boot.mode=normal && property:sys.user.0.ce_available=true && property:ro.odm.tr_keyfilevfy.support=1
         vfy_boot Trigger_keyfilesvfy /system_ext/bin/verify_key_files
     ```
   - 传音定制 `init` 内建指令 `vfy_boot` 在目标校验程序不存在（云机 `lineage` 基座无 `/vendor/bin/verify_rpmb` 与 `/vendor/bin/verify_para`）或返回非零时，直接调用 `HandlePowerctlMessage("reboot,recovery")`。只关 `rpmb` 仍会被 `anti.crack.p7`（`Trigger_P7`）接力重启。

## 证据

- 失败日志 [`log-fail.txt`](log-fail.txt)：
  - `init: processing action (ro.boot.mode=normal && ro.vendor.tran.rpmb.support=1 && sys.boot_completed=1) from (/system/etc/init/hw/init.rc:1232)`
  - `init: ExecStart failed: Cannot find '/vendor/bin/verify_rpmb': No such file or directory: [VFY_BOOT] Rebooting into recovery, reason: Trigger_Rpmb`
  - `init: Got shutdown_command 'reboot,recovery' Calling HandlePowerctlMessage()`
- 通过日志 [`log-pass.txt`](log-pass.txt)：
  - `07_build_odm` 明确覆盖 `ro.vendor.tran.anti.crack.p7.support=0`、`ro.vendor.tran.rpmb.factory.enable.support=0`、`ro.vendor.tran.rpmb.support=0`；
  - 冷启动 `bootCompleted=true`，等待 4 s 稳定期后复查 `postBootCompleted="1"`、`surfaceFlingerState="running"`，前台窗口为 `com.transsion.launcher3/com.android.launcher3.uioverrides.QuickstepLauncher`，VNC 抓帧 `1088x2436` `meanBrightness=56.88`（`peak=235`）。

## 改动

1. 机型配置 [`devices/manifest/configs/infinix/Infinix-X6876/Infinix-X6876-android16/odm/config.json`](../../../manifest/configs/infinix/Infinix-X6876/Infinix-X6876-android16/odm/config.json)：在 `build-prop-sets` 写入 `"ro.vendor.tran.rpmb.support": "0"`、`"ro.vendor.tran.rpmb.factory.enable.support": "0"`、`"ro.vendor.tran.anti.crack.p7.support": "0"`、`"ro.odm.tr_keyfilevfy.support": "0"`。
2. 引擎固化 [`agent/modules/pipeline/stages/02_prepare/apply_profile_configs.go`](../../../../agent/modules/pipeline/stages/02_prepare/apply_profile_configs.go)：在 `alignBuildOdm` 对传音系品牌（`infinix` / `tecno` / `itel`）自动写入上述四项 `odm` 属性（同步归入 [`#272`](../../common/issue-mtk-a16-composer-ext-aidl-stub-and-prepare-fallback/solution.md)）。

## 验证

1. 重跑 `07_build_odm` + `11_build_patch` 更新 `out/devices/dist/infinix/Infinix-X6876/14-UP1A.231005.007-20260423/patch.img`。
2. 执行 `verify_boot.py --match Infinix-X6876 --timeout 150` 干净盘冷启动：`passed=true`，`bootanim=stopped`，稳定停留在 XOS `QuickstepLauncher` 桌面且顶部灵动岛电量与壁纸正常渲染，无任何 `Trigger_*` 重启；验证后已销毁实例并回收中间目录。

## 遗留

- 无。传音系通用 `odm` 防篡改钩子收口规则已下沉至 `02_prepare`，后续 `itel` / `tecno` / `infinix` 新机型自动受益。
