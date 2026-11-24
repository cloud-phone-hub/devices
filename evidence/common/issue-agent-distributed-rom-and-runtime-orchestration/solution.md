# Issue #325: 完善 agent (`cdha`) 分布式编排体系——ROM 节点 `pipeline` 下发、运行节点运行栈（`daemon` + 宿主后端 + QEMU + 固件）部署、双机免密直传、外网 SSH 隧道管控与全链路存储红线

## 1. 问题

对照 `agent/core/node.md` 的分布式架构目标（ROM 服务器 `192.168.31.164` / `81.70.30.115:6000` `x86_64` 存放多路径 ROM 并通过下发 `pipeline` 生成云机盘集；运行服务器 `192.168.31.207` / `47.99.97.156:8080` `arm64` OrangePi 5 Plus 通过下发 `qemu` 与后端程序及 `daemon` 运行和管理云机；且运行服务器存储有限，空间不足必须立即停止），既有 `agent/core` 实现存在 6 处关键缺口与现网 Bug：

1. **ROM 节点探活误判离线（SIGPIPE 141）**：实测 `POST /api/nodes/rom-main/probe` 返回 `online: false`，而 `error` 字段完整打印了系统信息和 30 条 ROM 路径。
2. **ROM 节点缺少一键下发 `pipeline` 与最小构建上下文同步**：远端 `/home/backup/cloud-phone-hub/runtime` 缺 `pipeline` 与 `devices/{manifest,hardware}`，且构建时只传了单个 `CDH_ROMS_ROOT`，未将全部多路径 `RomsDirs` 透传；扫描深度仅 3 层，漏掉了 4~5 层深目录（如 `/home/roms/pixel/Pixel 10/16/OPEN/*.zip`）。
3. **运行节点部署缺宿主后端、缺 `-qemu-root`  wiring、路径重叠且无健康探活**：既有 `release.BuildHostDaemon` / `DeployHostDaemon` 仅打 `daemon` + 固件，未打包 Pure-Go 宿主后端（`net-host-service` / `vhost-modem-controller`）与 `out/qemu/dist/<os>` 组件；`start.sh` 未传 `-qemu-root`；当 `RuntimeRoot` 指向总工作目录 `/home/orangepi/cloud-phone-hub/runtime` 时会与 `images/`、`daemon/`、`qemu/` 混在一起导致 `daemon` 误扫目录；部署前未做存储红线校验，拉起后未做 HTTP 健康探活。
4. **ROM → 运行机无法直传**：两台服务器间未配置免密 SSH，`remote-direct-rsync` 直接报 `Permission denied` 并退回经控制机 Mac 中转（且中转前未校验本机余量、中转后未清理临时盘集）。
5. **外网无法管控运行机 `daemon` / VNC**：`daemonUrl` 写死局域网 IP `http://192.168.31.207:9050`，控制机切到外网走 `47.99.97.156:8080` 时无法访问远端 `daemon` API 与 VNC。
6. **存储红线未覆盖全链路 & Web 控制台缺少分布式运维视图**：此前仅 `SyncBundle` 检查了磁盘空间，下发运行栈、创建/启动实例、全链路流水线打包前置阶段均未拦截；前端节点卡片未展示磁盘使用率进度条、红线告警、组件就绪徽章与一键部署操作。

## 2. 根因

1. `cluster.Manager.RunScript` 头部统一注入 `set -euo pipefail`，而 `Probe()` 脚本中 `find ... | head -n 30` 在 ROM 文件超过 30 个时 `head` 提前关闭管道导致 `find` 收到 `SIGPIPE`（退出码 141），被 `pipefail` 判为整体执行失败。
2. `release` 包与 `cluster` 包此前面向单机打包设计，未把 `qemu/backend/{modem-host-controller,net-host-service}`、`out/qemu/dist/<os>` 及节点 `EffectiveQemuRoot()` / `EffectiveRuntimeRoot()` 纳入发布编排，且 macOS BSD `tar` 打包时默认附带 `LIBARCHIVE.xattr.com.apple.provenance` 扩展属性导致 Linux GNU `tar` 解压告警，远端 `daemon` 启动前 `reapOrphanBackends` 在多容器宿主机上耗时超过原固定 `sleep 1` 窗口。
3. 缺少集中式的 `CheckNodeDiskSpaceReport` 与 `SSHTunnel` / `SetupDirectSSHTrust` 基础设施，导致空间红线校验、双机直传信任和跨网段 `daemon` 隧道未在 `cluster` / `release` / `daemonctl` / `pipeline_run` 全链路打通。

