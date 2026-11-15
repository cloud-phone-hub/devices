# CPH2797（OPPO Find X9 / ColorOS 16 / mt6993）起机全链路收敛：VINTF 裁剪、MTK PowerHAL 锁、NFC 崩溃回环、ODM Telephony/IR 遮蔽与触控适配

- **编号**：#165
- **机型**：OPPO Find X9（`oppo/CPH2797`，`16-BP2A.250605.015-20260529` / ColorOS `16.0.8.301_EX01`，MediaTek `mt6993`，Android 16 / `lineage-23.2`）
- **上游条目**：[../issue-cph2797-frp-block-device-missing/](../issue-cph2797-frp-block-device-missing/)（#153）

---

## 1. 问题

对 `CPH2797_16-BP2A.250605.015-20260529` 执行镜像构建与实例起机验证时，依次暴露出 5 个阻断或劣化问题：

1. **阶段 1（VINTF 冲突卡启动）**：实例启动后停在 `bootanim`，`servicemanager` 持续报错 `Cannot add VINTF manifest fragment /odm/etc/vintf/manifest/... HAL "..." may have a duplicated/conflictingoverride="false"entry` 及 `NULL VINTF MANIFEST!: device`，`system_server` 无法完成引导。
2. **阶段 2（AMS 全局锁阻塞 65s 触发 Watchdog 硬重启 + `com.android.nfc` 无限崩溃）**：恢复 `vintf-drop` 后虽能偶发进入 `sys.boot_completed=1`，但运行约 2 分钟即触发 `Watchdog: *** WATCHDOG KILLING SYSTEM PROCESS: Blocked in handler on foreground thread (android.fg) for 65s` → `sysrq: Resetting` 整机硬重启，且 `com.android.nfc` 每秒崩溃重启一次。
3. **阶段 3（Telephony `com.android.phone` 主线程永久卡死，`gsm.sim.state` 恒空）**：`com.android.phone`（PID 13403）虽连接上我方 Radio HAL（`mHalVersion=2.2`），但主线程每秒打印 `Waited one second for android.hardware.radio.ims.IRadioIms/slot1`，`Phone` 服务永不完成注册，`gsm.sim.state` 与运营商属性恒为空。
4. **阶段 4（常驻服务每 5s 退出触发 `sys.init.updatable_crashing=1`）**：`android-hardware-media-c2-hal`、`vendor-dolby-media-c2-hal`、`vendor.nfc_hal_service`、`oplus_esepower_hal_service`、`secure_element-1-0`、`vendor.ir-default`、`magt`、`netdiag` 每 5 秒退出重启，触发 `flags_health_check UPDATABLE_CRASHING`。直接停用 `vendor.ir-default` 并遮蔽其 VINTF 后，`system_server` 在 `ConsumerIrService.<init>` 抛出 `RuntimeException: FEATURE_CONSUMER_IR present, but no IR HAL loaded!`。
5. **阶段 5（Web VNC 触控无响应）**：进入桌面后鼠标点击/滑动无响应。

---

## 2. 根因

1. **VINTF 冲突（`vendor.vintf-drop` 遗漏）**：
   清单入库至 `engine/build/assets/devices.db`（`versions` 表 `id=27`）时，遗漏了原 JSON 基线中的 `vendor.vintf-drop`（6 项），导致我方注入 `/vendor/etc/vintf/manifest/` 的 `omapi`、`face`、`vibrator`、`weaver`、`fingerprint`、`nfc` 与 `/odm/etc/vintf/manifest/` 官方同名 HAL 声明冲突，`libvintf` 合成设备 manifest 失败返回 `NULL`。
2. **MTK PowerHAL `applist_enable` 5s 同步等待 + `NfcNci` 持久应用高频崩溃放大**：
   - ColorOS 的 `system_server` 在每次应用切换与进程崩溃处理路径（持有 `ActivityManagerService` 全局锁）中调用 `/system_ext/lib64/libpowerhalwrap.so!getMtkPowerAPPList`。当 `persist.system.powerhal.applist_enable` 默认为 `"1"` 时，该函数调用 `AServiceManager_getService("vendor.mediatek.hardware.mtkpower_applist.IMtkpower_applist/default")` 同步轮询等待 **5 秒**才超时返回。
   - `/system/priv-app/NfcNci`（`android:persistent="true"`）启动即调 `IOplusNfc.getService()`，拿到 `null` 后在 `OplusNfcService.initOplusNfcService(OplusNfcService.java:90)` 空指针崩溃，AMS 立即重启它并在持锁状态下触发 `getMtkPowerAPPList` 阻塞 5 秒，导致 `android.fg` 等线程饥饿满 65 秒触发 Watchdog。
