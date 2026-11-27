# #356 — OPPO Find X9s Pro（`oppo/PME110`, ColorOS 17 / Android 17, `mt6993`, kernel `6.12.69`）全维度对齐、NFC、纯蜂窝、蓝牙 A2DP 崩溃根治与原生相机真彩色预览/拍照/录像/回放闭环

- **设备/机型**：OPPO Find X9s Pro（`oppo/PME110`, `17-BP2A.250605.015-20260923`, GRF `vendor=A16, system=A17`, SoC `mt6993` / 天玑 9500, 内核 `6.12.69`, `lineage-23.2`）
- **状态**：已闭环（`Enforcing` / `restarting=0` / 蓝牙 `State: ON` 0 崩溃 / 纯蜂窝 `ccmni1` 0% 丢包 / NFC `mState=on` + `/api/nfc/tap` / 原生相机真彩色预览、前后摄拍照、1080p 录像与硬解回放 / `CDCM_PAUSE` 冻结 / 静置 `top` 97.0% 空闲）
- **参考前序**：[../../cph2797/issue-cph2797-mt6993-cellular-nxp-nfc-bt-camera-aps-audit/solution.md](../../cph2797/issue-cph2797-mt6993-cellular-nxp-nfc-bt-camera-aps-audit/solution.md)（`#355`）

---

## 一、问题

修前基线审计（`log-fail.txt`、`home-before.png`、`camera-before.png`）暴露出 8 类阻塞与体验问题：
1. **过压与充电上限弹窗**：桌面弹出「电压异常：充电电压过高，请更换充电器继续充电」，相机页弹出「建议将充电上限设为 80%」。
2. **规格错配与内存容量倒挂**：`ro.vendor.cdh.device.market_name` 误写 `"OPPO PME110"`；`battery_config.xml` 误将 `full_uah="7500"`（少三位数量级，真机 7500 mAh = `7500000` µAh）且同时置位 `ac="1" wireless="1"`、`charge_mv="4680"`；实例分配 12 GiB 物理内存却因旧版 `cdh_device_info.ko` 硬编码导致 `MemTotal: 7760724 kB` 小于 `MemAvailable: 8100092 kB`。
3. **误注入 `kernelsu.ko`**：`module/config.json` 与 `vendor_boot` ramdisk 残留 `"kernelsu"`，运行态 `lsmod` 加载 `kernelsu 204800`。
4. **崩溃重启循环**：`init.svc.oplusHmbirdBpfManager=restarting`（`restarting=1`），每 5 秒拉起 `/system_ext/bin/oplusHmbirdBpfManager` 均以 `status 1` 退出。
5. **纯蜂窝移动数据不通**：旧镜像 `ccmni2 -> ../../../virtio7`，关闭 Wi-Fi 后 `ping 223.5.5.5` 100% 丢包。
6. **NFC 缺失**：`dumpsys nfc` 报 `Can't find service: nfc`，`PME110.json` 与 `dist/.../device.json` 缺 `"host-hvc-nfc": "hvc11"`，缺 ODM NFC 权限声明、`libnfc-{nci,nxp}.conf`、`vendor.nxp.nxpnfc_pnaidl.INxpNfc/default` 占位及 SELinux 上下文与规则。
7. **Android 17 蓝牙开启即 `SIGABRT` 循环崩溃**：在蓝牙设置页打开蓝牙开关后 3 秒内连续崩溃（`Bluetooth crashed 2 times`，`State: OFF`）。
8. **原生相机 `OplusCamera`（v7.006.105）未打通**：缺解密后的 `odm/etc/camera/` 配置与 MTK 专有 20 流收口、缺应用沙箱自动注入脚本及 `opluscamera_app`/`mediaserver` fd 传递 SELinux 规则。

---

## 二、根因

