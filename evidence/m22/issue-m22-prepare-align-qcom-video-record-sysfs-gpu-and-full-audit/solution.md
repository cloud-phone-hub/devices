# #349 `MEIZU 22` 真机规格重对齐、高通相机录像音频 C2 破障、`mediaserver` `sysfs_gpu` 崩溃根治与全子系统验收

- **编号**：#349
- **设备 / 分组**：`m22`（`meizu/Meizu22` · `16-AQ3A.250411.001-20260127` · Flyme 12.6 / QTI `SM8735` `sun` · GRF `system=16` + `vendor=15` · `lineage-22.2` + `6.6` 内核；内含跨机型通用修复：`composer3` OEM RenderIntent 兼容、`cdh_redirect` `zygote64` 无锁快表、`02_prepare` 高通 `vendor.audio.c2.preferred=true` 与假音频组件 `rank` 自愈、`android.hardware.audio` `vendor.audio.` 公共属性映射、`android.hardware.gpu` `mediaserver` `sysfs_gpu` 授权）
- **日期**：2026-11-27
- **状态**：**已闭环**（`MEIZU 22` 冷启动 `65.0s` `Enforcing` 就绪，`restarting=0`，8 核静置空闲率 `772%~796%/800%`；分辨率 `1200x2670`、12GB 内存、6GiB ZRAM、`5510mAh` 虚拟电池、20 项真机传感器全对齐；蓝牙、Wi-Fi、NFC APDU Tap、蜂窝双 PDN 纯移动数据外网、魅族原生相机前后摄拍照 `.jpg` + 1080p 录像 `.mp4` + 图库缩略图生成 + `c2.v4l2.avc.decoder` 硬解播放全绿通过）

---

## 1. 问题

在对镜像库 `#17 MEIZU 22`（`Meizu22_16-AQ3A.250411.001-20260127`）执行重对齐与全子系统体检时，暴露出硬件信息失真、外设/多媒体不可用及静置/交互异常共 6 类问题（修前现场见 [log-fail.txt](log-fail.txt)、[about-before.png](about-before.png)、[cam-before.png](cam-before.png)）：

1. **硬件规格与传感器未对齐真机**：屏幕错配为 `1080x2400@440dpi`（真机 `1200x2670` 6.3″）；`12GB` SKU 实例「我的手机」错报 `8GB`（`MemTotal: 7760724 kB < MemAvailable: 8838824 kB`）；`SwapFree: 0 kB` 无真实 ZRAM；`/sys/class/power_supply` 为空；传感器仍为早期沿用 `gold` 的 29 项 `MTK` 假传感器。
2. **蜂窝纯移动数据断网与 `.qtidataservices` 循环崩溃**：物理网卡被错命为 `ccmni2`（`DOWN`），`ccmni1` 落到 dummy 导致关闭 Wi-Fi 后 `ping 223.5.5.5` 100% 丢包；`com.qualcomm.qti.qtidataservices` 因 IWlan JNI 缺失每数秒 `SIGABRT` 重启。
3. **静置 CPU 偏高与多路系统日志刷屏**：无应用用机时 `android.hardware.camera-service` 常驻消耗 `7.6%` CPU + `khvcd` `3.8%` CPU；SurfaceFlinger 每帧刷 `I CompositionEngine: map unknown (DCI-P3 sRGB Full range)/(Unknown RenderIntent 265) to default color mode`（单次抓包 `2213+` 行）；`product/build.prop` 开启 `persist.vendor.meizu.smart_zram=true` 与 `debug.sf.show_refresh_rate_overlay_te_fps=1`，导致每次窗口动画触发 `E SmartSwapConfiger: java.lang.RuntimeException: Could not apply smart swap config`（`342` 次）与 TE 帧率刷屏。
4. **魅族原生相机冷启动黑屏（`qdgralloc` 穿透）**：打开相机偶发穿透加载官方原厂 `gralloc.qcom.so`，疯狂打印 `E qdgralloc: Invalid gralloc handle ... ints(34/26)` 取景全黑。
5. **相机切「录像」模式崩溃闪退**：修复预览与前后摄拍照后，切换录像模式报 `E ACodec: Unexpected nullptr for codec information` → `E StagefrightRecorder: Failed to create audio encoder` → `MediaRecorder: prepare failed: -2147483648`，重试 5 次后触发 `killProcess: MediaRecorder.init more than 5 times!!!` 自杀退出。
6. **图库提取视频缩略图导致 `mediaserver` `SIGABRT` 循环崩溃**：录制 `.mp4` 成功后打开图库或调用 `MediaProvider`，`/system/bin/mediaserver`（`capture_looper` 线程）报 `W EGL-MAIN: Failed to open any DRM device` → `F RenderEngine: no suitable EGLConfig found, giving up` 连续崩溃重启，视频缩略图呈灰色空白且导致 `MediaProvider` `DeadObjectException`。

