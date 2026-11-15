# m22 卡在 Flyme 启动动画：5 个自研 HAL 的进程域缺 `domain` 属性

- 设备：`flyme/m22`（MEIZU 22，QTI `sun`，清单 `manifest/flyme/m22/baselines/12.5.0.0A.json`）
- 阶段：`07_build_vendor`（数据层：5 个模块的 `sepolicy/vendor_sepolicy.cil`）
- 现场实例：daemon 实例 43（失败）/ 实例 44（通过）
- 日期：2026-10-30

## 1. 问题

`engine/out/dist/flyme/m22/12.5.0.0A` 这套盘集用 daemon 拉起后（实例 43），卡在
Flyme 启动动画不动，约 **每 72 秒重来一轮**：

```
01-01 00:00:28.957  SystemServer: Entered the Android system server!   # 第 1 轮
01-01 00:01:41.361  boot_progress_start: 101361                        # 第 2 轮
01-01 00:02:53.679  boot_progress_start: 173679                        # 第 3 轮
01-01 00:01:41.051  W Watchdog: *** WATCHDOG KILLING SYSTEM PROCESS:
                    Blocked in handler on main thread (main) for 66s
    at android.os.ServiceManager.waitForServiceNative(Native Method)
    at com.android.server.lights.LightsService$IAwLightWrapper.connectToHal(LightsService.java:876)
servicemanager: Since 'vendor.meizu.hardware.aw_light.IAwLight/default' could not be found
               trying to start it as a lazy AIDL service.
```

qemu 进程 CPU 69%、bootanimation 在跑，但 `boot_progress_enable_screen` 永不置位。
同一次运行里三组错误各上千次：

| 错误 | 次数 |
| --- | --- |
| `init: cannot setexeccon('u:r:…') for <服务>: Invalid argument` | 1,636 |
| `Service '<服务>' received signal 6` | 1,648 |
| `linker: CANNOT LINK EXECUTABLE "…c2@1.2-service-v4l2-64": library "libchrome.so" not found` | 479 |

## 2. 根因

两个互相独立、必须同时修掉的缺陷。

### 2.1 主因：5 个进程域没有归入 `domain` 属性

Android 的 CIL 里，一个类型要被当作**进程域**用，光有 `(type X)` 不够：
角色 `r` 的类型集合就是 `domain` 属性（平台策略 `(roletype r domain)`），而
`(roletype object_r X)` 只给了 `object_r` 角色。所以当 `X` 不在 `domain` 属性里时，
上下文 `u:r:X:s0` 的「角色–类型」关联不成立，内核 `security_context_to_sid` 判它非法，
init 的 `setexeccon` 拿回 **EINVAL**：

```
init: cannot setexeccon('u:r:hal_aw_light_default:s0') for vendor.default.aw_light: Invalid argument
init: Service 'vendor.default.aw_light' (pid 1075) received signal 6
```

服务 fork 出来立刻 abort，init 每秒重试一次。`aw_light` 起不来 →
`LightsService$IAwLightWrapper.connectToHal` 拿不到 `IAwLight` → system_server 主线程
阻塞 66 s → Watchdog 杀 system_server → 整机重启，即「卡在启动界面」。

m22 的 5 个自研 HAL 域全部命中这个坑：

| 模块 | 域 | 失败计数（setexeccon / signal 6） |
| --- | --- | --- |
| `vendor.meizu.hardware.aw_light` | `hal_aw_light_default` | 472 / 475 |
| `vendor.qti.diaghal` | `vendor_hal_diaghal_default` | 880 / 873 |
| `vendor.meizu.hardware.meizu_charging` | `hal_meizu_charging_default` | 95 / 95 |
| `vendor.awa.reality` | `hal_realitytap_default` | 94 / 95 |
| `vendor.qti.hardware.servicetrackeraidl` | `vendor_hal_srvctracker_default` | 95 / 92 |

