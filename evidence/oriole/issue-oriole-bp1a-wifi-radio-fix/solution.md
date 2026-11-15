# #160 oriole（BP1A.250505.005-20250326）：Wi-Fi 与 Radio 未生效（6.1 内核 GKI 模块导出符号保护 + Android 15 FEATURE_TELEPHONY 门禁）

## 1. 问题

`oriole_BP1A.250505.005-20250326`（Pixel 6，Android 15 QPR2 / `lineage-22.2` 底座 + `6.1` 内核）在解决 `#159` 进入桌面后，Wi-Fi 与移动网络（Radio / SIM）均未生效：
- **Wi-Fi**：`ip link` 无 `wlan0` 网卡，`lsmod` 中缺 `rfkill`、`libarc4`、`cfg80211`、`mac80211`、`mac80211_hwsim`、`virt_wifi`。
- **Radio**：虽然 `android.hardware.radio-service` 正常运行且已向 `servicemanager` 注册全部 AIDL `IRadio*` 服务，但系统服务 `phone`、`isub`、`carrier_config` 均为 `not found`，`gsm.sim.state` 未置位，`logcat -b radio` 持续报 `telephony service is null` 与 `ICarrierConfigLoader is null`。

## 2. 根因

### 2.1 Wi-Fi 根因：6.1 内核 `CONFIG_MODULE_SIG_PROTECT=y` 拒绝加载 strip 后无签名的 in-tree 模块

`dmesg` 显示 `dlkm_loader` 在加载 `/vendor_dlkm/lib/modules/rfkill.ko` 与 `libarc4.ko` 时被内核拒绝：
- `rfkill: exports protected symbol rfkill_alloc` → `Failed to insmod ... Permission denied`（`-EACCES`）
- `libarc4: exports protected symbol arc4_crypt` → `Failed to insmod ... Permission denied`（`-EACCES`）
- `dlkm_loader: Check failed: m.LoadListedModules(false)` 中止后续 `cfg80211.ko`、`mac80211.ko`、`mac80211_hwsim.ko`、`virt_wifi.ko` 的加载。

对比 `5.10`、`6.1`、`6.6` 三棵内核树的 `kernel/module/Makefile` 与 `gki_module.c`：
- `gki_is_module_protected_export()` 仅在 `NR_UNPROTECTED_SYMBOLS > 0` 时检查导出符号是否落在 `android/abi_gki_protected_exports_aarch64`。
- `6.6` 的 `kernel/module/Makefile` 仅在 `CONFIG_UNUSED_KSYMS_WHITELIST` 非空（需开启 `CONFIG_TRIM_UNUSED_KSYMS`）时才填充 `ALL_KMI_SYMBOLS`，否则生成空文件（`NR_UNPROTECTED_SYMBOLS == 0`），因此未触发拦截；`5.10` 的 `android/abi_gki_modules_protected` 为空文件（18 B），同样未触发。
- `6.1`（及 `5.15`）的 `kernel/module/Makefile` 无条件执行 `cat $(srctree)/android/abi_gki_aarch64_* > $(ALL_KMI_SYMBOLS)`，导致 `NR_UNPROTECTED_SYMBOLS > 0`；而 `03_build_modules` 在导出 `.ko` 时统一执行 `llvm-strip --strip-debug`，剥掉了内核构建阶段追加在文件尾部的 `~Module signature appended~` 签名（`mod->sig_ok == false`），触发 `verify_exported_symbols()` 拦截。

### 2.2 Radio 根因：缺少 `etc/permissions/android.hardware.telephony.gsm.xml`，触发 Android 15 `PhoneGlobals.onCreate()` 提前返回

反编译 `BP1A.250505.005` 的 `/system/priv-app/TeleService/TeleService.apk`（对比 Android 14 `AP2A`）：
- Android 14（`AP2A`）的 `PhoneGlobals.onCreate()` 不检查 `FEATURE_TELEPHONY`，直接调用 `PhoneFactory.makeDefaultPhones()`。
- Android 15 QPR2（`BP1A`）的 `PhoneGlobals.onCreate()` 新增了门禁：
  ```java
  if (!getResources().getBoolean(android.R.bool.config_flexibleSplitRatios)
          && !getPackageManager().hasSystemFeature("android.hardware.telephony")) {
      Log.v("PhoneGlobals", "onCreate()... but not defined FEATURE_TELEPHONY");
      return;
  }
  ```
  且 `CarrierConfigLoader`、`PhoneInterfaceManager` 在 `mVendorApiLevel >= 35` 时强制校验 `android.hardware.telephony.subscription` 等细粒度特性（`CompatChanges 297989574L`）。
- `lineage_virt_arm64` 基底的 `/vendor/etc/permissions` 仅含 `tablet_core_hardware.xml`，不含 `android.hardware.telephony.gsm.xml`。此前 `gold`、`m22`、`cph2797`、`ss9380` 因在 `buildVendor.files` 中整目录 `replace` 了官方 `etc/permissions` 而未暴露该缺口；`oriole` 未整目录替换 `etc/permissions`，导致 `pm list features` 中无 `android.hardware.telephony`。

## 3. 证据

- 失败日志见 [log-fail.txt](log-fail.txt)
- 通过日志见 [log-pass.txt](log-pass.txt)

## 4. 改动

1. **内核配置（`engine/build/02_build_kernel/lineage-kernel.config`）**：
   - 新增 `# CONFIG_MODULE_SIG_PROTECT is not set`，关闭 GKI 模块签名与受保护导出/导入符号拦截，与 `03_build_modules` 的 `llvm-strip --strip-debug` 及外部模块加载机制保持一致。
2. **Radio HAL 模块（`engine/vendor/hardware/android.hardware.radio`）**：
   - 在 `engine/vendor/hardware/android.hardware.radio/vendor/etc/permissions/android.hardware.telephony.gsm.xml` 补入 AOSP 标准 GSM 电话特性声明文件（源自 AOSP `frameworks/native/data/etc/android.hardware.telephony.gsm.xml`，含 `telephony`、`telecom`、`radio.access`、`subscription`、`gsm`、`calling`、`data`、`messaging`），使所有接入 `android.hardware.radio` 的机型在 07 步骤 4 自动获得电话特性声明，无需在各机型 `devices.db` 的 `buildVendor.files` 中单独维护。

## 5. 验证

1. 重编 `cdh-vendor hal build radio --android {14,15,16}`，并重跑 `oriole`（`BP1A.250505.005-20250326`）的 `02 -> 03 -> 04 -> 05 -> 07 -> 08 -> 09 -> 11` 全链路。
2. 重建并启动 `oriole_BP1A.250505.005-20250326` 实例 1（ADB `127.0.0.1:5919`），约 16 秒 `sys.boot_completed=1`：
   - **Wi-Fi**：`lsmod` 显示 `rfkill`、`cfg80211`、`libarc4`、`mac80211`、`mac80211_hwsim`、`virt_wifi` 全部加载成功；`wlan0` 分配 `192.168.60.100/24`，`cmd wifi status` 显示 `Wifi is connected to "CDH-A"`（`Supplicant state: COMPLETED`）。
   - **Radio**：`pm list features | grep telephony` 完整列出 8 项电话特性；`service check phone` 与 `service check isub` 均为 `found`；`getprop` 显示 `gsm.sim.state=LOADED`、`gsm.sim.operator.alpha=China Unicom`、`gsm.operator.alpha=中国联通`、`gsm.network.type=LTE`。

## 6. 遗留

- 无。

