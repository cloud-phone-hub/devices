# #167：Android 13 默认映射复用 `android14` HAL 与平台 SELinux 策略版本（`33.0`）跨代对齐

## 1. 问题

在构建并启动 Android 13 机型（首发复现于 `ASUS_AI2205` `13-TKQ1.220928.001-20240229`，共涉及库内 8 台 Android 13 模版：`ASUS_AI2205`、`ARW`、`HNLNA-M`、`bengal_515`、`itel-A6612L`、`PJE110`、`RMX3823`、`RMX5112`）时，存在两类跨机型公共卡点：

1. **Android 13 HAL 编译配置与默认硬件列表缺失**：
   - `cdh-vendor` 的 `internal/profile` 仅定义了 `14 / 15 / 16 / 17` 四个代际，`07_build_vendor` 解析出 `android="13"` 时无法直接复用 `out/vendor/android14` 编译产物；
   - 数据库 `configs` 表中 8 台 Android 13 模版（及 `01_unpack/prepare.go` 新建模版）的 `buildVendor.hardware` 为空数组 `[]`，导致 `07_build_vendor` 步骤 3/4 未编译或注入任何虚拟 HAL 服务（`service_daemon`、`hwcomposer`、`keymint`、`audio`、`wifi` 等全部缺失）。
2. **二阶段 `init` 编译 SELinux 策略触发 `InitFatalReboot`（0.40s ~ 0.50s 崩溃重启）**：
   - **第一层（找不到 `202404.cil`）**：`lineage-21.0` 基底 `vendor/etc/selinux/plat_sepolicy_vers.txt` 为 `"202404"`，而官方 Android 13 固件为 `"33.0"`（`/system/etc/selinux/mapping/` 下仅有 `29.0.cil`~`34.0.cil`，无 `202404.cil`）。原 `alignSepolicyVersion` 使用字符串字典序比较 `"33.0" >= "202404"`（`'3' > '2'` 为 `true`）跳过降级，导致 `/system/bin/secilc` 报错 `Could not open file: /system/etc/selinux/mapping/202404.cil: No such file or directory`；
   - **第二层（`33.0` 与 `202404` 平台公有策略代际断层）**：将版本号与后缀改为 `33.0` / `_33_0` 后，`secilc` 在 `/system/etc/selinux/mapping/33.0.cil:294` 报错 `Failed to resolve expandtypeattribute statement`（`dice_maintenance_service_33_0` 等 31 个 Android 13 公有类型在 Android 14 `202404` 中已被移除），且 `lineage-21.0` 的 `plat_pub_versioned.cil` 含有 Android 14 QPR2 新增的 `keystore2` `get_last_auth_time` 权限位（`Failed to resolve permission get_last_auth_time`）。

## 2. 根因

1. **Android 13 与 Android 14 共用 `lineage-21.0` 底包与 18 项 HAL 集合**：
   - 云机架构中 Android 13 与 Android 14 均以 `lineage-21.0` 为底包，其 vendor VNDK/NDK ABI 与 18 项虚拟 HAL（含 HIDL `android.hardware.graphics.composer@2.4` 与 `android.hardware.media.v412`）完全一致，但 `profile.Normalize`、`resolveAndroid` 及 `device.Config` 未将 `"13"` 映射至 `android14` 及默认 18 项硬件列表。
2. **AOSP 策略版本号命名从 `<API>.0` 切换为 `YYYYMM` 导致字典序倒置与 CIL 后缀差异**：
   - Android 13（`33.0`）与早期 Android 14（`34.0`）采用带小数点的 API 版本号，AOSP 在生成 CIL 符号时将 `.` 替换为 `_`（即 `_33_0` / `_34_0`）；Android 14 QPR2 起切换为 `202404`。直接用字符串比较 `officialVers >= baseVers` 会把 `"33.0"` 误判为大于 `"202404"`，且直接拼接 `"_" + officialVers` 会产生非法的 `_33.0` 标识符。
3. **`plat_pub_versioned.cil` 与系统 `plat_sepolicy.cil` / `mapping/33.0.cil` 强耦合**：
   - 云机运行时的 `/system`、`/system_ext`、`/product` 来自厂商官方固件（`33.0`），其 `mapping/33.0.cil` 仅包含 `(expandtypeattribute (xxx_33_0) true)` 与 `(typeattributeset xxx_33_0 (xxx))`，依赖 `/vendor/etc/selinux/plat_pub_versioned.cil` 声明所有 `33.0` 时代公有类型的 `(typeattribute xxx_33_0)`，且其 `plat_sepolicy.cil` 的 class/permission 表停留在 `33.0`/早期 `34.0`（无 `get_last_auth_time`，也无 `hal_graphics_mapper_service`）。
   - 直接沿用 `lineage-21.0`（`202404`）的 `plat_pub_versioned.cil` 既缺少 `33.0` 已废弃的 31 个公有类型声明，又引入了系统 `plat_sepolicy.cil` 不认识的 `get_last_auth_time` 权限；必须以厂商官方 `vendor_a.img:/etc/selinux/plat_pub_versioned.cil` 为主体，再将基底 `plat_pub_versioned.cil` 与 `vendor_sepolicy.cil` 引用但官方尚未声明的 `*_33_0` 符号以 `(typeattribute <sym>)` 补齐。

## 3. 证据

