# Issue #139: popsicle 的 `cust.img` 藏在 `userdata.img` 里，01 全程没抽出来

- 设备：`xiaomi/popsicle`（Xiaomi 17 Pro Max / canoe / lineage-23.2 / kernel 6.12.81）
- 阶段：`01_unpack`（清单 `rom.vendor.images` + `internal/f2fs`）→ `05_build_vendor_boot`（fstab）→
  `11_pack_vda`（附加分区）
- 状态：**已闭环**（2026-11-03，01→05→11 跑通，guest 内 `/cust` 已挂、34 个预装 APK 可见）
- 关联：[#136 issue-popsicle-external-modules](../issue-popsicle-external-modules/solution.md)
  （该条「cust 不可装」的结论被本条推翻，已在其遗留清单补注）、
  [#135 issue-vendor-selinux-labels-prune](../issue-vendor-selinux-labels-prune/solution.md)（同轮清理）

## 一、问题

popsicle 固件包**没有** `cust.img`：包内 0 个 cust 成员、`rawprogram*.xml` 的 163 个物理分区 label 无
cust、super 的 8 个逻辑分区无 cust、官方 `fstab.canoe` 无 `/cust` 行。上一轮据此判「popsicle 无
cust 分区，职能交给 `mi_ext` 的 overlay」（#136），于是基线 `rom.vendor.images` 只写
`["super.img","vendor_boot.img","userdata.img"]`、`vda.partitions` 一字不写。

但这个判断只看到了「独立分区」这一层：厂商把定制应用打成了
`userdata.img`（f2fs，45 GiB）里的 `/preinstall/cust.img`（**EROFS，2,717,908,992 B = 2.53 GiB，
34 个 APK**）。真机上它确实是独立分区、挂 `/cust`（HyperOS 的 `/cust/app`、`/cust/cust`、
`/cust/cust_variant` 都在那里），`/product/opcust` 等 overlay 只是 `mi_ext` 的补充，不是替代。

于是本轮的现象是：

1. `01_unpack` 抽完 45 GiB 的 `userdata.img` 就结束，`cust.img` 一个字节都没落地；
2. `11_pack_vda` 的 `vda.img` 只有 11 个分区（13.50 GiB），没有 cust；
3. guest 里 `mount | grep cust` 只有两条 overlay（`/product/opcust`、`/product/etc/precust_theme`），
   **`/cust` 本身不存在**，34 个预装 APK 无处可读。

## 二、根因

两层，必须一起修：

**（1）清单与流程缺「衍生」表达**：`rom.vendor.images` 只有「包里同名的成员抽出来」这一种语义，
`defaultImages` 还写死 `super.img` + `vendor_boot.img`；gold 之所以有 `/cust`，是因为它的
`cust.img` 是包内独立成员，靠清单点名即可。popsicle 这类「目标不在包里、在另一个分区镜像内部」
的形态没有任何入口，抽 `userdata.img` 的代码路径也从不解析它的文件系统。

**（2）`internal/f2fs` 的 `ADDRS_PER_INODE` 少减 inline xattr 槽数**（原始卡点）：
`cust.img` 的 inode `i_inline=0x21`（`INLINE_XATTR|EXTRA_ATTR`）、`i_extra_isize=8`，内核口径是

```
ADDRS_PER_INODE = DEF_ADDRS_PER_INODE(923) - i_extra_isize/4 - inline_xattr_addrs
```

本镜像 `features=0x1499` 没有 `F2FS_FEATURE_FLEXIBLE_INLINE_XATTR(0x40)`，所以
`inline_xattr_addrs` 走
`fs/f2fs/inode.c` 的第二条分支 → `DEFAULT_INLINE_XATTR_ADDRS = 50`。旧实现无条件读盘上
`i_inline_xattr_size`（偏移 362）、且算 `addrsPerInode` 时根本没减它，于是把 inode **尾部**的
inline xattr 区（以 `F2FS_XATTR_MAGIC` 开头）当成 `i_addr` 用：

```
块 4076150801 起 1 块只读到 0 B     # 4076150801 = 0xF2F52011 = F2FS_XATTR_MAGIC
```

此外 inline 区（inline data / inline dentry）的起点也被写成了
`addrBase() + inlineXattrAddrs*4`，而内核是
`inline_data_addr = &i_addr[extra_isize/4 + DEF_INLINE_RESERVED_SIZE]`
（即 `addrBase() + 4`）——xattr 在 `i_addr` 尾部，只通过缩短可用槽数让位，不移动起点。

## 三、证据

原始输出见同目录 [`log-fail.txt`](log-fail.txt)（改前三形态：01 只抽 userdata、guest `/cust` 空目录、f2fs 读出 `0xF2F52011` 当块号）与 [`log-pass.txt`](log-pass.txt)（01→05→11→guest 全链路的逐节回显与对账）。

**inline xattr 三条分支与槽数**（改前/改后用 `fsck.f2fs -M` 的块区间裁决）：

| 项 | 值 |
| :--- | :--- |
| 卷 `features` | `0x1499`（**无** 0x40 → 走固定 50 项分支） |
| `cust.img` inode | `i_inline=0x21`、`i_extra_isize=8`、`i_size=2717908992`、`i_blocks=664205` |
| 修复后 `ADDRS_PER_INODE` | 923 − 2 − **50** = **871** |
| `i_ext` | `{fofs 0xc00, blk 0x2df800, len 0x7ec00}`（命中率最高的加速段） |
| 全量块映射出的连续段 | `3008512-3009535`、`3010048-3012095`、`3012608-3531775`、`3532288-3673599`——与 `fsck.f2fs -M` **四段逐个相同** |

**抽取产物与独立参考产物逐字节一致**：`01` 落地的 `cust.img` 与上一轮在容器里用
`fsck.f2fs -M` 的块区间 + `dd` 手工拼出的 `out/tmp/ud/cust.img`（2,717,908,992 B）**sha256 相同**
（两边均为 `126dc9fad376902fdbf37e924b5732a5498caf002c2bcbf8c1c79efc2b18a756`，见 `log-pass.txt` 第 1.1 节）。

**装进盘集后仍是同一份字节**：`vda.img` 里 cust 分区（LBA 27586560-32894975）的 2.53 GiB
按 512 B 对齐重算 sha256 与 `cust.img` **完全相同**（第 4 节）；主/备 GPT 的分区项表逐字节相同，
backup header 的 `my_lba` = 盘尾 LBA 32897023、`entries_lba` = 32896991。

**fstab 与挂载**：`05` 的缓存判据（缺 vda.partitions 挂载行即过期）触发重算，`fstab.qcom` 末尾多出

```
/dev/block/by-name/cust /cust erofs ro wait,first_stage_mount,nofail
```

guest 内实测 `/dev/block/vda12 on /cust type erofs (ro,seclabel,relatime,user_xattr,acl,cache_strategy=readaround)`
（vda12 正是追加在盘尾的分区），`find /cust -name '*.apk' | wc -l` = **34**，
`cat /cust/cust_variant` = `cn`，`pm list packages` 里已能看到 `cn.wps.moffice_eng`、
`com.achievo.vipshop`、`com.taobao.taobao`、`com.zhihu.android` 等——说明系统侧的 cust
安装通道读的就是这个挂载点。

## 四、改动

1. **`internal/manifest`**：`rom.vendor.images` 支持 `"目标:来源"`（`ImageDerivation`）。
   `VendorImages()` 仍是**目标名**（01 的 keep 名单、`vda.partitions` 校验都按目标名走，gold 不用改），
   新增 `VendorImageSources()`（普通项 + 衍生来源 = 真正要抽的成员）与 `VendorImageDerivations()`。
   解析期拒绝：目标重复、自己抽自己、来源同时是普通保留项、多冒号、空目标、空来源。
2. **`internal/f2fs`**：
   - `inlineXattrAddrsOf`（照抄 `inode.c` 三条分支）、`addrsPerInode = 923 - extra_isize/4 - xattr`、
     `inlineBase = addrBase + 4`、`maxInlineData = 4*(addrsPerInode-1)`；
   - `findByName`（BFS，深度 4，唯一匹配）+ `ExtractByName`：01 不必写死 `/preinstall/cust.img`。
3. **`01_unpack`**：`imagePlanFromManifest` 同时返回「要抽的成员」与「衍生项」；`deriveImages`
   在来源抽完后按名剖出目标、`i_size` 对账通过再删来源；`keepPresent` 对「纯来源」改用
   `derivedLanded`（否则每次 01 都会重抽 45 GiB）；抽取器做成可注入字段，便于用桩覆盖删/不删逻辑。
4. **baseline**：`"userdata.img"` → `"cust.img:userdata.img"`，并新增
   `"vda": {"partitions": [{"image":"cust.img","mount":"/cust"}]}`。
5. **05 / 11 不改代码**：gold 的 `vda.partitions` 范式已具备（`lineVDA` 生成 by-name +
   `first_stage_mount,nofail`，`ensureExtraPartitions` 扩盘 + 重建 backup GPT + 回读校验）。

## 五、验证

| 项 | 结果 |
| :--- | :--- |
| `go test ./...`（engine/build） | 通过（新增 inline xattr 口径、`deriveImages` 六例、按名定位用例） |
| 01 清单回显 | `==> 清单保留镜像: super.img、vendor_boot.img、cust.img:userdata.img` |
| 01 衍生抽取 | `[ok] cust.img ← userdata.img:/preinstall/cust.img（2.53 GiB，i_size 对账一致）` + `[del] userdata.img` |
| 01 幂等 | 再跑一次 `[skip] 已解包（完成标记匹配，--force 可重解）`，不重抽 45 GiB |
| 01 产物 | `cust.img` 2,717,908,992 B；`userdata.img`（45 GiB）已释放；`cmp` 与 dd 参考产物一致 |
| 05 fstab | `缓存过期（缺 vda.partitions 挂载行 /cust）` → 重算后末尾为 `… /dev/block/by-name/cust /cust erofs ro wait,first_stage_mount,nofail` |
| 11 装配 | `[grow] 追加分区 cust 容量 2.53 GiB（LBA 27586560-32894975），backup GPT 已重建`、`[ok] cust 2.53 GiB @ 13.15 GiB … 回读一致`、`盘大小 13.50 GiB -> 15.69 GiB` |
| 盘集产物 | `vda.img` 15.69 GiB / **12 个分区**（原 11 + cust）；`vda-info.txt` 分区表含 cust 行 |
| 内容对账 | vda 内 cust 区段 sha256 == `cust.img` sha256（`126dc9fa…3cba`） |
| guest | `mount` 显示 `/dev/block/vda12 on /cust type erofs`；34 个 APK；`cust_variant=cn`；`pm list packages` 命中 **7 个** cust 应用（wps×2 / taobao×2 / zhihu / vipshop / com.miui.video） |

命令（可复现）：

```bash
cd /Volumes/work/cloud-phone-hub/engine
./out/bin/cdh device build popsicle --only unpack
./out/bin/cdh device build popsicle --only build_vendor_boot
./out/bin/cdh device build popsicle --only pack_vda
# guest：daemon 起在 9050，/api/create {"index":6}，再 out/tmp/chsh.sh '<命令>'
```

## 六、遗留

1. **`/data` 仍是独立 `data.qcow2`**（虚拟 110 GiB），本条只解决「cust 内容挂到 `/cust`」，
   不是把 `/data` 换成 userdata.img。
2. **APK 安装只抽检**：核了 4 个包名 + 总数 34，未逐个比对 `cust_apps_config` /
   `app_legacy_install_list` 的期望清单，也未验证业务属性（`business.prop`）是否生效。
3. **失败时故意保留来源**：抽取失败不删 `userdata.img`（45 GiB），磁盘紧张时要留意；
   缓存放不下时按 AGENTS 第 7 条改用 `CDH_TEMP_DIR`。
4. **`internal/f2fs` 只支持只读、f2fs 一种来源**：`deriveImages` 对非 f2fs 直接报错，
   厂商若改用 ext4/erofs 需另写实现；压缩簇（`COMPRESS_ADDR`）也直接报错不解压。
5. **真机镜像用例要重新解包才能跑**：`userdata.img` 抽完即删，`TestRealCust*` 在来源不在时自动
   `t.Skip`；要复核块映射需先 `--force` 重解一次 01。
6. **`01` 的阶段日志落在 ROM 输出根**（`out/roms/unpack-log.txt`），不在机型的 `img/` 目录，
   与 05/11 的落点不一致（既有行为，未改）。
