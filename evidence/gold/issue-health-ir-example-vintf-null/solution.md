# Issue: 官方 vendor 的 health/ir example 借件与自研 HAL 撞车 —— VINTF device manifest 整份变 NULL

- 设备: Xiaomi Redmi Note 13 5G（gold-15，MTK MT6833，云机 QEMU + hvf）
- ROM: 官方 `OS3.0.10.0.VNQCNXM`（gold 底包）+ lineage-22.0（lineage-22.6.6 内核）
- 状态: **已修复**（2026-10-25，实例 `gold_OS3.0.10.0.VNQCNXM/22`）

---

## 1. 问题现象

自研 `android.hardware.health` 模块接入后起机，servicemanager 报 VINTF device manifest
解析失败，且**整份 device manifest 不可用**：

```
servicemanager: getDeviceHalManifest: -2147483648 VINTF parse error:
Cannot add manifest fragment /vendor/etc/vintf/manifest/android.hardware.health-service.xml:
HAL "android.hardware.health" has a conflict: Conflicting FqInstance: IHealth/default (@3)
  (from /vendor/etc/vintf/manifest/android.hardware.health-service.example.xml)
vs. IHealth/default (@3)
  (from /vendor/etc/vintf/manifest/android.hardware.health-service.xml).
Check whether or not multiple modules providing the same HAL are installed.
servicemanager: NULL VINTF MANIFEST!: device
```

连锁症状（实例 21，qemu.log 里 Conflicting 10 次、NULL VINTF 9 次）：

- `vold` 通过 lazy 方式起 `android.system.keystore2.IKeystoreService/default` 失败
  （`could not be found trying to start it as a lazy AIDL service`），重试 **5027** 次；
  late-fs 的 `/data` 格式化后卡死，zygote / adbd 一个都没起（`starting service 'zygote'` 计数 0）。
- `vold: AIDL IBootControl not available, falling back to HIDL / Error getting bootctrl v1.0 module`。
- 另有 `init: /vendor/etc/init/android.hardware.health-service.rc: 1: ignored duplicate
  definition of service 'vendor.health-default'` —— 同名服务后定义者被忽略。

## 2. 根因

`build/07_build_vendor/replace.go` 的 `vendorReplaces`（步骤 5「厂商 vendor 原件覆盖」）
里有一组**借件**：早期 lineage VM vendor 不带健康 HAL，而 system_server 的 BatteryService
硬依赖 `IHealth/default`，缺服务会自杀循环，于是从**官方 gold vendor 分区**
（`out/roms/xiaomi/gold/OS3.0.10.0.VNQCNXM/super/vendor_a.img`）抽 AOSP 参考实现进装配树：

| 借件 | 说明 |
| --- | --- |
| `bin/hw/android.hardware.health-service.example` | AOSP 参考二进制 |
| `etc/init/android.hardware.health-service.example.rc` | 定义 `service vendor.health-default` |
| `etc/vintf/manifest/android.hardware.health-service.example.xml` | 声明 `IHealth/default @3` |
| `lib64/android.hardware.health-V3-ndk.so` | 参考实现的依赖库 |
| 红外同构四条 | `android.hardware.ir-service.example*` + `lib64/android.hardware.ir-V1-ndk.so` |

自研 `android.hardware.health` 模块落地后（`device.json` 已声明，07 步骤 4 写入
`bin/hw/android.hardware.health-service` + 自己的 rc + VINTF 片段），这组借件没有撤掉，
于是出现**两份实现同时存在**：

1. **VINTF 撞车**：自研的 `android.hardware.health-service.xml`（313 B）与官方
   `.example.xml`（313 B）**内容逐字相同**（自研那份照抄 AOSP 片段，连
   `<!-- Input: hardware/interfaces/health/aidl/default/... -->` 注释都带着），
   同一 fqname 出现两次 → manifest 唯一性校验失败 → 整份 device manifest 返回 NULL。
2. **init 服务撞名**：两份 rc 都定义 `service vendor.health-default`，按文件名排序
   `.example.rc` 先解析，自研 rc 的定义被 ignored —— 实际起的是**官方 example 二进制**，
   自研实现从未运行。
3. **IR 侧是反向缺口**：`etc/permissions` 整目录替换带来
   `android.hardware.consumerir.{xml,prebuilt.xml}` → `FEATURE_CONSUMER_IR` 有声明，
   但 `engine/vendor/hardware` 里没有 ir 模块、`device.json` 也没声明 → 只有 example 借件
   能补位；借件一撤，特性与 HAL 立刻不成对。

## 3. 证据

- `log-fail.txt`：实例 21 的 qemu.log 原行（VINTF 冲突 / NULL manifest /
  health 服务 duplicate / vold lazy 起 keystore2 失败）。
- `log-pass.txt`：实例 22 的 qemu.log 原行（health、ir 服务启动，servicemanager
  `Found android.hardware.ir.IConsumerIr/default in device VINTF manifest`，
  vold 与 keystore2 正常交互）。
- 装配树对照（07 产物）：
  - 修复前 `vendor/bin/hw/` 同时有 `android.hardware.health-service`（248184 B）与
    `android.hardware.health-service.example`（205880 B）；
  - 修复后只剩自研 `android.hardware.health-service`（248184 B）与
    `android.hardware.ir-service`（122104 B），example 全部消失；
  - 07 日志「厂商覆盖」由 **18 条降为 10 条**。
