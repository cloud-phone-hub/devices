# cmd / input / notification 全线 "Failed transaction"：system_server 对 pioneer unix socket 缺 fd 传递授权

- 设备：xiaomi/gold（Redmi Note 13 5G），engine 构建（lineage-22.2 基底 + 自研 HAL）
- 日期：2026-10-27
- 状态：**已修复并验证**（实例 index 4，adb 5911，boot 50 s）
- 产物：`engine/out/dist/xiaomi/gold/OS3.0.10.0.VNQCNXM/patch.img`（sha256 `cf4ebcde6430`）
- 失败态：`log-fail.txt`（实例 index 3，adb 5911）
- 通过态：`log-pass.txt`（实例 index 4，adb 5911）
- 设备侧原件（同一份）：`devices/xiaomi/gold-15/build/doc/evidence/issue-cmd-binder-transfer-fd/`
- 承接：`issue-boot-to-launcher/`（那里补的是 `binder (call transfer)`，本目录补的是传 fd 时的 `unix_stream_socket` 段）

## 1. 问题

engine 镜像上，凡是走 `cmd` 的命令全部失败，返回码 2：

```
$ adb shell "input keyevent 3; echo rc=$?"
cmd: Failure calling service input: Failed transaction (2147483646)
rc=2

$ adb shell "cmd notification post -S bigtext -t t tag1 hello; echo rc=$?"
cmd: Failure calling service notification: Failed transaction (2147483646)
rc=2

$ adb shell "settings get system screen_brightness; echo rc=$?"
cmd: Failure calling service settings: Failed transaction (2147483646)
rc=2

$ adb shell "am get-current-user; echo rc=$?"
cmd: Failure calling service activity: Failed transaction (2147483646)
rc=2
```

Android 13+ 的 `input` / `settings` / `am` / `pm` / `notification` 都是
`/system/bin/cmd` 的软链，所以这是**同一根因**的表现，不是四个独立问题。

对照事实（同一时刻、同一 shell）：

| 通道 | 结果 |
|---|---|
| `cmd <任意服务> <任意子命令>` | **失败**，`Failed transaction (2147483646)` |
| `dumpsys <任意服务>` | 正常（rc=0，有输出） |
| `service list` / `service call` | 正常 |
| 实例 1（gold-15 原厂 ROM）的 `cmd` | **正常**（rc=0） |

## 2. 根因

`SHELL_COMMAND_TRANSACTION` 会把调用方的 **stdin/stdout/stderr 三个 fd** 一起
传给 system_server；`cmd` 的 stdio 是 **adbd（pioneer 域）建的 unix socket**。
内核在 `binder_translate_fd()` 里对每个 fd 调 `security_binder_transfer_file()`，
该 hook 做**两段** SELinux 检查：

```c
// kernel security/selinux/hooks.c:2071-2091
if (sid != fsec->sid) {                       // 第一段
        rc = avc_has_perm(sid, fsec->sid, SECCLASS_FD, FD__USE, &ad);
        if (rc) return rc;
}
...
isec = backing_inode_security(dentry);        // 第二段
return avc_has_perm(sid, isec->sid, isec->sclass, file_to_av(file), &ad);
```

- `sid` = 目标进程（**system_server**，enforcing）的 SID，不是调用方；
- 第二段的 tclass 取自 fd 所在 inode 的 `sclass`：**unix socket 是
  `unix_stream_socket`，不是 `file`**。

engine 的机型策略此前只补了第一段（`(allow system_server_202404 pioneer (fd (use)))`）
和 dumpsys 用的 fifo 段，**第二段的 `unix_stream_socket` 一直缺**，于是：

```
security_binder_transfer_file()  -> -EACCES
binder_translate_fd()            -> ret = -EPERM      (binder.c:2565)
binder_transaction()             -> return_error = BR_FAILED_REPLY
IPCThreadState                   -> FAILED_TRANSACTION
cmd.cpp:243                      -> "Failed transaction (2147483646)"
```

