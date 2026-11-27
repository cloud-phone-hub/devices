# #353 OnePlus 15R (`oneplus/CPH2767`) 真机规格对齐、NFC/蜂窝/电池补齐与欧加 `FullApsImpl` 原生相机全链路打通

- **设备/机型**：OnePlus 15R（`oneplus/CPH2767` / `CPH2769_16.0.10.500_EX01_A51_full.zip` / SM8750 `canoe` / Android 16，lineage-23.2 + kernel 6.12.81）
- **状态**：已闭环（2026-11-27）
- **产物指纹**：`out/devices/dist/oneplus/CPH2767/16-BP2A.250605.015-20260810/patch.img`（`sha256: 1af714ea7e2b207e62d7441739920dc2dae36cd16e814003dfc93dcd8da29197`）

## 一、问题

在对 `oneplus/CPH2767`（`16-BP2A.250605.015-20260810`）进行全维度审计时，发现以下多类缺陷（详见证据 [log-fail.txt](log-fail.txt)）：

1. **规格与电池缺失**：内存与电池节点未完整对齐真机（7400 mAh `/sys/class/power_supply/battery` 缺失、`odm/etc/power_profile/power_profile.xml` 缺失），蓝牙初启停留在 `BLE_TURNING_ON` 未就绪。
2. **NFC 被长期屏蔽**：历史条目 [issue-nfc-crash-loop](../issue-nfc-crash-loop/solution.md) / [issue-nfc-oplus-hidl-missing](../issue-nfc-oplus-hidl-missing/solution.md) 遮蔽了 `/system/priv-app/NfcNci`，导致真机具备的 NXP SN220 NFC 无法使用。
3. **原生相机 `OplusCamera` 无法完成端到端拍照与录像**：
   - 真彩色 `allocator` 修复前存在灰度预览风险；
   - 原生 `OplusCamera` 通过 `libAPSClient-cmd-jni.so` 加载 `libAlgoProcess.so` 走 `FullApsImpl`（`aps_mode=1`），缺失 `libAlgoProcess.so` 与 `vendor/etc/public.libraries.txt` 暴露项，且 `/odm/lib64/libAlgoProcess.so` 被 `cdh_redirect` 拦截策略影响；
   - 接入 `libAlgoProcess.so` 初版后仍存在四道硬卡点：
     1. `previewdecision_sensormask="1,1,0,0"` 让 `BaseMode.createRequestTag()` 误判为双拍照流（`mCaptureStreamNumber=2`），物理单流永远无法满足 `CaptureImageCategory.isValid()`，`addFrameBuff` 从不触发；
     2. `previewdecision_thumbnail_index="0"` 与 `CaptureProcessor.java` 的 1-based `mCaptureStartedCallbackNum==1` 不匹配，跳过缩略图抓取与 MediaStore URI 创建，报 `ThumbnailCategory is invalid! thumbnail.identity: 0`；
     3. `jpeg_callback_hardware_buf=true` 时未分配 `AHARDWAREBUFFER_FORMAT_BLOB` 写入 `hwBuffer_HARDWAREBUFFER_OBJECT_`、未修正 `exifData_*` 键名与旋转角解析，且在 `transactCb(3)` 后误调用 `AHardwareBuffer_release(jpegAhb)` 与 Java `HardwareBuffer.close()` 双重释放导致拍照 30 ms 后 `SIGSEGV`；
     4. 切换 `VIDEO` 模式时 `AIEnhancementVideoPresenter` 访问 `com.qti.chi.statsaec.AecLux` 触发 `IllegalArgumentException: Could not find tag for key 'com.qti.chi.statsaec.AecLux'` 崩溃。

## 二、根因

