# #259 Honor Magic7 RSR (`honor/sun`) 缺失 `persist.sys.chr.chipsettype=1` 致 `HnChrService` 空指针触发 `RescueParty`

- 编号：`#259`
- 分组：`sun`（`honor/sun` 机型层与荣耀 MagicOS CHR 适配）
- 机型：`honor/sun`（Honor Magic7 RSR Porsche Design，`15-AQ3A.250214.001-20260904`）
- 状态：**已闭环**（2026-11-18）

## 1. 问题

消解跨分区 `genfscon` 冲突（见 [../../common/issue-sepolicy-platform-genfscon-conflict/solution.md](../../common/issue-sepolicy-platform-genfscon-conflict/solution.md)）后，`honor/sun` 成功穿透一阶段并进入 MagicOS 桌面 `UniHomeLauncher`，但数秒后触发 `Shutdown request from SYSTEM for reason RescueParty` 自动重启，导致 `verify_boot.py` 抓帧期间处于重启状态且 `ro.boot.bootreason=reboot,rescueparty`。

## 2. 根因

1. 系统常驻服务 `com.hihonor.hiview`（`/system/app/HnChrService/HnChrService.apk`，包名 `com.hihonor.android.chr`）每 ~0.6s 在 `ChrNonUiHandlerThread` 抛出 `NullPointerException: Attempt to invoke interface method '...ModemInfoCollector.getModemLocationInfo(int)' on a null object reference`（于 `ChrCommonInfo.getLAC`），被 `PackageWatchdog` / `RescueParty` 判定连续崩溃（`failureReason: 3`）并触发系统级重启。
2. 逆向 `HnChrService.apk` 与官方 `vendor_a.img:/etc/init/init.honor.chrproduct.rc` 确认：`ModemInfoCollectorFactory` 依赖系统属性 `persist.sys.chr.chipsettype`（`1:qcom`, `2:mtk`, `3:sprd`）实例化对应芯片平台采集器；由于机型整目录重定向 `/vendor/etc -> /mnt/vendorpatch/vendor/etc` 未纳入 `init.honor.chrproduct.rc`，且 `vendor/config.json` 未设置该属性，属性为空导致工厂方法返回 `null`。
3. 同时装配树缺失官方策略中 `vendor_init_202404` 写 `system_prop_202404` 权限，以及荣耀温控/功耗守护 `system_app`/`dubaid` 对 `sysfs_thermal`/`sysfs_hwthermal` 的只读权限，导致开机阶段存在 `avc: denied` 拦截。

## 3. 证据

- 失败现场：[log-fail.txt](log-fail.txt)（`logcat -b crash` 连续 NPE 堆栈与 `/data/system/shutdown-checkpoints/` 记录 `RescueParty` 重启堆栈）
- 通过日志：[log-pass.txt](log-pass.txt)（`verify_boot.py` `passed: true` 及 `93.66s` 稳态零崩溃复核）
- 桌面截图：[pass-screen.png](pass-screen.png)

## 4. 改动

1. [devices/manifest/configs/honor/sun/sun-android15/vendor/config.json](../../../../manifest/configs/honor/sun/sun-android15/vendor/config.json)：
   - `build-prop-sets` 新增 `"persist.sys.chr.chipsettype": "1"`；
   - `files` 新增从官方 `vendor_a.img` 取回 `etc/init/init.honor.chrproduct.rc`。
2. [devices/manifest/configs/honor/sun/sun-android15/vendor/sepolicy/vendor_sepolicy.cil](../../../../manifest/configs/honor/sun/sun-android15/vendor/sepolicy/vendor_sepolicy.cil)：
   - 新增 `(allow vendor_init_202404 system_prop_202404 (property_service (set)))` 与只读 `file` 权限，保证 `init.honor.chrproduct.rc` 可设定 `persist.sys.chr.chipsettype=1`；
   - 补齐 `system_app_202404` 对 `sysfs_thermal_202404` 及 `dubaid_202404` 对 `sysfs_hwthermal_202404` / `sysfs_thermal_202404` 的只读权限。

## 5. 验证

1. 重打 `honor/sun` (`15-AQ3A.250214.001-20260904`) 产出 `vda.img` `14.94 GiB` / `patch.img` `384.0 MiB`。
2. 全新 `data.qcow2` 冷启动运行 `verify_boot.py` 一次通过：`bootCompleted=true`、`postBootCompleted=1`、`surfaceFlingerState=running`、`bootanim=stopped`、`bootReason=reboot,factory_reset`、`topActivity=com.hihonor.android.launcher/.unihome.UniHomeLauncher`、`meanBrightness=32.28`（`peak=255`），人工审图确认为完整黑金保时捷主题 MagicOS 桌面。
3. `93.66s` 稳态现场复核：`chipsettype=1`、`RescueParty_logcat=0`、`chr_crash_count=0`，`com.hihonor.hiview` 与 `hwfactoryinterface-service` 稳定存活，`Service wifi: found`、`Service phone: found`、`Service SurfaceFlinger: found`。

## 6. 遗留

- `vendor.honor.hardware.hwfactoryinterface-service` 因未内置 `lib_factory_interface.so` 及其 12 个 OEM 闭源库在日志中仍有 `dlopen` 报错提示，但服务保持常驻注册且不影响开机动效与核心系统稳定，后续如有工厂信息调用需求可再补充该依赖库链；另已将荣耀通用 CHR/SELinux 预检经验纳入后续荣耀机型（`honor/chora`、`honor/HNLNA-M`、`honor/bengal_515`）构建前预检清单。
