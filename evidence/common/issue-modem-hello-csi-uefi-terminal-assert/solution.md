# #294 `vhost-modem-controller` UEFI 阶段主动下发含 `0x9B` 的 `HELLO` 帧触发 EDK2 `TerminalConIn.c(2082)` ASSERT 致启动卡死

- **编号**：#294
- **分组**：`common/`（影响全仓所有配置 `hvc4` Radio 通道且经 EDK2/rEFInd 或 GRUB 引导的云机机型，首发暴露于 `asus/ASUS_AI2205`）
- **日期**：2026-11-21

---

## 一、问题

`asus/ASUS_AI2205`（ROG Phone 7，`lineage-20.0` rEFInd 引导）在用户环境连续两次启动卡死在 UEFI 阶段，完全无法进入 Linux 内核。查看实例 `qemu.log`，在 `BdsDxe: starting Boot0001` 之后立刻死于：

```text
ASSERT [TerminalDxe] /home/kraxel/projects/qemu/roms/edk2/MdeModulePkg/Universal/Console/TerminalDxe/TerminalConIn.c(2082): ((BOOLEAN)(0==1))
```

---

## 二、根因

完整的四步因果链已逐字节实证：

1. **EDK2 `edk2-stable202408` 固件自身状态机缺陷**：读取 `MdeModulePkg/Universal/Console/TerminalDxe/TerminalConIn.c` 源码确认：
   - 第 2093 行遇到 `UnicodeChar == CSI`（`0x009B`）时将 `TerminalDevice->InputState` 设为 `INPUT_STATE_CSI`；
   - 第 1378 行 `switch (TerminalDevice->InputState)` **遗漏了 `case INPUT_STATE_CSI:`**；
   - 因此只要任意挂入 `ConIn` 的串口/虚拟控制台收到字节 `0x9B` 后再收到任意下一字节，立即落入第 2082 行 `default: ASSERT (FALSE);` 使 UEFI 永久停机。
2. **EDK2 `VirtioSerialDxe` 将 `hvc0..hvc7` 全部绑入 `TerminalDxe` `ConIn`**：QEMU 使用 `-device virtconsole` 暴露 `hvc0..hvc7`（含 `hvc4` Radio 通道与 `hvc7` Event 通道），在 rEFInd（`Boot0001`）初始化与轮询按键期间，`TerminalDxe` 会从所有 `hvc0..hvc7` 读取宿主下发字节。
3. **`vhost-modem-controller` 在 UEFI 阶段即开始高频主动发送二进制 `HELLO` 帧**：`qemu/backend/modem-host-controller/component/session.go` 启动后不等 Guest 上线，立即每 250ms 往 `hvc4`（`guest-fd`）发送 64B 头部 + TLV 快照的二进制 `CDRM` `HELLO` 帧，其中 `SessionID = uint64(time.Now().UnixNano())`（8 字节）及动态字段随时可能包含字节 `0x9B`。实测用户首次启动时间 `2026-11-21 12:39:33` 的 `UnixNano()` 为 `0x18e9f59b1aaaf200`，第 4 字节恰好连续 4.3 秒固定为 **`0x9B`**。
4. **`CleanGuestSocketpair` 下行方向无 UEFI 静默保护**：`agent/modules/daemon/backend/ansi_bridge.go` 原先只剥离 `Guest → Backend` 的前导 UEFI ANSI CSI，对 `Backend → Guest` 直接 `io.Copy`，导致含 `0x9B` 的 `HELLO` 帧在 UEFI 阶段直灌入 `hvc4` 并瞬间炸死 `TerminalDxe`。此外，`channel.go` 的 `syncGuestClockAfter(6s)` 在慢启动场景下也存在 UEFI 未退出前向 `hvc7` 写入的风险。

---

## 三、证据

- **失败与复现日志**：[log-fail.txt](log-fail.txt)（包含用户 `12:39:33` / `12:40:52` 实测 `qemu.log`、`UnixNano()` 命中 `0x9B` 十六进制、EDK2 `TerminalConIn.c` 精确行号源码，以及向 `virtconsole` 注入 `0x9B + 'A'` 100% 逐字节复现同一行 ASSERT 的日志）。
- **修复后验证日志**：[log-pass.txt](log-pass.txt)（新版 `host-daemon` 启动 `ASUS_AI2205` 在 `slot 1` 50s 达成 `boot_completed=1`、`SELinux=Enforcing`、0 崩溃，且 UEFI→rEFInd→Linux 内核完整通过）。

---

## 四、改动

1. **`agent/modules/daemon/backend/ansi_bridge.go`**：
   - 新增 `CleanGuestSocketpairGated(name, gate, fallbackDelay)` 与 `forwardBackendAfterGate`：在 `gate` 未关闭且 `fallbackDelay` 未到期且 Guest 尚未发出首个非 ANSI 业务字节前，按完整 `CDRM` 帧边界（64B header + `PayloadLength`）静默消费后端主动下发帧，**保证 UEFI 阶段向 `hvc4` 写入严格为 0 字节**；一旦开门，从下一个完整 `CDRM` 帧起始边界无缝切入 `io.Copy` 零错位直通。
2. **`agent/modules/daemon/backend/adbkey.go`**：
   - 为 `ADBKeyChan` 新增 `Ready() <-chan struct{}`，在收到 Guest `ADBKEY1 READY\n` 时幂等关闭，作为“Guest 已退出 UEFI 进入 Android 用户态”的零延迟放行信号。
3. **`agent/modules/daemon/backend/modem.go` & `agent/modules/daemon/run/qemu.go`**：
   - 新增 `StartModemProcessWithGate(..., guestReady)`（兜底超时 `modemUEFIGateFallback = 12s`），`qemu.go` 传入 `adbKey.Ready()` 作为 `modem-guest` 的 UEFI 静默闸门。
4. **`agent/modules/daemon/run/channel.go`**：
   - 将 `syncGuestClockAfter` 兜底延迟由 `6s` 放宽至 `12s`，避免慢 UEFI 窗口内向 `hvc7` 盲写。
5. **`agent/modules/daemon/backend/ansi_bridge_test.go`**：
   - 新增回归单测 `TestCleanGuestSocketpairGatedBlocksUEFIAndReleasesAligned`，使用实测触发崩溃的 `sessionID = 0x18e9f59b1aaaf200` 及含 `0x9B` 负载验证闸门关闭前 0 字节泄漏、开门后完整 `CDRM` 帧对齐直通。

---

## 五、验证

1. **Go 单测**：
   - `go test -v -count=1 ./modules/daemon/backend -run 'TestStripLeading|TestCleanGuestSocketpair'` → PASS（含新增门控单测）。
   - `go test -count=1 ./modules/daemon/...` → 全部包 PASS（0 FAIL）。
2. **二进制重编与实机冷启动**：
   - 已原子重编 `out/agent/dist/cdha` 与 `out/agent/dist/host-daemon`。
   - 隔离测试环境启动 `asus/ASUS_AI2205`（`slot 1`，`adb=5919`）：50s 达成 `sys.boot_completed=1`、`SELinux=Enforcing`、0 崩溃，详见 [log-pass.txt](log-pass.txt)。

---

## 六、遗留

无。运行中的旧版 `cdha serve` 进程需重启后方可加载新编译的二进制并使该门控生效。
