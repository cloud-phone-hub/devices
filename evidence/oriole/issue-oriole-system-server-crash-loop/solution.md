# Issue: oriole AP2A 纯 64 位锁补齐后，官方 `ro.frp.pst` 触发 `TestHarnessModeService` / `PersistentDataBlockService` `invalid checksum` 崩溃循环 + `ro.hwui.use_vulkan=true` 触发 UI 进程 `!gpuCount` 崩溃

- 设备: Google Pixel 6（oriole，lineage-21.0 底包 + 官方固件 AP2A.240905.003.F1，内核 `5.10.270-4k-cdh`）
- 宿主: macOS (darwin, QEMU + hvf)
- 状态: **已闭环**（2026-11-04）：在 `system_server` 重启窗口内通过 `adb disconnect + connect` 抓到 `logcat -b crash` 真凶；基线 `vendor.build-prop-unsets` 删除 `ro.frp.pst`、`vendor.build-prop-sets` 锁 `ro.hwui.use_vulkan=false`，重跑 `07 → 11` 后从零起盘，**27.98 s** `sys.boot_completed=1`，VNC 抓帧 1088×2400 平均亮度 **91.3** / 峰值 255（完整 Pixel Launcher 桌面）。

---

## 一、问题

#148 切换为以官方 `vendor_a.img:/build.prop` 为基底、#149 补齐纯 64 位锁后，实例 `runtime/oriole_AP2A.240905.003.F1/1` 起机分为两层卡点：

1. **run1（`system_server` 每 4~5 s 崩溃 → RescueParty → recovery）**：
   - `qemu.log` 始终无 `sys.boot_completed=1`；
   - 前 4 轮引导因 `critical process 'zygote' exited 4 times before boot completed` → `InitFatalReboot: signal 6` → `Restarting system with command 'zygote-fatal'` 重启；
   - 后 2 轮由 `system_server` 发起 `reboot,RescueParty`，末轮进入 `recovery --prompt_and_wipe_data`；
   - 初次排查因 `adb connect` 未在重启间隙 `disconnect` 重连而停留在 `offline`、未能取到 `logcat`。
2. **run2（仅删除 `ro.frp.pst` 后）**：
   - 整机在 62.21 s 顺利到达 `sys.boot_completed=1`，`system_server` 稳定不退；
   - 但 `com.google.android.setupwizard`、`com.android.systemui`、`com.android.settings` 的 `RenderThread` 持续 `SIGABRT`（`Abort message: 'Assertion failed: !gpuCount'`），导致桌面 UI 进程循环崩溃。

## 二、根因

