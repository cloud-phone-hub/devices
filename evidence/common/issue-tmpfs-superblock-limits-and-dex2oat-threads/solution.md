# #221：根 `tmpfs` `MS_REMOUNT` 注入 `size/nr_inodes` 上限及对齐 `dalvik.vm.background-dex2oat-threads=4`

## 问题

在 `gold_15-OS3.0.11.0.VNQCNXM` 改前 19 域对比（`7d10bffa4bc09406-cloud`）中，底层挂载与属性检查暴露两类偏离：

1. `proc` 域（`mountinfo` / `/proc/mounts`）中 `/dev`、`/mnt`、`/mnt/installer`、`/mnt/androidwritable`、`/apex`、`/apex/apex-info-list.xml`、`/bootstrap-apex`、`/bootstrap-apex/apex-info-list.xml`、`/linkerconfig`、`/linkerconfig,#2`、`/tmp`、`/storage`、`/data_mirror` 等 13 个 `tmpfs` 挂载点缺少 `size=3863628k,nr_inodes=965907` 显示参数（触发 `proc` 13 条能力差异）；
2. `build_props` 域 `dalvik.vm.background-dex2oat-threads` 为 `"8"`（来自 AOSP 系统默认值，真机为 `"4"`，已在 `/vendor/build.prop` 声明）。

## 根因

1. 真机 MIUI 内核与初始化会在根 `tmpfs` 超级块打印 `size=3863628k,nr_inodes=965907`，而云机第一阶段 / init 默认挂载 `tmpfs` 时未传显式上限参数；
2. 早期启动链使 `dalvik.vm.background-dex2oat-threads` 回落为系统默认 `"8"`，[`service_daemon`](../../../vendor/hardware/android.service.daemon/src/rust/) 的 `props/scrub.rs` 尚未按 `/vendor/build.prop` 声明覆写该属性，也未原位重挂根 `tmpfs` superblock。

## 证据

- 改前实机 `/proc/mounts`、`getprop dalvik.vm.background-dex2oat-threads` 与 `proc` / `build_props` 差异摘录见 [`log-fail.txt`](log-fail.txt)。
- 改后实例 `4abff687a02e421f-cloud` `/proc/mounts`、`getprop` 与重采验证结果见 [`log-pass.txt`](log-pass.txt)。

## 改动

1. **[`syscall.rs`](../../../vendor/hardware/android.service.daemon/src/rust/src/syscall.rs)**：定义 `MS_REMOUNT = 32` 与挂载系统调用封装。
2. **[`props/profile.rs`](../../../vendor/hardware/android.service.daemon/src/rust/src/props/profile.rs)**：为档案结构新增 `tmpfs_limits: Option<(u64, u64)>`，并在 `gold`（`mt6833`）档案配置 `Some((3_863_628, 965_907))`。
3. **[`props/scrub.rs`](../../../vendor/hardware/android.service.daemon/src/rust/src/props/scrub.rs)**：
   - 新增 `collect_tmpfs_remount_targets()` 与 `align_tmpfs_superblocks()`：解析 `mountinfo`，对尚未同时携带目标 `size=...k` 与 `nr_inodes=...` 的全部根 `tmpfs` 挂载点保留原有 VFS 标志后以 `MS_REMOUNT | vfs_flags` 执行原位重挂，注入 `size=3863628k,nr_inodes=965907` 且不破坏原有 `mode=` / `gid=` / `seclabel`（附单元测试）；
   - 在擦除阶段读取 `/vendor/build.prop` 中 `dalvik.vm.background-dex2oat-threads`（`gold-android15.json` 置为 `"4"`），若当前值不一致则通过 `set_prop_svc()` 强制同步为 `"4"`。

## 验证

1. 全量编译重打包并重建干净实例 `gold_15-OS3.0.11.0.VNQCNXM/2`（`4abff687a02e421f-cloud`）：
   - `batch2-post.log` `/proc/mounts`：`/dev`、`/mnt`、`/mnt/installer`、`/mnt/androidwritable`、`/apex`、`/bootstrap-apex`、`/linkerconfig`、`/tmp`、`/storage`、`/data_mirror` 等全部携带 `size=3863628k,nr_inodes=965907`，且原有 `mode=755`/`700`、`gid=1000` 与 `seclabel` 完整保留；
   - `getprop dalvik.vm.background-dex2oat-threads` 输出 `[4]`。
2. 19 域 `device-checker` 全量重采：`proc` 域 **13 项** `tmpfs` `superOptions` 能力差异 **全部清零**（配合 [`#218`](../issue-userdata-f2fs-geometry-and-metadata-opts/solution.md) `/metadata` 合计 `capability: 43 -> 29`）；`build_props` 域 `dalvik.vm.background-dex2oat-threads` 差异 **清零**。

## 遗留

1. 无新引入副作用；`proc` 域其余 `/mnt/vendor/persist`（`vfat -> ext4`）与 NV 分区在 P1 `#9` 后续条目继续推进。
