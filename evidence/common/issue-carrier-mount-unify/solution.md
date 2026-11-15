# 载体落点全库统一：22-6.6 / 23-6.12 内核与机型清单仍写 /mnt/patch，与 05/09 的 /mnt/vendorpatch 错位

## 1. 问题

oriole（lineage-21-6.1）那条线已经把 patch.img 载体从 `/mnt/patch` 改到
`/mnt/vendorpatch`，构建侧 `05_build_vendor_boot` 生成的 first_stage fstab 与
`09_build_patch` 的运行态布局也都按新名字写：

```
/dev/block/vdc /mnt/vendorpatch ext4 ro first_stage_mount
```

但仓库里另外两代内核树和一部分机型清单没跟上，仍是旧口径：

- `kernels/lineage-22-6.6`（gold 线，branch `gold-15`）：`fs/namei.c` 前向/逆变换表
  9 条、`fs/namespace.c` 的 `strcmp(p, "/mnt/patch")`、`fs/Kconfig` help 全部是
  `/mnt/patch`；
- `kernels/lineage-23-6.12`（m22 / CPH2797 线，branch `lineage-23.2`）：同上，且多
  `/system/lib/modules` 与 `odm` 预编译策略两条规则；
- `manifest/oppo/cph2797/vendor/etc/init/init.virtio_compat.rc`：4 条
  `mount … /mnt/patch/vendor/etc/compat/… bind wait` 的 **source** 直指旧落点。

后果比改名之前更糟：新构建出来的盘集里 vdc 会挂到 `/mnt/vendorpatch`，而内核仍把
`/vendor/etc`、`/vendor/lib64` 等改写为 `/mnt/patch/vendor/...`——那是个不存在的
路径，连"被 vold 当 stale mount 拆"都轮不到，直接全树 ENOENT：属性区初始化、
SELinux 策略加载、`vndservicemanager`、各 HAL、cph2797 的 4 条 bind 全部失败。

## 2. 根因

落点改名是**按线**修的：只改了 oriole 用到的内核树与构建侧 05/09，没有把"载体
落点"抽成唯一来源。两代内核是各自独立的 git 仓库（分支 `gold-15` / `lineage-23.2`），
机型清单是散装文件，三者之间没有任何机制保证同步——谁改谁记得，忘了就静默错位，
而错位只在整机启动时才暴露。

## 3. 证据

- `log-fail.txt`：迁移前 `git grep` 的三棵内核树命中统计（22-6.6：Kconfig 7、
  namei.c 53、namespace.c 5、proc/base.c 1、namei.h 1；23-6.12：Kconfig 6、
  namei.c 62、其余同上），以及构建侧/清单侧的逐行命中与错位机制。
- `log-pass.txt`：迁移后 `git grep` 清零、守卫用例通过、`go test ./...` 全绿、
  `check-worktree` 与 02 内核编译结果。

## 4. 改动

**内核（两棵树同一套 5 文件）**

| 树 | 提交 | 内容 |
| :--- | :--- | :--- |
| `kernels/lineage-22-6.6` | `0192a6feab2d` | namei.c 前向 9 条 + 逆变换 9 条 + 长注释、namespace.c 卸载拦截与注释、Kconfig help、proc/base.c 与 namei.h 注释 |
| `kernels/lineage-23-6.12` | `47d441573` | 同上，另含 `/system/lib/modules`、`odm/precompiled_sepolicy` 两条规则 |
| `kernels/lineage-21-6.1` | `128aa20763b6` | 仅 Kconfig help：原文仍写"只匹配 `/vendor` 后跟 `/'` 或 `_`"与"`/mnt/patch` 才是载体"的历史叙述，与表驱动实现和新落点不符，改成与另外两棵完全一致 |

**构建侧：把落点收成一个常量**

- 新增 `build/internal/carrier/carrier.go`：`carrier.MountPoint = "/mnt/vendorpatch"`
  与 `carrier.Path(rel)`，包注释写清两条外部约束（vold 白名单前缀匹配 /
  `mnt_vendor_file` 标签）。
- `build/05_build_vendor_boot/fstab.go`：`lineVdc` 由常量拼出，lineage 自带的 vdc
  行判重、写回 fstab 的注释行都改用常量。
- `build/09_build_patch/layout.go`、`plan.go`：布局日志与"屏蔽落点竟存在"报错改用常量。

**清单与 HAL 注释**

