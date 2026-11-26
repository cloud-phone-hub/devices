# Infinix X6876 五大子系统（Wi-Fi / 蓝牙 / NFC / 相机 / Radio）全栈打通与卡顿根治（#345）

- **设备/机型**：Infinix GT 30 / X6876（`infinix/Infinix-X6876`，SoC `MT6878`，GRF `vendor=14 / API 34` + `system=16`，`16-UP1A.231005.007-20260423`，`lineage-21.0` + `kernels/6.1`）
- **状态**：已闭环（2026-11-26，实例 `Infinix X6876 #1`，`SELinux=Enforcing`）

## 一、问题

实例 `Infinix X6876 #1`（`Infinix-X6876_16-UP1A.231005.007-20260423`）启动进入桌面后存在多项外设异常与明显系统卡顿：

1. **蓝牙无限崩溃弹窗（头号卡顿源）**：`com.android.bluetooth` 每 ~1.8 秒触发一次 `SIGABRT`（累计 750+ 次），桌面周期弹出 `Bluetooth keeps stopping`，拖累 `system_server`（`41.3%` CPU）与传音异常收集进程 `com.transsion.succ`（`31.0%` CPU）持续满载。
2. **NFC 服务完全缺失**：`service check nfc` 返回 `not found`，`pm list features` 无任何 `android.hardware.nfc*` 特性，`com.android.nfc` 被框架开机自动禁用。
3. **Radio 蜂窝移动数据黑洞且缺 `ccmni0` IMS 通道**：状态栏虽显示 `中国联通 LTE`，但关闭 Wi-Fi 后纯蜂窝外网 `100% packet loss`，且无 `ccmni0` IMS IPv6 网卡。
4. **相机主线程 500ms 卡顿循环、V4L2 权限缺失与后台无休眠**：
   - 打开 `com.transsion.camera` 时，`TZServiceProxy` 查找 `vendor.transsion.hardware.camera.tzservice.ITZService/default` 被 SELinux 拒绝并抛 `NullPointerException`，导致 UI 主线程每次操作阻塞等待 `CompletableFuture.get(500ms)`；
   - `/dev/video0`、`/dev/video1` 权限为 `0600 root:root`；
   - 退出相机后宿主机仍以 8fps 持续推流（`framesSent=16060+`），无 `CDCM_PAUSE` 空闲休眠。
5. **渲染热路径 `/proc/ged` 与串口 `cdh_redirect` 日志风暴**：
   - `SurfaceFlinger`、`cameraserver` 及所有 UI 应用每帧调用 `libged_sys.so` 打开 `/proc/ged` 失败，触发 `E GED: Failed to open /proc/ged` 与 `libgui.so` `E [onQueue/onAcquire/onDequeue]: ged ... fail` 每秒近千行刷屏，桌面滑动 Legacy Jank 达 `39.97%`；
   - 旧版 `vendor_boot` 内 `cdh_redirect.ko` 持续向串口输出 `cdh_redirect: forward /mnt/vendorpatch/...` 引发高频 VM-Exit；
   - 此外若混用未合入 `rates_5g` 的旧 `6.1` 内核构建卷，`mac80211_hwsim.ko` 在 5GHz 假 AP 上附带 CCK 速率会导致 5GHz Wi-Fi 关联跳过。

## 二、根因

