# ss9380 根治 Chromium / 三星浏览器误走 Vulkan Venus 自旋闪退并清零 ISehHyPer 与 Wi-Fi 心跳异常日志

- 设备：`samsung/pa3q`（Samsung Galaxy S25 Ultra 国行 `SM-S9380`，OneUI Android 16，基线 `16-AP3A.240905.015.A2-20260812` / `S9380ZCSDCZH1`）
- 日期：2026-11-26
- 最终盘集：`patch.img` sha256 `930ab15005ec9259996baf1ca6b89785b7748d9696c9150cfc45895abc8ffdc4`（`vendor.img` `c326a868c130a82e5f646c906b47053a0f493c69d0084761fe1ccdc10570486c`；官方 `super.img` 与 CSC 分区 100% 零改动原样直通）
- 结果：成对清空可写层冷启动 101s 达 `sys.boot_completed=1`，`Enforcing`；`/vendor/lib*/hw/` 下已无任何 `vulkan.*.so`，三星浏览器（`com.sec.android.app.sbrowser`）主进程、`privileged_process0` GPU 进程与双 `sandboxed_process0` 渲染进程全部走 OpenGL ES（`libEGL_meow.so -> libGLESv2_mesa.so`）稳定渲染网页，`Timed out waiting for GPU channel` 闪退归 **0**；三星负一屏（`spage`）与生活助手（`sreminder`）的 `Chrome_InProcGp` 100% 单核死循环自旋归 **0**（`spage` 常驻内存由 `748 MiB` 降至 `237 MiB`，整机 8 核稳态空闲达 `760%~781% / 800%`）；`ISehHyPer/default` SELinux 拒绝、`HYPER_NATIVE_INTF` 错误（原 559 条）与 `CdhWifiHal: unknown host frame type=pong` 告警（原 80 条）全部归 **0**。

## 一、问题

在 `ss9380` 完成国行 CSC 挂载、40 个内置应用铺设与主屏幕首屏布局还原（见 `../issue-oneui-mobile-network-menu-csc-and-preload-apps/`）后，对在线实例进行全缓冲区异常日志与进程负载排查时发现三项运行态问题（见同目录 `log-fail.txt`）：

1. **三星浏览器打开约 7 秒后闪退，且负一屏 / 三星生活助手后台长期吃满 100%~200% CPU**：
   - 打开三星浏览器（`com.sec.android.app.sbrowser`）通过首启协议后，`privileged_process0` GPU 子进程卡死，主进程报 `FATAL:content/browser/gpu/browser_gpu_channel_host_factory.cc:50] Timed out waiting for GPU channel.` 崩溃退出；
   - 三星负一屏（`com.samsung.android.app.spage`）与三星生活助手（`com.samsung.android.app.sreminder`）拉起内置 WebView 后，其 `Chrome_InProcGp` 线程持续占满 **100%~104% 单核 CPU**（`spage` 虚拟地址空间膨胀至 `36 GiB`、`RES 748 MiB`），导致宿主 `qemu-system-aarch64` 空载 CPU 长期高达 `100%~266%`。
2. **启动及每次触控持续刷屏 `HYPER_NATIVE_INTF` 错误（单次会话 559 条 `E`）**：
   - `system_server` 启动及每次屏幕滑动均打印 `W ServiceManagerCppClient: Failed to get isDeclared for vendor.samsung.hardware.hyper.ISehHyPer/default: Status(-1, EX_SECURITY): 'SELinux denied for service.'` 与 `E HYPER_NATIVE_INTF: Acquire on JNI, HyPerHalVersion is NULL`。
3. **`CdhWifiHal` 每 30 秒打印未知心跳帧告警（80 条 `W`）**：
   - `android.hardware.wifi-service` 周期打印 `W CdhWifiHal: unknown host frame type=pong`。

## 二、根因

1. **Chromium / WebView 绕过 `PackageManager` 直接 `dlopen("libvulkan.so")`，命中伪装成 `Adreno 830` 的 `vulkan.mali.so -> vulkan.virtio.so`（Mesa Venus）触发 AHB 自旋死锁**：
   - 尽管 `../issue-oneui-ril-bt-keymint-moneta-wallpaper/` 已在 `vendor/config.json` 中删除了 `android.hardware.vulkan.*.xml` 特性声明并设置 `debug.hwui.renderer=skiagl`（使 Android HWUI 走 OpenGL ES），但 `09_build_vendor` 仍自动把 `android.hardware.gpu@default` 编出的 `/vendor/lib64/hw/vulkan.mali.so`（转发至基底 `vulkan.virtio.so`，对外覆写硬件身份为 `Adreno 830` / `VK_DRIVER_ID_QUALCOMM_PROPRIETARY`）打入 `vendor.img`，并将 `ro.hardware.vulkan` 归一化为 `mali`；
   - Chromium（Android System WebView 与三星浏览器内核）不检查 `PackageManager.hasSystemFeature`，而是直接加载 `libvulkan.so` 枚举物理设备；看到 `Adreno 830` 后默认开启 Vulkan 光栅化与 `VK_ANDROID_external_memory_android_hardware_buffer` 合成管线；
   - 在 macOS `cgl-headless` 渲染后端（`minigbm` 仅暴露 VirGL `CAPSET 22`）下，Mesa Venus 在外部硬件缓冲路径上陷入 100% CPU 自旋死锁（日志中 `spage` `3285:9791`、`sreminder` `9456:10772`、`sbrowser:privileged_process0` `11656:11679` 三个挂死线程恰好与 `I cdh-gpu : vulkan: 已接管，基座 vulkan.virtio.so` 的三条记录一一对应）。
