# #69 m22 `vendor.awa.reality` + `vendor.qti.diaghal`：按原厂 `.so` 反解 AIDL 并自写实现

> 接 #68。`manifest/flyme/m22/sepolicy/vendor_service_contexts` 里只剩 3 个「只有标签、
> 没有实体」的名字：`vendor.awa.reality.IRealityTap/default`（V3）、
> `vendor.qti.diaghal.Idiag/default`（V1）、框架服务 `realitytap`。
> 本轮把前两个做成真实模块（`kind=aidl`，源码自写，模块里不留任何原厂 ELF），
> 第三个判定为「vendor 侧永久无法实现」，标签与授权留在机型层并改回原厂类型名。

## 1. 问题

1. **服务名只有标签没有实体**：这两个名字是 Flyme framework（`RealityTapManagerService`、
   `DiagManagerService`）会去查的 vendor HAL，本云 vendor 侧一条声明都没有
   （lineage-23.2 virtio 底座不含 `vendor.awa.reality` / `vendor.qti.diaghal`）→
   VINTF 无声明、`bin/hw` 无二进制、`lib64` 无 `-Vndk.so`。
2. **跨包类型卡住引擎**：`IRealityTap` 的三个方法都用
   `android.hardware.vibrator.EffectStrength` / `IVibratorCallback`（平台 AIDL 包），
   而 `cdh-vendor` 的 AIDL codegen 原本是「一个模块一个 AIDL 根」
   （`AIDLRoot()` 二选一：模块自带 `aidl/` 或 `depsHal` 快照），
   导致「模块自己的包 + 平台包」不能同时进 `-I`。
3. **机型层标签名与原厂不一致**：机型层把框架服务 `realitytap` 标成
   `hal_realitytap_service`，而这正是原厂 **HAL 实例**的类型名
   （原厂 `vendor_service_contexts` 第 139 行）；框架服务在原厂用的是
   `realitytap_service`（第 138 行）。两者混淆后，任何一侧接入真实体都会撞
   `(type …)` 重复声明 → secilc `Failed to resolve` → init 致命重启。

## 2. 根因

1. **本轮缺的是「实体」，不是标签**：#65 已经把「原厂 5 个私有 HAL 都有实体」这条
   纠正过一次，#66/#67/#68 用「逆向 `.so` → 自写 AIDL + C++ → 模块 rc/VINTF/sepolicy
   → spec → device.json → 机型层下放」模板做了 4 个；这两个是同一批剩下的。
2. **引擎的单根限制是设计遗留**：模块要么自带 `aidl/`（`aidlRootModule`），要么用
   `depsHal` 指一个平台 HAL 快照。`IRealityTap` 同时需要两者：
   自己的包 `vendor.awa.reality`（3 个 `.aidl`）+ 平台包 `android.hardware.vibrator`
   （只用到 2 个类型，可用 `aidlPackageSubset` 裁子集）。
3. **`realitytap` 无法做成模块**：它是 `system_server` 里
   `RealityTapManagerService` 自己 `addService("realitytap")` 的框架服务，不在 vendor
   进程里，vendor 侧只能给它打标签与 `add`/`find` 授权——所以它必须留在机型层。
4. **原厂两个标签本就分属两个类型**（原文 cil 可证）：
   `(typeattributeset vendor_service (hal_realitytap_service))`（HAL 实例）
   与 `system_server → realitytap_service (add find)`（框架服务）。

## 3. 证据（逆向事实）

原厂件根：`out/tmp/unpack-vendor_a/flyme-m22/`（记作 `$V`），逆向工作区 `out/tmp/rev2/`。

**a) 原厂件清单**

