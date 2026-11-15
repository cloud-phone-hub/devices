# #219：`cdh_redirect` `cdh_is_app_sphal_lib` 按 `in_compat_syscall()` 严格隔离 32/64 位 ABI

## 问题

在 `gold_15-OS3.0.11.0.VNQCNXM` 改前 `aligner.native_env_full`（`7d10bffa4bc09406-cloud`）中，64 位普通 App（`uid > 10000`）遍历或读取 `/vendor/lib/` 下 4 个 32 位库（`libdrm.so`、`hw/gralloc.default.so`、`hw/mapper.mediatek.so`、`hw/vulkan.mali.so`）时暴露异常：

- 真实设备上普通 64 位 App 可读取官方 `/vendor/lib/libdrm.so`（`md5=504e7332b6d389a850383966c1e76e8a`、`size=59096`）；
- 云机上却发生 `ENOENT`，导致 `native_env.availability.vendorReadable` 少 1 项（真机 `354` 对云机 `353`，`availability = 1`），并触发 `vendorTree.readable[/vendor/lib/libdrm.so]` 缺失。

## 根因

`cdh_redirect` 内核模块 [`rules.c`](../../../modules/cdh_redirect/core/rules.c) 中 `cdh_is_app_sphal_lib()` 对 `/vendor/lib64/` 与 `/vendor/lib/` 均无条件匹配放行重定向，没有结合当前系统调用 ABI 区分 64 位与 32 位进程：当 64 位普通 App 访问 `/vendor/lib/libdrm.so` 时也被重定向到载体 `/mnt/vendorpatch/vendor/lib/libdrm.so`，而纯 64 位构型的云机载体目录 `/mnt/vendorpatch/vendor/lib/` 下并没有放置这 4 个 32 位 SP-HAL 垫片，遂返回 `ENOENT`。

## 证据

- 改前 `native_env` `vendorTree.readable[/vendor/lib/libdrm.so]` 缺失与 `availability.vendorReadable`（`354` vs `353`）差异见 [`log-fail.txt`](log-fail.txt)。
- 改后实例 `4abff687a02e421f-cloud` 重采 `native_env_full` 验证记录见 [`log-pass.txt`](log-pass.txt)。

## 改动

1. **[`rules.c`](../../../modules/cdh_redirect/core/rules.c)**（`cdh_is_app_sphal_lib()`，L103–L115）：
   - 对 `/vendor/lib/` 前缀仅在 `in_compat_syscall()` 为真（32 位 compat 进程）时认定为需重定向的 SP-HAL 路径；
   - 对 `/vendor/lib64/` 前缀仅在 `!in_compat_syscall()` 为真（64 位原生进程）时认定为需重定向的 SP-HAL 路径；
   - 跨位宽访问（如 64 位 App 读取 `/vendor/lib/*.so`）直接绕过重定向，直通官方 `super.img` `/vendor/lib/` 原件。

## 验证

1. 重跑 `03_build_modules -> 08_build_vendor_dlkm -> 09_build_patch -> 11_pack_vda` 编入新版 `cdh_redirect.ko` 并在干净盘实例 `gold_15-OS3.0.11.0.VNQCNXM/2`（`4abff687a02e421f-cloud`）复测 19 域全量 `device-checker`。
2. `native_env.vendorTree.readable` 中 `/vendor/lib/libdrm.so` 已恢复可读且 MD5 `504e7332b6d389a850383966c1e76e8a`（`size=59096`）与真机完全一致；`availability.vendorReadable` 恢复 `354 == 354`（`availability: 1 -> 0`）。

## 遗留

1. 注意区分排查视角：`adb shell`（`uid=2000 <= cdh_max_uid=10000`）命中全目录前置规则而非 `cdh_is_app_sphal_lib()`，App 视角才走 `uid > 10000` 过滤链；64 位 `/vendor/lib64/` 垫片 MD5 与单文件合并仍在 P1 `#11` 剩余排期中。