## 3. 证据

- 失败与红线拦截现场见同目录 [`log-fail.txt`](log-fail.txt)。
- 真实双机全链路联调与单测通过日志见同目录 [`log-pass.txt`](log-pass.txt)。

## 4. 改动

1. **集群底座与探活/直传/隧道（`agent/core/internal/cluster/` + `agent/modules/pipeline/pkg/proc/session.go` + `agent/core/internal/fsx/fsx.go`）**：
   - `cluster.go`：彻底移除 `Probe()` 中 `| head -n 30`（改用 `while read` / `awk` 消费全流避免 `SIGPIPE 141`），放宽多目录扫描深度至 5 层并过滤 0 字节及 `.part`/`.aria2`/`.tmp`/隐藏文件；扩展 `NodeProbeReport` 返回 `PipelinePath`、`WorkspaceReady`、`RomCount`、`DirectSSHReady`、`QemuReady`、`QemuPath`、`KvmReady`、`BackendsReady`/`Missing`、`DaemonViaSSH`；新增 `EffectiveRuntimeRoot()`（总工作目录与 `images/` 重合时自动隔离到 `<RuntimeRoot>/instances`）、`EffectiveQemuRoot()`、`EffectiveDeployBase()`、`EffectiveDaemonPort()`；`DispatchEngineBuild` 增加 ROM 节点构建前空间检查并透传 `CDH_ROMS_DIRS`；`SyncBundle` 优先使用 `~/.ssh/id_ed25519_cdh` 直传，遇 `Permission denied` 自动建立信任后重试，中转兜底时先校验本机盘余量并 `defer` 清理。
   - 新增 `distributed.go`：实现 `CheckNodeDiskSpaceReport`、`CheckLocalTempDiskSpace`、`DeployPipeline`（交叉编译 `linux/amd64` `pipeline` + 增量同步 `devices/{manifest,hardware,files}` + 远端可执行验证）、`ListRemoteROMs`（多路径 python3 过滤扫描）、`SetupDirectSSHTrust`（生成 `~/.ssh/id_ed25519_cdh` + 写入目标 `authorized_keys` + 直连实测）、`PullFile` 与 `StartSSHTunnel`（支持 `SSH_ASKPASS` + `DetachTTY` 的受控 `-L` 端口转发隧道）。
   - `proc/session.go` 为 `SessionSpec` 新增 `DetachTTY bool`；`fsx/fsx.go` 新增受白名单保护的 `RemoveAll`。
2. **运行节点一体化运行栈构建与部署（`agent/core/internal/release/release.go`）**：
   - 新增 `DeployRuntimeStack` 并升级 `BuildHostDaemon` / `DeployHostDaemon`：支持交叉编译打包 `daemon` + Pure-Go 宿主后端（`vhost-modem-controller`、`net-host-service` 及配置文件）+ 架构校验匹配的 `out/qemu/dist/<os>` 组件 + EDK2 固件 + `start.sh`（加载 `daemon.env` 并传 `-qemu-root` 与 `LD_LIBRARY_PATH`）+ `stop.sh` + `manifest.json`；macOS `tar` 增加 `--no-xattrs` 消除 Linux 解压告警；部署前执行 `CheckNodeDiskSpaceReport` 存储红线硬拦截，拉起后通过 `/healthz` & `/api/state` 轮询健康检查。
3. **跨网段 `daemon` 管控与全链路存储红线（`agent/core/internal/daemonctl/` + `agent/core/internal/agent/pipeline_run.go`）**：
   - `daemonctl.go` / `vnc.go`：为远端运行节点 `CreateByIndexWithOptions`（预留 2 GiB）和 `StartInstance`（预留 512 MiB）注入磁盘空间红线硬闸门；当直连 `DaemonURL` 失败时自动经 `cluster.StartSSHTunnel` 建立按需 SSH 隧道，覆盖 HTTP API、SSE 事件流和 VNC WebSocket 投屏。
   - `pipeline_run.go`：全链路流水线打包阶段前增加 `pipeline.preflight_space` 前置红线检查（目标运行机空间不足立即停止，不浪费构建时间），并支持自动拉回远端 `build-report.json`。
