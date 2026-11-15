# `#223` `cdh_redirect` 新增 `maps` 逆向规则与 `mountinfo` `/dev/block/vda*` 挂载源掩码

- 设备/机型：`common`（全机型通用引擎能力；首次实测于 `redmi/gold` `15-OS3.0.11.0.VNQCNXM` 实例 `2`）
- 关联报告：[`gold-aligner-20261114`](../../../../report/xiaomi/gold-aligner-20261114/native_env.md)、[`proc`](../../../../report/xiaomi/gold-aligner-20261114/proc.md)
- 状态：已解决（`2026-11-14`）

## 问题

`gold_15-OS3.0.11.0.VNQCNXM`（及共用载体机制的机型）在 `aligner` 19 域对比中有两类对普通 App（`uid > cdh_max_uid(10000)`）直接可见的虚拟化暴露：

1. **`native_env` 渲染依赖暴露与文件原件偏离**：云机 64 位 GPU 渲染栈在载体中保留了三拆 `egl/libEGL_meow.so`、`egl/libGLESv1_CM_meow.so`、`egl/libGLESv2_meow.so` 及 `android.hardware.graphics.allocator@2.0/3.0/4.0.so`、`hw/gralloc.default.so`、`android.hardware.graphics.common-V6-ndk.so` 等，既出现在 App 的 `/proc/self/maps`（`loadedVendorSo.gpuSo` / `vendorSo`），也无法让 `gold` 上的 `/vendor/lib64/egl/libGLES_meow.so` 等非核心载体路径直接回到官方原件。若用常规 `dir`/`file` 规则映射，会同时改变前向查找语义，导致普通 App 或 HAL 在前向 `open/dlopen` 时找不到真实载体库。
2. **`proc` / `native_env` 块设备源暴露 `/dev/block/vda*`**：普通 App 读取 `/proc/self/mountinfo`、`/proc/mounts`、`/proc/self/mountstats` 时，直接看到 `/cust` 来自 `/dev/block/vda12`、`/metadata` 来自 `/dev/block/vda5`、`/mnt/vendor/persist` 来自 `/dev/block/vda4` 等 QEMU VirtIO 块设备名，与真机 `/dev/block/bootdevice/by-name/cust`、`/dev/block/sdc14`、`/dev/block/sdc15`、`by-name/rescue` 不符。

## 根因

1. `cdh_redirect` 原仅支持 `dir`（目录双向前缀）和 `file`（文件双向精确）两种规则，缺少「仅在 `d_path` / `readlink` 逆向出口生效、前向查找不生效」的规则类型，无法按机型把加载进进程地址空间的载体渲染库路径精确映回真机 `/vendor/lib64/...` 名单（原 `rules.c` 内置 `cdh_gpu_maps_builtin[]` 仅覆盖 Mesa/Virtio 库到 Mali/MEOW 的静态映射，无法按机型配置 `libEGL_meow.so -> libGLES_meow.so` 等跨库合并映射）。
2. `cdh_redirect/hook/mountinfo.c` 此前在 `show_mountinfo`、`show_vfsmnt`、`show_vfsstat` 返回钩子中仅隐藏 `/mnt/vendorpatch` 挂载行及重编号 peer group ID，未对保留在普通 App 视图中的 `/cust`、`/metadata`、`/mnt/vendor/persist` 等挂载行的块设备源列（`/dev/block/vda*`）按挂载点语义映射为真机设备节点。

## 证据

- 修复前差异与行为见同目录 [`log-fail.txt`](log-fail.txt)：
  - `native_env` `loadedVendorSo.gpuSo`（`24 对 23`）与 `vendorSo`（`59 对 34`）暴露云机独有 `libEGL_meow.so`、`libGLESv1_CM_meow.so`、`libGLESv2_meow.so`、`allocator@2.0/3.0/4.0`、`graphics.common-V6`、`hw/gralloc.default.so` 等；
  - `proc.mountinfo` 与 `native_env.mounts.blockMounts` 暴露 `/dev/block/vda4`（`/mnt/vendor/persist`）、`/dev/block/vda5`（`/metadata`）、`/dev/block/vda12`（`/cust`）。
- 修复后运行时与对拍证据见同目录 [`log-pass.txt`](log-pass.txt)。

## 改动

