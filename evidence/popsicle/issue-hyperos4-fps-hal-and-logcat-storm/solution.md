# popsicle HyperOS 4（16-OS4.0.5.0.XPBCN）流畅度差距根因与每帧 HAL/日志风暴治理

## 1. 问题

用户反馈 `popsicle_16-OS4.0.5.0.XPBCN`（HyperOS 4）相比同机型的 `popsicle_16-OS3.0.319.0.WPBCNXM`（HyperOS 3）明显卡顿，且 `logcat` 中有大量缺失 HAL 与错误日志持续刷屏，询问卡顿是否由缺失 HAL 引起。

## 2. 根因

`16-OS4.0.5.0.XPBCN` 的卡顿由**两层原因**共同构成：一层是**部分缺失 HAL 声明与错误属性在每一帧/每次动画时触发同步调用风暴**（可通过配置与 VINTF 补齐彻底消除），另一层是 **HyperOS 4 桌面架构从 OpenGL ES 硬件加速切换到了 Flutter Impeller + CPU 软件 Vulkan**（决定了在 macOS 宿主无 `VK_KHR_external_semaphore_fd` 时桌面绘制只能走 CPU）。

### 2.1 可修复的运行态同步风暴与降频卡点（5 项）

1. **LTPO 动态刷新率（AutoMode TP-Idle）将桌面锁死在 10Hz**：
   - 官方 `odm/etc/build.prop` 声明了 `ro.surface_flinger.use_content_detection_for_refresh_rate=true` 与 `vendor.display.enable_dpps_dynamic_fps=1`。
   - HyperOS `DisplayModeController` 在无物理 TP 中断（`onTpIdleChanged`）1 秒后将 `mActiveRenderFrameRate` 从 `60.0` 降至 **`10.0`（10fps）**。
2. **`SurfaceFlinger` `RenderEngine` 每帧 6 次 `DolbyVisionGralloc5` 元数据读取失败（4.5 分钟 14,333 次，2 秒切换 239 次）**：
   - 反汇编 `/system_ext/lib64/libsurfaceflinger.so`（`0x64fbec`–`0x64ffe8`）可见，每帧每个图层合成前都会读取 `debug.media.video.chipset`（默认 `11 >= 7`）与 `debug.config.media.video.dolby_vision_GPU_suports`（默认 `false`）。
   - 当 `chipset >= 7` 且 `!dolby_vision_GPU_suports` 时，`SurfaceFlinger` 对每个 GraphicBuffer 调用 `DVGraphicBufferMapper::getDolbyVisionMetadata` 查询 `Smpte2094_40`；而 `minigbm_upstream` 不支持该 metadata，每次返回 `-74` 并同步写 `logcat` 错误日志 `E DolbyVisionGralloc5: getDolbyVisionMetadata: Mapper failed to get Smpte2094_40 metadata! Status: -74`。
   - 同时 `SurfaceFlinger` 每帧查询 `ro.vendor.display.bt2020.supported` 与 `ro.vendor.display.max_hdr_luminance` 因缺 `vendor_default_prop` 读取权限触发 `W libc: Access denied finding property`（4.5 分钟各 1,347 次）。
3. **`vendor.qti.hardware.perf2` 缺 VINTF 声明导致 `system_server` 每次动画/触控提频都在主线程同步走两遍失败 IPC（2 秒切换 24+ 次）**：
   - 虽然 `vendor.qti.hardware.perf2-hal-service` 已在运行并注册了 `vendor.qti.hardware.perf2.IPerf/default`，但 `/vendor/etc/vintf/manifest/` 下未提供其 VINTF 片段。
   - 反汇编 `/system_ext/lib64/libqti-perfd-client_system.so`（`0xd5dc`–`0xd810`）证明：客户端在调用 `AServiceManager_checkService`（`0xd7fc`）前，**先调用 `AServiceManager_isDeclared("vendor.qti.hardware.perf2.IPerf/default")`（`0xd5dc`）**。
   - 因未声明 VINTF，`AServiceManager_isDeclared` 返回 `false`，`servicemanager` 打 `Could not find vendor.qti.hardware.perf2.IPerf/default in the VINTF manifest`，客户端将 `gPerfServiceType`（`[x20, #0x2b8]`）置为 `2`（HIDL 模式），完全跳过我们自研的 AIDL `perf2` 服务，转去调 HIDL `vendor.qti.hardware.perf@2.2::IPerf::tryGetService("default")`（打 `Trying to get transport ... without hwservicemanager`），失败后又把 `gPerfServiceType` 重置为 `0`（`0xd934`），导致**永远无法缓存句柄**，每次触控/窗口动画都在 `system_server` 线程上重复执行 `isDeclared` + HIDL `@2.2` 失败路径。
