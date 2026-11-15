# issue-m22-hardware-list-align — device.json 的 hardware 清单与 engine/vendor/hardware 双向对齐

- **设备**：`flyme/m22` 12.5.0.0A（QTI `sun`）
- **构建侧**：`engine/vendor`（cdh-vendor 源码树）↔ `devices/manifest/flyme/m22/device.json`
- **状态**：**已完成**（2026-10-29，m22 实例 36：6 个新服务全部 `running`、蓝牙可开启、开机 71.4 s）
- **日期**：2026-10-29
- **关联条目**：#53 [`issue-m22-hal-gap-fill`](../issue-m22-hal-gap-fill/solution.md)（上一轮把 15→26 条）、
  #54 [`issue-m22-bootctl-vold-deadlock`](../issue-m22-bootctl-vold-deadlock/solution.md)（boot 被撤下的原因）、
  #55 [`issue-m22-hal-exec-label`](../issue-m22-hal-exec-label/solution.md)（新二进制的 `_exec` 标签来自这里）

## 1. 问题

用户要求把两边的 HAL 名单对齐，原话：

> `engine/vendor/hardware` 有的你补到 `device.json` 里啊，没有的在 `hardware` 添加啊

即一次**双向**对齐：

1. **正向（hardware → device.json）**：`engine/vendor/hardware` 下有源码的模块，m22 该勾而没勾的补进
   `device.json` 的 `hardware` 列表。对齐前差集 **7 个**：
   `android.hardware.bluetooth`、`android.hardware.boot`、`android.hardware.fastboot`、
   `android.hardware.gpu`、`android.hardware.neuralnetworks`、`android.hardware.security`、
   `vendor.qti.hardware.perf2`。
2. **反向（device.json/机型 → hardware）**：m22 需要、但 `engine/vendor/hardware` 里根本没有源码的，
   在 engine 侧新建。缺 **3 个**：`android.hardware.power.stats`、`android.hardware.secure_element`、
   `vendor.meizu.sensor_aux`（后者在机型 CIL 里还留着「引擎侧尚无该桩模块」的注释）。

判据（沿用 #53）：**能挂 = ①服务域（`hal_X_default` + `_exec`）在「lineage 底座 + 机型片段」装配树策略里存在
②有 rc（VINTF 片段可选）③不引入 `status 127` / `waitForService` 互锁**。三条不满足的宁可不加——
加错的代价是整份 device manifest 不可用或 system_server 重启循环（#32/#35 的前车之鉴）。

## 2. 根因

### 2.1 正向差集：7 个模块逐个判定，只有 3 个该加

| 模块 | 域 / `_exec` | 自带 rc/VINTF | 判定 | 依据 |
| :--- | :--- | :--- | :--- | :--- |
| `android.hardware.bluetooth` | `hal_bluetooth_default` ✓ | rc + 同名 xml ✓ | **加** | gold 线在用；同名片段覆盖底座 `bluetooth-service-default.rc/.xml`，不会 `Conflicting FqInstance` |
| `android.hardware.boot` | `hal_bootctl_default` ✓ | rc + xml ✓ | **加，但先改时序** | 装配树 VINTF 无 `IBootControl` 声明（不冲突）；`class hal` 会重演 #54 的 `vdc checkpoint` × `waitForService` 死锁 |
| `android.hardware.fastboot` | `hal_fastboot_default` ✓ | **无 rc/VINTF** | **不加** | 仅 `fastbootd` 用；m22 走 patch.img 覆盖分区，不进 fastbootd，收益 0、白搭一个服务名 |
| `android.hardware.gpu` | — | 纯编译插件（`kind: gpu`） | **不加** | 模块 json 里写死 `Mali-G57 MC2` / `vulkan.mali.so`；m22 是高通 + mesa（`virtio-gpu` venus），装上等于宣告一台不存在的 Mali |
| `android.hardware.neuralnetworks` | 装配树**无** `hal_neuralnetworks_default(_exec)` | rc 有 | **不加** | 域不存在 → init `cannot execv` → `status 127`（#55 的 1773 次即此类），NN 也无人使用 |
| `android.hardware.security` | — | rc + xml | **不加** | 实例 34 `service list` 已有 `ISecureClock/default`、`ISharedSecret/default`（keymint 提供）；再加一遍是争抢同名实例 |
| `vendor.qti.hardware.perf2` | 装配树**无** `hal_perf2_default` / `vendor_hal_perf_default` | **无 rc** | **有条件加** | 实现（`engine/vendor/hardware/vendor.qti.hardware.perf2`）在，但机型 CIL 早给了名字专用类型 `cdh_perf2_service` + system_server/lmkd 的 `find`；用 `hal_sensors_default` 域跑，补一条 `add` 授权 + 机型层 rc 即可 |

