# Issue: gold 云机缺 15 个 ROM 预装应用 —— 盘集里没有 `cust` 分区

- 设备: Xiaomi Redmi Note 13 5G（gold，`OS3.0.10.0.VNQCNXM`，MIUI `V816`）
- 宿主: macOS (darwin, QEMU + hvf)
- 状态: **已修复并在云机上复验通过**（2026-11-02）。**是两个构建缺口叠加**：厂商固件带 `cust.img`
  （1.35 GiB，EROFS，内含 15 个 partner 预装 apk），但盘集 `vda.img` 的 GPT 里
  **没有 `cust` 分区**，`/cust` 是空目录，原厂 `PreInstallApp` 无从取包；把分区补上之后
  又发现**就算分区在，`/cust` 也不会自己挂**（见 2 节第二段），两条一起修才闭环。

  修复口径：**清单驱动，不写死**——`rom.vendor.images` 声明「01 要保留哪些镜像」，
  顶层 `vda.partitions` 声明「哪些镜像打进 vda」。`01_unpack` 抽出并转 raw，
  `11_pack_vda` 扩盘 + 追加 GPT 条目 + 重建 backup GPT 后写入。
  真实产物：`out/dist/xiaomi/gold/OS3.0.10.0.VNQCNXM/vda.img` 13.00 → 14.00 GiB，
  分区 11 → 12（`cust` LBA 26537984-29364223）；`05_build_vendor_boot` 再按清单
  `mount` 写一行 first_stage fstab，新实例首启后 `/cust` 即为 erofs（ro），
  17 个 partner 包全部安装。
- 触发背景: 真机（Redmi `2312DRAABC`）上淘宝/快手/美团/支付宝/高德/夸克/微博/今日头条/
  番茄/闲鱼/腾讯视频/抖音/小米小游戏等一应俱全，云机同机型固件一个都没有。
- **勘误**: 本条第一版结论写成「固件里本来就没有、是小米应用商店联网装的」——错。
  当时只查了 super 内的 8 个逻辑分区和 `images/userdata.img`，**漏了 `cust.img` 这个
  1.35 GiB 的独立分区**（`01_unpack` 只解 `super.img` 与 `vendor_boot.img`，从来没解过它）。

---

## 1. 问题现象

云机里 MIUI 自身应用齐全（`com.miui.home` / `com.miui.securitymanager` / `com.xiaomi.market`），
但第三方 partner 预装一个都没有：

```
$ adb -s 127.0.0.1:5911 shell 'pm list packages | grep -iE "taobao|gifmaker|meituan|AlipayGphone|autonavi|quark|weibo|qqlive|aweme"'
（空）
$ adb -s 127.0.0.1:5911 shell 'su -c "ls /cust"'
（空）
$ adb -s 127.0.0.1:5911 shell 'su -c "ls /dev/block/by-name/"'
BIOS_a BIOS_b EFI EFI_a EFI_b boot_a boot_b firmware grub_boot_a grub_boot_b metadata misc persist super vdc
                                                                    ↑ 没有 cust
```

## 2. 根因

**厂商固件的 `cust.img`（1.35 GiB）是这批应用的唯一载体，而它从未进入盘集。**

厂商 tgz 的 `images/` 里有 5 个「非 super」镜像，我们的流程只取了 `super.img` 和
`vendor_boot.img`：

| 镜像 | 大小 | 我们是否取用 |
| --- | --- | --- |
| `super.img` | 8.50 GiB | ✅ 10_pack_super（`mode: copy`） |
| `vendor_boot.img` | 64 MiB | ✅ 05_build_vendor_boot |
| **`cust.img`** | **1.35 GiB（sparse，raw 1.44 GiB，EROFS）** | ❌ **从未解包、从未写入** |
| `userdata.img` | 2.4 MiB | ❌（空 data，无关） |
| `rescue.img` / `pi_img.img` | 41 KB / 5.7 KB | ❌（无关） |

盘集 `vda.img` 由 lineage 的 `vda.qcow2` 底盘原位替换而来（`vda-info.txt`：
`GPT 未动，只写数据区`），GPT 只有 11 个分区（EFI / super / misc / persist / metadata /
firmware / vendor_boot_a|b / boot_a|b / BIOS），**没有 `cust`**。原厂 init 按
`by-name/cust` 挂载 `/cust`，设备节点不存在 → 挂载静默失败 → `/cust` 是空目录 →
`PreInstallApp` 扫不到 `app/customized/*.apk` → 15 个应用全部缺失。

