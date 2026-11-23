# #312 Honor `chora`（MagicOS A16）原生相机 Vendor Tags / `isNewArch` 预览出画与 NFC 全链路闭环

## 问题

Honor `chora`（BSN-NX1，MagicOS 10 / Android 16，高通平台，`vendor_sdk=36`，内核 `6.12.81-4k-cdh`）冷启后存在两项核心故障：

1. **原生相机（`com.hihonor.camera`）黑屏且设备永久停留在 `UNCONFIGURED`**：能够完成 `openCamera("0")`，但从未向 HAL 下发 `configureStreams`，预览画布全黑。
2. **NFC 系统服务未注册（`Service nfc: not found`）**：`android.hardware.nfc-service` 存活，但 `com.android.nfc` 跳过初始化或启动期崩溃，导致 `dumpsys nfc` 报错 `Can't find service: nfc`，且 `com.android.se` 报缺共享库 `com.hihonor.nfc`。

## 根因

通过拉取并反编译 `HnCamera2.apk`、`NfcNci_45_oversea.apk` 与反汇编 `/system/lib64/libnfc-nci-snxxx.so`、`vendor.honor.hardware.nxpnfc-V1-ndk.so`，逐层锁定完整根因链：

### 1. 原生相机三层阻塞链
1. **缺少 `com.hihonor.capture.metadata.cameraSceneMode` 导致会话未下发**：`RealDeviceSingle#createCaptureSessionBySessionConfiguration` 在组装 `SessionConfiguration` 时直接调用未包裹 `try/catch` 的 `CaptureRequest.get(w7.c.h1)`（`com.hihonor.capture.metadata.cameraSceneMode`，`TYPE_INT32`），HAL 未注册该 Vendor Tag 导致抛 `IllegalArgumentException: Could not find tag for key`，跳过 `CameraDevice.createCaptureSession`，设备卡死在 `UNCONFIGURED`。
2. **误入私有 `HwServiceHost` 管线导致无预览 target**：会话进入 `CONFIGURED` 后，因未宣告 `com.hihonor.device.capabilities.honor_sit_supported = 1`，`CameraServiceFactory` 判定 `isNewArch = false`，`SurfaceManager` 选中 `ServiceHostSurfaceProcessController`，其在 `applyTargets` 显式跳过 `normal_preview`（`applyTargets servicehost no need normal preview surface`），致 `CaptureRequest.Builder` 无任何目标 surface、`setRepeatingRequest` 返回 `seqId: -1`。
3. **首帧到达后主后摄列表为空触发 NPE**：开启 `isNewArch = true` 并成功下发 `setRepeatingRequest`、收到首帧回调 `onFirstValidFrameAvailable` 后，`SlowmotionUtil#getCameraIdForSlowMotion` 查询主后摄列表 `b.f668q` 为空返回 `""`，调用 `getCameraCharacteristics("").get(...)` 触发 `NullPointerException` 闪退。原因系后摄缺少 `com.hihonor.device.capabilities.mainModeSupported = 1`，未被 `CameraServiceFactory` 注册进主后摄列表。

### 2. NFC 五层阻塞链
1. **缺少 `/dev/pn544` 节点**：`NfcApplication#onCreate` 硬检 `new File(SystemProperties.get("nfc.node", "/dev/pn544")).exists()`，云机缺失该节点导致直接跳过 `new NfcService(...)`。
2. **缺少 `/sys/nfc/nfc_chip_type` 与 SELinux `sysfs` 读权**：`HnVendorAdapter` 调用 `HnNfcService.readFileByChars("/sys/nfc/nfc_chip_type").equalsIgnoreCase("sn220u")`（**未做 `.trim()`，不能含尾换行符**）以决定加载 `/system/lib64/libnfc_nci_jni_snxxx.so`；缺失节点、含尾 `\n` 或 Enforcing 模式下 `nfc` 域无权读取 `sysfs` 均会导致回退到不存在的 `libnfc_nci_jni.so` 触发 `UnsatisfiedLinkError`。
3. **`cdh_redirect` 映射 `/odm` 后缺少 `libnfc-nci-sz.conf`**：`libnfc-nci-snxxx.so` 的 `NfcConfig::loadConfig()` 依次在 `/odm/etc/`、`/vendor/etc/`、`/product/etc/` 查找 `libnfc-nci-sz.conf` 与 `libnfc-nxp-sz.conf`；`patch.img` 中 `/odm/etc/` 仅含 `build.prop`，致 `Check failed: config_path != ""` 中止。
4. **缺少 `vendor.honor.hardware.nxpnfc.INxpNfc/default` 及其 `ioctl` 协议回写**：`NfcAdaptation::InitializeAidlHalContext()` 在连接标准 `android.hardware.nfc.INfc/default` 后强检 `mHalNxpNfc != nullptr`（空则直接 `abort()`），随后 `NfcAdaptation::GetVendorConfigs()` 调用 `INxpNfc::ioctl(code=2, ioctlType=0xa7, inVec[0x1140])` 并在入参 `+0x00` 存入 `NfcAdaptation* context`、从出参 `nfc_nci_ExtnOutputData_t`（`0x1020` 字节）的 `+0x10` 读回该指针；通用空桩返回空响应导致空指针解引用 `SIGSEGV`。
5. **宿主虚拟 NFCC 对未知 `NFCEE_ID` 返回 `NCI_STATUS_SEMANTIC_ERROR (0x06)`**：`libnfc-nci-snxxx.so` 向预设 eSE/UICC `NFCEE_ID` 下发 `NFCEE_POWER_AND_LINK_CTRL_CMD (gid=02, oid=03)` 时，原 `nfcc.go` 返回 `0x06`，被 NXP 协议栈视为芯片状态致命错误并反复 `Aborting...`。

