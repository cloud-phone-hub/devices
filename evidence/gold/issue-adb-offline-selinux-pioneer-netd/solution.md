# 云机 ADB 恒 offline：pioneer 域缺网络 socket 授权（lineage 精简策略基底）

- 采集时间：2026-10-27
- 实例：`gold_OS3.0.10.0.VNQCNXM/3`（engine 新镜像，fingerprint `Redmi/vnd_gold/gold`）
- 对照实例：`gold_cn_OS3.0.10.0.VNQCNXM/1`（旧镜像 `15/os3.0.10.0.vnqcnxm`，adb 正常）
- 相关证据：#22（adb shell netns）、#27（hvc2 ADBKEY1 握手）

## 1. 问题

实例 3 的 adb 恒 `offline`，但 adbkey 链路本身已全部成功：

```
[15.736780] daemon: 版本探针 adbkey-handshake v2（ADB_SECURE=1）
[15.747487] adbkey: raw 模式已设置
[15.747540] adbkey: 已发 READY，等待宿主公钥
[15.751010] adbkey: 公钥已写入 adb_keys，OK
[15.752993] adbd: 子进程 pid=456 已拉起（/apex/com.android.adbd/bin/adbd）
adbd listening on tcp:5555 / adbd started
```

`/data/misc/adb/adb_keys` 内容与宿主 `~/.android/adbkey.pub` 逐字节一致、标签
`u:object_r:adb_keys_file:s0`、`ro.adb.secure=1`、adbd 与 init 同在主 netns
（`net:[4026531840]`）、`mgmt0 10.0.2.15/24` 与 local 表回程路由均正常。

故障表现有两层，且第二层才是真因：

1. 宿主 `adb connect 127.0.0.1:5911` → `failed to connect`，`adb devices` 显示 `offline`；
2. 用裸 TCP 探测：连接建立后 **adbd 立即 FIN（0.00s）**，不等任何 ADB 报文；
   guest 内任意 `nc`/`ping` 的 `connect()` 返回 **EMSGSIZE（Message too long）**，
   连 `127.0.0.1` 自己也一样。

## 2. 根因

**engine 的 `vendor_sepolicy.cil` 基底取自 lineage（216 KB，与 `plat_pub_versioned.cil`
同源，见 `07_build_vendor/replace.go`），只声明了 `pioneer` 类型，没有展开
`net_domain` 的 socket 权限，也没有 `netd` 与 `pioneer` 之间的互访授权。**

`pioneer` 虽然是 permissive 域（基底有 `(typepermissive pioneer)`），但 **permissive
只放行「主体是 pioneer」的方向**；netd / system_server 等**平台域是 enforcing**，
它们对 pioneer 的 fd / socket 的访问是**真拒绝（permissive=0）**。

bionic 的 `libnetd_client` 在每次 `socket()` 时都会把 fd 交给 netd 的
`/dev/socket/fwmarkd` 注册 fwmark。这条链断掉后：

- pioneer（adbd、guest shell、netmgr 里的所有命令）的 socket 拿不到 fwmark；
- netd 侧报 `control message was truncated when receiving file descriptors`；
- 内核/策略层面表现为 `connect()` 直接失败（EMSGSIZE/EACCES），
  adbd 接受连接后立即关闭 → 宿主 `adb` 恒 `offline`。

`setenforce 0` 后故障立刻消失，确认是策略问题而非网络栈/路由。

## 3. 证据

### 3.1 targeting 实验

| 操作 | 结果 |
| --- | --- |
| `setenforce 0` 后 guest 内 `nc -l` + `nc` 自连 | `rc=0`（成功） |
| `setenforce 1` 恢复 | `nc: connect: Message too long`，`rc=1` |
| 宿主裸 TCP 连 5911 后不发数据 | 0.00s 收到 FIN（adbd 立即关闭） |
| 同一探测对实例 1（5919） | 连接保持（响应超时，符合 ADB 等 AUTH） |
| 宿主 `info usernet` | 只有 `HOST_FORWARD *:5911 → 10.0.2.15:5555`，无 ESTABLISHED |

### 3.2 avc 拒绝分型（修复前实例 3，按次数排序）

```
netd       -> pioneer            [fd]                {use}         1375
pioneer    -> pioneer            [tcp_socket]        {getopt}       703
pioneer    -> fwmarkd_socket     [sock_file]         {write}        650
pioneer    -> netd               [unix_stream_socket]{connectto}    537
pioneer    -> pioneer            [tcp_socket]        {accept}       258
```

后两类（`pioneer -> pioneer`）被 permissive 放行，真正致命的是前四类：
**netd 与 pioneer 互访被 enforcing 拒绝**。

补 `fd use` 后残余：

```
netd -> pioneer [tcp_socket] {read write}  180 次（仍 offline）
```

再补 `read write ioctl getattr setattr setopt` 后残余：

