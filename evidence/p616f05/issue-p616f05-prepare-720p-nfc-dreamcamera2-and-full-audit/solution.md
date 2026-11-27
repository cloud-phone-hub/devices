# #352 ZTE Blade V70（`nubia/P616F05`）`prepare` `720×1600` 真机规格对齐、NFC 补齐、`DreamCamera2` + 64 位 `cdh-mediaserver` 相机录像打通与后台重启/ZRAM 卡顿根治

- **设备 / 机型**：ZTE Blade V70（`nubia/P616F05`，型号 `Z2459`，版本 `14-TP1A.220624.014-20260324`，Unisoc UMS9230T，GRF `system=A14` + `vendor=A13`，配置 `P616F05-android13`，`lineage-20.0` + `kernel 5.15`）
- **日期**：2026-11-27
- **状态**：已闭环

---

## 一、问题

存量镜像 `#21 nubia/P616F05`（`P616F05_14-TP1A.220624.014-20260324`）冷启动体检暴露八项功能缺失、规格失真与后台性能异常：

1. **分辨率与机型规格错配**：`P616F05.json` 误写为 `1080×2400`、`model="ZTE Blade V70 (Z2459)"` 且缺 `skuCombinations`；真机为 6.7″ `720×1600` IPS LCD（`320dpi`）、`4GB+128GB / 8GB+256GB`、5000mAh 电池；`ro.boot.ddrsize` 与 `ro.vendor.cdh.device.market_name` 均为空。
2. **真机具备 NFC 但云机漏配 HAL 致 `init` 高占 CPU**：官方 `system`/`vendor` 明确携带 `feature:android.hardware.nfc{,.hce,.hcef}`、`NfcNci.apk` 与 `/vendor/etc/libnfc-nxp.conf`，但 `hardware/config.json` 漏配 `android.hardware.nfc`，导致 `com.android.nfc` 死等 `INfc/default`、`init` 占 `52%` CPU。
3. **原生相机 `DreamCamera2` 无法使用且 `media.player` 缺失**：`hardware/config.json` 误配 `camera@nubia`（高通努比亚变体）而非展锐原生 `/system_ext/app/DreamCamera2/DreamCamera2.apk` 所需的 `camera@unisoc`；且 32 位 `/system/bin/mediaserver` 未重定向至 64 位 `cdh-mediaserver`（`Service media.player: not found`）。
4. **Radio 脱网（`OUT_OF_SERVICE`）与 Bluetooth `OFF`**：缺 `gsm.version.ril-impl=sprd-ril` 且旧 `android13` `radio@unisoc` / `bluetooth` 未合入 `/dev/hvc4` 启动竞态修复（参见 `g1130` `#336` [../../g1130/issue-g1130-boot-radio-ddrsize-camera-and-prepare-align/solution.md](../../g1130/issue-g1130-boot-radio-ddrsize-camera-and-prepare-align/solution.md)）。
5. **ZRAM 耗尽触发 `lmkd` 杀后台与虚拟电池缺失**：旧盘集未刷入最新 `5.15` ZRAM 模块链导致 `SwapFree: 0 kB`、`lowmemorykiller: swap is fatal` 刷屏；`battery_config.xml` 误写 `full_uah="5000"`（应为 `5000000` µAh）且无 `charge_full_design` 节点。
6. **展锐专有守护进程循环重启与 SELinux AVC 风暴**：`[init.svc.vendorcfgd]: [restarting]`（`/system_ext/bin/vendorcfgdeamon` 无限拉起）、`tool_service` 刷 423 条 AVC、`sprd_networkcontrol` 刷 51 条 AVC，且 `com.android.aftersaleservice.ZStats` 开机抛 `SecurityException: SELinux denied` 崩溃，静置 `top` 空闲率仅 `424%`（800% 满核）。

---

## 二、根因