1. **硬件与系统层**：`hostHvc` 与 NFC 服务端未贯通 NXP SN220 / `IOplusNfc` / `INfcExtns` 全链，且 `07_build_odm` 未自动回填原厂 `power_profile.xml`；`09_build_vendor` 在基底缺少 `vendor/etc/public.libraries.txt` 时跳过创建导致 `libAlgoProcess.so` 无法被 `sphal` 加载。
2. **欧加 APS 引擎与相机 HAL 协议**：
   - `OplusCamera` `FullApsImpl` 强依赖 `previewdecision_sensormask` 仅有 1 个主摄置 `1`（`"0,1,0,0"`）、`previewdecision_thumbnail_index="1"`、`previewdecision_master_pipeline="1"`、`previewdecision_request_format_list="35"`；
   - 新版 ColorOS 16 `OplusCamera` 启用 `jpeg_callback_hardware_buf=true`，要求原生层将编码后的 JPEG 字节封装为 `AHARDWAREBUFFER_FORMAT_BLOB` 并将其所有权通过 `hwBuffer_HARDWAREBUFFER_OBJECT_` 移交 Java `HardwareBuffer`（不得由原生层再次 `AHardwareBuffer_release`）；
   - `camera@oplus` 变体原先仅注册少量 vendor tag，缺少 `OplusCamera` 拍照/录像路径查询的 `CaptureResult`/`CaptureRequest` vendor tag（含 `com.qti.chi.statsaec.AecLux`），而直接注册静态 `CameraCharacteristics` 键又会破坏 `CameraCharacteristicsWrapper` 回退逻辑，因此必须精准注册仅限 `CaptureResult`/`CaptureRequest` 的 vendor tag 集合。

## 三、证据

- **修前审计基线**：[log-fail.txt](log-fail.txt)（无 `battery/charge_full_design`、蓝牙停在 `BLE_TURNING_ON`、NFC 屏蔽、原厂相机无法拍照落盘）。
- **修后全维度终验日志**：[log-pass.txt](log-pass.txt)。
- **实屏截图证据（均已人工审阅）**：
  - 干净桌面：[home-pass.png](home-pass.png)（1272×2800，蓝牙/Wi-Fi/85% 电池与图标清晰正常）
  - 关于本机：[about-pass.png](about-pass.png)（`OnePlus 15R` / `Snapdragon 8 Gen 5` / `7400 mAh (TYP)` / `12.0 GB` / `256 GB`）
  - 原生相机后摄真彩色预览：[camera-rear-preview.png](camera-rear-preview.png)
  - 原生相机前摄真彩色预览：[camera-front-preview.png](camera-front-preview.png)
  - 相册全屏查验实拍真彩色照片：[gallery-photo-pass.png](gallery-photo-pass.png)
  - 原生相机 VIDEO 模式 1080p30 实时录像：[camera-video-recording.png](camera-video-recording.png)
  - 视频 `c2.v4l2.avc.decoder` 硬解回放：[video-playback-pass.png](video-playback-pass.png)

## 四、改动

1. **内核模块与构建流水线通用加固**：
   - `devices/modules/cdh_redirect/`：新增 `CDH_CARRIER_ALGO_PROCESS_64`，仅放行 `linker64` 加载 `/odm/lib64/libAlgoProcess.so` 与 `/vendor/lib64/libAlgoProcess.so`（已刷新 6.12 与 5.15 内核模块）。
   - `agent/modules/pipeline/stages/09_build_vendor/files_compat.go`：`ensureVendorPublicLibraryIfPresent()` 缺失 `vendor/etc/public.libraries.txt` 时自动创建并写入 `libAlgoProcess.so`。
   - `agent/modules/pipeline/stages/07_build_odm/`：自动回填 `odm/etc/power_profile/power_profile.xml`。
2. **欧加通用 APS 引擎 `libAlgoProcess.so`（全代际 A13~A17 通用）**：
   - `devices/hardware/android.compat.libs/src/algo_process_oplus.cpp`：
     - `previewdecision_sensormask="0,1,0,0"`、`previewdecision_thumbnail_index="1"`、`previewdecision_master_pipeline="1"`、`previewdecision_request_format_list="35"`；
     - `handleProcessImages` 移除 `picture_exif_flag` 误转旋转角逻辑，透传 `picture_title` / `capture_job_type` / `defer_job_group_id` / `pipeline`；
     - 响应 `jpeg_callback_hardware_buf=true` 分配 `AHARDWAREBUFFER_FORMAT_BLOB` 写入 JPEG 与完整 `exifData_*` 键值并将句柄所有权移交 Java `HardwareBuffer`（避免双重释放 SIGSEGV），同时保留 `bufferPlanesData_BYTE_BASIC_ARRAY_` 兼容旧版欧加机型；
     - 全量编译落盘 `out/devices/vendor/android{13..17}/lib64/libAlgoProcess.so`。
