# #133 com.android.nfc 的崩点是 ColorOS 私有 IOplusNfc，不在 AOSP INfc

- **机型**：OnePlus 15R（ColorOS 16.0.10.500_EX01 / SM8750 `canoe` / Android 16）
- **实例**：16（pid 23941，验证「HAL 就位但包没回来」）、17（pid 28045，主复现）、
  18（pid 58397，回滚后的对照）
- **前置条目**：[#131](../issue-nfc-crash-loop/solution.md)（NfcNci 崩溃风暴 → 屏蔽）、
  [#130](../issue-odm-whole-tree-redirect/solution.md)（`/odm` 整棵重定向）
- **一句话**：`com.android.nfc` 是我们自己的 system 分区里 ColorOS 的 APK，它启动第一件事是
  拿 ColorOS 私有 `IOplusNfc` 服务；云机既没有该服务的 AIDL 实现、也没有 HIDL 服务管理器，
  而它的 try 只 catch `RemoteException`，于是类初始化失败、崩溃风暴复现。**补 AOSP
  `android.hardware.nfc.INfc` 不在这条链上，解决不了。**

## 1. 问题

用户在 #131 之后提出改判：「集成 `engine/vendor/hardware/android.hardware.nfc`，
`cdh_redirect.rules` 第 251 行可以移除」——不再靠「让包不存在」绕过崩溃，改成提供 HAL
让 `com.android.nfc` 自己降级成「NFC 不可用」。

改动按此落地：

1. `devices/manifest/oneplus/15r/device.json` 的 `hardware` 列表加
   `{ "android.hardware.nfc": { "variant": "default", "version": "1.0" } }`；
2. `devices/manifest/oneplus/15r/kernel/cdh_redirect.rules` 把
   `dir /system/priv-app/NfcNci /mnt/vendorpatch/system/priv-app/NfcNci` 注释停用。

构建链 `07 → 09 → 05 → 11` 全部通过，HAL 侧完全达标：

- `07`：`编译产物 android.hardware.nfc@default: 写入 3`，`vendor.img` 200773632 B；
  `vendor/bin/hw/android.hardware.nfc-service`（186624 B，ELF aarch64 PIE）、
  `vendor/etc/init/android.hardware.nfc-service.rc`、
  `vendor/etc/vintf/manifest/android.hardware.nfc-service.xml`；
  `vendor-meta-rules.txt` 回填 `u:object_r:hal_nfc_default_exec:s0`（bin/hw）与
  `vendor_configs_file`（rc/xml）；
- 实例 16：`init.svc.vendor.nfc-default=running`、
  `service check android.hardware.nfc.INfc/default=found`，但 `nfcPkg=0`——
  这一轮漏跑了 05，规则还在旧 vendor_ramdisk 里（`cdh_redirect.rules` 由
  `build_vendor_boot` 注入 `first_stage_ramdisk/lib/modules/`）；
- 补跑 05（规则 22059 B 注入，配方 `vboot-c4f1fb719b7e -> vboot-5e9f5b2938c1`）+ 11 后，
  实例 17：`nfcPkg=1`、`nfcSvc=running`、`INfc/default=found`。

**但崩溃照旧，而且更密**：实例 17 的 `am_crash` 中 `com.android.nfc` 占 **829 条**，
`am_proc_start` 也是 829 条（1:1，一条 crash 立刻被重拉），稳定态约 **7 次/秒**，
同期 1 分钟 load **14.02**。崩溃栈顶既不是 `INfc`、也不是 AOSP 的库，而是
`vendor.oplus.hardware.nfc.V1_0.IOplusNfc.getService`。

## 2. 根因

`com.android.nfc` 不是 AOSP 的 NfcNci，是 **ColorOS 的定制版**
（`/system/priv-app/NfcNci/NfcNci.apk`，13 866 262 B，单 `classes.dex` 9 017 908 B）。
它的启动链在碰任何 NFC HAL 之前先决定「用哪颗芯片」：

```
NfcApplication.onCreate(NfcApplication.java:65)
  → NfcService.<init>(NfcService.java:1299)
    → VendorNfcService.newVendorNfcService(VendorNfcService.java:393)
      → NfcChipManager.createNfcService(context, nfcService)
        → NfcChipManager.<clinit>(NfcChipManager.java:25)   ← 静态字段 CHIP_SET
          → NfcChipManager.getChipVersion(NfcChipManager.java:200)
```

反编译后 `NfcChipManager.getChipVersion()` 原文（jadx 还原，行号为 dex 原始行）：

```java
private static byte getChipVersion() {
    IOplusNfc iOplusNfcAsInterface =
        IOplusNfc.Stub.asInterface(ServiceManager.getService(AIDL_OPLUS_NFC_SERVICE_NAME));
    byte chipVersion = 2;
    try {
        if (iOplusNfcAsInterface == null) {
            Log.d(TAG, "aidl_oplusNfc == null , try to use IOplusNfc Hidl_service");
            V1_0.IOplusNfc service = V1_0.IOplusNfc.getService();   // ← 崩在这里
            if (service != null) chipVersion = service.getChipVersion();
            else Log.e(TAG, "iOplusNfcService null , need to check current NFC CHip");
        } else {
            chipVersion = iOplusNfcAsInterface.getChipVersion();
        }
    } catch (RemoteException e) {          // ← 只接 RemoteException
        e.printStackTrace();
    }
    return chipVersion;
}
```

三处缺陷叠在一起，云机必崩：

1. **AIDL 分支拿不到**：服务名是 `IOplusNfc.DESCRIPTOR + "/default"` =
   `vendor.oplus.hardware.nfc.IOplusNfc/default`。实现是厂商
   `/odm/bin/hw/vendor.oplus.hardware.nfc_aidl-service`；而云机 `/odm` 由 06 从清单目录
   重建，#130 之后只剩 `build.prop` 一个文件，服务二进制不存在。
   就算补上，`u:r:nfc:s0` 对 `u:object_r:default_android_service:s0` 的 `find` 也是被拒的
   （条目 4）。
2. **HIDL 分支恒不可达**：云机是 AIDL-only 系统，
   `HidlServiceManagement: hwservicemanager is not supported on the device`。
   `HwBinder.getService()` 在服务不存在时抛 **`NoSuchElementException`**，
   不是 `RemoteException`——上面的 catch 接不住，异常直接逃出 `getChipVersion()`。
3. **静态字段**：`CHIP_SET = getChipVersion()` 是 `<clinit>` 里的初始化表达式，
   一旦抛异常，整个 `NfcChipManager` 就是 `ExceptionInInitializerError`，
   之后所有引用它的路径都报废。`pm disable-user` 也没用——`com.android.nfc` 是
   persistent 应用（#131 已实测 `new state: default`），system_server 无限重启它。

**即使把前两项都补齐，还会崩在第三层**：`CHIP_SET` 决定走
`NxpNfcService` / `StNfcService` / `TmsNfcService`，三家的 `NativeNfcManager`
都有

```java
static {
    String str = NfcChipManager.LIBRARY_NAME;
    Log.d(TAG, "Loading library: " + str);
    System.loadLibrary(str);
}
```

`LIBRARY_NAME` 由 `CHIP_SET` 映射到 `sn100nfc_nci` / `st54nfc_nci` / `nq330nfc_nci` /
`thn31nfc_nci`。云机 `/vendor/lib64` 里 **一个都没有**（07 装配树 108 个库里 grep 不到
`nfc`），崩点只会从 `NoSuchElementException` 后移到 `UnsatisfiedLinkError`。

所以「集成 AOSP `android.hardware.nfc` 让 NfcNci 自己降级」这个前提不成立：AOSP `INfc`
在崩点之后才可能被调用（`libnfc-nci` 的 `HalOpen`），而进程在更早的类初始化就死了。

## 3. 证据

- [log-fail.txt](log-fail.txt)：6 节——HAL 就位（init 拉起 pid 624、服务注册）、
  完整崩栈（`ExceptionInInitializerError` → `NoSuchElementException` →
  `HwBinder.getService` → `V1_0.IOplusNfc.getService:88` → `NfcChipManager.getChipVersion:200`）、
  `hwservicemanager is not supported`、`avc denied { find }` 1888 条、
  crash/proc_start 各 829 条与分钟聚合、静态核查（`/odm` 只有 build.prop、
  `/vendor/lib64` 无 NFC 库、厂商 odm 三件套全是 AIDL）。
- [log-pass.txt](log-pass.txt)：回滚后的对照（见第 5 段）。
- 反编译产物（未入库，可复现）：`dump.erofs --path=/system/priv-app/NfcNci/NfcNci.apk`
  取 NID 后 `--cat` 导出，`jadx -d src --no-res NfcNci.apk`；关键文件
  `src/sources/com/android/nfc/NfcChipManager.java`（215 行）、
  `src/sources/vendor/oplus/hardware/nfc/V1_0/IOplusNfc.java`（135 行）、
  `src/sources/com/android/nfc/VendorNfcService.java`（1914 行，
  `newVendorNfcService` 在 328 行）。

顺带更正 #131 的一处推断：原文写「该 HIDL 的实现是官方
`/odm/bin/hw/vendor.oplus.hardware.nfc@1.0-service`，本表 `/odm` 整棵屏蔽后云机永远拿不到」。
实测厂商 odm 里**没有** HIDL V1_0 服务，只有 AIDL 三件套
（`android.hardware.nfc-service.nxp`、`vendor.oplus.hardware.nfc_aidl-service`、
`vendor.oplus.hardware.nfcExtns-service`），`odm/lib64` 也没有
`vendor.oplus.hardware.nfc@1.0.so`。真正的第一层原因是「云机没有 hwservicemanager」，
`/odm` 整棵只影响 AIDL 分支。

## 4. 改动

1. `devices/manifest/oneplus/15r/device.json` **保留** `android.hardware.nfc`：它与本崩溃
   无关，但能消掉 framework 侧每秒一条
   `Waited one second for android.hardware.nfc.INfc/default` 的空等，也是设备无关的正确声明。
2. `devices/manifest/oneplus/15r/kernel/cdh_redirect.rules`：`dir /system/priv-app/NfcNci`
   **恢复生效**；上面 37 行注释改为「复核（2026-11-03）：解除屏蔽的尝试失败，本条恢复生效」，
   写清崩栈、`NfcChipManager` 只 catch `RemoteException` 的事实、三层依赖的现状与决定；
   #131 的原文段落整段保留在下方备查。
3. 重建：`build_vendor_boot`（规则 23502 B 注入，配方 `vboot-5e9f5b2938c1 -> vboot-9b27edade3c8`）
   → `pack_vda`（vda.img 5 个分区刷新）。`vendor.img` / `patch.img` 未变
   （patch.img 仍 `4d0aa594e469`，`pack_vda` 判内容一致后 skip）。

不改 `engine/vendor/hardware/android.hardware.nfc/` 一个字符：它按 m22 的语义实现
（`open()` 受理后 20 ms 异步回 `OPEN_CPLT(FAILED)`），在这台机器上「正确但用不上」。

## 5. 验证

实例 18（pid 58397，8 vCPU，与实例 17 同规格），三次采样：

| 帧 | up (s) | boot | `nfcDir` | `nfcPkg` | `nfcCrash` | `nfcProc` | `avcIOplus` | load 1m |
| :--- | ---: | :--- | :--- | :--- | ---: | ---: | ---: | ---: |
| 1 | 83.89 | 1 | ENOENT | nfcengineertest + wallet | 0 | 0 | 0 | 20.46（首启峰） |
| 2 | 239.85 | 1 | — | — | 0 | 0 | 0 | 5.81 |
| 3 | 470.24 | 1 | ENOENT | nfcengineertest + wallet | 0 | 0 | 0 | **0.36** |
| 4 | 691.60 | 1 | — | — | 0 | 0 | — | 1.33 |

- 甲、屏蔽恢复有效：`/system/priv-app/NfcNci` = **ENOENT**、`pm list packages` 里没有
  `com.android.nfc`、`logcat -b events | grep -c 'am_crash.*com.android.nfc'` = **0**、
  `ps -A | grep -c com.android.nfc` = **0**、`dmesg | grep -c IOplusNfc` = **0**；
  load 从首启峰 20.46 落到 **0.36**（对照实例 17 的 14.02），691 s 帧仍为 0 条 / load 1.33。
- 乙、集成本身没被撤：`init.svc.vendor.nfc-default` = **running**、
  `service check android.hardware.nfc.INfc/default` = **found**，四帧一致（未重启、未被
  servicemanager 摘名）——用户两条诉求里的「集成 `android.hardware.nfc`」保留了，
  「移除第 251 行屏蔽」被实测证伪后回退。
- 丙、系统其余未受影响：`sys.boot_completed=1`、`dumpsys window` 正常返回
  `mCurrentFocus=Window{27ced0c u0 com.oplus.battery}`、`am_crash` 总量 9（开机期非 NFC 项）。

完整采样见 [log-pass.txt](log-pass.txt)。

## 6. 遗留

1. **NFC 在 15r 上不可用，且短期无解**：要真让 `com.android.nfc` 活着，得同时具备
   「ColorOS 私有 AIDL `IOplusNfc` 服务 + 对应芯片型的 NCI HAL 库 + 内核 NFC 驱动节点
   `/dev/nq-nci`」；云机三样都没有，而且没有 NFCC 硬件。真要做，第一步是解
   `vendor.oplus.hardware.nfc_aidl-V2-ndk.so` 的接口（`getChipVersion`、`initialize`）
   写一个 AIDL 桩，再补一个 `System.loadLibrary` 能过的空 NCI 库——即便如此，
   `libnfc-nci` 打开 HAL 失败后仍可能撞 `NfcService$WatchDogThread`。
2. `com.oplus.nfcengineertest` 未被屏蔽（#131 遗留，开机不自跑），本条目未动。
3. 顺带确认的旁证：这套「厂商私有 AIDL 服务名 + 默认 `default_android_service` 标签」
   的 avc 拒绝在本机是成片的（`IUrcc`、`IPerformance`、`IDvs` 等每开机数百条），
   NFC 只是其中一条；若后续要收口，应按 #128 的手法逐条补 `service_contexts`，
   而不是关 SELinux。
