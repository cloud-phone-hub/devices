# m22 perf2 的 SELinux find 授权迁到模块层

> 设备：`devices/flyme/m22`（QTI 12.5.0.0A，Android 16 / QTI `sun`）
> 关联：#53（perf2 空桩初建）、#56（hardware 名单对齐）、#31（odm 只有
> `precompiled_sepolicy`，故没有 `odm_sepolicy.cil` 可并）
> 日期：2026-10-29

## 1. 问题

两件事一起暴露，一件是运行期噪音，一件是归属错位。

### 1.1 运行期：服务在跑、名字也有标签，但除两个域外全都 `denied { find }`

perf2 的桩 HAL 本身是好的：`init.svc.vendor.perf2-default=running`、
`service list` 里有 `vendor.qti.hardware.perf2.IPerf/default`、`service_contexts`
也把它标成了 `u:object_r:cdh_perf2_service:s0`。但实例 37（迁移前镜像）的串口
流水里，`name=vendor.qti.hardware.perf2.IPerf/default` 的
`avc: denied { find } … tclass=service_manager` 有 **1554 条结构完整的记录**
（kernel console 454 + logd `auditd` 1100，另有 66 行被串口截断只剩片段），跨 **19 个域**：

| 次数 | 域 | 次数 | 域 |
| ---: | :--- | ---: | :--- |
| 804 | `mediaserver` | 12 | `nfc` |
| 468 | `platform_app` | 9 | `permissioncontroller_app` |
| 62 | `priv_app` | 9 | `secure_element` |
| 51 | `vendor_perfservice` | 9 | `mediaprovider_app` |
| 26 | `radio` | 9 | `rkpdapp` |
| 25 | `mediaprovider` | 9 | `vendor_voiceui_app` |
| 16 | `vendor_qtelephony` | 8 | `network_stack` |
| 8 | `vendor_dataservice_app` | 8 | `vendor_xpanprovider_app` |
| 8 | `vendor_systemhelper_app` | 7 | `vendor_qcc_app` |
| 6 | `vendor_qesdk_app` | | |

计数口径见 `log-fail.txt` D 段：串口会把上一条 guest 输出拼进 avc 行前面，因此**不从行首
`scontext` 归属域**（那样会多出 `pioneer` / `hal_face_default` / `traceur_app` 等 5 个假域），
而是先抽结构完整的 avc 记录再统计。

被拒之后客户端走失败路径：每次查询都要到 servicemanager 打一条
`Could not find … in the VINTF manifest` 再按「不可用」降级（实例 37 里 830 条）。
m22 的 rc 注释里记着：3a-15 上同一缺口曾放大成 app 主线程阻塞。

### 1.2 归属：有实体的 HAL，策略却留在机型层

`engine/vendor/hardware/vendor.qti.hardware.perf2/` 里有 `aidl/` 与 `src/`
（`IPerf` v1 空实现，方法即时回 0），但它的**类型声明、服务名标签、服务端与
客户端的 find 授权**全在机型层 `devices/manifest/flyme/m22/sepolicy/`。于是：

- 模块被换到别的机型复用时，`sepolicy/` 目录里没有任何与 perf2 有关的东西，
  服务名立刻退回 `default_android_service` 继续产 avc；
- 同一件事被拆在两层：类型在机型层 CIL、启动链在机型层 rc、二进制在模块层。

## 2. 根因

### 2.1 授权面只覆盖了 `system_server` 与 `lmkd`

迁移前的机型层 CIL 只有两条客户端授权：

```cil
(allow system_server_{{vers}} cdh_perf2_service (service_manager (find)))
(allow lmkd_{{vers}} cdh_perf2_service (service_manager (find)))
```

而真实发起方是 19 个域，横跨 plat（`mediaserver` / `platform_app` / `priv_app` /
`radio` / `nfc` / `mediaprovider` / `secure_element` / `network_stack` …）与 Flyme
的 system_ext（`vendor_perfservice` / `vendor_dataservice_app` /
`vendor_qtelephony` / `vendor_xpanprovider_app` / `vendor_systemhelper_app` …）。
这不是「漏了某几个」，而是「按域列举」这个做法本身在换底座时必然重数。

### 2.2 契约放错了层

本仓的约定是：**模块层 `engine/vendor/hardware/<模块>/sepolicy/` 放随 HAL 复用的
契约；机型层只放绑定本机路径与启动链的事实**（见 `07_build_vendor/sepolicy.go`
文件头）。perf2 的「服务名 → 类型 → 谁能看见它」是它自己的契约，理应随模块走。

### 2.3 客户端侧改用 `domain` 通配，但只授 `find`

- 只授 `service_manager find`：效果是「名字可见」，让 `getService` 从 denied 变成
  「查得到但 VINTF 未声明 → 按不可用降级」；