1. **电池告警弹窗**：`vendor/etc/battery_config.xml` 同时声明 `ac="1" wireless="1" level="100" charge_mv="4680" full_uah="7500"`，4680 mV 超过 ColorOS 17 充电电压保护阈值触发过压告警，满电插电触发 80% 充电上限提示。
2. **`oplusHmbirdBpfManager` 重启风暴**：Android 17 `/system_ext/etc/init/oplusHmbirdBpfManager.rc` 在 `on boot` 置 `sys.oplus.hmbird.manager.enable=1`，并在 `boot_completed=1` 且内核 `6.12` 时启动依赖 OPPO 私有内核 `hmbird` BPF `sched_ext` 扩展的守护进程，通用 6.12 GKI 内核无该扩展致其每秒退出重启。
3. **Android 17 蓝牙 `FetchAudioProvider` 崩溃**：`vendor/etc/cdh_audio_placeholders.txt` 通过 `soundtrigger3-service.example` 注册了 `android.hardware.bluetooth.audio.IBluetoothAudioProviderFactory/default` 占位服务，其 `openProvider` 返回 `EX_UNSUPPORTED_OPERATION`。Android 16 收到后可降级，但 Android 17 `libbluetooth_jni.so` 只要检测到该 AIDL 服务存在于 `servicemanager`（`is_aidl_available() == true`），便强制重试 `openProvider(A2DP_SOFTWARE_ENCODING_DATAPATH)` 10 次后直接触发 `LOG(FATAL)` `SIGABRT`。移除该假占位注册即可令蓝牙栈走 `is_aidl_available=false` 平稳运行。
4. **`kernelsu.ko` 与 8 GiB 内存倒挂**：`module/config.json` 遗留 `"kernelsu"` 且 `vendor_boot` / `vendor_dlkm` 仍为旧产物（旧版 `cdh_device_info.ko` 硬编码 8 GiB `7760724 kB`）。
5. **纯蜂窝 / NFC / 原生相机**：旧 `vendor.img` 未接入新版 `cph-netmgr-setup`（`ccmni1`）；缺 `hvc11` 及 NXP SN220 NFC 配置与 SELinux；MTK `camera_unit_config` `usecase_info` 默认声明 20 条 MTK 专有 RAW/tuning/`rear_tele` 流且加密 RUS 配置无 `/odm/lib64/libOplusSecurity.so` 解密路径，外加 `mediaserver` 缺少使用 `opluscamera_app_data_file` 录像输出 fd 的 SELinux 授权。

---

## 三、证据

- **修前证据**：
  - [log-fail.txt](log-fail.txt)：`restarting=1`（`oplusHmbirdBpfManager`）、`lsmod` 含 `kernelsu`、`MemTotal=7760724 kB`、`ccmni2` 100% 丢包、`Can't find service: nfc`、蓝牙 `FetchAudioProvider: IBluetoothAudioProvidersFactory::openProvider(A2DP_SOFTWARE_ENCODING_DATAPATH) failed 10 times` `SIGABRT`。
  - [home-before.png](home-before.png)、[camera-before.png](camera-before.png)：顶部「电压异常：充电电压过高」与「建议充电上限设为 80%」弹窗。
- **修后实测与截图（全部经 `read_file` 人工审阅）**：
  - [log-pass.txt](log-pass.txt)：完整系统健康、12 GiB 内存、6 GiB ZRAM、无 `kernelsu`、7500 mAh 未插电电池、37 个传感器、蓝牙 `State: ON`（运行 `5m 47s`，`Bluetooth crashed 0 times`）、Wi-Fi `0% loss`、纯蜂窝 `ccmni1` `0% loss`（avg `0.993 ms`）、NFC `mState=on` + `/api/nfc/tap`（`rxCommands=65, txResponses=106`）、`c2.v4l2.avc.decoder [1920*1080] 21fps 0 discard`、`CDCM_PAUSE` 冻结（`487 -> 487`）、静置 `top` `776% / 800%`（`97.0% idle`）。
  - [home-pass.png](home-pass.png)：干净 ColorOS 17 桌面，无任何过压/充电上限弹窗。
  - [about-pass.png](about-pass.png)：ColorOS 17.0、`PME110_17.0.0.102(CN01)`、`29.5 GB / 256 GB`、状态栏 4G 满格与 85 未插电自然电量。
  - [camera-rear-preview.png](camera-rear-preview.png)：`OplusCamera` 后摄 1x 真彩色 SMPTE 彩条 + 动态绿框预览。
  - [rear-photo.jpg](rear-photo.jpg)（277 KB）与 [front-photo.jpg](front-photo.jpg)（88 KB）：后摄与前摄实拍真彩色 JPEG，色彩与方向准确。
  - [camera-video-recording.png](camera-video-recording.png)：`OplusCamera` 视频模式录像中（顶部红底计时 `00:03`、红色停止键、真彩色画面），实录 `VID20261127151006.mp4`（679 KB）。
  - [gallery-photo-pass.png](gallery-photo-pass.png)：ColorOS 17 原生相册展示实拍前后摄真彩色照片与 6 秒录像，状态栏同步呈现蓝牙 ON 图标 + 4G 满格 + Wi-Fi + 85 电量。
  - [video-playback-pass.png](video-playback-pass.png)：OPPO 视频经 `c2.v4l2.avc.decoder` 硬解流畅播放 `VID20261127151006.mp4`（1080p, 21 fps, 0 discard）。

