# #315 Motorola `aito`（A16）原生相机 `McfService` OpenCL 崩溃与 `com.lenovo.moto.*` Vendor Tags 适配

## 1. 问题

- **机型与版本**：`motorola/aito`（Motorola Razr，`aito_16-W1UCS36H.96-35-8-9-20260801`，Android 16 / `vendor_sdk=34`，内核 `6.1.138-4k-cdh`）。
- **现象**：拉起原生相机 `com.motorola.camera5/com.motorola.camera.Camera` 后瞬时闪退回桌面，无法进入预览：
  1. `McfService` 线程触发 `SIGSEGV`（`fault addr 0x81a10`），崩溃于 `/vendor/lib64/egl/mt6878/libGLES_mali.so (clCreateContextFromType+160)`；
  2. Java 侧报 `com.motorola.camera.mcf.McfException: Could not start`（`native_start failed - cameraId = 0, sensorNameString = 0`）。

## 2. 根因

1. **原厂 Mali OpenCL 驱动崩溃与旧版 `cdh_redirect.ko` 未放行 OpenCL 桩库**：
   - `MotCamera5` 启动早期由后台 `McfService` 线程抢跑调用 `libmcf-base.so` 的 `mcf::McfOpenCLBase::initCL()` → `dlopen("/vendor/lib64/libOpenCL.so")` → `clCreateContextFromType()`。
   - `cdh_redirect` 内核模块对普通应用（`uid > 10000`）默认跳过 `/vendor` 分区重定向，仅对 SP-HAL 白名单放行；虽 `devices/modules/cdh_redirect/core/rules.c`（2026-11-22）已新增 `CDH_CARRIER_OPENCL_64` 支持，但 `aito` 盘集（构建于 2026-11-21）`vendor_boot` 内仍为旧版 `cdh_redirect.ko`（`has_opencl=False`）且 `patch.img` 未内置官方兼容桩库 `libOpenCL.so`，致 App 绕过重定向加载到原厂 Mali OpenCL 驱动并在无 `/dev/mali0` 的云机环境下空指针崩溃。
2. **缺失 Motorola 专有 Vendor Tags 与 MCF 开关下发**：
   - 反编译 `MotCamera5.apk` 的 `CustomKeyHelper`、`McfCustomKeyHelper`、`SettingsManager` 与 `McfInitRunnable` 确认：
     - `SettingsManager.getCameraSensorName()` 读取 `com.lenovo.moto.sensor.info.name`（`String`），缺失时退化为 `"0"`，无法匹配 `assets/config_device_aito` 中的 `mot_aito_s5kgn8_mipi_raw` / `mot_aito_ov32b_mipi_raw`；
     - `SettingsManager.processCharacteristicsStage2()` 读取 `com.lenovo.moto.staticinfo.supports_mcf` 与 `supports_reproc`（`Byte`）：当其值为 `0` 时自动将 `MCF_ALLOW` / `REPROC_ALLOW` 设为 `Boolean.FALSE`，令 `McfInitRunnable` 跳过闭源 MCF 多帧管线并走标准 Camera2 预览/拍照管线。

## 3. 证据

- 崩溃现场见同目录 [log-fail.txt](log-fail.txt)（`tombstone_09`：`McfService` `SIGSEGV` 于 `/vendor/lib64/egl/mt6878/libGLES_mali.so (clCreateContextFromType+160)` ← `/vendor/lib64/libOpenCL.so` ← `mcf::McfOpenCLBase::initCL()+924`）。
- 闭环验收日志与实屏截图见同目录：
  - [log-pass.txt](log-pass.txt)（`dumpsys media.camera` 导出 `com.lenovo.moto.sensor.info.name` / `supports_mcf=0` / `supports_reproc=0`，前后摄 `Stream[0]` 正常推帧、DCIM 落盘 JPEG）
  - [photo-rear.png](photo-rear.png)（后摄 3:4 满屏超清蓝天白云实时预览）
  - [photo-front.png](photo-front.png)（前摄实时预览 + 左下角已拍出高清照片缩略图）

## 4. 改动

1. **新增 Motorola 零前缀碰撞 Vendor Tag 表**：
   - 新增 [cdh_motorola_vendor_tags.inc](../../../../devices/hardware/android.hardware.camera/src/cdh_motorola_vendor_tags.inc)，提取 `CustomKeyHelper` + `McfCustomKeyHelper` 去重 154 项 Vendor Tags（20 个 section，使用 `0x8300xxxx..` 段，与基础表零前缀碰撞），导出 `kMotoSensorInfoNameTag`、`kMotoSupportsMcfTag`、`kMotoSupportsReprocTag`、`kMotoSupportIspHidlTag` 等常量。
2. **Camera HAL 运行时品牌适配**：
   - 修改 [camera_provider_service.cpp](../../../../devices/hardware/android.hardware.camera/src/camera_provider_service.cpp)，新增 `isMotorolaDevice()`（匹配 `motorola`/`lenovo`），在 `activeVendorTagDescs()` 追加 Motorola 标签表，并在 `buildCharacteristics` 注入 `supports_mcf=0`、`supports_reproc=0`、`support_isp-hidl=0` 及 `sensor.info.name`（`aito` 后摄 `mot_aito_s5kgn8_mipi_raw` / 前摄 `mot_aito_ov32b_mipi_raw`）。
   - 重编 `android14`（`vendor_sdk=34`）`android.hardware.camera-service` 刷入 `aito` `patch.img`。
3. **升级 `aito` OpenCL 兼容桩与 `cdh_redirect.ko`**：
   - 编译官方 [cdh_opencl_stub.c](../../../../devices/hardware/android.hardware.gpu/src/cdh_opencl_stub.c) 为 ARM64 `libOpenCL.so`（43,752 B）写入 `aito` `patch.img` `/vendor/lib64/libOpenCL.so`（`mode=0644`，`u:object_r:same_process_hal_file:s0`，`e2fsck -fn` 全绿）；
   - 将 `aito` `vda.img` 中 `vendor_boot_a`/`vendor_boot_b` 的旧版 `cdh_redirect.ko` 原位升级为同代 `6.1.138-4k-cdh` 新版（含 `CDH_CARRIER_OPENCL_64` 放行）。

## 5. 验证

- **冷启实测**（`aito_16-W1UCS36H.96-35-8-9-20260801`，ADB `127.0.0.1:5911`，注入 MP4 视频源）：
  - `dumpsys media.camera` 确认 `Device 0` / `Device 1` 均导出 `com.lenovo.moto.staticinfo.supports_mcf=0`、`supports_reproc=0` 与正确 `sensor.info.name`；
  - 启动 `com.motorola.camera5/com.motorola.camera.Camera` 全程零 `SIGSEGV`、零 `McfException`，后摄 `Device 0` `Stream[0]` 稳定产出 `167+` 帧，[photo-rear.png](photo-rear.png) 审图确认 3:4 满屏超清蓝天白云出画；
  - 点击快门实拍落盘 `/sdcard/DCIM/Camera/IMG_20261123_173825556.jpg`（21,530 B），左下角相册缩略图清晰显示实拍画面；
  - 点击切换前摄零崩溃切至 `Device 1`（`Stream[0]` 持续产出 `110+` 帧），[photo-front.png](photo-front.png) 审图确认前摄满屏超清出画。

## 6. 遗留

- 后续收尾阶段需将 `android.hardware.camera` 改动同步重编至全代 `android13..17`，并在 `09_build_vendor` 流水线中固化对带 `libOpenCL.so` 机型的官方 OpenCL 兼容桩自动装配。
