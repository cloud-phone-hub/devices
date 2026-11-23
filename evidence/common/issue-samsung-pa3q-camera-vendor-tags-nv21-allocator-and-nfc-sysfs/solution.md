# #309 — Samsung `pa3q`（Galaxy S25 Ultra / One UI A16）原生相机与 NFC 深度适配闭环

## 1. 问题

Samsung Galaxy S25 Ultra（`samsung/pa3q`，系统 A16 + `vendor_sdk=35`）存在两类阻塞：
1. **原生相机（`com.sec.android.app.camera`）**：突破后置逻辑多摄 ID `20` 缺失闪退后，进入相机因缺少三星专有 Vendor Tag `samsung.android.scaler.availablePreviewStreamConfigurations` 在 `PreviewManagerImpl.changePreviewSurfaceSize` 触发 `NullPointerException` 闪退；补齐 Vendor Tag 并收到 `1440x1080 YUV_420_888` 首帧回调后，取景框呈黑屏。
2. **NFC（`com.android.nfc` / `SecNfc.apk`）**：`android.hardware.nfc.INfc/default`、`nfc`、`samsungnfc` 服务均在线但冷启后 `mState=off`，执行 `svc nfc enable` / `cmd nfc enable-nfc` 均因 `NFC is not initialized yet:false` 被拒，无法启动轮询与刷卡闭环。

## 2. 根因

1. **三星相机专有 Vendor Tag 缺失**：反编译 `SamsungCamera.apk`（`SemCameraCharacteristics.java`、`ScalerCapability.java`、`StreamConfig.java`、`VideoStreamConfig.java`）证实三星相机通过 `SemCameraCharacteristics` 强依赖 20 项 `samsung.android.{scaler,control,unihal,sensor.info}.*` 静态 Vendor Tag（其中流配置采用标准 4 元组 `[hal_format, w, h, isInput]`，`33` 自动映射为 `SemImageFormat.JPEG`，`34` 为 `PRIVATE`，`35` 为 `YUV_420_888`；视频配置采用 6 元组 `[w, h, minFps, maxFps, previewW, previewH]`），缺失时 `getSamsungScalerAvailablePreviewSizes()` 返回空列表引发 NPE。
2. **预览黑屏根因（minigbm 缺失 `HAL_PIXEL_FORMAT_YCrCb_420_SP` 映射）**：三星 `Core2SurfaceUtil.nativeProduceFrameWithYuv420` 收到 Camera HAL 的 `YUV_420_888` 帧后，直接向 `SurfaceView` 的 `BufferQueueProducer` 申请 `format=17`（`0x11 = HAL_PIXEL_FORMAT_YCrCb_420_SP / NV21`，`usage=0x20900`）的 `GraphicBuffer`。上游 minigbm `cros_gralloc_convert_format()` 未映射该格式直接返回 `DRM_FORMAT_NONE`，导致每帧 `GraphicBufferAllocator: Failed to allocate (1440 x 1080) format 17 usage 20900: -2147483648`。
3. **三星 NFC 拒绝初始化根因**：`dumpsys samsungnfc` 显示 `SamsungExtension Kernel Driver is not exist, so boot is not allowed`；反编译 `SecNfc.apk`（`com.samsung.android.nfc.util.NfcUtil#isKernelDriverExist` 与 `SamsungExtension.java:549`）证实其硬检 `new File("/sys/class/nfc/nfc_support").exists() || new File("/sys/class/nfc_sec/nfc_support").exists()`，云机内核无物理 NFC 驱动故该 sysfs 节点不存在，导致 `SamsungExtension` 跳过 `executeTriggerBoot()`。
4. **工具链顺手修复**：排查中发现 `out/devices/tmp/patch_hal_refresh.py` 此前通过 `debugfs stat` 读取 `security.selinux`，当标签较长落入外部 xattr block（如 `hal_graphics_allocator_default_exec`）时 `stat` 不打印导致误写 `unlabeled`；已一并修复为强制使用 `ea_get -V` 并禁止空标签跳过校验。

## 3. 证据

- 失败日志：[log-fail.txt](log-fail.txt)（含 `PreviewManagerImpl` 缺 Vendor Tag、`GraphicBufferAllocator format 17` 分配失败、`SecNfc` `Kernel Driver is not exist` 反编译定位）
- 通过日志：[log-pass.txt](log-pass.txt)（含 `/sys/class/nfc/nfc_support=1`、`dumpsys nfc mState=on`、`/api/nfc/tap` & `remove`、`samsung.android.*` 20 项 Vendor Tag、`Device 20 is open` 与 `FpsStats stream 1 valid=1`、`20261123_113347.jpg` 实拍文件）
- 实屏预览截图：[cam_pa3q_final.png](cam_pa3q_final.png)
- 实拍照片原图：[photo.jpg](photo.jpg)（全幅高清蓝天白云，无条纹、色彩精准）

## 4. 改动

