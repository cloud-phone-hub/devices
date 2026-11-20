# `#282`：MTK `audio_vendor_parameter_parser` SP-HAL 跨命名空间 SELinux 授权与 TCL `ConfigProvider` 动态特性配置回填

- 编号：`#282`
- 分组：`common`（引擎级共性修复：MTK Android 15 非 VNDK `audio_vendor_parameter_parser` 跨命名空间 CIL 授权 + TCL 全系 `default_feature_config.json` / `product_dynamic_feature_config.json` 自动回填）
- 触发机型与复验样本：`tcl/ericanppro`（`TCL-60-NXTPAPER.zip`，`15-AP3A.240905.015.A2-20250702`）
- 状态：**已闭环**（2026-11-19，`09_build_vendor` 全量 Go 单测通过；`ericanppro` 重打后两次干净盘冷启动均 `passed=true`，无 `RescueParty` 与 `cdh_audio_parameter_parser` 异常，截图审阅通过）

## 1. 问题

1. `tcl/ericanppro`（Android 15，`202404` 非 VNDK）首次冷启动在 `210s` 超时失败（`bootanim=''`、`serviceCount="241"`）：`init` 周期性拉起 `/system_ext/bin/hw/android.hardware.audio.parameter_parser.service`（已通过 `cdh_redirect` 重定向至 `/mnt/vendorpatch/vendor/bin/hw/cdh_audio_parameter_parser`），但运行时持续报错 `CANNOT LINK EXECUTABLE ... library "libc++.so" not found`，同时内核打印 `avc: denied { read } for comm="cdh_audio_param" name="bin" ... scontext=u:r:audio_vendor_parameter_parser:s0 tcontext=u:object_r:vendor_file:s0 tclass=dir permissive=0`。
2. 解决第 1 个卡点后，次轮冷启动走到 ~95s 时触发 `RescueParty fatal reboot triggered`：日志持续抛出 `com.android.server.am.appboot.AppBootManagerService.byPassUids(AppBootManagerService.java:604)` `NullPointerException`（`Float.parseFloat(null)`）及 `E/Config:Grabber:AppBootPolicy: Index 0 requested, with a size of 0`。

## 2. 根因

1. **非 VNDK `[vendor]` 命名空间与 `system_ext` `coredomain` 的策略缺口**：`cdh_redirect` 的 `readlink` 将 `/proc/self/exe` 重写为 `/vendor/bin/hw/cdh_audio_parameter_parser`，使动态链接器把它归入 `[vendor]` namespace 并尝试搜索 `/vendor/bin`、`AudioParamOptions*.xml`（`vendor_configs_file`）与 `/vendor/lib64/libc++.so`（标签为 `same_process_hal_file`）；但该服务仍由 `system_ext` 声明的 `coredomain` 域 `u:r:audio_vendor_parameter_parser:s0` 运行，且 `system_ext_sepolicy.cil` 原本并未放行跨命名空间对 `same_process_hal_file` / `vndk_sp_file` / `vendor_configs_file` 的读取与执行。此外，`plat_sepolicy.cil` 对 `coredomain` 设有 `(neverallow ... vendor_file (file ...))` 红线，授权 `vendor_file` 时**严禁**放开 `file` 权限，仅可开放 `dir` 与 `lnk_file`。
2. **TCL `ConfigProvider` 依赖的 `/vendor/etc/default_feature_config.json` 未从官方固件回填**：反汇编 `com.tcl.frameworks.apex`（`tcl-services.jar`）与 `/system/priv-app/ConfigProvider/ConfigProvider.apk`（`uid=1000`）证实，`AppBootManagerService.byPassUids()` 依赖 `content://com.tcl.providers.config/AppBootPolicy` 的 `Version` 字段；`ConfigProvider.onCreate()` 仅在首次建库时读取 `/vendor/etc/default_feature_config.json`（及可选 `/vendor/etc/product_dynamic_feature_config.json`）初始化 `config_test.db`。由于 `/vendor/etc` 被 `cdh_redirect` 重定向到 `patch.img` 的 `/mnt/vendorpatch/vendor/etc`，而 `09_build_vendor` 默认未回填这两份 JSON，导致数据库表为空、`getVersion()` 返回 `null` 进而引发 `system_server` 连续崩 4 次触发 `RescueParty`。