4. **内核 `fbcon`（`vtcon1`）在进入 Android 桌面后仍绑定在 `virtio_gpudrmfb` 上**：
   - 为支持引导期 VNC 查看内核启动日志开启了 `CONFIG_FRAMEBUFFER_CONSOLE=y`；进入桌面后若不解绑 `vtcon1`，后台每次 `printk`（如 `cdh_redirect` / `audit`）都会触发 `fbcon` 同步软滚屏与 `virtio_gpu` 全屏脏区回读，与 `SurfaceFlinger` 争抢 DRM 主平面。
5. **后台 3 个原厂死循环服务每 5 秒崩溃重拉**：
   - `loc_sys_service`（缺 `libloc_api_v02.so`）、`iorapd`（缺 `ITaskManager`）、`millet_monitor`（`MILLET_EPOLL: failed to create socket`）每 5 秒被 `init` 拉起并崩溃，推高 `init`、`logd` 与 `cdh_redirect` 负载。

### 2.2 HyperOS 4 相比 HyperOS 3 桌面仍更吃 CPU 的架构根因

- **HyperOS 3（`16-OS3.0.319.0.WPBCNXM`）**：桌面 `com.miui.home` 是标准 Android Java/View 应用，受 `ro.hwui.use_vulkan=false` 控制，走 **HWUI OpenGL ES → `virtio-gpu` VirGL 宿主 GPU 硬件加速**。
- **HyperOS 4（`16-OS4.0.5.0.XPBCN`）**：小米将桌面 `com.miui.home`（以及天气 `com.miui.weather2`）彻底重写为基于 `/system_ext/bin/hyos_spawner` + `libhyper_os_flutter.so` 的 **Rust + Flutter Impeller 应用**，且 Impeller 引擎写死只走 **Vulkan**（见 [`../issue-hyperos4-pastel-vulkan-and-wdt/`](../issue-hyperos4-pastel-vulkan-and-wdt/solution.md)）。
- 由于 macOS 宿主 `MoltenVK` 不支持 `VK_KHR_external_semaphore_fd`（`SYNC_FD`），`virtio-gpu` Venus（`vulkan.virtio.so`）无法向 guest 暴露 `VK_KHR_swapchain`，因此 `popsicle` 只能使用 `vulkan.pastel.so`（Google SwiftShader **纯 CPU 软件渲染 Vulkan**，8 个 `Thread<00>..Thread<07>` 线程在 vCPU 上软光栅化整屏 1200×2608 像素与 `MiGlassV13F16` 毛玻璃着色器）。
- 此外，桌面首屏的天气 MAML 小部件（`rust_maml_sdk`）在用户未打开天气同意隐私协议前，每 2 秒轮询 `content://weather/...` 收到 `NOT_ENOUGH_DATA` 并触发 `Maml:1 requestDrawCallback`，导致桌面即使静置也每 2 秒重绘一次。

## 3. 证据

- 反汇编 `/system_ext/lib64/libsurfaceflinger.so` `0x64fbf8`–`0x64fc24`：
  ```text
  64fbf8: bl property_get_int32  ; "debug.media.video.chipset"
  64fc0c: bl property_get_bool   ; "debug.config.media.video.dolby_vision_GPU_suports"
  64fc14: cmp w22, #0x7
  64fc18: b.lt 0x650324          ; chipset < 7 直接跳过 DolbyVisionGralloc5
  64fc24: cbnz w0, 0x650324      ; dolby_vision_GPU_suports == true 直接跳过 DolbyVisionGralloc5
  ```
- 反汇编 `/system_ext/lib64/libqti-perfd-client_system.so` `0xd5dc`–`0xd810`：
  ```text
  d5dc: bl AServiceManager_isDeclared ; "vendor.qti.hardware.perf2.IPerf/default"
  d5e0: tst w0, #0x1
  d5f0: cinc w8, w8, eq               ; isDeclared=true -> 1(AIDL), false -> 2(HIDL)
  d618: cmp w8, #0x1
  d61c: b.ne 0xd638                   ; 未在 VINTF 声明则跳过 0xd69c(AServiceManager_checkService)
  ```
