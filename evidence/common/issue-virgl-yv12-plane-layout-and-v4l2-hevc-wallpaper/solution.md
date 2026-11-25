# 动态壁纸与非 32 对齐 YV12 视频纹理全绿、水平断层、右侧 16px 黑边及 V4L2 Codec2 漏选修复

## 1. 问题

在 `Xiaomi 17 Pro Max #1`（`xiaomi/popsicle`，`16-OS3.0.319.0.WPBCNXM`，HyperOS 3 / Android 16，分辨率 `1200×2608`）进入桌面与锁屏时，系统预置动态主题壁纸 `/system/media/wallpaper/wallpaper_group/00_CMFdynamic/wallpaper_BK.mp4`（原片为深宝石蓝玻璃光影 `1200×2608`、`HEVC Main 10` `yuv420p10le` 60fps 全关键帧视频，`primaryColors=#ff183878`）呈现三项严重视觉失真（见 `wallpaper-fail.png`）：

1. **全屏偏绿失色**：原片深蓝色背景（参考锁屏快照顶部均值 `RGB=[61.8, 101.6, 167.0]`）整体变成刺眼的亮青绿色（实测顶部均值 `RGB=[0.0, 153.1, 66.2]`）；
2. **屏幕正中水平硬断层**：在 `y = 1304`（恰好 `2608 / 2`）处出现横贯全屏的上下两截色阶跳变（非小组件区相邻行差 `|row[1304] - row[1303]| = 16.57`）；
3. **最右侧 16 像素暗绿竖条**：`x = 1184..1199`（共 16 列）整列呈现暗绿黑边（`RGB=[0.0, 85.4, 0.0]`，`x=1183 -> 1184` 边界突变达 `79.90`）。

## 2. 根因

问题由**编解码组件能力表缺失（Layer 2）**与 **`minigbm` + `virglrenderer` 的 `HAL_PIXEL_FORMAT_YV12` 平面契约及非 blob 资源布局未同步（Layer 1）**两层机制共同触发，且 Layer 1 同时影响软解 `ANativeWindow_lock` 路径与 `c2.v4l2.hevc.decoder` 硬解输出路径：

### 2.1 Layer 1：`minigbm` 与 `virglrenderer` 的 `YV12` 平面步幅/偏移失配及非 blob 资源未上报显式布局

1. **Guest `minigbm` 违反 `<system/graphics.h>` 的 `HAL_PIXEL_FORMAT_YV12` 步幅约定**：
   - 上游 `external/minigbm/virtgpu_virgl.c:virgl_get_emulated_metadata()` 在分配 `DRM_FORMAT_YVU420_ANDROID`（`HAL_PIXEL_FORMAT_YV12`）时，将宽度向上 32 字节对齐（`1200 -> 1216`），但把 `Cb`/`Cr` 两个半宽色度平面的 `strides[1..2]` 也错误设为全宽 `1216`（`offsets = {0, 3171328, 4756992}`）。
   - 按照 Android `<system/graphics.h>` 硬性规范，`YV12` 的色度行步幅必须为 `cStride = ALIGN(yStride / 2, 16) = 608`，三平面紧凑排列为 `Y(0, 3171328B)`、`V/Cr(3171328, 792832B)`、`U/Cb(3964160, 792832B)`（末尾停在 `4,756,991`）。通过 `ANativeWindow_lock` 仅拿到顶层 `stride=1216` 的生产者（如 `libfastplayer.so` `ANWDisplay::renderPicture`）严格按 `cStride=608` 写入。
2. **未修改的 Lineage Mesa 不会在非 blob 资源上发送 `VIRGL_CCMD_PIPE_RESOURCE_SET_TYPE`**：
   - 在无 host-gbm 的宿主（如 macOS CGL）上，`minigbm` 走 `virgl_3d_bo_create`（`DRM_IOCTL_VIRTGPU_RESOURCE_CREATE`，`blob_mem=0`）创建 `1216×5216` 的 `R8_UNORM` 打包纹理。原厂/Lineage 预编译 `libgallium_dri.so` 导入 `blob_mem=0` 的 `EGLImage` 时不会发送 `VIRGL_CCMD_PIPE_RESOURCE_SET_TYPE`，导致宿主 `virglrenderer` 的 `res->yuv_width` 恒为 `0`：
     - **右侧 16px 黑绿边**：宿主 `alloc_yuv_tex` 按打包宽度 `1216` 创建 `GL_R8` 平面纹理，而 Android `SurfaceTexture` 按真实 `GraphicBuffer` 宽度 `1200` 构造 `[0, 1]` 归一化 UV，导致屏幕 `x = 1200 * (1200/1216) = 1184.2` 右侧的 `16px` 全部采样到每行末尾 `1200..1215` 的对齐填充零字节（`Y=0, U=0, V=0` 经 BT.601 转换即为暗绿 `(0, 85, 0)`）；
     - **正中 `y=1304` 水平断层与全屏发绿**：`res->yuv_width == 0` 时 `put_yuv_tex` 按 `src_stride=1216` 读取半宽 `608` 的色度数据，把前 `792,832B`（`V` 平面）与后 `792,832B`（`U` 平面）拼成单个纹理的上下两半（在 `y = 2608 / 2 = 1304` 形成硬断层），而第二色度平面从偏移 `4,756,992` 处读到全 `0x00`（`Cb = -128`），在 YUV→RGB 矩阵下使深蓝全屏爆绿。
