# #316 RedMagic `NX809J`（A16）原生相机 Vendor Tags、默认模板与外部纹理 FBO 兜底

## 1. 问题

- **机型与版本**：`redmagic/NX809J`（REDMAGIC 11 Pro，`NX809J_16-BQ2A.250705.001-BP2A.250605.031.A3-20260527`，Android 16 / `vendor_sdk=36`）。
- **现象**：拉起原生相机 `com.android.camera/.Camera` 点过 `ACCEPT` 隐私弹窗后，约 3 秒无 `logcat` 输出直接退回桌面（该机型屏蔽了普通 `logcat`，实际崩溃记录在 `dumpsys dropbox`）。

## 2. 根因

通过 `dumpsys dropbox --print system_app_crash` 依次定位到三层连续故障：

1. **缺失高通/Nubia/ZTE 专有 Vendor Tags**：
   - `cn.nubia.camera.devicecontroller.AESettingHelper.<init>(AESettingHelper.java:62)` 直接 `CameraCharacteristics.get("org.codeaurora.qcamera3.iso_exp_priority.iso_available_modes")` 抛 `IllegalArgumentException: Could not find tag for key`。
2. **默认请求模板缺失 `Feature1Mode` 触发未判空拆箱 NPE**：
   - `cn.nubia.camera.utils.DeviceUtil.updateParametersSupports(DeviceUtil.java:166)` 对 `builderCreateCaptureRequest.get("org.quic.camera2.tuning.feature.Feature1Mode")` 仅捕获 `IllegalArgumentException`，未判 `null` 即调用 `.byteValue()`，在标签注册但默认请求未赋值时触发 `NullPointerException`。
3. **离屏辅助通道挂载 `GL_TEXTURE_EXTERNAL_OES` 至 FBO 触发 `0x8cd7`**：
   - 相机成功建流推帧进入 `RenderThread.drawPreview()` 后，`cn.nubia.camera.hardwarebuffer.HardwareBufferRenderer.onDraw(189)` 调用 `GLCanvas.beginRenderTarget(GL_TEXTURE_EXTERNAL_OES /* 0x8D65 */, mOESTextureId)`（依赖 `GL_EXT_YUV_target`）；Mesa VirGL 不支持将外部纹理作为 FBO 颜色附件，原样调用返回 `GL_INVALID_ENUM` 致 `glCheckFramebufferStatus` 报 `GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT (0x8CD7)` 崩溃。

## 3. 证据

- 原始崩溃现场见同目录 [log-fail.txt](log-fail.txt)（`dumpsys dropbox` 捕获的 `AESettingHelper` `IllegalArgumentException`）。
- 闭环验收日志与实屏截图见同目录：
  - [log-pass.txt](log-pass.txt)（`Device 0` / `Device 1` 推帧、DCIM 落盘 `IMG_20261123_175602.jpg`、`dropbox` 零新崩溃）
  - [nx809j-rear.png](nx809j-rear.png)（后摄 23mm Photo 模式满屏超清蓝天白云实时预览）
  - [nx809j-front.png](nx809j-front.png)（切前摄 `Device 1` 零崩溃出流 125 帧，受红魔屏下前摄 UDC 美颜通道遮罩影响呈黑屏）

## 4. 改动

1. **新增 Nubia/RedMagic 零前缀碰撞 Vendor Tag 表与默认请求预置**：
   - 从 `NubiaCamera.apk` `ExternalKeys` 提取 199 项 Vendor Tags（26 个 section，`0x8400xxxx..` 段，零前缀碰撞）生成 [cdh_nubia_vendor_tags.inc](../../../../devices/hardware/android.hardware.camera/src/cdh_nubia_vendor_tags.inc)，导出 `kNubiaQcomTuningFeature1ModeTag = 0x84190001u`。
   - 修改 [camera_provider_service.cpp](../../../../devices/hardware/android.hardware.camera/src/camera_provider_service.cpp)，新增 `isNubiaDevice()`（匹配 `nubia`/`redmagic`/`zte`），在 `activeVendorTagDescs()` 追加表项，并在 `buildRequestMetadata()` 为 Nubia/RedMagic 预置 `kNubiaQcomTuningFeature1ModeTag = (uint8_t)0`。
   - 重编 `android16` `android.hardware.camera-service` 刷入 `NX809J` `patch.img`。
2. **新增 `libGLESv2_meow.so` 外部纹理 FBO 兜底**：
   - 修改 [android.hardware.gpu.json](../../../../agent/modules/pipeline/stages/08_build_hardware/modules/android.hardware.gpu.json)（`gles2` overrides 追加 `glFramebufferTexture2D`）与 [gpu.go](../../../../agent/modules/pipeline/internal/halcore/halbuild/gpu.go)（传递 `CDH_HAS_GLFRAMEBUFFERTEXTURE2D=1`）。
   - 修改 [cdh_gl_strings.c](../../../../devices/hardware/android.hardware.gpu/src/cdh_gl_strings.c)，在 `glFramebufferTexture2D` 拦截到 `textarget == GL_TEXTURE_EXTERNAL_OES (0x8D65)` 且底层产生 GL 错误或 FBO 非 `GL_FRAMEBUFFER_COMPLETE` 时，清空错误并改挂线程本地兜底 `GL_TEXTURE_2D (RGBA8 64x64)` 纹理；同步路由 `eglGetProcAddress`。
   - 通过 `go test ./modules/pipeline/internal/halcore/halbuild/...`，重编 `out/agent/dist/cdh-pipeline` 与全代 `android13..17` `android.hardware.gpu`，并将 `android16` `libEGL_meow.so` / `libGLESv2_meow.so` 刷入 `NX809J` `patch.img`（`u:object_r:same_process_hal_file:s0`，`e2fsck -fn` 通过）。

## 5. 验证

- **冷启实测**（`NX809J_16-BQ2A.250705.001-BP2A.250605.031.A3-20260527`，ADB `127.0.0.1:5911`，注入 MP4 视频源）：
  - 点过 `ACCEPT` 后零崩溃秒进预览，`Device 0` `Stream[1]` 产出 `312+` 帧，`dropbox` 零新增 crash，[nx809j-rear.png](nx809j-rear.png) 审图确认后摄 23mm 满屏超清蓝天白云；
  - 点击快门实拍落盘 `/sdcard/DCIM/Camera/IMG_20261123_175602.jpg`（48,211 B）；
  - 切换前摄零崩溃切至 `Device 1`（`Stream[1]` 产出 `125+` 帧）。

## 6. 遗留

- 前摄切至 `Device 1` 虽零崩溃正常推帧，但实屏 [nx809j-front.png](nx809j-front.png) 因红魔屏下前摄（UDC）专属美颜滤镜链呈黑屏；后续如需要可进一步排查 `ZTE_UNDER_SCREEN_MODE` 滤镜节点。
- 需将含 Nubia 适配的最新 `android.hardware.camera` 同步重编至 `android13/14/15/17`（当前仅重编了 `android16`）。
