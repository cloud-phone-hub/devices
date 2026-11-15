# #194 realme GT7（realme/RMX6688）Android 15 云机全链路起机与运行态 Bug 根治

- **设备/机型**：realme GT7（`realme/RMX6688`，真我 GT7 国行，项目号 `24618` / `RE602AL1`，SoC `MediaTek 天玑 9400+` `mt6991`，ColorOS 15.0.2 / Realme UI 6.0 / Android 15，基线 `15-AP3A.240617.008-20250825`，`lineage-22.2` + 登记借用 `lineage-22.1` `kernel 6.6`）
- **上游条目**：[#193 vendor_boot 合成自动剔除未落盘逻辑分区](../../common/issue-vendor-boot-drop-missing-logical-partitions/solution.md)
- **状态**：已闭环（2026-11-12）

## 一、问题

在将 realme GT7（`RMX6688_15-AP3A.240617.008-20250825`）接入 11 阶段引擎流水线并执行冷启动及全栈交互测试时，依次暴露以下 9 类引导阻断与运行态致命缺陷：

1. **一阶段 `1.17s` 挂载缺失逻辑分区触发 `InitFatalReboot`**：官方 `fstab.mt6991` 声明了 18 个逻辑分区，而官方 `super.img` 实际仅含 16 个（不含空的地区定制分区 `my_company`、`my_preload`），`first_stage_init` 因 `Failed to mount /mnt/vendor/my_company` 直接重启（引擎级通用修复已独立归档于 [#193](../../common/issue-vendor-boot-drop-missing-logical-partitions/solution.md)）。
2. **二阶段 `2.43s` `PropertyInit` 解析属性上下文失败触发 `InitFatalReboot`**：`init` 报 `Invalid context u:object_r:vendor_engineer_prop:s0` 与 `vendor_oplus_prop`，以及 `installd` 缺少 `opluscamera_app_data_file` 类型。
3. **二阶段 `19.67s` `vendor.engineer_vendor_shell` 触发 `init` 子进程 `SIGABRT`**：官方 `/odm/etc/init/` 启动 `/odm/bin/engineer_vendor_shell.sh` 时因 `cannot setexeccon('u:r:engineer_vendor_daemon:s0'): Invalid argument` 导致 `init` 派生子进程崩溃。
4. **FRP 块设备 `EACCES` 与 `*-virtualhal` 反复崩溃触发 `sys.init.updatable_crashing=1`**：`PersistentDataBlockService` 打开 `/dev/block/by-name/frp` 报 `ENOENT`/`EACCES`；同时 `vendor.face-virtualhal`、`vendor.fingerprint-virtualhal`、`vendor.power.stats`、`nfc_hal_service.tms.aidl` 等因抢注 AIDL 或缺物理芯片每 5 秒退出一次，触发 `updatable_crashing=1` 回滚风暴。
5. **进入桌面后 `248s` 触发 `system_server_pre_watchdog`、`710s` `Watchdog` 杀死 `system_server` 重启**：`android.display` 线程卡死在 `OplusColorSpaceManager.setColorSpaceValue()` -> `SurfaceFlinger` Binder 调用长达 606 秒，日志高频刷屏 `Since 'vendor.oplus.hardware.displaycolorfeature.IDisplayColorFeature/default' could not be found`。
6. **显示密度与高刷档位未对齐真机，且 `com.oplus.camera` 后台每 15 秒 `SIGSEGV` 崩溃**：初始使用 `480dpi` 且仅 60Hz（真机为 `1280×2800 @ 560dpi`、支持 `60/90/120/144Hz`）；`com.oplus.camera` 后台进程在 `/product/lib64/libAPSClient-cmd-jni.so`（`APSClient_createClient+60`）持续空指针崩溃刷爆墓碑。
7. **蓝牙开启与关闭两阶段均发生 `com.android.bluetooth` `SIGABRT` 崩溃**：
   - 开启阶段：先因 LE Audio 要求 Audio HAL V2.1 触发 `LeAudioClient::Initialize` 断言崩溃；关闭 LE Audio 后，首启又因厂商专有 HCI 命令 `0xefd` 在虚拟控制器超时 2 秒触发 `hci_timeout_abort: Command timed out: 0xefd` 崩溃；
   - 关闭阶段：蓝牙进入 `ON` 后执行关闭时，停留在 `BLE_TURNING_OFF` 5 秒超时转 `OFF` 的瞬间必现 `system/gd/os/linux_generic/reactor.cc:103 Unregister: Check failed: control_result == 0: system_error: Bad file descriptor (9)` 崩溃。
8. **每次开机桌面及设置页均弹出 `com.oplus.battery`「建议充电上限设为 80%」全局遮挡弹窗**：即使将 `battery_config.xml` 设为未插电 `ac=0/usb=0`、`level=78%`（且修复 `health_service.cpp` 未插电仍报 `FULL` 的状态矛盾），冷启动进桌面后 `com.oplus.battery` 依然弹出 `TYPE_SYSTEM_ALERT (2003)` 模态框遮挡屏幕。

## 二、根因

1. **跨分区 SELinux 类型缺失与 `engineer_vendor_shell` 域未定义**：
   - 官方 `odm` / `system_ext` 的 `property_contexts` 与 `seapp_contexts` 引用了原厂 `odm_sepolicy.cil` 中的 `vendor_engineer_prop`、`vendor_oplus_prop`、`opluscamera_app_data_file`；云机屏蔽原厂 MTK `odm_sepolicy.cil` 后需在机型 `vendor_sepolicy.cil` 补齐声明；
   - `/odm/etc/init/` 定义的 `vendor.engineer_vendor_shell` 指定了未定义的 `engineer_vendor_daemon` 域，通过 `cdh_redirect` 将 `/odm/bin/engineer_vendor_shell.sh` 指向不存在的载体路径后，`init` 启动前检查二进制不存在即静默跳过，不再 fork 子进程调 `setexeccon`。
2. **`SurfaceFlinger` 无守卫持锁死等 `IDisplayColorFeature/default`**：
   - 反汇编 `/system/bin/surfaceflinger` 证实：虽然 `IIrisFeature`（`0x4f5830`）先调用 `AServiceManager_isDeclared` 守卫，但 `updateColorSpaceValue`（`0x48b0c0 -> 0x48b184`）与 `handleSharpness`（`0x4c9e50 -> 0x4c9f8c`）在持有内部 `pthread_mutex_lock` 的状态下，**漏调 `isDeclared` 直接调用无超时的 `AServiceManager_waitForService("vendor.oplus.hardware.displaycolorfeature.IDisplayColorFeature/default")`**！这导致仅遮蔽 VINTF XML 不仅无效，还会使 `SurfaceFlinger` 永久死锁并拖死调用它的 `system_server` `android.display` 线程。
   - 提取官方 `/odm/lib64/vendor.oplus.hardware.displaycolorfeature-V1-ndk.so` 确认该接口（V1，hash `d6787dd098a56609b744bb3fa3a0fdc00f9f2513`）仅 2 个方法（`getDisplayColorFeatureValue`、`setDisplayColorFeatureValue`），在 `hwc3-service.drm` 同进程按需注册即可根治。
3. **`ICharger/default` 缺失致粘性广播 `notifycode=-1` 误触发位掩码弹窗**：
   - 逆向 `/system_ext/framework/oplus-services.jar`、`/system_ext/framework/vendor.oplus.hardware.charger-V8-java.jar`、`/odm/lib64/vendor.oplus.hardware.charger-V7-ndk.so` 与 `/system_ext/app/Battery/Battery.apk` 查明完整跨进程因果链：
     1) 官方 `/odm/etc/vintf/manifest/manifest_oplus_charger_aidl.xml` 声明了 `vendor.oplus.hardware.charger.ICharger/default`（version 7，hash `faa5fc9d45d1b3a786b0e17b204bb0342b828db1`）；
     2) `system_server` 的 `OplusBatteryService.getBroadcastDataFromHal()` 调用 `OplusBatteryServiceFeature.getPsyBatteryNotify()` 连接 `ICharger/default` 失败，返回错误码 **`-1` (`0xffffffff`)**，并将其写入粘性广播 `android.intent.action.ADDITIONAL_BATTERY_CHANGED` 的 `notifycode` extra；
     3) `Battery.apk` 的 `SmartChargeProtectionController`（`Lj8/j;.b:0x1e6f48`）收到粘性广播读取 `notifycode = -1`（`0xffffffff`），未校验负数便直接位与 `(0x10000000 & notifycode) != 0`（`NOTIFY_CODE_LONG_CHARGE_LEVEL_ONE`），全 1 比特位使条件恒成立，随即无视开关直接调用 `Lj8/j.I0()` 弹出 `TYPE_SYSTEM_ALERT` 全局遮挡弹窗！
   - 由于 `com.oplus.battery` 同时承载系统「设置 -> 电池」（`PowerConsumptionActivity`），不能整包遮蔽；在 `android.hardware.health-service` 同进程检测到 VINTF 声明时注册 `ICharger/default`（覆盖全部 130 个业务事务码，其中 `getPsyBatteryNotify` 返回 `0`、`getChgConfig` 等供 `Integer.parseInt` 解析的方法返回 `"0"`，其余按 `HealthInfo` 实时快照返回一致的电压/温度/容量/健康度），即可保留完整电池设置 UI 并彻底消除弹窗。