3. **宿主 `virglrenderer` `set_yuv_type()` 与 `put_yuv_tex()` 的校验限制**：
   - `15-vrend-emulated-yuv-and-system-memory-fallback.patch` 中的 `set_yuv_type()` 原先仅接受单一平面偏移顺序（`offsets[1] == luma && offsets[2] == luma + c_size`）及 `chroma == stride / 2`，不兼容 `(Y, U, V)` 逆序偏移或 16 字节对齐 `ALIGN(y_stride/2, 16)`，且 macOS CGL 跨共享上下文调整 `aux_plane_gl_id` 尺寸后缺少 `glFlush()` 同步。

### 2.2 Layer 2：`/vendor/etc/media_codecs_c2.xml` 与官方 `media_codecs*.xml` 缺失致 `c2.v4l2.hevc.decoder` 被跳过

1. `popsicle` 静态配置此前缺少 `/vendor/etc/media_codecs_c2.xml`，且 `vendor/config.json` 因已有非空 `files` 列表在 `02_prepare` 非覆盖对齐时未回填官方 `etc/media_codecs*.xml`。
2. 尽管 `android.hardware.media.c2@1.2-service-v4l2-64` 已在线注册 `c2.v4l2.hevc.decoder`（支持 `PROFILE_HEVC_MAIN_10`），AOSP `Codec2InfoBuilder` 因读不到 `/vendor/etc/media_codecs_c2.xml` 中 `c2.v4l2.hevc.decoder` 的 `<Limit name="size" .../>`，无法生成 `size-range`，导致 `MediaCodecList::codecHandlesFormat` 直接跳过 `c2.v4l2.hevc.decoder`，回退至 8-bit 软解 `c2.android.hevc.decoder` 触发 `Fatal error in decoder 0x43dd` 后再降级到 `FFmpegDecoder + ANWDisplay`。

## 3. 证据

- **修复前日志与像素实测**（`log-fail.txt`、`wallpaper-fail.png`）：
  - `FastPlayer` 因无 `c2.v4l2.hevc.decoder` 尺寸声明选中 `c2.android.hevc.decoder`，解码 10-bit HEVC 立即报错 `C2SoftHevcDec: Fatal error in decoder 0x43dd` 并降级至 `decodemode:1`（`FFmpegDecoder` + `ANWDisplay` `HAL_PIXEL_FORMAT_YV12`）；
  - 像素实测：`Top RGB = [0.0, 153.1, 66.2]`，非小组件区 `y=1303->1304` 断层跳变 `16.57`，右侧 `x=1183->1184` 跳变 `79.90`（`x=1184..1199` 均值 `[0.0, 85.4, 0.0]`）。
- **修复后日志与像素实测**（`log-pass.txt`、`wallpaper-pass.png`）：
  - `CCodec` 直接创建硬解组件：`Created component [c2.v4l2.hevc.decoder] for [c2.v4l2.hevc.decoder]`；
  - 宿主 VideoToolbox 硬解经 `V4L2Decoder` 输出 `1200×2608` 帧：`Copied USERPTR CAPTURE staging to YV12 graphic 1200x2608 format=NV12` → `VideoDepth: onWallpaperFirstFrameRendered`，`qemu.log` 零 VirGL 派发报错；
  - 像素实测：`Top RGB = [62.2, 93.6, 152.4]`（与官方锁屏参考图 `[61.8, 101.6, 167.0]` 完全一致），非小组件区 `y=1303->1304` 行差降至 **`0.65`**（与相邻行 `0.55`/`0.56` 一致），右侧 `x=1183->1184` 跳变降至 **`0.74`**（`x=1184..1199` 均值 `[129.9, 149.0, 200.8]`）。

## 4. 改动