- `manifest/oppo/cph2797/vendor/etc/init/init.virtio_compat.rc`：4 条 bind source 改
  `/mnt/vendorpatch/vendor/etc/compat/…`，并补一段"为什么叫这个名字"的说明；
- `manifest/oppo{,/cph2797}/baselines/CPH2797_…json` 的 `comment`、`manifest/flyme/m22/README.md`
  的 idc 复盘、`vendor/hardware/android.hardware.gpu/src/cdh_vulkan_mali.c` 与
  `vendor/hardware/android.service.daemon/src/rust/src/main.rs` 的注释同步。

**防回归**

- 新增 `build/test/carrier_mount_test.go`：
  - `TestCarrierMountPointUnified`：每棵带 `CONFIG_CDH_PATH_REDIRECT` 的内核树，
    `fs/namei.c` / `fs/namespace.c` 里**非注释行**的所有 `/mnt` 字面量必须落在
    `carrier.MountPoint` 之下（`sizeof(...)` 那一份也算）；
  - `TestCarrierNoLegacyPatchPath`：`engine/` 下（`evidence/`、`out/`、`roms/`、
    内核树除外）不得再出现 `/mnt/patch/` 形式的实际路径——注释里"曾经叫
    `/mnt/patch`"的历史说明允许，因为它不带结尾斜杠。
- `build/test/build_vendor_boot_test.go`：`fstabLineVdc` 改引常量；`fstabRevMarker`
  由陈旧的 `merge-v6` 更正为 `merge-v8`（这只常量此前已与 05 脱节，导致
  `TestBuildVendorBootCacheReuse`、`TestBuildVendorBootMergeLineage` 两只用例长期红）。

**不改**

- `devices/evidence/` 下的历史日志与复盘：它们是当时的事实快照，保留 `/mnt/patch`；
- `devices/` 下的旧 python 构建：已决定废弃，不再迁移。

## 5. 验证

- `go build ./...`、`go vet ./...` 通过；`go test ./...` 全绿（含新增两只守卫用例，
  并顺带修好两只陈旧用例）。
- 迁移后逐文件计数与迁移前**完全相等**（22-6.6 为 `53/5/7/1/1`、23-6.12 为
  `62/5/6/1/1`，依次是 namei.c / namespace.c / Kconfig / proc.base.c / namei.h），
  说明既没漏改也没多改；剩下的 `/mnt/patch` 字样全是注释里的改名历史（不带结尾斜杠）。
- `TestCarrierMountPointUnified`：`载体口径一致：3 棵内核树均为 /mnt/vendorpatch`。
- `TestCarrierNoLegacyPatchPath`：`engine 内无 /mnt/patch/ 残留`。
- 三棵树 `cdh check-worktree` 通过（工作树干净、无大小写折叠风险）。
- 02 用 lineage-22-6.6 的 HEAD（`0192a6feab2d`）重新编了一轮内核（gold 配方，DDK
  `android15-6.6`，`-j10`，全量重配）：配方哈希 `4f2152cbe2de → ac3d1f410301`，
  6435 行日志零 error/零 warning，产出 `release 6.6.127-4k-cdh`、
  `Image` sha256 `21f89c9a8606…`、`CONFIG_CDH_PATH_REDIRECT=y`，`EXIT=0`。
  证明改动只动字符串与注释、不影响编译。逐条命令见 `log-pass.txt`。

整机层面**没有**复跑：13 台机型的盘集都是旧内核编出来的，要吃新口径必须按
02→…→11 重编（见遗留）。oriole 那台的正向验证见
`../../oriole/issue-zygote-bootstrap-ns/`。

## 6. 遗留

- 12 台非 oriole 机型的盘集需要重编才能生效；本文只统一了口径与源码，没有逐台
  出盘。按依赖顺序：02 编内核 → 03 编模块 → 04/05 boot 与 fstab → 07/08/09 装配
  → 10/11 打包。
- 三棵内核树的本地领先提交未推送（与既有习惯一致：`gold-15` 领先 18、
  `lineage-23.2` 领先 7、`lineage-21` 领先 4）。
- 守卫用例只覆盖 `namei.c` / `namespace.c` 的非注释行；将来若有人在其它的内核文件
  （如 `fs/open.c`）里写死载体路径，需要扩表。
- `devices/` 下的旧 python 构建仍写 `/mnt/patch`，随设备目录废弃一并消失。
