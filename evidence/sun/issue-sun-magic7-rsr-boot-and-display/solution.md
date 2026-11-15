# #171 Honor Magic7 RSR（honor/sun）全链路起机、原生 1280×2800@560dpi 显示与定制内置应用收口

- **设备/机型**：Honor Magic7 RSR Porsche Design（`honor/sun`，`PTP-N59`，MagicOS 9 / Android 15，基线 `15-AQ3A.250214.001-20260904`，`lineage-22.2` + `kernel 6.6`）
- **状态**：已闭环（2026-11-07）

## 一、问题

制作 Honor Magic7 RSR（`honor/sun`）云机并引导进入桌面过程中，依次遇到 8 个卡点：

1. **荣耀平铺逻辑分区缺失**：官方固件包不含 `super.img`，而是以平铺 `.img` 形式提供 `system`、`system_ext`、`product`、`vendor`、`vendor_dlkm`、`odm` 以及荣耀专有的 `product_h`、`preas`、`cust`、`version`、`preload`、`ai_model`、`ai_model_vendor` 分区；`01_unpack` 与 `10_pack_super` 未收录上述 7 个荣耀分区名，导致二阶段挂载找不到逻辑分区。
2. **同版本 Android 15（`202404`）SELinux CIL 平台自定义类型缺失**：二阶段 `init` 调用 `/system/bin/secilc` 编译策略时因找不到荣耀 `system/etc/selinux/plat_sepolicy.cil` 引用的平台自定义类型/属性与 `hal_service_type` 触发 `InitFatalReboot`。
3. **`FilteredPreloadAppManager` 抛 `NoSuchElementException` 崩 `system_server`**：`system_server` 在 `LoadFilteredPreloadRunnable.run` 中调用 `vendor.honor.hardware.hwfactoryinterface.IFactoryInterface/default`（通过 `libhwfactoryinterface.jni.so`）获取出厂预装配置时，因服务未注册直接抛出未捕获的 `NoSuchElementException` 导致 `system_server` 崩溃循环。
4. **`SensorService` 在 `convertToSensorEvent` 触发 `EventPayload::get<Tag::data>(): bad access: a wrong tag` 断言崩溃**：`system_server` 初始化传感器服务时在 `libsensorservice.so!android::SensorDeviceUtils::HidlSensorEventQueue::convertToSensorEvent` 收到 `SIGABRT`。
5. **`PersistentDataBlockService` 在 phase 500 超时崩溃**：`system_server` 等待 `PersistentDataBlockService.onBootPhase(500)` 打开 `/dev/block/bootdevice/by-name/frp` 失败而超时自杀；且初次通过 `etc/ueventd.qcom.rc` 赋权时被荣耀 `ueventd` 忽略（报 `Legacy ueventd configuration file detected and will not be parsed: /vendor/etc/ueventd.qcom.rc`）。
6. **进入桌面后「分辨率不对，底部偏大」**：进入 `UniHomeLauncher` 后，整个界面及底部 Dock 栏、`搜索` 按钮严重偏大，底部 Dock 直接遮挡压住桌面第 5 行应用图标（`我的荣耀`、`应用市场`、`Play 商店`、`Google`），且 `智慧建议` 小组件文字被截断为 `点击... / 畅享...`。
7. **大量内置应用与保时捷设计专属主题缺失**：进入桌面后缺失相机（`HnCamera2`）、图库（`Gallery2`）、运动健康（`HnHealthApp`）、荣耀钱包（`HonorWallet`）、录音机（`HnRecordAssistant`）、荣耀文档（`HnOffice`）、通话录音（`HnCallRecorder`）、智慧空间（`MagicHome`）及 `/preload` 预装应用，且桌面未加载保时捷设计专属黑金主题（`PORSCHEDESIGNHONORMagic7RSR.hwt`），`ro.build.fingerprint` 显示异常 `HONOR//HNPTP...preload format error.`。
8. **激活定制目录后原厂 `/vendor/app` 高通基带应用崩溃触发 `RescueParty` 重启**：恢复 `PTP-N59` 定制目录与 `frameworkResOverlay.apk` 后，原厂 `/vendor/app/CneApp` + `/vendor/app/IWlanService`（共享 persistent 进程 `.qtidataservices`）被激活调用 `vendor.qti.iwlan.JniHelper.getAllQualifiedNetworks(int, int)`，因云机 `/vendor/lib64` 不含高通专有 `libiwlanjni.so` 抛 `UnsatisfiedLinkError` 并在 4 秒内连续崩溃 5 次，触发 `RescueParty` 执行 `sys.powerctl='reboot,RescueParty'`。

