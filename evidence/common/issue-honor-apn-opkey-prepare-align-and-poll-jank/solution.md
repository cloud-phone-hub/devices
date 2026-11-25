# #335：荣耀 MagicOS `opkey`/APN 双 PDN 打通、全仓 `prepare` 设备信息对齐（44 盘集同步）与宿主轮询/`VipSchedProxy` 掉帧根治

- **设备 / 机型**：跨机型引擎共性（首发并重点验证于 `honor/sun` `Honor Magic7 RSR #1` `16-AQ3A.250214.001-20260904`，同步覆盖 `redmi/gold` `15-OS3.0.11.0.VNQCNXM`、`blu/G1170` `15-AP3A.240905.015.A2-20260209` 等全仓 36 个带 `device-profile.json` 机型及 44 个非运行 `dist` 盘集）
- **阶段 / 模块**：`devices/hardware/android.service.daemon`（`cph-ims-netmgr`）、`devices/modules/device_info`（`sched_proc.c`）、`agent/modules/daemon/run`（`instance_config.go`、`qemu.go`）、`agent/modules/pipeline/stages/02_prepare`（`apply_profile_configs.go`）、`devices/manifest/configs/honor/sun/sun-android15`

---

## 一、问题

在 `#334` 完成 QEMU VNC `TightJPEG` 硬解投屏改造后，对 `Honor Magic7 RSR #1` 及全仓非运行实例镜像继续深入对齐 `02_prepare` 设备信息并进入 `Settings` 逐项巡检 `About phone`、`APNs`、`Wi-Fi`、`Mobile data / Radio` 及分析滑动掉帧异常日志时，暴露五类关键问题：

1. **荣耀 MagicOS `Settings → APNs` 锁死且蜂窝双 PDN 断联**：
   - 进入 `Settings → Mobile network → Mobile data → Access Point Names (APNs)` 直接显示白屏提示：`"Access Point Name settings are not available for this user"`，右上角无新建/重置入口；
   - `ccmni0`（IMS）与 `ccmni1`（Internet）均处于 `DOWN` 未激活态，`dumpsys phone` 显示 `DataNetworkController` 因 `DATA_CONFIG_NOT_READY` 拒绝建链。
2. **滑动期间内核与 HAL 缺失节点高频刷屏拖慢 UI 主线程**：
   - 每次手指触摸滑动 `Settings` 列表，荣耀 UI 线程（`RenderThread` / 主线程）内的 `VipSchedProxy` 与 `FASProxy` 每帧反复尝试 `open("/proc/vip/ioctl")` 与 `open("/proc/fas/ioctl")`，2.5 秒滑动即刷出 **1,525+ 条** `E VipSchedProxy: Open failed: /proc/vip/ioctl`；
   - 同时伴随 SELinux `service_manager find`（`vendor_hal_perf_hwservice`、`hal_uniperf_service`、`hal_hndisplayengine_hwservice`）审计风暴（累计 `audit_lost=355,679`）以及 `vendor.honor.hardware.hwfactoryinterface-service` 报错 `dlopen lib_factory_interface.so is nullptr`。
3. **宿主 `daemon` 8 秒状态轮询引发客机周期性 GC 卡顿与 Binder 断管报错**：
   - Web 工作台每 8 秒请求 `/api/instance/config`，`daemon/run/instance_config.go` 每次在客机执行全量探测脚本，其中包含无参 `dumpsys nfc`、`pm list features`、`pm list packages` 及 `dumpsys ... | sed -n '...q'`；
   - 无参 `dumpsys nfc` 会触发 `com.android.nfc` 执行 `Unreachable memory` 全堆扫描（`libmemunreachable` 挂起线程遍历堆），每 8 秒在客机引发一次 **130~260ms** 的 `ART-GC CollectorTransition marksweep + semispace` 停顿；而 `sed '...q'` 提前关闭管道导致 `dumpsys wifi`/`connectivity`/`telephony.registry` 收到 `SIGPIPE` 退出，内核持续刷出 `binder: 2480:3013 transaction failed 29189/-22` / `reply target not found`。
4. **LTPO 旗舰屏 `1Hz` 档位导致 QEMU `virtio-gpu` EDID 校验失败无法起机**：
   - 全量对齐 `02_prepare` 的 `display.refresh-rates` 后，支持 LTPO `1~120Hz` 的机型（`honor/sun`、`meizu/Meizu22`、`vivo/PD2454`、`xiaomi/popsicle`）将 `[1, 60, 90, 120]` 写入 `device.json`，QEMU 启动立即报错退出：`virtio-gpu-pci: invalid refresh-rates '1:60:90:120': expected 1 to 8 unique integer values between 24 and 240 Hz`。
