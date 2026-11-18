# Issue: popsicle `16-OS4.0.5.0.XPBCN` 冷启动全链路图形与启动卡点：`sphal` `common-V6-ndk` 代际错配、39-bit VA PAC 掩码、QTI Mapper 自包含与 Android 16 Minigbm `lock()` 拦截、以及 `persist.sys.boot_audio.enabled` 20s 黑屏

- 设备：`xiaomi/popsicle`（Xiaomi 17 Pro Max / `canoe` / HyperOS 4 `16-OS4.0.5.0.XPBCN` / Android 16 / `lineage-23.2` / kernel `6.12.81`）
- 前序条目：`../issue-android16-hal-deps-missing/`（#247 跑通 01→13 构建，遗留「未上机验证新盘集」）
- 验证盘集：`vda-0ec932c6010e` + `patch.img`（`sha256: b2d8a2fcec4a`）
- 状态：**已闭环**（2026-11-18，实例 #1 全新冷启动）：`unbind_fbcon`（`20.32s`）至 `BootAnimationShownTiming`（`21.30s`）仅隔 **0.98s**（零长黑屏），`sys.boot_completed=1`、首屏 `com.miui.home/.launcher.Launcher` `mDrawState=HAS_DRAWN`、桌面图标/时钟小部件完整显示且手势滑动/点击实时响应（`responsive=true`），各项图形/SELinux 异常计数全为 `0`

---

## 一、问题

承接 `../issue-android16-hal-deps-missing/`（#247）产出的 `popsicle_16-OS4.0.5.0.XPBCN` 盘集上机冷启动，从开机 Logo 到桌面可交互共暴露出**五道串联卡点**：

1. **卡点 #1（卡开机 Logo，`zygote64` 无限 `SIGSEGV`）**：`zygote64` 每次启动在 `eglGetDisplay+68` 触发 `SIGSEGV` 重启；且 `libEGL_meow.so` 日志只打印 `dlopen 基座库失败: (null)`。
2. **卡点 #2（`com.miui.home` 加载 `vulkan.pastel.so` 被拦截）**：`com.miui.home`（`uid=10147`）报 `dlopen failed: library "vulkan.pastel.so" not found`。
3. **卡点 #3（`SystemUI` 与 `miwallpaper` 循环 `SIGABRT` + `mediaswcodec` SELinux `{ map }` 拒绝）**：`com.miui.miwallpaper` 与 `com.android.systemui` 报 `Abort message: 'gralloc-mapper is missing'`；切换自编 `allocator` 后若未开 `HAS_DMABUF_SYSTEM_HEAP`，元数据走 `memfd_create` 触发 `mediaswcodec` 对 `hal_graphics_allocator_default_tmpfs` 的 `{ map }` 拒绝。
4. **卡点 #4（内核控制台日志消失到 HyperOS Logo 之间有 ~20.4 秒黑屏）**：`unbind_fbcon` 在 `~24.3s` 解绑控制台后黑屏等待至 `~44.5s` 才显示 HyperOS Logo；在 `vendor/config.json` 配置 `"persist.sys.boot_audio.enabled": "0"` 写进 `/vendor/build.prop` 后被 `vendor_init` SELinux 直接拒绝丢弃。
5. **卡点 #5（进桌面仅显示纯绿壁纸、无图标小部件且点不动）**：进入桌面后只有绿色壁纸与状态栏，桌面无任何图标/小部件且触控无响应（下拉控制中心可点，仅桌面区域被 `ActivityRecordInputSink` 拦截）。

---

## 二、根因

### 1. `gpu.go` 双重调用 `dlerror()` + `common-V6-ndk.so` 重定向遮蔽（卡点 #1）

- `agent/modules/pipeline/internal/halcore/halbuild/gpu.go` 写成 `dlerror() ? dlerror() : "?"`，首次调用即清空 Bionic 内部错误槽，第二次必回 `NULL`。
- 真实报错为官方 `/vendor/lib64/android.hardware.graphics.allocator-V2-ndk.so`（SDK 36）依赖 `common-V6-ndk.so`，而 `/vendor/lib64` 目录重定向至 `lineage-23.2` 载体后只有 `common-V7-ndk.so`。按声明式重定向约束，需在机型 `module/config.json` 的 `dir /vendor/lib64` 之前声明 `common-V6-ndk.so` 自映射规则（严禁在 C/Go 代码写死路径）。

### 2. GKI 4K 内核 39-bit VA 下 ARMv8.3 PAC 指针认证高位未剥离（卡点 #2）

