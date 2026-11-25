# #330 SwiftShader (`pastel`) `VK_KHR_external_fence_fd` 桥接修复澎湃 OS 相册闪退，同步收敛 HWC3 VSync/`getLuts`、`ITouchFeature` 轮询与高通 `IWlanService` 连崩

- **设备/机型**：跨机型通用（首发验证：`xiaomi/popsicle` Xiaomi 17 Pro Max，`16-OS3.0.319.0.WPBCNXM`，实例 `#1`）
- **涉及模块**：
  - `devices/hardware/android.hardware.gpu`（`pastel` 变体）
  - `devices/hardware/android.hardware.graphics.composer3` + `stages/08_build_hardware/modules/android16/android.hardware.graphics.composer3.json`
  - `devices/hardware/android.compat.libs`（`libWlanServiceJni.so`）
  - `agent/modules/pipeline/stages/02_prepare`、`agent/modules/pipeline/stages/09_build_vendor`
  - `devices/manifest/configs/xiaomi/popsicle/popsicle-android16/`

---

## 一、问题

在 `Xiaomi 17 Pro Max #1`（`popsicle_16-OS3.0.319.0.WPBCNXM`，HyperOS 3 / Android 16）运行巡检与交互中存在 4 项问题：

1. **系统相册（`com.miui.gallery`）点击即闪退**：启动 `com.miui.gallery/.activity.HomePageActivity` 后毫秒级主动 `finish`，无法进入相册主界面；同时伴随 `W libc: Access denied finding property "ro.vendor.cdh.vulkan.keep_msaa"` 9 连刷。
2. **`SurfaceFlinger` 1Hz 阻塞轮询 `ITouchFeature/default`**：`init` 与 `logcat` 持续每秒刷屏 `Control message: Could not find 'aidl/vendor.xiaomi.hw.touchfeature.ITouchFeature/default' for ctl.interface_start from pid: 250 (/system/bin/servicemanager)`（1h45m 累计 `qemu.log` 20,063 条、`logcat` 25,187 条）。
3. **HWC3 `getLuts` 报错刷屏、VSync 时间戳超前告警，且 Android 16 未接入自研 `composer3`**：
   - `E HwcComposer: getLuts failed Status(-8, EX_SERVICE_SPECIFIC): '8: '` 每帧刷屏（累计 1,096 条）；
   - `W Choreographer: Frame time is 0.1~0.2 ms in the future! Check that graphics HAL is generating vsync timestamps using the correct timebase.` 告警（17 次）；
   - Android 16 缺少 `modules/android16/android.hardware.graphics.composer3.json`，`patch.img` 仍运行底包原生 `hwc3-service.drm_upstream`，导致 `ro.vendor.cdh.display.*`（120Hz 多档刷新率、`CSO/CSOT_69_1P5K` EDID、`416.2×415.8` DPI）未生效。
4. **`.qtidataservices`（`vendor.qti.iwlan`）开机连续 `UnsatisfiedLinkError` 崩溃及高频 SELinux `avc: denied` 刷屏**：
   - `/vendor/app/IWlanService/IWlanService.apk` 被 Telephony 绑定后反复因 `No implementation found for void vendor.qti.iwlan.JniHelper.getAllQualifiedNetworks(int, int)` 崩溃重启并频繁持有 `IWLAN_PROXY_WL` 唤醒锁；
   - `hal_mtdservice_service`（`qemu.log` 20,022 条）、`cameraserver fowner`（299 条）、`vendor_dpmtcm_socket`（111 条）等缺失硬件私有节点产生大量 SELinux 审计刷屏。

---

## 二、根因

