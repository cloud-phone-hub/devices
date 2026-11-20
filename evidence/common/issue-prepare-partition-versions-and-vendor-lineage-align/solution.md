# 290 · 引擎与 GRF 跨代共性：全分区 `build.prop` 版本解析、`buildId` 按 `system` 对齐、`vendor` 同代 Lineage 底座绑定与展锐跨代兼容

- **设备/机型**：`common`（引擎级 `02_prepare` / `09_build_vendor` / `manifestview` / `web` / `android.hardware.radio`；全仓 `47` 个官方版本统一回填与 `25` 个跨代 `buildId` 迁移，并以 `villaon/VILLAON-V671NP` `system=16, vendor=13` 端到端重编与冷启动实测验证）
- **状态**：已闭环（2026-11-20）

---

## 问题

1. **GRF 跨代机型身份与配置底座错配**：全仓大量 GRF（Google Requirements Freeze）包存在 `system`、`system_ext`、`product` 已升级至高代 Android（14/15/16/17），而 `vendor`、`odm`、`vendor_dlkm` 仍停留在低代 Android（13/14/15/16）的现象。此前：
   - `buildId` 前缀与 `vendor/build.prop` 耦合（如 `VILLAON-V671NP` 的 `13-TP1A.220624.014-20260713`），与系统真实版本（Android 16）不一致；
   - 配置模版与 Lineage 底包未统一严格绑定官方 `vendor` Android 版本，导致 `VILLAON-V671NP` 此前绑定了 `VILLAON-V671NP-android16`（`lineage-23.2`），而 `02_prepare` 也未解析并展示各个官方分区（`system`、`system_ext`、`product`、`vendor`、`odm`、`vendor_dlkm`、`odm_dlkm`）的 `build.prop` Android 版本，人工与 UI（ROM 版本基线面板）无法直观分辨跨代情况。
2. **`VILLAON-V671NP` 对齐至 `VILLAON-V671NP-android13`（`lineage-20.0`）与 `system=16` 并行冷启动暴露三项跨代共性卡点**：
   - 第 1 轮：`SurfaceFlinger` 启动报 `E ComposerResources: failed to get mapper 2.0 service` 并 `Abort message: 'failed to create composer client'` 循环崩溃（黑屏）；同时 `/system_ext/bin/unisocaudioutils` 为 ELF32，在纯 64 位系统上持续 `Exec format error`（`exited 4 times before boot completed`）；
   - 第 2 轮（修复上述两项并达成 `sys.boot_completed=1` 后约 `t=33s~40s`）：`com.android.phone` 与 `com.spreadtrum.ims` 连接 `vendor.unisoc.hardware.radio.*` 占位 binder 后连续崩溃，触发 `RescueParty` 执行 `WARM_REBOOT`，二次启动卡在 `vold`。

---

## 根因

1. **身份与 Lineage 对齐逻辑缺口**：
   - `02_prepare` 仅按需读取单一分区的版本字段，未提取全部公式官方镜像的 `build.prop` Android 版本并落入 `versions/*/config.json` 的 `extra.partitionAndroidVersions`；
   - `identity.go` 计算 `buildId` 时取自 `vendor` 大版本前缀，而未以 `system` 分区 Android 主版本替换 `vendor` 前缀；
   - 配置模版及 Lineage 基底未严格锚定官方 `vendor` 分区 Android 版本，导致 SELinux 策略代数（如 `plat_sepolicy_vers`）、VINTF 与官方 `vendor` AIDL 代际产生混用错位；
   - Web 工作台（`manifestview` / `agent/web`）仅渲染单个抽象 Android 版本号，缺少全部分区版本明示与 `S<sys>/V<vnd>` 跨代标识。
