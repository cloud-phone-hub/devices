# 全仓 45 Android 盘集缺失 HAL 补齐、引擎级兼容收口与零竞争冷启动体检闭环

- **编号**：#298
- **分组**：`common/`（跨全仓 37 份模版 / 45 个 Android 盘集及 7 处引擎/HAL 公共代码修复）
- **上游条目**：[issue-full-fleet-stage2-rebuild-and-fixes](../issue-full-fleet-stage2-rebuild-and-fixes/solution.md)（#292）、[issue-kernel-ppp-mppe-builtin-and-ai2205-selinux](../issue-kernel-ppp-mppe-builtin-and-ai2205-selinux/solution.md)（#295）、[../../xplore-x1/issue-xplore-x1-odmfactory-crash-and-trustkernel-watermark/solution.md](../../xplore-x1/issue-xplore-x1-odmfactory-crash-and-trustkernel-watermark/solution.md)（#296）、[../../g1130/issue-g1130-kernel-515-ppp-mppe-and-unisoc-daemons/solution.md](../../g1130/issue-g1130-kernel-515-ppp-mppe-and-unisoc-daemons/solution.md)（#297）

---

## 1. 问题

完成 #295~#297 单机根治后，对全仓 40 台 Android 机型（45 个 Android 盘集，另 2 台纯血鸿蒙 `huawei/HBN-AL00` / `HED-AL00` 除外）做统一 HAL 审计与零磁盘竞争冷启动 7 项增强体检（启动耗时 / SELinux / VINTF / 5 类核心 HAL 在线状态 / 前台错误弹窗 / 重启守护进程 / 退出循环 & `servicemanager` 刷屏 + VNC 人工审图），集中暴露出以下 8 类共性及 P0 阻断故障：

1. **全仓 37 份模版缺配核心自研 HAL**：多数历史模版缺配 `android.hardware.gpu@1.0`、`neuralnetworks@1.0`、`drm@1.0`、`power@1.0` 或 `radio@1.0`，且 5.15 旧内核机型尚未全部刷新至带 `CONFIG_PPP_MPPE=y` 的新版内核。
2. **补齐 `drm@1.0` 后部分直通 ODM/APEX 机型发生 VINTF 冲突或 SELinux 缺域**：部分机型官方 `/vendor/apex` 或直通 `/odm/etc/vintf` 已含 Widevine/ClearKey VINTF 片段，或缺少 `hal_drm_widevine` 域定义，导致构建或运行期 `NULL VINTF MANIFEST` / `secilc` 报错。
3. **多机型并发构建时 `02_prepare` 误匹配 `out/devices/tmp/` 解包树**：按通配目录匹配导致并发场景相互撞车。
4. **`nothing/Asteroids`（A15 & A16，GRF vendor=14）停留在内核 `fbcon` 字符终端**：HIDL `composer@2.4` 解析厂商扩展命令 `opcode=0x08000000` 失败返回 `BadParameter (4)`，导致 `SurfaceFlinger` 无法提交任何帧。
5. **`google/oriole`（A17）弹出「Android System: There's an internal problem with your device」且 `google/oriole`（A16）超时卡死**：A17 的预发布 `lineage-24.0` 底包 `compatibility_matrix.xml` 要求 `<system-sdk><version>Baklava</version>` 与正式版 A17 Framework Manifest 数字 SDK `[29..37]` 不匹配，`/system/bin/vintf` 返回 `error=1`；A16 `cdh_nn_placeholders.txt` 含 `vendor.google.wifi_ext.IWifiExt/default` 与 `/apex/com.google.pixel.wifi.ext` 冲突触发 `NULL VINTF MANIFEST`。
6. **`samsung/pa3q`（One UI 8 / A16）桌面弹出 3 个应用崩溃弹窗并伴随 `ss_conn_daemon2` 崩溃循环**：`com.android.bluetooth` 因 LE Audio Client 要求 Bluetooth Audio HAL >= V2.1 触发 `SIGABRT`（`vendor/build.prop` 写入的 `bluetooth.profile.*=false` 未覆盖官方 `/system`/`/product` 预置的 `true`）；`com.sec.android.sdhms`、`com.samsung.android.fmm` 持续弹出停止运行窗口；`/system/bin/ss_conn_daemon2` 持续退出重启。
7. **`oppo/CPH2797`（OPPO Find X9 / ColorOS 16 / MT6993）冷启动超时卡在 `FallbackHome`（`User 0: State: BOOTING`，`mAllowUserUnlocking: false`）**：`cdh_nn_placeholders.txt` 曾含 102 条空壳 HAL 引发 `system_server` Watchdog；清空后仍因该机是全系 ColorOS 16/17 中唯一沿用直通 3 GiB 官方 `/odm` 的遗留配置，暴露 40+ 份官方 `/odm/etc/vintf` 与 `/odm/etc/init` 私有服务（`cryptoeng` / `ged` / `oplus_performance` 等），导致引导链路永不触发 `AMS.finishBooting()`。
8. **少量机型后台噪声服务持续重启**：`infinix/Infinix-X6876` 与 `ulefone/GQ5011AF1` 的 `magt` 每 5s 退出（`ro.vendor.magt.mtk_magt_support` 未置 `0`）；`itel/itel-A6612L`、`villaon/VILLAON-V671NP` 存在后台守护进程退出噪声。

