# Issue: 设备侧输入目录（hardware/kernels/manifest/modules）迁入 devices

- 设备: 通用（构建流水线与 agent 全链路）
- 状态: **已闭环**（2026-11-15）

---

## 一、问题

`engine/` 下同时放着两类东西：

| 类别 | 目录 | 体积 |
| --- | --- | --- |
| 设备侧输入 | `engine/hardware`、`engine/kernels`、`engine/manifest`、`engine/modules` | 522 MB + 17 GB + 1.3 MB + 16 MB |
| 运行期产物 | `engine/out`、`roms` | 持续增长 |

设备侧输入按机型组织，和 `devices/evidence` 同属「设备资产」；与 engine 的
构建输出混在一起，既让 engine 根语义模糊，也让新增机型时必须同时理解两套根。
目标：把四类输入迁到 `devices/`，engine 只保留 `out/`、`roms/`，并统一路径访问器。

## 二、根因

历史布局是「engine 既是输入仓也是输出仓」：manifest、内核、HAL、外部模块都随
构建系统一起放在 `engine/`。随着 `devices/` 引入（证据、设备数据）与 agent 成为
统一控制面，engine 的职责应收窄为「运行期产物」，但代码仍以 `engine.Root()` 为
唯一基准拼接 `manifest` / `hardware` / `kernels` / `modules`，根目录标记也写死
`engine/manifest`、`engine/hardware`，因此必须同时改目录、访问器与根探测。

## 三、证据

- 迁移前 `engine/` 顶层为 `hardware / kernels / manifest / modules / out / roms`；
  四类输入合计约 17.5 GB。
- 代码硬编码（节选）：
  `03_build_kernel/plan.go:332,407`、`04_build_modules/plan.go:321,393`、
  `06_build_vendor_boot/modules.go:226`、`09_build_vendor/plan.go:282`、
  `10_build_vendor_dlkm/modules.go:88`。
- 访问器与根标记：`engine.ManifestRoot/DevicesRoot`、`profile.findRoot`、
  cdha `detectEngineRoot`、daemon `config.ProjectRoot`、`tools`/`manifestview`
  的 `engineRoot + "manifest"`。
- 迁移前后命令与输出见同目录 [log-fail.txt](log-fail.txt)、[log-pass.txt](log-pass.txt)。

## 四、改动

1. **目录迁移**：`engine/{hardware,kernels,manifest,modules}` → `devices/`；
   `engine/` 只留 `out/`、`roms/`。
2. **engine 访问器**（`internal/engine/root.go`）：
   - `WorkspaceRoot()`、`DevicesRoot()`、`ManifestRoot()`、`HardwareDir()`、
     `KernelsRoot()`、`ModulesRoot()`、`EvidenceRoot()`；
   - `DevicesDBPath()` = `devices/manifest`；`RomsRoot/OutRoot` 仍指 engine 下。
3. **根定位**：`engine.Init()` 改为「`<仓库根>/engine` + `<仓库根>/devices/manifest`」
   判据；`profile.findRoot` 改判 `devices/hardware`；daemon `ProjectRoot` 与
   cdha `detectEngineRoot` 同步（保留旧布局兼容分支）。
4. **阶段路径**：
   - 03/04 的内核树改 `engine.KernelsRoot()`；
   - 03/04/06/10 的外部模块改 `engine.ModulesRoot()`；
   - 09 的 HAL 源码树改 `engine.HardwareDir()`；
   - `kmod` 的展示基准改为 `devices/`。
5. **agent 侧**：新增 `tools.DevicesRoot/ManifestRoot`，`manifest.show`、诊断上下文包、
   `/api/manifest/*`、`/api/runs/{id}/evidence`、`CDH_DEVICES_DB` 改用；
   `manifestview` 增加 `devicesRoot()`，manifest/hardware 读取改新根；
   cdha 的 read/write roots 增加 `<仓库根>/devices`。
6. **规则与文档**：`AGENTS.md` 第 5/6 条路径、pipeline/daemon/README、
   `devices/manifest/README.md`、模块与内核补丁 README、清单 note、report、skill
   与证据树内导航链接统一改为 `devices/...`（历史 `log-*.txt` 保持事实快照）。

## 五、验证

- `cd agent && go build ./... && go vet ./... && go test ./...`：全绿；
- `cdh device list`：正常列出 32 台已登记机型（来自 `devices/manifest`）；
- `cdh device build gold --manifest 15-OS3.0.11.0.VNQCNXM --dry-run`：13 阶段计划正常；
- `cdh build-hardware --android 15 --dry-run`：源码树解析到 `devices/hardware`、45 模块；
- `host-daemon state`：默认镜像根仍为 `engine/out/dist`；
- `cdha tool manifest.show`：设备表路径显示 `devices/manifest/devices/...`；
- `cdha tool evidence.search`：仍命中 `devices/evidence/...`。

## 六、遗留

- 历史 `log-*.txt` 与 `agent/out/harness` 运行数据里的旧绝对路径不改写，
  回溯时按新根换算；
- 外部脚本/书签若硬编码 `engine/manifest|hardware|kernels|modules` 需改到 `devices/`；
- 远端 ROM 构建服务器部署时需同时保留 `devices/`（此前需 `engine/` 下的同名目录）；
- `engine/out`、`roms` 按本次范围**不迁移**，仍由 engine 承载。
