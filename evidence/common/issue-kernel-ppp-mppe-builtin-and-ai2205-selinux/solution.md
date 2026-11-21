# #295 Android 14 + 5.15 内核 `CONFIG_PPP_MPPE=m` 触发 VINTF 内部错误弹窗及 `ASUS_AI2205` 占位宿主/SELinux 收口

- **编号**：#295
- **日期**：2026-11-21
- **分组**：`common/`（内核 `CONFIG_PPP_MPPE=y` 与 `04_build_modules` 自动豁免 `modules.builtin` 改动影响全仓 5.15 及 Android 14+ 机型；首发实证机型 `asus/ASUS_AI2205`，版本 `14-TKQ1.220928.001-20240229`，`lineage-20.0` + `kernel 5.15`）

## 一、问题

`asus/ASUS_AI2205`（ASUS ROG Phone 7，`system=14 / vendor=13`，`lineage-20.0` + `kernel 5.15`）解决 UEFI 卡死（[#294](../issue-modem-hello-csi-uefi-terminal-assert/solution.md)）并正常进入 Android 后，桌面/锁屏底部持续弹出系统告警框：

> **Android System** — *"There's an internal problem with your device. Contact your manufacturer for details."*

同时排查 `qemu.log`（10,543 行）与 `logcat` 发现三类待修问题（`com.android.chrome` 32 位 `libmonochrome.so` 崩溃属不支持 arm32，用户明确要求跳过）：

1. **VINTF 内核兼容性校验失败**：`VintfObject.verifyWithoutAvb()` 返回 `1`，提示 matrix level 7（Android 14 FCM）要求 `CONFIG_PPP_MPPE=y`，而当前 5.15 内核为 `m`，导致 `Build` 报 `Vendor interface is incompatible, error=1` 并触发 `ActivityManagerService` 弹窗。
2. **占位服务缺少宿主**：`vendor/etc/cdh_nn_placeholders.txt` 声明了 12 个占位服务（含 `vendor.asus.hardware.auralight2.IAuralight2/default` 与 `vendor.qti.hardware.servicetrackeraidl.IServicetracker/default`），但 `hardware/config.json` 漏配 `android.hardware.neuralnetworks@1.0`，致使服务从未注册（`auralight2` 缺失日志 18 次）。
3. **SELinux 高频 `avc: denied` 与属性拒绝刷屏**：`qemu.log` 共产生 `3,261` 条 AVC，Top 项为 `system_server -> vendor_hal_servicetrackeraidl_service`（1,210 次）、`domain -> vendor.qti.hardware.perf2.IPerf/default` 落入 `default_android_service`（596 次）、`system_server/audioserver -> vendor_default_prop`（243 次）、`bootanim -> proc_asound`（43 次）；`logcat` 中 `Access denied finding property` 刷屏 `vendor.mesa.*`（5,000+ 次）、`vendor.dirac.*`（1,700+ 次）、`vendor.audio.*`（591 次）。

## 二、根因

1. **5.15 内核片段未显式内建 `CONFIG_PPP_MPPE`**：`lineage-20.0`（Android 13）默认 defconfig 将 `CONFIG_PPP_MPPE` 设为 `=m`；当 GRF 机型（`vendor=13 / system=14`，FCM matrix level 7）跑 Android 14 `system_a.img` 时，框架兼容性矩阵强制要求 `CONFIG_PPP_MPPE=y`。此外在 `drivers/net/ppp/Kconfig` 中 `PPP_MPPE` 执行 `select CRYPTO_LIB_ARC4`，将其提升为 `=y` 会同步把原本作为 `in-tree` 模块导出的 `libarc4.ko` 变为内核内建（出现在 `modules.builtin` 而不再生成 `.ko`），若 `04_build_modules` 不识别 `modules.builtin`，会因找不到 `libarc4.ko` 而在 04 阶段报错。
2. **`ASUS_AI2205` 清单漏配 `neuralnetworks` 占位宿主**：`ASUS_AI2205-android13/hardware/config.json` 未加入 `android.hardware.neuralnetworks@1.0`，导致占位服务注册表无人承载。
3. **`ASUS_AI2205` 缺少厂商服务/属性 SELinux 上下文**：`vendor.qti.hardware.perf2.IPerf/default` 未定义服务上下文而退回 `default_android_service`；`system_server` 缺少查询 `vendor_hal_servicetrackeraidl_service` 的 `find` 权限；`vendor.mesa.*` / `vendor.audio.*` / `vendor.dirac.*` 属性未映射至可公开读取的 `vendor_public_property_type`；`bootanim` 访问 `proc_asound` 目录未静默。

## 三、证据

- 修前基线日志：[log-fail.txt](log-fail.txt)
- 修后全量验证日志：[log-pass.txt](log-pass.txt)
- 修后冷启动审图截图：[lockscreen-pass.png](lockscreen-pass.png)（已正常进入 ASUS ROG 桌面，**"There's an internal problem with your device." 弹窗已 100% 消失**；仅余用户明确跳过不修的 32 位 Chrome 提示）

## 四、改动

### 1. 内核配置与 `04_build_modules` 自动识别内建模块（跨机型公共修复）
- [lineage-kernel.config](../../../../agent/modules/pipeline/stages/03_build_kernel/lineage-kernel.config)：追加 `CONFIG_PPP_MPPE=y`（同步使 `CONFIG_CRYPTO_LIB_ARC4=y` 内建）。
- [build.go](../../../../agent/modules/pipeline/stages/04_build_modules/build.go) 与 [plan.go](../../../../agent/modules/pipeline/stages/04_build_modules/plan.go)：在容器脚本与 Go 侧 `EffectiveExpectedOutputs` 中解析内核产物的 `modules.builtin`，若某个 `in-tree` 模块已转为内建则自动豁免其 `.ko` 导出检查并打印 `[builtin] <ko> 已内建进内核（modules.builtin），跳过 .ko 导出`（外部模块不豁免）；下游 `10_build_vendor_dlkm` 依赖闭包本来即合并 `modules.builtin`，`mac80211.ko -> libarc4` 依赖自然满足。
- [modules_test.go](../../../../agent/modules/pipeline/stages/04_build_modules/modules_test.go)：新增回归单测 `TestEffectiveExpectedOutputsSkipsBuiltins`。
- 重新原子编译 `/Volumes/work/cloud-phone-hub/out/agent/dist/cdh-pipeline`。

### 2. `ASUS_AI2205` 占位服务宿主与 SELinux 规则补齐
- [hardware/config.json](../../../../devices/manifest/configs/asus/ASUS_AI2205/ASUS_AI2205-android13/hardware/config.json)：追加 `android.hardware.neuralnetworks`（`variant: "default", version: "1.0"`）。
- 新建 [vendor_service_contexts](../../../../devices/manifest/configs/asus/ASUS_AI2205/ASUS_AI2205-android13/vendor/sepolicy/vendor_service_contexts)：将 `vendor.qti.hardware.perf2.IPerf/default` 映射至 `vendor_hal_perf2_service`。
- 新建 [vendor_property_contexts](../../../../devices/manifest/configs/asus/ASUS_AI2205/ASUS_AI2205-android13/vendor/sepolicy/vendor_property_contexts)：将 `vendor.mesa.`、`vendor.audio.`、`vendor.dirac.`、`ro.vendor.dirac.`、`persist.vendor.dirac.` 映射至 `vendor_cdh_public_prop`。
- 新建 [vendor_sepolicy.cil](../../../../devices/manifest/configs/asus/ASUS_AI2205/ASUS_AI2205-android13/vendor/sepolicy/vendor_sepolicy.cil)：
  - 声明 `vendor_hal_perf2_service`（`service_manager_type`）并对 `domain` 添加 `dontaudit ... find`；
  - 放行 `system_server_{{vers}} -> vendor_hal_servicetrackeraidl_service` 的 `service_manager find`；
  - 声明 `vendor_cdh_public_prop`（`property_type` / `vendor_public_property_type`）并授权 `domain` 读/映射；
  - 对 `bootanim_{{vers}} -> proc_asound_{{vers}} (dir search)` 添加 `dontaudit`。

## 五、验证

1. **Go 单元回归测试**：
   `go test -v -count=1 ./modules/pipeline/stages/03_build_kernel ./modules/pipeline/stages/04_build_modules ./modules/pipeline/stages/10_build_vendor_dlkm` 全绿 PASS（含 `TestEffectiveExpectedOutputsSkipsBuiltins`）。
2. **5.15 内核与 `ASUS_AI2205` 全链构建（01~13 阶段）**：
   - 产出内核配置确认为 `CONFIG_PPP_MPPE=y`、`CONFIG_CRYPTO_LIB_ARC4=y`；
   - `04_build_modules` 正确识别 `[builtin] libarc4.ko 已内建进内核（modules.builtin），跳过 .ko 导出`，`10_build_vendor_dlkm` 依赖闭包验证通过；
   - `13_pack_vda` 打包并原子替换 `out/devices/dist/asus/ASUS_AI2205/14-TKQ1.220928.001-20240229/`（`vda.img`、`patch.img` `sha256 e19dbe770841`、`data.qcow2`、`device.json`）。
3. **隔离 `host-daemon` 冷启动与审图验收**（详见 [log-pass.txt](log-pass.txt) 与 [lockscreen-pass.png](lockscreen-pass.png)）：

| 检查项 | 修前 | 修后 |
| :--- | :--- | :--- |
| 启动耗时 / SELinux | — / Enforcing | `50s` 达成 `sys.boot_completed=1`，`SELinux=Enforcing` |
| 内核 `PPP_MPPE` / `LIB_ARC4` | `=m` / `=m` | `CONFIG_PPP_MPPE=y` / `CONFIG_CRYPTO_LIB_ARC4=y` |
| `VintfObject` 错误 | `returns 1`（`CONFIG_PPP_MPPE=m`）+ 系统弹窗 | **`(NONE)` 0 错误，系统内部错误弹窗 100% 消失** |
| `init.svc.*restarting` | `(none)` | `(none)` |
| 占位服务在线状态 | `auralight2` 缺失 18 次，`neuralnetworks` 未注册 | `neuralnetworks`、`auralight2`、`servicetrackeraidl` 全部在线 |
| `qemu.log` AVC 总数 | `3,261`（Top 单项 `1,210`） | `1,815`（目标 4 类全部清零，剩余 Top 单项仅 `28`） |
| `vendor.mesa/dirac/audio` 属性拒绝 | `7,300+` 次刷屏 | **0 次（全部清零）** |

## 六、遗留

1. 仓内另 3 台同样使用 Android 14 + 5.15 内核的 GRF 机型（`hmd/ARW`、`symphony/G27`、`symphony/MAX_10`）尚未重打包，后续仅需增量重跑 `05_build_boot → 13_pack_vda` 即可共享本次已编好的 `CONFIG_PPP_MPPE=y` 5.15 新内核产物。
2. `ASUS_AI2205` 修后仍有少量低频属性拒绝（`vendor.camera.aux.packagelist` 13 次、`ro.vendor.cdh.gpu.*` 11 次、`persist.vendor.asus.grip.debug` 8 次）与 `pioneer` 系统探测单次 AVC，不影响功能与性能，暂不放宽策略。
3. `com.android.chrome` 32 位 `libmonochrome.so` 崩溃为纯 64 位环境预期行为，按用户指示不处理。