| 件 | 大小 | 说明 |
| --- | --- | --- |
| `$V/bin/hw/vendor.awa.realitytap.service` | 52,024 B | 服务二进制，`NEEDED` 含 `vendor.awa.reality-V3-ndk.so` + `vendor.awa.realitytap.impl.so` |
| `$V/lib64/vendor.awa.reality-V3-ndk.so` | 85,248 B | AIDL 接口层（Bn/Bp + 3 个 parcelable） |
| `$V/lib64/vendor.awa.realitytap.impl.so` | 52,072 B | 真机实现 |
| `$V/lib64/vendor.qti.diaghal-V1-ndk.so` | 85,776 B | AIDL 接口层 |
| `$V/lib64/vendor.qti.diaghal@1.0.so` | 169,648 B | 旧 HIDL 实现（本云不搬） |
| — | — | **没有** `vendor.qti.diaghal-service` 二进制：原厂由
`/vendor/bin/diag-router` 注册 `Idiag/default` |

**b) 接口 hash 与 descriptor**（`strings -a <so> | grep -E '^[0-9a-f]{40}$'`）

- `vendor.awa.reality-V3-ndk.so`：`054e790771427edda2465b91648721f3d1033d4b`（IRealityTap v3）
  与 `ea8742d6993e1a82917da38b9938e537aa7fcb54`（`android.hardware.vibrator` **V2** 的接口
  hash —— 与引擎 `deps vibrator` 快照打印的 `ea8742d6993e` 完全一致，说明我们引用的
  vibrator ABI 与原厂同版）。
- `vendor.qti.diaghal-V1-ndk.so`：`1180103e4052662bcb692e2ae3a1d6b326fc5dfb`（IIdiag v1）。
- descriptor：`vendor.awa.reality.IRealityTap` / `.RealityTapHe` / `.RealityTapRtp`；
  `vendor.qti.diaghal.Idiag` / `.Idiagcallback` / `.ParcelableMemory`。

**c) 事务表与写序**（`llvm-objdump -d --no-show-raw-insn` + `rev_any.py` / `codes.py`）

| 接口 | code | 方法 | 原厂写序（transact 前） | 回包 |
| --- | --- | --- | --- | --- |
| `BpRealityTap` | 1 | `performTap` | `Int32(1)` → `RealityTapHe::writeToParcel` → `AParcel_writeByte`（EffectStrength `@Backing(byte)`）→ `IVibratorCallback::writeToParcel` | `StatusHeader` + `Int32` |
| | 2 | `performRtp` | 同上，改用 `RealityTapRtp` | 同上 |
| | 3 | `cancel` | 空 | 同上 |
| | 4 | `calibrate` | 空 | 同上 |
| | 5 | `getF0` | 空 | 同上 |
| | 16777214/16777215 | `getInterfaceHash` / `getInterfaceVersion` | — | `String` / `Int32` |
| `BpIdiag` | 1 | `close` | 空 | `StatusHeader` + `Int32` |
| | 2 | `ioctl` | `Int32(cmd)` → `ParcelableMemory` → `Int32(flag)` | 同上 |
| | 3 | `open` | `IIdiagcallback`（binder） | 同上 |
| | 4 | `write` | `ParcelableMemory` → `Int32(length)` | 同上 |
| `BpIdiagcallback` | 1 | `send_data` | `ParcelableMemory` → `Int32(length)` | 同上 |

原厂 transact flags 全部为 `0x10000000`（`FLAG_PRIVATE_LOCAL`，**非** oneway；
`BpVibratorCallback::onComplete` 才是 `0x1` = `TF_ONE_WAY`）。

**d) parcelable 字段序（易错点）**

- `RealityTapHe`：`getDataPosition` → `Int32(size 占位)` → `writeInt32Array(values)` →
  4 × `Int32`（arg1..arg4）→ `setDataPosition` 回填 `size`。
- `RealityTapRtp`：`Int32(size 占位)` → `writeParcelFileDescriptor(fd)` → `Int32(arg1)` → 回填。
- `ParcelableMemory`：`Int32(size 占位)` → `Int32(1)`（**`AParcel_writeParcelable` 写的
  非空标记**）→ `common::MappableFile::writeToParcel` → `AParcel_writeString(name)` → 回填。
  `MappableFile` 的实现不在 `dg.so` 里（`NEEDED` 有 `android.hardware.common-V2-ndk.so`），
  故直接复用 AOSP 定义并静态编入。

**e) 原厂 rc / VINTF / SELinux 命名**

