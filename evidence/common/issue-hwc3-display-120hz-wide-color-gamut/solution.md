# 证据 #183：HWC3 (`drm_hwcomposer`) 60Hz / 120Hz / 90Hz 多档刷新率与 Display P3 广色域收口（`C2`）

## 1. 问题

在 `aligner_xiaomi_gold.all`（32 项真机/云机全方位对齐记分牌）的维度 C（显示 / OpenGL ES / WebView 指纹）中，`C2_displaySupports120HzAndWideColorGamut` 未通过（见 [log-fail.txt](log-fail.txt)，`run-00244`）：

1. **高刷新率缺失且暴露 VESA 桌面分辨率**：
   - 真机 `gold` 的 `Display.getSupportedModes()` 上报 3 个同分辨率高刷档位：`mode_1` (`1080×2400 @ 60Hz`，默认激活档位)、`mode_2` (`1080×2400 @ 120Hz`)、`mode_3` (`1080×2400 @ 90Hz`)，`supportedRefreshRates = [60, 90, 120]`，`supports120Hz = true`；
   - 云机 `Display.getSupportedModes()` 上报了 5 个模式：除 `1080×2400 @ 60Hz` 外，还混入了 4 个 VESA 桌面标准分辨率（`1024×768 @ 60Hz`、`800×600 @ 60.32Hz`、`640×480 @ 60Hz`、`640×480 @ 59.94Hz`），且 `supportedRefreshRates = [60]`、`supports120Hz = false`。
2. **广色域（Wide Color Gamut / Display P3）能力缺失**：
   - 真机 `Display.isWideColorGamut() = true`，`preferredWideGamutColorSpace = "Display P3"`，`supportedColorModes = [0, 7, 9]`（`NATIVE`、`SRGB`、`DISPLAY_P3`）；
   - 云机尽管已在 `/vendor/build.prop` 声明 `ro.surface_flinger.has_wide_color_display=true`，但 `Display.isWideColorGamut()` 仍返回 `false`、`preferredWideGamutColorSpace = null`、`supportedColorModes = [0]`。

## 2. 根因

1. **`drm_hwcomposer` 透传内核 `virtio-gpu` 默认 VESA 模式且仅含 60Hz**：
   - `/vendor/bin/hw/android.hardware.composer.hwc3-service.drm`（基于 AOSP `external/drm_hwcomposer`）在 `DrmConnector::UpdateModes()` 中调用 `drmModeGetConnector()` 读取 `virtio-gpu` 连接器的 `count_modes`（共 5 项：面板原生 `1080×2400@60Hz` + 内核 `drm_add_modes_noedid()` 默认追加的 4 个 VESA 桌面分辨率）并原样推入 `modes_` 向量；
   - `HwcDisplayConfigs::Update()` 按分辨率将模式分组（`configGroup`），导致 4 个 VESA 分辨率被暴露给 `SurfaceFlinger` 与应用层 `Display.getSupportedModes()`，且 `1080×2400` 分组下仅有 `60Hz` 单一档位。
2. **在 `virtio-gpu` 上直接注入多刷新率模式会触发 KMS 阻塞与 VSync 周期切换死锁**：
   - **KMS 阻塞**：当 `SurfaceFlinger` 在 `60Hz / 90Hz / 120Hz` 之间切换时，`HwcDisplay::CreateComposition` 将 `staged_mode_` 填入 `a_args.display_mode`。在 `DrmAtomicStateManager::CommitFrame` 中，即便 CRTC 已处于激活态（`!args.active`），只要 `args.display_mode` 存在就会调用 `CreateModeBlob()` 更新 `MODE_ID` 属性并提交阻塞式 atomic commit，触发内核 `virtio-gpu` 执行完整 CRTC disable/enable modeset 与宿主 `cgl-headless` 扫描输出重置，导致 `SurfaceFlinger` 主线程阻塞数秒；另外，`DrmMode::GetVRefresh()` 在 `clock != 0` 时按 `clock / (htotal * vtotal) * 1000.0f` 实时计算刷新率，且内核 `drm_mode_validate_basic` 拒绝 `clock == 0` 的 `MODE_ID` blob（返回 `MODE_CLOCK_LOW`）；
   - **VSync 周期切换时序竞态**：`HwcDisplay::QueueConfig` 在收到同组刷新率切换请求时，调用 `vsync_worker_->SetVsyncTimestampTracking(true)` 将 `last_vsync_ts_` 清零并置 `vsync_tracking_en_ = true`、向 `SurfaceFlinger` 返回 `timeline->refreshRequired = true`。`SurfaceFlinger` 收到 `refreshRequired = true` 后立即（`< 1ms`）提交下一帧 `presentDisplay`，而此时 `VSyncWorker::SyntheticWaitVBlank` 仍在 16.6ms 的 `clock_nanosleep` 中尚未产出首个 VSync 时间戳；`CreateComposition` 因读到 `last_vsync_ts_ == 0` 直接关闭跟踪并跳过 `SendVsyncPeriodTimingChangedEventToClient`，导致 `SurfaceFlinger` 调度器持续等待 `onVsyncPeriodTimingChanged`。
3. **HWC3 硬编码仅上报 `HAL_COLOR_MODE_NATIVE (0)`**：
   - `HwcDisplay::GetColorModes` 硬编码仅返回 `1` 个色域模式 `NATIVE (0)`；`HwcDisplay::GetRenderIntents` 对 `mode != 0` 直接返回 `BAD_PARAMETER`；`ComposerClient::getDataspaceSaturationMatrix` 对非 `SRGB_LINEAR` 数据空间拒绝返回单位矩阵。`SurfaceFlinger` 在初始化显示设备时因 HWC 未上报 `DISPLAY_P3 (9)`，判定 `HWC Support: wideColorGamut=false`。