---

## 四、改动

1. **`devices/manifest/devices/oppo/PME110.json`** & **`out/devices/dist/oppo/PME110/17-BP2A.250605.015-20260923/device.json`**：
   - 补齐 `"host-hvc-nfc": "hvc11"`。
2. **`devices/manifest/configs/oppo/PME110/PME110-android16/module/config.json`**：
   - 从 `external` 移除 `"kernelsu"`。
3. **`devices/manifest/configs/oppo/PME110/PME110-android16/vendor/config.json`**：
   - `ro.vendor.cdh.device.market_name` 修正为 `"OPPO Find X9s Pro"`；
   - 增补蓝牙 A2DP/LE Audio offload 禁用（含 `"ro.bluetooth.a2dp_offload.supported": "false"`）及 `persist.system.powerhal.applist_enable="0"`、`ro.vendor.magt.mtk_magt_support="0"`。
4. **`devices/manifest/configs/oppo/PME110/PME110-android16/vendor/etc/battery_config.xml`**：
   - 对齐真机 7500 mAh：`full_uah="7500000"`, `counter_min="6350000"`, `counter_max="6400000"`, `level="85"`, `capacity_level="HIGH"`, `charging_state="DISCHARGING"`, `ac="0"`, `wireless="0"`, `full_mv="4480"`, `charge_mv="4530"`。
5. **`devices/manifest/configs/oppo/PME110/PME110-android16/vendor/etc/cdh_audio_placeholders.txt`**：
   - 清空 `android.hardware.bluetooth.audio.IBluetoothAudioProviderFactory/default` 假占位符，根治 Android 17 蓝牙 `FetchAudioProvider` 10 次失败 `SIGABRT`。
6. **NFC 配置与占位**：
   - `vendor/etc/cdh_nn_placeholders.txt` 补齐 `vendor.nxp.nxpnfc_pnaidl.INxpNfc/default`（保留原 100 项防止 `system_server` 阻塞于 `IEngineer/default`）；
   - 新增 `vendor/etc/{libnfc-nci.conf,libnfc-nxp.conf}`、`vendor/sepolicy/vendor_service_contexts`、`odm/etc/permissions/{com.oplus.nfc_feature.xml,oplus_nfc_default_grant_permissions_list.xml}`。
7. **`vendor/sepolicy/vendor_sepolicy_rules.cil`**：
   - 补齐 NXP/Oplus NFC AIDL 占位、 `opluscamera_app` 沙箱读写、`cameraserver` `setsched` 以及 `mediaserver`/`mediaextractor`/`mediaswcodec` 使用 `opluscamera_app` 录像输出 fd 规则。
8. **`vendor/etc/init/cph_pme110_setup.rc`** & **`vendor/bin/cph_pme110_setup.sh`**：
   - `post-fs-data` 创建 `/data/vendor/nfc`、`/data/nfc`；
   - 监听 `sys.oplus.hmbird.manager.enable=1` 与 `sys.boot_completed=1` 置 `0` 并 `stop oplusHmbirdBpfManager`；
   - 将解密并收口后的 `odm/etc/camera/config/*` 写入 `com.oplus.camera` 沙箱并修正属主与 SELinux 上下文。
