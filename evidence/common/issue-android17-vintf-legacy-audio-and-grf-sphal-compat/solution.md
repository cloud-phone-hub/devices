# #288：Android 17 底座适配、冲突 Audio Effect 裁剪、跨代 GRF SP-HAL 兼容与孤儿厂商 APEX / VINTF 占位闭环

- **设备/机型**：`google/oriole`（`17-CP3A.261005.002.A1-20260905`）、`oppo/PME110`（OPPO Find X9s Pro，ColorOS 17 / `16-BP2A.250605.015-20260923`，跨代 GRF `system` Android 17 + `vendor` Android 16）及通用 Android 14~17 含孤儿 `/vendor/apex` 与扩展 AIDL 占位的机型
- **结论归属**：`common/`（涉及 `internal/halcore/profile`、`devices/modules/cdh_redirect`、`02_prepare` 与 `09_build_vendor` 多处引擎级共性机制，同时覆盖 `google/oriole` Android 17 与 `oppo/PME110` ColorOS 17）

## 问题

在推进 Android 17（`lineage-24.0`）底座接入及首款商业跨代 GRF Android 17 旗舰 `oppo/PME110`（`system_a.img` SDK 37 / Android 17，`vendor_a.img` SDK 36 / Android 16，MT6993 / kernel `6.12`）构建与冷启动验收过程中，依次暴露出 5 类引擎级共性卡点：

1. **Android 17 HAL 编译画像缺失**：`internal/halcore/profile` 仅覆盖至 `halTable16()`，`android.hardware.media` 在 Android 17 下缺少 `c2` V2 AIDL 依赖映射，无法构建 `out/devices/vendor/android17`。
2. **`lineage-24.0` 基底残留 `audio.effect.service-aidl.legacy` 引发 VINTF 冲突**：`lineage-24.0` `vendor_a.img` 自带 `android.hardware.audio.effect.service-aidl.legacy.xml`（声明 `IFactory/default` `@2`），与自研 `android.hardware.audio` 的 `android.hardware.audio.service.xml`（`@4`）同实例冲突，导致 `libvintf` 解析设备 VINTF 失败、`SurfaceFlinger` / `keystore2` 无法启动。
3. **跨代 GRF（`system` A17 + `vendor` A16）下 `zygote64` `eglGetErrorImpl()+32` (`pc=0x0`) 崩溃**：原厂 A16 `vendor/lib64` 仅含 `android.hardware.graphics.common-V6-ndk.so` 且 `libc++.so` 未导出 LLVM 19 `_ZNSt3__113__hash_memoryEPKvm`，而载体 `lineage-24.0` SP-HAL（`libEGL_mesa.so` 等）依赖 `common-V7-ndk.so`、`libaconfig_storage_read_api_cc.so` 与新 `libc++.so`；此外 `cdh_maps_prewarm_fn` 在 `/mnt/vendorpatch` 子目录尚未就绪前过早结束探测，致使 SP-HAL 标准库回落到原厂 A16 `/vendor/lib64` 链接失败（`dlopen 基座库失败` -> 函数表空指针崩溃）。
4. **原厂 `/vendor/apex/vendor.*.apex` 孤儿厂商 APEX 暴露 `unlabeled` 服务**：`PME110` 原厂 `/vendor/apex` 含 `vendor.oplus.hardware.engineer.aidl.apex`、`vendor.oplus.hardware.stability.aidl.apex` 等 4 个厂商私有 APEX；未重定向 `/vendor/apex` 时，`apexd` 挂载后暴露 `u:object_r:unlabeled:s0` 的二进制，`servicemanager` 每秒触发 `ctl.interface_start` 均因 SELinux 域转换失败被 `init` 拒绝。
5. **屏蔽孤儿 APEX 与替换 `/vendor/etc/vintf`、`/odm` 后的三连环占位断链**：
   - `OplusEngineerFwService.onBootPhase(480)` 在 `ro.adb.secure=1` 时不检查 `isDeclared`，直接无超时调用 `ServiceManager.waitForService("vendor.oplus.hardware.engineer.IEngineer/default")` 永久阻塞 `system_server` 主线程；
   - `02_prepare` 未扫描 `vendor.*.apex` 内嵌 `apex_payload.img` 的 `<manifest>`，也未在存在 `cdh_nn_placeholders.txt` 时自动启用承载进程 `android.hardware.neuralnetworks`；
   - `09_build_vendor` 未为占位承载域自动生成厂商自定义 `_service` 的 `service_manager (add find)` 最小授权，且载体替换 `/vendor/etc/vintf` 与精简 `/odm` 后丢失原厂 `<hal format="aidl">` 声明，导致 `servicemanager` 在 `20.45s` 以 `Could not find ... in the VINTF manifest` 拒绝全部 VINTF 稳定态占位服务注册。

## 根因