1. **虚拟 NFC sysfs 节点（`cdh_device_info.ko`）**：
   - 新增 `devices/modules/device_info/nfc/nfc_sysfs.c`：模块加载时安全注册 `/sys/class/nfc` 类与只读属性 `nfc_support`（返回 `"1\n"`），兼容 Linux `< 6.4` 与 `>= 6.4` API；
   - 更新 `devices/modules/device_info/Kbuild`、`include/cdh_device_info.h`、`core/main.c` 接入初始化与卸载清理；
   - 编译 6.6 版 `out/devices/kernel/6.6/modules/cdh_device_info.ko`（`verify-modules` 72 个符号版本全部一致）并注入 `pa3q` `patch.img`。
2. **跨品牌 Camera HAL 运行时扩展（`devices/hardware/android.hardware.camera/src/camera_provider_service.cpp`）**：
   - 将静态 `kNote13VendorTagDescs` 升级为运行时按品牌动态组装的 `activeVendorTagDescs()`（`vendor_tag_ops_t` 与 `ICameraProvider::getVendorTags` 共享同一表）；
   - 为 Samsung 设备在未冲突段 `0x80200000..0x8023xxxx` 注入 20 项核心 `samsung.android.*` Vendor Tags（预览/拍照/缩略图/视频流配置、缩放上下限、AE/AF/AWB/LDC/HDR/Metering/ColorSpace/BeautyFace 等）并同步登记至 `ANDROID_REQUEST_AVAILABLE_CHARACTERISTICS_KEYS`；为逻辑后摄 `"20"` 补充 `LOGICAL_MULTI_CAMERA` 与物理子摄 `{"0", "2"}`；
   - 同期并入 Meizu 逻辑后摄 `"4"` 的 `LOGICAL_MULTI_CAMERA` 与物理子摄 `{"0", "2"}`，以及 OPlus（OPPO/OnePlus/realme）运行时 `com.oplus.supported.cameraid.type` 与多摄拓扑；
   - 重编 `android14..17` 全代 `android.hardware.camera-service`。
3. **minigbm Allocator 支持 `HAL_PIXEL_FORMAT_YCrCb_420_SP (17)`（`devices/hardware/android.hardware.graphics.allocator/src/CdhAllocator.h`）**：
   - 在 `allocate2()` 新增第四级兜底：当请求 `format == 17` 失败时，按 `format == 35 (YCbCr_420_888)`（底层 VirGL 2-plane `DRM_FORMAT_NV12`，plane/stride/offset 与 NV21 布局一致）阶梯分配，并将 handle 的 `droid_format` 回写为 `17`、保留底层 `DRM_FORMAT_NV12` 供 Mesa EGL 正常采样；同步扩展 `isSupported()`；
   - 重编 `android14..17` 全代 `android.hardware.graphics.allocator-service.minigbm` 并刷入 `pa3q`（及 `Meizu22`、`CPH2797`、`CPH2767`）。
4. **刷新脚本强化（`out/devices/tmp/patch_hal_refresh.py`）**：
   - `read_inode_meta()` 改为优先通过 `debugfs ea_get -V` 获取 `security.selinux`，并增加已知 HAL 路径默认标签兜底与强制非空校验。

## 5. 验证

在 `samsung/pa3q`（`pa3q_16-AP3A.240905.015.A2-20260812`）全新干净实例冷启实测：
- **内核节点与 NFC**：`/sys/class/nfc/nfc_support` 存在且读回 `1`；冷启开箱即 `dumpsys nfc` 返回 `mState=on`、`listenTech=0x7`、`pollTech=0x2f`（`SamsungExtension onStateUpdated: STATE_TURNING_ON -> STATE_ON`）；调用 `POST /api/nfc/tap` 与 `/api/nfc/remove` 全链路畅通（`connected:true, initialized:true, discovering:true`，持续轮询 `rxCommands` 38→303、`txResponses` 56→586）。
- **三星原生相机**：`dumpsys media.camera` 完整导出全部 20 项 `samsung.android.*` Vendor Tags；喂入 `5b56c8b31b1ad.mp4` 启动 `com.sec.android.app.camera`，`CONNECT device 20` 零崩溃进入实时预览，30fps `FpsStats: stream 1 valid=1 invalid=0`，零 `GraphicBufferAllocator` / `Core2SurfaceUtil` 报错；实屏出画且连续执行快门拍照、前摄/后摄切换、视频/人像模式切换全程零崩溃，成功生成实拍照片 `/sdcard/DCIM/Camera/20261123_113347.jpg`（21671 B，蓝天白云全幅高清、色彩准确）。

## 6. 遗留

- 三星私有预览路径 `Core2SurfaceUtil.nativeProduceFrameWithYuv420` 将 `YUV_420_888` 软件写入 `format=17` 预览 `SurfaceView` 时，因底层 VirGL 2-plane `NV12` 与其私有单块 NV21 步长/UV 偏移约定略有差异，实时预览取景框可见偏绿且左侧存在 1/3 UV 区域条纹；拍照输出的 JPEG 原图（`photo.jpg`）完全正常无色偏无条纹，后续如需追求三星私有预览完美色准，可进一步对齐 `Core2SurfaceUtil` 的 `lock` stride 布局。
- 其余内核树（5.10/5.15/6.1/6.12）的 `cdh_device_info.ko` 在下次 `04_build_modules` 构建时将自动带上 `nfc_sysfs.c`（本次实测仅刷 6.6 版至 `pa3q`）。
