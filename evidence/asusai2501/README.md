# asusai2501：ASUS Zenfone 12 Ultra / ROG Phone 9（`asus/ASUSAI2501` / 高通 SM8750 `sun` / Android 15）

1 条机型条目：#260 记录 `asus/ASUSAI2501`（`15-AQ3A.240829.003-20250122`，`lineage-22.2` + `kernel 6.6`）因实际固件为 `ZWWAI2501`（Zenfone 产品线）导致 `SurfaceFlinger` `RefreshRateSelector` 按 `zf(1)` 仅放行 `{60, 120, 144}` Hz，而配置携带 `60,90,120,144,165,185` 使开机选中 `id=5 (185Hz)` 并从 `supportedDisplayModes` 被剔除，引发 `LocalDisplayAdapter$DisplayModeRecord.hasMatchingMode` NPE 重启循环；通过 `02_prepare` 按 `Fingerprint` 产品名增加华硕刷新率白名单清洗及单测、收口配置为 `60,120,144` 完成冷启动闭环。

## 条目

| # | 问题标识 | 说明 | 状态 |
| :--- | :--- | :--- | :--- |
| 260 | [issue-asusai2501-zf-refresh-rate-whitelist](issue-asusai2501-zf-refresh-rate-whitelist/solution.md) | 实测 `ro.product.system.name=ZWWAI2501` 触发 ASUS `SurfaceFlinger` `ltpo: zf(1)` 白名单 `{60,120,144}` 裁掉 `185Hz (id=5)`，导致 `bootanimation` 与 `LocalDisplayAdapter` 找不到 active mode 循环崩溃；在 `02_prepare` (`apply_profile.go`) 增加 `sanitizeAsusDisplayRefreshRates` + 同目录单测并收口 `ASUSAI2501` 为 `60,120,144` | **已闭环**（2026-11-19，实例 1）：`verify_boot.py` `passed=true`（`postBootCompleted="1"`、`surfaceFlingerState="running"`、`bootReason="reboot,factory_reset"`），截图清晰渲染锁屏画面（1088×2400，`meanBrightness=10.06`），零显示崩溃 |