**第二层缺口：就算分区补上，`/cust` 也不会自己挂上。**

- 厂商 first_stage fstab（`vendor_boot` 的 `first_stage_ramdisk/fstab.mt6833`，161 行）
  里**没有 `cust`**——真机 `/cust` 是 MIUI 自己的 vendor 逻辑挂的（`ro.miui.cust_erofs=1`
  那套），云机的 vendor 是 lineage 派生，那段逻辑不存在；
- `/dev/block/by-name/cust` 也不在任何 init 的静态 `symlink` 表里（`init.mt6833.rc` 只有
  vda1..vda10 的 EFI/misc/metadata/…，`init.virt.rc` 只建 BIOS/EFI 双槽）；内核只提供
  SoC 展开路径 `/dev/block/pci/pci0000:00/0000:00:04.0/by-name/cust`，
  `/dev/block/by-name/cust` 不存在。

实测（分区已加、fstab 未加时的旧实例）：`/proc/mounts` 无 `cust`，
`ro.boottime.init.mount.cust` 不存在，`dmesg [0.528681] init_hook_E: Could not find
/cust/cust_variant and /data/miui/cust_variant`——MIUI 内核 **0.52 s** 就去读 `/cust`，
晚挂等于没挂。所以 `/cust` 必须在 **first stage** 与 super 同批挂好。

## 3. 证据

### 3.1 `cust.img` 里就是这批应用

```
cust.img（Android sparse v1.0，27 chunks，raw 1447034880 B）→ EROFS
/                       app  app_legacy_install_list  cust  cust_variant  etc
/app/customized/        17 个 partner-<包名>_<ver> 目录 + 17 个同名 .channel
/cust/cn/customized_applist      17 行（安装名单）
/cust/cn/customized_channellist  渠道名单
/app_legacy_install_list          com.cmcc.cmvideo
```

`/app/customized/` 实际携带的 17 个（每个目录内是 `<name>.apk`）：

```
partner-com.autonavi.minimap_12        高德地图
partner-com.baidu.searchbox_9          百度
partner-com.dragon.read_104            番茄免费小说
partner-com.eg.android.AlipayGphone_23 支付宝
partner-com.phoenix.read_302           番茄小说
partner-com.quark.browser_271          夸克浏览器
partner-com.sankuai.meituan_21         美团
partner-com.sina.weibo_16              微博
partner-com.smile.gifmaker_13          快手极速版
partner-com.ss.android.article.news_76 今日头条
partner-com.ss.android.ugc.aweme_15    抖音
partner-com.taobao.idlefish_274        闲鱼
partner-com.taobao.taobao_24           淘宝
partner-com.tencent.qqlive_20          腾讯视频
partner-com.xiaomi.tinygame_243        小米小游戏
partner-com.xs.fm_270                  喜马拉雅（早期取证漏记，云机实测有这个目录）
partner-com.xunmeng.pinduoduo_19       拼多多（同上）
```

**勘误（2026-11-02 云机实测）**：早期在此处写「15 个目录」，是因为只看了前半段
目录列表；实盘 `/cust/app/customized` 是 **34 项 = 17 目录 + 17 .channel**，与
`customized_applist` 的 17 行一一对应，**不需要首启联网补装**。

### 3.2 真机：`cust` 分区在、`/cust` 已挂、15 个包全部安装

```
$ adb -s 4lmbvom7gifm5lbu shell 'ls -l /dev/block/by-name/cust'
lrwxrwxrwx 1 root root 16 2026-10-29 05:09 cust -> /dev/block/sdc56
$ adb -s 4lmbvom7gifm5lbu shell 'mount | grep cust'
/dev/block/sdc56 on /cust type erofs (ro,seclabel,relatime,user_xattr,acl,cache_strategy=readaround)
$ adb -s 4lmbvom7gifm5lbu shell 'ls /cust/app/customized | head -4'
partner-com.autonavi.minimap_12
partner-com.autonavi.minimap_12.channel
partner-com.baidu.searchbox_9
partner-com.baidu.searchbox_9.channel
$ adb -s 4lmbvom7gifm5lbu shell 'pm list packages | grep -iE "taobao|gifmaker|meituan|AlipayGphone|autonavi|quark|weibo|qqlive|aweme"'
package:com.sina.weibo
package:com.eg.android.AlipayGphone
package:com.xiaomi.tinygame
package:com.taobao.idlefish
package:com.taobao.taobao
package:com.quark.browser
package:com.sankuai.meituan
package:com.phoenix.read
package:com.autonavi.minimap
package:com.tencent.qqlive
package:com.dragon.read
package:com.smile.gifmaker
package:com.ss.android.article.news
```