5. **存量非运行盘集未同步最新 `prepare` 设备画像与系统服务补丁**：
   - 全仓 36 个已有 `device-profile.json` 的机型及 `out/devices/dist/` 下 44 个非运行盘集尚未统一同步市场机型名（`ro.vendor.cdh.device.market_name`）、基带版本（`gsm.version.baseband`）、合法刷新率与新版 `cph-ims-netmgr` / `cdh_device_info.ko`。

---

## 二、根因

1. **荣耀 `HwCfgFilePolicy` `opkey` 门控 + 海外版无明文 `apns-conf.xml` + `app_process` 外部 Provider 权限约束**：
   - 反编译荣耀 `/system/framework/honor-telephony-common.jar` 与 `/system/priv-app/TelephonyProvider/TelephonyProvider.apk` 查明：
     - `CarrierConfigLoader.isConnectCarrierService()` 读取 `HwCfgFilePolicy.getOpKey(slotId)`（底层读 `persist.sys.opkey<slot>`）；当 `opkey` 为 `null` 时拒绝绑定 `CarrierConfigApp` 更新配置，使 `carrier_config_applied_bool` 恒为 `false`——这既触发 `DataNetworkController` 的 `DATA_CONFIG_NOT_READY`，又使 `ApnSettings` 判定当前用户无权编辑 APN；
     - `TpDatabaseHelperHn` / `TpProviderHn` 将数据库扩展了 `opKey` 列：在 `persist.sys.opkey0` 非空时，`query("content://telephony/carriers/subId/<subId>")` 会追加 `opKey = '<opkey>' OR opKey = ''` 过滤，而向 `carriers/subId/<subId>` 插入行时 `TpProviderHn.addOpKeyValue()` 会自动补齐当前槽位的 `opKey`；
   - 此外，荣耀海外版固件（`PTP-N59`）不提供明文 `/system/etc/apns-conf.xml`（运营商参数在加密二进制块中），只能由自研 `cph-ims-netmgr` 预置 `46000/46001/46003/310260` 标准 APN；但 `cph_ims_netmgr` 原以 `user radio`（`uid=1001`）通过 `app_process` 启动：
     - `app_process` 无 `ActivityManagerService` 注册的 `ProcessRecord`，调用 `IActivityManager.getContentProvider(null, ...)` 必抛 `SecurityException: Isolated process not allowed to call getContentProvider (caller=null)`；
     - 改调 `getContentProviderExternal("telephony", ...)` 则在 Android 14+ 强检 `ACCESS_CONTENT_PROVIDERS_EXTERNALLY` 权限，该权限仅授予 `shell (2000)` / `root (0)` / `system (1000)`，未授予 `radio (1001)`。
2. **`/proc/vip/ioctl` 与 `/proc/fas/ioctl` 节点缺失及 `sun` 工厂库跨命名空间依赖**：
   - 荣耀 MagicOS 的帧感知调度（`FASProxy`）与 VIP 线程调度（`VipSchedProxy`）未做负缓存，节点不存在时每次 UI 交互都会反复 `open` 失败并打错误日志；
   - `vendor.honor.hardware.hwfactoryinterface-service` 除 `lib_factory_interface.so` 外，其依赖链中的 `liboeminfo.so`、`libhiview.so`、`libjsoncpp.so`、`libhitrace.so`、`libhilog.so` 属于非 LLNDK 库，vendor 链接器命名空间禁止回退加载 `/system/lib64/` 同名库，必须从官方 `unpack_vendor/lib64/` 补入 `/vendor/lib64/`。
3. **宿主 `instance_config.go` 无分级缓存与管道截断**：
   - `nfc` 在 `dumpsys nfc` 未传 `--short` 时默认调用 `NfcDiagnostics` / `UnreachableMemory` 扫描 native 堆；`service check nfc` + 读取持久属性即可无损获知服务存活与开关态，无需唤醒 `com.android.nfc` 做堆扫描；
   - `dumpsys <svc> | sed -n '1,120p'` 在输出超过 120 行时立刻关闭读端管道，导致 Binder 服务端写 fd 失败与 `dumpsys` 进程未收割完 Binder 应答就退出。
4. **QEMU EDID 刷新率区间限制**：
   - QEMU `hw/display/virtio-gpu-base.c` 硬性校验 `refresh_rates` 每个元素满足 `24 <= rate <= 240` 且至多 8 个唯一值；`02_prepare` 与 `daemon/run/qemu.go` 原先只判断 `r > 0`。

