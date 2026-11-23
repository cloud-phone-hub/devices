# #308 `libEGL_meow.so` 广色域/高色深 `eglChooseConfig` 与 `eglCreateWindowSurface` 回退——根治 Xiaomi `popsicle`（HyperOS 3）原生相机 `EGL_BAD_CONFIG (12293)` 闪退

## 问题

Xiaomi `popsicle`（HyperOS 3 / Android 16）冷启打开原生相机 `com.android.camera` 时，渲染线程 `PreviewRenderEngine` 立即崩溃退出，日志报 `com.xiaomi.gl.MIGLUtil.checkEglSurface: Failed to create window surface. error:12293 (EGL_BAD_CONFIG)`。

## 根因

抓取 `PreviewRenderEngine` 启动前两行关键日志：
- `I MCAM_MIGLUtil: Config chosen: R0 G0 B0 A0 D0 S0 RT0`
- `E MCAM_CRE_MIEGL: Failed to create EGLContext: 12293`

HyperOS 3 原生相机向 `eglChooseConfig()` 传入 10-bit / FP16 / 广色域 / `EGL_RECORDABLE_ANDROID` / 多重采样等高阶属性组合，而云机基座 Mesa VirGL 驱动无精确匹配的 `EGLConfig`，导致 `eglChooseConfig()` 返回 `num_config = 0`、`configs[0] = NULL`；随后 `MIGLUtil` 将空 config 传给 `eglCreateContext` 与 `eglCreateWindowSurface`，两者均触发 `EGL_BAD_CONFIG (12293)` 并导致进程自杀。此外，广色域/HDR 元数据属性传入 `eglCreateWindowSurface` 时在 VirGL 下也可能返回 `EGL_NO_SURFACE`。

## 证据

- 故障崩溃日志：[log-fail.txt](log-fail.txt)
- 修复后冷启验收日志：[log-pass.txt](log-pass.txt)
- 实屏截图：
  - 后置满屏高清实时预览（`LEICA VIBR`）：[popsicle-preview-pass.png](popsicle-preview-pass.png)
  - 8 次连续模式（专业/录像/拍照/人像）+ 前后摄切换 + 快门拍照后出缩略图实屏：[popsicle-after-clicks-pass.png](popsicle-after-clicks-pass.png)

## 改动

1. **扩展 GPU Interposer 代码生成器**：修改 [gpu.go](../../../agent/modules/pipeline/internal/halcore/halbuild/gpu.go#L118-L128) 支持 `CDH_HAS_EGLCHOOSECONFIG=1` 与 `CDH_HAS_EGLCREATEWINDOWSURFACE=1` 编译宏，并重编 `out/devices/bin/cdh-vendor`；
2. **更新 GPU 模块覆写清单**：修改 [android.hardware.gpu.json](../../../agent/modules/pipeline/stages/08_build_hardware/modules/android.hardware.gpu.json#L56-L58)，将 `libEGL_meow.so` 的 `overrides` 扩展为 `["eglGetProcAddress", "eglCreateContext", "eglChooseConfig", "eglCreateWindowSurface"]`；
3. **实现两级 `eglChooseConfig` 回退与 `eglCreateWindowSurface` 清洗回退**：修改 [cdh_gl_strings.c](../../../devices/hardware/android.hardware.gpu/src/cdh_gl_strings.c#L100-L250)：
   - `eglChooseConfig`：原调用返回失败或 `*num_config == 0` 时，第 1 级将 `R/G/B/A > 8` 收窄至 8-bit 并剔除 `EGL_COLOR_COMPONENT_TYPE_EXT`/`EGL_RECORDABLE_ANDROID`/`EGL_FRAMEBUFFER_TARGET_ANDROID`/`EGL_SAMPLES`/`EGL_SAMPLE_BUFFERS`/`EGL_CONFORMANT`/`EGL_CONFIG_CAVEAT`/`EGL_NATIVE_VISUAL_ID`，将 `RENDERABLE_TYPE` 规范为 `EGL_OPENGL_ES2_BIT`、`SURFACE_TYPE` 保留 `WINDOW|PBUFFER` 位重试；若仍无匹配，第 2 级兜底为标准 RGBA8888 ES2 窗口配置；
   - `eglCreateWindowSurface`：原调用失败且 `attrib_list != NULL` 时剔除 `EGL_GL_COLORSPACE_KHR`、SMPTE2086/CTA861.3 HDR 元数据、`EGL_PROTECTED_CONTENT_EXT`、`EGL_RENDER_BUFFER` 后重试，必要时回退 `NULL` 属性表；
   - 在 `eglGetProcAddress` 中一并导出这两个覆写入口；
4. **全代重编与同步**：重编 `android13`~`android17` 全部 5 代 `android.hardware.gpu`（`libEGL_meow.so` 等），并通过 `patch_hal_refresh.py --only egl` 安全刷入 `xiaomi/popsicle`（A16 & A17 盘集，均过 `ea_get -V` 与 `e2fsck -fn`）。

## 验证

- `xiaomi/popsicle`（HyperOS 3 / A16）冷启实测：`com.android.camera` 零崩溃稳定进实时预览，3 条输出流 `ACTIVE`，`MCAM_PreviewWatchDogCallback: currentFrameNO 4236; result=true`，实屏满屏高清出画；
- 连续执行 8 次交互点击（快门拍照、人像、录像、专业、拍照、切前置、切后置、再拍照）：`com.android.camera`（PID 13716）与 `android.hardware.camera-service`（PID 608）全程零崩溃零重启，相册缩略图成功落盘出图，`12293`/`EGL_BAD_CONFIG` 归零。

## 遗留

无；全代（`android13`~`android17`）新版 `libEGL_meow.so` 已落位 `out/devices/vendor/android<N>`，后续任意盘集增量刷新或全量构建均自动继承此兼容回退。