- 本机 GKI 4K 内核为 `CONFIG_ARM64_VA_BITS=39`（`DEFAULT_MAP_WINDOW_64 = 1UL << 39`），且 `popsicle` 开启 ARMv8.3 PAC（`pac_enabled_keys: 0xf`），栈帧 `LR` 在位 `[54:39]` 含 PAC 签名。
- `devices/modules/cdh_redirect/core/rules.c` 原硬编码 48-bit 掩码 `0x0000ffffffffffffUL`，残留位 `[47:39]` 导致 `find_vma(mm, pc)` 查不到 `vulkan.mali.so`，将 `vulkan.pastel.so` 误判拦截。改为 `#define CDH_USER_VA_MASK (DEFAULT_MAP_WINDOW_64 - 1UL)` 即可精确剥离。

### 3. 缺自包含 `graphics.allocator`、`_upstream` 后缀残留与 `HAS_DMABUF_SYSTEM_HEAP`（卡点 #3）

- `popsicle-android16` 原本未配 `"android.hardware.graphics.allocator"`，退回基座 `mapper.minigbm_upstream.so`（依赖载体私有库 `libminigbm_gralloc_upstream.so`，`uid > 10000` 进程在 `sphal` 下不可见）。
- 启用自编 `android.hardware.graphics.allocator` 并在 `09_build_vendor` 收口 `_upstream` 为 `mapper.qti.so` 后，还需开启 `-DHAS_DMABUF_SYSTEM_HEAP=1`（通过零外部库依赖的内联头 `BufferAllocator/BufferAllocator.h` 直调 `/dev/dma_heap/system` `DMA_HEAP_IOCTL_ALLOC`），让缓冲区元数据落入 `dmabuf_system_heap_device` 而非 `hal_graphics_allocator_default_tmpfs`。

### 4. 开机音效阻塞 `AudioFlinger` 20 秒 + Treble 禁止 `vendor_init` 写 `persist.sys.*`（卡点 #4）

- Xiaomi `bootanimation` 默认开启开机音效（`persist.sys.boot_audio.enabled` 未设为 `0`），在绘制首帧 Logo 前同步调用 `AudioSystem::getService("media.audio_flinger")` 轮询阻塞 ~20 秒（此时 `audioserver` 尚未就绪），而 `unbind_fbcon` 已在 `boot` 阶段解绑 framebuffer 控制台，造成 20.4 秒黑屏。
- Treble SELinux 禁止 `u:r:vendor_init:s0` 在解析 `/vendor/build.prop` 时写 `exported_system_prop` / `system_prop`（`init: Do not have permissions to set 'persist.sys.boot_audio.enabled' to '0' in property file '/vendor/build.prop'`）。必须由 `09_build_vendor` 将 `vendor.build-prop-sets` 中的 `persist.sys.*` / `persist.system.*` 从 `vendor/build.prop` 剥离，生成 `vendor/etc/init/cdh_system_props.rc` 在 `post-fs-data`（`~16s`，早于 `bootanim` 5 秒）以 `u:r:pioneer:s0` 执行 `setprop`。

### 5. Android 16 上游 Minigbm 新增非软渲染 `lock()` 门控打断 Impeller Vulkan Swapchain（卡点 #5）

- 实时抓取 `com.miui.home` 首帧日志定位到精确致命链：
  `Attempted to lock() a buffer that was not allocated with a BufferUsage::CPU_* usage.` → `QueueSignalReleaseImageANDROID failed: -1000001004`（`VK_ERROR_OUT_OF_DATE_KHR`）→ `Could not create swapchain: ErrorNativeWindowInUseKHR` → `SurfaceContextVK::AcquireNextSurface abnormal ... surface=0x0`。
- Android 16 上游 `@platform/external/minigbm/cros_gralloc/cros_gralloc_driver.cc`（第 161–165、487–494 行）新增了门控：当 `!is_running_with_software_rendering_` 且缓冲区未带 `CPU_*` 用途位时，`lock()` 直接返回 `-EINVAL`；而上游 `is_running_with_software_rendering()` 仅判断 `drv_get_os_option("ro.hardware.vulkan")` 是否含 `"pastel"`。
- 云机启用 Vulkan interposer 时 `ro.hardware.vulkan=mali`，真正的 SwiftShader 基座由 `ro.kernel.qemu.vulkan.base=pastel` / `/vendor/lib64/egl/gpu_id.cfg`（`ro.vendor.cdh.vulkan.base=pastel`）指定，导致 `is_running_with_software_rendering_` 误判为 `false`。`vulkan.pastel.so` 首帧呈现锁定缓冲区被拒后，Impeller Swapchain 断连使 `Launcher` 窗口一直处于 `mHasSurface=false`，WindowManager 的 `ActivityRecordInputSink` 随之拦截桌面区域一切触控事件。

---

## 三、证据

