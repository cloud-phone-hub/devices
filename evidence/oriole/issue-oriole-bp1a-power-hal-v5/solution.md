# issue-oriole-bp1a-power-hal-v5：oriole BP1A（Android 15）因 Power HAL 缺失及 AIDL v6 未冻结桩导致卡加载页

## 1. 问题

`oriole_BP1A.250505.005-20250326`（Pixel 6，官方 Android 15 QPR2 `BP1A.250505.005` + `lineage-22.2` 底座）启动后一直停在开机动画加载页，`system_server` 每 5 秒在 `com.android.server.power.hint.HintManagerService.<init>` 处崩溃重启，满 4 次后触发 `RescueParty` 重启进 recovery。

## 2. 根因

两层递进根因：

1. **基线未声明 `android.hardware.power`**：
   - `lineage-22.2` 的 `/vendor/apex/com.android.hardware.power.apex` 在 07 阶段按 `vendor-apex: default`（不摊平）处理，且 `BP1A.250505.005-20250326` 的 `buildVendor.hardware` 未包含 `android.hardware.power`，导致运行态无 `IPower/default` 服务（`mPowerHal == null`）。
   - Android 15 QPR2（`BP1A.250505.005`）的 `HintManagerService.<init>` 仅在 `mPowerHal != null` 时调用 `mSupportInfo = getSupportInfo()`，随后却无条件读取 `mSupportInfo.headroom.isCpuSupported`，当 `mPowerHal == null` 时直接触发 `NullPointerException: Attempt to read from field '...SupportInfo.headroom' on a null object reference`。
2. **Android 15 的 `framework.jar` 中 `IPower` v6 方法为未冻结抛错桩，要求 HAL 报告 `version = 5`**：
   - `engine/vendor` 此前将 `android.hardware.power` 在 `profile.go`（`halTable()`）与 `modules/android.hardware.power.json` 中统一钉在 v6（Android 16 才冻结）。
   - 反汇编设备端 `/system/framework/services.jar` 与 `/system/framework/framework.jar` 证实：
     - `HintManagerService.getSupportInfo()` 先调 `mPowerHal.getInterfaceVersion()`：若返回值 `>= 6`，则调用 `mPowerHal.getSupportInfo()`；若 `< 6`，则在 Java 侧本地构造 `new SupportInfo()` 与 `new SupportInfo.HeadroomSupportInfo()`（`isCpuSupported = false, isGpuSupported = false`）。
     - 但在 Android 15 的 `framework.jar` 中，`IPower` v6 属于未冻结接口（编译期启用了 `--never_allow_unfrozen`），`IPower$Stub$Proxy.getSupportInfo()` 的字节码直接是 `throw new RemoteException("Method getSupportInfo is unimplemented.")`，根本不发起 Binder 调用。
     - 因此当 Android 15 上的 `android.hardware.power-service` 按 v6 编译（`getInterfaceVersion()` 返回 `6`）时，`HintManagerService` 必走 `>= 6` 分支并被 `framework.jar` 自己的 Proxy 抛异常炸死。

## 3. 证据

见同目录 [log-fail.txt](log-fail.txt) 与 [log-pass.txt](log-pass.txt)。

## 4. 改动

1. **`engine/vendor/build/internal/profile/profile.go`**：
   - `halTable()`（Android 14/15 基线）中 `"power"` 的 `android.hardware.power` 版本改为 `"5"`；
   - `halTable16()`（Android 16/17 覆盖表）中覆写 `"power"` 的 `android.hardware.power` 版本为 `"6"`。
2. **`engine/vendor/build/modules/`**：
   - `modules/android.hardware.power.json` 基础版本改为 `"5"`；
   - 新增 `modules/android16/android.hardware.power.json` 与 `modules/android17/android.hardware.power.json`，覆写 `"android.hardware.power": "6"` 并注入 `-DCDH_POWER_AIDL_VERSION=6`。
3. **`engine/vendor/hardware/android.hardware.power/src/power_service.cpp`**：
   - 用 `#if CDH_POWER_AIDL_VERSION >= 6` 守卫 v6 新增头文件与方法（`getSupportInfo`、`getCpuHeadroom`、`getGpuHeadroom`、`sendCompositionData`、`sendCompositionUpdate`），使 Android 14/15 产出纯净的 IPower v5 服务（`getInterfaceVersion() == 5`），Android 16/17 产出 IPower v6 服务。
4. **`engine/build/assets/devices.db`**：
   - 为 `oriole` 的 Android 15/16/17 基线（`versionId = 2, 3, 4`）在 `buildVendor.hardware` 中加入 `android.hardware.power`（`variant: default, version: 1.0`）。

## 5. 验证

1. `cdh-vendor` 单元测试全绿，分别执行 `cdh-vendor hal build android.hardware.power --android 14/15/16` 全部编译通过。
2. 重跑 `oriole` `BP1A.250505.005-20250326` 的 `07_build_vendor` → `09_build_patch` → `11_pack_vda`。
3. 重建并启动 `oriole_BP1A.250505.005-20250326` 实例（ADB `127.0.0.1:5919`）：
   - 25 秒内 `sys.boot_completed=1`、`init.svc.bootanim=stopped`；
   - `service check android.hardware.power.IPower/default` 为 `found`；
   - `system_server`（pid 643）与 `android.hardware.power-service`（pid 515）稳定运行无重启，前台焦点进入桌面 `com.google.android.apps.nexuslauncher/.NexusLauncherActivity`。

## 6. 遗留

- 官方 `product_a.img` 预置的壳包 `com.android.chrome` 缺少 64 位 `libmonochrome.so`（与 #105 相同，不影响系统启动与桌面运行）。

