# cdh_redirect —— 路径重定向外部内核模块

把「`/vendor` 之类的虚拟路径 → 载体物理路径」的重定向从**内核源码补丁**
（`CONFIG_CDH_PATH_REDIRECT`）搬成一个**外部模块**：内核树保持 lineage 上游原样，
重定向目录按机型写成数据文件，同一个 `.ko` 二进制服务所有设备。

- 落地机型（均已起机复测，2026-11-02）：`google/oriole`（Pixel 6 / Android 14 /
  lineage-21，内核 `6.1.138-4k-cdh`，10 条规则）、`xiaomi/gold`（Redmi Note 13 5G /
  lineage-22.2，`6.6.127-4k-cdh`，10 条）、`flyme/m22` / `oppo/cph2797` /
  `samsung/ss9380`（lineage-23.2，`6.12.81-4k-cdh`，各 12 条），同一个 `.ko` 源码
  按内核树编出三份二进制，规则文件每机型一份
- 规则已备但尚未起机复测的机型（等其自身 bring-up）：`nothing/3a`（10 条）、
  `oneplus/CPH2769`（12）、`oneplus/PJE110`（10）、`realme/rmx3823`、`rmx5112`、
  `rmx6688`（各 10）、`xiaomi/popsicle`（11）、`oppo/PME110`（待补基线）
- 证据：`devices/evidence/common/issue-cdh-redirect-kernel-module/`（首发，oriole）与
  `devices/evidence/common/issue-cdh-redirect-full-migration/`（三棵树全面移植 + 五机型复测）
- 规则样例：设备表 `devices/manifest/devices/google/oriole.json` 的 `modules.redirect`（10 条，含
  `update_verifier` 屏蔽）、`devices/manifest/devices/flyme/m22.json` 的 `modules.redirect`
  （12 条，多 `/system/lib/modules` 与 `/odm` 预编译策略屏蔽），构建期物化在
  `out/devices/manifest/<brand>/<device>/device.json`

## 1. 它做了什么

两条语义，各由一个 kretprobe 实现：

| 方向 | 挂点 | 做的事 |
| :-- | :-- | :-- |
| 前向（查找） | `kretprobe getname_flags` | 返回处拿 `struct filename *`，当进程 `uid <= max_uid`（缺省 `10000`）时把 `f->name`（内核缓冲区）里的虚拟路径前缀换成物理前缀；`uid > 10000`（普通 App）不做分区重定向（直达原厂只读分区），且直接探测 `/mnt/vendorpatch` 或 `/data/adb`（及其子路径）时改写为不存在路径返回 `-ENOENT` |
| 逆向（显示） | `kretprobe proc_pid_readlink` | `/proc/<pid>/{exe,cwd,root,fd/N}` 的出口，当 `uid <= max_uid` 时把物理路径还原成虚拟路径 |
| 逆向（显示） | `kretprobe vfs_readlink` | `readlink(2)` 的公共出口，同上（幂等，第二层通常只读不写） |

为什么要两层逆向：`readlink(2)` 的调用链是 `do_readlinkat()`（`fs/stat.c`）→
`vfs_readlink()`（`fs/namei.c`），两个编译单元不会内联；`proc_pid_readlink` 只是它下面
覆盖 proc 魔术链接的那一层。实测只挂 proc 层时 `readlink /proc/$$/cwd` 拿不到改写，
而 `/proc/self/fd/N` 正常 —— 两者不是同一条路。

逆向为什么必须做：前向只改「查找」，`d_path()` 看到的仍是物理挂载点。bionic linker 用
`realpath(/proc/self/exe)` 去匹配 `ld.config.txt` 的 `dir.*` 段（`dir.vendor = /vendor/bin`），
拿到 `/mnt/vendorpatch/vendor/bin` 一段都匹配不上，vendor HAL 就落进兜底段、拿不到
`sphal` 命名空间。

为什么不搬 `ksys_umount` 兜底：那是 #101 为 `/mnt/patch` 落点加的（vold 的 `unmountAll()`
会把 `/mnt` 下的非白名单子挂载当 stale mount 撕掉）。现在载体是 `/mnt/vendorpatch`，
以 `/mnt/vendor` 为**字符串前缀**命中 vold 白名单，不再需要拦截。若某机型仍要挂
`/mnt/patch`，得另想方案（模块不提供）。

## 2. 规则文件

```
# 一行一条，# 起注释
<dir|file> <源前缀（虚拟路径）> <目标前缀（物理路径）>
```

