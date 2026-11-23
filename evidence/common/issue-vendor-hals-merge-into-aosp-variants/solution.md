# #322：将 7 个独立 `devices/hardware/vendor.*` HAL 全面归并至对应 AOSP HAL Variant

## 1. 问题

`devices/hardware/` 下历史上存在 7 个独立的厂商私有 HAL 模块（各自维护独立守护进程二进制、独立 `.rc`、独立 SELinux 进程域与 `init` transition 规则以及独立 `08_build_hardware/modules/vendor.*.json`）：

1. `vendor.meizu.hardware.meizu_charging`（`IMeizuCharging/default`）
2. `vendor.meizu.hardware.aw_light`（`IAwLight/default`）
3. `vendor.awa.reality`（`IRealityTap/default`）
4. `vendor.meizu.sensor_aux`（`ISensorAux/default`）
5. `vendor.qti.hardware.perf2`（`IPerf/default`）
6. `vendor.qti.hardware.servicetrackeraidl`（`IServicetracker/default`）
7. `vendor.qti.diaghal`（`Idiag/default`）

这 7 个服务均为 89~190 行的纯内存 AIDL 桩实现（零硬件节点、零 sysfs 依赖），仅被 `Meizu22-android15`、`pa3q-android15`、`popsicle-android16` 三个机型引用。独立拆成 7 个常驻进程与 7 个顶层模块带来额外维护与进程开销，也与 `#320`/`#321` 确立的「标准 AOSP HAL + 命名 Variant 扩展」架构不统一。

## 2. 根因

1. **架构历史演进**：早期为快速 bring-up `Meizu22` 与高通机型，直接按独立 `vendor.*` 模块逐个建模，尚未引入 `variants/<variant>/{vendor,sepolicy}` 与条件式 SELinux 规则机制；
2. **`halbuild/codegen.go` 限制**：`ExtraAidlRoots` 原本不支持读取 `.hash` 并传入 `aidl --hash=...`，导致携带冻结 `.hash` 的厂商 AIDL 包无法直接挂进 AOSP HAL 的 `extraAidlRoots`；
3. **`halcore/spec` 宏合并限制**：`LoadVariantFromDir` 叠加命名变体时对 `compile.defines` 做整体替换，若 `android.hardware.power@qti` 定义 `-DCDH_POWER_QTI=1`，在 Android 13/16/17 上会覆盖版本覆盖文件声明的 `-DCDH_POWER_AIDL_VERSION={3,6}`；
4. **`halbuild/vendor.go` 清单互踩**：多变体共用 `WorkDir()/vendor-tree.txt` 时，构建 `default` 变体会把同模块其他变体已发布的专属 VINTF 文件当成陈旧件删掉。

## 3. 证据

- 归并前状态与问题记录见同目录 `log-fail.txt`（7 个独立 `vendor.*` 目录、7 个模块 JSON、3 份机型配置引用及 `TestLoadVariant` 初测冲突记录）；
- 归并后全量构建、跨变体符号隔离校验与 Go 全量单测记录见同目录 `log-pass.txt`。

## 4. 改动

### 4.1 归并映射（7 个 `vendor.*` → 6 个 AOSP HAL 变体）

| 原独立模块 | 归并目标 AOSP HAL Variant | 编译宏 | 同进程宿主域 |
| :--- | :--- | :--- | :--- |
| `vendor.meizu.hardware.meizu_charging` | `android.hardware.health@meizu` | `-DCDH_HEALTH_MEIZU=1` | `hal_health_default` |
| `vendor.meizu.hardware.aw_light` | `android.hardware.light@meizu` | `-DCDH_LIGHT_MEIZU=1` | `hal_light_default` |
| `vendor.awa.reality` | `android.hardware.vibrator@meizu` | `-DCDH_VIBRATOR_MEIZU=1` | `hal_vibrator_default` |
| `vendor.meizu.sensor_aux` | `android.hardware.sensors@meizu` | `-DCDH_SENSORS_MEIZU=1` | `hal_sensors_default` |
| `vendor.qti.hardware.perf2` + `vendor.qti.hardware.servicetrackeraidl` | `android.hardware.power@qti` | `-DCDH_POWER_QTI=1` | `hal_power_default` |
| `vendor.qti.diaghal` | `android.hardware.neuralnetworks@qti` | `-DCDH_NN_QTI=1` | `hal_neuralnetworks_default` |

### 4.2 构建引擎加固

