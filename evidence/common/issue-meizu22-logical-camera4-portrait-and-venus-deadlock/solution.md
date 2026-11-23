# #310 Meizu `Meizu22` 原生相机逻辑多摄 `"4"` 人像闪退与 Mesa Venus `image_thread` 死锁导致主预览冻结

## 问题

Meizu `Meizu22`（Flyme A16，`vendor_sdk=35`）在补齐通用自研 Camera HAL 后，打开 `com.meizu.media.camera` 存在两处关键缺陷：
1. **切换人像模式（PORTRAIT）闪退**：拍照模式虽然可以建立 4 条流并拍出 JPEG 缩略图，但切换人像模式时 `CameraDeviceClient` 报 `Camera 4: Camera doesn't support physicalCameraId 0`，随后触发 `onConfigureFailed` 退出进程；
2. **主预览实时流冻结/无画面**：`Stream[0]`（`1440x1080`, `IMPLEMENTATION_DEFINED -> RGBA_8888`）持续产出数千帧，但屏幕取景框始终停在均匀灰屏或上次退出的静态模糊占位图，无法显示宿主实时视频流。

## 根因

1. **逻辑后摄 `"4"` 缺少物理子摄声明**：魅族原生相机以 Camera ID `"4"` 作为主逻辑后摄，在人像模式下按 `OutputConfiguration.setPhysicalCameraId("0")` 配置物理子摄流；原有 `camera_provider_service.cpp` 未给魅族 Camera `"4"` 导出 `ANDROID_REQUEST_AVAILABLE_CAPABILITIES_LOGICAL_MULTI_CAMERA` 与 `ANDROID_LOGICAL_MULTI_CAMERA_PHYSICAL_IDS = {"0", "2"}`。
2. **`PreviewHolder` `image_thread` 死锁于 Mesa Venus（`vulkan.virtio.so`）**：
   - 反编译魅族 `Camera.apk` 确认其预览管线为 `Camera HAL -> ImageReader(PRIVATE, 1440x1080) -> PreviewHolder.onPreviewImageListener (image_thread) -> VulkanUtil.b(hardwareBuffer, bArr2, ...) -> ImageWriter.queueInputImage() -> SurfaceTextureWrapper -> CameraPreviewRenderView (GL_TEXTURE_EXTERNAL_OES)`；
   - `VulkanUtil.b()`（`libvulkan_util.so`）仅用于将预览帧读回 CPU 字节数组给后台 AI 算法，与前台 GLES 预览渲染无关，却同步运行在 `image_thread` 且排在 `imageWriter.queueInputImage()` 之前；
   - `Meizu22` 默认 `ro.hardware.vulkan=mali` 经 `vulkan.mali.so` 转发到 Mesa Venus（`vulkan.virtio.so`）。在 VirGL 宿主环境下，`libvulkan_util.so` 通过 `VK_ANDROID_external_memory_android_hardware_buffer` 导入 `RGBA_8888` `AHardwareBuffer` 后在 `vulkan.virtio.so` 内进入 **97.7% CPU 无限自旋死锁**（`TID 7256 image_thread R 00:01:53 97.7%`），导致 `image_thread` 永不返回、`ImageWriter.queueInputImage()` 一次也未被调用、`SurfaceView(BLAST)` 永久卡在 `frame=2`。
3. **顺手加固 `host_video_frame.h` 单平面锁回退**：`renderFrameToBuffer()` 原先在 `AHardwareBuffer_lockPlanes()` 返回非 0 时一律落入 RAW 单色 `memset` 占位分支，已补齐对单平面 `RGBA_8888` 与 `BLOB` 的 `AHardwareBuffer_lock() + AHardwareBuffer_describe()` 完整渲染兜底，并加首帧诊断日志。

## 证据

