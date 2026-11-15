# `com.samsung.android.scpm` 的 uid 5009 解析不到 SELinux 用户 → zygote 致命错误循环

- 机型：samsung / Galaxy S25 Ultra（`SM-S9380`，`ss9380`，OneUI Android 16）
- 发现时间：2026-10-30
- 盘集：修复前 index 3 → build15 通过
- 状态：已解决（scpm 正常启动，JNI FatalError 归零）

## 一、问题

`com.samsung.android.scpm`（系统 priv-app，uid 5009）每次启动都让 zygote 直接
`Runtime aborting`，日志里反复出现：

```
E SELinux : seapp_context_lookup_internal:  No match for app with uid 5009,
            seinfo platform:privapp:targetSdkVersion=35:partition=system:complete,
            name com.samsung.android.scpm  username oem_5009 appid 5009
E SELinux : selinux_android_setcontext:  Error setting context for app with uid 5009 ...
F zygote64: JNI FatalError called: (com.samsung.android.scpm)
            ... selinux_android_setcontext(5009, 0, "...", "com.samsung.android.scpm") failed
```

整份日志里该 FatalError 出现 **166 次**，scpm 始终起不来，依赖它的三星组件
（SCPM provider、Knox/系统服务发现）也一并报错。

## 二、根因

`/system_ext/etc/selinux/system_ext_seapp_contexts` 第 37 行给该应用声明的
`user` 是 `vendor_samsungcloud`。`selinux_android_setcontext()` 先按 uid 去
**vendor 的 passwd** 里查用户名，再拿用户名匹配 seapp_contexts：

- 官方三星 vendor 的 `/etc/passwd` 有 `vendor_samsungcloud::5009:5009::/:/bin/sh`；
- lineage 基座的 `/vendor/etc/passwd` 是 **0 字节**（`/etc/group` 同样为空）。

于是 libselinux 只能按 uid 合成 `oem_5009`，seapp_contexts 里没有这个用户名，
返回 “No match”，`setcontext` 失败。zygote 在 `forkAndSpecialize` 阶段调用
`JNI FatalError`，整个进程 abort，system_server 再拉起 scpm，形成循环。

## 三、证据

- 失败态：`log-fail.txt`
  - `No match for app with uid 5009 … username oem_5009` 原文；
  - 同一条链上的 `selinux_android_setcontext` 失败与 `JNI FatalError`；
  - 计数：`JNI FatalError called: (com.samsung.android.scpm)` = 166。
- 通过态：`log-pass.txt`
  - build15 中该 FatalError = 0；
  - scpm 以 pid 4029 正常启动，`ps -Z` 显示
    `u:r:platform_app:s0:c512,c768 vendor_samsungcloud 4029 … com.samsung.android.scpm`；
  - 最终 `vendor.img` 的 `/etc/passwd` 1439 B / 37 条、`/etc/group` 884 B / 37 条，
    含 `vendor_samsungcloud::5009:5009::/:/bin/sh`。

## 四、改动

`devices/manifest/samsung/ss9380/baselines/SM-S9380_3_20260813083211_652iw49crl_fac.json`
增加两条整文件替换（`vendor.replaces`）：

```json
{"source": "etc/passwd", "note": "官方 vendor /etc/passwd（1439 B、37 条）"},
{"source": "etc/group",  "note": "官方 vendor /etc/group（884 B、37 条）"}
```

取官方原文件而非补一行：passwd 与 group 的条目集合共同决定进程的附加组与
`getgrnam()` 结果，只补一行会留下其它三星系统用户/组的缺口；两件同源整体替换，
也与基线里其它官方件（libpng、health HAL）的口径一致。

## 五、验证

1. `07_build_vendor` → `11_pack_vda` 重跑，`debugfs` 从最终 `vendor.img` 读回：
   - `stat etc/passwd` → Size 1439，37 行；
   - `stat etc/group` → Size 884，37 行；
   - `cat etc/passwd | grep 5009` → `vendor_samsungcloud::5009:5009::/:/bin/sh`；
2. build15 开机：`JNI FatalError called` 总数 0（修复前 206，其中 scpm 166）；
3. guest `ps -Z`：scpm 域为 `platform_app`、用户为 `vendor_samsungcloud`，
   进程持续存活；
4. `logcat -b crash` 中 scpm 相关记录 0。

## 六、遗留

- 本修复只解决“进程起不来”。scpm 自身的注册/校验业务（`RegistrationVerifier`
  等）仍会周期性打日志，属功能缺失而非崩溃。
- `/vendor/etc/passwd`、`/etc/group` 是整文件替换，若后续换基线镜像含同名文件，
  需要重新核对这两份官方件的条目是否仍匹配本机 system_ext 的 seapp_contexts。
