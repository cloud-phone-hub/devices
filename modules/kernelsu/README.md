# kernelsu —— KernelSU 外部内核模块（LKM 模式）

`kernelsu.ko` 由 `04_build_modules` 按统一的 `make M=<模块目录> modules` 编译。**kernelsu 的
特殊性全部收在本目录**：编译参数写在 `Kbuild`，源码按 `manifest/kernel.xml` 从上游
`tiann/KernelSU` 拉到 `source/`（当前 `v3.3.1`，tag 提交 `0c0fdb96`），云机侧改动一律是
`patches/*.patch`。03 那边没有为 kernelsu 加任何分支。

## 1. 目录结构

```
modules/kernelsu/
├── Kbuild                             编译入口（参数默认值 + 递归 + 应用补丁）
├── module.json                        模块自述（产物、注入阶段、内核代际）
├── patches/
│   ├── 0001-shell-profile.patch       云机定制（对 v3.3.1 基线生成）
│   ├── 0002-kernel-6.9-tracepoint-prio.patch   6.1/6.6 内核兼容
│   ├── 0003-ksym-shim.patch           GKI 未导出符号运行时 kprobe+跳板接入
│   └── 0004-restrict-domain-sepolicy.patch     最小权限 SELinux 规则（仅向特权/shell 域开放 ksu/ksu_file）
└── source/                            上游源码（repo 检出，不入库；见 §4）
    └── kernel/                        Kbuild / Kconfig / 各子目录
```

上游把编译入口放在 `source/kernel/` 而不是仓库根，所以本目录的 `Kbuild` 用
`obj-$(CONFIG_KSU) += source/kernel/` 递归进去，交给上游自己的 Kbuild 编译；产物是
`source/kernel/kernelsu.ko`（不在模块根，03 的产物查找按整棵模块树找、要求唯一）。

## 2. 为什么把这些写在本目录的 Kbuild 里

03 只做一件事：`make O=<out> ARCH=arm64 LLVM=1 -j<n> M=/src/.cdh/modules/kernelsu modules`。
kernelsu 需要的额外条件都在 `Kbuild` 里给默认值，而不是让 03 为单个模块传参：

| 变量 | 为什么必须给 |
| :-- | :-- |
| `CONFIG_KSU=m` | 外部模块不吃内核 `.config`，`auto.conf` 里没有 `CONFIG_KSU`，不给就是 `obj-` 空、编不出内核对象 |
| `KBUILD_MODPOST_WARN=1` | KernelSU 引用 `kallsyms_lookup_name`、`path_umount`、`selinux_state` 等未在 GKI `Module.symvers` 导出的符号（由 `ksuinit` 走 `/proc/kallsyms` 重定位），不降级 modpost 会直接失败 |
| `KSU_GIT_VERSION=2680`<br>`KSU_GIT_VERSION_VALID=1` | 上游按 `git rev-list --count` 算版本（`KSU_VERSION = 30000 + 提交数`）；03 导入模块树时排除 `.git`，卷内没有 git 信息，不写死就退化成 `KSU_VERSION=16`。`2680` 是 `v3.3.1` 的提交计数→`KSU_VERSION=32680`。**换基线必须同步改** |

四个变量都在本文件里 `export`，子 make（递归到 `source/kernel/` 的那一层）才能读到。

补丁应用也在这里：kbuild 会在编译、modpost 多个阶段反复 include 本文件，而补丁只能应用
一次。用 `git apply --reverse --check` 判断「已应用」不可行（两个补丁改同一文件，反向顺序
对不上），所以在容器 `/tmp` 落一个标记文件：03 每轮 `rm -rf /src/.cdh/modules` 重新导入、
每次构建都是新容器，标记不会跨轮残留，也不会因为源码被重置而误跳过。任一补丁
`--check` 不通过就直接 `$(error)` —— 静默跳过的后果是编出一个没有云机定制的 `.ko`，
日志一切正常，只有真机验 `su` 才暴露。

`source/` 没同步时同样直接 `$(error)`，比 kbuild 的「找不到目录」好定位。

## 3. 定制补丁（patches/）

`0001-shell-profile.patch` 相对上游 `v3.3.1` 生成，10 个文件（138+/10-），解决云机场景下
「adb shell 拿不到 root」与「shell 进程漏打标」两类问题：

