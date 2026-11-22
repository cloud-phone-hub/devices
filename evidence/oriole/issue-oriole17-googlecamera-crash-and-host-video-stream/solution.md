# #302 Pixel 6 (oriole) Android 17 原生 Google Camera 闪退全链路修复与 Host → Guest Camera 视频流注入

## 1. 问题

1. Pixel 6（`google/oriole`）Android 17（`17-CP3A.261005.002.A1-20260905`）云机上，打开原生 **Google Camera**（`com.google.android.GoogleCamera`）直接闪退（`Camera keeps stopping`），即便绕过首层崩溃后仍陆续出现黑屏取景、`onCaptureFailed` 与 `pck-temporal-bi` SIGABRT；
2. 宿主侧需要支持向云机 Guest Camera 实时注入视频流（本地视频文件、RTSP/RTMP/HTTP/UDP/SRT 网络流、宿主物理摄像头、内置测试源），并在 Web 投屏面板中一键切换、上传与监控推流状态。

## 2. 根因

本次排查共穿透 **10 层串联根因**：

1. **厂商依赖库与 Vendor Tag 缺失（层 1~5）**：Google Camera 依赖 `lib_aion_buffer.so`、`pixel-power-ext-V1-ndk.so`、`libOpenCL-pixel.so`（含 `public.libraries.txt` 公开声明与 `same_process_hal_file` SELinux 标签）及 109 条 `com.google.pixel.experimental*` Vendor Tag、RAW10/RAW_PRIVATE 流配置与 VINTF 幽灵 Codec2 清理。
2. **Camera HAL 协议与 Pixel 双摄拓扑不匹配（层 6）**：
   - 静态元数据宣告 `partialResultCount = 10`，但 `processCaptureRequest()` 写死 `result.partialResult = 1`，导致 `Camera3-Device` 永不完成 in-flight 请求（`In-flight list too large` / `waitUntilDrainedLocked -110`）；
   - Pixel 6 GoogleCamera Deblur Fusion（`hsy.<init>`、`hnd.d`、`libgcam` `StaticMetadata::Check`）强依赖后摄为 Logical Multi-Camera（`"0"` 聚合宽角主摄 `"2"` `4064×3072` / binned `2032×1536` 与超广角 `"3"` `4032×3016` / binned `2016×1508`）；
   - `CaptureResult` 缺少 `LENS_FOCUS_DISTANCE` 等 55+ 标准字段及 `ANDROID_STATISTICS_LENS_SHADING_MAP`（触发 `CallbackHndlr` `eru.a` NPE 与 `CAM_krf` `Frame rejected`）。
3. **VirGL EGL / GLSL ES 3.00 能力边界（层 7~8）**：
   - `fast_moments_hdr` 请求 GLES 3.1/3.2 或扩展属性触发 VirGL `EGL_BAD_MATCH` → `egl_context.IsValid()` 断言崩溃；
   - `rectiface` / `moment` / `raw_preprocessor` 共 15 个 GLSL ES 3.10/3.20 着色器使用了 VirGL GLES 3.00 不支持的 `GL_EXT_separate_shader_objects`、`GL_EXT_YUV_target`、`layout(binding=N)`、`layout(location=N) uniform`、`highp in/out` 语序、`textureGather` 与 `[3][3]` 二维数组。
4. **Minigbm Gralloc Allocator 与 ImageReader RAW10 协议不匹配（层 9~10）**：
   - VirGL minigbm 拒绝 `YCbCr_420_888` 叠加 `VIDEO_ENCODER`/`CAMERA_*`+`CPU_WRITE_OFTEN`（`usage=0x10133`），且 `cros_gralloc_convert_format()` 未映射 `RAW10(37)`/`RAW_OPAQUE(36)`/`RAW12(38)`；
   - 仅改用 `RAW16(32)` 分配时，`cros_gralloc_handle.droid_format` 变为 `32` 导致跨进程 `GraphicBufferMapper::validateBufferSize` 返回 `-22`；回写 `droid_format=37` 后，`ImageReader::getLockedImageInfo()` 进一步断言 `RAW10` 的 `stride` 单位为字节（`stride >= width * 10 / 8` 且 16 字节对齐），而 minigbm 的 `pixel_stride` 为像素数（`4064 < 5080`）触发 `SIGABRT`。

