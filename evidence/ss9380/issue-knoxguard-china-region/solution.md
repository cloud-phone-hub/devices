# `ro.csc.country_code` 缺失 → KnoxGuard 走非中国分支 → 开机即锁屏（8135）

- 机型：samsung / Galaxy S25 Ultra（`SM-S9380`，`ss9380`，OneUI Android 16 / `S9380ZCSDCZH1`）
- 发现时间：2026-10-31
- 盘集：build13（失败）→ build14（属性被 SELinux 拒）→ build15（通过）
- 状态：已解决（中国区分支不再锁机，SetupWizard / launcher 可用）

## 一、问题

`sys.boot_completed=1` 之后设备停在锁屏：

```
mCurrentFocus=Window{48e53df u0 Bouncer}
KnoxGuard: 手机已锁定 (8135)
```

`getprop ro.csc.country_code` 为空，`knox.kg.state=Error`。点掉锁屏、重启
SystemUI 都会立刻回到同一状态；`dumpsys device_policy` 里没有 device admin
或 device owner，锁来自 KnoxGuard 的 `lockSeDevice`，不是应用层。

## 二、根因

### 1. AP 固件不带 CSC/optics，地区属性为空 → 选错分支

本盘集只使用官方 AP 侧的 boot / init_boot / vbmeta / super / vendor_boot，
**没有 CSC（`optics`/`prism`）分区**，运行时 `/optics`、`/prism` 为空，OneUI
没有地方写入 `ro.csc.country_code`。

反编译 `services.jar` 里的 KnoxGuard（`KnoxGuardSeService` /
`SystemIntentProcessor` / `IntegritySeUtil`）核对，判定链是：

- `registerReceiver`：`if-eqz isChinaDevice` 跳到中国分支，只注册
  `SETUPWIZARD_COMPLETE` 动作，随后才无条件注册 `BOOT_COMPLETED` 与
  `com.android.systemui` 的监听；非中国分支额外注册 `PACKAGE_*` 监听。
- `handleBootCompleted`：`checkTaIntegrity` 为 null → 锁；`checkAPSerialIntegrity`
  为 false → 锁（`1002`）；然后 `checkKGClientIntegrityAndEnableComponentsWithFlag`。
- **中国分支（0x92 起）**：`state==0` → `setCheckingStateToKg` 后 return；
  `!isSetupWizardFinished` → return；`!isStateForEnrolledDevice(state)`（只认 2/3）
  → return；否则才 `autoLockDevice("3040")`。

`isChinaDevice()` 只读 `ro.csc.country_code` 并 `equalsIgnoreCase("China")`。
属性未定义即走**非中国分支**：云机没有 KG Trustlet，TA 状态取不到
（`knox.kg.state=Error/5`），`checkKGClientIntegrity` 又要求
`com.samsung.android.kgclient`（各分区均无此包，且需平台签名），失败即
`lockSeDevice` → SystemUI 显示“手机已锁定(8135)”。

`ZCH` 固件真机该值就是 `China`；把它补进 vendor 属性后，boot 时
`state=5`、SetupWizard 已完成、又未入网，中国分支直接 return，不触发任何锁机路径。

### 2. 只写属性不够：`vendor_init` 无权写 `csc_prop`

`ro.csc.country_code` 的类型在
`/system_ext/etc/selinux/system_ext_property_contexts` 第 201 行，为 `csc_prop`。
`/vendor/build.prop` 由 init 以 `vendor_init` 域装载，而 lineage 基座合并进来的
plat / system_ext / product / vendor 策略里没有 `vendor_init` 写 `csc_prop` 的允许
（真机上这些值由三星 CSC/OMC 流程写，不经 vendor build.prop）。build14 首次开机：

```
init: Do not have permissions to set 'ro.csc.country_code' to 'China'
      in property file '/vendor/build.prop': SELinux permission check failed
```

属性仍是空，锁机照旧。

## 三、证据

- 失败态：`log-fail.txt`
  - `ro.csc.country_code` 为空、`knox.kg.state=Error`、`sys.locksecured=true`；
  - `mCurrentFocus` 为 `Bouncer`；
  - `checkKGClientIntegrity false. Lock device without passcode.` 原文
    （`kg-boot-fail.log` 是同一段 qemu.log 的干净摘录）；
  - `lock-screen-8135.png`：1440×3120 锁屏，“手机已锁定 (8135)”。
