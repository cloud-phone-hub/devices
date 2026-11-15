# #192：纯 C++ 源码自编 `android.hardware.graphics.allocator` 替代 `07_build_vendor` 最后一处 AArch64 机器码补丁

## 1. 问题

在 [`#191`](../issue-hwc3-composer3-source-hal-replace-elf-patch/solution.md) 清理完 `hwc3-service.drm` 的 9 段机器码补丁后，[`07_build_vendor/elfcompat.go`](../../../build/stages/07_build_vendor/elfcompat.go) 中仍残留全仓最后一处 ELF `.text` / `.rodata` 二进制修补逻辑：`encodeAArch64MapperSuffixBlock` 与 `patchAllocatorMapperSuffix`。

AOSP 上游 `external/minigbm/cros_gralloc/aidl/Allocator.cpp` 将 `Allocator::getIMapperLibrarySuffix()` 硬编码写死为字面量 `"minigbm"`：
```cpp
ndk::ScopedAStatus Allocator::getIMapperLibrarySuffix(std::string* _aidl_return) {
    *_aidl_return = "minigbm";
    return ndk::ScopedAStatus::ok();
}
```
当 `07_build_vendor` 把 `/vendor/lib64/hw/mapper.minigbm.so` 重命名为厂商原厂库名（如 `mapper.mediatek.so`、`mapper.qti.so`、`mapper.gs101.so`）以消除 `/proc/<pid>/maps` 虚拟化泄露后，客户端 `libui.so` 通过 Binder 调用 `IAllocator::getIMapperLibrarySuffix()` 若仍拿到 `"minigbm"`，就会去 `dlopen("mapper.minigbm.so")` 失败导致整机黑屏崩溃。此前 `patchAllocatorMapperSuffix` 靠匹配 libc++ SSO 短字符串构造的 4 条 AArch64 指令字节序列并注入 `movz/movk` 立即数来改写返回值，存在三大隐患：
1. **耦合编译器指令序列与 SSO 内部布局**：底包重编或寄存器分配变化即失配；
2. **后缀长度受限（`<= 8` 字节）**：64 位寄存器立即数拼装最多容纳 8 字节 ASCII（`"mediatek"` 恰好卡满上限），超过 8 字节的平台名即失败；
3. **跨层职责越界**：违反「`engine/vendor` 负责源码编译虚拟 HAL、`07_build_vendor` 只做产物装配与标准 ELF 动态符号表（`.dynstr` `DT_NEEDED`/`DT_SONAME`）重映射」的工程边界。

## 2. 根因

1. Android 13 / 14（`lineage-21.0`）与 Android 15（`lineage-22.2`）均使用 AIDL `android.hardware.graphics.allocator-service.minigbm`，此前 `engine/vendor` 尚未建立对应的 `android.hardware.graphics.allocator` 源码编译模块，直接沿用了底包预编译二进制；
2. 上游 `external/minigbm` 在 Android 14（`android-14.0.0_r67`）与 Android 15（`android-15.0.0_r36`）之间的内部结构体 `cros_gralloc_buffer_descriptor` 字段存在差异（A15 新增了 `enable_metadata_fd`、`dataspace`、`blend` 等字段，并链接底包对应版本的 `libminigbm_gralloc.so` 与 `libminigbm_gralloc4_utils.so`），因此必须按各自 Android 版本的 `@platform/external/minigbm` 上游源码编译，而不能跨代硬编码结构体布局。

## 3. 证据

- 改造前状态见 [`log-fail.txt`](log-fail.txt)：`07_build_vendor/elfcompat.go` 中保留了 `encodeAArch64MapperSuffixBlock` 与 `patchAllocatorMapperSuffix`（共约 105 行机器码与 `.rodata` 二进制修补代码）。
- 改造后构建与干净盘真机终验日志见 [`log-pass.txt`](log-pass.txt)：
  1. **跨代双版本纯源码编译通过 + `.text`/`.rodata` 100% 零篡改**：
     - `cdh-vendor hal build android.hardware.graphics.allocator --android 15` 与 `--android 14` 均一次性编译产出 `bin/hw/android.hardware.graphics.allocator-service.minigbm`；
     - 核对 `out/vendor/android15/bin/hw/android.hardware.graphics.allocator-service.minigbm` 与 `gold` 装配树同名二进制：`.text` 段（15696 B，SHA-256 `4707ce26b740ffb3e5e43d8519fb8f63090181d6e5c0d45913685c8e70ec1f84`）与 `.rodata` 段（2248 B，SHA-256 `c2a90236a0d1b6ddd21c549239151dffb7dd5fc3cfdf04e70c7693abfaf3c733`）**逐字节 100% 相等**，仅 `.dynstr` 因动态链接库名重映射（`libminigbm_gralloc* -> libgralloc_extra.so`）正常更新；
  2. **干净盘冷启动亮屏**：销毁重建干净盘实例 `gold_15-OS3.0.10.0.VNQCNXM/1`，`boot_check.py` 一次性 `[pass]`（平均亮度 `209.3`，峰值 `255`）；
  3. **系统态运行与 `/proc/<pid>/maps` 零泄露**：
     - `ro.hardware.gralloc = mediatek`（SELinux 标签 `u:object_r:exported_default_prop:s0`，`hal_graphics_allocator_default` 天然可读，运行全程 `avc: denied` = `0` 条）；
     - `android.hardware.graphics.allocator-service.minigbm`（`pid=557`）稳定常驻；
     - `surfaceflinger` 的 `/proc/<pid>/maps` 加载 `/vendor/lib64/hw/mapper.mediatek.so`、`/vendor/lib64/hw/gralloc.mediatek.so`、`/vendor/lib64/libgralloc_extra.so`，无任何 `minigbm` 路径残留；
  4. **普通 App 全量自检 `32 / 32` (`100%`)**：干净盘实例（`ab1a805b0d43ec3a-cloud`，批次 `run-00283`）执行 `proc.maps`、`screen.display`、`screen.resource`、`hardware.gles`、`aligner_xiaomi_gold.display_gpu_webview`、`aligner_xiaomi_gold.all` 6 项套件全部 `succeeded`，`B5_mapsHasRealMaliAndNoMesaMinigbm = true`，`goldScorecard` **`32 / 32` 全绿通过**。