**为什么构建期完全没告警**：`(type X)` 声明齐全，secilc 编译**成功**（策略能加载、
`pioneer` 的 forced permissive 也生效），缺的只是属性归类这一条——它不影响编译，
只影响运行期上下文校验。日志里 `init: Compiling SELinux policy` +
`init: Loading SELinux policy` 均正常，正是这个静默失败的特征。
同一个仓库里 `android.service.daemon` 的 `pioneer` 是**唯一写对的**（第 28-33 行有整段
注释在讲这个坑），因此以它为准。

### 2.2 次因：v412 运行期闭包在冷启动那一趟必漏

`07` 的 `moduleTargets()` 在步骤 0 就调 `v412RuntimeLibs(outDir)`（`hal.go:409`），
而 `v412-runtime-libs.txt` 是步骤 3 `hal build` 编译完才写出的。产物根被清空后的**第一趟**
必然读到空集 → 15 个运行库（含 `libchrome.so`）不上装配树 → v4l2 服务每秒报
`CANNOT LINK` 起不来。`hal.go:459-461` 的注释本来就写着这个「两趟」假设。
本轮的处置就是**再跑一趟 07**：第二趟清单已在位，`android.hardware.media.v412@default`
从 5 项变 20 项。

> 附带效应：这一条同时解释了 #73 遗留的「装配树 654 vs 历史 669」——差的就是这 15 个
> 运行库（文件 438−423=15）。不是历史残留。

## 3. 证据

- 失败现场：同目录 `log-fail.txt`（实例 43，六节：setexeccon/signal 6 计数、
  Watchdog 栈、libchrome、`Loading SELinux policy` 成功、镜像内 type/domain 对照表）。
- 通过现场：同目录 `log-pass.txt`（实例 44，同一组计数全部归零 + 启动里程碑 + 域归属实证）。
- 镜像内策略自查（失败态）：5 个域 `type:1 domain-set:0`，只有 `pioneer` 是
  `type:1 domain-set:1` —— 类型都在，缺的只是属性。
- 平台侧属性大列表：`vendor_sepolicy.cil:88` 有一行把上百个 HAL 域（`hal_audio_default`
  … `hal_wifi_supplicant_default`、以及 `*_202504` 版本化域）塞进 `domain`，
  5 个自研域**一个都不在里面**。
- 排除了「策略没加载」：`init: Compiling SELinux policy`（现场 secilc 编译）+
  `Loading SELinux policy` 成功 + `SELinux: cdh: pioneer (type 2605) forced permissive` 生效。

## 4. 改动

5 个模块的 `sepolicy/vendor_sepolicy.cil` 各补一条属性归类，位置紧跟 `(type X)`：

```cil
(type hal_aw_light_default)
(typeattributeset domain (hal_aw_light_default))     ;; ← 新增
(roletype object_r hal_aw_light_default)
```

| 文件 | 新增行 |
| --- | --- |
| `engine/vendor/hardware/vendor.meizu.hardware.aw_light/sepolicy/vendor_sepolicy.cil` | `(typeattributeset domain (hal_aw_light_default))`（带 6 行注释解释坑） |
| `engine/vendor/hardware/vendor.qti.diaghal/sepolicy/vendor_sepolicy.cil` | `(typeattributeset domain (vendor_hal_diaghal_default))` |
| `engine/vendor/hardware/vendor.meizu.hardware.meizu_charging/sepolicy/vendor_sepolicy.cil` | `(typeattributeset domain (hal_meizu_charging_default))` |
| `engine/vendor/hardware/vendor.awa.reality/sepolicy/vendor_sepolicy.cil` | `(typeattributeset domain (hal_realitytap_default))` |
| `engine/vendor/hardware/vendor.qti.hardware.servicetrackeraidl/sepolicy/vendor_sepolicy.cil` | `(typeattributeset domain (vendor_hal_srvctracker_default))` |

后 4 个用统一短注释，指向 `aw_light` 那份的说明。然后重跑 07 让模块层 cil 重新合并进
装配树并重新打包 `vendor.img`：

```bash
cd engine
./out/bin/cdh device stop 43         # 或 daemon API: POST /api/stop {"index":0}
./out/bin/cdh device build m22 --from build_vendor
```

## 5. 验证

构建侧（`out/tmp/m22-rebuild-fix1.log`，`EXIT=0`）：