- `$V/etc/init/vendor.awa.realitytap.service.rc`：`service vendor.awa.realitytap` +
  `interface aidl vendor.awa.reality.IRealityTap/default` + `class hal` +
  `on init/on boot` 两段 `chmod/chown /dev/awa_realitytap_dev`。
- `$V/etc/vintf/manifest/vendor.awa.realitytap.service.xml`：manifest 8.0，
  `<name>vendor.awa.reality</name>`、`<version>3</version>`、`<fqname>IRealityTap/default</fqname>`。
- `$V/etc/init/vendor.qti.diag.rc`：`on early-boot start vendor.diag-router` +
  `service vendor.diag-router /vendor/bin/diag-router`（`capabilities KILL`、`disabled`）。
- `$V/etc/vintf/manifest/vendor.qti.diag.hal.service.xml`：manifest 8.0，
  `<name>vendor.qti.diaghal</name>`、`<fqname>Idiag/default</fqname>`（**无 `<version>`**）。
- 原厂 `vendor_service_contexts`：第 71 行 `Idiag/default → vendor_hal_diaghal_service`、
  第 138 行 `realitytap → realitytap_service`、第 139 行
  `IRealityTap/default → hal_realitytap_service`。
- 原厂 `vendor_file_contexts` 第 162 行
  `/(vendor|odm|system/vendor)/bin/hw/vendor\.awa\.realitytap\.service → hal_realitytap_default_exec`；
  第 1073 行 `/dev/awa_realitytap_dev → realitytap_device`。
- 原厂 cil：`(typeattributeset vendor_service (hal_realitytap_service))`（1249）、
  `hal_realitytap` / `hal_realitytap_server` 属性（1290/1294）、
  `(type vendor_hal_diaghal_service)`（2415），其余是设备节点/驱动相关的
  `realitytap_device`、`vendor_hal_diaghal_client/server`、`vendor_diag_device`、`mhi_*` 等。

## 4. 改动

1. **引擎：允许「模块 AIDL 根 + deps 平台包」共存**（`engine/vendor/build/internal/halbuild/`）
   - `build.go`：删 `AIDLRoot()`，新增 `moduleAidlRoot()` / `aidlRoots() []string` /
     `aidlDir(rel)` / `isDir()`；`checkDeps` 的 deps 就绪门槛去掉「非 `aidlRootModule`」的
     限制（只要 `depsHal` 非空就要快照就绪），模块包 `.hash` 校验按 `aidlDir(pkg/ver)`
     定位后再用 `strings.HasPrefix(baseDir, moduleAidlRoot()+sep)` 判断归属——
     落在 deps 快照根里的跨包包交给 `deps.AIDLReady` 统一校验。
   - `codegen.go`：`-I` 改为遍历 `aidlRoots()` 逐个追加；包目录 `b.aidlDir(pkg/ver)`；
     `FrameworkTypes` 用 `b.aidlDir(rel)`。
   - `profile.go`：halTable 增 `"common"`（下载 `common/aidl/aidl_api`，包
     `android.hardware.common` v2）——`MappableFile` 是跨 HAL 共用 parcelable。
   - 离线播种 `engine/vendor/out/android16/aosp/aidl/common/`（从 `…/media/android.hardware.common`
     复制 `{1,2,current}` + 写 `.aidl-complete`），`deps -check` 通过。
2. **新模块 `vendor.awa.reality`**（`engine/vendor/hardware/vendor.awa.reality/`）
   - `aidl/vendor.awa.reality/3/`：`IRealityTap.aidl`、`RealityTapHe.aidl`、
     `RealityTapRtp.aidl`、`.hash`=054e7907…；方法按上表声明序（= 事务号）。
   - `src/realitytap_service.cpp`（111 行）：`BnRealityTap` 全方法回 0；
     `performTap`/`performRtp` 若回调非空立即 `onComplete()`；不 open 任何设备节点。
   - `vendor/etc/init/vendor.awa.realitytap.service.rc`：只搬原厂 `service` 段 +
     `seclabel u:r:hal_realitytap_default:s0`；**丢弃** `on init`/`on boot` 的
     `/dev/awa_realitytap_dev` chmod/chown（云机无该字符设备）。
   - `vendor/etc/vintf/manifest/…xml`：原厂逐字（8.0 / version 3）。
   - `sepolicy/`：`hal_realitytap_default` + `_default_exec` + `hal_realitytap_service`
     （**逐字沿原厂类型名**）+ init 启动链 + `add/find` + system_server 双向 binder；
     service_contexts 一行。
   - spec：`kind=aidl`、`depsHal=vibrator`、`aidlPackageSubset` 只取
     `EffectStrength.aidl` + `IVibratorCallback.aidl`、`aidlRootModule=true`。
