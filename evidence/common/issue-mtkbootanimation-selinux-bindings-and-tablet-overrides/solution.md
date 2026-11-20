# #281：`mtkbootanimation` 跨分区 SELinux 授权、`honor/bengal_515` 平板版本独立覆盖与 `dumpsys -l` 验收卡死修复

- 编号：`#281`
- 分组：`common`（引擎级 + 多机型工具协同：MTK `mtkbootanimation` CIL 授权机制、`versions/` 多固件共用机型隔离、`verify_boot.py` ADB 探测机制）
- 触发机型与复验样本：`ulefone/GQ5011AF1` A16（`14-GQ5011AF1_EEA_V13-20260825`）、`honor/bengal_515`（`13-TKQ1.240227.001-20260922`，`Huawei-Honor-Pad-X7b-ONL-W09.zip`）
- 状态：**已闭环**（2026-11-19，`09_build_vendor` 单测全绿 + 两款样本均 `passed=true` 并已审图过关）

## 1. 问题

1. `ulefone/GQ5011AF1` A16 系统进程已启动，但开机动画超时卡死 180~300 秒才触发 `sys.boot_completed=1`，同时 `verify_boot.py` 通过 `service list | wc -l` 统计服务数时被 `media.VTS`（Thread Pool=0）永久阻塞；
2. `honor/bengal_515` `13-TKQ1.240227.001-20260922`（荣耀 Pad X7b Wi-Fi 平板）与同机型 `20260708`（Huawei P60 手机）共用 `bengal_515-android16` 配置：
   - 共用配置写入双卡属性导致无基带 Wi-Fi 平板狂刷数千条 `IRadio/slot1` 报错；
   - `odm`/`vendor` 的背景模糊属性结合平板 90° 横屏旋转及 `LauncherPostOOBEActivity` 过渡，导致首帧偶发黑屏（`meanBrightness=0.0`）；
   - `adb_exec` 在 `adb connect` 后未留握手间隔即执行，回落到超时较慢的 `hvc7_exec` 且 `serviceCount` 为空。

## 2. 根因

1. **`mtkbootanimation` 属 `coredomain` 且缺少跨分区 CIL 授权**：MTK `system_ext` 携带的 `mtkbootanimation_<vers>` 域需要读取 `bootanim_system_prop` 感知 `service.bootanim.exit=1` 并与 `surfaceflinger` / `mediaserver` / `dri_device` / `statsd` 交互；注意 `plat_sepolicy.cil` 存在 `neverallow` 禁止 `coredomain` 读取 `vendor_default_prop`，因此只能显式授权 `bootanim_system_prop_<vers>` 与 `bootanim_config_prop_<vers>`；
2. **多 ROM 版本未按 `buildId` 隔离差异属性**：手机版与 Wi-Fi 平板版共用 `configs/` 主配置，未将 `persist.radio.multisim.config=none`、`ro.radio.noril=yes`、`ro.sf.lcd_density=280` 与 `ro.surface_flinger.supports_background_blur=0` 隔离入 `devices/manifest/versions/honor/bengal_515/<buildId>/{vendor,odm}/config.json`；
3. **运行期检查阻塞**：`service list` 会逐个 `pingBinder` 并被空线程池服务（`media.VTS`）hang 死，改用 `dumpsys -l` 读取 binder 名单可瞬间返回；`adb_exec` 连接后短暂等待 `0.5s` 可避免设备列表瞬态 `offline`。

## 3. 证据

- 失败现场日志：[log-fail.txt](log-fail.txt)（含 `GQ5011AF1` A16 超时失败与 `bengal_515 20260922` `meanBrightness=0.0` / 超时记录）。
- 修复后实机通过日志：[log-pass.txt](log-pass.txt)（含 `09_build_vendor` 日志「补齐 MTK mtkbootanimation SELinux 绑定 13 条」、`GQ5011AF1` A16 `passed=true` / `serviceCount="329"` / `meanBrightness=124.85`、`bengal_515 20260922` `passed=true` / `serviceCount="377"` / `meanBrightness=24.02`）。

## 4. 改动

1. [sepolicy_cil.go](../../../../agent/modules/pipeline/stages/09_build_vendor/sepolicy_cil.go)（`L1924-L2002`）：新增 `reconcileMtkBootanimationBindingsCIL`，按版本后缀检测 `mtkbootanimation_<vers>` 后动态补齐 `bootanim_system_prop` / `bootanim_config_prop`、`dri_device`、`surfaceflinger`、`mediaserver`、`statsdw_socket` / `statsd` 共 13 条安全绑定，并接入 `reconcileVendorSepolicyCIL` 流水线。
2. [props_test.go](../../../../agent/modules/pipeline/stages/09_build_vendor/props_test.go)（`L543-L549`）：补齐 `reconcileMtkBootanimationBindingsCIL` 单元测试。
3. [vendor/config.json](../../../../devices/manifest/versions/honor/bengal_515/13-TKQ1.240227.001-20260922/vendor/config.json) 与 [odm/config.json](../../../../devices/manifest/versions/honor/bengal_515/13-TKQ1.240227.001-20260922/odm/config.json)：利用 `tablemerge.MergeSectionRaw` 为 `20260922` Wi-Fi 平板版单独覆盖无 RIL/单卡、平板 DPI 与关闭背景模糊开关，不影响 `20260708` 手机版。
4. [verify_boot.py](../../../../out/devices/tmp/verify_boot.py)（`L165`、`L558`）：`adb_exec` 在 `adb connect` 后增加 `0.5s` 稳定等待；服务总数统计改用 `dumpsys -l | tail -n +2 | wc -l`。

## 5. 验证

1. `cd agent && go test -count=1 ./modules/pipeline/stages/09_build_vendor` 通过并重编 `out/agent/dist/cdh-pipeline`；
2. 实机冷启动与截图复审：`GQ5011AF1` A16 `passed=true`（`serviceCount="329"`、`meanBrightness=124.85`、`SearchLauncher` 桌面正常）；`bengal_515 20260922` `passed=true`（`serviceCount="377"`、`meanBrightness=24.02`、`UniHomeLauncher` 横屏平板 Dock/状态栏/导航栏渲染清晰）。

## 6. 遗留

- 无。
