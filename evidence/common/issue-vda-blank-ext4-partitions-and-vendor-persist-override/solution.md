# `#224` `vda` 装配空白 `ext4` 分区与 `fstab` 接管覆盖 `/mnt/vendor/persist` 及 MTK NV 分区

- 设备/机型：`common`（通用 `vda.partitions` + `merge-v13` 阶段能力；首次实测于 `redmi/gold` `15-OS3.0.11.0.VNQCNXM`）
- 关联报告：[`gold-aligner-20261114`](../../../../report/xiaomi/gold-aligner-20261114/proc.md)、[`native_env`](../../../../report/xiaomi/gold-aligner-20261114/native_env.md)
- 状态：已解决（`2026-11-14`）

## 问题

`gold_15-OS3.0.11.0.VNQCNXM` 在 `proc` 与 `native_env` 对拍中存在两批存储挂载能力差异：

1. `/mnt/vendor/persist` 在真机上为 `ext4`（`rw,nosuid,nodev,noatime` / `nodelalloc,noauto_da_alloc,commit=1`），而 lineage-22.2 底盘将 GPT slot 4 `persist`（`vda4`）格式化为 `vfat` 并以 `sync,context=u:object_r:persist_file:s0,uid=1000,gid=1000,fmask=0117,dmask=0007` 挂载，导致 4 项 `fsType`/`options`/`superOptions` 差异；按 `AGENTS.md §8` 与底盘约束，不宜就地修改/篡改 lineage 原生分区镜像或 GPT 槽位内容。
2. 云机完全缺少真机已挂载的 5 个 `ext4` 持久化/保护分区：`/mnt/vendor/protect_f`（`8 MiB`）、`/mnt/vendor/protect_s`（`8 MiB`）、`/mnt/vendor/nvdata`（`64 MiB`）、`/mnt/vendor/nvcfg`（`32 MiB`）与 `/mnt/rescue`（`64 MiB`）。

## 根因

1. `11_pack_vda` 原仅支持将现成镜像文件（如 `cust.img`）追加进 `vda.img` 尾部 GPT 分区，尚不支持按清单声明式参数动态生成带特定根目录 POSIX 权限位（`mode`/`uid`/`gid`）及 SELinux 扩展属性（`security.selinux`）的空白 `ext4` 分区镜像。
2. `05_build_vendor_boot` 的 `MergeCloudFstabWithLogical`（原 `merge-v12`）在合并 lineage `fstab.virtio` 与 `vda.partitions` 时，未支持 `vda.partitions` 对相同挂载点按优先接管覆盖基座 `vfat /mnt/vendor/persist` 行，也未透传自定义 `ext4` 挂载参数（`noatime,nosuid,nodev,noauto_da_alloc,commit=1,nodelalloc`）及 `first_stage_mount,nofail` 标志。

## 证据

- 修复前基线差异与日志见 [`log-fail.txt`](log-fail.txt)：
  - `/mnt/vendor/persist` `fsType` 为 `vfat`（真机 `ext4`）、`options` 为 `rw,relatime`、`superOptions` 携带 `vfat` `fmask/dmask`；
  - `/mnt/rescue`、`/mnt/vendor/{protect_f,protect_s,nvdata,nvcfg}` 在云机侧均报告 `<缺>`。
- 修复后构建与运行态验收见 [`log-pass.txt`](log-pass.txt)。

## 改动

1. **清单 schema 扩展**：
   - [`build/internal/manifest/manifest.go`](../../../build/internal/manifest/manifest.go) 与 [`manifest_test.go`](../../../build/internal/manifest/manifest_test.go)：为 `VDAPartition` 新增空白 `ext4` 分区所需字段（`name`、`fstype`、`size`、`options`、`flags`、`mode`、`uid`、`gid`、`label`）与合法性校验。
