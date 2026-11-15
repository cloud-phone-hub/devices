# 普通 App (uid >= 10000) 旁路 cdh_redirect 暴露官方分区与 Gralloc/Mapper 虚拟库名及 /proc/self/maps 路径伪装

- 设备：跨机型通用（首发闭环验证于 `xiaomi/gold` `15-OS3.0.10.0.VNQCNXM` 与 `google/oriole` `14-AP2A.240905.003.F1-20240815`）
- 关联检测项：`aligner_xiaomi_gold.cpu_memory_storage`、`aligner_xiaomi_gold.all`（`B5` / `B6` / `B7`）
- 日期：2026-11-09
- 结果：普通应用进程（`uid >= 10000`）访问 `/vendor_dlkm/lib/modules` 与 `/vendor` 时自动透传回原厂 `super.img` 只读分区（消除 `cdh_*.ko` 模块名泄露，193 个 MTK 原厂 `.ko` 原样可见）；同时对同进程图形 HAL（`mapper.<suffix>.so`、`gralloc.<suffix>.so` 及其底层依赖库）精准保留重定向，并在构建期将 `mapper.minigbm.so` / `libminigbm_gralloc*.so` 伪装为厂商同名库（MTK `mapper.mediatek.so` / `libgralloc_extra.so`、Pixel `mapper.pixel.so` / `libdrmresource.so`），配合 `d_path` 逆向钩子消除 `/proc/self/maps` 中的 `/mnt/vendorpatch` 路径暴露；`gold` 实测 `B6_vendorDlkmNoCdhModuleNameLeak` 通过，总分由 `14/32` 提升至 `15/32`。

## 一、问题

在对 `xiaomi/gold`（`15-OS3.0.10.0.VNQCNXM`）执行 `device-checker` 全维度对齐审计（`aligner_xiaomi_gold.cpu_memory_storage` / `aligner_xiaomi_gold.all`）时，维度 B 存在四处与路径重定向、SP-HAL 库名及 SELinux 权限相关的暴露：

1. **`/vendor_dlkm/lib/modules` 暴露云机自研内核模块（`B6` DEADLY）**：
   `cdh_redirect` 将所有进程对 `/vendor_dlkm/lib` 与 `/vendor/{etc,bin,lib,lib64}` 的访问无差别重定向到 `/mnt/vendorpatch/...`。普通 App（`untrusted_app`，`uid >= 10000`）调用 `new File("/vendor_dlkm/lib/modules").list()` 时，只能看到 `patch.img` 内的 19 项文件（含 `cdh_device_info.ko`、`cdh_goodix_fingerprint.ko`、`cdh_input_identity.ko`、`cdh_media.ko`、`mac80211_hwsim.ko`、`virtio_console.ko`），而看不到原厂 `super.img` 内 `vendor_dlkm_a` 分区自带的 193 个联发科 `.ko` 模块。
2. **普通 App 按 `uid > 10000` 旁路重定向后，SP-HAL Gralloc Mapper 断链 + 虚拟库名暴露（`B5`）**：
   - 若简单让 `uid > 10000` 跳过全部 `cdh_redirect` 前向规则，App 进程通过 `libui.so -> Gralloc5` 加载同进程图形 Mapper（SP-HAL）时会落回原厂 `/vendor/lib64/hw/`，或者因 `allocator-service.minigbm` 返回后缀 `"minigbm"` 而在原厂分区找不到 `mapper.minigbm.so` 导致渲染断链；
   - 即便对 Mapper 保留重定向，进程 `/proc/self/maps` 仍直接暴露开源虚拟图形库名 `mapper.minigbm.so`、`gralloc.minigbm.so`、`libminigbm_gralloc.so`、`libminigbm_gralloc4_utils.so`，且 `mapsAudit.hasRealMtkMapperSo` 为 `false`。
3. **`/proc/self/maps` 暴露 `/mnt/vendorpatch/vendor/lib64/...` 真实底层挂载路径（`B5` `suspiciousPathLeaks`）**：
   `cdh_redirect` 原有的逆向路径改写只挂在 `proc_pid_readlink` 与 `vfs_readlink` 上（仅覆盖 `readlink("/proc/self/fd/N")` 等符号链接读取），而内核生成 `/proc/<pid>/maps` 走的是 `show_map_vma -> seq_file_path -> seq_path -> d_path`，不经过 `readlink`，导致 `/proc/self/maps` 中全部 35 条 vendor 库映射路径均以 `/mnt/vendorpatch/vendor/lib64/...` 原样泄露。
