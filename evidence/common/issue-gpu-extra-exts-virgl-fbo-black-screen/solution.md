# #255 跨机型：`gl_extra_exts` 注入宿主机 VirGL 不支持的 MSAA / Framebuffer Fetch 等扩展致 FBO `0x8d56` 全黑屏

## 1. 问题

`google/oriole` Android 15（`15-BP1A.250505.005-20250326`）冷启动完成后，`sys.boot_completed=1`、`init.svc.bootanim=stopped`，且前台焦点窗口已是 `com.google.android.apps.nexuslauncher/.NexusLauncherActivity`，但宿主 VNC / 截帧全黑（`meanBrightness=0.0`，`peak=0`），实例内执行 `screencap` 也因超时被强杀。

该问题属于 `02_prepare` 与 `android.hardware.gpu` GL interposer 的**跨机型公共缺陷**：只要 AI 采风表（`device-profile.json`）把真机 Mali / Adreno 的渲染到纹理多重采样、Framebuffer Fetch 等 GLES 扩展直接写入 `ro.vendor.cdh.gpu.gl_extra_exts*`，就会在所有 Android 版本与机型上复现。

## 2. 根因

`02_prepare` 将真机扩展字符串写入 `vendor/config.json` 的 `ro.vendor.cdh.gpu.gl_extra_exts` 与 `gl_extra_exts2`，`android.hardware.gpu` 的 `libGLESv2_meow.so`（`cdh_gl_strings.c`）随后将其拼接进 `glGetString(GL_EXTENSIONS)` 返回给 Android `RenderEngine`（SkiaGL）及 App `RenderThread`。

当包含下列扩展时：
- `GL_EXT_multisampled_render_to_texture` / `GL_EXT_multisampled_render_to_texture2`
- `GL_IMG_multisampled_render_to_texture` / `GL_OVR_multiview_multisampled_render_to_texture`
- `GL_EXT_shader_framebuffer_fetch` / `GL_EXT_shader_framebuffer_fetch_non_coherent` / `GL_ARM_shader_framebuffer_fetch*` / `GL_QCOM_shader_framebuffer_fetch_noncoherent`
- `GL_EXT_shader_pixel_local_storage*`、`GL_KHR_blend_equation_advanced*`、`GL_EXT_buffer_storage` 等

Android SkiaGL 与 `RenderThread` 会走隐式 MSAA Render-to-Texture（color attachment `res-samples=0, surf-samples=4`，搭配 depth/stencil `res-samples=4, surf-samples=0`）等路径。宿主机 macOS Apple OpenGL 4.1 + `virglrenderer` (`vrend`) 不支持此类组合与语义，在宿主侧连续报 `Failed to complete framebuffer 0x8d56` 与 `Illegal command buffer 524295`，导致 SurfaceFlinger 与 UI 帧提交静默丢弃、屏幕全黑。

## 3. 证据

- 失败日志见同目录 [log-fail.txt](log-fail.txt)：
  - `verify_boot` 报 `bootCompleted=true`、`topActivity=...NexusLauncherActivity`，但 `vnc.meanBrightness=0.0`、`peak=0`；
  - `qemu.log` 中 `surfaceflinger`（`ctx=3`）与 `RenderThread`（`ctx=9/10/11`）反复输出：
    - `Failed to complete framebuffer 0x8d56 surfaceflinger`
    - `fbo-diag ctx=3 name=surfaceflinger fbo=18 status=0x8d56 colors=1 gles=0 gl=4.1`（color 附件 `surf-samples=4` 与 depth/stencil 附件 `res-samples=4 surf-samples=0` 样本不一致，触发 `GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE` / `0x8d56`）
    - `vrend_decode_ctx_submit_cmd: context error reported 3 "surfaceflinger" Illegal command buffer 524295`
- 核对历史同源条目：配置模版反补层（`02_prepare`）与运行期 `cdh_gl_strings.c` 过滤表须两端严格同步；且已入库的 `oriole-android15`、`CPH2797-android16` 等既有模版中已残存上述危险扩展，仅靠过滤新生成项无法自愈存量配置。

## 4. 改动

采取「**配置反补层静态清洗 + 既有配置自动自愈 + GL interposer 运行期兜底剥离**」双端拦截，均不动 Mesa `src` 原生扩展列表：

1. **运行期 GL interposer（`devices/hardware/android.hardware.gpu/src/cdh_gl_strings.c`）**：
   - 新增 `cdh_is_virgl_unsafe_extra_ext()` 与共享助手 `cdh_append_filtered_extra_exts()`，在拼接 `ro.vendor.cdh.gpu.gl_extra_exts` / `gl_extra_exts2` 时剔除上述 43 项 VirGL / Apple GL 4.1 不兼容扩展。
   - 在同目录 `devices/hardware/android.hardware.gpu/src/cdh_gl_strings_test.c` 新增 `test_virgl_unsafe_extra_exts_filtered()` 覆盖截断、前中后位置匹配与安全扩展保留逻辑。
2. **配置反补与自动自愈（`agent/modules/pipeline/stages/02_prepare/apply_profile.go`）**：
   - 扩充 `sanitizeVirglSafeGLExtraExts()` 至同源 43 项黑名单，并新增 `resolveVirglSafeGLExtraExts()`（过滤后若为空，按 GPU 家族回退安全扩展串）；
   - 在 `alignBuildVendorAndHardware()` 中增加对存量 `vendor/config.json` 中 `ro.vendor.cdh.gpu.gl_extra_exts*` 的幂等清洗，确保旧配置执行 `apply-profile` / `02_prepare` 时自动清除危险扩展；
   - 在同目录 `agent/modules/pipeline/stages/02_prepare/apply_profile_test.go` 新增 `TestSanitizeVirglSafeGLExtraExtsAndAutoHeal` 并重编 `out/agent/dist/cdh-pipeline`。

## 5. 验证

1. C 单测（`cdh_gl_strings_test`）：`[pass] cdh_gl_strings_test: 6/6 passed`（见 [log-pass.txt](log-pass.txt)）；
2. Go 单测（`02_prepare`）：`ok cloud-phone-hub/agent/modules/pipeline/stages/02_prepare 0.600s`；
3. 执行 `cdh-pipeline device apply-profile` 验证幂等自愈：自动剥离 `oriole-android15` 与 `CPH2797-android16` `vendor/config.json` 中的危险扩展；
4. 在 `google/oriole`（`15-BP1A.250505.005-20250326`）重打 `build_hardware -> pack_vda` 并冷启验收：
   - 截图与状态：`bootCompleted=true`、`postBootCompleted="1"`、`surfaceFlingerState="running"`、`topActivity=...NexusLauncherActivity`、`vnc.meanBrightness=91.4`、`peak=255`（见 [pass-screen.png](pass-screen.png)，`read_file` 肉眼确认为完整 Pixel 桌面）；
   - 实例内 `/vendor/lib64/egl/gpu_id.cfg` 与 `qemu.log` 核查：`0x8d56` 出现 `0` 次、`Illegal command buffer` 出现 `0` 次（见 [log-pass.txt](log-pass.txt)）。

## 6. 遗留

- 后续其余存量机型模版及新采集机型在执行 `02_prepare` / `apply-profile` 与 `08_build_hardware`（`android.hardware.gpu`）时均会自动携带双端拦截，无需逐一手工修改；
- 若日后宿主机切换到支持真 GLES3.2 framebuffer fetch / MSAA RTT 的 Vulkan/VirGL 后端，可根据宿主特性开关重新放行对应子集扩展。
