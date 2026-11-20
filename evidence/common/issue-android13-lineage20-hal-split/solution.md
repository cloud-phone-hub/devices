# #289：Android 13（`lineage-20.0`）底座接入、HIDL/AIDL 架构拆分与跨平台 6 机型 Enforcing 闭环

- **设备/机型**：`asus/ASUS_AI2205`（`13-TKQ1.220928.001-20240229`，高通 `kalama`）、`blu/G1130`（`13-UP1A.231005.007-20260511`，展锐 `ums9230` GRF）、`hmd/ARW`（`13-QP1A.190711.020-20250620`，展锐 `ums9620`）、`nubia/P616F05`（`13-TP1A.220624.014-20260324`，展锐 `ums9230`）、`symphony/G27`（`13-TP1A.220624.014-20240929`，展锐 `SC9863A` + PowerVR GE8322 GRF）、`symphony/MAX_10`（`13-TP1A.220624.014-20250212`，展锐 `SC9863A` + PowerVR GE8322 GRF）
- **结论归属**：`common/`（涉及 `internal/env`、`internal/fsimg`、`internal/halcore`、`02_prepare`、`08_build_hardware`、`09_build_vendor`、`daemon/backend`、`cdh_redirect`、`hardware/` 与 `hardware-hidl/` 等引擎级架构拆分与跨平台兼容修复，同时覆盖高通与多代展锐 Mali/PowerVR 6 台 Android 13 云机）

## 问题

推进 Android 13（`lineage-20.0`，`kernel 5.15`）云机体系接入，以及按「`system <= 14` 且 `vendor = 13`」口径将 GRF 跨代机型统一收敛到 `android13 + lineage-20.0` 并入库新 ROM（`symphony/G27`、`symphony/MAX_10`）的过程中，依次暴露出 11 类引擎级共性卡点：

