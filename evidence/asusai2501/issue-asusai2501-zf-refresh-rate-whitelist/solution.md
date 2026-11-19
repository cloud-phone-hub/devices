# #260 ASUS Zenfone 12 Ultra / ASUSAI2501 刷新率越过 `RefreshRateSelector` `zf(1)` 白名单导致 `LocalDisplayAdapter` NPE 重启循环

- **编号**：#260
- **设备 / 机型**：`asus/ASUSAI2501`（基线 `15-AQ3A.240829.003-20250122`，`lineage-22.2` + `kernel 6.6`）
- **固件包**：`roms/devices/asus/R2-Asus-ROG-Phone-9-Pro-AI2501.zip`（包名标注 ROG Phone 9 Pro，但固件实测指纹为 `asus/ZWWAI2501/ASUSAI2501:15/AQ3A.240829.003/35.1810.1810.243-0:user/release-keys`，即 `ro.product.system.name=ZWWAI2501`）
- **状态**：已闭环（2026-11-19）

## 1. 问题

`asus/ASUSAI2501` 首轮冷启动 `verify_boot.py` 180s 超时，`sys.boot_completed` 无法置 1；`system_server` (`android.display`) 在 `LocalDisplayAdapter$DisplayModeRecord.hasMatchingMode(LocalDisplayAdapter.java:1594)` 持续抛 `NullPointerException` 循环崩溃：

- `D RefreshRateSelector: [Display] ltpo: zf(1), factory(0)`
- `D DisplayModeController: setDesiredMode 4613503761327890176 {mode={fps=185.00 Hz, modePtr={id=5, vsyncRate=185.00 Hz, peakRefreshRate=185.00 Hz}}, emitEvent=true, force=false}`
- `E SurfaceComposerClient: Active display mode not found.`
- `E LocalDisplayAdapter: Can't find display mode with id 5` / `id -1`

## 2. 根因

反汇编官方 `system_a.img:/system/bin/surfaceflinger` 证实，华硕在 `SurfaceFlinger` 内按 `ro.product.system.name`（对应 fingerprint 第 2 段产品名）区分 Zenfone 与 ROG 两条产品线并硬编码刷新率白名单：

1. `RefreshRateSelector` 初始化函数（`0x22d290`）读取 `ro.product.system.name` 首字符是否为 `'Z'`（`0x5a`）并写入成员标志位 `is_zf`（本固件为 `ZWWAI2501`，故运行日志输出 `ltpo: zf(1), factory(0)`）；
2. `SurfaceFlinger::getDynamicDisplayInfoInternal`（`0x25b274` -> `0x23f1e0`）在返回 `DynamicDisplayInfo` 时对主屏可用 `DisplayMode` 执行白名单过滤：
   - 当 `is_zf == 1`（Zenfone 产品线）时，仅放行 `{60, 120, 144}` Hz；
   - 当 `is_zf == 0`（ROG Phone 产品线）时，仅放行 `{60, 120, 165, 185}` Hz。
3. 规格采集模板因识别到包名与 `ASUSAI2501` 型号，将 `refreshRatesHz` 写为全量 `[60, 90, 120, 144, 165, 185]`（`composer3` 按序生成 `configId 1..6`，SurfaceFlinger 默认偏好选最高档 `185.00 Hz / id=5`），但 `getDynamicDisplayInfo` 返回给 `bootanimation` 与 `system_server` 的 `supportedDisplayModes` 被白名单裁掉 `90/165/185` 后仅剩 `{60, 120, 144}`，致使 `activeDisplayModeId=5` 在 `supportedDisplayModes` 列表中查无对应项（`findMatchingModeLocked` 返回 `null`），直接触发 `hasMatchingMode` 空指针崩溃，导致 `system_server` 循环重启。

## 3. 证据

- **失败现场**（见同目录 `log-fail.txt`）：
  - `ro.product.system.name = ZWWAI2501`、`ro.build.fingerprint = asus/ZWWAI2501/ASUSAI2501:15/...`、`ro.vendor.cdh.display.refresh_rates = 60,90,120,144,165,185`、`ro.vendor.cdh.display.default_refresh_rate = 120`；
  - `D RefreshRateSelector: [Display] ltpo: zf(1), factory(0)` 紧随其后 `setDesiredMode ... id=5 (185.00 Hz)`，`bootanimation` 报错 `Active display mode not found.`，随后 `system_server` 在 `LocalDisplayAdapter.java:1594` 循环崩溃。
