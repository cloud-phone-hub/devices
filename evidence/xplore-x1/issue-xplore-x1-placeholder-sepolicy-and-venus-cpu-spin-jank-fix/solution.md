# #347 — `blackview/XPLORE_X1` 切应用每次死锁 5~30s、Chromium Mesa Venus 100% CPU 自旋与 `netdiag` 崩溃风暴修复

## 1. 问题

`Blackview Xplore 1 #1`（`blackview/XPLORE_X1`，Build ID `15-AP3A.240905.015.A2-20251126`，Doke OS 5 / MT6877 / Android 15，`1080x2460`，`lineage-22.2` + `kernel 6.6`）在完成 [../issue-xplore-x1-yv12-v4l2-codec-and-video-encode-fix/](../issue-xplore-x1-yv12-v4l2-codec-and-video-encode-fix/)（`#346`）编解码修复后，交互与静置态存在严重卡顿：

1. **应用切换 / Home 键每次同步卡死 `5~30 秒`**：冷启动计算器 `WaitTime: 29,701 ms`、设置 `WaitTime: 20,159 ms`、原生相机 `WaitTime: 15,436 ms`；按一次 Home 键触发 `W Looper: Slow dispatch took 5030ms android.ui h=PhoneWindowManager$PolicyHandler` 与 `W BLASTSyncEngine: Sync group 38 timeout`；设置列表滑动出现单帧 `4950 ms` 卡死，整机冷启动耗时 `48s`。
2. **息屏/静置态常驻吃满一整颗 CPU 核心（`100%`）**：宿主机 `qemu-system-aarch64` 在无交互静置态常驻 `107.5%~134.3% CPU`；客机 `com.google.android.googlequicksearchbox:googleapp`（桌面负一屏 Google Discover WebView）的 `Chrome_InProcGp` 线程单核 `100% CPU` 死循环运行。
3. **后台守护进程崩溃循环与 SELinux 审计风暴**：`/system_ext/bin/netdiag` 每 5 秒因 socket 创建与属性读取被拒退出重启（`[init.svc.netdiag]: [restarting]`），每 4 次触发 `flags_health_check UPDATABLE_CRASHING`（`[sys.init.updatable_crashing_process_name]: [netdiag]`）。

## 2. 根因

1. **51 个 MTK / Blackview 占位 AIDL 服务因 `09_build_vendor` 无平台 CIL 分支提前返回而漏生 SELinux `add` 授权，导致 `system_server` 每次焦点切换同步等待 `IMtkpower_applist/default` 超时 `5008 ms`**：
   - `XPLORE_X1-android15/vendor/etc/cdh_nn_placeholders.txt` 虽已声明 `vendor.mediatek.hardware.mtkpower_applist.IMtkpower_applist/default`、`IMtkPowerService/default`、`IComposerExt/default`、`IHelper/default` 等 51 项服务，且 VINTF manifest 已宣告 `isDeclared == true`；
   - 但 `agent/modules/pipeline/stages/09_build_vendor/sepolicy_cil.go` 的 `reconcilePlatformGenfscon` 在未提供 `OfficialVendorImg`（第 991 行）或未挂载官方 `system_a.img` 致 `len(platCils) == 0`（第 1185 行）时**提前 `return`**，跳过了排在其后的 `reconcilePlaceholderServiceBindingsCIL`、`reconcileDrmWidevineBindingsCIL` 与 `reconcileSensorsIdentityPropsCIL`；
   - 结果 `hal_neuralnetworks_default` 缺失对 `mtk_hal_applist_service`、`mtk_hal_power_service`、`mtk_hal_composer_ext_service`、`hal_blackview_service` 等 19 类厂商自定义服务标签的 `(service_manager (add find))` 权限，启动时全部注册失败；同时机型 `vendor_sepolicy_rules.cil` 亦未放行 `system_server` / `surfaceflinger` 等跨分区客户端对承载域 `hal_neuralnetworks_default` 的 `find` 与 `binder (call transfer)`；
   - `system_server`（含 `android.ui` 主线程）每次窗口焦点切换调用 `PowerHalWrapper -> mtkpower_client -> PowerHal_Wrap_notifyAppState`，因 VINTF 已声明 `IMtkpower_applist/default` 而 `servicemanager` 无实例在线，`ServiceManagerCppClient` 每次调用同步阻塞等待 **5008 ms**（一次 Activity 切换触发 4~6 次状态通知即串行死锁 20~30 秒）；
   - 此外，`devices/hardware/android.hardware.neuralnetworks/src/mtk/nn_mtk_ext.cpp` 尚未为 `IMtkpower_applist`（V3）与 `IMtkPowerService`（V3）提供 AIDL NDK 状态头兼容桩（默认 `OnPlaceholderTransact` 返回 `STATUS_UNKNOWN_TRANSACTION`）。