- 失败日志：[log-fail.txt](log-fail.txt)（含人像模式 `doesn't support physicalCameraId 0` 与 `image_thread 97.7% CPU` 死锁、`SurfaceView frame=2` 冻结记录）
- 通过日志：[log-pass.txt](log-pass.txt)（含逻辑后摄 `"4"` `physicalIds = [0, 2]`、`vulkan.mali.so` 跳过 Venus 日志、`SurfaceView(BLAST)` 帧计数及拍照落盘记录）
- 实屏截图与实拍原图：
  - 后摄主预览实时流：[preview-rear.png](preview-rear.png)
  - 后摄人像模式（24/35/50/85/100mm）：[preview-portrait.png](preview-portrait.png)
  - 前摄主预览实时流（镜像）：[preview-front.png](preview-front.png)
  - 实拍原图（1.09 MB，含 `MEIZU 22` 水印）：[photo-captured.jpg](photo-captured.jpg)

## 改动

1. **`devices/hardware/android.hardware.camera/src/camera_provider_service.cpp`**：
   - 为 Meizu 运行时逻辑后摄 `"4"` 补齐 `LOGICAL_MULTI_CAMERA` 能力、`physicalIds = {"0", "2"}` 与 `CALIBRATED` 同步类型；
   - 在 `configureStreamsLocked()` 输出每条流配置摘要日志。
2. **`devices/hardware/android.hardware.camera/src/host_video_frame.h`**：
   - 引入 `AHardwareBuffer_describe`，在 `AHardwareBuffer_lockPlanes()` 失败回退单平面 `AHardwareBuffer_lock()` 时，对 `kFmtRgba8888` 与 `kFmtBlob` 按真实 `desc.stride` 完整渲染像素/JPEG，不再误落入单色填充。
3. **`devices/hardware/android.hardware.gpu/src/cdh_vulkan_mali.c`**：
   - 在 `cdh_open()` 中识别调用进程为 `com.meizu.media.camera` 且基座为 `vulkan.virtio.so` 时返回 `-ENOENT` 并打印提示日志，使 `libvulkan_util.so` 安全跳过 Venus 算法读回，消除 `image_thread` 100% CPU 自旋死锁，恢复前台 GLES 预览管线。
4. **`out/devices/tmp/patch_hal_refresh.py`**：
   - 增补 `vulkan` 增量刷新组（`/vendor/lib64/hw/vulkan.mali.so` + `same_process_hal_file` SELinux 标签回读校验）。

## 验证

在 `meizu/Meizu22/16-AQ3A.250411.001-20260127`（`android15 / vendor_sdk=35`）刷入新版 `android.hardware.camera-service` 与 `vulkan.mali.so` 后重启实测：
1. 喂入宿主视频流 `5b56c8b31b1ad.mp4` 并启动 `com.meizu.media.camera/.CameraActivity`，`cdh-gpu` 准确打印 `com.meizu.media.camera 跳过 vulkan.virtio.so 以避免 image_thread Venus 自旋死锁`；
2. 5 秒内 `SurfaceView(BLAST)` 从死锁时的 `frame=2` 飙升至 `frame=192`，间隔 2 秒双截屏像素差异 `489,687` 字节，肉眼审图确认后摄 4:3 蓝天白云满屏超清流动、色彩准确；
3. QMP 点击快门成功落盘 `/sdcard/DCIM/P20261123-123027.jpg`（`1,087,762` 字节），实拍原图清晰且带 `MEIZU 22` 水印；
4. QMP 切换 **人像模式（24/35/50/85/100mm + f 光圈）** 零崩溃、满屏高清出画；切换 **前摄（Camera ID 1）拍照模式** 零崩溃、满屏高清出画且正确水平镜像；全程无 `FATAL` 与 `onConfigureFailed`。

## 遗留

1. **录像模式（VIDEO）`CamcorderProfile` 缺失**：切换到录像模式时，魅族相机调用 `CamcorderProfile.get(4, quality)` 触发 `RuntimeException: Error retrieving camcorder profile params`（Lineage 基座 `/vendor/etc/media_profiles_V1_0.xml` 的 `cameraId="4"` 仅含 `qvga`/`timelapseqcif`，后续需按官方媒体配置补齐 `480p`/`720p`/`1080p`/`2160p` 档位）；
2. **前摄人像模式软美颜读回**：前摄人像模式走 `drawYuvFrame` 软美颜路径依赖 `VulkanUtil` 读回 YUV 字节数组，跳过 Venus 后呈纯绿占位（不崩溃，后摄人像及前后摄拍照均 100% 正常出画）。
