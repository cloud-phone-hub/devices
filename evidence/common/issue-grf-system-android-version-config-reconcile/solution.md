# #269：`02_prepare` GRF 机型 `system_a.img` 真实 Android 代际自动校准与异代 HAL 清洗

- 编号：`#269`
- 分组：`common`（引擎级 / 跨机型，适用全部 GRF 跨代厂商冻结机型）
- 触发机型与复验样本：`nothing/Asteroids`（`14-UKQ1.241011.001-20251013`，`Asteroids_V3.2-251013-1406.zip`，`vendor=14 / system=15`，`Asteroids-android15` / `lineage-22.2`）
- 状态：**已闭环**（2026-11-19，单元测试全绿 + `#17 nothing/Asteroids` 实机冷启动验证通过）

## 1. 问题

GRF（Google Requirements Freeze）机型的官方 `vendor_a.img/build.prop` 常保留出厂冻结版本号（例如 `ro.vendor.build.version.release=14`、`14-UKQ1.241011.001-20251013`），而 OTA 升级后的 `system_a.img` 实际已演进为 Android 15/16。此前 `02_prepare` 仅按 `vendor/build.prop` 决定系统主版本与配置目录名，导致生成的 `<device>-android14` 配置错误绑定到 `lineage-21` 与异代 `graphics.composer@2.4`，并留存错配的配置文件夹。

## 2. 根因

1. `02_prepare` 原先未对 `super/system_a.img`（或 `system.img`）读取 `system/build.prop` / `build.prop` 的 `ro.system.build.version.release` 进行实测校准。
2. 当多个 version 版本号先后引用同一错误 `-android14` 配置或拆出 `-android15` 后，缺少自动按真实 `system` 主版本迁移 `configId`、清理无主过期 `-android<N>` 配置以及清洗异代 `graphics.composer` / `composer3` HAL 的联动逻辑。

## 3. 证据

- 现象记录：[log-fail.txt](log-fail.txt)。
- 测试与实机验收日志：[log-pass.txt](log-pass.txt)（`go test -count=1 ./modules/pipeline/stages/02_prepare/... ./modules/pipeline/internal/device/...` 全部通过；`nothing/Asteroids` `14-UKQ1.241011.001-20251013` 由 `Asteroids-android14` 自动拆分切换为 `Asteroids-android15` + `lineage-22.2` 并完成冷启动进入 Nothing OS 3.2 桌面，详见 `../../asteroids/issue-asteroids-a15-grf-sepolicy-and-refresh-config/`）。

## 4. 改动

（本次仅修改 `agent/modules/pipeline/stages/02_prepare/`，未触碰官方镜像）：

1. 新增 [grf_reconcile.go](../../../../agent/modules/pipeline/stages/02_prepare/grf_reconcile.go) 与 [grf_reconcile_test.go](../../../../agent/modules/pipeline/stages/02_prepare/grf_reconcile_test.go)：
   - `measureSystemAndroidProps`：从 `system_a.img` / `system.img` 提取 `system/build.prop` / `build.prop`，归一化 `13~17` 合法主版本；
   - `mergeSystemPropsIntoPrepareProps`：在不覆盖厂商 `vendor/odm` 身份键的前提下回填 `ro.system.*` 与缺失的 `ro.build.*`；
   - `reconcileCrossGenGraphicsHALs`：在切换/校准 `androidVersion` 时按代际剔除异代 `graphics.composer`/`composer3` 并补齐当代理应携带的 `graphics.allocator@2.0` / `composer(3)` / `power@1.0`。
2. 同步接入 [prop.go](../../../../agent/modules/pipeline/stages/02_prepare/prop.go)、[ai_profile.go](../../../../agent/modules/pipeline/stages/02_prepare/ai_profile.go)、[register.go](../../../../agent/modules/pipeline/stages/02_prepare/register.go)、[apply_profile.go](../../../../agent/modules/pipeline/stages/02_prepare/apply_profile.go)、[plan.go](../../../../agent/modules/pipeline/stages/02_prepare/plan.go) 等位置，支持对 GRF 包自动推导、拆分与清理无主过期 `-android<N>` 配置。

## 5. 验证

1. Go 单元测试（复测于 2026-11-19）：
   ```text
   ok  	cloud-phone-hub/agent/modules/pipeline/stages/02_prepare	1.459s
   ok  	cloud-phone-hub/agent/modules/pipeline/internal/device	4.642s
   ```
2. 实机闭环验证：`nothing/Asteroids` `14-UKQ1.241011.001-20251013` 经 `unpack -> prepare -> apply-profile` 后自动校准至 `Asteroids-android15`（`lineage-22.2`、`configId=37`、`vintf-version=8.0`、`graphics.allocator@2.0` + `graphics.composer3@3.0`），并清除无主 `Asteroids-android14`；叠加机型 SELinux/配置项修复（见 `../../asteroids/issue-asteroids-a15-grf-sepolicy-and-refresh-config/`）后冷启动一次通过，`passed=true`。

## 6. 遗留

- 对于 Android 14（`plat_sepolicy_vers=34.0`）GRF `vendor` 搭配 Android 15/16 `system` 的机型，`09_build_vendor` 把 `mapper.minigbm*` 重命名为官方 `mapper.<vendor>` 时因 `34.0` `plat_pub_versioned.cil` 无 `hal_graphics_mapper_service_34_0`，以及厂商私有受限属性/配置文件白名单，仍需在对应机型 `vendor/sepolicy/` 与 `vendor/config.json` 中逐机核查补齐（如 `#270`）。