### 2.2 反向缺口：3 个模块在 engine 侧不存在，且缺 AIDL 下载源

- `power.stats`、`secure_element` 在 `engine/vendor/build/internal/profile/profile.go` 的 `halTable` 里
  **没有条目** ⇒ `out/android16/aosp/aidl/` 下没有对应快照，源码无法 `#include <aidl/...>`，`hal build` 直接编不过。
- `vendor.meizu.sensor_aux` 的 AIDL 由厂商自带（旧设备链 `devices/flyme/m22/build/vendor/source/` 有 8 个 `.aidl`
  且已在 slot 11 实测通过），但没有模块 json；它的包没有 `.hash`，只能走 `extraAidlRoots` 范式
  （与 `android.hardware.biometrics` 的 virtualhal 同口径），不能走 `depsHal`。

### 2.3 顺带暴露的三个构建/集成卡点

1. **`out/bin/cdh-vendor` 身份错**：它与 `out/bin/cdh` 的 md5 完全相同（`dd8f8077…`，7,216,162 B），
   07 的 `ensureCompiler` 只判「文件存在」→ 一路跑到步骤 3 才以 `[err] 未知子命令: hal` 失败，
   白跑一遍解包与复制。
2. **boot 的启动时机**：#54 的互锁不是代码问题，是 `class hal`（boot 阶段）与 `late-fs` 的
   `vdc checkpoint`（init 同步等 vold，vold 又等 `IBootControl`）撞在一起。改成 `on post-fs` 显式 start 即可绕开。
3. **蓝牙通道的 SELinux 授权**：基座给 `hal_bluetooth_default` 的蓝牙设备授权只有 `bt_device` / `rfkill_device`，
   virtio-console 通道（运行时标签 `console_device`）没有——但同族的 radio(`/dev/hvc4`)、wifi(`/dev/hvc9`)
   都各有一条。缺这条时 HAL 进程活着、服务也注册了，只是 framework 每次 enable 都失败重试。

## 3. 证据

### 3.1 差集（对齐前）

```
$ python3 -c "…对比 engine/vendor/hardware 与 device.json 的 hardware…"
count= 25
dir-only: ['android.hardware.fastboot', 'android.hardware.gpu',
           'android.hardware.neuralnetworks', 'android.hardware.security',
           'android.hardware.bluetooth', 'android.hardware.boot',
           'vendor.qti.hardware.perf2']
json-only: []
```

### 3.2 对齐前的运行期计数（实例 34，`20:52:48` → `21:30:07`，约 37 分钟）

| 现象 | 计数 | 说明 |
| :--- | ---: | :--- |
| `Waiting for service 'vendor.meizu.sensor_aux.ISensorAux/default'` | 892 | 每 ~2.4 s 一次；引擎侧无该模块 |
| `Could not find vendor.qti.hardware.perf2.IPerf/default … VINTF` | 472 | 只有标签、无实体 |
| `Watchdog triggered`（`NfcService` 自杀循环） | 96 行 / 24 次 | 严格 90 s 一次（`20:54:04` → `21:28:39`），与本变更无关 |
| `Waited one second for …IBootControl` / `Too many pending` | 0 / 0 | boot 已在 #54 从 `device.json` 撤下，互锁不存在 |
| `Waited one second for …IBluetoothHci` | 22 | 本盘尚缺 `console_device` 授权（见 2.3-3 / 3.5），每次 enable 重试等 1 s |

> 注：#54 记录的 144 次 `Waited one second` 发生在**实例 32**（boot 还在清单里时），不是实例 34。

### 3.3 `cdh-vendor` 身份错（首轮 07 实跑）

