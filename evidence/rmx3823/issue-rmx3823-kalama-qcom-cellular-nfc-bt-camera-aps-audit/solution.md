# #357 realme GT5 240W（`realme/RMX3823`）真机规格对齐、高通 GRF 纯蜂窝/NXP NFC/蓝牙补齐与 `OplusCamera` 拍录全链路打通

- **设备 / 机型**：realme GT5 240W（`realme/RMX3823` / `RE5C33`，realme UI 6.0 / ColorOS 15 / `15-TP1A.220905.001-20250912` / 骁龙 8 Gen 2 `SM8550-AB` / GRF `vendor=Android 13, system=Android 15`, `lineage-20.0`, kernel `5.15.167`）
- **日期**：2026-11-27
- **状态**：已验证闭环

---

## 1. 问题

修前基线审计（见同目录 `log-fail.txt`、`home-before.png`、`camera-before.png`）发现以下 8 类缺陷：

1. **真机规格失真与充电伪态**：官方固件确认商业名为 `真我GT5 240W`（`realme GT5 240W`），真机电池为 **4600 mAh**，支持 `12GB+256GB` / `16GB+512GB` / `24GB+1TB`；但原 `vendor/config.json` 误写 `"realme GT5 (150W)"`，`RMX3823.json` 缺少 `24GB+1TB` 顶配 SKU，`battery_config.xml` 误配 `full_uah="5240"`（单位错 1000 倍）、`level="100"`、`ac="1"` 导致状态栏常亮绿色充电图标。
2. **高通 GRF 纯蜂窝无信号**：状态栏蜂窝信号打叉（`OUT_OF_SERVICE`），关 Wi-Fi 后 `ping 223.5.5.5` 报 `connect: Network is unreachable`（无 `ccmni1` 数据承载）。
3. **缺少 6 GiB ZRAM**：`/proc/swaps` 为空，`SwapTotal: 0 kB`。
4. **内存倒挂与误注入 `kernelsu.ko`**：旧版 5.15 `cdh_device_info.ko` 硬编码 8 GiB 导致 `MemTotal: 7760724 kB < MemAvailable: 8285176 kB`；`module/config.json` 与 `vendor_dlkm` 误注入 `kernelsu.ko`。
5. **NFC 缺失**：`dumpsys nfc` 报 `Can't find service: nfc`，缺少 `"host-hvc-nfc": "hvc11"`、ODM NFC 权限与配置、`vendor/etc/libnfc-{nci,nxp}.conf`、`vendor.nxp.nxpnfc_{aidl,pnaidl}.INxpNfc/default` 占位、`vendor_service_contexts` 与 `vendor_sepolicy_rules.cil`。
6. **`workloadclassifier` 崩溃循环弹窗**：`com.qualcomm.qti.workloadclassifier` 反复在 `WLCReceiver.onReceive` 抛 `FATAL EXCEPTION`，触发 `Showing crash dialog` 与 `Re-adding persistent process`。
7. **缺少 `android.hardware.soundtrigger3`**：切换 `board/fstab=qcom` 后音频/语音触发框架需要 `android.hardware.soundtrigger3` 避免启动异常。
8. **原生相机 `OplusCamera` 黑屏**：打开 `com.oplus.camera` 画面全黑（`camera-before.png`），缺少解密后的 `odm/etc/camera/config/` 明文配置与开机沙盒写入脚本。

---

## 2. 根因

1. **高通 GRF `board=kalama` + `IWlanService.apk` 拦截**：`RMX3823.json` 设 `board="kalama"`、`vendor_boot/config.json` 设 `fstab="kalama"`、rEFInd EFI 引导参数写 `androidboot.hardware=kalama`，且 `/vendor/app/IWlanService/IWlanService.apk` 未屏蔽，导致高通 RIL/数据通道与 `fstab.qcom`（含 6 GiB `zram0` 配置）全部失配。
2. **rEFInd EFI 引导底盘（`lineage-20.0`）**：`vda.img` 无独立 `vendor_boot_a` 分区，内核与 `vendor_boot` ramdisk 合并存放于 FAT32 `EFI` 分区的 `/kernel` + `/combined-ramdisk.img`，内核启动参数固化在 `/EFI/BOOT/refind.conf`；仅替换 `patch.img` 不会生效新的 `androidboot.hardware=qcom`、`fstab.qcom` 与 `cdh_redirect.rules`。
3. **NXP NFC 与 SELinux 缺项**：`RMX3823` 为高通平台 NXP NFC 架构，缺少 `hvc11` 通道、`odm/etc/permissions/*nfc*`、`vendor/etc/libnfc-{nci,nxp}.conf` 与 `hal_oplus_nfc_service` 绑定导致 `NfcService` 无法启动。
4. **`OplusCamera` 加密配置未落盘**：官方 `odm/etc/camera/config/oplus_camera_config` 经 `libOplusSecurity.so` 加密（其余 10 项为明文），且 `camera_unit_config` 声明 5 颗摄像头 `["0","1","2","3","4"]`，未收口到云机虚拟相机 ID 集并写入沙盒路径。