- 不给 `(binder (call transfer))`：真正跨进程调用仍会被拒，而本服务方法全是即时
  回 0 的空实现，没有调用价值；
- `domain` 在 vendor CIL 里可 resolve：用 guest 的 `/system/bin/secilc` 按 init 同序
  实测过（逐域列举与 `domain` 通配两种写法 RC 均为 0，见 `log-pass.txt` B 段）。

### 2.4 必须同步删掉机型层那份 `(type …)`

片段机制是**追加 + 按 marker 整块替换**，只能加语句、不能删语句。如果只在模块层
补 `(type cdh_perf2_service)` 而机型层那份还留着，secilc 会因重复类型声明失败 →
init `InitFatalReboot`。所以迁移必须「模块层加 + 机型层删」成对做。

## 3. 证据

| 文件 | 内容 |
| :--- | :--- |
| `log-fail.txt` | 实例 37（迁移前）串口全量流水：1554 条结构完整的 `denied { find }` 记录、19 个域、日志跨度 52.7 min；每域一条原始记录；口径说明（串口行拼接导致假域、两路上报通道） |
| `log-pass.txt` | A 环境/镜像 sha；B 三态编译实证；C 装配树 marker；D 设备侧落点；E 实例 38 同口径对照；F 事后窗口；G 构建与测试 |

关键对照（同口径，都是「开机起的串口全量流水」）：

| 实例 | 策略 | `denied { find }`（记录级） | `Could not find … VINTF`（查询被放行） | 日志跨度 |
| :--- | :--- | ---: | ---: | :--- |
| 37 | 迁移前（只授 system_server/lmkd） | **1554**（19 个域） | 830 | 52.7 min |
| 38 | 迁移后（模块层 + `domain` 通配） | **0** | **1219** | 18.2 min |

提示数 1219 > 830 说明查询量更大而拒绝归零，排除「没人查」的假绿；两侧都随日志增长
略有浮动（38 仍在运行，宽松同行匹配分别为 837 / 1232），量级关系不变。

## 4. 改动

### 4.1 模块层新增两个片段

`engine/vendor/hardware/vendor.qti.hardware.perf2/sepolicy/vendor_sepolicy.cil`：

```cil
(type cdh_perf2_service)
(roletype object_r cdh_perf2_service)
(typeattributeset service_manager_type (cdh_perf2_service))

(allow hal_sensors_default cdh_perf2_service (service_manager (add find)))   ;; 服务端
(allow domain cdh_perf2_service (service_manager (find)))                    ;; 客户端
```

`.../sepolicy/vendor_service_contexts`：一行
`vendor.qti.hardware.perf2.IPerf/default  u:object_r:cdh_perf2_service:s0`。

两个文件都不含 `{{vers}}`（只有 vendor 侧类型与 `domain`），因此不依赖平台策略
版本号。注入后 marker 为 `<文件名>@vendor.qti.hardware.perf2`，与机型层同名文件
互不覆盖。

### 4.2 机型层删掉重复声明（6 条 CIL 语句 + 1 条 service_contexts）

`devices/manifest/flyme/m22/sepolicy/vendor_sepolicy.cil`：

- `(type cdh_perf2_service)` / `(roletype …)` / `(typeattributeset …)` 三行；
- `(allow system_server_{{vers}} cdh_perf2_service (service_manager (find)))`；
- `(allow lmkd_{{vers}} cdh_perf2_service (service_manager (find)))`；
- 服务端的 `(allow hal_sensors_default cdh_perf2_service (service_manager (add find)))`
  及其两行注释；
- 文件头说明由「6 个高通/魅族 vendor HAL 服务名」改为「5 个」，并注明 perf2 已
  移入模块层。

`devices/manifest/flyme/m22/sepolicy/vendor_service_contexts`：删 perf2 那一行，
段注释同步为 5 个。

### 4.3 注释口径同步

`devices/manifest/flyme/m22/vendor/etc/init/vendor.qti.hardware.perf2-hal-service.rc`
的 SELinux 注释原写「机型 CIL 已授该域…」，改为指向模块层片段（rc 本身仍留机型层，
它声明的是启动链与 `seclabel`）。

### 4.4 没有动的部分

其余 5 个 vendor HAL 名（`servicetracker` / `meizu_charging` / `aw_light` /
`awa_reality` / `idiag`）、框架服务 `realitytap`、`sensor_aux` 全部留在机型层：
它们在引擎侧**没有实体模块**，属「机型事实」。等真有实体再按 perf2 这套归位。

## 5. 验证

### 5.1 静态：guest 内 secilc 按 init 同序编译

| 输入 | 结果 |
| :--- | :--- |
| 迁移前的机上策略（基线） | RC=0，869,038 B |
| 装配树原文件（含新模块块，未过 09） | RC=254：`Failed to resolve permission nlmsg` @2423 |
| 装配树 + 09 清单改写（= 镜像里的最终文件） | **RC=0**，869,026 B，`db2b9f65b91a` |

