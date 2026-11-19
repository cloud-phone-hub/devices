# #267：`NX809J` 中兴 `ScreenRefreshRateController` 在 `~48s` 将 `hwc3-drm-upstream` 切至 `modeId=4 (640x480)` 退化修复

- 编号：`#267`
- 分组：`nx809j`（Redmi Note / Nubia / REDMAGIC 11 Pro `redmagic/NX809J` 机型专属）
- 机型：`redmagic/NX809J`（REDMAGIC 11 Pro，`16-BQ2A.250705.001-BP2A.250605.031.A3-20260527`，`lineage-23.2` + `kernel 6.12.81`）
- 状态：**已闭环**（2026-11-19）

## 1. 问题

在修复 `#266` 的 `QtiTelephony` `RescueParty` 循环并使 `verify_boot.py` 首次返回 `bootCompleted=true`、`passed=true` 后，人工调用 `read_file` 审阅 VNC 截图及核查脚本输出发现：

- 开机前 `46.6s` `cgl-headless` 渲染帧一直为完整的 `1216x2688`；
- 在 `[46.6s..48.9s]` 阶段（系统服务与 ZTE 服务拉起后），屏幕分辨率突然被切到 `640x480`（`cgl-headless flush #2600: rect=0,0 640x480 surface=640x480`、`Physical size: 640x480`、`mActiveModeId=4`），截图画面退化为局部放大的 `640x480` `ResolverActivity` 弹窗（`meanBrightness=54.53`）；
- 即使现场执行 `cmd display set-user-preferred-display-mode 1216 2688 60 0` 将 `mUserPreferredModeId` 设为 `1`，`mActiveModeId` 仍停留在 `4`（`640x480`）无法切回。

## 2. 根因

从官方 `system_a.img` 提取 `/system/framework/services.jar` 与 `/system/framework/framework.jar` 进行 DEX 字节码分析，100% 锁定中兴刷新率框架与 `hwc3-drm-upstream` 多 VESA 分辨率模式冲突机制：

1. `com.zte.feature.FeatureImplProperty` 将特性键映射为 `"ro.vendor.feature." + key.toLowerCase()`；`services.jar` 中的 `ScreenRefreshRateService`、`LocalDisplayAdapter.<clinit>`、`WindowState.<clinit>` 与 `framework.jar` 中的 `ScreenRefreshRateManager` 均读取 `Feature.getBoolean("ZTE_FEATURE_SCREEN_REFRESH_RATE_ENABLED")`（即原厂 `vendor/build.prop` 第 497 行的 `ro.vendor.feature.zte_feature_screen_refresh_rate_enabled=true`）赋给静态开关 `ZTE_REFRESH_RATE_ENABLED`。
2. `lineage-23.2` APEX 自带 `hwc3-drm-upstream` 向 `LocalDisplayAdapter` 上报 `virtio-gpu` 的 5 个 VESA 模式：`[{id=1, 1216x2688@60}, {id=2, 1024x768@60.00384}, {id=3, 800x600@60.316544}, {id=4, 640x480@60}, {id=5, 640x480@59.94048}]`（对应 `settings system` `all_refresh_rate=auto,59,60,60,60,60`）。
3. 当 `ZTE_REFRESH_RATE_ENABLED == true` 时，`ScreenRefreshRateController` 将 `supportedModes` 误当作同分辨率多档刷新率并在启动阶段通过 `setDesiredDisplayConfig` 选中并下发 `index 3`（`modeId=4`, `640x480@60.0`），同时 `LocalDisplayAdapter$LocalDisplayDevice.setDesiredDisplayModeSpecsLocked` 拦截了标准 `DisplayManager` 模式切换请求，使 `mActiveModeId` 锁定在 `4` 且忽略 `set-user-preferred-display-mode`。

## 3. 证据

- 失败现场日志：[log-fail.txt](log-fail.txt)（`qemu.log` 中 `cgl-headless` 由 `1216x2688` 在 `flush #2600` 切至 `640x480`、`dumpsys display` 实证 `mActiveModeId=4`、`mDefaultModeId=1`、`mUserPreferredModeId=1` 仍停留 `640 x 480`、`persist.sys.singlehand.screen.width=640 / height=480`）。
- 通过验证日志：[log-pass.txt](log-pass.txt)（设置 `ro.vendor.feature.zte_feature_screen_refresh_rate_enabled=false` 后，`cgl-headless` 从 `flush #1` 到 `flush #3000` 全程保持 `1216x2688`，`wm size` 返回 `1216x2688`，`dumpsys display` `mActiveModeId=1`，零 `RescueParty`）。
- 审阅截图：[pass-screen.png](pass-screen.png)（`1216x2688` 全屏 REDMAGIC 11 Pro 完整桌面，`meanBrightness=177.45`，`pngBytes=1790615`）。

## 4. 改动

1. 修改 [devices/manifest/configs/redmagic/NX809J/NX809J-android16/vendor/config.json](../../../../devices/manifest/configs/redmagic/NX809J/NX809J-android16/vendor/config.json)：
   - 在 `"build-prop-sets"` 中显式覆盖 `"ro.vendor.feature.zte_feature_screen_refresh_rate_enabled": "false"`。

## 5. 验证

1. 重打 `redmagic/NX809J` `build_vendor --refresh -> build_patch --refresh -> pack_vda --refresh`，确认 `patch/stage/root/vendor/build.prop` 写入 `ro.vendor.feature.zte_feature_screen_refresh_rate_enabled=false`。
2. 确认当前无其它运行中实例后执行 `verify_boot.py` 冷启动验收：
   - 返回 `exit 0`、`passed=true`、`bootCompleted=true`、`bootanim=stopped`、`postBootCompleted=1`、`surfaceFlingerState=running`、`bootReason=reboot,factory_reset`、`serviceCount=358`；
   - `vnc` 分辨率为完整 `1216x2688`；通过 `am start` 切换到默认桌面 `com.zte.mifavor.launcher/.uioverrides.QuickstepLauncher` 后复抓 `pass-screen.png`（`1216x2688`，`meanBrightness=177.45`）并人工调用 `read_file` 审阅确认状态栏、Google 搜索栏、天气/时钟微件、Goper、Game Center、Gemini、Settings 及底部 Dock 全屏清晰渲染；
   - `dumpsys display` 实证 `mActiveModeId=1`（`1216x2688 @ 60Hz`）、`Physical size: 1216x2688`、`Physical density: 520`。

## 6. 遗留

- 当前在 `lineage-23.2` 上复用 `hwc3-drm-upstream` 锁定单一原生分辨率 `1216x2688@60Hz`；后续若为 Android 16 接入自研 `android.hardware.graphics.composer3` 过滤 VESA 桌面分辨率并合成 60/90/120/144Hz 同组高刷（参见 [../../common/issue-hwc3-composer3-source-hal-replace-elf-patch/solution.md](../../common/issue-hwc3-composer3-source-hal-replace-elf-patch/solution.md)），可评估重新启用高刷选项。
