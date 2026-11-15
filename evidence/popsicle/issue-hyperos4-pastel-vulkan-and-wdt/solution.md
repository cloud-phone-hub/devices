# Issue: popsicle HyperOS 4 (16-OS4.0.5.0.XPBCN) 桌面黑屏/缺图标、/cust 恢复后桌面无限重启与返回后 system_server Watchdog 软重启

- 设备：`xiaomi/popsicle`（Xiaomi 17 Pro Max / `canoe` / HyperOS 4 `16-OS4.0.5.0.XPBCN` / lineage-23.2 / kernel 6.12.81）
- 阶段：`07_build_vendor`（属性、文件裁剪、SELinux）、`engine/vendor/hardware/android.hardware.gpu`、`engine/vendor/hardware/android.hardware.gnss`
- 状态：**已闭环**（2026-11-06）
- 关联：同机 [#125](../issue-hal-hardware-missing/solution.md) / [#139](../issue-cust-from-userdata/solution.md) / [#140](../issue-weather2-impeller-vulkan/solution.md)（本条顺带通过 `vulkan.mali.so` + `vulkan.pastel.so` 彻底解决 #140 在 macOS 宿主上的 Impeller/Vulkan 渲染问题）

## 一、问题

Xiaomi 17 Pro Max 升级至 HyperOS 4（`16-OS4.0.5.0.XPBCN`）后出现三类致命现象：

1. **桌面黑屏 / 无应用图标**：开机后 `com.miui.home/.launcher.Launcher` 虽成为焦点窗口，但桌面壁纸、小部件及全部应用图标均无法渲染（或 `Create engine failed, just finish` 反复闪退）。
2. **`/cust` 分区挂载后桌面无限崩溃重启（掉入 `SafeLauncher`）**：按 `#139` 从 `userdata.img` 剖出 `cust.img` 并挂上 `/cust` 后，`com.miui.home`（`/system_ext/bin/hyos_spawner`）在渲染桌面首屏 `widget_stack_4` 毛玻璃小部件时反复触发 `Fatal signal 11 (SIGSEGV), fault addr 0x0 in sw::SpirvEmitter::EmitCompositeConstruct(sw::Spirv::InsnIterator)+388 (/mnt/vendorpatch/vendor/lib64/hw/vulkan.pastel.so)`，连崩数次后切入 `com.miui.home.safemode.SafeLauncher` 并继续无限重启。
3. **返回重启（system_server Watchdog 软重启）**：从设置等界面按返回键回桌面或开机约 60~90 秒后，系统触发 `Blocked in handler on foreground thread (android.fg) for 66s`，`system_server` 自杀重启并重播开机动画。

## 二、根因

### 1. HyperOS 4 桌面全面切换至 `libhyper_os_flutter.so`（Impeller/Vulkan），而默认 Vulkan 驱动链在 macOS 宿主上断裂 + SwiftShader 缺 SPIR-V `Float16`/`OpFConvert` 实现

- **云机双轨图形栈架构**：
  - **OpenGL ES 栈（`SurfaceFlinger` 合成器 + 普通 Android View/HWUI 应用）**：走 **`virtio-gpu` 硬件加速（VirGL）**——QEMU 启用 `-device virtio-gpu-gl-pci,blob=on,hostmem=256M,venus=on -display cgl-headless,gl=on,readback=on`，guest 侧由 `ro.hardware.egl=mesa` + `ro.hwui.use_vulkan=false` 经 `/dev/dri/renderD128` 交宿主 `virglrenderer` + macOS OpenGL/Metal 硬件渲染。
  - **Vulkan 栈（HyperOS 4 桌面 `com.miui.home` / 天气等自研 Flutter 应用）**：在 HyperOS 3（`OS3.0.319.0.WPBCNXM`）中仅天气等个别应用使用 `libhyper_os_flutter.so`（见 [#140](../issue-weather2-impeller-vulkan/solution.md)）；到了 HyperOS 4（`OS4.0.5.0.XPBCN`），小米将整个桌面 `com.miui.home`（经 `/system_ext/bin/hyos_spawner` 孵化）重写为 `libhyper_os_flutter.so` 场景引擎，且其渲染后端硬编码要求 Vulkan（`vk::CreateEngine`）。
- 厂商 `/vendor/build.prop` 原件带有 `ro.gfx.driver.1=com.qualcomm.qti.gpudrivers.canoe.api36`，导致 Android `GraphicsEnv` 优先加载 `/vendor/app/com.qualcomm.qti.gpudrivers.canoe.api36` 内的 `vulkan.adreno.so`，因云机无 `/dev/kgsl-3d0` 直接失败。
- 若改指 `ro.hardware.vulkan=virtio`（Mesa Venus），因 macOS 宿主 `libMoltenVK.dylib` 不支持 POSIX `SYNC_FD` 外置信号量扩展（`VK_KHR_external_semaphore_fd`），guest 侧 `vulkan.virtio.so` 不宣告 `VK_KHR_swapchain`，Impeller 报 `Device does not support required Android extension: VK_KHR_swapchain` 退出。
- 若改指 `ro.hardware.vulkan=lvp_mesa3d`（Mesa Lavapipe），虽支持 Vulkan 1.3，但 Lavapipe 通过 `IMapper 5` 直接写 guest CPU 内存后未触发 `minigbm` 的 `VIRTGPU_IOC_TRANSFER_TO_HOST`，导致宿主 `virglrenderer` 纹理保持全黑。
- Lineage 23.2 底包 `/vendor/lib64/hw/vulkan.pastel.so`（Google SwiftShader Vulkan 1.3）完整支持 `VK_KHR_swapchain`、`VK_ANDROID_native_buffer` 且能通过 `minigbm` 同步到 `virtio-gpu`，但存在一处致命缺陷：**未实现 SPIR-V `Float16` / `OpFConvert`（opcode `115`）**：
  - `vkCreateGraphicsPipelines` 阶段只跑声明遍历 `sw::Spirv::Spirv`（`0x7cf240`），遇到 `OpFConvert` 时在 `SpirvShader.cpp:534`（`0x7d06ac`）打一行 `WARNING: UNSUPPORTED: SPIR-V Float16 or Float64 Capability (OpFConvert)` 并**跳过 `DefineResult(insn)`**，但仍向调用方返回 `VK_SUCCESS`；
  - 真正的 Reactor JIT 编译延迟到首次绘制（`vkCmdDraw` -> `sw::SpirvShader::emit` -> `sw::SpirvEmitter::EmitInstructions`）。当 `/cust` 挂载后，桌面首页加载 `widget_stack_4` 毛玻璃卡片并实际绘制 `MiGlassV13F16` 着色器（`libhyper_os_flutter.so` 内置 125 个 SPIR-V 模块中仅有 `MiGlassSdf`、`mi_glass_v13_f16_fragment_main`、`mi_glass_v13_f16_legacy_fragment_main` 3 个模块开启了 `OpCapability Float16`），`EmitCompositeConstruct`（`0x7db38c`）在 `shader->defs` 哈希表查找 `OpFConvert` 产生的 SSA 结果 ID（`%610`）时命中空桶返回 `NULL`（`x8 = 0`），解引用 `ldr x8, [x8]` 直接 `SIGSEGV`！
- **正解**：扫描 `libhyper_os_flutter.so` 全部 125 个 SPIR-V 模块确认，上述 3 个 `Float16` 着色器仅将 `OpTypeFloat 16` 用于函数内部 SSA 寄存器中间量与标量 `OpConstant`（0 个 `Float16` 指针/Uniform/输入输出变量）。在 `engine/vendor/hardware/android.hardware.gpu/src/cdh_vulkan_mali.c`（产物 `/vendor/lib64/hw/vulkan.mali.so`）中：
  1. 支持通过 `ro.kernel.qemu.vulkan.base` / `ro.vendor.cdh.vulkan.base` 动态指定基座驱动（`popsicle` 设为 `"pastel"`，默认仍为 `vulkan.virtio.so`）；
  2. 拦截 `vkGetDeviceProcAddr` 与 `vkCreateShaderModule`，当检测到 SPIR-V 含 `OpCapability Float16` / `OpTypeFloat 16` / `OpFConvert` 时，在内存副本中单趟将纯寄存器级 `Float16` 提升为 `Float32`：
     - `OpCapability Float16` (`17, 9`) → `OpCapability Shader` (`17, 1`)；
     - `OpTypeFloat %id 16` (`22, %id, 16`) → `OpTypeFloat %id 32`；
     - 属于原 `Float16` 标量类型的 `OpConstant` / `OpSpecConstant`（`43` / `50`）→ 低 16 位 IEEE 754 `binary16` 经硬件 `fcvt s0, h0` 转为 `binary32`；
     - `OpFConvert` (`115`) → `OpCopyObject` (`83`)。

### 2. 返回重启（`system_server` Watchdog）由两处 Binder 死锁叠加引起

从 `/data/anr/traces_SystemServer_WDT06_11_12_20_08.279_pid12829` 取证定位到两条阻塞链：

1. **`android.hardware.gnss-service` 同步回调引发 `system_server` 锁序反转（ABBA 死锁）**：
   - `system_server` 的 `android.fg` 线程（`tid=12844`）持有 `GnssNative$GnssHal` 锁调 `IGnss::start()`；
   - `gnss_service.cpp` 的 `start()` 在同一条 Binder 事务栈内**同步回调** `callback->gnssStatusCb(ENGINE_ON)`，Binder 将该回调路由回调用方线程 `android.fg`，使其重入 `GnssStatusProvider.onReportStatus` 并尝试获取 `ListenerMultiplexer` 锁 `<0x02fec1c5>`；
   - 与此同时，HyperOS 的 `"GnssDfs thread"`（`tid=13401`）已持有 `<0x02fec1c5>` 锁并向 `gnss-service` 发起 `IGnss::startSvStatus()`；
   - 因 `gnss_service.cpp` 的 `main()` 未调用 `ABinderProcess_startThreadPool()`，且 `start()` 阻塞在同步回调上，两边形成死锁，直接卡死 `android.fg` 超过 60 秒触发 Watchdog。
2. **孤立的 C2 VINTF 片段导致 `mediacodeclist_generator` / `mediaserver64` 阻塞 96 秒**：
   - 基线 `vendor.files` 删除了 `etc/init/android.hardware.media.c2-mediatek.rc` 与 `etc/init/vendor.dolby.media.c2-default-service-dax.rc`，但未删除同模块安装的 `etc/vintf/manifest/manifest_media_c2_default.xml`（声明 `IComponentStore/default`）与 `etc/vintf/manifest/vendor.dolby.media.c2-default-service-dax.xml`（声明 `IComponentStore/dolby`）。
   - `mediacodeclist_generator` 按 VINTF 枚举 C2 服务时对这两个永不启动的实例 `waitForService` 阻塞 96 秒，连带卡死 `mediaserver64` 与 `system_server` 的 `StorageManagerService`。

## 三、证据

- `log-fail.txt`：Adreno KGSL 报错、Venus 缺 `VK_KHR_swapchain` 报错、`vulkan.pastel.so` 在 `/cust` 挂载后因 `UNSUPPORTED: SPIR-V Float16 or Float64 Capability (OpFConvert)` 触发 `sw::SpirvEmitter::EmitCompositeConstruct+388` `SIGSEGV` 的崩溃栈，以及 `traces_SystemServer_WDT` 中 `android.fg`、`GnssDfs thread` 与 `mediacodeclist_generator` 的完整死锁栈。
- `log-pass.txt`：启用 `ro.hardware.vulkan=mali` + `ro.kernel.qemu.vulkan.base=pastel` 后，`cdh-gpu` 拦截改写 3 个 `Float16` SPIR-V 模块（`size=16700/22816/25400, fconverts=18/35/48`），桌面与 `/cust` 小部件稳定渲染、零崩溃、零 Watchdog 重启。
- `screenshot.png`：VNC 实测抓取的 HyperOS 4 完整桌面、`MiGlassV13F16` 毛玻璃小部件/文件夹与 `/cust` 预装应用图标渲染画面。

## 四、改动

1. **Vulkan HAL Interposer（`engine/vendor/hardware/android.hardware.gpu/src/cdh_vulkan_mali.c`）**：
   - `cdh_open` 支持读取 `ro.kernel.qemu.vulkan.base` / `ro.vendor.cdh.vulkan.base` 动态选择基座驱动（如 `vulkan.pastel.so`），`cdh_stamp` 支持读取 `ro.kernel.qemu.vulkan.name` / `ro.vendor.cdh.vulkan.name` 覆写 `deviceName`；
   - 拦截 `vkGetDeviceProcAddr` 与 `vkCreateShaderModule`（`cdh_csm`），对含 `Float16` 的 SPIR-V 模块自动执行 `Float16` → `Float32` 寄存器级提升与 `OpFConvert` → `OpCopyObject` 改写。
2. **配置模版 `popsicle-android16`（`assets/devices.db` 的 `configs.id = 5`，由 `#8` 与 `#9` 共享）**：
   - `buildVendor.build-prop-sets`：设置 `"ro.hardware.vulkan": "mali"`、`"ro.kernel.qemu.vulkan.base": "pastel"`、`"ro.kernel.qemu.vulkan.name": "Adreno (TM) 840"`、`"ro.vendor.audio.dolby.vision.support": "false"`；
   - `buildVendor.build-prop-unsets`：新增 `"ro.gfx.driver.1"`（阻止加载高通 Adreno GPU 驱动 APK）；
   - `buildVendor.files`：新增 `{"mode":"delete","source":"etc/vintf/manifest/manifest_media_c2_default.xml"}` 与 `{"mode":"delete","source":"etc/vintf/manifest/vendor.dolby.media.c2-default-service-dax.xml"}`；
   - `buildVendor.sepolicy`（`vendor_sepolicy.cil`）：补充 `pioneer` 与 `system_server` 的双向 binder 调用/fd 传递授权，以及 `system_app` 对 `sysfs` 的读取授权。
3. **GNSS HAL 服务（`engine/vendor/hardware/android.hardware.gnss/src/gnss_service.cpp`）**：
   - `main()` 在 `ABinderProcess_setThreadPoolMaxThreadCount(4)` 后显式调用 `ABinderProcess_startThreadPool()`；
   - `start()`、`stop()`、`startSvStatus()`、`startNmea()`、`injectLocation()`、`injectBestLocation()` 改为仅在互斥锁下置位异步标记并立即返回 `ScopedAStatus::ok()`，由后台 `poller_` 线程（25ms 周期）异步派发 `IGnssCallback` 回调，彻底消除 `system_server` 同步重入锁反转。

## 五、验证

1. `getprop ro.hardware.vulkan` 返回 `mali`，`getprop ro.kernel.qemu.vulkan.base` 返回 `pastel`，`mount | grep /cust` 显示 `/dev/block/vda12 on /cust type erofs`。
2. `logcat -d | grep cdh-gpu` 打印：
   - `I cdh-gpu : vulkan: 已接管，基座 vulkan.pastel.so`
   - `I cdh-gpu : vulkan: SPIR-V Float16->Float32 已改写 (size=16700, fp16_types=1, consts=3, fconverts=18) -> 0`
   - `I cdh-gpu : vulkan: SPIR-V Float16->Float32 已改写 (size=22816, fp16_types=1, consts=25, fconverts=35) -> 0`
   - `I cdh-gpu : vulkan: SPIR-V Float16->Float32 已改写 (size=25400, fp16_types=1, consts=32, fconverts=48) -> 0`
3. 桌面 `com.miui.home/.launcher.Launcher` 稳定常驻（不再崩溃或回退 `SafeLauncher`），完整渲染壁纸、`MiGlassV13F16` 毛玻璃材质小部件/文件夹及 `/cust` 预装应用图标（见 `screenshot.png`）。
4. 从设置等应用按返回键回到桌面流畅无卡顿，连续运行无 `Watchdog` / `ANR` / 软重启。

## 六、遗留

- `vulkan.pastel.so`（SwiftShader CPU Vulkan）可满足桌面 Launcher 与常规 Flutter UI 渲染，但重度 3D Vulkan 游戏帧率受 CPU 软光栅限制；后续若在 macOS 宿主实现高性能硬件 Vulkan，仍需在 `virglrenderer`/MoltenVK 侧桥接二进制信号量与导出纹理。

