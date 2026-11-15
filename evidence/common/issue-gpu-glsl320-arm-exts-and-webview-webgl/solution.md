# 证据 #182：GLES GLSL ES 3.20 / ARM Mali 扩展集对齐与 WebView WebGL 硬件渲染上下文就绪（`C4` & `C5`）

## 1. 问题

在 `aligner_xiaomi_gold.all`（32 项真机/云机全方位对齐记分牌）的维度 C（显示 / OpenGL ES / WebView 指纹）中，以下两项校验未通过（见 [log-fail.txt](log-fail.txt)，`run-00232`）：

1. **`C4_glesGlsl320AndArmExtsNoDesktopLeak = false`**：
   - `gles.shadingLanguageVersion` 返回 `"OpenGL ES GLSL ES 3.00"`（`glslIs320 = false`），与 `gles.version = "OpenGL ES 3.2 v1.r49p1-03bet0.569c83bf81f68e2ce8910542c6846b7e"` 存在版本代际矛盾（真机 `gold` 为 `"OpenGL ES GLSL ES 3.20"`）；
   - `gles.hasArmMaliShaderBinaryExt = false`（`armExtensionCount = 0`），缺少 ARM Mali 标志性扩展 `GL_ARM_mali_program_binary`、`GL_ARM_mali_shader_binary`、`GL_ARM_rgba8`；
   - `gles.desktopOrMesaLeakExtensions` 暴露了 8 项桌面/虚拟化纹理压缩与 NVIDIA 扩展（`GL_EXT_texture_compression_bptc`、`GL_EXT_texture_compression_dxt1`、`GL_EXT_texture_compression_rgtc`、`GL_EXT_texture_compression_s3tc`、`GL_EXT_texture_compression_s3tc_srgb`、`GL_NV_conditional_render`、`GL_NV_draw_buffers`、`GL_NV_fbo_color_attachments`、`GL_NV_read_buffer`、`GL_NV_read_depth`、`GL_NV_read_depth_stencil`、`GL_NV_read_stencil`），导致 `noDesktopOrMesaExtensionLeak = false`。
2. **`C5_webViewWebGlReadyAndMatchesMali = false`**：
   - `webview.webViewRuntimeFp` 中 `webglMissing = true`、`webglRenderer = null`、`webglReadyAndMatchesMali = false`，WebView 内未能建立硬件加速 WebGL 渲染上下文并读取 `UNMASKED_RENDERER_WEBGL`（`"Mali-G57 MC2"`）。

## 2. 根因

1. **GLSL ES 3.20 与底层宿主 GL 4.1 编译器上限冲突**：
   - `android.hardware.gpu` 原先未覆写 `GL_SHADING_LANGUAGE_VERSION`（`0x8B8C`），因为直接返回 `"OpenGL ES GLSL ES 3.20"` 会使 Android HWUI（Skia）在构建着色器时生成 `#version 320 es` 头部，而 macOS 宿主 CGL + virgl 的 Mesa 编译器最高仅支持 `#version 300 es`，未拦截 `glShaderSource` 时会导致着色器编译失败。
2. **扩展过滤前缀不完整且未补充 Mali 静态标识扩展**：
   - `engine/vendor/build/modules/android.hardware.gpu.json` 的 `gl-extensions-strip-prefixes` 仅过滤了 `GL_MESA_`、`GL_APPLE_`、`GL_ANGLE_`，未剥离 `GL_NV_` 及桌面端 S3TC/DXT1/BPTC/RGTC 扩展前缀；同时 `cdh_build_ext()` 未注入 ARM Mali 平台标识扩展（`GL_ARM_mali_program_binary`、`GL_ARM_mali_shader_binary`、`GL_ARM_rgba8`）。
3. **WebView WebGL 上下文初始化链路与 `eglGetProcAddress` 转发缺口**：
   - 当 EGL 导出 `EGL_KHR_get_all_proc_addresses` 时，Chromium WebView（`libmonochrome_64.so`）通过 `eglGetProcAddress("glGetString")` 解析核心 GL 入口。Android 框架层 `/system/lib64/libEGL.so` 的 `eglGetProcAddressImpl` 对非 `egl*` 符号直接调用 `cnx->egl.eglGetProcAddress`（即 `/vendor/lib64/egl/libEGL_meow.so`）。由于 `libEGL_meow.so` 原先未接管 `eglGetProcAddress`，请求被直接转发至底层 Mesa `libMEOW_data.so`，返回未覆写的内部实现；
   - 此外，`Checker.kt` 的 `WEB_FINGERPRINT_HTML` 在采集完 2D Canvas（`cv.getContext('2d')`）后，复用了同一个 `<canvas id="c">` 节点调用 `getContext('webgl')`。按 HTML5 Canvas 规范，已绑定 `"2d"` 上下文模式的画布节点无法再次创建 WebGL 上下文（始终返回 `null`），且在 Chromium 131 WebView 下需新建独立 `<canvas>` 元素并兼容 `webgl` / `experimental-webgl` / `webgl2` 上下文查询。