1. **版本绑定与 GRF 跨代 `vendor=13` 选型偏差**：部分 `system = 14`、`vendor = 13` 的 GRF 机型原按 Android 14 配置构建，与原厂 `vendor=13` 异构；且流水线缺少对 `android13 -> lineage-20.0`（`5.15`）的严格锁定。
2. **HIDL / AIDL 源码树与编译画像耦合**：Android 13 中 `composer@2.4`、`audio`、`bluetooth@1.1`、`boot@1.2`、`confirmationui@1.0`、`fastboot@1.1`、`media.c2@1.2`、`media.v412`、`secure_element`、`thermal`、`wifi` 仍依赖 HIDL 接口实现，无法与 Android 14+ 纯 AIDL 代码和头文件混用；且 `cdh_os_option_wrap.cpp` 对 `drv_get_os_option` 的 `--wrap` 在 Android 13/14 `external/minigbm` 中不存在对应符号，直接链接会残留未定义 `__real_drv_get_os_option`。
3. **`lineage-20.0` rEFInd EFI 引导盘形态不兼容**：`UTM-VM-lineage-20.0-20261107-UNOFFICIAL-virtio_arm64.zip` 没有 GPT `boot_a`/`vendor_boot_a` 分区，而是在 FAT32 EFI 分区通过 `/EFI/BOOT/refind.conf` 加载 `/kernel` 与双段 `lz4_legacy` 串联的 `/combined-ramdisk.img`；若不剥离菜单等待并固化 `androidboot.boot_devices=any` 与 `vbmeta` 参数，系统停在引导菜单或一阶段挂载失败。
4. **UEFI / rEFInd 向 `hvc*` 广播前导 ANSI CSI 污染通道协议**：UEFI `ConOut->SetMode(0)/ClearScreen()` 向 `hvc2(adbkey)`、`hvc3(keymint)`、`hvc4(radio)`、`hvc7(event)` 头部注入 29 字节 ANSI CSI 清屏序列（`1b 5b 32 4a ...`），导致宿主与 guest 间的二进制长度前缀帧、H4 与 `ADBKEY1 READY` 握手失步。
5. **跨 ROM `base_typeattr_NNN_33_0` 编号错位**：替换官方 `plat_pub_versioned.cil` 后，`lineage-20.0` `vendor_sepolicy.cil` 引用的基底外部匿名属性 `base_typeattr_345_33_0` 等在不同 ROM 编译期独立编号，错绑或丢失后导致 `system_app` / `platform_app`（如 `com.android.systemui`）在访问 `/dev/dri/card0`（`graphics_device`）与 `sysfs_gpu` 时被 SELinux 拒绝，引发 `EGL_NOT_INITIALIZED` 黑屏。
6. **Android 13 HIDL Mapper SP-HAL 与低版本图形库被 `cdh_redirect` 拦截**：`cdh_redirect` 原 SP-HAL 分类未放行 `hw/android.hardware.graphics.mapper@*`、`android.hardware.graphics.mapper@2.0..4.0.so`、`allocator-V1-ndk.so` 与 `common-V3/V4-ndk.so`，使 `platform_app` 查找 `IQtiMapper` / `GraphicBufferAllocator` 分配缓冲失败（`-22`）。
7. **HIDL `bluetooth@1.1` 缺失多项标准 HCI 响应导致协议栈崩溃**：`com.android.bluetooth` 初始化期间查询本地名称（`0x0c14`）、CoD、EIR（`0x0c51`）及 `0xfd53`（LE Vendor Capabilities）时因虚拟控制器应答不匹配，触发 `controller.cc(456) Check failed: page_number == 0` `SIGABRT`。
8. **`gnss` AIDL V2 编不过与高通扩展接口 `STATUS_UNKNOWN_TRANSACTION` 崩溃循环**：`injectNiSuplMessageData` 在 Android 13 GNSS AIDL V2 中不存在；且扩展占位 Binder `OnPlaceholderTransact` 原直接返回 `STATUS_UNKNOWN_TRANSACTION`，导致 `com.qualcomm.location` 调用 `ILocAidlGnss.getExtensionLocAidl*` 抛出 `RemoteException` 循环崩溃。
9. **HIDL `media.c2@1.2` `getConfigurable()` 返回空指针与 `composer@2.4` 模块名失配**：`EmptyComponentStore::getConfigurable()` 返回 `nullptr`；同时 `composer@2.4-service` 默认按 `ro.hardware.hwcomposer`（`unisoc`/`mtk`）查找 `hwcomposer.<impl>.so`，而云机统一产出 `hwcomposer.drm.so`。
10. **PowerVR `ro.hardware.vulkan` 未归一化为 `mali`**：`symphony/G27` 与 `symphony/MAX_10` 使用 `ro.hardware.vulkan=powervr`，原 `alignVendorGpuConfig` 仅替换 `virtio`/`pastel`/`adreno`，未覆盖 `powervr` 等其他原名，与 GPU interposer 固定落位名 `vulkan.mali.so` 不匹配。
11. **展锐 PowerVR 板级 `gralloc.sp9863a.so` 漏探与 `gralloc.minigbm.so` 误删黑屏**：在 `symphony/G27` 首次冷启动实测中（`Enforcing` + `boot_completed=1`），VNC 抓图 `720x1612` 持续全黑、`dumpsys SurfaceFlinger` 返回 `FAILED_TRANSACTION`、`screencap` 挂起，`Launcher3` 报错 `E IMGSRV : GrallocOpenPVRServices: Failed to open services connection (PVRSRV_ERROR_INIT_FAILURE)` / `buffer_unisoc_init: Failed to open gralloc`。

## 根因

