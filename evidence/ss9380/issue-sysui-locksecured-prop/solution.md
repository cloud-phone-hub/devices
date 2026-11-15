# SystemUI 写 `sys.locksecured` 被拒 → 反复崩溃（15 分钟内 25 次）

- 机型：samsung / Galaxy S25 Ultra（`SM-S9380`，`ss9380`，OneUI Android 16）
- 发现时间：2026-10-30
- 盘集：修复前 index 3（`qemu.log`）→ build15 通过
- 状态：已解决（属性拒绝与 SystemUI 崩溃均归零）

## 一、问题

SystemUI 进程反复重启（13 分钟内 25 次 `am_crash`），每次崩溃前都伴随：

```
W libc    : Unable to set property "sys.locksecured" to "true": PROP_ERROR_PERMISSION_DENIED (0x18)
E AndroidRuntime: FATAL EXCEPTION: pool-10-thread-1
  java.lang.RuntimeException: failed to set system property "sys.locksecured" to "true"
    at android.os.SystemProperties.set(SystemProperties.java:237)
    at com.android.systemui.keyguard.KeyguardViewMediatorHelperImpl$onSecurityPropertyUpdated$1.run(...)
E PluginManagerImpl: System Crash; Disabling all plugins
```

init 侧同一时刻也有拒绝：

```
init: Unable to set property 'sys.locksecured' from uid:10045 gid:10045 pid:2648:
      SELinux permission check failed
```

uid 10045（`u0_a45`）就是 OneUI 下的 SystemUI。崩溃循环导致锁屏/状态栏反复重建，画面无法稳定使用。

## 二、根因

`sys.locksecured` 的类型是 `system_prop`。OneUI 把 SystemUI 跑在 `platform_app`
域（`ps -Z` 实测 `u:r:platform_app:s0:c512,c768 u0_a45 … com.android.systemui`），
而 lineage 基底合并后的策略里：

- plat 侧没有 `platform_app` 写 `system_prop` 的允许；
- 官方三星策略把对应允许放在自己的 vendor 侧（AOSP 对应的是 `system_app` 域，
  不适用于 OneUI 的 `platform_app`）。

于是 `property_service` 的 `set` 每次都被拒，`SystemProperties.set` 抛
`RuntimeException`，异常发生在线程池线程上直接杀死进程。

## 三、证据

- 失败态：`log-fail.txt`
  - init 拒绝 70 条、libc `PROP_ERROR_PERMISSION_DENIED` 75 条；
  - `am_crash` 里 SystemUI 因该异常 25 条，crash buffer 中
    `Process: com.android.systemui` 26 行；
  - 崩溃栈原文（`KeyguardViewMediatorHelperImpl` → `SystemProperties.set`）。
- 通过态：`log-pass.txt`
  - build15 整份 `qemu.log` 中两类拒绝均为 0；
  - guest `pidof com.android.systemui` 连续采样稳定（2508）；
  - `logcat -b crash` 中 `Process: com.android.systemui` = 0。

## 四、改动

`devices/manifest/samsung/ss9380/sepolicy/vendor_sepolicy.cil` **第九段**追加：

```cil
(allow platform_app system_prop (property_service (set)))
```

只放开 `property_service` 的 `set`，不改属性的读权限，也不扩大 `platform_app`
的其它能力。该片段在 `07_build_vendor` 注入，必须从 07 起重跑流水线。

## 五、验证

1. 重跑 `07_build_vendor` → `11_pack_vda`，build15 产物
   `vendor.img 0ab16bc8…`、`patch.img 2af63406…`；
2. `debugfs` 读回 `/vendor/etc/selinux/vendor_sepolicy.cil`，第九段在文件中
   （`platform_app system_prop` 命中 1 次）；
3. 新实例开机后：init/libc 拒绝 0/0，SystemUI 崩溃 0，进程 pid 稳定，
   锁屏与桌面可正常渲染。

## 六、遗留

- 本次修复只保证 SystemUI 有权限写该属性；`sys.locksecured` 是否被写成 `true`
  取决于 KnoxGuard/锁屏业务流程。通过态实测该属性为空（没有锁机流程触发），
  不能据此认为锁定逻辑失效。
- SystemUI 其它非致命告警（插件禁用、Sec 特性缺失）仍在，但不再导致进程重启。

## 七、复测记录（2026-10-31，最终盘集）

第九段之后又叠加了第十段（`ro.csc.country_code`），在最终盘集上重跑了一遍：

- 盘集：`patch.img` sha256 `53e483856316d50c05f117efeb13d9b2012aca02bf9e0afe760700a156f6259d`
  （`engine/out/tmp/ss9380-build14.log`）。注意：本文件第五节写的“build15”是当时
  手写的日志名，与 `engine/out/tmp/` 现存的编号对不上（缺 11、15 两个编号），
  **构建以 sha256 为准**，两次的 sepolicy 内容一致；
- 实例：`ss9380_S9380ZCSDCZH1` / index 5，QEMU 08:12:35 拉起；
- `qemu.log` 整份计数：

```
init: Unable to set property 'sys.locksecured'              = 0
W libc: ... "sys.locksecured" ... PERMISSION_DENIED          = 0
Process: com.android.systemui（crash buffer）               = 0
```

- guest 复核：

```
$ adb shell getprop sys.tile.lazy_booted
true                       # 同类被拒的另一个属性，确实写成功了
$ adb shell 'for i in 1 2 3 4 5; do pidof com.android.systemui; sleep 4; done'
2609 2609 2609 2609 2609  # 20 s 内不变
$ adb shell 'logcat -b crash -d | grep -c "Process: com.android.systemui"'
0
```

- 连锁效果：`KeyguardServiceDelegate` 变成 `showing=false / inputRestricted=false /
  secure=false`，`isKeyguardShowing=false` —— **锁屏自行消失**；随后只靠 `input tap`
  就走完了 SetupWizard 到 launcher 桌面（截图见
  `../issue-sysui-uevent-netlink/pass-desktop.png`）。本轮**没有**动 `locksettings.db`，
  也没有改任何锁屏相关 settings。

### 踩坑记录：属性域要看最长前缀，不能只看兜底行

第九段的第一版（build13，`patch.img` sha256 `29c08a1bea2a…`）错写成
`(allow platform_app default_prop (property_service (set)))`：secilc 编得过、
`debugfs` 回读也确认规则进了镜像（第 5051 行），但运行时照样被拒。

原因是属性上下文按**最长前缀**匹配：`plat_property_contexts` 第 50 行

```
sys.    u:object_r:system_prop:s0
```

已经把整个 `sys.*` 前缀拿走；第 181 行的 `*  u:object_r:default_prop:s0` 只给
完全没命中任何前缀的属性兜底。当时 `grep '^sys\.'` 在 `adb shell` 里被转义吃掉、
返回空，才误判成 `default_prop`。查属性域必须把整张表 grep 出来看具体前缀。

