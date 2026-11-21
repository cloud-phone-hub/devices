# xplore-x1：Blackview XPLORE X1（`blackview/XPLORE_X1` / Doke OS 5 / MT6877 / Android 15）

1 条机型条目：#296 记录 `blackview/XPLORE_X1`（`15-AP3A.240905.015.A2-20251126`，`lineage-22.2` + `kernel 6.6`）因 `/system/app/OdmFactory` 在 `BOOT_COMPLETED` 拉起 `AtCommandService` 触发 `com.android.phone` 空指针闪退、因缺少 `vendor.trustkernel.{keybox.deployed,rkp.uploaded}=true` 与对应 `vendor_init` SELinux 授权在状态栏右上角常驻 `Not Write Google Key / Not Write RKP` 红字水印、及漏配 `android.hardware.drm` / `android.hardware.neuralnetworks` 导致 `rkpdapp` 狂刷 71,496 条失败日志；全部修复重打包后 45s `Enforcing` 冷启动审图与 5 款预装应用体检零崩溃通过。

## 条目

| # | 问题标识 | 说明 | 状态 |
| :--- | :--- | :--- | :--- |
| 296 | [issue-xplore-x1-odmfactory-crash-and-trustkernel-watermark](issue-xplore-x1-odmfactory-crash-and-trustkernel-watermark/solution.md) | 1）`/system/app/OdmFactory`（`com.wtk.factory`）`AtCommandService$1.handleMessage:268` 空指针带崩 `com.android.phone`；2）`WindowManagerService.createWatermark()` 读不到 `vendor.trustkernel.{keybox.deployed,rkp.uploaded}=true` 绘出右上角红色 `Not Write Google Key / Not Write RKP` 水印；3）漏配 `android.hardware.drm@1.0` 与 `neuralnetworks@1.0` 致 `rkpdapp` 刷屏 7 万+ 条。**改动**：`cdh_redirect` 屏蔽 `/system/app/OdmFactory`；`vendor/config.json` 置 `vendor.trustkernel.{keybox.deployed,rkp.uploaded}=true` 并新增 `vendor_trustkernel_prop`（含 `vendor_init` `set` 与 `domain` 读权限）；补齐 `drm@1.0`（+ `vintf-drop` + `hal_drm_widevine` CIL）与 `neuralnetworks@1.0` 并重打盘集 | **已闭环**（2026-11-21）：45s `Enforcing` 冷启动，`OdmFactory` 屏蔽、`am_crash=0`、5 款内置 App 拉起零闪退、`widevine_spam=0`，`About Phone` 审图确认右上角红字水印与崩溃弹窗 100% 消失 |