1. **Android 13 历史包袱集中于 HIDL 与独立底层架构**：Android 13 平台需要独立维护一套 `devices/hardware-hidl/` 与 `out/devices/hal/android13/aosp/hidl/` 接口生成链，避免污染 Android 14~17 AIDL 主干代码。
2. **rEFInd 与宿主串口桥接缺口**：`lineage-20.0` UTM 盘无独立 `boot`/`vendor_boot` 分区，需在 `fsimg` 拆解/重编串联 `lz4_legacy` 流并改写 `refind.conf`；而 UEFI 向串口控制台写入的 ANSI CSI 控制串若不在宿主读取侧过滤，会破坏所有基于长度帧或固定文本前缀的 `hvc` 协议。
3. **SELinux 匿名属性编号不可跨 ROM 移植**：`secilc` 生成的 `base_typeattr_NNN_<vers>` 属于单次编译局部编号，必须在合并 `vendor_sepolicy.cil` 前按官方 `plat_pub_versioned.cil` 的原始表达式内联重命名为 `cdh_base_typeattr_*`。
4. **`cdh_redirect` 与 HIDL HAL 兼容细节缺口**：Android 13 App 侧仍经 HIDL Mapper 2.0~4.0 与 V1/V3/V4 NDK 库分配和映射缓冲，必须纳入 SP-HAL 白名单；同时 `bluetooth@1.1`、`gnss`、`media.c2@1.2`、`composer@2.4` 需对标真机协议语义给出合乎规范的空/标准响应并显式优先加载 `"drm"` HWC 模块。
5. **`alignVendorGrallocConfig` 两处叠加缺陷击中展锐 PowerVR**：
   - **误删重命名源文件**：当原厂使用无后缀 `android.hardware.graphics.mapper@4.0-impl.so` 时，`detectOfficialMapperSuffix` 返回 `""` 并使 `suffix` 回落为 `"minigbm"`；原逻辑在 `grallocSuffix == "default"` 时提前删除 `gralloc.<suffix>.so`（即 `gralloc.minigbm.so`），随后重命名 `gralloc.minigbm.so -> gralloc.default.so` 因源已删而静默跳过，最终 `patch.img` 中残留的是 `lineage-20.0` 基底 20072 字节 AOSP stub `gralloc.default.so`（而非自编 14088 字节 `CrOS Gralloc`）。
   - **漏探板级 `gralloc.<board>.so`**：`G27` / `MAX_10` 官方 `vendor/build.prop` 未设 `ro.hardware.gralloc`，但 `lib64/hw/` 存在板级 `gralloc.sp9863a.so`（PowerVR `IMGSRV`）。按 `libhardware` `hw_get_module("gralloc")` 探测顺序（`ro.hardware.gralloc` → `ro.hardware` / `ro.product.board` / `ro.board.platform` → `"default"`），移除 `ro.hardware.gralloc` 后系统按 `ro.hardware=sp9863a` 查找 `gralloc.sp9863a.so`；载体缺该文件时，`uid >= 10000` 进程直接读到官方 `IMGSRV` 版 `gralloc.sp9863a.so`，而 `uid < 10000` 进程退回基底 stub `gralloc.default.so`，导致 `SurfaceFlinger` 与 `drm_hwcomposer` 渲染全黑挂起。

## 证据

- **失败日志归档（[`log-fail.txt`](log-fail.txt)）**：
  - `log-fail-refind-timeout0.txt` / `log-fail-refind-ansi.txt`：rEFInd 菜单等待、缺失合并 `cmdline` 与 `hvc` 前导 29 字节 `1b 5b 32 4a ...` ANSI CSI 转储。
  - `log-fail-boot-and-selinux.txt`：一阶段与 SELinux 早期启动阻断记录。
  - `log-fail-base-typeattr-gpu.txt`：`com.android.systemui`（`platform_app`）与 `system_app` 报 `avc: denied { getattr } path="/dev/dri/card0"` 及 `avc: denied { search } tcontext=u:object_r:sysfs_gpu:s0`，引发 `egl: failed to create dri2 screen` / `EGL_NOT_INITIALIZED`。
  - `log-fail-sphal-hidl-mapper.txt`：`platform_app` 查 `vendor.qti.hardware.display.mapper::IQtiMapper` 被拒，导致 `GraphicBufferAllocator: Failed to allocate ... -2147483648`。
  - `log-fail-bt-hidl-ansi.txt`：`droid.bluetooth` 死于 `FATAL:controller.cc(456) Check failed: page_number == 0`。
  - `log-fail-powervr-gralloc-sp9863a.txt`：`symphony/G27` 修复前 `patch.img` 残留 20072B `gralloc.default.so`，`Launcher3` 报错 `E IMGSRV : GrallocOpenPVRServices: Failed to open services connection`、`dumpsys SurfaceFlinger` 报 `FAILED_TRANSACTION`，`qemu.log` `cgl-headless` 停在 `720x1600` 无 `720x1612` 帧输出，VNC 全黑（`3.4 KB`）。
