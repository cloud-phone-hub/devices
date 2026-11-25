# #333 闭环 `02_prepare` 硬件/市场名/动态内存向云机对齐断链与 `ASUSAI2501` 空占位 `RescueParty` 自动重启

- **编号**：#333
- **设备 / 分组**：`common`（跨全机型 `02_prepare` 市场名与刷新率透传、`service_daemon` 只读属性同步与多 SKU 内存档位探测、`cph-setup-display` 市场名自愈、`cdh_device_info.ko` 多档位动态内存容量与 `09_build_vendor` `/vendor/app` 隔离；首发真机闭环 `asus/ASUSAI2501` `15-AQ3A.240829.003-20250122`）
- **日期**：2026-11-25
- **状态**：**已闭环**（`ASUS ROG Phone 9 Pro #1` 连续运行 >400s 零重启、`RescueParty` 零触发、`About phone` 立绘图与全字段 100% 对齐真机口径，同目录单测全绿）

---

## 1. 问题

启动 `ASUS ROG Phone 9 Pro #1`（`asus/ASUSAI2501`，`15-AQ3A.240829.003-20250122`，`16GB+512GB`）后出现两类严重问题：

1. **每隔 45～135 秒自动触发重启**：开机进入桌面后无需人工操作即反复重启，`qemu.log` 记录 `init: Received sys.powerctl='reboot,RescueParty' from pid: 622 (system_server)`（`ro.boot.bootreason=reboot,rescueparty`）。
2. **`02_prepare` 规格档案未对齐到云机运行态与 `About phone` 页面**（对照 `about-phone-before.jpg`）：
   - 左侧手机外观立绘图错误显示为工程默认占位图 `FOR ROG 6`；
   - `CPU`、`RAM`、`Build number` 三个核心字段数值均为空白；
   - `Device name` 与蓝牙名称显示内部代号 `ASUSAI2501`（而非市场名 `ASUS ROG Phone 9 Pro`）；
   - 客机 `/proc/meminfo` 中 `MemTotal: 7760724 kB`（死写 8GB）小于 `MemAvailable: 11344948 kB`（16GB 实例真实可用内存），发生严重容量倒挂；
   - 盘集 `device.json` 缺少 `baseband-version` 与 `display.refresh-rates`（QEMU `virtio-gpu` 回退单档 `60Hz`）。

---

## 2. 根因

1. **`RescueParty` 自动重启与三类后台进程循环崩溃**：
   - **直接元凶（`.slss` 10Hz 死循环崩溃）**：反汇编 `/system/app/slsservice/slsservice.apk`（`PERSISTENT` 标志，`sharedUserId=android.uid.system`）证实，`SLSService.onCreate -> registerNetworkRttUtilCallback` 调用 `ServiceManager.getService("vendor.asus.network.rttutil.INetworkRttUtil/default")`。当服务未注册返回 `null` 时，`SLSService` 仅打印日志并安全返回；但 `02_prepare` 将该实例与 `vendor.asus.network.sla.ISlaService/default`（对应 `RescueParty` 监控名单中的 `com.asus.slaservice`）扫入了 `vendor/etc/cdh_nn_placeholders.txt`，由 `neuralnetworks-service` 抢注无协议空桩并返回 `STATUS_UNKNOWN_TRANSACTION`，致使 AIDL Proxy 抛出 `RemoteException("Method registerCallback is unimplemented.")` 并被包装为 `IllegalStateException` 炸毁主线程，`ActivityManager` 每 100ms 重拉一次 `PERSISTENT` 进程直至触发 `PackageWatchdog` / `RescueParty` `WARM_REBOOT`。
   - **原厂 `/vendor/app` 未隔离引发 `.cacertservice` 与 `.qtidataservices` 连崩**：`ASUSAI2501` 未重定向 `/vendor/app`，直通扫描了原厂 `vendor_a.img:/app/{CACertService,IWlanService,CneApp,...}`；其中 `CACertService`（`PERSISTENT`）因通用载体缺少高通私有 `qva_seapp_contexts` 触发 Zygote `selinux_android_setcontext` `SIGABRT` 循环，`IWlanService` 因未重跑含 `libWlanServiceJni.so` 回填的 `09_build_vendor` 循环抛 `UnsatisfiedLinkError`。