- 修复前日志见同目录 [log-fail.txt](log-fail.txt)，修复后实测见 [log-pass.txt](log-pass.txt)。

## 4. 改动

全部改动收敛在 `engine/build/assets/devices.db` 的 `configs.id = 5`（`popsicle-android16`）：

1. **`buildVendor.build-prop-sets`**：
   - 新增 `"ro.surface_flinger.use_content_detection_for_refresh_rate": "false"` 与 `"vendor.display.enable_dpps_dynamic_fps": "0"`（禁用 LTPO AutoMode 10Hz 降频，锁定 60Hz）；
   - 新增 `"debug.config.media.video.dolby_vision_GPU_suports": "true"` 与 `"debug.media.video.chipset": "0"`（跳过 `SurfaceFlinger` 每帧 6 次的 `DolbyVisionGralloc5` `Smpte2094_40` 元数据查询与错误刷屏）。
2. **`buildVendor.fileTrees`**：
   - 新增 `etc/vintf/manifest/vendor.qti.hardware.perf2.xml`，声明 `vendor.qti.hardware.perf2`（AIDL v1 `IPerf/default`，与 `/system/etc/vintf/compatibility_matrix.device.xml` 对齐），使 `AServiceManager_isDeclared` 返回 `true` 并一次性绑定并缓存自研 `vendor.qti.hardware.perf2-hal-service`；
   - 在 `etc/init/frp_symlink.rc` 新增 `unbind_fbcon` 服务（`on boot` 与 `on property:sys.boot_completed=1` 触发）：执行 `echo 0 > /sys/class/vtconsole/vtcon1/bind` 解绑内核 `fbcon`，并停止 `loc_sys_service`、`iorapd`、`millet_monitor`。
3. **`buildVendor.sepolicy["vendor_sepolicy.cil"]`**：
   - 补 `(allow domain hal_sensors_default (binder (call transfer)))` 与反向规则，允许各进程域在 `perf2` VINTF 生效后无 AVC 拒绝地调用 `vendor.perf2-default`；
   - 补 `(allow surfaceflinger_{{vers}} vendor_default_prop_{{vers}} (file (open read getattr map)))`，消除 `SurfaceFlinger` 每帧读取 `ro.vendor.display.*` 的 `Access denied` 刷屏。

## 5. 验证

重跑 `07_build_vendor -> 09_build_patch -> 11_pack_vda` 并重启 `popsicle_16-OS4.0.5.0.XPBCN/1`（见 [log-pass.txt](log-pass.txt)）：

- `dumpsys display` 显示 `mActiveRenderFrameRate=60.0`（不再跌落 10.0）；
- `/sys/class/vtconsole/vtcon1/bind` = `0`，`init.svc.{loc_sys_service,iorapd,millet_monitor}` 全部为 `stopped`；
- 开机全过程 + 窗口切换（`Settings <-> Launcher`）后复核 `logcat -d -b all`：
  - `DolbyVisionGralloc5`: **0**（修复前 14,333）
  - `vendor.qti.hardware.perf2.IPerf/default in the VINTF manifest`: **0**（修复前 260）
  - `vendor.qti.hardware.perf@2.2::IPerf/default`: **0**（修复前 244）
  - `ro.vendor.display.bt2020.supported` / `max_hdr_luminance`: **0**（修复前各 1,347）
  - `avc: denied.*hal_sensors_default` / `hal_perf2_service`: **0**

## 6. 遗留

1. **HyperOS 4 桌面 CPU 软件 Vulkan 渲染瓶颈**：只要宿主运行在 macOS（`MoltenVK` 缺 `VK_KHR_external_semaphore_fd`），`libhyper_os_flutter.so` 只能走 `vulkan.pastel.so`（SwiftShader CPU 软渲染），滑动桌面帧率上限受宿主 CPU 单核/多核吞吐限制，无法达到 HyperOS 3 下 `virtio-gpu` OpenGL ES 硬件加速的丝滑度；若后续在 Linux 宿主（原生支持 `VK_KHR_external_semaphore_fd`）运行或为 macOS Venus 补齐金属信号量桥接，方可切回 `vulkan.virtio.so` 硬件加速。
2. **剩余非阻塞日志**：`MiSurfaceFlingerImpl::setPrivacyMode AidlIntf is null`（仅做本地空指针判断后直接返回，无 IPC 开销）与 `com.xiaomi.finddevice` 每秒查询一次 `IMTService`（约 1Hz，系统强校验该包存在性不可卸载）对帧率无实质影响，暂不额外造桩。