## 3. 证据

- 失败日志：[log-fail.txt](log-fail.txt)（包含全部 10 层根因现场错误与栈帧）
- 通过日志：[log-pass.txt](log-pass.txt)（`GoogleCamera` PID `4283` 前台稳态、Host `framesSent=1504, framesDropped=0`、Guest `cdh_camera_video` 持续 15fps 接收、关键错误计数全为 `0`）
- 实机取景截图：[camera-live-pass.png](camera-live-pass.png)（原生 Google Camera 取景框清晰实时渲染 Host MP4 彩条+动态扫描线+时间码视频流，双摄变焦 `.7 / 1x / 2` 与隐私绿点、快门/模式栏完整可用）

## 4. 改动

1. **Camera HAL（`devices/hardware/android.hardware.camera/src/`）**：
   - [camera_provider_service.cpp](../../../../devices/hardware/android.hardware.camera/src/camera_provider_service.cpp)：修复 `result.partialResult = kPartialResultCount (10)` 与 `physicalCameraMetadata` 回填；按 `ro.product.device` 为 Pixel 双摄机型启用 Logical Multi-Camera 拓扑（`"0"` → 主摄 `"2"` `4064×3072` + 超广角 `"3"` `4032×3016` 及全套 binned RAW 档位）；补齐 55+ 标准 `CaptureResult` 字段及 `ANDROID_LENS_INFO_SHADING_MAP_SIZE (4×3)` / `ANDROID_STATISTICS_LENS_SHADING_MAP`；
   - [host_video_frame.h](../../../../devices/hardware/android.hardware.camera/src/host_video_frame.h)：`StreamMeta` 新增 `physicalCameraId`；`importBuffer()` 识别 `cros_gralloc_handle` `pixel_stride`；`renderFrameToBuffer()` 对 `RAW10`/`RAW_OPAQUE`/`RAW16` 增加 `AHardwareBuffer_lock` 单平面安全回退。
2. **GPU EGL/GLSL 兼容层（`devices/hardware/android.hardware.gpu/src/` & `halbuild/gpu.go`）**：
   - [cdh_gl_strings.c](../../../../devices/hardware/android.hardware.gpu/src/cdh_gl_strings.c)、[android.hardware.gpu.json](../../../../agent/modules/pipeline/stages/08_build_hardware/modules/android.hardware.gpu.json)、[gpu.go](../../../../agent/modules/pipeline/internal/halcore/halbuild/gpu.go)：在 `libEGL_meow.so` 覆写 `eglCreateContext`，对 VirGL `EGL_BAD_MATCH` 自动降级 GLES 3.0 重试；
   - [cdh_gl_shader_patch.h](../../../../devices/hardware/android.hardware.gpu/src/cdh_gl_shader_patch.h) & [cdh_gl_strings_test.c](../../../../devices/hardware/android.hardware.gpu/src/cdh_gl_strings_test.c)：补齐 GLSL ES 3.10/3.20 → 3.00 改写（`GL_EXT_separate_shader_objects`、`GL_EXT_YUV_target` polyfill、`layout(binding/yuv/location)`、修饰语序、`textureGather`、`[3][3]` 平铺），宿主单测 8/8 通过。
3. **Graphics Allocator（`devices/hardware/android.hardware.graphics.allocator/src/`）**：
   - [CdhAllocator.h](../../../../devices/hardware/android.hardware.graphics.allocator/src/CdhAllocator.h)：`allocate2`/`isSupported` 按阶梯剥离非必需直通位解决 YUV `0x10133` 分配失败；对 `RAW10(37)`/`RAW_OPAQUE(36)`/`RAW12(38)` 用 `RAW16(DRM_FORMAT_R16)` 分配后回写 `cros_gralloc_handle.droid_format` 为原格式，并同步 `pixel_stride` 与 `AllocationResult.stride` 为字节步长 `strides[0]`（如 `8128 >= 5080`，16 字节对齐）。
