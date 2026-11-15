# 卡点：OneUI 的 system_server 要三星专有 ISehHealth，AOSP 健康 HAL 不够

- 机型：samsung / Galaxy S25 Ultra（SM-S9380，`ss9380`，国行 CHC）
- 系统：官方 OneUI Android 16，`samsung/pa3qzcx/pa3q:16/BP4A.251205.006/S9380ZCSDCZH1`
- vendor 基底：lineage 23.2（`roms/lineage/UTM-VM-lineage-23.2-...`），官方 super 零改动 + patch.img 三子树
- 时间：2026-10-30

## 1. 问题

01_unpack → 11_pack_vda 全部跑通、QEMU 能起、开机动画能截屏，但 `sys.boot_completed` 永不置位；
`init.svc.zygote` 每 5 s 重启一次，system_server 起不到一半就崩。

## 2. 根因

保留官方 super 意味着 framework 侧是**三星 OneUI 的 system_server**，它把
`com.android.server.health.HealthServiceWrapperAidl` 改造成**先取三星扩展接口
`vendor.samsung.hardware.health.ISehHealth/default`**，再用 AOSP 的
`android.hardware.health.IHealth/default`。而本机 vendor 是 lineage 基底，
只注册了 AOSP 的 `IHealth`（`init.svc.vendor.health-default=running`，
`service list` 里也有 `android.hardware.health.IHealth/default`），
三星接口在 servicemanager 里根本不存在。

于是 `HealthRegCallbackAidl.onRegistration` 拿着 null 的 ISehHealth 代理调
`registerCallback`，NPE 冒泡到 `BatteryService.onStart`，BatteryService 是
system_server 的 bootstrap 服务，起不来就整进程自杀；zygote 被 init 按 restart 拉起，
形成 5 s 一轮的死循环。

## 3. 证据

崩溃栈（`adb logcat -b crash -d`，改之前）：

```
java.lang.RuntimeException: Failed to start service com.android.server.BatteryService: onStart threw an exception
Caused by: java.lang.NullPointerException: Attempt to invoke virtual method
  'void vendor.samsung.hardware.health.ISehHealth$Stub$Proxy.registerCallback(
     com.android.server.health.HealthRegCallbackAidl$HalInfoCallback)'
  on a null object reference
    at com.android.server.health.HealthRegCallbackAidl.onRegistration
```

同时能证明「AOSP 那套一直在跑」的旁证（同一时刻）：

```
init.svc.vendor.health-default = running
service list | grep health
  android.hardware.health.IHealth/default
```

官方 vendor 里的真相（`vendor_a.img` 抽取）：

- `/vendor/bin/hw/vendor.samsung.hardware.health-service`（186752 B，aarch64 PIE）
  - `strings` 命中 `BnSehHealth::createBinder`、`vendor/samsung/interfaces/health/aidl/default/SehHealth.cpp`、`getSehHealthInfo`
  - `DT_NEEDED`（自写 `elf_needed.py` 解析）：`android.hardware.health-V3-ndk.so`、`android.hardware.thermal-V2-ndk.so`、
    `vendor.samsung.hardware.health-V1-ndk.so`、`vendor.samsung.hardware.thermal-V1-ndk.so`、
    `libbase/libbinder_ndk/libc++/libc/libcutils/libdl/libhidlbase/liblog/libm/libutils`
  - 官方 `vendor_file_contexts` 里它仍是标准标签 `u:object_r:hal_health_default_exec:s0`
- `/vendor/etc/init/vendor.samsung.hardware.health-service.rc`
  `service vendor.health-samsung /vendor/bin/hw/vendor.samsung.hardware.health-service`
  `class hal` / `user system` / `group system` / `capabilities WAKE_ALARM BLOCK_SUSPEND` / `file /dev/kmsg w`