## 4. 改动

1. **拉取上游只读源码仓 `@platform/external/minigbm`**：
   - Android 14（`android-14.0.0_r67`）落位 `engine/vendor/out/android14/aosp/platform/external/minigbm`；
   - Android 15（`android-15.0.0_r36`）落位 `engine/vendor/out/android15/aosp/platform/external/minigbm`，零修改只读复用。
2. **新建纯 C++ 源码自编模块 `android.hardware.graphics.allocator`**：
   - [`engine/vendor/build/modules/android.hardware.graphics.allocator.json`](../../../vendor/build/modules/android.hardware.graphics.allocator.json)：使用 `kind: "hidl"` 多源文件服务编译机制，自动调用 `aidl` 生成 `IAllocator` 等 NDK 头文件、运行 `hidl-gen` 生成 `mapper@4.0` 依赖头，只读编译 `@platform/external/minigbm/cros_gralloc/aidl/Allocator.cpp` 与 `NativeHandle.cpp`，链接底包 `libminigbm_gralloc.so` / `libminigbm_gralloc4_utils.so` / `android.hardware.graphics.allocator-V2-ndk.so` 等库，产出 `bin/hw/android.hardware.graphics.allocator-service.minigbm`；
   - [`engine/vendor/hardware/android.hardware.graphics.allocator/src/CdhAllocator.h`](../../../vendor/hardware/android.hardware.graphics.allocator/src/CdhAllocator.h) 与 [`service.cpp`](../../../vendor/hardware/android.hardware.graphics.allocator/src/service.cpp)（及说明文档 [`README.md`](../../../vendor/hardware/android.hardware.graphics.allocator/README.md)）：`CdhAllocator` 直接继承上游 `aidl::android::hardware::graphics::allocator::impl::Allocator`（完整保留各 Android 版本原生的 `init()`、`allocate()`、`allocate2()`、`isSupported()` 与 `createBinder()` `AIBinder_setInheritRt` 实时优先级继承），仅覆写 `getIMapperLibrarySuffix()` 通过 `android::base::GetProperty("ro.hardware.gralloc", "minigbm")` 动态返回任意长度的厂商后缀。
3. **彻底清空 `07_build_vendor` 机器码修补并接入硬件清单**：
   - [`engine/build/stages/07_build_vendor/elfcompat.go`](../../../build/stages/07_build_vendor/elfcompat.go)、[`elfcompat_test.go`](../../../build/stages/07_build_vendor/elfcompat_test.go)：彻底删除 `encodeAArch64MapperSuffixBlock` 与 `patchAllocatorMapperSuffix`（至此 `engine/build` 全仓机器码二进制补丁清零），并加固 `reconcileVendorGrallocIdentity` 在 `vendor/build.prop` 缺失 `ro.hardware.gralloc` 行时自动补写 `ro.hardware.gralloc=<suffix>`（补齐对应单测）；
   - [`engine/build/internal/device/config.go`](../../../build/internal/device/config.go)、[`engine/build/web/static/app.js`](../../../build/web/static/app.js)、[`gold-android15.json`](../../../manifest/configs/redmi/gold/gold-android15.json)、[`G1170-android15.json`](../../../manifest/configs/blu/G1170/G1170-android15.json)、[`RMX6688-android15.json`](../../../manifest/configs/realme/RMX6688/RMX6688-android15.json)、[`oriole-android14.json`](../../../manifest/configs/google/oriole/oriole-android14.json)：Android 13 / 14 与 Android 15 默认搭载 `android.hardware.graphics.allocator`（`default` / `2.0`）。

## 5. 验证

1. `engine/vendor/build` 全量单测 `go test -count=1 ./...` 与 `engine/build` 单测 `go test -count=1 ./stages/07_build_vendor/... ./internal/device/...` 全部通过。
2. `cdh-vendor hal build android.hardware.graphics.allocator` 在 `--android 15` 与 `--android 14` 下均编译通过。
3. 重编 `cdh` 并执行 `cdh device build gold --manifest 15-OS3.0.10.0.VNQCNXM --from 07_build_vendor --skip 08_build_vendor_dlkm --skip 10_pack_super`，产出 `vendor.img`（`b49d20283c69`）→ `patch.img`（`352cd0b74fa2`）→ `vda.img`（`vda-9679a721d0e2`），`.text` 与 `.rodata` 段零修改。
4. 干净盘重建启动 `gold_15-OS3.0.10.0.VNQCNXM/1`，`boot_check.py` `[pass]`，系统态 `maps` 与 `device-checker` `run-00283`（`32 / 32` 全绿）全部验证通过。

## 6. 遗留

- Android 16（`lineage-23.2`）的 minigbm allocator 封装在 `com.android.hardware.graphics.allocator.minigbm.apex` 内（摊平路径位于 `/vendor/apex/.../bin/hw/` 而非顶层 `/vendor/bin/hw/`），当前 `DefaultHardwareForAndroid` 仅对 Android 13 / 14 / 15 默认启用顶层 `android.hardware.graphics.allocator`；后续清理 Android 16 vendor APEX 形态时可按相同 `CdhAllocator` 源码复用于 APEX 摊平替换。
