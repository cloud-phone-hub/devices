# #68 m22 `vendor.meizu.hardware.meizu_charging`：按原厂 `.so` 反解 AIDL 并自写实现

- 设备：m22（MEIZU 22 / Flyme 12.5.0.0A / Android 16 / QTI `sun`）
- 时间：2026-10-29
- 产物：`engine/vendor/hardware/vendor.meizu.hardware.meizu_charging/`（7 个 .aidl + 源码 + rc + VINTF + sepolicy）、
  `engine/vendor/build/modules/vendor.meizu.hardware.meizu_charging.json`、
  `out/vendor/android16/bin/hw/vendor.meizu.hardware.meizu_charging-service`（104,800 B / `a3419220…`）、
  `out/devices/flyme/m22/12.5.0.0A/img/vendor.img`（**206,413,824 B** / `b6af3300…`）
- 接续：#67（同一套逆向模板，aw_light）；与 #67 同批，机型层只剩 `realitytap` 与 `diaghal`

## 1. 问题

`manifest/flyme/m22/sepolicy/vendor_service_contexts` 里的
`vendor.meizu.hardware.meizu_charging.IMeizuCharging/default` 只有标签、没有实体：
镜像里没有 `/bin/hw/…-service`、没有 rc、没有 VINTF、没有 `*_exec` 标签。
Flyme 的电池/充电设置（充电上限、SOH、UPS、反向无线充、无线档位/风扇）全部拿不到服务。

## 2. 根因

- 引擎 `sepolicySources()` 只遍历 `device.json` 声明的模块，没有模块宿主 → 片段无处落、
  `hal list` 里也没有该模块；
- MEIZU 未公开该 HAL 的 AIDL 源码，原厂只给
  `lib64/vendor.meizu.hardware.meizu_charging-V1-ndk.so`（85,200 B）+ `bin/hw/…-service`；
- 原厂 rc 里还有三个 chmod/chown 本机充电 sysfs 的触发器段，云机 virtio 底座没有这些节点。

## 3. 证据（逆向事实）

反汇编源：`out/tmp/unpack-vendor_a/flyme-m22/lib64/vendor.meizu.hardware.meizu_charging-V1-ndk.so`。

| code | 方法 | 入参 | 返回 | 推导签名 |
|---|---|---|---|---|
| 1 | `getWirelessTxModel` | — | String | `String ()` |
| 2 | `setReverseWirelssChargingMode` | Int32（枚举） | Bool | `boolean (ReverseWirelssChargingMode)` |
| 3 | `setWiredCharingLevel` | Int32（枚举） | Bool | `boolean (WiredCharingLevel)` |
| 4 | `setWirelessChargingLevel` | Int32（枚举） | Bool | `boolean (WirelessChargingLevel)` |
| 5 | `setWirelessTxFanMode` | Int32（枚举） | Bool | `boolean (WirelessTxFanMode)` |
| 6 | `getBatterySOH` | — | Int32 | `int ()` |
| 7 | `setBatteryUPSMode` | Int32（枚举） | Bool | `boolean (BatteryUPSMode)` |
| 8 | `getCharingThresholdCtrl` | — | Int32 | `int ()` |
| 9 | `setCharingThresholdCtrl` | Int32（枚举） | Bool | `boolean (CharingThresholdCtrl)` |
| 10 | `getCharingThreshold` | — | Int32 | `int ()` |
| 11 | `setCharingThreshold` | Int32 | Bool | `boolean (int)` |

- 11 条 transact 的 `w4` 全为 `0x10000000`（同步）；没有 out 参数、没有 parcelable。
- 6 个枚举入参在 wire 上都是 `AParcel_writeInt32`，且 `.so` 里没有它们的 `writeToParcel`
  → 是 AIDL `enum` 而不是 parcelable；成员名/取值在原厂二进制里不可得（无 toString 符号）。
- 接口 hash：`strings -a` → `be64757e0ec349824b9e76e8982900f3c680e1d7`，逐字写进 `1/.hash`。
- 原厂策略事实：域 `hal_meizu_charging_default`、exec `hal_meizu_charging_default_exec`、
  服务名类型 `hal_meizu_charging_service`；另有 `hal_meizu_charging_client/hwservice` 与
  `/sys/class/meizu/charger/**` 的标签（真硬件才需要）。

## 4. 改动

