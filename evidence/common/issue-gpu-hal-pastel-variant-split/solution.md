# issue: `android.hardware.gpu` 按渲染基座拆分为 `default` 与 `variants.pastel`，解耦软光栅补丁并去重配置解析

机型：跨机型通用（`gold` / `oriole` / `cph2797` 等 Venus 机型走纯净 `default`；`popsicle` HyperOS 4 走 `variant: "pastel"`）
阶段：`08_build_hardware`（`halcore/spec` + `halcore/halbuild` + `devices/hardware/android.hardware.gpu`）& `09_build_vendor`
时间：2026-11-18
改动类型：GPU HAL 模块变体化重构 + 公共配置头抽离 + 宿主 C / Go 单元测试

## 1. 问题

在打通 `popsicle`（Xiaomi 17 Pro Max，HyperOS 4 / Android 16，见 `../../popsicle/issue-android16-boot-sphal-pac-and-qti-mapper/` 与 `../../popsicle/issue-powerkeeper-thermal-crash-and-blur-jank/`）时，由于 HyperOS 4 系统级锁死 Flutter Impeller Vulkan 管线且 macOS 宿主使用 SwiftShader（`vulkan.pastel.so`）软光栅后端，`devices/hardware/android.hardware.gpu/src/cdh_vulkan_mali.c` 内塞入了大量针对 `pastel` 的专用补丁（SPIR-V `Float16 -> Float32` 字节码提升、`VK_SAMPLE_COUNT_1_BIT` 1x 采样上限收敛），文件膨胀至 737 行：

1. **跨路线耦合污染**：`gold`、`oriole`、`cph2797` 等常规机型使用宿主加速的 `vulkan.virtio.so`（Mesa Venus）与 Skia GLES，根本不需要 SPIR-V 半精度重写或软光栅 MSAA 钳制，但 `halbuild/gpu.go` 硬编码单文件编译导致这些软光栅逻辑与日志串被无条件编进所有机型的 `vulkan.mali.so`；
2. **重复解析代码**：`cdh_gl_strings.c` 与 `cdh_vulkan_mali.c` 各自复制了一份约 105 行的 `/vendor/lib64/egl/gpu_id.cfg` 键值缓存解析器，维护时需双端同步。

## 2. 根因

1. **为什么按渲染后端 `variants` 拆分，而不按 Android 版本或单台机型拆目录**：
   - `08_build_hardware` 已天然按 `out/devices/vendor/android{14,15,16}[/<variant>]` 隔离构建产物，并从各代 `lineage` 基底镜像动态提取 `libEGL_mesa.so` / `libGLESv2_mesa.so` 导出符号表生成零手写签名的汇编蹦床；
   - 各机型硬件身份（Mali-G57 / Adreno 840 / Xclipse、VendorID/DeviceID、UUID、GL 扩展与纹理上限）已全部数据驱动化，由 `09_build_vendor` 抽离写入 `/vendor/lib64/egl/gpu_id.cfg`；
   - 真正的代码行为分水岭仅有**底层 Vulkan 渲染基座差异**：常规 `virtio`（Venus）vs 软光栅 `pastel`（SwiftShader）。这与 `android.hardware.health`（`oplus`/`samsung`）和 `android.hardware.graphics.composer3`（`oplus`，见 `../issue-hal-oplus-variant-split/`）完全同构。
2. **构建器缺口**：`08_build_hardware` 与 `09_build_vendor` 已原生支持非基础变体目录路由与独立缓存指纹（`<module>@<variant>.sha256`），但 `halcore/spec.GpuVulkan` 缺少 `Sources` 字段，且 `halcore/halbuild/gpu.go` 硬编码只编 `cdh_vulkan_mali.c` 并忽略了 `Module.IncludeDirs` 与 `Module.Compile.Defines`。

## 3. 证据

- `log-fail.txt`：拆分前 `cdh_vulkan_mali.c` 737 行混杂 `pastel` 软光栅字节码改写与采样钳制逻辑，且 `gpu_id.cfg` 解析代码在 GL/Vulkan 两处重复。
- `log-pass.txt`：拆分后全仓 30 个流水线测试包与宿主 C 单测全绿；实编对比确认 `android15`/`android16` 的 `default` 变体 `vulkan.mali.so` 降至 **`13,248 B`**（零 `SPIR-V Float16` / `keep_msaa` 符号残留），`android16/pastel` 变体 `vulkan.mali.so` 为 **`18,320 B`** 并由 `popsicle` `09_build_vendor` 精准消费落盘。

## 4. 改动

