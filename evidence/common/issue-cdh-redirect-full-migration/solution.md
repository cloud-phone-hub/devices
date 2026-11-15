# 五台机型全面复测：cdh_redirect 外部模块接管路径重定向（并修掉 08 的重复注入与缓存失效）

- 设备: `google/oriole` AP2A.240905.003.F1（6.1.138-4k-cdh）、`xiaomi/gold`
  OS3.0.10.0.VNQCNXM（6.6.127-4k-cdh）、`flyme/m22` 12.5.0.0A、`oppo/cph2797`
  16.0.8.301_EX01、`samsung/ss9380` S9380ZCSDCZH1（后三台 6.12.81-4k-cdh）
- 宿主: macOS (darwin, QEMU + hvf)，daemon `127.0.0.1:9050`
- 状态: **已闭环**（2026-11-02，实机实例 oriole/24、gold/6、m22/49、cph2797/18、
  ss9380/8；五台 `sys.boot_completed=1`，重定向全部生效、残留 0）

---

## 1. 问题

需求原话：「cdh_redirect 继续做的已经完成的机型测试」。

此前 #107 只把 **oriole 一台**迁到了外部模块（`.ko` + 机型规则文件），末尾「遗留」第一条就是
其余机型的推广清单。#107 之后三棵内核树都已回退（`kernels/lineage-21-6.1@6aaf51263dc9`、
`lineage-22-6.6@bcd998d394e0f`、`lineage-23-6.12@5f9f2d9c2`，提交信息均为「CDH 重定向：
回退内核内置实现，改由外部模块 cdh_redirect.ko 承担」），13 份机型规则文件也已就位，
但只有 oriole 有「干净内核 + 模块生效」的起机证据。于是本轮要回答两件事：

1. 有 dist 产物的 5 台（= 「已经完成的机型」）是不是都能靠模块跑起来 —— 尤其是 6.6 与
   6.12 两棵树，`getname_flags` / `proc_pid_readlink` / `vfs_readlink` 三个符号与
   「第 2、3 个参数是 buffer/buflen」的假设在 #107 里只在 6.1 上验过；
2. 迁移到模块后有没有引入新的构建口径问题。

第 2 问在本轮给出两个「是」：08 阶段把 first-stage 模块重复塞进 `vendor_dlkm`，
以及 08 的配方对「注入集」不敏感导致改完代码仍命中旧缓存（构建不报错、静默沿用旧镜像）。
两者都在第 4 节修掉。

## 2. 根因

### 2.1 08 的注入集是「产物目录全量」，不区分加载时机

`08_build_vendor_dlkm` 的 `discoverModules()` 以 `out/kernel/<tag>/modules` 下**所有**
`.ko` 为注入集。`cdh_redirect.ko` 由 03 编进同一目录（baseline
`rom.lineage.kernel.modules: ["cdh_redirect"]`），于是必然被 08 收进去，再在 late-init
被 `dlkm_loader_mainline` 加载一次。第一次 stage 已经装过同名模块，这次必然 EEXIST：

- 日志上只有 `dlkm_loader_mainline: Loading module /vendor_dlkm/lib/modules/cdh_redirect.ko`，
  **没有**对应的 `Loaded kernel module …`（见 `log-fail.txt` ①）；
- 语义上无害（内核里那份是好的、钩子已就位），但把「模块来自哪里」搞反了：
  看到 `/vendor_dlkm/lib/modules/cdh_redirect.ko` 会以为改 `.ko` 只需重跑 08→11，
  实际生效的那份在 **vendor_ramdisk**（必须走 05）。这是一个只有踩过才知道的误导。

### 2.2 08 的配方只哈希整棵模块目录，对「选了哪几个」不敏感

`ComputeDlkmRecipe` 原输入是 `baseSHA + kbuild.HashTree(modulesDir) + props`。
`HashTree` 是目录树的哈希，而 `cdh_redirect.ko` 仍躺在产物目录里（只是不再**注入**），
目录树一个字都没变 → 配方号不变 → `[skip] vendor_dlkm.img 可复用（dlkm-7910035d54f4）`
→ 新加的 `firstStageModules` 排除逻辑第一轮完全没跑，`EXIT=0` 看起来一切正常
（见 `log-fail.txt` ②）。这是典型的「缓存键漏了影响输出的输入」。

### 2.3 三棵树的 kretprobe 都成立（#107 遗留 2 的答复）

- `fs/namei.c` 的 `getname_flags()` 在 6.1/6.6/6.12 都是非 static 的对外符号，
  `struct filename` 布局未变，`f->name` 就在返回对象里；
- `do_readlinkat()` → `vfs_readlink()` 与 `fs/proc/base.c` 的 `proc_pid_readlink()`
  在 6.12 仍然是两个编译单元；`(const char __user *buffer, int buflen)` 的参数位次也未变，
  证据是五台的 `readlink[vfs]: entries/rets` 都等于调用总数（6.12 三台分别 93598、
  102138、230747），说明符号存在、kretprobe 真的附上了（而不是静默退化）；