2. **A16 `system` + A13 `vendor`（`lineage-20.0`）三项运行时兼容缺陷**：
   - **HIDL `mapper@4.0-impl.so` 软链接缺失**：`09_build_vendor/files_compat.go` 的 `downgradeLegacyNativeMapperVintf` 仅当官方 `system` 缺失 `AIMapper`（`sysLacksAIMapper`）时才创建 `vendor/lib64/hw/android.hardware.graphics.mapper@4.0-impl.so -> mapper.arm.so`；当 GRF 高代 `system`（A16，自带 AIMapper）搭配 `android13`（`lineage-20.0`）`composer@2.4-service` 时，跳过创建该软链接，但 `lineage-20.0` 编译的 `mapper.arm.so` 仅导出 HIDL `HIDL_FETCH_IMapper`，`composer@2.4` 直取 HIDL `IMapper@4.0` 失败引发 `SurfaceFlinger` 中止。
   - **32 位二进制未重定向**：`firmware_measured_extra.go` 的 `elf32Candidates` 漏列 `/system_ext/bin/unisocaudioutils`，未能自动生成空占位重定向规则。
   - **展锐 Radio/IMS AIDL 占位事务返回 `STATUS_UNKNOWN_TRANSACTION`**：`VILLAON-V671NP` 在 `/vendor/etc/cdh_radio_placeholders.txt` 声明了 15 个 `vendor.unisoc.hardware.radio.*` 实例。`devices/hardware/android.hardware.radio/src/main.cpp` 原 `OnPlaceholderTransact` 无条件返回 `STATUS_UNKNOWN_TRANSACTION`：
     - `com.spreadtrum.ims`（`ImsRIL` / `ImsRadioServiceProxy.setAidl`）调 `IImsRadio.getInterfaceVersion()`（`0xffffff`）与 `setImsResponseFunctions` 抛 `RemoteException: Method setImsResponseFunctions is unimplemented.`，`mHalVersion` 置为 `-1.-1`，随后 `ImsRadioInterface.canMakeRequest` 落入空/版本不足分支，触发 OEM `String.format` 格式化参数个数错误（`MissingFormatArgumentException: Format specifier '%s'`）；
     - `com.android.phone`（`RadioInteractorCore`）同样因 `ExtRadio*Proxy` 置空、`mHalVersion` 为 `-1.-1`，在 `RadioInteractorHandler.handleMessage(what=72)` 调度期间进入 `RadioInteractorCore.canMakeRequest` 的失败分支，对仍在使用的入参 `Message` 同步调用 `msg.sendToTarget()`，连续抛出 `IllegalStateException: This message is already in use.` 最终触发 `RescueParty` `WARM_REBOOT`。

---

## 证据

- [`log-fail.txt`](log-fail.txt)：记录重跑前 `13-TP1A.220624.014-20260713 -> VILLAON-V671NP-android16` 错配状态、第 1 轮切回 `android13` 时 `E ComposerResources: failed to get mapper 2.0 service` / `unisocaudioutils: Exec format error` 失败日志、以及第 2 轮 `Method setImsResponseFunctions is unimplemented.` / `MissingFormatArgumentException` / `This message is already in use` / `RescueParty WARM_REBOOT` 崩溃堆栈与 `qemu.log` 两次 `Starting kernel`。
- [`log-pass.txt`](log-pass.txt)：记录 `02_prepare` 打印 `分区 Android 版本: system=16, system_ext=16, product=16, vendor=13, odm=13, vendor_dlkm=13`、`BuildID` 与 `VILLAON-V671NP-android13 (lineage-20.0)` 对齐过程、`build_hardware`（`android.hardware.radio`）~ `pack_vda` 构建结果，以及冷启动 60s+ `Enforcing` / `sys.boot_completed=1` / `ro.system.build.version.release=16` / `ro.vendor.build.version.release=13` / `dumpsys -l service count = 322` / 零重启零电话崩溃日志。
- [`villaon_v671np_a13_vendor_home.png`](villaon_v671np_a13_vendor_home.png)：`VILLAON-V671NP`（`16-TP1A.220624.014-20260713`，`VILLAON-V671NP-android13` / `lineage-20.0`）60s+ 冷启动后的 VNC 桌面截图，画面完整进入 `Launcher3QuickStepGo` 桌面且无任何崩溃弹窗。

---

## 改动

1. **`02_prepare` 全分区版本解析、`system` 版前缀 `buildId` 与 `vendor` 版 Lineage 对齐**：
   - 新增 [`partition_versions.go`](../../../../agent/modules/pipeline/stages/02_prepare/partition_versions.go) 与 [`partition_versions_test.go`](../../../../agent/modules/pipeline/stages/02_prepare/partition_versions_test.go)：提供 `MeasurePartitionAndroidVersions` / `FormatPartitionAndroidVersions`，从 `system`、`system_ext`、`product`、`vendor`、`odm`、`vendor_dlkm`、`odm_dlkm` 提取 `build.prop` 的 Android 版本。
   - 更新 [`identity.go`](../../../../agent/modules/pipeline/stages/02_prepare/identity.go)（`ResolvePrepareIdentity`、`resolveBuildIDAndroidMajor`、`resolveVendorAndroidMajor`、`replaceOrPrefixBuildIDMajor`）与 [`identity_test.go`](../../../../agent/modules/pipeline/stages/02_prepare/identity_test.go)：云机 `buildId` 前缀改为由 `system` 分区 Android 主版本替换 `vendor` 前缀。
   - 更新 [`grf_reconcile.go`](../../../../agent/modules/pipeline/stages/02_prepare/grf_reconcile.go)（`reconcileGRFVersionConfig`、`measureSystemAndroidProps`、`vendorAndroidMajorFromProps`、`sanitizeCrossGenVendorHardwareWithBrand`）、[`grf_reconcile_test.go`](../../../../agent/modules/pipeline/stages/02_prepare/grf_reconcile_test.go)、[`plan.go`](../../../../agent/modules/pipeline/stages/02_prepare/plan.go)、[`register.go`](../../../../agent/modules/pipeline/stages/02_prepare/register.go)、[`ai_profile.go`](../../../../agent/modules/pipeline/stages/02_prepare/ai_profile.go) 与 [`db_crud.go`](../../../../agent/modules/pipeline/internal/device/db_crud.go)：持久化 `extra.partitionAndroidVersions`、打印各分区版本、并将配置模版与 Lineage 基底严格切换/校验到官方 `vendor` Android 版本。
