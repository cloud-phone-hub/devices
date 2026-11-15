# Issue: device_info 的 UTS 改为从清单 uname-version 渲染伴随文件读取，并打通 5.10/6.1 内核与 /mnt/vendorpatch 内核态直读

- **设备/机型**: 引擎级 / 跨机型（`devices/modules/device_info`、`08_build_vendor_dlkm`、`internal/manifest`、`internal/modmeta`；在 `google/oriole` `5.10.270-4k-cdh` 实机闭环验证）
- **首次暴露**: 2026-11-04 `device_info/uts/uts.c` 原本把 gold 的 `6.6.89-android15-8-...` 写死在 C 常量里，且 `device_info` 只支持 `6.6`/`6.12`，导致 `oriole`（`5.10`）等机型 `uname -a` 仍暴露 `5.10.270-4k-cdh`
- **状态**: **已闭环**（2026-11-04）

---

## 一、问题

1. `devices/modules/device_info/uts/uts.c` 原本把 `gold`（Redmi Note 13 5G）的 `uname` 字段硬编码在 C 源码里，不同机型无法按基线清单 `rom.vendor.kernel.uname-version` 动态对齐；
2. `device_info` 原本只支持 `6.6` 与 `6.12`（依赖 6.6 新增的 `<trace/hooks/cpuinfo.h>`、6.1 新增的 `android_vh_si_meminfo_adjust` 与 `cpu_topology.cluster_id`），在 `5.10`/`6.1` 上无法编译；`oriole` 基线 `modules.external` 也因此未纳入 `"device_info"`；
3. 将 `device_info` 迁为 `kernel_read_file_from_path()` 读取伴随文件后，首轮在 `oriole` 上起机时 `dlkm_loader` 报：
   `cdh_device_info: uts: 读不到 UTS 文件 /vendor_dlkm/lib/modules/cdh_device_info.uts（-2）`。

---

## 二、根因

1. **硬编码 UTS**：`uts.c` 没有像 `cdh_redirect` 读取 `cdh_redirect.rules` 那样从伴随文件读取目标机型的 `uname-version`；
2. **5.10 / 6.1 内核接口差异**：
   - `arch/arm64/kernel/cpuinfo.c` 的 `trace_android_rvh_cpuinfo_c_show` 与 `drivers/base/arch_topology.c` 的 `trace_android_rvh_cpu_capacity_show` 仅在 6.6+ 存在；在 5.10/6.1 中，`/proc/cpuinfo` 走 `cpuinfo_op.show`（注意内核同时存在 `arch/arm64/kernel/cpuinfo.c:c_show` 与 `crypto/proc.c:c_show` 两个同名静态符号，不能按名字挂 kretprobe，必须解析唯一导出的 `cpuinfo_op` 取 `op->show`），`cpu_capacity` 直接读导出的 `per_cpu(cpu_scale, cpu)`；
   - `struct cpu_topology` 的 `cluster_id` / `cluster_sibling` 在 6.1+ 才有，5.10 只有 `package_id` / `core_id` / `thread_id` / `core_sibling` / `thread_sibling` / `llc_sibling`；
   - `trace_android_vh_si_meminfo_adjust` 在 6.1+ 才有，5.10 需挂 `si_meminfo` 的 `kretprobe`；
3. **内核态 `kernel_read_file_from_path` 绕过 `cdh_redirect`**：
   - `cdh_redirect` 挂在 `getname_flags()`（用户态 `__user` 路径入口），而 `08_build_vendor_dlkm` 产出的 `/lib/modules/cdh_device_info.uts` 由 `09_build_patch` 打包进 `patch.img`（挂载在 `/mnt/vendorpatch/vendor_dlkm/lib/modules/cdh_device_info.uts`）；
   - 当用户态 `dlkm_loader` 打开 `/vendor_dlkm/lib/modules/cdh_device_info.ko` 时，经 `getname_flags()` 重定向到 `/mnt/vendorpatch/vendor_dlkm/...`；但模块初始化函数 `cdh_uts_init()` 在内核态调用 `kernel_read_file_from_path()` 走的是 `filp_open -> getname_kernel()`，**不经过 `getname_flags()`**，因此直接读 `/vendor_dlkm/lib/modules/cdh_device_info.uts` 会落到底层 `super.img` 的原厂 `vendor_dlkm` 分区而返回 `-ENOENT`（`-2`）。