真机上这些包的 `installerPackageName=com.xiaomi.market`、`codePath=/data/app/~~xxx`：
**预装流程走的就是小米应用商店的 installer 通道**（`PreInstallApp` 调
PackageInstaller 时装源标成 market），所以「安装来源是商店」不能推出「是用户下载的」。
第一版就是被这个字段带偏的。

### 3.3 盘集 GPT 现状：11 个分区，128 个槽位只用 11 个

```
slot  0  EFI            LBA       2048
slot  1  super          LBA     264192    12417 MiB
slot  2  misc           LBA   25430016
slot  3  persist        LBA   25432064
slot  4  metadata       LBA   25464832
slot  5  firmware       LBA   25530368
slot  6  vendor_boot_a  LBA   25792512
slot  7  vendor_boot_b  LBA   25997312
slot  8  boot_a         LBA   26202112
slot  9  boot_b         LBA   26365952
slot 10  BIOS           LBA   26529792
usable LBA 34 - 27262942（13.00 GiB），最后分区之后仅余 ~357 MiB
```

槽位充足（128），但**尾部空间不足**：`cust.img` raw 需 1.38 GiB，现有空闲仅 ~357 MiB
→ 必须先扩盘。

## 4. 改动

方案：**清单驱动 + 扩 `vda.img` 新增 GPT 分区 `cust`**。动手前先否掉了「在代码里
写死 cust.img」——写死名单正是它被漏掉的原因（`imageTarget` 只认 super/vendor_boot），
再写死一次只把缺陷换个地方埋。

### 4.1 清单：两个字段分工

`devices/manifest/xiaomi/gold/baselines/gold_images_OS3.0.10.0.VNQCNXM_20260715.0000.00_15.0_cn_a9729c6694.json`：

```json
"rom": { "vendor": { "images": ["super.img", "vendor_boot.img", "cust.img"] } },
...,
"kernel": "6.6",
"modules": { "external": [...], "in-tree": ["cdh_virt_wifi.ko"] },
"vda": { "partitions": [ { "image": "cust.img", "mount": "/cust" } ] }
```

- `rom.vendor.images` = **01 要保留哪些镜像**（可多个，如多个 cust）；为空/未声明时
  走默认集 `super + vendor_boot`（m22/oriole 的 `"images": []` 是占位，不能理解成
  “除 super 外都不要”，否则 vendor_boot 不再抽取、05 直接断料）；
- 顶层 `vda.partitions` = **哪些镜像打进 vda、各自挂到哪**，与 `kernel` / `modules` 平级
  （这是构建意图，不是从镜像里量出来的厂商事实）。每项是**对象**：
  - `image`（必填）写**镜像文件名**，分区名 = 去 `.img` 后缀（`cust.img` → `cust`），
    真机本来就是一套名字，不另开映射；
  - `mount`（可省）缺省 `/<分区名>`（`cust` → `/cust`），05 拿它写 fstab；
  - `fstype`（可省）缺省 `erofs`（真机 `ro.miui.cust_erofs=1`）。

  第一版曾写成字符串数组 `["cust.img"]`：挂载点只能靠调用方猜默认值，而“猜”正是
  本 issue 的病根，故改为对象并在解析期拒绝裸字符串。

解析期四条闸门（`internal/manifest`）：

1. 每项必须是对象 `{image, mount?, fstype?}`，裸字符串直接报错；
2. `image` 必须形如 `*.img`（会直接变成 GPT 分区名，只允许字母数字与 `_.-`）；
3. 必须在 `rom.vendor.images` 里——否则 01 不会抽它，11 必然取不到料；
4. 不得是 `super.img` / `boot.img` / `vendor_boot.img`——它们已有步骤 3/4 原位替换，
   写成附加分区会把同一块盘写两遍（实测确认过这个形态会在 info 里列两次）；
   `mount` 还必须是已规范化的非根绝对路径（会直接进 fstab 与 init 的 mkdir）。

### 4.2 `01_unpack`：保留名单改成清单驱动

- `defaultImages`（super + vendor_boot）+ `setKeeps/keepList/keepTarget/satisfied`：
  `super.img` 恒认（漏写不能让它静默消失），其余只认清单；