---

## 2. 根因

1. **HAL 补齐联动边界**：`09_build_vendor` 未在所有直通 `/odm` 与官方 vendor APEX 场景自动对齐 DRM VINTF 冲突，也未在缺失 `hal_drm_widevine` 域时自动注入 CIL 声明。
2. **HIDL `composer@2.4` 厂商扩展命令解析**：Nothing OS（QTI `volcano`）框架向 `IComposerClient::executeCommands` 下发私有扩展命令（`opcode >= 0x08000000`），AOSP 默认 `CommandReaderBase::parseCommand` 将未知 opcode 视为协议错误直接返回 `Error::BAD_PARAMETER`，打断整帧 `presentOrValidateDisplay`。
3. **预发布底包 `system-sdk` 代号与正式版 Framework Manifest 漂移**：`lineage-24.0` 的设备 `compatibility_matrix.xml` 保留了预发布代号 `<version>Baklava</version>`，而正式版 Android 17 的 `/system/etc/vintf/manifest.xml` 仅声明数字版本 `29..37`，`libvintf` 严格比对失败触发 `VintfObject.verifyWithoutAvb() == 1` 系统内部错误弹窗。
4. **`bluetooth.profile.*` 属性被 `/system`/`/product` 抢先锁定**：`init` 装载 `build.prop` 对已有非 `ro.` 属性默认不覆盖，因此写在 `vendor/build.prop` 中的 `bluetooth.profile.bap.*=false` 对 `/product` 已预置为 `true` 的 8 项全部失效，必须通过 `pioneer` 在 `post-fs-data`/`boot` 阶段显式 `setprop`（并授予 `bluetooth_config_prop_{{vers}}` 的 `property_service set` 权限）。
5. **`oppo/CPH2797` 直通 `/odm` 遗留架构**：孪生同平台机型 `oppo/PME110`（MT6993 / ColorOS）及全系 `oneplus/CPH2767`、`PJE110`、`realme/RMX3823`/`RMX5112` 均采用标准两段 `/odm -> /mnt/vendorpatch/odm` 整目录重定向并 60~85s 正常开机，唯独 `CPH2797` 仍直通官方 3 GiB `/odm` 并靠零散 `mount --bind` + 8 项 `vintf-drop` 遮蔽，补齐新 HAL 后与官方 `/odm` 声明/守护进程产生隐式耦合阻塞用户解锁。

---

## 3. 证据

详见同目录 [log-fail.txt](log-fail.txt) 与 [log-pass.txt](log-pass.txt)，核心摘录如下：

- **`nothing/Asteroids`**：`hwc-composer: failed to parse command 0x8000000, length 1` → `presentOrValidate failed: BadParameter (4)`，画面滞留内核 `fbcon`。
- **`google/oriole` 17**：运行态 `/system/bin/vintf` 返回 `error=1`，明确指出 `Device compatibility matrix requires system-sdk Baklava, not found in framework manifest [29..37]`。
- **`google/oriole` 16**：`servicemanager: NULL VINTF MANIFEST!: device`（`vendor.google.wifi_ext.IWifiExt/default` 与 `/apex/com.google.pixel.wifi.ext` 撞车）。
- **`samsung/pa3q`**：`com.android.bluetooth` tombstone 报 `Abort message: 'system/bta/le_audio/client.cc:8904 Initialize: LE Audio Client requires Bluetooth Audio HAL V2.1 at least'`，`getprop bluetooth.profile.bap.unicast.client.enabled` 实测返回 `[true]`；`pm path` 实锤弹窗包路径为 `/system/priv-app/SamsungDeviceHealthManagerService`、`/system/priv-app/Fmm` 与 `/system/bin/ss_conn_daemon2`。
- **`oppo/CPH2797`**：`dumpsys activity users` 实测 `User #0: state=BOOTING`、`mAllowUserUnlocking: false`、`mLastUserUnlockingUptime: 0`、`service.bootanim.exit=0`；对比同款 MT6993 的 `oppo/PME110`（整目录重定向 `/odm`）85s 正常亮屏开机且 5 HAL 全绿。

