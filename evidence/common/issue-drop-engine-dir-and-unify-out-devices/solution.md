# Issue #244: 消灭 `engine/` 目录，统一输出根到 `out/devices/` 并重命名 `cdh-pipeline`

- 设备: 通用（仓库工作区结构、构建输出根、流水线 CLI 与全仓文档/配置）
- 状态: **已闭环**（2026-11-16）

---

## 一、问题

随着仓库架构升级（接续 [issue-repo-manifest-drop-core-engine](../issue-repo-manifest-drop-core-engine/solution.md) 与 [issue-agent-out-move-to-out-agent](../issue-agent-out-move-to-out-agent/solution.md)），`/Volumes/work/cloud-phone-hub/engine` 子目录已整体消灭，原 `engine/out/` 统一迁入仓库根 `out/devices/`（含 `dist/`、`roms/`、`kernel/`、`hal/`、`vendor/`、`devices/`、`bin/`、`tmp/`），流水线独立 CLI 由 `cdh-engine` 升级为 `cdh-pipeline`。

但仓库内仍存在大量对 `engine/`、`engine/out`、`cdh-engine` 与旧 `cdh <子命令>` 的依赖及描述：
1. Go 运行代码与单测仍以 `<仓库根>/engine` 为锚点，再用 `..` 反推 `devices/`、`roms/`、`agent/`，并把输出与临时目录拼向 `engine/out/`；
2. 独立构建 CLI 入口仍叫 `agent/cmd/cdh-engine`，策略白名单、远程部署脚本、Web 前端与配置均硬编码 `cdh-engine` 与 `engine/out/dist`；
3. 运行配置（`out/agent/config/nodes.json`、`out/agent/config/dsh-tools.patch.yml`、`.trae/mcp.json`）、项目约束文档、各阶段 README 与技能文件仍写着 `engine/` 与 `engine/out/`。

## 二、根因

历史布局下构建引擎源码与产物独立落在 `<仓库根>/engine/`，导致三个层面的耦合散落在全仓各处：
1. **根目录与输出根耦合**：`agent/modules/pipeline/internal/engine/root.go`、`agent/cmd/cdha/main.go`、`agent/internal/{fsx,manifestview,server,cluster,tools,agent}` 以及多个 `*_test.go` 假定 `engine/` 子目录存在且 `OutRoot = <engineRoot>/out`，同时测试残留目录曾触发 `searchUp` 误判；
2. **入口命名耦合**：`agent/cmd/cdh-engine`、`agent/configs/policy.yaml`、`agent/internal/cluster/cluster.go`、`agent/internal/server/roms.go`、`agent/internal/agent/build.go` 与各阶段/技能说明均按 `cdh-engine` 或 `../engine/out/bin/cdh` 拼接命令；
3. **持久化配置与运维文档滞后**：代码路径升级后，磁盘上的 `nodes.json`、`dsh-tools.patch.yml`、`.trae/mcp.json`、前端目录向导（`agent/web/{app.js,index.html}`）以及仓库文档仍指向 `<repo>/engine` 与 `engine/out/dist`。

## 三、证据

- `/Volumes/work/cloud-phone-hub/engine` 已不存在，`out/devices/` 作为统一设备构建输出根就位。
- 改造前抽查现场：`out/agent/config/nodes.json` 中 `local.engineRoot` 为 `/Volumes/work/cloud-phone-hub/engine`、`distDir`/`deviceRoot` 为 `/Volumes/work/cloud-phone-hub/engine/out/dist`；`out/agent/config/dsh-tools.patch.yml` 與 `.trae/mcp.json` 分别指向 `/Volumes/work/cloud-phone-hub/engine` 与 `engine/out/bin/cdh`；`agent/internal/cluster/ssh_wizard.go`、`agent/web/{app.js,index.html}` 与多处 `*_test.go` 仍拼接 `engine/`（详见同目录 [log-fail.txt](log-fail.txt)）。
- 全仓构建、静态检查、测试、语法检查、实跑冒烟及残留扫描记录见 [log-pass.txt](log-pass.txt)。

## 四、改动

1. **入口与策略升级为 `cdh-pipeline`**：
   - 将构建入口迁为 [agent/cmd/cdh-pipeline/main.go](../../../../agent/cmd/cdh-pipeline/main.go)，移除旧 `agent/cmd/cdh-engine` 与产物 `out/agent/dist/cdh-engine`；
   - 更新 [agent/configs/policy.yaml](../../../../agent/configs/policy.yaml)、[agent/internal/agent/build.go](../../../../agent/internal/agent/build.go)、[agent/internal/server/roms.go](../../../../agent/internal/server/roms.go) 与 [agent/internal/cluster/cluster.go](../../../../agent/internal/cluster/cluster.go) 统一使用/交叉编译并投递 `cdh-pipeline`。
2. **`pipeline` 与 `daemon` 模块根路径重锚**：
   - 更新 `agent/modules/pipeline/internal/engine/root.go`：`isWorkspaceRoot` 同时校验 `devices/manifest` 与 `agent`（防止散落 `out/`/`devices/` 误判），输出根设为 `<仓库根>/out/devices`、`TempRoot` 设为 `out/devices/tmp`，兼容归一化旧 `/engine` 后缀环境变量；同步更新 `pipeline` `internal/` 与 12 个 `stages/*` 及测试并清理历史测试残留目录；
   - 更新 `agent/modules/daemon/` 默认 `DeviceDir` 为 `<仓库根>/out/devices/dist` 及相关探测/配置逻辑。
