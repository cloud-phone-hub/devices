# #187：Vulkan 物理设备 / `pNext` 链 / UUID 全量伪装与 OpenGL ES 6 项硬件上限对齐

## 1. 问题

继 `../issue-gpu-mesa-maps-disguise/`（`#181`）与 `../issue-gpu-glsl320-arm-exts-and-webview-webgl/`（`#182`）解决驱动库路径与 GLSL/扩展名之后，深水区图形硬件探针仍检出两处底层特征差异：

1. **Vulkan 1.1/1.2 `pNext` 扩展属性链与 UUID 未完整覆写**：应用直接通过 `libvulkan.so` 调用 `vkGetPhysicalDeviceProperties` 及 `vkGetPhysicalDeviceProperties2` / `vkGetPhysicalDeviceProperties2KHR` 查询 `VkPhysicalDeviceIDProperties`、`VkPhysicalDeviceDriverProperties`、`VkPhysicalDeviceVulkan11Properties`、`VkPhysicalDeviceVulkan12Properties` 时，底层 `vulkan.virtio.so`（Venus/Mesa）仍可能暴露 Virtio `vendorID`/`deviceID`、Mesa 驱动名/驱动信息或全零/虚拟 `deviceUUID`/`driverUUID`/`pipelineCacheUUID`。
2. **OpenGL ES `glGetIntegerv` 暴露 Mesa 桌面级纹理上限**：云机 `GL_MAX_TEXTURE_SIZE`、`GL_MAX_CUBE_MAP_TEXTURE_SIZE`、`GL_MAX_RENDERBUFFER_SIZE` 返回 Mesa 软/虚拟渲染默认上限 `32768`（`GL_MAX_VERTEX_ATTRIBS = 32`），而 Mali-G57 MC2 真机硬件上限为 `16383`（2 的幂档位 `16384`）且 `GL_MAX_VERTEX_ATTRIBS = 16`。

## 2. 根因

1. **`cdh_vulkan_mali.c` 原先仅做模块加载转发，未深层拦截 `pNext` 结构体链表**：Vulkan 1.1+ 风控探针（如 `ultimate.hardware_env`）不仅读取顶层 `VkPhysicalDeviceProperties`，还会串联 `sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES`（`1000071004`）、`VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES`（`1000196000`）、`VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES`（`49`）与 `VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES`（`50`）读取驱动 ID（`VK_DRIVER_ID_ARM_PROPRIETARY = 9`）、驱动名（`"Arm Proprietary"`）及设备 UUID。
2. **`cdh_gl_strings.c` 原先只拦截 `glGetString` / `glGetStringi` 与 `glShaderSource`，未拦截 `glGetIntegerv`**：应用调用 `GLES20.glGetIntegerv` 查询 6 项核心能力常量时直通底层 Gallium `virgl` 状态跟踪器，返回桌面 GPU 的 `32768` 纹理尺寸与 `32` 顶点属性槽位。
3. **`reconcileVendorGpuIdentity` 变体判定范围偏窄**：`07_build_vendor/elfcompat.go` 对 GPU 伪装的触发条件需兼容各厂商 Mali/Adreno 配置属性，确保 `libEGL_meow.so` / `libGLESv2_meow.so` / `vulkan.mali.so` 稳定接管符号解析。

## 3. 证据

- 失败日志见 [`log-fail.txt`](log-fail.txt)：
  - 修复前 `glGetIntegerv` 返回桌面级 `maxTextureSize = 32768`、`maxVertexAttribs = 32`，且 Vulkan `pNext` 链未统一覆写为 ARM Mali-G57 MC2 专有驱动身份。
- 通过日志见 [`log-pass.txt`](log-pass.txt)：
  - 全新干净盘双机复测（`run-00014..00020`）中，`display_gpu_webview.gles` 实测 `maxTextureSize = 16384`、`maxCubeMapTextureSize = 16384`、`maxRenderbufferSize = 16384`、`maxVertexAttribs = 16`、`maxTextureImageUnits = 32`、`maxCombinedTextureImageUnits = 160`，彻底消除 `32768` 桌面特征。
  - `ultimate.hardware_env` 中 Vulkan 物理设备名称、VendorID（`0x13B5` ARM）、DriverID（`ARM_PROPRIETARY`）、UUID 及 `pNext` 链检测 **0 命中**（与真机完全一致）。

