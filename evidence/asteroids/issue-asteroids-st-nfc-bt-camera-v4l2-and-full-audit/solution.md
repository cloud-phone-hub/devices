# Nothing Phone (3a) (`nothing/Asteroids` A15 & A16) 恢复 ST NFC 硬件、根治蓝牙 LE Audio 与相机快门闪退及全维度闭环验收

- **设备/机型**：Nothing Phone (3a)（`nothing/Asteroids`，Qualcomm Snapdragon 7s Gen 3 `SM7635 / volcano`，GRF `vendor=14`，双系统版本：`15-UKQ1.241011.001-20251013` Nothing OS 3.2 与 `16-UKQ1.250612.001-20260810` Nothing OS 4.1，`lineage-21.0` + `kernel 6.1`）
- **编号**：`#351`

## 一、问题

对镜像库 `#19` (`Asteroids_15-UKQ1.241011.001-20251013`) 与 `#20` (`Asteroids_16-UKQ1.250612.001-20260810`) 执行修前体检时，发现 6 项功能缺失与异常：

1. **真机物理具备的 NFC 硬件在云机中完全丢失**：`pm list features` 无 `android.hardware.nfc`，`svc nfc enable` 报 `Got a null NfcAdapter`，`/api/nfc/status` `initialized=false, rxCommands=0`。
2. **开启蓝牙后 `com.android.bluetooth` 每 5 秒触发 `SIGABRT` 循环崩溃**：`LeAudioClient::Initialize` 因无 Bluetooth Audio Offload HAL V2.1+ 断言退出。
3. **Nothing 原生相机 `com.nothing.camera` 按快门直接闪退**：抛出 `java.lang.IllegalArgumentException: Could not find tag for key 'com.nothing.camera.insensorzoom.enable'`（及 `AWBFrameControlCCT`、`ae_bracket.mode`、`jpeg.flip`），无法拍照。
4. **Codec2 空桩 `rank="1"` 抢占与跨代属性缺失**：`Asteroids-android14/vendor/etc/media_config.xml` 中 `c2.qti.*` 空桩全部配置为 `rank="1"`，且 GRF 跨代缺 `media.c2.hal.selection=aidl` 与 `vendor.audio.c2.preferred=true`。
5. **硬件规格与虚拟节点未生效**：`12GB+256GB` SKU 启动后仅识别 `8GB` RAM（`MemTotal=7760724 kB`）、`SwapFree=0 kB`、缺失 `/sys/class/power_supply/battery/charge_full_design`（5000mAh 电池）与 `ro.vendor.cdh.device.market_name`。
6. **首启引导遮挡主桌面**：`com.android.setupwizard.overlay` / `com.nothing.introduction*` 遮挡 `com.nothing.launcher`，且 `cph_nothing_setup.rc` 若含 `$` shell 变量会触发 `init` `ExpandProps` 失败。

## 二、根因

1. **`09_build_vendor/assemble.go` 遗留早期无 `/dev/hvc11` 时代的 `officialSystemHasStNfc` 强删逻辑**：
   - 在宿主侧尚未实现 `/dev/hvc11` 虚拟 NCI 2.0 NFCC 前，`nfc_service.cpp` 只能返回 `OPEN_CPLT(FAILED)`，引发 `Nfc_st.apk` (`libstnfc_nci_jni.so`) 在销毁 GKI 内存池后调用 `GKI_getbuf` 崩溃，当时权宜增加了 `officialSystemHasStNfc(plan)` 强行剥离所有携带 `Nfc_st.apk` 机型的 `android.hardware.nfc*.xml`。
   - 现宿主侧虚拟 NCI 2.0 NFCC 已完整支持标准 NCI 2.0 及 `0x0F` Vendor-Specific 指令返回 `STATUS_OK`（`OPEN_CPLT(OK)`），该过时拦截反而误删了 Nothing Phone (3a) 等 ST NFC 真机的 NFC 硬件声明。
2. **旧盘集未合入最新 `camera@nothing`、`daemon` 与 `6.1` 内核模块，且 `cdh_nothing_vendor_tags.inc` 缺 3 项快门捕获标签**：
   - 旧盘集打包于 11 月 19 日，尚未编入 180 项 Nothing Vendor Tags（且快门路径额外查询 `org.quic.camera2.statsconfigs.AWBFrameControlCCT`、`org.codeaurora.qcamera3.ae_bracket.mode`、`com.nothing.available.jpeg.flip`），也未合入 `cph-zram-setup` 蓝牙 LE Audio 关闭逻辑及新版 `cdh_device_info.ko`。

## 三、证据

- 修前故障日志见 [log-fail.txt](log-fail.txt)，修前遮挡界面见 `a15-before.png`：
  - `NO_NFC_FEATURE` / `Got a null NfcAdapter`；
  - `com.android.bluetooth` `LeAudioClient::Initialize` `SIGABRT`；
  - `Process: com.nothing.camera` -> `IllegalArgumentException: Could not find tag for key 'com.nothing.camera.insensorzoom.enable'`；
  - `MemTotal: 7760724 kB`、`SwapFree: 0 kB`、无 `battery/charge_full_design`。
