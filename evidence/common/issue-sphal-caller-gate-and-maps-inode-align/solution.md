# `#228` SP-HAL 动态链接加载路由、`app_process` 原厂 4KB VNDK 回退与 `/proc/pid/maps` 设备/inode 归一化

- 设备/机型：`common`（全机型通用 `cdh_redirect` 内核机制；`redmi/gold` `15-OS3.0.11.0.VNQCNXM` 首次实测闭环）
- 关联报告：[`gold-aligner-20261114/native_env`](../../../../report/xiaomi/gold-aligner-20261114/native_env.md)、[`proc`](../../../../report/xiaomi/gold-aligner-20261114/proc.md)、前置条目 [`#223`](../issue-cdh-redirect-maps-rule-and-vda-mount-mask/solution.md)
- 状态：已解决（`2026-11-15`）

## 问题

在 [`#223`](../issue-cdh-redirect-maps-rule-and-vda-mount-mask/solution.md) 完成基础 `maps` 规则映射后，普通 App（`uid > cdh_max_uid(10000)`）侧的 `native_env` 与 `proc` 仍存在两类设备兼容性偏离：

1. **SP-HAL 共享库与原厂 `super.img` 文件哈希不一致**：普通 App 直接读取 `/vendor/lib64/hw/gralloc.default.so`、`/vendor/lib64/hw/mapper.mediatek.so`、`/vendor/lib64/hw/vulkan.mali.so`、`/vendor/lib64/libdrm.so` 计算 MD5 时，由于旧逻辑对这 4 个核心 SP-HAL 路径一律做文件打开重定向，读到的是 `/mnt/vendorpatch/vendor/lib64/...` 载体实现，与官方 `super.img` 内原厂共享库 MD5 产生 4 项差异（`vendorLibMd5`）。
2. **`/proc/self/maps` 映射元数据与页偏移偏离**：
   - 云机 `app_process64/32` 运行过程中映射的一批载体 VNDK 依赖库（如 `android.hardware.graphics.common-V5-ndk.so`、`arm.graphics-V5-ndk.so` 等）使用 16KB ELF `p_align` 编译且驻留在 `/mnt/vendorpatch`（设备号 `fe:20`、小 inode 编号）；即使仅靠路径名字符串替换还原了路径，`/proc/self/maps` 中的 `(dev, ino)` 仍暴露 `fe:20`（`7xx` 等连续载体 inode），而与官方原件的 `(fe:23, real_ino)` 及 4KB 页对齐偏移不匹配；
   - `gold` 上 `/vendor/lib64/hw/mapper.mediatek.so` 尚缺少向 `/vendor/lib64/mt6833/libpq_sec.so` 对应的第 26 条 `maps` 映射，且多条既有 `maps` 规则映回库与当前官方树加载布局需同步校准。

## 根因

1. **文件打开通路缺少调用来源上下文区分**：SP-HAL 库的实际使用分为「动态链接器 `linker64`/`libdl.so`/`libnativeloader.so` 在 `dlopen`/映射阶段的加载」与「普通 App 在非预期路径通过 `openat`/`read` 读取磁盘 `.so` 内容」。此前 `cdh_redirect` 仅在内核态按文件名白名单判断是否为 SP-HAL，不区分系统动态链接加载链与普通用户态文件读取，导致直接读取也命中载体实现。
2. **`d_path` 与 `show_map_vma` 未同步还原官方磁盘文件元数据**：在仅字符串替换路径名时，`show_map_vma` 仍直接输出载体 `vma->vm_file->f_inode` 的 `dev`（`fe:20`）与 `ino`，且 `app_process64/32` 启动期未回退加载官方 4KB 原厂 VNDK 库，导致部分 `.so` 在载体侧的映射大小与页对齐边界偏离官方文件真实长度。

## 证据

- 修复前基线摘录见 [`log-fail.txt`](log-fail.txt)：
  - `batch4-risk-summary.txt` §1 显示 4 项 `vendorLibMd5` 不一致（`gralloc.default.so`、`mapper.mediatek.so`、`vulkan.mali.so`、`libdrm.so`）；
  - `batch4-maps-vendor.txt` 显示云机 `9f0cf68f69184fc9-cloud` `/proc/self/maps` 中大量 `/vendor/lib64/*.so` 暴露 `fe:20` 与 `7xx` inode 及 16KB 对齐偏移（如 `libdrm.so` `00000000/0000c000/0001c000` vs 真机 `00000000/00008000/00016000`）。
- 修复后实测见 [`log-pass.txt`](log-pass.txt)。

## 改动

1. **`cdh_redirect` SP-HAL 动态链接加载路由鉴权（[`rules.c`](../../../modules/cdh_redirect/core/rules.c)、[`readlink.c`](../../../modules/cdh_redirect/hook/readlink.c)）**：
   - 新增 ARM64 用户态帧指针调用链回溯 `cdh_sphal_caller_allowed()`：在当前进程处于 App 范围（`uid > cdh_max_uid`）且访问 Core-4 SP-HAL 时，无锁遍历当前线程帧指针链（通过 `cdh_file_raw_path()` 获取底层未改写路径，借 per-CPU `cdh_raw_d_path_depth` 递归护栏避开 `cdh_d_path_ret`），仅当调用栈上帧落入系统动态链接与加载基础设施（`linker64`/`linker`、`libdl.so`、`libnativeloader.so` 等）时路由到载体 SP-HAL；其余常规磁盘文件读取（如哈希核对、文件结构读取）直通官方 `super.img` 原厂文件；
   - 保留 `/proc/cdh_redirect` 统计项 `sphal_blocked`（记录未走链接加载链路而回落官方原件的次数）以便运行态审计。