2. **`neuralnetworks@samsung` 漏补 `system_server` 对 `hal_hyper_service` 的 `service_manager find` 授权**：
   - `dmesg` 捕获到 `servicemanager: selinux_check_access(u:r:system_server:s0, u:object_r:hal_hyper_service:s0, service_manager, find, ... ISehHyPer/default) -1`；此前 `variants/samsung/sepolicy/vendor_sepolicy_rules.cil` 仅放开了 `system_server` 与 `hal_neuralnetworks_default` 的 `binder call/transfer`，遗漏了 `(allow system_server hal_hyper_service (service_manager (find)))`。
3. **`wifi` HAL 宿主控制通道未消费 `pong` 应答帧**：
   - `devices/hardware/android.hardware.wifi/src/wifi_host_channel.cpp` 的 `HostChannel::handleFrame()` 处理了宿主发来的 `"ping"` 帧，但收到宿主 `net-host-service` 回复的 `{"type":"pong"}` 心跳应答时未分支返回，落入兜底 `ALOGW("unknown host frame type=%s")`。

## 三、证据

修复前实测日志（完整输出见 `log-fail.txt`）：

```text
=== [PRE-FIX] Guest CPU Top (Chrome_InProcGp 100% Spin) ===
800%cpu 104%user   0%nice   0%sys 692%idle   0%iow   4%irq   0%sirq   0%host
  TID USER         PR  NI VIRT  RES  SHR S[%CPU] %MEM     TIME+ THREAD          PROCESS
 9791 u0_a55       16  -4  36G 748M 440M R  104   6.5   4:04.46 Chrome_InProcGp com.samsung.android.app.spage

=== [PRE-FIX] cdh-gpu Vulkan Takeover & Samsung Browser GPU Channel Timeout Crash ===
11-26 11:38:01.000  3285  9791 I cdh-gpu : vulkan: 已接管，基座 vulkan.virtio.so
11-26 11:39:07.312  9456 10772 I cdh-gpu : vulkan: 已接管，基座 vulkan.virtio.so
11-26 11:40:08.114 11656 11679 I cdh-gpu : vulkan: 已接管，基座 vulkan.virtio.so
11-26 11:40:53.863 11451 11451 F chromium: FATAL:content/browser/gpu/browser_gpu_channel_host_factory.cc:50] Timed out waiting for GPU channel.
11-26 11:40:53.972 11451 11451 E chromium: ERROR:components/crash/core/app/crashpad_android.cc:203] package = [com.sec.android.app.sbrowser], versionCode = [1291503508]

=== [PRE-FIX] ISehHyPer SELinux Denial & Wi-Fi Pong Spam ===
[   64.682125] [  T384] servicemanager: selinux_check_access(u:r:system_server:s0,u:object_r:hal_hyper_service:s0,service_manager,find,AuditCallbackData(sctx,vendor.samsung.hardware.hyper.ISehHyPer/default))-1
11-26 11:36:29.009   710   710 W libbinder.ServiceManagerCppClient: Failed to get isDeclared for vendor.samsung.hardware.hyper.ISehHyPer/default: Status(-1, EX_SECURITY): 'SELinux denied for service.'
HYPER_NATIVE_INTF_count=559
pong_warn_count=80
```

## 四、改动

1. **剔除 `pa3q` 装配树中的 Vulkan ICD 库（统一回退成熟的 OpenGL ES VirGL 渲染链）**：
   - `devices/manifest/configs/samsung/pa3q/pa3q-android15/vendor/config.json`：在 `vendor.files`（`09_build_vendor` 步骤 9.5，位于步骤 9.4 HAL 产物叠加之后执行）新增删除 `lib64/hw/vulkan.mali.so`、`lib64/hw/vulkan.virtio.so`、`lib64/hw/vulkan.pastel.so`、`lib/hw/vulkan.virtio.so`、`lib/hw/vulkan.pastel.so`，不仅使 `libvulkan.so` 零报错回退 OpenGL ES，还为 `vendor.img` 缩减约 46 MiB 体积。