- `/vendor/etc/vintf/manifest/vendor.samsung.hardware.health-service.xml`
  只声明 `android.hardware.health` v3 `IHealth/default`（**没有任何地方声明 ISehHealth**；
  扫过官方 vendor 全部 70 个 VINTF 片段 + `/system/etc/vintf` + `/odm/etc/vintf` 确认）

## 4. 改动

三处（都在 engine 清单里，不动官方 super）：

1. `devices/manifest/samsung/ss9380/device.json`
   删除 `android.hardware.health: {variant: default, version: 1.0}` 声明。
   理由：07 步骤 4 按声明装模块，两套实现会重复 `addService("android.hardware.health.IHealth/default")`
   并出现 VINTF `Conflicting FqInstance`，device manifest 直接变 NULL（`07_build_vendor/replace.go:83-92` 有同样教训的记录）。
2. `devices/manifest/samsung/ss9380/baselines/SM-S9380_3_*_fac.json`
   `vendor.replaces` 由 2 条扩到 8 条，新增 6 条官方件（每条带 note 记录崩溃栈与依赖闭包）：
   `bin/hw/vendor.samsung.hardware.health-service`、
   `etc/init/vendor.samsung.hardware.health-service.rc`、
   `lib64/vendor.samsung.hardware.health-V1-ndk.so`、
   `lib64/vendor.samsung.hardware.thermal-V1-ndk.so`、
   `lib64/android.hardware.health-V3-ndk.so`、
   `lib64/android.hardware.thermal-V2-ndk.so`
   （后两个 lineage 基底没有——自研健康 HAL 是 `staticLibs` 静态编入的）。
3. `devices/manifest/samsung/ss9380/vendor/etc/vintf/manifest/android.hardware.health-service.xml`
   机型静态树补 VINTF 声明（`android.hardware.health` v3 `IHealth/default`）。
   为什么必须补：Android 14+ servicemanager 对 AIDL 命名服务做 VINTF 强校验，
   system_server 侧走 `waitForDeclaredService`，没声明会一直等。

为什么是「借官方二进制」而不是「自研 shim」：AIDL 的 `interface hash` 必须客户端与服务端
逐字节一致，自写 `ISehHealth` 几乎不可能对上；官方件自带一套 `.so`，闭包只到
`libbinder_ndk/libc++/libc/libdl/libm`，且标签 `hal_health_default_exec` 在 lineage 策略里已存在，
不需要新增 sepolicy。

## 5. 验证

- 构建：`厂商覆盖: 8 条`，步骤 5 逐条 `[replace] vendor/bin/hw/vendor.samsung.hardware.health-service ...`，
  静态树 `vendor/etc/vintf/manifest/android.hardware.health-service.xml（0644）` 写入成功；
  09 出 `patch.img`（sha256 `ac6836e3ba96`）→ 10（super 复用）→ 11 `[ok] 已完成`。
- 起机：`init.svc.vendor.health-samsung = running`（init 日志 `starting service 'vendor.health-samsung'`，pid 731）。
- `logcat -b crash` 里 **BatteryService 的 NPE 消失**，只剩 `fabric_crypto` 的 SIGABRT（下一个卡点）。
- servicemanager 侧确认声明生效：
  `Caller(pid=19009,uid=1000,sid=u:r:system_server:s0) Found android.hardware.health.IHealth/default in device VINTF manifest.`

## 6. 遗留

- 三星健康 HAL 会去 `find android.hardware.thermal.IThermal/default`，被 SELinux 拒
  （`hal_health_default:s0` → `hal_thermal_service:s0`，`service_manager find -1`）；
  只影响 `getSehThermal()` 拿不到，日志里表现为告警，不影响 boot，暂不处理。
- 该机型仍是「官方 system + lineage vendor」，后续凡是 OneUI 用到三星专有
  vendor HAL/标签的地方都要按同一套路补（下一个是 `vendor.samsung.hardware.security.fkeymaster`）。

## 7. 复测记录（2026-10-31，最终盘集）

- 盘集：`patch.img` sha256 `53e483856316…`（含后续第一～十段 sepolicy 片段），
  实例 `index 5`；
