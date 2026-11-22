# #303 oriole A17：原生 Google Camera 底部操作栏与设置页全黑消失（宿主 virglrenderer + macOS Apple OpenGL 4.1 separable shader 链接污染进程 VirGL 上下文）

## 1. 问题

- **机型 / 实例**：Google Pixel 6（`oriole_17-CP3A.261005.002.A1-20260905`，实例 `#5`，ADB `127.0.0.1:5911`，VNC `127.0.0.1:5912`）。
- **现象**：打开原生 Google Camera 后，取景框（`SurfaceView` 图层）内可清晰显示 Host 推流的视频画面，但底部全部操作控件（快门、相册缩略图、前后镜头翻转、`.7/1x/2` 变焦胶囊、`Portrait/Photo/Night Sight/Pano` 模式栏、拍照/录像切换、设置齿轮）及首次弹窗、进入 `CameraSettingsActivity` 设置页全部变为纯黑消失，仅有底部 SystemUI 白色手势条与右上角绿色相机隐私指示点。

## 2. 根因

两层叠加，核心根因位于**宿主机 `virglrenderer` 与 macOS Apple OpenGL 4.1 Core Profile 的 Separate Shader Objects（SSO）不兼容**：

1. **宿主 `virglrenderer` 在 macOS Apple OpenGL 4.1 下启用 host separable programs 触发链接失败并污染整进程共享 VirGL 上下文**：
   - 在 Mesa VirGL 中，同一 Android 进程内的所有 `EGLContext` 共享同一个内核 DRM VirGL 上下文（例如 `ctx 22 "RenderThread"`）。
   - Google Camera 内部 `separable_shader_program.cc`（`rectiface/forward_warp.vert`、`raw_preprocessor/fullscreen_rect_highp_es.vert`）使用 `glCreateShaderProgramv` / `GL_PROGRAM_SEPARABLE`，Mesa 将其转为带 `TGSI_PROPERTY_SEPARABLE_PROGRAM 1` 的 TGSI 发给宿主机 `virglrenderer`。
   - `virglrenderer`（[vrend_shader.c](file:///Volumes/work/cloud-phone-hub/qemu/external/virglrenderer/src/vrend/vrend_shader.c#L2111-L2120) / [vrend_renderer.c](file:///Volumes/work/cloud-phone-hub/qemu/external/virglrenderer/src/vrend/vrend_renderer.c#L4541-L4544)）此前仅对 `use_gles` 宿主禁用 separable shaders，对 macOS Desktop OpenGL 4.1 仍走 `GL_PROGRAM_SEPARABLE` 独立链接路径（`vrend_link_separable_shader`）。
   - 然而 `vrend_shader.c` 对 `is_last_vertex_stage` 的顶点着色器未输出 `out gl_PerVertex { vec4 gl_Position; };`，且通用 varying 使用 `layout(location = 31 - sid)`（超过 Apple OpenGL 4.1 的 16 个 varying 向量上限）。Apple OpenGL 4.1 严格按 GLSL 规范在链接阶段直接拒绝：
     ```
     Error linking program:
     ERROR: Built-in variable 'gl_Position' must be redeclared before use, with separate shader objects.
     vrend_link_separable_shader: context error reported 22 "RenderThread" Illegal shader 0
     ```
   - `vrend_report_context_error(..., VIRGL_ERROR_CTX_ILLEGAL_SHADER, 0)` 污染了整个 Google Camera 进程共享的 VirGL context `22 ("RenderThread")`，导致该进程内 Skia `RenderThread` 之后提交的所有 UI 帧（底部操作栏、弹窗、`CameraSettingsActivity`）渲染全部失效变黑，唯有走独立 `SurfaceView` 图层的相机预览画面仍可见。
2. **Guest 侧 `cdh_gl_shader_patch.h` 曾无条件向顶点着色器注入 `GL_EXT_separate_shader_objects`**：
   - 修复前 `cdh_gl_shader_patch.h` 在升级 `#version 300 es` 时会给普通 Skia 顶点着色器也注入 `GL_EXT_separate_shader_objects`，额外放大了 separable shader 报错触发面（由 2 次降为 1 次后首次弹窗恢复显示，但 Google Camera 自带 separable 顶点着色器仍会触发 1 次并黑屏）。

## 3. 证据

- 控制变量实验（见 [log-fail.txt](log-fail.txt)）：
  - 直接冷启动 `CameraSettingsActivity`（不经过 `CameraLauncher`）：文字与开关 100% 清晰可见；
  - 先启动 `CameraLauncher` 1 秒后再打开 `CameraSettingsActivity`：100% 纯黑（`nonblack=0`）；
  - 禁用 HWC Overlay（`service call SurfaceFlinger 1008 i32 1`）、重启 `camera-service`、`pm clear` 均无法消除黑屏；
  - 宿主机 `qemu.log` 在 `CameraLauncher` 启动瞬间精确捕获 `ERROR: Built-in variable 'gl_Position' must be redeclared before use, with separate shader objects.` 与 `vrend_link_separable_shader: context error reported 22 "RenderThread" Illegal shader 0`。

## 4. 改动

1. **宿主机 `virglrenderer`（根治）**：
   - [qemu/external/virglrenderer/src/vrend/vrend_shader.c](file:///Volumes/work/cloud-phone-hub/qemu/external/virglrenderer/src/vrend/vrend_shader.c#L2111-L2126) 与 [qemu/external/virglrenderer/src/vrend/vrend_renderer.c](file:///Volumes/work/cloud-phone-hub/qemu/external/virglrenderer/src/vrend/vrend_renderer.c#L4541-L4546)：在 `#ifndef __APPLE__` 守卫下与 `use_gles` 宿主一致，在 macOS 上禁用宿主 separable programs，改由 `virglrenderer` 走标准 VS+FS 联合链接路径（Guest 侧 OpenGL ES `GL_EXT_separate_shader_objects` API 仍由 Mesa Gallium 完整承接，零功能损失）。
   - 归档为幂等补丁 [qemu/patch/virglrenderer/29-macos-disable-separable-shaders.patch](file:///Volumes/work/cloud-phone-hub/qemu/patch/virglrenderer/29-macos-disable-separable-shaders.patch)，增量重编 `libvirglrenderer.a`、`virgl_render_server` 与 `qemu-system-aarch64` 并部署至 `out/qemu/dist/darwin/`（保留 `com.apple.security.hypervisor` entitlement）。
2. **Guest 侧 GPU 着色器兼容垫片（前置优化）**：
   - [devices/hardware/android.hardware.gpu/src/cdh_gl_shader_patch.h](file:///Volumes/work/cloud-phone-hub/devices/hardware/android.hardware.gpu/src/cdh_gl_shader_patch.h) 与 [cdh_gl_strings_test.c](file:///Volumes/work/cloud-phone-hub/devices/hardware/android.hardware.gpu/src/cdh_gl_strings_test.c)：改为按需注入 `GL_EXT_separate_shader_objects`，避免向普通 Skia 顶点着色器误注 SSO 扩展；`out gl_PerVertex` 补齐 `#ifdef GL_OES_shader_io_blocks` 守卫。

## 5. 验证

详见 [log-pass.txt](log-pass.txt) 与同目录实屏 VNC 截图：

- 补丁幂等检验：`qemu/patch/apply-virglrenderer-patches.sh` 输出 `应用0 跳过29(已存在) 失败0`；
- 宿主单测：`cdh_gl_strings_test` `11/11` 通过；
- 实例 `#5` 重启后全流程实机验收（VNC `127.0.0.1:5912` 实屏抓帧人工审图 + `qemu.log` 检索）：
  1. `qemu.log` 中 `Error linking` / `vrend_link_separable_shader` / `Illegal shader` / `gl_Position` 报错数量精确为 **`0`**；
  2. 主拍照界面（[camera-photo-pass.png](camera-photo-pass.png)）：取景框视频流、`.7/1x/2` 变焦胶囊、快门、相册缩略图、翻转镜头、`Long Exposure/Portrait/Photo/Night Sight/Pano` 模式栏、左下角设置齿轮、拍照/录像切换胶囊、右下角快捷调节按钮全部清晰渲染；
  3. 点击快门拍照：正常触发快速分享气泡并真实落盘 `/sdcard/DCIM/Camera/PXL_20261122_120324669.jpg`（`617084 B`）；
  4. 点击左下角设置按钮弹出 `Photo Settings` 抽屉（[camera-settings-sheet-pass.png](camera-settings-sheet-pass.png)）→ 再点 `More settings` 进入完整 `CameraSettingsActivity` 设置页（[camera-more-settings-pass.png](camera-more-settings-pass.png)）：全部文字与开关清晰可见，不再黑屏；
  5. 返回相机、切回桌面再重新打开相机、切换至录像模式（[camera-video-pass.png](camera-video-pass.png)）：`FHD` 标志、`Pan/Video/Slow Motion/Time Lapse` 与全套操作控件始终稳定显示，全过程 `ZERO_SHADER_ERRORS`。

## 6. 遗留

- 当前禁用 host separable programs 仅针对 `#ifdef __APPLE__`（Apple OpenGL 4.1 宿主），Linux Desktop OpenGL 宿主不受影响；若未来在 Linux 上遇到严格遵循 GLSL 4.10+ 规范要求 `out gl_PerVertex` 重声明的宿主驱动，可在 `vrend_shader.c` 的 `emit_ios_vs` 为 separable 顶点着色器补齐 `out gl_PerVertex` 声明。