4. **蓝牙双阶段崩溃根因（`0xefd` 厂商 HCI 命令 + `AccessoryFramework` 常驻 BLE 广播未注销）**：
   - 首启 `0xefd` 超时：Oplus 蓝牙栈在 `persist.vendor.bt.iotinfo.report.enable` 未显式关闭时向控制器发送厂商专有 HCI 命令 `0xefd`（`VS_OPLUS_IOTINFO_REPORT`），虚拟 RootCanal 不支持该 opcode 导致 2s 超时 abort；设为 `false` 后首启崩溃归零；
   - 关闭阶段 `Reactor::Unregister` `EBADF`：反汇编 `/system/lib64/libbluetooth_jni.so`（`ModuleRegistry::StopAll` `0x582040` -> `LeAdvertisingManager::Stop` `0x5e95a0` -> `~impl` `0x60b970` -> `~Alarm` `0x5958a0` -> `Reactor::Unregister` `0x598220`）并结合运行态日志证实，欢太设备互联框架 `/product/priv-app/AccessoryFramework/AccessoryFramework.apk`（`com.heytap.accessory`）在蓝牙开启后立即注册常驻无感发现 BLE 广播（`DiscoveryClient(id=accept-senseless, pkg=com.heytap.accessory)`），且在 `BLE_TURNING_OFF` 阶段不响应注销。5 秒超时强制转 `OFF` 析构残留 `Advertiser` 时，其内部 `Alarm`（`[node + 0x98]`）绑定的上层线程 `Reactor` `epoll_fd_` 已提前关闭，`epoll_ctl(EPOLL_CTL_DEL)` 报 `EBADF (9)` 触发 `CHECK` 自杀。云机无物理近场配件，通过 `cdh_redirect` 遮蔽 `/product/priv-app/AccessoryFramework` 即可根治。

