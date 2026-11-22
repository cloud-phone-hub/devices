# #304 HMD Crest Max（`hmd/ARW`）启动卡点修复：展锐扩展 Radio 主线程死锁 ANR、5 GHz Wi-Fi 断联、基带缺失、SetupWizard 拦截与 A13 Allocator 编译兼容

- **设备 / 机型**：HMD Crest Max（`hmd/ARW`，版本 `14-QP1A.190711.020-20250620`，Unisoc UMS9620 / T760-T770，GRF `system=A14` + `vendor=A13`，`lineage-20.0` + `kernel 5.15`）
- **日期**：2026-11-22
- **状态**：已闭环

---

## 一、问题

`HMD Crest Max #1`（`ARW_14-QP1A.190711.020-20250620`）存在六项耦合的启动与运行异常：

1. **SIM 恒空与 `com.android.phone` 循环死锁 ANR**：`gsm.sim.state` 恒为 `,ABSENT`，`mServiceState` 恒为 `OUT_OF_SERVICE`，设置「网络和互联网」里 SIM 卡灰显「没有 SIM 卡」，同时 `/data/anr/` 每约 6 分钟产生一次 `com.android.phone` ANR 并被系统强杀重启。
2. **5 GHz Wi-Fi 无法连通**：实测扫到 `5180 MHz` 假 AP `TP-LINK_2464` 后始终无法关联，设置显示「互联网未连接」。
3. **基带版本缺失**：`getprop gsm.version.baseband` 为 `,`，设置「关于手机 → Android 版本」基带版本显示异常。
4. **冷启动停留在 `WelcomeSetupWizard` 且桌面偶现空壁纸**：首启未跳过 HMD 自带引导页 `com.hmdglobal.app.setupwizardext/.WelcomeSetupWizard`；且 `ro.surface_flinger.has_HDR_display` 未关闭导致 101010-2 / wide-gamut 协商异常与渲染告警。
5. **后台高频 AVC 刷屏**：`/system_ext/bin/tool_service` 以每秒 2 条频率触发 `hwservicemanager_prop` AVC 拒绝。
6. **Android 13 HAL 编译回归**：`CdhAllocator.h` 引入的 AIDL V2 方法 `allocate2` / `isSupported` 缺少 `ANDROID_API_LEVEL >= 34` 条件编译保护，导致 `android13` 硬件栈编译失败。

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
4. **HMD 首次向导未自动退出**：`cph-setup-display` 置位 `device_provisioned=1` 与 `user_setup_complete=1` 时，`com.google.android.setupwizard` 已启动 `com.hmdglobal.app.setupwizardext/.WelcomeSetupWizard` 占据 `type=home` 任务且不会监听设置自动退出。
5. **`/system_ext/bin/tool_service` 未屏蔽**：原厂展锐调试后台进程持续访问未授权属性。
6. **`CdhAllocator.h` API 守卫遗漏**：commit `63b1d3e` 新增的 `allocate2` 与 `isSupported` 落在 `#if ANDROID_API_LEVEL >= 34` 之外，在 API 33（Android 13）编不过。

---

## 三、证据

- **修前失败快照**：见 [log-fail.txt](log-fail.txt)
  - `gsm.version.baseband` 为 `,`（见 [fail-baseband.png](fail-baseband.png)）；
  - `gsm.sim.state` 为 `,ABSENT`，`mServiceState` 为 `OUT_OF_SERVICE`，互联网未连接、SIM 卡灰显（见 [fail-network.png](fail-network.png)）；
  - `/data/anr/` 连续产生 4 份 ANR，主线程死锁栈：
    `RadioInteractorHandler.waitForResult(RadioInteractorHandler.java:2067) <- getSimlockWhitelist <- HMDSimLockController.onRiServiceConnected(HMDSimLockController.java:226)`。
- **字节码逆向证据**：
  - `RadioResponse.responseInts` 偏移 `0x02ab08`：`array-length v2, v6` 无判空；
  - `RadioInteractorHandler$SyncHandler` 偏移 `0x0233f2 ~ 0x023410`：`if-nez v2, 0242` 后执行 `iput-object v1, v3, ThreadRequest.result`（`v1=null`），配合 `waitForResult` 的 `if-nez v0` 死循环导致永久阻塞。