4. **`B6` / `B7` 关联的 SELinux `getattr` 与 `/proc/uptime` 偏移漂移**：
   - 云机 `untrusted_app` 缺少对 `vendor_file_202404` 的 `getattr`（导致 `Os.stat("/vendor/build.prop")` 返回 `exists: false`）、缺少对 `proc_uptime_202404` 的 `read open getattr`（导致 `/proc/uptime` 不可读）、缺少对 `logdw_socket` 与 `tombstoned_crash_socket` 的 `getattr`（命中 `U-LOGD-TOMBSTONE`）；
   - 放开 `proc_uptime` 读取后，`cdh_device_info/time/uptime.c` 默认给 `/proc/uptime` 与 `sysinfo(2).uptime` 硬编码增加了 `CDH_UPTIME_OFFSET_SECONDS = 172800`（48 小时），而 `SystemClock.elapsedRealtime()` 走 vDSO `CLOCK_BOOTTIME` 未加偏移，导致 `/proc/uptime` 与 `elapsedRealtime()` 相差 `172800s`，直接触发 `B6`（`uptimeCrossCheck.consistent: false`）、`H-84` 与 `U-BOOT-TIME-CROSS`。

## 二、根因

1. **`cdh_redirect` 缺乏按 UID 与 SP-HAL 白名单的分流机制**：
   内核态加载模块（`init` / `dlkm_loader`、`uid=0`）与系统服务（`surfaceflinger` / `allocator-service`、`uid=1000`）需要读取 `/mnt/vendorpatch` 下的云机定制文件；而普通第三方应用（`uid > 10000`）除必须加载进自身地址空间的同进程 HAL（SP-HAL：`mapper.<suffix>.so`、`gralloc.<suffix>.so`、EGL/GLES 库及其直接 `DT_NEEDED` 辅助库）外，其余对 `/vendor` 和 `/vendor_dlkm` 的文件系统探测都应当直接落回原厂只读 EROFS/EXT4 分区。
2. **Minigbm Gralloc/Mapper 后缀与 `DT_NEEDED` 硬编码**：
   - `libui.so` 的 `Gralloc5` 通过 AIDL 调用 `IAllocator::getIMapperLibrarySuffix()` 获取后缀 `<suffix>`，随后拼接 `/vendor/lib64/hw/mapper.<suffix>.so` 并 `dlopen`；
   - 在 Android 15（`lineage-22.2`）的 `android.hardware.graphics.allocator-service.minigbm` 中，`getIMapperLibrarySuffix` 被编译器内联为 13 条 AArch64 `MOV/MOVK/STR/STUR` 指令直接向 `aidl_return` 写入小端常量 `"minigbm"`（长度 7）；在 Android 14（`lineage-21.0`）中则通过 `ADRP + ADD` 引用 `.rodata` 中的 `\x00minigbm\x00` 字面量；
   - `mapper.minigbm.so`、`gralloc.minigbm.so` 及 `allocator-service.minigbm` 的 `.dynamic` 段 `DT_NEEDED` 引用了 `libminigbm_gralloc.so`（21 字符）与 `libminigbm_gralloc4_utils.so`（28 字符）。
3. **`d_path` 的末尾对齐契约（Right-Aligned Buffer Contract）**：
   Linux 内核 `d_path(const struct path *path, char *buf, int buflen)` 从缓冲区末尾 `buf + buflen` 向前填充路径字符，并保证 `buf[buflen - 1] == '\0'`。虽然 `seq_path`（`/proc/self/maps`）使用 `strlen(p)` 计算长度，但同调 `d_path` 的 `do_proc_readlink`（`fs/proc/base.c:1851`）直接用 `len = tmp + PATH_MAX - 1 - pathname` 计算长度！若在 `d_path` 返回钩子中把 `/mnt/vendorpatch/vendor/...`（前缀长 16）原地改写为 `/vendor/...` 而不右移对齐到 `buf + buflen - 1`，`do_proc_readlink` 读到的长度会比实际字符串多出 16 字节（含末尾垃圾字节），引发 `init` / `apexd` 在符号链接解析时崩溃。