- **全仓解包固件 `plat_sepolicy_vers.txt` 实测对账**：
  - 8 台 Android 13 机型（`ASUS_AI2205`、`ARW`、`HNLNA-M`、`bengal_515`、`itel-A6612L`、`PJE110`、`RMX3823`、`RMX5112`）官方 `vendor` 的 `plat_sepolicy_vers.txt` 均为 `33.0`，`nothing/Asteroids` 为 `34.0`。
- **首轮与二轮启动失败日志**（见 [log-fail.txt](log-fail.txt)）：
  - 首轮（`0.408s`）：`secilc: SELinux: Could not open file: /system/etc/selinux/mapping/202404.cil: No such file or directory` -> `InitFatalReboot: signal 6`；
  - 二轮（`0.507s`，仅改版本号与后缀）：`init: /system/bin/secilc: Failed to resolve expandtypeattribute statement at /system/etc/selinux/mapping/33.0.cil:294`；
  - 容器 `cdh-secilc:latest` 离线复现：补齐 31 个缺失声明后若保留基底 `plat_pub_versioned.cil`，报 `Failed to resolve permission get_last_auth_time at /tmp/merged_plat_pub.cil:11590`；改用官方 `plat_pub_versioned.cil` + 补齐跨代 `typeattribute` 266 项后，`secilc -m -M true -G -N -c 30` 返回 `rc=0`。

## 4. 改动

1. **Android 13 映射复用 `android14` HAL 编译配置**：
   - `engine/vendor/build/internal/profile/profile.go`：`Normalize` 将 `"13"` / `"13.0"` / `"android13"` 归一为 `14`，使 `ForAndroid("13")` 复用 `android14` 配置；同步补充 `profile_test.go` 单测。
   - `engine/build/07_build_vendor/plan.go`：`resolveAndroid` 在解析出 `"13"` 时映射为 `"14"`（日志打印 `==> android: 14（... 13 -> 默认映射 android14）`），使 `HalRoot` 指向 `out/vendor/android14`。
2. **默认硬件 HAL 列表补齐（Android 13/14 默认 18 项，Android 15+ 默认 17 项）**：
   - `engine/build/internal/device/config.go`：新增 `DefaultHardwareForAndroid(androidVer)` 与 `ensureVendorDefaultHardware`，在 `UpsertConfig` 与 `ExtractConfigAndSlimVersion` 中对空 `hardware` 自动填入对应代际的默认 HAL 列表；
   - `engine/build/01_unpack/prepare.go` 与 `engine/build/web/static/app.js`：新建模版/版本时按 Android 版本默认填入 18 项（13/14）或 17 项（15+）硬件列表；
   - `engine/build/assets/devices.db`：将 `configs` 表中 8 个 Android 13 模版及空硬件模版补齐默认硬件列表。
3. **`07_build_vendor/sepolicy.go` 平台 SELinux 策略版本跨代对齐**：
   - 新增 `sepolicyCILSuffix`（将 `"33.0"` 转为 `"33_0"`）、`parseSepolicyMajor` 与 `shouldAlignSepolicyVersion(officialVers, baseVers)`（修复 `"33.0"` vs `"202404"` 字典序倒置，同时保留 Android 16 GRF 冻结在 `202404` 但系统已含 `202504.cil` 的场景不降级）；
   - 新增 `mergeOfficialPlatPubVersioned`：当 `parseSepolicyMajor(officialVers) < 202404` 时，从 `OfficialVendorImg` 提取官方 `etc/selinux/plat_pub_versioned.cil` 作为主体，自动扫描并追加基底 `plat_pub_versioned.cil` 与 `vendor_sepolicy.cil` 中引用但官方尚未声明的 `(typeattribute <sym>)`，并移除 `vendor_service_contexts` 中官方尚未定义的 `hal_graphics_mapper_service`；
   - `engine/build/07_build_vendor/sepolicy_test.go`：新增 `TestShouldAlignSepolicyVersion`、`TestApplySepolicyPatchesAndroid13CILSuffix`、`TestMergeOfficialPlatPubVersioned`。

## 5. 验证

详见 [log-pass.txt](log-pass.txt)：
1. **单元测试**：`cd engine/vendor/build && go test ./...` 与 `cd engine/build && go test -count=1 ./internal/device ./07_build_vendor` 全部 PASS。
2. **构建验证**：`ASUS_AI2205` 执行 `07_build_vendor` 输出 `==> android: 14（命令行 13 -> 默认映射 android14）`、装配 18 项 HAL 模块，并打印 `==> [步骤 6] 对齐平台 SELinux 策略版本: 202404 -> 33.0（匹配厂商固件 mapping/33.0.cil）` 与 `替换官方 plat_pub_versioned.cil 并补齐跨代兼容 typeattribute 266 项`，产出 `vendor.img`（`134545408` B）与 `patch.img`（sha256 `9f4c1c19dd1c`）。
3. **实例启动验证**：启动 `ASUS_AI2205` 实例 2（`pid=84955`），`qemu.log` 中 `/system/bin/secilc` 零报错完成策略编译并加载 SELinux 策略，成功进入二阶段 `init`、`post-fs` 及 `service_daemon` 启动阶段，不再触发 `InitFatalReboot`。

## 6. 遗留

- `ASUS_AI2205` 进入二阶段 `init` 后暴露该机型自身的后续卡点（如内核 `lockdown` LSM 拦截 `dlkm_loader` 加载未签名 `.ko`、VINTF schema `7.0` 对齐等），在 `ASUS_AI2205` 后续条目中单独闭环固证。

