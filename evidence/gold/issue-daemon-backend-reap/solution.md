# daemon 未回收相对路径启动的宿主后端（双重 wifi 网关）

实例：gold-15 / `gold_cn_OS3.0.10.0.VNQCNXM` / index 5
日期：2026-10-28

## 1. 问题

daemon 重启后，实例 5 重新拉起时数据面上同时存在**两个** `wifi-host-service`：

- 旧进程 92706：当日为验证 WiFi 修复，在仓库根以**相对路径**手工启动；
- 新进程（daemon 拉起）：由 `startInstance` 正常拉起。

两个网关各自持有同一批 guest TCP 流的一半状态，宿主日志出现成对、反复刷新的
流控停顿；guest 侧 HTTP 请求停在 SYN_SENT / 只收到部分响应，表现为「WiFi 又坏了」。

## 2. 根因

`core/host/daemon/run/reap.go` 的 `isBackendCommand` 只用 `argv[0]` 与 QEMU 发布根
做字符串前缀比较：

```go
exe := fields[0]
if !strings.HasPrefix(exe, qemuRoot+sep) {
    return false
}
```

手工以相对路径（`./out/qemu/dist/darwin/libexec/wifi-host-service` 或
`out/qemu/dist/darwin/libexec/wifi-host-service`）启动时，`ps` 的 command 列是相对
路径，不以 `qemuRoot` 开头 → 判定为「非后端」→ `listBackendProcesses` 扫不到它 →
`stopInstanceBackends` / `reapOrphanBackends` 都不会回收 → 旧网关一直占着
`/tmp/stage-007d12b0/wifi-data.sock`，与新实例的网关同时挂在数据面。

该进程也不在实例目录的 `backend-pids` 清单里（清单只记录 daemon 自己拉起的 pid），
两条兜底同时失效。

## 3. 证据

### 3.1 双重网关期间宿主日志（`runtime/wifi/wifi-host-service.log`）

```
2026/10/28 04:09:07 [tcp] 流控停顿 115.231.33.2:80 inflight=66412 limit=65535 win=65535 mySeq=3514816603 lastAck=3514750191
2026/10/28 04:09:07 [tcp] 流控停顿 115.231.33.2:80 inflight=66124 limit=65535 win=65535 mySeq=4152920794 lastAck=4152854670
2026/10/28 04:09:08 [stat] rx=1831(+23) tx=1895 drop=0 reset=0 队列=0/1024
2026/10/28 04:09:18 [tcp] 流控停顿 115.231.33.2:80 inflight=66412 limit=65535 win=65535 mySeq=3514816603 lastAck=3514750191
2026/10/28 04:09:18 [tcp] 流控停顿 115.231.33.2:80 inflight=66124 limit=65535 win=65535 mySeq=4152920794 lastAck=4152854670
2026/10/28 04:09:33 [tcp] 流控停顿 115.231.33.2:80 inflight=66412 limit=65535 win=65535 mySeq=3514816603 lastAck=3514750191
2026/10/28 04:09:38 [tcp] 流控停顿 115.231.33.2:80 inflight=66124 limit=65535 win=65535 mySeq=4152920794 lastAck=4152854670
2026/10/28 04:09:38 [stat] rx=1832(+1) tx=1895 drop=0 reset=0 队列=0/1024
```

两条流**成对**出现、ISN 不同，且 `inflight` 钉死在 64KB、`drop=0`：不是链路丢帧，
是两份网关状态互相打架后 guest 无法确认。

### 3.2 现场判定

- 发现时 `ps` 同时列出两个 `wifi-host-service`：旧 92706（当日为验证 WiFi 修复，
  在仓库根以相对路径手工启动）与 daemon 拉起的另一份；旧进程不在实例目录的
  `backend-pids` 清单里，进程表扫描也没命中它。
- 修复前的判定逻辑在相对路径下必然返回 false（`strings.HasPrefix("out/...",
  "/Volumes/.../out/qemu/dist/darwin")` 为假），故两条兜底同时失效。
- 处置：先 `kill -TERM 92706` 清掉旧网关，修代码、重编译 daemon 后重启 daemon。

## 4. 改动

`core/host/daemon/run/reap.go`：

```go
func isBackendCommand(pid int, command, qemuRoot string) bool {
    fields := strings.Fields(command)
    if len(fields) == 0 {
        return false
    }
    exe := filepath.Clean(fields[0])
    if !filepath.IsAbs(exe) {
        cwd := processCwd(pid)       // lsof -a -d cwd -p <pid> -Fn
        if cwd == "" {
            return false
        }
        exe = filepath.Join(cwd, exe)
    }
    if !strings.HasPrefix(exe, qemuRoot+string(filepath.Separator)) {
        return false
    }
    return backendBinaries[filepath.Base(exe)]
}
```

`listBackendProcesses` 调用点补传 `pid`。`go build ./...`、`go vet ./run/` 通过；
`./build/core/host-main build` 重编译 daemon：

```
daemon_sha=a7cc830a2666160d3f5c4248401da7247c0161b179dbd8cb35256b546bdbe53e
```

重启 daemon（`nohup ./out/core/daemon/dist/darwin/daemon >> out/core/daemon/daemon-console.log 2>&1 &`）。

## 5. 验证

见 `log-pass.txt`：

1. 以相对路径手工启动的真实网关进程（`ps` 的 argv[0] 为相对路径）被
   `isBackendCommand` 识别为 true；绝对路径 true、无关命令行 false、发布根外 false。
2. daemon 重启后启动实例 5，`pgrep -fl wifi-host-service | wc -l` = **1**（进程
   15087，由 daemon 拉起）。
3. 重启后宿主日志 `流控停顿` 0 条、`非法以太帧` 0 条、`drop=0`。
4. 停实例时网关进程被回收，`pgrep` 计数归 0（实例 5 随后重新启动）。

## 6. 遗留

1. `processCwd` 依赖 `lsof`；宿主没有 `lsof` 时相对路径 argv[0] 仍会漏扫。
2. 相对路径识别逻辑目前没有常驻单测（需要可控 cwd 的子进程），本次以临时测试
   实证后删除；若后续再改扫描逻辑，应补一个 `TestMain` 里 `chdir` + 子进程的用例。
3. `backend-pids` 清单仍只覆盖 daemon 自己拉起的进程；手工启动的后端只能靠进程表
   扫描兜底（本次修复的正是这条路径）。
4. `reapOrphanBackends` 对「PPID=1 且 cwd 已删除」的进程直接回收的判定未改。