1. **官方 `/product` 开启 LE Audio 与蓝牙 Audio HAL 代际不匹配**：传音 Android 16 `/product` 默认预置 `bluetooth.profile.{bap.*,bas.client,ccp.server,csip.set_coordinator,hap.client,mcp.server,vcp.controller}.enabled=true`，而云机蓝牙音频 HAL 未提供 LE Audio 所需的 V2.1 Offload，触发 `[FATAL:client.cc(8109)] Assert failed: std::move(hal_2_1_verifier).Run()`。
2. **传音独立 `odm_a.img` 将 NFC 配置存放于 `/etc/vconfig/NFC/` 子目录**：
   - 传音 `X6876` 的 NFC 权限 XML 在 `odm_a.img:/etc/permissions/`，而核心配置 `libnfc-nci.conf`、`libnfc-nxp.conf`、`libnfc-nxp_RF.conf` 位于 `odm_a.img:/etc/vconfig/NFC/`；
   - `09_build_vendor` 的 `backfillOfficialNfcConfigs` 原先只扫描 `etc/` 与 `odm/etc/`，漏扫 `etc/vconfig/NFC/`，不仅未回填配置，还在 `hasAnyConf == false` 分支主动删空了 `vendor/etc/permissions/*nfc*.xml`；同时 `Infinix-X6876.json` 未声明 `host-hvc-nfc`（`hvc11`）与 `host-hvc-camera`（`hvc12`）。
3. **`android14` HAL 产物过期（停留在 `Nov 19`）且 `radio` HAL 存在两处 Android 14 条件编译笔误**：
   - 旧版 `android14` `cph-netmgr-setup` 将真实 `virtio-net`（`c2:36:b4:12:34:58`）重命名为未启用的 `ccmni2`，而把 `ccmni1` 建成了 `dummy` 黑洞网卡，且不含 `cph-ims-netmgr`（`ccmni0`）与相机 `CDCM_PAUSE`；
   - 重编 `android14` `android.hardware.radio`（`IRadioConfig v2` / `IRadioModem v2`）时，`radio_config.cpp` 的 `PhoneCapability::maxActiveVoice`、`onSimultaneousCallingSupportChanged` 与 `radio_modem.cpp` 的 `onImeiMappingChanged` 均系 Android 15（v3）新增符号，原守卫误写为 `CDH_ANDROID_VERSION >= 14` 导致 Android 14 编译报错。
4. **传音相机 `ITZService` 缺 SELinux 授权与 AIDL 事务应答**：`cdh_nn_placeholders.txt` 虽将 `ITZService/default` 交由 `hal_neuralnetworks_default` 注册，但 `platform_app` 缺 `tran_tzservice_binder_service` 的 `find` 与双向 `binder call/transfer` 权限，且默认占位返回 `STATUS_UNKNOWN_TRANSACTION` 会令 `ITZService$Stub$Proxy` 抛 `RemoteException`。
5. **GKI 云机内核无 MTK `/proc/ged` 节点且 `proc_type` 受 SELinux `allowx` 约束**：反汇编 `/system_ext/lib64/libged_sys.so` 证实 `ged_bridge_call` 通过 `_IOWR('g', nr, 0x28)` 发送 40 字节 `GED_BRIDGE_PACKAGE`；缺失 `/proc/ged` 时每帧打印多条 `ALOGE`，而仅建节点不补 `genfscon proc "/ged" proc_ged` 与 `(allowx ... proc_ged (ioctl file ((range 0x6700 0x67ff))))` 会触发 `untrusted_app_all proc neverallow` 或 `ioctlcmd=0x6765/0x6766` 审计拒绝。

## 三、证据

- 修复前故障采样详见 [log-fail.txt](log-fail.txt)：
  - 蓝牙断言崩溃：`[FATAL:client.cc(8109)] Assert failed: std::move(hal_2_1_verifier).Run(). Initialize, LE Audio Client requires Bluetooth Audio HAL V2.1 at least`；
  - NFC 缺失：`Service nfc: not found`、`pm list features` 无 NFC 条目；
  - Radio 黑洞：`ccmni1` 为 `dummy`、`ccmni2` 持有真实 MAC `c2:36:b4:12:34:58` 却处于 `DOWN`；
  - 相机卡顿：`avc: denied { find } name=vendor.transsion.hardware.camera.tzservice.ITZService/default scontext=u:r:platform_app:s0 ... tcontext=u:object_r:tran_tzservice_binder_service:s0`；
  - 渲染刷屏：`E GED: Failed to open /proc/ged`、`E [onQueue]: ged queue fail`、`cdh_redirect: forward` 持续刷屏，静置 `534%idle / 800%cpu`。