3. **ODM SKU Telephony Manifest 覆盖我方 Radio VINTF 声明**：
   本机内核/引导参数带 `ro.boot.product.hardware.sku=dsds`，`libvintf` 自动加载 `/odm/etc/vintf/manifest_dsds.xml -> telephony_manifest_dsds.xml`。该文件以 `override="true"` 声明了 `android.hardware.radio.ims`（`IRadioIms/slot1`、`slot2`）及全套 `mtkradioex.*` / `vendor.oplus.hardware.radio`。由于云机不提供 `IRadioIms`，`com.android.phone` 主线程在 `PhoneFactory.makeDefaultPhone` 中调用 `ServiceManager.waitForDeclaredService("android.hardware.radio.ims.IRadioIms/slot1")` 永久阻塞。
4. **Codec2 配置缺失、无硬件/专有库服务崩溃与 `FEATURE_CONSUMER_IR` 强校验**：
   - 缺少 `/vendor/etc/media_config.xml`，导致 `android.hardware.media.c2-mediatek`（`default1`）与 `vendor.dolby.media.c2-default-service-dax`（`default9`）因找不到 store 配置退出（status 1）。
   - `vendor.nfc_hal_service`、`oplus_esepower_hal_service`、`secure_element-1-0` 缺 Trustonic `libMcClient.so` 等专有库；`magt` 由 `ro.vendor.magt.mtk_magt_support=1` 触发但无硬件支持（status 253）；`netdiag` socket 权限不匹配（status 1）；`vendor.ir-default` 因无 `/dev/oplus_consumer_ir` 退出（status 1）。
   - 停用 `vendor.ir-default` 并遮蔽 `/odm/etc/vintf/manifest/android.hardware.ir-service.xml` 后，`/odm/etc/permissions/android.hardware.consumerir.xml` 仍向 `PackageManager` 声明 `android.hardware.consumerir` 特性，导致 `ConsumerIrService.<init>` 因找不到 IR HAL 直接抛 `RuntimeException` 崩掉 `system_server`。
5. **触控设备名未对齐官方固件**：
   官方 `vendor_dlkm_a.img:/lib/modules/oplus_hbp_core.ko` 与 `odm_a.img:/firmware/tp/zhujiang/vnd_touch_project_config.xml`（`synaptics-s3910`）注册的主触控设备名为 `touchpanel`（`bus=0, vendor=0, product=0, version=0`），而 `versions.buildModules` 未配置 `input` 段，QEMU 默认回退 `virtio-tablet-pci`（命中 `excluded-input-devices.xml` 黑名单）。

---

## 3. 证据

详见同目录 [log-fail.txt](log-fail.txt) 与 [log-pass.txt](log-pass.txt)：

- **VINTF 冲突**：`servicemanager: Cannot add VINTF manifest fragment /odm/etc/vintf/manifest/manifest_oplus_face.xml: HAL "android.hardware.biometrics.face" may have a duplicated/conflictingoverride="false"entry.` → `servicemanager: NULL VINTF MANIFEST!: device`。
- **Watchdog 阻塞栈与 `libpowerhalwrap.so` 反汇编**：
  - `qemu-fail-wdt.log`：`Watchdog: Blocked in handler on foreground thread (android.fg) for 65s`；
  - `debuggerd -b <system_server>` 抓取持锁线程停在 `libbinder_ndk.so!AServiceManager_getService` ← `libpowerhalwrap.so!getMtkPowerAPPList+136`，且 `libpowerhalwrap.so` 符号表确认其读取 `persist.system.powerhal.applist_enable`。
- **Telephony `IRadioIms/slot1` 阻塞**：
  - `logcat-fail-radio-ims.log`：`13403 13403 W ServiceManagerCppClient: Waited one second for android.hardware.radio.ims.IRadioIms/slot1`；
  - `/odm/etc/vintf/telephony_manifest_dsds.xml` 第 78 行 `<name>android.hardware.radio.ims</name>`（`<fqname>IRadioIms/slot1</fqname>`）。
- **`ConsumerIrService` 异常**：
  - `logcat-fail-consumerir.log`：`java.lang.RuntimeException: FEATURE_CONSUMER_IR present, but no IR HAL loaded! at com.android.server.ConsumerIrService.<init>(ConsumerIrService.java:73)`。

