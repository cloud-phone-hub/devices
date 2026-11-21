# #291 全库 15 台机型 / 18 个版本 Vendor→Lineage 底座对齐重建与跨代 GRF 共性修复

- **设备/机型**：全库 15 台历史未对齐机型共 18 个版本（`oppo/PME110`、`tcl/vinca`、`meizu/Meizu22`、`samsung/pa3q`、`nothing/Asteroids` A15/A16、`motorola/aito`、`infinix/Infinix-X6876`、`ulefone/GQ5011AF1` A15/A16、`oneplus/PJE110`、`realme/RMX3823`、`realme/RMX5112`、`tcl/asterpro`、`itel/itel-A6612L`、`honor/HNLNA-M`、`honor/bengal_515` `20260708`/`20260922`）
- **日期**：2026-11-21
- **关联条目**：[../issue-prepare-partition-versions-and-vendor-lineage-align/solution.md](../issue-prepare-partition-versions-and-vendor-lineage-align/solution.md)（#290）、[../issue-android13-lineage20-hal-split/solution.md](../issue-android13-lineage20-hal-split/solution.md)（#289）

---

## 一、问题

在 #290 确立「**Lineage 底座由官方 `vendor` 分区 Android 大版本决定**」（`13→lineage-20.0`、`14→lineage-21.0`、`15→lineage-22.2`、`16→lineage-23.2`、`17→lineage-24.0`）的共识后，仓库中仍有 17 份历史未对齐模版（覆盖 15 台机型、18 个版本）停留在按 `system` 对齐的旧底座（且部分机型存在如 `Asteroids-android15/16`、`GQ5011AF1-android15/16` 重复模版）。

将这 18 个版本按 4 个底座批次全量迁回正确 `vendor` 代际底座并执行 Enforcing 冷启动审图验收时，依次暴露出 7 类共性/机型级故障与缺失硬件通道：

1. **`oppo/PME110`（Sys A17 + Ven A16 → `lineage-23.2`）**：`GraphicBufferMapper` 循环报 `F GraphicBufferMapper: gralloc-mapper is missing` 崩溃，黑屏无法进桌面。
2. **Android 14 `allocator` 编译链接失败**：`cdh_os_option_wrap.cpp` 触发 `undefined symbol: __wrap_drv_get_os_option`。
3. **`nothing/Asteroids`（Sys A16 + Ven A14 → `lineage-21.0`）**：
   - `vndservicemanager` 启动即因 `libapexsupport.so` 缺失 `_ZNSt3__122__libcpp_verbose_abortEPKcz` 崩溃，导致全部 APEX 未挂载；
   - `system_server` 循环崩溃（`com.aura.oobe.solutions` 缺 `INSTALL_PACKAGES` 特权白名单），且 `vendor_hal_perf2_service` AVC 刷屏。
4. **`motorola/aito` 等跨后缀 MTK 机型（`gralloc.common.so` + `mapper.mediatek.so`）**：`CdhGrallocProxy: failed to dlopen mapper.common.so`，触发 `SurfaceFlinger` `RenderEngine` `GaneshBackendTexture: Failed to create a valid texture` 循环 `SIGABRT`。
5. **`motorola/aito` 缺失 `android.hardware.biometrics` HAL**：`com.android.systemui` 的 `WakeUpAnimationController` 抛出 `FingerprintManager` 空指针异常。
6. **MTK 机型 `com.android.phone` 循环崩溃**（实测复现于 `motorola/aito`、`infinix/Infinix-X6876`）：`IMtkRadioExData$Stub$Proxy.getInterfaceVersion()` 抛出 `IllegalArgumentException`。
7. **`ulefone/GQ5011AF1`（15 版）及批次四多款机型缺失蓝牙/振动 HVC 通道**：`com.android.bluetooth` 反复 `SIGABRT`（`Done waiting for debug information after HCI timeout (RESET)`）。

---

## 二、根因

