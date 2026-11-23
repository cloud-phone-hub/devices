# #304 HMD Crest Max（`hmd/ARW`）启动、属性与相机全链修复：展锐 Radio 死锁 ANR、5 GHz Wi-Fi、基带/Model/Camera 属性补齐与原生相机五层崩溃打通

- **设备 / 机型**：HMD Crest Max（`hmd/ARW`，版本 `14-QP1A.190711.020-20250620`，Unisoc UMS9620 / T760-T770，GRF `system=A14` + `vendor=A13`，`lineage-20.0` + `kernel 5.15`）
- **日期**：2026-11-22 ~ 2026-11-23
- **状态**：已闭环

---

## 一、问题

`HMD Crest Max #1`（`ARW_14-QP1A.190711.020-20250620`）存在八项耦合的启动、属性与相机异常：

1. **SIM 恒空与 `com.android.phone` 循环死锁 ANR**：`gsm.sim.state` 恒为 `,ABSENT`，`mServiceState` 恒为 `OUT_OF_SERVICE`，设置「网络和互联网」里 SIM 卡灰显「没有 SIM 卡」，同时 `/data/anr/` 每约 6 分钟产生一次 `com.android.phone` ANR 并被系统强杀重启。
2. **5 GHz Wi-Fi 无法连通**：实测扫到 `5180 MHz` 假 AP `TP-LINK_2464` 后始终无法关联，设置显示「互联网未连接」。
3. **基带版本缺失**：`getprop gsm.version.baseband` 为 `,`，设置「关于手机 → Android 版本」基带版本显示异常。
4. **「关于手机」`Model` 与 `Camera` 规格空白**：设置页「型号」和「相机」栏均为空白。
5. **原生相机 `com.hmdglobal.app.camera` 无法打开/取景/拍照**：冷启动即触发五层连续故障——缺展锐 Vendor Tags → `cameraserver` `SIGSEGV` → `android.hardware.camera-service` `SIGABRT` → `com.hmdglobal.app.camera` 在官方 Mali OpenCL `clCreateContextFromType` 处 `SIGSEGV` → 多线程并发初始化时 `cdh_redirect` `mmap_read_trylock` 竞态致冷启动 4/5 偶发崩溃。
6. **冷启动停留在 `WelcomeSetupWizard` 且桌面偶现空壁纸**：首启未跳过 HMD 自带引导页 `com.hmdglobal.app.setupwizardext/.WelcomeSetupWizard`；且 `ro.surface_flinger.has_HDR_display` 未关闭导致 101010-2 / wide-gamut 协商异常与渲染告警。
7. **后台高频 AVC 刷屏**：`/system_ext/bin/tool_service` 以每秒 2 条频率触发 `hwservicemanager_prop` AVC 拒绝。
8. **Android 13 HAL 编译回归**：`CdhAllocator.h` 引入的 AIDL V2 方法 `allocate2` / `isSupported` 缺少 `ANDROID_API_LEVEL >= 34` 条件编译保护，导致 `android13` 硬件栈编译失败。

---

## 二、根因

1. **`com.android.phone` 展锐扩展 AIDL 双层死锁**：
   - `android.hardware.radio-service` 此前对展锐 6 个扩展接口（`IExtRadioSim`、`IExtRadioModem`、`IExtRadioNetwork`、`IExtRadioData`、`IExtRadioMessaging`、`IExtRadioVoice`）仅提供最小占位应答，未保存客户端通过 `setExtResponseFunctions` 注册的 `IExtRadio*Response` 远端代理，也未向异步请求发回响应回调。
   - HMD 扩展框架 `/system_ext/framework/unisoc-framework.jar` 中的 `HMDSimLockController.onRiServiceConnected()` 在 `com.android.phone` 主线程连续同步调用 `RadioInteractor.getSimlockWhitelist()`、`getSimlockStatus()` 与 `getSimlockDummys()`，经由 `RadioInteractorHandler.waitForResult()` 执行 `while (request.result == null) request.wait()`，无回调即永久阻塞主线程并阻断整个 `UiccController`/`SST` 初始化。
   - 进一步反汇编 `radio_interactor_service.apk`（`classes.dex`）证实两处展锐原厂缺陷：
     - `RadioResponse.responseInts` / `responseStrings` 未判空直接执行 `array-length`，回调传入 `null` 数组（`-1`）会触发 `NullPointerException` 导致 `IExtRadioSimResponse$Stub.onTransact` 丢弃回调；
     - `RadioInteractorHandler$SyncHandler.handleMessage` 在 `EVENT_GET_SIMLOCK_DUMMYS`、`EVENT_REQUEST_GET_CPCOMLOG_VERSION_DONE`、`EVENT_GET_VOLTE_ALLOWED_PLMN_DONE` 分支中，当 `ar.exception != null` 时竟向 `ThreadRequest.result` 写入 `null`（初始寄存器 `v1 = 0`）再调用 `notifyAll()`，主线程唤醒后仍满足 `result == null` 而继续永久死锁。