- 服务在跑：

```
$ adb shell getprop | grep health
[init.svc.vendor.health-samsung]: [running]
[init.svc_debug_pid.vendor.health-samsung]: [745]
$ adb shell 'service list | grep -i health'
50  android.hardware.health.IHealth/default: [android.hardware.health.IHealth]
186 healthconnect: [android.health.connect.aidl.IHealthConnectService]
```

- `logcat -b crash -d | grep -c BatteryService` → `0`（之前是每次开机必现的 NPE）；
- `dumpsys battery` 能完整输出，且尾部的
  `[SS][BattInfo]QrData efsValue:` / `FirstUseDateData` / `DischargeLevelData` /
  `BatteryInfoBackUp` 段都是三星 `BatteryService` 自己的代码路径，说明它真的
  跑在 `ISehHealth` 上（AOSP 版拿不到这些）；
- 观察：`present: false / level: 0 / charge_counter: 0` —— 云机没有接真实电池，
  HAL 报“无电池”属预期，不影响开机与桌面使用；若要展示电量百分比，需要另做
  虚拟电池/VM 侧的电源模拟。

## 8. 方案变更（2026-10-31）：自研 HAL 加 `samsung` 变体，撤掉官方借件

第 4 节“借官方二进制”是当时的权宜之计，代价是本机型永远跟官方件绑死、
`battery_config.xml` 的模拟电量也用不上（官方件报“无电池”），而且三星专有 AIDL 会污染
所有云机的 health 模块。本轮改成与 `android.hardware.radio` 的 `mtk` 变体同一套路：
**模块基础版保持纯 AOSP，三星部分做成 `samsung` 变体，由 `device.json` 选**。

### 8.1 问题

`engine/vendor/hardware/android.hardware.health` 的模块配置把三星 AIDL
（`aidl/vendor/samsung/...`）与 `ISehHealth` 服务端写进了基础构建，而 ss9380 又在借官方二进制
——两套实现只能留一套，且三星专有 AIDL 不该出现在其他云机的 health 产物里。

### 8.2 根因 / 可行性

第 3 节曾判断“自研 shim 几乎不可能对上”—— 实测**可以完全对上**，因为官方
`lib64/vendor.samsung.hardware.health-V1-ndk.so` 里带着完整的 AIDL 生成代码：

- descriptor = `vendor.samsung.hardware.health.ISehHealth`；
- version = 1（`BnSehHealth::getInterfaceVersion` 反汇编 `mov w8, #0x1`）；
- 4 个方法的 transaction code = 声明顺序（`registerCallback`=1、`unregisterCallback`=2、
  `update`=3、`sehWriteEnableToParam`=4，均是无返回值的同步调用）；
- `SehHealthInfo` = AOSP `HealthInfo`（+0x00，0xf0 字节）+ 10 个私有基础类型字段
  （`writeToParcel` 逐条反汇编，见 `aidl/samsung/.../SehHealthInfo.aidl` 文件头）。

另一个关键事实：官方服务**不是**注册独立的 `ISehHealth` 命名服务，而是用
`AIBinder_setExtension` 把 ISehHealth 挂到 `IHealth/default` binder 的 extension 上（所以 `service list`
与官方 VINTF 片段里都看不到它）。因此自研 HAL 只要在 `addService` 前把自研 `ISehHealth`
挂成 extension，既不产生实例冲突，也不依赖官方二进制。

hash 问题也澄清了：OneUI 走 Java 侧 `ISehHealth.Stub.asInterface()`（崩溃栈里的类名就是
`ISehHealth$Stub$Proxy`），它不校验 interface hash，跨进程只依赖 descriptor + 版本 +
transaction code；codegen 不传 `--hash` 也能通，`ISehHealth.aidl` 文件头写了补救路径。

### 8.3 证据