- `dir`：精确相等或其后紧跟 `/`，覆盖整棵子树；`file`：只认精确相等。
- `file X X` 自映射是「屏蔽」的对偶：命中后原样写回（`grow == 0`），语义是
  「这个路径本身不改写」。给「路径本身另有用处」的目录当护栏用：`/odm` 既是整棵
  要交给载体的子树，又是 init early mount 的 super 逻辑分区挂载点——单独一条
  `dir /odm` 会把 init 对 `/odm` 的 `realpath`/`mount` 也改写到载体上，载体（vdc）
  那时还没挂，实测 `failed to realpath(/odm)` → `Failed to mount required partitions
  early` → `InitFatalReboot: signal 6`。自映射必须排在对应 `dir` **之前**（同一路径只应用
  最先命中的一条）；`internal/carrier` 的测试只允许 `file X X` 形态的自映射，
  避免一条 `dir` 自映射把整棵子树的重定向静默关掉。
- **逆变换自动派生**：把每条的源/目标互换，不需要另写一份。
- **顺序有意义**：同一路径只应用最先命中的那条（与内核补丁一致）。一个别名目标会
  造成逆向歧义，靠顺序消歧：6.12 的 `dir /system/lib/modules ->
  /mnt/vendorpatch/vendor_dlkm/lib/modules` 必须排在 `dir /vendor_dlkm/lib` **之后**，
  这样物理前缀 `/mnt/vendorpatch/vendor_dlkm/lib/...` 会先被前一条逆向成
  `/vendor_dlkm/lib/...`（与内核补丁的呈现一致）；反序就会让所有 dlkm 模块路径
  显示成 `/system/lib/modules/...`。
- 上限：`CDH_RULES_MAX = 32` 条、单条路径 `CDH_RULE_PATH_MAX = 2048`，超了报错而不是
  静默丢弃；**一条规则都没有时模块拒绝加载**（`-EINVAL`）—— 空转比加载失败更危险：
  启动看起来正常，实则 `/vendor` 还指着官方树。
- 缺省路径 `/lib/modules/cdh_redirect.rules`，可用 `insmod cdh_redirect.ko rules_path=…`
  或调试用的内联参数 `rules="dir:/vendor/lib:/mnt/vendorpatch/vendor/lib;file:…"` 覆盖。

## 3. 构建与注入

1. `03_build_modules`：baseline 里 `rom.lineage.kernel.modules` 含 `"cdh_redirect"`
   （见 `out/devices/manifest/<brand>/<device>/baselines/*.json`，由三表物化）→ 对该目录做 `make M=… modules`
   → `llvm-strip --strip-debug` → 落到 `out/devices/kernel/<tag>/modules/cdh_redirect.ko`。
2. `05_build_vendor_boot`：把模块塞进 **vendor_ramdisk 根级**（initramfs），不是 `/vendor`：

   ```
   lib/modules/<kernelrelease>/cdh_redirect.ko
   lib/modules/<kernelrelease>/modules.load     # 一行 cdh_redirect.ko
   lib/modules/<kernelrelease>/modules.dep      # 一行 "cdh_redirect.ko:"
   lib/modules/cdh_redirect.rules               # 机型规则
   ```

   规则表是机型 device.json 的 `modules.redirect`（**只放对象元素** `{"type","from","to"}`；
   分段标题与逐条说明放该机型的 `manifest/<vendor>/<device>/README.md`，不写进数组），
   05 渲染成 `<type>  <from>  <to>` 文本形态；`--no-redirect` 可整体关闭本机型注入。
   渲染器仍支持字符串元素当注释原样输出，但 2026-11-03 起 13 份 device.json 已统一
   只放规则对象——注释混在规则里会被误当数据读，且在 JSON 清单里没法正常阅读。
   **配方号（`vboot-…`）的输入哈希含 `.ko`、加载参数与规则内容**，改任一都会自动失效重打。

为什么必须在 vendor_ramdisk：模块要在 **first stage**（`LoadKernelModules()`，
SwitchRoot 与 SELinux 策略加载之前）就位，因为首屏就要读 `/vendor/etc/selinux/*`、
`/vendor/etc/fstab.*`；等 `/vendor` 挂上来再加载就晚了。同理规则文件必须和 `.ko` 一起
待在 initramfs 里。

## 4. 运行时观测

```
# cat /proc/cdh_redirect
rules: 10/32
  dir  /vendor/etc -> /mnt/vendorpatch/vendor/etc
  …
max_uid: 10000
forward_hits: 11395
reverse_hits: 3010
uid_skipped: 42
forward_missed: 0
reverse_missed: 0
readlink[proc]: entries=55805 rets=55805 hits=3010 skipped=52795
readlink[vfs]: entries=58904 rets=58904 hits=0 skipped=58904
```

- `max_uid` / `uid_skipped`：重定向生效的最大 UID（缺省 `10000`，可通过 `/sys/module/cdh_redirect/parameters/max_uid` 动态调整）以及因 `uid > max_uid` 跳过前向重定向的命中次数。
- `*_missed`：命中规则但改写放不下（容量不足）。正常恒 0，非 0 说明路径长度假设破了。
- `readlink[vfs] hits` 恒 0 属预期：proc 层已改写，vfs 层读回的是虚拟串、命中不到
  逆向规则（幂等，只读不写）。
