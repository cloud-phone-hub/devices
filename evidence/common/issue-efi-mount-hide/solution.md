# #207：剔除 `/mnt/vendor/EFI`（`/dev/block/vda1`）挂载与 `by-name/EFI` 符号链接

## 问题

`gold_15-OS3.0.11.0.VNQCNXM` 首轮 `aligner.proc_full` 与 `aligner.native_env_full` 对比真机发现，云机挂载表内直接暴露了虚拟化固件分区 `/mnt/vendor/EFI`：

- `proc.mountinfo`：`dev=254:1 fsType=vfat mountPoint=/mnt/vendor/EFI source=/dev/block/vda1 superOptions=rw,context=u:object_r:efi_file:s0,...`
- `native_env.mounts.blockMounts`：`{"fstype":"vfat","majMin":"254:1","mountpoint":"/mnt/vendor/EFI","ro":false,"source":"/dev/block/vda1"}`

真机无任何 `EFI` 挂载点，且 `/dev/block/vda1` 与 `vfat` UEFI 分区结合，App 无需特权读取 `/proc/self/mountinfo` 或 `mounts` 即可直接识别虚拟化平台。

## 根因

1. `engine/build/stages/05_build_vendor_boot/fstab.go` 的 `MergeCloudFstabWithLogical` 在合并 lineage `fstab.virtio` 时，只过滤了 `patch.img`（`vdc`），将 lineage 自带的 `/mnt/vendor/EFI` 挂载行原样保留并写进了 `first_stage_ramdisk` fstab，导致二阶段挂载进 Android 命名空间（同时 `fstabMergeRevision` 未推进版本）。
2. `engine/build/stages/07_build_vendor/fstab.go` 复制出的 lineage 板级 `init.virt.rc` / `init.<board>.rc` 里包含 `symlink /dev/block/vda1 /dev/block/by-name/EFI`、`symlink /dev/block/by-name/EFI /dev/block/by-name/EFI_{a,b}` 与 `mkdir /mnt/vendor/EFI ...`，未做清理。

## 证据

- 修复前：`4503ac70a1c898f3-cloud` `proc.mountinfo` 与 `native_env.blockMounts` 命中 `/mnt/vendor/EFI`（`/dev/block/vda1`），见同目录 [`log-fail.txt`](log-fail.txt)。
- 修复后：Go 单测（`TestMergeCloudFstabDropsEFIMount`、`TestSanitizeCloudInitRCDropsEFIAndPrefixesServices`）+ 实例 `2` shell 与 `aligner.proc_full` / `aligner.native_env_full` 重采验证见同目录 [`log-pass.txt`](log-pass.txt)。

## 改动

1. **`05_build_vendor_boot` 剔除 EFI fstab 挂载**：
   - `engine/build/stages/05_build_vendor_boot/fstab.go`：将 `fstabMergeRevision` 从 `merge-v10` 升为 `# fstab-rev: merge-v11` 以使旧缓存失效；在 `MergeCloudFstabWithLogical` 遍历 lineage fstab 记录时跳过 `record.mount == "/mnt/vendor/EFI"`；
   - `engine/build/stages/05_build_vendor_boot/vendor_boot_test.go`：新增 `TestMergeCloudFstabDropsEFIMount`。
2. **`07_build_vendor` 清洗 `init.virt.rc` 与板级 `init.<board>.rc`**：
   - `engine/build/stages/07_build_vendor/fstab.go`：`placeVendorInitRC` 对 `init.virt.rc`（不再仅限 `Density > 0`）与由 `init.virtio.rc` 改名的目标 `init.<board>.rc` 调用 `sanitizeCloudInitRC`，移除 `symlink /dev/block/vda1 /dev/block/by-name/EFI`、`EFI_a`/`EFI_b` 软链以及 `mkdir /mnt/vendor/EFI*` 行；
   - `engine/build/stages/07_build_vendor/fstab_test.go`：新增 `TestSanitizeCloudInitRCDropsEFIAndPrefixesServices`（同时验证剔除 EFI 与 [`../issue-init-svc-vendor-prefix-hide/`](../issue-init-svc-vendor-prefix-hide/solution.md) 的 `vendor.` 服务名前缀）。

## 验证

1. 单测：`cd /Volumes/work/cloud-phone-hub/engine/build && go test -v -run 'TestMergeCloudFstabDropsEFIMount|TestSanitizeCloudInitRCDropsEFIAndPrefixesServices' ./stages/05_build_vendor_boot ./stages/07_build_vendor` 全绿（`0.00s`）。
2. 实例 `gold_15-OS3.0.11.0.VNQCNXM/2`（`4d5128bd8ca07462-cloud`）全链路重跑后实机复测：
   - shell 验证 `mount | grep EFI` 无结果（`none ...` 非 EFI），`ls /mnt/vendor/EFI` 与 `ls /dev/block/by-name/EFI*` 均报 `No such file or directory`；
   - `aligner.proc_full` 与 `aligner.native_env_full` 重采全量扫描 `EFI` 命中均为 `False` / `[]`（全局扫描 `vda1` 仅命中 `254:12` `/cust` 的 `/dev/block/vda12` 前缀子串，`/dev/block/vda1` 已完全消失）。

## 遗留

1. `/dev/block/vda*`、`/dev/block/dm-2` vs 真机 `dm-33`（动态分区/dm 数量差异）、`/mnt/vendor/persist` 仍为 `vfat` 等其余挂载项差异属 `proc` / `native_env` P1 收口范畴，不在本条目范围。