- **通过日志与 6 机型亮屏桌面截图（[`log-pass.txt`](log-pass.txt)）**：
  - 6 台机型冷启动全部达到 `getenforce=Enforcing`、`sys.boot_completed=1`、`init.svc.bootanim=stopped`：
    - `asus/ASUS_AI2205`：[`asus_ai2205_home.png`](asus_ai2205_home.png)
    - `blu/G1130`：[`g1130_home.png`](g1130_home.png)
    - `hmd/ARW`：[`arw_home.png`](arw_home.png)
    - `nubia/P616F05`：[`p616f05_home.png`](p616f05_home.png)
    - `symphony/G27`：[`g27_home.png`](g27_home.png)（修复后 `patch.img` 落盘 `14088` 字节 `gralloc.sp9863a.so`，`SurfaceFlinger` 与 `Launcher3` `/proc/*/maps` 均映射载体 `/vendor/lib64/hw/gralloc.sp9863a.so`，`720x1612` 桌面与图标完整渲染）
    - `symphony/MAX_10`：[`max10_home.png`](max10_home.png)（`720x1600` `SearchLauncher` 主桌面完整渲染）

## 改动

1. **Android 13 版本绑定与 GRF 同构选型**：
   - [`agent/modules/pipeline/internal/env/env.go`](../../../../agent/modules/pipeline/internal/env/env.go)、[`agent/modules/pipeline/stages/02_prepare/grf_reconcile.go`](../../../../agent/modules/pipeline/stages/02_prepare/grf_reconcile.go)、[`agent/modules/pipeline/stages/02_prepare/lineage.go`](../../../../agent/modules/pipeline/stages/02_prepare/lineage.go)：将 `android13` 严格绑定至 `lineage-20.0`（`kernel 5.15`），并将 `system <= 14` 且 `vendor = 13` 的 GRF 跨代机型统一收敛至 `android13` 对齐原厂 `vendor=13`。
2. **AIDL / HIDL 源码树与编译画像拆分**：
   - [`devices/hardware-hidl/`](../../../hardware-hidl/)：独立承载 Android 13 HIDL 模块（`audio`、`bluetooth`、`boot`、`confirmationui`、`fastboot`、`graphics.composer@2.4`、`media`、`media.v412`、`secure_element`、`thermal`、`wifi`）。
   - [`agent/modules/pipeline/internal/halcore/profile/profile.go`](../../../../agent/modules/pipeline/internal/halcore/profile/profile.go)、[`deps/hidl.go`](../../../../agent/modules/pipeline/internal/halcore/deps/hidl.go)、[`halbuild/hidl.go`](../../../../agent/modules/pipeline/internal/halcore/halbuild/hidl.go) 与 [`08_build_hardware/modules/android13/`](../../../../agent/modules/pipeline/stages/08_build_hardware/modules/android13/)：固定 Android 13 HIDL 接口生成目录为 `out/devices/hal/android13/aosp/hidl/`，补齐 Android 13 全套模块定义与构建映射。
   - [`devices/hardware/android.hardware.graphics.allocator/src/cdh_os_option_wrap.cpp`](../../../hardware/android.hardware.graphics.allocator/src/cdh_os_option_wrap.cpp)：增加 `#if !defined(CDH_ANDROID_VERSION) || CDH_ANDROID_VERSION >= 15` 门控，避免 Android 13/14 无 `drv_get_os_option` 时残留未定义符号，且不引入重复的 `vendor-android13/` 目录。