3. **`cdha` 核心、Web 前端与测试夹具同步**：
   - 更新 [agent/cmd/cdha/main.go](../../../../agent/cmd/cdha/main.go)：新增 `--workspace-root` / `--remote-workspace`（保留 `--engine-root` / `--remote-engine` 与内部 `EngineRoot` 字段别名兼容），`fsx.Guard` 禁写 `out` 与 `out/devices`（放行 `out/agent`），`detectEngineRoot` 返回仓库根；
   - 更新 [agent/internal/manifestview/inspector.go](../../../../agent/internal/manifestview/inspector.go)、[agent/internal/tools/helpers.go](../../../../agent/internal/tools/helpers.go)、[agent/internal/agent/context_pack.go](../../../../agent/internal/agent/context_pack.go)、[agent/internal/agent/dsh_bridge.go](../../../../agent/internal/agent/dsh_bridge.go)、[agent/internal/agent/loop.go](../../../../agent/internal/agent/loop.go)、[agent/internal/cluster/ssh_wizard.go](../../../../agent/internal/cluster/ssh_wizard.go)、[agent/internal/fsx/fsx.go](../../../../agent/internal/fsx/fsx.go)、[agent/internal/store/store.go](../../../../agent/internal/store/store.go) 与对应 7 组 `*_test.go`；
   - 更新 [agent/web/app.js](../../../../agent/web/app.js) 与 [agent/web/index.html](../../../../agent/web/index.html) 中 `out/devices/dist`、`devices/manifest/versions`、目录向导与标签文案（保留 DOM id 与 JSON 字段 `engineRoot` 协议兼容）。
4. **配置、技能与文档全仓对齐**：
   - 更新 [out/agent/config/nodes.json](../../../../out/agent/config/nodes.json)、[out/agent/config/dsh-tools.patch.yml](../../../../out/agent/config/dsh-tools.patch.yml) 与 [.trae/mcp.json](../../../../.trae/mcp.json)；
   - 更新 [AGENTS.md](../../../../AGENTS.md)（按规则保留 §2 停用注释原文）、[TODO.md](../../../../TODO.md)、[manifest/default.xml](../../../../manifest/default.xml)、[build/justfile](../../../../build/justfile)、[.trae/skills/cloud-phone-build/SKILL.md](../../../../.trae/skills/cloud-phone-build/SKILL.md)、[skills/aligner-compare/SKILL.md](../../../../skills/aligner-compare/SKILL.md)、[skills/build-device/SKILL.md](../../../../skills/build-device/SKILL.md)、[agent/README.md](../../../../agent/README.md)、[agent/evals/evidence_regression.yaml](../../../../agent/evals/evidence_regression.yaml)、`agent/modules/{daemon,pipeline}/README.md`、`agent/modules/daemon/hvc.md`、`agent/modules/pipeline/stages/*/README.md`、[agent/modules/checker/checks/java/ultimate/VERIFY-REPORT.md](../../../../agent/modules/checker/checks/java/ultimate/VERIFY-REPORT.md)、`devices/kernels/patches/{README.md,6.12/README.md}`、`devices/modules/{cdh_redirect/README.md,kernelsu/{Kbuild,README.md}}` 及 [common/README.md](../README.md) 备注；
   - 不回改 `devices/manifest/**/*.json`、既有 `devices/evidence/<分组>/issue-*/` 历史记录、`doc/manifest.md`（图形引擎术语）与 `AGENTS.md` §2 停用注释块。

## 五、验证

全部检查均在最终改动后执行并通过（详见 [log-pass.txt](log-pass.txt)）：
1. **编译与产物检查**：`go build` 成功生成 `out/agent/dist/{cdha,cdh-pipeline,host-daemon}`；`test ! -e /Volumes/work/cloud-phone-hub/engine` 与 `test ! -e /Volumes/work/cloud-phone-hub/out/agent/dist/cdh-engine` 均成立。
2. **静态检查、全量单测与前端语法**：
   - `go vet ./...` 通过；
   - `go test -count=1 ./...` 汇总：`51 ok, 14 no test files, total 65`（全绿零失败）；
   - `node --check /Volumes/work/cloud-phone-hub/agent/web/app.js` 退出码 `0`。
3. **实跑冒烟（live smoke）**：
   - `./out/agent/dist/cdh-pipeline device list` 输出 `40` 台机型清单；
   - `./out/agent/dist/cdh-pipeline device build gold --manifest 15-OS3.0.10.0.VNQCNXM --dry-run` 正确识别 `out/devices/tmp/db-manifest/...`、`roms/...` 与 13 个构建阶段；
   - `./out/agent/dist/host-daemon state` 返回 `"deviceRoot": "/Volumes/work/cloud-phone-hub/out/devices/dist"`；
   - `./out/agent/dist/cdha tool list`（13 个工具）与 `./out/agent/dist/cdha node list` 均正常返回新工作区根与 `out/devices/dist`。
4. **全仓残留扫描**：排除历史归档、内核/第三方源码与 `out/` 后，活动代码与文档无失效 `cdh-engine`、`engine/out` 或 `/Volumes/work/cloud-phone-hub/engine` 引用（仅 `agent/internal/cluster/cluster.go` 第 652 行保留对远端旧主机 `${ENG_ROOT}/out/bin/cdh` 的向后兼容探针兜底）。

## 六、遗留

- 既有历史证据目录（`devices/evidence/<分组>/issue-*/`）、`devices/manifest/**/*.json` 与历史评测归档（`report/`）保留当时客观记录，不作追溯改写。
- 远端 SSH 节点若此前已部署过旧 `cdh-engine` 二进制，下次通过 `cdha` 下发构建时会自动交叉编译并覆盖为 `cdh-pipeline`。
