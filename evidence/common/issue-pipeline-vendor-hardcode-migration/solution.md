# #321 流水线特定机型/厂商 SELinux 规则、文件回填与建档映射全面配置化迁移

## 1. 问题

全仓 13 阶段构建流水线静态审计发现：
1. **文件重定向（`cdh_redirect`）在构建期已完全配置化（零硬编码）**，但 `09_build_vendor` 在 Go 源码中硬编码了大量特定厂商/机型专属逻辑：
   - [sepolicy_cil.go](../../../../agent/modules/pipeline/stages/09_build_vendor/sepolicy_cil.go) 写死了 6 个厂商专属 SELinux CIL 函数（约 600 行）：OPlus `hal_displaycolorfeature`/`hal_charger`、Honor `hal_hwfactoryinterface_default`、Unisoc `hal_miscdata_default`、MTK `mtkbootanimation`、MTK `audio_vendor_parameter_parser`、以及 Honor NFC / Vivo Camera / Nothing Camera 规则；
   - [files_compat.go](../../../../agent/modules/pipeline/stages/09_build_vendor/files_compat.go) 写死了 10 处特定厂商文件回填、VINTF 注入与属性逻辑：Honor `hwfactoryinterface`、Unisoc `miscdata`、TCL `default_feature_config.json`/`product_dynamic_feature_config.json`、Honor NFC（`com.hihonor.nfc.xml`/`libnfc-*-sz.conf`/`cdh_nn_placeholders.txt`）、Vivo `IVivoCameraProvider` VINTF、ASUS `vendor_set_camera_prop`、MTK `cdh_audio_parameter_parser` file_contexts、YFT 9 项 `/system/build.prop` 改写、HONOR `persist.sys.chr.chipsettype=1` 与 ASUS `vendor.camera.lens_type` 硬编码、`synthesizePlaceholderVintfManifest` 扫描 ELF 字符串特判 `radio@mtk` 与 `health@oplus`；
2. `02_prepare` 建档期在 Go 源码中硬编码了品牌/机型映射表（`officialGroupMap`/`readableModelMap`）、ASUS 刷新率白名单、Vivo/Honor 传感器 payload 规则、品牌默认属性表及 ELF32 守护进程候选清单，且尚未自动为新接入机型生成迁移后的 `vendor/sepolicy/` 条件规则。

## 2. 根因

1. `09_build_vendor/sepolicy.go` 此前仅支持静态无条件片段，缺少跨 Android 版本/底包防 `secilc` 缺符号报错的**声明式条件求值引擎**，也不支持 HAL **变体级** `variants/<variant>/{sepolicy/, vendor/}` 与 `vendor_hwservice_contexts`，导致每次适配新品牌卡点时被迫在 `sepolicy_cil.go` 与 `files_compat.go` 中追加特判 Go 函数；
2. `spec.Module` 与 `vendor/config.json` 缺少通用声明字段（变体占位剔除/接管/追加、兜底复制 `fallback-copies`、`runtime-prop-sets`、`system-build-prop-patch`），迫使 `files_compat.go` 通过扫二进制字符串或硬编码品牌分支实现功能。

## 3. 证据

- 审计基线与改造前硬编码统计见 [log-fail.txt](log-fail.txt)；
- 改造后全仓 31 包单测、131 份 JSON 校验、`09_build_vendor` 非测试 Go 代码硬编码清零审计及 `cdh-pipeline` 编译/干跑结果见 [log-pass.txt](log-pass.txt)。

## 4. 改动