- `imageTarget` 从函数改成方法，优先级 super > 清单保留 > `extraImages`（boot/init_boot/
  dtbo/vbmeta 仍落 `firmware/` 留档）；清单显式保留的名字即使碰巧在 `extraImages` 里
  也以清单为准（`toFirmware`）；
- 搬运与记账不再点名：`moveStagedImages` 把 staging 根下所有 `.img` 搬到解包根，
  `writeMarker` 把解包根/`super/`/`firmware/` 下的 `.img` 全量记入完成标记；
- sparse → raw **复用已有的 `image.unsparse`**（super 一直在用），对所有保留镜像生效：
  gold 的 `cust.img` 是 1.35 GiB sparse，不展开就写进分区，云机挂载只会看到 sparse 魔数；
- `keepsFromManifest` 放在 `RunVendor` 内部读 `CDH_MANIFEST`——编排器与 `unpack-vendor`
  单跑共用同一条路（单跑漏读就退回默认集，cust 又静默丢了）；
- 缺件分级：`super.img` 缺 → 硬失败；**清单声明过的镜像缺 → 也硬失败**（那是操作者的
  明确声明，漏抽正是本 issue 的病根，不能降级成告警）；只有未声明（走默认集）时
  `vendor_boot.img` 缺仍按既有口径告警；
- 完成标记记 `keeps`：标记原来只比包路径/大小/类型，对「同一个包里还要多抽一个镜像」
  一无所知。gold 实跑就撞上了——清单加了 `cust.img`，01 却打印「已解包」直接跳过，
  cust 永远不落地。现在标记里有 `keeps`，少一项即判未解包；
- 已解包但清单新增镜像时走 **补抽**（`extractKeepsInto` + `only` 收窄抽取范围），
  只读到自己那几个成员为止，不再重写 8.5 GiB 的 `super.img`（gold 实测 `super.img`
  mtime 保持 10-22，没被碰过）。

### 4.3 `11_pack_vda`：步骤 5 装配附加分区

阶段从 6 步变 7 步（编号与 04/05/07/08/09/10 口径一致）：

```
步骤 3  原位替换 super
步骤 4  原位替换 boot_a/_b、vendor_boot_a/_b
步骤 5  装配附加分区（清单 vda.partitions：扩盘 + 追加 GPT 条目 + 写镜像）   ← 新增
步骤 6  EFI grub.cfg 固化
步骤 7  发布 patch.img / data.qcow2 / device.json
```

- `resolveVDAExtras`：取料先看 `--img` 目录（10 的产物）、再回退
  `out/roms/<vendor>/<brand>/<版本>`（01 的解包根）。缺件直接报错——不静默降级成
  “少一个分区”；只有 `--dry-run` 宽容（镜像未就位也把计划打出来）；
- `gpt.go` 新增 `ensurePartition(name, needBytes)`：已有同名分区则只校容量（装不下报错，
  不自动挪动别的分区）；没有则找空槽、起始 LBA 按 1 MiB 对齐、盘长不够就 `Truncate`
  扩到 `last + 1 + 表扇区 + 1`，再重算主/备两处头与表 CRC；
- **没有写第二套 GPT 封印逻辑**：将 `grow.go` 原有的 CRC + backup 重建抽成
  `writeGPTBack`，`growPartition`（放大已有分区）与 `ensurePartition`（追加新条目）共用；
- 新条目 type GUID 抄底盘 `super` 的（名字才是挂载依据），唯一 GUID 用
  `crypto/rand` 生成 v4；
- `ensureExtraPartitions` 每项：读表 → 追加/复用 → **重读表**（盘长变了，offset 必须取新值）
  → `replace` 按 FirstLBA 写数据区 → `verifyRegion` 回读 4 KiB 比对；
- `recipeLabel` 加入 `vda-part:<分区>:<大小:mtime>`：换了批预装应用的 cust.img 同名同路径，
  不进配方就会被当成“盘集可复用”整段跳过；
- 载入期摘要、`--dry-run` 计划行、`vda-info.txt`（“新增 N 个分区条目，主/备 GPT 已重算”）、
  阶段日志（`[grow] 追加分区 cust 容量 …，backup GPT 已重建`）全部按清单动态生成。

### 4.4 `05_build_vendor_boot`：把挂载点写进 first_stage fstab

清单对象里的 `mount` 由 05 落成一行，追加在 patch.img 补丁之后（与 vdc 同级）：

```
# vda.partitions补丁（清单声明的独立分区，first_stage 与 super 同批挂载）
/dev/block/by-name/cust /cust erofs ro wait,first_stage_mount,nofail
```

