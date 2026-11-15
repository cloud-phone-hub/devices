# 云机 ADB 恒 offline：hvc2 ADBKEY1 握手被误删

## 1. 问题

所有实例（新旧镜像都一样）在宿主侧 `adb devices` 里恒为
`127.0.0.1:<ADBPort>  offline`，`adb connect` 报
`failed to connect to 127.0.0.1:5919`，无法 shell / install。

- 实例 1（`gold_cn_OS3.0.10.0.VNQCNXM/1`，镜像
  `out/device/gold-15/15/os3.0.10.0.vnqcnxm`）从 09:08 起跑，adb 一直 offline。
- 实例 3（新镜像）同样 offline，说明与镜像新旧无关。

## 2. 根因

ADB 链路是「guest-daemon `adbkey.rs` 在 `/dev/hvc2` 发 `ADBKEY1 READY` →
宿主下发 ADB 公钥 → guest 原子写 `/data/misc/adb/adb_keys` → fork APEX adbd
监听 TCP 5555 → 宿主经 net0 hostfwd `<ADBPort>`→5555 连上」。

daemon 在 `945dbac refactor(daemon): 用 stage 实现整体替换旧 daemon 代码`
里删除了 `backend/adbkey.go`（hvc2 握手）与 `backend/adb.go`（hvc8 自研桥），
并把 hvc2/hvc8 改成 `-chardev null` 占位（hvc.md 当时记为「旧 ADBKEY1 握手已
移除」）。但**客机镜像里的 adbkey 进程没有删**：它照旧在 hvc2 发 READY 并
阻塞等公钥。hvc2 是 null，READY 进黑洞，公钥永远拿不到 → adbd 永不 fork →
5555 无监听 → 宿主 adb 停在 offline。

`git log -S "ADBKEY1"` / `-S "adbkey"` 定位到删除提交；`945dbac^:backend/adbkey.go`
保留完整旧实现，`fa601c3:run/run.go` 保留旧接线（`ADBKeyOpen → BindADBKey →
go Serve(readADBPublicKey())`）。

## 3. 证据

- 失败现场（`log-fail.txt`，实例 1 重启前 `qemu-20261027-104329.log`）：
  ```
  [    7.287427][  T459] adbkey: raw 模式已设置
  [    7.287486][  T459] adbkey: 已发 READY，等待宿主公钥
  ```
  之后无任何公钥写入 / adbd 拉起记录；`adb devices` 只有 offline。
- 对照：同一镜像在旧 daemon（`fa601c3` 时代）能正常 adb，说明镜像侧无问题。
- 修复后（`log-pass.txt`）：
  ```
  adbkey: 公钥已写入 adb_keys，OK
  adbd: 子进程 pid=458 已拉起（/apex/com.android.adbd/bin/adbd）
  127.0.0.1:5919  device
  ```

## 4. 改动

daemon 仓（`core/host/daemon`）：

1. 新增 `backend/adbkey.go`：从 `945dbac^` 找回并适配新包结构——
   `ADBKeyOpen()`（socketpair）、`Bind(args, extra)`（fd 占位符
   `STAGE_ADBKEY_GUEST_FD` → ExtraFiles 索引）、`Serve(publicKey)`
   （`ADBKEY1 READY` → 下发帧 → 等 `ADBKEY1 OK`，循环）、`Close/CloseGuest`。
2. `run/qemu.go`：
   - `virtconArgs` 中 hvc2 固定接 `-chardev socket,id=stage-adbkey,fd=...`
     + `virtconsole,id=stage-hvc2-adbkey`（不依赖 device.json 的
     `host-hvc-adbkey`，槽位常量 `adbKeyHVCPort = 2`）；
   - `launchQemu` 在 buildQemuArgs 之后 `ADBKeyOpen → Bind → go Serve(readADBPublicKey(opt))`，
     startQemu 后 `CloseGuest()` 关父进程副本；
   - 新增 `readADBPublicKey(opt)`：`-adb-key` > `$HOME/.android/adbkey.pub`，
     读不到时打印 warning（客机 adbd 不会启动）。
3. `run/run.go`：`Options.ADBKeyPath` + flag `-adb-key`。
4. 文档：`hvc.md`（hvc2 行改为 adbkey 握手、各机型表 hvc2 列填 adbkey）、
   `README.md` 参数表补 `-adb-key`。

## 5. 验证

实例 1（旧镜像 `out/device/gold-15/15/os3.0.10.0.vnqcnxm`）用
`POST /api/stop` + `POST /api/start` 重启后：

- QEMU 命令行出现 `-chardev socket,id=stage-adbkey,fd=3` 与
  `-device virtconsole,chardev=stage-adbkey,id=stage-hvc2-adbkey`；
- guest 日志：`adbkey: 公钥已写入 adb_keys，OK` →
  `adbd: 子进程 pid=458 已拉起`；
- 宿主：`adb connect 127.0.0.1:5919` 成功，`adb devices` 显示
  `127.0.0.1:5919  device`；`adb shell` 返回 `sys.boot_completed=1`、
  `uid=2000(shell)`、`ro.build.version.release=15`；
- 旧镜像口径不变：`mount` 中 `/dev/block/vdc on /mnt/vendor type ext4`。

## 6. 遗留

- `-adb-key` 未指定且 `$HOME/.android/adbkey.pub` 缺失时 adbd 仍不会启动，
  只有 stderr warning；是否需要自动生成密钥对，待定。
- hvc2 现在恒接握手（不按 device.json 映射）；若未来某个客机镜像没有
  adbkey 进程，host 侧 `Serve` 会阻塞到进程退出，无副作用但占一个 fd。
- 实例 1 之外仍在运行的旧实例（如实例 3）需各自重启一次才会接上 hvc2。