4. **Host → Guest Camera 视频流注入全链路（宿主 + API + Web）**：
   - 宿主推流引擎：[host.go](../../../../agent/modules/daemon/backend/camera/host.go)、[source.go](../../../../agent/modules/daemon/backend/camera/source.go)、[camera_test.go](../../../../agent/modules/daemon/backend/camera/camera_test.go)（支持 `test`/`file`/`url`(RTSP/RTMP/HTTP/UDP/SRT)/`device` 四类源与 macOS `avfoundation` `nv12`）；
   - Daemon 与 `cdha` API：[instance_config.go](../../../../agent/modules/daemon/run/instance_config.go)、[peripherals.go](../../../../agent/modules/daemon/run/peripherals.go)、[http.go](../../../../agent/modules/daemon/run/http.go)、[daemonctl.go](../../../../agent/internal/daemonctl/daemonctl.go)、[handlers_daemon.go](../../../../agent/internal/server/handlers_daemon.go)、[server.go](../../../../agent/internal/server/server.go)（暴露 `GET /api/daemon/camera/status`、`POST /api/daemon/camera/source`、`POST /api/daemon/camera/upload` 与 `InstanceConfigSnapshot.Camera`）；
   - Web 投屏面板：[api.ts](../../../../agent/web/src/core/api.ts)、[types.ts](../../../../agent/web/src/core/types.ts)、[config-panel.ts](../../../../agent/web/src/modules/daemon/config-panel.ts)（新增「Camera · Host 视频流注入」卡片，支持一键切换源、上传 MP4/视频文件并热推送、实时展示帧率与推流统计）。

## 5. 验证

1. **构建与回归测试**：
   - 宿主 C 单测 `cdh_gl_strings_test` 8/8 全通过；
   - Go 单元测试 `cd agent && go test -count=1 ./modules/daemon/backend/camera/... ./modules/daemon/run/... ./internal/daemonctl/... ./internal/server/... ./modules/pipeline/internal/halcore/...` 9 个包全部 `ok`；
   - `cdh-pipeline device build oriole --manifest 17-CP3A.261005.002.A1-20260905 --from 08_build_hardware --skip 10_build_vendor_dlkm --skip 12_pack_super` 成功发布 `patch.img`（`sha256 cfeedcfbeee2`）。
2. **Pixel 6 `#5` 端到端实机验证**：
   - `POST /api/daemon/camera/source` 推送测试 MP4（`640×480@15fps`）及 `POST /api/daemon/camera/upload` 上传热切换均返回 `200 OK`，`GET /api/daemon/instance/config` 返回 `camera.hostStatus.connected=true, framesSent=1504, framesDropped=0`；
   - 启动 `com.google.android.GoogleCamera/com.android.camera.CameraLauncher`，进程（PID `4283`）全程零崩溃、关键错误计数为 `0`；
   - VNC（`127.0.0.1:5912`）实屏截屏人工审图（[camera-live-pass.png](camera-live-pass.png)）确认 Host MP4 动态彩色视频流在 Google Camera 取景框内清晰实时渲染。

## 6. 遗留

- `unroll_tex3d_to_tex2d_es.comp`（Compute Shader，需 GLES 3.1+）与 `libedgetpu_util.so`（Pixel EdgeTPU 人像重打光）在纯 VirGL GLES 3.0 环境下被 Google Camera 自动降级跳过，不影响拍照/取景核心链路；
- 当前 RAW10 缓冲写入的是安全亮度占位（预览流走 YUV/RGBA 渲染 Host 真视频帧），若后续需要 HDR+ 拍照合成 RAW 静态图片，可将 Host I420 帧按 RGGB Bayer 10-bit 打包写入 RAW10 缓冲。
