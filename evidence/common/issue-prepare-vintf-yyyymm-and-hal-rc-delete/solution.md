# #263 `02_prepare` VINTF `YYYYMM` `target-level` 误映与 `delete` 规则误删自编 HAL `.rc` 修复（`blackview/XPLORE_X1` 闭环）

- **编号**：#263
- **设备 / 机型**：跨机型（`02_prepare` 通用构建准备，首发实证于 `blackview/XPLORE_X1` `15-AP3A.240905.015.A2-20251126`）
- **状态**：已闭环（2026-11-19）

## 1. 问题

`blackview/XPLORE_X1`（Android 15 / `lineage-22.2`）首次全链路构建后独占冷启动 `verify_boot.py` 300s 超时，`sys.boot_completed` 无法置 1；日志停在 `late-fs`：

- `servicemanager: getDeviceHalManifest: -22 VINTF parse error: Illformed file: /vendor/etc/vintf/manifest.xml: Unrecognized manifest.version 9.0 (libvintf@8.0)`
- `servicemanager: NULL VINTF MANIFEST!: device`
- `vold`（pid=216）持续每隔 1 秒等待 `android.system.keystore2.IKeystoreService/default` 直至 300 秒超时。
- 同批次在 `vendor-log.txt` 中观察到 `[delete] vendor/etc/init/android.hardware.media.c2-mediatek.rc（已删除）`，把自编 `android.hardware.media` 模块的服务 `.rc` 一并抹除。

## 2. 根因

1. **VINTF `YYYYMM` `target-level` 解析错位**：
   - `firmware_measured_extra.go` 的 `extractMeasuredVintfAndPlaceholders` 读取官方 `manifest.xml` 的 `target-level="202404"`（Android 14 QPR2/15 起采用的年月格式，对应 `libvintf@8.0`），此前直接拼接 `.0` 生成 `"vintf-version": "202404.0"` 并写入 `vendor/config.json`；
   - `09_build_vendor/vintf.go` 检查 `vintfVersionAtMost("9.0", "202404.0")` 认为 `lineage-22.2` 底包 `manifest.xml` 等 7 份 VINTF XML 的 `version="9.0"` 未超上限而不做降级；运行态 Blackview `servicemanager` 链接 `libvintf@8.0`，拒绝解析 `version="9.0"` 并返回 `NULL VINTF MANIFEST!: device`，引发 `keystore2`/`vold` 初始化卡死。
2. **冲突 `.rc` `delete` 清单误杀自编 HAL**：
   - `extractMeasuredVendorFilesAndRedirects` 无条件扫描官方 `vendor/etc/init/android.hardware.media.c2-mediatek.rc` 与 `android.hardware.neuralnetworks-service.rc` 并生成 `{"mode": "delete", ...}`；由于 `09_build_vendor` 使用 LineageOS vendor 为基底（官方 `.rc` 本就不会入树），而步骤 5 `delete` 晚于步骤 4 自编 HAL `.rc` 落盘，结果反过来删掉了引擎自编 HAL 的 `.rc`。
3. **`vendor` 纯 64 位 ABI 锁未随通用候选下发**：
   - `apply_profile.go` 仅在 `odm/config.json` 补全 `ro.odm.product.cpu.abilist*`，`deriveVendorPropCandidates` 未同步输出 `ro.vendor.product.cpu.abilist*`。

## 3. 证据

- 失败日志与超时输出：见同目录 [`log-fail.txt`](log-fail.txt)。
- 通过构建校验与验收日志/截图：见同目录 [`log-pass.txt`](log-pass.txt) 与 [`pass-screen.png`](pass-screen.png)。

## 4. 改动

1. [`agent/modules/pipeline/stages/02_prepare/firmware_measured_extra.go`](../../../agent/modules/pipeline/stages/02_prepare/firmware_measured_extra.go)：
   - 新增 `mapVintfVersionToken` 与 `normalizeVintfSchemaVersion(rawVer, targetLevel, androidMajor)`：将 `>= 202500` 的 `YYYYMM` 规范为 `"9.0"`、`>= 202400` 规范为 `"8.0"`，`4..15` 规范为 `<N>.0`；替换 `extractMeasuredVintfAndPlaceholders` 原拼接逻辑；
   - 删除 `android.hardware.neuralnetworks-service.rc` 与 `android.hardware.media.c2-mediatek.rc` 的自动 `mode: "delete"` 生成代码块。
2. [`agent/modules/pipeline/stages/02_prepare/apply_profile_configs.go`](../../../agent/modules/pipeline/stages/02_prepare/apply_profile_configs.go)：
   - `alignVendorExtraFields` 接收 `androidVer`，通过 `normalizeVintfSchemaVersion` 自动识别并自愈存量 `"202404.0"` / `"202504.0"` 脏值；
   - 新增 `isBuggyHalRcDelete`，在 Android 14+ 机型上同时清洗 `VendorOfficialFiles` 与存量 `vendor/config.json` `files` 中的上述两条误删 `delete` 项（保留 Android 13 原行为）。
3. [`agent/modules/pipeline/stages/02_prepare/apply_profile.go`](../../../agent/modules/pipeline/stages/02_prepare/apply_profile.go)：
   - `deriveVendorPropCandidates` 默认输出纯 64 位 `ro.vendor.product.cpu.abilist=arm64-v8a`、`abilist32=""`、`abilist64=arm64-v8a`，并在属性合并循环为 `abilist32` 放行显式空串落盘。
4. [`agent/modules/pipeline/stages/02_prepare/apply_profile_test.go`](../../../agent/modules/pipeline/stages/02_prepare/apply_profile_test.go)：
   - 新增 `TestNormalizeVintfSchemaAndPruneHalRcDelete` 验证映射、自愈清洗与 64 位 ABI 锁输出，并重编 `out/agent/dist/cdh-pipeline`。
5. `blackview/XPLORE_X1` 执行 `device apply-profile ... --overwrite` 与增量重打 `09_build_vendor` → `11_build_patch` → `13_pack_vda`。

## 5. 验证

1. **单测验证**：`cd agent && TMPDIR=/Volumes/work/cloud-phone-hub/out/agent/tmp go test -count=1 ./modules/pipeline/stages/02_prepare/...` 全量通过（`1.239s`），`TestNormalizeVintfSchemaAndPruneHalRcDelete` 通过（`0.260s`）。
2. **构建树复核**：`blackview/XPLORE_X1` 重打后 `vendor/etc/vintf/` 仅含 `version="8.0"`（无 `9.x` 残留），且 `vendor/etc/init/android.hardware.media.c2-mediatek.rc`（1058 B）正常落盘。
3. **冷启动验收**：独占 CPU 运行 `verify_boot.py` 一次通过：`passed: true`、`bootCompleted: true`、`postBootCompleted: "1"`、`surfaceFlingerState: "running"`、`serviceCount: "351"`、`topActivity: com.blackview.launcher/com.android.searchlauncher.SearchLauncher`，VNC 截图 `pass-screen.png`（1088×2460，`meanBrightness: 71.52`）完整呈现 Blackview 主桌面与时间/Google 搜索栏/应用图标。

## 6. 遗留

- 无阻断性遗留；既有已入库配置中如其他新采集 Android 15 机型曾残留 `"vintf-version": "202404.0"`，在 `apply-profile` 时将被自动自愈为 `"8.0"`。
