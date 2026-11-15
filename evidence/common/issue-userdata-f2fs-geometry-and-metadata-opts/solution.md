# #218：`/data` 切换厂商 `f2fs` 精密对齐几何（块数 / inode）与 `/metadata` 挂载选项

## 问题

在 `gold_15-OS3.0.11.0.VNQCNXM` 改前 19 域对比（`7d10bffa4bc09406-cloud`）中，云机 `/data` 与 `/metadata` 存在明显底层文件系统差异：

1. `/data` 使用 `ext4`（真机为 `f2fs`），`statvfs` 实测块数 `27,868,855`（`totalMb = 108,863`）、inode 数 `7,233,536`，显著偏离真机 `28,079,608` 块（`totalMb = 109,686`）、`13,511,677` inode，导致 `storage` 域暴露 66 条 `statvfs.blocks / files / totalMb` 身份差异、`proc` 域暴露 18 条 `fsType` 差异、`native_env` 暴露 17 条 `fstype` 差异；
2. `/data` 的 `fsid` 为负的实例 UUID 哈希 `-1986760211985553000`（真机 `f2fs` 按设备号输出正值 `65105`），且 `vold.has_compress = 1`（真机 `0`）；
3. `/metadata`（`ext4`）挂载选项携带 `nodioread_nolock,nodelalloc,commit=1,data=journal`，与真机 `rw,seclabel,discard` 不符。

## 根因

1. `05_build_vendor_boot` 的 `MergeCloudFstabWithLogical()` 在合并 `lineage fstab.virtio` 时保留了 `/data ext4` 优先行（使 `vdb` 按 `ext4` 挂载），且对 `/data f2fs` 与 `/metadata ext4` 直接沿用 lineage virtio 默认挂载参数（带 `data=journal,commit=1` 与 `fscompress`，缺少 `reservedsize=` / `readahead_size_kb=` / `fsverity`）。
2. `gold-android15.json` 原 `buildVda.data-size` 偏小/未按 `f2fs` + `vold` 计算模型精确折算（实测 `vold` 创建 `dm-48` 线性映射时未从块设备层面扣减 `reservedsize=128m`，`115150389248` B 多出 `32768` 块）。

## 证据

- 改前实机 `stat /data`、`/proc/mounts`、`vold.has_compress=1` 及改前 `storage` / `proc` / `ids` / `build_props` 差异摘录见同目录 [`log-fail.txt`](log-fail.txt)。
- 改后实例 `gold_15-OS3.0.11.0.VNQCNXM/2`（`4abff687a02e421f-cloud`）`batch2-post.log` 与 19 域重采差异摘录见同目录 [`log-pass.txt`](log-pass.txt)。

## 改动

1. **[`fstab.go`](../../../build/stages/05_build_vendor_boot/fstab.go)**（`fstabMergeRevision = "# fstab-rev: merge-v12"`）：
   - `MergeCloudFstabWithLogical()`：当厂商原厂 `/data` 使用 `f2fs` 时跳过 lineage `/data ext4` 优先行；当厂商 `/metadata` 未启用 `data=journal` 时通过 `stripMountOptions()` 剥除 `data=journal` 与 `commit=1`；
   - 新增 `adaptLineageDataF2fs()`：以厂商 `/data` `f2fs` 挂载选项为基底并剔除 QEMU 不支持的 `inlinecrypt`，若厂商未启用 `fscompress` 则从 `fs_mgr` 标记剥除，并并入厂商 `reservedsize=`、`readahead_size_kb=`、`fsverity` 标记。
2. **[`gold-android15.json`](../../../manifest/configs/redmi/gold/gold-android15.json)**：
   - 依据真机目标总块数 `28,079,608` 与 `f2fs` 元数据开销 `(28079608 + 512) * 4096`，将 `buildVda.data-size` 调整为 `115016171520` 并重跑 `11_pack_vda`。

## 验证

1. 全量重建 `03_build_modules -> 05_build_vendor_boot -> 07_build_vendor -> 08_build_vendor_dlkm -> 09_build_patch -> 11_pack_vda` 后微调 `data-size` 重跑 `11_pack_vda`，并干净盘新建运行实例 `gold_15-OS3.0.11.0.VNQCNXM/2`（`4abff687a02e421f-cloud`）。
2. 实机 `stat -f /data`：`Type: f2fs`、`Blocks: Total: 28079608`、`Inodes: Total: 13511677`；`getprop vold.has_compress` 变为 `0`；`/proc/mounts` `/metadata` 为 `rw,seclabel,nosuid,nodev,noatime,discard`，`/data` 挂载为 `/dev/block/dm-48 /data f2fs`。
3. 19 域 `device-checker` 全量重采对拍：`storage` 66 条 `/data` `statvfs.{blocks,files,totalMb}` 差异 **全部清零**（`fatal 97 -> 31`，`raw 353 -> 259`）；`proc` 18 条 `/data` `fsType` 与 `/metadata` `superOptions` 清零；`native_env` 17 条 `/data` `fstype` 清零；`ids.partitionFsid.{data,cache}` 从负数 UUID 转为正设备号 `64816`（对真机 `65105`）。

## 遗留

1. `/data` 超级块选项故意保留省略 `inlinecrypt` 一项（因 QEMU 虚拟块设备不支持硬件内联加密）；`/mnt/vendor/persist`（vfat 对 ext4）与块设备 `minor` 序号导致的 `fsid` 数字偏移（`64816` vs `65105`）属后续存储基座/dm 项，不阻断本条目闭环。