## 三、证据

- 修复前：`log-fail.txt`（`run-00161` / `run-00163`：`/proc/self/maps` 暴露 35 条 `/mnt/vendorpatch/...`、`vendorBuildPropExists: false`、`procUptimeSec: 0` → 放开 SELinux 后 `procUptimeSec: 173319.8` vs `elapsedRealtimeSec: 519.8`、`B6: false`）
- 修复后：`log-pass.txt`（`run-00166`：`mapsAudit.suspiciousPathLeaks: []`、`hasRealMtkMapperSo: true`、`noCdhModuleNameLeak: true`、193 个官方 `.ko`、`uptimeCrossCheck.consistent: true`、`B6: true`；以及 `oriole` `07_build_vendor` 日志中 `mapper.minigbm.so -> mapper.pixel.so`、`libminigbm_gralloc.so -> libdrmresource.so` 自动伪装记录）

## 四、改动

1. **`devices/modules/cdh_redirect/core/rules.c`（UID > 10000 分流与跨平台 SP-HAL 白名单）**：
   - 新增模块参数 `max_uid`（默认 `10000`）：当调用进程 `uid > max_uid` 时，除命中 `cdh_should_redirect_for_app(src)` 的同进程 SP-HAL 库路径外，其余前向重定向一律跳过（记入 `uid_skipped`），使普通 App 直接看到原厂 `super.img` 内的 `/vendor` 与 `/vendor_dlkm/lib/modules`；
   - `cdh_is_app_sphal_lib` 放行 `/vendor/lib{,64}/egl/*`、`/vendor/lib{,64}/hw/{mapper,gralloc,vulkan}.*` 以及 Gralloc/EGL 直接依赖的辅助库（含伪装后的厂商库名：`libgralloc_extra.so`、`libgralloctypes_mtk.so`、`libgralloc_metadata.so`、`libgralloc.qti.so`、`libgralloccore.so`、`libgrallocutils.so`、`libdrmresource.so`）。
2. **`devices/modules/cdh_redirect/hook/readlink.c`（新增 `d_path` kretprobe 逆向路径改写）**：
   - 挂载 `d_path` 的 `kretprobe`（`cdh_d_path_kretprobe`）：当返回指针 `ret` 位于内核缓冲区且命中 `/mnt/vendorpatch/...` 逆向规则时，计算改写后的虚拟路径长度 `new_len`；
   - 若 `new_len <= orig_len`，将改写后的字符串**右移对齐**至 `ret + (orig_len - new_len)`（保持末尾 `'\0'` 严格落在 `buf + buflen - 1`），同时满足 `seq_path`（`/proc/self/maps`、`strlen(p)`）与 `do_proc_readlink`（`buf + buflen - 1 - p`）的长度契约。
3. **`engine/build/stages/07_build_vendor/elfcompat.go`（自适应厂商 Gralloc/Mapper 库名伪装）**：
   - 新增 `reconcileGrallocMapperDisguise`：
     - `detectOfficialMapperSuffix`：扫描厂商解包树 `unpack_vendor/lib64/hw/mapper.*.so`（AIDL Mapper v5）及 `android.hardware.graphics.mapper@4.0-impl-*.so`（HIDL Mapper 4.0），结合 `ro.board.platform`（`gs101/gs201/zuma -> pixel`、`mt* -> mediatek`）自动推导厂商后缀（如 `mediatek`、`pixel`、`qti`）；
     - `detectOfficialGrallocHelpers`：按厂商 `unpack_vendor/lib64` 实际存在的辅助库自适应选择替换名（MTK：`libgralloc_extra.so` + `libgralloctypes_mtk.so`；Pixel：`libdrmresource.so` + `libgralloccore.so`；QTI：`libgralloc.qti.so` + `libgralloccore.so`），长度均严格不超过原串（21 / 28 字节）；
     - 重命名 `mapper.minigbm.so -> mapper.<suffix>.so`、`gralloc.minigbm.so -> gralloc.<suffix>.so` 及两个辅助库，原地改写 `.dynstr` 中的 `DT_NEEDED` 与 `DT_SONAME`；
     - `patchAllocatorMapperSuffix`：兼容修补 `android.hardware.graphics.allocator-service.minigbm` 中 Android 15 的 13 条 AArch64 内联指令块（支持任意 `<= 15` 字节的 `<suffix>`）与 Android 14 的 `.rodata` `\x00minigbm\x00` 字面量；
     - 同步更新 `vendor/etc/vintf/manifest/mapper.<suffix>.xml`、`vendor/build.prop`（`ro.hardware.gralloc=<suffix>`）、`vendor_service_contexts` 与 `vendor_file_contexts`（赋予 `same_process_hal_file` 标签）。
