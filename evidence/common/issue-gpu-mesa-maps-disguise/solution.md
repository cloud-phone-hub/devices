# issue-gpu-mesa-maps-disguise：GPU Mesa 底层驱动库身份对齐与 `/proc/self/maps` Mali/MEOW 映射收口

## 1. 问题

在 `gold`（Redmi Note 13 5G，`OS3.0.10.0.VNQCNXM`）对齐自检 `run-00231` 中，维度 B 的 `B5_mapsHasRealMaliAndNoMesaMinigbm` 未通过（`27 / 32`）：
- `mapsAudit.hasRealMaliDriverSo = false`：进程 `/proc/self/maps` 中缺少 `/vendor/lib64/egl/libGLES_mali.so` 与 `/vendor/lib64/hw/vulkan.mali.so`；
- `mapsAudit.noMesaOrMinigbmInMaps = false`：`mapsAudit.mesaOrMinigbmLeaks` 检出 5 个底层 Mesa 驱动库路径：
  1. `/vendor/lib64/egl/libEGL_mesa.so`
  2. `/vendor/lib64/egl/libGLESv1_CM_mesa.so`
  3. `/vendor/lib64/egl/libGLESv2_mesa.so`
  4. `/vendor/lib64/libgallium_dri.so`
  5. `/vendor/lib64/libglapi.so`

## 2. 根因

1. **`android.hardware.gpu` 转发层直接按原名 `dlopen` 基底 Mesa 库**：
   `libEGL_meow.so`、`libGLESv1_CM_meow.so`、`libGLESv2_meow.so` 在构造函数 `cdh_init()` 中按原名 `dlopen` `/vendor/lib64/egl/libEGL_mesa.so`、`libGLESv1_CM_mesa.so`、`libGLESv2_mesa.so`，且 `libEGL_mesa.so` 通过 ELF `.dynstr` 的 `DT_NEEDED` 依赖 `libgallium_dri.so` 与 `libglapi.so`；`zygote64` 预加载 OpenGL 时将上述 5 个库映射进地址空间并由所有子进程继承。
2. **`vulkan.mali.so` 未在进程启动期映射，且 `vulkan.virtio.so` 保留虚拟化命名**：
   真机 `gold` 的应用进程 `/proc/self/maps` 同时包含 `/vendor/lib64/egl/libGLES_mali.so` 与 `/vendor/lib64/hw/vulkan.mali.so`，而云机 `zygote64` 仅加载了 `libEGL_meow.so`，未预加载 `/vendor/lib64/hw/vulkan.mali.so`。
3. **`cdh_redirect` 的 `cdh_is_app_sphal_lib` 白名单未覆盖 `egl/` 与 `vulkan.mali.so`**：
   `rules.c` 中 `cdh_is_app_sphal_lib()` 原先匹配了 `/vendor/lib/`（32 位）与通用前缀 `hw/gralloc.`，但未包含 `/vendor/lib64/egl/` 下的转发库及 `hw/vulkan.mali.so`、`libgpud.so`、`libged.so`。

## 3. 证据

- 修复前（[`log-fail.txt`](log-fail.txt)，`run-00231`）：
  - `B5_mapsHasRealMaliAndNoMesaMinigbm: false`
  - `hasRealMaliDriverSo: false`
  - `mesaOrMinigbmLeaks: ["/vendor/lib64/egl/libEGL_mesa.so", "/vendor/lib64/egl/libGLESv1_CM_mesa.so", "/vendor/lib64/egl/libGLESv2_mesa.so", "/vendor/lib64/libgallium_dri.so", "/vendor/lib64/libglapi.so"]`
- 修复后（[`log-pass.txt`](log-pass.txt)，`run-00232`）：
  - `B5_mapsHasRealMaliAndNoMesaMinigbm: true`
  - `hasRealMaliDriverSo: true`（`/vendor/lib64/egl/libGLES_mali.so` 与 `/vendor/lib64/hw/vulkan.mali.so` 均在 `vendorSoList` 与 `gpuSoList` 中）
  - `hasRealMtkMapperSo: true`
  - `mesaOrMinigbmLeaks: []`，`noMesaOrMinigbmInMaps: true`，`suspiciousPathLeaks: []`