## 证据

- 失败现场日志：[log-fail.txt](log-fail.txt)（包含相机 `cameraSceneMode` 缺失、`applyTargets servicehost` 跳过 `normal_preview`、`SlowmotionUtil` NPE，以及 NFC `/dev/pn544` 缺失、`UnsatisfiedLinkError`、`config_path != ""`、`mHalNxpNfc is nullptr`、`HalIoctl` SEGV、`NCI_STATUS_SEMANTIC_ERROR` 原始堆栈）。
- 修复后通过日志：[log-pass.txt](log-pass.txt)（`Service nfc: found`、`mState=on`、`discovering=true`、`/api/nfc/tap` & `remove` 及相机 `ACTIVE` 预览/拍照/录像流状态）。
- 修复后实屏视觉验收截图：
  - 后摄拍照模式预览（`1440x1088`，满屏 4:3 超清蓝天白云流动出画）：`photo-preview-pass.png`
  - 快门拍照落盘（`3264x2448 JPEG`，左下角相册缩略图清晰显示实拍蓝天白云）：`photo-captured-pass.png`
  - 录像模式预览（`1920x1080`，`1080 · 30` 全屏无黑边出画）：`video-preview-pass.png`

## 改动

1. **相机 HAL（[camera_provider_service.cpp](../../../../devices/hardware/android.hardware.camera/src/camera_provider_service.cpp)）**：
   - 新增运行时品牌识别 `isHonorDevice()`；
   - 在未冲突区间 `0x8030xxxx..0x8031xxxx` 注册 19 项荣耀专有 Vendor Tags，包含精确类型的 `com.hihonor.capture.metadata.cameraSceneMode`（`TYPE_INT32`）等 17 项请求参数及 `com.hihonor.device.capabilities.honor_sit_supported` / `mainModeSupported`（`TYPE_BYTE`）；
   - 在静态元数据中为荣耀设备注入 `honor_sit_supported = 1`（进入 `isNewArch=true` 标准 Camera2 预览管线）与后摄 `mainModeSupported = 1`（注册主后摄 `"0"` 至 `b.f668q`），并同步加入 `ANDROID_REQUEST_AVAILABLE_CHARACTERISTICS_KEYS`；
   - 重编 `android16` `android.hardware.camera-service` 并刷入 `honor/chora` `patch.img`。
2. **内核模块 `cdh_device_info`（[nfc_sysfs.c](../../../../devices/modules/device_info/nfc/nfc_sysfs.c)）**：
   - 保留 `/sys/class/nfc/nfc_support`（三星 One UI）并新增只读杂项字符设备 `/dev/pn544`（AOSP `plat_file_contexts` 原生贴标 `u:object_r:nfc_device:s0`）与只读 sysfs `/sys/nfc/nfc_chip_type`（严格无尾换行输出 `"sn220u"`）；
   - 通过 Docker `6.12` 内核构建卷重编 `cdh_device_info.ko`，经 `cdh-pipeline verify-modules` 校验 76 个符号版本全部一致，刷入 `honor/chora` `patch.img` 的 `/vendor_dlkm/lib/modules/` 与 `/vendor/lib/modules/`。