- 中间态：build14 的 `init` 拒写属性原文（见上节，`log-pass.txt` 第 2 段）。
- 通过态：`log-pass.txt`
  - `getprop ro.csc.country_code` → `China`，`knox.kg.state=Error`（预期），
    `sys.boot_completed=1`；
  - build15 整份 qemu.log 中 `Lock device without passcode` = 0、
    `China Binary and previously enrolled` = 0、属性权限拒绝 = 0；
  - 中国分支日志止于 `isSetupWizardFinished : true`，没有任何后续锁机调用；
  - `mCurrentFocus` 为 launcher，SystemUI 进程稳定，crash buffer 里
    `Process: com.android.systemui` = 0；
  - `pass-setupwizard.png`（中国区条款页）、`pass-home.png`（桌面/状态栏/dock）。

> 计数口径：`grep 8135` 会误命中时间戳、天气哈希与 HID 日志，锁机与否一律以
> `Lock device without passcode` / `autoLockDevice` 与窗口焦点为准。

## 四、改动

1. `devices/manifest/samsung/ss9380/baselines/SM-S9380_3_20260813083211_652iw49crl_fac.json`
   的 `vendor.build-prop-sets` 增加：

   ```json
   "ro.csc.country_code": "China"
   ```

   放 vendor 的原因：CSC 分区不存在，`/vendor` 是唯一“可写且 init 装载在 system
   之后”的分区（内核 namei 白名单对 `/vendor/build.prop` 精确重定向到
   `/mnt/patch/vendor/build.prop`），07 步骤 6 的 `props.go` 会在装配树里覆盖/追加。

2. `devices/manifest/samsung/ss9380/sepolicy/vendor_sepolicy.cil` 追加**第十段**：

   ```cil
   (allow vendor_init csc_prop (property_service (set)))
   ```

   只放开 `property_service` 的 `set`，作用域限定在 `vendor_init` 装载属性，
   不触及属性读取或其它域。

sepolicy 在 `07_build_vendor` 注入（`sepolicy.go` 的 marker 块），所以改完必须
从 07 起重跑；只从 09 起会因 patch 指纹不含 sepolicy 而直接复用旧件。

## 五、验证

1. `07_build_vendor` → `11_pack_vda` 重跑（build15），产物：
   - `super.img` `215e6964…`（官方 super 零改动）；
   - `vendor.img` `0ab16bc8…`、`patch.img` `2af63406…`；
   - `vda.img` 19.07 GiB，`pack_vda` 回读 5 个分区一致；
2. `debugfs` 读回 `/vendor/etc/selinux/vendor_sepolicy.cil`，第十段在文件末尾
   （`vendor_init csc_prop` 命中 1 次）；
3. 新实例（`index 4`）首启 `boot_completed=1`；`getprop` 为 `China`；
4. 锁机路径归零，SetupWizard 中国区条款页正常渲染；标记
   `device_provisioned=1` / `user_setup_complete=1`（仅本测试实例的运行态操作，
   非镜像改动）后 launcher 桌面与导航正常；
5. 同 build 上复核前三个卡点仍为修复态：`sys.locksecured` 的 init/libc 拒绝
   70/75 → 0，scpm `JNI FatalError` 166 → 0，SystemUI 进程崩溃 25 → 0，
   fabric_crypto 启动 185 → 1、`Not found Fkeymaster` 347 → 0。

> 换盘后首次启动若进入 recovery 提示，是旧实例的 `writable-data.qcow2` 仍按旧
> `metadata` 加密（vda 重建后不匹配）。把 runtime 目录下的 `writable-base.qcow2` /
> `writable-data.qcow2` 移开让 daemon 重建即可，与镜像无关。

## 六、遗留

- `knox.kg.state=Error` 在云机上属预期：没有 KG Trustlet，TA 调用必然失败；
  本次修复只保证它不再触发锁机，不代表 KG 全链路可用。
- 没有 CSC/optics：`persist.audio.sales_code` 等依赖销售码的属性仍缺失
  （启动时可见 `avc denied`），音频策略 XML 与部分 OMC 功能可能不完整。
