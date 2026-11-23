# #313 — vivo `PD2454`（OriginOS A15）`cameraserver` 启动 17 分钟死锁与 vivo 专有相机适配

## 1. 问题

`vivo/PD2454`（OriginOS A15，`PD2454_15-AP3A.240905.015.A2-20250416`，`vendor_sdk=35`）冷启后，`cameraserver` 与 `vivocameraserver` 进程均在线且 `media.vivocamera` 可见，但标准 `media.camera` 与 `android.frameworks.cameraservice.service.ICameraService/default` 长时间 `not found`，原生相机（`com.android.camera`）进入后卡死或黑屏崩溃。

## 2. 根因

拉取并反汇编 `/system/lib64/libcameraservice.so`、`/system/lib64/libvivocameraservice.so`、`/system/lib64/vendor.vivo.hardware.vivocameraprovider-V1-ndk.so`、`/system/app/VivoCamera/VivoCamera.apk` 与 `/system/app/Vivo3rdAlgoService/Vivo3rdAlgoService.apk` 锁定四层递进根因：

1. **`cameraserver` 1000 秒（16 分 40 秒）启动死锁**：
   - `CameraService::instantiate()` 同步执行 `CameraService::onFirstRef()` → `VivoDevice::GetInstance()` → `VivoDevice::registVivoDevice()`（`libcameraservice.so` 偏移 `0x3b7868..0x3b7af0`）；
   - 该函数在 `addService("media.camera")` 之前硬编码循环 100 次（`mov w24, #0x64`）调用 `AServiceManager_getService("vendor.vivo.hardware.vivocameraprovider.IVivoCameraProvider/default")`，Android 15 每次查找缺失服务阻塞超时 10 秒，合计卡死整整 **1000 秒**（实测 `15:18:53` 启动，直到 `15:35:50` 才完成 `ADD device 0/1` 并注册 `media.camera`）。
2. **`vivocameraserver` 同步死等 `IVivoCameraProvider`**：
   - `libvivocameraservice.so` 处理 `com.android.camera` 的 `connectDevice`（code 1）时调用 `AServiceManager_waitForService("vendor.vivo.hardware.vivocameraprovider.IVivoCameraProvider/default")`，导致相机主线程无限卡死在 Splash Screen。
3. **vivo 原生相机专有 Vendor Tags 与 `halversion` 管线门控**：
   - `com.android.vcamera.core.VCameraInfo.<init>` 直接读取 `com.vivo.chi.override.CameraType`（`TYPE_BYTE` ASCII 串），缺失抛 `IllegalArgumentException` 闪退；
   - `VModeCharacteristicsTable` 按 `"Master"` 识别主摄全部模式，而 `VCameraInfo.D()` 按 `com.vivo.halversion` 位掩码选型管线：bit2（`& 4`）进入 `VIF_RAW_TYPE`（`RawVifCameraDeviceImpl`），强校验 4~5 个 `Master_Dummy` 虚拟相机否则抛 `physicalId 0 do not have a dummyId!!!`；置 `halversion = 2`（`0b10`）则走标准 `VIF_YUV_TYPE` / `VIF_JPEG_TYPE` Camera2 管线。
4. **`Vivo3rdAlgoService` 访问 `hal_camera3rd_service` 受 SELinux 拦截**：
   - `com.android.camera` 通过 `ContentProvider` 查询 `com.vivo.vivo3rdalgoservice`（`platform_app`），其查找 `vendor.vivo.hardware.vivoalgoservice.IVASAlgoProvider/default`（`hal_camera3rd_service`）被 Enforcing 拦截；放行后其原生库仍依赖闭源高通 `libvivo.vas.interface.so` 异步算法回调。

## 3. 证据

- 修复前故障与反汇编铁证见同目录 [log-fail.txt](log-fail.txt)。
- 修复后冷启 44 秒内全服务就绪日志（`media.camera` / `ICameraService/default` / `IVivoCameraProvider/default` / `IVASAlgoProvider/default` / `nfc` 全 `found`，`ADD device 0/1` 在开机第 28 秒完成，168 项元数据含 23 项 `com.vivo.*` 标签）见同目录 [log-pass.txt](log-pass.txt)。

## 4. 改动