1. **`cdh_redirect` 新增 `maps` 仅逆向规则**：
   - [`include/cdh_redirect.h`](../../../modules/cdh_redirect/include/cdh_redirect.h)：`struct cdh_rule` 新增 `bool maps_only`。
   - [`core/rules.c`](../../../modules/cdh_redirect/core/rules.c)：
     - `cdh_add()` 解析 `maps` 类型（`maps_only = true`、`dir = false`）；
     - `cdh_rewrite_forward()` 在跳 UID 审计及规则循环中跳过 `rule->maps_only`；
     - `cdh_rewrite_reverse()` 优先于 `cdh_gpu_maps_builtin[]` 与普通 `dir/file` 规则匹配 `rule->maps_only`，将物理 `rule->to` 原地替换回虚拟 `rule->from`。
   - [`build/internal/device/modules.go`](../../../build/internal/device/modules.go) 与 [`modules_test.go`](../../../build/internal/device/modules_test.go)、[`carrier_mount_test.go`](../../../build/internal/device/carrier_mount_test.go)：`parseRedirectEntries()` 允许 `"maps"` 类型并校检验目标长度不小于源前缀（`len(rule.To) >= len(rule.From)`，保障逆向原地回写不越界扩展）；同步更新 Web 管理前端 [`web/static/app.js`](../../../build/web/static/app.js)。
   - `redmi/gold` 清单 `modules.redirect` 新增 9 条 `maps` 规则，将 `/mnt/vendorpatch/vendor/lib64/{egl/libEGL_meow.so,egl/libGLESv1_CM_meow.so,egl/libGLESv2_meow.so,hw/gralloc.default.so,libui.so,android.hardware.graphics.allocator@2.0..4.0.so,android.hardware.graphics.common-V6-ndk.so}` 在 `/proc/*/maps` 中分别映回 `/vendor/lib64/{egl/libGLES_meow.so,egl/libMEOW_qt.so,libgpu_aux.so,libgralloc_extra.so,libdmabufheap.so,libgralloc_metadata.so,libgralloctypes_mtk.so,arm.graphics-V5-ndk.so,android.hardware.graphics.common-V5-ndk.so}`。
2. **`mountinfo.c` 掩码 `/dev/block/vda*` 挂载源**：
   - [`hook/mountinfo.c`](../../../modules/cdh_redirect/hook/mountinfo.c)：新增 `cdh_map_vda_source()`（L203–L240），在 `uid > cdh_max_uid` 时于 `show_mountinfo`、`show_vfsmnt`、`show_vfsstat` 的返回钩子中将 `/dev/block/vda*` 按挂载点映射表重写为 `/dev/block/bootdevice/by-name/cust`（`/cust`）、`/dev/block/by-name/rescue`（`/mnt/rescue`）、`/dev/block/sdc14`（`/metadata`）、`/dev/block/sdc15`（`/mnt/vendor/persist`）、`/dev/block/sdc6`（`/mnt/vendor/nvcfg`）、`/dev/block/sdc7`（`/mnt/vendor/nvdata`）、`/dev/block/sdc18`（`/mnt/vendor/protect_f`）、`/dev/block/sdc19`（`/mnt/vendor/protect_s`），未上表项回退为 `/dev/block/sdc<N>`，配合 `cdh_replace_span()` 原位更新 `seq_file` 缓冲。

## 验证

1. **Go 单元测试**：`cd engine/build && go test ./...` 全 22 个有测包通过（含 `TestParseRedirectEntriesMaps`、`TestParseRedirectEntriesMapsRejectsShortTarget`、`TestRedirectRulesCarrierTargets`）。
2. **模块与全链路重打**：`03 -> 05 -> 07 -> 08 -> 09 -> 11` 编译 `cdh_redirect.ko`（40 个符号版本一致）并生成新 `patch.img`（sha256 `ef95bd07802d`）。
3. **非特权 App 运行态与 `aligner` 重采**：在干净盘实例 `gold_15-OS3.0.11.0.VNQCNXM/2`（`dfc3bfc1d643037a-cloud`，`dev.checker` `uid=10286`）实测：
   - `proc.mountinfo` / `proc.mounts` / `proc.mountstats` 中 `/dev/block/vda*` 匹配计数均为 **`0`**（原 `/cust`、`/metadata`、`/mnt/vendor/persist` 分别显示 `/dev/block/bootdevice/by-name/cust`、`/dev/block/sdc14`、`/dev/block/sdc15`）；
   - `native_env.loadedVendorSo` 中 `libEGL_meow.so` / `libGLESv1_CM_meow.so` / `libGLESv2_meow.so` / `allocator@2.0~4.0` / `graphics.common-V6` / `gralloc.default.so` **全部消失（云机零多余库）**，`gpuSo` 由 `24 对 23` 变为 `24 对 22`、`vendorSo` 由 `59 对 34` 变为 `59 对 33`；
   - 普通 App 读 `/vendor/lib64/egl/libGLES_meow.so` MD5 为 `e5884bc660bd75888f69209c9503325e`，与宿主官方 `super/unpack_vendor/lib64/egl/libGLES_meow.so` 一致。

## 遗留

- `native_env.vendorLibMd5.files` 仍余 4 个由载体承载、与前向运行必须挂接的 64 位垫片 MD5 不同（`/vendor/lib64/{hw/gralloc.default.so,hw/mapper.mediatek.so,hw/vulkan.mali.so,libdrm.so}`），以及 `loadedVendorSo` 仅真机侧有 `libgralloc_extra.so`、`libpqhdrmetaparser.so` 等 26 个辅助库；
- 块设备 `majMin` / `dev` 号（如 `dm-*`/`loop*` 序号）与前序问题合并保留在低优先级序列，可参考 [issue-vda-blank-ext4-partitions-and-vendor-persist-override](../issue-vda-blank-ext4-partitions-and-vendor-persist-override/solution.md) 同批次 `ext4` 分区挂载验证。
