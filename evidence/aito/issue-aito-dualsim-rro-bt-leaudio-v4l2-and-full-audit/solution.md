# Motorola Razr 2024 (`motorola/aito`) 设备规格对齐、`com.android.phone` 双卡 RRO 越界崩溃、蓝牙 LE Audio `SIGABRT` 与全硬件闭环验收

- **设备/机型**：Motorola Razr 2024（`motorola/aito`，MediaTek Dimensity 7300X `mt6878`，GRF `vendor=14` + `system=16`，`16-W1UCS36H.96-35-8-9-20260801`，`lineage-21.0` + `kernel 6.1`）
- **编号**：`#350`

## 一、问题

对镜像库 `#18` `aito_16-W1UCS36H.96-35-8-9-20260801` 执行修前全维度体检时，暴露出 6 项功能阻断与性能/规格缺陷：

1. **主界面持续弹出 `com.android.phone keeps stopping` 崩溃窗，静置 CPU 飙至 `710%/800%`（仅 `90% idle`），蜂窝移动数据完全断连（`ccmni0..20` 全 `DOWN`）**：`com.android.phone` 与 `com.motorola.modemservice` 每秒死循环崩溃重启。
2. **开启蓝牙立即触发 `com.android.bluetooth` `SIGABRT` 循环崩溃**：`cmd bluetooth_manager enable` 后 `LeAudioClient::Initialize` 断言失败退出，蓝牙无法保持 `ON`。
3. **V4L2 硬件编解码被空桩抢占且 GRF 跨代 C2 HAL 选择缺失**：`vendor=14 / system=16` 未声明 `media.c2.hal.selection=aidl`，且 `Asteroids/aito` 早期 `vendor/etc/media_config.xml` 中空桩组件全部写为 `rank="1"`，压过 `c2.v4l2.*`（`rank=128`）与 `c2.android.*`。
4. **真机规格与内核虚拟节点未对齐**：`aito.json` `model` 为 `"Motorola Razr (aito)"`；`12GB+512GB` SKU 启动后仅识别 `8GB` RAM、`SwapTotal: 0`（无 6GiB ZRAM）、缺失 `/sys/class/power_supply/battery`（无虚拟电池 `4200000` µAh）与 `/proc/bootprof`。
5. **相机服务静置空转**：未引入新版按需休眠 `CDCM_PAUSE` 的 `android14` `camera@motorola`，静置时 `android.hardware.camera-service` 常驻消耗 `~10%` CPU。
6. **SELinux 审计风暴**：冷启动 100 秒内 `dmesg` 刷出 `29,758` 条 `avc`，其中 `lbs_dbg_ext -> hwservicemanager_prop`、`hal_radio_default -> serialno_prop`、`system_server -> binderfs_logs_proc` 高频刷屏。

## 二、根因

1. **双卡 RRO Overlay 属性门控与 SELinux 跨分区不可读**：
   - 官方 `/vendor/overlay/MotoFrameworkResOverlayHWDualSim.apk` 的 `AndroidManifest.xml` 声明了 `<overlay android:requiredSystemPropertyName="ro.vendor.hw.dualsim" android:requiredSystemPropertyValue="true"/>`（将 `config_num_physical_slots` 覆盖为 `2`）。
   - 旧配置既未注入 `ro.vendor.hw.dualsim=true`，也未为 `ro.vendor.hw.` 配置 `exported_config_prop` 属性上下文，导致 `ro.vendor.hw.*` 默认落入 `vendor_default_prop`，`idmap2` / `system_server` 跨分区读取被拒，双卡 RRO 未激活；`UiccController` 按单槽 `mCis.length=1` 初始化，而自研 `radio` 双槽上报 `slot2`（`index=1`）时触发 `java.lang.ArrayIndexOutOfBoundsException: length=1; index=1`。
2. **GRF Android 16 默认启用蓝牙 LE Audio Unicast Client**：
   - `/product/etc/build.prop` 开启 `bluetooth.profile.bap.unicast.client.enabled=true` 等 LE Audio 特性，而云机通用 Bluetooth Audio AIDL HAL 未挂载硬件 Offload V2.1+，导致 `LeAudioClient::Initialize()` 触发 `LOG_ALWAYS_FATAL` 断言。
3. **旧盘集构建早于 `prepare`/`daemon`/`6.1` 模块近期升级**：
   - `6.1` 内核外部模块（`cdh_device_info.ko`、`cdh_redirect.ko`）及 `android14` HAL（`daemon`、`camera@motorola`、`gpu`、`allocator`）未重新编译合入，`media_config.xml` 仍停留在早期 `rank="1"` 模板。

## 三、证据

- 修前故障日志与崩溃栈见同目录 [log-fail.txt](log-fail.txt)，修前主界面崩溃弹窗见 `home-before.png`：
  - `AndroidRuntime: Process: com.android.phone` -> `java.lang.ArrayIndexOutOfBoundsException: length=1; index=1 at com.android.internal.telephony.uicc.UiccController.<init>`；
  - `bluetooth` 崩溃断言：`packages/modules/Bluetooth/system/bta/le_audio/client.cc: LeAudioClient::Initialize: Hal version ... is not supported`；
  - 修前 `top`：`800%cpu ... 90%idle`，`dmesg` `avc` 计数 `29758`。
- 修后全量通过日志见 [log-pass.txt](log-pass.txt)，实屏截图见 `about-phone.png`、`launcher-desktop.png`、`camera-rear.png`、`camera-front.png`、`video-playback.png`。

## 四、改动

