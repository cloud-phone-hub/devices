# Issue: m22 现场合并策略编不出来（secilc 解析失败三连）—— 解决方案

- 设备: MEIZU 22（flyme/m22，Android 16，lineage 23.2 底座）
- 宿主: macOS (darwin, QEMU + hvf)，engine 流水线 01→11
- 状态: **已解决**（2026-10-28，实例 8）：`Compiling SELinux policy` 一次成功 →
  `Loading SELinux policy`，property area 与 invalid_context 均 0，`InitFatalReboot`
  不再出现；启动卡点转移到内核模块（见 issue-m22-system-modules-symlink）

---

## 1. 问题现象

`/odm` 预编译策略被屏蔽后（见 issue-m22-odm-precompiled-sepolicy），init 改走
`Compiling SELinux policy` 现场合并，但连续三轮都编不出来，每轮都是
`secilc → Failed to compile cildb: -2 → Unable to open SELinux policy →
InitFatalReboot: signal 6` 的死循环：

| 轮次 | 实例 | 报错 |
| :--- | :--- | :--- |
| 1 | 5 | `Failed to resolve permission nlmsg` @ `vendor_sepolicy.cil:2423`（`vendor/ot_rcp.te:21`） |
| 2 | 6 | `Failed to resolve permission firmware_load` @ `vendor_sepolicy.cil:3231`（`mainline/.../kernel.te:2`） |
| 3 | 7 | `Failed to resolve allow statement` @ `vendor_sepolicy.cil:4425`（无权限名 → 符号缺失） |

第三轮的 4425 行是 `(allow init_202504 pioneer_exec (file (…)))`：`init_202504` 在
patch 的 `plat_pub_versioned.cil` 里有 310 处声明，而 `pioneer_exec` 连
`(type pioneer_exec)` 都没有——allow 引用了不存在的 type。

## 2. 根因

| # | 事实 | 结论 |
|---|---|---|
| 1 | plat 的 class/权限表来自**内核 classmap**（本机 6.12.81），而 lineage vendor 策略的原始 te（`system/sepolicy/vendor/ot_rcp.te`）用了 Android 15+ 才在 `netlink_route_socket` 上引入的 `nlmsg` 权限和 permissionx 的 `nlmsg` kind | 权限名内核没有 → secilc 解析失败 |
| 2 | `vendor_firmware_file` 的 `(system (firmware_load))` 来自 mainline kernel.te，`firmware_load` 是 6.13+ 的 system class 权限位 | 同上 |
| 3 | `ot_rcp` 在本机 vendor 树里**不存在**（无 `vendor/bin/rcp`、无 init rc 引用），`firmware_load` 对 `vendor_firmware_file` 也没有实际调用方 | 降级/删除这两类语句不损失任何功能 |
| 4 | `pioneer` / `pioneer_exec` 是 cloud-droid-hub 自有域，声明本应在机型/模块层的追加片段里；m22 的 lineage-23.2 virtio 基底（plat + 基底 vendor cil）**没有**这两个 type，模块层的 `android.service.daemon/sepolicy/vendor_sepolicy.cil` 只补了 allow、没补 type | allow 引用未声明符号 → secilc 报「Failed to resolve allow statement」 |
| 5 | 符号级全表扫描（声明集 = plat + plat_pub_versioned + vendor；引用集 = allow*/allowx/typetransition + typeattributeset 成员）另查出 `vendor_perf_service`（2 处）、`vendor_perfservice`（1）、`vendor_dpmd`（1）也未声明 | 同类缺失，必须一并补 |

```
# 实例 7（第三轮）关键行
[    0.390479][    T1] init: /system/bin/secilc: Failed to resolve allow statement at /vendor/etc/selinux/vendor_sepolicy.cil:4425
[    0.391093][    T1] init: /system/bin/secilc: Failed to compile cildb: -2
[    0.391610][    T1] init: Unable to open SELinux policy
```

## 3. 证据