1. **Guest `minigbm` 分配器链接期桥接**（保持上游 `external/minigbm` 与预编译 Mesa 零修改）：
   - `devices/hardware/android.hardware.graphics.allocator/src/cdh_os_option_wrap.cpp`：新增 `__wrap_drv_bo_create` 与 `__wrap_drv_bo_create_with_modifiers`（覆盖 Android 13~17），对 `DRM_FORMAT_YVU420_ANDROID` 按 `<system/graphics.h>` 修正 `bo->meta` 三平面 `strides={yStride, cStride, cStride}` 与 `offsets={0, ySize, ySize+cSize}`（保留 `total_size` 兼容整块映射传输），并在 `virtgpu_virgl` 后端通过 `DRM_IOCTL_VIRTGPU_RESOURCE_INFO` + `DRM_IOCTL_VIRTGPU_EXECBUFFER` 即时向宿主发送 `VIRGL_CCMD_PIPE_RESOURCE_SET_TYPE` 同步真实宽高与 `(Y, V, U)` 步幅偏移；
   - `agent/modules/pipeline/stages/08_build_hardware/modules/android.hardware.graphics.allocator.json` 与 `android13/android.hardware.graphics.allocator.json`：补齐 `-Wl,--wrap=drv_bo_create`、`-Wl,--wrap=drv_bo_create_with_modifiers` 及 Android 13 的 `cdh_os_option_wrap.cpp` 源单元。
2. **宿主 `virglrenderer` 纯补丁增量**（遵循 `../issue-virglrenderer-readonly-upstream-patch-mode/` 只读上游约束）：
   - 新增 `qemu/patch/virglrenderer/30-vrend-yv12-plane-order-and-stride.patch`：扩展 `set_yuv_type()` 同时支持 `(Y, V, U)` 与 `(Y, U, V)` 两种平面偏移顺序及 `ALIGN(y_stride/2, 16)` 色度步幅，清理上下文残留 GL 错误并在 macOS CGL 下追加 `glFlush()` 确保跨共享上下文即时可见；将 `put_yuv_tex()` 分流条件统一为 `if (res->yuv_width)` 走 `put_yuv_std()`。
3. **编解码配置流水线固证与 `popsicle` 清单闭环**：
   - `agent/modules/pipeline/stages/09_build_vendor/{assemble.go,media_codecs_c2.go,media_codecs_c2_test.go}`：新增 `align-media-codecs-c2` 收口步骤，检测到自编 `v4l2` Codec2 服务时自动生成或幂等合并 `vendor/etc/media_codecs_c2.xml` 的 `c2.v4l2.{avc,hevc}.decoder` 与 `c2.v4l2.avc.encoder` 声明；
   - `agent/modules/pipeline/stages/02_prepare/{apply_profile_configs.go,media_codecs_c2_align.go,media_codecs_c2_align_test.go}`：`buildVendorEtcFileTreeCandidates` 默认产出 `etc/media_codecs_c2.xml`（存量幂等合并），并在存量 `files` 未含任何 `etc/media_codecs*` 时自动回填跨版本稳定的官方 `media_codecs*.xml`；
   - `devices/manifest/configs/xiaomi/popsicle/popsicle-android16/vendor/{config.json,etc/media_codecs_c2.xml}`：落盘 `media_codecs_c2.xml` 并补齐 10 份官方 `media_codecs*.xml` 引用。

## 5. 验证

1. **单元测试与全代编译**：
   - `go test ./stages/02_prepare ./stages/08_build_hardware ./stages/09_build_vendor` 全部通过（含新增 `TestAlignVendorMediaCodecsC2_*` 与 `TestAlignVendorExtraFields_BackfillsMediaCodecsAndC2`）；
   - `pipeline build-hardware -android {13..17} -module android.hardware.graphics.allocator -refresh` 五代全部零报错编译产出；
   - `build-qemu-darwin` 应用 `01..30` 全部 30 个补丁零失败重编 `libvirglrenderer.a`、`virgl_render_server` 与 `qemu-system-aarch64`（保留 `com.apple.security.hypervisor` 签名），`qemu/external/virglrenderer` `git status` 干净无改动。
2. **实机冷启动与画面像素复核（`Xiaomi 17 Pro Max #1`）**：
   - 刷新 `patch.img`（`e2fsck -fy` 零错误、`security.selinux` 扩展属性核验通过）后重启实例，`sys.boot_completed=1`；
   - 桌面与锁屏动态壁纸经 `c2.v4l2.hevc.decoder` 60fps 硬解流畅渲染，深蓝色相完整还原，水平断层与右侧 16px 黑绿边彻底消失（对比见 `wallpaper-fail.png` vs `wallpaper-pass.png`）。

## 6. 遗留

- 无阻塞项。其他已生成历史 `patch.img` 的存量盘集若使用非 32 像素对齐分辨率播放 `YV12` 视频，在下次跑 `09_build_vendor -> 11_build_patch` 或增量刷新 `allocator` / `media_codecs_c2.xml` 时即可自动获得同等修复。