4. **HTTP API、CLI、Agent 工具与 Web 控制台**：
   - `server.go` / `handlers_cluster.go`：新增 `POST /api/nodes/{id}/deploy-pipeline`、`POST /api/nodes/{id}/deploy-runtime`、`POST /api/nodes/setup-direct-ssh`、`GET /api/nodes/{id}/roms`、`POST /api/nodes/{id}/check-space`。
   - `cmd/cdha/main.go`：新增 `cdha node {deploy-pipeline,deploy-runtime,trust,roms,check-space}` 与 `cdha release daemon --include-qemu --include-backends`，退出时回收 SSH 隧道。
   - `tools/cluster_tools.go` & `configs/policy.yaml`：注册 `cluster.status`（L0 只读）分布式探活工具。
   - `agent/core/web/src/`：升级节点卡片（磁盘进度条、红线告警、ROM/运行组件徽章、一键下发 `pipeline`/部署运行栈/查 ROM/空间检查按钮）、跨机同步面板（一键打通直传免密）、向导弹窗（外网地址通用化 + `qemuRoot` + `minFreeGB`/`maxUsagePercent`）与发布中心（勾选打包宿主后端与 QEMU 组件）；更新 `web/embed_test.go`。
   - `out/agent/config/nodes.json`：按 `agent/core/node.md` 精确对齐 `rom-main` 与 `rom-main-runtime` 路径、多 ROM 路径与红线参数。

## 5. 验证

1. **真实双机端到端联调（`192.168.31.164` x86_64 + `192.168.31.207` ARM64 OrangePi 5 Plus）**：
   - `cdha node deploy-pipeline rom-main --sync-workspace=true` 5.6s 完成交叉编译下发与清单同步，远端 `pipeline device list` 正常输出全量机型；
   - `cdha node trust --src rom-main --dst rom-main-runtime` 689ms 自动打通双机 ED25519 直传免密；
   - `cdha node deploy-runtime rom-main-runtime` 成功交叉编译下发 `linux/arm64` `daemon` + `vhost-modem-controller` + `net-host-service` + EDK2 固件并拉起通过 `/healthz` 健康检查；
   - `cdha node probe rom-main`（`online: true`, `romCount: 1494`, `engineReady: true`, `workspaceReady: true`, `directSshReady: true`）与 `cdha node probe rom-main-runtime`（`online: true`, `daemonOnline: true`, `kvmReady: true`, `backendsReady: 3`）全通；
   - `cdha node roms rom-main --filter pixel` 检出 78 个深层目录 Pixel 官方包；
   - `cdha node check-space rom-main-runtime --min-free-gb 999999` 触发 `ErrInsufficientDiskSpace` 退出码 1 硬拦截；
   - `cdha node sync --src rom-main --dst rom-main-runtime --subpath _cdh_smoke/test_gold --only-patch` 走 `remote-direct-rsync` 690ms 双机直传完成，OrangePi `daemon` `/api/state` 实时扫描到盘集（验毕清理）；
   - 新版 `cdha` Web 服务线上接口 `/api/nodes/rom-main/probe`、`/api/daemon/state?nodeId=rom-main-runtime`、`/api/nodes/rom-main-runtime/check-space`、`/api/nodes/rom-main/roms` 全部验证通过。
2. **自动化单测与门禁**：
   - 新增单测 `TestProbeNoSIGPIPEOver30ROMsAndListRemoteROMs`、`TestRuntimeEffectivePathsAndSpaceReport`、`TestBuildRuntimeStackWithGoBackendsAndDiskGuard`；
   - `cd agent/core && go test -count=1 ./internal/cluster ./internal/release ./internal/daemonctl ./internal/server ./internal/tools ./internal/agent ./cmd/cdha ./web` 全部绿灯通过（含 `TestSingleGatewayASTAudit`、`TestModuleIndependence`、`TestStandaloneBinaryPortable`）；
   - `cd agent/core/web && npm run build`（`tsc --noEmit && vite build`）零错误通过。

## 6. 遗留

1. **Linux ARM64 QEMU 原生二进制构建**：本机 `out/qemu/dist/linux/` 当前尚未运行 `build/qemu/build-qemu-linux` 编译 C/Rust 组件（`qemu-system-aarch64`、`qemu-img`、`virgl_render_server`、`vhost-device-media`），`release` 打包器与运行节点探针已自动检测并清晰标出缺失清单；待执行 `build/qemu/build-qemu-linux all` 产出 `out/qemu/dist/linux/` 后，直接在 Web 控制台或执行 `cdha node deploy-runtime rom-main-runtime` 即可一键打包补齐到 OrangePi。
