# #246：消除 `gold` `cppreopts` 100 秒超时与 `/dev/block/bootdevice/by-name/cust` 5 秒等待卡顿

## 问题

`gold_15-OS3.0.11.0.VNQCNXM`（`out/core/daemon/runtime/gold_15-OS3.0.11.0.VNQCNXM/1`）冷启到 `sys.boot_completed=1` 耗时高达 **172.71 秒**（加宿主 QEMU/EDK2 引导约 178 秒）：

1. `PackageManagerService` 在 `boot_progress_pms_start`（`20,533 ms`）之后卡死整整 **100.075 秒**，直到 `120,608 ms` 打出 `am_wtf: [...,PackageManager,cppreopt did not finish!]` 才进入 `boot_progress_pms_system_scan_start`；
2. `init` 在 `on fs` 阶段执行 `wait /dev/block/bootdevice/by-name/cust` 固定超时阻塞 **5,000 ms**（`3.658s -> 8.664s`）。

## 根因

1. **空白重定向删掉了 `sys.cppreopt=finished` 回填触发器**：
   - 官方 `/vendor/build.prop`（第 714 行）设置了 `ro.cp_system_other_odex=1`（覆盖 `/system_ext/etc/build.prop:26` 的 `0`），且 `ro.postinstall.fstab.prefix=/system`；
   - [`#222`](../issue-gold-cppreopts-hide-and-vendor-init-svc-align/solution.md) 为隐藏 `init.svc.cppreopts=stopped`，将 `/system/etc/init/cppreopts.rc` 重定向到空白的 `/mnt/vendorpatch/vendor/etc/init/cph_empty.rc`，把原本的 `on property:sys.cppreopt=requested ... -> setprop sys.cppreopt finished` 一并消除；
   - 结果首启时 `PackageManagerService` 见 `ro.cp_system_other_odex=1` 仍设置 `sys.cppreopt=requested` 并死等 100 秒超时（`sys.cppreopt=timed-out`）。
2. **`cust` 缺少 `bootdevice/by-name` 前置符号链接**：
   - `first_stage_mount` 已在 `0.8s` 按 `/dev/block/by-name/cust`（`vda12`）成功挂载 `/cust`，但 `/system_ext/etc/init/init.miui.cust.rc` 在 `on fs` 按 `${ro.miui.block_device_path}/cust`（即 `/dev/block/bootdevice/by-name/cust`）执行 `wait`；
   - 原 [`frp_symlink.rc`](../../../manifest/configs/redmi/gold/gold-android15/vendor/etc/init/frp_symlink.rc) 仅在 `on post-fs-data` 建了 `frp` 链接，未在 `on fs` 之前补建 `cust` 符号链接。

## 证据

- 改前实测（`boot_progress_pms_start: 20533` → `120608`，`sys.cppreopt=timed-out`，`init: wait for '/dev/block/bootdevice/by-name/cust' timed out and took 5000ms`，`sys.boot_completed=1 @ 172.71s`）见 [`log-fail.txt`](log-fail.txt)。
- 改后实测（`boot_progress_pms_start: 14444` → `14575`，`sys.cppreopt=finished`，`init.svc.cppreopts` 为空，`wait for '/dev/block/bootdevice/by-name/cust' took 0ms`，`sys.boot_completed=1 @ 73.73s`）见 [`log-pass.txt`](log-pass.txt)。

## 改动

1. **新增 [`vendor/etc/cdh_init_override/cppreopts.rc`](../../../manifest/configs/redmi/gold/gold-android15/vendor/etc/cdh_init_override/cppreopts.rc)**：
   - 不定义 `service cppreopts`（继续保持 `#222` 的 `init.svc.cppreopts` 不可见对齐），仅保留 `on property:sys.cppreopt=requested` → `setprop sys.cppreopt finished`。
2. **修改 [`module/config.json`](../../../manifest/configs/redmi/gold/gold-android15/module/config.json#L266-L270)**：
   - 将 `/system/etc/init/cppreopts.rc` 的 `cdh_redirect` 目标从 `/mnt/vendorpatch/vendor/etc/init/cph_empty.rc` 改为 `/mnt/vendorpatch/vendor/etc/cdh_init_override/cppreopts.rc`（规则总数不变）。
3. **新增 [`vendor/etc/init/cph_gold_cust_symlink.rc`](../../../manifest/configs/redmi/gold/gold-android15/vendor/etc/init/cph_gold_cust_symlink.rc)**：
   - 在 `on init`（早于 `on fs`）创建 `/dev/block/bootdevice/by-name/cust -> /dev/block/by-name/cust`。

## 验证

1. 重跑 `cdh-pipeline device build gold --manifest 15-OS3.0.11.0.VNQCNXM` 的 `build_vendor_boot`、`build_vendor`、`build_patch`、`pack_vda`（`patch.img` sha256 `08c2bbac2506`），删除并重建干净实例 `gold_15-OS3.0.11.0.VNQCNXM/1`：
   - `getprop init.svc.cppreopts` 仍为空（`#222` 不回退）；
   - `getprop sys.cppreopt` = `finished`；
   - `boot_progress_pms_start` → `boot_progress_pms_system_scan_start` 从 **`100.075s` 降至 `0.131s`**；
   - `init: wait for '/dev/block/bootdevice/by-name/cust'` 从 **`5000ms` 降至 `0ms`**，`cust` 挂载与 `ro.miui.cust_variant=cn` 正常；
   - `sys.boot_completed=1` 总耗时从 **`172.71s` 降至 `73.73s`**（`wm_boot_animation_done` 从 `172.07s` 降至 `73.05s`，减少 **~99 秒 / 57%**）。

## 遗留

1. 剩余 ~73.7s 冷启耗时的主要构成为首启 `PreInstallApp` 拷贝 `/cust` 17 个预装应用 + MIUI 可卸载应用与 `artd` 编译（`vdb` 写入约 9.29 GiB），以及期间触发的 2 次 `nativehang-dumpstate`（共约 15.3s）；若需进一步压缩至 40s 内，可后续评估 `nativehang-dumpstate` 屏蔽或预置 data 基线优化。
