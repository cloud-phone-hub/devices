# 12.5.0.0A 启动失败（第二轮）：`/odm` 预编译策略短路 `/vendor`，property area 初始化失败

- 设备: flyme/m22 12.5.0.0A（MEIZU 22，Flyme 12.5.0.0A / Android 16 / lineage 23.2 virtio 底盘）
- 宿主: macOS (darwin, QEMU 10 + hvf)，daemon 控制台 `127.0.0.1:9050`
- 状态: **已定位并修复待验证**（2026-10-28；内核改动已提交，02→11 重跑中，第 5 节随实测补）
- 前置: 本轮承接 #49（`issue-m22-boot-path-redirect-missing`），那里已把 6.12 内核的
  CDH_PATH_REDIRECT 移植完，`/mnt/patch` 不再被 vold 撕掉、`fstab.qcom` 注入生效。

---

## 1. 问题

#49 修完后重跑 02→11，实例 `m22_12.5.0.0A/3`（19:05:46 起）能挂上 `/mnt/patch`、
`cdh_redirect` 也按预期打印，但启动在 **0.31s** 停在属性区初始化，循环 15 次：

```
[    0.284321][    T1] cdh_redirect: /mnt/patch/vendor/etc/fstab.qcom
[    0.285651][    T1] init: Opening SELinux policy
[    0.289375][    T1] init: Loading SELinux policy
[    0.293968][    T1] SELinux:  Class memfd_file not defined in policy.
[    0.317444][   T78] avc:  denied  { mac_admin } for  pid=1 comm="init" capability=33
                                             scontext=u:r:init:s0 tclass=capability2 permissive=0
[    0.317925][    T1] init: Failed to initialize property area
[    0.318084][   T78] audit: type=1401: op=setxattr invalid_context="u:object_r:vendor_camera_remapid_prop:s0"
[    0.318538][    T1] init: InitFatalReboot: signal 6
[    0.319524][   T78] audit: type=1401: op=setxattr invalid_context="u:object_r:vendor_camera_skip_failed_ids_prop:s0"
[    0.320613][   T78] audit: type=1401: op=setxattr invalid_context="u:object_r:vendor_create_partition_table_prop:s0"
[    0.939520][    T1] reboot: Restarting system with command 'recovery'
```

`grep -c "Failed to initialize property area"` = 15、`zygote` = 0。

## 2. 根因

**init 加载的是 `/odm/etc/selinux/precompiled_sepolicy` 这帧预编译二进制，完全没有走
cil 合并，于是 patch 里 `/vendor/etc/selinux/vendor_sepolicy.cil` 的类型声明全部无效。**

1. second stage init 按 `/system` → `/vendor` → `/odm` 找预编译二进制；命中就直接
   mmap 装载（本机 `Opening` 到 `Loading` 只隔 3.7ms，日志里既没有 `Compiling SELinux
   policy`，也没有 gold 那条 `No precompiled sepolicy at /vendor/...`）。
2. 官方 `system_a.img` 的 `/system/etc/selinux` 只有 cil 和 contexts（18 项，没有
   precompiled）；`/vendor` 被重定向到 patch，patch 的 selinux 目录也没有 precompiled；
   **flyme 把预编译放在了 `/odm`**：`precompiled_sepolicy` +
   `precompiled_sepolicy.{plat,product,system_ext}_sepolicy_and_mapping.sha256`。
3. 该二进制是用**官方 vendor** 的策略编的（`strings` 里 `vendor_qms_0` 在、
   `vendor_camera_remapid_prop` 不在）。而运行时 `/vendor` 是 patch 的 lineage 内容，
   `vendor_property_contexts` 里有 3 条属性指向官方二进制里没有的类型：
   `vendor.camera.remapid` / `vendor.camera.skip_failed_ids` /
   `vendor.create_partition_table.`
4. `setxattr(security.selinux)` → `security_context_to_sid()` 返回 `-EINVAL` →
   内核退一步要 `CAP_MAC_ADMIN` 强制转换（`security/selinux/hooks.c:3258` 的
   `has_cap_mac_admin()`）→ init 没有该权限 → 审计出 `mac_admin denied` →
   setxattr 失败 → `PropertyInit()` 的 MUST 失败 → `InitFatalReboot`。

