# #191：纯 C++ 源码自编 `android.hardware.graphics.composer3` 替代 `07_build_vendor` AArch64 二进制机器码补丁

## 1. 问题

在 [`#183`](../issue-hwc3-display-120hz-wide-color-gamut/solution.md) 与 [`#190`](../issue-gold-fine-field-alignment-input-gpu-audio-wifi-edid/solution.md) 中，为使 Android 15（`lineage-22.2`）云机上的 `android.hardware.composer.hwc3-service.drm` 支持过滤 `virtio-gpu` 冗余 VESA 分辨率、合成 `60/120/90 Hz` 同组高刷并消除运行态 KMS modeset 阻塞、开启 `NATIVE/SRGB/DISPLAY_P3` 广色域、注入精确物理像素密度 `xdpi/ydpi` 以及返回 128 字节标准 EDID v1.3 面板身份，当时在 [`07_build_vendor/elfcompat.go`](../../../build/stages/07_build_vendor/elfcompat.go) 中编写了 `reconcileVendorHwcDisplayModes` 与 `patchDrmHwc3DisplayConfig`，对底包预编译的 `bin/hw/android.hardware.composer.hwc3-service.drm` 原地替换了 **9 段硬编码 AArch64 机器码指令序列**（共 455 行）。

这种直接修补预编译 ELF 机器码的做法存在显著工程隐患：
1. **脆性极高**：强耦合特定底包编译器版本生成的寄存器分配、栈帧偏移（如 `[sp, #0xc]`、`[x0, #0x98]`）与指令字节序列，一旦底包重编或升级编译器即静默失配或引发崩溃；
2. **职责越界**：违反「`engine/vendor` 负责源码编译虚拟 HAL、`07_build_vendor` 只消费编译产物并装配」的清晰分层（与 [`#117`](../issue-composer-compat-vendor-compile/solution.md) 治理方向一致）；
3. **可维护性差**：刷新率档位数量（原先硬编码上限 3 档）与像素时钟倍率靠位运算拼装 AArch64 指令，难以阅读、调试与扩展。

## 2. 根因

1. Android 13 / 14（`lineage-21.0`）已在 `engine/vendor/hardware/android.hardware.graphics.composer` 中通过源码编译 HIDL `composer@2.4-service`，但 Android 15（`lineage-22.2`）切换到了 AIDL `android.hardware.graphics.composer3-V4-ndk`，此前 `engine/vendor` 尚未建立对应的 `composer3` 源码编译模块，而是直接沿用了 `lineage-22.2` 底包内置的预编译 `hwc3-service.drm`；
2. `cdh-vendor` 构建器（[`engine/vendor/build/internal/halbuild/hidl.go`](../../../vendor/build/internal/halbuild/hidl.go)）原先在 `kind: "hidl"` 模式下仅编译 `service.mains` 单个入口文件并链接 `libs` 共享库，尚不支持像 `kind: "aidl"` 那样将上游仓库源码目录（`@platform/external/drm_hwcomposer` 的 34 个 `.cpp`）与模块自有源码（`src/*.cpp`）作为静态对象单元直接编译链接进服务 PIE 可执行文件。

## 3. 证据