- `com.samsung.android.fmm` 的 Firebase 初始化崩溃弹窗，以及
  themecenter / ims / `com.qualcomm.location` / sdhms 的非致命崩溃仍在，
  不影响桌面可用性，但会留下错误弹窗。
  → **部分消退（2026-10-31 复测）**：`pm disable-user --user 0 com.samsung.android.fmm`
  直接失败（`Failed to change state of package`，系统包不可禁用）；
  `com.qualcomm.location` 的 `DebugReportingMonitor` 当时已刷到 1900+ 条崩溃
  （约 1 条/s，持续崩会累计 RescueParty 计数），用
  `pm disable-user --user 0 com.qualcomm.location` + `am force-stop` 后
  30 s 内增量归 0，能立刻止住；但它是 `/data` 里的包状态，
  **实例重建（全新 `/data`）后会恢复启用**，需要每次开新实例再禁一次，
  或后续在镜像层预置 disabled 状态/直接删包；
  `com.sec.android.sdhms` 仍反复崩（`SiopExpression` 的
  `NullPointerException: Attempt to get length of null array`），由 `com.samsung.android.sm_cn`
  的 `AppErrorDialogActivityCN` 弹“屡次停止运行”——与本条的 health HAL 无关。
- SetupWizard 的跳过/完成动作依赖测试实例的运行态标记；正式交付前需要确认
  首次开机流程（或预置 provisioning 值）是否要进镜像。
  → **已明确（2026-10-31 复测）**：不需要预置。全新 `/data` 的实例 5 上只用
  `input tap` 走完 SetupWizard（Rubin 同意 → 供您审阅勾 4 项 → 隐私长文 →
  三星浏览器首启 → HOME）后，`device_provisioned=1` / `user_setup_complete=1`
  由流程自己写入，本轮**没有**手工 `settings put`。

## 七、复测记录（2026-10-31，最终盘集）

第十段之后又叠加了第九段（`sys.locksecured`）的修正，在最终盘集上重跑了一遍：

- 盘集：`patch.img` sha256 `53e483856316d50c05f117efeb13d9b2012aca02bf9e0afe760700a156f6259d`
  （`engine/out/tmp/ss9380-build14.log`）。本文件第五节写的 build15 / `2af63406…`
  是上一轮的手写编号与产物，与 `engine/out/tmp/` 现存日志编号对不上
  （缺 11、15），**构建以 sha256 为准**，两轮的 sepolicy 内容一致；
- 实例：`ss9380_S9380ZCSDCZH1` / index 5，QEMU 08:12:35 拉起，全新 `/data`；
- 属性与锁机路径：

```
$ adb shell getprop ro.csc.country_code
China
$ adb shell getprop knox.kg.state
Error                        # 预期：无 KG Trustlet
# qemu.log 整份："Lock device without passcode" = 0
```

- 锁屏：`KeyguardServiceDelegate showing=false / secure=false`、
  `isKeyguardShowing=false` —— 锁屏自行消失。注意本轮**没有**做
  `locksettings.db` 手术、也没有 `wm dismiss-keyguard`，锁屏是随策略生效自己退掉的。
- KG 启动链本身照旧跑了（中国分支提前 return，但 `checkKGClientIntegrity` 仍会被调用）：

```
KG.SystemIntentProcessor: handling boot completed
KG.SystemIntentProcessor: isSetupWizardFinished : true
KG.IntegritySeUtil: kgState() : 5
KG.IntegritySeUtil: checkKGClientIntegrity Exception ... NameNotFoundException: com.samsung.android.kgclient
KG.IntegritySeUtil: checkSignatures : false / isSystemApp : false / isEnabled : false
```

  但锁机动作归零：整份 `qemu.log` 中 `Lock device without passcode` = 0、
  `autoLockDevice` = 0、`Set RemoteLock` = 0；锁屏侧只剩 2 次
  `KeyguardUpdateMonitor: updateRemoteLockInfo()`（读远程锁信息，内容为空）。

- 桌面：`topResumedActivity=com.sec.android.app.launcher/.activities.LauncherActivity t8`，
  截图 `pass-home.png`；SetupWizard 中国区条款页截图 `pass-setupwizard.png`；
  这两张是上一轮（index 4）的图，本轮同流程重走了一遍，桌面图另见
  `../issue-sysui-uevent-netlink/pass-desktop.png`。

### 补充：本轮没动过的东西

为免后续误读，这里明确列出——本轮**未**执行、也未依赖以下任何一项：

- `locksettings.db` 增删改（`3locked` / `3clientname` / `3message` 均未动）；
- `locksettings clear` / `wm dismiss-keyguard` / `settings put global lockscreen.disabled`；
- 禁用 `com.samsung.android.kgclient`、`com.samsung.android.fmm` 等系统包。

锁屏能退掉的唯一依赖就是本条的 `ro.csc.country_code=China` + 第十段写权限，
再加上 `../issue-sysui-locksecured-prop/` 的第九段（否则 SystemUI 每 30 s 死一次，
锁屏会反复重建）。