3. **新模块 `vendor.qti.diaghal`**（`engine/vendor/hardware/vendor.qti.diaghal/`）
   - `aidl/vendor.qti.diaghal/1/`：`IIdiag.aidl`（close/ioctl/open/write）、
     `IIdiagcallback.aidl`、`ParcelableMemory.aidl`（`common.MappableFile file; String name;`）、
     `.hash`=1180103e…。
   - `src/diaghal_service.cpp`（89 行）：`open` 存回调回 0、`close` 回 0 并 reset、
     `ioctl`/`write` 回 -1（无 `/dev/diag`），从不 `send_data`。
   - rc：服务名/二进制改成 `vendor.default.diaghal` / `vendor.qti.diaghal-service`，
     不搬 `capabilities KILL` 与 `disabled`+early-boot（没有 diag-router 依赖），
     补 `interface aidl` + `seclabel u:r:vendor_hal_diaghal_default:s0`。
   - VINTF 原厂逐字；sepolicy 用 `vendor_hal_diaghal_default/_default_exec/_service`
     （服务名类型沿原厂同名）。
   - spec：`depsHal=common`、`aidlPackageSubset` 只取 `MappableFile.aidl`。
4. **声明**：`device.json` 的 `hardware` 追加 `vendor.awa.reality@default`、
   `vendor.qti.diaghal@default`（34 → **36** 项）。
5. **机型层下放**（`devices/manifest/flyme/m22/sepolicy/`）
   - `vendor_service_contexts`：删 `IRealityTap/default`、`Idiag/default` 两行；
     `realitytap` 改用**原厂类型名** `realitytap_service`（27 行不变）。
   - `vendor_sepolicy.cil`：删 `hal_awa_reality_service` / `hal_idiag_service` 的
     type/roletype/typeattributeset 与两条 `find`；`hal_realitytap_service` 块改名
     `realitytap_service`（3 行 + 1 条 `add find`）；`allow vendor_dpmd hal_idiag_service (find)`
     连同 `(type vendor_dpmd)` 一并删除（diaghal 的 `find` 已由模块层
     `(allow domain vendor_hal_diaghal_service (find))` 覆盖，本基底也没有 `vendor_dpmd`）。
     94 → 86 行。

## 5. 验证

- `hal list`：`vendor.awa.reality aidl -> vendor.awa.realitytap.service`、
  `vendor.qti.diaghal aidl -> vendor.qti.diaghal-service`。
- `hal build`：reality `(v3, hash 054e79077142)` + vibrator `(v2, hash ea8742d6993e)`，
  编 5+1 源文件 → 产物 **140,504 B**；diaghal `(v1, hash 1180103e4052)` +
  common `(v2, hash c32ddfdeb69c)`，编 4+1 → **122,472 B**。
  两者 `NEEDED` 都只有 `libc++/libbinder_ndk/liblog/libdl/libm/libc`（**无**原厂 so）。
- **wire 回放**（自研 vs 原厂，按 code 逐条）：`BpRealityTap` 1..5 + 16777214/16777215、
  `BpIdiag` 1..4、`BpIdiagcallback` 1 全部 `OK`；写序逐条一致
  （见 §3c）；parcelable 写序一致，含 `AParcel_writeParcelable` 的 `Int32(1)` 非空标记、
  `EffectStrength` 走 `AParcel_writeByte`（`writeData<int8_t>`）、
  `ParcelData.Fd` 走 `AParcel_writeParcelFileDescriptor`。
  唯一差异：自研 transact flags=0（未定义 `BINDER_STABILITY_SUPPORT`），
  原厂为 `FLAG_PRIVATE_LOCAL`；该 flag 只影响本进程内短路判断，不进 parcel。
