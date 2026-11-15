# `#230` `dev.mnt.*` 挂载块属性归一化、不可 `stat` 分区 `maj:min` 与 `dm-*` `f2fs` `,inlinecrypt` 对齐

- 设备/机型：`common`（`service_daemon` 机型档案规则与 `cdh_redirect` 通用挂载视图机制；`redmi/gold` `15-OS3.0.11.0.VNQCNXM` 首次实测闭环）
- 关联报告：[`gold-aligner-20261114/proc`](../../../../report/xiaomi/gold-aligner-20261114/proc.md)、[`build_props`](../../../../report/xiaomi/gold-aligner-20261114/build_props.md)、[`native_env`](../../../../report/xiaomi/gold-aligner-20261114/native_env.md)、关联条目 [`#223`](../issue-cdh-redirect-maps-rule-and-vda-mount-mask/solution.md) 与 [`#224`](../issue-vda-blank-ext4-partitions-and-vendor-persist-override/solution.md)
- 状态：已解决（`2026-11-15`）

## 问题

在 [`#224`](../issue-vda-blank-ext4-partitions-and-vendor-persist-override/solution.md) 补齐 `/mnt/vendor/persist`、`protect_f`、`protect_s`、`nvdata`、`nvcfg`、`rescue` 6 个空白 `ext4` 分区及 [`#218`](../issue-userdata-f2fs-geometry-and-metadata-opts/solution.md) 切换 `/data` 为 `f2fs` 后，`build_props`、`proc` 与 `native_env` 仍遗留三项与存储挂载视图强相关的设备号及选项偏差：

1. **15 项 `dev.mnt.*` 属性暴露 `vda*`**：[`#224`](../issue-vda-blank-ext4-partitions-and-vendor-persist-override/solution.md) 新增挂载了 `vda14..vda18` 后，`vold` 生成了对应的 `dev.mnt.{blk,dev,rootdisk}.mnt.*=vda14..vda18 / vda` 属性，而真机为 `sdc6`/`sdc7`/`sdc18`/`sdc19`/`sdc57 / sdc`；同时若存在 `dev.mnt.*.mnt.vendorpatch` 载体痕迹亦需清理。
2. **7 个不可 `stat` 分区 `maj:min` 偏差**：普通 App 无法直接 `stat("/dev/block/...")` 这 7 个系统/NV/元数据挂载点对应块节点（`/metadata`、`/mnt/vendor/{persist,protect_f,protect_s,nvdata,nvcfg}`、`/mnt/rescue`），但在 `/proc/self/mountinfo` 中云机输出 `254:5`、`254:13..15`、`259:0..2`，与真机 `8:46`、`8:47`、`259:2`、`259:3`、`8:39`、`8:38`、`259:41` 存在 7 项 `dev`/`majMin` 差异。
3. **`/data` 及其 17 条叠挂 `dm-*` `f2fs` 缺少 `,inlinecrypt`**：因主机 QEMU 侧无硬件 inline-crypto keyslot，[`#218`](../issue-userdata-f2fs-geometry-and-metadata-opts/solution.md) 切 `/data` 为 `f2fs` 时在真实 `fstab` 中移除了 `inlinecrypt` 选项，导致 `mountinfo`/`mounts`/`mountstats` 全部 17 条 `/data` 相关行 `superOptions` 均与真机少一个 `,inlinecrypt`（`proc` 与 `native_env` 各自报告十余条差异）。

## 根因

1. [`props/profile.rs`](../../../vendor/hardware/android.service.daemon/src/rust/src/props/profile.rs) 中 `MOUNT_REWRITES_GOLD` 早先仅登记了 `cust`、`metadata`、`mnt.vendor.persist` 与 `data` 后缀，未覆盖 `#224` 新加入的 `mnt.rescue` 与 `mnt.vendor.{nvcfg,nvdata,protect_f,protect_s}`；同时 [`props/scrub.rs`](../../../vendor/hardware/android.service.daemon/src/rust/src/props/scrub.rs) 的 `DROP_MOUNT_SUFFIXES` 未包含 `"mnt.vendorpatch"`。
2. [`cdh_redirect/hook/mountinfo.c`](../../../modules/cdh_redirect/hook/mountinfo.c) 的 `cdh_map_vda_source()` 仅改写 `mountinfo` 源设备列，未对上述 7 个不可从普通 App 交叉验证真实块设备的挂载点归一化第 3 列 `maj:min`，也未处理 `f2fs` `super_options` 的 `,inlinecrypt` 视图对齐。

## 证据

