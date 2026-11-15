# issue: m22 内核源码与「要跑的那颗内核」对齐（清单内核事实 + 6.12 源码 + CDH 补丁）

## 1. 问题

m22 的 02–05 四阶段阻塞，`boot.img` / `vendor_boot.img` 一直没有产物：

1. `devices/kernels/lineage-23-6.12` 是空目录，02 找不到源码；
2. 清单只有官方 ROM 内核事实（`rom.vendor.kernel.uname-version` = `Linux version 6.6.66-android15-8-maybe-dirty-4k`），
   `ManifestFacts` 据此推出目录名 `lineage-23-6.6` 与镜像 `android16-6.6`——目录不存在、本机也没有这个 DDK 镜像；
3. 把 `git@github.com:cloud-phone-hub/android_kernel_virt_virtio.git` 的 `lineage-23.2`
   分支装进 `kernels/lineage-23-6.12` 后能编通，但**项目配置片段里的 `CONFIG_CDH_PIONEER_PERMISSIVE=y`
   静默失效**（该分支没有 CDH 补丁），产出的内核缺 pioneer 域强制 permissive 等三处能力。

## 2. 根因

- **内核事实指错了侧**：云机跑的是 lineage 底包内核，官方 ROM 内核只是机型事实记录。m22 这两代
  正好不同代（官方 6.6.66-android15 vs 底包 6.12.81），`ManifestFacts` 只读了 `rom.vendor.kernel`，
  于是镜像标签、源码目录名、模块目录名全部跟着官方内核走。
- **源码目录是缺件**，不是路径 bug。
- **补丁不是「基底分支自带」的**：CDH 三处能力（pioneer permissive、mac80211_hwsim 假扫描/`hvc10`
  数据面直桥、virtgpu host-visible blob 16 KiB 对齐）是同仓库设备分支上的提交，`lineage-23.2`
  是纯上游基底，不含这些提交。

## 3. 证据

**底包内核就是这条分支编的**（最强证据）：底包 `vendor_dlkm_a.img` 里任一 `.ko` 的 vermagic：

```
$ /opt/homebrew/opt/e2fsprogs/sbin/debugfs -R "dump /lib/modules/9pnet_fd.ko /tmp/9p.ko" \
      out/roms/lineage/lineage-23.2/super/vendor_dlkm_a.img
$ strings -a /tmp/9p.ko | grep vermagic
vermagic=6.12.81-4k-g4f6bf47200d9 SMP preempt mod_unload modversions aarch64
```

`g4f6bf47200d9` 正是 `lineage-23.2` 分支 HEAD（`4f6bf47200d9cf735e65cad80823295d2141982c`），
即「底包内核 = 该分支 6.12.81 编出来的」。

**旧口径的失败**（清单去掉 `rom.lineage.kernel` 后复现，见 `log-fail.txt`）：

```
[err] 无法确定内核源码：请用 --source 或 CDH_KERNEL_SOURCE 指定
      （kernels/ 下可用: kernels/lineage-22-6.6, kernels/lineage-23-6.12）
```

它去找的是 `kernels/lineage-23-6.6`——官方内核代际，不是我们要跑的 6.12。

**补丁缺失的静默性**：首编（纯 `lineage-23.2`）后配置只剩

```
CONFIG_VIRTIO_CONSOLE=m
```

片段里要的 `CONFIG_CDH_PIONEER_PERMISSIVE=y` 整条消失（Kconfig 里没有这个符号，`merge_config`
不报错，`olddefconfig` 直接丢弃）。同一时刻 `grep -rn "CDH_PATH_REDIRECT\|CDH_PIONEER_PERMISSIVE"
kernels/lineage-23-6.12` 空输出，证明源码侧确实没有。

**补丁来源与取舍**（同仓库 `15R-16` 分支，`git ls-remote` 实测 `6e8576671a43…` = `refs/heads/15R-16`）：

