# asusai2501：ASUS Zenfone 12 Ultra / ROG Phone 9（`asus/ASUSAI2501` / 高通 SM8750 `sun` / Android 15）

2 条机型条目：#260 记录 `asus/ASUSAI2501`（`15-AQ3A.240829.003-20250122`，`lineage-22.2` + `kernel 6.6`）因实际固件为 `ZWWAI2501`（Zenfone 产品线）导致 `SurfaceFlinger` `RefreshRateSelector` 按 `zf(1)` 仅放行 `{60, 120, 144}` Hz，收口刷新率为 `60,120,144` 完成冷启动闭环；#293 记录该官方内测固件自带 `ro.vendor.asus.beta=1` 触发 `ImsWatermarkService` 全屏 IMEI 防泄密水印遮挡所有应用，通过 `cdh_redirect` 屏蔽 `/system/priv-app/ImsWatermarkService` 彻底清除。

## 条目

| # | 问题标识 | 说明 | 状态 |
| :--- | :--- | :--- | :--- |
| 260 | [issue-asusai2501-zf-refresh-rate-whitelist](issue-asusai2501-zf-refresh-rate-whitelist/solution.md) | 实测 `ro.product.system.name=ZWWAI2501` 触发 ASUS `SurfaceFlinger` `ltpo: zf(1)` 白名单 `{60,120,144}` 裁掉 `185Hz (id=5)`，导致 `bootanimation` 与 `LocalDisplayAdapter` 找不到 active mode 循环崩溃；在 `02_prepare` (`apply_profile.go`) 增加 `sanitizeAsusDisplayRefreshRates` + 同目录单测并收口 `ASUSAI2501` 为 `60,120,144` | **已闭环**（2026-11-19，实例 1）：`verify_boot.py` `passed=true`（`postBootCompleted="1"`、`surfaceFlingerState="running"`、`bootReason="reboot,factory_reset"`），截图清晰渲染锁屏画面（1088×2400，`meanBrightness=10.06`），零显示崩溃 |
| 293 | [issue-asusai2501-ims-watermark-overlay](issue-asusai2501-ims-watermark-overlay/solution.md) | 官方内测底包带 `ro.vendor.asus.beta=1`，开机由 `MiscSetupService` 拉起 `/system/priv-app/ImsWatermarkService`（`com.asus.ims.watermark`）绘制全屏 `SECURE_SYSTEM_OVERLAY` 竖排 IMEI 水印遮盖所有应用界面；在 `module/config.json` 追加 `/system/priv-app/ImsWatermarkService` `dir` 重定向屏蔽并重打盘集 | **已闭环**（2026-11-21）：30s `Enforcing` 冷启动，`com.asus.ims.watermark` 包与 `ImsWatermark` 窗口彻底清零，深色 `Settings` 与桌面人工审图无任何水印 |
