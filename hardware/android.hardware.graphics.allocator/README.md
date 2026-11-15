# android.hardware.graphics.allocator（AIDL V2 / Minigbm 全家桶源码自编模块）

本模块从纯 C / C++ 源码编译产出完整的 Minigbm 图形分配与映射组件：
- `/vendor/bin/hw/android.hardware.graphics.allocator-service.minigbm`
- `/vendor/lib64/hw/mapper.minigbm.so`（无 `DT_SONAME`，装配期原样重命名为 `mapper.<suffix>.so`）
- `/vendor/lib64/hw/gralloc.minigbm.so`（无 `DT_SONAME`，装配期原样重命名为 `gralloc.<suffix>.so`）

适配 Android 13 / 14（`lineage-21.0`）与 Android 15（`lineage-22.2`）图形栈，**100% 脱离 `libminigbm_gralloc*.so` 共享库，无需任何 ELF `.text` / `.rodata` / `.dynamic` / `.dynstr` / `DT_SONAME` 二进制修补**。

## 设计说明

1. **零侵入继承上游 `external/minigbm` 并静态内聚 `libminigbm_core.a`**：
   - 直接编译对应 Android 版本 AOSP tag 的 `@platform/external/minigbm` C 驱动核心为静态库 `libminigbm_core.a`，内聚进 `allocator-service.minigbm` 与 `mapper.minigbm.so`，彻底消除 `/proc/<pid>/maps` 中的 `libminigbm_gralloc*.so` 痕迹。
2. **同进程 `cros_gralloc_driver` 单例共享（`mapper` + `gralloc_proxy.c`）**：
   - 上游 `cros_gralloc_driver::get_instance()` 使用局部静态单例维护进程内已导入的 `buffer_handle_t` 表。
   - `mapper.minigbm.so` 同时编译 `CrosGralloc4Mapper.cc`、`Mapper.cpp` 与 `gralloc0.cc`，一并导出 `AIMapper_loadIMapper`（IMapper v5）、`HIDL_FETCH_IMapper`（IMapper v4）与 `HMI`（`gralloc0`），共享进程内唯一一份 `cros_gralloc_driver` 实例。
   - `gralloc.minigbm.so` 由纯 C 轻量转发层 [`src/gralloc_proxy.c`](src/gralloc_proxy.c) 编译产出（不重复静态链接 `libminigbm_core.a`），运行态读取公开属性 `ro.hardware.gralloc`，`dlopen` 同目录下的 `mapper.<suffix>.so`（Bionic 按 inode 复用已加载的同一 `soinfo`）并通过 `dlsym("HMI")` 转发全部 `gralloc0` 操作（如 Mesa 的 `GRALLOC_DRM_GET_BUFFER_INFO`）。
3. **运行态动态返回 Mapper 后缀**：
   - `CdhAllocator` 继承 `aidl::android::hardware::graphics::allocator::impl::Allocator`，覆写 `getIMapperLibrarySuffix()` 通过 `android::base::GetProperty("ro.hardware.gralloc", "minigbm")` 动态返回厂商后缀（如 `mediatek`、`qti`、`pixel`）。
   - `mapper.minigbm.so` 与 `gralloc.minigbm.so` 均设置 `"soname": "-"`（不生成 `DT_SONAME`），`07_build_vendor` 仅执行纯文件 `os.Rename` 与纯文本配置对齐，产物二进制逐字节零修改。

## 构建命令

```bash
cdh-vendor hal build android.hardware.graphics.allocator --android 14
cdh-vendor hal build android.hardware.graphics.allocator --android 15
```