## 二、根因

1. **荣耀平铺逻辑分区**：`01_unpack/official.go` 的 `flatPartitions` 与 `10_pack_super/layout.go` 的 `slotLogicalOrder` / `plainLogicalOrder` 未包含 `product_h`、`preas`、`cust`、`version`、`preload`、`ai_model`、`ai_model_vendor`。
2. **SELinux `202404` 合并门限**：`07_build_vendor/sepolicy.go` 原本仅在 `officialVers != baseVers`（跨版本降级，如 Android 13 `33.0`）时合并官方 `plat_pub_versioned.cil`、公共 vendor 属性与自定义服务；`sun` 官方与 `lineage-22.2` 基底同为 `202404`（Android 15），跳过了官方平台导出类型同步。
3. **`hwfactoryinterface` AIDL 服务缺失**：荣耀框架 `FilteredPreloadAppManager` 强依赖 `/vendor/bin/hw/vendor.honor.hardware.hwfactoryinterface-service` 注册 `vendor.honor.hardware.hwfactoryinterface.IFactoryInterface/default`。实测该二进制仅依赖 `/vendor/lib64/libc_secshared.so` 与 `vendor.honor.hardware.hwfactoryinterface-V1-ndk.so`，无内核私有驱动节点强依赖，缺失时直接崩 `system_server`。
4. **荣耀 `libsensorservice.so` 魔改 `SENSOR_TYPE_LIGHT (5)` 与 `SENSOR_TYPE_PROXIMITY (8)` 的 AIDL `EventPayload` union tag**：
   - 反汇编官方 `/system/lib64/libsensorservice.so` 的 `convertToSensorEvent`（偏移 `0x39d64`~`0x3a6a0`，跳转表位于 `0x163d0`）确认：标准 AOSP 对 `type 5`（光线）和 `type 8`（距离）读取 `EventPayload::Tag::scalar (4)`，而荣耀将其改成了跳转到 `0x3a084` 读取 `EventPayload::Tag::data (0xa)`（`SensorsEventPayloadData`，前 3 个 float）！
   - 云机虚拟传感器 HAL（`engine/vendor/hardware/android.hardware.sensors`）按标准 AOSP 对 `type 5` / `type 8` 填充 `set<Tag::scalar>(1.0f)`（tag = `4`），荣耀 `libsensorservice.so` 调 `get<Tag::data>()` 检查 `tag == 0xa` 失败直接 `abort()`。
5. **FRP 块设备软链与荣耀 `ueventd` 配置路径限制**：
   - `ro.frp.pst=/dev/block/bootdevice/by-name/frp`，云机 `vda3`（`misc`）未建立该软链且默认权限 `0600 root:root`；
   - 荣耀定制版 `ueventd` 硬编码拒绝解析 `/vendor/etc/ueventd.qcom.rc`（报 `Legacy ueventd configuration file detected`），只解析 `/vendor/etc/ueventd.rc`。