1. **抽离公共底座与 `pastel` 变体源码（`devices/hardware/android.hardware.gpu/`）**：
   - 新增 `src/cdh_gpu_cfg.h`：统一 GL 与 Vulkan 的 `gpu_id.cfg` + `__system_property_get` 读取逻辑（多编译单元同库内通过 `weak` + `hidden` 共享单份 `.bss` 缓存，消除两处共 ~210 行重复代码，并兼容 `CDH_GL_STRINGS_HOST_TEST` 宿主单测）；
   - 新增 `src/cdh_vulkan_ext.h`：定义编译期扩展钩子，未定义 `CDH_VK_PASTEL_COMPAT` 时全部展开为空内联函数（零开销），定义时链接 `pastel` 实现；
   - 新增 `src/pastel/cdh_spv_fp16.h`（纯 C99 可移植 SPIR-V `Float16 -> Float32` 提升算法）与 `src/pastel/cdh_vulkan_pastel.c`（封装 SPIR-V 拦截与 `1x` `VK_SAMPLE_COUNT_1_BIT` 收敛），新增宿主单测 `src/pastel/cdh_spv_fp16_test.c`；
   - 精简 `src/cdh_gl_strings.c` 与 `src/cdh_vulkan_mali.c` 为纯通用转发与身份覆写底座，补充 `README.md`。
2. **扩展 `halcore` 构建器与模块配置**：
   - `agent/modules/pipeline/internal/halcore/spec/spec.go`：`GpuVulkan` 新增可选 `Sources []string`；
   - `agent/modules/pipeline/internal/halcore/halbuild/gpu.go`：新增 `resolveGpuVulkanSources` 与 `normalizeGpuDefines`，在 `compileGpuLib` 中透传去重后的 `IncludeDirs` 与 `Compile.Defines`；
   - `agent/modules/pipeline/stages/08_build_hardware/modules/android.hardware.gpu.json`：声明 `variants.pastel`（独立 `workSubdir: "gpu-interposer-pastel-work"`、`-DCDH_VK_PASTEL_COMPAT=1`、基座 `vulkan.pastel.so`、源 `["cdh_vulkan_mali.c", "pastel/cdh_vulkan_pastel.c"]`）；
   - `agent/modules/pipeline/stages/09_build_vendor/files_compat.go`：`alignVendorGpuConfig` 同步覆盖 `pastel` 属性替换与 `vulkan.pastel.so` 的 `same_process_hal_file` 标签。
3. **切换 `popsicle` 清单并补齐同目录单测**：
   - `devices/manifest/configs/xiaomi/popsicle/popsicle-android16/{hardware,vendor}/config.json` 将 `android.hardware.gpu` 的 `variant` 从 `"default"` 切为 `"pastel"`；
   - 新增 `spec_test.go` 的 `TestRealGpuDefaultAndPastelVariants` 与 `halbuild/gpu_test.go`（含自动编译运行 `cdh_gl_strings_test.c` 与 `cdh_spv_fp16_test.c` 的 `TestGpuHostCUnitSuites`）。

## 5. 验证

1. **Go 与宿主 C 单元测试**：
   - `cd agent && TMPDIR=.../out/agent/tmp go test -count=1 ./modules/pipeline/...` 全 30 个有测包一次通过（含 `cdh_gl_strings_test: 5/5 passed` 与 `cdh_spv_fp16_test: 2/2 passed`）。
2. **跨版本双变体实编与 `09_build_vendor` 装配校验**：
   - `cdh-pipeline build-hardware --android 15 --module android.hardware.gpu --refresh` 与 `--android 16` 产出纯净 `default` `vulkan.mali.so`（`13,248 B`，`strings` 验证仅指向 `vulkan.virtio.so`，`SPIR-V Float16` 与 `keep_msaa` 命中为 `0`）；
   - `cdh-pipeline device build popsicle --manifest 16-OS4.0.5.0.XPBCN --only 08_build_hardware` 与 `--only 09_build_vendor` 自动选中 `android.hardware.gpu@pastel`，产出 `out/devices/vendor/android16/pastel/lib64/hw/vulkan.mali.so`（`18,320 B`，含 `vulkan.pastel.so`、`SPIR-V Float16->Float32` 与 `keep_msaa`）并装配进 `popsicle` 的 `vendor.img`。

## 6. 遗留

- 当前运行中的 `popsicle` 实例 `#1` 正只读挂载 `out/devices/dist/xiaomi/popsicle/16-OS4.0.5.0.XPBCN/patch.img`，为不中断在线会话暂未覆盖 `dist/patch.img`；下次停机执行 `11_build_patch` 时将自动打入新装配的 `vendor.img`（运行期行为与当前在线实例一致）。