## 三、证据

- 失败与通过日志：[log-fail.txt](log-fail.txt)、[log-pass.txt](log-pass.txt)
- 全新干净盘冷启动实测截图：
  - 原生 Realme UI 6.0 桌面（`1280×2800 @ 560dpi`，Wi-Fi + 4G 在线，电量 78%，零弹窗遮挡）：[desktop.png](desktop.png)
  - 关于本机页（`realme RMX6688`、`realme UI 6.0`、`MediaTek 天玑 9400+`、`7200 mAh (典型值)`、`8.00 GB`、`6.8 英寸`）：[about-device.png](about-device.png)

## 四、改动

1. **HWC3 内建 Oplus 显示色彩扩展兼容服务（跨 ColorOS 机型通用）**：
   - [CdhOplusDisplayColor.h](../../../../vendor/hardware/android.hardware.graphics.composer3/src/CdhOplusDisplayColor.h)、[CdhOplusDisplayColor.cpp](../../../../vendor/hardware/android.hardware.graphics.composer3/src/CdhOplusDisplayColor.cpp)、[service.cpp](../../../../vendor/hardware/android.hardware.graphics.composer3/src/service.cpp)、[README.md](../../../../vendor/hardware/android.hardware.graphics.composer3/README.md)：当设备 VINTF 清单声明了 `IDisplayColorFeature/default` 时在 `hwc3-service.drm` 同进程注册 V1 兼容实现；非 ColorOS 机型零副作用。
2. **Health HAL 内建 Oplus 充电扩展兼容服务与未插电状态修正（跨机型通用）**：
   - [CdhOplusCharger.h](../../../../vendor/hardware/android.hardware.health/src/CdhOplusCharger.h)、[CdhOplusCharger.cpp](../../../../vendor/hardware/android.hardware.health/src/CdhOplusCharger.cpp)、[health_service.cpp](../../../../vendor/hardware/android.hardware.health/src/health_service.cpp)、[android.hardware.health.json](../../../../vendor/build/modules/android.hardware.health.json)：
     - 当设备 VINTF 声明了 `vendor.oplus.hardware.charger.ICharger/default` 时在 `android.hardware.health-service` 同进程注册兼容实现（自适应 V7/V8 版本与哈希，`getPsyBatteryNotify` 返回 `0`，`getChgConfig` 等返回 `"0"`，其余按 `HealthInfo` 实时快照返回）；
     - 修正 `sampleBattery()` 在所有充电器均离线（`ac=0/usb=0/wireless=0/dock=0`）时上报 `DISCHARGING`（满电 `NOT_CHARGING`）与平稳放电电流，消除无充电器却报 `CHARGING`/`FULL` 的状态矛盾；
     - Android 14 / 15 / 16 / 17 全四代际（含 `aosp` 与 `samsung` 变体）重编通过。