2. **`manifestview` 与前端工作台全分区版本展示**：
   - 更新 [`types.go`](../../../../agent/internal/manifestview/types.go)、[`inspector.go`](../../../../agent/internal/manifestview/inspector.go)（`PartitionVersions`、`extractPartitionVersions`）与 [`inspector_test.go`](../../../../agent/internal/manifestview/inspector_test.go)。
   - 更新 [`types.ts`](../../../../agent/web/src/core/types.ts)、[`format.ts`](../../../../agent/web/src/core/format.ts)（`partitionVersionsTooltip`、`partitionVersionsBadge` 显示跨代 `S16/V13` 警示徽标及各分区悬浮详情）与 [`catalog.ts`](../../../../agent/web/src/modules/workbench/catalog.ts) / [`index.ts`](../../../../agent/web/src/modules/workbench/index.ts) / [`stages.ts`](../../../../agent/web/src/modules/workbench/stages.ts)。
3. **全仓存量回填与 25 个跨代 `buildId` 迁移**：
   - 为全仓 `47` 个 `devices/manifest/versions/*/*/*/config.json` 回填 `extra.partitionAndroidVersions`；
   - 将 `25` 个前置版本错配的跨代 `buildId` 零冲突重命名为 `<system_major>-*`，同步更新 `devices/manifest/devices/*/*/device-profile.{json,md}`、`out/devices/dist/*/*/*/device.json` / `vda-info.txt`，并对 3 个对应的已停止运行态实例（`Asteroids_16-UKQ1.250612.001-20260810`、`Infinix-X6876_16-UP1A.231005.007-20260423`、`sun_16-AQ3A.250214.001-20260904`）目录重命名并执行 `qemu-img rebase -u`。
4. **GRF `system=16 + vendor=13` 三项运行态兼容修复**：
   - [`files_compat.go`](../../../../agent/modules/pipeline/stages/09_build_vendor/files_compat.go)（`downgradeLegacyNativeMapperVintf`）与 [`props_test.go`](../../../../agent/modules/pipeline/stages/09_build_vendor/props_test.go)：检测到 `android.hardware.graphics.composer@2.4-service` 或 vendor VINTF 含 HIDL `mapper`（且不含 native `mapper`）时，即使 `system` 已含 AIMapper（如 A16 GRF），仍保留/补建 `vendor/lib64/hw/android.hardware.graphics.mapper@4.0-impl.so -> mapper.arm.so` 并补充 `android.hardware.graphics.mapper@4.0.xml` 声明；
   - [`firmware_measured_extra.go`](../../../../agent/modules/pipeline/stages/02_prepare/firmware_measured_extra.go)（`extractMeasuredSystemCompatRedirects` `elf32Candidates`）与 [`VILLAON-V671NP-android13/module/config.json`](../../../manifest/configs/villaon/VILLAON-V671NP/VILLAON-V671NP-android13/module/config.json)：新增 ELF32 `/system_ext/bin/unisocaudioutils -> /mnt/vendorpatch/system_ext/bin/unisocaudioutils` 重定向规则；
   - [`main.cpp`](../../../hardware/android.hardware.radio/src/main.cpp)：保留非展锐占位的 `OnPlaceholderTransact`，新增 `OnUnisocRadioPlaceholderTransact` 仅对 `vendor.unisoc.hardware.radio.*` 占位应答 `EX_NONE`、`getInterfaceVersion=2`（`0xffffff`）、`getInterfaceHash="notfrozen"`（`0xfffffe`）与空串负载（`code >= FIRST_CALL_TRANSACTION`）。

---

## 验证

