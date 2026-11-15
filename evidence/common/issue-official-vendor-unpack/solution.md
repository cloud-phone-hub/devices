# Issue: 07 从厂商 vendor 分区取的东西各解各的临时目录、用完即删，没有留档

- 设备：机制改动落在 `07_build_vendor`（gold 实跑验证，其余机型下次 07 自动生效）
- 阶段：`07_build_vendor` 步骤 1b（新增）/ 步骤 5 / 步骤 6 / 步骤 7
- 状态：**已闭环**（2026-11-03，gold `--only 07_build_vendor` 实跑，首次解包 + 复用两次）
- 关联：[#139 issue-vendor-buildprop-official-source](../issue-vendor-buildprop-official-source/solution.md)
  （build.prop 基底改从厂商镜像现解；本次把"现解"升级成"整树解一次、留档复用"）

## 一、问题

07 要从厂商 `super/vendor_a.img` 拿三类东西，每类都各自解一份临时目录，用完即删：

| 用途 | 改造前实现 | 落点 |
| --- | --- | --- |
| 步骤 5 `vendor.files` 的 `official` 条目 | `files.go` `extractSource` → `extractImageErofs/Ext4` 逐条解 | `os.MkdirTemp("", "cdh-vendor-file-*")`，`defer cleanup` 删 |
| 步骤 6 厂商 `build.prop` | `props.go` `placeVendorBuildProp` 解一份再 copy | 同上，解完 copy 到 `unpack_vendor/` 与装配树 |
| 步骤 7 SELinux 标签真值 | `setxattr.go` `extractVendorReference` **每次**整树解 | `os.MkdirTemp("", "cdh-vendor-ref-*")`，读完标签就删 |

后果：

1. **没有留档**——想核对"装配树里这个文件是不是厂商原件"，只能重跑一遍 07 或手工
   `dump.erofs --cat`/`fsck.erofs --extract` 到别处；`super/unpack_vendor/` 里当时只有
   一个 `build.prop`。
2. **重复 IO**——同一份 1.3 GB erofs 镜像，一次构建至少解三遍（gold 标签真值那遍
   实测 2.0 GB / 6482 条目 / 5 秒，解完立刻删）。
3. **取件与标签可能读到不同次解包**——逐条解出来的目录树与标签真值那份临时树，
   在镜像被换掉（01 重跑）时互相之间没有一致性约束。

## 二、根因

07 没有"厂商 vendor 唯一解包树"这个概念：取件按需现解，每个消费者自己管临时目录的
生命周期，于是"解包"既是手段也是垃圾。lineage 基底那边早就有固定落点
（`<厂商解包根>/super/unpack_vendor`，步骤 1），厂商侧却没有对应物——两边的解包口径
不一致，才出现"同一个镜像构建内解三遍、构建结束什么都不剩"。

顺带暴露的第二个缺陷：`readSelinuxLabels` 用 `CombinedOutput()` 读 `xattr -lr`，
悬空符号链接（`lib/modules` → `/vendor_dlkm/lib/modules`、`lib/vndk/libstagefright_omx.so`
→ `/apex/com.android.vndk.v31/...`）会让 `xattr` 往 stderr 写 `xattr: No such file`，
两个流一交错，紧邻的正常行被拼接进错误信息里解析失败——gold 实测丢 2 条标签真值
（`lib/libz.so`、`lib/vendor.mediatek.hardware.mtkpower-V2-ndk.so`），且丢哪两条取决于
遍历顺序，静默且不可复现。

## 三、证据

1. **改造前日志**（`log-fail.txt` 第 1 节）：步骤 6 打「厂商 build.prop 解包 -> 装配树 /
   （步骤 6 现解）」，步骤 7 打「解包官方 vendor 镜像（取 SELinux 标签真值）」，
   标签真值 6481 条。
2. **临时目录痕迹**（`log-fail.txt` 第 2 节）：构建期间 `out/tmp/` 下出现
   `cdh-vendor-ref-3498153936`（整树解包），构建结束即消失；`out/tmp` 里
   `cdh-vendor-file-*` 同理。
3. **丢标签实测**（`log-fail.txt` 第 3 节）：改造后（复用步骤 1b 的树）标签真值变成
   6480 条，用探针复刻解析定位到 `badRel=1`、丢的正是 `lib/libz.so` 与
   `lib/vendor.mediatek.hardware.mtkpower-V2-ndk.so`；`xattr -s -p` 单独读这两个文件
   都能拿到 `same_process_hal_file` / `vendor_file`，说明数据在、解析被 stderr 污染。
4. **镜像内条数与标签数对账**：解包树 `find` 6484 行（含根与 `.cdh-unpack.json`），
   镜像条目 6482，其中 6482 条带 `security.selinux`（`log-pass.txt` 第 4 节）。

## 四、改动

- **新增步骤 1b**（`engine/build/07_build_vendor/officialunpack.go`，222 行）：把厂商
  `super/vendor_a.img` 整树解包到 `<厂商解包根>/super/unpack_vendor/`。
  - erofs（gold/三星/魅族）：`fsck.erofs --extract=<dir> --xattrs`，解完校验 `build.prop`
    与 `etc/` 存在；ext4（官方 Pixel 工厂包）：**不整树解**（macOS `rdump` 带不出
    `security.selinux`、丢执行位），只打一行说明，用到哪个路径由 `unpackOfficialPath`
    逐件解到同一目录。
  - 幂等：`.cdh-unpack.json` 记 `{image, kind, size, mtime}`，四项全等且 `build.prop`
    在 → 打「复用: 镜像没变，沿用上次的解包树（N 条目）」；戳记不是镜像条目，统计时
    用 `countOfficialUnpack` 剔除（否则首次/复用两次计数差 1）。
  - 整树成立后树里没有的路径直接判 `errVendorPathMissing`（布局差异），不再逐条试。
- `files.go`：`fileSource` 新增 `Official`；`officialFileSource` 把解包树放 `Trees` 首位，
  `extractSource` 命中树即返回，树里没有才按镜像逐件解；`replaceSourceHint` 优先给
  解包树路径 +「（步骤 1b 解包留档）」。
- `props.go`：`placeVendorBuildProp` 不再二次解镜像，直接从步骤 1b 的树取 `build.prop`
  落位，日志改「厂商 build.prop（取自步骤 1b 解包树）-> 装配树」。
- `setxattr.go`：`extractVendorReference` 改为复用步骤 1b 的树（要求 `kind=="erofs"` 且
  戳记新鲜，否则先 `ensureOfficialVendorUnpacked` 一次）；`readSelinuxLabels` 改用
  `cmd.Output()` 只取 stdout、stderr 单独收并打 `[note]`；新增 `missingLabelPaths`
  把没有真值的条目点名。
- `execute.go` / `plan.go`：步骤 1 之后插入步骤 1b；dry-run 与 `describeBuildPropSource`
  改口径。
- `officialunpack_test.go`：4 个用例（erofs 解包 + 复用计数一致 + 缺路径判定、
  ext4 不整树解、复用已有件、戳记过期）。
- 文档：`07_build_vendor/README.md` 增「步骤 1b」节与产物表/文件表/遗留；`manifest/README.md`、
  `Template.md`、`xiaomi/gold/README.md`、`flyme/m22/README.md` 的 build.prop 来源口径同步。

## 五、验证

gold，`./out/bin/cdh device build gold --only 07_build_vendor`：

1. 首次（删戳记强制重解）：`解包条目 6482（文件 5995 / 链接 222 / 目录 265，含 security.selinux
   扩展属性）`，2.0 GB / 约 5 秒。
2. 二次：`复用: 镜像没变，沿用上次的解包树（6482 条目）`——与首次计数一致。
3. 步骤 5 十条全部命中解包树，日志只有 `[replace] …（文件覆盖）/（目录，46|165|40 个文件，取代）`，
   没有逐条 `[unpack]` 抽取行。
4. 步骤 6：`==> [步骤 6] 厂商 build.prop（取自步骤 1b 解包树）-> 装配树` +
   `解包: …/super/unpack_vendor/build.prop` → 落位装配树。
5. 步骤 7：`[note] xattr 遍历报了 2 行错误（悬空链接等，不影响其余条目）` +
   `官方镜像标签真值: 6482 条（含 222 个符号链接自身）`（修复前 6480，改造前 6481）。
6. 产物对账（`log-pass.txt` 第 4 节）：装配树 800 条目与改造前逐条一致（`diff` 为空）、
   `vendor/build.prop` md5 `d9ea34b4107220af01052070801be2f7` 不变、
   `vendor-meta-rules.txt` 与改造前 `diff` 为空（丢的那 2 条标签不在装配树里）。
7. `gofmt`/`go vet`/`go test ./07_build_vendor` 全绿（含新增 4 个用例）。

## 六、遗留

- ext4 厂商镜像（oriole 等官方 Pixel 工厂包）不整树解，只把用到的路径解到同一棵树；
  标签继续走 `debugfs` 直读 inode。换 `e2fsprogs` 能带 xattr 的 `rdump` 后可统一成整树。
- `super/unpack_vendor` 会常驻占盘（gold 2.0 GB / 6482 条目）；换盘或清空间时删它即可，
  下次 07 自动重解（戳记随之消失）。
- 戳记只认 image 路径/大小/mtime：同一镜像被原地改写且大小与 mtime 都不变时会误判复用
  （正常流程下 01 重解会换 mtime，不触发）。
- 本次只重跑 07；`09_pack_super` → `11`（盘集）未重跑，`vendor.img` 里元数据已按新标签
  重建（规则表 diff 为空，预期无差异）。
- 其余机型未实跑：m22/popsicle/ss9380 等下次 07 会走步骤 1b（erofs 整树），需留一条
  「解包条目 / 复用」日志确认没有布局差异路径。