1. **新建模块**（`kind=aidl`，`workSubdir=meizu-charging-ndk-work`）：
   - `aidl/vendor.meizu.hardware.meizu_charging/1/`：`IMeizuCharging.aidl` + 6 个枚举
     （`BatteryUPSMode` / `CharingThresholdCtrl` / `ReverseWirelssChargingMode` /
     `WiredCharingLevel` / `WirelessChargingLevel` / `WirelessTxFanMode`，各含一个
     `UNKNOWN = 0` 占位成员）+ `.hash`。枚举只为通过类型检查：本实现不解释取值，
     wire 恒为 int32，接口 hash 由 `.hash` 固定。
   - `src/meizu_charging_service.cpp`（148 行）：set 系列记内存并回 `true`；
     get 系列**回显最近一次设置**（SOH 默认 -1、阈值/控制默认 0），
     `getWirelessTxModel` 回空串；不 open 任何 sysfs/设备节点。
   - `vendor/etc/init/…-service.rc`：只搬原厂 `service` 段（`class late_start`、
     user/group system）+ `interface aidl` + `seclabel u:r:hal_meizu_charging_default:s0`；
     **刻意丢弃**原厂 `on property:sys.run.charger_file_perms` / `on boot` / `on charger`
     三段——全是 chmod/chown 本机 Meizu 充电 sysfs 且带 `wait <路径>`，云机没有这些节点，
     chmod 只会刷噪音、`wait` 会卡住 trigger 队列。
   - `vendor/etc/vintf/manifest/…-service.xml`：原厂逐字（`version="8.0"`）。
   - `sepolicy/`：域 + exec + 服务名类型 + init 启动链 + `add/find` + system_server 双向 binder；
     service_contexts 一行。
2. **声明**：`device.json` 的 `hardware` 加 `vendor.meizu.hardware.meizu_charging@default`（33 → 34 项）。
3. **机型层下放**（同批）：删 `hal_meizu_charging_service` 的 type / roletype /
   typeattributeset / `allow system_server … find`，以及 service_contexts 里那一行
   （未实现名 3 → 2 个：`realitytap`、`diaghal`）。

## 5. 验证

- `hal list`：`vendor.meizu.hardware.meizu_charging aidl -> …-service`。
- `hal build`：`(v1, hash be64757e0ec3)`，编 7+1 源文件（接口 + 6 枚举 + 实现）→ 链接；
  产物 `strings` 含 descriptor + 实例名 + 同一 hash；NEEDED 只有 libc++/libbinder_ndk/
  liblog/libdl/libm/libc。
- **wire 回放**（自研产物 vs 原厂 `.so`，按 code 逐条对方法名）：1..11 全 `OK`，
  `getInterfaceHash`/`getInterfaceVersion` 也一致。
- 07 exit=0：步骤 4「`meizu_charging@default` 写入 3」；步骤 6 追加模块层 cil
  （源文件 2,718 B，镜像内 marker 块 2,854 B——07 会注释化改写正文里像 marker 的行，
  见 `log-fail.txt` 坑 5）/ service_contexts 567 B；6b 新增
  `…-service -> hal_meizu_charging_default_exec`；
  装配树 661 → **664**（文件 433 / 链接 199 / 目录 32）；`vendor.img` 206,295,040 →
  **206,413,824 B**。
- 镜像回读：二进制 `a3419220…` / rc `3d3146df…` / VINTF `41fa583b…` 与
  `out/vendor/android16` 产物逐字节一致；`ea_get … security.selinux` =
  `u:object_r:hal_meizu_charging_default_exec:s0`；`IMeizuCharging/default` 只在模块
  marker 块内出现 1 次；`vendor_file_contexts` 由 6b 派生 1 行；`{{vers}}` 已替换为 202504。
- 判据：装配树条目 + 逐文件 sha + 镜像 xattr（镜像 sha 不作一致性判据，mke2fs 非确定性）。

## 6. 遗留

1. **运行期未复验**：`init.svc.vendor.default.meizu_charging-service=running`、
   Flyme 电池页读写阈值往返、avc denied 0（`class late_start`，boot 后才起）。
2. **语义是内存回显**：阈值不会真的限制充电（云机没有 Meizu PMIC 控制）。
   若 framework 期望「设 80 但电池仍充到 100」这种物理反馈，会看到不一致——但那本来就是
   真机行为，云机无从复现。
3. **枚举成员名不可得**：`UNKNOWN = 0` 是占位，客户端传入的其它取值我们只存储不解释。
   若将来要按取值分支（如「反向无线充=开」需要写某个 sysfs），需要先补齐枚举语义。
4. **原厂 sysfs 授权未搬**：`/sys/class/meizu/charger/**`、`/sys/class/qcom-battery/
   charge_control_en`、`typec/port0/data_role` 等属板级事实，接硬件时放机型层。
5. 剩余同类 HAL：`realitytap`（V3 + 跨包 vibrator 类型）、`diaghal`（服务由
   `/vendor/bin/diag-router` 注册）。`patch.img` / `out/dist` 仍旧版（09/11 未跑）。