1. **存量清单早于展锐 GRF `02_prepare` 对齐改造**：`P616F05` 系中兴基于展锐 `ums9230` 公版构建的机型（`Z2459`），相机、RIL、守护进程布局均为标准展锐 `UMS9230T` 形态而非高通努比亚形态；早期入库时未解析 DTB 面板时序（`720×1600`）与 `sprd,power-scene` 25 档频点，亦未识别官方固件内建的 NXP/NFC 特性。
2. **32 位 `mediaserver` 与 HIDL Codec2 选型约束**：与 `g1130` `#337`（[../../g1130/issue-g1130-camera-video-switch-hang-cdh-mediaserver-64/solution.md](../../g1130/issue-g1130-camera-video-switch-hang-cdh-mediaserver-64/solution.md)）一致，官方 `system.img` 仅带 32 位 `mediaserver`，必须在 `cdh_redirect` 中把 `/system/bin/mediaserver -> /mnt/vendorpatch/vendor/bin/hw/cdh-mediaserver` 置于 `/vendor/bin` 目录规则之前并设置 `debug.stagefright.c2inputsurface=-1`；同时因 `vendor` 为 `android13`（HIDL `c2@1.2-service-v4l2-64`），**不得**设置 `media.c2.hal.selection=aidl`，否则 Android 14 `Codec2Client` 会跳过 HIDL `v4l2` store。
3. **中兴专有守护进程与售后统计服务无云机后端**：`/system_ext/bin/{vendorcfgdeamon,tool_service,sprd_networkcontrol}` 在无展锐物理 modem/misc 节点时循环退出或越权读属性；`com.android.aftersaleservice` 试图向系统 `servicemanager` 注册未授权服务触发异常。

---

## 三、证据

- **修前失败现场**：见 [log-fail.txt](log-fail.txt)、[home-before.png](home-before.png)
  - `Physical size: 1080x2400`、`market=`、`ddr=`、`[init.svc.vendorcfgd]: [restarting]`、`SwapFree: 0 kB`、`Service media.player: not found`、`Bluetooth state: OFF`、`mVoiceRegState=1(OUT_OF_SERVICE)`、`tool_service` 423 条 AVC、`top` `424%idle`、`com.android.aftersaleservice` 崩溃。
- **修后实机全维度验收日志与截图**：见 [log-pass.txt](log-pass.txt)、[home-after.png](home-after.png)、[camera-rear-clean.png](camera-rear-clean.png)、[camera-front-preview.png](camera-front-preview.png)、[camera-video-rec-active.png](camera-video-rec-active.png)、[photos-video-playback.png](photos-video-playback.png)。

---

## 四、改动

1. **真机规格重对齐（`devices/manifest/devices/nubia/P616F05*`）**：
   - 对齐 `model="ZTE Blade V70"`、`resolution="720x1600"`、`inch="70x155"`、`density=320`、`skuCombinations=["4GB+128GB", "8GB+256GB"]`；`vendor_boot/config.json` 注入 `androidboot.ddrsize=8192M`。
2. **硬件 HAL 补齐与变体纠偏（`P616F05-android13/hardware/config.json`）**：
   - 新增 `"android.hardware.nfc"`（恢复真机 NFC 硬件能力）；将 `"android.hardware.camera"` 与 `"android.hardware.radio"` 变体对齐为 `"unisoc"`；重编刷新 `android13` 下 `media`、`camera@unisoc`、`radio@unisoc`、`bluetooth`、`nfc`、`wifi`、`gpu`、`allocator`、`service.daemon` 9 个模块与 `5.15` 外部内核模块。
3. **内核模块重定向、频点表与系统属性收敛（`P616F05-android13/{module,vendor}/**`，遵循 `AGENTS.md §8` 不改官方 `super.img`）**：
   - `module/config.json`：写入 `UMS9230T` 25 档 `sprd-cpufreq` 频点表（小核 `614.4MHz..1.82GHz` 13 档、大核 `921.6MHz..2.028GHz` 12 档），对齐触控 `11520×25600`，将 `file /system/bin/mediaserver -> /mnt/vendorpatch/vendor/bin/hw/cdh-mediaserver` 置于 `dir /vendor/bin` 之前，并追加屏蔽 `/system_ext/bin/{tool_service,sprd_networkcontrol,vendorcfgdeamon}`（共 21 条 `<= 32`）。
   - `vendor/config.json` & `vendor/etc/{battery_config.xml,media_config.xml,init/cph_p616f05_setup.rc}`：补入 `debug.stagefright.c2inputsurface=-1`、`gsm.version.ril-impl=sprd-ril`、`ro.vendor.cdh.device.market_name=ZTE Blade V70`、`ro.vendor.cdh.wifi.disable_he=1`（保持 HIDL C2 默认选型，不设 `media.c2.hal.selection=aidl`）；修正 `battery_config.xml` `full_uah="5000000"`、`media_config.xml` `rank="512/1024"`；新增 `cph_p616f05_setup.rc` 在开机完成后幂等禁用 `com.android.aftersaleservice`。