1. **HAL 画像表未覆盖 Android 17**：`profile.go` 的 `Lookup("17")` 无分支，`media.c2` 在 AOSP 17 冻结至 V2。
2. **双份 `IFactory/default` VINTF 片段并存**：`lineage-24.0` 新增 `audio.effect.service-aidl.legacy` 独立二进制/rc/xml，与自研一体化 `android.hardware.audio-service-aidl` 撞实例。
3. **`cdh_redirect` 跨代 GRF 判据与预热时机缺口**：`cdh_probe_sphal_vndk_compat()` 原仅探测 `VNDK-34` 缺失特征，未探测 `common-V7-ndk.so` 缺失（A17 载体 vs A16 vendor），`tier_b_exact` 漏列 `common-V7-ndk.so` 与 `libaconfig_storage_read_api_cc.so`，且 `cdh_maps_prewarm_fn` 未等待具体 `cdh_rules[i].to` 目录挂载完成即读取。
4. **孤儿厂商 APEX 未纳入兼容重定向**：载体已摊平图形与 `wpa_supplicant` 必需 APEX，原厂 `vendor.*.apex` 的私有 SELinux 域不在基底策略内，挂载必落 `unlabeled`。
5. **VINTF 稳定态占位链路缺闭环**：自 [`#266`](../issue-qti-radio-placeholder-vintf-stability/solution.md) 为占位 Binder 标记 `AIBinder_markVintfStability` 后，`servicemanager` 强制要求服务同时满足「VINTF 清单已声明」与「SELinux 允许承载域 `add`」两条硬约束；而独立 `/odm` 机型经 `07_build_odm` 收口后仅保留 `build.prop` 与 `fstab`，原厂 `/vendor/etc/vintf`、`/odm/etc/vintf` 及被屏蔽 `vendor.*.apex` 中的 `<hal format="aidl">` 声明均不在运行态可见路径中。

## 证据

- **修复前日志与诊断（[`log-fail.txt`](log-fail.txt)）**：
  - 跨代 SP-HAL 链接失败：`E cdh-gpu : egl: dlopen 基座库失败: dlopen failed: cannot locate symbol "_ZNSt3__113__hash_memoryEPKvm" referenced by "/vendor/lib64/libunwindstack.so"`，`zygote64` 死于 `eglGetErrorImpl()+32` (`pc=0x0`)。
  - 孤儿厂商 APEX 阻塞：`init: Control message: Could not ctl.interface_start for 'aidl/vendor.oplus.hardware.engineer.IEngineer/default' ... File /apex/vendor.oplus.hardware.engineer.aidl/bin/hw/vendor-oplus-hardware-engineer-V1-service(labeled "u:object_r:unlabeled:s0") has incorrect label`。
  - 屏蔽 `/vendor/apex` 并启用 `neuralnetworks` 后 VINTF 拒绝注册：`[ 20.451952] servicemanager: Caller(pid=578,uid=1000,sid=u:r:hal_neuralnetworks_default:s0) Could not find vendor.oplus.hardware.engineer.IEngineer/default in the VINTF manifest. No alternative instances declared in VINTF.`，280s 超时 `bootCompleted=false`（`serviceCount=367`）。
- **修复后验收日志与桌面截图（[`log-pass.txt`](log-pass.txt)、[`pass-pme110-desktop.png`](pass-pme110-desktop.png)）**：
  - `09_build_vendor` 日志确认：`补齐占位 HAL 厂商服务 SELinux 授权 69 条`、`[vintf] vendor/etc/vintf/manifest/cdh_vendor_placeholders.xml: 合成 87 个 HAL 包共 133 项占位 AIDL 声明`。
  - `verify_boot.py` 冷启动一次通过：`bootCompleted=true`、`bootanim="stopped"`、`serviceCount="550"`、`topActivity=com.android.settings/.Settings`、VNC `1280x2772` `meanBrightness=55.27` / `peak=255`，ColorOS 17 桌面（「小布建议」、状态栏 4G/100% 电量、应用图标与导航栏）完整清晰渲染。

## 改动

1. **Android 17 HAL 画像与冲突 Audio Effect 裁剪**：
   - [`agent/modules/pipeline/internal/halcore/profile/profile.go`](../../../../agent/modules/pipeline/internal/halcore/profile/profile.go) 与 [`profile_test.go`](../../../../agent/modules/pipeline/internal/halcore/profile/profile_test.go)：新增 `halTable17()`（对齐 `media.c2` V2 AIDL 等 Android 17 接口版本）。
   - [`agent/modules/pipeline/stages/09_build_vendor/files_compat.go`](../../../../agent/modules/pipeline/stages/09_build_vendor/files_compat.go) 与 [`files_test.go`](../../../../agent/modules/pipeline/stages/09_build_vendor/files_test.go)：新增 `pruneConflictingBaseAudioEffect` 及 `TestPruneConflictingBaseAudioEffect`，当自研 `android.hardware.audio.service.xml` 已声明 `IFactory` 时自动剔除基底冲突的 `audio.effect.service-aidl.legacy` 三件套。