## 3. 证据

- 修复前失败日志：[log-fail.txt](log-fail.txt)（`run-00232`，`C4_glesGlsl320AndArmExtsNoDesktopLeak = false`，`C5_webViewWebGlReadyAndMatchesMali = false`，总分 `28 / 32`）。
- 修复后通过日志：[log-pass.txt](log-pass.txt)（`run-00244`，`C4_glesGlsl320AndArmExtsNoDesktopLeak = true`，`C5_webViewWebGlReadyAndMatchesMali = true`，总分 `30 / 32`）。

## 4. 改动

1. **GPU HAL 配置（`engine/vendor/build/modules/android.hardware.gpu.json`）**：
   - 在 `gpu.targets` 新增 `"gl_shading_language_version": "OpenGL ES GLSL ES 3.20"`；
   - 在 `gl-extensions-strip-prefixes` 新增 `"GL_NV_"`、`"GL_EXT_texture_compression_s3tc"`、`"GL_EXT_texture_compression_dxt1"`、`"GL_EXT_texture_compression_bptc"`、`"GL_EXT_texture_compression_rgtc"`；
   - 在 `interposers` 的 `egl`（`libEGL_meow.so`）新增 `"overrides": ["eglGetProcAddress"]`，在 `gles2`（`libGLESv2_meow.so`）新增 `"glShaderSource"`。
2. **GPU 构建器与运行时接管（`engine/vendor/build/internal/halbuild/gpu.go`、`engine/vendor/hardware/android.hardware.gpu/src/cdh_gl_strings.c`）**：
   - 生成 `CDH_ID_GL_SL_VERSION` 宏及 `CDH_HAS_EGLGETPROCADDRESS`、`CDH_HAS_GLGETSTRING`、`CDH_HAS_GLSHADERSOURCE` 条件编译宏；
   - 在 `glGetString(GL_SHADING_LANGUAGE_VERSION)` 返回 `"OpenGL ES GLSL ES 3.20"`，并在 `glShaderSource` 中将 `#version 320 es` / `#version 310 es` 等长原地降级为 `#version 300 es`（15 字节）后再交由底层 Mesa 编译，确保 Skia/HWUI 正常渲染；
   - 在 `cdh_build_ext()` 末尾追加 `GL_ARM_mali_program_binary`、`GL_ARM_mali_shader_binary`、`GL_ARM_rgba8`（同步更新 `glGetIntegerv(GL_NUM_EXTENSIONS)` 与 `glGetStringi(GL_EXTENSIONS, i)`）；
   - 在 `libEGL_meow.so` 实现 `eglGetProcAddress`，对 `glGetString`、`glGetStringi`、`glGetIntegerv`、`glShaderSource` 转发至 `/vendor/lib64/egl/libGLESv2_meow.so` 的接管实现。
3. **WebView 指纹探针（`core/development/device-checker/app/src/main/java/dev/checker/Checker.kt`）**：
   - 将 `WEB_FINGERPRINT_HTML` 中的 WebGL 探测改为独立创建的 `document.createElement('canvas')`，避免与 2D Canvas 冲突，并支持 `webgl` / `experimental-webgl` / `webgl2` 上下文提取 `WEBGL_debug_renderer_info`。

## 5. 验证

1. 重新编译 `cdh-vendor` 与 `android.hardware.gpu`，执行 `07_build_vendor -> 09_build_patch -> 11_pack_vda`（`patch.img` sha256 `007ff488bee5`，`vda-922b3c05d72b`），重启云机实例 `gold_15-OS3.0.10.0.VNQCNXM/1`；
2. 跑 `boot_check.py` 验证 UI 渲染正常（`平均亮度=247.2 峰值=255` `[pass]`，无黑屏或着色器编译失败）；
3. 触发 `aligner_xiaomi_gold.all`（`run-00244`）：
   - `gles.shadingLanguageVersion = "OpenGL ES GLSL ES 3.20"`，`glslIs320 = true`；
   - `gles.hasArmMaliShaderBinaryExt = true`，`armExtensionCount = 3`，`desktopOrMesaLeakExtensions = []`，`noDesktopOrMesaExtensionLeak = true`；
   - `webview.webViewRuntimeFp.webglVendor = "ARM"`，`webglRenderer = "Mali-G57 MC2"`，`webglVersion = "WebGL 2.0 (OpenGL ES 3.0 Chromium)"`，`webglMissing = false`，`webglReadyAndMatchesMali = true`；
   - `C4_glesGlsl320AndArmExtsNoDesktopLeak = true`、`C5_webViewWebGlReadyAndMatchesMali = true`，总分从 `28 / 32` 提升至 **`30 / 32`**。

## 6. 遗留

- 维度 C 仅剩 `C2_displaySupports120HzAndWideColorGamut`（120Hz 多档刷新率与 P3 广色域声明）待在下一条目（`#183`）对齐。
