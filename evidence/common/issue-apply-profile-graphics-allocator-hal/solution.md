# #252：全代际默认补齐自包含 `graphics.allocator`（及 A13-15 `composer`）解决存量 Android 16+ 模版 `gralloc-mapper is missing` 黑屏

- 设备：跨机型通用（首发定位并闭环于 `oneplus/CPH2767` `16-BP2A.250605.015-20260810`，同步覆盖全仓 31 个存量配置模版）
- 日期：2026-11-18
- 状态：**已闭环**（`oneplus/CPH2767` 冷启动 `sys.boot_completed=1`、`init.svc.bootanim=stopped`、VNC 截图 `pass-cph2767.png` 平均亮度 `50.28` / 峰值 `255`、原生界面正常绘制）

---

## 一、问题

`oneplus/CPH2767`（`16-BP2A.250605.015-20260810`，Android 16 / `lineage-23.2`）在 `01→13` 构建后冷启动，内核与 init 均在 ~106 s 达到 `sys.boot_completed=1` 且 `init.svc.bootanim=stopped`，但 VNC 抓帧始终全黑（`1280x2800`，`meanBrightness: 0.0`，`peak: 0`）。

同时排查 `devices/manifest/configs/*/*/*/hardware/config.json` 全仓 36 个版本配置发现，共有 **31 个存量模版**缺少自包含的 `android.hardware.graphics.allocator@2.0`（含全部 Android 16/17 模版及新迁入的 A13/A14/A15 模版），另有 7 个 Android 15 模版同时缺少 `android.hardware.graphics.composer3@3.0`。

---

## 二、根因

1. **`uid > 10000` UI 进程在 `sphal` 命名空间加载底包 `mapper.minigbm_upstream.so` 断链**：
   - 当机型未在 `hardware/config.json` 启用 `android.hardware.graphics.allocator` 时，`09_build_vendor` 摊平 `lineage-23.2` 的 `org.lineageos.device.gralloc.minigbm_upstream.apex`，镜像内落盘 `/vendor/bin/hw/android.hardware.graphics.allocator-service.minigbm_upstream`、`/vendor/lib64/hw/mapper.minigbm_upstream.so` 以及未静态内聚的私有共享库 `/vendor/lib64/libminigbm_gralloc_upstream.so` 与 `libminigbm_gralloc4_utils_upstream.so`。
   - 系统服务（`uid=1000`）命中 `cdh_redirect` 可见这些库，但普通 App/SystemUI/Launcher/WallpaperService（`uid > 10000`）受 `cdh_redirect` SP-HAL 白名单限制无法加载 `libminigbm_gralloc_upstream.so`，在 `sphal` 命名空间下报 `dlopen failed: library "libminigbm_gralloc_upstream.so" not found` → `GraphicBufferMapper: gralloc-mapper is missing` 循环 `SIGABRT`，导致所有 UI 进程无法提交图层、VNC 全黑。
2. **默认 HAL 清单与 `apply-profile` 未覆盖全代际与存量模版**：
   - [`DefaultHardwareForAndroid`](../../../agent/modules/pipeline/internal/device/config.go) 此前仅给 Android 13/14/15 添加 `android.hardware.graphics.allocator@2.0`，遗漏了同样可由自研静态内聚源码产物接管（且已在 `09_build_vendor` 实现 `_upstream` APEX 收口）的 Android 16/17；
   - [`alignHardwareHALList`](../../../agent/modules/pipeline/stages/02_prepare/apply_profile.go) 原先仅处理 `radio` / `ir` / `nfc`，不会向已存在但缺失必选基础图形 HAL 的存量 `hardware/config.json` 幂等补齐 `graphics.allocator` 与 `graphics.composer(3)`。

---

## 三、证据

- 失败现场：[`log-fail.txt`](log-fail.txt)（实机通过 `hvc7` `cmd.run` 抓取，显示 `android.hardware.graphics.allocator-service.minigbm_upstream` 与 `mapper.minigbm_upstream.so`、`libminigbm_gralloc_upstream.so` 残留，以及 `com.android.systemui`、`WallpaperService` 等连续触发 `E Gralloc5: Failed to load mapper.minigbm_upstream.so` 与 `F GraphicBufferMapper: gralloc-mapper is missing`）。
- 修复后与验收通过记录：[`log-pass.txt`](log-pass.txt) 及 [`pass-cph2767.png`](pass-cph2767.png)（`go test` 新增单测通过、`09_build_vendor` 输出自动删除 `minigbm_upstream` 残留库并写入自包含 `mapper.minigbm.so`、`verify_boot.py` 验收平均亮度 `50.28`）。