- 失败现场：同目录 `log-fail.txt`（完整记录阶段 1~5 报错原文）。
- 通过现场：同目录 `log-pass.txt`（`vda-0ec932c6010e` 冷启动 `persist.sys.boot_audio.enabled=0`、`BootAnimationShownTiming: 21298ms`、`Launcher` `mDrawState=HAS_DRAWN`、桌面手势 `GestureBinding` 命中、以及全量单测通过记录）。

---

## 四、改动

1. **机型声明式配置（`popsicle-android16`）**：
   - `module/config.json`：在 `dir /vendor/lib64` 前声明 `file /vendor/lib64/android.hardware.graphics.common-V6-ndk.so` 自映射规则（`02_prepare` 同步支持自动测量生成）。
   - `hardware/config.json` & `vendor/config.json`：启用自建 `"android.hardware.graphics.allocator"`；`vendor.build-prop-sets` 增加 `"persist.sys.boot_audio.enabled": "0"`（`02_prepare/apply_profile.go` 同步对小米系机型默认推导该属性）并将 `ro.vendor.cdh.gpu.gl_extra_exts2` 设为 `"none"`。
2. **Android 16 自包含 `android.hardware.graphics.allocator` 增强**：
   - `devices/hardware/android.hardware.graphics.allocator/src/BufferAllocator/BufferAllocator.h` + `08_build_hardware/modules/android.hardware.graphics.allocator.json`：新增零外部依赖 DMA-BUF system heap 分配头并开启 `-DHAS_DMABUF_SYSTEM_HEAP=1`。
   - `devices/hardware/android.hardware.graphics.allocator/src/cdh_os_option_wrap.cpp` + `-Wl,--wrap=drv_get_os_option`：在不改动上游 `external/minigbm` 源码的前提下，当查询 `"ro.hardware.vulkan"` 且基座配置（`ro.kernel.qemu.vulkan.base` 或 `gpu_id.cfg`）为 `pastel` 时返回 `"pastel"`，使 `allocator` 与 `mapper` 正确开启软件渲染 `lock()` 兼容。
3. **`09_build_vendor` 通用对齐与单测**：
   - `files_compat.go`：`alignVendorGrallocConfig` 收口 `lineage-23.2` `_upstream` 后缀；新增 `alignVendorSystemProps`（注册于 `assemble.go`）剥离 `vendor/build.prop` 中的 `persist.sys.*` / `persist.system.*` 并生成 `vendor/etc/init/cdh_system_props.rc`（`post-fs-data` + `boot` 以 `u:r:pioneer:s0` 执行 `setprop`）；同目录 `props_test.go` 补齐 `TestAlignVendorSystemProps`。
4. **`gpu.go` 与 `cdh_redirect` 修复**：
   - `halbuild/gpu.go` 单次缓存 `dlerror()`；`cdh_redirect/core/rules.c` 改用 `(DEFAULT_MAP_WINDOW_64 - 1UL)` 剥离 39-bit VA PAC 高位。

---

## 五、验证

| 核验项 | 结果 |
| :--- | :--- |
| 构建与单测 | `go test ./modules/pipeline/...` 全包通过；`cdh-pipeline device build popsicle --manifest 16-OS4.0.5.0.XPBCN --from 08_build_hardware` 产出 `vda-0ec932c6010e` + `patch.img`（`b2d8a2fcec4a`） |
| 开机 Logo 衔接耗时 | `persist.sys.boot_audio.enabled=0` 在 `15.99s`（`post-fs-data`）生效；`unbind_fbcon`（`20.32s`）到 `BootAnimationShownTiming`（`21.30s`）仅隔 **0.98s**（消除原 20.4s 黑屏） |
| 首屏桌面渲染与触控 | `com.miui.home/.launcher.Launcher` `Surface: shown=true mDrawState=HAS_DRAWN`；`FocusedWindows` 命中 `Launcher`（`touchableRegion=[0,0][1200,2608]`, `responsive=true`），桌面应用图标/时钟部件完整呈现，滑动翻页与点击应用实时响应 |
| 关键错误计数 | `attempted_to_lock=0`、`acquire_next_surface_abnormal=0`、`error_native_window_in_use_khr=0`、`gralloc_mapper_missing=0`、`allocator_tmpfs_avc=0`、`vendor_init_boot_audio_denied=0` |

---

## 六、遗留

1. **`.qtidataservices` Java 层 `UnsatisfiedLinkError`**：后台扩展进程 `.qtidataservices` 查找 `Java_vendor_qti_iwlan_JniHelper_getAllQualifiedNetworks` 的非致命异常不影响桌面与系统稳定性，后续在 Radio/IWLAN 专项收口。
2. **同代 Android 16 机型直接受益**：`BufferAllocator.h`、`cdh_os_option_wrap.cpp`、`_upstream` 对齐与 `cdh_system_props.rc` 均为通用机制，后续 `lineage-23.2` 机型直接复用。