2. **macOS 宿主下 Mesa Venus（`vulkan.virtio.so`）触发 Chromium `Chrome_InProcGpu` `CreateInstance` 自旋死循环**：
   - `XPLORE_X1` 原配置 `"android.hardware.gpu": {"variant": "default"}` 且未配置 `ro.vendor.cdh.vulkan.base=pastel`，`vulkan.mali.so` 默认转发至 `vulkan.virtio.so`（Mesa Venus），同时普通 App 因 `gpu_id.cfg` 无该键回退读取系统属性触发 `W libc: Access denied finding property "ro.vendor.cdh.vulkan.base"`；
   - 与 `../../common/` 及 popsicle `#140`/`#166` 同源：macOS 宿主 `libMoltenVK.dylib` 缺少 `VK_KHR_external_semaphore_fd`，Chromium GPU 线程在 `libmonochrome_64.so -> vulkan::api::CreateInstance -> vulkan::driver::CreateInstance` 内陷入 100% CPU 无限重试自旋。
3. **官方 `netdiag.rc` 在非工程调试态默认自启并因无物理网络诊断 socket 反复退出**：
   - 原厂 `/system_ext/etc/init/netdiag.rc` 自带官方停用触发器 `on property:persist.vendor.em.dy.debug=1 -> stop netdiag`，此前未在 `vendor/config.json` 置位。

## 3. 证据

详见同目录 [log-fail.txt](log-fail.txt)、[log-pass.txt](log-pass.txt) 与修后桌面实拍 [home-pass.png](home-pass.png)：

- **修前焦点切换 5008ms 串行阻塞与 29.7s 慢启动**：
  ```text
  Starting: Intent { act=android.intent.action.MAIN cat=[android.intent.category.LAUNCHER] pkg=com.android.calculator2 }
  TotalTime: 20816
  WaitTime: 29701
  11-26 21:19:33.109   749   794 W Looper  : Slow dispatch took 5030ms android.ui h=com.android.server.policy.PhoneWindowManager$PolicyHandler
  11-26 21:19:33.115   749  3999 I mtkpower_client: [PowerHal_Wrap_notifyAppState] com.blackview.launcher/com.android.searchlauncher.SearchLauncher pid=1245 activityId:7651584 state:1
  11-26 21:19:33.115   749  3999 I ServiceManagerCppClient: Waiting for service 'vendor.mediatek.hardware.mtkpower_applist.IMtkpower_applist/default' on '/dev/binder'...
  11-26 21:19:38.121   749  3999 W ServiceManagerCppClient: Service vendor.mediatek.hardware.mtkpower_applist.IMtkpower_applist/default didn't start. Returning NULL
  11-26 21:19:38.122   749   796 W BLASTSyncEngine: Sync group 38 timeout
  ```
- **修前占位服务 SELinux `add` 被拒与 Venus 100% CPU**：
  ```text
  I auditd  : avc:  denied  { add } for pid=598 uid=1000 name=vendor.blackview.helper.IHelper/default scontext=u:r:hal_neuralnetworks_default:s0 tcontext=u:object_r:hal_blackview_service:s0 tclass=service_manager permissive=0
  I auditd  : avc:  denied  { add } for pid=598 uid=1000 name=vendor.mediatek.hardware.composer_ext.IComposerExt/default scontext=u:r:hal_neuralnetworks_default:s0 tcontext=u:object_r:mtk_hal_composer_ext_service:s0 tclass=service_manager permissive=0
  W libc    : Access denied finding property "ro.vendor.cdh.vulkan.base"
  cuimingqiang 36413 107.5 3.3 ... qemu-system-aarch64 ... (guest Chrome_InProcGp tid=11203 100% CPU in vulkan::driver::CreateInstance)
  ```

## 4. 改动

1. **修复 `09_build_vendor` 无平台 CIL 分支漏调本地策略协调器的管线缺陷 + 单元测试**：
   - `agent/modules/pipeline/stages/09_build_vendor/sepolicy_cil.go`：提炼 `finalizeVendorLocalAndConditionalCIL`，使 `OfficialVendorImg` 缺失分支、`len(platCils) == 0` 增量构建分支与完整 `platCils` 主分支统一执行 `reconcilePlaceholderServiceBindingsCIL`、`reconcileDrmWidevineBindingsCIL`、`reconcileSensorsIdentityPropsCIL`、`applyConditionalCILRules` 与 `applyConditionalContextRules`（同时保持无 `platCils` 时不调依赖平台类型全集的 `reconcileLegacyCompatBindingsCIL`，杜绝重复声明平台服务类型）。
   - `agent/modules/pipeline/stages/09_build_vendor/sepolicy_test.go`：新增 `TestReconcilePlatformGenfsconWithoutPlatCils`，覆盖无 `OfficialVendorImg` 与仅有 `OfficialVendorImg` 而无 `system_a.img` 两种场景的占位服务授权与防重复类型声明断言。
2. **补齐 MTK NeuralNetworks 变体 `IMtkpower_applist` / `IMtkPowerService` V3 AIDL 兼容应答桩**：
   - `devices/hardware/android.hardware.neuralnetworks/src/mtk/nn_mtk_ext.cpp`：新增 `OnMtkPowerApplistTransact`（V3，hash `f2fe2189a9da42ccf608e55da9420bad2fa42c81`）与 `OnMtkPowerServiceTransact`（V3，hash `a484177ceab5bb684c14f2e87cf686ac32b6757c`），按实机 NDK vtable 签名返回 `ScopedAStatus::ok()` 与空串 / `int32(0)`。