2. **补齐 `system_server` 查找 `ISehHyPer/default` 的 SELinux 授权**：
   - `devices/hardware/android.hardware.neuralnetworks/variants/samsung/sepolicy/vendor_sepolicy_rules.cil` 与 `devices/manifest/configs/samsung/pa3q/pa3q-android15/vendor/sepolicy/vendor_sepolicy.cil`（第十六段）：新增 `(allow system_server hal_hyper_service (service_manager (find)))` 及双向 `binder (call transfer)` 规则。
3. **静默消费宿主 Wi-Fi 通道 `pong` 心跳应答**：
   - `devices/hardware/android.hardware.wifi/src/wifi_host_channel.cpp`：在 `HostChannel::handleFrame()` 中增加 `if (type == "pong") { return; }`。

## 五、验证

1. **单元测试与镜像复核**：
   - `go test ./stages/08_build_hardware ./stages/09_build_vendor` 通过；
   - `device build -from 08_build_hardware pa3q` 重新编译 `android.hardware.wifi` 并生成 `vendor.img`（`134,615,040 B`，sha256 `c326a868c130…`，确认 `vendor/lib64/hw/*vulkan*` 为空）与最终 `patch.img`（sha256 `930ab15005ec…`）。
2. **成对清盘冷启动实机验证**（`pa3q_16-AP3A.240905.015.A2-20260812` 实例 `#1`，完整日志见 `log-pass.txt` 与截图 `pass-sbrowser.png`）：
   - **起机与安全态**：101s 达 `sys.boot_completed=1`，`getenforce=Enforcing`；注意清理时需将 `writable-base.qcow2`（含 `/metadata` FBE 密钥）与 `writable-data.qcow2`（`/data`）成对删除，单删 `writable-base.qcow2` 会因 `/data` FBE 密钥失配触发 `reboot,recovery`。
   - **三星浏览器与负一屏 / 生活助手 WebView 恢复正常**：滑入负一屏（`spage`）、启动生活助手（`sreminder`）并打开三星浏览器同意协议访问百度首页持续渲染超过 15 秒（远超原 7s 超时阈值），`sbrowser` 主进程（PID `7997`）、`privileged_process0` GPU 进程（PID `9655`）与双 `sandboxed_process0` 渲染进程全部存活并正常显示网页（`pass-sbrowser.png`）；`spage` 常驻内存由 `748 MiB` 降至 `237 MiB`，`Chrome_InProcGp` 100% CPU 自旋彻底消失，整机 8 核稳态空闲达 **`760%~781% / 800%`**。
   - **目标异常日志全部归零**：`cdh_vulkan_takeover_count=0`、`gpu_channel_timeout_count=0`、`sbrowser_crash_count=0`、`hal_hyper_avc_count=0`、`HYPER_NATIVE_INTF_err_count=0`、`pong_warn_count=0`。

修复前后关键指标对比（已排除 `adbd` 回显）：

| 核验项 | 修复前（`log-fail.txt`） | 修复后（`log-pass.txt`） |
| :--- | :--- | :--- |
| `/vendor/lib64/hw/vulkan.*.so` 与 `cdh-gpu : vulkan` 接管数 | 3 个 `.so`（45.2 MiB）/ `3` 次接管 | **已清空（0 个） / `0`** |
| 三星浏览器 `Timed out waiting for GPU channel` 闪退数 | `1`（打开约 7s 必现闪退） | **`0`（主进程 + GPU 进程 + 渲染进程长期稳定在线）** |
| 负一屏 `spage` CPU 占用与常驻内存（`RES`） | `Chrome_InProcGp` **104% 单核自旋** / `748 MiB` | **`0.0%`** / **`237 MiB`**（整机 `760%~781% / 800%` 空闲） |
| `hal_hyper_service` SELinux 拒绝 / `HYPER_NATIVE_INTF` 错误数 | `1` 次拒绝 / `559` 条 `E` | **`0` / `0`** |
| `CdhWifiHal: unknown host frame type=pong` 告警数 | `80` 条 `W` | **`0`** |

## 六、遗留

1. **三星私有 Wi-Fi 流量统计与被屏蔽 AI 服务的低频日志**：`semwifi-service.jar` 的 `WifiConnectivityMonitor.NetworkStatsAnalyzer: mSemTrafficStats is null.`（因未实现三星专有 `ISehWifi` 扩展）与 SystemUI `OnScreenNudge` 每 5s 重连已被主动屏蔽的 `com.samsung.android.smartsuggestions` 属于无性能损耗的缺省日志，不影响 WLAN/蜂窝上网与 UI 流畅度。
2. **`meminfo` 中 `SwapTotal: 6143M / SwapFree: 0M` 系三星 RAM Plus 虚报值**：`/proc/swaps` 实测为空（未启用 zram 交换分区），OneUI 内核模块在未挂载 swap 时固定将虚拟 `SwapUsed` 报为与 `SwapTotal` 相等，并非真实 swap 耗尽（各进程 `VmSwap` 实测均为 `0 kB`）。