- `log-fail.txt`：三轮失败原文（实例 5/6/7 各取关键行）。
- `log-pass.txt`：实例 8 的 `Compiling SELinux policy` → `Loading SELinux policy`。
- 逐行定位依据（不入库的大文件，记录取法）：
  - `vendor_sepolicy.cil` 来自 patch.img：`debugfs -R "cat /vendor/etc/selinux/vendor_sepolicy.cil" patch.img`；
  - 声明/引用差集用 6.12 内置的 plat cil（`/system/etc/selinux/plat_sepolicy.cil`）作对照，
    注意 plat 用 `(class X ())` + `(classcommon X file)` 的**分解式**，统计必须把 common
    权限并入 class，否则会得到几百条假阳性（第一版脚本就栽在这里）；
  - gold 侧的写法参考：从 gold 的 patch.img 提 `vendor_sepolicy.cil`，`pioneer` 段落
    （第 601–607 行）是 `(type pioneer)` / `(roletype object_r pioneer)` /
    `(typepermissive pioneer)` / `(type pioneer_exec)` / `(roletype object_r pioneer_exec)`。

## 4. 改动

### 4.1 新增机制：09_build_patch 的 vendor cil 定向改写

`engine/build/09_build_patch/cil.go`（新建）+ `plan.go` / `layout.go` 接线：

- 清单新增顶层 `patch.vendor-cil-rewrite`（`from` / `to` / `count`），09 在
  「屏蔽落点校验」之后、取元数据之前就地改写 `/vendor/etc/selinux/vendor_sepolicy.cil`；
- **必须按 count 精确命中**，命中数不符直接让 09 失败（静默失效会让「策略又编不过」
  的回归无迹可寻）；`to` 为空表示删语句，替换成 `;;` 注释便于事后取证；
- 规则表指纹（`cil:` + sha256(JSON)[:12]）进 patch 配方，规则变了缓存必然失效。

### 4.2 清单里落地的 4 条规则（`manifest/flyme/m22/baselines/12.5.0.0A.json`）

| # | 原语句（节选） | 改成 |
| :--- | :--- | :--- |
| 1 | `(allow ot_rcp self (netlink_route_socket (nlmsg)))` | 降级为 `nlmsg_write`（内核有该位） |
| 2 | `(allowx ot_rcp self (nlmsg netlink_route_socket (0x16 … 0x7a)))` | 删（permx kind 内核无） |
| 3 | `(allowx ot_rcp self (nlmsg netlink_route_socket (0x12)))` | 删 |
| 4 | `(allow kernel_202504 vendor_firmware_file (system (firmware_load)))` | 删（6.13+ 权限位） |

### 4.3 补 type 声明（符号级缺口的正解）

- `engine/vendor/hardware/android.service.daemon/sepolicy/vendor_sepolicy.cil`：
  补 `pioneer` / `pioneer_exec` 的 `type` + `roletype`（`pioneer` 另加
  `typepermissive`，与 gold 一致；启动期日志会打印 `cdh: pioneer (type N) forced permissive`）。
- `devices/manifest/flyme/m22/sepolicy/vendor_sepolicy.cil`：补
  `vendor_perf_service` / `vendor_perfservice` / `vendor_dpmd` 的 `type` + `roletype`，
  并纠正原注释里「这三者属底座自带」的错误说法（基底根本没有）。

## 5. 验证

| # | 验证点 | 结果 |
|---|---|---|
| 1 | 09 日志 | `改写 …（清单 4 条规则）`，4 条各「命中 1 次」 |
| 2 | patch.img | 三次重出配方随规则变化：`099cf43da891` → `294316d25ab6` → `b7097b50e4ac` → `3dcc195d13fe` |
| 3 | 策略加载（实例 8） | `No precompiled sepolicy at /vendor/etc/…` → `Compiling SELinux policy` → `Loading SELinux policy`，全程无 secilc 报错 |
| 4 | 属性区 | property area 失败 0、invalid_context 0、`InitFatalReboot` 0 |
| 5 | 残留 avc | 27 条但均非致命（keymint 读 vendor_default_prop 15 次、crash_dump ptrace 3 次等），不再阻断 |

## 6. 遗留

1. 规则 #1–#4 是「按当前 lineage vendor 策略原文精确匹配」的快照：换底包（lineage 版本
   或 vendor 策略分支）时必须重跑一遍符号级扫描，命中数不符会让 09 直接失败并指明是哪条。
2. `memfd_file` class 仍提示 `not defined in policy`（内核 classmap 与 plat cil 版本
   差）；当前只影响该 class 的判定（一律 deny），未观察到功能影响。
3. avc 里 keymint 读 `vendor_default_prop` 被拒属于属性上下文对齐范畴，属独立议题。
