# `android.hardware.graphics.composer3`

面向 Android 15+ 的纯源码自编 AIDL HWC3 显示合成服务模块，产出 `/vendor/bin/hw/android.hardware.composer.hwc3-service.drm`，直接覆盖底包预编译同名服务二进制（复用底包自带的 `etc/init/hwc3-drm.rc`、`etc/vintf/manifest/hwc3-drm.xml` 与 SELinux `hal_graphics_composer_default_exec` 标签），彻底取代 `07_build_vendor` 历史上的 AArch64 机器码补丁。

## 架构设计

模块采用**零侵入上游 `external/drm_hwcomposer` 的进程内 AIDL 包装器（Decorator）模式**：

1. 上游 `@platform/external/drm_hwcomposer` 的 KMS/DRM 与 HWC3 实现单元原样参与编译；
2. `src/service.cpp` 在进程内创建上游 `impl::Composer`，并在外层包裹 `CdhComposer` 注册 `android.hardware.graphics.composer3.IComposer/default`；
3. `CdhComposerClient` 对帧合成热路径（`executeCommands`、图层生命周期、电源与 VSync 开关等）直接 C++ 虚函数透传给上游 `impl::ComposerClient`，并在标准 AIDL 接口层按运行态 `ro.vendor.cdh.display.*` 系统属性完成面板特征对齐：
   - **VESA 冗余分辨率过滤与多档高刷合成**：仅保留底层首选原生面板模式（如 `1080×2400`），剔除 `virtio-gpu` 暴露的 `1024×768` / `800×600` / `640×480` 桌面模式，按 `refresh_rates` 合成同 `configGroup` 多档刷新率配置；
   - **无阻塞刷新率切换**：`setActiveConfig` / `setActiveConfigWithConstraints` 在包装层即时切换激活档位并返回 `refreshRequired = false`，不向 `virtio-gpu` 下发阻塞式 KMS `MODE_ID` modeset，亦不重置底层 `VSyncWorker`；`CdhComposerCallback::onVsync` 同步改写上报给 `SurfaceFlinger` 的 `vsyncPeriodNanos`；
   - **广色域（Display P3 / sRGB）**：开启时对物理屏上报 `NATIVE(0)`、`SRGB(7)`、`DISPLAY_P3(9)` 与 `COLORIMETRIC` 渲染意图，`getDataspaceSaturationMatrix` 返回 `4×4` 单位矩阵；
   - **各向异性物理像素密度（`xdpi` / `ydpi`）**：同步覆盖 HWC3 v3 `getDisplayConfigurations` 的 `DisplayConfiguration::dpi` 与 HWC3 v1/v2 `getDisplayAttribute` 的 `DPI_X` / `DPI_Y`（单位 `dots per 1000 inches`）；
   - **面板 EDID v1.3 身份（`getDisplayIdentificationData`）**：按配置的厂商 3 字母 EISA PnP ID、面板名、产品号与生产周/年动态构造 128 字节标准 EDID v1.3 数据块并置 `port = uint8_t(display)`，使 `SurfaceFlinger` 原生算出与物理真机一致的 `DeviceProductInfo` 与 `uniqueId`（`local:...`）。
4. **`oplus` 变体（`--variant oplus` / `-DCDH_COMPOSER_OPLUS=1`，源码在 `src/oplus/`，不进基础产物）**：当检测到设备 VINTF 清单声明了欧加显示色彩扩展接口 `vendor.oplus.hardware.displaycolorfeature.IDisplayColorFeature/default` 时，`CdhOplusDisplayColor` 在同一 `hal_graphics_composer_default` 进程内按需注册 v1 兼容服务（与 ColorOS 原厂 HWC 同进程承载架构一致），避免定制 `SurfaceFlinger` 无条件调用 `AServiceManager_waitForService` 引发 `system_server` `android.display` 线程死锁。

## 运行态属性契约

| 系统属性 | 示例值 | 缺省值 / 回退规则 |
|---|---|---|
| `ro.vendor.cdh.display.refresh_rates` | `"60,120,90"` | 缺省 `"60,120,90"` |
| `ro.vendor.cdh.display.wide_color_gamut` | `"true"` | 未设时回退 `ro.surface_flinger.has_wide_color_display`，再缺省 `true` |
| `ro.vendor.cdh.display.xdpi` | `"394.40994"` | 未设或 `<= 0` 时保留底层 `mm_width` 推算值 |
| `ro.vendor.cdh.display.ydpi` | `"393.39185"` | 未设或 `<= 0` 时保留底层 `mm_width` 推算值 |
| `ro.vendor.cdh.display.edid_pnp` | `"MTK"` | 3 个大写字母（与 `edid_name` 同时非空时启用 EDID 覆盖） |
| `ro.vendor.cdh.display.edid_name` | `"MTKDEV"` | 最多 12 字符 + 换行符填入 EDID `0xfc` 描述槽 |
| `ro.vendor.cdh.display.edid_product_id` | `"35638"` | `0..65535`，缺省 `0` |
| `ro.vendor.cdh.display.edid_week` | `"42"` | `0..255`，缺省 `0` |
| `ro.vendor.cdh.display.edid_year` | `"2021"` | `>= 1990`，缺省 `1990` |