1. **端到端机型验证（`villaon/VILLAON-V671NP`）**：
   - `prepare` 实测识别 `system=16, system_ext=16, product=16, vendor=13, odm=13, vendor_dlkm=13`，将版本重对齐为 `16-TP1A.220624.014-20260713` 并绑定 `VILLAON-V671NP-android13`（`lineage-20.0`），移除无主旧模版 `VILLAON-V671NP-android16`。
   - 全流程重编并冷启动等待 >60s：
     - `qemu.log` `Starting kernel` 计数 = `1`、`Exec format error` = `0`、`getenforce` = `Enforcing`；
     - `sys.boot_completed=1`、`init.svc.bootanim=stopped`、`init.svc.surfaceflinger=running`；
     - `ro.system.build.version.release=16`、`ro.vendor.build.version.release=13`、`dumpsys -l service count = 322`；
     - `android.hardware.radio-service` / `com.android.phone` / `com.spreadtrum.ims` 常驻运行，`logcat` 中 `This message is already in use`、`MissingFormatArgumentException`、`RescueParty` 与 `phone`/`ims` `am_crash` 均为 `0`；
     - 抓取 VNC 桌面截图（[`villaon_v671np_a13_vendor_home.png`](villaon_v671np_a13_vendor_home.png)）直入 `Launcher3QuickStepGo` 桌面且无错误弹窗。
2. **自动化构建与单元测试验证**：
   - 前端 `npm run build`（`tsc --noEmit && vite build`）通过；
   - `go test -count=1 ./internal/manifestview/... ./internal/server/... ./web/... ./modules/pipeline/... ./modules/daemon/...` 全量通过；
   - `cdh-pipeline` / `cdha` / `host-daemon` 全套重编并通过 `cdh-pipeline device list` 校验 `47` 个版本全部就位。

---

## 遗留

1. **其余历史跨代 GRF 机型模版待按需重编切换**：全仓当前 `27` 个跨代 GRF 版本中，已有 `9` 个（`ASUS_AI2205`、`G1130`、`ARW`、`honor/sun`、`P616F05`、`symphony/G27`、`symphony/MAX_10`、`villaon/VILLAON-V671NP`、`xiaomi/popsicle 17-OS4.0.5.0.XPBCN`）的 `configId` 对齐至 `vendor` 同代 Lineage 模版；以下 `18` 个版本已完成 `buildId`（`<system>-*`）与全部分区版本记录，其模版仍为历史按 `system` 绑定的高代配置，后续重跑对应机型的 `prepare` + `device build` 即可由 `reconcileGRFVersionConfig` 自动切回 `vendor` 同代 Lineage 并完成整机验证：
   - `honor/HNLNA-M/16-TP1A.220624.014-20260422`（`sys=16, vnd=13`，当前 `android16`）
   - `honor/bengal_515/{16-TKQ1.240227.001-20260708,16-TKQ1.240227.001-20260922}`（`sys=16, vnd=13`，当前 `android16`）
   - `infinix/Infinix-X6876/16-UP1A.231005.007-20260423`（`sys=16, vnd=14`，当前 `android16`）
   - `itel/itel-A6612L/16-BP2A.250605.031.A3-20260823`（`sys=16, vnd=13`，当前 `android16`）
   - `meizu/Meizu22/16-AQ3A.250411.001-20260127`（`sys=16, vnd=15`，当前 `android16`）
   - `motorola/aito/16-W1UCS36H.96-35-8-9-20260801`（`sys=16, vnd=14`，当前 `android16`）
   - `nothing/Asteroids/{15-UKQ1.241011.001-20251013,16-UKQ1.250612.001-20260810}`（`sys=15/16, vnd=14`，当前 `android15`/`android16`）
   - `oneplus/PJE110/15-TP1A.220905.001-20250926`（`sys=15, vnd=13`，当前 `android15`）
   - `oppo/PME110/17-BP2A.250605.015-20260923`（`sys=17, vnd=16`，当前 `android17`）
   - `realme/RMX3823/15-TP1A.220905.001-20250912` 与 `realme/RMX5112/15-TP1A.220905.001-20251025`（`sys=15, vnd=13`，当前 `android15`）
   - `samsung/pa3q/16-AP3A.240905.015.A2-20260812`（`sys=16, vnd=15`，当前 `android16`）
   - `tcl/asterpro/15-TP1A.220624.014-20251217`（`sys=15, vnd=13`，当前 `android15`）
   - `tcl/vinca/16-AP3A.240905.015.A2-20260512`（`sys=16, vnd=15`，当前 `android16`）
   - `ulefone/GQ5011AF1/{15-GQ5011AF1_EEA_V17-20260429,16-GQ5011AF1_EEA_V13-20260825}`（`sys=15/16, vnd=14`，当前 `android15`/`android16`）
2. **个体运行态次要日志**：`VILLAON-V671NP` 实测 `logcat -b events` 仍记录 `com.android.bluetooth` 1 次 `SIGABRT` 与 `com.android.chrome` 缺 `libmonochrome.so` 导致的后台 `UnsatisfiedLinkError`（不影响桌面启动、VNC 展示与 `RescueParty` 稳定性）。