### 4.1 引擎声明式基础设施（纯通用机制，零品牌硬编码）
- [spec.go](../../../../agent/modules/pipeline/internal/halcore/spec/spec.go)：`Module` 新增运行期 `Variant` 字段与 4 个通用声明字段：`PlaceholderDropPrefixes`、`PlaceholderOwnedServices`、`PlaceholderAddServices`、`VendorFallbackCopies`；
- [cache.go](../../../../agent/modules/pipeline/internal/halcore/halbuild/cache.go)、[vendor.go](../../../../agent/modules/pipeline/internal/halcore/halbuild/vendor.go)、[08_build_hardware/plan.go](../../../../agent/modules/pipeline/stages/08_build_hardware/plan.go)、[09_build_vendor/plan.go](../../../../agent/modules/pipeline/stages/09_build_vendor/plan.go)、[hal.go](../../../../agent/modules/pipeline/stages/09_build_vendor/hal.go)、[assemble.go](../../../../agent/modules/pipeline/stages/09_build_vendor/assemble.go)、[execute.go](../../../../agent/modules/pipeline/stages/09_build_vendor/execute.go)：全面支持变体级静态树 `devices/hardware/<mod>/variants/<variant>/vendor/`（同名覆盖基础 `vendor/`）、变体级 SELinux 目录 `variants/<variant>/sepolicy/` 并纳入缓存指纹；
- [sepolicy.go](../../../../agent/modules/pipeline/stages/09_build_vendor/sepolicy.go) 与新增 [sepolicy_rules.go](../../../../agent/modules/pipeline/stages/09_build_vendor/sepolicy_rules.go)：
  - `sepolicyPatches` 补齐 `vendor_hwservice_contexts`（支持按需创建与同名去重），`sepolicySources` 依次加载模块层 → 变体层 → 机型层；
  - 新增通用声明式条件求值器 `applyConditionalCILRules`（处理 `vendor_sepolicy_rules.cil`）与 `applyConditionalContextRules`（处理 `vendor_*_contexts.rules`），支持 `@when-file`、`@when-declared`、`@when-any-declared`、`@when-not-declared`、`@when-svcctx`、`@when-not-contains`、`@when-or`、`@ensure-type`、`@end` 及 `{{vers}}` / `{{pick:sym}}` / `{{pick_list:a,b,c}}`，并内置行级 CIL 符号声明守卫与幂等去重；
- [files_compat.go](../../../../agent/modules/pipeline/stages/09_build_vendor/files_compat.go)：新增纯通用 `applyVendorFallbackCopies`（支持变体与 `vendor.fallback-copies`）、`applyModulePlaceholderAdds`、`applySystemBuildPropPatch`（读 `vendor.system-build-prop-patch`，严守不改官方 `system.img` 约束）、`readVendorRuntimeProps`（读 `vendor.runtime-prop-sets`），并将 `synthesizePlaceholderVintfManifest` 改为 100% 读取变体元数据。

### 4.2 P0 + P1 全部迁出与清理
- 从 [sepolicy_cil.go](../../../../agent/modules/pipeline/stages/09_build_vendor/sepolicy_cil.go) 彻底删除全部 6 个厂商硬编码 CIL 函数；
- 从 [files_compat.go](../../../../agent/modules/pipeline/stages/09_build_vendor/files_compat.go) 与 [assemble.go](../../../../agent/modules/pipeline/stages/09_build_vendor/assemble.go) 彻底删除全部 10 处厂商硬编码逻辑；
- 迁入 5 个 HAL 变体目录：
  - `composer3/variants/oplus/sepolicy/vendor_sepolicy_rules.cil`
  - `health/variants/oplus/sepolicy/vendor_sepolicy_rules.cil` + `android.hardware.health.json` `variants.oplus` 占位声明
  - `camera/variants/vivo/{sepolicy/vendor_sepolicy_rules.cil, vendor/etc/vintf/manifest/android.hardware.camera-service.xml}`
  - `neuralnetworks/variants/honor/{sepolicy/vendor_sepolicy_rules.cil, vendor/etc/permissions/com.hihonor.nfc.xml}` + `android.hardware.neuralnetworks.json` `variants.honor` 占位/兜底声明
  - `neuralnetworks/variants/nothing/sepolicy/vendor_sepolicy_rules.cil`
  - `android.hardware.radio.json` `variants.mtk` 占位剔除前缀声明