1. **`CdhAllocator.h` 未校验目标 `mapper.<gralloc>.so` 存在性**：`ResolveMapperSuffix()` 直接返回非空的 `ro.hardware.gralloc` 属性值，未检查 `/vendor/lib64/hw/mapper.<suffix>.so` 是否实际存在，导致跨代重命名后返回错误后缀。
2. **`cdh_os_option_wrap.cpp` 版本门控过严**：原条件 `CDH_ANDROID_VERSION >= 15` 排除了 Android 14（其 `external/minigbm` 已启用 `--wrap=drv_get_os_option`）。
3. **跨代 GRF `libapexsupport.so` C++ ABI 不兼容**：`files_compat.go` 的 `backfillApexSupport()` 无条件提取高代官方 `system/lib64/libapexsupport.so`（依赖新版 `libc++` 符号 `_ZNSt3__122__libcpp_verbose_abortEPKcz`），放入低代 Android 14 `vendor/lib64/` 后无法被旧版 `libc++.so` 解析。
4. **`Asteroids` 官方 `init` overlay 挂载被 `lineage-21.0` SELinux 拦截**：官方 `/system/bin/init` 在 `late-fs` 执行 `mount overlay /system/noth/etc ... context=u:object_r:system_file:s0`，但切到 `lineage-21.0` 后缺失官方 `vendor_sepolicy.cil` 中的 `filesystem (mount relabelfrom relabelto associate)` 授权，导致 `/system/noth/etc/permissions/priv-com.aura.oobe.solutions.xml` 未落位，而 `fstab.aura` 已 bind-mount `NTAura.apk`，触发特权权限校验崩溃。
5. **`gralloc_proxy.c` 回退逻辑短路**：`init_target()` 步骤 1 从自身文件名 `gralloc.common.so` 提取 `suffix="common"` 尝试 `dlopen("mapper.common.so")` 失败后未清空 `suffix`，导致步骤 2/3/4 的 `if (handle == NULL && suffix[0] == '\0')` 条件全部跳过，无法继续尝试 `mapper.mediatek.so`。
6. **`cdh_radio_placeholders.txt` 与 `android.hardware.radio@mtk` 冲突**：`radio@mtk`（`devices/hardware/android.hardware.radio/src/main.cpp`）已内置 `IMtkRadioExSim` 真实现及 `data/messaging/modem/network/voice` 五类不可声明于 VINTF 的别名；若这 6 类前缀残留在 `cdh_radio_placeholders.txt`，`synthesizePlaceholderVintfManifest()` 会将其写入 `cdh_vendor_placeholders.xml`（VINTF declared）并在运行态被占位 binder 覆盖，导致 `MtkRIL` 调 `waitForDeclaredService` 事务码错位崩溃。
7. **设备清单 `extra.hostHvc` 漏配 `host-hvc-bluetooth` / `host-hvc-haptic`**：`GQ5011AF1.json` 及批次四 7 台机型均声明了 `android.hardware.bluetooth` / `android.hardware.vibrator`，但 `extra.hostHvc` 未声明 `host-hvc-bluetooth: hvc5` 与 `host-hvc-haptic: hvc6`，`host-daemon` 未拉起 `netsimd`，`/dev/hvc5` 悬空为 null，导致开机拉起蓝牙栈发送 HCI RESET 后必超时 `SIGABRT`。

---

## 三、证据

- 修复前 5 类典型失败日志汇总：[log-fail.txt](log-fail.txt)
- 全部 18 版对齐通过汇总、全库 42 模版/47 版本零错配对账与全量 37 包 Go 单测记录：[log-pass.txt](log-pass.txt)
- 各批次代表性 Enforcing 冷启动亮屏审图截图：
  - 批次一（Ven A16 → `lineage-23.2`）：[PME110_home.png](PME110_home.png)
  - 批次二（Ven A15 → `lineage-22.2`）：[pa3q_home.png](pa3q_home.png)
  - 批次三（Ven A14 → `lineage-21.0`）：[Asteroids_16_home.png](Asteroids_16_home.png)、[aito_16_home.png](aito_16_home.png)、[Infinix-X6876_16_home.png](Infinix-X6876_16_home.png)、[GQ5011AF1_15_home.png](GQ5011AF1_15_home.png)
  - 批次四（Ven A13 → `lineage-20.0`）：[PJE110_home.png](PJE110_home.png)、[asterpro_home.png](asterpro_home.png)、[itel-A6612L_home.png](itel-A6612L_home.png)、[bengal_515_20260708_home.png](bengal_515_20260708_home.png)、[bengal_515_20260922_home.png](bengal_515_20260922_home.png)

