# #342 根治 `Honor X80i 5G` 抖音冷启动黑屏/崩溃（MTK 板级子目录原厂 Mali Mapper 穿透 + Allocator V1 缺回退）并打通荣耀相机 `IHwCameraResourceService`

- **编号**：#342
- **设备 / 分组**：`common`（跨机型 `cdh_redirect.ko` 板级子目录 `hw/<board>/` SP-HAL 分类与 `/vendor/lib(64)/` 框架库白名单、`09_build_vendor` 板级子目录及厂商后缀 `mapper@4.0-impl*.so` 同 inode 软链接遮蔽、`android.hardware.graphics.allocator` AIDL V1 `allocate()` 4 级回退、`android.hardware.camera@honor` 变体 `IHwCameraResourceService` AIDL 服务端及策略/VINTF，首发实机闭环 `Honor X80i 5G #1` `16-TP1A.220624.014-20260422`）
- **日期**：2026-11-26
- **状态**：**已闭环**（`Honor X80i 5G #1` 抖音冷启动秒出高清视频流、原厂 `fd:06` Mali Mapper 映射彻底清零；荣耀相机取景框实时输出虚拟相机画面且 `HwServiceHost` 全程零崩溃；`09_build_vendor` 单测、`allocator` Android 13/16、`camera@honor` Android 13/15 与 `5.15` `cdh_redirect.ko` 编译全绿）

---

## 1. 问题

在完成 [../issue-honor-x80i-prepare-profile-cust-and-battery-sysfs/](../issue-honor-x80i-prepare-profile-cust-and-battery-sysfs/)（#341）后，在线实例 **`Honor X80i 5G #1`**（`honor/HNLNA-M`，GRF 跨代 Android 16 `system` + Android 13 `vendor`，`ro.board.platform=mt6835`）暴露两项核心多媒体应用不可用问题：

1. **抖音（`com.ss.android.ugc.aweme`）打开黑屏、偶发冷启动闪退、视频渲染卡顿/崩溃**（见 [dy-fail.png](dy-fail.png)）：
   - 冷启动时黑屏数秒或直接因 `gralloc-mapper is missing` 崩溃（同症状亦出现在荣耀相机 `com.hihonor.camera` 与负一屏 `com.hihonor.hiboard`）；
   - 进入视频流后每 5 秒 `RenderThread` 出现 ~4950ms 掉帧卡顿，YUV 视频帧导入报 `EGL-MAIN: failed to create DRI image from FD` 并触发 `AutoBackendTextureRelease.cpp: Invalid GrBackendTexture. Width==576, height==1024, protected==0` 崩溃；
   - 伴随 Android 16 HWUI `ShaderCache::load` 分配 60KB `BLOB (61440x1)` 触发 `2D Texture size components (61440, 1) exceeds supported value (16384)` 失败。
2. **荣耀原生相机（`com.hihonor.camera`）打开黑屏并每 5 秒循环重启闪退，无法出图预览**（见 [cam-fail.png](cam-fail.png)）：
   - 解决 `gralloc-mapper` 后相机虽能进入 `CameraActivity` 并连接 `CameraDevice 0`，但取景框全黑、`Device status: UNCONFIGURED`，恰好 5 秒后退出并自动拉起新进程循环重试，最终闪退回桌面。

---

## 2. 根因

### 2.1 抖音黑屏与图形异常的三重叠加根因