| 改动 | 位置 | 作用 |
| :-- | :-- | :-- |
| `shell_profile` 策略 | `policy/shell_profile.c/.h`（新增）、`policy/allowlist.c` | 给 UID 2000（`com.android.shell`）强制套 `allow_su=true` + `use_default` root profile；`allowlist.c` 统一用 `KSU_SHELL_UID`，prune 时不清理 shell 条目 |
| `allow_shell` 默认开启 | `core/init.c` | 去掉 `CONFIG_KSU_DEBUG` 门控，默认 `true` |
| shell 进程批量打标 | `hook/tp_marker.c/.h`、`hook/setuid_hook.c`、`hook/syscall_event_bridge.c`、`hook/syscall_hook_manager.c` | `ksu_mark_start/shell/stop` + `shell_mark_work`：setresuid 落到 2000 时异步把现有 shell 线程补打 tracepoint 标记；exec 收尾时 shell 不被 unmark |
| `faccessat2` 走 sucompat | `hook/syscall_hook_manager.c` | 除 `newfstatat`/`faccessat` 外，`__NR_faccessat2` 也注册到同一 hook |
| 编译单元 | `Kbuild` | `kernelsu-objs += policy/shell_profile.o` |

`0002-kernel-6.9-tracepoint-prio.patch` 只有一处：上游无条件调
`register_trace_prio_sys_enter()`，这个入口 6.9 才有，而清单里的 6.1/6.6 没有，按
`LINUX_VERSION_CODE` 分支退回 `register_trace_sys_enter()`。基线换到「最低支持内核 ≥ 6.9」后
这个补丁可以删掉。

上游 `v3.2.4 → v3.3.1` 已自己吸收了原先的几处定制（`adb_root` 参数化、`execveat` sucompat、
`newfstatat`/`faccessat` sucompat、`/stub_zygote` 保护），所以补丁比旧的 v3.2.4 源码拷贝小得多。

## 4. 获取源码与升级

`source/` 由 `manifest/kernel.xml` 声明：

```xml
<project path="devices/modules/kernelsu/source" name="tiann/KernelSU" remote="github" revision="v3.3.1" />
```

`repo sync devices/modules/kernelsu` 拉取。注意两点：

- repo 只接管它自己创建的工作树，**手工 `git clone` 出来的 `source/` 需先移走再 sync**
  （与 `devices/kernels/` 同处理）；`devices/.gitignore` 已排除该目录，不会误入 devices 仓库。
- 03 导入模块树时排除 `.git`、`.o`、`.ko`、`.cmd`，卷内是纯工作树；补丁在卷内
  `git apply --check` 通过后才应用，失败即构建失败。

升级步骤：

1. 改 `kernel.xml` 的 `revision`，并在本文件里同步基线与 `KSU_GIT_VERSION`；
2. `repo sync` 后在 `source/` 上重放定制，逐条解决与上游冲突，确认上游已吸收的项不再重复；
3. 重新生成补丁（路径前缀必须是 `a/source/...`，供 `git -C <模块目录> apply` 使用）：
   ```bash
   git -C <一份干净的 KernelSU 检出> diff --src-prefix=a/source/ --dst-prefix=b/source/ \
       <旧tag>..<新定制分支> > devices/modules/kernelsu/patches/0001-shell-profile.patch
   ```
4. 按新基线 `git rev-list --count <新tag>` 更新 `Kbuild` 里的 `KSU_GIT_VERSION`；
5. 重跑 `03 → 08 → 09 → 11`，并在云机上验 `su`、shell 打标与 `KSU_VERSION`。

## 5. 构建

```bash
# 随 03 构建（清单 modules.external 里声明过 kernelsu，或命令行指定）
cd agent && go run ./cmd/cdh-pipeline build-modules --module kernelsu

# 手工编译（只需要内核源码 + 一个 O= 输出目录，参数全在模块 Kbuild 里）
mkdir -p out/devices/tmp/ksuout && make -C devices/kernels/lineage-22-6.6 O="$PWD/out/devices/tmp/ksuout" ARCH=arm64 LLVM=1 gki_defconfig
make -C devices/kernels/lineage-22-6.6 O="$PWD/out/devices/tmp/ksuout" ARCH=arm64 LLVM=1 M=devices/modules/kernelsu modules
```

产物发布到 `out/devices/kernel/<tag>/modules/kernelsu.ko`（已 `llvm-strip --strip-debug`，并做
`Module.symvers` 符号 CRC 校验）。

2026-11-02 在 `lineage-22-6.6`（6.6.127）+ `droid-ddk:android15-6.6` 上实测通过：构建日志
出现 `kernelsu: 应用 0001-shell-profile.patch` / `0002-…`、`-- KernelSU version: 32680`，
产物 `source/kernel/kernelsu.ko` 的 `vermagic = 6.6.127-4k-g4d42ee3d41c6`（与内核同代）。
`Module.symvers is missing` 的告警是「只 `modules_prepare`、没编 vmlinux」的实验环境现象，
真实构建里 02 已产出 `Module.symvers`。