- `agent/modules/pipeline/internal/halcore/halbuild/codegen.go`：`ExtraAidlRoots` 支持 `hasHash: true` 读取 `<rootDir>/.hash` 并向 `aidl` 传入 `--hash=<hash>`，100% 保持原厂 wire 哈希一致；
- `agent/modules/pipeline/internal/halcore/spec/spec.go`：命名变体叠加时，`compile.defines` 保留基础及 `android<ver>` 版本宏并追加去重变体宏（确保 `power@qti` 在 Android 16 同时拥有 `-DCDH_POWER_AIDL_VERSION=6` 与 `-DCDH_POWER_QTI=1`）；
- `agent/modules/pipeline/internal/halcore/halbuild/vendor.go`：非基础变体使用独立的 `vendor-tree-<variant>.txt` 清单文件，避免构建 `default` 变体时误删同模块命名变体写入的专属静态树文件；
- `agent/modules/pipeline/internal/halcore/spec/spec_test.go`：更新既有变体断言并新增 `TestRealMergedVendorHalVariants`，覆盖 6 个新变体、Android 16 `power@qti` 宏保留、`HasHash` 及旧 `vendor.*` 清零校验。

### 4.3 源码、AIDL、VINTF 与声明式 SELinux

- 将 7 组厂商 `.aidl`（含 6 份 `.hash`）迁入对应宿主模块的 `aidl/{meizu,qti}/`；
- 新增 6 组变体 C++ 扩展（`src/meizu/{health,light,vibrator,sensors}_meizu_ext.{h,cpp}` 与 `src/qti/{power,nn}_qti_ext.{h,cpp}`），通过条件宏接入各宿主 `main()`，对 `default` 变体零侵入；
- 为 6 个变体创建 `variants/<variant>/sepolicy/{vendor_service_contexts,vendor_sepolicy_rules.cil}`（复用宿主域并采用 `@ensure-type` 避免与官方策略重复声明，删去 7 组多余的独立域/exec/`init` transition）及 `variants/<variant>/vendor/etc/vintf/manifest/*.xml`（`sensors@meizu` 按原口径不声明 VINTF）；
- 在 6 份 `agent/modules/pipeline/stages/08_build_hardware/modules/android.hardware.*.json` 中追加 `meizu` / `qti` 变体定义与 `placeholderDropPrefixes`。

### 4.4 机型配置迁移与旧模块清除

- 更新 3 个机型 `hardware/config.json`：
  - `devices/manifest/configs/meizu/Meizu22/Meizu22-android15/hardware/config.json`：`health`/`light`/`vibrator`/`sensors` → `meizu`，`power`/`neuralnetworks` → `qti`，删除 7 项 `vendor.*`；
  - `devices/manifest/configs/samsung/pa3q/pa3q-android15/hardware/config.json`：`power`/`neuralnetworks` → `qti`，删除 3 项 `vendor.qti.*`；
  - `devices/manifest/configs/xiaomi/popsicle/popsicle-android16/hardware/config.json`：`power` → `qti`，删除 `vendor.qti.hardware.perf2`，清理 `popsicle` 重复的 `vendor/etc/vintf/manifest/vendor.qti.hardware.perf2.xml` 与 `vendor/sepolicy/vendor_sepolicy.cil` 中过时的 `hal_sensors_default` perf2 双向 binder 规则；
- 更新 `agent/modules/pipeline/stages/02_prepare/firmware_measured_extra.go` 注释；
- 删除全部 7 个旧 `devices/hardware/vendor.*` 目录及 7 份 `agent/modules/pipeline/stages/08_build_hardware/modules/vendor.*.json`。

## 5. 验证

1. **交叉编译验证**：使用新版 `out/agent/dist/cdh-pipeline build-hardware -refresh` 完整编译并链接：
   - 7 个命名变体目标：`android15` 下的 `health@meizu`、`light@meizu`、`vibrator@meizu`、`sensors@meizu`、`power@qti`、`neuralnetworks@qti`，以及 `android16` 下的 `power@qti`（v6 + `perf2` + `servicetrackeraidl`），全部 0 错通过，6 份带 `.hash` 的厂商 AIDL 均按原哈希生成；
   - 6 个对应的 `android15` `default` 变体目标，全部 0 错通过；
2. **跨变体符号隔离校验**：使用 `strings` 抽检 12 个 `android15` 产物二进制——6 个命名变体全部精准包含对应厂商接口描述符（7/7 命中），6 个 `default` 变体全部 0 泄露（7/7 计数为 `0`）；
3. **Go 单元与集成测试**：`cd agent && go test -count=1 ./modules/pipeline/...` 全 31 个包（含新增 `TestRealMergedVendorHalVariants`）100% 通过；重建 `out/agent/dist/cdh-pipeline` 成功；
4. **残留清零检查**：`devices/hardware/vendor.*` 目录数为 0，旧 7 份 `modules/vendor.*.json` 已全部删除，全仓 40+ 机型 `hardware/config.json` 无任何旧 `vendor.*` 依赖残留。

## 6. 遗留

- 当前已完成编译链接、跨变体符号隔离、配置迁移与全量流水线单测验证；后续重打包并冷启动 `Meizu22-android15`、`pa3q-android15`、`popsicle-android16` 时，可顺带复核 `service list` 中 7 个厂商服务均由对应宿主域在线提供且零 AVC。