1. **相册闪退根因**：
   - 澎湃 OS 3 的 `MIUIGallery.apk` 基于 `hyos_spawner` Rust + Flutter 原生引擎（`libapp_gallery.so` + `libhyper_os_flutter.so`），硬编码 `--enable-impeller --impeller-backend=vulkan`。
   - 反汇编 `libhyper_os_flutter.so`（`CapabilitiesVK::GetEnabledDeviceExtensions`）确认：SwiftShader（`vulkan.pastel.so`）已满足图形/计算/传输队列位检查，但仅实现了 `VK_KHR_external_fence`，**未实现 Android Impeller 硬性要求的 `VK_KHR_external_fence_fd`（`SYNC_FD` 语义）**，导致 `capabilities_vk.cc(285)` 判定设备不兼容 → `context_vk.cc(292) No valid Vulkan device found.` → `flutter_scene:290 Create engine failed, just finish`。
   - 此外 `cdh_vulkan_pastel.c` 原先在每次查询物理设备属性时调用 `cdh_vk_cfg_get_u32("ro.vendor.cdh.vulkan.keep_msaa")`；当 `gpu_id.cfg` 未配置该可选开关时回退 `__system_property_get`，普通 App 因 SELinux 无权读 `vendor_default_prop` 触发 Bionic 告警。
2. **`ITouchFeature/default` 1Hz 轮询根因**：
   - `/product/etc/vintf/touch_framework_compatibility_matrix.xml` 声明了 `vendor.xiaomi.hw.touchfeature.ITouchFeature/default`（AIDL），且 `/vendor/etc/selinux/vendor_service_contexts` 已映射为 `hal_touchfeature_xiaomi_service`，但 `cdh_nn_placeholders.txt` 与 `02_prepare` 自动推导列表此前仅覆盖 `vendor.xiaomi.hardware.*` 前缀而遗漏了 `vendor.xiaomi.hw.touchfeature.*`，导致 `servicemanager` 按 lazy HAL 反复请求 `init` 拉起不存在的服务。
3. **HWC3 告警与 Android 16 未生效根因**：
   - 上游 `drm_hwcomposer` 的 `VSyncWorker` 按锚点外推下一拍 VSync 时间戳时偶发超出当前 `CLOCK_MONOTONIC` 0.1~0.2ms，触发 Android 16 `Choreographer` 未来帧时间检查；
   - HyperOS `SurfaceFlinger` 定期调用 HWC3 新增接口 `IComposerClient::getLuts`，上游 `DrmHwcThree::getLuts` 返回 `EX_UNSUPPORTED (8)` 被 `HwcComposer` 打成 `E` 级错误；
   - AOSP r36（Android 16）`external/drm_hwcomposer` 移除了 `hwc3/ComposerResources.cpp`（改用 `utils/LibdisplayEdidWrapper.cpp`）并将 `ui/ColorSpace.h` 拆入 `libs/ui/include_types`，且服务二进制名为 `android.hardware.composer.hwc3-service.drm_upstream`，原模块配置仅有 Android 14/15 规格；同时 `hal_graphics_composer_default` 缺少 `vendor_default_prop` 读取授权。
4. **`.qtidataservices` 崩溃与高频 AVC 根因**：
   - 官方 `/vendor/app/IWlanService/IWlanService.apk`（`sharedUserId="com.qualcomm.qti.qtidataservices"`，分配普通应用 `uid=10116`）仅包含 `.IWlanDataService`、`.IWlanNetworkService`、`.QualifiedNetworksServiceImpl` 三个 AP-assisted IWlan 服务；因未设置 `ro.telephony.iwlan_operation_mode=legacy`，Telephony 默认按 AP-assisted 模式拉起该进程；而 `uid > 10000` 不走非 SP-HAL `/vendor/lib64` 重定向，落回官方 `dm-5` 的 `libWlanServiceJni.so`（`vendor_file`）被 `platform_app` SELinux 拦截抛出 `UnsatisfiedLinkError`。
   - `hal_mtdservice_service`、`cameraserver` `fowner`、`vendor_dpmtcm_socket` 等属于云机无实体芯片的探测噪音，缺少对应 `allow` / `dontaudit` 收口。

---

## 三、证据

- 修复前日志见同目录 [log-fail.txt](log-fail.txt)：
  - `capabilities_vk.cc(285): Device does not support required Android extension: VK_KHR_external_fence_fd` → `hyper_gallery_app: Create engine failed, just finish`；
  - `init: Control message: Could not find 'aidl/vendor.xiaomi.hw.touchfeature.ITouchFeature/default' for ctl.interface_start`（25,187 条）；
  - `HwcComposer: getLuts failed Status(-8, EX_SERVICE_SPECIFIC): '8: '`（1,096 条）与 `Choreographer: Frame time is ... ms in the future!`（17 条）；
  - `.qtidataservices` `java.lang.UnsatisfiedLinkError: No implementation found for void vendor.qti.iwlan.JniHelper.getAllQualifiedNetworks(int, int)`；
  - `hal_mtdservice_service`（20,022 条）、`fowner`（299 条）、`vendor_dpmtcm_socket`（111 条）`avc: denied`。