## 3. 证据

- 修复前失败日志：[log-fail.txt](log-fail.txt)（`run-00244`，`C2_displaySupports120HzAndWideColorGamut = false`，`supports120Hz = false`，`isWideColorGamut = false`，`supportedModeCount = 5`，总分 `30 / 32`）。
- 修复后通过日志：[log-pass.txt](log-pass.txt)（`run-00245`，`C2_displaySupports120HzAndWideColorGamut = true`，`supports120Hz = true`，`isWideColorGamut = true`，`preferredWideGamutColorSpace = "Display P3"`，`supportedModeCount = 3`，总分 `31 / 32`）。

## 4. 改动

在 `engine/build/stages/07_build_vendor/elfcompat.go` 新增 `reconcileVendorHwcDisplayModes` 与 `patchDrmHwc3DisplayAndColorModes`（并在 `assemble.go` 装配流水线末尾调用，单测见 `elfcompat_test.go` 的 `TestPatchDrmHwc3DisplayAndColorModes`），对 `/vendor/bin/hw/android.hardware.composer.hwc3-service.drm` 执行确定性指令级收敛：

1. **过滤 VESA 桌面分辨率并按原生像素时钟比例合成 `60Hz / 120Hz / 90Hz` 三档同组模式（`DrmConnector::UpdateModes`）**：
   - 仅取 `connector_->modes[0]`（面板原生 `1080×2400 @ 60Hz`）构造 `DrmMode` 副本，过滤其余 4 个 VESA 桌面分辨率；
   - 循环生成 3 个同分辨率模式推入 `modes_`：
     - `i = 0`（`mode_1`）：保持原生 `60Hz` 与 `DRM_MODE_TYPE_PREFERRED` 标志，作为 `preferred_config_id`；
     - `i = 1`（`mode_2`）：清除 `PREFERRED` 标志，`clock = clock_60 * 2`、`vrefresh = 120`，使 `DrmMode::GetVRefresh()` 精确算出 `120.00001 Hz`；
     - `i = 2`（`mode_3`）：清除 `PREFERRED` 标志，`clock = clock_120 - (clock_120 >> 2) = 1.5 * clock_60`、`vrefresh = 90`，使 `DrmMode::GetVRefresh()` 精确算出 `90.0 Hz`。
2. **消除运行态刷新率切换的 KMS modeset 阻塞与 VSync 周期切换竞态**：
   - 在 `DrmAtomicStateManager::CommitFrame` 的 `!args.active` 分支（CRTC 已激活，仅做页面翻转或同分辨率帧率档位切换）中，置 `nonblock = true` 并跳过 `args.display_mode` 的 `CreateModeBlob` / `MODE_ID` 原子属性绑定，确保仅在开机首次点亮屏幕（`args.active == true`）时提交一次原生 `MODE_ID`；
   - 在 `HwcDisplay::QueueConfig` 与 `ComposerClient::setActiveConfigWithConstraints` 中，将同分辨率档位切换立即更新至 `configs_.active_config_id = config`，并置 `timeline->refreshRequired = false`，避免重置 `VSyncWorker` 造成 `last_vsync_ts_ == 0` 竞态死锁。
3. **开启 `NATIVE (0)` / `SRGB (7)` / `DISPLAY_P3 (9)` 广色域上报**：
   - 修补 `HwcDisplay::GetColorModes`：当 `modes == nullptr` 时返回 `*num_modes = 3`，当 `modes != nullptr` 时写入 `[0 (NATIVE), 7 (SRGB), 9 (DISPLAY_P3)]`；
   - 修补 `HwcDisplay::GetRenderIntents`：放行色域模式 `0 / 7 / 9`，统一返回 `HAL_RENDER_INTENT_COLORIMETRIC (0)`；
   - 修补 `ComposerClient::getDataspaceSaturationMatrix`：对任意色域数据空间返回 `4×4` 单位饱和度矩阵。

## 5. 验证

1. 运行 `go test -v ./stages/07_build_vendor -run "TestPatchDrmHwc3DisplayAndColorModes|TestReconcileVendorGrallocIdentity"` 全绿；
2. 重新执行 `gold` 的 `07_build_vendor -> 09_build_patch -> 11_pack_vda`（`patch.img` sha256 `58c17e5d76d6`，`vda-24646574630a`），重启云机实例 `gold_15-OS3.0.10.0.VNQCNXM/1`，`sys.boot_completed=1` 正常点亮桌面且零帧率切换卡顿；
3. `dumpsys display` 实测 `supportedModes` 为 `id=1 (1080×2400 @ 60.0Hz)`、`id=2 (1080×2400 @ 120.00001Hz)`、`id=3 (1080×2400 @ 90.0Hz)`，`supportedColorModes = [0, 7, 9]`；
4. 触发 `aligner_xiaomi_gold.display_gpu_webview` 与 `aligner_xiaomi_gold.all`（`run-00245`）：
   - `display.isWideColorGamut = true`，`preferredWideGamutColorSpace = "Display P3"`；
   - `display.refreshRate = 120`，`supportedRefreshRates = [60, 90, 120]`，`supports120Hz = true`，`supportedModeCount = 3`（无任何 VESA 桌面分辨率泄露）；
   - `C2_displaySupports120HzAndWideColorGamut = true`（维度 C 全部 5 项 `C1..C5` 100% 全绿），总分从 `30 / 32` 提升至 **`31 / 32`**。

## 6. 遗留

- 32 项对齐记分牌仅剩最后 1 项 `H1_criticalSystemServicesAndIfaaReady`（6 项系统服务与 IFAA TEE 响应）待在下一条目（`#184`）收口。
