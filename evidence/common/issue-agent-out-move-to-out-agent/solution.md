# Issue: agent 产物根从 `agent/out` 迁到仓库根 `out/agent`

- 设备: 通用（仓库目录结构与产物落点）
- 状态: **已闭环**（2026-11-15）

---

## 一、问题

`agent/` 的构建与运行期产物一直落在 `agent/out`（352 MB：dist / harness / config / web / releases / tmp），
而仓库根已把其它模块的产物统一收到 `out/`（`out/qemu`、`out/cuttlefish`、`out/sources` …）。
同一类产物分散在 `agent/out` 与 `out/` 两处：agent 仓库内部混入运行数据，清理/备份/忽略规则
要记两个位置，`out/` 也不是唯一的产物根。

## 二、根因

产物根只有一个计算点：`agent/cmd/cdha/main.go` 的 `agentOutDir := filepath.Join(agentRoot, "out")`，
`store` / `release` / `cluster` / DSH 会话全部继承该值；路径一经计算就写进启动参数、
运行态配置（`nodes.json` 的 `deployDir`、`dsh-tools.patch.yml` 的 command）与文档，所以只搬目录不够。

另有一个约束：`fsx.Guard` 原先把 `<仓库根>/out` 整棵列为禁写根，产物根一旦放进 `out/agent`，
它也会被自己拦掉，必须给禁写根加「例外子目录」。

## 三、证据

- `agentOutDir` 单点见 `cmd/cdha/main.go`；`dshsdk.Locate` 用 `agentRoot/out/harness/dsh`；
  `server/roms.go` 的 `cdh-engine` 回退路径写 `<engine>/../agent/out/dist/cdh-engine`；
- 旧管理台启动输出：`产物与发布根: /Volumes/work/cloud-phone-hub/agent/out/dist`；
- `du -sh agent/out` = 352 MB；`out/` 下已有 `cuttlefish/ qemu/ sources/ …`；
- 原始检查与迁移后记录见同目录 [log-fail.txt](log-fail.txt)、[log-pass.txt](log-pass.txt)。

## 四、改动

1. **产物根**：`cmd/cdha/main.go` 改为 `filepath.Join(workspaceRoot, "out", "agent")`；
   TMPDIR 随之指向 `out/agent/tmp`。
2. **fsx 禁写例外**：`fsx.Options` 新增 `DenyWriteExempts`（禁写根内的例外子目录），
   `ResolveWrite` 对命中例外的路径放行；cdha 仍禁写 `<仓库根>/out`，只放行 `out/agent`，
   `engine/out` 红线不变。新增单测覆盖「例外可写、兄弟目录仍拦」。
3. **其它计算点**：`dshsdk` 的 `DSH_HOME` → `<仓库根>/out/agent/harness/dsh`；
   `server/roms.go` 的 `cdh-engine` → `<仓库根>/out/agent/dist/cdh-engine`。
4. **文档与文案**：`agent/README.md` 第 2/3/4 节、`modules/daemon/README.md` 的构建命令、
   Web 控制台文案、`store` / `cluster` / `bash_tool` / `eval` 的路径注释统一为 `out/agent/…`；
   `agent/.gitignore` 移除已不再产生的 `out/`（仓库根 `.gitignore` 仍忽略整个 `out/`）。
5. **运行态迁移**：停掉旧 `cdha serve` 后 `mv agent/out out/agent`（原样保留 dist/harness/config/
   web/releases/tmp），修正 `config/nodes.json` 的 `deployDir`、`config/dsh-tools.patch.yml` 的
   command 与 4 份 `releases/*.json` 里的旧绝对路径；三个二进制重编到 `out/agent/dist` 并重启管理台。

## 五、验证

- `cd agent && go build ./... && go vet ./... && go test ./...` 全绿（含 `internal/fsx` 新例外用例）；
- 新管理台启动输出 `产物与发布根: /Volumes/work/cloud-phone-hub/out/agent/dist`，
  `HTTP 200`（`http://127.0.0.1:17890/?token=…`），进程命令行指向 `../out/agent/dist/cdha`；
- `agent/out` 不再存在、也不再被生成；`out/agent/{config,dist,harness,releases,tmp,web}` 齐全；
- `agent` 与 `out/agent/config` 中已无 `agent/out` 旧路径（历史日志/会话除外）。

## 六、遗留

- `out/agent/web/cdha.log`、`out/agent/harness/chats/*.json` 属历史运行数据，其中的旧路径不改写
  （与 #237/#238/#239 的口径一致）；
- 本机 `out/agent/config/nodes.json` 的 `local.romsDir` 仍是 `engine/roms`（#239 ROM 根迁移时
  未同步运行态配置），本次只改 agent 产物路径，未顺带修改；
- `devices/evidence/` 历史条目里的 `agent/out/...` 引用保持原文，按证据快照对待。