## 4. 改动

1. **Vulkan `Properties` / `Properties2{,KHR}` 与 `pNext` 链全量覆写**：
   - [`engine/vendor/hardware/android.hardware.gpu/src/cdh_vulkan_mali.c`](../../../vendor/hardware/android.hardware.gpu/src/cdh_vulkan_mali.c)：拦截 `vkGetInstanceProcAddr` / `vkGetDeviceProcAddr` 分发的 `vkGetPhysicalDeviceProperties`、`vkGetPhysicalDeviceProperties2` 与 `vkGetPhysicalDeviceProperties2KHR`；将 `vendorID`（默认 `0x13B5` ARM）、`deviceID`（默认 `0x90930000` Mali-G57）、`apiVersion`、`driverVersion`、`deviceType = VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU`、`deviceName`（`"Mali-G57 MC2"`）及基于机型标识确定性派生的非零 `pipelineCacheUUID`、`deviceUUID`、`driverUUID` 全量写入，并遍历 `pNext` 链覆写 `VkPhysicalDeviceIDProperties`、`VkPhysicalDeviceDriverProperties`（`driverID = VK_DRIVER_ID_ARM_PROPRIETARY`，`driverName = "Arm Proprietary"`）、`VkPhysicalDeviceVulkan11Properties` 与 `VkPhysicalDeviceVulkan12Properties`。
2. **OpenGL ES `glGetIntegerv` 6 项上限拦截与配置化**：
   - [`engine/vendor/hardware/android.hardware.gpu/src/cdh_gl_strings.c`](../../../vendor/hardware/android.hardware.gpu/src/cdh_gl_strings.c)：新增 `glGetIntegerv` 覆写（同步加入 `libEGL_meow.so` 的 `eglGetProcAddress` 转发列表），支持通过 `ro.vendor.cdh.gpu.max_*` 属性配置并默认对齐移动 GPU 档位（纹理/CubeMap/Renderbuffer 上限收敛至 `<= 16384`、`GL_MAX_VERTEX_ATTRIBS = 16`、`GL_MAX_TEXTURE_IMAGE_UNITS = 32`）。
3. **装配阶段 GPU 伪装覆盖率与单测加固**：
   - [`engine/build/stages/07_build_vendor/elfcompat.go`](../../../build/stages/07_build_vendor/elfcompat.go) 与 [`elfcompat_test.go`](../../../build/stages/07_build_vendor/elfcompat_test.go)：扩展 GPU 伪装匹配条件并补充单测。
   - [`devices/manifest/configs/redmi/gold/gold-android15.json`](../../../manifest/configs/redmi/gold/gold-android15.json)：补充 `ro.vendor.cdh.vulkan.*` 与 `ro.vendor.cdh.gpu.max_*` 机型参数。

## 5. 验证

1. 全量重编 `android.hardware.gpu`（Android 14 / 15 / 16 / 17），跑通 `cd engine/vendor/build && go test -count=1 ./...` 与 `cd engine/build && go test -count=1 ./stages/07_build_vendor`。
2. 重打 `gold` `15-OS3.0.10.0.VNQCNXM`（`patch.img` `196c697bb633`，`vda-4ab0452c965b`），干净盘冷启动 `boot_check.py` `[pass]`（桌面截图亮度 `209.3`）。
3. 双机实测 `hardware.gles`、`aligner_xiaomi_gold.display_gpu_webview`（`C1..C5` 全绿）与 `ultimate.hardware_env`（0 云机独有命中）全部通过。

## 6. 遗留

- Mesa GLES 状态跟踪器在内部创建纹理对象时按 `1 << (MaxTextureLevels - 1)` 约束 2 的幂（`16384`），而 ARM Mali 闭源驱动对 `GL_MAX_TEXTURE_SIZE` 返回 `16383`（`2^14 - 1`）；当前 `16384` 已完全消除桌面虚拟 GPU 的 `32768` 特征且不影响 WebGL/Skia 2 的幂纹理分配假设。