| 项 | 结果 |
| --- | --- |
| 07 | 全阶段通过，`EXIT=0` |
| v412 模块产物 | `android.hardware.media.v412@default` **20 项**（编译产物 17 + 静态树 3） |
| 装配树 | **669** 条目（文件 438 / 链接 199 / 目录 32），与 #71 记录逐字一致 |
| `vendor.img` | **206,700,544 B**（与 #71 一致） |
| 镜像内 `vendor_sepolicy.cil` | 348,423 → **349,844 B**；5 个域全部 `type:1 domain-set:1` |

运行侧（实例 44，`out/core/daemon/runtime/m22_12.5.0.0A/44/qemu.log`，52,843 行）：

| 项 | 实例 43 | 实例 44 |
| --- | --- | --- |
| `cannot setexeccon` | 1,636 | **0** |
| `received signal 6` | 1,648 | **0** |
| `CANNOT LINK`（libchrome） | 479 | **0** |
| `Entered the Android system server` | 3 | **1** |
| `zygote` / `surfaceflinger` starting | 各 3 | **各 1** |
| `boot_progress_enable_screen` | 从不 | **66,593 ms** |
| `sys.boot_completed=1` | 从不 | **`init.rc:1308` 处理（69.4 s）** |

5 个服务各 start 一次即成功（`realitytap` pid 862 / `diaghal` 864 /
`servicetrackeraidl` 865 / `aw_light` 1039 / `meizu_charging` 1042，
v4l2 843），无第二次「starting」重试。

域归属的**直接**实证：`pioneer` 的 `pidof` 跨域读 `/proc/<pid>/cmdline` 被拒，日志的
`tcontext` 恰好是这 5 个域（`u:r:hal_aw_light_default:s0` 等）——修复前这些进程都在
`u:r:init:s0`。

画面：`.tmp/m22-run-44-ok.png`（Flyme 桌面）、`.tmp/m22-run-44-late.png`
（已点进「设置 → 无线网络」，页面与触摸正常，不是只画一帧的假桌面）。

## 6. 遗留

1. **引擎侧没有「进程域必须归入 domain 属性」的校验**。`halexec.go` 的
   `definedPolicyTypes()` 只判断 `<域>_exec` 类型存不存在（`(type …)` 正则），
   不检查域本身是否在 `domain` 属性里 —— 本次这种「能编译、能加载、运行期 EINVAL」
   的静默失败因此没有任何构建期提示。建议在步骤 6 增加一条
   `[warn] 域 X 未归入 domain 属性`（正则找 `(typeattributeset domain (… X …))`），
   与既有的 `<域>_exec 缺失` 告警并列。**本次未改代码。**
2. **`hal.go:409` 的「两趟」假设未改**。`v412RuntimeLibs()` 在读清单时点在步骤 0，
   产物根被清空后第一趟必漏 15 个运行库，只能靠重跑一趟补齐。建议把该分支的目标
   集合计算挪到步骤 3 编译之后再求值（或编译后重建一次 targets）。
3. **#73 的遗留项需回填**：装配树 654 vs 669 已确认是 v412 的 15 个运行库
   （文件 438−423=15），不是「历史残留」，也不再是「无法定论」。
4. **diaghal 的客户端授权不全**：`vendor_dpmd` 对 `vendor_hal_diaghal_default`
   的 binder `call` 被拒（`permissive=0`，audit 276/277 两条事件）。
   #69 当时只保留了 system_server 作为客户端，`vendor_dpmd` 这条没搬。
   diaghal 目前是空实现（`ioctl`/`write` 恒 -1），不影响开机，接真实 diag 时补。
5. **assemble 侧仍无运行期回归基线**：本次靠人工 grep 日志确认归零。若要长期防回归，
   可把「`cannot setexeccon` 计数必须为 0」做成 daemon 自检项。
6. **09/11 已随 `--from build_vendor` 一起跑掉**：`patch.img` 402,653,184 B、
   `out/dist/flyme/m22/12.5.0.0A`（`vda.img` 13.00 GiB，5 个分区已刷新）均在 11:11 更新，
   实例 44 起的就是这份新盘集，不存在「dist 与构建产物不一致」的残留。
