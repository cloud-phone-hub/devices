# kernelsu 未导出内核符号：模块内 kprobe + trampoline 自解析（不改内核、不引入 ksuinit）

- 机型：`xiaomi/gold`（Redmi Note 13 5G，MT6833，`lineage-22.2`，内核 `6.6.127-4k-cdh`）
- 模块：`devices/modules/kernelsu`（上游 `tiann/KernelSU` v3.3.1，LKM 形态）
- 前置条目：[#120 issue-kernelsu-upstream-source](../issue-kernelsu-upstream-source/solution.md)（构建层打通，遗留「运行态待验」）
- 日志：[log-fail.txt](log-fail.txt) / [log-pass.txt](log-pass.txt)

## 1. 问题

gold 基线在 `modules.external` 里声明 `kernelsu` 后，`03 → 05 → 09 → 11` 全部成功：
`kernelsu.ko`（354080 B，`vermagic=6.6.127-4k-cdh`，`KernelSU version: 32680`，166 个符号版本一致）
被 05 按 `inject: first-stage` 放进 vendor_ramdisk 根级 `lib/modules/6.6.127-4k-cdh/` 并写进
`modules.load`。但云机起来的第一秒就崩：

```
[0.135840] init: Loading module /lib/modules/6.6.127-4k-cdh/kernelsu.ko with args ''
[0.136289] kernelsu: Unknown symbol path_umount (err -2)
[0.136340] kernelsu: Unknown symbol strncpy_from_user_nofault (err -2)
...（共 61 个不同符号，854 行含重启循环）
[0.146352] init: Failed to insmod '.../kernelsu.ko' with args '': No such file or directory
[0.147021] init: Failed to load kernel modules
[0.147220] init: InitFatalReboot: signal 6
```

init 的首阶段模块加载是**致命路径**：一个模块失败 = `InitFatalReboot`，云机开不了机。
（现场见 `log-fail.txt`；这一版已回滚基线并重跑 05+11 恢复可启动盘集。）

## 2. 根因

`insmod` 走内核的 `simplify_symbols()`：`.ko` 里每个 `SHN_UNDEF` 符号都必须在
`__ksymtab`（`EXPORT_SYMBOL*`）里找到地址，`modversions` 还要比对 CRC。KernelSU 的内核侧实现
是按「编进 vmlinux」设计的，直接引用大量**没有导出**的内部符号，共 61 个：

- 通用内核 API：`path_umount`、`path_mount`、`kallsyms_lookup_name`、`kallsyms_on_each_symbol`、
  `init_mm`、`__set_fixmap`、`task_work_add`、`alloc_uid`、`set_cred_ucounts`、`change_pid`、
  `ext4_unregister_sysfs`、`ns_get_path`、`ksys_unshare`、`set_fs_pwd`、`anon_inode_getfile_secure`…
- SELinux 私有实现：`selinux_state`、`selinux_blob_sizes`、`avtab_*`、`ebitmap_*`、`policydb_*`、
  `sidtab_*`、`mls_*`、`avc_ss_reset`、`selnl_notify_policyload`…

上游对 LKM 的官方解法是 userspace 侧的 `ksuinit`（读 `/proc/kallsyms` 打 ELF 重定位再
`init_module`），代价是替换 ramdisk 里的 `/init`。本项目要求**与 `cdh_redirect` 同形态**：
纯外部模块、first-stage 注入、内核零补丁、不进任何 userspace 装载器。

结论：必须在**模块内部**把这些引用换成运行时地址。`cdh_redirect` 之所以天然满足，是因为它
只碰导出 API（`register_kretprobe`），内部函数地址由 kprobe 的 `symbol_name` 让内核自己查；
把同一手法放大到 61 个符号即可。

## 3. 证据

1. **符号确实在 kallsyms 里（含 static 数据）**：guest 通道（非 adb）回读
   `/proc/kallsyms` 共 144652 行，抽查 `kallsyms_lookup_name`、`path_umount`、`avtab_search_node`
   为 `T`，`selinux_blob_sizes` 为 `D`，`selinux_state` 为 `B`；`kptr_restrict=2`（地址显示 0，
   但 root/pid1 有权限降到 1 后读真实地址）。
2. **内核配置支持**：`out/kernel/lineage-22-6.6/config` 中 `CONFIG_KALLSYMS=y`、
   `CONFIG_KALLSYMS_ALL=y`、`CONFIG_KPROBES=y`、`CONFIG_CFI_CLANG=y`。
   - `ALL=y` 是数据符号（`selinux_state`）能被查到的前提；
   - `KPROBES=y` 是 `register_kprobe()`（导出）能用的前提；
   - `CFI_CLANG=y` 决定 trampoline 只能尾跳（`br`），不能走间接调用（`blr` 会做 kCFI 校验）。
3. **引用面精确**：在容器卷里对每个 `.o` 做 `llvm-nm -u`，得到
   `符号 -> 目标文件` 映射（20 个 `.o`：`selinux_hide.o`、`sepolicy.o`、`rules.o`、`selinux.o`、
   `file_wrapper.o`、`su_mount_ns.o`、`patch_memory.o`、`syscall_hook_manager.o`、`symbol_resolver.o`、
   `sucompat.o`、`supercall.o`、`dispatch.o`、`boot_event.o`、`lsm_hook.o`、`app_profile.o`、
   `kernel_umount.o`、`ksud_integration.o`、`sulog/event.o`、`pkg_observer.o`、`allowlist.o`；
   合并后 `kernelsu.o`）。源码文本引用 161 处，其中 `symtab_search` 29、`selinux_state` 24、
   `ebitmap_*` 15，集中在 SELinux 策略改写一条线上。
4. **同形态先例**：`modules/cdh_redirect` 只 include `<linux/kprobes.h>`，两个 `kretprobe`
   让内核按 `symbol_name` 解析 `getname_flags` / `proc_pid_readlink`，模块本身零未导出引用。

## 4. 改动

全部落在 `devices/modules/kernelsu/patches/0003-ksym-shim.patch`（新增文件 + 12 个文件的定点修改，
上游 `source/` 保持 pristine，补丁在 03 的容器里按 `0001 → 0002 → 0003` 顺序应用）：

| 落点 | 内容 |
| :--- | :--- |
| `kernel/infra/ksym.c`（新增） | `register_kprobe(&kp{ .symbol_name = "kallsyms_lookup_name" })` 取到地址后 `unregister_kprobe`，再用这个函数指针逐个 `kallsyms_lookup_name(name)` 填 61 个 `void *ksu_sym_*`；缺符号直接 `-ENOENT`。`selinux_blob_sizes` 另存一份本地同名变量（见下） |
| `kernel/infra/ksym_tramp.S`（新增） | 56 个函数符号的同名跳板：`adrp x16, ksu_sym_x` / `ldr x16, [x16, :lo12:...]` / `br x16`，模块内的调用直接落到本地定义，不再产生 `U` 条目；`br` 是尾跳转，kCFI 不校验 |
| `kernel/include/infra/ksym.h`（新增） | 5 个数据符号的宏（`selinux_state`、`init_mm`、`mntns_operations`、`__tracepoint_sys_enter`）+ `ksu_ksym_init()` 声明 |
| `kernel/Kbuild` | `kernelsu-objs += infra/ksym.o`；ARM64 分支加 `infra/ksym_tramp.o` |
| `kernel/core/init.c` | `kernelsu_init()` 第一句就 `ksu_ksym_init()`，失败即 `return -ENOENT`（不能带空指针跑） |
| 7 个使用文件 | `feature/selinux_hide.c`、`selinux/selinux.c`、`selinux/rules.c`、`infra/file_wrapper.c`、`infra/su_mount_ns.c`、`hook/arm64/patch_memory.c`、`hook/syscall_hook_manager.c` 各加一行 `#include "infra/ksym.h"`（必须在各文件内核头之后：宏是延迟展开的） |
| `hook/syscall_hook_manager.c` | 注册/反注册改用导出的 `tracepoint_probe_register(_prio)` / `tracepoint_probe_unregister`，tp 指针取自 `ksu_sym___tracepoint_sys_enter`。原因：`register_trace_sys_enter()` 是头文件 inline，预处理期就把 `__tracepoint_sys_enter` 固化成 `U`，宏替换拦不住 |
| `kernel/infra/ksym.c` | `selinux_blob_sizes` 同理在 inline（`selinux_cred()`）里被引用，改为模块内定义同名变量 + init 时从真实地址 `memcpy` 快照（LSM blob 大小在 `security_init()` 后不变，语义等价） |

`devices/modules/kernelsu/module.json` 保持 `inject: first-stage` 不变（KernelSU 需要在 init
早期、SELinux 策略生效前装 syscall dispatcher）；`engine/baseline` 的 `modules.external`
重新加回 `kernelsu`。03 与 05/08 的 Go 代码零改动。

## 5. 验证

1. **产物级**：`nm -u kernelsu.ko | grep -Fxf 61 符号表` → 空（61 个全部不再出现）；
   `nm -u` 去 `__crc_` 后剩 165 个，全部是导出符号。`nm kernelsu.ko` 可见
   `T avtab_search_node` / `T kallsyms_lookup_name` / `T __arm64_sys_close` 等本地 trampoline
   与 `B ksu_sym_selinux_state`。
2. **构建链**：`03` 实编通过（`KBUILD_MODPOST_WARN=1` 下无 `Unknown` 报错）；
   `05` 输出 `cdh_redirect.ko（39736 B）+ kernelsu.ko（366552 B）-> lib/modules/6.6.127-4k-cdh/`；
   `11` 重打 `vda.img`（13.00 GiB，5 个分区）。
3. **运行态**（`log-pass.txt`）：
   - `init: Loading module .../kernelsu.ko` 后立即 `KernelSU: welcome to KernelSU version 32680`
   - `KernelSU: ksym: 61 symbols resolved (56 funcs via trampoline, 5 data)`
   - `sys_call_table=0xffffffdf98720c98`、`dispatcher installed at slot 42`
   - `hook_manager: sys_enter tracepoint registered`
   - guest 回读：`/proc/modules` → `kernelsu 200704 0 - Live (O)`；`getenforce` → `Enforcing`；
     系统正常起机（BootAnimation / zygote 起来，无 `InitFatalReboot`）
4. **边界**：`/data/adb/` 为空 → `/system/bin/su` 报 `inaccessible or not found`。
   这是 KernelSU 的设计：`su` 的 sucompat 把 `/system/bin/su` 重定向到 `/data/adb/ksud`，
   ksud 是本项目未提供的 userspace 二进制（见遗留）。

## 6. 遗留

1. **`su` 仍需 ksud**：内核态已就绪（dispatcher + allowlist + sulog），但 `su -c` 需要
   `/data/adb/ksud`（从 KernelSU Manager APK 提取或自行交叉编译）。`0001-shell-profile.patch`
   已让 UID 2000 shell 走 `allow_su`，装好 ksud 后应可直接验证；Manager APK 未纳入镜像。
2. **跨内核代际**：`ksym_tramp.S` 的 56 个跳板与 61 条符号清单是按 6.6.127 编的。
   22.1（6.1）/ 23.2（6.12）机型接入时，符号集合与 inline 引用点可能变化，需要按
   `nm -u` 重新生成（生成脚本逻辑：`符号 -> trampoline` + 数据符号宏）。
3. **快照语义**：`selinux_blob_sizes` 用快照依赖「LSM 初始化已完成」，在 init 阶段加载成立；
   若将来改成 late load（ksud 手动 insmod）也成立，但需复测。
4. **`KBUILD_MODPOST_WARN=1` 仍保留**：现在只剩导出符号，理论上可以去掉；保留是因为上游
   升 tag 时可能新增未导出引用，去掉后会在 modpost 阶段直接失败（更早暴露）。
5. **未验**：`selinux_hide` / `kernel_umount` / `su_mount_ns` 这些功能的**行为**（只验证了装载
   与初始化路径），以及 Manager 侧的版本比对、模块管理的端到端流程。