## 3. 证据

- 失败现场日志：[log-fail.txt](log-fail.txt)（包含 `fail1` 的 `CANNOT LINK EXECUTABLE ... libc++.so`、`avc: denied ... tcontext=u:object_r:vendor_file:s0`，以及 `fail2` / `ericanppro_rescue_diag.log` 的 `Index 0 requested, with a size of 0` 与 `AppBootManagerService.byPassUids:604` `NullPointerException` → `RescueParty`）。
- 修复后实机通过日志：[log-pass.txt](log-pass.txt)（包含 `09_build_vendor` 输出 `补齐 MTK audio_vendor_parameter_parser SELinux 绑定 11 条`、`[add] vendor/etc/default_feature_config.json <- 官方 vendor`，以及 `verify_boot.py` 冷启动 `passed: true`）。

## 4. 改动

1. [sepolicy_cil.go](../../../../agent/modules/pipeline/stages/09_build_vendor/sepolicy_cil.go)（`L1142-L1148`、`L2011-L2083`）：新增 `reconcileAudioParameterParserBindingsCIL`，在识别到 `audio_vendor_parameter_parser`（含无版本后缀与带版本号形态）时幂等补齐对 `vendor_file`（仅 `dir`/`lnk_file`，严禁 `file`）、`same_process_hal_file` / `vndk_sp_file`（`dir`/`file`/`lnk_file` + `execute`）与 `vendor_configs_file`（`dir`/`file`/`lnk_file`）的最小授权并接通 `reconcileVendorSepolicyCIL`。
2. [files_compat.go](../../../../agent/modules/pipeline/stages/09_build_vendor/files_compat.go)（`L331-L387`）：新增 `backfillOfficialTclFeatureConfigs` 并挂载于 `reconcileVendorSystemCompat`，自动从官方 `vendor` 解包树或镜像提取 `etc/default_feature_config.json` 与 `etc/product_dynamic_feature_config.json` 原样回填至 `vendor/etc/`（同时前置保护后续全部 TCL 机型）。
3. [props_test.go](../../../../agent/modules/pipeline/stages/09_build_vendor/props_test.go)（`L550-L558`、`L598-L607`）：新增 `reconcileAudioParameterParserBindingsCIL`（断言无 `vendor_file` `file` 类违例）与 `backfillOfficialTclFeatureConfigs` 单元测试。
4. [verify_boot.py](../../../../out/devices/tmp/verify_boot.py)（`L543-L551`）：将首次冷启动黑屏唤醒重抓改为最多重试 3 次（间隔 `3.5s`），规避首次冷启动 SystemUI `NotificationShade` 壁纸延迟上屏导致的瞬态黑屏误判。

## 5. 验证

1. `cd /Volumes/work/cloud-phone-hub/agent && TMPDIR=/Volumes/work/cloud-phone-hub/out/devices/tmp GOCACHE=/Volumes/work/cloud-phone-hub/out/agent/gocache /opt/homebrew/bin/go test -count=1 ./modules/pipeline/stages/09_build_vendor` 通过（`ok 1.247s`），并重编 `/Volumes/work/cloud-phone-hub/out/agent/dist/cdh-pipeline`；
2. 增量重打 `tcl/ericanppro`（`15-AP3A.240905.015.A2-20250702`）输出 `补齐 MTK audio_vendor_parameter_parser SELinux 绑定 11 条` 与 `[add] vendor/etc/default_feature_config.json <- 官方 vendor（TCL ConfigProvider 动态特性配置回填）`；
3. 连续两次干净盘冷启动验证均一次通过：一次 `--keep` 复测 `passed: true`、`bootanim="stopped"`、`serviceCount="339"`、`meanBrightness=107.24`；另一次带日志留存复测 `passed: true`、`bootanim="stopped"`、`serviceCount="340"`、`meanBrightness=53.71`（`peak=255`），人工审阅 VNC PNG 截图均正常渲染壁纸与启动画面，无 `RescueParty` 与 `cdh_audio_parameter_parser` 崩溃。

## 6. 遗留

- 无。
