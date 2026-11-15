# issue-engine-build-13-stages-and-hardware-move：engine/build 升级 13 阶段 + HAL 源码树上移

## 问题

`engine/build/stages` 是 11 阶段（01_unpack 直接接 02_build_kernel，HAL 编译不在流水线里），
而 `agent/modules/enginebuild/stages` 已有一份 13 阶段的改造草案。两套并行导致：

1. 流水线缺「构建准备」与「HAL 编译」两步——工具链缺失要到 07/12 才暴露，HAL 产物只能
   靠手工 `cdh-vendor hal build` 预先生成，脱离引擎的配方/指纹/产物清单机制；
2. `engine/vendor/hardware`（522 MB、46 个模块的 HAL 源码树）埋在 `vendor/` 下，
   与 `vendor/build`（旧构建器）混放，语义上它属于引擎级输入而非 vendor 中间件；
3. `agent/internal/manifestview` 与 `engine/build/web` 的阶段视图各自硬编码 11 步编号。

## 根因

历史上构建系统是从 Python `build/NN_*/main.py` + 独立 `cdh-vendor` 工具迁到单二进制的：
迁移时只把 11 个镜像阶段收进注册表，HAL 编译仍留在 `vendor/build` 单独跑；13 阶段草案
（agent/modules/enginebuild）创建后没有回灌到现役 `engine/build`，且副本逐渐落后
（例如 09_build_vendor 的 `dropDuplicateServiceContexts` / `mergeOfficialVendorServiceContexts`
只在 engine/build 侧，副本没有）。

## 证据

- `diff -rq engine/build/stages/02_build_kernel agent/modules/enginebuild/stages/03_build_kernel`
  剔除 import 路径后 **0 差异** → 副本是 engine/build 的忠实拷贝，只改了编号与包路径；
- `engine/build/stages/07_build_vendor/props.go` 比副本多 11 行、`sepolicy_cil.go` 多 52 行
  → **engine/build 更新**，因此以 engine/build 代码为准，只取副本的结构改造；
- 13 阶段副本可编译（临时加 go.mod 后 `go build ./...` 通过），02_prepare 与
  08_build_hardware 是完整实现而非占位；
- `08_build_hardware/core` 与 `engine/vendor/build/internal` 差异只有 import 路径；
- 副本 `profile.BuildDir()` 指向不存在的 `agent-plugin/modules/pipeline/stages/08_build_hardware`
  → 搬迁时一并修正为 `engine/build/stages/08_build_hardware`。

## 改动

### 1. `engine/build/stages` 重命名（11 → 13）

| 旧 | 新 |
| --- | --- |
| 01_unpack | 01_unpack（不变） |
| 02_build_kernel | 03_build_kernel |
| 03_build_modules | 04_build_modules |
| 04_build_boot | 05_build_boot |
| 05_build_vendor_boot | 06_build_vendor_boot |
| 06_build_odm | 07_build_odm |
| 07_build_vendor | 09_build_vendor |
| 08_build_vendor_dlkm | 10_build_vendor_dlkm |
| 09_build_patch | 11_build_patch |
| 10_pack_super | 12_pack_super |
| 11_pack_vda | 13_pack_vda |
| — | **02_prepare**（新增，来自 13 阶段草案） |
| — | **08_build_hardware**（新增，来源同上） |

编号引用用单遍正则替换（不会出现 02→03 再被 03→04 链式改错），共 65 个文件。

### 2. 新阶段接线

- `internal/pipeline/pipeline.go`：13 条注册表（含 `prepare` / `build_hardware`，DBColumn 分别为
  `prepare` / `buildHardware`）；`pipeline_test.go` 同步。
- `main.go`：新增 `cdh prepare`、`cdh build-hardware` 子命令与 usage 文本。
- `cli_stages.go`：`runPrepare` / `runBuildHardware`。
- `cli_stage_run.go`：worker 能力表补 `prepare`（02_prepare）与 `build_hardware`（08_build_hardware）。

### 3. HAL 源码树上移

- `engine/vendor/hardware/` → **`devices/hardware/`**（522 MB / 46 模块，同盘 mv）。
- `08_build_hardware/core/profile/profile.go`：`findRoot` 与 `HardwareDir()` 改判
  `devices/hardware`；`BuildDir()` 修正到 `engine/build/stages/08_build_hardware`（模块 JSON 所在）。