2. **`02_prepare` 到云机展示层与内核层的五处断链**：
   - **跨机型市场名断链**：`02_prepare` 已在 `manifestAligned.model` 中持有市场机型名（如 `ASUS ROG Phone 9 Pro`），但未下发至 `vendor/config.json`；`service_daemon`（`ensure_bluetooth_default_name`）与 `cph-setup-display` 在非小米机型（无 `ro.product.marketname`）上全部回退读取内部代号 `ro.product.model`（`ASUSAI2501`），将 `bluetooth.device.default_name`、`Settings.Global.device_name` 与 `Settings.Secure.bluetooth_name` 写成了内部代号。
   - **华硕 `/vendor/bin/sp` 工厂属性缺失与 `ro.vendor.asus.beta=1` 锁死**：反汇编 `AsusSettings.apk`（`DeviceAboutPhonePreferenceController.updateDeviceInformation`）与原厂 `/vendor/bin/sp` 证实，真机由 `/vendor/bin/sp` 读取 `/vendor/factory/*` 与 `sysinfo` 分区后写入 `ro.vendor.cpufreq`（`4.3GHz`）、`ro.vendor.memsize`（`%dGB` -> `16GB`）、`ro.vendor.build.csc.version`（`WW_AI2501_35.1810.1810.243`，UI 切分跳过机型段展示为 `WW_35.1810.1810.243`）、`ro.vendor.asus.product.mkt_name`、`ro.vendor.build.asus.sku`（`WW` 对应 ROG 的 `Utils.isChicago()`）与 `ro.vendor.config.idcode`（`3B` 加载 `assets/a25_p_b.c` ROG Phone 9 Pro 黑色真机立绘）。而官方 R2 内测包预置了 `ro.vendor.asus.beta=1`（见 [../../asusai2501/issue-asusai2501-ims-watermark-overlay/solution.md](../../asusai2501/issue-asusai2501-ims-watermark-overlay/solution.md)）且其 SELinux 类型 `vendor_asus_sp_system_prop` 禁止 `vendor_init` 覆写，直接命中 `if (beta == 1)` 跳过立绘加载停留在 `FOR ROG 6` 占位图（注：`SurfaceFlinger` 刷新率白名单只检查 `ro.product.system.name[0] == 'Z'`，见 [../../asusai2501/issue-asusai2501-zf-refresh-rate-whitelist/solution.md](../../asusai2501/issue-asusai2501-zf-refresh-rate-whitelist/solution.md)，与 `ro.vendor.build.asus.sku=WW` 互不冲突）。
   - **`cdh_device_info.ko` 内存容量硬编码 8GB**：`devices/modules/device_info/meminfo/meminfo.c` 将 `si_meminfo` 的 `totalram` 固定改为 gold 8GB 机型的 `7760724 kB`，而 `MemAvailable` 仍按 QEMU `-m 16384` 真实空闲页计算（`11344948 kB`），导致所有 >8GB 云机出现 `MemAvailable > MemTotal` 倒挂并错报 8GB 内存。
   - **多档刷新率与高通 `radio@qti` 对齐遗漏**：`alignModuleSubsections` 未将清洗后的多档刷新率同步写入 `module/config.json` 的 `display.refresh-rates`；`alignHardwareHALList` 在旧档案含 `"radioVariant": "default"` 时将高通机型的 `radio@qti` 降级回了 `default`。

---

## 3. 证据