`dumpsys` 之所以正常：它用 `pipe()` 把输出端传过去（`dumpsys.cpp:425`
`pipe(sfd)`），走的是 `fifo_file` 段（已授权），不碰 socket。

## 3. 证据

### 3.1 错误码换算（三个常量全部对得上）

```
BR_*: 29201 = 0x7211 = _IO('r', 17) = BR_FAILED_REPLY        （kernel uapi binder.h）
code 1598246212 = 0x5F434D44 = ('_'<<24)|('C'<<16)|('M'<<8)|'D' = SHELL_COMMAND_TRANSACTION
2147483646 = -(-2147483646) = -(UNKNOWN_ERROR + 2) = -FAILED_TRANSACTION
error: -1 = -EPERM
```

### 3.2 libbinder 侧原始日志

```
E/IPCThreadState(31912): Binder transaction failure. id: 625689, BR_*: 29201, error: -1 (Operation not permitted)
W/BpBinder(31912): Large or Failed outgoing transaction of 164 bytes, interface descriptor , code 1598246212
W/cmd     (31912): Failure calling service input: Failed transaction (2147483646)
```

`IPCThreadState.cpp:1692` 用 `BINDER_GET_EXTENDED_ERROR` 取到
`{id=625689, command=BR_FAILED_REPLY, param=-EPERM}`，即内核明确告诉我们
"这是 `binder_transaction` 里某个返回 `-EPERM` 的分支"。

### 3.3 缩小到 fd 传递（重定向矩阵）

| # | 命令 | stdin | stdout | stderr | 结果 |
|---|---|---|---|---|---|
| 1 | `cmd audio ...` | socket | socket | socket | 失败 |
| 2 | `... 0</dev/null` | null | socket | socket | 失败 |
| 3 | `... 1>/dev/null` | socket | null | socket | 失败 |
| 4 | `... 2>/dev/null` | socket | socket | null | 失败 |
| 5 | `... 0</dev/null 1>/dev/null 2>/dev/null` | null | null | null | **成功 rc=0** |
| 6 | `adb shell -t "cmd audio ..."` | pty | pty | pty | **成功 rc=0** |
| 7 | `dumpsys audio` | socket | socket | null | **成功 rc=0** |

结论：**stdio 里只要有一个是 adb 的 unix socket 就失败，全换成 /dev/null 或 pty
就成功**；`dumpsys` 用 pipe 也不受影响。指向 "socket 这类 fd 的第二段检查"。

### 3.4 内核 `-EPERM` 的唯一来源链

`drivers/android/binder.c` 里 `return_error_param = -EPERM` 只有两处：
`security_binder_transaction()` 失败（3365）与 `binder_translate_fd()` 里
`security_binder_transfer_file()` 失败后硬编码（2565）。前者可排除：

- `pioneer` 是 permissive 域（dmesg 里 `scontext=u:r:pioneer:s0 ... permissive=1`），
  以它为主体的检查一律放行；
- 更直接的反证：`dumpsys` / `service list` 走同一个 `binder_transaction` 到同一个
  system_server，全部成功。

所以只能是后者——而它的主体正是 enforcing 的 system_server。

### 3.5 dmesg 为什么没有 avc 记录（重要陷阱）

`binder_user_error()` 有一次性的 `binder_stop_on_user_error = 2` 抑制：
boot 早期（17.2 s，pid 655）出现过两条 `transaction credentials failed` 之后，
**后续所有 binder 用户态错误都不再打印**。所以"dmesg 没有 avc"不能反证
"不是 SELinux"，必须去策略文件里比对。

### 3.6 与 gold-15 原厂策略的逐条 diff

```
$ grep -o "(allow [a-z_0-9]* pioneer ([a-z_]* ([a-z_ ]*)))" v1.cil | sort -u > p1.txt   # 实例 1
$ grep -o "(allow [a-z_0-9]* pioneer ([a-z_]* ([a-z_ ]*)))" v3.cil | sort -u > p3.txt   # 实例 3
$ diff p1.txt p3.txt
< (allow system_server_202404 pioneer (unix_stream_socket (getattr getopt ioctl read setopt shutdown write)))
```