3. **切换 `XPLORE_X1` Vulkan 基座至 `pastel` 并收敛系统属性与跨分区 SELinux 策略**：
   - `devices/manifest/configs/blackview/XPLORE_X1/XPLORE_X1-android15/hardware/config.json`：将 `android.hardware.gpu` 的 `variant` 从 `"default"` 切为 `"pastel"`（启用 `vulkan.pastel.so` + SPIR-V Float16->Float32 / 1x MSAA 兼容层，OpenGL ES 仍保持 `libEGL_mesa.so` 宿主硬件加速）。
   - `devices/manifest/configs/blackview/XPLORE_X1/XPLORE_X1-android15/vendor/config.json`：新增 `"ro.vendor.cdh.vulkan.base": "pastel"`（自动剥离进 `gpu_id.cfg`）、`"ro.kernel.qemu.vulkan.base": "pastel"`、`"persist.system.powerhal.applist_enable": "0"`、`"debug.sf.vsync_reactor_ignore_present_fences": "true"` 与 `"persist.vendor.em.dy.debug": "1"`。
   - `devices/manifest/configs/blackview/XPLORE_X1/XPLORE_X1-android15/vendor/sepolicy/vendor_sepolicy_rules.cil`：声明式放行 `system_server`、`surfaceflinger`、`audioserver`、`cameraserver`、`radio`、`platform_app`、`priv_app`、`system_app` 对已由 `hal_neuralnetworks_default` 承载的 `mtk_hal_{applist,power,composer_ext,nvramagent,lbs,bgs,mms}_service` 与 `hal_blackview_service` 的 `find` 及双向 `binder (call transfer)`，并添加无关硬件探测的 `dontaudit` 规则。

## 5. 验证

1. **单测与镜像构建**：`go test -count=1 ./stages/09_build_vendor` 全绿（`2.261s`）；重编 `pipeline`、增量重编 `android.hardware.neuralnetworks@mtk` 与 `android.hardware.gpu@pastel`，产出新 `patch.img`（`402,653,184 B`，`sha256 c56fb9b06ac1`，968/968 回读一致，`e2fsck` 通过）。
2. **修前后实机量化对比（实例 #1，ADB `127.0.0.1:5911`，`getenforce = Enforcing`）**：

   | 指标 | 修复前 | 修复后 |
   | :--- | :--- | :--- |
   | 整机冷启动至 `sys.boot_completed=1` | `48 s` | **`27 s`** |
   | `IMtkpower_applist/default` & 51 个占位服务 | `not found`（19 类 SELinux `add` 拒绝） | **`found`（51 项全量在线）** |
   | 计算器 `am start -W`（冷启动 / 热启动） | `29,701 ms` | **`813 ms` / `100 ms`** |
   | 系统设置 `am start -W`（冷启动 / 热启动） | `20,159 ms` | **`562 ms` / `156 ms`** |
   | 原生相机 `am start -W`（冷启动 / 热启动） | `15,436 ms` | **`1,026 ms` / `17~44 ms`**（预览 `28.52 fps`） |
   | `Waiting for ... IMtkpower_applist` / `>5000ms` 慢分发 | 每次切应用触发（单次 `5,008~5,030 ms`） | **`0` 次** |
   | `Settings` 连续 10 次滑动（597 帧） | 出现 `4950 ms` 死锁帧 | **Janky `1.68%`（P50 `8ms` / P90 `15ms` / P99 `29ms`，`4950ms=0`）** |
   | 桌面 + Google 负一屏 + 抽屉滑动（491 帧） | 负一屏触发 GPU 线程 100% CPU 自旋 | **Janky `2.04%`（P50 `7ms` / P90 `13ms` / P95 `17ms`）** |
   | 客机静置 CPU 空闲率（`800%` 总核） | `Chrome_InProcGp` 常驻 `100% CPU` | **`778% idle / 800% cpu`（空闲率 `97.25%`）** |
   | 宿主机 `qemu-system-aarch64` 静置 CPU | `107.5% ~ 134.3%` | **`8.2%`** |
   | `netdiag` 服务状态 | `restarting`（触发 `UPDATABLE_CRASHING`） | **`stopped`**（`persist.vendor.em.dy.debug=1`） |
   | 桌面动态壁纸硬解回归核验 | （`#346` 基线） | **深宝石蓝 `32.35%`、荧光绿 `0.00%`、零红字水印** |

   ![修后桌面动态壁纸与状态栏实拍](home-pass.png)

## 6. 遗留

- `system_server` 内 `MBrainLocalService` 在 `ro.vendor.mbrain.mode=none`（原厂默认）时未初始化 `ApplicationHelper.mPackageManager`，在 WiFi/应用状态广播回调时仍会输出轻量非阻塞日志 `E ApplicationHelper: Fail to get PackageManager`（耗时 `<0.02 ms`，不影响 UI 帧率与 CPU 空闲率）；后续若需彻底静默，可在 MTK 框架服务属性层评估统一收敛。
