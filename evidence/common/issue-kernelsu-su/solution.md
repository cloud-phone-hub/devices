# issue-kernelsu-su — 云机里的 `su` 为什么不通、怎么打通

- 机型：xiaomi / gold / `OS3.0.10.0.VNQCNXM`
- 实例：`gold_cn_OS3.0.10.0.VNQCNXM/3`（qemu pid 1958，console 5910 / adb 5911 / vnc 5912）
- 承接：`#120`（kernelsu 源码走上游）、`#121`（61 个未导出符号 kprobe + trampoline）
- 结论：**能用，但只在 `adb shell`（uid 2000）里能用；root 身份的终端里 `su` 永远 `not found`，这是
  KernelSU 上游的设计，不是故障。**

## 1. 问题

`#121` 之后 `kernelsu.ko` 已经能在 first-stage 装起来（`KernelSU: ksym: 61 symbols resolved`、
`/proc/modules` 见 Live、系统正常起机），但一敲 `su` 就只有一句：

```
$ su -c id
sh: su: inaccessible or not found        # exit=127
```

三种身份下表现不一致，容易被误读成「模块没生效」：

| 身份 | uid | SELinux 域 | `command -v su` | `su -c id` |
| --- | --- | --- | --- | --- |
| daemon guest 通道（Web 控制台终端） | 0 root | `u:r:pioneer:s0` | 空 | `not found`，exit 127 |
| `adb -s 127.0.0.1:5911 shell` | 2000 shell | `u:r:pioneer:s0` | `/system/bin/su` | `uid=0(root) context=u:r:ksu:s0` |
| app uid（如 10123） | 10xxx | `u:r:untracked_app:s0` | — | 需 allowlist（本轮未做） |

盘上**并不存在** `/system/bin/su`（`ls` 报 No such file），`/data/adb` 当时是空的
（`ls /data/adb` → `total 0`），`/sys/module/kernelsu/parameters/` 也不存在。

## 2. 根因

`su` 是 KernelSU 的 **sucompat**（`kernel/feature/sucompat.c`）在做「字符串改写」，不是真的有
一个 su 二进制。三道门每一道不过，表现都是静默回落到原生路径：

### 2.1 门一：改写目标必须存在（`/data/adb/ksud`）

```c
#define SU_PATH   "/system/bin/su"     // feature/sucompat.c
#define KSUD_PATH "/data/adb/ksud"     // runtime/ksud.h
```

`is_ksud_exists()` 用 `kern_path(KSUD_PATH)` 探一次，**不在就直接 `goto do_orig_execve`**。
所以 ksud 不落盘时，`stat/access /system/bin/su` 原样走到 VFS → 文件本来就不存在 → `ENOENT`
→ shell 的 PATH 查找落空，连 execve 都到不了。

ksud 就位后，三处被改写成同一个目标：

```
faccessat / newfstatat  →  拿 ksud 的 stat 冒充 /system/bin/su（所以 command -v 能看到它）
execve                  →  filp_open(KSUD_PATH, O_PATH) + execveat(fd, "", argv, envp, AT_EMPTY_PATH)
```

`argv` 原样保留（argv[0] 仍是 `/system/bin/su`），ksud 侧靠这个分派：

```rust
// userspace/ksud/src/cli.rs
if arg0 == "su" || arg0.ends_with("/su") { return crate::su::root_shell(); }
```

### 2.2 门二：uid 必须被允许（shell 需要 `allow_shell=1`）

`kernel/policy/allowlist.c` 的判据链，任一不过即静默回落：

```c
forbid_system_uid(uid)                  // return uid < SHELL_UID && uid != SYSTEM_UID
                                        //   → 0<2000 且 0!=1000 → true（root 被排除）
is_uid_manager(uid)                     // 只有 Manager apk 的 uid
allow_shell && uid == SHELL_UID(2000)   // 要 allow_shell=1
allow_list 里 profile.curr_uid == uid && profile.allow_su
```

`allow_shell` 的默认值与写入通道是两个坑：

```c
// kernel/core/init.c
#ifdef CONFIG_KSU_DEBUG
bool allow_shell = true;
#else
bool allow_shell = false;        // 云机发行构建走这里
#endif
module_param(allow_shell, bool, 0);   // perm=0 → 不落 /sys/module/kernelsu/parameters/
```

`perm=0` 意味着**模块装好后再也改不了**，只能在 insmod 那一刻给。gold 的 `kernelsu.ko`
是 `CONFIG_KSU=m` 的正常构建，故默认 `false`，uid 2000 被门二挡住。

### 2.3 门三：`/data/adb/ksud` 的上下文（`ksu_file`）

内核 `apply_kernelsu_rules()` 建了两个类型：