---

## 四、改动

1. **通用图形 HAL 修复（重编 Android 13~17 `android.hardware.graphics.allocator` 全代产物）**：
   - `devices/hardware/android.hardware.graphics.allocator/src/CdhAllocator.h`：新增 `MapperLibExists()` 校验 `ro.hardware.gralloc` 对应的 `mapper.<suffix>.so` 存在性，不存在时自动扫描 `/vendor/lib64/hw/mapper.*.so`。
   - `devices/hardware/android.hardware.graphics.allocator/src/cdh_os_option_wrap.cpp`：编译门控由 `CDH_ANDROID_VERSION >= 15` 调整为 `>= 14`。
   - `devices/hardware/android.hardware.graphics.allocator/src/gralloc_proxy.c`：抽取 `try_open_mapper_by_suffix()`，步骤 1 按自身文件名后缀 `dlopen` 失败后继续依次探测 `ro.hardware.gralloc`、常见厂商后缀表（`mediatek/qti/pixel/arm/exynos/minigbm`）与目录扫描。
2. **构建流水线 `09_build_vendor` 两项通用防呆 + 白盒单测**：
   - `agent/modules/pipeline/stages/09_build_vendor/files_compat.go`：
     - 新增 `vendorLibcxxHasSymbol()`，检测高代官方 `libapexsupport.so` 依赖 `_ZNSt3__122__libcpp_verbose_abortEPKcz` 且当前 `vendor/lib64/libc++.so` 未导出时，自动回退纯 C 零 C++ 依赖自编版本；
     - `synthesizePlaceholderVintfManifest()` 检测 `bin/hw/android.hardware.radio-service` 含 `IMtkRadioExSim` 时，自动从 `vendor/etc/cdh_radio_placeholders.txt` 剔除上述 6 类冲突前缀并跳过写入 `cdh_vendor_placeholders.xml`。
   - `agent/modules/pipeline/stages/09_build_vendor/files_test.go`：补齐 `TestVendorLibcxxHasSymbol` 与 `TestSynthesizePlaceholderVintfManifestPrunesMtkRadioReserved` 回归单测。
3. **机型配置、SELinux、HAL 与硬件 HVC 通道补齐**：
   - `devices/manifest/configs/nothing/Asteroids/Asteroids-android14/vendor/sepolicy/vendor_sepolicy.cil`：补齐 `init` overlay 挂载 `system_file`/`vendor_configs_file` 所需的 `filesystem (mount relabelfrom relabelto associate)` 权限及 `(dontaudit domain vendor_hal_perf2_service (service_manager (find)))`。
   - `devices/manifest/configs/motorola/aito/aito-android14/hardware/config.json`：补齐 `android.hardware.biometrics` HAL（并编出 A14 产物），同步清理 `cdh_radio_placeholders.txt` 冲突项。
   - `devices/manifest/devices/{ulefone/GQ5011AF1,oneplus/PJE110,realme/RMX3823,realme/RMX5112,tcl/asterpro,itel/itel-A6612L,honor/HNLNA-M,honor/bengal_515}.json`：补齐 `extra.hostHvc` 中的 `host-hvc-bluetooth: hvc5` 与 `host-hvc-haptic: hvc6`。
   - 随 18 个版本全量 `--refresh` 重建完成全部 17 份历史未对齐模版迁移合并与孤儿旧模版自动清理（如 `Asteroids-android15/16` → `Asteroids-android14`、`GQ5011AF1-android15/16` → `GQ5011AF1-android14`、`bengal_515-android16` → `bengal_515-android13`）。

