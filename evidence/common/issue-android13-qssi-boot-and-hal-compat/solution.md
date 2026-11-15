# #168：Android 13 QSSI 跨代复用 `lineage-21.0` 底包的全链路启动与 HAL 兼容收口

## 1. 问题

在完成 #167 的 `secilc` 策略版本对齐使 `ASUS ROG Phone 7`（`asus/ASUS_AI2205`，版本 `13-TKQ1.220928.001-20240229`）进入二阶段 `init` 后，Android 13（API 33）QSSI 官方系统分区与 Android 14（`lineage-21.0`）VNDK-less 基底及 `android14` 虚拟 HAL 之间暴露了 8 处跨代兼容与启动崩溃卡点：

1. **内核模块加载被 SELinux `lockdown` LSM 拦截**：官方 Android 13 的 `plat_pub_versioned.cil` 仅授予 `(allow domain self (lockdown (confidentiality)))`，缺少 `integrity`，导致 `dlkm_loader` 加载未签名 `.ko`（`virtio_snd.ko` 等）报 `avc: denied { integrity } lockdown_reason="unsigned module loading"`；
2. **纯 64 位与 VNDK 属性、VNDK-less 搜索路径及 VINTF 7.0 不匹配**：
   - 基线缺少纯 64 位锁与虚拟化图形属性，且 `vendor/build.prop` 继承了 `lineage-21.0` 基底的空 `ro.vndk.version=`，导致 linker 无法为官方 `ro.vndk.version=33` 创建 `vndk` 命名空间，只能退回 `/vendor/lib64/vndk` 子目录查找；
   - `hwcomposer@2.4` 依赖的 `android.hardware.graphics.composer@2.{1..4}.so` 在 `lineage-21.0` 基底中位于 `system_ext/apex/com.android.vndk.v34/` 而不在 `vendor/lib64/`；
   - `lineage-21.0` 基底 `vendor/etc/vintf/` 下的 XML 版本为 `8.0`（`target-level="8"`、`<sepolicy><version>202404</version></sepolicy>`），官方 Android 13 `libvintf` 仅支持到 schema `7.0`，导致 `surfaceflinger` 报 `VINTF parse error`；
3. **ELF 跨代符号与 `DT_NEEDED` 缺失**：
   - `lineage-21.0` 基底的 `vendor/lib64/libbinder.so` 带有未实际调用的 `DT_NEEDED: libapexsupport.so`（Android 13 `/system/lib64/` 无此库），导致 `sensors`、`camera` 等 HAL 无法链接；
   - `android14` 编译的 `libaudio_aidl_conversion_common_ndk.so` 等库强引用 Android 14 `libbinder_ndk.so` 新增的 31 个 `APersistableBundle_*` 符号（Android 13 `/system/lib64/libbinder_ndk.so` 尚未导出）；
4. **QSSI Android 13 `libaudiohal.so` 硬编码 HIDL 4.0 导致 `audioserver` 无限崩溃**：官方 `/system/lib64/libaudiohal.so` 的 `sAudioHALVersions` 表仅含 HIDL `7.1 / 7.0 / 6.0 / 5.0 / 4.0`（无 AIDL），`DevicesFactoryHalHidl` 找不到 HIDL Audio 触发 `audioserver` `SIGABRT` 循环；
5. **Lights HAL 缺 `light_config.xml` 返回 0 盏灯触发 `NotificationManagerService` 空指针**：`lights_service.cpp` 在无 `/vendor/etc/light_config.xml` 时返回空灯列表，官方 `NotificationManagerService.updateLightsLocked` 对 `mAttentionLight` 解引用触发 `NullPointerException` 重启 `system_server`；
6. **WiFi HAL `getLinkLayerStats` 触发 `system_server` 数组越界与除零崩溃**：
   - `wifi_service.cpp` 的 `getLinkLayerStats` 原先返回空 `links` / `radios`，触发 `AidlStaIface.getWifiLinkLayerStats` `ArrayIndexOutOfBoundsException: length=0; index=0`；
   - `out->timeStampInMs` 默认为 `0`，连续两次轮询 `timeDelta = newStats.timeStampInMs - oldStats.timeStampInMs == 0`，而官方 `WifiDataStall.isThroughputSufficientInternal(WifiDataStall.java:536)` 的字节码 `if-ltz v8` 仅跳过 `< 0` 而不跳过 `== 0`，在 Wi-Fi 连接约 24 秒后触发 `ArithmeticException: divide by zero` 重启 `system_server`；