| 提交 | 内容 | m22 采用 |
|---|---|---|
| `349c5c2` | virtgpu_vram host-visible blob 16 KiB 对齐（Apple Silicon 宿主映射粒度） | ✅ |
| `de160d2` | selinux: pioneer 域内核强制 permissive（`CDH_PIONEER_PERMISSIVE`） | ✅ |
| `6e85766` | wifi: hwsim 假扫描/假连接 + 数据面直桥 `hvc10`（控制面 `hvc9`） | ✅ |
| `9c689a8` + `7ca90ec` | 加、又删 `/vendor → /mi_ext` 路径重定向 | ❌ 相抵，不引入 |
| `129c4e4` | 记录 15R-16 设备元数据（`CDH.md`、`cdh/device.json`） | ❌ v1 设备事实，不搬 |

graft 后相对 `lineage-23.2` 的净差异：4 个文件、+1122 行，无任何设备元数据文件。

## 4. 改动

- `build/internal/kbuild/kbuild.go` `ManifestFacts`：内核版本改「优先 `rom.lineage.kernel`
  （`version` 或 `uname-version`），缺省回退 `rom.vendor.kernel.uname-version`」；Android 主版本不变。
- `manifest/flyme/m22/baselines/12.5.0.0A.json` `rom.lineage.kernel`：记录实测事实
  （`version` 6.12.81、`vermagic` 原文、`source` 仓库+分支+commit、`measured-from` 出处）。
- `devices/kernels/lineage-23-6.12`：`git clone --depth 1 -b lineage-23.2`（1.9 GiB / 87,269 文件），
  再 cherry-pick 上表 3 个提交（本地分支 `lineage-23.2`，HEAD `bccd296879a5`）。
- 新增 `build/internal/kbuild/manifest_test.go`：底包内核优先、`uname-version` 等价写法、回退官方内核三个用例。

## 5. 验证

- 计划：`==> 内核源码: kernels/lineage-23-6.12（清单推导 lineage-23-6.12）`、`==> 构建镜像:
  ghcr.io/feicong/droid-ddk:android16-6.12（清单推导 android16-6.12）`、`release 6.12.81-4k-cdh`。
- 02 产物：`Image` 43,969,024 B、`config` 2450 个 `CONFIG_*`、`CONFIG_CDH_PIONEER_PERMISSIVE=y`。
- 03 产物：10 个 `.ko`，`vermagic=6.12.81-4k-cdh` 与内核 `kernelrelease.txt` 逐字一致。
- 04/05：`boot.img` 83,886,080 B（内核段换成自研 Image）、`vendor_boot.img` 22,395,140 B
  （`fstab.qcom` 3618 B 合并成 `first_stage_ramdisk/system/etc/fstab.sun`）。
- `go test ./internal/kbuild/` 全通过。

## 6. 遗留

- 本次 graft 时 GitHub SSH/HTTPS 均不可达（`Connection closed by UNKNOWN port 65535`、
  `SSL_ERROR_SYSCALL`），补丁对象取自同一仓库的本地克隆；提交 SHA 与 `git ls-remote` 当时抓到的
  `refs/heads/15R-16` 一致。网络恢复后应复核这 3 个提交仍在上游分支上。
- `lineage-23-6.12` 是浅克隆（`--depth 1`）+ 3 个本地提交：02 走 `git archive HEAD` 不受影响，
  但以后若要 rebase 到上游新 HEAD，需要重新拉取全历史。
- 配置片段里仍留着 `CONFIG_CDH_PATH_REDIRECT=y`，m22 这条线不引入重定向（v1 同结论：走
  lineage 分区重打包，挂载点无需改名），该行属「无对应符号、静默丢弃」，后续可考虑按机型拆分片段。
- `CONFIG_VIRTIO_CONSOLE` 在 02 容器脚本里被 `scripts/config --module` 强制为 `m`，与片段注释里的
  「内建」叙述矛盾；v1 m22 实编同样是 `m`，本次沿用未改，属待裁决项。