正是这一条。实例 1 有 → `cmd` 正常；实例 3 缺 → 全灭。

## 4. 改动

文件：`engine/vendor/hardware/android.service.daemon/sepolicy/vendor_sepolicy.cil`
（追加 13 行，其中 1 条 allow + 12 行注释）

```cil
(allow system_server_{{vers}} pioneer (unix_stream_socket (getattr getopt ioctl read setopt shutdown write)))
```

`{{vers}}` 由构建期替换为 `plat_sepolicy_vers.txt` 的值（本机 202404）。

构建（改的是 vendor 内容，必须重打 patch，否则被 patch.img 覆盖）：

```
cd engine && ./out/bin/cdh device build gold --from 07_build_vendor --refresh
# 5 阶段：build_vendor → build_vendor_dlkm → build_patch → pack_super → pack_vda
# patch.img sha256 cf4ebcde6430
```

注入核对：

```
装配树 out/devices/.../vendor/etc/selinux/vendor_sepolicy.cil  223537 B
  第 2122 行 = (allow system_server_202404 pioneer (unix_stream_socket (...)))
patch.img 内同文件 debugfs stat Size: 223537   ← 一致
```

## 5. 验证

新实例（index 4，pid 16253，boot 50 s）：

| 项 | 结果 |
|---|---|
| `cmd audio get-surround-format-supported` | **rc=0** |
| `input keyevent 3` | **rc=0** |
| `cmd notification post ...` | **rc=0**，打印 `posting: Notification(...)` |
| `settings get system screen_brightness` | **rc=0**，输出 `23` |
| `am get-current-user` | **rc=0**，输出 `0` |
| 链式 `input && settings && am && cmd notification` | **ALL_OK** |
| `dumpsys audio` 的 `Devices:` | `speaker(2)`（audio 修复未回归） |
| `listAudioPorts error` | 0 |
| `persist.system.powerhal.applist_enable` | 0 |
| `debug.sf.vsync_reactor_ignore_present_fences` | true |
| `dmesg` 里 `tcontext=u:object_r:pioneer` 的新 avc | 0 条 |

原始输出见同目录 `log-pass.txt`，修复前见 `log-fail.txt`。

## 6. 遗留

- 本修复只覆盖 **adb shell 场景**。若有其他进程把自己的 unix socket 作为
  `ParcelFileDescriptor` 传给 system_server，同样会命中这条检查；目前未发现第二例。
- `stat -Lc %C /proc/self/fd/N` 对 socket 返回值不可靠（会显示进程域或默认类型），
  判断 fd 类别应以"实验矩阵 + 策略 diff"为准，不要只看 stat。

## 排查方法沉淀

1. **`Failed transaction (2147483646)` = `FAILED_TRANSACTION`**，即内核对
   `BR_FAILED_REPLY`，与 `-28 ENOSPC`、"客户端超时"是不同层次的问题，先算常量。
2. **`BR_*: 29201` 是 `_IO('r',17)`**，不是随机数——kernel 的 `BR_*` 全是
   `_IO/_IOR/_IOW` 编码后的值，要用 `uapi/linux/android/binder.h` 反查。
3. **先分清哪条通道坏**：`dump` 用 pipe、`shellCommand` 用 socket，
   一条通一条不通就说明问题在 fd 类型而不是 binder 本身。
4. **permissive 位只管"以该域为主体"的检查**。`security_binder_transfer_file`
   的主体是**目标进程**，所以 pioneer 再 permissive 也救不了 system_server 侧的拒绝。
5. **dmesg 无 avc 不等于不是 SELinux**：`binder_user_error` 触发过一次后会
   自我抑制，必须去比对策略文件。
6. **改 vendor 必重打 patch**：只跑 `07_build_vendor` 不会更新 `patch.img`。