```
==> 编译 HAL 模块 android.hardware.bluetooth（变体 default，待落盘 bin/hw/android.hardware.bluetooth-service, …）
  + out/bin/cdh-vendor hal build android.hardware.bluetooth --android 16 --out-dir …/out/vendor/android16
[err] 未知子命令: hal（cdh help 查看用法）
[err] 阶段 build_vendor 失败: HAL 模块 android.hardware.bluetooth 编译失败: … 退出码非零（exit status 1）

$ md5 -q out/bin/cdh out/bin/cdh-vendor
dd8f80773f437f88784c0ba17a1a3893
dd8f80773f437f88784c0ba17a1a3893      ← 同一个二进制
```

### 3.4 蓝牙：服务在、通道不通（实例 35，首轮对齐后）

```
service list: 29  android.hardware.bluetooth.IBluetoothHci/default
init.svc.vendor.bluetooth-default = running
dumpsys bluetooth_manager:
  enabled: false / state: OFF / "Bluetooth never enabled!"
logcat / 宿主串口（framework 每次 enable 起一个新进程重试，5 次尝试共 12 行全同；
实例 35 已停，这里取自 `35/qemu.log`）：
  W bluetooth: system/gd/hal/hci_backend_aidl.cc:37 initializationComplete: status != AidlStatus::SUCCESS
```

`initializationComplete` 带非 `SUCCESS` 只有一个来源——本模块 `initialize()` 的
`openDevice()` 失败分支（`Status::UNABLE_TO_OPEN_INTERFACE`，`bluetooth_service.cpp:203-209`）。

### 3.5 装配树里 `hal_bluetooth_default` 的设备授权

```
(allow hal_bluetooth_default bt_device_202504 (chr_file (ioctl read write open)))
(allow hal_bluetooth_default rfkill_device (chr_file (ioctl read write getattr lock append map open watch watch_reads)))
                       ← 没有 console_device；而 radio/wifi 各有等价一条
(allow hal_radio_default console_device_202504 (chr_file …))
(allow hal_wifi_default  console_device_202504 (chr_file …))
```

### 3.6 不注入 VINTF 的影响面（实例 36）

`perf2` 的 `Could not find … VINTF` 227 行仍存在（实例 36，开机 → `20:59:51`；`Waiting for service` 为 0）：这是「未在 VINTF 声明」的
**快速失败**回话，调用方不睡（#52 已证）；声明一个不在 FCM 里的包反而会把 `getService` 拖成
`waitForDeclaredService` 阻塞。故 power.stats / secure_element / sensor_aux / perf2 一律不带 VINTF 片段。

## 4. 改动

### 4.1 新建的 HAL 模块（`engine/vendor/hardware/`）

| 文件 | 内容 |
| :--- | :--- |
| `android.hardware.power.stats/src/power_stats_service.cpp` | `IPowerStats` **v2**（Android 16 FCM 202504 精确钉 v2）六个方法全回空数组 |
| `android.hardware.power.stats/vendor/etc/init/android.hardware.power.stats-service.rc` | `disabled` + `on post-fs-data start`，域 `hal_power_stats_default` |
| `android.hardware.secure_element/src/secure_element_service.cpp` | `ISecureElement/eSE1` 空实现：`isCardPresent=false`，`init()` 里 oneway 回 `onStateChange(false)`，`openBasicChannel/openLogicalChannel/transmit` 回空 |
| `android.hardware.secure_element/vendor/etc/init/android.hardware.secure_element-service.rc` | 域 `hal_secure_element_default`，`disabled` + `post-fs-data` |
| `vendor.meizu.sensor_aux/aidl/vendor/meizu/sensor_aux/*.aidl`（8 个） | 从旧设备链 `devices/flyme/m22/build/vendor/source/vendor.meizu.sensor_aux/` 原样移植 |
| `vendor.meizu.sensor_aux/src/sensor_aux_service.cpp` | `BnSensorAux` 十方法桩；注释记录「5 s 持闸 → oneway 预算贴顶 → system_server 崩」的根因链（#52） |
| `vendor.meizu.sensor_aux/vendor/etc/init/vendor.meizu.sensor_aux-service.rc` | 域 `hal_sensors_default`，`disabled` + `post-fs-data` |
| `android.hardware.bluetooth/sepolicy/vendor_sepolicy.cil` | `(allow hal_bluetooth_default console_device_{{vers}} (chr_file …))`（新增，理由见 3.4/3.5） |

### 4.2 模块清单（`engine/vendor/build/`）