2. **跨代 GRF SP-HAL 标准库分流与挂载预热同步**：
   - [`devices/modules/cdh_redirect/include/cdh_redirect.h`](../../../modules/cdh_redirect/include/cdh_redirect.h)、[`hook/maps.c`](../../../modules/cdh_redirect/hook/maps.c)、[`core/rules.c`](../../../modules/cdh_redirect/core/rules.c)：新增 `cdh_probe_sphal_vndk_compat()` / `cdh_maps_sphal_vndk_compat()`，将 `android.hardware.graphics.common-V7-ndk.so` 与 `libaconfig_storage_read_api_cc.so` 纳入 `tier_b_exact`，并在 `cdh_maps_prewarm_fn` 等待各规则 `to` 目标路径就绪后再探测与预热 inode 缓存。
3. **孤儿 `/vendor/apex` 自动屏蔽与内嵌 VINTF 占位提取**：
   - [`agent/modules/pipeline/stages/02_prepare/firmware_measured_extra.go`](../../../../agent/modules/pipeline/stages/02_prepare/firmware_measured_extra.go)：新增 `hasOrphanVendorApex`（自动产出 `dir /vendor/apex -> /mnt/vendorpatch/vendor/apex`）与 `extractOrphanVendorApexManifestXMLs`（用标准库 `archive/zip` 从 `vendor.*.apex` 的 `apex_payload.img` 提取内嵌 `<manifest>` 并合入 `VintfPlaceholders`）。
   - [`agent/modules/pipeline/stages/02_prepare/apply_profile.go`](../../../../agent/modules/pipeline/stages/02_prepare/apply_profile.go) 与 [`apply_profile_configs.go`](../../../../agent/modules/pipeline/stages/02_prepare/apply_profile_configs.go)：`buildAlignedModuleRedirect` 允许 `>12` 条存量规则幂等合入 `/vendor/apex`；`alignHardwareHALList` 在存在 `NeuralNetworks` 占位服务时自动补齐 `android.hardware.neuralnetworks@1.0`；`alignVendorExtraFields` 支持超集占位列表幂等合并更新。
   - [`agent/modules/pipeline/stages/02_prepare/apply_profile_test.go`](../../../../agent/modules/pipeline/stages/02_prepare/apply_profile_test.go)：新增 `TestOrphanVendorApexRedirectExtractionAndAlign` 与 `TestOrphanVendorApexVintfPlaceholdersAndNeuralNetworksHALAlign`。
4. **占位服务 SELinux 最小授权与 VINTF 声明合成**：
   - [`agent/modules/pipeline/stages/09_build_vendor/sepolicy_cil.go`](../../../../agent/modules/pipeline/stages/09_build_vendor/sepolicy_cil.go) 与 [`sepolicy_test.go`](../../../../agent/modules/pipeline/stages/09_build_vendor/sepolicy_test.go)：新增 `reconcilePlaceholderServiceBindingsCIL` 及单测，按 [`#227`](../../gold/issue-gold-vendor-binder-placeholders-and-init-svc/solution.md) 最小权限原则为 `cdh_{nn,gnss,audio,radio}_placeholders.txt` 命中的纯厂商 `_service` 类型自动生成 `(service_manager (add find))`（严格跳过平台版本化类型与 `neverallow` `add` 目标，避免任何 `secilc` 冲突）。
   - [`agent/modules/pipeline/stages/09_build_vendor/files_compat.go`](../../../../agent/modules/pipeline/stages/09_build_vendor/files_compat.go) 与 [`files_test.go`](../../../../agent/modules/pipeline/stages/09_build_vendor/files_test.go)：`reconcileVendorSystemCompat` 确保创建空 `vendor/apex` 目录，并新增 `synthesizePlaceholderVintfManifest`（及 `TestSynthesizePlaceholderVintfManifest`），结合 SELinux 可注册性校验自动合成 `etc/vintf/manifest/cdh_vendor_placeholders.xml`（保留原厂与孤儿 APEX 的精确 `<version>`，绝不声明不可注册的孤儿服务）。

## 验证

1. **Go 单元测试**：`internal/halcore/profile`、`02_prepare`、`09_build_vendor` 新增回归单测全部通过。
2. **Android 17 双机型构建与冷启动亮屏验收**：
   - `google/oriole`（`17-CP3A.261005.002.A1-20260905`）：全 13 阶段构建与冷启动亮屏验收通过。
   - `oppo/PME110`（`16-BP2A.250605.015-20260923`，`patch.img` sha256 `b146b19c6d87`）：`apply-profile` 自动补齐 `/vendor/apex` 重定向（`27/32`）、`neuralnetworks@1.0` 与 `IEngineer/default`、`IOplusProject/default`；冷启动验收 `passed=true`（`bootCompleted=true`、`serviceCount=550`、`meanBrightness=55.27`、`peak=255`、零 tombstone），人工审查 [`pass-pme110-desktop.png`](pass-pme110-desktop.png) 桌面显示正常。

## 遗留

- 平台版本化标准服务类型（如 `vendor.nxp.nxpnfc_aidl.INxpNfc/default` 复用 `hal_nfc_service`）受 AOSP `plat_sepolicy` `neverallow` 约束，刻意不在 `hal_neuralnetworks_default` 授权与 `cdh_vendor_placeholders.xml` 中伪造声明，保持 `isDeclared=false` 由框架走标准回退路径。
