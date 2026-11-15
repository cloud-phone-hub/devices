# Issue: popsicle HyperOS 4 (16-OS4.0.5.0.XPBCN) 桌面 com.miui.home 启用 Impeller OpenGL ES (VirGL) 宿主 GPU 硬件加速消除 SwiftShader CPU 软光栅卡顿

- 设备：`xiaomi/popsicle`（Xiaomi 17 Pro Max / `canoe` / HyperOS 4 `16-OS4.0.5.0.XPBCN` / lineage-23.2 / kernel 6.12.81）
- 阶段：`internal/vendorimg`、`05_build_vendor_boot`、`07_build_vendor`
- 状态：**已闭环**（2026-11-07）
- 关联：同机 [#140](../issue-weather2-impeller-vulkan/solution.md) / [#166](../issue-hyperos4-pastel-vulkan-and-wdt/solution.md)

## 一、问题

同机型对比下，`popsicle_16-OS4.0.5.0.XPBCN`（HyperOS 4）比 `popsicle_16-OS3.0.319.0.WPBCNXM`（HyperOS 3）明显卡顿：

1. **HyperOS 3（`OS3.0.319.0.WPBCNXM`）**：桌面 `com.miui.home` 是标准 Android View + HWUI 应用，通过 `ro.hwui.use_vulkan=false` + `ro.hardware.egl=mesa` 走 **`virtio-gpu` Mesa VirGL → 宿主 Apple GPU 硬件加速**，桌面滑动仅占单核 ~30% CPU，帧率满 60 FPS。
2. **HyperOS 4（`OS4.0.5.0.XPBCN`）**：小米将整个桌面 `com.miui.home`（由 `/system_ext/bin/hyos_spawner` 孵化）重写为基于 `/system_ext/lib64/libhyper_os_flutter.so` 的自研 Impeller 场景引擎，並在引擎初始化处将渲染后端硬编码锁死为 `kImpellerVulkan (1)`。在 [#166](../issue-hyperos4-pastel-vulkan-and-wdt/solution.md) 中通过 `vulkan.mali.so` → `vulkan.pastel.so`（Google SwiftShader CPU Vulkan 1.3）解决了桌面黑屏与 SPIR-V `Float16` 崩溃，但 SwiftShader 是**纯 CPU 软光栅化**——每次滑动桌面（分辨率 `1200×2608`）及绘制 `MiGlassV13F16` 毛玻璃小部件时，`com.miui.home` 的 Reactor JIT 线程池瞬时吃满 **295% ~ 615% CPU**（8 核占满 3~6 核），导致桌面滑动与动效严重掉帧卡顿。

同时，按项目约束（`AGENTS.md` 第 8 条），**严禁直接修改或重打 `system.img`、`system_ext.img`、`product.img`、`super.img` 等官方只读镜像**，一切针对官方分区文件（如 `/system_ext/lib64/libhyper_os_flutter.so`）的修补必须落地在载体镜像（`/mnt/vendorpatch/vendor/...`）并通过 `cdh_redirect` 路径重定向生效。

## 二、根因

对 `/system_ext/lib64/libhyper_os_flutter.so`（19,080,696 B，SHA-256 `4fb795d576d03da1836f2a6c7ba833ab7b8878f84540f529fc8df6c31f001740`）进行完整反汇编与运行态追踪，定位到从 Vulkan 软光栅切换到 Impeller OpenGL ES（Mesa VirGL 硬件加速）一共存在 **6 道关卡**：

### 关卡 1：三处渲染后端选择入口硬编码锁死 `kImpellerVulkan (1)`

1. **`hyperos::GetHyperRenderingAPI`（`vaddr 0x6dcc74` / `file_off 0x6ccc74`）**：`com.miui.home`（`hyos_spawner`）走 `hyperos/hyper_engine.cc` 入口，默认忽略或覆盖 OpenGL ES 选项，只有返回 `0`（`kImpellerOpenGLES`）才创建 `AndroidSurfaceGLImpeller`。
2. **`flutter::AndroidShellHolder`（`vaddr 0x6d47fc` / `file_off 0x6c47fc`）**：独立 Flutter Activity 入口，在 `0x6d47fc` 处无条件执行 `mov w8, #1`（`kImpellerVulkan`）覆盖 `GetAndroidRenderingAPI` 的返回值。
3. **`flutter::AndroidContextDynamicImpeller`（`vaddr 0x6b0b8c` / `file_off 0x6a0b8c`）**：动态上下文初始化总是优先走 `InitVulkan`（`0x6b0ba8`），仅当 `InitVulkan` 失败才回退 `InitGL`（`0x6b0bf0`）；而在配置了 `vulkan.pastel.so` 的机器上 `InitVulkan` 会成功，导致永远落入 SwiftShader CPU Vulkan。

### 关卡 2：内置 6 个 GLSL ES 3.00 着色器声明了 Mesa VirGL 不支持的 `GL_EXT_shader_implicit_conversions` 与 `__fp16`

`libhyper_os_flutter.so` 内置完整的 Impeller OpenGL ES 后端（`ContextGLES`、`ReactorGLES`、`TextureGLES`、`PipelineGLES`）及 125 个预打包的 OpenGL ES 3.00 着色器源码（位于 `.rodata` 段），但其中 6 个毛玻璃/圆角着色器（`mi_glass_sdf.frag`、`rrect_shadow.frag`、`solid_rrect_blur.frag`、`mi_border_glow.frag`、`mi_glass_v13_f16.frag`、`mi_glass_v13_f16_legacy.frag`）在头部写了：
```glsl
#extension GL_EXT_shader_implicit_conversions : require
```
而 `mi_glass_v13_f16.frag` 与 `mi_glass_v13_f16_legacy.frag` 还在宏分支中使用了 `#define f16vec4 __fp16 vec4`（ARM Mali 专有语法）。在 `virtio-gpu` Mesa VirGL（OpenGL ES 3.2）上编译这 6 个着色器直接报：
- `'GL_EXT_shader_implicit_conversions' : extension is not supported`
- `'__fp16' : syntax error: syntax error`

导致 `PipelineLibraryGLES` 编译失败并返回空管线。且这 6 个源码字符串长度直接由代码段 `impeller::Mapping` 构造指令硬编码引用，替换时**字节长度必须分毫不差**。

### 关卡 3：`TextureGLES::OnSetContents` 对非 2 的幂（NPOT）纹理拒绝生成 Mipmap

在 `impeller/renderer/backend/gles/texture_gles.cc(485)`（`vaddr 0xd29c4c` / `file_off 0xd19c4c`）中，`TextureGLES::GenerateMipmap` 检查纹理宽高是否为 2 的幂（Power of Two）。由于 OpenGL ES 3.0+ 原生支持 NPOT Mipmap，而桌面毛玻璃及图标纹理尺寸并非 2 的幂，该过时检查直接报 `ImpellerValidationBreak: Cannot generate mipmaps for non-power-of-two textures.` 并中断后续纹理采样。

### 关卡 4：HyperOS 4 定制 `RenderTarget::SetupDepthStencilAttachments` 在 GLES 离屏 FBO 上因缺 Stencil Attachment 报错中断

HyperOS 4 在 `impeller/renderer/render_target.cc(357)`（`vaddr 0xd65460` / `file_off 0xd55460`）修改了 `SetupDepthStencilAttachments`：
- 当 `EntityPass` 要求离屏 `RenderTarget`（如桌面文件夹图标、小部件图层、搜索栏的 `SaveLayer`）同时携带 Depth 与 Stencil 附件时，`RenderTargetAllocator` 创建完 Color0 纹理后调用 `SetupDepthStencilAttachments`；
- 在 `0xd654b0` 处，`context.GetCapabilities()->GetDefaultDepthStencilFormat()` 返回 `kD24UnormS8Uint (13)`，但 `0xd65608` 处 `GetStencilAttachment()` 因尚未挂载 Stencil 附件返回空（`w8 == 0`），跳入 `0xd657a4` 立刻返回 `false`；
- 这使得 `RenderTargetCache::CreateOffscreen` / `CreateOffscreenMSAA` 返回空 `RenderTarget`，进而导致 `EntityPass::OnRender` 在任何含 `SaveLayer` 的子节点（文件夹、搜索栏、小部件、页面指示器）上直接报 `Could not complete contextSetup for context stage.` 并丢弃整棵子树；若简单跳过报错但保留 `SaveLayer` 离屏 FBO 切换，又会因 GLES 状态机切换导致整个桌面黑屏或泄漏数千个 `Impeller Texture`。

### 关卡 5：HyperOS 4 定制 `PipelineBuilder` 对所有 2D 着色器强设 `PixelFormat=13 (kD24UnormS8Uint)` 致 GLES 管线校验拒绝

在 `content_context.cc` 中，HyperOS 4 将大量 2D 管线（`PorterDuffBlendPipeline`、`GlyphAtlasPipeline`、`SolidFillPipeline`、`TexturePipeline`、`TiledTexturePipeline`、`RRectBlurPipeline` 等）的默认 `PipelineDescriptor` 硬编码设为 `SetStencilAttachmentDescriptors(kD24UnormS8Uint)` + `SetDepthStencilAttachmentDescriptor(kD24UnormS8Uint)`，而片元着色器 `frag_info` 的某个结构体字段同时被编译器向量化写入 `PixelFormat=13`。当 GLES 帧缓冲（Onscreen FBO 0）仅含 `kS8Uint (12)` 或无 Depth 附件时：
- `PipelineLibraryGLES`（`vaddr 0xd24350`）检查 `PipelineDescriptor::HasStencilAttachmentDescriptors()`，若与当前 FBO 不匹配则拒绝绑定；
- `EntityPass::OnRender` 在 `vaddr 0xc97b74` 检查 `RenderTarget` 的 Stencil 附件并在 `vaddr 0xc98b6c` 调用 `EntityPassClipStack::RecordEntity`（`vaddr 0xc9cd18`）录制硬件 Stencil Clip（`ClipContents::Render` `vaddr 0xcab058`），在无 Stencil 附件或 FBO 0 上导致文字（`GlyphAtlas`）和图标（`PorterDuff` / `TiledTexture`）被 Stencil Test 全部裁掉。

### 关卡 6：`MiGlass` 离屏捕获在 GLES 下会递归创建离屏纹理

`hyperos/mi_glass/mi_glass_Snapshot.cc` 中的 `IsLegacyGlassRender()`（`vaddr 0xd6c3e4` / `file_off 0xd5c3e4`）默认读取 `persist.sys.impeller.glass.legacy_render`（缺省为 `false`），开启实时离屏背景捕获与模糊；在 GLES 下开启实时离屏捕获会导致每帧创建离屏纹理。将其入口直接返回 `true (1)` 即可走轻量级 Legacy Glass 渲染路径，且无需依赖外部系统属性。

### 关卡 7：`com.miui.home` 多窗口共享 `onscreen_context_` 触发 `EGL_BAD_ACCESS (12290)` 致返回桌面假死

HyperOS 4 桌面 `com.miui.home` 在主桌面 Activity 之外还会创建负一屏（智能助理）overlay 窗口，两个窗口属于不同 `AndroidShellHolder`（各自拥有独立的 `ui_thread` / `raster_thread`），但通过 `EngineGroup` 共享同一个 `AndroidContextGLImpeller` 及其唯一的 `onscreen_context_`（`EGLContext`）。
当用户滑到负一屏或从其它 App（如 `Settings`）按 Home 返回桌面时：
- 负一屏的 `raster_thread` 在 `AndroidSurfaceGLImpeller::RecreateOnscreenSurfaceAndMakeOnscreenContextCurrent`（`vaddr 0x70d698` / `file_off 0x6fd698`）中调用 `onscreen_context_->MakeCurrent(*onscreen_surface_)`；
- 根据 EGL 规范，同一个 `EGLContext` 不能同时在两个线程处于 current 状态，导致 `eglMakeCurrent` 报 `EGL_BAD_ACCESS (12290)` 并返回 `false`；
- 调用方 `PlatformViewAndroid::NotifyCreated` 收到 `false` 后中断绑定，主桌面渲染线程被迫退回 `1x1` 的 `offscreen_surface_`（pbuffer）空转，导致返回桌面后画面彻底假死不刷新。
将该处 `bl MakeCurrent`（`0x94211e70`）修补为 `mov w0, #1`（`0x52800020`），只创建新的 `eglCreateWindowSurface` 而不跨线程抢占 `EGLContext`（真正的绑定由该窗口自己的 `raster_thread` 在每帧 `GLContextMakeCurrent` 时安全执行），彻底消除 `EGL_BAD_ACCESS`。

### 关卡 8：`PassBlurLightLayer::Paint` 与 `hyper_setWindowPassBlurAvailable` 每帧多重离屏模糊掉帧 + 权限文件被二次拷回

1. `flutter::PassBlurLightLayer::Paint`（`vaddr 0xb33000` / `file_off 0xb23000`）与 `flutter::hyper_setWindowPassBlurAvailable`（`vaddr 0x6f4514` / `file_off 0x6e4514`）在桌面滑动与文件夹展开时对背景做多重高斯模糊，每帧耗费约 20ms GPU/CPU 时间；将 `0x6f4514` 修补为 `ret`（`0xd65f03c0`）、将 `0xb33000` 修补为 `b ContainerLayer::Paint`（`0x17ffe030`），直接走父容器绘制，保留所有子节点内容同时省去离屏模糊开销。
2. `07_build_vendor/assemble.go` 原先在 `applyVendorFiles`（按清单删除 `android.hardware.biometrics.face.xml` 与 `android.hardware.fingerprint.xml`）**之后**才调用 `syncOfficialHandheldPermissions`，把官方 `vendor/etc/permissions/*.xml` 里的 `face.xml` 和 `fingerprint.xml` 又拷了回来，导致 `system_server` 在每次 Activity 切换时阻塞等待不存在的 `IVirtualHal/virtual` 生物识别服务。将 `syncOfficialHandheldPermissions` 移至 `applyVendorFiles` 之前即可。

## 三、证据

- `log-fail.txt`：未修补前 `com.miui.home` 运行在 `vulkan.pastel.so`（SwiftShader CPU 软光栅）下滑动桌面占用 **295% ~ 615% CPU** 的 `top` 采样，以及仅切换 `debug.flutter.renderer=opengles` 时的 GLSL 编译错误、`NPOT mipmap` 报错与 `Could not complete contextSetup for context stage.` 日志。
- `log-pass.txt`：修补并重建镜像全新冷启动后，`vendor_boot-log.txt` / `vendor-log.txt` 构建日志、`cdh_redirect` 11 条规则加载日志、`/proc/$(pidof com.miui.home)/maps` 映射（加载 `/mnt/vendorpatch/vendor/lib64/libhyper_os_flutter.sys.so` + `/mnt/vendorpatch/vendor/lib64/egl/libGLESv2_mesa.so`，零 `vulkan.pastel.so`）、零 `ImpellerValidationBreak`、以及静置（`792% idle`，`com.miui.home` `0.0%` CPU）与桌面滑动（`com.miui.home` `48.1%` CPU，下降约 85%~92%）的 `top` 实测数据。
- `screenshot-page1.png` / `screenshot-page2.png`：全新冷启动（`debug.flutter.renderer=[]`、`persist.sys.impeller.glass.legacy_render=[]`）后桌面第 1 页（时钟、天气预报卡片、系统工具文件夹、全部应用图标、搜索栏、底栏）与第 2 页（热门/推荐文件夹及全部应用图标）100% 正常渲染的 VNC 实测截图。

## 四、改动

严格遵循 `AGENTS.md` 第 8 条（**不触碰 `super.img` / `system_ext.img` 等官方镜像，通过 `patch.img` `/mnt/vendorpatch/vendor/...` + `cdh_redirect` 重定向生效**）：

1. **新增 HyperOS 4 `libhyper_os_flutter.so` 二进制修补器（`engine/build/internal/vendorimg/hyperos_flutter.go` + `hyperos_flutter_test.go`）**：
   - `ExtractAndPatchHyperOSFlutter(systemExtImg, dstPath)` 从官方只读 `system_ext.img` 读取 `lib64/libhyper_os_flutter.so`，先执行 **前置锚点精确校验**（核对全部 27 处 `.text` 指令原始机器码与 6 处 `.rodata` GLSL 原始字节，任一不匹配即跳过，绝不盲改未知版本），通过后应用：
     - **6 处 `.rodata` 等长 GLSL 修补**：将 `#extension GL_EXT_shader_implicit_conversions : require` 替换为等长注释 `//extension GL_EXT_shader_implicit_conversions : require`，并将 `#if defined(IMPELLER_TARGET_OPENGLES) && defined( Confirming GPU Support )` 替换为 `#if 0 //ned(IMPELLER_TARGET_OPENGLES) && defined( Confirming GPU Support )`（消除 `__fp16` 语法错误，走标准 `half` → `float` 分支）；
     - **27 处 `.text` 指令修补**：
       1. `0x6dcc74` (`GetHyperRenderingAPI`) 与 `0x6d47fc` (`AndroidShellHolder`) 强制返回 `kImpellerOpenGLES (0)`，`0x6b0ba4` (`AndroidContextDynamicImpeller`) 直接跳 `InitGL`（无需 `debug.flutter.renderer` 属性）；
       2. `0xd6c3e4` (`IsLegacyGlassRender`) 强制返回 `1`（无需 `persist.sys.impeller.glass.legacy_render` 属性）；
       3. `0x6f4514` (`hyper_setWindowPassBlurAvailable`) 改为 `ret`，`0xb33000` (`PassBlurLightLayer::Paint`) 改为 `b ContainerLayer::Paint`，跳过每帧 20ms 的离屏多重模糊；
       4. `0x70d698` (`AndroidSurfaceGLImpeller::RecreateOnscreenSurfaceAndMakeOnscreenContextCurrent`) 将跨线程抢占 `onscreen_context_` 的 `bl MakeCurrent` 改为 `mov w0, #1`，消除多窗口（主桌面 + 负一屏）切换及返回桌面时的 `EGL_BAD_ACCESS (12290)` 死锁；
       5. `0xd29c4c` (`TextureGLES::OnSetContents`) 将 `cbz w0` 改为 `nop`，放行 GLES 3.0+ NPOT 纹理 Mipmap 生成；
       6. `0xd65460` (`RenderTarget::SetupDepthStencilAttachments`) 直接返回 `true (1)`，避免缺 Stencil Attachment 时中断 `CreateOffscreen`；
       7. `0xc970b4` (`EntityPass::OnRender` 子 Pass 离屏分支) 将 `tbz w25, #0, 0xc97360` 改为无条件 `b 0xc97360`，使子 `EntityPass` 直接复合并渲染到父 `RenderTarget`（避免创建离屏 FBO 导致状态丢失或每帧泄漏离屏纹理）；
       8. `0xc97b7c` (`EntityPass::OnRender` 根 Pass Stencil 检查)、`0xc98b6c` (`RecordEntity` 调用)、`0xc9cd18` (`RecordEntity` 入口)、`0xcab058` (`ClipContents::Render` 入口) 与 `0xd24350` (`PipelineLibraryGLES` Stencil 门控) 禁用硬件 Stencil Clip；
       9. `0xc7bbec`、`0xc7e68c`、`0xc7eba0`、`0xc7f124`、`0xc7f280`、`0xc81650`、`0xc822bc`、`0xd1d6e0`、`0xd1d7bc`、`0xd1f5e4`、`0xd1f698`、`0xd1fc08` 将 `PipelineDescriptor` 的 Depth/Stencil `PixelFormat` 清零为 `kUnknown (0)`，确保所有 2D/Glyph/PorterDuff/Texture/RRectBlur 着色器在 GLES 下无条件绑定并绘制。
2. **在 `07_build_vendor` 落盘修补件、标注 SELinux 上下文并修正权限文件删除顺序（`engine/build/07_build_vendor/elfcompat.go` + `assemble.go` + `elfcompat_test.go`）**：
   - 新增 `patchOfficialHyperOSFlutter`，将修补后的文件写入 `vendor/lib64/libhyper_os_flutter.sys.so`（随 `09_build_patch` 打包进 `patch.img`，运行态位于 `/mnt/vendorpatch/vendor/lib64/libhyper_os_flutter.sys.so`），并在 `vendor_file_contexts` 追加 `/vendor/lib64/libhyper_os_flutter\.sys\.so u:object_r:system_lib_file:s0`；
   - 在 `assemble.go` 中将 `syncOfficialHandheldPermissions` 移至 `applyVendorFiles` 之前执行，确保清单 `vendor.files` 删除 `android.hardware.biometrics.face.xml` / `android.hardware.fingerprint.xml` 不会被官方权限同步重新盖回。
3. **在 `05_build_vendor_boot` 自动注入首条 `cdh_redirect` 文件重定向规则（`engine/build/05_build_vendor_boot/plan.go`）**：
   - 新增 `ensureHyperOSFlutterRedirect`：当 `vendorimg.NeedsHyperOSFlutterPatch(systemExtImg)` 为真时，自动在 `spec.Modules.Redirect` **首项前置注入**：
     `file /system_ext/lib64/libhyper_os_flutter.so -> /mnt/vendorpatch/vendor/lib64/libhyper_os_flutter.sys.so`
   - 无需手工修改 SQLite 数据库 `modules.redirect`，对所有含该版本 `libhyper_os_flutter.so` 的 HyperOS 4 固件自动生效。

## 五、验证

1. **单元测试**：
   - `cd engine/build && go test ./internal/vendorimg ./05_build_vendor_boot ./07_build_vendor` 全部 PASS（含对真实 19MB `libhyper_os_flutter.so` 的前置锚点校验、修补后字节对账与幂等拒绝测试）。
2. **全链路镜像构建（`03 -> 05 -> 07 -> 09 -> 11`）**：
   - `vendor_boot-log.txt` 打印：`==> [fix] 检测到 HyperOS 4 system_ext/lib64/libhyper_os_flutter.so 锁死 Vulkan 软渲染，前置重定向 /system_ext/lib64/libhyper_os_flutter.so -> /mnt/vendorpatch/vendor/lib64/libhyper_os_flutter.sys.so`；
   - `vendor-log.txt` 打印：`[fix] vendor/lib64/libhyper_os_flutter.sys.so: 修补官方 system_ext/lib64/libhyper_os_flutter.so 启用 Impeller OpenGL ES 硬件加速渲染`；
   - 官方 `super.img`（`system.img`、`system_ext.img`、`product.img`、`mi_ext.img`）零修改。
3. **全新冷启动实机验证（清空 `writable-base.qcow2` 与 `writable-data.qcow2`）**：
   - `dmesg | grep cdh_redirect` 显示：`cdh_redirect: 已加载：11 条规则（/lib/modules/cdh_redirect.rules），内核未打补丁`；
   - `getprop debug.flutter.renderer` 与 `getprop persist.sys.impeller.glass.legacy_render` 均为空（零外部属性依赖）；
   - `/proc/$(pidof com.miui.home)/maps` 映射为 `/mnt/vendorpatch/vendor/lib64/libhyper_os_flutter.sys.so` 与 `/mnt/vendorpatch/vendor/lib64/egl/libGLESv2_mesa.so`，进程内无 `vulkan.pastel.so`；
   - 桌面第 1 页、第 2 页所有小部件、文件夹（系统工具、热门、推荐）、应用图标、文字标签、搜索栏、底栏 100% 正常渲染（见 `screenshot-page1.png`、`screenshot-page2.png`），`ImpellerValidationBreak` 计数为 `0`；
   - 性能对比：静置时整机 `792% / 800% idle`（`com.miui.home` CPU `0.0%`）；连续左右滑动桌面时 `com.miui.home` CPU 仅 **48.1%**（未修补前跑 `vulkan.pastel.so` 时高达 **295% ~ 615%**），下降约 **85% ~ 92%**，滑动帧率恢复 60 FPS。

## 六、遗留与 2026-11-08 零二进制修补收口

- **2026-11-08 收口为不修补官方 `.so` 二进制方案**：
  - 鉴于 `PatchHyperOSFlutterBytes` 依赖特定 ROM 版本的指令偏移锚点，且 [#170](../../common/issue-qemu-virtio-input-and-vnc-ws-smoothness/solution.md) 已解决 QEMU `virtio-input-hid` 首帧去重吞噬与 VNC Tight 全彩壁纸编码瓶颈（全屏编码从 `113.9ms` 降至 `9.42ms`），现已彻底移除 `internal/elfpatch` 及 `05`/`07` 中的 `libhyper_os_flutter.sys.so` 修补与重定向；
  - 改为在 [popsicle-android16.json](file:///Volumes/work/cloud-phone-hub/devices/manifest/configs/xiaomi/popsicle/popsicle-android16.json) 的 `buildVendor.build-prop-sets` 中直接配置原厂 `libhyper_os_flutter.so` 内建读取的系统属性开关：
    - `"persist.sys.impeller.glass.legacy_render": "true"`（由原厂 `IsLegacyGlassRender` `0xd6c3e4` 原生读取，关闭每帧实时背景截图毛玻璃，切换为轻量 Legacy Glass 渲染）；
    - `"persist.sys.rtc.onscreen_backdrop": "false"`；
    - `"persist.sys.gradient_blur_ddk": "false"`；
  - 配合 [#166](../issue-hyperos4-pastel-vulkan-and-wdt/solution.md) 的 `cdh_vulkan_mali.c`（SPIR-V `Float16 -> Float32`），全程 0 字节修改官方 `/system_ext/lib64/libhyper_os_flutter.so`、0 条额外 `cdh_redirect` 规则。
- `/cust/app/customized/MIUIWeather/` 下的独立应用 `com.miui.weather2` 同样直接使用该零修补链路运行。