9. **`odm/etc/camera/`**：
   - 提取官方 `odm_a.img:/etc/camera/` 并经 `libOplusSecurity.so` + `dec_sec` 解密 5 份加密配置；对 `oplus_camera_config` 包装 `file_version=1005.0` 并关闭 HDR/10bit RAW/日志加密；对 `oplus_camera_algo_switch_config` 置 `aps_mode=1` 且禁用 37 项 `aps_capture_configs` 实体；对 `camera_unit_config` / `third_camera_unit_config` 清理全部 MTK 专有 RAW/tuning/`rear_tele` 流并补齐标准 `preview`+`capture`/`video_recorder` 双流。
10. **重新构建与原位更新**：
    - 执行 `build_vendor_boot`（剔除 ramdisk `kernelsu.ko` 并生成 `fstab.mt6993`，GPT 原位更新 `dist/.../vda.img` 的 `vendor_boot_a`）、`07_build_odm`、`09_build_vendor`、`10_build_vendor_dlkm`（刷新新版 12 GiB 档 `cdh_device_info.ko` 且 0 基座 `.ko`、无 `kernelsu.ko`）、`11_build_patch`（最终 `patch.img` `sha256: 0e6be9a4c72b4c2d4b6c2fb7eb58b547f1f3aa273d73c84b3e22877a9659b30c`）。

---

## 五、验证

| 验收维度 | 实测结果 | 结论 |
| :--- | :--- | :--- |
| SELinux & 服务健康 | `getenforce=Enforcing`，`init.svc.*` `restarting=0`（`oplusHmbirdBpfManager=stopped`），无 `kernelsu` | PASS |
| 规格/内存/ZRAM/电池 | `OPPO Find X9s Pro` / `MT6993` / `1272x2772` / `MemTotal=11649276 kB` (12 GiB 档) / `zram0=6291452 kB` (6 GiB) / 7500 mAh 未插电 85% `4481 mV` / 37 传感器 | PASS |
| 静置 CPU 负载 | 退桌面静置 `top`: `776% / 800%`（**97.0% idle**），无日志风暴 | PASS |
| 蓝牙 | `State: ON`，持续运行 `5m 47s`，`Bluetooth crashed 0 times` | PASS |
| Wi-Fi & 纯蜂窝 | Wi-Fi `0% loss`；关闭 Wi-Fi 后纯蜂窝 `ccmni1`（`100.64.155.15/24`）`ping -c 5 223.5.5.5` **`0% packet loss`**（avg `0.993 ms`） | PASS |
| NFC | `dumpsys nfc` `mState=on`，`/api/nfc/tap` 返回 `initialized=true, discovering=true, cardPresent=true, rxCommands=65, txResponses=106` | PASS |
| 原生相机预览/拍照/录像/回放 | 后摄真彩色预览、后摄拍照 277 KB (`rear-photo.jpg`)、前摄拍照 88 KB (`front-photo.jpg`)、`VIDEO` 录像 679 KB (`VID20261127151006.mp4`)、原生相册查图、`c2.v4l2.avc.decoder [1920*1080] 21fps 0 discard` 硬解回放 | PASS |
| 宿主按需冻结 | 退桌面后 `CDCM_PAUSE` 冻结宿主推帧：`framesSent 487 -> 487`（间隔 4s 0 增量） | PASS |

---

## 六、遗留

1. 前摄 `Device 1` 实拍照片（`front-photo.jpg`, 88 KB）为清晰真彩色 SMPTE 彩条，但切前摄瞬间预览界面在 `PHOTO` 模式下因 `front_single_photo_case` 使用 `var_camera` 存在首帧送显偏暗/黑屏现象；拍照落盘与双流均正常，后续可按需细化前摄 `var_camera` 预览 surface 路由。
2. 新发现的 **Android 17 蓝牙 `FetchAudioProvider` 假占位崩溃规律**（清空 `cdh_audio_placeholders.txt`）可直接复用于后续所有 Android 17 镜像（如 `#44 xiaomi/popsicle` A17）。
