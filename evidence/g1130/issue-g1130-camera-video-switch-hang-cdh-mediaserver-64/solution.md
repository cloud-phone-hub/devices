# #337 BLU G65L（`blu/G1130`）原生相机 `DreamCamera2` 切换 `Photo <-> Video` 卡死根治与 64 位 `cdh-mediaserver` H.264 录像全链打通

- **设备 / 机型**：BLU G65L（`blu/G1130`，版本 `14-UP1A.231005.007-20260511`，Unisoc UMS9230T，GRF `system=A14` + `vendor=A13`，配置 `G1130-android13`，`lineage-20.0` + `kernel 5.15`）
- **日期**：2026-11-26
- **状态**：已闭环（承接 `#336` [../issue-g1130-boot-radio-ddrsize-camera-and-prepare-align/solution.md](../issue-g1130-boot-radio-ddrsize-camera-and-prepare-align/solution.md)）

---

## 一、问题

在 `#336` 打通 `BLU G65L #1` 原生相机 `DreamCamera2`（`/system_ext/app/DreamCamera2/DreamCamera2.apk`）冷启动与拍照后，点击底部模式栏从 `Photo` 切换到 `Video`（或往返切换）时界面永久卡死在高斯模糊过渡遮罩页（见 [before-video-stuck.png](before-video-stuck.png)），无法进入录像取景、无法录制视频，也无法切回 `Photo`。

---

## 二、根因

排查定位出三层递进耦合根因：

1. **官方 `system.img` 仅含 32 位 `mediaserver` 被空桩屏蔽，导致 `media.player` 系统服务缺失死等**：
   - `G1130` 官方 `system.img` 虽为 64 位系统，但 `/system/bin/mediaserver` 与 `/system/lib/libmediaplayerservice.so` 仅为 32 位 ARM ELF，在纯 64 位云机内核（无 AArch32 EL0）上无法执行；此前 `02_prepare` 将 `/system/bin/mediaserver` 重定向至不存在的 `/mnt/vendorpatch/system/bin/mediaserver` 屏蔽退出。
   - 当 `DreamCamera2` 切换至 `Video` 执行 `VideoModule.prepareMediaRecorder() -> new MediaRecorder()` 时，相机工作线程在 `/system/lib64/libmedia.so` 的 `IMediaDeathNotifier::getMediaPlayerService()` 内执行 `ServiceManager::waitForService("media.player")` 的 `while (true)` 无限循环（`Waiting for service 'media.player' on '/dev/binder'...`），永久锁死模式切换队列。
2. **Android 13/14 HIDL Codec2 未设 `debug.stagefright.c2inputsurface=-1`，退回不存在的 32 位 `IOmx` 触发空指针崩溃**：
   - 展锐原厂依赖 32 位 OMX，未设置 `debug.stagefright.c2inputsurface`（默认为 `0`）。在 `Codec2Client::CreateInputSurface()` 中 `<= 0` 直接返回 `nullptr`，随后 `CCodec::CreateCompatibleInputSurface()` 尝试获取不存在的 `android.hardware.media.omx@1.0::IOmx/default` 亦返回 `nullptr`，触发 `CCodec::createInputSurface()` 对空 `sp<PersistentSurface>` 解引用 `SIGSEGV`。
   - 当设置 `debug.stagefright.c2inputsurface=-1` 时，`CCodec::CreateInputSurface()` 走进程内原生 `OmxGraphicBufferSource` + `TWGraphicBufferSource` 构造 `PersistentSurface`，完全不依赖外部 `IOmx` 或 vendor C2 Store 的 `InputSurface`。