- **二进制逆向证据**：`surfaceflinger` 偏移 `0x22d2b4..0x22d32c` 检查 `ro.product.system.name[0] == 'Z'` 写入 `[x19, #0x5f2]`；函数 `0x23f1e0` 依据 `[x24, #0x5f2]` 在 `{60, 120, 144}` 与 `{60, 120, 165, 185}` 之间分支构造白名单向量并过滤主屏模式。

## 4. 改动

1. **`02_prepare` 通用清洗规则**：
   - 在 [`agent/modules/pipeline/stages/02_prepare/apply_profile.go`](../../../agent/modules/pipeline/stages/02_prepare/apply_profile.go) 新增 `sanitizeAsusDisplayRefreshRates(fm FirmwareMeasuredFacts, rates []int, defaultHz int) ([]int, int)` 与辅助函数 `parseRefreshRateCSV` / `formatRefreshRateCSV`：对华硕机型按实测 `fm.Fingerprint` 第 2 段产品名（`Z*` 限 `{60, 120, 144}`，非 `Z` 且 `ASUSAI2501` 限 `{60, 120, 165, 185}`）过滤 `refresh_rates` 并对 `default_refresh_rate` 兜底（优先 `120`）；同时在 `deriveVendorPropCandidates` 生成属性和 `alignBuildVendorAndHardware` 存量属性自愈两处调用；
   - 在 [`agent/modules/pipeline/stages/02_prepare/apply_profile_test.go`](../../../agent/modules/pipeline/stages/02_prepare/apply_profile_test.go) 新增 `TestSanitizeAsusDisplayRefreshRates`，覆盖 `ZWWAI2501`、`WW_AI2501`、全非法档位回退 `[60, 120]`、非华硕机型不误伤及存量配置自愈。
2. **机型配置收口**：
   - [`devices/manifest/devices/asus/ASUSAI2501/device-profile.json`](../../../devices/manifest/devices/asus/ASUSAI2501/device-profile.json) 将 `refreshRatesHz` 收口为 `[60, 120, 144]`；
   - [`devices/manifest/configs/asus/ASUSAI2501/ASUSAI2501-android15/vendor/config.json`](../../../devices/manifest/configs/asus/ASUSAI2501/ASUSAI2501-android15/vendor/config.json) 将 `ro.vendor.cdh.display.refresh_rates` 改为 `"60,120,144"`。

## 5. 验证

1. **单测验证**：
   - `cd agent && TMPDIR=/Volumes/work/cloud-phone-hub/out/agent/tmp go test -v -run TestSanitizeAsusDisplayRefreshRates ./modules/pipeline/stages/02_prepare/...` -> `PASS`；
   - `go test ./modules/pipeline/stages/02_prepare/...` 全量通过并重编 `out/agent/dist/cdh-pipeline`。
2. **真机云机冷启动验证**（重构 `vendor`/`patch`/`vda`，独占 CPU 执行 `verify_boot.py`）：
   - `passed: true`、`bootCompleted: true`、`postBootCompleted: "1"`、`bootanim: "stopped"`、`surfaceFlingerState: "running"`、`bootReason: "reboot,factory_reset"`；
   - `ro.vendor.cdh.display.refresh_rates = 60,120,144`、`default_refresh_rate = 120`；
   - `topActivity`: `com.asus.launcher/com.android.launcher3.uioverrides.QuickstepLauncher`；
   - VNC 截图 `pass-screen.png`（1088×2400，`meanBrightness: 10.06`）人工审图清晰展示 Android 15 锁屏画面（状态栏 AUTO Hz、Wi-Fi、100% 电池、SIM 提示卡片与锁屏解锁提示），无显示模式崩溃。

## 6. 遗留

- 运行态曾观测到基底或厂商侧 `android-hardware-media-c2-hal` / `vendor-dolby-media-c2-hal` 等非关键媒体服务在日志中重拉（`sys.init.updatable_crashing=1`），但未触发 `RescueParty` 且桌面与显示系统完全正常；待全量机型媒体深测时一并核查。