- **07 exit=0**：步骤 3 两模块「指纹不一致 → 重编」；步骤 4 各写入 3 项；
  步骤 6 追加模块层 cil（reality 3,245 B / diaghal 3,407 B）与 service_contexts
  （770 B / 525 B）；6b 新增 `vendor.awa.realitytap.service -> hal_realitytap_default_exec`、
  `vendor.qti.diaghal-service -> vendor_hal_diaghal_default_exec`；
  装配树 664 → **670**（文件 439 / 链接 199 / 目录 32）；
  `vendor.img` 206,413,824 → **206,704,640 B**（`b2245b13…`）。
- **镜像回读**：六个新条目 dump 后 sha 与 `out/vendor/android16` 产物 **6/6 SAME**；
  `ea_get security.selinux` = `u:object_r:hal_realitytap_default_exec:s0`（0755/0:2000）、
  `u:object_r:vendor_hal_diaghal_default_exec:s0`（0755/0:2000），rc/VINTF 四件均
  `vendor_configs_file`；`vendor_file_contexts` 派生 2 行（带转义点号）；
  `(type hal_realitytap_service)` / `(type vendor_hal_diaghal_service)` /
  `(type realitytap_service)` 在镜像 cil 内各出现 **1 次**（模块层两块 + 机型层一块）；
  `hal_awa_reality_service` / `hal_idiag_service` 只剩 3 处注释命中、**0 条**声明与
  `allow`；`{{vers}}` 残留 **0**（已替换为 202504）。
- 判据：装配树条目 + 逐文件 sha + 镜像 xattr（镜像 sha 不作一致性判据，mke2fs 非确定性）。

## 6. 遗留

1. **运行期未复验**：`init.svc.vendor.awa.realitytap=running` / `…vendor.default.diaghal=running`、
   `vintf` 里两个 fqname 可见、Flyme `RealityTapManagerService` 与 `DiagManagerService`
   `getService` 成功、avc denied 0、`secilc` 现场编译通过（本机无 `secilc`，
   只做了「类型声明唯一 + 平台符号存在」的静态核对）。
2. **语义是空实现**：reality 不产生任何振动、`getF0` 恒 0、`cancel`/`calibrate` 恒成功；
   diaghal `ioctl`/`write` 恒 -1（无 `/dev/diag` 与 MHI 通道），`open` 只存回调不 `send_data`。
   若 Flyme 侧依赖真实波形/日志抓取，会看到「调用成功但无效果」。
3. **未搬的原厂授权**：`/dev/awa_realitytap_dev`（`realitytap_device`）、diag 的
   `vendor_diag_device` / `mhi_*_pipe_4` / `mdlog_vendor_data_file`、
   `vendor_hal_diaghal_client/server` 属性、`vendor_dpmd` 的 find —— 全是绑本机
   PCIe/MHI 拓扑与驱动的事实，接硬件时再按「板级事实留机型层」放回。
4. **`realitytap` 客户端授权比原厂窄**：原厂另有 `platform_app` / `untrusted_app →
   realitytap_service (find)` 与 `system_app → hal_realitytap_default (binder call transfer)`；
   本轮只保留 `system_server → realitytap_service (add find)`（既有行为），
   若实机出现应用侧 `find` denied 再按原厂补。
5. **引擎改动的回归面**：`aidlRoots()` 改变了 `-I` 顺序与包归属判定，已用
   `common` / `vibrator` / `media` 三类模块各跑一次 `hal build` 验证，但
   `kind=aidl` 全量（36 模块）复跑仍靠 07 步骤 3 的「指纹不一致才重编」保证——
   引擎后续再动 codegen 时应整体 `--refresh` 一次。
6. `patch.img` / `out/dist` 仍是旧版（09/11 未跑）。