---

## 2. 根因

1. **旧盘集滞后与 `hasReadme: true` 保护旧模板**：旧 `dist` 盘集构筑于双 PDN、6GiB ZRAM、虚拟电池 `sysfs_power` 与相机 `CDCM_PAUSE` 落地之前；且 `Meizu22.json` 带 `hasReadme: true`，`apply-profile -overwrite` 默认保留存量 `vendor/etc/{sensors_config,battery_config,media_config}.xml`（含 `gold` 传感器与 `rank="8"` 假音频组件），`versions/.../vendor/config.json` 亦残留 `"ro.hardware.vulkan": "virtio"`。
2. **OEM 自定义色彩意图 `0x100..0x110` 未在 `composer3` 声明**：Flyme 设置 `persist.sys.sf.native_mode=265`（`0x109`），`CdhComposer::getRenderIntents()` 此前仅返回标准枚举 `0..3`，导致 `CompositionEngine` 每帧回退并刷屏。
3. **高并发应用冷启动 `mmap_lock` 竞争与 `libaimapper.so` 白名单缺口**：64 位 App 冷启动主线程密集 `dlopen`/`mmap` 持有 `mm->mmap_lock` 写锁超过原 `82` 次重试窗口时，`cdh_caller_in_guest_Test` trylock 超时放行原厂 SP-HAL；而所有 `zygote64` 派生的 64 位应用天然共享完全一致的 `linker64` VMA 区间，可通过原子快表零锁判定。
4. **高通 `ExtendedSFRecorder` 跨分区读取 `vendor.audio.c2.preferred` 被 SELinux 拒读**：`/system_ext/lib64/libavenhancements.so` 依赖 `vendor.audio.c2.preferred=true` 跳过已废弃的 `OMX.qcom.audio.encoder.aac`（日志串 `"CCodec preferred, skip creation of custom OMX audio encoders"`）；Lineage `virtio` 基底未映射 `vendor.audio.` 前缀，使其落入 `default_prop` 被 `mediaserver` 拒读，且旧 `media_config.xml` 将 `c2.qti.*.audio.*` 空桩设为 `rank="8"` 与 `c2.android.aac.encoder` 冲突。
5. **Lineage `virtio` 基底遗漏 `mediaserver` 的 `sysfs_gpu` 读权限**：基底策略仅为 `appdomain`、`surfaceflinger`、`system_server`、`bootanim` 授予了 `/sys/devices/pci0000:00/0000:00:03.0`（`sysfs_gpu`）访问权；`mediaserver` 执行 `FrameCaptureProcessor` 缩略图抓帧调用 `drmGetDevices2()` 时因 `avc: denied { search } tcontext=u:object_r:sysfs_gpu:s0` 找不到任何 DRM 设备而 `SIGABRT`。

---

## 3. 证据

- **修复前失败证据**：见 [log-fail.txt](log-fail.txt)、[about-before.png](about-before.png)（8GB / `2400×1080`）、[cam-before.png](cam-before.png)（相机黑屏）。
- **修复后通过证据**：见 [log-pass.txt](log-pass.txt) 及实屏截图：
  - [about_clean_1.png](about_clean_1.png)：「我的手机」实屏显示 `MEIZU 22`、`32.34GB / 256GB`、`第四代骁龙®8s 移动平台`、运行内存 `12GB`、屏幕尺寸 `6.3 英寸`、分辨率 `2670×1200`；
  - [camera-rear-live.png](camera-rear-live.png) & [camera-front-live.png](camera-front-live.png)：原生相机前后摄实时彩条预览与拍照落盘（`P20261127-011144.jpg` `280KB`、`Selfie/P20261127-011149.jpg` `76KB`）；
  - [cam_recording.png](cam_recording.png)：原生相机 1080p 录像计时出画，`c2.v4l2.avc.encoder` + `c2.android.aac.encoder` 协同生成 `Video/V20261127-011155.mp4`（`540KB`）；
  - [gallery_thumb_ok.png](gallery_thumb_ok.png) & [gallery_video_playing_ok.png](gallery_video_playing_ok.png)：图库彩色视频缩略图正常生成（`mediaserver_crash=0`、`mediaserver_drm_err=0`），点击视频触发 `V4L2Decoder: Set state Idle => Decoding` 硬件解码流畅播放；
  - [nfc-tap-after.png](nfc-tap-after.png)：宿主 `/api/nfc/tap` 触发 13 次 APDU 交互并弹出系统「发现 NFC 标签」卡片。

---

## 4. 改动

