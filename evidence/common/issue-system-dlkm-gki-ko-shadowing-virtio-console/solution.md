# #265：官方 `system_dlkm` 异代 GKI `.ko` 经 `/system/lib/modules` 软链遮蔽 `virtio_console.ko` 通用重定向及 `vendor.build-prop-unsets` 默认 5 键对齐

- 编号：`#265`
- 分组：`common`（引擎级 `02_prepare` / `internal/device` 跨机型修复，实证首发 `redmagic/NX809J`）
- 机型：通用带官方 `system_dlkm` 的 Android 14~17 机型（首发实证：`redmagic/NX809J` `16-BQ2A.250705.001-BP2A.250605.031.A3-20260527`，`lineage-23.2` + `kernel 6.12.81`）
- 状态：**已闭环**（2026-11-19）

## 1. 问题

在冷启动 `#15 redmagic/NX809J`（`16-BQ2A.250705.001-BP2A.250605.031.A3-20260527`）时，尽管 `09_build_vendor` 与 `10_pack_vendor_dlkm` 已按 `AGENTS.md` #6 全量自建了 `6.12.81-4k-cdh` 的全部 `.ko` 到 `vendor_dlkm.img`，但首轮启动 `vendor.dlkm_loader_mainline` 仍在 `[4.188s]` 连续报：

- `zsmalloc: disagrees about version of symbol module_layout`
- `dlkm_loader_mainline: Loading module /system/lib/modules/rfkill.ko with args ''`
- `rfkill: disagrees about version of symbol module_layout`
- `Failed to insmod '/system/lib/modules/rfkill.ko' with args '': Exec format error`
- 继而 `virtio_console.ko`、`zsmalloc.ko`、`zram.ko` 等均无法加载，导致 `/dev/hvc3`（keymint）与 `/dev/hvc7`（daemon）通道缺失、系统启动失败。

同时核查全仓基线配置发现，仅 `ro.frp.pst` 与 `ro.vndk.lite` 被默认放入 `build-prop-unsets`，而高通/跨代固件厂商原厂 `vendor/build.prop` 中常见的 `ro.crypto.metadata_init_delete_all_keys.enabled`、`ro.gfx.driver.0`、`ro.gfx.driver.1` 若未被默认 unset，会干扰密钥初始化与图形驱动加载。

## 2. 根因

1. 按 `AGENTS.md` #8，`super.img` 保持厂商原厂只读直通，而官方 `system_a.img` 中存在符号链接 `/system/lib/modules -> /system_dlkm/lib/modules`。当官方 `system_dlkm_a.img` 自带与云机内核代际/ABI 不一致的原厂 GKI `.ko` 时，`lineage-23.2` `dlkm_loader_mainline` 同时扫描 `/vendor_dlkm/lib/modules` 与 `/system/lib/modules`，会优先从 `/system/lib/modules/...` 读取异代 `.ko` 覆盖自建模块，`insmod` 因 `module_layout` 符号 CRC 不匹配报 `Exec format error`。
2. `internal/device/config.go` 与 `02_prepare` 原先默认仅剔除 `ro.frp.pst`、`ro.vndk.lite`，未将 `ro.crypto.metadata_init_delete_all_keys.enabled`、`ro.gfx.driver.0`、`ro.gfx.driver.1` 纳入通用默认 `build-prop-unsets`；且 `module/config.json` 的 `redirect` 规则表未自动将 `/system/lib/modules` 重定向到 `/mnt/vendorpatch/vendor_dlkm/lib/modules`。

## 3. 证据

- 失败现场日志：[log-fail.txt](log-fail.txt)（`cdh_redirect` 仅 14 条规则，`dlkm_loader_mainline` 读取 `/system/lib/modules/{rfkill,libarc4}.ko` 报 `disagrees about version of symbol module_layout` / `Exec format error`）。
- 通过验证日志：[log-pass.txt](log-pass.txt)（补齐 15 条 `cdh_redirect` 规则后，`dlkm_loader_mainline` 加载全部 11 个 `6.12.81-4k-cdh` 模块成功，`/proc/modules` 全量就位）。
- 运行截图：[pass-screen.png](pass-screen.png)（`redmagic/NX809J` `1216x2688` 完整桌面渲染）。