7. **QTI 蓝牙栈 `libbluetooth_qti.so` 创建 `CLOCK_BOOTTIME_ALARM` 定时器失败触发 `FATAL:alarm.cc(179)`**：官方 `/system_ext/lib64/libbluetooth_qti.so` 在 `alarm_new_internal` 中调用 `timer_create(CLOCK_BOOTTIME_ALARM=9)` 失败（`EPERM`），触发 `Check failed: false` 使 `com.android.bluetooth` 反复 `SIGABRT`；
8. **官方 `vendor_sepolicy.cil` 公共属性、平台域 uevent 规则与自定义 `vendor_service_contexts` 缺失**：
   - 基底缺少官方 `system_property_type` / `vendor_public_property_type`（17 项，如 `vendor_asus_touch_prop`），导致 `system_server` 设置 `vendor.screen.rotation` 时因 `SELinux avc: denied { set }` 抛出 `RuntimeException`；
   - 基底缺少官方 `vendor_sepolicy.cil` 为核心平台域补充的授权规则（如 `(allow radio_33_0 radio_33_0 (netlink_kobject_uevent_socket (read create bind setopt)))`），导致 `com.android.phone` 的 `UEventObserver` 线程因 `Permission denied` 无限崩溃重启；
   - 基底缺少官方 `vendor_service_contexts` 与 `vendor_sepolicy.cil` 定义的 18 项自定义 `service_manager_type`（如 `vicewifi` -> `vicewifi_service`、`wifi_extended_service` -> `wifiextended_service`），导致 `SystemServer` 注册 `"vicewifi"` 被拒，在冷启动第 102 秒分发 `ACTION_BOOT_COMPLETED` 时 `OsuNetworkConnection.init` 因获取不到 `"vicewifi"` 服务触发 `NullPointerException` 重启 `system_server`；
9. **高通 QSSI `vendor/build.prop` 未写死 `ro.sf.lcd_density` 被 `init.virt.rc` 兜底为 `160` dpi，且 `vendor/etc/permissions` 残留基底 `tablet_core_hardware.xml`，导致手机被识别为 `tablet`（平板）及 `com.android.settings` 空指针闪退**：
   - 真机由 `init.qcom.early_boot.sh` 动态设置 `vendor.display.lcd_density` 再由 `init.qcom.rc` 赋给 `ro.sf.lcd_density`，各分区 `build.prop` 原件均未定义 `ro.sf.lcd_density`；云机改用 `lineage` 基底的 `/vendor/etc/init/hw/init.virt.rc` 后，`early-init` 执行 `setprop ro.sf.lcd_density ${ro.boot.lcd_density:-160}`，将只读属性 `ro.sf.lcd_density` 设为 `160`；
   - 未声明替换 `etc/permissions` 时，`vendor/etc/permissions/` 仍保留 `lineage-21.0` 基底的 `tablet_core_hardware.xml` 与 `android.hardware.sensor.hinge_angle.xml`，缺少官方 `handheld_core_hardware.xml` 与 `android.hardware.fingerprint.xml`：不仅使设备被识别为 `tablet`，还导致 `SystemServer` 不启动 `FingerprintService`（`Service fingerprint: not found`），华硕 `ROGAsusSettings.apk` 在 `FingerprintStatusUtils.hasEnrolled(FingerprintStatusUtils.java:100)` 中未判空直接解引用 `mFingerprintManager.hasEnrolledFingerprints(mUserId)` 触发 `NullPointerException` 闪退；同时若直接整目录拷入官方 `etc/permissions` 且未启用 `android.hardware.nfc` 模块，残留的 `*nfc*.xml` / `*mifare*.xml` 会激活 `FEATURE_NFC`，导致常驻应用 `com.android.nfc` 因无 NFC HAL 而无限崩溃重启；