3. **持久输入 Surface（`MR_SET_INPUT_SURFACE`）透传与编码器选型**：
   - `DreamCamera2` 在 `VideoModule` 初始化时先调用 `MediaCodec.createPersistentInputSurface()` 创建持久录制 Surface 并送入 `CameraCaptureSession`，再通过 `IMediaRecorder::setInputSurface(PersistentSurface)`（事务码 `4`）传给 `mediaserver`；若 `IMediaRecorder` 忽略该事务或按默认 MIME 选中仅支持线性缓冲的 `c2.v4l2.avc.encoder`，会导致 `AMediaCodec_start` 失败或 `c2@1.2-service-v4l2-64` 异常断开。
   - 同时 `cdh_redirect` 中 `file /system/bin/mediaserver -> /mnt/vendorpatch/vendor/bin/hw/cdh-mediaserver` 必须排在 `dir /vendor/bin -> /mnt/vendorpatch/vendor/bin` 之前，确保 Bionic `linker64` 读取 `/proc/self/exe` 逆向还原为 `/system/bin/mediaserver`，从而命中 `system` linker 命名空间加载系统 64 位 `libbinder.so`、`libgui.so`、`libstagefright.so` 与 `libmediandk.so`。
   - 附带构建缓存修复：`08_build_hardware` 的 `halbuild.ModuleSourceDigest` 原先只哈希当前 `HardwareDir/<module>` 子树，当 `android13`（`hardware-hidl`）模块 JSON 通过 `../../../hardware/...` 复用跨根源码时修改源码会误判 `[skip]`。

---

## 三、证据

- **修前失败现场**：见 [log-fail.txt](log-fail.txt)、[before-video-stuck.png](before-video-stuck.png)
  - `ServiceManagerCppClient: Waiting for service 'media.player' on '/dev/binder'...` 无限刷屏，`DreamCamera2` 卡死在高斯模糊过渡遮罩；
  - `debug.stagefright.c2inputsurface` 为空时 `hwservicemanager: Cannot find entry android.hardware.media.omx@1.0::IOmx/default`，`libsfplugin_ccodec.so (android::CCodec::createInputSurface()+148)` 空指针 `SIGSEGV`。
- **修后实机验收日志与实屏截图**：见 [log-pass.txt](log-pass.txt)、[after-video-preview.png](after-video-preview.png)、[after-video-recording.png](after-video-recording.png)、[after-switch-back-photo.png](after-switch-back-photo.png)。

---

## 四、改动

1. **新增 64 位 `cdh-mediaserver` 系统服务（`media.player`）与 SELinux 规则**：
   - `devices/hardware/android.hardware.media/src/mediaserver_64_service.cpp`：
     - 实现完整 64 位 `IMediaPlayerService`、`IMediaRecorder`（全部 37 个事务）、`IMediaPlayer`（全部 43 个事务，含 `MEDIA_PREPARED` / `MEDIA_PLAYBACK_COMPLETE` / `MEDIA_SEEK_COMPLETE` 异步通知回调）、`IMediaMetadataRetriever`（全部 10 个事务）及原生 `MediaCodecList::getLocalInstance()` 桥接；
     - `CDHMediaRecorder` 支持反序列化客户端 `MR_SET_INPUT_SURFACE` 传入的 `PersistentSurface`（`IGraphicBufferProducer` + `HalToken` 还原 `HGraphicBufferSource`，借由原生 `CCodec::CreateInputSurface()` 构造具备合法 CFI vtable 的 `PersistentSurface` 并绑定至 `MediaCodec::setInputSurface`），同时兼容常规 `AMediaCodec_createInputSurface` + `querySurfaceMediaSource` 模式；
     - 显式优先选用 `media.swcodec` 软件编码器（`c2.android.{avc,hevc,mpeg4,h263,vp8}.encoder`）配合 `AMediaMuxer` 实时封装输出标准 `.mp4` 文件。
   - `agent/modules/pipeline/stages/08_build_hardware/modules/{android.hardware.media.json,android13/android.hardware.media.json}`、`devices/hardware{,-hidl}/android.hardware.media/sepolicy/{vendor_file_contexts,vendor_sepolicy.cil}`、`agent/modules/pipeline/stages/09_build_vendor/pack.go`：编译产出 `/vendor/bin/hw/cdh-mediaserver`，标注 `u:object_r:mediaserver_exec:s0` 并补齐 `init` / `mediaserver` 对 `mnt_vendor_file` 与 `vendor_file` 的目录搜索权限。