---

## 4. 改动

1. **全仓 37 份 Android 模版 HAL 补齐与 45 盘集重打**：
   - 为全仓缺项模版统一补齐 `android.hardware.gpu@1.0`、`neuralnetworks@1.0`、`drm@1.0`、`power@1.0`、`radio@1.0`，并刷新全部 5.15 旧内核机型至 `CONFIG_PPP_MPPE=y` 新内核，原子更新 45 个 Android 盘集至 `out/devices/dist/`。
2. **引擎级兼容性增强（Go 流水线 + 单元测试全绿 + 重编 `cdh-pipeline`）**：
   - `agent/modules/pipeline/stages/09_build_vendor/sepolicy_cil.go`：新增 `reconcileDrmWidevineBindingsCIL`，自动补齐缺失的 `hal_drm_widevine` 域声明。
   - `agent/modules/pipeline/stages/09_build_vendor/vintfdrop.go`：新增 `reconcileOfficialVendorApexDrmVintf`，当官方 vendor APEX 已含 Widevine/ClearKey 时自动裁除重复 VINTF 片段。
   - `agent/modules/pipeline/stages/09_build_vendor/files_compat.go`：
     - 直通 `/odm` 机型自动 `fsimg.Extract` 官方 `odm_a.img` 的 `etc/vintf` 做精确去重；
     - `isSystemDomainPropKey` 纳入 `bluetooth.profile.*`，由 `vendor.cdh_system_props` 在 `post-fs-data` / `boot` 通过 `pioneer` 显式 `setprop` 覆盖 `/system`/`/product` 预置值。
   - `agent/modules/pipeline/stages/09_build_vendor/vintf.go`：新增 `officialFrameworkVintfManifest`、`parseFrameworkSystemSDKs`、`alignDeviceMatrixSystemSDK`，自动将预发布底包的 `<system-sdk><version>Baklava</version>` 对齐为官方 Framework Manifest 支持的最高数字 SDK 版本（如 `37`）。
   - `agent/modules/pipeline/stages/02_prepare/plan.go`：并发构建时按 `buildEnv.VendorROM` 精确匹配 `out/devices/tmp/` 解包目录。
   - `agent/modules/pipeline/stages/09_build_vendor/{vintf_test.go,props_test.go}`：补充上述 4 项白盒单元测试。
3. **HAL 与 SELinux 公共代码修复**：
   - `devices/hardware-hidl/android.hardware.graphics.composer/src/service.cpp`：派生 `CdhComposerCommandEngine` / `CdhComposerClient` / `CdhComposer`，在标准 HIDL 2.4 命令解析之上优雅跳过 `opcode >= 0x08000000u` 的厂商私有扩展命令（兼容 A13/A14 边界检查 `keySize > maxBytes`），重编 `android13` 与 `android14` 产物。
   - `devices/hardware/android.service.daemon/sepolicy/vendor_sepolicy.cil`：追加 `(allow pioneer bluetooth_config_prop_{{vers}} (property_service (set)))`（实测 `lineage-20.0`~`24.0` 五代均含该类型）。
4. **机型定向配置收口（严守 `AGENTS.md §8`，不改官方系统镜像）**：
   - `google/oriole` (`oriole-android16/vendor/etc/cdh_nn_placeholders.txt`)：移除与 APEX 冲突的 `vendor.google.wifi_ext.IWifiExt/default`。
   - `samsung/pa3q` (`pa3q-android15/module/config.json`)：新增 3 条 `cdh_redirect`（总计 15/32）屏蔽 `/system/priv-app/SamsungDeviceHealthManagerService`、`/system/priv-app/Fmm` 与 `/system/bin/ss_conn_daemon2`，配合 `bluetooth.profile.*` 桥接全链重打 `vda.img` + `patch.img`。
   - `oppo/CPH2797` (`CPH2797-android16/{module/config.json,vendor/config.json,vendor/etc/init/init.virtio_compat.rc,vendor/etc/cdh_nn_placeholders.txt}`)：清空 102 条空壳占位；将 `/odm` 切换为与 `oppo/PME110`/`oneplus/CPH2767` 一致的标准两段整目录重定向（`file /odm -> /odm` + `dir /odm -> /mnt/vendorpatch/odm`）；清空 8 项 `vintf-drop`；精简 `init.virtio_compat.rc` 仅保留 `/system_ext/lib64/librenderengineextimpl.so` 遮蔽与 `stop magt/netdiag`，全链重打 `vda.img` + `patch.img`。
   - `infinix/Infinix-X6876` 与 `ulefone/GQ5011AF1` (`vendor/config.json`)：追加 `ro.vendor.magt.mtk_magt_support=0` 消除 `magt` 重启循环。