- **修后验收日志与实屏截图**：见 [log-pass.txt](log-pass.txt)、[pass-home.png](pass-home.png)、[pass-network.png](pass-network.png)。

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
4. **`devices/manifest/configs/hmd/ARW/ARW-android13/vendor/config.json`**：
   - 补齐真机基带 `"gsm.version.baseband": "5G_MODEM_V2_23B_W24.26.4_P11"`，关闭 `"ro.surface_flinger.has_HDR_display": "false"`，补齐纯 64 位收口属性 `"ro.zygote": "zygote64"`、`"ro.bionic.2nd_arch": ""`、`"ro.bionic.2nd_cpu_variant": ""`。
5. **`devices/manifest/configs/hmd/ARW/ARW-android13/module/config.json`**：
   - `redirect` 数组追加屏蔽 `/system_ext/bin/tool_service`（19/32 条，遵循 `AGENTS.md §8` 不改官方镜像）。
6. **全链重编与发布**：执行 `cdh-pipeline device build ARW -manifest 14-QP1A.190711.020-20250620 -refresh -jobs 2`（含重编 5.15 内核吸收 `#301` 5 GHz OFDM 补丁与全部 10 个 `vermagic=5.15.151-4k-cdh` 模块），随后增量重编产出最终 `patch.img`（sha256 `2783574327d1`）发布至 `out/devices/dist/hmd/ARW/14-QP1A.190711.020-20250620/`。

---

## 五、验证

在重建的全新实例 `HMD Crest Max #1`（`ARW_14-QP1A.190711.020-20250620/1`，ADB `127.0.0.1:5919`，VNC `127.0.0.1:5920`）上完成**全新冷启动**与 **stop → start 二次重启**双轮零干预实测（详见 [log-pass.txt](log-pass.txt)）：

- **引导与安全**：`sys.boot_completed=1`、`getenforce=Enforcing`、`restarting_services=0`、`CONFIG_PPP_MPPE=y`、`CONFIG_CRYPTO_LIB_ARC4=y`、`device_provisioned=1`、`user_setup_complete=1`。
- **电话栈与 SIM**：`[3656]< GET_SIMLOCK_DUMMYS {} [SUB0]` 1 ms 即刻返回；`gsm.sim.state=LOADED,ABSENT`、`gsm.operator.alpha=中国联通,`、`gsm.operator.numeric=46001,`、`gsm.network.type=LTE,Unknown`、`mVoiceRegState=0(IN_SERVICE)`、`mDataRegState=0(IN_SERVICE)`、`mDataConnectionState=2(CONNECTED)`、`ccmni2` 获配 `100.64.155.15/24`。
- **基带版本**：`gsm.version.baseband=5G_MODEM_V2_23B_W24.26.4_P11,5G_MODEM_V2_23B_W24.26.4_P11`。
- **5 GHz Wi-Fi**：`wlan0` 状态 `UP`，获配 `192.168.60.100/24`，自动连通 `5180 MHz` `"TP-LINK_2464"`（`Supplicant state: COMPLETED`、`score: 60`、`isUsable: true`）。
- **稳定性（二次重启零干预验收）**：`/data/anr/` 为空、`anr_events=0`、`non_chrome_crashes=0`、`dri_image_err=0`、`tool_service` 持续刷屏归零（仅开机 init 屏蔽探测 2 条静态日志）。
- **人工 VNC 审图**：
  - [pass-home.png](pass-home.png)：零干预直达完整中文桌面，天气小组件、桌面与 Dock 图标、搜索栏、导航栏、满格 Wi-Fi 与蜂窝信号图标全部清晰渲染，无任何弹窗；
  - [pass-network.png](pass-network.png)：「网络和互联网」清晰显示「互联网 · `TP-LINK_2464`」、「通话和短信 · 中国联通」、「SIM 卡 · 中国联通」。

---

## 六、遗留

- `com.android.chrome` 为官方 32 位 `libmonochrome.so` 预装包，在纯 64 位云机上属既定预期行为（同 `#298` [../../common/issue-full-fleet-hal-backfill-and-health-audit/solution.md](../../common/issue-full-fleet-hal-backfill-and-health-audit/solution.md) 口径），本次按约定不改动。
- 首次全新冷启动（`/data` 初建）在联通 SIM 46001 加载触发 `en_GB → zh_CN` 语言切换瞬间，系统仍会弹出一次原厂「电池保护」提示卡片，且 `SearchLauncher` 在首次语言热切换重绘期间偶发旧图标缓冲缺失；二次启动（语言已固化）及后续正常使用中 `dri_image_err=0`、桌面零弹窗直达且完全稳定。