- **修复前失败现场**（详见同目录 [log-fail.txt](log-fail.txt) 与 [about-phone-before.jpg](about-phone-before.jpg)）：
  - `qemu.log`：`init: Received sys.powerctl='reboot,RescueParty' from pid: 622 (system_server)`；
  - `logcat -b crash`：`com.asus.slsservice` 主线程每 100ms 抛 `java.lang.IllegalStateException: android.os.RemoteException: Method registerCallback is unimplemented.`，伴随 `.cacertservice` Zygote `selinux_android_setcontext` `SIGABRT` 与 `.qtidataservices` `UnsatisfiedLinkError`；
  - 客机属性与内存：`ro.vendor.asus.beta=1`、`ro.vendor.build.asus.sku=ZWW`、`ro.vendor.cpufreq=""`、`ro.vendor.memsize=""`、`ro.vendor.build.csc.version=""`、`bluetooth.device.default_name=ASUSAI2501`；`/proc/meminfo` 显示 `MemTotal: 7760724 kB` < `MemAvailable: 11344948 kB`。
- **修复后通过现场**（详见同目录 [log-pass.txt](log-pass.txt) 与 [about-phone-after.png](about-phone-after.png)）：
  - 稳定性：新实例 `ASUSAI2501 #1` 冷启动一次点亮，连续运行 `402.66s`（>6.7 分钟）零重启，`ro.boot.bootreason=reboot,factory_reset`，`RescueParty` 零告警，`.slss` / `.cacertservice` / `.qtidataservices` 零崩溃；
  - 属性与 UI：`ro.vendor.cpufreq=4.3GHz`、`ro.vendor.memsize=16GB`、`ro.vendor.build.csc.version=WW_AI2501_35.1810.1810.243`、`ro.vendor.build.asus.sku=WW`、`ro.vendor.config.idcode=3B`、`ro.vendor.asus.beta=0`、`global.device_name=ASUS ROG Phone 9 Pro`；`About phone` 页面完整呈现 ROG Phone 9 Pro 真机背板立绘、`4.3GHz`、`16GB`、`512 GB`、`ASUS ROG Phone 9 Pro`、`15`、`WW_35.1810.1810.243`、`144 Hz AMOLED`；
  - 内核 `/proc/meminfo`：`MemTotal: 15594256 kB`、`MemAvailable: 11978220 kB`、`MemFree: 2956736 kB`、`CommitLimit: 14088580 kB`，严格满足 `MemTotal > MemAvailable > MemFree > 0`；QEMU 启动参数透传 `refresh-rates=60:120:144`。

---

## 4. 改动

1. **`02_prepare` 空占位黑名单、`/vendor/app` 隔离、跨机型市场名与华硕属性推导**：
   - `agent/modules/pipeline/stages/02_prepare/firmware_measured_extra.go`：新增 `isBannedNNPlaceholderService`（排除 `vendor.asus.network.rttutil.` 与 `vendor.asus.network.sla.`）与 `hasIncompatibleVendorApps`（检测到 `CACertService` 或 `com.qualcomm.qti.gpudrivers.*` 时自动产出 `dir /vendor/app -> /mnt/vendorpatch/vendor/app`）。
   - `agent/modules/pipeline/stages/02_prepare/apply_profile.go`：`deriveVendorPropCandidates` 跨机型写入 `ro.vendor.cdh.device.market_name`；新增 `deriveAsusVendorProps` 与 `deriveAsusCSCVersion` 自动推导华硕 `ro.vendor.{cpufreq,memsize,asus.beta,asus.product.mkt_name,build.asus.sku,build.csc.version,config.idcode}`；`ApplyProfile` 对已入库档案执行占位服务清洗；`alignHardwareHALList` 对高通平台强制对齐 `radio@qti`。
   - `agent/modules/pipeline/stages/02_prepare/apply_profile_configs.go`：新增 `pruneBannedNNPlaceholdersText` 自愈剥离存量高危占位项，新增 `buildAlignedModuleDisplay` 将多档刷新率写入 `module/config.json` 的 `display.refresh-rates`，并在存量 >12 条 `redirect` 分支放行 `/vendor/app` 目录隔离规则。