- 构建/符号/依赖闭包：`log-pass-variant.txt`（本轮新增）
  - `--variant samsung`：`vendor/samsung/hardware/health (v1, 模块自带无 hash)`，编 16+2 个源；
    基础变体只编 13+1 个源，`strings` 命中 `vendor.samsung.hardware.health` 的次数 **0**；
  - samsung 产物命中 `ISehHealth` / `ISehHealthInfoCallback` 两个 descriptor；
  - 两变体 `DT_NEEDED` 完全一致（`libc++/libbinder_ndk/liblog/libdl/libm/libc`），
    即三星接口代码静态编入，不需要任何官方 `.so`；
  - codegen 产物：`ISehHealth::version = 1`，`case FIRST_CALL_TRANSACTION + 0/1/2/3`
    分别是 4 个方法，与官方反汇编逐条对齐。
- 装配计划：`cdh build-vendor --dry-run` 输出
  `HAL 模块: android.hardware.health（变体 samsung，变体产物根 out/vendor/android16/samsung …）`，
  且 `厂商覆盖集合: 基线清单 vendor.replaces（7 项）`（不再有健康 HAL 相关借件）。

### 8.4 改动

1. `engine/vendor/hardware/android.hardware.health/`
   - 三星 AIDL 从 `aidl/vendor/samsung/...` 移到 `aidl/samsung/vendor/samsung/...`；
   - 新增 `src/samsung/seh_health.{h,cpp}`：`SehHealth`（`BnSehHealth` 子类）+
     `AttachSehHealth()`（`AIBinder_setExtension` + 30 s 推送线程，数据源是基础实现注入的
     `HealthInfoFiller`）；
   - `src/health_service.cpp` 删掉内嵌的三星代码，只保留 `#if CDH_HEALTH_SAMSUNG` 挂载点。
2. `engine/vendor/build/modules/android.hardware.health.json`
   基础版去掉 `extraAidlRoots`；新增 `variants.samsung`（`extraAidlRoots: aidl/samsung`、
   `extraSourceDirs: src/samsung`、`includeDirs`、`-DCDH_HEALTH_SAMSUNG=1`），写法与 radio 的
   `mtk` 一致。
3. `devices/manifest/samsung/ss9380/device.json`
   重新声明 `android.hardware.health: {variant: samsung, version: 1.0}`。
4. `devices/manifest/samsung/ss9380/baselines/SM-S9380_3_*_fac.json`
   `vendor.replaces` 由 13 条减到 7 条，撤掉 6 条健康 HAL 借件（服务二进制、rc、
   `vendor.samsung.hardware.health-V1-ndk.so`、`vendor.samsung.hardware.thermal-V1-ndk.so`、
   `android.hardware.health-V3-ndk.so`、`android.hardware.thermal-V2-ndk.so`）。
5. 删除 `devices/manifest/samsung/ss9380/vendor/etc/vintf/manifest/android.hardware.health-service.xml`
   —— 模块 `vendor/` 静态树本来就产出同名片段，机型层不再重复声明。

### 8.5 验证

- 模块自测：`cd engine/vendor/build && go vet ./... && go test ./...` 全绿；
- 两变体实编译通过（`log-pass-variant.txt`），产物分别在
  `engine/out/vendor/android16/bin/hw/` 与 `engine/out/vendor/android16/samsung/bin/hw/`；
- 装配计划 dry-run：health 走 `samsung` 变体产物根，厂商覆盖 7 条；
- `engine/build` 的 `go test ./...`：仅 `05_build_vendor_boot` 的 3 个 fstab 用例失败
  （`TestMergeCloudFstab` / `TestBuildVendorBootCacheReuse` / `TestBuildVendorBootMergeLineage`），
  与本次改动无关（改动前即存在，均属 vendor_boot fstab 口径）。

### 8.6 遗留

- **实机起机复测待做**：需整机 `cdh device build ss9380` 出盘集后确认
  `init.svc.vendor.health-default=running`、`logcat -b crash` 无 BatteryService NPE，
  且 `dumpsys battery` 能读到 `battery_config.xml` 的模拟电量（这是本轮相对“借官方件”的收益点）。