10. **`halclientdomain` 缺失版本化平台域导致 `vendor.perfservice` 死循环与全系统 `BoostFramework` 死锁 ANR，以及自定义 `coredomain` 缺失导致 `AsusRouteMonitor` 崩溃**：
    - `lineage-21.0` 基底 `vendor_sepolicy.cil` 的 `halclientdomain` 不含官方 `vendor_perfservice_33_0`、`system_app_33_0`、`platform_app_33_0`、`priv_app_33_0` 等 29 个版本化域；当 `/system_ext/bin/perfservice`（`u:r:vendor_perfservice:s0`）收到应用 `BoostFramework.<init>` 的同步 Binder 请求去查询 HIDL `IPerf` 时，因无权读取 `hwservicemanager.ready` 属性而在 `WaitForProperty("hwservicemanager.ready", "true")` 中陷入死循环，把 `com.android.settings` 等所有应用主线程卡死在 `IPerfManager$Stub$Proxy.setClientBinder` 上触发超时 ANR；
    - 官方 `/system/etc/init/AsusPingMonitor.rc` 与 `AsusRouteMonitor.rc` 使用官方 `vendor_sepolicy.cil` 声明的非版本化 `coredomain`（`AsusPingMonitor`、`AsusRouteMonitor`），基底缺失其类型与 `netlink_route_socket` 授权导致 `init` 无法 `setexeccon` 或守护进程因 `FD_SET(-1)` 触发 `SIGABRT`（`sys.init.updatable_crashing=1`）；
11. **虚拟 Thermal HAL 缺省传感器列表为空导致 `com.asus.services` 循环崩溃，以及 `AsusLauncher` 首启自动拉起横屏 `ArAppWelcome`**：
    - 机型未提供 `/vendor/etc/thermal_config.xml` 时，`thermal_service.cpp` 的 `loadConfig()` 返回空传感器列表，`HardwarePropertiesManager.getDeviceTemperatures(DEVICE_TEMPERATURE_SKIN, TEMPERATURE_CURRENT)` 返回空数组 `float[0]`，华硕 `/system/app/FocusAppListener/FocusAppListener.apk`（运行于 `com.asus.services`，UID 1000）直接访问 `aget v8, v8, 0` 触发 `ArrayIndexOutOfBoundsException: length=0; index=0` 循环崩溃；
    - `AsusLauncher` 在 `onCreate` 中检测到 `user_setup_complete == 1` 且 `Settings.System.getInt("haslaunched_ArAppWelcomeActivity", 0) == 0` 时自动拉起 `com.asus.ar.welcome/.ArAppWelcome`，而 `MainArWelcome.onCreate` 在 `this.finish()` 后未 `return` 继续执行 `setRequestedOrientation(SCREEN_ORIENTATION_LANDSCAPE)`，将刚启动的桌面强制切为横屏 `ROTATION_90`。

## 2. 根因

1. **VNDK 33 与 VNDK-less 基底（Android 14+）的命名空间契约差异**：
   - Android 13 的 `/system/etc/ld.config.txt` 依赖 `ro.vndk.version=33` 激活 `[vendor]` 的 `vndk` 命名空间，并将 `/vendor/lib64/vndk` 与 `/vendor/lib64/vndk-sp` 加入搜索路径；而 `lineage-21.0` 将 `ro.vndk.version` 置空且不带 `lib64/vndk` 子目录，必须保留官方 `ro.vndk.version` 并在 `vendor/lib64/` 下建立 `vndk -> .` 与 `vndk-sp -> .` 符号链接，同时从基底 VNDK APEX 回填 `composer@2.{1..4}.so`。
2. **Android 13 平台二进制与 Android 14 vendor 产物的双向代差**：
   - 正向（vendor 调 system）：`lineage-21.0` 的 `libbinder.so` 多出未使用的 `libapexsupport.so` 依赖，`android14` HAL 编入的 `libaudio_aidl_conversion_common_ndk.so` 引用了 Android 14 新增但启动期未实际执行的 `APersistableBundle_*` 符号；
   - 反向（system 预期的 HAL 形态）：QSSI Android 13 的 `libaudiohal.so` 已内建完整的 `libaudiohal@aidl.so` 加载分支（`type=1, major=1`），仅因 `sAudioHALVersions[4]` 写死 `{0, 4, 0}`（HIDL 4.0）而未走 AIDL；QTI 蓝牙栈 `libbluetooth_qti.so` 在无 `CAP_WAKE_ALARM` 时对 `CLOCK_BOOTTIME_ALARM(9)` 直接 `LOG(FATAL)`，改为 `CLOCK_BOOTTIME(7)` 即可正常创建定时器。根据 `AGENTS.md` 第 8 条，严禁修改 `super.img`，必须在 `07_build_vendor` 中提取官方 `.so` 修补后落入 `/vendor/lib64/*.sys.so`（打 `system_lib_file` 标签），并由 `05_build_vendor_boot` 自动注入 `cdh_redirect` 单文件重定向规则。
