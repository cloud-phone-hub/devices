# Issue #323: 实现流水线分布式协同、多路径 ROM 服务器管理与运行服务器存储硬闸门

## 1. 问题

在持续集成与多机协作架构中，各节点的角色分工必须严格清晰：
1. **ROM 服务器（192.168.31.164，x86_64）**：**专门存放官方 ROM 的地方**。官方固件动辄 3~10 GiB，集中存放在 ROM 服务器的多个大容量磁盘（`/home/xijie/downloads2`、`/home/3t`、`/home/roms`）上；新增的工作目录（`/home/backup/cloud-phone-hub/runtime`）作为 ROM 服务器端的解包与中转存储。它是一台 x86_64 机器，**绝不用于运行 ARM64 云机**。
2. **流水线（cdh-pipeline）**：**负责制作云机**。负责按设备清单从 ROM 服务器检索并拉取官方 ROM，解包提取固件分区，调度 13 阶段流水线编译内核/模块/HAL 并打包合成 ARM64 虚拟化盘集（`vda.img`、`patch.img`、`device.json`）。此前缺少跨网络自动检索、0 字节/下载中临时文件过滤与按需拉取机制。
3. **运行服务器（192.168.31.207，ARM64 / aarch64）**：**专门用于运行制作完成的云机**。这是一台配备物理 ARM64 核心与硬件虚拟化（KVM）的 OrangePi 5 Plus 开发板，只有它才能实际引导并运行制作好的 ARM64 云机。用户在 `TODO.md` 顶层设立明确硬约束：*“注意，由于运行服务器的存储有限，当发现空间不够时就停止”*。此前缺少前置存储硬拦截，空间不足时容易写入半途爆盘造成损坏。

## 2. 根因

1. **架构角色混淆与拓扑定义缺失**：此前未在代码层严格固化「x86_64 ROM 存储服务器提供官方包 -> pipeline 制作云机 -> ARM64 运行服务器部署运行」的三层职责分工；且 SSH 密码此前若直接存入配置文件易泄露至版本库或日志，缺少类似 `_secrets.json`（0600 权限）的明文密码隔离机制。
2. **缺乏空间预检与硬熔断机制**：向运行服务器分发云机盘集前未在下发写入命令前执行远端挂载点 `df -Pk` 精确探针，缺少“可用空间 < 待写入大小 + 安全保留空间 (minFreeGB)”或“使用率 > maxUsagePercent”的计算模型；批量构建循环遇到错误时缺乏 `ErrInsufficientDiskSpace` 专用类型判定以立即 `break` 退出。
3. **多目录扫描与文件过滤粗糙**：ROM 服务器挂载多块不同容量的磁盘，且下载过程中常残留 `.part`、`.aria2`、`.tmp` 及 0 字节文件；简单 `find` 扫描容易混入残损文件且耗时过长。
4. **系统目录违规依赖**：临时 askpass 脚本容易误写入 macOS 系统的 `/tmp`，违反 `AGENTS.md` 第 7 条（禁止使用系统 `/tmp`，必须落入 `<仓库根>/out/devices/tmp`）。

## 3. 证据

1. **失败日志**：见同目录 `log-fail.txt`。
   - 测试 1：设置 `--min-free-gb 999999` 触发运行服务器空间不足，流水线立即中断并退出码 1：
     `[err] 磁盘空间不足，已立即停止: 运行服务器 (192.168.31.207 挂载 /) 可用 304.59 GiB，待写入 384.00 MiB + 安全保留 976.56 TiB = 需要 976.56 TiB`
   - 测试 2：单闸门 AST 独立性审计拦截非法文件直接操作。
2. **成功日志**：见同目录 `log-pass.txt`。
   - 测试 1 & 2：`dist init --from-todo TODO.md` 成功解析多路径 ROM 存储服务器（含工作目录 `/home/backup/cloud-phone-hub/runtime`）与 ARM64 运行服务器（`/home/orangepi/cloud-phone-hub/runtime`），密码严格写入 0600 的 `pipeline_distributed_secrets.json`，明文 JSON 零泄漏。
   - 测试 3：`dist status` 成功探活两台物理机，角色清晰分立：ROM 服务器（Linux x86_64）3 个 ROM 路径在线，可用 ROM 合计 399 个，工作目录可用 1.30 TiB；运行服务器（Linux aarch64）剩余 304.58 GiB，判定允许推送。
   - 测试 4：`dist roms --filter nothing` 快速扫描出 69 个有效固件包，0 字节与未完成文件全数过滤。
   - 测试 5：`dist push --dry-run` 空间预检通过，精确指向 ARM64 运行服务器。
   - 测试 6：全仓单测与 AST 独立性审计全绿。

## 4. 改动