3. **rEFInd EFI 引导转换与宿主串口前导 ANSI CSI 剥离**：
   - [`agent/modules/pipeline/internal/fsimg/refind_boot.go`](../../../../agent/modules/pipeline/internal/fsimg/refind_boot.go)：实现 `SplitLZ4LegacyStreams`、`BuildBootV4`/`BuildVendorBootV4` 与 `PatchRefindConf`（菜单零等待、剥离 Recovery、注入合并 `cmdline` 与 `vbmeta`），打通 `01_unpack`/`02_prepare`/`06_build_vendor_boot`/`13_pack_vda`。
   - [`agent/modules/daemon/backend/ansi_bridge.go`](../../../../agent/modules/daemon/backend/ansi_bridge.go)：新增 `StripLeadingANSICSIBytes`、`ReadFullAfterLeadingANSICSI`、`CopyAfterLeadingANSICSI`、`CleanGuestSocketpair` 与 `WrapLeadingANSICSIConn`，在 `keymint`、`bt-hci`、`adbkey` 与 `hvc7(event)` 读方向自动剥离 UEFI 前导 ANSI CSI 序列。
4. **SELinux CIL 跨 ROM 匿名属性内联**：
   - [`agent/modules/pipeline/stages/09_build_vendor/sepolicy_cil.go`](../../../../agent/modules/pipeline/stages/09_build_vendor/sepolicy_cil.go)：合并官方 `plat_pub_versioned.cil` 前将 `vendor_sepolicy.cil` 引用的基底外部 `base_typeattr_NNN_<vers>` 重命名为 `cdh_base_typeattr_*` 并内联原始集合定义，消除跨 ROM 序号错位。
5. **`cdh_redirect` HIDL Mapper SP-HAL 白名单**：
   - [`devices/modules/cdh_redirect/core/rules.c`](../../../modules/cdh_redirect/core/rules.c) 与 [`hook/maps.c`](../../../modules/cdh_redirect/hook/maps.c)：放行 `hw/android.hardware.graphics.mapper@*`、`android.hardware.graphics.mapper@2.0..4.0.so`、`allocator-V1-ndk.so` 及 `common-V3/V4-ndk.so` 并纳入预热表。
6. **HIDL `bluetooth@1.1` / `gnss` / `media.c2@1.2` / `composer@2.4` 协议与模块加载修复**：
   - [`devices/hardware-hidl/android.hardware.bluetooth/src/bluetooth_hidl_service.cpp`](../../../hardware-hidl/android.hardware.bluetooth/src/bluetooth_hidl_service.cpp)：补齐 `0x0c14`（Local Name）、`0x0c23`/`0x0c25`/`0x0c38`/`0x0c39`/`0x0c44`/`0x0c46`/`0x0c51`/`0x0c55`/`0x0c58` 标准响应，并将 `0xfd53`（LE Get Vendor Capabilities）改为返回 `0x01`（Unknown HCI Command）。
   - [`devices/hardware/android.hardware.gnss/src/gnss_service.cpp`](../../../hardware/android.hardware.gnss/src/gnss_service.cpp)：对 `injectNiSuplMessageData` 加 `CDH_GNSS_AIDL_VERSION >= 3` 条件编译；`OnPlaceholderTransact` 写入 `EX_NONE (0)` 与空 `IBinder`，消除扩展接口查询触发 `STATUS_UNKNOWN_TRANSACTION` 崩溃循环。
   - [`devices/hardware-hidl/android.hardware.media/src/codec2_hidl_store_service.cpp`](../../../hardware-hidl/android.hardware.media/src/codec2_hidl_store_service.cpp)：新增 `EmptyConfigurable` 实现并在 `EmptyComponentStore::getConfigurable()` 返回有效实例。
   - [`devices/hardware-hidl/android.hardware.graphics.composer/src/service.cpp`](../../../hardware-hidl/android.hardware.graphics.composer/src/service.cpp)：优先通过 `hw_get_module_by_class(HWC_HARDWARE_MODULE_ID, "drm", &module)` 加载 `hwcomposer.drm.so`，兼容保留原厂 `ro.hardware.hwcomposer` 指纹的机型。