6. **屏幕物理分辨率/DPI 错配（`1080×2400 @ 440` vs 原生 `1280×2800 @ 560`）**：
   - `/product_h/etc/prop/local.prop` 写明了荣耀 Magic7 RSR 真机原生屏幕规格：`ro.config.default_screensize=1280,2800`、`ro.sf.lcd_density=560`、`ro.sf.real_lcd_density=560`、`msc.config.new_screen_aspect=2800:2664:1280`。
   - 反编译 `/system/framework/services-magic.jar` 的 `com.android.server.wm.HwRogEx`（`processMultiDpi` / `setCurrentRatio`）确认：荣耀 WindowManager 启动时优先读取 `ro.sf.real_lcd_density`（`560`）并调用 `setForcedDisplayDensityForUser(0, 560, userId)`，同时按 `ro.sf.lcd_density / ro.sf.real_lcd_density` 计算 `persist.sys.rog.currentratio`。
   - `devices.db` 中 `sun`（`devices.id=10`）最初填了占位值 `1080x2400`、`density=440`，导致 QEMU 物理屏幕只有 `1080×2400`，却被 `HwRogEx` 强制按 `560 dpi` 渲染（逻辑尺寸仅 `sw309dp w309dp h686dp`，`persist.sys.rog.currentratio=0.78571427`），整个界面和底部 Dock 被放大 `560/440 = 1.27` 倍，6 行桌面网格（总高 `2142 px`）在 `2400 px` 高度下与底部锚定的 `Hotseat`（`y=1941..2284`）发生严重重叠。
7. **缺少 `/proc/cmdline` 荣耀 OEMINFO 定制参数（`vendorcountry=porsche/normal`、`devmodel=PTP-N59`）且 `SynthesizeDeviceJSON` 漏读 `verVendorBoot["cmdline"]`**：
   - 逆向官方 `/system/bin/init`（内嵌 `libhwcust_init`）与 `/system/lib64/liboeminfo_nvme_recovery.so`（被 `/system/bin/cust_init` 链接），在 `g_oeminfoCmdlineTab`（`init` 偏移 `0x3de820`）与 `ReadOeminfoFromCmdline`（`init` 偏移 `0x36eff4`）查明：荣耀二阶段 `init` 的 `cust_parse` 与 `set_cust_info` 通过 `/proc/cmdline` 中的 `vendorcountry=`（ID 18，`OEMINFO_VENDER_AND_COUNTRY_NAME`）和 `devmodel=`（ID 97，`OEMINFO_DEVICE_MODEL`）读取定制渠道与设备型号，且 `ReadOeminfoFromCmdline` 使用 `strchr(val, ' ')` 查找空格作为值结束符（要求后面必须紧跟其它 cmdline 参数以保证带空格）。
   - 未注入这两个参数时，`msc.sys.vendor` 回退为 `def`、`msc.sys.country` 回退为 `default`、`msc.sys.oemName` 为空，`CUST_POLICY_DIRS` 漏掉了 `/product_h/hn_oem/PTP-N59`（含 `parent_cust.cfg -> /product_h/region_comm/oversea` 及保时捷设计主题）、`/product_h/region_comm/oversea`（含相机、图库、运动健康、荣耀钱包、录音机、荣耀文档、智慧空间等核心系统应用）、`/version/cust/porsche/normal`（含通话录音等）和 `/preload/PTP-N59/porsche/normal`（含商业预装应用）。
   - 同时发现 `engine/build/internal/device/db_bridge.go` 的 `SynthesizeDeviceJSON` 只读取了 `devExtra["cmdline"]`，漏读了 `verVendorBoot["cmdline"]`（`configs.buildVendorBoot.cmdline`），导致 `11_pack_vda` 未将配置表中的 `cmdline` 注入 EFI `grub.cfg`。
8. **原厂 `/vendor/app` 未屏蔽导致 `.qtidataservices` 连续崩溃触发 `RescueParty`**：
   - 原厂 `/vendor/app` 下存有 `CACertService`、`CneApp`、`IWlanService`、`TimeService`、`TxPwrAdmin`、`com.honor.gpudrivers` 6 个高通基带/Adreno GPU 强耦合应用；其中 `CneApp` 与 `IWlanService` 共享 `persistent="true"` 进程 `.qtidataservices`。激活 `PTP-N59` 定制 overlay 后，Telephony 绑定 `IWlanService` 并调用 JNI `getAllQualifiedNetworks`，因缺高通专有 `libiwlanjni.so` 抛 `UnsatisfiedLinkError` 高频崩溃触发 `RescueParty`。

