# nx809j：REDMAGIC 11 Pro（`redmagic/NX809J` / 骁龙 SM8850 `canoe` / Android 16）

1 条机型条目：#267 记录 `redmagic/NX809J`（`16-BQ2A.250705.001-BP2A.250605.031.A3-20260527`，`lineage-23.2` + `kernel 6.12.81`）在消除 `/system/lib/modules` 异代 GKI 遮蔽（[../common/issue-system-dlkm-gki-ko-shadowing-virtio-console/solution.md](../common/issue-system-dlkm-gki-ko-shadowing-virtio-console/solution.md)）与 `QtiTelephony` `IQtiRadioStable` `RescueParty`（[../common/issue-qti-radio-placeholder-vintf-stability/solution.md](../common/issue-qti-radio-placeholder-vintf-stability/solution.md)）之后，因中兴 `ScreenRefreshRateController` 将 `hwc3-drm-upstream` 上报的 5 档 VESA 模式切至 `modeId=4 (640x480@60)` 并拦截标准显示模式切换，通过置 `ro.vendor.feature.zte_feature_screen_refresh_rate_enabled=false` 恢复原生 `1216x2688@60Hz` 全屏渲染闭环。

## 条目

| # | 问题标识 | 说明 | 状态 |
| :--- | :--- | :--- | :--- |
| 267 | [issue-nx809j-zte-refresh-rate-640x480-hijack](issue-nx809j-zte-refresh-rate-640x480-hijack/solution.md) | 字节码分析 `services.jar` / `framework.jar` 实证中兴 `ScreenRefreshRateController` 与 `LocalDisplayAdapter` 读取 `ro.vendor.feature.zte_feature_screen_refresh_rate_enabled=true`，误将 `hwc3-drm-upstream` VESA 列表当作多档刷新率在 `~48s` 切至 `modeId=4 (640x480@60)` 并拦截 `DisplayManager` 恢复；在 `NX809J-android16/vendor/config.json` 置该属性为 `false` 解除劫持 | **已闭环**（2026-11-19，实例 1）：`verify_boot.py` `passed=true`（`bootCompleted=true`、`bootanim=stopped`、`bootReason=reboot,factory_reset`、`serviceCount=358`），`wm size` 保持 `1216x2688`（`mActiveModeId=1`）并完整渲染 REDMAGIC 11 Pro 桌面（`meanBrightness=177.45`） |