- `modules/android.hardware.power.stats.json`：`depsHal: "power.stats"`、`aidlPackageVersions {…: "2"}`
- `modules/android.hardware.secure_element.json`：`depsHal: "secure_element"`、版本 `"1"`
- `modules/vendor.meizu.sensor_aux.json`：`depsHal: ""` + `extraAidlRoots: [{root: "aidl", packages: ["vendor/meizu/sensor_aux"], version: "1"}]`
- `internal/profile/profile.go`：`halTable` 在 memtrack 之后新增 `power.stats`（v2）与 `secure_element`（v1）两个 AIDL 下载源

### 4.3 机型集成层（`devices/manifest/flyme/m22/`）

- `device.json`：`hardware` **25 → 31 条**（+bluetooth、+boot、+power.stats、+secure_element、
  +vendor.meizu.sensor_aux、+vendor.qti.hardware.perf2）；`hvc` 段补 `"host-hvc-bluetooth": "hvc5"`
  （对齐 `vendor/etc/ueventd.qcom.rc` 里「hvc5/hvc6 蓝牙与马达」的板级布局，gold 同值）。
- `vendor/etc/init/vendor.qti.hardware.perf2-hal-service.rc`（新建）：机型层给 perf2 一个实体，
  域 `hal_sensors_default`、`disabled` + `post-fs-data`、不注入 VINTF。
- `sepolicy/vendor_sepolicy.cil`：补 `(allow hal_sensors_default cdh_perf2_service (service_manager (add find)))`
  （类型与客户端的 `find` 早已存在，缺的只是服务端注册权限）。
- `vendor/hardware/android.hardware.boot/vendor/etc/init/android.hardware.boot-service.default.rc`：
  加 `on post-fs / start vendor.boot-default`，并在 rc 顶部写明 #54 的三方互锁。

### 4.4 构建器鲁棒性（`engine/build/07_build_vendor/hal.go`）

`ensureCompiler` 从「判文件存在」改成「判文件存在 **且** 跑一次 `help` 的用法里含 `cdh-vendor`」
（新增 `isVendorCompiler`）。避免下一次再把主 CLI 误复制成 `cdh-vendor` 时白跑一遍装配。

### 4.5 单模块验证（旁证，非最终态）

```
$ /tmp/cdh-hal hal build android.hardware.power.stats --android 16 --out-dir /tmp/haltest
[ok] android.hardware.power.stats 构建完成（v2, hash c3e113101b73）
$ … secure_element  → v1, hash 3430a4053a6d
$ … vendor.meizu.sensor_aux → 模块自带 no-hash
```

## 5. 验证

**盘集**：07→09→11 重跑，`vendor.img` 206,651,392 B（上一轮 206,647,296 B）、
`patch.img` 402,653,184 B / sha256 `e51ecc57cba9`、`vda.img` 13.00 GiB 五分区回读一致。
07 步骤 3 输入行确认 `hardware 31 项`，步骤 6b 补 **18 条** `_exec` 标签（含 6 个新二进制）。

**实例 36（m22，`sys.boot_completed=1`，`bootstat --record_boot_complete` @ 71.43 s）**

