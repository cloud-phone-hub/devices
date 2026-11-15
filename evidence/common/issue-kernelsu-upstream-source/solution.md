# kernelsu 改为「官方源码 + 模块内构建入口 + 补丁」

## 问题

`devices/modules/kernelsu/` 里存的是一份被 fork 过的上游源码拷贝（v3.2.4 系，616 KiB，
含改造过的 `Makefile`/`Kbuild`），既看不出与上游的差异，也没有升级路径：上游发新版只能
人工 diff 一遍。用户要求两件事：

1. **直接依赖官方地址**，把 `tiann/KernelSU` 拉到 `kernelsu/source`（不落库、不 fork）；
2. **构建方式留在 kernelsu 里**，不要为它改 03 的通用流程（第一版实现给 `module.json`
   加了 `kbuild-dir`/`make-args`、给 03 加了补丁应用函数，被否）。

顺带把基线从 v3.2.4 提到 v3.3.1（用户选择最新）。

## 根因

- **编译入口被写死在通用脚本里**：03 只会 `make M=<模块目录> modules`，没有传变量、换
  编译根的通道，所以 kernelsu 只能把源码铺在模块根，再靠改 Kbuild 塞默认值——一换上游
  就得重改。
- **上游 KernelSU 的布局与外部模块假设不一致**：编译入口在仓库的 `kernel/` 子目录
  （`kernel/include/uapi → ../../uapi` 是软链），且上游 Kbuild 假定「模块目录 = 独立 git
  仓库根」、内核 `.config` 里有 `CONFIG_KSU`。本项目三条都不成立（卷内无 `.git`、
  内核不认 `CONFIG_KSU`、源码在子目录）。
- **v3.3.1 不再兼容 6.6**：`hook/syscall_hook_manager.c` 无条件调
  `register_trace_prio_sys_enter()`，该入口 6.9 才有；清单里的 6.1/6.6 编不过。
- **云机定制代码基于旧结构体**：`shell_profile.c` 用 `struct app_profile.current_uid`，
  v3.3.1 已改名 `curr_uid`；`allowlist.c` 里上游的 `#define SHELL_UID 2000` 在平移定制时
  被换成 `KSU_SHELL_UID`，第 297 行漏改。
- **模块树哈希算不出来**：`kbuild.HashTree` 把指向目录的软链当文件读，报
  `read …/include/uapi: is a directory`（03 的 dry-run 用例直接失败）。

## 证据

- `log-fail.txt`：三轮失败日志（`register_trace_prio_sys_enter` 未声明 → `current_uid`/
  `SHELL_UID` → modpost 阶段补丁幂等误报），以及两类实验环境现象（`flask.h` 缺失、
  `Module.symvers is missing`）。
- `log-pass.txt`：最终 `make M=… modules` 输出（补丁应用两行、`KernelSU version: 32680`、
  `LD [M] …/source/kernel/kernelsu.ko`）、`modinfo` 的 `vermagic 6.6.127-4k-…`、以及补丁链
  `diff -r` 与 cdh 分支一致的校验。

## 改动

**模块内（`devices/modules/kernelsu/`）**

- 新增 `Kbuild`：本模块唯一编译入口。给 `CONFIG_KSU=m`、`KBUILD_MODPOST_WARN=1`、
  `KSU_GIT_VERSION=2680`、`KSU_GIT_VERSION_VALID=1` 默认值并 `export`（子 make 才看得到）；
  `obj-$(CONFIG_KSU) += source/kernel/` 递归进上游编译根；按文件名顺序应用
  `patches/*.patch`（容器 `/tmp` 落标记保证幂等，`--check` 不过就 `$(error)`）；缺
  `source/kernel` 也直接 `$(error)`。
