# #354 OnePlus Ace 3（`oneplus/PJE110`）真机规格对齐、高通纯蜂窝/NXP NFC/蓝牙补齐与 GRF 跨代 `OplusCamera` 预览拍录全链路打通

- **设备 / 机型**：OnePlus Ace 3（`oneplus/PJE110`，ColorOS 15.0 / `15-TP1A.220905.001-20250926` / 骁龙 8 Gen 2 `SM8550` / GRF `vendor=Android 13, system=Android 15`, `lineage-20.0`, kernel `5.15.167`）
- **日期**：2026-11-27
- **最终产物**：`out/devices/dist/oneplus/PJE110/15-TP1A.220905.001-20250926/patch.img`（`sha256: 0a44022ecaadb6675822264d52fba4d599ebf33b63833b85001b42cf89c8073d`）

---

## 一、问题

对 `#23 oneplus/PJE110` 进行修前全维度审计（见 [log-fail.txt](log-fail.txt) 与 [home-before.png](home-before.png)、[camera-before.png](camera-before.png)）发现 6 类核心缺陷：

1. **真机规格失真**：`PJE110.json` 分辨率误配 `1080x2400`、SKU 误配 `8GB+128GB/256GB`、电池容量未对齐真机 `5500 mAh`（`5500000 µAh`）；
2. **蜂窝移动数据完全不通**：先是 `Phone Id=0` 永久 `OUT_OF_SERVICE`，修复后仍出现 `DPM-0: NO_SUITABLE_DATA_PROFILE` 无法建立 `ccmni1` 蜂窝互联网连接；
3. **NFC 缺失且启动即崩溃**：缺少 `"host-hvc-nfc": "hvc11"` 与 `com.oplus.nfc_feature.xml`，导致 `NfcBootCompletedReceiver` 自动禁用 `com.android.nfc`；补齐特性后 `libsn100nfc_nci.so` 在 `SN100HalHelper::halGetProperty` 触发 `SIGSEGV (fault addr 0x0)`；
4. **`build_vendor` 漏拷 `libAlgoProcess.so`**：`09_build_vendor` 的 `ensureCompatLibs` 缓存目标表漏列 `lib64/libAlgoProcess.so`，导致欧加机型在命中旧 compat 缓存时缺失 `vendor/lib64/libAlgoProcess.so`；
5. **`OplusCamera` 启动后配置会话失败黑屏（第一层）**：`CameraCharacteristics.getAvailableSessionKeys()` 抛 `IllegalArgumentException: Could not find tag for key 'com.mediatek.control.capture.early.notification.support'`，触发 `PREVIEW_CONFIG_FAILED (10002)`；
6. **`OplusCamera` GRF 跨代 `configureStreams` 被 Android 15 `CameraService` 拒绝（第二层）**：`AidlCamera3-Device: configureStreams: Stream 0: INPUT streams must have 0 for producer usage`，返回 `-38 (ENOSYS)`，导致相机反复建会话失败。

---

## 二、根因

1. **ColorOS 15 高通蜂窝双因**：
   - `/vendor/app` 未纳入 `cdh_redirect`，导致原厂 `IWlanService.apk` 穿透启动，`NRM-I-0` 永不回调 `requestNetworkRegistrationInfo`，把 `SST[0] mPollingContext` 卡死在 `1`；
   - `board=kalama` 使 `ro.boot.hardware=kalama`，ColorOS 15 `android.os.Build.isQcomPlatform()` 严格检查 `ro.boot.hardware.startsWith("qcom")`，误判为 MTK 平台并在 `DataProfileManager.java:193` 调用 `arrayList2.clear()` 清空全部 `3gnet` APN；改为 `board=qcom` 后又触发 `SoundTriggerModule` 缺失 `android.hardware.soundtrigger3` 抛 `NoSuchElementException` 卡死开机。
