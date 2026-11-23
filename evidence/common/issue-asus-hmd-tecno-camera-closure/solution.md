# #317 ASUS `ASUSAI2501`、HMD `ARW`、Tecno `TECNO-CM5` 原生相机全闭环与 6.6 `cdh_redirect` `libOpenCL_system.so` 适配

## 1. 问题

本条目完成跨机型原生相机适配收尾的最后 3 台机型排查与实机验收：

1. **ASUS `ASUSAI2501`**（ROG Phone 9 Pro，`ASUSAI2501_15-AQ3A.240829.003-20250122`，A15 / `vendor_sdk=35`，内核 `6.6.127-4k-cdh`）：
   - 干净冷启实例 `/storage/emulated` 仍处于 `UNMOUNTABLE`（`vold` 报 `code -5 EIO`、`MediaProvider` 启动超时 ANR）；
   - 修通存储后拉起 `com.asus.camera` 依次暴露弹窗 `"Camera is not ready. Please try again later."`、开流后 0 帧黑屏（`qdgralloc: Invalid gralloc handle`）以及 `DIT-Enq` 线程在 `libarcsoft_high_dynamic_range_v5.so` 发生 `SIGSEGV`。
2. **HMD `ARW`**（HMD Crest Max，`ARW_14-QP1A.190711.020-20250620`，A14 / `vendor_sdk=33`）：
   - 此前旧 EGL/HAL 下 `WhatsNewPage` 翻页后 ANR，需刷新最新 `android13` EGL 插桩库与相机 HAL 后冷启复测并完成前后摄/快门实机审图验收。
3. **Tecno `TECNO-CM5`**（TECNO Camon 30，`TECNO-CM5_16-BP2A.250605.031.A3-20260324`，A16 / `vendor_sdk=36`）：
   - 刷新最新 `android16` 相机 HAL 与 EGL 插桩库后，关闭休眠干扰完成快门拍照、模式切换与前后摄互切全流程复测与实屏审图。

## 2. 根因

对 ASUS `ASUSAI2501` 通过 ANR 堆栈、Dex 反汇编（`dexdump`）、`logcat` 与 `dropbox` 定位出四层串联根因：

1. **旧版 `media_config.xml` 缺失 `default1`/`default9` 映射致 `mediaserver` 死锁进而拖死 `MediaProvider` 与 `/storage/emulated`**：
   - `ASUSAI2501` 配置文件生成于流水线修复之前，`/vendor/etc/media_config.xml` 仅含 `default2` 且带 `/vendor/bin/` 非法前缀，导致 `android.hardware.media.c2-mediatek`（`IComponentStore/default1`）与 `vendor.dolby.media.c2-default-service-dax`（`default9`）启动即 `exit(1)`；
   - `mediaserver` 在 `getCodecList()` 无限死等 `IComponentStore/default1`，阻塞 `MediaProvider.onCreate()` → `TranscodeHelperImpl.hasHDRPlugin()` → `MediaCodec.createDecoderByType()`，致使 `ExternalStorageServiceImpl` 永久处于 `Pending` 状态并触发 `StorageUserConnection` 超时、`/storage/emulated` 沦为 `UNMOUNTABLE`。
2. **`vendor_property_contexts` 未放行 `vendor.camera.*` 致 `AsusCamera` 过滤掉全部相机**：
   - 反汇编 `LO1/i;`（`AbstractModeList`）、`LG1/d0;.L()` 与 `LH1/n;`（`AsusSystemProperty`）确认：`com.asus.camera`（`platform_app` 域）依赖 `vendor.camera.lens_type` 解析每个 `cameraId` 的镜头类型；未在 `vendor_property_contexts` 映射到 `vendor_set_camera_prop` 时全部 `Access denied`，所有 `cameraId` 被判为 `Not use cameraID`；
   - 此外原厂 `vendor.camera.lens_type=4:std;1:std;2:wid;3:tel;0:multi` 将 `0` 标为依赖物理子摄 `4/2/3` 的 `multi`，云机双摄下仅剩前摄 `1` 可用，需对齐为 `0:std;1:std`。
3. **高通原厂 `libOpenCL.so` → `libCB.so` 提前拉入原厂 `QtiMapper5` 污染 Bionic `mapper.qti.so` 缓存**：
   - `AsusCamera` 声明 `uses_libraries=libOpenCL.so`，在旧版 6.6 `cdh_redirect.ko`（无 `CDH_CARRIER_OPENCL_64` 放行）与原厂 `libOpenCL.so` 组合下，`libOpenCL_adreno.so` → `libCB.so` → `libgralloc.qti.so` 提前把原厂 85 KB 的真机 `QtiMapper5` `mapper.qti.so` 载入进程，导致后续 `CdhGrallocProxy` 找不到 `HMI`、`libui` 拒收云机 `minigbm` 句柄（`ints(33/26) fds(3/2)`），预览流全部 `dequeueBuffer` 失败 0 帧。
4. **`libxditk_ditArchLIB.so` 绕过 `/vendor/lib64/libOpenCL.so` 直连 `/system_ext/lib64/libOpenCL_system.so` 致 ArcSoft HDR `SIGSEGV`**：
   - 替换 `/vendor/lib64/libOpenCL.so` 后，`DIT-Enq` 线程仍经 `/system_ext/lib64/libOpenCL_system.so` 拉起高通 `libCB.so` / `Adreno-GSL` stub GPU，致 `libarcsoft_high_dynamic_range_v5.so` 解引用空指针（`fault addr 0x30`）崩溃。

## 3. 证据

