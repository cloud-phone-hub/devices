# oriole：/vendor 路径重定向从内核补丁改为外部模块 cdh_redirect.ko（内核保持原样、规则按机型数据化）

- 设备: google/oriole AP2A.240905.003.F1（Pixel 6，Android 14 / lineage-21.0 / 6.1.138-4k-cdh）
- 宿主: macOS (darwin, QEMU + hvf)，daemon `127.0.0.1:9050`
- 状态: **已闭环**（2026-11-02，实例 23；`patch.img` sha256 `8011f6eb1113…`、
  `vendor_boot.img` `ca17f170ffd1…`、`cdh_redirect.ko` `3ca9267f7294…`）

---

## 1. 问题

需求原话：「内核的重定向可以使用内核模块改造？不想修改内核了，以后不同的设备需要重定向的
目录也可以自定义，可以先拿 oriole 14 测试」。

改造前的实现（#101 / #102）是**内核补丁** `CONFIG_CDH_PATH_REDIRECT`：`fs/namei.c` 的
`getname_flags()` 里就地改写 `kname`，`fs/proc/base.c` 的 `do_proc_readlink()` 里做逆变换，
`fs/namespace.c` 的 `ksys_umount()` 兜底；9 条白名单（外加 `/system/bin/update_verifier`
屏蔽）是 `.c` 里的常量数组。三笔代价在换机型时同时出现：

1. 每上一台新机就要改内核树、重编 Image，6.1 → 6.6 → 6.12 三次移植（#102）都是同一套代码
   重抄一遍，且每次都是「内核仓领先本地未推送」的状态；
2. 规则写在代码里，模版化只能靠改 `.c` 再重编，机型之间没法共用同一份内核产物；
3. 内核树长期 dirty（02 阶段拒绝脏树），构建链与发布链互相牵扯。

## 2. 根因

这里「根因」要回答的是**为什么可以用外部模块完全替代补丁**，以及代价落在哪：

- 重定向的两半都发生在 VFS 的固定出口，且都在**内核内存**上动手，不需要改源码：
  前向是 `getname_flags()`（用户路径串进入 VFS 的唯一入口，返回值是 `struct filename *`，
  `f->name` 已经指向内核缓冲区）；逆向是 `proc_pid_readlink()`（proc 魔术链接的唯一出口，
  `do_proc_readlink` 被内联进它）与 `vfs_readlink()`（`readlink(2)` 的公共出口）。
  三者都是 kprobe 可解析的符号 → 语义可以 100% 复刻，内核源码一行不动。
- 规则形态天然是数据：`<dir|file> <源前缀> <目标前缀>`，逆变换把源/目标互换即可派生，
  于是「换机型 = 换一份规则文件」，`.ko` 二进制对所有设备通用。
- 剩下的全是工程坑，而不是「能不能做」：① 钩子必须在 **first stage**（SELinux 策略加载
  之前）就位，模块要靠 `modules.load` 被 init 主动加载；② `vfs_readlink` 这层不能省
  （只挂 `proc_pid_readlink` 时 `readlink /proc/$$/cwd` 拿不到改写）；③ 用户缓冲区可能带
  MTE 标签，`copy_from_user_nofault()` 会因 `access_ok()` 判越界而静默失败。
  三个坑各自的现场见 `log-fail.txt`。

## 3. 证据

### 3.1 干净内核 + 模块提供重定向（实例 23，first stage 0.16 s）

```
[    0.000000] Linux version 6.1.138-4k-cdh (…) #1 SMP PREEMPT Sun Nov  1 15:51:22 UTC 2026
[    0.162427] init: Release specific kernel module dir 6.1.138-4k-cdh found, loading modules from here with no fallbacks.
[    0.162761] init: Loading module /lib/modules/6.1.138-4k-cdh/cdh_redirect.ko with args ''
[    0.163976] cdh_redirect: 规则来自 /lib/modules/cdh_redirect.rules：10 条
[    0.164650] cdh_redirect: 前向钩子就绪：getname_flags（maxactive 4096）
[    0.165646] cdh_redirect: 逆向钩子就绪：proc_pid_readlink + vfs_readlink
[    0.165893] cdh_redirect: 已加载：10 条规则（/lib/modules/cdh_redirect.rules），内核未打补丁
[    0.167707] cdh_redirect: forward /mnt/vendorpatch/vendor/etc/fstab.oriole
```