2. **NXP SN220 NFC 崩溃**：缺少 `vendor.nxp.nxpnfc_aidl.INxpNfc/default` 与 `vendor.nxp.nxpnfc_pnaidl.INxpNfc/default` 占位及 `vendor_service_contexts` 绑定至 `hal_oplus_nfc_service`（`u:r:nfc:s0` 被 SELinux 拒绝 `find default_android_service`），使 `SN100HalHelper` 拿不到服务句柄空指针解引用。
3. **Vendor Tag 最长前缀劫持**：`devices/hardware/android.hardware.camera/src/oplus/cdh_oplus_vendor_tags.inc` 将 10 个已在 `camera_vendor_tags.inc` 注册于 `com.mediatek.control.capture` 下的多段键拆成了更长的子 section（如 `com.mediatek.control.capture.early.notification`），而 Android `VendorTagDescriptor::lookupTag` 采用最长前缀匹配，把同前缀的 `early.notification.support` 劫持到新子 section 下导致查无此键。
4. **Camera3 AIDL `StreamType::INPUT` Usage 规范违背**：`camera_provider_service.cpp` 的 `configureStreamsLocked` 对所有流无条件写入 `hal.producerUsage = AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN`、`consumerUsage = 0`；当 `OplusCamera` 在 ZSL/Reprocess 会话中配置 `StreamType::INPUT`（Stream 0）时，Android 15 `AidlCamera3-Device` 按规范拒绝非零 `producerUsage` 并返回 `-ENOSYS (-38)`。

---

## 三、证据

- 修前基线日志：[log-fail.txt](log-fail.txt)
- 修前桌面/相机截图：[home-before.png](home-before.png)、[camera-before.png](camera-before.png)
- 终版全维度通过日志：[log-pass.txt](log-pass.txt)
- 实屏审阅证据：
  - 干净桌面（1264×2780，4G/蓝牙/85% 电量）：[home-pass.png](home-pass.png)
  - 关于本机（一加 Ace 3 / ColorOS 15.0 / 5500 mAh / 12.0 GB / 6.78 英寸 / PJE110）：[about-pass.png](about-pass.png)
  - 原生相机后摄真彩色实时预览：[camera-rear-preview.png](camera-rear-preview.png)
  - 原生相机前摄真彩色实时预览：[camera-front-preview.png](camera-front-preview.png)
  - 原生相机后摄/前摄实拍原图：[photo-rear.jpg](photo-rear.jpg)（252 KB）、[photo-front.jpg](photo-front.jpg)（110 KB）
  - 相册查看实拍照片与录像缩略图（含 GNSS 位置标签）：[gallery-photo-pass.png](gallery-photo-pass.png)
  - 原生相机 VIDEO 1080p30 录像中实屏：[camera-video-recording.png](camera-video-recording.png)
  - 相册直接解码回放实录 `VID20261127121901.mp4`（30fps/0 丢帧）：[video-playback-pass.png](video-playback-pass.png)

---

## 四、改动

### 4.1 跨机型公共底座改动（全仓 A13~A17 生效）
1. `agent/modules/pipeline/stages/09_build_vendor/hal.go`：`ensureCompatLibs` 的 `targets` 补入 `lib64/libAlgoProcess.so` 并重编 `pipeline`。
2. `devices/hardware/android.hardware.camera/src/oplus/cdh_oplus_vendor_tags.inc`：消除 10 条重复 full key 与子 section 最长前缀劫持，收口为 363 条零冲突 vendor tag（总计 840 条 / 60 个 section）。
3. `devices/hardware/android.hardware.camera/src/camera_provider_service.cpp`：
   - `configureStreamsLocked` 区分 `StreamType::INPUT`（`producerUsage = 0`, `consumerUsage = AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN`）与 `OUTPUT`；
   - `processCaptureRequest` 补齐 `request.inputBuffer.streamId != -1` 时的 `result.inputBuffer` 正常回传；
   - 全量重编 Android 13~17 `android.hardware.camera`（`default` 与 `oplus` 全部 10 个产物）及 Android 13 `android.hardware.soundtrigger3`。