- 实测口径：五台全部 `reverse_hits > 0` 且 `reverse_missed = 0`，
  `readlink /proc/self/cwd` 在 `/vendor/etc`、`/vendor/lib64`、`/odm/etc` 三处都给出虚拟路径。

## 3. 证据

### 3.1 五台起机总表（全部为重建后新实例）

| 机型 | 内核 | 规则 | forward_hits | reverse_hits | missed | 残留物理路径 | dlkm 重复加载 | 实例 | `patch.img` |
| :-- | :-- | --: | --: | --: | --: | --: | --: | :-- | :-- |
| oriole | 6.1.138-4k-cdh | 10/32 | 10131 | 2730 | 0 | 0 | 0 行 | 24 | `cf4252b7b202` |
| gold | 6.6.127-4k-cdh | 10/32 | 11503 | 3220 | 0 | 0 | 0 行 | 6 | `c4b1a5c788d2` |
| m22 | 6.12.81-4k-cdh | 12/32 | 15763 | 4190 | 0 | 0 | 0 行 | 49 | `cf68ae8c0b6f` |
| cph2797 | 6.12.81-4k-cdh | 12/32 | 19543 | 4990 | 0 | 0 | 0 行 | 18 | `b9135408860a` |
| ss9380 | 6.12.81-4k-cdh | 12/32 | 13829 | 3596 | 0 | 0 | 0 行 | 8 | `f192d10dfbaf` |

「残留」= 遍历 `/proc/[0-9]*`，`readlink` 出来的 `exe` / `cwd` 里含 `vendorpatch` 的
进程数；五台都是 0，说明逆向改写对所有已存在进程都生效（不是只有新进程）。

### 3.2 宿主侧 first stage 装载（m22 实例 49，0.219 s）

```
[    0.219518][    T1] init: Loading module /lib/modules/6.12.81-4k-cdh/cdh_redirect.ko with args ''
[    0.224085][    T1] cdh_redirect: 规则来自 /lib/modules/cdh_redirect.rules：12 条
[    0.224878][    T1] cdh_redirect: 前向钩子就绪：getname_flags（maxactive 4096）
[    0.226001][    T1] cdh_redirect: 逆向钩子就绪：proc_pid_readlink + vfs_readlink
[    0.226296][    T1] cdh_redirect: 已加载：12 条规则（/lib/modules/cdh_redirect.rules），内核未打补丁
[    0.226750][    T1] init: Loaded kernel module /lib/modules/6.12.81-4k-cdh/cdh_redirect.ko
```

`grep -c 'cdh_redirect_vendor_path\|cdh_path_redirect' /proc/kallsyms` 五台均为 **0**
（内核确实没有旧补丁符号）；五台 `dlkm_loader.*cdh_redirect` 命中 **0 行** —— 这是本轮
08 修复后的判据，修复前是 1 行（`log-fail.txt` ①）。

### 3.3 前向 / 逆向行为（m22 为例，全文见 `log-pass-m22.txt`）

```
$ cd /vendor/etc && pwd -P; ls -l /proc/self/cwd | sed 's/.*-> //'; ls -l /proc/1/exe | sed 's/.*-> //'
/vendor/etc
/vendor/etc
/system/bin/init
$ for p in /proc/[0-9]*; do … case "$e$c" in *vendorpatch*) n=$((n+1));; esac; done; echo 残留=$n
残留=0
$ for p in $(pidof android.hardware.audio.service); do echo "pid $p -> $(readlink /proc/$p/exe)"; done
pid 830 -> /vendor/bin/hw/android.hardware.audio.service
```

其余四台同形（`log-pass-{oriole,gold,cph2797,ss9380}.txt`）：`/vendor` 与
`/vendor_dlkm/lib/modules` 读到载体内容、`/mnt/vendorpatch ext4 ro` 单挂载点、
`update_verifier` 屏蔽生效（`Cannot find '/system/bin/update_verifier'`）。
`readlink[vfs] hits=0` 是预期：#107 已说明 proc 层改写完成后 vfs 层读到的是虚拟串。

### 3.4 三个失败态

见同目录 `log-fail.txt`：① 08 重复注入（m22 实例 47/48 的 `dlkm_loader` 行）；
② 08 配方命中旧缓存（`dlkm-7910035d54f4`，注入清单已变仍 skip）；
③ 实例创建后 QEMU 秒退（media 端点与 `-chardev socket` 的启动竞态，与本议题无关）。

## 4. 改动

1. **`build/08_build_vendor_dlkm/modules.go`**：
   - 新增 `var firstStageModules = []string{"cdh_redirect.ko"}`（带注释说明它由 05 放进
     vendor_ramdisk 根级、必须在 SELinux 策略加载前就位），`discoverModules()` 里
     `if stringInSlice(firstStageModules, name) { continue }`；
   - 新增 `InjectDigest(modules []KernelModule) string`：按 `File:Size` 排序后 sha256
     取前 16 位，把「注入集」压成一个可进配方的字符串。