2. **5 GHz Wi-Fi 未同步 `#301` 补丁内核产物**：ARW 当前盘集运行的是 `#301`（[../../common/issue-hwsim-5g-cck-rate-all-kernels/solution.md](../../common/issue-hwsim-5g-cck-rate-all-kernels/solution.md)）之前的旧 5.15 内核与模块（5 GHz Beacon 携带 2.4G CCK Basic Rate `0x82`），需重编 5.15 内核与 `vendor_dlkm` 模块并重打盘集。
3. **基带与属性收口缺失**：`vendor/config.json` 未写入真机基带版本（官方 PAC 内 `SC9600_QogirN6Pro_PS_modem.bin` 的 `BASE Version` 为 `5G_MODEM_V2_23B_W24.26.4_P11`），且缺少 `ro.surface_flinger.has_HDR_display=false` 及纯 64 位属性 `ro.zygote=zygote64`、`ro.bionic.2nd_arch=""`、`ro.bionic.2nd_cpu_variant=""`。
4. **「关于手机」`Model` / `Camera` 依赖 `androidboot.*` 内核命令行**：
   - 反编译原厂 `Settings.apk` 证实：`Model` 栏读取 `ro.product.model.display`（由 `/system/etc/init/init.oneimage.rc` 在 `on post-fs` 从 `ro.boot.product.model.display` 复制；直接写入 `/vendor/build.prop` 会因 `SELinux` 拒绝设置系统前缀属性而静默失败）；`Camera` 栏由 `ro.boot.board_device` 映射板型（`ARP` 对应 `正面：50MP / 背面：64MP + 5MP + 2MP`）。
5. **原生相机五层崩溃根因**：
   - **层 1（缺展锐 / HMD 私有 Vendor Tags）**：`HMDCamera` 配置会话时查询 36 项 `com.addParameters.*`、`com.hmdglobal.parameters.*`、`com.sxscamera.*`、`com.sxs.supernight.*`、`com.awcamera.af.*`、`org.codeaurora.qcamera3.*`、`com.mediatek.*` 等私有 tag，缺失会阻塞预览会话建立。
   - **层 2（`cameraserver` `SIGSEGV`）**：反汇编 `/system_ext/lib64/libunisoccameraservice.so` 证实 `UnisocCamera3Device::getSlowMotion()` 硬编码查找 `tagId = 0x8000000A` 后未判空直接解引用 `entry.data.u8[0]`，导致 `cameraserver` 空指针崩溃。
   - **层 3（`android.hardware.camera-service` `SIGABRT`）**：`Note13CameraSession::processCaptureRequest -> importBuffer -> AHardwareBuffer_createFromHandle -> GraphicBufferMapper::GraphicBufferMapper()` 查询 `hal_graphics_mapper_hwservice` / `hal_graphics_allocator_service` 时被 `Enforcing` SELinux 拒绝（`avc: denied { find }`），报错 `gralloc-mapper is missing` 自杀。Android 13（`lineage-20.0`）基底未将 `hal_camera_default` 纳入 `halclientdomain` 与 `hal_graphics_allocator_client`。
   - **层 4（`com.hmdglobal.app.camera` 在真机 `libGLES_mali.so!clCreateContextFromType` 处 `SIGSEGV`）**：ARW 原先拷贝了官方悬空软链 `lib64/libOpenCL.so -> libOpenCL.so.1.2 -> libGLES_mali.so`，且 `5.15/cdh_redirect.ko` 仍是旧版未包含载体 `libOpenCL.so` 的 SP-HAL 白名单放行（`cdh_maps_has_carrier_opencl`），导致 `HMDCamera` 的 `libpixsmart_mf_base2.so` / `libpixsmart_mfsr.so` 直接落进官方 43 MB 真机 `libGLES_mali.so` 并在无 `/dev/mali0` 硬件时解引用空指针崩溃。
   - **层 5（`cdh_redirect` `mmap_read_trylock` 并发竞态致 4/5 偶发崩溃）**：`cdh_sphal_caller_allowed()` 仅执行 16 次 `cpu_relax()`（约数十纳秒）尝试获取 `mmap_read_trylock(mm)`；而 `HMDCamera` 冷启动时多个线程池并行 `dlopen` / `mmap` 算法库与图像缓冲，`mm->mmap_lock` 写锁高频争用，16 次自旋后返回 `false` 放行回官方 `libGLES_mali.so`，5 次冷启仅通过 1 次。