guest 侧：`grep -c "cdh_redirect_vendor_path\|cdh_path_redirect" /proc/kallsyms` = **0**
（旧补丁符号确实不在内核里），`uname -r` = `6.1.138-4k-cdh`（本地版本串没变，01/02 未动）。

### 3.2 为什么逆向要挂两层

- `fs/proc/base.c` 的 `proc_pid_link_inode_operations` 同时挂了 `.readlink = proc_pid_readlink`
  与 `.get_link`；`fs/proc/fd.c` 里 `/proc/PID/fd/N` 的 inode 复用同一份 ops —— 所以
  fd 链接也走 `proc_pid_readlink`。
- `readlink(2)` 的调用链是 `do_readlinkat()`（`fs/stat.c`）→ `vfs_readlink()`（`fs/namei.c`），
  两个编译单元，不可能内联，kretprobe 必然命中真实调用。
- 实测：实例 22（只挂 proc 层）时 `readlink /proc/$$/cwd` 返回物理路径，而
  `/proc/self/fd/N` 能拿到改写；补挂 vfs 层后两者一致。

### 3.3 三个失败态（全文见 `log-fail.txt`）

| # | 现象 | 根因 |
| :-- | :-- | :-- |
| ① | 实例 21 卡在 early-init，`init: Unable to open /lib/modules, skipping module loading.` | 05 注入了 0 个文件：`out/bin/cdh` 是改注入代码之前的旧二进制，静默不注入 |
| ② | `readlink /proc/self/cwd` 仍是 `/mnt/vendorpatch/...`，`ls -l` 却正常 | 只挂了 `proc_pid_readlink`；`readlink(2)` 的公共出口 `vfs_readlink` 没挂 |
| ③ | 两层都挂了，日志只有「进入」没有「读回」，输出仍是物理路径 | 缓冲区指针带 MTE 标签 `buf=b4000070b28b0380 buflen=64`，`access_ok()` 判越界 |

### 3.4 MTE 标签的根因

`arch/arm64/include/asm/uaccess.h:38` 的 `access_ok()` 只在 `PF_KTHREAD` 或
`TIF_TAGGED_ADDR` 时才 `untagged_addr()`；toybox 的 `readlink` applet 走 `xreadlinkat()`，
缓冲区来自 `malloc(64)`、指针带标签（`b400…`），于是 `copy_from_user_nofault()` 里
`access_ok()` 判越界 → `-EFAULT` → 我们的返回钩子静默跳过。同一时刻 `ls -l /proc/self/cwd`
（缓冲区 `0000007fed814750`，无标签）改写成功 —— 这就是「`ls` 能显示虚拟路径、`readlink`
不能」的不对称根因。

### 3.5 修好后

实例 23：`buf` 变成剥标签后的 `00000074f01a9150`，
`读回=/mnt/vendorpatch/vendor_dlkm/lib` → `写回=/vendor_dlkm/lib len=16 err=0`
→ 外层 `vfs_readlink` 读回已经是虚拟路径（幂等，只读不写）。完整日志见 `log-pass.txt`。

## 4. 改动

1. **新增外部模块** `devices/modules/cdh_redirect/`（Kbuild + Makefile + `core/{main,rules}.c`
   + `hook/{namei,readlink}.c` + `include/cdh_redirect.h`）：
   - 前向 `kretprobe getname_flags`：返回处拿 `struct filename *`，按 `name == f->iname`
     判断嵌入式/整页容量，就地改写 `f->name`；
   - 逆向 `kretprobe proc_pid_readlink` + `kretprobe vfs_readlink`：入口记 buffer/buflen
     （顺手 `untagged_addr()` 剥 MTE 标签），返回处 `copy_from_user_nofault` 读回、
     `cdh_rewrite_reverse()` 改写、`copy_to_user_nofault` 写回，再 `regs_set_return_value()`
     改长度；
   - 规则表 `core/rules.c`：解析 `<dir|file> <源前缀> <目标前缀>`，逆变换自动派生；
   - `/proc/cdh_redirect` 输出规则 + `forward_hits/reverse_hits/*_missed` +
     按挂点分开的 `readlink[proc]/readlink[vfs]: entries/rets/hits/skipped`；
     `module_param debug` 控制逐次打印；0 条规则直接 `-EINVAL` 拒绝加载。
