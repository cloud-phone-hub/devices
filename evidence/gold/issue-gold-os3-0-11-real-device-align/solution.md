# issue: gold `15-OS3.0.11.0.VNQCNXM` 与在线 USB 真机全量属性、SELinux、GPU 配置及虚拟化特征对齐

机型：`redmi/gold`（Xiaomi Redmi Note 13 5G，型号 `2312DRAABC`，`15-OS3.0.11.0.VNQCNXM`，`lineage-22.2` / 内核 `6.6.127-4k-cdh`）
阶段：`03_build_modules` / `05_build_vendor_boot` / `07_build_vendor` / `09_build_patch` / `11_pack_vda` + 运行侧 `device-checker`
时间：2026-11-12
改动类型：跨层引擎机制（`cdh_redirect` + GPU/Allocator HAL + `pioneer` + `07_build_vendor`）+ `gold-android15.json` 清单 + `aligner_xiaomi_gold` 评分卡校准

## 1. 问题

将云机 `gold_15-OS3.0.11.0.VNQCNXM` 与同固件版本在线 USB 物理真机（`fmvwkrheonugvgjb` / `ec519c7d513a3ad3`）接入同一本地 `checker-server` 做全套双机实测对拍时，暴露出五类深层不一致（详载于 `log-fail.txt`）：

1. **`ro.vendor.*` 双向错位（+54 项云机独有泄露 / -128 项真机公开缺失）**：
   - 真机对 `untrusted_app` 公开 229 项 `ro.vendor.*`，云机仅 155 项，其中 **54 项是真机不可见的云机独有项**（含 **25 项 `ro.vendor.cdh.{gpu,vulkan}.*`** 明文暴露自研 GPU 伪装参数，以及 29 项因宽泛前缀规则越权公开的 `ro.vendor.mtk*` / `ro.vendor.have_aeev_feature` / `ro.vendor.mediatek.rsc_name`）；
   - 同时有 **128 项真机公开 `ro.vendor.*`** 在云机侧缺失或因缺少 SELinux 上下文被隐藏。
2. **全命名空间 64 项云机独有 `ro.*` 与虚拟化运行态指纹泄露**：
   - 早启动/内核 cmdline 与运行态残留 `ro.boot.boot_devices=any`、`ro.boot.console=hvc0`、`ro.boot.first_stage_console=1`、`ro.boot.partition_map`、`ro.boot.theme=0`、`ro.boot.hypervisor.*`、`dev.mnt.*=vdc/vda*`；
   - Lineage/虚拟化基座遗留 `ro.hardware.gralloc=mediatek`（真机无此属性）、`bluetooth.device.default_name=" Redmi Note 13 5G "`、`ro.hw_timeout_multiplier`、`debug.sf.vsync_reactor_ignore_present_fences`、`hwservicemanager.disabled`；
   - 缺少真机公开的 18 项 `androidboot.*`（`ro.boot.hwc=CN`、`ro.boot.hwlevel=MP`、`ro.boot.hwversion=6752.9.0`、`ro.boot.slot_suffix=_a` 等）及 `ro.hardware.hwcomposer=mtk_common`；GNSS 型号名仍为旧快照而非真机实测 `MTK_MNLD_default,MNL_VER_default`。
3. **SELinux 规则相对原厂 Android 15 `Enforcing` 真机过宽**：
   - `android.service.daemon/sepolicy/vendor_sepolicy.cil` 与 `android.hardware.thermal/sepolicy/vendor_sepolicy.cil` 存在允许 `appdomain` / `untrusted_app_all` 读取 `cdh_gpu_public_prop`、访问 `sysfs_thermal` 等越权授权，与真机严格隔离行为背离。
4. **`aligner_xiaomi_gold` 检测脚本存在 4 项“惩罚真机、放行越权云机”的反向判据**：
   - 物理真机因原厂 `Enforcing` SELinux 拦截 `/vendor/dlkm/lib/modules`、`/dev/socket/logd*`、`/sys/class/thermal` 以及 `Context` 服务名手误（`"package"` / `"telephony.registry"`）和物理陀螺仪动态温漂补偿（单轴 `0.000137`）被误判为 `28/32`。

## 2. 根因

1. **GPU 伪装参数借道系统属性传递**：
   `cdh_gl_strings.c` 与 `cdh_vulkan_mali.c` 在每个 App（含 `isolated_app` WebView 沙箱）进程内读取 `ro.vendor.cdh.{gpu,vulkan}.*`；为了让沙箱进程可读，历史实现在 `daemon/sepolicy` 定义了 `cdh_gpu_public_prop` 并向 `appdomain` 全面放行，导致任何三方风控 SDK 调用 `getprop` 或遍历 `/dev/__properties__` 即可直接捕获 25 条 `ro.vendor.cdh.*` 铁证。