6. **HMD 首次向导未自动退出**：`cph-setup-display` 置位 `device_provisioned=1` 与 `user_setup_complete=1` 时，`com.google.android.setupwizard` 已启动 `com.hmdglobal.app.setupwizardext/.WelcomeSetupWizard` 占据 `type=home` 任务且不会监听设置自动退出。
7. **`/system_ext/bin/tool_service` 未屏蔽**：原厂展锐调试后台进程持续访问未授权属性。
8. **`CdhAllocator.h` API 守卫遗漏**：commit `63b1d3e` 新增的 `allocate2` 与 `isSupported` 落在 `#if ANDROID_API_LEVEL >= 34` 之外，在 API 33（Android 13）编不过。

---

## 三、证据

- **修前失败快照**：见 [log-fail.txt](log-fail.txt)
  - `gsm.version.baseband` 为 `,`（见 [fail-baseband.png](fail-baseband.png)）；
  - `gsm.sim.state` 为 `,ABSENT`，`mServiceState` 为 `OUT_OF_SERVICE`，互联网未连接、SIM 卡灰显（见 [fail-network.png](fail-network.png)）；
  - `/data/anr/` 连续产生 4 份 ANR，主线程死锁栈：
    `RadioInteractorHandler.waitForResult(RadioInteractorHandler.java:2067) <- getSimlockWhitelist <- HMDSimLockController.onRiServiceConnected(HMDSimLockController.java:226)`。
- **字节码与二进制逆向证据**：
  - `RadioResponse.responseInts` 偏移 `0x02ab08`：`array-length v2, v6` 无判空；
  - `RadioInteractorHandler$SyncHandler` 偏移 `0x0233f2 ~ 0x023410`：`if-nez v2, 0242` 后执行 `iput-object v1, v3, ThreadRequest.result`（`v1=null`），配合 `waitForResult` 的 `if-nez v0` 死循环导致永久阻塞；
  - `libunisoccameraservice.so` `UnisocCamera3Device::getSlowMotion()` 查找 `0x8000000A` 未判空直接 `ldrb w0, [x8]`；
  - `android.hardware.camera-service` `avc: denied { find } for interface=android.hardware.graphics.mapper::IMapper scontext=u:r:hal_camera_default:s0` + `Abort message: 'gralloc-mapper is missing'`；
  - `HMDCamera` `libpixsmart_mf_base2.so` / `libpixsmart_mfsr.so` 崩在 `/vendor/lib64/libGLES_mali.so (clCreateContextFromType+200)`；压测 5 次冷启动仅 1/5 通过，加日志实证 `cdh_redirect: sphal trylock timeout` 命中。
- **修后验收日志与实屏截图**：见 [log-pass.txt](log-pass.txt)、[pass-home.png](pass-home.png)、[pass-network.png](pass-network.png)、[pass-about.png](pass-about.png)、[pass-camera.png](pass-camera.png)。

---

## 四、改动

