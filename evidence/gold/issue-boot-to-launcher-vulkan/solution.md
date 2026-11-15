# gold 首启直达 launcher（hwui Vulkan SIGABRT 闭环）—— 里程碑证据

机型：xiaomi/gold（Redmi Note 13 5G）
清单：`gold_images_OS3.0.10.0.VNQCNXM_20260715.0000.00_15.0_cn_a9729c6694`
产物：`engine/out/dist/xiaomi/gold/OS3.0.10.0.VNQCNXM`
失败态：`log-fail.txt`（实例 25/26/27）
通过态：`log-pass.txt` + `launcher.png`（实例 28，全新 data，2026-10-26 01:12 起）
承接：`../issue-boot-to-launcher/`（OOBE/provisioning 已在该里程碑闭环，本轮不再涉及）

## 1. 问题

用当前 dist 启动后，系统能走到 `sys.boot_completed=1`（provisioning 也已为 1/1），
但界面停在开机动画：`com.miui.home` 与 `system_server` 的 RenderThread 反复
SIGABRT，launcher 无法呈现。

tombstone 首条断言：

```
Abort message: 'Assertion failed: err < 0'
#04 libhwui.so (VulkanManager::initialize()::$_0)+292
#06 libhwui.so (RenderThread::requireVkContext())
#07 libhwui.so (RenderProxy::preload())
```

`setenforce 0` 后仍以同一断言崩溃，排除 SELinux。修复宿主后，断言变为
`Assertion failed: queueProps[i].queueCount < kRequestedQueueCount`，第二条
根因才暴露。

## 2. 根因

两条根因串联，第二条在第一条修好后出现：

1. **宿主 virglrenderer 的 venus worker 没有可用的 Vulkan ICD。**
   分两层：
   - venus worker 以裸名 `dlopen("libvulkan.1.dylib")`，而 daemon 拉起 QEMU 时
     未把 `out/qemu/dist/darwin/lib` 放进 dyld 搜索路径，dlopen 直接失败，
     随后 `failed to dispatch context op 1`、`ctrl 0x10c error 0x1200`；
   - 补上搜索路径后仍失败：dist 的 `MoltenVK_icd.json` 声明
     `is_portability_driver: true`，而 venus 创建实例时未启用
     `VK_KHR_portability_enumeration`，loader 按 portability 规则拒绝，
     `vkCreateInstance` 返回 `-9`（`VK_ERROR_INCOMPATIBLE_DRIVER`）。

   guest 侧 hwui 在 `VulkanManager.cpp:199` 用
   `LOG_ALWAYS_FATAL_IF(err < 0)` 断言，于是 home 与 system_server 的
   RenderThread 直接 abort。

2. **venus/MoltenVK 的队列能力不满足 hwui 的硬性假设。**
   宿主 Vulkan 可用后，枚举到 4 个 queue family，但每个 family 都
   `queueCount=1`（Metal/MoltenVK 不提供多队列）；hwui
   `VulkanManager.cpp:234-245` 要求首个 GRAPHICS family
   `queueCount >= kRequestedQueueCount(2)`，否则
   `LOG_ALWAYS_FATAL_IF(queueProps[i].queueCount < kRequestedQueueCount)`。
   因此只要 hwui 走 Vulkan 后端，在这套虚拟 GPU 上必然 abort。

   GLES(virgl) 路径本身正常（SurfaceFlinger：`GLES: ARM, Mali-G57 MC2,
   OpenGL ES 3.2`），故在 guest 配置层关闭 hwui Vulkan 后端
   （`ro.hwui.use_vulkan=false`）是这套栈上可用的根因配置；改 libhwui
   属于系统库侵入，未采用。

## 3. 证据

- `log-fail.txt`
  - A：实例 26 qemu.log 的 venus dlopen / dispatch / ctrl 错误原文；
  - B：实例 26 crash buffer，home 与 system_server 的 RenderThread
    `err < 0` tombstone 及 libhwui 栈帧；`setenforce 0` 对照；
  - C：实例 27 通道日志中 `queueProps[i].queueCount <
    kRequestedQueueCount`（home + system_server），以及 163.8s 的
    `reboot,RescueParty`（旧 data 残留，故换全新实例）；
  - D：宿主探针 `vkprobe.py` —— ICD portability=true → `-9`；
    false → `0`，物理设备数 1（Apple M4）；
  - E：`vkqueues.py` —— 4 个 family 全部 `queueCount=1`；
  - F：`engine/out/tmp/vulkanmanager.cpp` 的断言行号（199、234-245）。
- `log-pass.txt`：最终产物哈希、patch.img 抽件、实例 28 全部运行态判据、
  截图与 qemu.log 计数。
- `launcher.png`：1088x2400 完整 HyperOS 桌面。
- 复现材料：`vkprobe.py`、`vkqueues.py`、`MoltenVK_icd_portability.json`。

## 4. 改动

1. `qemu/backend/gpu-host-macos/MoltenVK_icd.json`
   `is_portability_driver: true → false`，使 venus 不启用 portability 位也能
   创建实例。sha256 `2af79f0d0a44a23d42f2d6dfb94406de9f93fdc13f7ab14f9c3ad78d7fa977f8`，
   已同步到 `out/qemu/dist/darwin/lib/MoltenVK_icd.json`（逐字节一致）。
   说明：`build/qemu/build-qemu-darwin` 全量脚本因宿主缺 PyYAML 无法执行
   （受“不得安装工具”约束），故按脚本内同一条 `install -m 0644` 语义手工
   同步并核哈希；脚本恢复后重跑安装即可复现。