2. **`build/08_build_vendor_dlkm/plan.go`**：`ComputeDlkmRecipe` 增加 `injectDigest` 参数，
   payload 从 `dlkm-v1` 升到 `dlkm-v2` 并多一行 `inject:<digest>`；调用处传
   `InjectDigest(modules)`。
3. **`build/05_build_vendor_boot/README.md`** 步骤 2b 口径补一条：`cdh_redirect.ko`
   只由 05 注入，08 的 `firstStageModules` 已排除，并写明重复加载的现象（只有
   `Loading module` 没有 `Loaded kernel module`）。
4. **`modules/cdh_redirect/README.md`**：落地机型改为五台已复测（含每台规则条数）+
   未复测机型清单；规则样例 m22 由「11 条」更正为「12 条」；已知坑 4 补 08 不再注入
   与 `dlkm-v2` 配方；迁移步骤 5 补「确认 `dlkm_loader` 段里没有 cdh_redirect」。

## 5. 验证

构建：五台统一 `cdh device build <设备> --from build_vendor_dlkm`（EXIT=0）。修好后
注入集 6.12 三台 7→6 个、6.1/6.6 两台 8→7 个（产物目录里 `cdh_redirect.ko` 仍在，
不再进 vendor_dlkm），配方号随之首次变化：

| 机型 | 08 配方 | 注入 .ko | 合计字节 | `vendor_boot.img` |
| :-- | :-- | --: | --: | :-- |
| oriole | `dlkm-21a806d630df` | 7 | 4241720 | `ca17f170ffd1` |
| gold | `dlkm-51684bc87961` | 7 | 3830768 | `ae64d5803763` |
| m22 | `dlkm-6065dd9209aa` | 6 | 4442536 | `62f8c73bc56c` |
| cph2797 | `dlkm-a994059f1033` | 6 | 4442536 | `e60af4166e2e` |
| ss9380 | `dlkm-49c9d6044b45` | 6 | 4442536 | `68d075a49b1e` |

`.ko` 指纹（同一个源码三棵树各编一份）：6.1 `3ca9267f7294`（41560 B）、
6.6 `ca5257ea02a7`（39736 B）、6.12 `6f5778349987`（42808 B）。

`go build` + `go test ./08_build_vendor_dlkm/` 通过；`patch.img` 因 ext4 元数据每次
重建都会变（402653184 B，sha 见 3.1 表），因此五台全部重新起机取证，不复用旧实例结论。

起机（第 3 节）：五台 `sys.boot_completed=1`，规则条数与机型规则文件一致、
`forward_missed` / `reverse_missed` 全 0、残留物理路径 0、`dlkm_loader` 重复 0 行。

## 6. 遗留

- **13 份机型规则里仍有 8 台没起机**：`nothing/3a`（10 条）、`oneplus/CPH2769`（12）、
  `oneplus/PJE110`（10）、`realme/rmx3823`、`rmx5112`、`rmx6688`（各 10）、
  `xiaomi/popsicle`（12），外加 `oppo/CPH2797` 那份重复目录。这些机型自身 bring-up 未完成，
  规则只是「先备好」，等它们有 dist 产物时按本轮同法复测；`oppo/PME110` 则连基线都还没有。
- **仓库里有一份大小写重复的目录**：`devices/manifest/oppo/CPH2797` 与
  `devices/manifest/oppo/cph2797` 都在，前者是早期半份（无 `device.json` / `sepolicy` /
  `vendor`，基线缺 vintf-drop）。macOS 默认大小写不敏感，两者落盘会打架；本轮只用小写
  那份，重复目录待清理。
- **daemon media 端点竞态**：`StartMediaEndpoint` 与 QEMU `-chardev socket,…media-decoder.sock`
  存在启动顺序问题，偶发 `Connection refused` 导致实例秒退（`log-fail.txt` ③）。重试即好，
  但会让「创建后等 boot_completed」的自动化误判成镜像问题，建议 daemon 侧改成先起端点再
  拉 QEMU（或在 chardev 上改 `reconnect-ms`）。
- kretprobe 仍依赖符号名与参数位次，属于「已验证但无编译期保障」：将来换 6.13+ 内核时
  必须先跑一遍本轮的 9 项取证（`readlink` 三处 + 残留计数 + `kallsyms` 计数）。
- `cdh_debug=1` 会逐次打印（一条命令 ~900 行），只在取证时开；取证脚本
  （`.tmp/verify_cdh_redirect.py` / `.tmp/run-verify.py`）目前留在 `.tmp`，若这套复测要
  常态化，应挪进 `engine/` 并纳入 `cdh` 子命令。