---

## 4. 改动

1. **`engine/build/assets/devices.db`（`versions` 表 `id=27`，`CPH2797` `16-BP2A.250605.015-20260529`）**：
   - `buildVendor.vintf-drop`：补齐 6 项（`android.se.omapi.xml`、`android.hardware.biometrics.face-service.xml`、`android.hardware.vibrator-service.xml`、`android.hardware.weaver-service.xml`、`android.hardware.biometrics.fingerprint-service.xml`、`android.hardware.nfc-service.xml`）。
   - `buildVendor.build-prop-sets`：新增 `"persist.system.powerhal.applist_enable": "0"` 与 `"ro.vendor.magt.mtk_magt_support": "0"`。
   - `buildVendor.fileTrees`：
     - 新增 `etc/media_config.xml`（登记 `default1` / `default9` Codec2 store）；
     - 新增 `etc/compat/permissions_empty.xml`（`<permissions/>`）；
     - 在 `etc/init/init.virtio_compat.rc` 的 `on early-init` 追加对 `/odm/etc/vintf/telephony_manifest_dsds.xml`、`/odm/etc/vintf/telephony_manifest_ssss.xml`、`/odm/etc/vintf/manifest/android.hardware.ir-service.xml` 的 `vintf_empty.xml` `bind mount`，对 `/odm/etc/permissions/android.hardware.consumerir.xml` 的 `permissions_empty.xml` `bind mount`，以及 `stop vendor.nfc_hal_service`、`stop oplus_esepower_hal_service`、`stop secure_element-1-0`、`stop vendor.ir-default`、`stop magt`、`stop netdiag`。
   - `buildModules.redirect`：追加 `{"from": "/system/priv-app/NfcNci", "to": "/mnt/vendorpatch/system/priv-app/NfcNci", "type": "dir"}`。
   - `buildModules.input`：配置 `touch: {name: "touchpanel", bus: 0, vendor: 0, product: 0, version: 0, abs-to-mt: true}, keyboard: {}, tablet: false`。
2. **`engine/build/web/ui.html`**：
   - 在 Step 07（`buildVendor`）可视化配置编辑器中增加 `vintf-drop` 芯片增删与计数展示支持，防止前端保存时丢失 `vintf-drop`。

---

## 5. 验证

重跑 `build_vendor_boot`、`build_vendor`、`build_patch`、`pack_vda`（最终 `patch.img` sha256 `1cea2c70312f`，配方 `vda-0f61ea99759d`），销毁并新建实例 1（PID 40059）冷启动验证：

| 检查项 | 修复前 | 修复后（实例 1） |
| :--- | :--- | :--- |
| `servicemanager: NULL VINTF MANIFEST!` | 持续报错，卡 `bootanim` | **0 次**，`Successfully processed VINTF information` |
| `sys.boot_completed` | 恒空或 2 分钟后 Watchdog 重启 | **`1`**（约 60s 完成冷启动） |
| `sys.init.updatable_crashing` | `1`（每秒刷 `flags_health_check`） | **空**（0 个常驻服务异常退出） |
| `system_server` / `surfaceflinger` / `com.android.phone` PID | 反复变动或 Watchdog SIGKILL | **恒定**（`877` / `655` / `3431`） |
| Telephony & SIM（`gsm.sim.state` / `gsm.operator.alpha`） | 恒空（卡 `IRadioIms/slot1`） | **`LOADED,ABSENT`** / **`中国联通,`** / **`LTE,Unknown`**（CS/PS 均 `IN_SERVICE`） |
| Wi-Fi（`wlan0`） | 超时或断连 | **`CDH-A` 已连接，IP `192.168.60.100/24`** |
| 触控与显示（`dumpsys input` / `wm size`） | 无 `touchpanel`，点击无效 | **`touchpanel`（`TOUCH_MT`，`1256x2760`），焦点 `com.android.launcher/.Launcher`** |

---

## 6. 遗留

- `/odm/etc/vintf/manifest/` 下仍有其余未触发阻塞的 OPPO 私有 HAL 声明（如 `cryptoeng`、`gameopt` 等，目前客户端仅 `checkService` 而不 `waitForDeclaredService`），后续若个别系统应用阻塞可沿用同一 `vintf_empty.xml` 遮蔽或像 `15r`（#130）一样收敛整目录。