2. **`09_build_vendor` 空目录保障**：
   - `agent/modules/pipeline/stages/09_build_vendor/files_compat.go`：`reconcileVendorSystemCompat` 确保创建空的 `vendor/app` 目录（与 `vendor/apex` 一致），保障 `cdh_redirect` 目标目录始终存在。
3. **`android.service.daemon` 只读属性同步、动态内存档位探测与市场名自愈**：
   - `devices/hardware/android.service.daemon/src/rust/src/props/{mod.rs,fixups.rs}`：新增 `sync_vendor_display_and_brand_props()`，将 `/vendor/build.prop`（叠加 `/odm/etc/build.prop`）中的 `ro.vendor.{cdh,asus,build.asus,build.csc,config,atd}.*`、`ro.vendor.{cpufreq,memsize}` 与 `ro.{vendor.,}soc.*` 通过 `resetprop::set_direct` 对齐（绕过早期分区先写先得与 `vendor_asus_sp_system_prop` 写限制），并按 `/proc/meminfo` 真实 `MemTotal` 自动吸附物理内存档位（8/12/16/24GB 等）刷新 `ro.vendor.memsize`；`ensure_bluetooth_default_name` 纳入 `ro.vendor.cdh.device.market_name` 并在旧值等于内部代号时刷新。
   - `devices/hardware/android.service.daemon/vendor/bin/cph-setup-display`：优先读取 `ro.vendor.cdh.device.market_name`，并在 `Settings.Global.device_name` 为空或仍等于内部代号 `ro.product.model` 时自动自愈更新为市场机型名。
4. **`cdh_device_info.ko` 多档位动态内存对齐**：
   - `devices/modules/device_info/meminfo/meminfo.c`：8GB 档（`6.5GiB ~ 9.0GiB`）严格保持 gold 真机 `7760724 kB`，其余内存档位按真实分配页等比扣除 SoC 保留页（16GB 实测 `15594256 kB`）、按 `MemTotal/2 + SwapTotal` 动态计算 `CommitLimit`，并钳制 `freeram` / `MemAvailable` / `MemFree` 留出最小内核余量，杜绝 `MemAvailable > MemTotal`。
5. **单元测试与 `asus/ASUSAI2501` 配置/盘集刷新**：
   - 新增同目录单测 `agent/modules/pipeline/stages/02_prepare/asus_profile_align_test.go` 与 `agent/modules/pipeline/stages/09_build_vendor/vendor_app_empty_dir_test.go`；
   - 执行 `pipeline device apply-profile asus/ASUSAI2501` 与 `pipeline device build ASUSAI2501` 全 13 阶段重编发布。

---

## 5. 验证

1. **单元测试**：
   - `cd agent/modules/pipeline && go test ./stages/02_prepare ./stages/09_build_vendor ./stages/13_pack_vda` 全部 PASS。
2. **全链路构建验证**：
   - `pipeline device apply-profile asus/ASUSAI2501` 与 `pipeline device build ASUSAI2501` 1～13 阶段零错误跑通（含 6.6 内核模块 `cdh_device_info.ko` 重编与符号版本校验、`android.service.daemon` 交叉编译、`patch.img` `94e69567f476`、`vda-568af1a10ef6`）。
3. **真机冷启动、长稳与人工审图验证**：
   - 通过 `cdha` 重建启动 `ASUSAI2501 #1`（`16GB+512GB`，ADB `127.0.0.1:5911`），冷启动 `sys.boot_completed=1`，连续运行 `402.66s` 零重启；
   - 核验全部系统属性、`Settings.Global.device_name`、`/proc/meminfo` 与 `About phone` 实机截图（[about-phone-after.png](about-phone-after.png)）全部符合预期。

---

## 6. 遗留

- 首启 `BOOT_COMPLETED` 广播阶段偶见原厂非持久应用 `com.asus.zenmotion` 因体感硬件服务未连接产生单次 `DeadObjectException` 退出（非 `PERSISTENT`、不在 `RescueParty` 观察集内，不影响触控、显示与整机稳定性）；后续如接入华硕体感手势设置可按需补充对应服务桩。