- 三星 health 的推送线程与基础更新线程各自 30 s 轮询，属预期（两条链路互不依赖）；
- 若以后出现 native 侧的 `ISehHealth` 客户端，需要补 `.hash` 与 codegen 的 `--hash` 支持
  （`ISehHealth.aidl` 文件头已写清路径）。

## 9. 起机复测失败与修复（2026-10-31）：extension 被挂到了临时 binder 上

第 8 段的自研变体首轮起机**没有通过**：`sys.boot_completed` 永不置位，`init.svc.zygote`
一直在 `restarting`，crash 缓冲还是第 2 节那个 NPE。本节记录定位与修复。

### 9.1 问题

同一个 NPE 复现，但这次服务端是**自研** HAL：

```
Caused by: java.lang.NullPointerException: Attempt to invoke virtual method
  'void vendor.samsung.hardware.health.ISehHealth$Stub$Proxy.registerCallback(...)'
  on a null object reference
    at com.android.server.health.HealthRegCallbackAidl.onRegistration
```

（`log-fix-extension.txt` 是本次整机 `cdh device build ss9380` 的完整日志）

### 9.2 根因

`AttachSehHealth()` 与 `AServiceManager_addService()` 各调了一次 `health->asBinder()`。
`ndk::BnCInterface<INTERFACE>::asBinder()` 只持**弱**缓存：

```cpp
// out/android16/aosp/.../ndk/include_cpp/android/binder_interface_utils.h:353
SpAIBinder binder;
if (mWeakBinder.get() != nullptr) binder.set(AIBinder_Weak_promote(mWeakBinder.get()));
if (binder.get() == nullptr) { binder = createBinder(); mWeakBinder.set(AIBinder_Weak_new(binder.get())); }
return binder;
```

所以 `health->asBinder().get()` 是个**语句结束即析构**的临时对象：`AIBinder_setExtension`
把 ISehHealth 挂在 ABBinder 实例 A 上，临时 `SpAIBinder` 一析构，A 的引用计数归零被销毁
（extension 跟着没）；下一次 `asBinder()` 弱引用 promote 失败，**重建**出 ABBinder 实例 B
并注册给 servicemanager——客户端对 B 调 `getExtension()` 只能拿到 null，于是对 null 代理
调 `registerCallback`，BatteryService 再次起不来。

AOSP 自己在这个 API 的注释里就点了这个坑（`binder_ibinder.h:766-777`）：

> SpAIBinder binder = foo->asBinder(); // target binder to extend
> ... = AIBinder_setExtension(binder.get(), bar->asBinder().get());
> ... = AServiceManager_addService(binder.get(), instanceName);
> Do not use foo->asBinder().get() as the target binder argument to AIBinder_setExtensions
> because asBinder it creates a new binder object that will be destroyed after the function is called.
> The same binder object must be used for ...

### 9.3 证据

排除法（都是改之前的实测）：

- 设备上跑的就是本轮产物：服务 pid 704，二进制 823808 B，`strings` 命中
  `vendor.samsung.hardware.health.ISehHealth` 与 `ISehHealthInfoCallback`；
- 编译开关生效：`main_android.hardware.health-service.o` 有 `U cdh::health::AttachSehHealth`，
  最终二进制 `3ea4c: bl 0x3bd74 <...AttachSehHealth...>`；
- `AIBinder_setExtension` 没报错（否则会有 `E cdh_health: AIBinder_setExtension(ISehHealth) failed`），
  即“设置成功但客户端读到 null”；
- 客户端机制无误：反编译官方 `services.jar`，`HealthServiceWrapperAidl.<init>` 的字节码是
  `ServiceManager.waitForDeclaredService` → `Binder.allowBlocking` →
  `IBinder.getExtension()` → `ISehHealth.Stub.asInterface` → `onRegistration`；