- `TranssionCamera.apk`（`classes4.dex`）反汇编确认 `vendor.transsion.hardware.camera.tzservice.ITZService` 共 7 个方法：transaction `1..6` 返回 `int32`，transaction `7`（`getOffPeakModeSupportCfg`）返回 `int[]`。
- 修复后全量验证日志详见 [log-pass.txt](log-pass.txt) 及同目录截图 `settings-status.png`、`camera-rear.png`、`camera-front.png`、`camera-video-recording.png`。

## 四、改动

1. **内核模块 `/proc/ged` 桥接节点与 SELinux `allowx` 策略**：
   - `devices/modules/device_info/sched/sched_proc.c`：注册 `/proc/ged`（`0666`），实现 `cdh_ged_proc_ioctl` 解析 40 字节 `cdh_ged_bridge_package`；对 `0xc0286764`（`GED_BRIDGE_COMMAND_GE_ALLOC`）保留调用方预置的 `-1` fd 直接返回 `0`（避免清零误关 `fd=0`），对其余 `'g'` 命令安全清零 `param_out`（置 `GED_OK = 0`）后返回 `0`；
   - `devices/hardware/android.service.daemon/sepolicy/vendor_sepolicy_rules.cil`：当基底未含 `(genfscon proc "/ged"` 时自动声明 `proc_ged`、绑定 `genfscon` 并向 `surfaceflinger`、`cameraserver`、`mediaserver`、`system_server`、`platform_app`、`untrusted_app_all` 等域授予 `file` 读写/`ioctl` 及 `(allowx ... proc_ged (ioctl file ((range 0x6700 0x67ff))))`。
2. **传音相机 `ITZService` AIDL 兼容桩与机型 SELinux 授权**：
   - `devices/hardware/android.hardware.neuralnetworks/src/mtk/nn_mtk_ext.cpp`：新增 `OnTranTzServiceTransact` 处理 `vendor.transsion.hardware.camera.tzservice.ITZService` 的 transaction `1..7` 及版本/哈希查询，返回 `STATUS_OK` 与 `0` / 空 `int[0]`；
   - `devices/manifest/configs/infinix/Infinix-X6876/Infinix-X6876-android14/vendor/sepolicy/vendor_sepolicy_rules.cil`：授予 `platform_app`、`priv_app`、`system_app`、`cameraserver` 查找 `tran_tzservice_binder_service` 及与 `hal_neuralnetworks_default` 双向 Binder 调用权限，并 `dontaudit` 蓝牙对 `mtk_hal_audio_service` 的探测噪声。
3. **流水线传音独立 `odm` `vconfig/NFC` 回填与同目录单测**：
   - `agent/modules/pipeline/stages/09_build_vendor/files_compat.go`：扩展 `backfillOfficialNfcConfigs` 同时扫描解包树与独立 `odm_a.img` / `vendor_a.img` 下的 `etc/vconfig/NFC/` 与 `odm/etc/vconfig/NFC/`；
   - `agent/modules/pipeline/stages/09_build_vendor/standalone_odm_nfc_test.go`：新增 `TestBackfillOfficialNfcConfigsFromTranssionVconfigDir` 单元测试并通过。
4. **Android 14 `radio` HAL 编译守卫修正**：
   - `devices/hardware/android.hardware.radio/src/radio_config.cpp`、`radio_modem.cpp`：将 Android 15（AIDL v3）新增字段/回调 `maxActiveVoice`、`onSimultaneousCallingSupportChanged`、`onImeiMappingChanged` 的宏守卫由 `>= 14` 修正为 `CDH_ANDROID_VERSION >= 15`。
