# 15R 云机：/odm 由「逐条遮蔽 23 个路径」改为整棵重定向到载体 /mnt/vendorpatch/odm

- **设备/机型**：OnePlus 15R（CPH2769 / ColorOS 16.0.10.500_EX01 / 骁龙 SM8750 `canoe` /
  Android 16 + lineage-23.2 底座）
- **固件基线**：`devices/manifest/oneplus/15r/baselines/CPH2769_16.0.10.500_EX01_A51_full.json`
- **首次暴露**：不是故障，是口径变更——用户要求「**不复制厂商的 /odm**」：patch 的 `/odm`
  只放我方内容，官方 odm 一律不可见，不为它增加 3.5 GB
- **结论**：把 `cdh_redirect.rules` 里 23 条 `/odm/**` 单文件规则换成
  `dir /odm → /mnt/vendorpatch/odm` 一条整棵规则，并**必须**在它前面加一条
  `file /odm → /odm` 自映射空操作，否则首阶段 `mount(odm_a, "/odm")` 会挂不上、整机起不来

---

## 一、问题

改前 15r 的 `/odm` 是「按需逐条遮蔽」：遇到哪个官方文件想屏蔽（VINTF 片段、rc、同名库、
audio policy、预编译策略…），就在规则表里加一条 `file /odm/...`。累计 23 条，且每遇到一个新
卡点就要再加一条。

官方 odm 有 **3855 个条目**（`du -sm`：`lib64` 1200 MB、`etc` 627 MB、`firmware` 35 MB、
`lib` 11 MB、`bin` 7 MB），逐条遮蔽只覆盖 23 个点，其余全部照旧可达：

- `/odm/lib64` 里 1.2 GB 的同名库（`libui.so`、`libgui.so`…）会被 linker 按 `LD_LIBRARY_PATH`
  顺序命中，与 lineage `/vendor/lib64` 的同代副本混用；
- `/odm/etc/camera`（627 MB 里的大头）、`/odm/firmware`、`/odm/etc/audio`、`/odm/overlay`
  对云机没有任何意义，却在 `init` 解析 rc、`hwservicemanager` 扫 VINTF 时照样被读；
- 每次官方布局变化（换基线）都要重新枚举一遍，漏一条就多一个「声明有、实现旷缺」的卡点。

## 二、根因

不是故障，是**口径**问题：`/odm` 的正确语义是「patch 提供什么就是什么」，而逐条遮蔽的语义是
「官方 odm 仍是底座，只在冲突处贴补丁」。后者与「不复制厂商 /odm」相矛盾，且成本随官方条目数
线性增长。

换成一条 `dir /odm` 之后会遇到一个**隐蔽的引导期风险**，这是本条目真正要记的东西：

> **`mount(2)` 的目标路径同样会过 `getname_flags`。**

`devices/kernels/lineage-23-6.12/fs/namespace.c` 的 `do_mount()` 用
`user_path_at(AT_FDCWD, dir_name, LOOKUP_FOLLOW, &path)` 解析挂载点，走的就是
`getname_flags()`——与普通 `open()` 同一条路径、同样被 `cdh_redirect` 的 kretprobe 改写。

而首阶段挂载顺序是：`odm` 先挂、载体 `vdc → /mnt/vendorpatch` 后挂（实测只差 19 ms）：

```
[    0.247434][    T1] init: [libfs_mgr] __mount(source=/dev/block/dm-5,target=/odm,type=erofs)=0: Success
[    0.265829][    T1] init: [libfs_mgr] __mount(source=/dev/block/vdc,target=/mnt/vendorpatch,type=ext4)=0: Success
```

若把**精确的 `/odm`** 也改写成 `/mnt/vendorpatch/odm`，此刻载体还没挂上 → 目标不存在 →
`mount` 返回 ENOENT → 首阶段 FATAL → 起不来。

历史规则里从来没踩到这一点，是因为 `dir /vendor/etc` 这类规则不匹配精确的 `/vendor`，
而 `file /odm/build.prop` 之类的精确规则又恰好都不是挂载点。

## 三、证据

失败侧口径（`log-fail.txt`，实例 10 与规则表实测）：

- 23 条 `/odm/**` 单文件规则的完整清单（现以注释形式保留在规则表里）；
- 官方 odm 3855 条目、`lib64` 1200 MB / `etc` 627 MB / `firmware` 35 MB 全部仍可达；
- 实例 10：`in property file '/odm/etc/build.prop'` 属性 set 被 SELinux 拒 **64** 次、
  `sys.init.updatable_crashing` 被点亮 **116** 次、`vendor.secure_element` 退出 **129** 次、
  `vendor.oplus.vibrator` 退出 **127** 次——都是「官方 odm 的 rc 仍被解析、二进制却拿不到
  配套实现」这一类；
- 挂载时序：`odm` 0.244563 s vs `/mnt/vendorpatch` 0.263692 s（实例 10 日志）。

通过侧（`log-pass.txt`，实例 11）：

- `/proc/cdh_redirect` 12 条规则、`forward_missed 0` / `reverse_missed 0`；
- `__mount(source=/dev/block/dm-5,target=/odm,type=erofs)=0: Success`（自映射护栏生效，
  没被改写到载体）、`/odm/build.prop` 与 `/mnt/vendorpatch/odm/build.prop` md5 一致；