3. **跨代替换 `vendor_sepolicy.cil` 后丢失厂商对 `/system` 框架与常驻应用的策略契约**：
   - 官方 `/system` 与 `/system_ext` 中的 `services.jar`、`vicewifi-service.jar`、`TeleService.apk` 等按厂商 `vendor_sepolicy.cil` 编写，依赖厂商定义的公共属性类型、自定义 `service_manager_type` 服务名映射，以及厂商在 `vendor_sepolicy.cil` 中为 `radio_33_0`、`system_server_33_0`、`system_app_33_0` 等核心平台域追加的平台类型 `allow` 规则。用 `lineage-21.0` 基底替换 `vendor` 后，必须在 `alignSepolicyVersion` 阶段从官方 `vendor_sepolicy.cil`、`vendor_property_contexts`、`vendor_service_contexts` 自动同步这些契约。

## 3. 证据

详见 [log-fail.txt](log-fail.txt) 与 [log-pass.txt](log-pass.txt)：
- **音频与蓝牙二进制静态取证**：
  - `system/lib64/libaudiohal.so`（19592 B）的 `.rodata` 段含 5 项 12 字节结构体 `sAudioHALVersions`，末项 `00000000 04000000 00000000`（HIDL 4.0）改为 `01000000 01000000 00000000`（AIDL 1.0）后，`DevicesFactoryHalInterface::createPreferredImpl` 直接加载 `/system/lib64/libaudiohal@aidl.so` 对接虚拟 Audio AIDL HAL；
  - `system_ext/lib64/libbluetooth_qti.so` 的 `alarm_new_internal`（`0x475c84`）处指令 `52800120`（`mov w0, #9`）之后紧跟 `bl timer_create_internal`（`97ffff92`），改为 `528000e0`（`mov w0, #7`）后不再触发 `alarm.cc(179)` 断言。
- **Wi-Fi 除零与 `vicewifi` 空指针反编译取证**：
  - 官方 `service-wifi.jar` 的 `WifiDataStall.isThroughputSufficientInternal(WifiDataStall.java:536)`：`long timeDelta = newStats.timeStampInMs - oldStats.timeStampInMs; if (timeDelta < 0) return true; int txSuccessRate = (int) ((txDelta * 1000) / timeDelta);`，当 `timeStampInMs` 恒为 `0` 时 `timeDelta == 0` 触发除零；
  - 官方 `vicewifi-service.jar` 的 `OsuNetworkConnection.init(OsuNetworkConnection.java:122)` 在 `ACTION_BOOT_COMPLETED` 回调中调用 `mWifiViceManager.isWifiEnabled()`，因 `servicemanager` 在 `8.59s` 拒绝 `u:r:system_server:s0` 向 `default_android_service` 注册 `vicewifi`，导致 `mWifiViceManager` 为 `null`。

## 4. 改动