两层卡点均源于 [#148](../../common/issue-vendor-buildprop-official-source/solution.md) 将 `07_build_vendor` 的 `vendor/build.prop` 基底从 `lineage-21.0` 切换为官方 `AP2A.240905.003.F1/super/vendor_a.img:/build.prop` 后带入的真机属性：

| 层 | 官方 `vendor/build.prop` 属性 | `lineage-21.0` 基底原值 | 崩溃链与断言点 |
| :--- | :--- | :--- | :--- |
| 1 | 第 185 行 `ro.frp.pst=/dev/block/by-name/frp` | 无此键（空） | `SystemServer` 见 `ro.frp.pst` 非空即启动 `PersistentDataBlockService` 并注册 `PersistentDataBlockManagerInternal`；本盘 GPT 来自 lineage 底盘、**无 `frp` 分区**，底层打开失败。到 Phase 500（`PHASE_ACTIVITY_MANAGER_READY`）时，`TestHarnessModeService.onBootPhase(500)` 调 `PersistentDataBlockService$InternalService.getTestHarnessModeData()` → `readInternal()` → `enforceChecksumValidity()` 返回 `false` → 抛 `java.lang.IllegalStateException: invalid checksum`，未被捕获而直接杀掉 `system_server` |
| 2 | 第 45 行 `ro.hwui.use_vulkan=true` | 空（默认 GL） | App 的 `RenderThread` 按 `ro.hwui.use_vulkan=true` 走 `SkiaVulkanPipeline` → `libhwui.so!VulkanManager::initialize()` 枚举物理 GPU 数量为 0 → `Assertion failed: !gpuCount`（与 [#151](../../15r/issue-15r-gpu-backend-name-mismatch/solution.md) 同源） |

排除项确认：初次排查怀疑的 `avc: denied { read } ... vendor_mesa_prop`（`permissive=0`）仅使 bionic 属性读取返回默认值，并非 `system_server` 崩溃原因。

## 三、证据

- `log-fail.txt`：
  - 第 1~9 节：初次 6 轮引导 `zygote-fatal` → `RescueParty` → `recovery` 时序与排除项；
  - 第 10 节：在重启窗口内执行 `adb disconnect 127.0.0.1:5911 && adb connect 127.0.0.1:5911` 抓到的 `TestHarnessModeService` / `PersistentDataBlockService$InternalService.readInternal` `IllegalStateException: invalid checksum` 完整崩溃栈，以及 `vendor/build.prop:185` 的 `ro.frp.pst=/dev/block/by-name/frp`；
  - 第 11 节：run2 删除 `ro.frp.pst` 后 `62.208252 s` 到达 `sys.boot_completed=1`，以及 `setupwizard` / `systemui` / `settings` 的 `Assertion failed: !gpuCount` 崩溃栈与在线 `setprop debug.hwui.renderer skiagl` 免重编恢复旁证。
- `log-pass.txt`：
  - 装配树 `vendor/build.prop` 实值复核（`ro.hwui.use_vulkan=false`、`ro.frp.pst` 0 匹配）；
  - run3 从零起盘 `27.981012 s` 到达 `sys.boot_completed=1`；
  - `system_server`（873）、`surfaceflinger`（765）、`com.android.systemui`（2067）、`com.google.android.apps.nexuslauncher`（2560）稳定存活，焦点停在 `NexusLauncherActivity`；
  - `TestHarnessModeService` 走 `no implementation of PersistentDataBlockManagerInternal was bound!` 正常跳过，`invalid checksum` 与 `!gpuCount` 均为 **0 次**。
- `desktop-pass.png`：run3 VNC WebSocket（端口 5913）抓帧，1088×2400，平均亮度 **91.3** / 峰值 255，完整 Pixel 6 桌面（状态栏 Wi-Fi 图标已连接、时钟、Play Store / Gmail / Photos / YouTube / Phone / Messages / Chrome / Camera 图标与搜索栏完整）。

## 四、改动

仅修改基线清单 [oriole-ap2a.240905.003.f1-factory-655c44e7.json](file:///Volumes/work/cloud-phone-hub/devices/manifest/google/oriole/baselines/oriole-ap2a.240905.003.f1-factory-655c44e7.json)，保持 `oriole` 无机型静态树（`vendor/` 与 `sepolicy/` 0 文件）的精简形态：

1. `vendor.build-prop-unsets` 新增 `"ro.frp.pst": ""`（与 `gold` 及 `Template.md` 同口径）；
2. `vendor.build-prop-sets` 新增 `"ro.hwui.use_vulkan": "false"`（与 `m22` / `gold` / `15r` / `cph2797` / `ss9380` / `popsicle` 同口径）；
3. `vendor.note` 补充两条属性改动的因果与实测日志说明。

## 五、验证

1. 执行 `cdh device build oriole --manifest oriole-ap2a.240905.003.f1-factory-655c44e7 --from build_vendor`（`07 → 11` 全绿，`patch.img` `sha256: 32a528d8c46b`，`vda-recipe: vda-681ba1a2f915`）；
2. `/api/stop` + `/api/delete` 清理旧实例目录后，通过 `host-main run 1` 从零拉起实例 `oriole_AP2A.240905.003.F1/1`（PID `19671`）；
3. 核验通过（详见 `log-pass.txt`）：
   - 内核：`Linux localhost 5.10.270-4k-cdh #1 SMP PREEMPT Wed Nov 4 02:55:20 UTC 2026 aarch64`；
   - `sys.boot_completed=1` @ `27.98 s`，`init.svc.bootanim=stopped`；
   - `FATAL EXCEPTION IN SYSTEM PROCESS` 0 次、`invalid checksum` 0 次、`!gpuCount` 0 次；
   - VNC 抓帧 `desktop-pass.png` 1088×2400 平均亮度 `91.3` / 峰值 `255`。

## 六、遗留

- `oriole` 其余三份未实跑基线（`bp1a` / `cp1a` / `cp3a`）后续若启用，需同样在 `vendor.build-prop-unsets` 中删除 `ro.frp.pst` 并在 `vendor.build-prop-sets` 中锁 `ro.hwui.use_vulkan=false`（其中 `cp1a` 当前在 `build-prop-sets` 中显式写了 `"ro.frp.pst": "/dev/block/by-name/frp"`，届时需一并移除）。
- 官方 `product_a.img` 预置的 Chrome 壳包缺 `libmonochrome.so` 首次解锁会弹一次 `Application Error: com.android.chrome`（已在 [#105](../issue-oriole14-hwc-drm/solution.md) 遗留中登记，与引导及桌面渲染无关）。