---

## 3. 证据

同目录归档完整证据文件（所有实屏截图与实拍原图均已亲眼审阅）：

- 修前基线：`log-fail.txt`、`home-before.png`（蜂窝打叉 + 充电图标）、`camera-before.png`（黑屏）
- 修后日志：`log-pass.txt`（合并核心健康、内存/ZRAM、电池、蓝牙、Wi-Fi、纯蜂窝 `ccmni1`、NFC `tap`、相机拍录、`media.metrics` 解码指标、`CDCM_PAUSE` 与静置 `top`）
- 修后实屏截图与原图：
  - `home-pass.png`：干净桌面，状态栏显示蓝牙开启、4G 满格双箭头移动数据、85% 未插电自然电量
  - `about-pass.png`：关于本机正确显示 `realme 真我GT5 240W`、`realme UI 6.0`、`35.8 GB / 256 GB`、`高通八核`、`4600 mAh (典型值)`、`12.0 GB`、`6.74 英寸`、`RMX3823`
  - `camera-rear-preview.png`：原生相机后摄真彩色实时预览
  - `camera-video-recording.png`：原生相机 VIDEO 模式录制 `00:03` 实时真彩色画面
  - `rear-photo.jpg`（276 KB）与 `front-photo.jpg`（112 KB）：后摄与前摄实拍清晰真彩色原图
  - `gallery-photo-pass.png`：相册大图清晰真彩色查看实拍照片
  - `video-playback-pass.png`：相册流畅播放 `00:02/00:07` 1080p H.264 录像

---

## 4. 改动

### 4.1 机型与发布清单
1. `devices/manifest/devices/realme/RMX3823.json` 与 `out/devices/dist/realme/RMX3823/15-TP1A.220905.001-20250912/device.json`：
   - `board` 改为 `"qcom"`，补 `"host-hvc-nfc": "hvc11"`；
   - 补齐真机 240W SKU：`memory: [12288, 16384, 24576]`、`storage: [256000, 512000, 1024000]`、`skuCombinations: ["12GB+256GB","16GB+512GB","24GB+1TB"]`。

### 4.2 `devices/manifest/configs/realme/RMX3823/RMX3823-android13/`
1. `vendor_boot/config.json`：`fstab` 改为 `"qcom"`。
2. `hardware/config.json`：追加 `android.hardware.soundtrigger3` (`variant: "default", version: "1.0"`)。
3. `module/config.json`：`external` 移除 `"kernelsu"`；`redirect` 追加 `/vendor/app -> /mnt/vendorpatch/vendor/app` 与 `/system_ext/app/workloadclassifier -> /mnt/vendorpatch/system_ext/app/workloadclassifier`。
4. `odm/config.json` & `vendor/config.json`：补 `"ro.oplus.wifi.wsa": "none"`；`ro.vendor.cdh.device.market_name` 修正为 `"realme GT5 240W"`；补 `"media.c2.hal.selection": "aidl"`、`"vendor.audio.c2.preferred": "true"`、`"vendor.audio.hw.aac.encoder": "false"`。
5. `vendor/etc/battery_config.xml`：修正为真机 4600 mAh 未插电自然电量（`full_uah="4600000"`, `counter_min="3880000"`, `counter_max="3930000"`, `level="85"`, `capacity_level="HIGH"`, `charging_state="DISCHARGING"`, `ac="0"`, `full_mv="4480"`, `charge_mv="4530"`）。
6. **NXP NFC 全套**：
   - `vendor/etc/cdh_nn_placeholders.txt` 补 `vendor.nxp.nxpnfc_aidl.INxpNfc/default` 与 `vendor.nxp.nxpnfc_pnaidl.INxpNfc/default`；
   - 新增 `vendor/sepolicy/vendor_service_contexts` 并在 `vendor/sepolicy/vendor_sepolicy_rules.cil` 放行 `nfc` 域查找调用 `hal_oplus_nfc_service` / `hal_neuralnetworks_default`；
   - 新增 `vendor/etc/{libnfc-nci.conf,libnfc-nxp.conf}`、`odm/etc/libnfc-nci.conf`、`odm/etc/nfc/{libnfc-mtp-SN100.conf_22635,libnfc-nci.conf_22635,libnfc-mtp-SN220.conf_23603,libnfc-nci.conf_23603}`、`odm/etc/permissions/{android.hardware.nfc.uicc.xml,com.oplus.nfc_feature.xml,oplus_nfc_default_grant_permissions_list.xml}`。
