# #254：未遮蔽 `/vendor/apex` 且启用自研 `drm` 机型的 Widevine APEX VINTF 冲突与 `hal_drm_widevine` 域补齐

- 设备：跨机型通用（闭环验证于 `meizu/Meizu22` `16-AQ3A.250411.001-20260127`，同步覆盖 `oppo/CPH2797` 与 `samsung/pa3q`）
- 日期：2026-11-18
- 状态：**已闭环**（`Meizu22` 冷启动无 `VINTF parse error`、步骤 6b 自动赋予 `hal_drm_widevine_exec` 标签、VNC 截图 `pass-meizu22.png` 平均亮度 `45.4` / 峰值 `255`）

---

## 一、问题

当机型 `hardware/config.json` 启用了 `android.hardware.drm`（其公共静态树自 2026-11-08 起包含自研 `android.hardware.drm-service.widevine` 二进制、rc 及 `android.hardware.drm-service.widevine.xml`）且未对 `/vendor/apex` 做重定向遮蔽时，在 `meizu/Meizu22` 上暴露出两连环故障：

1. **VINTF 双花冲突阻断开机**：`servicemanager` 启动即报 `VINTF parse error: Cannot add manifest fragment /apex/com.google.android.widevine/etc/vintf/com.google.android.widevine.xml: Conflicting FqInstance: IDrmFactory/widevine (@1)` → `NULL VINTF MANIFEST!: device`，全部 HAL 服务查询失败、无法引导。
2. **裁掉 XML 后仍周期性崩溃**：若仅在 `vintf-drop` 移除自研 `android.hardware.drm-service.widevine.xml` 但机型 `vendor_sepolicy.cil` 缺少 `hal_drm_widevine` 域定义，`init` 启动自研 `/vendor/bin/hw/android.hardware.drm-service.widevine` 时报 `cannot setexeccon('u:r:hal_drm_widevine:s0'): Invalid argument` 并以 signal 6 循环退出（46 次）触发 `sys.init.updatable_crashing=1`。

全仓排查 36 个配置确认：仅 7 个配置启用了 `android.hardware.drm`，其中 `gold`、`G1170`、`oriole-android14`、`RMX6688` 已分别在机型层配齐 `vintf-drop` 与 `hal_drm_widevine` 策略，仅 `meizu/Meizu22`、`oppo/CPH2797` 与 `samsung/pa3q` 缺漏。

---

## 二、根因

1. 官方 `vendor_a.img:/apex/com.google.android.widevine*.apex`（`Meizu22`、`CPH2797` `dump.erofs` 已实测存在）在运行态由 `apexd` 挂载到 `/apex/com.google.android.widevine/`，其内已含 `etc/vintf/com.google.android.widevine.xml` 声明 `IDrmFactory/widevine (@1)`；与自研模块 `/vendor/etc/vintf/manifest/android.hardware.drm-service.widevine.xml` 重叠导致 `libvintf` 判冲突。
2. `android.hardware.drm` 模块的 `rc` 指定 `seclabel u:r:hal_drm_widevine:s0`，但因已有多台机型在各自 `vendor/sepolicy/vendor_sepolicy.cil` 中声明了 `(type hal_drm_widevine)`（`secilc` 禁止同名 `(type ...)` 重复声明），该策略无法直接塞入公共 `android.hardware.drm/sepolicy/`，必须在缺失该定义的启用 `drm` 机型层按通用片段补齐。

---

## 三、证据

- 失败现场：[`log-fail.txt`](log-fail.txt)（`Meizu22` `VINTF parse error` 与 `NULL VINTF MANIFEST!: device`；另 `issue-prepare-audio-config-ensure-r-submix/log-fail.txt` 中记录 `cannot setexeccon('u:r:hal_drm_widevine:s0'): Invalid argument` 46 次重启记录）。
- 修复与通过现场：[`log-pass.txt`](log-pass.txt) 及 [`pass-meizu22.png`](pass-meizu22.png)（`09_build_vendor` 步骤 6b 输出 `android.hardware.drm-service.widevine -> hal_drm_widevine_exec`、步骤 9.6c 裁掉 `android.hardware.drm-service.widevine.xml`；冷启动通过 `verify_boot.py` 并进入 Flyme 桌面，平均亮度 `45.4` / 峰值 `255`）。

---

## 四、改动

1. **`meizu/Meizu22`（`Meizu22-android16`）**：
   - [`vendor/config.json`](../../../devices/manifest/configs/meizu/Meizu22/Meizu22-android16/vendor/config.json) 新增 `"vintf-drop": ["android.hardware.drm-service.widevine.xml"]`；
   - [`vendor/sepolicy/vendor_sepolicy.cil`](../../../devices/manifest/configs/meizu/Meizu22/Meizu22-android16/vendor/sepolicy/vendor_sepolicy.cil) 追加通用 `hal_drm_widevine` 类型声明与注册/访问授权（对齐 `RMX6688`，不含平台私有 `base_typeattr_*`）。
2. **`oppo/CPH2797`（`CPH2797-android16`）与 `samsung/pa3q`（`pa3q-android16`）**：
   - `dump.erofs` 核实 `CPH2797` 官方 `vendor_a.img:/apex` 含 `com.google.android.widevine.nonupdatable.apex`，在其 [`vendor/config.json`](../../../devices/manifest/configs/oppo/CPH2797/CPH2797-android16/vendor/config.json) 的 `vintf-drop` 增加 `"android.hardware.drm-service.widevine.xml"`；
   - 为 [`CPH2797/.../vendor_sepolicy.cil`](../../../devices/manifest/configs/oppo/CPH2797/CPH2797-android16/vendor/sepolicy/vendor_sepolicy.cil) 与 [`pa3q/.../vendor_sepolicy.cil`](../../../devices/manifest/configs/samsung/pa3q/pa3q-android16/vendor/sepolicy/vendor_sepolicy.cil) 追加同款通用 `hal_drm_widevine` 策略段。

---

## 五、验证

1. `meizu/Meizu22` 从 `build_vendor` 重构至 `pack_vda`（`patch.img 9da7ccfc7a8f`，`vda-6cb992ba9f6c`），`09_build_vendor` 消除原有的 `hal_drm_widevine_exec` 未定义告警并精准授予可执行标签。
2. 独占 CPU 冷启动 `Meizu22` 一次通过（`task-683`：`sys.boot_completed=1`、`init.svc.bootanim=stopped`、前台 `com.meizu.flyme.launcher/...QuickstepLauncher`、`roHardwareEgl=mesa`、`roHardwareVulkan=virtio`、`meanBrightness=45.4`、`peak=255`），`pass-meizu22.png` 核验完整 Flyme 桌面、时钟与图标渲染正常。

---

## 六、遗留

- `samsung/pa3q` 是否需要增加 `vintf-drop: ["android.hardware.drm-service.widevine.xml"]`，将在其后续 `01_unpack` 解出官方 `vendor_a.img` 后通过 `dump.erofs --ls --path=/apex` 实测确认。
- 若未来决定将 `hal_drm_widevine` 策略统一沉入 `devices/hardware/android.hardware.drm/sepolicy/`，需同步移除 `gold` / `G1170` / `oriole-android14` / `RMX6688` / `Meizu22` / `CPH2797` / `pa3q` 等机型层重复声明以避免 CIL 冲突。