```
avc: denied { getopt } for comm="binder:472_2"
     laddr=::ffff:10.0.2.15 lport=5555 faddr=::ffff:10.0.2.2
     scontext=u:r:netd:s0 tcontext=u:r:pioneer:s0 tclass=tcp_socket permissive=0
```

**avc 直接落在 adbd 的 `10.0.2.15:5555` 监听连接上**。补 `getopt` 后 adb 立刻 `device`。

### 3.3 新旧镜像策略对比

| 项 | 实例 1（旧包 15/） | 实例 3（engine 新包） |
| --- | --- | --- |
| `/vendor/etc/selinux/vendor_sepolicy.cil` | 371366 B（MIUI 完整策略） | 216774 B（lineage 精简基底 + 片段） |
| pioneer 相关行数 | 36 | 29 |
| `(typepermissive pioneer)` | 无（权限已展开，enforcing 可用） | 有（但只保护主体方向） |
| netd/pioneer 互访 avc | 0 条 | 2845 条 truncated、1344 条 tclass=fd |

## 4. 改动

只改一处（源码），不依赖手工注入：

`engine/vendor/hardware/android.service.daemon/sepolicy/vendor_sepolicy.cil`
在 `system_server` 反向授权之后追加三组规则（`{{vers}}` 由 07 按
`plat_sepolicy_vers.txt` 替换为 `202404`）：

```cil
;; pioneer 自身 socket 基础权限（基底未展开 net_domain）
(allow pioneer pioneer (tcp_socket (create bind connect listen accept getopt setopt shutdown read write ioctl getattr setattr append lock map)))
(allow pioneer pioneer (udp_socket (...)))
(allow pioneer pioneer (unix_stream_socket (...)))
(allow pioneer pioneer (unix_dgram_socket (...)))

;; fwmarkd 注册链
(allow pioneer fwmarkd_socket_{{vers}} (sock_file (write)))
(allow pioneer netd_{{vers}} (unix_stream_socket (connectto)))
(allow netd_{{vers}} pioneer (fd (use)))

;; netd 对 pioneer socket 的 setsockopt/getsockopt（平台域 enforcing）
(allow netd_{{vers}} pioneer (tcp_socket (read write ioctl getattr setattr setopt getopt)))
(allow netd_{{vers}} pioneer (udp_socket (...)))
(allow netd_{{vers}} pioneer (unix_stream_socket (...)))
(allow netd_{{vers}} pioneer (unix_dgram_socket (...)))
```

重建与发布：

```
engine/out/bin/cdh device build gold --only build_vendor --manifest gold_images_OS3.0.10.0.VNQCNXM_20260715.0000.00_15.0_cn_a9729c6694.json
engine/out/bin/cdh device build gold --only build_patch  --manifest ...
cp engine/out/devices/xiaomi/gold/OS3.0.10.0.VNQCNXM/img/patch.img \
   engine/out/dist/xiaomi/gold/OS3.0.10.0.VNQCNXM/patch.img
```

产物：`patch.img` 402653184 B，sha256 `9e4a80ce700e417e6da7…`。

## 5. 验证

```
$ adb devices
List of devices attached
fmvwkrheonugvgjb        device
127.0.0.1:5911  device

$ adb -s 127.0.0.1:5911 shell 'getprop ro.build.fingerprint; ip addr show mgmt0'
Redmi/vnd_gold/gold:15/AP3A.240905.015.A2/OS3.0.10.0.VNQCNXM:user/release-keys
    inet 10.0.2.15/24 scope global mgmt0

$ adb shell 'ping -c 1 127.0.0.1; nc 自连'
ping_rc=0
nc_rc=0

$ grep -c "avc.*pioneer.*permissive=0" qemu.log
0
```

pioneer 相关的 enforcing 拒绝从 2845 条降到 **0**，adbkey 握手日志保持不变，
`adb shell` 全功能可用。

## 6. 遗留

1. **基底仍是 lineage 精简策略**：本次只补了 pioneer 这一条链。其它 vendor 域
   （各 HAL、`hal_*_default`）若需与平台域（netd/system_server/installd 等）
   互访，可能同样缺规则；后续遇到同类 avc 应按同样方式在**对应模块**的
   sepolicy 片段补齐，不要改 lineage 基底（会破坏 `plat_pub_versioned.cil` 同源）。
2. **`(typepermissive pioneer)` 不是保障**：它只放行主体方向，平台域访问 pioneer
   仍然是 enforced。补规则时必须以 `permissive=0` 的 avc 为准。
3. 旧包（`out/device/gold-15/15/`）用 MIUI 完整策略，不受影响；两套策略的差异
   已在 3.3 记录，后续迁移到 engine 新构建链时要留意这一层。
4. `out/device/gold-15/0-engine/os3.0.10.0.vnqcnxm/` 是给实例 3 旧 overlay 找
   backing 的兼容软链（指向 `engine/out/dist/...`），待停机后可 `qemu-img rebase -u`
   清理。