---

## 三、证据

- 失败日志：[log-fail.txt](log-fail.txt)
- 通过日志：[log-pass.txt](log-pass.txt)

---

## 四、改动

1. **构建期从 `rom.vendor.kernel.uname-version` 渲染伴随文件**：
   - `engine/build/internal/manifest/manifest.go`：新增 `UTSFields`、`ParseUnameVersion()`、`UTSFromManifest()`，解析 `uname -a` 或 `/proc/version` 格式的 `rom.vendor.kernel.uname-version` 并渲染为 6 行键值文本（`sysname` / `nodename` / `release` / `version` / `machine` / `domainname`）；
   - `engine/build/internal/modmeta/modmeta.go`：`Asset` 新增 `FromManifest` 字段（支持 `"uname-version"`）；
   - `engine/build/08_build_vendor_dlkm/{plan.go,modules.go,execute.go}`：解析 `inject=vendor-dlkm` 模块的 `assets`，将伴随文件纳入缓存配方 `AssetsDigest` 并通过 `debugfs` 写入 `/lib/modules/cdh_device_info.uts`。
2. **`devices/modules/device_info` 读取伴随文件并兼容 `5.10`/`6.1`/`6.6`/`6.12`**：
   - `uts/uts.c` + `include/cdh_device_info.h`：用 `kernel_read_file_from_path()` 读取 UTS 伴随文件（同时支持 6 行键值格式与单行原始 `Linux ...` 文本）；默认先读载体物理路径 `/mnt/vendorpatch/vendor_dlkm/lib/modules/cdh_device_info.uts`，`-ENOENT` 时回退 `/vendor_dlkm/lib/modules/cdh_device_info.uts`；
   - `core/main.c`：`CDH_GKI_ABI_RELEASE` 改取 `<generated/utsrelease.h>` 的 `UTS_RELEASE`；
   - `cpu/cpu_display.c`：`< 6.6` 分支通过 `kallsyms_lookup_name("cpuinfo_op")` 取 `op->show` 注册 `kretprobe` 改写 `/proc/cpuinfo`，并保存/覆写 `per_cpu(cpu_scale, cpu)`；
   - `cpu/topology.c`：`< 6.1` 编译期守卫 `cluster_id` / `cluster_sibling`；
   - `meminfo/meminfo.c`：`< 6.1` 分支通过 `si_meminfo` 的 `kretprobe` 覆写 `totalram`；
   - `module.json`：`kernel` 扩为 `["5.10", "6.1", "6.6", "6.12"]`，声明 `assets` `from-manifest: "uname-version"`；
   - `oriole-ap2a.240905.003.f1-factory-655c44e7.json`：`modules.external` 补齐 `"device_info"`。

---

## 五、验证

1. **单测**：`go test -count=1 ./internal/manifest/... ./internal/modmeta/... ./08_build_vendor_dlkm/...` 全部通过（含 `TestParseUnameVersionAndRender`、`TestModuleAssetsFromManifestAndRecipe`）。
2. **`oriole`（`5.10.270-4k-cdh`）重编与实机验证**：
   - `03_build_modules -> 11_pack_vda` 全绿（`patch.img` `91adf7f1c3fe`，`vda-e5fec0be5063`）；
   - `qemu.log`：`[ 0.910692] cdh_device_info: uts: patched init_uts_ns (/mnt/vendorpatch/vendor_dlkm/lib/modules/cdh_device_info.uts) release=5.10.198-android13-4-00050-g12f3388846c3-ab11920634 version=#1 SMP PREEMPT Mon Jun 3 20:51:42 UTC 2024`，`dlkm_loader: Loaded kernel module /vendor_dlkm/lib/modules/cdh_device_info.ko`；
   - `adb shell uname -a` 输出：
     `Linux localhost 5.10.198-android13-4-00050-g12f3388846c3-ab11920634 #1 SMP PREEMPT Mon Jun 3 20:51:42 UTC 2024 aarch64 Toybox`，与基线 `rom.vendor.kernel.uname-version` 逐字一致。

---

## 六、遗留

- `device_info` 的 CPU/内存固定画像参数（核心簇、MIDR、MemTotal）目前仍为内置默认值，后续如需按机型画像动态对齐，可沿用同一伴随文件机制扩展。