7. **PowerVR `ro.hardware.vulkan` 归一化与板级 `gralloc.<board>.so` 对齐修复**：
   - [`agent/modules/pipeline/stages/09_build_vendor/files_compat.go`](../../../../agent/modules/pipeline/stages/09_build_vendor/files_compat.go) 与 [`props_test.go`](../../../../agent/modules/pipeline/stages/09_build_vendor/props_test.go)：
     - `alignVendorGpuConfig` 与新增辅助 `rewriteInitHardwareVulkanSetprop` 将 `vendor/build.prop` 与 init `.rc` 中的非空 `ro.hardware.vulkan`（含 `powervr` 等）统一归一化为 `mali`；
     - 新增 `detectOfficialGrallocTarget(plan, mapperSuffix)` 按 `libhardware` 探测优先级（`ro.hardware.gralloc` → `ro.hardware` / `ro.product.board` / `ro.board.platform` → `gralloc.<board>.so` → `"default"`）定位原厂板级 gralloc 文件名（如展锐 `gralloc.sp9863a.so`），并在 `alignVendorGrallocConfig` 限定仅在 `grallocSuffix != suffix && suffix != "minigbm"` 时才清理同名残留，绝不先删重命名源文件 `gralloc.minigbm.so`；补充 `TestAlignVendorGrallocPowerVRBoardAndMinigbmDefault`。

## 验证

1. **6 台 Android 13 机型全阶段构建与冷启动亮屏验收**：
   - `asus/ASUS_AI2205`（`13-TKQ1.220928.001-20240229`）、`blu/G1130`（`13-UP1A.231005.007-20260511`）、`hmd/ARW`（`13-QP1A.190711.020-20250620`）、`nubia/P616F05`（`13-TP1A.220624.014-20260324`）、`symphony/G27`（`13-TP1A.220624.014-20240929`）、`symphony/MAX_10`（`13-TP1A.220624.014-20250212`）均达到 `getenforce=Enforcing`、`sys.boot_completed=1`、`init.svc.bootanim=stopped`，VNC 抓图并人工审图确认全部清晰进入正常桌面/欢迎页（见 [`log-pass.txt`](log-pass.txt) 与 6 张 `*_home.png`）。
   - `symphony/G27` 与 `symphony/MAX_10` 重打/构建后 `patch.img` 内 `vendor/lib64/hw/gralloc.sp9863a.so` 均为 `14088` 字节自编 CrOS Gralloc，运行时 `surfaceflinger` 与 `com.android.launcher3` 均正确映射该模块。
2. **Android 15 HAL 无回归编译抽检**：
   - 执行 `cdh-pipeline build-hardware --android 15 --board drm --module android.hardware.gnss --refresh`、`--module android.hardware.graphics.allocator --refresh` 及全量 `--android 15 --board drm`（45 个模块）均 `exit 0` 无报错。
3. **Go 全量单元测试**：
   - `cd agent && go test -count=1 ./modules/pipeline/... ./modules/daemon/...` 全部通过（含 `TestAlignVendorGrallocPowerVRBoardAndMinigbmDefault`）。

## 遗留

- `roms/devices/symphony/Helio-40_Needrom.7z` 为加密压缩包且无解压密码，无法入库解包，已确认跳过；Android 版本 `< 13` 的旧固件按既定约束跳过。
- `symphony/G27` 与 `symphony/MAX_10` 沿用官方系统分区预置的 `com.android.chrome` 空壳包（缺 64 位 `libmonochrome.so`，与 [`../../oriole/issue-oriole14-hwc-drm/solution.md`](../../oriole/issue-oriole14-hwc-drm/solution.md) 一致），云机无 Play 分发环境下启动该应用会触发提示弹窗，不影响系统核心链路与桌面验收。
