# #358 realme 15T（`realme/RMX5112`）真机规格/7000mAh 电池对齐、A13 `radio@mtk` 纯蜂窝、蓝牙崩溃根治、NFC 与原生相机全链路打通

- **设备 / 机型**：realme 15T（`realme/RMX5112` / `RE6404L1`，realme UI 6.0 / ColorOS 15.0.2 / `15-TP1A.220905.001-20251025` / 联发科天玑 6400 Max `MT6835` / GRF `vendor=Android 13, system=Android 15`, `lineage-20.0`, kernel `5.15.167`）
- **日期**：2026-11-27
- **状态**：已验证闭环

---

## 1. 问题

修前基线审计（见同目录 `log-fail.txt`、`home-before.png`、`camera-before.png`）发现以下 7 类缺陷：

1. **蓝牙启用即 `SIGABRT` 崩溃**：执行 `cmd bluetooth_manager enable` 后 `libbluetooth_jni.so` 在 `vendor::mediatek::bluetooth::audio::aidl::BluetoothAudioClientInterface::FetchAudioProvider()` 触发 `LOG(FATAL)` `SIGABRT`（`Bluetooth crashed 1 time`, `state: OFF`）。
2. **纯蜂窝移动数据 `ccmni1` 100% 丢包**：关 Wi-Fi 后虽然 `ccmni1` 获配 `100.64.155.15/24`，但 `ping -c 3 223.5.5.5` 报 **100% packet loss**；`ip link show` 显示 `ccmni1` 为假 `NOARP/qdisc noqueue` 接口，真正的 `virtio-net-pci (c2:36:b4:12:34:58)` 被错绑在处于 `DOWN` 状态的 `ccmni2`。
3. **电池容量错配千倍与「建议充电上限设为 80%」弹窗**：真机为 **7000 mAh** 泰坦电池，原 `battery_config.xml` 误配 `full_uah="5000"`、`level="100"`、`ac="1"`，导致开机持续弹出「建议充电上限设为 80%」大弹窗且状态栏常亮绿色充电图标。
4. **真机规格与 SKU 残缺**：`RMX5112.json` 缺 `12GB` 与 `512GB` SKU 及 `refresh-rates: [60, 90, 120]`，`vendor/config.json` 缺 `ro.vendor.cdh.device.market_name="realme 15T"`。
5. **误注入 `kernelsu.ko` 与 ZRAM 未激活**：`lsmod` 显示 `kernelsu 233472 0`，且 `/proc/swaps` 为空（`zram` 模块已载入但未 `swapon`）。
6. **NFC 缺失及回退崩溃**：`RMX5112.json` 缺少 `"host-hvc-nfc": "hvc11"`，缺少 ODM NFC 权限/配置与 `vendor.nxp.nxpnfc_{aidl,pnaidl}.INxpNfc/default` 占位、`vendor/etc/libnfc-nxp.conf`、`vendor_service_contexts` 与 `vendor_sepolicy_rules.cil`；首次启用时 `NfcNci.apk` 在 `libsn100nfc_nci.so` 的 `SN100HalHelper::halGetProperty` 处空指针 `SIGSEGV`。
7. **原生相机加密配置未落盘**：官方 `odm_a.img` 下 `oplus_camera_algo_switch_config`、`oplus_camera_aps_config`、`oplus_camera_config`、`oplus_preview_decision_params.json` 4 个文件经 `libOplusSecurity.so` 加密，缺少明文配置与沙盒落盘脚本。

---

## 2. 根因

1. **MTK 蓝牙音频 AIDL 假占位触发 `LOG(FATAL)`**：`vendor/etc/cdh_nn_placeholders.txt` 注册了 `vendor.mediatek.hardware.bluetooth.audio.IBluetoothAudioProviderFactory/default`，`vendor/etc/cdh_audio_placeholders.txt` 注册了 `android.hardware.bluetooth.audio.IBluetoothAudioProviderFactory/default`，返回不支持时 MTK 蓝牙栈 `FetchAudioProvider()` 直接 `abort`。
2. **Android 13 `android.hardware.radio@mtk` 预编译二进制过期**：`out/devices/vendor/android13/mtk/bin/hw/android.hardware.radio-service` 仍为 2026-11-22 旧产物，尚未包含最新双 PDN 承载网口修正（将真 `virtio-net-pci` 绑定至 `ccmni1`）。
3. **MTK `fstab.enableswap` 未被 init 调用**：官方 `vendor` 将 `zram0` 写在独立 `fstab.enableswap` 中，云机合并 `fstab.mt6835` 后需在开机 setup 脚本显式激活 6 GiB `zram0`。
4. **欧加多芯片 `NfcNci.apk` 回退 SN100 栈**：虽官方 ODM 声明 `com.tms.nfc`（THN31），但无物理芯片探测时 `NfcChipManager` 回退实例化 `NxpNfcService`（`libsn100nfc_nci.so`），若缺少 `vendor.nxp.nxpnfc_{aidl,pnaidl}.INxpNfc/default` 占位、`hal_oplus_nfc_service` 绑定与 `/vendor/etc/libnfc-nxp.conf` 便空指针解引用。

