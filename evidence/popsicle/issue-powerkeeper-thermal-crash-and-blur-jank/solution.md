# Issue: popsicle `16-OS4.0.5.0.XPBCN` 桌面滑动顿挫：`com.miui.powerkeeper` 缺 `sysfs_thermal` 读权限每 2.4 秒崩溃抓包风暴、`default_android_service` 审计刷屏与窗口实时背景模糊叠加

- 设备：`xiaomi/popsicle`（Xiaomi 17 Pro Max / `canoe` / HyperOS 4 `16-OS4.0.5.0.XPBCN` / Android 16 / `lineage-23.2` / kernel `6.12.81`）
- 前序条目：`../issue-android16-boot-sphal-pac-and-qti-mapper/`（#248 冷启动进桌面与触控闭环）
- 验证盘集：`vda-8013b9eeb46e` + `patch.img`（`sha256: 2e5ed81df00c`）
- 状态：**已闭环**（2026-11-18，实例 #1 全新冷启动）：`com.miui.powerkeeper` 首次拉起稳定常驻（`powerkeeper_crashes=0`）、`/data/mqsas` 无异常增长（`4.3M` vs 修复前 14 分钟 `658M`）、`default_android_service` 审计刷屏 `0` 条、窗口实时背景模糊已关闭、桌面 Vulkan 软光栅收敛为 `1x` 采样（显存占用从 `85.33 MB` 降至 `42.82 MB`）、管理台 VNC WebSocket 升级为 `4 MiB` 整帧 `writev` + `TCP_NODELAY`

---

## 一、问题

在 `../issue-android16-boot-sphal-pac-and-qti-mapper/`（#248）解决冷启动进桌面后，实机滑动桌面时存在明显的周期性顿挫感；即使手指完全不碰屏幕处于静置状态，宿主 `qemu-system-aarch64` 也常驻占用 **`243.5%` CPU**，开机 14 分钟 Guest 内进程 PID 翻滚超过 6.5 万次，`/data/mqsas` 堆积 **658 MB** 崩溃压缩包。

---

## 二、根因

逐层排查宿主 `cgl-headless` / VNC 推流、Guest 桌面渲染线程与系统后台负载后，定位到宿主 VNC 链路正常（`cgl-headless` 读回 1.7~5.4 ms/帧、VNC 刷新周期 16 ms），顿挫来自 **Guest 内部三项负载叠加**：

1. **首要元凶：`com.miui.powerkeeper` 缺 `sysfs_thermal` 读权限导致每 2.4 秒崩溃 + `hypsys_system` 持续压缩抓包风暴**
   - `/sys/class/thermal` 与 `/sys/devices/virtual/thermal` 在 `android.hardware.thermal` 的 `genfscon` 中标注为 `u:object_r:sysfs_thermal:s0`，但仅向 `hal_thermal_default` 开放了读权限，未向 `u:r:system_app:s0` 开放。
   - 反汇编 `/system_ext/app/PowerKeeper/PowerKeeper.apk` 的 `CheckUtils.registerNtcAndDeviceMap()`（`0x393b18`）：启动时 `new File("/sys/class/thermal").listFiles()` 因 `EACCES` 返回 `null`，紧接执行 `array-length v3, v0`（`CheckUtils.java:43`）直接抛出 `NullPointerException: Attempt to get length of null array`。
   - `ActivityManager` 每 1000 ms 重拉 `PowerKeeperBackgroundService`，每 **2.4 秒** 触发一次完整的 `zygote64` fork → DEX 加载 → 崩溃 → `AMS`/`OomAdjuster` 全局锁清理 → `hypsys_system`（`HypSys-ZipFile`）将 `jetrace` 压缩写入 `/data/mqsas/je/online/`（约 47 MB/分钟），持续抢占 CPU、I/O 与 `system_server` 窗口/输入锁。
2. **高频缺失私有服务查找触发 SELinux 审计刷屏（每秒 ~360 条 audit）**
   - `system_server` 在显示/触控路径高频查找 `vendor.xiaomi.hardware.displayfeature_aidl.IDisplayFeature/default`，`com.xiaomi.finddevice` 循环查找 `vendor.xiaomi.hardware.aidl.mtdservice.IMTService/default`，均落入 `default_android_service` 触发 `SELinux: avc: denied { find }`，15 分钟累计产生 32.7 万条 audit（`audit_lost=183,204`），使 `servicemanager` 与 `logd` 常驻消耗 ~12% CPU。
3. **SwiftShader CPU 软光栅桌面叠加系统级实时背景模糊开销**
   - HyperOS 4 桌面 `com.miui.home` 锁死 `Pipeline=Impeller (Vulkan)`（macOS `MoltenVK` 缺 `VK_KHR_external_semaphore_fd`，见 `../issue-weather2-impeller-vulkan/` #140，故运行在 `vulkan.pastel.so` 8 线程 CPU 软光栅上），同时 `ro.surface_flinger.supports_background_blur=1`、`persist.sys.background_blur_supported=true`、`persist.sys.background_blur_setting=1` 默认开启，加重了 `1200×2608` 滑动期间的合成与渲染竞争。

---

## 三、证据