---

## 三、证据

- 修复前异常现象与关键日志见同目录 [log-fail.txt](file:///Volumes/work/cloud-phone-hub/devices/evidence/common/issue-honor-apn-opkey-prepare-align-and-poll-jank/log-fail.txt)。
- 修复后跨机型实测指标与日志见同目录 [log-pass.txt](file:///Volumes/work/cloud-phone-hub/devices/evidence/common/issue-honor-apn-opkey-prepare-align-and-poll-jank/log-pass.txt)。
- 实机 `Settings` 巡检截图（均保存在本固证目录）：
  - `Honor Magic7 RSR #1`：
    - `About phone`（上半屏机型名/SoC/RAM/分辨率）：[sun-settings-about.png](file:///Volumes/work/cloud-phone-hub/devices/evidence/common/issue-honor-apn-opkey-prepare-align-and-poll-jank/sun-settings-about.png)
    - `About phone`（下半屏双 IMEI / 完整基带版本 / 内核版本）：[sun-settings-about-storage.png](file:///Volumes/work/cloud-phone-hub/devices/evidence/common/issue-honor-apn-opkey-prepare-align-and-poll-jank/sun-settings-about-storage.png)
    - `APNs`（`中国联通 3gnet` + `3gwap` 正常展示与选中）：[sun-settings-apn.png](file:///Volumes/work/cloud-phone-hub/devices/evidence/common/issue-honor-apn-opkey-prepare-align-and-poll-jank/sun-settings-apn.png)
    - `Wi-Fi`（已连接 `TP-LINK_5G_C390`）：[sun-settings-wifi.png](file:///Volumes/work/cloud-phone-hub/devices/evidence/common/issue-honor-apn-opkey-prepare-align-and-poll-jank/sun-settings-wifi.png)
    - `Mobile network / Radio`（SIM 1 中国联通 4G 在线、移动数据开启）：[sun-settings-radio.png](file:///Volumes/work/cloud-phone-hub/devices/evidence/common/issue-honor-apn-opkey-prepare-align-and-poll-jank/sun-settings-radio.png)
  - 实例之外的其他镜像抽检：
    - `Redmi Note 13 5G #1`（`gold_15-OS3.0.11.0.VNQCNXM`，本地管线重打代表）：[gold-settings-about.png](file:///Volumes/work/cloud-phone-hub/devices/evidence/common/issue-honor-apn-opkey-prepare-align-and-poll-jank/gold-settings-about.png)、[gold-settings-apn.png](file:///Volumes/work/cloud-phone-hub/devices/evidence/common/issue-honor-apn-opkey-prepare-align-and-poll-jank/gold-settings-apn.png)
    - `BLU G75 #1`（`G1170_15-AP3A.240905.015.A2-20260209`，远端同步盘集增量刷新代表）：[g1170-settings-about.png](file:///Volumes/work/cloud-phone-hub/devices/evidence/common/issue-honor-apn-opkey-prepare-align-and-poll-jank/g1170-settings-about.png)、[g1170-settings-apn.png](file:///Volumes/work/cloud-phone-hub/devices/evidence/common/issue-honor-apn-opkey-prepare-align-and-poll-jank/g1170-settings-apn.png)

---

## 四、改动

1. **荣耀/华为 `opkey` + `CarrierConfig` + 跨全机型 APN 外部 Provider 写入闭环**：
   - [cph_ims_netmgr.rc](file:///Volumes/work/cloud-phone-hub/devices/hardware/android.service.daemon/vendor/etc/init/cph_ims_netmgr.rc)：将 `user radio` 升级为 `user system`、`group system radio inet`（`UID 1000` 同时具备 `ACCESS_CONTENT_PROVIDERS_EXTERNALLY`、`WRITE_APN_SETTINGS`、`MODIFY_PHONE_STATE` 与 `CONNECTIVITY_USE_RESTRICTED_NETWORKS`）；
   - [ImsNetAgent.java](file:///Volumes/work/cloud-phone-hub/devices/hardware/android.service.daemon/src/ims_netmgr/ImsNetAgent.java) 及产物 [cph-ims-netmgr.jar](file:///Volumes/work/cloud-phone-hub/devices/hardware/android.service.daemon/vendor/etc/cph-ims-netmgr.jar)：
     - 新增 `isHonorOrHuaweiOpKeyRom()` 与 `ensureHonorOpKeyAndCarrierConfig(subId, numeric)`：检测到荣耀/华为 ROM 且 SIM `numeric` 就绪后，幂等设置 `persist.sys.opkey<slot>=<numeric>` 与 `ril.hw_sim_opkey<slot>=<numeric>`，并通过反射调用 `ICarrierConfigLoader.updateConfigForPhoneId(slot, "LOADED")`（回退 `notifyConfigChangedForSubId`）驱动 `carrier_config_applied_bool=true`；
     - `acquireTelephonyProvider()` 在 `system/root/shell` UID 下优先走 `IActivityManager.getContentProviderExternal("telephony", 0, token, "*cmd*")`；荣耀机型优先通过 `content://telephony/carriers/subId/<subId>` 查询与插入预置 APN（自动填充 `opKey` 列），插入完成后调用 `notifyChange(CARRIERS_URI)` 立即唤醒 `DataProfileManager` 拉起 `ccmni1`（`default`）与 `ccmni0`（`ims`）。
2. **`cdh_device_info.ko` 虚拟调度探针节点（5.15 / 6.1 / 6.6 / 6.12 全量重编）**：
   - 新增 [sched_proc.c](file:///Volumes/work/cloud-phone-hub/devices/modules/device_info/sched/sched_proc.c)，并在 [cdh_device_info.h](file:///Volumes/work/cloud-phone-hub/devices/modules/device_info/include/cdh_device_info.h)、[Kbuild](file:///Volumes/work/cloud-phone-hub/devices/modules/device_info/Kbuild)、[main.c](file:///Volumes/work/cloud-phone-hub/devices/modules/device_info/core/main.c) 接入：创建 `/proc/vip/ioctl` 与 `/proc/fas/ioctl`（`0666`，自动匹配平台 `genfscon` 的 `proc_vip_ioctl` / `proc_fas` 标签），对读方向 `_IOC_READ` 清零用户缓冲后返回 `0`，彻底消除 `VipSchedProxy` / `FASProxy` 热路径 `open` 失败刷屏。
3. **荣耀 `sun` 工厂库依赖链与 SELinux 审计阻塞补齐**：
   - [sun-android15/vendor/config.json](file:///Volumes/work/cloud-phone-hub/devices/manifest/configs/honor/sun/sun-android15/vendor/config.json)：补齐官方 `unpack_vendor/lib64/` 下的 `lib_factory_interface.so`、`lib_mmi_interface.so`、`libfaccommon_share.so`、`libfacsignedapi_share.so`、`libqmi_{nv_api,oem_api,cci,common_so,encdec}.so` 及同命名空间依赖 `lib{oeminfo,hiview,jsoncpp,hitrace,hilog}.so`；
   - [sun-android15/vendor/sepolicy/vendor_sepolicy.cil](file:///Volumes/work/cloud-phone-hub/devices/manifest/configs/honor/sun/sun-android15/vendor/sepolicy/vendor_sepolicy.cil) 与 [android.hardware.radio/sepolicy/vendor_sepolicy.cil](file:///Volumes/work/cloud-phone-hub/devices/hardware/android.hardware.radio/sepolicy/vendor_sepolicy.cil)：对齐官方策略补齐 `vendor_hal_perf_client`、`hal_hndisplayengine_client`、`hal_uniperf_aidl_client` 属性成员，并为 `hal_radio_default` 增加 `serialno_prop` `dontaudit`。
4. **宿主 `daemon` 状态轮询减负与 LTPO EDID 刷新率双重收敛**：
   - [instance_config.go](file:///Volumes/work/cloud-phone-hub/agent/modules/daemon/run/instance_config.go) & [instance_config_test.go](file:///Volumes/work/cloud-phone-hub/agent/modules/daemon/run/instance_config_test.go)：拆分为高频轻量脚本 `guestFastProbeScript` 与 5 分钟 TTL 缓存脚本 `guestStaticProbeScript`；彻底移除无参 `dumpsys nfc`（改用 `service check nfc` + 属性读取），并用读尽 EOF 的 `awk` 替代所有 `sed -n '...q'` 消除 `SIGPIPE` / `binder: reply target not found`；
   - [qemu.go](file:///Volumes/work/cloud-phone-hub/agent/modules/daemon/run/qemu.go)、[qemu_display_test.go](file:///Volumes/work/cloud-phone-hub/agent/modules/daemon/run/qemu_display_test.go) & [apply_profile_configs.go](file:///Volumes/work/cloud-phone-hub/agent/modules/pipeline/stages/02_prepare/apply_profile_configs.go)：新增 `sanitizeEDIDRefreshRates` 将 EDID `refresh-rates` 严格限制在 `[24, 240]` Hz（至多 8 个唯一整数），重编 `out/agent/dist/{daemon,cdha,pipeline}`。
5. **全仓 36 个机型 `apply-profile` 对齐与 44 个非运行 `dist` 盘集全量同步**：
   - 对除运行中 `blu/G1130` 外的全部 36 个带 `device-profile.json` 机型执行 `apply-profile` 对齐市场机型名、基带与合法刷新率；
   - 本地 `pipeline device build` 重打具备完整官方固件的 5 个盘集（`honor/sun`、`asus/ASUSAI2501`、`redmi/gold` `15-OS3.0.11.0.VNQCNXM`、`hmd/ARW`、`google/oriole` `17-CP3A...`），并对剩余 39 个远端同步盘集原位刷新 `device.json` 及 `patch.img` 内的 `cph-ims-netmgr.{jar,rc}`、`vendor/build.prop`、`cdh_device_info.ko`（`39/39` `e2fsck -fn` 零错通过，严守 `AGENTS.md §8` 零触碰官方 `super.img`）。

---

## 五、验证

1. **单元测试门禁**：
   - `agent/modules/daemon`：`go test -race ./...` 全包通过（含新增 `TestGuestProbeScriptsAvoidHeavyNfcDumpAndBrokenPipes` 与 LTPO `1Hz` EDID 过滤用例）；
   - `agent/modules/pipeline`：`02_prepare`、`09_build_vendor`、`10_build_vendor_dlkm`、`11_build_patch`、`13_pack_vda` 单测全绿。
2. **`Honor Magic7 RSR #1` 实机 `Settings` 全项与掉帧压测**：
   - `About phone`：设备名自动自愈并展示 `PORSCHE DESIGN HONOR Magic7 RSR` / `HONOR Magic7 RSR 保时捷设计`、`Snapdragon 8 Elite Mobile Platform`、`16 GB` RAM、`2800 × 1280`、双 IMEI、完整 56 字节基带 `MPSS.DE.7.0-02024.3-PAKALA_GEN_PACK-1.80805.123.93847.33`；
   - `APNs`：正常打开并选中 `中国联通 (3gnet)`，附带 `中国联通彩信 (3gwap)`；
   - `Wi-Fi` & `Radio`：Wi-Fi 已连接 `TP-LINK_5G_C390`（外网 IPv4 `223.5.5.5` `0.697ms` / IPv6 `2400:3200::1` `0.625ms` `0%` 丢包）；SIM 1（`46001`）`dataConn=CONNECTED`，`ccmni1`（`100.64.155.15/24` + 全局 IPv6）与 `ccmni0`（IMS 全局 IPv6 + P-CSCF）双网卡 `UP` + `VALIDATED`；
   - 滑动性能与异常日志压测（`Settings` 连续 8 次长滑 + 并发触发 4 次 `/api/instance/config` 轮询）：`Total frames: 349`，**`Janky frames: 15 (4.30%)`**，`P50=9ms / P90=15ms / P95=18ms`，GPU `P99=14ms`；期间 `VipSchedProxy=0`、`FASProxy=0`、`lib_factory_interface.so=0`、`com.android.nfc` GC=`0`、`audit_lost=0`、`binder: reply target not found=0`。
3. **实例之外的其他镜像抽检**：
   - `gold_15-OS3.0.11.0.VNQCNXM #1`（HyperOS 3.0.11.0）：`About phone` 显示 `Redmi Note 13 5G` / `天玑 6080` / `128GB`，`APNs` 正常展示 `3gnet` / `3gwap`，`/proc/vip/ioctl` & `/proc/fas/ioctl` 就位，`ccmni0` IPv6 在线；
   - `G1170_15-AP3A.240905.015.A2-20260209 #1`（BLU G75 Android 15）：`About device` 显示 `BLU G75` / `MediaTek Helio G81` / `256GB`，`APNs` 选中 `中国联通 3g 网络 (3gnet)`，`ccmni1` `100.64.155.15/24` `dataConn=CONNECTED`，`Settings` 滑动 `Janky frames: 4/63 (6.35%)`、`P50=9ms`。

---

## 六、遗留

- `blu/G1130` 因另一并发会话正在独立运行调试（PID `33105`），按会话隔离原则未在本次触碰其运行目录与盘集；待其调试收尾后重跑一次 `apply-profile` + `build` 即可自然继承本条全部公共改动。
- 5.10 内核本机未挂载源码树，使用 5.10 的少量早期非荣耀机型沿用既有 `cdh_device_info.ko`（原生/MTK ROM 不访问 `/proc/{vip,fas}/ioctl`，无任何影响）；后续在 ROM 构建服重编一次 5.10 模块即可对齐。