2. **机型规则数据化** `devices/manifest/google/oriole/kernel/cdh_redirect.rules`
   （10 条、2377 B：照搬内核白名单 9 条 + `/system/bin/update_verifier` 屏蔽）。
3. **03_build_modules**：baseline 里 `rom.lineage.kernel.modules = ["cdh_redirect"]`
   → 在 `M=` 下编出 `.ko`，与 in-tree 模块一起 `llvm-strip --strip-debug` 后导出。
4. **05_build_vendor_boot**（`modules.go`）注入 vendor_ramdisk 根级：
   `lib/modules/<release>/cdh_redirect.ko`、`modules.load`、`modules.dep`、
   `lib/modules/cdh_redirect.rules`；配方输入哈希含 `.ko` + 规则文件
   （`--redirect-rules` / `CDH_VENDOR_BOOT_REDIRECT_RULES` / `--no-redirect` 三个开关）。
5. **02_build_kernel**：oriole 的内核树回退到 `origin/lineage-21`（`6aaf51263dc9`），
   `fs/namei.c`、`fs/proc/base.c`、`fs/namespace.c`、`fs/Kconfig`、`include/linux/namei.h`
   五处补丁全部还原。`build/02_build_kernel/lineage-kernel.config` 里那行
   `CONFIG_CDH_PATH_REDIRECT=y` 是给 22-6.6 / 23-6.12 两棵树留的，本树已无该 Kconfig 符号，
   `olddefconfig` 会直接丢掉它（产物 `out/kernel/lineage-21-6.1/config` 里 grep 不到）。
6. **本轮三处修正**（`hook/readlink.c`）：① 入口 `untagged_addr()` 剥 MTE 标签；
   ② 计数按挂点分开（`entries/rets/skipped/hits` × 2）；③ 拷贝失败时打印原因
   （原先只 `skipped++`，导致「进入有日志、失败无日志」极难定位）。

## 5. 验证

构建：`cd engine && ../out/bin/cdh device build oriole --manifest ap2a --from build_modules --refresh`（EXIT=0）

```
==> 重定向模块: out/kernel/lineage-21-6.1/modules/cdh_redirect.ko（41560 B，内核 6.1.138-4k-cdh，
    规则 manifest/google/oriole/kernel/cdh_redirect.rules）
    重定向模块: cdh_redirect.ko（41560 B）+ 规则 …cdh_redirect.rules（2377 B）-> lib/modules/6.1.138-4k-cdh/
    缓存配方:   vboot-55e1253b1894
fragment0 platform: 4281414 B -> 4296025 B（注入模块 6.1.138-4k-cdh、…/cdh_redirect.ko、
    …/modules.load、…/modules.dep、cdh_redirect.rules）
```

产物指纹：`cdh_redirect.ko` `3ca9267f7294b74b…`（41560 B）、`vendor_boot.img`
`ca17f170ffd1e310…`、`patch.img` `8011f6eb1113c017…`、`vda.img` `e7739b1808b48b58…`。

起机（实例 23，干净内核）：

- `sys.boot_completed=1`；`init: starting service 'zygote'` @ 9.94 s；
  `boot_progress_enable_screen: 38716`（38.7 s）
- `init.svc.vendor.hwcomposer-2-4=running`、`surfaceflinger` pid 735、
  `mCurrentFocus=Window{… com.google.android.apps.nexuslauncher/…NexusLauncherActivity}`；
  `HWC did not hotplug` 0 次
- `avc: denied … vendorpatch` 0、`CANNOT LINK` 0、`mount | grep -c vendorpatch` = 1

前向重定向：