1. **`devices/hardware/android.hardware.radio/src/main.cpp`**：
   - 为展锐 6 个扩展接口实现 `UnisocExtPlaceholderState` 与 `OnUnisocExtTransact`：
     - 处理 `setExtResponseFunctions`（`IExtRadioModem` 为 tx=4，其余 5 个为 tx=2），通过 `AParcel_readStrongBinder` + `AIBinder_associateClass` 持久化 `IExtRadio*Response` 远端代理；
     - 对已知异步请求读取 `serial` 并以 `FLAG_ONEWAY` 回调 `ExtRadioResponseInfo{nonNull=1, size=16, type=0, serial, error}` 与完整尾部参数；
     - 将数组类型回调统一编码为非空 0 长度数组（`UnisocRespTail::kEmptyArray` 写 `0` 而非 `-1`），规避 `RadioResponse.responseInts`/`responseStrings` 空指针崩溃；
     - 对 `SyncHandler` 遇错写 `null` 会死锁的 3 项只读查询（`getSimlockDummys`、`getCpComLogVersion`、`getVoLTEAllowedPLMN`）回报 `error = 0 (NONE)` 与空安全默认值，其余保持 `6 (REQUEST_NOT_SUPPORTED)`。
2. **`devices/hardware/android.hardware.graphics.allocator/src/CdhAllocator.h`**：
   - 将 AIDL V2 方法 `allocate2` 与 `isSupported` 纳入 `#if !defined(ANDROID_API_LEVEL) || ANDROID_API_LEVEL >= 34` 条件编译块，修复 Android 13（API 33）编译。
3. **`devices/hardware/android.service.daemon/vendor/bin/cph-setup-display`**：
   - 仅当检测到 `com.hmdglobal.app.setupwizardext` 存在时（零影响非 HMD 机型），在置位 `device_provisioned=1` / `user_setup_complete=1` 后先 `am force-stop` 再 `pm disable` 并二次 `am force-stop` 停止 `com.hmdglobal.app.setupwizardext` 与 `com.google.android.setupwizard`，首启时幂等拉起 `HOME` 桌面。
4. **`devices/manifest/configs/hmd/ARW/ARW-android13/vendor_boot/config.json`**：
   - `cmdline` 追加 `androidboot.board_device=ARP` 与 `androidboot.product.model.display=TA-1635`，由系统原厂 `init.oneimage.rc` 自动填充 `ro.product.model.display` 与相机规格。
5. **原生相机全链修复（5 处）**：
   - **`devices/hardware/android.hardware.camera/src/camera_vendor_tags.inc`**：补齐 36 项展锐 / HMD 私有相机 Vendor Tags（`0xC2000000` ~ `0xC20B0000`）。
   - **`devices/hardware/android.hardware.camera/src/camera_provider_service.cpp`**：在 `buildRequestMetadata()` 中对 `ro.hardware.camera == "unisoc"` 预置 `tagId = 0x8000000A`（`int32_t{0}`），消除 `libunisoccameraservice.so!getSlowMotion` 空指针解引用。
   - **`devices/hardware/android.hardware.camera/sepolicy/vendor_sepolicy.cil`**：按 AOSP `hal_client_domain(hal_camera_default, hal_graphics_allocator)` 追加 `(typeattributeset halclientdomain (hal_camera_default))` 与 `(typeattributeset hal_graphics_allocator_client (hal_camera_default))`，赋予 `hal_camera_default` 访问 `hal_graphics_mapper_hwservice` / `hal_graphics_allocator_service` / `same_process_hal_file` 权限且不触碰 `neverallow`。
   - **`devices/manifest/configs/hmd/ARW/ARW-android13/vendor/lib64/libOpenCL.so`（+ `.1`/`.1.1`/`.1.2` 软链）与 `vendor/config.json`**：移除官方悬空软链 `lib64/libOpenCL.so`，部署 `cdh_opencl_stub` 安全桩库并标注 `u:object_r:same_process_hal_file:s0`，使 `HMDCamera` 的 HDR / MFSR 算法库探测 OpenCL 时安全回退 CPU。
   - **`devices/modules/cdh_redirect/core/rules.c`**：在 `cdh_sphal_caller_allowed()` 中新增 `(tgid, start_time, mm)` 维度的 `linker64` VMA 范围缓存快路径（仅放行 `CDH_SPHAL_TIER_B` / `CDH_SPHAL_VNDK_FALLBACK`），并将 `mmap_read_trylock(mm)` 重试拓宽至 `32 x cpu_relax() + 50 x udelay(10)`（最多 500 µs），根除多线程并发加载 `.so` 时的抢锁竞态。