3. **欧加相机 HAL vendor tag 补齐（全代际 A13~A17 通用）**：
   - `devices/hardware/android.hardware.camera/src/oplus/cdh_oplus_vendor_tags.inc`：补齐 373 个 Oplus/QTI/MTK `CaptureResult`/`CaptureRequest` vendor tag（严格剔除 `CameraCharacteristics` 键并与 base `camera_vendor_tags.inc` 849 项零冲突，包含 `com.qti.chi.statsaec.AecLux` `TYPE_FLOAT`），全量重编 `android13`~`android17` 的 `android.hardware.camera@oplus`。
4. **CPH2767 机型清单与硬件补齐**：
   - 对齐 `12GB+256GB`、7400 mAh 虚拟电池 + `ICharger`、6 GiB ZRAM、QTI 双 PDN Radio、`host-hvc-nfc` + NXP NFC HAL + `IOplusNfc`/`INfcExtns`、解除 `NfcNci` 屏蔽、配置 `odm/etc/camera/` `aps_mode=1` 与 `QPerformance` 零报错，重打 `odm.img` / `vendor.img` / `patch.img`。

## 五、验证

在冷启动 `CPH2767_16-BP2A.250605.015-20260810` 实例 `1`（`12GB+256GB`，`patch.img sha256: 1af714ea7e2b`）上完成全维度实测：

- **系统与安全健康**：`getenforce=Enforcing`、`sys.boot_completed=1`、`init.svc.*` `restarting=0`、分辨率 `1272x2800 @ 560dpi`、`Mem: 11376M / Swap: 6143M`、静置 `top` `746% idle / 800% cpu`（93.3% 空闲）。
- **蓝牙**：`enabled: true`、`state: ON`、`name: OnePlus 15R`、`Bluetooth crashed 0 times`。
- **Wi-Fi 外网**：`ping -c 3 223.5.5.5` → `0% packet loss`，`rtt avg = 3.056 ms`。
- **纯蜂窝移动数据（关闭 Wi-Fi）**：`ccmni1 (100.64.155.15/24)` `ping -c 3 223.5.5.5` → `0% packet loss`，`rtt avg = 0.342 ms`。
- **NFC 全链路**：`dumpsys nfc` `mState=on`；`POST /api/nfc/tap?spec=CPH2767_16-BP2A.250605.015-20260810&index=1` 返回 `ok: true, cardUid: "04112233445566", cardPayload: "cloud-phone-hub-nfc-ok", rxCommands: 34, txResponses: 46`。
- **原生相机 `OplusCamera`（`FullApsImpl`, 0 崩溃）**：
  - 后摄/前摄实时预览 100% 真彩色；
  - 连续拍照成功落盘 `/sdcard/DCIM/Camera/IMG20261127111453.jpg`（275 KB）、`IMG20261127111500.jpg`（275 KB）、`IMG20261127111505.jpg`（279 KB，4096×3072）及前摄 `IMG20261127110123.jpg`（112 KB），相册全屏审阅真彩色正向清晰；
  - 切 `VIDEO` 模式 0 崩溃，录 5 秒生成 `/sdcard/DCIM/Camera/VID20261127111549.mp4`（1.07 MB）；
  - 视频硬解回放实证 `c2.v4l2.avc.decoder [1920*1080] inputFps=28, outputFps=28, renderFps=28, discardFps=0`；
  - 退回桌面 `CDCM_PAUSE` 严格冻结：`T0 framesSent=326 → T1 (+4s) framesSent=326`。

## 六、遗留

- `devices/hardware/android.compat.libs/src/algo_process_oplus.cpp` 与 `devices/hardware/android.hardware.camera/src/oplus/cdh_oplus_vendor_tags.inc` 已同步编出 `android13`~`android17` 全代际产物，后续欧加机型（`PJE110`、`CPH2797`、`PME110`、`RMX3823`、`RMX5112`、`RMX6688`）可直接复用。
- 内核 `6.6` 的 `cdh_redirect.ko` 待轮到 `RMX6688` 时按实际匹配卷刷新。