1. **属性、屏幕密度、VNDK 搜索路径与 VINTF 对齐**：
   - [engine/build/01_unpack/prepare.go](file:///Volumes/work/cloud-phone-hub/engine/build/01_unpack/prepare.go)、[engine/build/internal/device/config.go](file:///Volumes/work/cloud-phone-hub/engine/build/internal/device/config.go) 与 [engine/build/assets/devices.db](file:///Volumes/work/cloud-phone-hub/engine/build/assets/devices.db)：补齐纯 64 位锁、虚拟化图形属性与 `ro.sf.lcd_density=440`，保留官方 `ro.vndk.version`；
   - [engine/build/07_build_vendor/plan.go](file:///Volumes/work/cloud-phone-hub/engine/build/07_build_vendor/plan.go)、[engine/build/07_build_vendor/props.go](file:///Volumes/work/cloud-phone-hub/engine/build/07_build_vendor/props.go)、[engine/build/07_build_vendor/fstab.go](file:///Volumes/work/cloud-phone-hub/engine/build/07_build_vendor/fstab.go) 与 [engine/build/07_build_vendor/props_test.go](file:///Volumes/work/cloud-phone-hub/engine/build/07_build_vendor/props_test.go)：当 `device.json` 声明了 `screen.dpi`（`plan.Density > 0`）且 `vendor/build.prop` 与基线均未设置 `ro.sf.lcd_density` 时自动补齐 `ro.sf.lcd_density=<Density>`，同时将 `vendor/etc/init/hw/init.virt.rc` 中的 `${ro.boot.lcd_density:-160}` 收口为 `${ro.boot.lcd_density:-<Density>}`，防止缺省 `160` dpi 触发 `sw>=600dp` 平板模式；
   - [engine/build/07_build_vendor/hal.go](file:///Volumes/work/cloud-phone-hub/engine/build/07_build_vendor/hal.go) 与 [engine/build/07_build_vendor/assemble.go](file:///Volumes/work/cloud-phone-hub/engine/build/07_build_vendor/assemble.go)：新增 `backfillComposerHidlLibs`（从基底 VNDK APEX 回填 `composer@2.{1..4}.so`）、`ensureVndkSymlinks`（创建 `vendor/lib64/vndk -> .` 与 `vndk-sp -> .`）、`syncOfficialHandheldPermissions`（当装配树残留 `tablet_core_hardware.xml` 而官方 vendor 含 `handheld_core_hardware.xml` 时自动以官方 `etc/permissions` 替换）与 `pruneUnsupportedHardwarePermissions`（未启用 `android.hardware.nfc` 时自动剔除 `*nfc*.xml` / `*mifare*.xml`）；
   - [engine/build/07_build_vendor/vintf.go](file:///Volumes/work/cloud-phone-hub/engine/build/07_build_vendor/vintf.go) 与 [engine/build/07_build_vendor/vintf_test.go](file:///Volumes/work/cloud-phone-hub/engine/build/07_build_vendor/vintf_test.go)：新增 `alignVintfCompatibility`，按官方 VINTF 自动对齐 schema 版本（`7.0`）、`target-level="7"` 与 `<sepolicy><version>33.0</version></sepolicy>`。
2. **ELF 跨代兼容与音频/蓝牙零二进制修补收口（2026-11-08 更新）**：
   - 彻底移除 `internal/elfpatch` 对官方 `.so` 二进制的字节修补；
   - **QTI 蓝牙 `CLOCK_BOOTTIME_ALARM` 内核级兼容**：在 [devices/modules/device_info/time/uptime.c](file:///Volumes/work/cloud-phone-hub/devices/modules/device_info/time/uptime.c) 中新增 `__arm64_sys_timer_create` kprobe（`cdh_timer_create_pre`），当内核无 EFI RTC 唤醒设备（`!alarmtimer_get_rtcdev()`）或进程无 `CAP_WAKE_ALARM` 时，将 `CLOCK_BOOTTIME_ALARM(9)` / `CLOCK_REALTIME_ALARM(8)` 透明降级为 `CLOCK_BOOTTIME(7)` / `CLOCK_REALTIME(0)`，零修改官方 `libbluetooth_qti.so`；
   - **QSSI Audio HAL 原样回填 + VINTF 兼容**：在 [engine/build/stages/07_build_vendor/elfcompat.go](file:///Volumes/work/cloud-phone-hub/engine/build/stages/07_build_vendor/elfcompat.go) 中由 `backfillQSSIAudioHalAidl` 从官方 `system_a.img` **原样提取（0 字节修改）** `/system/lib64/libaudiohal@aidl.so` 到 `/vendor/lib64/libaudiohal.sys.so`，配合 [ASUS_AI2205-android13.json](file:///Volumes/work/cloud-phone-hub/devices/manifest/configs/asus/ASUS_AI2205/ASUS_AI2205-android13.json) 的 `etc/vintf/manifest/audio_qssi_compat.xml` 与 `/system/lib64/libaudiohal@7.1.so -> /mnt/vendorpatch/vendor/lib64/libaudiohal.sys.so` 重定向规则，使原厂未修改的 `libaudiohal.so` 直接加载同批编译的原厂 `libaudiohal@aidl.so`。
3. **Lights HAL、WiFi HAL、Thermal HAL 与首启显示防呆完善**：
   - [engine/vendor/hardware/android.hardware.light/src/lights_service.cpp](file:///Volumes/work/cloud-phone-hub/engine/vendor/hardware/android.hardware.light/src/lights_service.cpp)：无 `/vendor/etc/light_config.xml` 时回退提供 8 盏标准灯（含 `NOTIFICATIONS` / `ATTENTION`）；
   - [engine/vendor/hardware/android.hardware.wifi/src/wifi_service.cpp](file:///Volumes/work/cloud-phone-hub/engine/vendor/hardware/android.hardware.wifi/src/wifi_service.cpp)：`getLinkLayerStats` 填充严格单调递增的 `out->timeStampInMs`、`radio.onTimeInMs` 以及默认 `StaLinkLayerLinkStats`（`linkId=0, state=IN_USE`）与 `StaLinkLayerRadioStats`（`radioId=0`）；
   - [engine/vendor/hardware/android.hardware.thermal/src/thermal_service.cpp](file:///Volumes/work/cloud-phone-hub/engine/vendor/hardware/android.hardware.thermal/src/thermal_service.cpp)：无 `/vendor/etc/thermal_config.xml` 时由 `ensureDefaultSensors` 回退提供 `CPU(0)`、`GPU(1)`、`BATTERY(2)`、`SKIN(3)` 四温区默认传感器；
   - [engine/vendor/hardware/android.service.daemon/vendor/bin/cph-setup-display](file:///Volumes/work/cloud-phone-hub/engine/vendor/hardware/android.service.daemon/vendor/bin/cph-setup-display)：在置位 `user_setup_complete=1` 之前先行置位 `settings put system haslaunched_ArAppWelcomeActivity 1` 并禁用 `com.asus.ar.welcome`，防止 `AsusLauncher` 首启自动拉起横屏 AR 引导页。
4. **SELinux 跨代策略补齐（`lockdown`、公共属性、核心平台域规则、自定义服务、`halclientdomain` 与自定义 `coredomain`）**：
   - [engine/build/07_build_vendor/sepolicy.go](file:///Volumes/work/cloud-phone-hub/engine/build/07_build_vendor/sepolicy.go) 与 [engine/build/07_build_vendor/sepolicy_test.go](file:///Volumes/work/cloud-phone-hub/engine/build/07_build_vendor/sepolicy_test.go)：
     - `mergeOfficialPlatPubVersioned` 将 `(lockdown (confidentiality))` 提升为 `(lockdown (integrity confidentiality))`；
     - 新增 `syncOfficialPublicVendorProperties`：同步官方 `system_property_type` / `vendor_public_property_type` 属性类型（17 项）、属性前缀映射及核心平台域（`system_server`、`system_app`、`platform_app`、`priv_app`、`radio`、`bluetooth`）对平台公有类型的 `allow` 规则（63 条）；
     - 新增 `syncOfficialVendorServices`：同步官方 `service_manager_type` 自定义服务类型（18 项，含 `vicewifi_service`、`wifiextended_service` 等）、`halclientdomain` 版本化成员（29 项，含 `vendor_perfservice_33_0`、`system_app_33_0` 等）与非版本化自定义 `coredomain` 守护进程域（5 项，含 `AsusPingMonitor`、`AsusRouteMonitor` 及其 `netlink_route_socket` 规则，共 52 项）。

## 5. 验证

详见 [log-pass.txt](log-pass.txt)：
1. **单元测试**：`cd engine/build && go test ./...` 全量通过。
2. **构建验证**：`ASUS_AI2205` 重跑 `07 -> 09 -> 11`，日志输出 `替换官方 plat_pub_versioned.cil 并补齐跨代兼容 typeattribute 254 项`、`同步官方公共/系统可见 vendor 属性类型 17 项`、`同步官方自定义 vendor 服务类型 52 项`，产出 `patch.img`（sha256 `66e5c3e1aa39`）与 `vda.img`（13.00 GiB）。
3. **冷启动、`phone` 识别、竖屏显示与 `Settings` 稳定性验证**：
   - 全新创建实例 3 冷启动，`getenforce=Enforcing`、`sys.boot_completed=1`、`init.svc.bootanim=stopped`、`ro.sf.lcd_density=440`、`sys.init.updatable_crashing` 为空、`AsusPingMonitor` 与 `AsusRouteMonitor` 均为 `running`；
   - `/vendor/etc/permissions/` 含 `handheld_core_hardware.xml` 与 `android.hardware.fingerprint.xml`，无 `tablet_core_hardware.xml`、`hinge_angle` 与 `nfc`，`pm list features` 正确识别为 `phone` 并包含 `feature:android.hardware.fingerprint`；
   - 开机全程保持 `mCurrentRotation=ROTATION_0`（竖屏 `sw393dp w393dp h801dp 440dpi port`），无 `ArAppWelcome` 横屏切换；
   - `dumpsys hardware_properties` 正常返回四温区温度（`Skin temperatures: [41.747665]`），`com.asus.services`（PID `1355`）稳定运行零崩溃；
   - `am start -W -n com.android.settings/.Settings` 正常秒开（`Status: ok`，`Activity: com.android.settings/.homepage.SettingsHomepageActivity`），`com.android.settings`（PID `1926`）稳定前台运行不闪退、不超时。

## 6. 遗留

- 库内其余 7 台 Android 13 模版（`ARW`、`HNLNA-M`、`bengal_515`、`itel-A6612L`、`PJE110`、`RMX3823`、`RMX5112`）已共享上述全链路兼容逻辑，待各自固件构建验收时按需复核。