4. **全量盘集构建与同步**：同步 `rom-main` 并执行 `pipeline device build P616F05`，刷新 `vda.img`（`7.06 GiB`）、`patch.img`（`sha256: de7ac8faae0b`）与 `device.json`。

---

## 五、验证

冷启动 `P616F05_14-TP1A.220624.014-20260324 #1`（`8GB+256GB` SKU，ADB `127.0.0.1:5911`），`42.8s` 就绪进入桌面（详见 [log-pass.txt](log-pass.txt)）：

1. **规格与系统稳态**：`SELinux=Enforcing`、`restarting=0`、`tool_service_avc=0`、`sprd_networkcontrol_avc=0`、`com.android.aftersaleservice` 崩溃为 `0`；`Physical size: 720x1600 @ 320dpi`、`market=ZTE Blade V70`、`ro.boot.ddrsize=8192M`、`MemTotal=7760724 kB`、`SwapTotal=6291452 kB / SwapFree=6290684 kB`（`lowmemorykiller: swap is fatal` 为 `0`）、电池 `charge_full_design=5000000`（`100% Full`）、25 档 `cpufreq` 与 14 项硬件传感器全部在线；静置 `top` 空闲率由 `424%` 提升至 **`746% idle`**，桌面 `com.zte.mifavor.launcher` 连续滑动 547 帧 `P50=16ms, P99=29ms`（GPU `P99=13ms`）。
2. **Bluetooth / Wi-Fi / Radio / NFC 四大通信硬件**：
   - **Bluetooth**：`enabled: true, state: ON, address: DA:4C:10:DE:00:00, name: nubia V70, Bluetooth crashed 0 times`。
   - **Wi-Fi**：关联 5GHz `"MStand-Guest-6262"`（`192.168.20.100/24`），外网 `ping 223.5.5.5` `3/3 received, 0% loss`。
   - **Radio**：`gsm.sim.state=LOADED,ABSENT`、中国联通 `LTE` `IN_SERVICE`、基带 `4G_MODEM_22B_W25.24.3_P5`；执行 `svc wifi disable` 关闭 Wi-Fi 后纯蜂窝移动数据经 `ccmni1`（`100.64.155.15/24`）`ping 223.5.5.5` `3/3 received, 0% loss (rtt avg 1.696 ms)`。
   - **NFC**：右上角状态栏点亮 `N` 图标，`mState=on`，HCE 已注册 Nearby 与 Google Pay；调用 `/api/nfc/tap` 返回 `connected=true, initialized=true, discovering=true, cardPresent=true, cardUid=04112233445566, rxCommands=22, txResponses=32`。
3. **原生相机 `DreamCamera2` 与 V4L2 硬件编解码**：
   - `Service media.player: found`（64 位 `cdh-mediaserver` 在线），`/dev/video0`、`/dev/video1` 与 `android.hardware.media.c2@1.2-service-v4l2-64` 注册 `c2.v4l2.{avc.decoder,hevc.decoder,avc.encoder}`；
   - `DreamCamera2` 后摄与前摄实时取景、拍照落盘 `.jpg`（`IMG_20261127_032232.jpg` 12.4 KB、`IMG_20261127_032242.jpg` 16.2 KB 等）及左下角圆形缩略图均清晰正常；
   - 切入录像模式录制 5.06s 生成 `VID_20261127_032644.mp4`（`371,789 B`，`ffprobe` 校验 `h264 1920x1080 @ 30fps, 587 kb/s`）；Google Photos 回放该视频时 `V4L2Decoder` 连续硬解输出 `1920x1080 YV12` 画面（`bitstreamId=783..815`）；
   - 按 Home 退回桌面后连续两次采样 `/api/camera/status`，`framesSent` 恒为 `242 -> 242`（录像后二次复核 `906 -> 906`），证实 `CDCM_PAUSE` 相机休眠生效。

---

## 六、遗留

- 官方 `super.img` 预装的 32 位 `com.android.chrome` 在纯 64 位云机后台自启探针时会记录 `UnsatisfiedLinkError: library "libmonochrome.so" not found`（与 `g1130` `#336`、`arw` `#304` 一致，不影响桌面及任何系统功能）。