- 批量迁入受影响机型配置（Honor 4 台、Unisoc 7 台、TCL 3 台、ASUS 2 台、MTK 15 台、未选 `composer3@oplus` 的 OPlus 6 台、Ulefone YFT 1 台）：共落盘 28 份机型 `vendor_sepolicy_rules.cil`、各机型 `vendor_*_contexts.rules` 及对应 `vendor/config.json` 的 `files` / `fallback-copies` / `runtime-prop-sets` / `system-build-prop-patch`。

### 4.3 P2：`02_prepare` 建档数据化与联动生成
- 新建集中建档目录 [devices/manifest/prepare_catalog.json](../../../../devices/manifest/prepare_catalog.json)（同步内嵌副本 `prepare_catalog_default.json`）与 [catalog.go](../../../../agent/modules/pipeline/stages/02_prepare/catalog.go)；
- 改造 [register_parse.go](../../../../agent/modules/pipeline/stages/02_prepare/register_parse.go)、[apply_profile.go](../../../../agent/modules/pipeline/stages/02_prepare/apply_profile.go)、[apply_profile_configs.go](../../../../agent/modules/pipeline/stages/02_prepare/apply_profile_configs.go)、[vendor_etc_templates.go](../../../../agent/modules/pipeline/stages/02_prepare/vendor_etc_templates.go)、[firmware_measured_extra.go](../../../../agent/modules/pipeline/stages/02_prepare/firmware_measured_extra.go)，移除品牌/机型映射表、刷新率白名单、传感器 payload 规则、品牌默认属性与 ELF32 候选硬编码，并在 `alignVendorExtraFields` 联动调用 `applyCatalogVendorCompat` 为未来新接入机型自动生成对应的 P0/P1 设备配置文件。

### 4.4 测试加固
- 升级 [camera_nfc_compat_test.go](../../../../agent/modules/pipeline/stages/09_build_vendor/camera_nfc_compat_test.go)、[files_test.go](../../../../agent/modules/pipeline/stages/09_build_vendor/files_test.go)、[props_test.go](../../../../agent/modules/pipeline/stages/09_build_vendor/props_test.go)、[sepolicy_test.go](../../../../agent/modules/pipeline/stages/09_build_vendor/sepolicy_test.go) 直接加载仓库真实变体/机型规则文件验证等价行为；
- 新增 [sepolicy_rules_test.go](../../../../agent/modules/pipeline/stages/09_build_vendor/sepolicy_rules_test.go)（含全仓 35+ 份条件规则文件 `@when-*`/`@end` 与括号配平校验）、[catalog_test.go](../../../../agent/modules/pipeline/stages/02_prepare/catalog_test.go)（含工作区与内嵌 catalog 字节同步校验）及 [vendor_test.go](../../../../agent/modules/pipeline/internal/halcore/halbuild/vendor_test.go) 变体静态树覆盖测试。

## 5. 验证

- 静态审计：`09_build_vendor/*.go`（非测试）对 31 项特定厂商/机型特征关键字残留扫描结果为 `0`；
- 全仓 131 份 JSON 配置文件解析校验 `ALL OK`；
- `cd agent && go test -count=1 ./modules/pipeline/...` 全 31 包 100% 通过；
- `go build -o ../out/agent/dist/cdh-pipeline ./cmd/cdh-pipeline` 编译通过，`device list` 及 `PD2454` / `chora` `build_vendor --dry-run` 准确识别变体静态树与变体/机型层 SELinux 片段。

## 6. 遗留

- 本次为纯构建期机制等价迁移与单测/干跑验证，未重新批量全量重建 45 盘镜像；后续任一机型执行 `09_build_vendor` 或全流程构建时将自动按新声明式规则生效并受缓存指纹保护。