- `09_build_vendor/plan.go`：`HardwareDir` 改 `filepath.Join(engine.Root(), "hardware")`。
- `agent/internal/manifestview/inspector.go`：`scanHardwareInternal` 的 modules 目录改
  `engine/build/stages/08_build_hardware/modules`、HAL 根改 `devices/hardware`。
- 文档与清单注释（README / device.json 内嵌说明）同步 14 个文件。

### 4. 阶段视图对齐 13 步

- `agent/internal/manifestview/inspector.go`：已有 renumber 映射与 02/08 条目，本次仅随目录
  编号同步（无结构改动）。
- `engine/build/web/pipeline.go`：新增 `buildStagePrepare` / `buildStageHardware`，插进
  13 步序列并统一重编号，`TotalStages` 11 → 13；`web_test.go` 断言同步。

## 验证

- `cd engine/build && go build ./... && go vet ./...`：通过；`go test ./...` **28 包全绿**；
- `cd agent && go build ./... && go test ./...`：**50 包全绿**；
- `cdh help`：13 步清单与两个新子命令齐全；
- `cdh device build gold --manifest 15-OS3.0.11.0.VNQCNXM --dry-run`：输出
  `==> 阶段(13): unpack, prepare, build_kernel, …, pack_vda`，顺序正确；
- `cdh prepare --manifest … --dry-run`：正确推导 lineage 22.2、厂商/lineage 解包根并列出待建目录；
- `cdh build-hardware --android 15 --dry-run`：`源码树: …/devices/hardware`、45 个模块，AIDL/编译
  命令路径全部落在 `devices/hardware/**`。

## 后续：`engine/vendor` 目录整体下线（同日收口）

上一步只迁了 HAL 源码树，`engine/vendor` 还留着旧构建器与 HAL 依赖缓存，本次一并清掉：

| 内容 | 体积 | 处置 |
| --- | --- | --- |
| `engine/vendor/build`（旧独立 HAL 构建器，module `build`） | 588 KB | **删除**——逐文件比对确认 08 已完整吸收；删除前先同步了它比 08 新的 `modules/android.hardware.gpu.json`（多 24 项 `=GL_EXT_*` 剥离项，来自 #232） |
| `engine/vendor/out`（AIDL/platform 依赖缓存 + 编译工作区） | 6.1 GB | **迁移**到 `engine/out/hal/`，`profile.OutRoot()` 同步改 `engine/out/hal/android<ver>` |
| `engine/vendor/.gitignore`（仅 `out/`） | — | 随目录删除（`engine/.gitignore` 已忽略 `out/`） |
| `engine/out/bin/cdh-vendor`（旧二进制副本） | — | 删除，手工入口改用 `cdh build-hardware` / `cdh vendor-deps` |

补齐一个真实缺口：08 只校验依赖、不下载，原本依赖下载只能手工编 `cmd/vendor` 再跑
`cdh-vendor deps`，而错误提示里写的 `cdn vendor-deps` 是一条不存在的命令。现已在 cdh 增加
**`cdh vendor-deps [--android N] [--check|--force|--abi|--platform|--v412...] [hal...]`**，
直接调 `deps.Run`。

验证：`go build/vet/test` 28 包全绿；`cdh vendor-deps --android 15 --check` →
`[ok] platform-source 齐全（15 仓）`、`[ok] ABI 依赖齐全（4 库）`、各 HAL `AIDL 依赖齐全`，
证明迁移后的 `out/hal` 缓存被正确识别；`cdh build-hardware --android 15 --dry-run` 的
中间件路径已变为 `engine/out/hal/android15/tmp/...`，产物仍在 `engine/out/vendor/android15`。

`engine/` 顶层现为：`build / evidence / hardware / kernels / manifest / modules / out / roms`。

## 遗留

- `agent/modules/enginebuild` 副本保留（本次以它为结构参考）；确认 13 步稳定后可删除，
  避免两份继续漂移；
- 已构建过的旧 `out/` 产物目录不做迁移：阶段日志文件名不变，但 `runs` 记录里的阶段号
  （07/09 等）与旧编号语义不同，历史记录只读回溯时需按新表换算；
- `engine/out/hal/build-all`、`build-check`、`build-check16`（约 97 MB）是历史脚本产物，
  无代码引用，确认后可直接删。