1. **`cdh_redirect.ko` 遗漏板级子目录 `hw/<board>/*` SP-HAL 分类与 `/vendor/lib(64)/` 框架库白名单**：
   - `Honor X80i 5G` 为 Android 13 `vendor`（`ANDROID_API_LEVEL=33`），运行 `allocator-V1` 且无 AIMapper V5，因此 Android 16 `/system/lib64/libui.so` 在所有进程中均回落至 HIDL `Gralloc4Mapper`（`IMapper@4.0` passthrough）；
   - 原厂 `/vendor/lib64/hw/android.hardware.graphics.mapper@4.0-impl-mediatek.so` 是指向板级子目录 **`mt6835/android.hardware.graphics.mapper@4.0-impl-mediatek.so`** 的相对软链接；
   - `cdh_redirect/core/rules.c` 中 `cdh_classify_sphal_lib()` 此前仅匹配紧贴 `hw/` 顶层的 `hw/mapper.*` / `hw/android.hardware.graphics.mapper@*` / `hw/gralloc.*`，遇到单层板级子目录 `hw/mt6835/...` 直接返回 `CDH_SPHAL_NONE`；同时 `cdh_path_is_gfx_framework()` 未放行 `/vendor/lib(64)/` 下的 `libhidlbase.so` / `android.hardware.graphics.mapper@4.0.so`，导致普通 App（`uid > 10000`）调 `opendir("/vendor/lib64/hw")` 触发 `sphal_blocked`（实测达 `1048` 次）落回原厂 `dm-6`（`fd:06`）；
   - 普通 App 穿透加载了原厂 ARM Mali `/vendor/lib64/hw/mt6835/android.hardware.graphics.mapper@4.0-impl-mediatek.so`（`fd:06`）：
     - 路径 1：当 `opendir` 读到原厂名 `android.hardware.graphics.mapper@4.0-impl-mediatek.so` 而 `linker64` 被重定向至载体 `/mnt/vendorpatch/vendor/lib64/hw/` 时，因载体缺少 `-mediatek` 后缀软链接返回 `ENOENT`，触发 `LOG_ALWAYS_FATAL("gralloc-mapper is missing")`；
     - 路径 2：当直接穿透至 `fd:06` 加载原厂 Mali Mapper 时，`libui` 的 `importBuffer` 从未调用 CDH `mapper.mediatek.so`（`fe:20`）中的 `cros_gralloc_driver::retain()`，导致 Mesa `libEGL_mesa.so` 调 `gralloc0_perform(GRALLOC_DRM_GET_BUFFER_INFO)` 返回 `-EINVAL`；单平面 RGB 走回退勉强可画，所有多平面 YUV 视频纹理全部报 `failed to create DRI image from FD` → 黑屏 / `Invalid GrBackendTexture` 崩溃，同时原厂 Mali Mapper 每 5 秒打印 `@get: Buffer ... has not been registered with Gralloc` 并展开全栈卡死渲染线程。
2. **`09_build_vendor` 未生成厂商后缀 `mapper@4.0-impl-<suffix>.so` 及板级子目录 `lib(64)/hw/<board>/` 遮蔽软链接**：
   - `downgradeLegacyNativeMapperVintf` 仅在顶层生成 `android.hardware.graphics.mapper@4.0-impl.so -> mapper.mediatek.so`，未同步遮蔽顶层 `android.hardware.graphics.mapper@4.0-impl-mediatek.so` 与板级子目录 `lib(64)/hw/mt6835/{mapper,gralloc,vulkan,mapper@4.0-impl*}`。
3. **`CdhAllocator.h` 将 4 级兜底全写在 `#if ANDROID_API_LEVEL >= 34` 的 `allocate2()` 中，Android 13 V1 `allocate()` 为空壳**：
   - `allocator-V1` 仅走 `allocate(const std::vector<uint8_t>&, ...)`，缺失剥离 `GPU_DATA_BUFFER`/`CAMERA_*`/`VIDEO_*` 与 `RAW10`/`NV21` 兜底，导致 Android 16 HWUI `ShaderCache::load` 的 60KB `BLOB` 申请直触 VirGL `16384` 2D 纹理尺寸上限失败。

### 2.2 荣耀相机未打通根因

- 荣耀 MagicOS 原生相机 `com.hihonor.camera` 启动打开 `CameraDevice 0` 后，会通过 `ServiceHostGlobalSession` 连接系统守护进程 **`/system/bin/HwServiceHost`** 并触发 `CPlatformAdapter::jpegOpenClient()`；
- 反汇编 `/system/bin/HwServiceHost`（`CPlatformAdapter::getImpl()` `0x23668` 与构造函数 `0x23954..0x239dc`）证实：它首先查询 AIDL 服务 **`vendor.honor.hardware.camera.camResource.IHwCameraResourceService/default`**；
- 因 `android.hardware.camera@honor` 变体尚未注册该 AIDL 服务，`AServiceManager_checkService` 返回空指针，`HwServiceHost` 退回 `HidlCPlatformAdapterImpl::InitHidlIntf()` 阻塞等待原厂 HIDL `camResource@1.0/1.1` 超时 5 秒后触发 `LOG_ALWAYS_FATAL("failed to get hidl service")` 崩溃；
- `com.hihonor.camera` 监听到 `ServiceHostGlobalSession: servicehost died! -> ServiceHost death error` 后主动退出重启，陷入 5 秒循环，始终未执行到 `configureStreams()` 开启预览流。