## 4. 改动

1. **`internal/device` 默认 `build-prop-unsets` 扩充为 5 键**（[config.go](../../../../agent/modules/pipeline/internal/device/config.go)）：
   - `injectSecurityPatchToVendor` 中 `defaultUnsets` 扩充为 `{"ro.crypto.metadata_init_delete_all_keys.enabled", "ro.frp.pst", "ro.gfx.driver.0", "ro.gfx.driver.1", "ro.vndk.lite"}`，并保持显式写入 `build-prop-sets` 的键优先不被移除。
2. **`02_prepare` 通用 `redirect` 与 `propUnsets` 自动补齐**（[register_parse.go](../../../../agent/modules/pipeline/stages/02_prepare/register_parse.go)、[apply_profile_configs.go](../../../../agent/modules/pipeline/stages/02_prepare/apply_profile_configs.go)、[apply_profile_test.go](../../../../agent/modules/pipeline/stages/02_prepare/apply_profile_test.go)）：
   - `buildBaselineJSON` 的 `propUnsets` 同步为上述 5 键；
   - `buildDeviceJSON` 在 `Modules.Redirect` 中默认写入 `{Type: "dir", From: "/system/lib/modules", To: "/mnt/vendorpatch/vendor_dlkm/lib/modules"}`（排在 `/odm` 之后、`/system/bin/update_verifier` 之前）；
   - 新增 `ensureSystemLibModulesRedirect`，对 `hasReadme=false` 的既有 `module/config.json` 幂等补入 `/system/lib/modules -> /mnt/vendorpatch/vendor_dlkm/lib/modules`；
   - 新增单测 `TestEnsureSystemLibModulesRedirectAndDefaultVendorUnsets`。
3. **`redmagic/NX809J` 配置同步**（[module/config.json](../../../../devices/manifest/configs/redmagic/NX809J/NX809J-android16/module/config.json)、[vendor/config.json](../../../../devices/manifest/configs/redmagic/NX809J/NX809J-android16/vendor/config.json)）：
   - 同步 15 条 `redirect`（含 `/system/lib/modules` 与 `/vendor/app`）与 5 项 `build-prop-unsets`。

## 5. 验证

1. Go 单测与构建：
   - 执行 `TMPDIR=/Volumes/work/cloud-phone-hub/out/agent/tmp go test -count=1 ./modules/pipeline/internal/device/... ./modules/pipeline/stages/02_prepare/... ./modules/pipeline/stages/06_build_vendor_boot/... ./modules/pipeline/stages/09_build_vendor/... ./modules/pipeline/stages/11_build_patch/...` 全部 `ok`；
   - 重编 `out/agent/dist/cdh-pipeline`。
2. `redmagic/NX809J` 实机冷启动验证：
   - 内核日志输出 `cdh_redirect: 规则来自 /lib/modules/cdh_redirect.rules：15 条`；
   - `vendor.dlkm_loader_mainline` 成功加载 `cdh_device_info`、`cdh_input_identity`、`cdh_media`、`rfkill`、`cfg80211`、`libarc4`、`mac80211`、`mac80211_hwsim`、`virtio_console`、`zsmalloc`、`zram` 共 11 个模块，零 `Exec format error`；
   - 运行态 `/proc/modules` 确认全部 11 模块 `Live`，`/dev/hvc3`（keymint）与 `/dev/hvc7`（`hvc-event` daemon）通道正常连通。

## 6. 遗留

- 已完成 `02_prepare` 自动生成逻辑的通用机型会直接继承该重定向与 5 键 `build-prop-unsets`；此前已生成且 `hasReadme=true` 的极少数手动维护机型仍按原配置保留不越权改写。