**`mac_admin` 是连带现象，不是独立缺陷**：类型存在时这条 fallback 根本不会走。
`SELinux: Class memfd_file not defined in policy` 同理只是「内核 class 表比策略新」的
提示（内核 6.12 class map 里有 memfd_file，flyme 的 plat 策略没定义），不影响启动。

**为什么 gold 不踩**：gold 的官方固件没有 odm 预编译策略，init 找不到二进制就
`Compiling SELinux policy`，用运行时实际的 plat + vendor(含追加段) 现场合并 ——
它在 v1 时代也踩过「vendor 属性类型没声明」的坑，是靠
`../../gold/issue-vendor-property-area-align` 的 cil 追加段补类型解决的。
m22 的差分只有一处：**flyme 把预编译放进了 /odm**。

## 3. 证据

- `log-fail.txt`：15 次循环的时间线、`has_cap_mac_admin()` 源码路径、odm/system/vendor
  三处 selinux 目录清单、sha256 相等证明「校验必过」、三处 strings/type 比对、
  `plat_sepolicy_vers` 假设的排除实验（实例 4）、gold 的 `Compiling` 对照。
- 关键数字：`precompiled_sepolicy` 1405958 B；odm 的 plat sha 与 system 侧
  `plat_sepolicy_and_mapping.sha256` 同为 `797927f6…`；lineage `vendor_sepolicy.cil`
  319418 B（含 `(type vendor_camera_remapid_prop)` 第 748 行）；官方 flyme
  `vendor_sepolicy.cil` 1216306 B（不含该 type）。

## 4. 改动

**内核（`devices/kernels/lineage-23-6.12`，提交 `90a6eb9fe`）**

| 文件 | 改动 |
| --- | --- |
| `fs/namei.c` | 前向表增第 10 条 `/odm/etc/selinux/precompiled_sepolicy` → `/mnt/patch/odm/etc/selinux/precompiled_sepolicy`（文件精确匹配）；逆变换表同步加一条；注释写清「为什么屏蔽预编译」与 `/odm 三条规则` |

逻辑：patch.img 里该落点**不存在**（09 硬校验），重定向后 init 拿到 ENOENT → 三个候选
全部落空 → 回退 `Compiling SELinux policy`，用运行时实际文件集（plat + system_ext +
product + vendor，含机型 sepolicy 片段）现场合并，策略才与 `/vendor` 一致。

与 #49 的 `/mnt/patch` 卸载保护、`update_verifier` 屏蔽一样，这条也属于「patch 覆盖
odm 内容」的口径；gold 的 6.6 内核不受影响（它本来就没有 odm 预编译策略，且 6.6 未移植
这条规则）。

**引擎（`engine/build/09_build_patch/layout.go`）**

- `mustAbsent` 增加 `odm/etc/selinux/precompiled_sepolicy`：屏蔽落点必须真的不存在，
  否则「指向不存在的覆盖」会反过来命中真文件；
- `layoutVersion` 追加 `;odm-selinux=none`，让旧缓存失效、09 必须重新校验。

## 5. 验证

（待补：02 重编配方 → 03 模块符号版本 → 04→11 续跑 → 实例启动实测。判据：日志出现
`Compiling SELinux policy`、`op=setxattr invalid_context` 计数为 0、
`Failed to initialize property area` 计数为 0、`zygote` 起来、`sys.boot_completed=1`。）

## 6. 遗留

- 现场合并的耗时与成功率依赖 plat 侧 libsepol 对 lineage vendor cil 的兼容性：若合并
  报错（`Failed to resolve ...`），init 会 fatal 重启，日志会指明具体类型/版本冲突；
  此时退路是把 `/vendor/etc/selinux` 的属性上下文表换成与官方二进制配套的版本
  （v1 在 gold 上验证过「整目录替换不可用」，只能换 contexts 表 + 补类型）。
- `Class memfd_file not defined in policy` 会在每次启动出现一次，属内核 class 表比策略新
  的提示，不影响功能；若要消掉，需要 plat 侧策略补 class 定义（超出本仓库范围）。
- 内核提交仍在本地（`90a6eb9fe` 叠加在 `9a41f692c` 上），未推送 graft 仓。
- 实验遗留：验证 `plat_sepolicy_vers.txt` 假设时曾就地把 dist 的 patch.img 改成 `202404`，
  该文件在 09 重出 patch、11 重打盘集后自然覆盖；`out/devices/**/img/patch.img` 未被改动。