7. **原生相机解密配置与开机初始化**：
   - 从官方 `odm_a.img` 提取 `CameraHWConfiguration.config` 及 `odm/etc/camera/config/*`（剔除 5.1 MB `sr_params.xml`），经 `RMX3823` `libOplusSecurity.so` + `dec_sec` 解密 `oplus_camera_config`（34735 B 明文 JSON），收口 `camera_unit_config` 的 `camera_id_list` 为 `["0","1","2","3"]`；
   - 新增 `vendor/etc/init/cph_rmx3823_setup.rc` 与 `vendor/bin/cph_rmx3823_setup.sh`（创建 `/data/vendor/nfc`、`/data/nfc`，禁用 `workloadclassifier` / `vendor.qti.iwlan`，写入解密后的相机配置到 `com.oplus.camera` 沙盒）。

### 4.3 增量重打与原位更新
- 顺序增量构建 `build_boot` → `build_vendor_boot` → `build_odm` → `build_vendor` → `build_vendor_dlkm` → `build_patch`；
- 将新 `boot.img` + `vendor_boot.img` 重新合成 `/kernel` + `/combined-ramdisk.img`，并将 `EFI/BOOT/refind.conf` 的 `androidboot.hardware=kalama` 原位更新为 `androidboot.hardware=qcom` 写入 `vda.img` 的 EFI 分区；
- 发布新 `patch.img`（`sha256: 9c99e24aa2423bce1b528c0abdb437480df0ceb5b2fdbdc7918dd63e0ff6b505`）。

---

## 5. 验证

在全新冷启 `RMX3823_15-TP1A.220905.001-20250912` 实例 1（`12GB+256GB`, `adb: 127.0.0.1:5911`）上 82 秒完成冷启，全维度验收全部通过：

| 验收项 | 实测结果 |
|---|---|
| 系统健康 | `getenforce=Enforcing`、`restarting_count=0`、无 `kernelsu`、`workloadclassifier`/`iwlan` `NONE_ENABLED`、0 崩溃、静置 `top` `796%idle / 800%cpu`（**99.5% 空闲**） |
| 规格与内存/ZRAM | `1240x2772`、`MemTotal=11658380 kB`（12 GiB 真机档，无倒挂）、`SwapTotal=6291452 kB`（6 GiB `/dev/block/zram0`） |
| 虚拟电池 | `charge_full_design=4600000`（4600 mAh）、`AC/USB/Wireless=false`、`level=85`、`voltage=4479 mV`、`Charge counter=3922917`、状态栏无充电闪电 |
| 蓝牙 | `enabled: true, state: ON, name: 真我GT5 240W, time since enabled: 00:04:56.010, Bluetooth crashed 0 times` |
| Wi-Fi | `Wi-Fi is enabled`，扫描到 8 个 2.4GHz/5GHz AP |
| 纯蜂窝（Wi-Fi OFF） | `IN_SERVICE (LTE, 中国联通)`、`mDataConnectionState=2`、`ccmni1 (100.64.155.15/24) ping 223.5.5.5` **0% packet loss, avg 0.226 ms** |
| NFC | `mState=on, mScreenState=ON_UNLOCKED`，`POST /api/nfc/tap` 返回 `ok: true, initialized: true, discovering: true, cardUid: "04112233445566", rxCommands: 67, txResponses: 85` |
| 相机预览与实拍 | 后摄真彩色预览正常；后摄实拍 `IMG20261127163405.jpg`（276 KB）与前摄实拍 `IMG20261127163415.jpg`（112 KB）均为清晰真彩色；相册查图清晰 |
| 录像与解码回放 | VIDEO 模式录制 `VID20261127163426.mp4`（H.264 `1920x1080`, `7.14s`, 882 KB，`c2.android.avc.encoder` 218 帧 ≈ 30.6 fps）；相册流畅播放 `00:02/00:07`（`c2.android.avc.decoder` 1920x1080 0 崩溃） |
| 后台休眠冻结 | 退桌面触发 `cdh_camera_video: camera session idle, sent CDCM_PAUSE on /dev/hvc12`，宿主 `framesSent` 严格冻结 `884 -> 884`（0 丢帧） |

---

## 6. 遗留

1. `5.15` 内核 `mac80211_hwsim` 跨机通用行为同 [../../pje110/issue-pje110-kalama-qcom-cellular-nfc-bt-camera-aps-audit/](../../pje110/issue-pje110-kalama-qcom-cellular-nfc-bt-camera-aps-audit/)；
2. Android 13 GRF 镜像下录像与回放由系统默认调度 `c2.android.avc.{encoder,decoder}`，实测 1080p30 满帧流畅且整机空闲率 99.5%。