---

## 5. 验证

1. **Go 单元测试与二进制编译**：
   - `cd agent && go test ./modules/pipeline/stages/09_build_vendor` 全部通过（0.480s，含 `TestAlignVendorSystemProps`、`TestAlignDeviceMatrixSystemSDK`、`TestReconcileDrmWidevineBindingsCIL`、`TestReconcileOfficialVendorApexDrmVintf`）；`cdh-pipeline` 原子重编通过。
2. **全仓 45/45 Android 盘集零磁盘竞争冷启动 7 项体检 + VNC 人工审图**：
   - **45/45 盘集 100% 完成冷启动（`NOT_BOOTED = 0`）**，其中 **20 盘集 7 项满分 `CLEAN PASS`**，**44/45 盘集正常亮屏渲染系统桌面/引导页（`motorola/aito` 为历史 #271 已知限制）**。
   - **关键修复机型实测对比**：
     - `nothing/Asteroids`（15 & 16）：`fbcon` 滞留 → **30s / 35s 高清渲染 Nothing OS 3.0 / 4.0 金色点阵欢迎页**（`bright=115.91 / 115.31`、5 HAL 全绿、0 重启、0 弹窗）。
     - `google/oriole`（16 & 17）：16 从 `boot_timeout` → **55s Pixel 桌面（5 HAL 全绿、0 弹窗）**；17 从「Android System 内部错误」弹窗 → **60s Pixel 桌面且弹窗 100% 消失**（`bright=109.04`）。
     - `samsung/pa3q`（16）：3 个应用停止弹窗 + `ss_conn_daemon2` 循环 → **126s 中文 One UI 8 引导页（`bright=237.77`、5 HAL 全绿、`restarting=''`、`wins=''`、0 崩溃弹窗）**。
     - `oppo/CPH2797`（16）：273s `boot_timeout` 卡 `FallbackHome` → **105s 满分 `CLEAN PASS`（`svcs=457`、`bright=51.98`、5 HAL 全绿、0 重启、0 崩溃、0 弹窗、高清 ColorOS 16 桌面）**。
     - `infinix/Infinix-X6876`、`ulefone/GQ5011AF1`：`magt` 重启计数清零（`restarting=''`）。
   - 完整 45 盘集实测数据见同目录 [log-pass.txt](log-pass.txt)。

---

## 6. 遗留

1. **`motorola/aito`（16）**：属历史条目 [../../aito/issue-aito-mtk-composer-ext-fbcon/solution.md](../../aito/issue-aito-mtk-composer-ext-fbcon/solution.md)（#271）已知限制（待实现 MTK `IComposerExt` 源码桩），本轮 45s 完成冷启动、无崩溃弹窗，但显示层仍停留在 `fbcon`。
2. **轻度非阻断后台项**：
   - `google/oriole`（17）预发布 `lineage-24.0` 底包下 `widevine`/`clearkey`/`power`/`radio` 4 项在线，`nn` 未在线；14~17 的 32 位 `drm`/`media` 守护进程重启为 Pixel 纯 64 位已知非致命噪声（桌面与功能正常）。
   - `ulefone/GQ5011AF1`（16）5 HAL 全绿且 `magt` 已清，残留一次性 `BatteryCheckOtaBootBroadcastReceiver` 引发 `com.android.settings` 对话框，后续可按需屏蔽 OTA 检查组件。
   - `tcl/vinca`（16）70s 启动且 5 HAL 全绿、0 重启、0 崩溃、0 弹窗，首帧抓图时处于锁屏息屏态（`bright=0.0`）。
   - `itel/itel-A6612L` 与 `villaon/VILLAON-V671NP` 仍残留单条后台 `srmi_proxyd` 退出噪声（无弹窗、5 HAL 全绿），待核实该二进制在 `/system` 或 `/vendor` 侧的精确路径后按需屏蔽。
   - 按用户要求，`com.android.chrome` 32 位 `libmonochrome.so` 崩溃属纯 64 位预期行为，不做修改。
