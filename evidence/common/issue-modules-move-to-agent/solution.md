# issue-modules-move-to-agent：构建/宿主/校验三模块迁入 agent 并统一为 CLI

## 问题

三块与 agent 强相关的代码一直散在 agent 之外：

| 旧位置 | 内容 | 问题 |
| --- | --- | --- |
| `engine/build` | 13 阶段镜像流水线（独立 Go module `build`） | 对 agent 是外部 module，`cdha` 无法直接 import；自带 `cdh web` 浏览器控制台 |
| `core/host/daemon` | 云机宿主守护进程（独立 Go module） | 单进程 Web 控制台 + 静态前端，agent 只能调 HTTP；不能作为库内嵌 |
| `core/development/device-checker` | Rust 设备校验服务 + Android App + Web 前端 | 前端与 REST 管理面耦合，agent 只能走浏览器同款接口 |

三个模块各自带一套浏览器 UI，与「agent 统一调用」的目标冲突：agent 要么再写一遍
客户端逻辑，要么把 Web 控制台暴露给用户；同时独立 module 让 `go build ./...`
无法覆盖流水线与 daemon，跨模块改一行要两边分别编译。

## 根因

历史分层把「构建（data plane）」放在 engine、把「运行/校验（control plane）」放在
core，agent 只做编排。实际演进中三者都需要读取 `devices/manifest`、`engine/out` 与
agent 自己的 `out/`，并且越来越多地互相调用（`cdha` 的 daemonctl 已经直接 import
daemon 的 config）。按仓库分区而不是按调用关系分区，导致：

1. 跨 module 引用需要先合并成一个 Go module，否则无法共享类型；
2. 各模块的 Web 控制台是「人看」的入口，agent 需要的是可解析的 CLI/JSON；
3. 路径全部按旧目录写死，搬迁后 `engine/build/...`、`engine/vendor/build/...`
   这类判据与资源定位同时失效。

## 证据

- `agent/go.mod` 现在覆盖三个模块：`agent/modules/{pipeline,daemon,checker}` 下已无
  `go.mod`，`go build ./...` 一次覆盖全部包；
- `agent/modules/pipeline` 保留 `internal/`、`stages/`、`cli/`；`web/` 已删除；
- `agent/modules/daemon` 保留 `config/`、`run/`、`backend/`、`test/`；`web/` 已删除；
- `agent/modules/checker` 保留 `app/`、`checks/`、`core/`、`server/`；`web/` 已删除；
- 首轮编译/测试失败与修复过程的原始输出见同目录
  [log-fail.txt](log-fail.txt)、[log-pass.txt](log-pass.txt)。

典型证据：搬迁后 `go build ./...` 报

```
cmd/cdh-engine/main.go:4:8: package cloud-phone-hub/agent/modules/pipeline/cli is not in std
internal/daemonctl/daemonctl.go:81:17: undefined: daemonrun.NewHandler
internal/daemonctl/vnc.go:67:13: undefined: daemonrun.BridgeWSAndTCP
```

说明 CLI 层与新 HTTP 接线都还没按新布局补齐，而不是简单的路径替换能带过。

## 改动

### 1. 目录搬迁与 import 统一

- `engine/build` → `agent/modules/pipeline`；
- `core/host/daemon` → `agent/modules/daemon`；
- `core/development/device-checker` → `agent/modules/checker`；
- 删除 `pipeline/go.mod`、`daemon/go.mod`，三模块统一为 `cloud-phone-hub/agent` 子包；
- 批量替换 166 个文件的 import：
  `cloud-phone-hub/engine/build/` → `cloud-phone-hub/agent/modules/pipeline/`，
  `cloud-phone-hub/core/daemon/` 与 `cloud-phone-hub/agent/modules/hostdaemon/` →
  `cloud-phone-hub/agent/modules/daemon/`，
  `cloud-phone-hub/agent/modules/enginebuild/` → `cloud-phone-hub/agent/modules/pipeline/`。

### 2. pipeline 去 Web、CLI 成包

- 平铺入口 `main.go / cli_device.go / cli_stage_run.go / cli_stages.go` 移入 `cli/`，
  `package main` → `package cli`，`func main()` → `func Main()`；
  入口只保留 `agent/cmd/cdh-engine`（`cli.Main()`）；
- 删除 `web/` 包、`cdh web` 子命令、usage 中的 web 行与 `runWeb()`；
- `internal/engine` 的根定位改为「`manifest/` 标记 + `hardware/` 目录」判据，
  并在向上查找时额外检查 `<祖先>/engine`，因此从仓库根、`agent/`、任意子目录执行都能命中；
- 新增 `engine.PipelineDir()`（= `<engine 根>/../agent/modules/pipeline`），
  修正 03/04 的内核配置片段、09 的 HAL 模块目录与 `cdh-vendor` 入口路径。

### 3. daemon 去 Web、CLI + API 双入口