1. **`02_prepare` 对齐保护与高通音频 C2 / `media_config.xml` 自愈**：
   - `agent/modules/pipeline/stages/02_prepare/apply_profile.go`：补齐 `neuralnetworks` `qcom -> qti` / `samsung -> samsung` 变体识别；保护 `hasReadme` 机型的 `fstab="qcom"` 与非空 `vintf-version`；对 `qcom` 平台自动注入 `"vendor.audio.c2.preferred": "true"` 与 `"vendor.audio.hw.aac.encoder": "false"`；
   - `agent/modules/pipeline/stages/02_prepare/apply_profile_configs.go`：对存量含 `rank="1"` 或音频 `rank="8"` 的旧 `etc/media_config.xml` 自动自愈升级为 `512/1024` 安全优先级；同目录 `apply_profile_test.go` 新增单元测试覆盖。
2. **跨机型图形、音频属性、GPU SELinux 与 ZRAM 脚本增强**：
   - `devices/hardware/android.hardware.graphics.composer3/src/CdhComposer.cpp`：支持并透传 OEM 自定义色彩意图 `0x100..0x110`；
   - `devices/modules/cdh_redirect/core/rules.c`：增补 `libaimapper.so` 框架白名单与 64 位应用 `linker64` VMA 原子无锁快表，重试上限提升至 `232`；
   - `devices/hardware/android.hardware.audio/sepolicy/vendor_property_contexts`：映射 `vendor.audio. -> u:object_r:exported_config_prop:s0`；
   - `devices/hardware/android.hardware.gpu/sepolicy/vendor_sepolicy_rules.cil`：向 `mediaserver` 与 `mediacodec` 授予 `sysfs_gpu` 只读访问权并 `dontaudit` `vendor_mesa_prop`；
   - `devices/hardware/android.service.daemon/vendor/bin/cph-zram-setup`：`on boot` 幂等关闭 `product/build.prop` 强开的 `persist.vendor.meizu.smart_zram` 与 `debug.sf.show_refresh_rate_overlay_te_fps`。
3. **`meizu/Meizu22` 清单重对齐与重建**：
   - 解包官方固件重新运行 `prepare` 与 `apply-profile -overwrite meizu/Meizu22`，更新 `Meizu22.json`（`1200x2670@480dpi`）、20 项真机传感器、`battery_config.xml`（`5510000` µAh）与 `vendor_sepolicy.cil`（`vendor_display_notch_prop`），移除版本层冗余 `"ro.hardware.vulkan": "virtio"`，全链刷新 `vda.img` 与 `patch.img`（`sha256: 69e71492cd91`）并单向同步至 `rom-main`。

---

## 5. 验证

1. **单元测试与编译**：`go test ./stages/02_prepare/... ./stages/09_build_vendor/...` 全绿；`6.6` 内核 12 个模块与 `android15` 全量 HAL 编译零报错。
2. **实机全维度验收（`MEIZU 22` `12GB+256GB`）**：
   - **冷启与静置性能**：`65.0s` 冷启就绪，`Enforcing`，`restarting_svcs=0`；静置 `800%cpu` 中 `772%~796%idle`，`camera-service` 静置 `0.0%` CPU，相机退后台自动发送 `CDCM_PAUSE`；桌面连续滑动 758 帧 `P50=8ms / P90=19ms / P99=30ms / Missed Vsync=0`；
   - **异常与刷屏清零**：`mediaserver_crash=0`、`mediaserver_drm_err=0`、`smartswap_err=0`、`te_fps_spam=0`、`map_unknown_color=0`、`qdgralloc_err=0`；
   - **六大外设与编解码**：Wi-Fi 5GHz `0% loss`（`0.55ms`）；蜂窝双 PDN `ccmni0`+`ccmni1` 在线，关 Wi-Fi 纯移动数据 `ping 223.5.5.5` `0% loss`（`0.37ms`）；蓝牙开启正常；NFC Tap 13 次 APDU 弹卡正常；原生相机前后摄拍照、录像 `.mp4`、图库缩略图与 `c2.v4l2.avc.decoder` 硬解播放全部正常。

---

## 6. 遗留

- 魅族相机进程因 `cdh_vulkan_mali.c` 主动跳过 `vulkan.virtio.so`（规避 Mesa Venus `AHardwareBuffer` 死锁），在取景期间其算法辅助线程会打印 `E CAMERA_VK: vkEnumerateInstanceVersion failed`，不影响 GLES 预览、拍照与 V4L2 硬编录像；
- 首启阶段 `com.meizu.earphone`（魅族蓝牙耳机固件管家）在停止未注册广播接收器的后台服务时偶发一次有界 `IllegalArgumentException`，不循环重启、不影响蓝牙与音频子系统。