- 修复前基线摘录见 [`log-fail.txt`](log-fail.txt)：`vm-shell-baseline.txt` 中 15 项 `dev.mnt.*=vda*`，以及 `batch4-risk-summary.txt` §4 中 7 个分区的 `dev` 偏差与 `/data` `dm-*` 缺少 `,inlinecrypt`。
- 修复后实测见 [`log-pass.txt`](log-pass.txt)。

## 改动

1. **`dev.mnt.*` 持久化块设备属性归一化（[`profile.rs`](../../../vendor/hardware/android.service.daemon/src/rust/src/props/profile.rs)、[`scrub.rs`](../../../vendor/hardware/android.service.daemon/src/rust/src/props/scrub.rs)）**：
   - 在 `profile.rs` `MOUNT_REWRITES_GOLD`（L20–L39）补充 `mnt.rescue -> sdc57`、`mnt.vendor.nvcfg -> sdc6`、`mnt.vendor.nvdata -> sdc7`、`mnt.vendor.protect_f -> sdc18`、`mnt.vendor.protect_s -> sdc19`；
   - 在 `scrub.rs` `DROP_MOUNT_SUFFIXES`（L103–L136）加入 `"mnt.vendorpatch"`，由 `scrub_virtual_mount_props()` 将全量 24 项 `dev.mnt.*` 映射至 `sdc*` 并删除系统只读/载体挂载属性；新增 `gold_mount_rewrites_cover_all_vendor_and_rescue_partitions` 单元测试。
2. **不可 `stat` 分区 `maj:min` 与 `dm-*` `f2fs` `,inlinecrypt` 挂载表归一化（[`hook/mountinfo.c`](../../../modules/cdh_redirect/hook/mountinfo.c)）**：
   - 新增 `cdh_map_unstatable_dev()`（L242–L300）：仅对普通 App 读取 `/proc/*/mountinfo` 时生效，针对普通 App 不可直接 `stat` 块设备节点的 7 个挂载点，将 `maj:min` 对齐为真机值：`/metadata`（`8:46`）、`/mnt/vendor/persist`（`8:47`）、`/mnt/vendor/protect_f`（`259:2`）、`/mnt/vendor/protect_s`（`259:3`）、`/mnt/vendor/nvdata`（`8:39`）、`/mnt/vendor/nvcfg`（`8:38`）、`/mnt/rescue`（`259:41`）；
   - 新增 `cdh_inject_dm_f2fs_inlinecrypt()`（L425–L605）：在普通 App 读 `mountinfo`、`mounts` 与 `mountstats` 时，匹配挂载点关联 `/dev/block/dm-*` 且文件系统为 `f2fs` 的行，在 `superOptions` 中的 `,alloc_mode=` 之前按需插入 `,inlinecrypt`（若已包含则幂等跳过）。

## 验证

1. **构建与单测说明**：
   - [`scrub.rs`](../../../vendor/hardware/android.service.daemon/src/rust/src/props/scrub.rs) 新增单元测试 `gold_mount_rewrites_cover_all_vendor_and_rescue_partitions`（注：在 macOS 宿主直接执行 `cargo test` 会因 `resetprop/build.rs` 报错 `resetprop only supports android target` 返回退出码 101，该 Rust 代码已在 `build` 流水线经 Android NDK 交叉编译随 `service_daemon` 落盘实测）。
   - `cd engine/build && TMPDIR=/Volumes/work/cloud-phone-hub/engine/out/tmp go test ./...` 全 22 包 `ok`，`patch.img`（sha256 `eb8d477e11ce`）完成重打。
2. **实例 `gold_15-OS3.0.11.0.VNQCNXM/3`（`61f8a3a6a912df72-cloud`）运行时验证**：
   - `getprop dev.mnt` 输出 24/24 项全部指向 `sdc*`（`sdc6`/`sdc7`/`sdc14`/`sdc15`/`sdc18`/`sdc19`/`sdc56`/`sdc57`，`rootdisk.* = sdc`），`vda` 与 `mnt.vendorpatch` 命中均为 `0`；
   - `core3-verify.txt` 验证：`mountinfo lines=182 /dev/block/vda=0 vendorpatch=0 dm_f2fs=17 with_inlinecrypt=17`；
   - `aligner` 19 域重采（`run-00222` ~ `run-00240`，`2026-11-15`）：`proc` `fatal 122 -> 92 (-30)`、`native_env` `fatal 225 -> 162`，7 项 `maj:min`、17 项 `f2fs` `,inlinecrypt` 及 15 项 `dev.mnt.*=vda*` 全部清零。

## 遗留

- `storage`、`proc`、`native_env` 中可被 App 直接比对或随只读逻辑分区动态分配的设备序号（如 `dm-*`、`loop*` 序号与 `/data` `fsid` 设备号偏移 `65105` vs `64816`），因与内核实际分配的设备拓扑强耦合，保留为低优先级跟踪项。