- 修复后日志与实机截图见同目录 [log-pass.txt](log-pass.txt) 与 [gallery-pass.png](gallery-pass.png)。

---

## 四、改动

1. **`android.hardware.gpu`（`pastel` 变体）补齐 `VK_KHR_external_fence_fd` 与配置缓存**：
   - 新增 `devices/hardware/android.hardware.gpu/src/pastel/cdh_vk_pastel_fence.h` 与宿主机单测 `cdh_vk_pastel_fence_test.c`（接入 `halbuild/gpu_test.go`）；
   - 在 `devices/hardware/android.hardware.gpu/src/pastel/cdh_vulkan_pastel.c` 拦截 `vkEnumerateDeviceExtensionProperties`（追加 `VK_KHR_external_fence_fd` v1）、`vkGetPhysicalDeviceExternalFenceProperties{,KHR}`（宣告 `SYNC_FD` 可导入/导出）、`vkCreateDevice`（剥离 `VK_KHR_external_fence_fd` 并确保开启 `VK_KHR_external_fence`）、`vkCreateFence`（剥离 `VkExportFenceCreateInfo` 的 `SYNC_FD` 位避免 SwiftShader 断言）、`vkDestroyFence` / `vkResetFences` / `vkGetFenceStatus` / `vkWaitForFences` / `vkQueueSubmit{,2,2KHR}`，并在 `vkGetInstanceProcAddr` / `vkGetDeviceProcAddr` 完整导出 `vkGetFenceFdKHR`（已触发 fence 返回 `-1` 合法语义）与 `vkImportFenceFdKHR`（`poll(fd)` 同步等待并标记 fence 已触发）；
   - 新增 `cdh_cfg_only_u32` 仅在 `cdh_vk_pastel_on_open` 从 `gpu_id.cfg` 读取一次 `ro.vendor.cdh.vulkan.keep_msaa`，不再向普通 App 发起 `__system_property_get`。
2. **`ITouchFeature/default` 占位与 `02_prepare` 自动识别**：
   - 在 `devices/manifest/configs/xiaomi/popsicle/popsicle-android16/vendor/etc/cdh_nn_placeholders.txt` 增加 `vendor.xiaomi.hw.touchfeature.ITouchFeature/default`；
   - 在 `agent/modules/pipeline/stages/02_prepare/firmware_measured_extra.go` 自动扫描 `vendor_service_contexts` 识别 `vendor.xiaomi.hw.touchfeature.ITouchFeature/default` 并配单测 `touchfeature_placeholder_test.go`。
3. **HWC3 `CdhComposer` 修复与 Android 16 `composer3` 模块接入**：
   - 在 `devices/hardware/android.hardware.graphics.composer3/src/CdhComposer.cpp`：
     - `CdhComposerCallback::onVsync` 将 `timestamp` 钳制为 `<= NowMonotonicNs()`；
     - `CdhComposerClient::getLuts` 将底层 `EX_UNSUPPORTED` 转换为空 `out_luts` + `ScopedAStatus::ok()`；
   - 在 `modules/android.hardware.graphics.composer3.json` 补充 `-DANDROID`（修复 `ALOGI` 落入 `printf` 导致日志丢失），新增 `modules/android16/android.hardware.graphics.composer3.json`（产出 `bin/hw/android.hardware.composer.hwc3-service.drm_upstream`），新增模块级 `devices/hardware/android.hardware.graphics.composer3/sepolicy/vendor_sepolicy.cil` 授权 `hal_graphics_composer_default` 读取 `vendor_default_prop_{{vers}}`，并在 `popsicle-android16/hardware/config.json` 声明 `android.hardware.graphics.composer3`。