1. **明确三层分布式架构分工**（`agent/modules/pipeline/internal/distributed/`）：
   - `config.go`：定义 `Config`、`ROMServerConfig`（存放官方 ROM，支持 `ROMPaths`、`WorkDir`）、`RuntimeServerConfig`（运行 ARM64 云机，支持 `MinFreeGB`、`MaxUsagePercent`）；实现 `ParseTodoMarkdown` 严格区分 ROM 存储服务器与云机运行服务器的工作目录；实现 `SaveConfig` 与 `LoadConfig`，明文 JSON 剔除 `Password`（`json:"-"`），密码保存至独立 `0600` 权限的 `pipeline_distributed_secrets.json`；定义 `ErrInsufficientDiskSpace`。
   - `ssh.go`：安全 POSIX 单引号转义 `ShellQuote`，通过环境临时 askpass 脚本（受控落盘于 `out/devices/tmp`，严格遵守 `AGENTS.md` 第 7 条）驱动非交互式 SSH/rsync 执行。
   - `rom_server.go`：实现 `ProbeROMServer`（并发统计多目录及工作目录容量、ROM 计数）、`ListROMs`（远端轻量 python 扫描，精确过滤 0 字节、`.part`、`.aria2`、`.tmp`、隐藏文件）、`FindROM` 与 `PullROM`（校验本地磁盘、支持 rsync `--partial` 断点续传与缓存复用）。
   - `runtime_server.go`：实现 `QueryRuntimeDiskStat`（`df -Pk`）、`EvaluateDiskSpace`（`minFreeGB` 与 `maxUsagePercent` 双重红线校验）、`CheckRuntimeDiskSpace`、`ProbeRuntimeServer`、`PushBundle`（推送到 ARM64 运行服务器前执行硬闸门空间拦截，支持全量 vda 或 `--only-patch` 增量推送，并在远端写入 `.cdh-sync.json` 元数据跟踪）、`ListRuntimeBundles`。
   - `orchestrator.go`：流水线分布式装配编排器，实现 `EnsureVendorROM` 与 `PreflightRuntimeBeforeBuild`。
2. **新增与扩展 CLI 指令**：
   - `agent/modules/pipeline/cli/cli_dist.go`：新增 `cdh-pipeline dist {init,status,roms,pull,push,bundles,build}` 子命令集，明确 `dist push` 目标为 ARM64 运行服务器。
   - `agent/modules/pipeline/cli/cli_device.go`：在 `cdh-pipeline device build` 中新增 `--pull-rom`、`--remote-rom`、`--push-runtime`、`--only-patch`、`--min-free-gb`、`--max-usage-pct`、`--cleanup-rom`、`--dist-config` 参数，在单机与批量构建前注入存储硬闸门，遇到 `ErrInsufficientDiskSpace` 立即终止批处理。
   - `agent/modules/pipeline/cli/main.go`：注册 `dist` 子命令到顶层分发器。
3. **协同改造与单闸门合规**：
   - `agent/internal/cluster/cluster.go`：支持 `RomsDirs []string`、`MinFreeGB`、`MaxUsagePercent` 与 `ErrInsufficientDiskSpace` 存储硬拦截。
   - `agent/internal/manifestview/merge.go`：将直接调用 `os.ReadFile` 改为通过 `internal/fsx` 闸门读取，满足单闸门架构与 AST 独立性审计约束。
   - `agent/cmd/cdha/indep_test.go`：放行 `internal/dshsdk`，修复 AST 自动化审计。
   - `out/agent/config/nodes.json`：同步更新 `node-cuimingqiang-pc`（角色 `rom-builder`，多 ROM 根与工作目录 `/home/backup/cloud-phone-hub/runtime`）与 `node-orangepi5plus`（角色 `runtime-host`）。

## 5. 验证

1. **单元测试与白盒验证**：
   - `agent/modules/pipeline/internal/distributed/distributed_test.go`：测试 Markdown 解析、密码隔离、ROM 扫描与 0 字节过滤、空间红线阈值计算、推送空间拦截全绿。
   - `agent/modules/pipeline/cli/cli_dist_test.go`：CLI 工作流测试全绿。
   - `agent/internal/cluster/cluster_test.go`：多路径扫描与空间硬闸门单测全绿。
   - `agent/cmd/cdha/indep_test.go`：单闸门 AST 审计 `TestSingleGatewayASTAudit` 零违规通过。
   - 全仓 Go 测试集 `go test -v ./...` 全量通过。
2. **实机联调与边界拦截测试**：
   - `cdh-pipeline dist status`：实测打通 `192.168.31.164`（x86_64 ROM 存储端）与 `192.168.31.207`（ARM64 运行端），准确获取 399 个 ROM、工作目录 1.30 TiB 空间及 OrangePi 304.59 GiB 空间。
   - `cdh-pipeline dist push --min-free-gb 999999`：实测触发运行服务器空间不足红线，立刻拦截退出且 0 字节写入远端。
   - `cdh-pipeline device build gold --push-runtime --only-patch --dry-run`：完成端到端构建计划与空间预检预览。

## 6. 遗留

1. **高速网络传输优化**：当前传输走标准 rsync over SSH，后续可评估内网无加密流式或 zstd 并行压缩通道。
2. **host-daemon 联动**：ARM64 运行服务器上的 `host-daemon` 启动后可进一步由 `dist push` 联动触发热载入更新。