- **源用 by-name**：分区是 11 追加进 vda 的，vdaN 随机型变化，写死跨机即错。
  实测 first_stage 的 fs_mgr 会按 fstab 建出 `/dev/block/by-name/cust -> vda12`；
  `by-name/vdc`、`by-name/super` 同源，都是 fstab 引用后出现的，不是 init 静态表建的；
- `first_stage_mount`：与 super 同批，早于 zygote 与 kernel `init_hook` 读
  `/cust/cust_variant`（实测内核 0.59 s 已能读到 `ro.miui.cust_variant=cn`）；
- `nofail`：分区缺失时只丢这个挂载点，不把整机拖进 `InitFatalReboot`；
- 合并版本 `merge-v8 → merge-v9`；缓存除版本标记外还**逐条核对** `vda.partitions`
  行，缺一条即重算——旧缓存不能当“已经加了 cust”。

一度尝试过另一套（`/vendor/bin/cph-vda-extra` + `cph_vda_extra.rc` 在 `post-fs-data`
扫 `PARTNAME` 建链接再挂载），在本方案落地后已**全部撤销**：晚于 `init_hook`，且多一个
服务/脚本要维护；`/cust` 本该就是 fstab 的一行。

被否决的备选：

- **塞进 `patch.img`（vdc）**：vdc 已固定挂 `/mnt/vendorpatch`（384 MiB），容量与挂载点都不对；
- **合并进 super**：`cust` 必须是独立 GPT 分区（init 按分区名挂载），super 里没有这个名字；
  且 gold 的 super 是 `mode: copy`（官方 8.5 GiB 逐字节复制），加逻辑分区要 `--rebuild`，
  等于推翻现有口径；
- **加到 vdd**：需要改 daemon 的 qemu 参数，且 fstab 里 `/data` 已占 vdb，不如直接扩 vda 干净；
- **预装流程自己 cp 到 /data/app**：绕开 `cust` 语义，每次新实例都要重跑，且拿不到
  `.channel` 渠道信息与 `/cust/etc/cust_apps_config` 配置；
- **在 `01_unpack` 里写死 `cust.img`**：把缺陷从 11 挪到 01，下一个机型再踩一遍。

## 5. 验证

### 5.1 本地已验（2026-11-02）

1. 清单：gold baseline 解析通过；`vda.partitions` 三条闸门各有反例用例
   （不在 images 里 / 缺 `.img` 后缀 / 写了 `super.img`）；
2. 01：合成 tgz（`images/cust.img` + `super.img`）按清单抽出 cust 到解包根、不落
   `firmware/`；sparse cust 经 `unsparseKeeps` 转成 raw；
3. 11：合成底盘（6 分区）追加 cust 后变 7 分区，起始 LBA 1 MiB 对齐、盘长变长、
   主/备头与表 CRC 自洽、cust 数据区回读与源镜像逐字节一致、super 未被破坏、底盘文件
   未被改动；缺源镜像报错、`--dry-run` 不报错且不产物；cust.img 变化触发“配方变化”重建；
4. `go build ./... && go vet ./... && gofmt -l .` 干净，`go test ./...` 全绿
   （01/02/03/04/05/06/07/08/09/10/11 + 6 个 internal 包）；
5. `pack-vda --dry-run` 对真实 gold 清单输出
   `==> 附加分区: cust <- out/roms/xiaomi/gold/15.0/cust.img（dry-run，源镜像未就位）`。
6. **真实链路跑通**（gold，`cdh device build gold --only unpack` → `--only pack_vda`）：
   - 01 命中「补抽」分支：`[..] super.img 已就绪，补抽清单保留镜像 cust.img`，
     得 `cust.img 1447034880 B`（raw，sparse 已展开），`super.img` 未被重写；
   - 11 步骤 5：`[grow] 追加分区 cust 容量 1.35 GiB（LBA 26537984-29364223），
     backup GPT 已重建`、`盘大小 13.00 GiB -> 14.00 GiB`、回读一致；
   - 独立脚本复核（`verify-vda-gpt.py`）：12 个分区、主头/备头 CRC ok、`alternate`
     指向末扇区、备份表与主表字节一致、`lastUsable = 末扇区-34`；cust 数据区
     sha256 `14f88c88…c11845` 与源镜像**逐字节一致**；
   - 二次运行 `pack_vda`：`[skip] 盘集可复用`（配方 `vda-10e847766a28` 未变），
     说明扩盘与追加分区是幂等的，不会越跑越长。