4. **高通 `IWlanService` 收口与高频 SELinux 策略收敛**：
   - 在 `devices/hardware/android.compat.libs/src/wlan_service_jni.c` 新增纯 C 零 QMI 依赖的 `libWlanServiceJni.so`；在 `09_build_vendor/files_compat.go`（`backfillWlanServiceJni` + `ensureIwlanLegacyOperationModeProp`）自动回填 `libWlanServiceJni.so` 并写入 `ro.telephony.iwlan_operation_mode=legacy`（同步写入 `popsicle-android16/vendor/config.json`），阻止无 QMI 基带时绑定 `vendor.qti.iwlan`；
   - 在 `popsicle-android16/vendor/sepolicy/vendor_sepolicy.cil` 补齐 `hal_touchfeature_xiaomi_service`、`vendor_hal_displayfeature_xiaomi_service`、`sysfs_thermal` 授权及 `hal_mtdservice_service`、`cameraserver fowner`、`vendor_dpmtcm_socket_{{vers}}` 等 `dontaudit` 规则。

---

## 五、验证

1. **单测与交叉编译验证**：
   - `go test ./internal/halcore/halbuild ./internal/halcore/spec ./stages/02_prepare ./stages/08_build_hardware ./stages/09_build_vendor` 全部 PASS（含 `cdh_vk_pastel_fence_test.c` 宿主机 C 单测、`touchfeature_placeholder_test.go`、`wlan_jni_compat_test.go`）；
   - Android 15 / Android 16 `gpu@{default,pastel}`、`composer3`、`compat` 交叉编译零错误通过。
2. **`Xiaomi 17 Pro Max #1`（`127.0.0.1:5927`）更新 `patch.img` 冷启实测（32s `sys.boot_completed=1`）**：
   - **相册（`com.miui.gallery`）**：冷启动 `1942ms` 直达前台 `mCurrentFocus=Window{... u0 com.miui.gallery/com.miui.gallery.activity.HomePageActivity}`（及 `MainActivity#374` 4 块 `1200×2608` BLAST 帧缓冲），Impeller Vulkan 正常解码渲染缩略图（见 [gallery-pass.png](gallery-pass.png)），`VK_KHR_external_fence_fd` 报错 **`0`**，`keep_msaa` 属性告警 **`0`**；
   - **`ITouchFeature/default`**：`service check vendor.xiaomi.hw.touchfeature.ITouchFeature/default` 返回 **`found`**，`logcat` 中 `ctl.interface_start` 告警由 **25,187 → 0**；
   - **HWC3 显示与同步**：`dumpsys display` 确认 `DeviceProductInfo{name=CSOT_69_1P5K, manufacturerPnpId=CSO, productId=50231}`、`416.2 x 415.8 dpi`、`supportedRefreshRates [30, 60, 90, 120Hz]`、`defaultModeId=4 (120.0Hz)` 全部生效；`getLuts failed` 由 **1,096 → 0**，`Choreographer ... in the future` 由 **17 → 0**；
   - **`IWlanService` 与 SELinux**：`ro.telephony.iwlan_operation_mode=legacy` 生效，`WlanServiceJni` `UnsatisfiedLinkError` 崩溃 **`0`**；`qemu.log` 中 `hal_mtdservice_service` 由 **20,022 → 0**、`vendor_dpmtcm_socket` 由 **111 → 0**、`fowner` 由 **299 → 1**。

---

## 六、遗留

- `com.miui.weather2`（同为 `hyos_spawner` 进程，但未像 `MIUIGallery` 那样预链 `libmivklumos.so`/EGL）首次在纯 Rust 栈线程 `dlopen("libvulkan.so")` 时，因 Rust `-fomit-frame-pointer` 栈帧跨度超出 `cdh_redirect.ko`（位于 `vendor_boot.img`）的 `cdh_sphal_caller_allowed` FP 回溯窗口而被记入 `sphal_blocked`；后续若重编 `cdh_redirect.ko` 升级 `vendor_boot.img`，可放宽 `/apex/com.android.runtime/bin/linker64` 直接发起 `openat` 时的 `CDH_SPHAL_TIER_B` 判定以一并放行纯 `dlopen` Vulkan 的 `hyos_spawner` 应用。
- 远端 ROM 服务器（`192.168.31.164`）下次全量重打 `popsicle` 时，将由 `cluster.SyncLocalManifestToNode` 自动同步本机 `devices/manifest/` 最新配置并生成同构 `patch.img`。