2. **`app_process64/32` 原厂 4KB VNDK 回退（[`rules.c`](../../../modules/cdh_redirect/core/rules.c)）**：
   - 新增无锁 RCU 进程名判定 `cdh_is_app_process()`（匹配 `app_process64`/`app_process32`）及 `tier_b_fallback[]` 18 项原厂 4KB VNDK 库清单（含 `android.hardware.graphics.common-V5-ndk.so` 等），令应用进程族优先回退消费官方 `super.img` 4KB 原厂 VNDK，消除 16KB 页对齐与文件长度偏移差异；
   - 维护 `tier_b_exact[]` 与 `cdh_gpu_maps_builtin[]`，并在 `cdh_rewrite_forward()` 入口严格执行 `maps_only` 跳过与位宽隔离。
3. **`/proc/pid/maps` 设备/inode 归一化与预热缓存（[`hook/maps.c`](../../../modules/cdh_redirect/hook/maps.c)、[`core/main.c`](../../../modules/cdh_redirect/core/main.c)、[`include/cdh_redirect.h`](../../../modules/cdh_redirect/include/cdh_redirect.h)、[`Kbuild`](../../../modules/cdh_redirect/Kbuild)）**：
   - 新增 `hook/maps.c`，挂载内核 `show_map_vma` 钩子：模块加载后通过工作队列 `cdh_maps_work_fn()`（`CDH_MAPS_PREWARM_MS=500`、`CDH_MAPS_PREWARM_TRIES=120`，命中 `/vendor/lib64`、`/vendor/lib` 关键库，含 `/vendor/lib/android.hardware.graphics.common-V5-ndk.so` 等，上限 `CDH_MAPS_CACHE_MAX=256`）预热缓存官方原厂文件的真实 `(dev, ino, size)` 元组；
   - 当普通 App 读取 `/proc/*/maps` 时，若 `vma->vm_file` 经逆向规则映回虚拟路径（包括 `maps_only` 规则或载体库映射），同步把该行的 `dev:ino`（含内置 GPU 渲染库映回表 `cdh_gpu_maps_builtin[]`）归一化为对应官方文件缓存的真实 `(dev, ino)`，并在 `/proc/cdh_redirect` 导出 `maps_ino_rewrites` 计数。
4. **`gold` 规则同步（[`gold-android15.json`](../../../manifest/configs/redmi/gold/gold-android15.json)）**：
   - 将 `modules.redirect` 增至 26 条（上限 32）：更新 5 条 `maps` 映回目标（`libgpudataproducer.so -> libui.so`、`libaal_sec.so -> allocator@2.0.so`、`libFrameRecord.so -> allocator@3.0.so`、`libpng.so -> allocator@4.0.so`、`libMEOW_data.so -> graphics.common-V6-ndk.so`），并新增第 26 条 `maps` 规则将 `/vendor/lib64/mt6833/libpq_sec.so` 与 `/mnt/vendorpatch/vendor/lib64/hw/mapper.mediatek.so` 绑定。

## 验证

1. **构建与模块单元测试**：
   - `cd engine/build && TMPDIR=/Volumes/work/cloud-phone-hub/engine/out/tmp go test ./...` 全 22 个有测包 `ok`（`internal/testkit` 为 `[no test files]`）；
   - `cdh device build gold --manifest 15-OS3.0.11.0.VNQCNXM --from build_modules`（`task-6177`，exit `0`）编出 `cdh_redirect.ko` 及 `patch.img`（sha256 `eb8d477e11ce`），`vendor_dlkm-log.txt` 满足全部 4 项 `AGENTS.md §6` 闸门。
2. **实例 `gold_15-OS3.0.11.0.VNQCNXM/3`（`61f8a3a6a912df72-cloud`）运行时验证**：
   - `dmesg` 无任何内核 BUG / Oops / atomic-sleep 告警：`cdh_redirect: maps 缓存预热完成：ready=66/68`，`/proc/cdh_redirect` 显示 `rules: 26/32`、`sphal_blocked: 0`、`maps_ino_rewrites: 0`（普通 App 态 `maps` 已走通原厂 VNDK 与预热缓存，未残留未对齐 `fe:20`）；
   - `core3-verify.txt` 验证通过：
     - `maps_lines=3810 fe:20=0 /mnt/vendorpatch=0`；
     - `checked_vendor_libs=36 CLOUD_MAP>PAGE_ALIGN(REAL_FILE)=0 CLOUD_MAP>EXACT_REAL_FILE=10 off>=real_st_size=0 ino_mismatch=0`；
     - `vendorLibMd5: cloud=4 real=4 diffs=0 []`；
     - `vendorTree diffs=0 []`；
   - `aligner` 19 域重采（`run-00222` ~ `run-00240`，`2026-11-15`）：`native_env` `fatal 225 -> 162`（原 4 项 `vendorLibMd5` 与 `/vendor/lib64/libdrm.so` 差异全清零）。

## 遗留

- `native_env.loadedVendorSo` 仍余 3 项仅云机出现的辅助库（如 `libgpudataproducer.so`）与仅真机加载的辅助库依赖，后续如需进一步收敛可单独补齐载体空库或筛选依赖；
- `ultimate.*` 对拍完成 13/14 项无云机独有风险命中，其中 `ultimate.super_scan` 在 `97.7s` 时双端均处于 `pending` 超时，需后续排查扫描工具超时原因。