中间态失败与本条无关，是 lineage 自带的 `ot_rcp` `nlmsg` 语句（09 的
`patch.vendor-cil-rewrite` 负责改掉）。详见 §6 第 4 条。

### 5.2 装配树与设备树

- 装配树 / 设备 `/vendor/etc/selinux/vendor_sepolicy.cil` 都在 4583 行出现
  `;; cdh-patch-begin: vendor_sepolicy.cil@vendor.qti.hardware.perf2`，块内
  4606/4611/4614 为类型与服务端/客户端授权；机型层块（4617 起）内已无
  `cdh_perf2_service`。
- `vendor_service_contexts` 模块层块 51–60 行、机型层块内无 perf2 条目。
- 全文件 `cdh_perf2_service` 只出现 6 次，全在模块层块 → 没有重复 `(type …)`。
- `{{vers}}` 残留 0 处。

### 5.3 运行期（实例 38，全新镜像）

- `sys.boot_completed=1`；`init.svc.vendor.perf2-default=running`，
  `pidof vendor.qti.hardware.perf2-hal-service=510`（与窗口结束时一致）；
  `service check vendor.qti.hardware.perf2.IPerf/default` → `found`。
- 开机日志 `init: Compiling SELinux policy` 1 次、`InitFatalReboot` 0 次、
  `Failed to resolve` 0 次 → 新策略在宿主机上真编过并装载。
- 串口全量流水：`denied { find }` **0**（记录级，与实例 37 的 1554 同口径）；
  `Could not find … VINTF` **1219** 条（= 1219 次查询被放行）。对照实例 37 的 1554 / 830。

### 5.4 事后窗口（8 min 8 s）

guest `20:55:57.528` → `21:04:05.212`，24 轮 `force-stop + monkey + HOME` 冷启
settings/browser/calendar/camera，采集 12,971 行、`avc:` 608 行，其中含 `perf2`
的行 **0** 行。同窗口的 VINTF 提示也是 0 条 —— 说明这段触发没有走到 perf2 查询
路径（框架已缓存「不可用」），所以本窗口只作「无新增 denied」的补充，查询侧结论
以 5.3 为准。

### 5.5 构建与测试

`go build ./...` PASS；`go test ./07_build_vendor/... ./09_build_patch/...` PASS。
产物：`vendor.img` `ab98e9997362…`、`super.img` `3eb133bbc4af…`、
`patch.img` `6c8d3628de51…`、vda 配方 `vda-9011ef633636`。

## 6. 遗留

1. **其余名字的归位时机**：`servicetracker` / `meizu_charging` / `aw_light` /
   `awa_reality` / `idiag` / `realitytap` / `sensor_aux` 仍在机型层。它们没有实体，
   迁过去只会得到「模块层替一个不存在的服务发标签」的假契约；等哪个真移植了
   实体，按 perf2 这四步（模块层 CIL + 模块层 service_contexts + 机型层删重复 +
   rc 注释）照做即可。
2. **只授了 `find`**：如果哪天框架真的去调 `IPerf` 方法（而不只是
   `getService`），还需要 `(binder (call transfer))`；当前服务是全空实现，授了也
   没意义。
3. **`domain` 通配的收敛路径**：若以后想收紧，`log-fail.txt` B 段就是 19 域的实测
   清单，可按它逐域列举；代价是换底座要重数，且漏一个就回到今天的现场。
4. **验证方法学（本轮踩到的坑）**：`07` 装配树里的 `vendor_sepolicy.cil` 仍含
   lineage 自带、本机内核/plat 都不认识的 `ot_rcp nlmsg` 与
   `kernel firmware_load` 语句，直接拿它做 secilc 静态校验会得到
   `Failed to resolve permission nlmsg`（RC=254）。这些由 `09_build_patch` 的
   `patch.vendor-cil-rewrite` 在组装 patch 时改掉，所以**静态编译校验必须用 09
   改写后的文件**，否则会把基底问题误判成自己引入的。
5. **服务端 `seclabel` 仍复用 `hal_sensors_default`**（机型层 rc 决定）。若将来给
   perf2 独立域，模块层 CIL 里那条服务端 `(allow hal_sensors_default … (add find))`
   要跟着改域。
6. **串口取证的口径**：第一版按「行内首个 `scontext`」归属域，得到 24 个域；复测时
   发现其中 5 个是拼接杂音造成的假域（`pioneer` / `hal_face_default` /
   `hal_drm_clearkey_aidl` / `traceur_app` / `audioserver`），已改为「先抽结构完整的
   avc 记录再统计」，数值同步更正为 1554 条 / 19 域。以后从 `qemu.log` 统计任何 avc
   都应沿用这个次序，否则会凭空多出域。