- 逐次打印：`echo 1 > /sys/module/cdh_redirect/parameters/debug`（`module_param debug`）。
  **会刷爆 dmesg**（实测一条 `readlink` 命令 ~900 行），取证完记得关。
- 卸载：`rmmod cdh_redirect` 会打印两个 kretprobe 的 `nmissed`（漏抓）。

## 5. 已知坑

1. **MTE 标签的用户缓冲区**：toybox 的 `readlink` applet 用 `malloc(64)`，
   指针带标签（`b400…`）。arm64 的 `access_ok()` 只在 `PF_KTHREAD` / `TIF_TAGGED_ADDR`
   时剥标签，别的进程带标签就是「越界」→ `copy_from_user_nofault()` 直接 `-EFAULT`，
   表现为「钩子进去了、日志没有读回、结果没变」。模块在入口处自己 `untagged_addr()`；
   返回值写回用同一个（已剥离的）指针。
2. **符号名依赖**：`getname_flags` / `proc_pid_readlink` / `vfs_readlink` 三个符号必须存在，
   且「第 2、3 个参数是 buffer/buflen」的假设要成立。6.1 / 6.6 / 6.12 实测通过；
   注意 6.12 的 `getname_flags()` 已只有两个参数（少了 `empty` 出参），但本模块只取
   返回值，不解析入参，所以不受影响。
3. **旧的构建二进制静默不注入**：`out/agent/dist/cdh-pipeline` 没重编时，06 不会打印
   `==> 重定向模块: …` 那一行，vendor_ramdisk 里就是没有模块，起机表现为
   `init: Unable to open /lib/modules, skipping module loading.`（实例 21 踩过）。
   改了 `agent/modules/pipeline/stages/06_build_vendor_boot/` 就要
   `cd agent && TMPDIR=$PWD/../out/agent/tmp go build -o ../out/agent/dist/cdh-pipeline ./cmd/cdh-pipeline`。
4. **改 `.ko` 必须重跑 05 → 09 → 11**：模块只在 vendor_boot 里，`patch.img`/`vda.img`
   不带它；只重跑 11 拿不到新模块。按 #101/#102 的经验，`10_pack_super` 对这种机型是 no-op。
   **08 不再往 vendor_dlkm 里塞它**（`08_build_vendor_dlkm` 的 `firstStageModules`）：
   除了会在 late-init 被 `dlkm_loader` 重复加载一次（EEXIST，日志里只有
   `Loading module /vendor_dlkm/lib/modules/cdh_redirect.ko` 而没有对应的
   `Loaded kernel module`，容易误以为模块来自 `/vendor_dlkm`），还会让「模块到底从
   哪来」这件事有两处真源。08 的配方（`dlkm-v2`）已把「实际注入的 .ko 集合」纳入
   哈希，改注入集不会再命中旧缓存。
5. **调试参数与计数会漂**：`/proc/cdh_redirect` 的 `entries` 数量级很大（一次命令就能
   6 万次），因为每个新起的进程 bionic 都会 `realpath` 一串目录；对比实验要 `dmesg -C`
   清屏并按 `pid=` / `comm=` 过滤。

## 6. 迁一台新机型要做什么

1. 在设备表 `devices/manifest/devices/<brand>/<device>.json` 里写 `modules.redirect`
   （照抄该机型原内核补丁里的白名单；落点路径按 patch.img 的装法定）。**数组里只放对象
   元素**（一条规则一个），分段标题与「为什么这么改」写进同目录 `README.md` 的
   `modules.redirect` 一节——那份说明与规则本身同等重要，别丢，也别塞回 JSON 数组。
2. baseline 的 `modules.external` 加 `"cdh_redirect"`。
3. 该机型用的内核树去掉 `CONFIG_CDH_PATH_REDIRECT`（连同 `fs/namei.c`、
   `fs/proc/base.c`、`fs/namespace.c`、`fs/Kconfig`、`include/linux/namei.h` 五处补丁），
   内核树回到上游分支（2026-11-02 起三棵树已全部回退，新机型直接用干净树即可）。
4. 重跑 02 → 05 → 09 → 11，核对 05 打印的 `first-stage 模块:` 行与 `vboot-` 配方号。
5. 起机核对：`grep -c cdh_redirect_vendor_path /proc/kallsyms` = 0（内核干净）、
   `/proc/cdh_redirect` 规则条数对、`readlink /proc/self/cwd` 在重定向目录下返 回虚拟路径；
   另确认 `dlkm_loader` 那一段里没有 `cdh_redirect`（命中即说明又被 08 注进了 vendor_dlkm）。
