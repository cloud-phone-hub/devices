# #304 oriole A17：Portrait 人像模式无画面 + Long Exposure 长曝光闪退（RAW_DEPTH10 / OpenCL 桩库与 cdh_redirect SP-HAL 放行）

## 1. 问题

Pixel 6 `#5`（`oriole_17-CP3A.261005.002.A1-20260905`）在原生 Google Camera（`com.google.android.GoogleCamera`）中存在两个独立模式故障：

1. **Portrait（人像模式）无画面**：切到 `Portrait` 后底部 UI（`1×/2` 变焦、快门、模式栏、设置入口）齐全，但上方取景框全黑、无预览画面；
2. **Long Exposure（长曝光模式）闪退**：点击 `Long Exposure` 立即触发 native `SIGSEGV` 闪退。

## 2. 根因

### 2.1 Portrait 无画面：Camera HAL 缺失 `RAW_DEPTH10` 深度流静态配置

- 反编译设备内 `GoogleCamera.apk`（`classes.dex`）确认人像模式配置链 `knd.a(PG:19) ← koc.a ← koi.a(kow.j /* PD */, ...)` 在全系 7 套 Pixel 配置文件中均开启 `pd_image_format_is_raw_depth10 = true`（`gpg.l`），强制查询 `ImageFormat.RAW_DEPTH10 (4099 = 0x1003)` 的可用输出尺寸；
- 反编译 `/system/framework/framework.jar` 的 `StreamConfigurationMap.depthFormatToPublic` 与 `CameraMetadataNative` 确认：`RAW_DEPTH10 (4099)` 由静态元数据 `ANDROID_DEPTH_AVAILABLE_DEPTH_STREAM_CONFIGURATIONS` 中的 `HAL_PIXEL_FORMAT_RAW10 (0x25)` 映射而来，`RAW_DEPTH (4098)` 由 `HAL_PIXEL_FORMAT_RAW16 (0x20)` 映射而来；
- 原 `camera_provider_service.cpp` 未向 Pixel 双摄静态元数据注入 `ANDROID_DEPTH_AVAILABLE_DEPTH_STREAM_CONFIGURATIONS` 及配套时长/独占键，导致 Portrait 查询输出尺寸为空，抛 `java.lang.IllegalStateException: No supported output sizes found!!` → `OneCamera failed to open`，取景框无帧输出。

### 2.2 Long Exposure 闪退：三层串联根因

1. **MotionBlur（Lasagna/Paneer）强依赖 OpenCL 且 JNI 未判空**：
   - 反汇编 `libgcastartup.so`：`LasagnaNativeProcessorJni_create`（`0x2f8b758`）调用 `CreatePaneerProcessor`（`0x2fa44d4`），只读配置表 `0x70cab0 = {4, 4032, 3024, 1}` 将 `use_opencl` 默认置为 `1`；
   - `MotionBlurProc` 线程查询 OpenCL 平台失败（`No OpenCL platforms found`）后，`CreatePaneerProcessor` 返回 `nullptr`，而 `lasagna_processor_jni.cc` 未判空就将 `nullptr` 写入 `LasagnaNativeProcessor + 0x48` 返回给 Java（绕过 Java `ijr` 的 `this.d != 0` 守卫）；切入 Long Exposure 调用 `setOptions` 时直接 `ldr w8, [x0, #0x10]` 空指针解引用 `SIGSEGV`（`fault addr 0x10`）。
2. **`GoogleCamera` 的 `LoadOpenCL()` 强依赖 Pixel 专属符号**：
   - 反汇编 `LoadOpenCL()`（`0x590bd58..0x590bf00`）：`GoogleCamera` 通过 `uses_libraries=lib_aion_buffer.so:libOpenCL-pixel.so` 加载 `libOpenCL-pixel.so` 后，依次解析 `enableOpenCL` 与 `loadOpenCLPointer`；任一缺失会导致整个 OpenCL 函数指针表为 `0`，引发 `NativeDeblurFusion_nativeInitialize`（`0x59141ac`）跳 `pc=0x0` 崩溃；
   - 同时不可对全部线程暴露虚拟 OpenCL 平台，否则 `PortraitSegmenter` / `NativeDeblurFusion` / `HDR+` 会放弃稳定的 CPU/OpenGL 回退路径。
3. **冷启下普通 App（`uid > 10000`）未命中载体 `libOpenCL*.so`**：
   - `cdh_redirect` 前向重定向默认只对 `uid <= cdh_max_uid(10000)` 生效，`uid > 10000` 仅对 `cdh_classify_sphal_lib()` 收录的 SP-HAL 库按调用栈门控放行；
   - 原 `cdh_classify_sphal_lib()` 未包含 `libOpenCL.so` 与 `libOpenCL-pixel.so`，导致冷启后 `GoogleCamera`（`uid=10138`）绕过 `patch.img` 加载了官方只读 `dm-4` 上的原厂 Mali `libOpenCL*.so`（82808 B），报 `library "/vendor/lib/egl/libGLES_mali.so" needed or dlopened by "/vendor/lib64/libOpenCL.so"` 并回落 `No OpenCL platforms found`。

## 3. 证据

- 失败日志与根因定位摘要：[log-fail.txt](log-fail.txt)
- 冷启终验日志：[log-pass.txt](log-pass.txt)
- Portrait 模式冷启实屏截图：[portrait-pass.png](portrait-pass.png)
- Long Exposure 模式冷启实屏截图：[long-exposure-pass.png](long-exposure-pass.png)

## 4. 改动

