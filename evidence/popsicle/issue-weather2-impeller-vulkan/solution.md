# Issue: popsicle 天气（com.miui.weather2）打开即退出——Impeller/Vulkan 在 macOS 宿主拿不到可用设备

- 设备：`xiaomi/popsicle`（Xiaomi 17 Pro Max / `canoe` / lineage-23.2 / kernel 6.12）
- 阶段：运行态（图形栈：guest `vulkan.virtio.so`(venus 客户端) → host `virgl_render_server --socket-fd N`(venus 服务端) → MoltenVK → Metal）
- 状态：**未闭环**（macOS 宿主这条路已确认无解；本轮把「为什么无解」钉死，并**推翻**了本轮中途一条「GLES 已落 llvmpipe」的误判）
- 实例：21（`popsicle_cn_OS3.0.319.0.WPBCNXM`，console 5910 / adb 5911 / vnc websocket 5913）
- 关联：同机起机链 [#125](../issue-hal-hardware-missing/solution.md) / [#126](../issue-ueventd-frp-device-nodes/solution.md)；
  同一宿主 venus 的单队列先例 [gold #issue-boot-to-launcher-vulkan](../../gold/issue-boot-to-launcher-vulkan/solution.md)

## 一、问题

桌面点「天气」图标，屏幕闪一下回桌面；任务栈里留一条空记录（`topActivity` 被清）。
不是卡死、不是 ANR，是应用在 `onCreate` 里**主动 finish**。

## 二、根因

三层链条，逐层都有实证：

1. **天气是 HyperOS 自研 Flutter 应用，渲染后端写死 Impeller/Vulkan。**
   `ActivityWeatherMain` 只是壳，真正的 UI 在 `libhyper_os_flutter.so` + `libapp.so`（APK 内为
   Stored 压缩，可 mmap）。传递 `--ez enable-impeller false --ez enable-software-rendering true`
   被 HyperOS 场景框架忽略，仍走 Vulkan。

2. **Impeller 判定「没有可用 Vulkan 设备」。**
   `capabilities_vk.cc(274)` 报缺 `VK_KHR_swapchain` → `(310)` 扩展不满足 → `(393)` 队列也不满足
   → `context_vk.cc(292)` `No valid Vulkan device found` → `platform_view_android.cc(168)`
   `Could not create surface from invalid Android context` → `shell.cc(251)`
   `on_create_platform_view return null` → `hyper_os_flutter_backend::flutter_scene(290)`
   `Create engine failed, just finish` → `hyos_spawner ... finish_activity_v1()`。

3. **`VK_KHR_swapchain` 为什么永远不会出现（源码级，这是本轮的核心结论）。**
   guest 侧 Mesa 25.3.3 `src/virtio/vulkan/vn_physical_device.c`：

   | 行 | 条件 | 结果 |
   | :--- | :--- | :--- |
   | 1099-1117 | 仅当 `renderer_extensions.KHR_external_semaphore_fd` 为真 | 才向 host 查 `SYNC_FD` 的 importable/exportable，写入 `renderer_sync_fd.*` |
   | 1163-1165 | `semaphore_importable && fence_exportable` | 才给 `ANDROID_native_buffer` |
   | 1175-1181 | `semaphore_importable` | 才给 `KHR_swapchain`（连带 `present_id`/`present_wait`/`EXT_hdr_metadata` 等整套 WSI） |

   而 host 侧 **MoltenVK 没有 fd 语义**：`libMoltenVK.dylib`（5,328,032 B）里能 `strings` 出
   158 个扩展名，`VK_*_fd` 命中 **0** 个，只有 `VK_EXT_metal_objects` / `VK_EXT_external_memory_metal`
   / `VK_EXT_metal_surface` / `VK_EXT_headless_surface`。

   venus 服务端虽然在自己的静态扩展表里把三个 `*_fd` 写成了 `true`
   （`vkr_common.c:17` 表内 119-121 行 `KHR_external_fence_fd` / `KHR_external_memory_fd` /
   `KHR_external_semaphore_fd`），但 `vkr_extension_table_init()`（`vkr_common.c:241`）是
   **按 host 枚举到的扩展名逐个置位**——host 不宣告，venus 就不会转告 guest。

   于是 `renderer_sync_fd.semaphore_importable` 恒为 false，guest 永不宣告 `KHR_swapchain`，
   Impeller 的硬性检查必然失败。**这条与 guest 配置无关，改任何 guest 属性都动不了它。**

4. **第二条独立限制（gold 已证，本机同样成立）**：venus/MoltenVK 每个 queue family 只有 1 个队列，
   Android 图形栈要求 `queueCount >= 2`（`VulkanManager.cpp:234-245`）。即便 swapchain 有了，
   Impeller/hwui 仍会栽在队列上。Metal 不提供多队列，这条在宿主侧同样无解。

## 三、证据

- `log-fail.txt` 第 1 节：天气失败链原文（19:36 录，2829 行全量 logcat 中抽出的连续片段）。
- `log-fail.txt` 第 2 节：当前图形属性快照 + `persist.sys.force_sw_gles` 的真实出处。
- `log-fail.txt` 第 3 节：`MESA-LOADER` / `minigbm` 现象原文（21:02 复现）。
- `log-fail.txt` 第 4 节：宿主 MoltenVK 扩展清单探针、qemu 命令行中的图形设备参数。
- `log-fail.txt` 第 5 节：venus 服务端扩展表与 guest Mesa 条件判断的源码行号摘录。
- `log-pass.txt`：**本条没有通过态**；该文件记录的是已被证伪的方向、已排除项，以及本轮
  实验现场（改属性、重启实例、误入 recovery）的处置与复原。

## 四、改动

**仓库文件零改动。** 本轮只做实验，且在收尾时把现场恢复原状：

| 对象 | 实验改动 | 收尾状态 |
| :--- | :--- | :--- |
| guest `persist.sys.force_sw_gles` | 置 `0`（试图切回硬件 GLES） | **已复原为 `1`**，与 lineage 基座 `/system/build.prop:172` 同值 |
| guest `debug.egl.trace` | 置 `1`（拿 EGL 日志） | **已清空** |
| guest `/data/local/tmp` | 落 `w.log`、`w-pub.log`、`hwtest`、`b` | **已删除** |
| 清单 `vendor.build-prop-sets` | 未动（现仍为 `ro.hardware.egl=mesa` / `ro.hardware.vulkan=virtio` / `ro.hwui.use_vulkan=false`） | 未动 |

## 五、验证

1. **失败链完整**：19:36 从桌面图标启动录到完整七跳链（`log-fail.txt` 第 1 节）。之后 21:0x 三次
   `am start`/`monkey` 都被 HyperOS 的 `data-app` 组件解析挡住（`Activity class … does not exist`），
   未能取到第二份新副本——**本条不具备"重跑即复现"的条件，复现只能人工点图标**（见 `log-pass.txt` 第 4 节）。
2. **与 GLES 开关无关（按失败点判定，不是按重跑判定）**：把 `persist.sys.force_sw_gles` 由 `1` 改 `0`
   再冷启动，天气仍未起来；但受上一条限制，这一轮的判据是**失败发生的位置**而不是重跑：
   失败点是 Vulkan 物理设备枚举（`capabilities_vk.cc`），而该属性只决定 GLES 驱动走硬还是走软
   （`libEGL` 侧），二者不交叉。
3. **已排除项**（逐条实测，均不成立）：
   - 包状态：`enabled=0`、`hidden/suspended/stopped=false`、`notLaunched=false`、`quarantined=false`；
   - 缺 so：APK 内 `libhyper_os_flutter.so` / `libapp.so` / `libweather_app.so` 齐全且 Stored；
   - 内核没开 virgl：qemu.log 有 `[drm] features: +virgl +edid +resource_blob +host_visible`；
   - 设备节点缺失/权限：`/dev/dri/{card0,renderD128}` 为 `crw-rw-rw- system graphics gpu_device`，
     `dmesg | grep avc | grep dri` = **0**；
   - Mesa 没编 virgl：`/vendor/lib64/libgallium_dri.so` 里 virgl 驱动在内（`virgl=6 swrast=1 kms_swrast=0`）；
   - PCI 身份不全：`/sys/class/drm/renderD128/device/{vendor,device,class,subsystem_*}`
     齐全（0x1af4 / 0x1050 / 0x038000），uevent `DRIVER=virtio-pci`。
4. **GL 侧的两条硬证据，正好否掉「已落 llvmpipe」**（本轮中途的误判，见遗留 1）：
   - `com.android.settings`（pid 9353）与 `surfaceflinger`（pid 603）内：`llvmpipe/swrast` 线程 **0** 个、
     `libLLVM` 映射 **0** 条，而 settings 进程持有 **4 个** `/dev/dri/renderD128` 的 fd；
   - 宿主机 `virgl_render_server`（pid 91824）跑满 23:47 只累计 **0.09 s** CPU——但这条**不能**当
     「GLES 走软」的证据：GL（virgl）渲染在 QEMU 进程内由 cgl-headless 的 GL 上下文执行，
     `virgl_render_server` 只承载 venus/Vulkan 上下文。

## 六、遗留

1. **系统 GLES 究竟是 host virgl 还是 guest 软件光栅：未定性。** 判据缺口是自省通路全断——
   `dumpsys SurfaceFlinger` 在本机只回 `FAILED_TRANSACTION`、`cmd gpu vkjson` 回
   `Failure calling service gpu`、`screencap` 挂住，拿不到那句权威的
   `GLES: <vendor>, <renderer>, OpenGL ES <ver>`。已排掉的是「llvmpipe 在跑」（无线程、无 LLVM 映射、
   持有 renderD128 fd），**没有**排掉「virgl 驱动起来了但 EGL 仍走了别的路径」。
   下一步要么修 `dumpsys SurfaceFlinger` 通路，要么在 guest 里跑一个最小 EGL 探针打印 `GL_RENDERER`。
2. **天气（以及所有 HyperOS Flutter 应用）在这套栈上只有三条出路，且都不属于「软渲染」**：
   a. 宿主图形栈提供 fd 语义的 external semaphore/fence——Metal 没有，只能换 Linux/Vulkan 宿主；
   b. 应用侧允许 Impeller 回退 GLES——HyperOS 场景框架忽略了启动参数，要改 framework/引擎；
   c. 改 guest venus 客户端绕过 WSI 需求——等于自养一个 Mesa fork。
   本轮均未做，等方向决策。
3. **`virgl_render_server` 反复报 `failed to receive framed message: truncated or empty`**
   （20:46–21:02 共 8 次，qemu.log），说明 guest 侧有进程在反复建/断 venus 上下文。与天气失败
   无直接因果（天气是在设备枚举阶段就失败，还没建上下文），但值得在关掉 venus 的实例上单独复核一次。
4. **两条只在本机成立的「别踩」结论**（已写进本组 `README.md` 备注）：
   运行态 `setprop ctl.restart surfaceflinger` 会打坏 HyperOS 的窗口状态（launcher 进程消失、
   `mCurrentFocus` 长期为 null，`am start`/`stop;start` 都救不回）；`pm enable <data-app 包>`
   会让 system_server 崩一次并最终窜进 recovery（`recovery: Boot command: boot-recovery`）。
   两者都只能用「宿主 daemon API 停/起实例」或 QMP `system_reset` 恢复。
