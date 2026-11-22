# #300：停止实例时同步回收全部 QEMU 宿主后端与独立进程组子进程（`virgl_render_server` 等）

- **设备/机型**：全仓所有机型（`agent/modules/daemon` 宿主生命周期管理）
- **日期**：2026-11-22
- **状态**：已闭环

## 一、问题

调用 `POST /api/stop` 或 `host-daemon stop` 停止云机实例后，宿主机进程表中仍残留部分 QEMU 后端程序未同步退出：
1. 宿主机实测残留 9 个 `virgl_render_server --socket-fd N`（`PPID=1`）及此前测试异常中断留下的 `wifi-host-service`、`netsimd` 孤儿进程；
2. 部分实例停机返回 `200 OK` 时，进程内后端与慢退子进程尚未彻底退出，存在与异步 `onExit` 的竞态。

## 二、根因

排查代码与运行态进程树，一次性定位 5 处叠加工况：

1. **`virgl_render_server` 自立进程组且无视 `SIGTERM`**：QEMU (`virtio-gpu-gl-pci,venus=on`) 按需拉起 `libexec/virgl_render_server`，其启动后自建独立 `PGID`（实测 `qemu PID=13625 PGID=13625`，`virgl_render_server PID=13628 PPID=13625 PGID=13628`）。`StopInstance` 仅向 `-qemuPid` 发信号完全打不到它；同时它不在 `hosts.go:backendBinarySet` 白名单中，且实测无视 `SIGTERM`（必须 `SIGKILL`），导致停机后大量残留占用 Metal/GPU 与 `/private/tmp/mesa-shared-*`。
2. **`StopInstance` 未同步等待本进程 `activeHosts.stop()`**：`hostRuntime.stop()` 原无互斥锁，且 `StopInstance` 仅在 `defer` 跑 `stopInstanceBackends`、把 `hosts.stop()` 留给异步 `onExit` goroutine，导致停机返回时进程内后端（`camera`/`nfc`/`haptic`/`adbkey`/`bthci`）与子进程尚未同步停完，且两边竞争删除 `backend-pids`。
3. **`ModemProcess` 未关闭父进程持有的 `qemuGuest`/`qemuControl` dup FD**：`backend/modem.go:claimQEMUFiles()` 调用 `unixConn.File()` 产生 dup FD 传给 QEMU，却既未存进 `process.qemuControl` 也未实现 `CloseGuest()`，导致 `hosts.sealGuests()` 对其跳过，父进程长期握住 `supervision` dup FD，使 `vhost-modem-controller` 停机时收不到 EOF 只能干等 2s 超时被杀。
4. **信号与路径匹配盲区**：`WiFiProcess.Stop()` / `ModemDataProcess.Stop()` 发送 `os.Interrupt` 而非标准 `syscall.SIGTERM`；`reap.go` 的 `cwdUnderDir` 未做 `EvalSymlinks` 规范化，且 `consoleMatches` 仅限 `PPID==1` 忽略 `PPID==os.Getpid()`；`terminatePid` 串行逐个等 2s 效率低。
5. **QEMU 自退出路径漏扫子进程**：`qemu.go` 的 `onExit` 未在 `hosts.stop()` 后调用 `dropChannelFor` 与 `stopInstanceBackends`。

## 三、证据

详见同目录 [log-fail.txt](log-fail.txt) 与 [log-pass.txt](log-pass.txt)。

## 四、改动

1. [hosts.go](file:///Volumes/work/cloud-phone-hub/agent/modules/daemon/run/hosts.go)：新增 `virglRenderServerBinary = "virgl_render_server"` 并纳入 `backendBinarySet`；为 `hostRuntime` 加 `sync.Mutex`，保证 `stop()` 严格互斥且幂等。
2. [peripherals.go](file:///Volumes/work/cloud-phone-hub/agent/modules/daemon/run/peripherals.go)：统一以 `canonicalPath` 为键，新增 `stopActiveHosts(instDir)` 原子摘除并同步停止该实例全部进程内与进程外后端。
3. [reap.go](file:///Volumes/work/cloud-phone-hub/agent/modules/daemon/run/reap.go)：新增 `listDescendantPids` / `descendantPidsFromTable`（停机前快照 QEMU 全量直接/间接子进程）、`canonicalPath`（解决 macOS 符号链接路径偏差）、并发批量终止 `terminatePids`（统一 `SIGTERM` → 最多 1s → `SIGKILL` 并确认消失，彻底处理无视 `SIGTERM` 的 `virgl_render_server` 与 `netsimd --no-shutdown`）及 `stopInstanceBackendsWithExtra`（`consoleMatches` 覆盖 `PPID==1 || PPID==os.Getpid()`）。
4. [manage.go](file:///Volumes/work/cloud-phone-hub/agent/modules/daemon/run/manage.go) & [qemu.go](file:///Volumes/work/cloud-phone-hub/agent/modules/daemon/run/qemu.go)：`StopInstance` 发信号前先快照 `qemuChildren := listDescendantPids(inst.Pid)`，`defer` 按 `dropChannelFor -> stopActiveHosts -> stopInstanceBackendsWithExtra` 同步清扫；`SIGKILL` 后等待 QEMU 彻底消失；`onExit` 同步补充 `dropChannelFor` 与 `stopInstanceBackends`。
5. [modem.go](file:///Volumes/work/cloud-phone-hub/agent/modules/daemon/backend/modem.go)、[wifi.go](file:///Volumes/work/cloud-phone-hub/agent/modules/daemon/backend/wifi.go)、[modem_data.go](file:///Volumes/work/cloud-phone-hub/agent/modules/daemon/backend/modem_data.go)：`ModemProcess` 保存 `qemuControl`、实现 `CloseGuest()` 满足 `backend.GuestSealer` 契约并在 `Stop()` 发 `SIGTERM`；`WiFiProcess` / `ModemDataProcess` 改发 `syscall.SIGTERM`（1.5s 超时 `Kill()`）。
6. [hosts_test.go](file:///Volumes/work/cloud-phone-hub/agent/modules/daemon/run/hosts_test.go)：补齐 `GuestSealer` 编译期断言、更新 `virgl_render_server` 白名单测试，新增 `TestDescendantPidsFromTable` 与 `TestStopActiveHostsIdempotent`；重编 `out/agent/dist/{host-daemon,cdha}`。

## 五、验证

1. **全量 Go `-race` 竞态测试**：`cd agent && go test -race -count=1 ./modules/daemon/...` 全部 `PASS`。
2. **真机端到端「启动 → 停止 → 零秒复查」实测**：
   - 使用新版 `host-daemon` 启动 `google/oriole` A15，停机前确认 9 个进程全部在线（`secure_env`、`wifi-host-service`、`modem-data-gateway`、`netsimd`、`vhost-device-media` ×2、`vhost-modem-controller`、`qemu-system-aarch64` 以及独立 `PGID=15018` 的 `virgl_render_server`）；
   - 调用 `POST /api/stop`，**0.56 秒**返回 `ok: true`；
   - 返回瞬间（不 sleep）立即扫描全部进程与 `cwd`：**残留进程 = 0**。
3. 已清理宿主机遗留的 11 个临时测试目录孤儿进程（不动用户原有 `cdha serve` 进程及运行实例）。

## 六、遗留

- 宿主机上在本次重编之前已经启动的常驻 `cdha serve` 进程（若仍运行旧二进制）需重启一次 `cdha serve` 以加载本次修复后的停机逻辑。