4. **`engine/vendor/hardware/android.service.daemon/sepolicy/vendor_sepolicy.cil` & `devices/modules/device_info/time/uptime.c`**：
   - 在 `vendor_sepolicy.cil` 中为 `appdomain` 补齐 `vendor_file_{{vers}} (file (getattr))`、`proc_uptime_{{vers}} (file (read open getattr))`、`logdw_socket_{{vers}} (sock_file (getattr))`、`tombstoned_crash_socket_{{vers}} (sock_file (getattr))`；
   - 将 `uptime.c` 的 `CDH_UPTIME_OFFSET_SECONDS` 默认值由 `172800` 改为 `0L`（模块参数权限设为 `0644`），使 `/proc/uptime`、`sysinfo(2).uptime` 与 vDSO `CLOCK_BOOTTIME`（`SystemClock.elapsedRealtime()`）保持零漂移。

## 五、验证

1. **单元测试与构建验证**：
   - `cd engine/build && go test -v -run "TestReconcileVendor" ./stages/07_build_vendor` 全绿通过；
   - `gold`（`15-OS3.0.10.0.VNQCNXM`）与 `oriole`（`14-AP2A.240905.003.F1-20240815`）均全链路通过 `03_build_modules -> 05_build_vendor_boot -> 07_build_vendor -> 08_build_vendor_dlkm -> 09_build_patch`。
2. **`gold` 云机运行态与 `device-checker` 实测（`run-00166`，设备 `86213baaa20d964b-cloud`）**：
   - `dmesg` 确认 `cdh_redirect: 逆向钩子就绪：proc_pid_readlink + vfs_readlink + d_path`；
   - `mapsAudit.hasRealMtkMapperSo: true`（加载 `/vendor/lib64/hw/mapper.mediatek.so`、`/vendor/lib64/hw/gralloc.mediatek.so`、`/vendor/lib64/libgralloc_extra.so`），`vendorSoList` 中 `minigbm` 相关库名降为 **0**；
   - `mapsAudit.suspiciousPathLeaks: []`（`/proc/self/maps` 中 `/mnt/vendorpatch` 路径暴露由 35 条降为 **0 条**）；
   - `vendorPartitionAudit.noCdhModuleNameLeak: true`（枚举出原厂 193 个 MTK `.ko` 模块，0 个 `cdh_*.ko` 泄露），`vendorBuildPropExists: true`，`vendorBuildPropReadable: false`（与真机一致）；
   - `uptimeCrossCheck: {"consistent": true, "elapsedRealtimeSec": 284.7, "procUptimeSec": 284.7}`；
   - `socketsAudit: {"logdwExists": true, "tombstonedCrashExists": true}`；
   - `goldScorecard.B6_vendorDlkmNoCdhModuleNameLeak` 由 `false` 变为 **`true`**，总分由 `14/32` 升至 **`15/32`**。

## 六、遗留

- `mapsAudit.mesaOrMinigbmLeaks` 中剩余的 5 个 EGL/GLES Mesa 库（`libEGL_mesa.so`、`libGLESv1_CM_mesa.so`、`libGLESv2_mesa.so`、`libgallium_dri.so`、`libglapi.so`）属于 OpenGL ES 驱动层伪装范畴（对应 `TODO.md` 中 `B5` 后续的 `libGLES_mali.so` 伪装任务）。
- `/proc/self/mountinfo` 中仍可见 `/mnt/vendorpatch` 挂载行（对应 `B7` 的 `mountinfo` 隐藏项）。