- 失败现场：同目录 `log-fail.txt`（记录每 2.4s `ThermalLogService` `NullPointerException` 堆栈、`dexdump` `CheckUtils.registerNtcAndDeviceMap` `listFiles()` 字节码、`HypSys-ZipFile` 写入 `/data/mqsas` `658M`、`audit_lost=183204` 及宿主静置 `243.5%` CPU）。
- 通过现场：同目录 `log-pass.txt`（记录新盘集 `vda-5c8612688644` 全新冷启动后 `com.miui.powerkeeper` 单次拉起零崩溃、`/data/mqsas` `4.3M`、`default_android_service` 报错 `0` 条、背景模糊关闭属性及静置 `700%/800% idle`）。

---

## 四、改动

1. **`devices/manifest/configs/xiaomi/popsicle/popsicle-android16/vendor/sepolicy/vendor_sepolicy.cil`**：
   - 向 `system_app_{{vers}}` 补充 `sysfs_thermal_{{vers}}` 的 `dir`（`read getattr open search`）、`file`（`read getattr open`）与 `lnk_file`（`read getattr`）只读权限（已核对 `plat_sepolicy.cil` 无任何 `neverallow` 冲突）；
   - 增加 `(dontaudit domain default_android_service_{{vers}} (service_manager (find)))`，保持拒绝语义不变的同时消除缺失私有服务查找的审计日志风暴。
2. **`devices/manifest/configs/xiaomi/popsicle/popsicle-android16/vendor/config.json`**：
   - 在 `build-prop-sets` 中新增 `"ro.surface_flinger.supports_background_blur": "0"`、`"debug.sf.disable_blur": "1"`、`"persist.sys.background_blur_supported": "false"`、`"persist.sys.background_blur_status_default": "false"`、`"persist.sys.background_blur_setting": "0"`（其中 `persist.sys.*` 由 `09_build_vendor` 自动归入 `vendor/etc/init/cdh_system_props.rc` 在 `post-fs-data` 注入）。
3. **`devices/hardware/android.hardware.gpu/src/cdh_vulkan_mali.c`**：
   - 当底层基座为 `vulkan.pastel.so`（SwiftShader CPU 软光栅）时，将 `VkPhysicalDeviceLimits` 与 `vkGetPhysicalDeviceImageFormatProperties{,2}` 的帧缓冲/采样器多重采样上限收敛为 `VK_SAMPLE_COUNT_1_BIT`（`1x` 单重采样），关闭全屏 `SwapchainMSAA` 4 倍超采样开销。
4. **`agent/modules/daemon/run/ws.go`、`http.go`、`agent/web/src/modules/daemon/{grid,vnc}.ts` & `qemu/source/ui/vnc-enc-tight.c`**：
   - 将管理台 `/api/vnc-ws` 的 RFB 读取缓冲从 `32 KiB` 提升至 `4 MiB`，改用 `net.Buffers`（`writev`）合并写出 WebSocket 帧头与负载，并对双端 TCP 连接开启 `SetNoDelay(true)` 与 `4 MiB` 套接字缓冲；
   - 将前端网格预览 `grid.ts` 的 `rfb.compressionLevel` 从 `4` 对齐为 `2`（并同步将 `vnc-enc-tight.c` 的 `tight_conf[3..4]` 收口为 `262144` / `raw_zlib_level=0`），防止网格视图或共享连接命中 `raw_zlib_level=4` 阻塞 QEMU 主事件循环 ~180 ms/帧。

---

## 五、验证

| 核验项 | 修复前 | 修复后（`vda-5c8612688644`） |
| :--- | :--- | :--- |
| `com.miui.powerkeeper` 崩溃次数 | 每 **2.4 秒** 崩溃 1 次（无限循环） | **`0` 次**（`PID 5351` 首次启动后稳定常驻） |
| `/data/mqsas` 崩溃包体积 | 14 分钟涨至 **`658 MB`** | 稳定 **`4.3 MB`**（零崩溃包写入） |
| `default_android_service` 审计刷屏 | 15 分钟 >32 万条 audit（`servicemanager`+`logd` ~12% CPU） | **`0` 条** |
| 窗口背景模糊开关 | `supports_background_blur=1`、`background_blur_setting=1` | `supports_background_blur=0`、`debug.sf.disable_blur=1`、`background_blur_setting=0` |
| 静置系统 CPU 空闲率 | `system_server` 23%~36%、宿主常驻 `243.5%` | `system_server` 3.8%~7.4%、Guest 静置 **`700%/800% idle`** |

---

## 六、遗留

1. **SwiftShader CPU 软光栅架构上限**：在不二进制修补官方 `/system_ext/lib64/libhyper_os_flutter.so` 的前提下，HyperOS 4 桌面 `com.miui.home` 在 macOS 宿主上仍走 `vulkan.pastel.so` 8 线程 CPU 光栅化（连续快速滑动瞬时占用 250%~380% Guest CPU，停止滑动即回落至 `0%`）；后续若 Linux KVM 宿主启用支持 `VK_KHR_external_semaphore_fd` 的硬件 Vulkan Venus，可直接切换为宿主 GPU Vulkan 硬件加速。