2. **Allocator/Gralloc 依赖 `ro.hardware.gralloc` 选择后端**：
   `engine/build/internal/device/config.go` 默认追加 `ro.hardware.gralloc=minigbm`，且 `CdhAllocator` / `gralloc_proxy` 依赖 `ro.hardware.gralloc` 找厂商 `mapper.<suffix>.so`；而 `gold` 官方 vendor 原件根本不设 `ro.hardware.gralloc`，其 `/vendor/lib64/hw/` 下只有 `gralloc.default.so` + `mapper.mediatek.so`（无 `gralloc.mediatek.so`）。
3. **宽泛前缀 `vendor_property_contexts` 导致双输**：
   `daemon/sepolicy/vendor_property_contexts` 与 `gold-android15.json` 之前采用 `ro.vendor.mtk` 等粗粒度前缀映射，把真机本属于 `vendor_default_prop`（对 App 不可见）的 29 项内部属性暴露给 `appdomain`，却又漏标真机实际映射到 `vendor_public_prop` / `vendor_ro_audio_prop` 的 118 项属性及 10 项构建期漏拷贝属性。
4. **首阶段 cmdline 与 `init` 挂载属性未擦除**：
   QEMU / bootloader 注入的 `androidboot.console`、`hypervisor`、`partition_map` 及 `fs_mgr` 挂载虚拟盘生成的 `dev.mnt.*=vd*` 在启动后持续留在属性区；`pioneer` 的 `boardid` 在 HyperOS 上无条件写了 `bluetooth.device.default_name`。

## 3. 证据

- `log-fail.txt`：修复前双机同批在线实测快照（真机 `28/32`；云机独有 `ro.vendor.*` 54 项、缺失真机 `ro.vendor.*` 128 项、全命名空间云机独有 `ro.*` 64 项、`dev.mnt.*` 暴露 `vdc`/`vda`）。
- `log-pass.txt`：修复后全链路重编（`patch.img` sha256 `cb23c0180674`）、干净盘实例冷启动（日志 10461 行，桌面截图平均亮度 `209.3`）及双机 19 项套件复测结果。
- `boot-launcher.png`：修复后新建干净盘实例 `gold_15-OS3.0.11.0.VNQCNXM/1` 冷启动直达 HyperOS 桌面截图。

## 4. 改动

严守 `AGENTS.md` 第 7、8 条红线（全程不触碰官方 `super.img` 及系统分区镜像，全部通过 `patch.img`、`vendor_boot.img` 与运行态收敛落地）：

1. **剥离 25 项 `ro.vendor.cdh.{gpu,vulkan}.*` 至 SPHAL 配置文件 `gpu_id.cfg`**：
   - `modules/cdh_redirect/core/rules.c`：`cdh_is_app_sphal_lib()` 白名单新增 `egl/gpu_id.cfg`，允许普通 App 与 `isolated_app`（WebView 渲染进程）透过路径重定向读取 `/vendor/lib64/egl/gpu_id.cfg`；
   - `vendor/hardware/android.hardware.gpu/src/{cdh_gl_strings.c,cdh_vulkan_mali.c}`：优先解析并进程内缓存 `/vendor/lib64/egl/gpu_id.cfg`（32 位回退 `/vendor/lib/egl/gpu_id.cfg`），无文件时才回退属性；
   - `build/stages/07_build_vendor/props.go`（`alignVendorGpuConfig`）：构建期自动将 `vendor/build.prop` 中的 `ro.vendor.cdh.{gpu,vulkan}.*` 抽离写入 `lib64/egl/gpu_id.cfg`，并在 `vendor_file_contexts` 赋予 `u:object_r:same_process_hal_file:s0`；
   - `vendor/hardware/android.service.daemon/sepolicy/`：删除 `vendor_property_contexts`，移除 `cdh_gpu_public_prop` 类型及其 `appdomain` 授权。
2. **零 `ro.hardware.gralloc` 属性自动解析 `gralloc.default.so` → `mapper.mediatek.so`**：
   - `vendor/hardware/android.hardware.graphics.allocator/src/{CdhAllocator.h,gralloc_proxy.c}`：无 `ro.hardware.gralloc` 时，通过 `dladdr` 定位自身所在 `hw/` 目录，按确定性厂商后缀表（`mediatek`, `qti`, `pixel`, `arm`, `exynos`, `minigbm`）直接探测 `mapper.<suffix>.so`（无需 `opendir` 权限，兼容 `isolated_app`）；
   - `build/stages/07_build_vendor/props.go`（`alignVendorGrallocConfig`）：检查官方 `unpack_vendor/build.prop`，若原厂不含 `ro.hardware.gralloc` 且无 `gralloc.<suffix>.so`，自动剥离默认注入的 `ro.hardware.gralloc` 并将 `gralloc_proxy` 落位为 `lib64/hw/gralloc.default.so`（AOSP `hw_get_module("gralloc")` 默认回退路径，且 `/proc/self/maps` 模块名与真机完全一致）。