- 自研 health 的 `DT_NEEDED` 只有 `libc++ / libbinder_ndk / liblog / libdl / libm / libc`，
  证明借来的 `android.hardware.health-V3-ndk.so`、`libpng.so` 并非自研实现所需。
- 策略面：`lineage` 的 `unpack_vendor/etc/selinux/vendor_sepolicy.cil` 第 398/400 行
  已定义 `hal_ir_default` / `hal_ir_default_exec`，`vendor_file_contexts` 第 38/57/60 行
  预留了三种 ir 档名规则 —— 说明自研 ir 不需要新造 SELinux 类型，只缺二进制与片段。

## 4. 改动

| 文件 | 改动 |
| --- | --- |
| `engine/build/07_build_vendor/replace.go` | 删除 health 借件 4 条与 ir 借件 4 条（共 8 条），注释改写为「自研模块提供，借件会撞 fqname 与服务名」；`lib64/libpng.so` 保留 |
| `engine/vendor/build/internal/profile/profile.go` | HAL 表新增 `ir`（`hardware/interfaces/ir/aidl/aidl_api` + `android.hardware.ir` v1），供 `depsHal` 校验与 AIDL 快照 |
| `engine/vendor/build/modules/android.hardware.ir.json` | 新建 spec：`kind=aidl`、`depsHal=ir`、服务 `android.hardware.ir-service`、依赖 `libc++/libbinder_ndk/-llog` |
| `engine/vendor/hardware/android.hardware.ir/src/consumerir_service.cpp` | 新建：`IConsumerIr` 的云机口径实现（`getCarrierFreqs` 返回 30k~56kHz，`transmit` 只记日志） |
| `.../android.hardware.ir/vendor/etc/init/android.hardware.ir-service.rc` | 新建：`service vendor.ir-default`，class hal |
| `.../android.hardware.ir/vendor/etc/vintf/manifest/android.hardware.ir-service.xml` | 新建：声明 `IConsumerIr/default` v1（档名不带 `.example`，避免与借件同名） |
| `.../android.hardware.ir/sepolicy/vendor_file_contexts` | 新建：`/vendor/bin/hw/android\.hardware\.ir-service → hal_ir_default_exec`（基底只覆盖 `.example`/`.lineage`/`ir@1.0-service`） |
| `devices/manifest/xiaomi/gold/device.json` | `hardware` 新增 `android.hardware.ir@default`（17 → 18 项） |

配套重出盘集：`07_build_vendor`（vendor.img）→ `09_build_patch`（patch.img）→
`11_pack_vda`（vda.img 5 分区）。`cdh` 与 `cdh-vendor` 均需重编（前者改 07，后者改 profile）。

## 5. 验证结果

| # | 验证点 | 结果 |
| --- | --- | --- |
| 1 | `cdh-vendor deps ir` + `hal build android.hardware.ir` | AIDL v1（hash `3e04aed366e9`）→ `bin/hw/android.hardware.ir-service` 122104 B |
| 2 | 07 厂商覆盖条数 | 18 → **10**，`bin/hw` 下 example 全部消失 |
| 3 | 09 patch.img | 402653184 B，sha256 **f4d9440688cb**，964 条目回读 964/964 |
| 4 | 11 vda.img | super/boot_a/boot_b/vendor_boot_a/b 五分区回读一致 |
| 5 | 起机 `Conflicting FqInstance` / `NULL VINTF MANIFEST` | **0 / 0** |
| 6 | 起机 `Failed to resolve allow statement` / `InitFatalReboot` | **0 / 0**（odm 策略屏蔽仍有效） |
| 7 | VINTF 可查 | `Found android.hardware.ir.IConsumerIr/default in device VINTF manifest` |
| 8 | 服务启动 | `vendor.health-default` pid 488、`vendor.ir-default` pid 493，均无退出记录 |
| 9 | keystore2 死等消失 | vold 正常走到 `/data` 加密策略设置与 user_keys 备份（`generateKey -67` 是 rollback-resistance 的预期 fallback） |

## 6. 遗留 / 下一步

- **boot 仍未完成**：实例 22 跑到 130 s 时 `sys.boot_completed` 仍为 0，`adb 127.0.0.1:5911`
  offline。`adbd` 在 `init.usb.rc` 只由 `on property:sys.usb.config=adb && sys.usb.configfs=0`
  触发，云机没有 USB gadget，`sys.usb.config` 永不被设置 —— 需要确认机型 rc/属性侧
  是否有主动 `start adbd`（旧 devices 线曾解决过，需回查 `issue-adb-shell-netns`）。
- **HAL 进程反复退出**：`android-hardware-media-c2-hal`、`media-c2-v4l2-hal-1-2`、
  `vendor-dolby-media-c2-hal`、`vendor.cdh_gold_radio` 各「exited 4 times before boot
  completed」，需另开 issue（media c2 与 `issue-media-c2-seclabel` 可能同源）。
- **avc denied**：`bootanim` 读 `mcd_data_file`、`hal_wifi` 读写 `console_device:hvc9`、
  `hal_thermal` 读 `proc_stat` 等仍在 permissive=0 下被拒。
- **manifest 缺项**：system_server 报
  `android.hardware.power.stats.IPowerStats/default` 不在 device manifest（有模块但未声明/未上树）。
- 借件撤掉后 `lib64/android.hardware.health-V3-ndk.so` / `lib64/android.hardware.ir-V1-ndk.so`
  不再被拷入；若将来别的官方原件依赖它们，需单独按需补回。