### 4.2 `oneplus/PJE110` 机型配置改动
1. `devices/manifest/devices/oneplus/PJE110.json`：分辨率对齐 `1264x2780 @ 560dpi`、`70x155mm`、`memory: [12288, 16384]`、`storage: [256000, 512000, 1024000]`、`skuCombinations: ["12GB+256GB","16GB+512GB","16GB+1TB"]`、`board: "qcom"`、补 `"host-hvc-nfc": "hvc11"`。
2. `devices/manifest/configs/oneplus/PJE110/PJE110-android13/`：
   - `vendor_boot/config.json`：`fstab: "qcom"`；
   - `hardware/config.json`：加入 `android.hardware.soundtrigger3`；
   - `module/config.json`：触控轴对齐 `20224x44480`、`refresh-rates: [60,90,120]`、redirect 屏蔽 `/vendor/app`（清除 `IWlanService.apk`）与 `/system_ext/app/workloadclassifier`；
   - `odm/config.json` & `vendor/config.json`：补 `ro.oplus.wifi.wsa=none`、`ro.soc.model=SM8550`、`Adreno (TM) 740`、`OnePlus Ace 3`、`media.c2.hal.selection=aidl`；
   - `vendor/etc/battery_config.xml`：修正为真机 `5500000` µAh（5500 mAh）、`level=85`、`ac=0`；
   - NXP SN220 NFC 全套：`odm/etc/permissions/{com.oplus.nfc_feature.xml,android.hardware.nfc.uicc.xml,oplus_nfc_default_grant_permissions_list.xml}`、`odm/etc/nfc/*`、`vendor/etc/{libnfc-nci.conf,libnfc-nxp.conf}`、`vendor/etc/cdh_nn_placeholders.txt`、`vendor/sepolicy/{vendor_service_contexts,vendor_sepolicy_rules.cil}`；
   - 原生相机解密配置：通过 `dec_sec` 解密官方 `odm/etc/camera/config/*`（13 个明文配置，去除 5.9M `sr_params.xml`，收口 `camera_id_list`）并配置 `cph_pje110_setup.{rc,sh}`。

---

## 五、验证

在冷启全新构建的 `PJE110_15-TP1A.220905.001-20250926`（`12GB+256GB`，`patch.img sha256: 0a44022ecaad`）上全项实证通过（详见 [log-pass.txt](log-pass.txt)）：

| 维度 | 实测结果 |
| :--- | :--- |
| 规格对齐 | `PJE110` / `SM8550` / `qcom` / `1264x2780 @ 560dpi` / `12GB+256GB` / `6143M ZRAM` / `5500 mAh` / `level=85` |
| 系统健康 | `getenforce=Enforcing`、`restarting_count=0`、0 崩溃循环、静置 `top` `768% idle / 800% cpu`（**96.0% 空闲**） |
| 蓝牙 | `enabled: true, state: ON, name: 一加 Ace 3, Bluetooth crashed 0 times` |
| 纯蜂窝（Wi-Fi OFF） | `Phone Id=0 IN_SERVICE (LTE, 中国联通)`, `mDataConnectionState=2`, `ccmni1 (100.64.155.15/24) ping 223.5.5.5` **0% loss, avg 0.253 ms** |
| NFC | `mState=on, mScreenState=ON_UNLOCKED`, `POST /api/nfc/tap` 返回 `ok: true, initialized: true, discovering: true, cardUid: "04112233445566", rxCommands: 48, txResponses: 60` |
| 原生相机预览与拍照 | 后摄/前摄真彩色实时预览均清晰通过；实拍 `IMG20261127121843.jpg`（252 KB）与 `IMG20261127121850.jpg`（110 KB）原图及相册查看均为清晰真彩色 |
| 录像与解码回放 | VIDEO 模式录制 `VID20261127121901.mp4`（1.3 MB, 6s）0 崩溃；相册直接播放 `1920x1080 inputFps=30 outputFps=30 renderFps=30, discardFps=0` |
| 后台摄像头休眠 | 退回桌面触发 `cdh_camera_video: camera session idle, sent CDCM_PAUSE on /dev/hvc12`，宿主 `framesSent` 冻结在 `729 -> 729` |

---

## 六、遗留

1. `5.15` 内核 `mac80211_hwsim` 在随机扫到的 5GHz AP 广播 1 Mbps 基础速率时会被 `wpa_supplicant` 跳过（跨 5.15 核通用行为，连接 2.4GHz 开放 AP 或已配 SSID 可直通外网）；
2. Android 13 GRF 镜像在录像与播放时由系统默认优先调度 `c2.android.avc.{encoder,decoder}`，实测 1080p30 满帧 0 丢帧、CPU 余量充足；
3. 本条验证的 ColorOS 15 高通 GRF（`board/fstab=qcom` + `soundtrigger3` + 屏蔽 `/vendor/app` + NXP NFC + `camera@oplus` `StreamType::INPUT` 修复）可直接复用于 `#26 realme/RMX3823`。