5. **`Infinix-X6876` 清单补齐与全栈重编**：
   - `devices/manifest/configs/infinix/Infinix-X6876/Infinix-X6876-android14/vendor/config.json`：追加 10 项 `bluetooth.profile.*.enabled = "false"`（参考 [../../common/issue-le-audio-crash-loop-system-prop-bridge/solution.md](../../common/issue-le-audio-crash-loop-system-prop-bridge/solution.md)）；
   - `devices/manifest/devices/infinix/Infinix-X6876.json`：补齐 `"host-hvc-nfc": "hvc11"` 与 `"host-hvc-camera": "hvc12"`；
   - 全量重编 `android14` 23 个 HAL 模块、最新 `6.1` 内核与 13 个模块（含 `0002-hwsim-fake-wifi.patch` `rates_5g`），合成发布 `patch.img`、`vda.img`、`device.json`（官方 `super.img` 保持原样直通）。

## 五、验证

基于新盘集干净创建并冷启动 `Infinix X6876 #1`（`adb=127.0.0.1:5927`，`SELinux=Enforcing`）：

1. **Wi-Fi**：自动关联 5GHz `"CRH-FreeWiFi-13B-6A8"`（`5200MHz`，`IP: 192.168.20.100/24`，`Supplicant state: COMPLETED`），内核 `cdh-wifi: /dev/hvc10 已打开（raw）`，网络状态 `wifi+host-vpn (validated=true)`，`ping -c 3 -I wlan0 223.5.5.5` `0% packet loss`（平均 `0.562 ms`）。
2. **蓝牙**：`enabled: true, state: ON`，`Bluetooth crashed 0 times`，零崩溃弹窗，虚拟对端 `CDH-Headset-Pro` / `CDH-Speaker-Mini` / `CDH-Watch-S3` 就绪。
3. **NFC**：`pm list features` 含 `android.hardware.nfc` / `.any` / `.hce`，`Service nfc: found`，`mState=on`；执行 `/api/daemon/nfc/tap` 与 `remove` 成功识别卡号 `04112233445566`（`cloud-phone-hub-nfc-ok`，`rxCommands=51, txResponses=79`）。
4. **相机**：`/dev/video0..1` 为 `0660 media:camera`；`com.transsion.camera` 热启动 `417~499ms` 且零 `TZService` 异常；后置拍照 `.jpg`、前置切换拍照 `.jpg`、视频模式录制 `VID_20261126_173931.mp4`（706 KB）全部落盘；退出回桌面 2 秒后触发 `sent CDCM_PAUSE on /dev/hvc12`，后台 `framesSent` 增量为 `0`。
5. **Radio**：双卡 `LOADED,ABSENT`，`中国联通 (46001)` `LTE` `IN_SERVICE`；`ccmni0`（IMS IPv6 `2408:8456:...`）与 `ccmni1`（真实 `virtio-net` `c2:36:b4:12:34:58`，`100.64.155.15/24`）双 PDN `VALIDATED`；关闭 Wi-Fi 后纯蜂窝 `ping 223.5.5.5` `0% packet loss`（平均 `0.864 ms`）。
6. **流畅度与静置负载**：
   - `Failed to open /proc/ged`、`ged queue/acquire/dequeue fail`、`avc: denied ... proc_ged`、`hal_2_1_verifier`、`tran_tzservice_binder_service`、`cdh_redirect: forward` 计数**全部为 `0`**；
   - 桌面静置 CPU 空闲率升至 `719%idle / 800%cpu`（`system_server` `3.8%`、`logd` `0.0%`、`com.transsion.succ` `0.0%`）；
   - `QuickstepLauncher` 连续 10 次左右快速滑动：`Total frames rendered: 1336`，**`Janky frames: 0 (0.00%)`**，`Missed Vsync: 0`，`Slow UI thread: 0`（P50 `6ms` / P90 `10ms` / P99 `17ms`）。

## 六、遗留

无。五项外设子系统与 UI 流畅度已在 `Infinix X6876 #1` 闭环固化。