---

## 3. 证据

同目录归档完整证据文件（所有实屏截图与实拍原图均已亲眼审阅）：

- 修前基线：`log-fail.txt`、`home-before.png`（80% 充电建议弹窗 + 100% 绿色充电图标）、`camera-before.png`
- 修后日志：`log-pass.txt`（合并核心健康、ZRAM、7000 mAh 电池、蓝牙 `ON`、Wi-Fi 扫描、纯蜂窝 `ccmni1`、NFC `mState=on` + `/api/nfc/tap`、相机拍录、`media.metrics`、`CDCM_PAUSE` 与静置 `top`）
- 修后实屏截图与原图：
  - `home-pass.png`：干净无弹窗的 realme UI 6.0 桌面
  - `about-pass.png`：关于本机（状态栏蓝牙 + 4G 满格 + 85% 未插电电量，正文 `realme RMX5112`、`realme UI 6.0`、`15.3 GB / 128 GB`、`联发科天玑6400 Max`）
  - `camera-rear-preview.png` & `camera-front-preview.png`：原生相机后摄与前摄真彩色实时预览
  - `camera-video-recording.png`：VIDEO 模式录制 `00:03` 实时真彩色画面
  - `rear-photo.jpg`（286 KB）& `front-photo.jpg`（112 KB）：后摄与前摄实拍清晰真彩色原图
  - `gallery-photo-pass.png` & `video-playback-pass.png`：相册大图查图与 1080p 录像真彩色播放界面

---

## 4. 改动

### 4.1 公共 HAL 刷新（全仓 Android 13 MTK 受益）
- 失效旧版 `out/devices/vendor/android13/mtk/bin/hw/android.hardware.radio-service`，触发 `halcore` 按最新 `devices/hardware/android.hardware.radio` 源码重编（将真实 `virtio-net-pci` 正确绑定到 `ccmni1`）。

### 4.2 `realme/RMX5112` 机型与配置改动
1. `devices/manifest/devices/realme/RMX5112.json` 与 `out/devices/dist/realme/RMX5112/15-TP1A.220905.001-20251025/device.json`：
   - 补 `"host-hvc-nfc": "hvc11"`、`refresh-rates: [60, 90, 120]`；
   - 补齐真机 SKU：`memory: [8192, 12288]`、`storage: [128000, 256000, 512000]`、`skuCombinations: ["8GB+128GB","8GB+256GB","12GB+256GB","12GB+512GB"]`。