---

## 五、验证

1. **全库配置与产物对账**：脚本扫描全仓 `devices/manifest/configs/` 与 `versions/`，确认 `Configs: 42, Versions: 47, Referenced: 42, Mismatches: 0, Orphans: 0`，全部 18 版 `out/devices/dist/<brand>/<device>/<buildId>/{vda.img,patch.img,data.qcow2,device.json}` 零缺失。
2. **全量 Go 单元测试**：`go test -count=1 ./modules/pipeline/... ./modules/daemon/... ./internal/manifestview/...` 共 37 个包全部 `PASS`（含新增两项回归测试）。
3. **18 / 18 版本 Enforcing 冷启动人工审图验收**（独立测试 `host-daemon` 端口 `19055`）：
   - 批次一（`lineage-23.2`）：`oppo/PME110`（83s Enforcing，ColorOS 17 桌面，`mean_luma=74.2`）。
   - 批次二（`lineage-22.2`）：`tcl/vinca`（206s Enforcing，TCL UI 9.0，`mean_luma=183.6`）、`meizu/Meizu22`（69s Enforcing，Flyme，`mean_luma=231.0`）、`samsung/pa3q`（132s Enforcing，OneUI，`mean_luma=241.7`）。
   - 批次三（`lineage-21.0`）：`nothing/Asteroids` 15 版（33s，Nothing OS 3.2，`mean_luma=70.3`）& 16 版（33s，Nothing OS 4.1，`mean_luma=71.4`）、`motorola/aito`（41s，Moto 桌面，`mean_luma=160.5`）、`infinix/Infinix-X6876`（61s，满信号桌面，`mean_luma=98.7`）、`ulefone/GQ5011AF1` 16 版（37s，4G 满格桌面，`mean_luma=96.6`）& 15 版（37s，4G 满格桌面且蓝牙零崩溃，`mean_luma=166.2`）。
   - 批次四（`lineage-20.0`）：`oneplus/PJE110`（75s，ColorOS 15，`mean_luma=159.7`）、`realme/RMX3823`（112s，realme UI 6.0，`mean_luma=191.9`）、`realme/RMX5112`（91s，realme UI 6.0，`mean_luma=66.1`）、`tcl/asterpro`（96s，4G 满格 TCL 桌面，`mean_luma=146.4`）、`itel/itel-A6612L`（37s，itel 主桌面，`mean_luma=139.0`）、`honor/HNLNA-M`（70s，4G 满格 MagicOS 欢迎页，`mean_luma=171.8`）、`honor/bengal_515` `20260708`（55s，MagicOS 竖屏桌面，`mean_luma=98.5`）& `20260922`（59s，MagicOS 横屏 Honor Pad X7b 桌面，`mean_luma=29.7`）。

---

## 六、遗留

- 荣耀 `Sys 16 + Ven 13` 机型（`HNLNA-M`、`bengal_515`）在冷启动早期窗口偶见单个应用 `hwuiTask` 触发一次 `gralloc-mapper is missing` 后自愈（重启后常驻正常、亮屏与核心 HAL 均不受阻），后续可针对 Android 13 `minigbm` 并发初始化 `/dev/dri/renderD128` 竞态做专项加固。
- `itel/itel-A6612L` 官方预装 `com.android.chrome` 仅含 32-bit `libmonochrome.so`，在 64-bit-only ARM64 环境中属于上游预装包特性，不影响系统核心服务与桌面运行。
- 其余历史未涉及本次重建的机型若声明了 `android.hardware.bluetooth` / `android.hardware.vibrator` 但 `extra.hostHvc` 未配 `hvc5`/`hvc6`，建议后续在 `02_prepare` 中增加按 HAL 清单自动补齐 `hostHvc` 的统一守卫。