---

## 3. 证据

- **修复前失败现场**（详见 [log-fail.txt](log-fail.txt)、[dy-fail.png](dy-fail.png)、[cam-fail.png](cam-fail.png)）：
  - 抖音 `/proc/5521/maps` 同时存在 `fd:06 8905112 /vendor/lib64/hw/mt6835/android.hardware.graphics.mapper@4.0-impl-mediatek.so` 与 `fe:20 1701 /vendor/lib64/hw/mapper.mediatek.so`；
  - `logcat` 捕获 `arm::mapper::common::get`、`@get: Buffer ... has not been registered with Gralloc`、`W EGL-MAIN: failed to create DRI image from FD`、`Invalid GrBackendTexture. Width==576, height==1024, protected==0`、`gralloc-mapper is missing` 与 `ShaderCache::load value buffer allocate failed`；`/proc/cdh_redirect` 显示 `sphal_blocked: 1048`；
  - 荣耀相机 `dumpsys media.camera` 显示 `Device status: UNCONFIGURED` 且每 5 秒 `DIED client(s) with PID 823`；`logcat` 捕获 `HwServiceHost` `Abort message: 'failed to get hidl service'`（`HidlCPlatformAdapterImpl::InitHidlIntf()+1332`）紧接 `ServiceHostGlobalSession: servicehost died!`。
- **修复后通过现场**（详见 [log-pass.txt](log-pass.txt)、[dy-pass.png](dy-pass.png)、[cam-pass.png](cam-pass.png)）：
  - 抖音（`PID 12386`/`14683`）与荣耀相机（`PID 11782`）进程 `/proc/<pid>/maps` 中原厂 `fd:06` `mt6835/mapper@4.0-impl-mediatek.so` **100% 清零**，仅加载唯一一份 `fe:20 1701 /vendor/lib64/hw/mapper.mediatek.so` 与 `fe:20 1696 /vendor/lib64/hw/gralloc.common.so`；
  - 抖音冷启动即流畅播放视频（[dy-pass.png](dy-pass.png)），`gralloc-mapper is missing`、`has not been registered with Gralloc`、`failed to create DRI image`、`Invalid GrBackendTexture`、`ShaderCache::load` 全归 **`0`**；
  - `service check vendor.honor.hardware.camera.camResource.IHwCameraResourceService/default` 返回 `found`；`HwServiceHost`（原始 `PID 830`）全程零崩溃；荣耀相机稳定配置 2 路输出流（`Stream 0/1`、`Latest received frame: 65/65 entries`、`framesSent` 持续递增），取景框实时输出虚拟相机彩条与动态绿块（[cam-pass.png](cam-pass.png)），拍照与切镜均无闪退。

---

## 4. 改动

1. **`cdh_redirect` 内核模块补齐板级子目录 SP-HAL 分类与 `/vendor/lib(64)/` 框架库白名单**：
   - 更新 `devices/modules/cdh_redirect/core/rules.c`：新增 `cdh_extract_hw_leaf()` 支持单层板级子目录 `hw/<board>/<leaf>`（如 `hw/mt6835/android.hardware.graphics.mapper@4.0-impl-mediatek.so`、`hw/mt6835/gralloc.common.so`、`hw/mt6835/vulkan.mali.so`）归入 SP-HAL 重定向；在 `cdh_path_is_gfx_framework()` 中增补 `/vendor/lib(64)/` 前缀与 `libhidltransport.so` / `libnativedisplay.so`，消除 `libhidlbase.so` / `mapper@4.0.so` 调 `opendir("/vendor/lib64/hw")` 的门控误伤；
   - 重编发布 `out/devices/kernel/5.15/modules/cdh_redirect.ko`（64 个符号版本全部一致）。
2. **`09_build_vendor` 补齐厂商后缀与板级子目录 SP-HAL 相对软链接遮蔽**：
   - 更新 `agent/modules/pipeline/stages/09_build_vendor/files_compat.go`：新增 `collectVendorHwBoardSubDirs()`、`ensureRelativeSymlink()` 与 `shadowBoardSubdirGraphicsSphal()`；在 `downgradeLegacyNativeMapperVintf()` 中同步创建顶层 `android.hardware.graphics.mapper@4.0-impl-<suffix>.so -> mapper.<suffix>.so` 及板级子目录 `lib(64)/hw/<board>/{mapper.*,gralloc.*,vulkan.*,android.hardware.graphics.mapper@4.0-impl*.so} -> ../<target>` 相对软链接（复用同一 inode 单例），并补齐 `vendor_file_contexts` 的板级子目录 `same_process_hal_file` 规则；
   - 在同目录 `agent/modules/pipeline/stages/09_build_vendor/files_test.go` 新增单元测试 `TestBoardSubdirGraphicsMapperAndGrallocShadowSymlinks` 并重编 `out/agent/dist/pipeline`。