2. **`02_prepare` 与 `G1130` 清单同步对齐（遵循 `AGENTS.md §8` 不改官方 `super.img`）**：
   - `agent/modules/pipeline/stages/02_prepare/{catalog.go,prepare_catalog_default.json,firmware_measured_extra.go,apply_profile_configs.go,apply_profile.go}`、`devices/manifest/prepare_catalog.json` & 同目录单测 `unisoc_profile_align_test.go`：
     - 将 32 位 `/system/bin/mediaserver` 重定向目标设为 `/mnt/vendorpatch/vendor/bin/hw/cdh-mediaserver`，自动升级存量空桩路径并排在 `dir /vendor/bin` 之前；
     - 为展锐及含 32 位 `mediaserver` 重定向的机型自动生成 `"debug.stagefright.c2inputsurface": "-1"`。
   - `devices/manifest/configs/blu/G1130/G1130-android13/{module,vendor}/config.json`、`devices/manifest/devices/blu/G1130/device-profile.json`：同步更新重定向规则与 `"debug.stagefright.c2inputsurface": "-1"`。
3. **`08_build_hardware` 跨根源码增量指纹修复**：
   - `agent/modules/pipeline/internal/halcore/halbuild/cache.go`：`ModuleSourceDigest` 自动追踪模块 JSON 中 `"../` 开头的跨根源码文件内容哈希，杜绝 `hardware-hidl` 复用 `hardware` 源码时漏编。
4. **增量构建与运行态验证发布**：重编刷新 `out/devices/dist/blu/G1130/14-UP1A.231005.007-20260511/patch.img`（`sha256: aea1ebe2018b5ee1b46efefaa371be142e274abd548ccb3e3f15f5d1f489a6aa`）并冷启动 `BLU G65L #1`。

---

## 五、验证

1. **Go 单元测试**：
   - `go test ./stages/02_prepare/... ./stages/08_build_hardware/... ./stages/09_build_vendor/... ./internal/halcore/halbuild/...` 全量 PASS。
2. **实机 `BLU G65L #1`（ADB `127.0.0.1:5911`）冷启动与相机全链压测**（详见 [log-pass.txt](log-pass.txt)）：
   - **服务与属性核验**：冷启动 38s 就绪，`getenforce=Enforcing`、`debug.stagefright.c2inputsurface=-1`、`ro.boot.ddrsize=4096M`、`gsm.version.baseband=4G_MODEM_22B_W26.02.5_P2,4G_MODEM_22B_W26.02.5_P2`、`gsm.sim.state=LOADED,ABSENT`；`mediaserver`（PID `538`）运行于 `u:r:mediaserver:s0` 域，`service list` 确认 `media.player: [android.media.IMediaPlayerService]` 在线；`Waiting for service 'media.player'` 计数为 **0**，`mediaserver` SELinux AVC 计数为 **0**，ANR 计数为 **0**。
   - **5 轮 `Photo <-> Video` 连续往返切换压测**：`DreamCamera2`（PID `29635`）、`mediaserver`（PID `538`）、`cameraserver`（PID `529`）连续完成 5 轮 `Photo -> Video -> Photo` 往返切换 + 第 6 次切入 `Video` 录制 + 切回 `Photo` 拍照，三个核心进程 **PID 全程恒定不变（`529 / 538 / 29635`）**，0 崩溃、0 卡死。
   - **真实 H.264 `.mp4` 录像与切回 `.jpg` 拍照落盘校验**：
     - 录制 4.71 秒生成 `/sdcard/DCIM/Camera/VID_20261126_080747.mp4`（**259 KiB / 264,935 B**），`ffprobe` 校验为合法 `ISO Media, MP4 v2`（`Video: h264 (Constrained Baseline) (avc1), yuvj420p, 720x480, 444 kb/s, 32.69 fps, rotation -90.00 degrees`）；
     - 随后从 `Video` 切回 `Photo` 点击快门生成 `/sdcard/DCIM/Camera/IMG_20261126_080754.jpg`（**16 KiB**）；
     - 人工审图 [after-video-preview.png](after-video-preview.png)、[after-video-recording.png](after-video-recording.png)、[after-switch-back-photo.png](after-switch-back-photo.png) 确认 `Video` 预览页、录像计时页（`00:02`）与切回 `Photo` 页取景画面、控件状态及左下角最新缩略图全部清晰正常。

---

## 六、遗留

- `cdh-mediaserver` 当前聚焦相机视频流 `Surface -> H.264/HEVC/MPEG4/VP8 -> MP4` 录制与通用 `IMediaPlayer` / `IMediaMetadataRetriever` 状态机闭合；若后续业务需要在 `Video` 成片内混流麦克风 AAC 音轨，可在 `CDHMediaRecorder` 中扩展 `AAudio` / `AudioRecord` + `c2.android.aac.encoder` 音轨写入。