- `run/web.go` 换成 `run/http.go`：
  - `run.NewHandler(opt)` 只组装 `/healthz`、`/api/{state,create,start,stop,delete}`、
    `/api/channel/*`、`/api/vnc-ws`，供 cdha 进程内挂载；
  - `run.Serve(opt)` 做孤儿后端回收、历史实例目录迁移后监听 `-addr`；
  - 删除 `/` 静态控制台与 `web` 包；
- `run.Main` 改为 CLI 子命令分发：
  `serve / state / images / create --index|--match / start|stop|delete --spec --index`，
  除 `serve` 外全部把结果以缩进 JSON 打到 stdout；
- 新增 `run/ws.go`：`upgradeWebSocket` + `BridgeWSAndTCP`（RFC 6455 帧编解码、
  ping/pong/close 处理），补上 `/api/vnc-ws` 端点，`daemonctl` 远端隧道不再悬空；
- 目录探测标记从 `core/host` 改为 `devices/manifest`，避免旧路径消失后无法定位项目根。

### 4. checker 去 Web、补 CLI

- 删除 `web/`（单页前端）与 `ServeDir` fallback，`--web` 参数移除；
- `server/src/main.rs` 改为子命令：
  - `serve [--addr --ws --checks --log]`：REST + `/ws`，无静态页；
  - `checks [--checks]`：输出功能点元数据 JSON；
  - `run --ids a,b [--devices x,y] [--args k=v]… [--ws --wait]`：进程内起 `/ws`
    接入端，等待设备注册后下发，结果 JSON 打到 stdout；
- `log.rs` 改为写 stderr，stdout 只留 CLI 结果；`Cargo.toml` 移除 `tower-http`；
- `start.sh` / `docker-entrypoint.sh` / `Dockerfile` / `justfile` / `deploy_docker.py`
  同步去掉 `web/` 与 `--web`，统一 `checker-server serve …`。

### 5. cdha 侧收口

- noVNC 资源从 `modules/daemon/web/static/vendor` 迁到 `agent/web/vendor`，
  `web.Assets` embed 清单加入 `vendor`，`/vendor/` 路由改由 cdha 自己的 webFS 提供；
- `release.go` 不再复制 host-daemon 的 web 静态资源，`start.sh` 改为
  `daemon serve -addr …`；
- `modules/devicechecker` 引用统一改为 `modules/checker`（cdha、测试、.gitignore）；
- 旧的顶层构建入口 `build/core/host-main` 改为在 `agent/` 模块内编
  `./cmd/host-daemon`，`web` 子命令改为 `serve`，`just core host` 注释同步新路径；
- 重新生成 `engine/out/bin/cdh`、`agent/out/dist/{cdh-engine,cdha,host-daemon}` 与
  `out/core/daemon/dist/darwin/daemon`，并清掉发布目录里遗留的 `web/`。

## 验证

- `cd agent && go build ./... && go vet ./... && go test ./...`：全绿
  （pipeline / daemon / checker / internal 全部包）；
- `cd agent && go run ./cmd/cdh-engine device build gold --manifest 15-OS3.0.11.0.VNQCNXM --dry-run`：
  输出 `==> 阶段(13): unpack, prepare, …, pack_vda`，顺序正确；
- `cd agent && go run ./cmd/cdh-engine build-hardware --android 15 --dry-run`：
  源码树解析到 `devices/hardware`，45 个模块，中间件落 `engine/out/hal/android15`；
- `go run ./cmd/host-daemon images` / `state`：返回镜像与实例 JSON，默认根目录正确；
- `cd agent/modules/checker && cargo test -p checker-server`：1 passed；
  `cargo run -q -p checker-server -- checks|run`：JSON 输出正常（run 用 Mock agent）；
- `TestUnifiedPlatformRoutes` 覆盖 `/vendor/noVNC/core/rfb.js` 200 与 javascript MIME。

## 遗留

- `agent/out/dist/host-daemon-*/start.sh` 是搬迁前的历史发布产物，仍写着 `-web`
  与旧 web 目录；下次 `cdha release` 会按新模板重生成，不手工改产物；
- 远端 ROM 构建服务器若只投递 `cdh-engine` 二进制、不部署 `agent/` 源码树，
  依赖 `agent/modules/pipeline/stages/08_build_hardware/modules/*.json` 与
  `03_build_kernel/lineage-kernel.config` 的步骤会缺件；需要时把
  `agent/modules/pipeline` 一并部署（与 13 阶段改造前的 `engine/build` 同口径）；
- 历史文档（`TODO.md`、部分 `devices/evidence/*`）仍引用 `engine/build`，属只读回溯材料，
  未批量改写；`AGENTS.md` 第 4 条与各模块 README 已同步新路径；
- `agent/modules/enginebuild` 草案副本已不在仓库中（`agent/modules` 现只有
  `checker/daemon/pipeline`），无需再清理。