3. **`android.hardware.graphics.allocator` 补齐 AIDL V1 `allocate()` 4 级回退**：
   - 更新 `devices/hardware/android.hardware.graphics.allocator/src/CdhAllocator.h`：抽离 `PatchRaw16Result()` 与 `PatchNv21Result()`，在 `CdhAllocator` 中无条件覆写 V1 `allocate(const std::vector<uint8_t>&, ...)`，通过 `android::gralloc4::{decode,encode}BufferDescriptorInfo` 应用与 `allocate2()` 完全一致的 4 级标志剥离与格式回退；
   - 重编发布 `android13` 与 `android16` `android.hardware.graphics.allocator` 产物。
4. **`android.hardware.camera@honor` 变体新增 `IHwCameraResourceService` AIDL 服务端、VINTF 与 SELinux 策略**：
   - 更新 `devices/hardware/android.hardware.camera/src/honor/camera_honor_ext.cpp`：在 `RegisterVendorExtraServices()` 中注册 `vendor.honor.hardware.camera.camResource.IHwCameraResourceService/default`（VINTF 稳定性、`version=1`、`hash=cff0830a2ff3f8186c2846e0231042d30445f481`，完整实现 15 个事务码与 `HwCamResouceLongType` Parcel 布局）；
   - 新增变体静态配置 `devices/hardware/android.hardware.camera/variants/honor/vendor/etc/vintf/manifest/vendor.honor.hardware.camera.camResource.xml` 与条件 SELinux 规则 `devices/hardware/android.hardware.camera/variants/honor/sepolicy/{vendor_service_contexts.rules,vendor_sepolicy_rules.cil}`；
   - 重编发布 `android13` 与 `android15` `android.hardware.camera@honor` 产物，并经客机 `secilc` 离线校验通过后无损热更新至本地 `HNLNA-M` `dist` 盘集（严守 `AGENTS.md §8`，未触碰 `super.img`）。

---

## 5. 验证

1. **自动化单测、策略门禁与编译验证**：
   - `cd agent/modules/pipeline && go test ./stages/09_build_vendor/...` 全绿（含新增 `TestBoardSubdirGraphicsMapperAndGrallocShadowSymlinks`）；
   - `pipeline build-hardware --android {13,16} --module android.hardware.graphics.allocator --refresh` 与 `--android {13,15} --module android.hardware.camera --variant honor --refresh` 全绿；
   - `pipeline device build HNLNA-M --only build_modules` 产出 `cdh_redirect.ko` 64 符号一致；
   - 客机 `secilc -m -M true -G -N -c 30` 全量 CIL 离线预编译输出 `SECILC_OK`（零 `neverallow` 违背）；`patch.img` `e2fsck -fn` 零错。
2. **`Honor X80i 5G #1` 端到端实机验证**：
   - 抖音冷启动即流畅播放视频（[dy-pass.png](dy-pass.png)），`maps` 中 `fd:06` Mali Mapper 清零，五类图形/ShaderCache 报错全归 `0`；
   - 荣耀相机稳定进入预览界面实时输出宿主测试彩条流（[cam-pass.png](cam-pass.png)），`IHwCameraResourceService/default: found`，`HwServiceHost` 与 `com.hihonor.camera` 全程零崩溃，点击快门与切镜稳定。

---

## 6. 遗留

- `cdh_redirect.ko` 本次已重编发布 `5.15` 内核线，其余内核线（`5.10`/`6.1`/`6.6`/`6.12`）在下次执行 `build_modules` 时将自动生效；
- 压测期间 `dmesg` 偶见 `cdh_redirect: sphal trylock timeout comm=RenderThread tier=3`（高并发加载期主线程持有 `mmap_lock` 写锁超过 500µs 触发 `mmap_read_trylock` 超时），当前虽未造成功能回退，但后续可在 `cdh_redirect` 中将 `cdh_linker_cache` 扩展至 `RenderThread` 首次命中前或适当放宽重试等待上限以进一步消除偶发 `trylock timeout`。
