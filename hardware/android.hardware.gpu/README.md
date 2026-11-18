# android.hardware.gpu（EGL / GLES / Vulkan 拦截转发与变体模块）

本模块从各代 `lineage` 基底镜像动态抽取导出符号生成零手写签名的 GL/EGL 转发库，并提供按渲染基座变体解耦的 Vulkan HAL 转发库：

- `/vendor/lib64/egl/libEGL_meow.so`
- `/vendor/lib64/egl/libGLESv1_CM_meow.so`
- `/vendor/lib64/egl/libGLESv2_meow.so`
- `/vendor/lib64/hw/vulkan.mali.so`

## 架构分层与变体设计

无需按 Android 版本（14/15/16）或单个机型复制代码——`08_build_hardware` 自动按 `out/devices/vendor/android<ver>[/<variant>]` 隔离构建产物，机型硬件身份参数（Mali / Adreno / Xclipse 等）统一由 `09_build_vendor` 写入 `/vendor/lib64/egl/gpu_id.cfg`。源码按职责与渲染后端变体拆分：

### 1. 公共底座（`src/`）
- [`src/cdh_gpu_cfg.h`](src/cdh_gpu_cfg.h)：统一的 `gpu_id.cfg` 与系统属性解析器（GL 与 Vulkan 共用）。
- [`src/cdh_gl_strings.c`](src/cdh_gl_strings.c) + [`src/cdh_gl_shader_patch.h`](src/cdh_gl_shader_patch.h)：覆写 `glGetString{,i}` / `glGetIntegerv` / `eglGetProcAddress`，过滤宿主特征扩展，并将 `#version 310/320 es` 降级兼容至 Mesa VirGL `300 es`。
- [`src/cdh_vulkan_mali.c`](src/cdh_vulkan_mali.c) + [`src/cdh_vulkan_ext.h`](src/cdh_vulkan_ext.h)：纯通用的 `hwvulkan` HMI 装载与 `VkPhysicalDeviceProperties` / `DriverProperties` / `UUID` 身份覆写底座。

### 2. 变体划分（`08_build_hardware/modules/android.hardware.gpu.json`）
| 变体名 (`variant`) | 默认 Vulkan 基座 | 适用机型 | 差异能力 |
| :--- | :--- | :--- | :--- |
| `default` | `vulkan.virtio.so`（Mesa Venus） | `gold`、`oriole`、`cph2797`、`NX809J`、`PD2454` 等常规机型 | 仅编入 `cdh_vulkan_mali.c` 通用底座，**不包含**任何软光栅 SPIR-V 或 MSAA 干预钩子 |
| `pastel` | `vulkan.pastel.so`（SwiftShader） | `popsicle`（HyperOS 4 等锁死 Impeller Vulkan 且在 macOS 走软光栅的机型） | 开启 `-DCDH_VK_PASTEL_COMPAT=1` 并编入 [`src/pastel/cdh_vulkan_pastel.c`](src/pastel/cdh_vulkan_pastel.c)（含 [`src/pastel/cdh_spv_fp16.h`](src/pastel/cdh_spv_fp16.h) SPIR-V `Float16 -> Float32` 提升与 `1x` 采样上限收敛） |

## 构建与单测

```bash
# 构建默认变体（Venus + VirGL）
cdh-pipeline build-hardware --android 15 --module android.hardware.gpu --refresh

# 构建 pastel 变体（SwiftShader Vulkan + VirGL）
cdh-pipeline build-hardware --android 16 --module android.hardware.gpu --variant pastel --refresh

# 运行 Go 与宿主 C 单元测试
cd agent && go test ./modules/pipeline/internal/halcore/...
```