6. **`devices/manifest/configs/hmd/ARW/ARW-android13/vendor/config.json` 与 `module/config.json`**：
   - 补齐真机基带 `"gsm.version.baseband": "5G_MODEM_V2_23B_W24.26.4_P11"`，关闭 `"ro.surface_flinger.has_HDR_display": "false"`，补齐纯 64 位收口属性 `"ro.zygote": "zygote64"`、`"ro.bionic.2nd_arch": ""`、`"ro.bionic.2nd_cpu_variant": ""`；`module/config.json` `redirect` 追加屏蔽 `/system_ext/bin/tool_service`（19/32 条，遵循 `AGENTS.md §8` 不改官方镜像）。
7. **全链重编与发布**：重编 5.15 内核（合入 `#301` 5 GHz OFDM 补丁）、重编 `5.15` 模块（含新版 `cdh_redirect.ko`）及 `android13` 硬件栈，最终发布盘集（`vda.img` + `patch.img` sha256 `8b777c0cc5c0`）至 `out/devices/dist/hmd/ARW/14-QP1A.190711.020-20250620/`。

---

## 五、验证

在 `HMD Crest Max #1`（`ARW_14-QP1A.190711.020-20250620/1`，ADB `127.0.0.1:5919`，VNC `127.0.0.1:5920`）上完成全项实测（详见 [log-pass.txt](log-pass.txt)）：

- **引导与安全**：`sys.boot_completed=1`、`getenforce=Enforcing`、`anr_events=0`、`crashes_since_boot=0`、`cdh_redirect: 载体 OpenCL 桩库就绪：mask=0x1`。
- **电话栈、SIM 与基带**：`gsm.sim.state=LOADED,ABSENT`、`gsm.operator.alpha=中国联通,`、`gsm.network.type=LTE,Unknown`、`mVoiceRegState=0(IN_SERVICE)`、`mDataRegState=0(IN_SERVICE)`、`mDataConnectionState=2(CONNECTED)`、`ccmni2` 获配 `100.64.155.15/24`、`gsm.version.baseband=5G_MODEM_V2_23B_W24.26.4_P11,5G_MODEM_V2_23B_W24.26.4_P11`。
- **5 GHz Wi-Fi**：`wlan0` 获配 `192.168.60.100/24`，自动连通 `5180 MHz` `"TP-LINK_2464"`（`Supplicant state: COMPLETED`、`score: 60`、`isUsable: true`）。
- **关于手机 Model & Camera 属性**：`ro.product.model.display=TA-1635`、`ro.boot.board_device=ARP`，实屏 [pass-about.png](pass-about.png) 清晰展示 **`型号: TA-1635`** 与 **`相机: 正面：50MP / 背面：64MP + 5MP + 2MP`**。
- **原生相机 5/5 压测与拍照落盘**：
  - 连续 5 次 `am force-stop` + 冷启 `com.hmdglobal.app.camera/.CameraActivity` **5/5（100%）通过**（PID `4388 / 5611 / 6620 / 7313 / 7766`）；
  - 实时取景流畅出帧，点击快门成功落盘 2 张 HDR 照片（`IMG_20261122_235233_HDR.jpg` 257 KB、`IMG_20261123_000122_HDR.jpg` 264 KB），前后摄切换无崩溃；
  - `android.hardware.camera-service`（PID `429`）、`cameraserver`（PID `554`）、`com.hmdglobal.app.camera`（PID `7766`）全程稳定 0 崩溃；
  - 人工审图 [pass-camera.png](pass-camera.png)：取景器画面、HDR/缩放/模式控件及右下角实拍缩略图均清晰正常。
- **人工桌面与网络审图**：[pass-home.png](pass-home.png)（完整中文桌面 + 满格 Wi-Fi 与蜂窝信号）与 [pass-network.png](pass-network.png) 均通过。

---

## 六、遗留

- `com.android.chrome` 为官方 32 位 `libmonochrome.so` 预装包，在纯 64 位云机上属既定预期行为（同 `#298` [../../common/issue-full-fleet-hal-backfill-and-health-audit/solution.md](../../common/issue-full-fleet-hal-backfill-and-health-audit/solution.md) 口径），本次按约定不改动。
- 首次全新冷启动（`/data` 初建）在联通 SIM 46001 加载触发 `en_GB → zh_CN` 语言切换瞬间，系统仍会弹出一次原厂「电池保护」提示卡片；二次启动（语言已固化）及后续正常使用中桌面零弹窗直达且完全稳定。