- `module.json` 回到通用字段（去掉 `kbuild-dir`/`make-args`）。
- `patches/0001-shell-profile.patch`（346 行，10 文件：shell profile 策略、allow_shell
  默认开、shell 打标、`faccessat2` sucompat）；`patches/0002-kernel-6.9-tracepoint-prio.patch`
  （17 行：按 `LINUX_VERSION_CODE` 分支退回 `register_trace_sys_enter`）。
- 旧源码拷贝移到 `.tmp/kernelsu-legacy-v3.2.4/` 留档，不再进仓库。

**清单与忽略**

- `manifest/kernel.xml`：新增 `github` remote 与
  `<project path="devices/modules/kernelsu/source" name="tiann/KernelSU" remote="github" revision="v3.3.1" />`。
- `engine/.gitignore`：排除 `modules/kernelsu/source/`（与 `kernels/` 同处理）。

**引擎（只改通用缺陷，不加 kernelsu 分支）**

- `03_build_modules`：撤回上一版的 `kbuild-dir`/`make-args`/`applyModulePatches`；唯一保留
  的改动是外部模块产物查找去掉 `-maxdepth 1`（整棵模块树找、数量仍要求唯一）——
  `obj-m += 子目录/` 时 `.ko` 落在编译根，否则 kernelsu 的产物找不到。
- `internal/kbuild.HashTree`：符号链接只哈希「相对路径 + 链接目标」，不跟随。其它模块
  没有软链，哈希值不变（同一字节序列口径），只有 kernelsu 从「报错」变成「可算」。

**文档**：`modules/kernelsu/README.md`（重写）、`modules/README.md`（编译入口归模块自己）、
`03_build_modules/README.md`（产物可在子目录）。

## 验证

- `lineage-22-6.6`（6.6.127）+ `droid-ddk:android15-6.6` 容器内实编通过，产物
  `source/kernel/kernelsu.ko`，`vermagic = 6.6.127-4k-g4d42ee3d41c6`（与内核同代），
  `KernelSU version: 32680`（= 30000 + 2680）。
- 补丁链等价：`v3.3.1` 工作树 apply `0001`+`0002` 后与定制分支 `diff -r` 无差异。
- 幂等：同一容器内第二次解析 Kbuild（modpost 阶段）不再重复应用补丁，也不误报基线不符。
- `engine/build`：`gofmt -l` 空、`go build ./...`、`go vet ./...`、`go test ./...` 全绿
  （`TestBuildModulesDryRun` 由之前的「HashTree 读软链目录失败」转为通过）。
- `modmeta` 的目录用例把上面这套收口钉住：`kernelsu/Kbuild` 必须含四个变量默认值 + `export`
  那一行与 `obj-$(CONFIG_KSU) += source/kernel/`，`patches/*.patch` 的路径基准必须是
  `a/source/kernel/…`——改回「产物在模块根」或把补丁路径写成模块根相对都会直接失败。
- 其它模块不受影响：产物仍在模块根、无软链，配方哈希口径未变。

## 遗留

- **6.12 / 6.1 未实编**：6.12 走 `0002` 的另一支（有 `register_trace_prio_*`），6.1 与 6.6
  同支；三条线都要在真实 03 里各跑一遍才算闭环。
- **整链与运行态未验**：`03 → 08 → 09 → 11` 与云机上 `su`、shell 打标、Manager 侧
  `KSU_VERSION` 比对都还没做；本次只证明模块能编出来且与本套内核同代。
- **卷内 modpost 用本次 `make modules` 产出的 `Module.symvers`**：KernelSU 那几个运行时
  自解析的符号仍依赖 `KBUILD_MODPOST_WARN` 降级，没有换成显式导出。
- **升级耦合**：`KSU_GIT_VERSION`、`KSU_GIT_VERSION_VALID` 两个变量名取自上游 Kbuild，
  上游改名/改语义时要同步本模块 `Kbuild`；`0002` 在「最低支持内核 ≥ 6.9」后可删。
- `source/` 目前是手工 `git clone`，manifest 接管前需先移走（与 `devices/kernels/` 同一处理）。
