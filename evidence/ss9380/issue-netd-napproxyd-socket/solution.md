# 卡点：netd 建不出 napproxyd socket，整组 updatable 组件被杀，zygote 5 s 一轮

- 机型：samsung / Galaxy S25 Ultra（SM-S9380，`ss9380`，国行 CHC）
- 系统：官方 OneUI Android 16，`samsung/pa3qzcx/pa3q:16/BP4A.251205.006/S9380ZCSDCZH1`
- vendor 基底：lineage 23.2（`roms/lineage/UTM-VM-lineage-23.2-...`），官方 super 零改动 + patch.img 三子树
- 盘集：build6 → build7（`patch.img` sha256 `3ac3572d9bbf` → `0071f17bbfaf`）
- 时间：2026-10-30

## 1. 问题

health HAL 换件之后（见 `issue-health-hal-samsung-isehhealth`），BatteryService 的 NPE 消失，
但 `sys.boot_completed` 仍不置位，`init.svc.zygote` 每 5 s 重启一次。

串口日志里能看到的只是下游症状：

```
init: Service 'zygote' (pid 1024) received SIGKILL
init: process with updatable components 'netd' exited 4 times before boot completed
```

即 init 不是因为 zygote 自己崩才重启它，而是因为 **netd 反复退出**，按 `netd.rc` 的
`onrestart restart zygote` 把整组一起带走（zygote/audioserver/media/cameraserver/wificond）。

## 2. 根因

两件事叠在一起：

1. **netd 起不来**。OneUI 的 `/system/etc/init/netd.rc` 里除 AOSP 的 `dnsproxyd`/`mdns`/`fwmarkd`
   之外还多一条三星 Knox NPA 的控制 socket：
   `socket napproxyd stream 0660 root inet`。
   init 建它时 `bind(2)` 直接 `EACCES`：

   ```
   init: Could not create socket 'napproxyd': Failed to bind socket 'napproxyd': Permission denied
   ```

   设备上 `/dev/socket` 里因此**只有** `dnsproxyd`/`mdns`/`fwmarkd`，没有 `napproxyd`。
   netd 启动链里拿不到这个 fd，`Netd` 打印 `Unable to start NapProxyListener (Success)`
   （errno 没被设置，所以 strerror 是 "Success"）后 300 ms 内 `exit(1)`。

2. **为什么 bind 被拒**。官方把 socket 文件的标签放在 **vendor** 的 file_contexts：

   ```
   /dev/socket/napproxyd   u:object_r:napproxyd_socket:s0      （官方第 1874 行）
   ```

   而本机 vendor 是 lineage 基底，这条上下文与 `napproxyd_socket` 类型定义都不存在，
   init 的 `setfscreatecon()` 拿不到可用上下文 → 内核 `may_create(dir, dentry, sock_file)`
   拒绝。

   关键细节（第一版修复只补了一半，实测仍失败）：`may_create` 的 **subject 是父目录
   `/dev/socket` 的 sid**，即 `socket_device`，object 才是 fscreate 上下文。OneUI 的 plat 里
   对 `socket_device` 没有任何直指某个 socket 类型的 allow，官方能建出 `napproxyd` 靠的是
   `napproxyd_socket` 属于 `file_type` 属性 + 通用授权。所以必须同时补：

   - `(typeattributeset file_type (napproxyd_socket))`
   - `(allow socket_device napproxyd_socket (sock_file (create)))`

   漏掉这两条时的现象很容易误判：`restorecon -v` 能打印
   `Relabeling ... to u:object_r:napproxyd_socket:s0`（说明 file_contexts 命中了），
   但 `bind` 仍然 EACCES，而且 `dmesg | grep comm=".init."` **看不到 init 的 avc**，
   因为被拒的是 dir→file 这一对，与 init 域无关。

## 3. 证据

失败态（build6 盘集，实例 1）：`log-fail.txt`
- `init: Could not create socket 'napproxyd': Failed to bind socket 'napproxyd': Permission denied`（dmesg）
- `E Netd : Unable to start NapProxyListener (Success)`（清 buffer 后实时 logcat + `setprop ctl.restart netd`）
- `init: Service 'netd' (pid X) exited with status 1` → `exited 4 times before boot completed`
- `ls /dev/socket` 缺 `napproxyd`/`napproxy*`
- `restorecon -v /dev/socket/napproxyd` 能查到标签但 `Could not set context ... Permission denied`

官方对照（`vendor_a.img` 抽取）：
- `/etc/selinux/vendor_file_contexts` 第 1874 行：`/dev/socket/napproxyd u:object_r:napproxyd_socket:s0`
- `/etc/selinux/vendor_sepolicy.cil`：`napproxyd_socket` 出现 5 处
  （1403 `file_type` / 1426 `mlstrustedobject` / 2698 `type` / 2699 `roletype` / 14858 `allow netdomain ... write`）
- `plat_sepolicy.cil` 里 `napproxyd` 出现 **0** 次（不与 plat 冲突）

通过态（build7 盘集，同一实例）：`log-pass.txt`
- `init.svc.netd = running`、`init.svc.zygote = running`
- `srw-rw---- 1 root inet u:object_r:napproxyd_socket:s0 /dev/socket/napproxyd`

## 4. 改动

两处，都在 engine 清单里（不动官方 super）：

1. 新增 `devices/manifest/samsung/ss9380/sepolicy/vendor_file_contexts`
   ```
   /dev/socket/napproxyd          u:object_r:napproxyd_socket:s0
   ```
   与官方第 1874 行逐字一致。

2. 新增 `devices/manifest/samsung/ss9380/sepolicy/vendor_sepolicy.cil`（第一段）
   ```cil
   (type napproxyd_socket)
   (roletype object_r napproxyd_socket)
   (typeattributeset file_type (napproxyd_socket))
   (typeattributeset mlstrustedobject (napproxyd_socket))
   (allow socket_device napproxyd_socket (sock_file (create)))
   (allow init napproxyd_socket (sock_file (create write unlink setattr getattr)))
   (allow netdomain napproxyd_socket (sock_file (write)))
   ```

为什么不需要重编 `precompiled_sepolicy`：本机型运行时 init 打印
`No precompiled sepolicy at /vendor/etc/selinux/precompiled_sepolicy`，走
`Compiling SELinux policy` + `/system/bin/secilc` 现场编译，改片段重打 vendor 即生效。

## 5. 验证

- 构建：`07_build_vendor` 步骤 6 打印
  `追加 .../vendor_sepolicy.cil <- manifest/samsung/ss9380/sepolicy/vendor_sepolicy.cil`，
  09 出 `patch.img`（sha256 `0071f17bbfaf`）。
- 起机：`init.svc.netd = running`，`/dev/socket/napproxyd` 标签为
  `u:object_r:napproxyd_socket:s0`，netd 不再退出。
- 下游：system_server 得以推进到 `startOtherServices` / `PHASE_ACTIVITY_MANAGER_READY`（=500），
  暴露出下一个卡点（PersistentDataBlockService，见 `issue-pdb-frp-block-device`）。

## 6. 遗留

- 本机型是「官方 system + lineage vendor」，OneUI 把一批原本在 plat 的权限挪到了 vendor 侧
  （三星自己的 vendor_sepolicy 里补）。凡是这类权限缺失都会以 avc 形式暴露，按需逐条补。
- 同一批还发现 `vold` 的 `netlink_socket create` 被拒（vold 死循环、刷爆 logcat），
  已在同一份 `vendor_sepolicy.cil` 的第二段补上，验证见 `issue-vold-netlink-socket`。
