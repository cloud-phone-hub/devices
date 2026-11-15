# Issue: 06 只认清单里的 odm 属性件抄件，基线 `odm` 段写了没人读

- 设备：`xiaomi/gold`（机制改动落在 06 与内部库，gold 实跑验证）
- 阶段：`06_build_odm` 步骤 1（属性件）、`internal/proplines`、`internal/vendorimg`
- 状态：**已闭环**（2026-11-03，gold `--only 06_build_odm` 实跑 + 二次复用）
- 关联：[#139 issue-vendor-buildprop-official-source](../issue-vendor-buildprop-official-source/solution.md)
  （同一口径的 vendor 侧：属性件真值 = 厂商原件 + 基线增量）、
  [#141 issue-official-vendor-unpack](../issue-official-vendor-unpack/solution.md)
  （厂商侧唯一解包树，06 复用它取原件）

## 一、问题

gold 基线里早有 `odm` 段：

```json
"odm": {
  "img":  "vendor_a.img",
  "prop": "/etc/build.prop",
  "build-prop-sets": {
    "ro.odm.product.cpu.abilist":   "arm64-v8a",
    "ro.odm.product.cpu.abilist32": "",
    "ro.odm.product.cpu.abilist64": "arm64-v8a"
  }
}
```

但 `06_build_odm` 的输入只有**机型目录里的手抄件**：`selectOdmProps` 取
`manifest/xiaomi/gold/odm/build.prop`（顶层）> `odm/etc/build.prop`，整段基线**没有任何读取点**
（全仓 Go 代码里 `odm.build-prop-*` 共 0 处读取）。于是真值是两份文件靠人同步：

- 基线 `odm` 段三条 `abilist*` 只是“写着”；
- 真正落盘的是 `manifest/xiaomi/gold/odm/etc/build.prop`（32 行），它是厂商
  `vendor_a.img:/odm/etc/build.prop`（34 行）人工套上三条 sets 的结果（见 `log-fail.txt` 第 3 节）。

而且基线 `prop` 本身就写错：`/etc/build.prop` 在 vendor 镜像里不存在（vendor 的属性件在镜像根
`/build.prop`，odm 的在 `/odm/etc/build.prop`，见 `log-fail.txt` 第 1 节）。旧代码不读它也看不出错，
一旦按它取件就会撞 `ErrPathMissing`、退回抄件——抄件成了“政策兜底”。

## 二、根因

06 建立时 odm 属性件的唯一来源就是清单静态树，`odm` 段是 2026-10~11 一批清单迁移时按
“将来要由镜像现取”的设想写下的，只落了字段没落消费者。这与 07 在 #139 里修掉的是同一个病：
**同一份属性有两处真值（基线 + 抄件）且没有代码把它们对上**，抄件漂移时构建仍全绿。

第二个根因是取件能力分散：读 erofs/ext4 单件、定位 `unpack_vendor`、`fsck.erofs`/`debugfs`
的三级查找都写在 07 里，06 想按镜像取件只能复制一份——复制就会与 07 的阈值/文案/回退链漂移。

## 三、证据

1. **旧写法指错文件**（`log-fail.txt` 第 1 节）：`unpack_vendor/odm/etc/build.prop` 存在，
   `unpack_vendor/etc/build.prop` 不存在（镜像根才是 `build.prop`）。
2. **厂商原件是三档 abilist**（第 2 节）：`abilist=arm64-v8a,armeabi-v7a,armeabi`、
   `abilist32=armeabi-v7a,armeabi`。所以“先做 ABI 校验、再套增量”的写法永远过不去，
   顺序必须是**先套 sets/unsets、后校验**。
3. **抄件与真值的差异恰好是三条 sets + 两个末尾空行**（第 3 节 `diff -u`）：证明抄件没有额外
   信息，删掉不丢东西。
4. **改后同一条错路径直接失败**（第 4 节）：`[warn] 镜像里没有 odm.prop 声明的路径` →
   `[err] 取不到 odm 属性件 …（先跑 01_unpack，或核对清单 odm.img / odm.prop）`，退出码 1，
   不再“静默退回抄件”。
5. **基线 `odm` 段此前无消费者**（第 5 节）：`grep` 只在 07 的 `vendor` 段命中。

## 四、改动

**新增 `internal/proplines`（属性表语义单一化，07/06 共用）**
- `Section{Sets, Unsets}`、`Read(mf, "vendor"|"odm")`、`DecodeSets/DecodeUnsets`（只认对象，
  旧数组报错）、`Split/Join`（统一单换行）、`Apply`（**先 unsets 删行，再 sets 覆盖或追加**，
  同键只改第一处，返回 `[prop] 删除/覆盖/追加 …` 文案）、`Abi`（取最后一个同名键）；
- `07_build_vendor/props.go` 改为委托：删掉内联的 `decodePropSets` / `splitPropLines` /
  覆盖逻辑，保留 `readVendorBuildProps` / `applyVendorBuildProps` 作为薄壳。

**新增 `internal/vendorimg`（镜像取件单一实现，07/06 共用）**
- `Kind(image)`（ext4/erofs magic 判定，常量导出给测试）、`UnpackDir(image)`、
  `LocateDebugfs()` / `LocateErofs()`、`Extract(opts, image, source, destDir)`
  （ext4 走 `debugfs dump/rdump` + 按 stat 补执行位；erofs 走 `fsck.erofs --extract`，
  `failed to lookup` → `ErrPathMissing`）；
- 07 的 `files.go`（两个 extract + 缺失哨兵）、`apex.go`（payload 类型判定）、
  `assemble.go`（debugfs 定位）、`unpackbase.go`（解包目录名）全部改为委托，
  测试可见符号（`locateErofs`/`locateDebugfs`/`unpackVendorDir`）保留为包装。

**06（`build/06_build_odm`）**
- `plan.go` 新增 `PropDecl`（`img` / `prop` / sets / unsets）、`OfficialImg`、`UnpackDir`、
  `ManifestPath`；`readOdmBaseline` 读清单 `odm` 段并做三条硬校验：
  空段 `{}` 视为未声明、只写 `img` 或只写 `prop` 报错、`prop` 必须是规范化绝对路径；
- `resolveOfficialImage`：`<厂商解包根>/super/<name>` → `<厂商解包根>/<name>`；
- `officialOdmProp`：`<UnpackDir>/<prop 去前导 />` 命中即用（01/07 留档）→ 否则
  `vendorimg.Extract` 解到同一目录留档 → 镜像里没有则 `[warn]` 并退静态副本（+ `[warn]`）
  → 全无则报错；
- `applyBaselineOdmProps`：取原件 → `proplines.Apply` → `checkOdmAbi` → `writeOdmProp`
  → `warnOdmStaticDrift`（静态副本降级为漂移核对，只告警不阻断）；
- `applyStaticOdmProps` 保留无 `odm` 段机型的老口径（顶层 `odm/build.prop` 优先、不落 etc）；
- `RunStage` 的 skip 判据改为「清单 `odm/` 目录存在 **或** 基线 `odm` 段声明了属性件」，
  这样删掉 gold 的静态副本后仍然出产物，且 `syncOdmTree` 用 `keepProps` 保住生成的
  `etc/build.prop`；
- `main.go` 的 `cdh build-odm` 新增 `CDH_MANIFEST`（整份 `env.Load`，与编排同口径），
  未设时保留 `CDH_VENDOR/CDH_BRAND/...` 最小集合。

**数据与文档**
- gold 基线 `odm.prop` 由 `/etc/build.prop` 校正为 `/odm/etc/build.prop`（并补 `note`）；
- 删除 `manifest/xiaomi/gold/odm/etc/build.prop`（32 行手抄件），`odm/` 目录随之消失；
- `devices/manifest/Template.md` 第 1/2/5/8/9 节、`devices/manifest/README.md` 新增
  「基线里的 odm 属性」、`engine/build/README.md` 阶段表、`xiaomi/gold/README.md`
  目录表与新增小节，全部改为「属性件真值在基线 `odm` 段，机型目录不放抄件」。

## 五、验证

1. **gold 实跑**（`log-pass.txt` 第 1~2 节）：首次 `CDH_ODM_REFRESH=1` 打镜像
   `odm-5695f4893471`（旧 `odm-78ba56d2a671`），日志打
   `基线 odm 属性: vendor_a.img:/odm/etc/build.prop（sets 3 项 / unsets 0 项）`、
   `原件: …/unpack_vendor/odm/etc/build.prop（整树解包留档）`、
   `[prop] 覆盖 ro.odm.product.cpu.abilist=arm64-v8a`、`[prop] 覆盖 …abilist32=`；
   二次运行 `odm 覆盖件已一致` + `[skip] odm.img 可复用`。
2. **产物对账**（第 3 节）：新生成件 34 行（= 厂商原件行数），与旧手抄件逐行相同，
   差异只有原件末尾那两个空行——即“抄件没有额外信息”成立。
3. **镜像内实测**（第 4 节）：`debugfs -R 'cat /etc/build.prop' img/odm.img` 第 11~13 行
   `abilist=arm64-v8a` / `abilist32=` / `abilist64=arm64-v8a`。
4. **错误路径**（`log-fail.txt` 第 4 节）：`prop` 写回 `/etc/build.prop` 时 06 直接失败，
   不再退回抄件。
5. **测试**：`go build ./...`、`go vet ./...` 通过；`gofmt -l` 对本轮改动文件无输出
   （仅既有的 `09_build_patch/execute.go`、`internal/f2fs/*`、`internal/manifest/manifest.go`
   仍在历史未格式化清单里）；`go test ./...` 全绿，其中
   `06_build_odm` 新增 6 个用例（基线驱动生成 / 缺 `prop` 报错 / 相对路径报错 /
   原件缺失报错 / 空段保持老口径 / 只有基线段无覆盖树），`07_build_vendor`（15 文件）、
   `internal/proplines`、`internal/vendorimg` 全 ok。

## 六、遗留

- **只验证到 06**：`09_build_patch`（`patch.img` 的 `/odm` 子树）与 `11_pack_vda` 未重跑，
  现有 `patch.img` 里那份 `/odm/etc/build.prop` 仍是旧内容（语义相同，仅少两个末尾空行）；
  下次整线构建会自动跟上。
- **`prop` 语义是镜像内绝对路径**，但 `odm.img` 段用的是镜像**名**而非路径；若将来出现机型
  无独立 odm 分区且 odm 树也不在 `vendor_a.img` 里（如放在 `system`/`my_*`），需要在
  `resolveOfficialImage` 里扩候选，当前只认 `super/<name>` 与解包根 `<name>`。
- **其它机型没有 `odm` 段**（`m22` 的 `odm/etc/build.prop` 仍在静态树），它们的 06 行为与改前
  完全一致；等各自基线补 `odm` 段后再切同一口径，切换时要一并删静态副本。
- `proplines.Join` 只统一末尾单个换行、保留文件中间的空行（与厂商原件一致）；若将来要求
  “生成件与抄件逐字节相等”，需要在 `Apply` 之后额外裁剪末尾空行——当前刻意不做，
  保留厂商原件的原貌更利于对账。