```c
ksu_type(db, KERNEL_SU_DOMAIN, "domain");            // "ksu"，permissive + allow all
ksu_type(db, KERNEL_SU_FILE, "file_type");           // "ksu_file"
ksu_allow(db, "domain", KERNEL_SU_FILE, ALL, ALL);   // 任何域都能用
```

ksud 必须 `chcon u:object_r:ksu_file:s0`，否则 execveat 那一步照样被 SELinux 拦。

### 2.4 为什么 root 里永远测不到 su

门二第一行 `forbid_system_uid` 把 `uid < 2000 && uid != 1000` 全排除，root(0) 必在其中。
这是上游刻意的：**能当 root 的进程不需要再 su 一次**。所以 daemon 的 guest 通道（uid 0）里敲
`su` 必然 `not found`，跟模块状态无关；在 root 里「测出 su 能用」的一律是别的来源
（magisk 残留、toybox 的 su 别名等）。

## 3. 证据

同目录两份日志，取样环境写明在文件头：

- `log-fail.txt` — 改动前三种身份的实测输出 + 上面三道门的代码位置；
  含 `ls -l /system/bin/su` → No such file、`ls /data/adb` → total 0、
  `ls /sys/module/kernelsu/parameters/` → 不存在。
- `log-pass.txt` — 打通后的正向证据：
  - `qemu.log:456` `init: Loading module …/kernelsu.ko with args 'allow_shell=1'`
  - `qemu.log:459` `KernelSU: shell is allowed at init!`
  - `qemu.log:99.007779` `KernelSU: faccessat su->ksud!` / `sys_execve su found` /
    `newfstatat su->ksud!` —— 三条改写各命中一次
  - `adb -s 127.0.0.1:5911 shell 'su -c id'` → `uid=0(root) … context=u:r:ksu:s0`，exit=0
  - `su 10123 -c id` → `uid=10123(u0_a123)`
  - `su -c 'getenforce'` → `Enforcing`（ksu 域 permissive，但全局仍 Enforcing）

关键对照（本次重启后现测，2026-11-02）：

```
# 身份 A：guest 通道（uid 0）
uid=0(root) gid=0(root) groups=0(root) context=u:r:pioneer:s0
---
---                              ← command -v su 空
exit=127  (stderr: sh: su: inaccessible or not found)

# 身份 B：adb shell（uid 2000）
uid=2000(shell) ... context=u:r:pioneer:s0
---
/system/bin/su
---
uid=0(root) gid=0(root) groups=0(root) context=u:r:ksu:s0
exit=0
```

`adb shell` 侧 `ls -lZ /data/adb/ksud` 报 `Permission denied` 也符合预期——shell 域读不到
`ksu_file` 目录项，但 sucompat 的改写走的是内核里的 `kern_path`/`filp_open`，不受这个限制。

## 4. 改动

两件事，都在「不改内核树、不引入 ksuinit」的口径内：

### 4.1 `modules.options`：把加载参数注入 first-stage（`allow_shell=1`）

`module_param(..., perm=0)` 只能在 insmod 时给，而 first-stage 的 insmod 由 init 调
libmodprobe 做，参数唯一来源就是 `modules.options` 表：

```
options <模块名不带 .ko> <参数>…
```

`05_build_vendor_boot` 原来只写 `modules.load` / `modules.dep`，现在按模块的
`module.json` 派生第三张表：

- `internal/modmeta/modmeta.go` 新增 `ModuleName(meta)` —— 产物名去 `.ko`，多产物取第一个；
- `05_build_vendor_boot/modules.go`：`firstStageInjection` 加 `Options`，`files()` 非空时写
  `lib/modules/<release>/modules.options`，新增 `optionList()`，`describe()` 打印
  `（加载参数：options kernelsu allow_shell=1）`；
- `recipeSource()` 把 options 计入摘要 —— **参数变了必须让缓存失效**，否则重跑 05 会命中旧
  vendor_boot，`.ko` 一个字节没变、日志也看不出，新参数被静默盖掉；
- `modules/kernelsu/module.json` 加 `"options": ["allow_shell=1"]`，note 写清「non-DEBUG 构建
  默认 false、perm=0 只能 insmod 时给」。

两张表的口径不能写反：`modules.load` 用**文件名**（`kernelsu.ko`）定位模块，
`modules.options` 用**模块名**（`kernelsu`）匹配参数；写反了模块照装、参数不生效。

测试（与代码同目录，`package vendorbootbuild` 白盒）：

- `05_build_vendor_boot/modules_test.go`：`TestFirstStageOptionList`（内容式样 + 进注入集）、
  `TestFirstStageNoOptionsFile`（无参数不写空表）、`TestFirstStageRecipeTracksOptions`
  （参数进配方）；