- 修后双版本全量实测通过日志见 [log-pass.txt](log-pass.txt)，实屏截图见 `a15-home-clean.png`、`a16-home-clean.png`、`a16-about.png`、`a15-camera-rear.png`、`a16-camera-rear.png`、`a15-playback.png`、`a16-playback.png`。

## 四、改动

1. **引擎级解除 ST NFC 特性误删并补单测**：
   - `agent/modules/pipeline/stages/09_build_vendor/assemble.go`：移除过时的 `officialSystemHasStNfc` 拦截，仅在未启用 `android.hardware.nfc` 模块时剥离 NFC 权限；启用时统一由 `ensureEnabledNfcPermissions` 保留/合成 `android.hardware.nfc{,.hce,.hcef}.xml`；
   - `agent/modules/pipeline/stages/09_build_vendor/camera_nfc_compat_test.go`：新增 `TestNfcPermissionsRetainedWhenEnabledAndPrunedWhenDisabled` 单测。
2. **补齐 Nothing 相机快门 Vendor Tags**：
   - `devices/hardware/android.hardware.camera/src/nothing/cdh_nothing_vendor_tags.inc`：新增 `org.quic.camera2.statsconfigs.AWBFrameControlCCT`、`org.codeaurora.qcamera3.ae_bracket.mode`、`com.nothing.available.jpeg.flip`（共 183 项），重编 `android14` `camera@nothing`。
3. **`apply-profile` 自愈对齐与首启脚本修正**：
   - 执行 `pipeline device apply-profile nothing/Asteroids`：将 `Asteroids-android14/vendor/etc/media_config.xml` 空桩自愈调整为 `rank=512/1024`，升级 `media_codecs_c2.xml`，注入 `vendor.audio.c2.preferred=true`、`vendor.audio.hw.aac.encoder=false` 与 `media.c2.hal.selection=aidl`；
   - `Asteroids-android14/vendor/etc/init/cph_nothing_setup.rc`：去除 `$` 变量循环改为平铺命令并加入 `com.android.setupwizard.overlay`，消除 `init` `ExpandProps` 中止，冷启动自动直达 `com.nothing.launcher`；
   - 全链重打 A15 与 A16 双版本盘集并同步 `rom-main`。

## 五、验证

在 `#19` A15 (`127.0.0.1:5911`) 与 `#20` A16 (`127.0.0.1:5919`) 双实例（`12GB+256GB` SKU）上并行实测全通：

1. **冷启动、零崩溃与静置性能**：
   - A15 `32.2s`、A16 `59.2s~63.5s` 冷启动完成，`getenforce=Enforcing`，`restarting=0`；双版本 `logcat -d -b crash` 均为 **0 行**，自动直达 `com.nothing.launcher` 主桌面；
   - 静置 CPU 空闲率达 **`783%~791% idle / 800% cpu`**；`Nothing Launcher` 连续滑动 A15 `P50=6ms / P90=12ms / P95=16ms`、A16 `P50=5ms / P90=11ms / P95=16ms / Janky=2.79%`。
2. **真机设备信息对齐**：
   - 「关于本机」实屏展示 `Nothing Phone (3a)`、`Qualcomm® Snapdragon 7s Gen 3`、`12GB` 内存、`256 GB` 存储；
   - `MemTotal=11662204 kB`、`SwapFree=6291452 kB` (6GiB ZRAM)、`battery/charge_full_design=5000000` µAh、48 档 `cpufreq`、21 项硬件传感器全部在线。
3. **蓝牙 / Wi-Fi / Radio / NFC**：
   - **蓝牙**：双版本 `state: ON`、`name: Nothing Phone (3a)`、`Bluetooth crashed 0 times`；
   - **Wi-Fi & Radio**：5GHz Wi-Fi 与关闭 Wi-Fi 后的双 PDN (`ccmni0`+`ccmni1`) 纯蜂窝移动数据 `ping 223.5.5.5` 均为 **`0% packet loss`**；
   - **NFC（补回缺失硬件）**：A15（含 `Nfc_st.apk` 栈）与 A16 均具备完整 `android.hardware.nfc{,.hce,.hcef}` 特性，`/api/nfc/tap` 返回 `connected=true, initialized=true, discovering=true, cardPresent=true`（`rxCommands=22/27, txResponses=34/41`），零崩溃发现 `android.nfc.tech.Ndef` 标签。
4. **相机与 V4L2 硬件编解码**：
   - `com.nothing.camera` 前后摄实时彩色预览正常，快门拍照零缺失 Tag 报错，双版本各落盘 2 张 JPEG（`29KB` / `21KB`）及 1 段 1080p H.264+AAC `.mp4`（A15 `574KB 5.66s`、A16 `594KB 5.15s`）；
   - 录制采用 **`c2.v4l2.avc.encoder`** 硬件编码（`1920x1080`），Google 相册生成彩色视频缩略图并通过 **`c2.v4l2.avc.decoder`** 硬件解码正常播放；退回桌面后宿主机 `framesSent` 恒定冻结在 `210`（`CDCM_PAUSE` 休眠生效）。

## 六、遗留

无。双实例已停机回收，临时解包与构建中间目录已清理。