2. `core/host/daemon/run/qemu.go`（darwin 分支，`buildQemuArgs` 内）
   注入：
   - `RENDER_SERVER_EXEC_PATH = <qemuRoot>/libexec/virgl_render_server`
   - `VK_ICD_FILENAMES = <qemuRoot>/lib/MoltenVK_icd.json`
   - `DYLD_LIBRARY_PATH = <qemuRoot>/lib[:原值]`
   daemon 已重建并重启（`out/core/daemon/dist/darwin/daemon`
   sha256 `ce12541a018f834c93e4bc1e87dbd13c3a9701b2ac87a63f1d532b0dd6b5376c`，
   01:00 启动）。运行中的 QEMU 进程环境已确认含上述三项。
3. `devices/manifest/xiaomi/gold/vendor/build.prop`
   line 50 `ro.hwui.use_vulkan=false`（原 true）。
   保留 `ro.hardware.vulkan=mali`、`ro.hardware.egl=meow`，GLES 仍走
   virgl/mali 路径。
4. 按顺序重建 07（build-vendor）→ 08（build-vendor-dlkm）→ 09（build-patch）
   → 10（pack-super）→ 11（pack-vda），降并发执行。
   - 08 复用 `vendor_dlkm.img`（配方 `dlkm-d7b1ba341054` 未变）；
   - 10 复制官方 `super.img`（不解析、不重排，vendor 改动走 patch.img）。

## 5. 验证

1. 最终产物（01:09 发布）：
   - `patch.img` 402653184 B，sha256 `9798a090184e…`；
   - `vda.img` 13958643712 B，sha256 `892202fa291b…`（5 个分区原位刷新，
     boot_a/boot_b、vendor_boot_a/b 回读一致）；
   - `vendor.img` sha256 `0dc64a44ddcb…`；`boot.img` sha256 `932bfb7260…`；
   - `device.json` id `gold_cn_OS3.0.10.0.VNQCNXM`。
2. 从最终 `patch.img` 用 debugfs 抽件（不读装配树）：
   - `/build.prop` sha256 `115459d2f75d…`，line 50 `ro.hwui.use_vulkan=false`，
     line 583 `ro.hardware.egl=meow`；
   - `/etc/selinux/vendor_sepolicy.cil` 中
     `(allow system_server_202404 pioneer (binder (call transfer)))` 计数 1
     （provisioning 里程碑的规则未回退）。
3. 宿主栈探针（`log-pass.txt` 第 5 节）：portability=true → -9；
   false → 实例创建成功、设备数 1；queue family 全部 `queueCount=1`。
4. 全新 data 实例 28（`POST /api/create {"index":0}`，01:12:21）：
   - guest 90.26s 到达 `sys.boot_completed=1`，system_server pid 677 全程未变；
   - `device_provisioned=1`、`user_setup_complete=1`；
   - `cmd -l | wc -l` = 354，立即返回；
   - `mCurrentFocus=Window{da06dc5 u0 com.miui.home/com.miui.home.launcher.Launcher}`、
     `pidof com.miui.home` = 2523；
   - `dumpsys SurfaceFlinger`：`GLES: ARM, Mali-G57 MC2, OpenGL ES 3.2`、
     `vulkan_renderengine: false`；
   - `dmesg | grep -c virtio_gpu_dequeue` = 0；
   - crash buffer 无 RenderThread 条目，唯一的 `Abort` 是
     `setup_sound_card` 的 `stoi` 既有噪声（01-01 08:00:09，pid 446）；
   - qemu.log 中 `failed to open libvulkan` / `failed to dispatch context op`
     / `ctrl` 错误 / `Abort message` 计数均为 0；本轮 guest 不创建 Vulkan
     上下文，故 `vkr:` 计数 0，与 `vulkan_renderengine: false` 一致。
5. 截图 `launcher.png`：完整桌面，非黑像素 2591741，
   sha256 `44589dc4445a…`。

## 6. 遗留

- **宿主单队列限制仍在**：venus/MoltenVK 每个 family 仅 1 队列。若未来要求
  hwui 走 Vulkan，需要渲染栈提供多队列（Metal 路线不可行），或调整系统
  libhwui 的 `kRequestedQueueCount`（侵入路线）。本轮按配置关闭 Vulkan
  后端，GLES 路径正常；venus 修复保留，应用侧 Vulkan 仍可用（宿主探针已验证）。
- ~~**codec2 v4l2 HAL 反复重启**~~ **已解决**：由 `../issue-v4l2-codec2/` 闭环
  （补 15 个非 LLNDK 运行库 + `hal_codec2_default_exec` 标签 + servicemanager
  通道授权）。2026-10-27 在实例 4 上复核：
  `init.svc.android-hardware-media-c2-v4l2-hal-1-2=running`、pid 563。
- ~~`setup_sound_card` 在无声卡 VM 下 `std::stoi` 抛异常 abort~~ **已消失**：
  `../issue-qemu-hda-sound-card/` 给 QEMU 接上 intel-hda 后，该 oneshot 输出变为
  `I setup_sound_card: No recognized sound card found, Exiting.`，不再 abort。
- 宿主 python3 缺 PyYAML，`build/qemu/build-qemu-darwin all` 无法全量执行；
  受“不得安装工具”约束，本轮只做 ICD 安装同步。补依赖属环境决策。
- ~~`engine/build/07_build_vendor/hal.go:104` 的 `halCommand()` 不传
  `--abi-dir`~~ **已修**：`hal.go:117` 现在会为 v412 追加两个 `--abi-dir`
  （先 system 后 apex）。
- 实例 27 复用了旧 data，163.8s 被 `reboot,RescueParty` 重启（旧
  `rescue_boot_count` 残留），不再作为通过态；通过态以全新 data 的实例 28
  为准。实例 25/26/27 与更早调试实例尚未删除，实例 28 保留运行供人工查看。
- daemon/QEMU/清单改动均未提交；提交与清理动作待用户确认。