## 4. 改动

1. **[`engine/vendor/build/internal/halbuild/gpu.go`](../../vendor/build/internal/halbuild/gpu.go) & [`engine/vendor/hardware/android.hardware.gpu/src/cdh_vulkan_mali.c`](../../vendor/hardware/android.hardware.gpu/src/cdh_vulkan_mali.c)**：
   - 为 `egl`、`gles1`、`gles2` 注入 `CDH_ALT_PATH`（分别为 `/vendor/lib64/egl/libMEOW_data.so`、`/vendor/lib64/egl/libMEOW_gift.so`、`/vendor/lib64/egl/libGLES_mali.so`），在 `cdh_init()` 中优先 `dlopen(CDH_ALT_PATH, RTLD_NOW | RTLD_LOCAL)`，失败再回退 `CDH_BASE_PATH`；
   - 在 `egl`（`libEGL_meow.so`）的 `cdh_init()` 中增加 `(void)dlopen("/vendor/lib64/hw/vulkan.mali.so", RTLD_NOW | RTLD_LOCAL);`，使 `zygote64` 及其派生的应用进程自动映射 `/vendor/lib64/hw/vulkan.mali.so`；
   - 在 `cdh_vulkan_mali.c` 的 `cdh_open()` 中优先加载 `/vendor/lib64/egl/libMEOW_trace.so`。
2. **[`engine/build/stages/07_build_vendor/elfcompat.go`](../../build/stages/07_build_vendor/elfcompat.go) & [`assemble.go`](../../build/stages/07_build_vendor/assemble.go)**：
   - 新增 `reconcileVendorGpuIdentity(plan, stdout)`：
     - 将 `vendor/{bin,lib64,lib}` 下 ELF `.dynstr` 中的 `libgallium_dri.so`（17 B）与 `libglapi.so`（11 B）就地替换为同目录官方库名 `libgpud.so`（10 B）与 `libged.so`（9 B）；
     - 将 `egl/libEGL_mesa.so` 重命名为 `egl/libMEOW_data.so`、`egl/libGLESv1_CM_mesa.so` 重命名为 `egl/libMEOW_gift.so`、`egl/libGLESv2_mesa.so` 重命名为 `egl/libGLES_mali.so`、`libgallium_dri.so` 重命名为 `libgpud.so`、`libglapi.so` 重命名为 `libged.so`、`hw/vulkan.virtio.so` 重命名为 `egl/libMEOW_trace.so`，并清理旧 `DT_SONAME`；
     - 在 `vendor_file_contexts` 中确保上述库具备 `u:object_r:same_process_hal_file:s0` 标签。
3. **[`devices/modules/cdh_redirect/core/rules.c`](../../modules/cdh_redirect/core/rules.c)**：
   - 收紧 `cdh_is_app_sphal_lib()` 仅匹配 `/vendor/lib64/`，并将 `hw/vulkan.mali.so`、`egl/libEGL_meow.so`、`egl/libGLESv1_CM_meow.so`、`egl/libGLESv2_meow.so`、`egl/libGLES_mali.so`、`egl/libMEOW_{data,gift,trace}.so`、`libgpud.so`、`libged.so` 纳入白名单。

## 5. 验证

1. 执行 `go test ./stages/07_build_vendor` 通过。
2. 对 `gold`（`15-OS3.0.10.0.VNQCNXM`）执行 `03 -> 07 -> 08 -> 09 -> 11` 重建（`patch.img` sha256 `dd953218c7c0`，`vda-e1cff16b6c0d`），重启云机实例 `"1"`。
3. 触发 `device-checker` `run-00232`：
   - `B5_mapsHasRealMaliAndNoMesaMinigbm = true`（维度 B `7 / 7` 全部通过）；
   - `aligner_xiaomi_gold.all` 总分从 `27 / 32` 提升至 **`28 / 32`**。

## 6. 遗留

- 维度 C 剩余 `C2_displaySupports120HzAndWideColorGamut`、`C4_glesGlsl320AndArmExtsNoDesktopLeak`、`C5_webViewWebGlReadyAndMatchesMali` 及维度 H 剩余 `H1_criticalSystemServicesAndIfaaReady` 共 4 项待后续条目逐一收口。