- `/odm/etc/vintf`、`/odm/etc/init`、`/odm/etc/audio`、`/odm/etc/selinux`、`/odm/lib64`、
  `/odm/firmware`、`/odm/etc/build.prop` 全部 ENOENT；`/odm/build.prop` 存在（我方顶层件）；
- `getprop ro.oplus.wifi.wsa` = `none`（#127 未回退）；
- `sys.init.updatable_crashing` 116 → **0**、`vendor.secure_element` 129 → **0**、
  `vendor.oplus.vibrator` 127 → **0**（它们的 rc 在 `/odm/etc/init`，整棵后连服务定义都没了）；
- `property file '/odm/etc/build.prop'` 归零（该文件已不存在）；顶层件仍报 128 条属性 set 被拒
  （first stage 与 second stage 各拒一遍，属 #127 已知类别，最终值仍正确）；
- `NULL VINTF MANIFEST` 0 次；`sys.boot_completed=1`、launcher 前台、`FATAL EXCEPTION IN
  SYSTEM PROCESS` / `WATCHDOG KILLING` 均 0。

## 四、改动

**`devices/manifest/oneplus/15r/kernel/cdh_redirect.rules`**（唯一改动，纯配置）：

```
# 自映射护栏：/odm 这个名字本身不改写
file /odm        /odm
# 整棵：/odm/** 全部落到 patch 的 odm 子树
dir  /odm        /mnt/vendorpatch/odm
```

- 23 条旧 `file /odm/...` 规则**全部注释保留**（AGENTS.md 的停用口径），各段标题改为
  「停用：…」并注明「整棵后该路径必然 ENOENT」；
- 自映射之所以安全：`core/rules.c` 里 `dir` 的匹配是「精确相等或其后紧跟 `/`」、`file` 只认
  精确；命中后 `grow == 0` 时 `memcpy` 原样返回、等价于空操作；首条命中生效，所以自映射必须
  排在 `dir /odm` 之前。

配套（同一轮，另有条目）：

- `06_build_odm` 判据从「`odm/etc/build.prop` 必备」改为「清单 `odm/` 目录存在」，
  属性件顶层 `odm/build.prop` 优先、回退 `etc/build.prop`，并加「清单为准」的陈旧件清理 +
  空目录剪枝（否则 `odm/etc/vintf/manifest` 这类空壳仍会进镜像）；
- `internal/carrier` 的规则守卫允许 `file X X` 自映射例外（`dir` 自映射仍然报错）。

构建链：`build_odm`（06，odm.img 3 条目）→ `build_vendor_boot`（05，rules 18886 B 进
vendor_ramdisk）→ `build_patch`（09，patch.img 657 条目）→ `pack_vda`（11，vda.img 五分区刷新）。

## 五、验证

实例 11（2026-11-03 10:19 启动，`sys.boot_completed=1`，开机 72 s）：

| 指标 | 改前（实例 10） | 改后（实例 11） |
| :--- | :--- | :--- |
| `/odm` 规则形态 | 23 条逐文件 | `file /odm /odm` + `dir /odm /mnt/vendorpatch/odm` |
| `/odm` 精确挂载 | 0: Success | **0: Success（自映射保住了挂载点）** |
| `/odm/lib64`、`/odm/firmware`、`/odm/etc/{vintf,init,audio,selinux}` | 可达 | **ENOENT** |
| `/odm/build.prop` | 官方件 | **我方件**（md5 与载体一致，`ro.oplus.wifi.wsa=none`） |
| `sys.init.updatable_crashing` | 116 | **0** |
| `vendor.secure_element` / `vendor.oplus.vibrator` 退出次数 | 129 / 127 | **0 / 0** |
| `property file '/odm/etc/build.prop'` 被拒 | 64 | **0**（文件不存在） |
| `NULL VINTF MANIFEST` | 有 | **0** |
| `forward_missed` / `reverse_missed` | — | **0 / 0**（12 条规则全命中） |

## 六、遗留

1. **相对路径仍可达官方 odm**。`cdh_redirect` 改写的是 `getname_flags()` 拿到的路径字符串，
   而 `openat(dirfd, "etc/vintf", ...)` 这种相对解析的 `name` 里没有 `/odm` 前缀，规则不匹配。
   实测：`cd /odm && ls etc` 能看到官方 `/odm/etc` 的条目，而 `ls /odm/etc` 是 ENOENT。
   影响面小（init / HAL / app 都用绝对路径），但**不是零**；彻底解法见第 2 条。
2. 官方 odm 仍挂在 `/odm`（3.5 GB erofs），只是除「`/odm` 这个名字」外没有绝对路径能进去。
   彻底解法是把 super 的 `odm` 分区换成 06 的 `odm.img`（10_pack_super 已支持从分区镜像目录
   取 `odm.img`），那样 `/odm` 挂的就是我方内容，连自映射护栏都不需要。代价是重打 super +
   重开实例，留作下一轮。
3. `/odm/build.prop` 仍有 128 条属性 set 被 SELinux 拒（first/second stage 各一遍）。根因是
   odm 的 `property_contexts` 由 `/odm/etc/selinux/*` 提供，而该目录已被整棵屏蔽；属 #127
   已记录的类别，抽查的关键属性（`ro.oplus.wifi.wsa`、`ro.odm.build.fingerprint`）值均正确。
4. 本轮后半段的卡死与本条目无关（NFC 崩溃风暴 + 内核自旋锁），另见
   `../issue-nfc-crash-loop/` 与 `../issue-kernel-spinlock-hang/`。