```
md5_vendor_build_prop:ded146b247040c50898964ed66430872  /vendor/build.prop
md5_physical:ded146b247040c50898964ed66430872  /mnt/vendorpatch/vendor/build.prop
odm_prop:#  start|####################################|# from generate-common-build-props|
update_verifier:ls: /system/bin/update_verifier: No such file or directory
fd6:/vendor/build.prop
```

逆向（`readlink` 出口全部是虚拟路径；改前是 `/mnt/vendorpatch/...`）：

```
A:/vendor/lib64   B:/vendor_dlkm/lib   C:/vendor/etc
D:/system/bin/init   E:/   F:/
G:lrwxrwxrwx 1 root root 0 2026-11-02 01:16 /proc/self/cwd -> /vendor/lib64
```

计数（`cat /proc/cdh_redirect`）：

```
rules: 10/32
forward_hits: 11395
reverse_hits: 3010
forward_missed: 0
reverse_missed: 0
readlink[proc]: entries=55805 rets=55805 hits=3010 skipped=52795
readlink[vfs]:  entries=58904 rets=58904 hits=0    skipped=58904
```

`readlink[vfs] hits=0` 是预期：proc 层已经改写完成，vfs 层读回的是虚拟串、命中不到逆向规则
（幂等），只读不写。

## 6. 遗留

- **只有 oriole 迁到模块**。`kernels/lineage-22-6.6`（gold-15）、`lineage-23-6.12`
  （lineage-23.2 / m22 / ss9380）里仍是 `CONFIG_CDH_PATH_REDIRECT` 在编，
  `build/test/carrier_mount_test.go` 的守卫也仍以「带该 config 的内核树」为扫描范围。
  每台迁移要三件事：写 `<manifest>/kernel/cdh_redirect.rules`、删该树 config 与五处补丁、
  重跑 02→11。
- kretprobe 依赖符号名（`getname_flags` / `proc_pid_readlink` / `vfs_readlink`）和
  「第 2、3 个参数是 buffer/buflen」的假设：6.1 实测通过，6.6 / 6.12 上要逐个复核
  （6.12 的 `readlink` 相关原型有变动，不能默认沿用）。
- `vfs_readlink` 这一层是防御性冗余（实测 hits 恒 0），但确实修好了「只挂 proc 层时
  `readlink` 拿不到改写」的差异，暂留；若要压开销，可只在 `cdh_debug` 关闭时卸掉它。
- `cdh_debug=1` 会逐次打印并刷爆 dmesg（实测一条命令 ~900 行），只在取证时打开。
- 模块在 vendor_ramdisk 里，改 `.ko` 必须重跑 05（配方含 `.ko` 哈希，会自动失效）→ 09 → 11；
  只重跑 11 不会带新模块。
- **换 `out/bin/cdh` 后忘记重编**会静默不注入模块（本轮踩过）：此时 05 只打印
  `重定向模块: 不注入`，构建不报错。收尾核对这一行，或直接看 `vboot-` 配方号是否变化。

## 7. 后续（#108，2026-11-02）

- 遗留 1（只有 oriole 迁完）与遗留 2（6.6 / 6.12 需复核符号与参数约定）已在
  `issue-cdh-redirect-full-migration` 闭环：五台机型（oriole / gold / m22 / cph2797 /
  ss9380）全部起机，`forward_missed` / `reverse_missed` 全 0，6.6 与 6.12 上的
  `getname_flags` / `proc_pid_readlink` / `vfs_readlink` 三处挂点均实际命中。
- 新增一个本条目未遇到的坑：`08_build_vendor_dlkm` 会把产物目录里的 `cdh_redirect.ko`
  再注一份到 `/vendor_dlkm`，late-init 被 `dlkm_loader` 重复加载（EEXIST 静默）。现已由
  `firstStageModules` 排除，并把「注入集」纳入 08 配方（`dlkm-v2`）。
- 遗留 3（`vfs_readlink` 层 hit 恒 0）、遗留 4（`debug=1` 刷屏）、遗留 5（改 `.ko` 必走
  05→09→11）仍然成立。