1. **真机规格与双卡/媒体配置收口**：
   - `devices/manifest/devices/motorola/aito.json`：`model` 对齐真机上市名 `"Motorola Razr 2024"`；
   - `devices/manifest/configs/motorola/aito/aito-android14/vendor/config.json`：保留原厂 `etc/audio_param` 规则，新增 `"ro.vendor.hw.dualsim": "true"`、`"ro.telephony.sim_slots.count": "2"`、`"media.c2.hal.selection": "aidl"`、`"persist.vendor.vilte_support": "0"`、`"persist.vendor.viwifi_support": "0"`，刷新率收敛至 `"60,90,120"`；
   - `devices/manifest/configs/motorola/aito/aito-android14/vendor/etc/media_config.xml`：将空桩组件 `rank` 自愈调低至 `512/1024`，释放 `c2.v4l2.*`（`rank=128`）优先级；重新经 `prepare`/`apply-profile` 对齐真机电池（`4200000` µAh）、42 档 `cpufreq` 与传感器表。
2. **SEPolicy 属性导出与降噪**：
   - 新建 `devices/manifest/configs/motorola/aito/aito-android14/vendor/sepolicy/vendor_property_contexts`：映射 `ro.vendor.hw. u:object_r:exported_config_prop:s0`，使 `idmap2`/`system_server` 合法激活 `MotoFrameworkResOverlayHWDualSim.apk`；
   - 更新 `devices/manifest/configs/motorola/aito/aito-android14/vendor/sepolicy/vendor_sepolicy_rules.cil`：对 `lbs_dbg_ext`、`serialno_prop`、`binderfs_logs_proc` 补充 `dontaudit`。
3. **跨机型蓝牙 LE Audio 与 GPU 缩略图通病根治 + 6.1/A14 模块刷编**：
   - 在 `devices/hardware/android.service.daemon/vendor/bin/cph-zram-setup` 的 `on boot` 阶段幂等置 `bluetooth.profile.{bap.unicast.client,bap.broadcast.assist,bap.broadcast.source,csip.set_coordinator,hap.client,mcp.server,ccp.server,vcp.controller}.enabled=false` 及 `persist.bluetooth.leaudio_offload.disabled=true`；
   - 合入 `devices/hardware/android.hardware.gpu/sepolicy/vendor_sepolicy_rules.cil`（授予 `mediaserver`/`mediacodec` 只读访问 `sysfs_gpu`），刷编 `6.1` 外部模块与 `android14` HAL 并全链重打 `aito` 盘集。

## 五、验证

在 `aito_16-W1UCS36H.96-35-8-9-20260801`（`12GB+512GB` SKU，`127.0.0.1:5911`）冷启动全量实测通过：

1. **启动与静置性能**：`40.7s` 冷启动完成，`getenforce=Enforcing`，`restarting=0`；桌面零崩溃弹窗，`com.android.phone`（PID `8823`）与 `com.motorola.modemservice`（PID `8071`）常驻零崩溃；静置 `top` 达 **`790%~793% idle / 800% cpu`**（修前仅 `90% idle`）；`qemu.log` `avc` 下降 `90%`。
2. **设备规格对齐**：「关于手机」实屏显示设备名称 `Motorola Razr 2024`、运行内存 `12GB+4GB`、存储 `512 GB`、显示屏 `2640x1080`；`/proc/meminfo` `MemTotal=11662200 kB`、`SwapTotal=6291452 kB`（6GiB ZRAM）；`/proc/bootprof` 存在；电池 `charge_full_design=4200000`；42 档 `cpufreq` 与 22 项真机硬件传感器在线；`com.motorola.launcher3` 滑动 826 帧 `Janky=2 (0.24%)`、`P50=6ms / P90=11ms / P95=13ms / P99=17ms`、`Missed Vsync=0`。
3. **蓝牙 / Wi-Fi / Radio / NFC**：
   - **蓝牙**：`state: ON`、`name: Motorola Razr 2024`、`Bluetooth crashed 0 times`；
   - **Wi-Fi**：关联 `ASUS_DA7C_2G` (`192.168.50.100`)，`lostTxPackets: 0`，`ping 223.5.5.5` `0% loss`；
   - **Radio**：中国联通 LTE `IN_SERVICE`（`sim_state=LOADED,ABSENT`），双 PDN `ccmni0` (IPv6) + `ccmni1` (`100.64.155.x`) 在线，关闭 Wi-Fi 后纯蜂窝移动数据 `ping 223.5.5.5` `0% packet loss`（`avg 0.562ms`）；
   - **NFC**：`/api/nfc/tap` 贴卡返回 `connected=true, cardPresent=true`，系统成功解析 `techs: android.nfc.tech.Ndef` 并唤起 `TagViewer`。
4. **相机与 V4L2 硬件编解码**：
   - Motorola 原生相机 `com.motorola.camera5` 前后摄实时彩色预览清晰，拍照落盘 2 张 JPEG（`16K`）；
   - 切录像模式通过 **`c2.v4l2.avc.encoder`** 硬件编码录制落盘 `VID_20261127_014733085.mp4`（`671KB`，`ffprobe` 校验 `h264 1920x1080 5.21s` + `aac 48kHz 256kbps`）；
   - Google 相册生成彩色视频缩略图（`0:05`，`mediaserver` 零崩溃）并通过 **`c2.v4l2.avc.decoder`** 硬件解码正常回放；
   - 按 `HOME` 退回桌面后宿主机 `framesSent` 恒定冻结在 `592`，`android.hardware.camera-service` 瞬时 CPU 归 `0.0%`。

## 六、遗留

无。实例已停机回收，临时解包目录已清理，源码与清单已同步至 `rom-main`。