- 不是异常：崩溃日志里没有 `Unable to register DeathRecipient for null`（catch 分支的日志），
  说明 transaction 返回 OK、extension 就是 null；
- 官方件同款机制：`out/roms/samsung/ss9380/.../super/vendor_a.img` 里的
  `vendor.samsung.hardware.health-service` 反汇编同样调 `AIBinder_setExtension@plt`，
  说明方向没错，错在“挂在哪个对象上”。

### 9.4 改动

1. `src/health_service.cpp` 的 `main()`：binder 只取一次并全程持有，
   `ndk::SpAIBinder binder = health->asBinder();`，`AttachSehHealth(binder.get(), …)`
   与 `AServiceManager_addService(binder.get(), …)` 用同一个 AIBinder（附注释说明弱缓存坑）。
2. `src/samsung/seh_health.cpp` 的 `AttachSehHealth()`：
   - 成功后**无条件**打 `I cdh_health: ISehHealth 已挂到 IHealth binder 的 extension 上（interval=30s）`；
   - 新增自检：立刻用 `AIBinder_getExtension()` 读一次，读不到就 `E` 日志并放弃挂载
     —— 下次这类问题在 logcat 里一眼可见，不用再上 dex 反查。

### 9.5 验证

- 模块：`cdh-vendor hal build android.hardware.health --android 16 --variant samsung`
  → `--variant samsung` 编 16+2 源、`vendor/samsung/hardware/health (v1, 模块自带无 hash)`；
- 整机：`cdh device build ss9380` → `[ok] 已完成`，`patch.img` sha256 `1184adc64d50…`
  （`log-fix-extension.txt`）；
- 实例 `index 3`（全新 `/data`，adb `127.0.0.1:5911`）：`sys.boot_completed=1`，
  `init.svc.zygote=running`，`init.svc.vendor.health-default=running`
  （`log-pass-fix.txt` 第 [1] 段）；
- `logcat -b crash -d | grep -c BatteryService` → **0**（第 [2] 段）；
- 客户端确实拿到了非 null 代理（`logcat -b all -d`）：

```
01-01 08:01:40.270 D HealthRegCallbackAidl: onRegistration: oldService: null newService: vendor.samsung.hardware.health.ISehHealth$Stub$Proxy@346fbc4
01-01 08:01:40.270 D HealthRegCallbackAidl: registerCallback: newService: vendor.samsung.hardware.health.ISehHealth$Stub$Proxy@346fbc4
08-12 17:08:14.311 D HealthServiceWrapperAidl: sehWriteEnableToParam[afc]: , offset: -1, enable: false
```

- `dumpsys battery` 读到自研模拟口径（第 [3] 段），不再是借官方件时的“无电池”：

```
  AC powered: true          present: true
  level: 100                scale: 100
  Charge counter: 5966667   voltage: 4431   temperature: 350
  technology: Li-poly       status: 2
  [SS][BattInfo]QrData efsValue: …   （三星私有段，证明跑在 ISehHealth 上）
```

### 9.6 遗留

- `/vendor/etc/battery_config.xml` 在 ss9380 机型层并不存在（全仓库只有代码引用，没有文件），
  所以走 `BatteryProfile` 通用缺省：6000 mAh、level 100、counter 5960000..5970000。
  要用 S25 Ultra 的真机口径需补这个文件（格式见 `src/health_service.cpp` 文件头）。
- 首轮注册瞬间出现过一条 `E [SS]BattUtils` 栈（`ISehHealth$Stub$Proxy.update` →
  `HealthRegCallbackAidl$HalInfoCallback.onTransact` → `BatteryService…update`，同一毫秒 2 条），
  是 BatteryService 自己读 efs 失败后打栈，之后不再出现，boot 与电量上报都不受影响，暂不处理。
- `asBinder()` 弱缓存还影响别处：任何“先 setExtension 再 addService”的地方都必须
  显式持有 `SpAIBinder`（本段已固化为 `main()` 的写法，可作为模板）。