- `internal/modmeta/modmeta_test.go`：`TestModuleName`（单产物 / 多产物 / 无产物）。

### 4.2 ksud：自建交叉编译并落到 `/data/adb/ksud`

上游 `userspace/ksud` 用 Rust 写，供应链侧没有可直接取用的二进制（从 Manager APK 里抠也行，
但版本要与模块 uapi 对齐），故按源码自建，与模块同源（`v3.3.1`，`uapi: 5`）：

- 工具链：`rustup toolchain install 1.91.0`（`adb_client@3.1.1` 要求），
  `rustup target add aarch64-linux-android`；
- linker/API：NDK r29 的 `aarch64-linux-android30-clang`，`--target=aarch64-linux-android30`
  （API 24 的 bionic 不导出 `__system_property_read_callback`，链接期直接 undefined）；
- bindgen 同步：`LIBCLANG_PATH` + `BINDGEN_EXTRA_CLANG_ARGS_aarch64_linux_android=--target=…30`；
- 产物：4937464 B，`ksud --version` → `ksud 3.3.1 (uapi: 5)`，归档在
  `engine/out/tmp/ksud/ksud-3.3.1-aarch64-linux-android`。

落盘（`/data` 是 FBE 加密 + 实例 `writable-data.qcow2` 持久层，host 侧写不进去）：

```
adb -s 127.0.0.1:5911 push ksud /data/local/tmp/ksud      # sh 域可写的地方
# 再经 guest root 通道：
cp /data/local/tmp/ksud /data/adb/ksud
chown root:root /data/adb/ksud && chmod 755 /data/adb/ksud
chcon u:object_r:ksu_file:s0 /data/adb/ksud
```

## 5. 验证

命令与原始输出见 `log-pass.txt`，这里只列判据：

1. **参数进镜像**：05 摘要出现 `（加载参数：options kernelsu allow_shell=1）`，
   注入文件列表含 `modules.options`，配方 `vboot-ab64d08b8d7d -> vboot-e3d0c0f93d14`；
2. **参数被 init 取到**：`qemu.log` `with args 'allow_shell=1'` + `shell is allowed at init!`；
3. **改写生效**：`qemu.log` 三条 `su->ksud` / `su found`；
4. **su 真能用**：`adb shell` 下 `su -c id` → `uid=0(root) context=u:r:ksu:s0`，exit=0；
   `su -c 'getenforce'` → `Enforcing`；`ls /data/adb` → `ksu` `ksud`；
5. **能切指定 uid**：`su 10123 -c id` → `uid=10123(u0_a123)`；
6. **重启仍成立**：05/11 重打 → `POST /api/stop` → `POST /api/start`（复用实例目录）后，
   `/data/adb/ksud` 仍在（writable-data 保留），`su` 复测通过；
7. **构建层**：`go build ./...` 干净，`go test ./internal/modmeta ./05_build_vendor_boot` ok。

## 6. 遗留

1. **ksud 未随盘集固化**（最重要）。现在它只在 `/data/adb/ksud`，靠实例的
   `writable-data.qcow2` 活着；换新实例、重打 userdata、或 `data.qcow2` 种子重置就丢，
   `su` 立刻退回 `not found`。两个候选：
   - 固化进 vendor 分区（`/vendor/bin/ksud`）+ `patches/0004` 把 `KSUD_PATH` 指过去；
   - 或保留 `KSUD_PATH` 不变，用 init rc 在 `post-fs-data` 前从 `/vendor/bin/ksud` 复制过去。
   两者都要动模块源码或补丁集，需先定口径再实施。
2. **app uid（10xxx）的 su 未授权**：要走 allowlist（Manager 或 `ksud` 的 `app grant`），
   本轮只验了 shell(2000) 与显式 `su 10123`（后者是 root 主动降权，不是 app 申请）。
3. **ksu 域下的功能未验**：`sulog`、`selinux_hide`、`post-fs-data`/`services`/`boot-completed`
   三段 ksud 钩子。实测 `ksud post-fs-data` 在 `cmd.run` 下超时（但已建出
   `/data/adb/ksu/{bin,log}`），钩子链路要单独取证。
4. **`su` 在 root 里不可用这件事，是否要「修」**：不改内核模块源码就修不了
   （`forbid_system_uid` 是编译期逻辑）。若将来确有需求（例如某些脚本在 root 上下文里
   硬调 `su`），再评估是否给 `uid==0` 加例外——代价是偏离上游、每次升级要重打补丁。
5. **6.1 / 6.12 内核代际**未验：sucompat 用的 `execveat` 路径与 hook 点位在各代一致，
   但未实机复测。