| 判据 | 结果 |
| :--- | :--- |
| 6 个新服务 init 状态 | `vendor.boot-default`(21.07 s，post-fs) / `vendor.power.stats-default`(26.67) / `vendor.secure_element-default`(26.67) / `vendor.bluetooth-default`(26.70) / `vendor.sensor_aux-default`(26.71) / `vendor.perf2-default`(26.71) 全部 **running**，各只启动 **1** 次、无退出 |
| `service list` | `IBluetoothHci/default`、`IBootControl/default`、`IPowerStats/default`、`ISecureElement/eSE1`、`ISensorAux/default`、`IPerf/default` 全在 |
| 框架侧随之而来 | `powerstats`（`android.os.IPowerStatsService`）、`secure_element`（`android.se.omapi.ISecureElementService`）、`android.se.omapi.ISecureElementService/default` 三者 `service check` = found |
| `ls -Z /vendor/bin/hw` | 6 个新二进制分别为 `hal_bluetooth_default_exec` / `hal_bootctl_default_exec` / `hal_power_stats_default_exec` / `hal_secure_element_default_exec` / `hal_sensors_default_exec` ×2 |
| **蓝牙可用** | `cmd bluetooth_manager enable` → `state: ON`、`address: XX:XX:XX:XX:17:00`、`Bluetooth crashed 0 times`；`initializationComplete` 告警 **0** 行 |
| sensor_aux 等待 | `Waiting for service '…sensor_aux…'` **892 → 0** |
| perf2 等待 | `Waiting for service '…perf2…'` **0**；`Could not find …VINTF` 472（实例 34，37 min）→ 227（实例 36，8.4 min，仍为快速失败，见 3.6） |
| boot 互锁 | `Waited one second for …IBootControl` **0**；vold `Using AIDL version of IBootControl` + `Found … in device VINTF manifest` |
| `Too many pending` / `cannot execv` / `not found in VINTF` / `entrypoint` 拒绝 | **0 / 0 / 0 / 0** |
| `status 127`（`exited with status 127`） | **0**；`exited with status` 共 64 行 = `status 0` 55 / `status 1` 8 / `status 254` 1，全为既有 oneshot（`modprobe zram.ko`、`setup_sound_card`、`qccsyshal@1.2`、`cppreopts`、`misctrl`、`recovery-refresh`…） |
| `Waited one second` 总计 | 18 行，目标**全是 `activity`**（开机后应用启动期，属正常） |
| `Watchdog triggered` | **10 次**（20:54:06 / 20:55:36 / 20:57:06 / … / 21:07:38，每 4 行交错重复，严格 90 s 一次）；实例 34 为 24 次。这是 `com.android.nfc` 在 `enableInternal` 上的既有循环，本变更前后同因（见 6.3，已由 #57 修复） |
| UI 可用 | `am start -a android.settings.SETTINGS` → OK；`input keyevent HOME` → OK |
| 传感器 | `dumpsys sensorservice`：29 h/w sensors 全 running |
| avc（新名字） | `cdh_perf2_service` 722 次（见 6.1）、`cdh_sensor_aux_service` **6** 次 |

## 6. 遗留

1. **perf2 的客户端 `find` 缺口（722 次 avc/8.4 min）**：`mediaserver` 421、`platform_app` 178、`priv_app` 22、
   `nfc` 18、`vendor_perfservice` 17、`radio` 9、`mediaprovider` 9…。它们拿到的是「不可用」——与对齐前**同档**
   （上一轮只有标签无实体，denial 一样存在），所以不是回归；但要让这些客户端真正吃到空实现，
   得按清单补 `find`。代价是 `platform_app`/`priv_app` 属平台应用域，是否开这个面需要单独决策（未做）。
2. **perf2 的 227 行 VINTF 快速失败**：不注入 VINTF 的必然产物（调用方不睡，见 3.6）。若某天要消音，
   唯一安全做法是把片段加进 FCM，但那会把 `getService` 变成 `waitForDeclaredService`——先算清风险再动。
3. **NFC 的能力面**：`dumpsys nfc` 仍 `FAILED_TRANSACTION`（`android.hardware.nfc` 桩的 dump 未实现，
   与 #55 记的 vendor 域 fd `use` 被拒同族）；`Watchdog triggered` 在本盘实测 **10 次**（20:54:06 → 21:07:38，
   每 90 s 一次，`enableInternal` 32 行 / `FatalError` 24 行），即 90 s 自杀循环**当时仍在**——
   根因是 `android.hardware.nfc` 空桩的 `open()` 只返回异常、不回 `OPEN_CPLT` 事件，
   `NfcAdaptation::HalOpen` 便死等 90 s 被 Watchdog abort。
   **已由 [#57](../issue-m22-nfc-watchdog-loop/solution.md) 修复**（异步回 `OPEN_CPLT(FAILED)`），
   并顺带修了本条暴露的「07 复用判据不看源码」缺陷。
4. **观察窗**：实例 36 本表多数计数取自 **8.4 min** 快照；之后续跑到 **13.5 min**（21:07:38 停，
   `Watchdog triggered` 累计 10 次，见 6.3）。`#52` 的自动亮度 / binder async 池话题未在本盘复验；
   长稳（≥30 min）需要一次专门跑。
5. **08/10 未跑**：沿用 10-28 的 `vendor_dlkm` 与 `super` 口径（`--skip build_vendor_dlkm --skip pack_super`）。
6. **不加的 4 个模块的判定有时效**：`neuralnetworks`（缺域）、`gpu`（Mali 编译插件）、`security`（与 keymint 争名）、
   `fastboot`（无使用者）都基于当前装配树与策略。若以后补 `hal_neuralnetworks_default` 域或给 gpu 插件加机型参数，
   需要重新判定，不能直接照搬本表。