3. **收敛虚拟化属性与 SELinux 越权规则**：
   - `vendor/hardware/android.service.daemon/src/rust/src/{main.rs,boardid.rs}`：MIUI/HyperOS 跳过写 `bluetooth.device.default_name`；新增 `scrub_early_virtual_props()` 擦除 `ro.boot.{console,first_stage_console,partition_map,theme,hypervisor.*}` 并对齐 `mt6833` 的 `ro.boot.boot_devices`；新增 `spawn_post_boot_scrub()` 在 `sys.boot_completed=1` 后擦除 `ro.hw_timeout_multiplier`、`debug.sf.vsync_reactor_ignore_present_fences`、`hwservicemanager.disabled` 并将 `dev.mnt.*` 中的 `vd*` 归一化为真机 `sdc*`；
   - `vendor/hardware/android.hardware.thermal/sepolicy/vendor_sepolicy.cil`：撤除对 `untrusted_app_all` 访问 `sysfs_thermal` 的越权放行。
4. **`manifest/configs/redmi/gold/gold-android15.json` 精确对齐真机**：
   - `buildVendorBoot.cmdline` 补齐 18 项真机 `androidboot.*`；
   - `build-prop-sets` 对齐 `ro.hardware.hwcomposer=mtk_common`、补齐 10 项真机公开 `ro.vendor.*`、移除 `ro.vendor.mediatek.rsc_name`；
   - `gnss_config.xml` 更新 `system-info-name` 为 `'MTK_MNLD_default,MNL_VER_default'`；
   - 用与真机逐键核对的 82 条精确 `vendor_property_contexts`（零重复匹配）+ `vendor_ro_audio_prop` 读权限替换旧宽泛前缀。
5. **校准 `aligner_xiaomi_gold` 检测套件**：
   - 修正 `all.js`、`cpu_memory_storage.js`、`package_service_security.js`、`sensors_input.js` 中违背 Android 15 `Enforcing` 隔离模型与物理陀螺仪温漂特性的 4 处判据。

## 5. 验证

1. **引擎测试与产物门禁**：
   - `cd engine/build && go test ./...` 全部包 `ok`（含新增 `props_test.go` 单测）；
   - `08_build_vendor_dlkm` 清空 99 个基座 `.ko`、回读 0 残留、12 个注入模块依赖闭包成立；
   - `07_build_vendor` 产物 `vendor/lib64/egl/gpu_id.cfg` 25 行，`vendor/build.prop` 中 `ro.vendor.cdh.*` = 0、`ro.hardware.gralloc` = 0。
2. **干净盘冷启动与双机 19 项套件在线实测**（USB 真机 `ec519c7d513a3ad3` × 新云机 `396654a3a70d0fab-cloud`）：
   - `aligner_xiaomi_gold.all`：**真机 `32/32`、云机 `32/32`**（双满分）；
   - `ro.vendor.*`（`untrusted_app` 可见）：**真机 `229` 项、云机 `229` 项，云机独有 `0` 项（54 → 0），真机独有 `0` 项（128 → 0）**，227 项值逐字一致（仅 2 项 `abilist`/`abilist32` 因 Apple Silicon 纯 64 位硬件差异刻意锁 `arm64-v8a`，见 `../../oriole/issue-ap2a-missing-64bit-abi-lock/`）；
   - 全命名空间云机独有 `ro.*`：**`0` 项（64 → 0）**；含 `cdh`/`virtio`/`qemu`/`cuttlefish`/`kvm` 的公开属性 **`0` 项**；含 `vd*` 的 `dev.mnt.*` **`0` 项**；
   - 图形与沙箱渲染：普通 App GLES（`ARM / Mali-G57 MC2 / OpenGL ES 3.2 v1.r49p1-03bet0...`）、Canvas2D（`pixelMd5=5e4abd56d6e2f12ca4eb209ffb373dc7`）、GNSS（`MTK_MNLD_default,MNL_VER_default`）双机逐字一致；`isolated_app` WebView WebGL 正常输出 `Mali-G57 MC2`；
   - 风控全集：`dev.dirty_policy` 命中 `0`；完成的 13 项 `ultimate.*` 套件（`cloud_emulator`、`native_env`、`root_selinux`、`attestation_integrity`、`boot_unlock`、`debug_integrity`、`device_identity`、`hardware_env`、`hook_inject`、`location_network`、`probe_runtime`、`app_risk`、`webview_fp`）**云机独有异常命中均为 `0`**。

## 6. 遗留

- `dalvik.vm.background-dex2oat-threads`：官方 `system/build.prop` 侧优先写入了 `8`（真机运行态由 runtime 覆盖为 `4`）；若后续需严格锁 `4`，可在 `pioneer` post-boot 阶段统一调用 `resetprop` 覆写，无需触碰官方 `system.img`。
- 真机侧存在 5 项烧录/解锁态只读属性（`ro.boot.vbmeta.device`、`ro.hardware.gatekeeper=mitee`、`ro.ril.factory_id`、`ro.ril.oem.{btmac,wifimac}`；以及测试样机自身 Bootloader 解锁态 `orange`/`unlocked`，云机保持量产锁机态 `green`/`locked` 更安全）。
- `ultimate.super_scan` 聚合脚本单次串行跑全量探针超出 `checker-server` 单任务默认超时阈值（真机与云机均报 `执行超时`），其拆分后的 13 个子套件已全部独立跑通并完成逐项对拍。