3. **NFC 扩展 AIDL 协议兼容（[neuralnetworks_service.cpp](../../../../devices/hardware/android.hardware.neuralnetworks/src/neuralnetworks_service.cpp)）**：
   - 新增 `OnHonorNxpNfcTransact` 处理 `vendor.honor.hardware.nxpnfc.INxpNfc`（精确接口哈希 `b1e9a470c2e6879277d271b0fa9c327b876d341e`），其中 `ioctl (code=2)` 构造 `0x1020` 字节 `nfc_nci_ExtnOutputData_t` 并将入参 `+0x00` 处的 `NfcAdaptation*` 精确回写至出参 `+0x10`；重编 `android16` `android.hardware.neuralnetworks-service` 刷入 `patch.img`。
4. **NFC 策略与配置（[vendor_sepolicy.cil](../../../../devices/hardware/android.hardware.nfc/sepolicy/vendor_sepolicy.cil) 与 `honor/chora` `patch.img`）**：
   - 在 `android.hardware.nfc/sepolicy/vendor_sepolicy.cil` 放行 `nfc_{{vers}}` 读取 `sysfs_{{vers}}`；
   - 在 `honor/chora` `patch.img` 补齐 `/vendor/etc/permissions/com.hihonor.nfc.xml`、`/vendor/etc/libnfc-nci-sz.conf`、`/vendor/etc/libnfc-nxp-sz.conf`、`/odm/etc/libnfc-nci-sz.conf`、`/odm/etc/libnfc-nxp-sz.conf`，并在 `cdh_nn_placeholders.txt`、`cdh_vendor_placeholders.xml`、`vendor_sepolicy.cil` 注册 `INxpNfc/default` 与 `IHnNfc/default`。
5. **宿主虚拟 NFCC（[nfcc.go](../../../../agent/modules/daemon/backend/nfc/nfcc.go)）**：
   - 对未知 `NFCEE_ID` 的 `NFCEE_MODE_SET_CMD (gid=02, oid=01)` 与 `NFCEE_POWER_AND_LINK_CTRL_CMD (gid=02, oid=03)` 容错返回 `0x00 (NCI_STATUS_OK)`，避免 NXP SNxxx 协议栈因 `0x06 (NCI_STATUS_SEMANTIC_ERROR)` 触发 abort；单测全绿并重编重启 `host-daemon`。

## 验证

冷启 `honor/chora`（`chora_16-BQ2A.250913.001-BP2A.250605.031.A3-20260804`，`sys.boot_completed=1`）全链路验收通过：

1. **原生相机实屏与拍照/录像验收**：
   - 拍照模式：`Device status: ACTIVE`，`Stream[0] 1440x1088` 产出 **1872+ 帧**，实屏 `photo-preview-pass.png` 4:3 蓝天白云超清流动出画；
   - 快门拍照：`Stream[1] 3264x2448 JPEG` 产出 1 帧并落盘 `/sdcard/DCIM/Camera/IMG_20261123_140232_1.jpg`，`photo-captured-pass.png` 左下角相册缩略图清晰呈现；
   - 录像模式：切换 VIDEO 后自动重建 `Stream[0] 1920x1080`（`1080 · 30`）产出 86+ 帧，`video-preview-pass.png` 全屏无黑边出画，全程零崩溃。
2. **NFC 服务与虚拟刷卡验收**：
   - `service check nfc` 返回 `Service nfc: found`；
   - `dumpsys nfc` 稳态回报 `mState=on`、`mScreenState=ON_UNLOCKED`、`mTechMask: default`、`mEnableHostRouting: true`、`mIsReaderOptionEnabled=true`，全程 0 崩溃、0 重启；
   - `POST /api/nfc/tap` 与 `POST /api/nfc/remove` 均返回 `ok: true`，虚拟 NFCC 状态为 `connected: true, initialized: true, discovering: true`（`rxCommands: 38, txResponses: 48 -> 49`）。
3. **单测校验**：`cd agent && go test ./modules/daemon/backend/nfc/...` 通过（`ok 1.531s`）；`cdh-pipeline verify-modules` 通过（76 个符号一致）。

## 遗留

1. 本轮 `honor/chora` 的 `com.hihonor.nfc.xml`、`libnfc-nci-sz.conf`、`INxpNfc`/`IHnNfc` 占位与 SELinux 已先在 `patch.img` 验证生效，后续需在全仓流水线收尾阶段同步固化进 `09_build_vendor`、全代 `android13..17` HAL 产物与 6.6 等其余内核版本的 `cdh_device_info.ko` 重编。
2. 荣耀相机在 `tap` 移卡测试时宿主侧 `discovering=true` 正常，若需在荣耀桌面直接弹系统 NDEF 卡片弹窗，可后续在解锁前景下进一步核对 `mEnableReader` 开关状态。