2. `devices/manifest/configs/realme/RMX5112/RMX5112-android13/`：
   - `module/config.json`：移除 `"kernelsu"`，补 `"display": {"refresh-rates": [60, 90, 120]}`；
   - `vendor/config.json`：设 `"ro.vendor.cdh.device.market_name": "realme 15T"`、`"ro.bluetooth.a2dp_offload.supported": "false"`、`"media.c2.hal.selection": "aidl"`、`"vendor.audio.c2.preferred": "true"`、`"vendor.audio.hw.aac.encoder": "false"` 与刷新率属性；
   - `vendor/etc/battery_config.xml`：改为真机 **7000 mAh** 未插电自然电量（`full_uah="7000000"`, `counter_min="5920000"`, `counter_max="5980000"`, `level="85"`, `ac="0"`, `charging_state="DISCHARGING"`）；
   - `vendor/etc/cdh_nn_placeholders.txt` & `cdh_audio_placeholders.txt`：移除 MTK 蓝牙音频假占位、清空 `cdh_audio_placeholders.txt`；补入 `vendor.nxp.nxpnfc_{aidl,pnaidl}.INxpNfc/default`；
   - NFC 全套：新增 `vendor/sepolicy/vendor_service_contexts` 并在 `vendor_sepolicy_rules.cil` 放行 `nfc` 域查找调用 `hal_oplus_nfc_service` / `hal_neuralnetworks_default`；放入 `vendor/etc/{libnfc-nci.conf,libnfc-nxp.conf,libnfc-tms.conf}` 与官方 `odm/etc/permissions/*nfc*`、`odm/etc/nfc/*25604*`；
   - 原生相机与初始化：经 `RMX5112` `libOplusSecurity.so` + `dec_sec` 解密全部 4 个加密相机配置文件写入 `odm/etc/camera/config/`（剔除 `sr_params.xml` 与 `sr_ext_res.bin`），新增 `vendor/etc/init/cph_rmx5112_setup.rc` 与 `vendor/bin/cph_rmx5112_setup.sh`（建 NFC 数据目录、激活 6 GiB `zram0`、写入解密相机配置至沙盒）。
3. 增量重建 `build_boot → build_vendor_boot → build_odm → build_vendor → build_vendor_dlkm → build_patch`，原位更新 `vda.img` EFI（`/kernel` + `/combined-ramdisk.img`）并发布 `patch.img`（`sha256: f0ea5521e145ecdf40b264a106b175ca213a89cde2c7213f43458266dc1b23a6`）。

---

## 5. 验证

全新冷启 `RMX5112_15-TP1A.220905.001-20251025` 实例 1（`8GB+128GB`, `adb: 127.0.0.1:5911`）56 秒完成冷启，全维度验收全部通过：

| 验收项 | 实测结果 |
|---|---|
| 系统健康 | `getenforce=Enforcing`、`restarting_count=0`、无 `kernelsu`、0 崩溃、静置 `top` `792%idle / 800%cpu`（**99.0% 空闲**） |
| 内存与 ZRAM | `MemTotal=7760724 kB`（8 GiB 档）、`SwapTotal=6291452 kB`（6 GiB `/dev/block/zram0` 已激活） |
| 虚拟电池 | `charge_full_design=7000000`（7000 mAh）、`AC/USB/Wireless=false`、`level=85`、`voltage=4480 mV`、无 80% 充电建议弹窗 |
| 蓝牙 | `enabled: true, state: ON, name: RMX5112, time since enabled: 00:03:18.328, Bluetooth crashed 0 times` |
| Wi-Fi | `Wi-Fi is enabled`，扫描到 8 个 2.4GHz/5GHz AP |
| 纯蜂窝（Wi-Fi OFF） | `ccmni1 (100.64.155.15/24, state UP, pfifo_fast)` `ping 223.5.5.5` **0% packet loss, avg 0.660 ms** |
| NFC | `mState=on, mScreenState=ON_UNLOCKED`，`POST /api/nfc/tap` 返回 `ok: true, initialized: true, discovering: true, cardUid: "04112233445566", rxCommands: 53, txResponses: 72` |
| 相机预览与实拍 | 后摄与前摄实时预览均为清晰真彩色；后摄实拍 `IMG20261127170709.jpg`（286 KB）与前摄实拍 `IMG20261127170718.jpg`（112 KB）均为清晰真彩色；相册查图清晰 |
| 录像与解码回放 | VIDEO 模式录制 `VID20261127170730.mp4`（H.264 `1920x1080`, `7.10s`, 1.47 MB，`c2.android.avc.encoder`）；相册流畅加载与解码（`c2.android.avc.decoder` 1920x1080 0 崩溃） |
| 后台休眠冻结 | 退桌面后宿主 `framesSent` 严格冻结 `1052 -> 1052`（0 丢帧） |

---

## 6. 遗留

1. `5.15` 内核 `mac80211_hwsim` 跨机通用行为同 [../../pje110/issue-pje110-kalama-qcom-cellular-nfc-bt-camera-aps-audit/](../../pje110/issue-pje110-kalama-qcom-cellular-nfc-bt-camera-aps-audit/)；
2. 此次同步重编的 Android 13 `android.hardware.radio@mtk`（`ccmni1` 绑定修正）已写入公共产物目录 `out/devices/vendor/android13/mtk/`，后续 Android 13 MTK 机型可直接复用。