- 改造前状态见 [`log-fail.txt`](log-fail.txt)：`07_build_vendor/elfcompat.go` 含 `reconcileVendorHwcDisplayModes` / `patchDrmHwc3DisplayConfig` 等 455 行 AArch64 机器码替换逻辑，`assemble.go` 在每次装配 vendor 时原地篡改预编译 `android.hardware.composer.hwc3-service.drm`。
- 改造后构建与干净盘真机终验日志见 [`log-pass.txt`](log-pass.txt)：
  1. **纯源码编译零修补落盘**：`cdh-vendor hal build android.hardware.graphics.composer3 --android 15` 编译上游 `@platform/external/drm_hwcomposer`（零修改只读复用）+ 模块自有包装层共 38 个编译单元，链接底包 `/vendor/lib64/android.hardware.graphics.composer3-V4-ndk.so`（ABI 版本 4，接口哈希 `d24fcd9648b8b2e7287f9238eee9180244612c10`）产出 `out/vendor/android15/bin/hw/android.hardware.composer.hwc3-service.drm`（SHA-256 `e1569e982808450bf1e4689f751745633d4c01c81d39dcf364ba2aa6afb067f3`），与 `07_build_vendor` 装配树内同名二进制 SHA-256 **逐字节一致**；
  2. **干净盘冷启动与动态高刷切换**：销毁重建干净盘实例 `gold_15-OS3.0.10.0.VNQCNXM/1`，`boot_check.py` 一次性 `[pass]`（平均亮度 `209.3`，峰值 `255`）；触摸滑动瞬间升至 `mode 2 (120.00001 Hz)`、静止自动回落 `mode 1 (60.0 Hz)`，全程无 KMS `MODE_ID` 原子提交阻塞、无 `VSyncWorker` 死锁；
  3. **系统态 `dumpsys display` 100% 对齐**：
     - `uniqueId="local:4627039422300187648"`；
     - `supportedModes` 仅含 `1080×2400` 同组 3 档（`60.0` / `120.00001` / `90.0 Hz`），彻底剔除 `1024×768` / `800×600` / `640×480` VESA 桌面模式；
     - `supportedColorModes [0, 7, 9]`（`NATIVE` / `SRGB` / `DISPLAY_P3`）；
     - `density 440 (394.40994 x 393.39185) dpi`；
     - `DeviceProductInfo{name=MTKDEV, manufacturerPnpId=MTK, productId=35638, modelYear=null, manufactureDate=ManufactureDate{week=42, year=2021}, connectionToSinkType=0}`；
  4. **普通 App 全量自检 `32 / 32` (`100%`)**：干净盘实例（`0fb0407b27c20848-cloud`，批次 `run-00282`）执行 `screen.display`、`screen.resource`、`hardware.gles`、`aligner_xiaomi_gold.display_gpu_webview`、`aligner_xiaomi_gold.all` 5 项套件全部 `succeeded`，`RealMetrics` 为 `xdpi = 394.4099426269531`、`ydpi = 393.391845703125`，`WideColorGamut = true`（`Display P3`），`goldScorecard` **`32 / 32` 全绿通过**。

## 4. 改动

1. **扩展 `cdh-vendor` 支持 `kind: "hidl"` 服务直接编译多源码目录与上游外置源文件**：
   - [`engine/vendor/build/internal/halbuild/hidl.go`](../../../vendor/build/internal/halbuild/hidl.go)：新增 `buildHidlServiceSharedObjs`，自动收集模块 `src/` 顶层非 `main` `.cpp`、`extraSourceDirs` 与 `externalSources` 编译为 `.o` 并链接进服务 PIE 二进制（保持既有 `composer@2.4` 等模块行为完全不变）；
   - [`engine/vendor/build/internal/halbuild/build.go`](../../../vendor/build/internal/halbuild/build.go)：`externalSources()` 在 `DryRun` 下跳过文件存在性硬校验。