1. **补齐 Pixel 双摄 `RAW_DEPTH10` / `RAW_DEPTH` 静态元数据**：
   - 修改 [camera_provider_service.cpp](../../../hardware/android.hardware.camera/src/camera_provider_service.cpp)：在 `isPixelDualRearDevice()` 下补齐 `ANDROID_DEPTH_AVAILABLE_DEPTH_STREAM_CONFIGURATIONS`（`kRaw10=0x25` → `RAW_DEPTH10`、`kRawSensor=0x20` → `RAW_DEPTH`，后摄 `{halfW, 756}, {pixelWidth, 756}, {halfW, halfH}`，前摄 `{halfW, halfH}, {pixelWidth, pixelHeight}`）、`ANDROID_DEPTH_AVAILABLE_DEPTH_MIN_FRAME_DURATIONS`（`33333333`）、`ANDROID_DEPTH_AVAILABLE_DEPTH_STALL_DURATIONS`（`0`）、`ANDROID_DEPTH_DEPTH_IS_EXCLUSIVE = FALSE`，并同步追加到 `characteristicsKeys`。
2. **升级 OpenCL 兼容桩库（按线程隔离 + Pixel 专属入口 + 完整 3.0 符号）**：
   - 修改 [cdh_opencl_stub.c](../../../hardware/android.hardware.gpu/src/cdh_opencl_stub.c)：
     - 默认线程调 `clGetPlatformIDs` 仍返回 `CL_PLATFORM_NOT_FOUND_KHR (-1001)`，保持 `PortraitSegmenter` / `NativeDeblurFusion` / `HDR+` 等原 CPU/OpenGL 回退路径零变化；
     - 仅对 `MotionBlur` 线程（`strncmp(name, "MotionBlur", 10) == 0`）及带 magic `0x43444843` 的虚拟句柄提供 OpenCL 3.0 内存型实现（platform/device/context/queue/program/kernel/mem/image/sampler/event 与真实 `calloc`/`memcpy`）；
     - 补齐 Pixel `libOpenCL-pixel.so` 专属导出符号 `enableOpenCL()`、`loadOpenCLPointer(const char*)` 及 155 个 OpenCL 2.0/3.0 符号；
   - 以 NDK 35 编译产出 `libOpenCL.so` 与 `libOpenCL-pixel.so`（各 `43864 B`）落盘至 `devices/manifest/configs/google/oriole/oriole-android17/vendor/lib64/`。
3. **`cdh_redirect` 载体存在探测 + SP-HAL Tier B 放行 + `maps` `dev:ino` 预热**：
   - 修改 [cdh_redirect.h](../../../modules/cdh_redirect/include/cdh_redirect.h)、[maps.c](../../../modules/cdh_redirect/hook/maps.c)、[rules.c](../../../modules/cdh_redirect/core/rules.c)：
     - 在 `cdh_maps_prewarm_fn()` 启动预热阶段探测 `/mnt/vendorpatch/vendor/{lib64,lib}/{libOpenCL.so,libOpenCL-pixel.so}` 存在掩码（`cdh_carrier_opencl_mask`），仅当对应载体文件实际存在时才对 `uid > max_uid` 的 `linker` 加载放行（`CDH_SPHAL_TIER_B`），未携带桩库的其他机型自动直通原厂、零副作用；
     - 将 `/vendor/{lib64,lib}/{libOpenCL.so,libOpenCL-pixel.so}` 加入 `cdh_maps_prewarm_libs[]`，保证 App 读 `/proc/self/maps` 时 `dev:ino` 保持原厂只读分区一致性，且普通 App 非 linker 直接 `open/stat` 仍回落原厂文件；
   - 重跑 `04_build_modules → 06_build_vendor_boot → 10_build_vendor_dlkm → 11_build_patch → 13_pack_vda`（模块配方 `6.1-cdh-e12cb389986b`，`patch.img` `4fd5a8d80a74`，`vda.img` `vda-9f488adfa426`）。

## 5. 验证

停止 Pixel 6 `#5` 并使用新盘集**无热挂冷启**终验（2026-11-22，ADB `127.0.0.1:5911`，VNC `127.0.0.1:5912`）：

1. **Portrait 人像模式**：冷启切换 `Portrait`，logcat 命中 `CAM_RAW_DEPTH10w2032h15`，`OneCamera failed to open` 与 `No supported output sizes found!!` 归零；VNC 实屏审图（[portrait-pass.png](portrait-pass.png)）清晰呈现实时视频流、`1×/2` 变焦与完整控件。
2. **Long Exposure 长曝光 & Action Pan**：冷启进入 `Long Exposure`、连续切换 `Long Exposure → Portrait → Long Exposure → 按快门拍照 → Action Pan`，`GoogleCamera` PID 全程恒定为 `4057`，`logcat -b crash` 精确为 `ZERO_GCAM_CRASH`，`libGLES_mali.so` linker 告警与 `No OpenCL platforms found` 均为 `0`；VNC 实屏审图（[long-exposure-pass.png](long-exposure-pass.png)）清晰呈现 `Long Exposure` 胶囊高亮、动态条纹快门图标、`.7/1×/2` 变焦与实时取景画面。

## 6. 遗留

- `cdh_opencl_stub.c` 当前按需用 NDK 35 预编落盘到 `oriole-android17/vendor/lib64/`；后续如需覆盖更多搭载 Google Camera / 强依赖 OpenCL 的机型，可将其纳入 `08_build_hardware` 自动化构建产物，配合本次 `cdh_redirect` 载体存在探测机制即可开箱即用。
- Long Exposure 虚拟 OpenCL kernel 为无副作用内存型空算子（保证预览与拍照全链路不崩），不执行真实 GPU 运动模糊滤波计算。