3. **RMX6688 机型与配置清单**：
   - [RMX6688.json](../../../../manifest/devices/realme/RMX6688.json)：对齐真机 `density: 560`（`1280×2800`）。
   - [RMX6688-android15.json](../../../../manifest/configs/realme/RMX6688/RMX6688-android15.json)：
     - `buildModules.redirect`（共 16 条）：包含 `/odm/bin/engineer_vendor_shell.sh`、`/system/priv-app/NfcNci`、`/product/app/OplusCamera` 与 `/product/priv-app/AccessoryFramework` 遮蔽规则；
     - `buildVendor.build-prop-sets`：关闭 LE Audio 各 profile 与卸载、关闭 `persist.vendor.bt.iotinfo.report.enable` / `persist.vendor.service.bt.iotinfo.report.enable`、设置 `ro.sf.lcd_density=560`、`ro.vendor.cdh.display.refresh_rates=60,120,90,144`、`Immortalis-G925` GLES/Vulkan 身份串与 `ro.frp.pst=/dev/block/by-name/frp`；
     - `buildVendor.fileTrees`：配置 7200mAh `etc/battery_config.xml`、`etc/init/frp_symlink.rc`、`etc/init/init.virtio_compat.rc`（保留 `displaycolorfeature` 与 `charger` 原厂 VINTF 声明，遮蔽无实现的虚拟音频/双卡专有 RIL/红外/面板/定位/subsys VINTF 并 `stop` 10 个冲突/缺芯片服务）、`etc/media_config.xml` 与 `etc/ueventd.rc`；
     - `buildVendor.sepolicy`：补齐 `vendor_engineer_prop`、`vendor_oplus_prop`、`opluscamera_app_data_file`、`hal_displaycolorfeature`（`hal_graphics_composer_default`）及 `hal_charger`（`hal_health_default`）SELinux CIL 规则。

## 五、验证

- **全仓单测**：`engine/vendor/build` 与 `engine/build` 执行 `go test ./...` 全部包全绿通过。
- **镜像构建**：`go run . device build RMX6688 --from 05_build_vendor_boot --skip 08_build_vendor_dlkm --skip 10_pack_super` 全阶段 `[ok]`，产出 `patch.img`（sha256 `599ff1a35282`）与 `vda.img`（配方 `vda-e69d03f754d7`）。
- **全新干净盘冷启动与长稳全栈验收**（实例 `RMX6688_15-AP3A.240617.008-20250825/1`）：
  1. `boot_check.py` 验收 `[pass]`（`1280×2800` 平均亮度 `116.2` 峰值 `255`），`sys.boot_completed=1`、`sys.init.updatable_crashing` 为空；
  2. `service check` 确认 `IDisplayColorFeature/default: found`、`ICharger/default: found`；
  3. `dumpsys activity broadcasts` 确认粘性广播 `ADDITIONAL_BATTERY_CHANGED` 中 `notifycode=0`、`pps_chg_mode=0`、`status=3`（`DISCHARGING`）；桌面无任何遮挡弹窗，打开「设置 -> 电池」（`com.oplus.battery/.PowerConsumptionActivity`）与「关于本机」（`MyDeviceInfoActivity`）界面正常渲染且无弹窗；
  4. 蓝牙连续执行两轮完整开关循环（`enable -> 8s 确认 state: ON -> disable -> 7s 确认 state: OFF`）并重新开启常驻在线，`logcat -b crash -d` **0 条致命崩溃**；
  5. 持续运行 `uptime > 270s`（越过原 248s `pre_watchdog` 阈值），`dumpsys dropbox` **0 条 `watchdog` / `anr` / `crash`**，`system_server` 与 `surfaceflinger` PID 全程恒定。

## 六、遗留

- 原厂相机 `/product/app/OplusCamera` 因深度强耦合联发科/OPPO 专有 APS 图像算法守护进程（`libAPSClient-cmd-jni.so`），在云机无硬件 ISP/APS 服务时会后台自启空指针崩溃，当前通过 `cdh_redirect` 遮蔽以保证整机零崩溃；后续如需在 ColorOS 云机提供相机 App UI，可在载体层预置兼容标准 Camera2 HAL 的通用相机应用替代。