---

## 四、改动

1. **[`agent/modules/pipeline/internal/device/config.go`](../../../agent/modules/pipeline/internal/device/config.go)** 与 **[`config_test.go`](../../../agent/modules/pipeline/internal/device/config_test.go)**：
   - 修改 `DefaultHardwareForAndroid(androidVer)`，全代际（Android 13/14/15/16/17）一律默认包含静态内聚 `android.hardware.graphics.allocator@2.0`（13/14 另含 `composer@2.4`，15 另含 `composer3@3.0`）；新增单测 `TestDefaultHardwareForAndroid`。
2. **[`agent/modules/pipeline/stages/02_prepare/apply_profile.go`](../../../agent/modules/pipeline/stages/02_prepare/apply_profile.go)** 与 **[`apply_profile_test.go`](../../../agent/modules/pipeline/stages/02_prepare/apply_profile_test.go)**：
   - `alignSingleConfigRecord` 将 `cfg.AndroidVersion` 透传给 `alignBuildVendorAndHardware` → `alignHardwareHALList`；
   - 当 `hardware` 列表为空时先用 `DefaultHardwareForAndroid(androidVer)` 初始化，并幂等补齐缺失的 `android.hardware.graphics.allocator@2.0`、Android 13/14 的 `graphics.composer@2.4`、Android 15 的 `graphics.composer3@3.0`（非 `hasReadme` 机型缺 `gpu` 时同时补齐 `gpu@1.0`）；
   - 新增单测 `TestAlignHardwareHALListAutoCompletesGraphics` 并修正旧断言。
3. **`oneplus/CPH2767` 重同步与重构**：
   - 执行 `cdh-pipeline device apply-profile oneplus/CPH2767` 自动向 [`hardware/config.json`](../../../devices/manifest/configs/oneplus/CPH2767/CPH2767-android16/hardware/config.json) 补齐 `android.hardware.graphics.allocator@2.0`，重编 `cdh-pipeline` 并增量构建 `build_hardware → build_vendor → build_patch → pack_vda`。

---

## 五、验证

1. **单元测试**：
   - `cd /Volumes/work/cloud-phone-hub/agent && TMPDIR=/Volumes/work/cloud-phone-hub/out/agent/tmp go test -count=1 ./modules/pipeline/internal/device/... ./modules/pipeline/stages/02_prepare/...` 全部通过。
2. **构建与运行态验收（`oneplus/CPH2767/16-BP2A.250605.015-20260810`）**：
   - `09_build_vendor` 自动移除 6 项底包 `minigbm_upstream` 与 `libminigbm_gralloc_upstream.so` 残留，写出静态内聚 `mapper.minigbm.so` 与 `allocator-service.minigbm`，产出 `patch.img`（sha256 `02fa23ebc2bf`）及 `vda-b16325c746b5`；
   - 独占 CPU 冷启动并通过新版 SSE + `cmd.run` + VNC `verify_boot.py` 验收：`sys.boot_completed=1`、`init.svc.bootanim=stopped`、`ro.hardware.egl=meow`、`mCurrentFocus=Window{... com.oplus.battery}`、`mFocusedApp=...SetupWizardActivity`，VNC 抓帧 `1280x2800` 平均亮度 `50.28`、峰值 `255`，画面完整显示 ColorOS 弹窗与导航栏（`pass-cph2767.png`）。

---

## 六、遗留

1. 其余存量缺失 `graphics.allocator`（及 A15 `composer3`）的配置模版，需在本轮批量闭环中依次通过 `apply-profile` 自动补齐并随全量构建上机验证。
2. 顺带记录通道踩坑：`POST /api/daemon/channel/cmd` 仅异步入队（返回 `{"ok":true,"queued":true}`），执行结果需订阅 `GET /api/daemon/channel/events`（SSE）接收，且 guest QuickJS 函数名为 `cmd.run(...)` 而非 `exec(...)`；另 `adb connect` 在无虚拟网口云机上会 `Connection refused`，自动化验机应统一走 `hvc7`。