2. **新建纯 C++ 源码自编模块 `android.hardware.graphics.composer3`**：
   - [`engine/vendor/build/modules/android.hardware.graphics.composer3.json`](../../../vendor/build/modules/android.hardware.graphics.composer3.json)：声明上游只读源码 `@platform/external/drm_hwcomposer`（Android 15 `lineage-22.2` 分支 34 个 `.cpp`）+ `NativeHandle.cpp`，自动运行 `hidl-gen` 与 `aidl` 生成头文件，并从底包提取 13 个链接期库，编译产出 `bin/hw/android.hardware.composer.hwc3-service.drm`；
   - [`engine/vendor/hardware/android.hardware.graphics.composer3/src/CdhDisplayConfig.{h,cpp}`](../../../vendor/hardware/android.hardware.graphics.composer3/src/CdhDisplayConfig.h)：运行态动态解析 `ro.vendor.cdh.display.{refresh_rates,wide_color_gamut,xdpi,ydpi,edid_pnp,edid_name,edid_product_id,edid_week,edid_year}`（及 `ro.surface_flinger.has_wide_color_display` 回退），支持任意数量刷新率档位（不再受 3 档硬编码限制），并按 VESA EDID v1.3 标准生成带校验和的 128 字节面板身份块；
   - [`engine/vendor/hardware/android.hardware.graphics.composer3/src/CdhComposer.{h,cpp}`](../../../vendor/hardware/android.hardware.graphics.composer3/src/CdhComposer.h) 与 [`service.cpp`](../../../vendor/hardware/android.hardware.graphics.composer3/src/service.cpp)（及说明文档 [`README.md`](../../../vendor/hardware/android.hardware.graphics.composer3/README.md)）：以 Decorator 模式包装上游 `DrmHwcThree` / `ComposerClient`（上游源码零改动），在 C++ 层干净实现 VESA 桌面分辨率过滤、同 `configGroup` 多档刷新率合成、无阻塞虚拟高刷档位切换与 `onVsync` 周期同步、广色域色彩模式/渲染意图/单位饱和度矩阵、各向异性物理 `xdpi/ydpi`（覆盖 HWC3 v3 `DisplayConfiguration::dpi` 与 v1/v2 `DPI_X`/`DPI_Y`）以及 `getDisplayIdentificationData` 标准 EDID 返回。
3. **彻底删除 `07_build_vendor` 机器码修补并接入默认硬件清单**：
   - [`engine/build/stages/07_build_vendor/assemble.go`](../../../build/stages/07_build_vendor/assemble.go)、[`elfcompat.go`](../../../build/stages/07_build_vendor/elfcompat.go)、[`elfcompat_test.go`](../../../build/stages/07_build_vendor/elfcompat_test.go)：整块删除 `reconcileVendorHwcDisplayModes`、`patchDrmHwc3DisplayConfig`、`buildEdid128` 等全部 455 行二进制指令修补代码及 200 行机器码补丁测试；
   - [`engine/build/internal/device/config.go`](../../../build/internal/device/config.go)、[`engine/build/web/static/app.js`](../../../build/web/static/app.js)、[`gold-android15.json`](../../../manifest/configs/redmi/gold/gold-android15.json)、[`G1170-android15.json`](../../../manifest/configs/blu/G1170/G1170-android15.json)：Android 15 默认搭载 `android.hardware.graphics.composer3`（`default` / `3.0`）。

## 5. 验证

1. `engine/vendor/build` 全量单元测试 `go test -count=1 ./...` 与 `engine/build` 单元测试 `go test -count=1 ./stages/07_build_vendor/... ./internal/device/...` 全部通过。
2. 执行 `cdh device build gold --manifest 15-OS3.0.10.0.VNQCNXM --from 07_build_vendor --skip 08_build_vendor_dlkm --skip 10_pack_super`，`07` 自动调用 `cdh-vendor` 源码编译 `android.hardware.graphics.composer3` 并直接落入 `vendor.img` → `patch.img`（`f1a4f31c8b6d`）→ `vda.img`（`vda-1b76a4f7f922`），全程无任何二进制指令修补步骤。
3. 干净盘重建启动 `gold_15-OS3.0.10.0.VNQCNXM/1`，`boot_check.py` `[pass]`，`dumpsys display`、动态刷新率切换与 `device-checker` `run-00282`（`32 / 32` 全绿）全部验证通过。

## 6. 遗留

- Android 16（`lineage-23.2`）图形栈位于 vendor APEX（`com.android.hardware.graphics.composer.drm_hwcomposer_upstream.apex`）且本地尚未拉取 `android16` 的 `external/drm_hwcomposer` 源码仓，当前 `DefaultHardwareForAndroid` 仅对 `major == "15"` 默认启用 `android.hardware.graphics.composer3`；后续 Android 16 机型需定制 HWC3 时，拉取对应分支 `external/drm_hwcomposer` 后即可直接复用同一套 `CdhComposer` 源码编译。
