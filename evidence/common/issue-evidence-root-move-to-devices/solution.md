# Issue: 证据根从 engine/evidence 迁到 devices/evidence

- 设备: 通用（证据体系与构建/运行/对齐共用）
- 状态: **已闭环**（2026-11-15）

---

## 一、问题

证据体系把 767 个文件（62 MB、11 个分组）统一放在 `engine/evidence/`，但：

1. 证据内容按**机型**分组（`gold/`、`m22/`、`ss9380/`、`common/`），与 engine 的
   manifest / out / hardware / kernels / modules / roms 不是一类东西；
2. #236 之后构建流水线已迁到 `agent/modules/pipeline`，engine/ 不再承载构建代码，
   证据继续挂在 engine 下会让「engine = 引擎输入/产物」的边界变模糊；
3. 代码里三处硬编码 `filepath.Join(engineRoot, "evidence")`（`tools/evidence_tools.go`、
   `server/server.go`、`agent/context_pack.go`），目录一动就全部失效。

目标：把证据根整体迁到仓库根 `devices/evidence/`，并让 agent 的检索/写入、cdha 的
`POST /api/runs/{id}/evidence`、AGENTS 规则与所有文档导航同步到新根。

## 二、根因

证据最初与构建流水线一起放在 `engine/build` 时代，`evidence/` 作为 engine 的兄弟目录
自然成立；随着机型清单、构建流水线、运行/校验模块陆续迁到 `devices/` 与 `agent/`，
证据仍然按旧布局解析。「证据是设备侧资产、按机型分组」这一事实没有反映到目录树上，
而代码又把 `EngineRoot` 当成唯一基准，于是形成「内容属于 devices、路径依赖 engine」的错位。

## 三、证据

- 迁移前 `engine/evidence` 有 767 个文件、62 MB，分组为
  `15r / common / cph2797 / g1170 / gold / m22 / oriole / popsicle / rmx6688 / ss9380 / sun`；
  `devices/` 为空。
- 代码硬编码位置：
  `agent/internal/tools/evidence_tools.go:87`、`:406`，
  `agent/internal/server/server.go:1211`，
  `agent/internal/agent/context_pack.go:55`。
- `AGENTS.md` 第 2 条、`agent/README.md`、模块 README、`devices/manifest/configs/*.json`
  的 note、`report/` 与 `skills/aligner-compare/SKILL.md` 均引用旧路径。
- 迁移前后的原始命令与输出见同目录 [log-fail.txt](log-fail.txt)、[log-pass.txt](log-pass.txt)。

## 四、改动

1. **目录迁移**：`engine/evidence/` → `devices/evidence/`（同盘 mv，767 个文件原样保留）；
   分组、slug、`#N` 与各组 README 索引不重排。
2. **统一证据根解析**：`agent/internal/tools/helpers.go` 新增
   `EvidenceRoot(engineRoot) = <仓库根>/devices/evidence`；替换
   `evidence.search` / `ev.new` / `CreateEvidenceEntry` / `server.handleSaveEvidence` /
   诊断上下文包里的三处 `engineRoot/evidence`。
3. **cdha 工作区**：`evDir` 改为 `<workspaceRoot>/devices/evidence`，仍在 writeRoots 内；
   `detectEngineRoot` 去掉 `evidence` 目录标记，只用 `devices/manifest` 判定。
4. **相对路径口径**：`SearchEvidence` 与 `ev.new` 的 `RelPath`/`solutionPath`
   统一带 `devices/evidence/` 前缀；`tools_test` 的 guard 读根与断言同步。
5. **规则与文档**：`AGENTS.md` 第 2 条全部改为 `devices/evidence/`；
   `agent/README.md` 补证据根说明；模块 README、清单 note、report、skill 内的
   导航路径批量替换；证据树内 `solution.md` 的 `engine/evidence` 链接同步，
   历史 `log-*.txt` 保持原文作为事实快照。
6. **顶层索引**：`devices/evidence/README.md` 改为 devices 视角，说明迁移日期与
   agent/cdha 的同步点；新增本条目 #237 并更新 `common/README.md` 条目表与
   顶层 `common/` 行的计数/编号范围。

## 五、验证

- `cd agent && go build ./... && go vet ./... && go test ./...`：全绿；
- `cdha tool evidence.search '{"query":"#151","limit":2}'`：命中 #151/#154，
  路径显示为 `devices/evidence/...`；
- `cdha tool ev.new … --dryRun`：分配 `#237`，目标路径为
  `devices/evidence/common/issue-test-dryrun/solution.md`；
- `find devices/evidence -type f | wc -l` 仍为 767（加本条目后为 770），
  `ls engine` 已无 `evidence`。

## 六、遗留

- 历史 `log-fail.txt` / `log-pass.txt` 中仍有 `engine/evidence/...` 绝对路径：
  它们是当时的事实快照，按 AGENTS 第 2 条不改写，回溯时按新根换算；
- `agent/out/harness/chats/*.json` 等历史运行数据里也留有旧路径，属产物不入库；
- 外部书签或旧脚本若直接引用 `engine/evidence/` 需要改到 `devices/evidence/`；
- 迁移本身不涉及编号重排，`#1~#236` 的归属与各组索引保持原样。