### 5.2 云机复验（2026-11-02，实例 5，fresh data）

链路：`--only build_vendor_boot`（fstab merge-v9）→ `--only pack_vda`
（vendor_boot_a/_b 刷新，cust 分区复用）→ `POST /api/create {"index":4}`。

```
$ adb -s 127.0.0.1:5911 shell 'ls -l /dev/block/by-name/cust'
lrwxrwxrwx 1 root root 16 1970-01-01 08:00 /dev/block/by-name/cust -> /dev/block/vda12

$ adb -s 127.0.0.1:5911 shell 'grep cust /proc/mounts'
/dev/block/vda12 /cust erofs ro,seclabel,relatime,user_xattr,acl,cache_strategy=readaround 0 0

$ adb -s 127.0.0.1:5911 shell 'ls /cust/app/customized | wc -l'
34            # 17 个 partner 目录 + 17 个 .channel
$ adb -s 127.0.0.1:5911 shell 'wc -l /cust/cust/cn/customized_applist'
17

$ adb -s 127.0.0.1:5911 shell 'pm list packages | grep -E "autonavi|searchbox…"'
17 行（逐项）：xs.fm / sina.weibo / xunmeng.pinduoduo / eg.android.AlipayGphone /
xiaomi.tinygame / taobao.idlefish / taobao.taobao / quark.browser / sankuai.meituan /
baidu.searchbox / phoenix.read / autonavi.minimap / tencent.qqlive / ss.android.ugc.aweme /
dragon.read / smile.gifmaker / ss.android.article.news
# 注意 grep 会多命中一个 com.android.quicksearchbox（系统包，不在这批）

$ dmesg | grep -i cust_variant
[ 0.593196] init_hook_I: cust prop white key: ro.miui.cust_variant
[ 0.597039] init_hook_I: cust prop white key: ro.vendor.miui.cust_variant
# 不再有旧实例的 “init_hook_E: Could not find /cust/cust_variant”
$ getprop ro.miui.cust_variant   → cn
$ getprop sys.boot_completed     → 1
```

与旧实例（分区在、fstab 无 cust）对比：旧实例 `/proc/mounts` 无 cust、
`dmesg [0.528681] init_hook_E` 报找不到；新实例同一时刻已在读 variant 属性。

## 6. 遗留

1. **预装包数按实盘算 17 个**（含拼多多与 `xs.fm`）：早期写的“15 个，另 2 个靠联网补装”
   已由 2026-11-02 云机实测证伪（见 3.1 勘误），此条仅保留作为取数口径提醒：
   判定“全在”要看 `customized_applist` 行数与 `pm list packages` 同时对齐；
2. **`cust_variant` 为空目录**，`/cust/cust/cn/...` 的渠道差异化配置（`.channel` 内的
   二进制内容）未解析，暂按原样整盘写入；
3. **扩盘会改变 `vda.img` 的 GPT 布局**：daemon 侧不改（仍按整盘偏移读），grub 固化
   由 `readGPT` 现算 EFI 偏移（不写死），但顺序上已改成「先追加分区、再固化 grub」，
   扩盘后必须整链重启验证引导（grub 菜单、`_a` 槽固定、vbmeta 注入）；
4. 其他机型（ss9380 三星、cph2797 OPPO、m22 flyme）是否也有类 cust 分区未取用，
   需要同样排查一遍——这条是**通用缺口**，不是 gold 独有。现在修好了也不用改代码：
   `rom.vendor.images` 加上镜像名、`vda.partitions` 声明 `{image, mount}` 即可；
   若该机型 cust 是 ext4 而非 erofs，在对象里补 `"fstype": "ext4"`（默认 erofs）；
5. **`cust.img` 容量（raw 1.45 GiB）会抬高 `vda.img` 体积**：当前按源镜像大小 + 1 MiB
   对齐取值，gold 实跑后分区容量 1447034880 B **与镜像等身（余量 0 B）**，盘从 13.00 GiB
   涨到 14.00 GiB；后续 cust 若比现在大 1 字节，`ensurePartition` 会直接报错（不静默截断），
   需要 `--refresh` 重建才能扩容；
6. **`first_stage_mount` 靠 fs_mgr 按 fstab 建 by-name 链接**（实测 `by-name/cust`
   就是它建的），未在 `init.<board>.rc` 里加静态 symlink；若某机型 first_stage 的
   fs_mgr 行为不同，备选是在 rc 里补 `symlink`（需知道 vdaN，或改用 `PARTUUID=`）。