## 三、证据

- 失败与通过日志摘录：[log-fail.txt](file:///Volumes/work/cloud-phone-hub/devices/evidence/sun/issue-sun-magic7-rsr-boot-and-display/log-fail.txt)、[log-pass.txt](file:///Volumes/work/cloud-phone-hub/devices/evidence/sun/issue-sun-magic7-rsr-boot-and-display/log-pass.txt)
- 截图对比：
  - 分辨率修复前（`1080×2400 @ 560dpi`，`sw309dp`，底部偏大遮挡第 5 行图标）：[screen-fail-1080x2400.png](file:///Volumes/work/cloud-phone-hub/devices/evidence/sun/issue-sun-magic7-rsr-boot-and-display/screen-fail-1080x2400.png)
  - 分辨率修复后（原生 `1280×2800 @ 560dpi`，未注入 `vendorcountry`/`devmodel` 时缺内置应用）：[screen-pass-1280x2800.png](file:///Volumes/work/cloud-phone-hub/devices/evidence/sun/issue-sun-magic7-rsr-boot-and-display/screen-pass-1280x2800.png)
  - 定制参数注入后桌面第 1 页（保时捷设计黑金主题、相机、图库、运动健康卡片、应用市场、Play 商店等全部就位）：[screen-pass-apps-page1.png](file:///Volumes/work/cloud-phone-hub/devices/evidence/sun/issue-sun-magic7-rsr-boot-and-display/screen-pass-apps-page1.png)
  - 定制参数注入后桌面第 2 页（日常应用、实用工具、热门应用、智慧空间、荣耀 AI、Facebook、Booking、LinkedIn 等全部就位）：[screen-pass-apps-page2.png](file:///Volumes/work/cloud-phone-hub/devices/evidence/sun/issue-sun-magic7-rsr-boot-and-display/screen-pass-apps-page2.png)

## 四、改动

1. **平铺逻辑分区扩展**：
   - [official.go](file:///Volumes/work/cloud-phone-hub/engine/build/01_unpack/official.go) 与 [layout.go](file:///Volumes/work/cloud-phone-hub/engine/build/10_pack_super/layout.go)（含 [layout_test.go](file:///Volumes/work/cloud-phone-hub/engine/build/10_pack_super/layout_test.go)）：支持 `product_h`、`preas`、`cust`、`version`、`preload`、`ai_model`、`ai_model_vendor`。
2. **SELinux `202404` 平台公有类型同步**：
   - [sepolicy.go](file:///Volumes/work/cloud-phone-hub/engine/build/07_build_vendor/sepolicy.go)：将合并官方 `plat_pub_versioned.cil`、公共 vendor 属性及自定义 vendor 服务的条件扩展至 `officialVers == baseVers && parseSepolicyMajor(officialVers) <= 202404`，并在 `stdServiceTypeAttrs` 中补充 `"hal_service_type"`。
3. **虚拟传感器 HAL 支持 `payload="data"` 配置**：
   - [sensor_event_model.h](file:///Volumes/work/cloud-phone-hub/engine/vendor/hardware/android.hardware.sensors/src/sensor_event_model.h)、[sensor_event_model.cpp](file:///Volumes/work/cloud-phone-hub/engine/vendor/hardware/android.hardware.sensors/src/sensor_event_model.cpp)、[sensors_service.cpp](file:///Volumes/work/cloud-phone-hub/engine/vendor/hardware/android.hardware.sensors/src/sensors_service.cpp)：支持在 `sensors_config.xml` 的 `<sensor>` 节点声明 `payload="data"`，显式使用 `EventPayload::Tag::data` 填充传感器事件；在 `sun` 的 `etc/sensors_config.xml` 中为 `type-id="5"`（光线）和 `type-id="8"`（距离）配置 `payload="data"`。
4. **`SynthesizeDeviceJSON` 优先读取版本 `buildVendorBoot.cmdline`**：
   - [db_bridge.go](file:///Volumes/work/cloud-phone-hub/engine/build/internal/device/db_bridge.go#L297-L304)：`SynthesizeDeviceJSON` 合成 `device.json` 的 `cmdline` 字段时，优先从 `verVendorBoot["cmdline"]` 获取，若为空再回退 `devExtra["cmdline"]`。
5. **`devices.db` 中 `sun`（`devices.id=10`, `configs.id=10`）配置收口**：
   - `buildVendor.files` 引入官方 `vendor` 的 `hwfactoryinterface-service` 5 件套（二进制、2 个 `.so`、init rc、vintf manifest）；
   - `fileTrees` 增加 `etc/init/frp_symlink.rc`（软链 `/dev/block/bootdevice/by-name/frp -> /dev/block/vda3`）与 `etc/ueventd.rc`（`/dev/block/vda3 0660 root system`），配合 `vendor_sepolicy.cil` 授权；
   - 将 `devices.id=10` 的 `resolution` 修正为 `'1280x2800'`、`density` 修正为 `560`、`inch` 修正为 `'72x158'`；
   - 将 `configs.id=10` 的 `buildVendorBoot.cmdline` 与 `devices.id=10` 的 `extra.cmdline` 设为 `["vendorcountry=porsche/normal","devmodel=PTP-N59","androidboot.product.hardware.sku=PTP-N59"]`；
   - 在 `configs.id=10` 的 `buildModules.redirect` 中追加 `{"from":"/vendor/app","to":"/mnt/vendorpatch/vendor/app","type":"dir"}`，屏蔽原厂 `/vendor/app` 下的 6 个高通基带/Adreno GPU 专有应用。

## 五、验证

- `cd engine/build && go test ./01_unpack ./07_build_vendor ./10_pack_super ./internal/device ./11_pack_vda` 全部通过。
- 重跑 `05_build_vendor_boot` 与 `11_pack_vda`，删除旧实例并新建启动 `sun_15-AQ3A.250214.001-20260904/1`：
  - `qemu.log` 确认 `Kernel command line` 含 `vendorcountry=porsche/normal devmodel=PTP-N59 androidboot.product.hardware.sku=PTP-N59`，`libhwcust_init` 输出 `vendor_country=porsche/normal`、`get product name from oeminfo: PTP-N59`、`product_path=/product_h/hn_oem/PTP-N59`；
  - `msc.sys.vendor=porsche`、`msc.sys.country=normal`、`msc.sys.oemName=PTP-N59`、`ro.product.model=PTP-N59`、`ro.config.marketing_name=PORSCHE DESIGN HONOR Magic7 RSR`、`ro.build.fingerprint=HONOR/PTP-N59/HNPTPX:16/HONORPTP-N59/10.0.0.166C721E8R3P3:user/release-keys`、`persist.cust_service.done=true`；
  - `CUST_POLICY_DIRS` 完整包含 `/product_h/region_comm/oversea`、`/product_h/hn_oem/PTP-N59`、`/preload/PTP-N59/porsche/normal`、`/cust/def/normal`、`/version/cust/porsche/normal`、`/product_h/special_cust/PTP-N59/porsche/normal`、`/version/special_cust/PTP-N59/porsche/normal`；
  - 系统应用总数从 299 个恢复至 334 个（新增相机 `com.hihonor.camera`、图库 `com.hihonor.photos`、运动健康 `com.hihonor.health`、荣耀钱包 `com.hihonor.wallet`、录音机 `com.hihonor.recordassistant`、荣耀文档 `com.hihonor.hnoffice`、通话录音 `com.hihonor.phone.recorder`、智慧空间 `com.hihonor.magichome` 以及 `WPS`/`Booking`/`Trip`/`Facebook`/`LinkedIn` 等），保时捷设计专属黑金主题与壁纸自动生效，且全程零 `RescueParty` 重启。

## 六、遗留

- 无。