1. **[camera_provider_service.cpp](file:///Volumes/work/cloud-phone-hub/devices/hardware/android.hardware.camera/src/camera_provider_service.cpp)**：
   - 新增 `isVivoDevice()`（识别 `vivo` / `iqoo`）；
   - 新增 `registerVivoCameraProviderIfNeeded()`，在 `hal_camera_default` 域（与 `vendor_service_contexts` 中 `u:object_r:hal_camera_service:s0` 天然一致）注册最小合规 VINTF AIDL 服务 `vendor.vivo.hardware.vivocameraprovider.IVivoCameraProvider/default`（version=1，hash=`eee68c0710662f59503dcfb62464d1c7139183a0`）及子接口 `IVivoCameraDevice`（hash=`c351d76052cc9e032c73327d70efe585464cc31f`）、`ICameraVIF`（hash=`eb565c2632918fe598f93a93288030332232108f`）；
   - 新增 23 项 `kVivoVendorTagDescs`（`0x8040xxxx..0x8044xxxx` 段），为前后摄注入 `com.vivo.chi.override.CameraType="Master"`、`com.vivo.halversion=2`、`SensorSize`、`image_sizes`、`focallen.35mm` 等静态特性；
   - 重编 `android15` 版 `android.hardware.camera-service`。
2. **[neuralnetworks_service.cpp](file:///Volumes/work/cloud-phone-hub/devices/hardware/android.hardware.neuralnetworks/src/neuralnetworks_service.cpp)**：
   - 新增 `OnVivoVasAlgoProviderTransact` / `OnVivoVasAlgoContextTransact` 处理 `vendor.vivo.hardware.vivoalgoservice.IVASAlgoProvider`（V2，hash=`88a8a75e50b62b2dd308786c7bf7d5587a47d9d8`）握手与 `VASResultIPC` 序列化响应；重编 `android15` 版 `android.hardware.neuralnetworks-service`。
3. **`vivo/PD2454` `patch.img`**：
   - 刷入上述 `android15` 版 `camera-service` 与 `neuralnetworks-service`，在 `/vendor/etc/vintf/manifest/android.hardware.camera-service.xml` 追加 `vendor.vivo.hardware.vivocameraprovider` / `IVivoCameraProvider/default` 声明，在 `/vendor/etc/selinux/vendor_sepolicy.cil` 放行 `platform_app_202404` 查找 `hal_camera3rd_service` / `hal_cameralog_service` / `hal_camera_service` 及与承载 HAL 的 binder 通信，通过 `ea_get -V` 与 `e2fsck -fn` 校验。

## 5. 验证

冷启 `vivo/PD2454`（`PD2454_15-AP3A.240905.015.A2-20250416`）实测：
- **启动耗时**：`16:44:29` 拉起实例，`16:44:57`（**28 秒**，此前需 1000 秒 / 17 分钟）`cameraserver` 即完成 `ADD device 0` 与 `ADD device 1`；`+44.3s` `sys.boot_completed=1` 时 `media.camera`、`ICameraService/default`、`IVivoCameraProvider/default`、`ICameraProvider/internal/0`、`media.vivocamera`、`IVASAlgoProvider/default`、`nfc` 全部 `found`；
- **元数据与连接**：`dumpsys media.camera` 展示 168 项特性（含 `com.vivo.chi.override.CameraType="Master"`、`com.vivo.halversion=2` 等 23 项 vivo 标签），`com.android.camera` 零 Java 崩溃通过 `VCameraInfo` 与 `VModeCharacteristicsTable` 校验并稳定 `CONNECT device 0`；
- **NFC 回归**：`Service nfc: found`、`com.android.nfc` 在线运行正常。

## 6. 遗留

1. **vivo 原生相机 `Vivo3rdAlgoService` 闭源算法框架依赖**：`com.android.camera` 在 UI 首帧渲染前同步查询 `content://com.vivo.vivoalgoservicecontentprovider/allAlgoInfo`，而 `Vivo3rdAlgoService` 后台线程依赖闭源 `libvivo.vas.interface.so` / `VivoAlgoCallbackAidl` 异步回调返回算法能力表，未返回时 60s `System.exit(0)` 触发连坐重启，导致原生相机停留在 `CONNECT device 0 / UNCONFIGURED` 未下发预览 `Surface`（标准 `media.camera` / Camera2 服务与虚拟摄像头 HAL 本身已完全就绪；后续如需打通 vivo 原生相机预览，需补齐 `IVASAlgoCallback` 异步回调或用 `cdh_redirect` 提供 `libAlgoNativeInterface.so` 垫片使 `NativeProcess.isVAS()=false` 直接走内置 JSON）。
2. **`09_build_vendor` 流水线持久化**：`android.hardware.camera-service.xml` 的 `IVivoCameraProvider` 声明及 `platform_app` 放行 `hal_camera3rd_service` 规则需在全仓收尾阶段固化进 `agent/modules/pipeline/stages/09_build_vendor/`。