2. **`05_build_vendor_boot` 升级至 `# fstab-rev: merge-v13`**：
   - [`build/stages/05_build_vendor_boot/fstab.go`](../../../build/stages/05_build_vendor_boot/fstab.go) 与 [`vendor_boot_test.go`](../../../build/stages/05_build_vendor_boot/vendor_boot_test.go)：
     - `vdaMounts()` / `lineVDA()` 将清单 `vda.partitions` 渲染为 `/dev/block/by-name/<name> <mount> <fstype> <options> <flags>`；
     - `MergeCloudFstabWithLogical()` 在接入 `extras` 后，自动剔除来自 vendor/lineage 同挂载点的旧行，使 `vendor_persist` 干净接管 `/mnt/vendor/persist`（保留基座 GPT 4 `persist` 原样不动）；同时保留其它分区逻辑（见 [`issue-userdata-f2fs-geometry-and-metadata-opts`](../issue-userdata-f2fs-geometry-and-metadata-opts/solution.md)）。
3. **`11_pack_vda` 空白 `ext4` 分区生成器**：
   - 新增 [`build/stages/11_pack_vda/ext4gen.go`](../../../build/stages/11_pack_vda/ext4gen.go) 与 [`ext4gen_test.go`](../../../build/stages/11_pack_vda/ext4gen_test.go)，并在 [`plan.go`](../../../build/stages/11_pack_vda/plan.go)、[`vda.go`](../../../build/stages/11_pack_vda/vda.go) 集成：
     - `ensureBlankExt4()` 按 `specKey()` 创建稀疏镜像，调用 `mke2fs -t ext4 -O ^metadata_csum ...` 生成空白 `ext4`；
     - `applyBlankExt4Meta()` 与 `verifyBlankExt4Meta()` 通过 `debugfs` 设置并回读复核根 inode 权限（`mode`/`uid`/`gid`）及 `ea_get -V / security.selinux`；
     - `runBlankE2fsck()` 执行 `e2fsck -fy` 修复一致性，随后在步骤 5 追加进 `vda.img` GPT 尾部并重建 backup GPT（参见 [issue-cdh-redirect-maps-rule-and-vda-mount-mask](../issue-cdh-redirect-maps-rule-and-vda-mount-mask/solution.md) 对应用侧源节点掩码）。
4. **`redmi/gold` 清单配置**：
   - [`gold-android15.json`](../../../manifest/configs/redmi/gold/gold-android15.json) `buildVda.partitions` 新增 6 个 `ext4` 分区：`vendor_persist`（`32 MiB`, `0771`, `1000:1000`, `u:object_r:persist_file:s0`）、`protect_f`（`8 MiB`, `0770`, `u:object_r:mnt_vendor_file:s0`）、`protect_s`（`8 MiB`, `0770`, `u:object_r:mnt_vendor_file:s0`）、`nvdata`（`64 MiB`, `0771`, `u:object_r:mnt_vendor_file:s0`）、`nvcfg`（`32 MiB`, `0771`, `u:object_r:mnt_vendor_file:s0`）、`rescue`（`64 MiB`, `0770`, `u:object_r:cache_file:s0`，`options: "noatime"`）。

## 验证

1. **Go 单元测试**：`TestMergeCloudFstabVDAExtrasOverridePersist`、`TestEnsureBlankExt4GenerateReuseAndRebuild`、`TestRunAppendsVDAPartition` 等全部 `PASS`；`engine/build` 全 22 包全绿。
2. **`11_pack_vda` 构建实测**：生成 `vendor_persist.img`..`rescue.img` 共 6 份空白 `ext4` 并追加入 `vda.img`（`13.00 GiB -> 14.21 GiB`，GPT LBA 29364224–29790207，`vda13..vda18`），全部回读一致。
3. **运行态与 `aligner` 19 域重采**（实例 `dfc3bfc1d643037a-cloud`）：
   - 设备内 6 个挂载点全部以 `ext4` 及指定 `options` / `seclabel` 成功挂载，`/dev/block/vda4`（原 `vfat`）未占用 `/mnt/vendor/persist`；
   - `proc` `capability 29 -> 22`、`fatal 137 -> 124`，`native_env.mounts.blockMounts` 缺失 5 分区与 `persist vfat` 全部清零（总挂载数 `127 对 125`）。

## 遗留

- 新追加分区的设备主次号 `dev` / `majMin`（如 `254:13..15`、`259:0..2` vs 真机 `8:38`、`8:39`、`259:2/3/41`）仍随虚拟机内核磁盘分配次序自然不同，归属低优先级底层块号差异；
- 原有的 `17` 条 `/data` 及 `pass_through` `superOptions` 仅因 QEMU 无硬件内联加密刻意保留省略 `inlinecrypt`。