- 故障日志见同目录 [log-fail.txt](log-fail.txt)。
- 三机验收日志见同目录 [log-pass.txt](log-pass.txt)，实屏审图截图见同目录：
  - ASUS `ASUSAI2501`：[asus-rear.png](asus-rear.png)（后摄 1x 满屏超清蓝天白云 + 左下角实拍照片缩略图）、[asus-front.png](asus-front.png)（前摄满屏超清出画）
  - HMD `ARW`：[arw-rear.png](arw-rear.png)（后摄 1x 满屏超清蓝天白云 + 右下角实拍照片缩略图）、[arw-front.png](arw-front.png)（前摄肤色预设实时预览蓝天白云）
  - Tecno `TECNO-CM5`：[tecno-rear.png](tecno-rear.png)（后摄 AI CAM 满屏超清蓝天白云 + 左下角实拍缩略图）、[tecno-front.png](tecno-front.png)（前摄 Portrait 模式满屏超清出画）

## 4. 改动

1. **修复 ASUS Codec2 假登记配置**：
   - 更新 [media_config.xml](../../../../devices/manifest/configs/asus/ASUSAI2501/ASUSAI2501-android15/vendor/etc/media_config.xml) 补齐 `android.hardware.media.c2-mediatek -> default1` 与 `vendor.dolby.media.c2-default-service-dax -> default9` 映射及非空组件，同步写入 ASUS `patch.img`。
2. **修复 ASUS 相机属性 SELinux 与镜头类型对齐**：
   - 在 ASUS `patch.img` `/vendor/etc/selinux/vendor_property_contexts` 追加 `vendor.camera.` 与 `ro.vendor.camera.sound.forced` 映射至 `vendor_set_camera_prop`；
   - 在 ASUS `patch.img` `/vendor/etc/init/cdh_system_props.rc` 追加 `setprop 'vendor.camera.lens_type' '0:std;1:std'`。
3. **升级 `cdh_redirect` 支持 `/system_ext/lib{,64}/libOpenCL_system.so` 并同步重编 6.6 内核模块**：
   - 修改 [rules.c](../../../../devices/modules/cdh_redirect/core/rules.c) 的 `cdh_classify_sphal_lib()`，当载体已提供 OpenCL 桩库时同步放行 `/system_ext/lib64/libOpenCL_system.so`（与 32 位）；
   - 在 [config.json](../../../../devices/manifest/configs/asus/ASUSAI2501/ASUSAI2501-android15/module/config.json) 追加单文件重定向规则 `/system_ext/lib64/libOpenCL_system.so -> /mnt/vendorpatch/vendor/lib64/libOpenCL.so`（严守不修改官方 `system_ext.img`）；
   - 通过 Docker 6.6 内核构建卷同步重编 6.6 全套 13 个 `.ko` 并通过 `cdh-pipeline verify-modules`（`cdh_redirect.ko` 65 符号、`cdh_device_info.ko` 76 符号全一致）；
   - 将官方 OpenCL 兼容桩写入 ASUS `patch.img` `/vendor/lib64/libOpenCL.so`（`same_process_hal_file`），并将新版 6.6 `cdh_redirect.ko` 与含 `libOpenCL_system.so` 规则的 `cdh_redirect.rules` 原位写入 ASUS `vda.img` `vendor_boot_a`/`vendor_boot_b`。
4. **HMD `ARW` 与 Tecno `TECNO-CM5` 刷新最新代际 EGL 插桩库**：
   - 通过 `patch_hal_refresh.py --only egl` 分别向 `ARW` 刷入 `android13`、向 `TECNO-CM5` 刷入 `android16` 最新 `libEGL_meow.so`/`libGLESv2_meow.so`/`libGLESv1_CM_meow.so`，均通过 `e2fsck -fn` 与 `security.selinux` 回读校验。

## 5. 验证

- **ASUS `ASUSAI2501` 冷启实测**：`/storage/emulated` 状态 `MOUNTED`，`IComponentStore/{default,default1,default9,software}` 全部在线；拉起 `com.asus.camera/com.asus.camera2.app.CameraActivity` 零崩溃秒入预览，后摄 `Device 0` `Stream[0]/[1]` 产出 `937+` 帧满屏超清蓝天白云（[asus-rear.png](asus-rear.png)），快门实拍落盘 `/sdcard/DCIM/Camera/P_20261123_185604.jpg`（1.25 MB），切换前摄 `Device 1` `Stream[0]/[1]` 产出 `128+` 帧满屏超清出画（[asus-front.png](asus-front.png)），全程 `dropbox` 零新增崩溃。
- **HMD `ARW` 冷启实测**：跳过 `WhatsNewPage` 零 ANR 秒入预览，后摄 `Device 0` 产出 `249+` 帧满屏超清蓝天白云（[arw-rear.png](arw-rear.png)），快门实拍落盘 `/sdcard/DCIM/Camera/IMG_20261123_185909.jpg`（1.03 MB），切前摄 `Device 1` 产出 `120+` 帧满屏超清出画（[arw-front.png](arw-front.png)），全程零 ANR/崩溃。
- **Tecno `TECNO-CM5` 冷启实测**：后摄 AI CAM 产出 `641+` 帧满屏超清蓝天白云（[tecno-rear.png](tecno-rear.png)），快门实拍落盘 `/sdcard/DCIM/Camera/IMG_20261123_190157_907.jpg`（46 KB），切换前摄 Portrait / AI CAM（`Device 1`，`126+`/`94+` 帧，[tecno-front.png](tecno-front.png)）并切回后摄（`Device 0`，`96+` 帧）全程零崩溃、零 ANR。

## 6. 遗留

- ASUS 的 `vendor_property_contexts`（`vendor.camera. -> vendor_set_camera_prop`）、`vendor.camera.lens_type=0:std;1:std` 与 OpenCL 兼容桩装配需一并固化进 `09_build_vendor` 流水线（随本轮收尾工作同步完成）。
