# Issue: 07 步骤 5 的"隐形默认值"——引擎内置取件表退场，改过的件整份留在机型静态树

- 机型/环境：xiaomi/gold（Redmi Note 13 5G，MT6833，`OS3.0.10.0.VNQCNXM`，Android 15，
  `lineage-22.2`）
- 采样日期：2026-11-03
- 相关阶段：`07_build_vendor`（步骤 5 文件操作）
- 前置条目：[#141](../issue-official-vendor-unpack/solution.md)（厂商 vendor 整树解包留档）、
  [#116](../issue-vendor-files-unify/solution.md)（`vendor.files` 合成一张表）

## 一、问题

07 步骤 5 的「最终裁决」有两处不写在机型清单里：

1. **引擎里有一份内置表**：`files.go` 的 `builtinFileOps`（MTK 口径 10 条：6 个 GPU 身份
   扫描库 + `lib64/libpng.so` + `etc/{permissions,security,md}` 整目录 replace）。机型基线
   未声明 `vendor.files` 时自动套用——"某一家机的取件清单"成了全仓默认值。
2. **机型静态树里存官方件抄件**：gold 的 `manifest/xiaomi/gold/vendor/etc/` 有 7 个
   `media_codecs*.xml`，实际都是厂商 `vendor_a.img:/etc` 的副本（6 个逐字节相同，1 个带
   CDH 增量），同步的 `primary_audio_policy_configuration.xml` 则是 lineage 基底 + 增量。
   而步骤 5 的 `replace` 本来就会把这些路径盖掉——抄件在仓库里既不是真值，也不参与产物。

结果：`vendor.img` 里有什么，既不能只看机型清单（还有引擎默认），也不能只看清单条目
（还有静态树）。

## 二、根因

- **内置集合是"gold 一家机的口径"被当成了通用默认**。#116 把 `vendor.replaces` /
  `vendor.lifts` 合成 `vendor.files` 时留了这份兜底，理由是"零清单机型行为不变"；
  但真在用 07 的 7 台机全都自己声明了（兜底一次没生效过），而没跑过 07 的 6 台按 gold 的
  MTK 口径取件只可能失败——默认值是错的，只是没人踩到。
- **来源与落点混在一处**：静态树既承担"CDH 自有件"，又被动承载"官方件的抄件"，而这两类
  的维护口径完全相反（自有件是唯一真值，抄件应当跟随官方）。清单里 `etc/media_codecs*.xml`
  这 6 条与静态树同名文件并存时，"谁是最终内容"取决于步骤 5 与步骤 4 的先后，而不是写在
  repo 里的任何声明。

## 三、证据

### 3.1 兜底默认值必然错（未跑过 07 的机型）

`engine/out/devices/*/*/*/vendor` 里跑过 07 的 7 台（gold / m22 / oriole AP2A / 15r /
cph2797 / ss9380 / popsicle）基线**全部显式声明** `vendor.files`；剩下 6 台
（`nothing/3a`、`oneplus/ace3`、`oppo/pme110`、`realme/rmx3823`、`rmx5112`、`rmx6688`）
从没跑过 07，若按内置集合取件：非 MTK 机型没有 Mali/MEOW 这 5 个文件，
`etc/security` / `etc/md` 也大概率不存在（m22 官方 `/etc` 只有 `permissions` 与
`selinux`，照搬 gold 集合实测报 `厂商 vendor 缺少该路径: etc/security`，见 #116）——
第一步就硬失败。

### 3.2 静态树里 7 个文件与官方件的关系

`cmp` 逐个比对 `super/unpack_vendor/etc/` 与 `manifest/xiaomi/gold/vendor/etc/`：

| 文件 | 与官方件 | 处置 |
| --- | --- | --- |
| `media_codecs.xml` | 逐字节相同 | 删副本，基线 `from: official` 直取 |
| `media_codecs_dolby_audio.xml` | 逐字节相同 | 同上 |
| `media_codecs_google_audio.xml` | 逐字节相同 | 同上 |
| `media_codecs_google_video_le.xml` | 逐字节相同 | 同上 |
| `media_codecs_performance.xml` | 逐字节相同 | 同上 |
| `media_codecs_system_default.xml` | 逐字节相同 | 同上 |
| `media_codecs_c2.xml` | **不同**（28387 → 29400 B，519 → 539 行） | **整份留在静态树**（官方件 + v4l2 三条声明） |
| `primary_audio_policy_configuration.xml` | 官方**无此路径** | 整份留在静态树（lineage 基底 + `Remote Submix In`） |

`media_codecs_c2.xml` 的增量（`diff -u` 官方件 → 装配树）就是三个 codec 声明：

```text
+    <Decoders>
+        <MediaCodec name="c2.v4l2.avc.decoder" type="video/avc">
+        <MediaCodec name="c2.v4l2.hevc.decoder" type="video/hevc">
     <Encoders>
+        <MediaCodec name="c2.v4l2.avc.encoder" type="video/avc">
```

所以"直接改成 `from: official` 一把迁走"会让 v4l2 codec 静默消失（`media_config.xml`
里那几个 codec 名在前、HAL 却拿不到对应声明）。带增量的件必须有一个承载位置。

### 3.3 元数据不吃亏

这 7 个路径在 `img/vendor-meta-rules.txt` 里本来就是
`0644 0 0 1230768000 u:object_r:vendor_configs_file:s0 official official`——按**相对路径**
匹配官方镜像得出，与"静态树提供"还是"步骤 5 取官方件"无关；`etc/` 也在
`allowedVendorTop` 允许范围内。本次迁移实测元数据规则表 0 行差异（见 5.2）。

### 3.4 备选方案 `mode: patch`：实现并跑通，评估后否决

先按"官方件 + 补丁"做过一版：新增 275 行 unified diff 应用器（`patch.go`，64 行漂移、
无 fuzz、`\ No newline` 还原、纯插入 `-a,0`）、`fileOp.Patch` 字段与 `Plan.ManifestDir`、
gold 基线 17 条（6 条直取 + c2 走 `patches/etc/media_codecs_c2.xml.patch`），实跑 07
产物与迁移前逐字节相同（系统 `patch -p1` 交叉验证 md5 `c46a2a68…`）。但**否决**：

- 同一个文件出现两处真值（官方原件 + `.patch`），静态树里看不到成品，评审要同时读 diff
  与原件；仓库里"这个文件最终长什么样"多了一跳推理。
- 补丁只能表达"在官方件上加减几行"，表达不了"整段改写"；一旦厂商改动落进同一 hunk，
  只能硬失败并要求重做，换版成本并没有比整份 diff 低。
- 用户口径明确：`etc` 下不引入 patch，改过的件就整份放到 `vendor`。

代价是厂商换版时 c2 要人工 `diff` 一次——换版本就要重做整个基线，这点成本换不来第二处真值。
`patch.go` / `patch_test.go` / `patches/` 目录随之删除（净减约 500 行代码与测试）。

## 四、改动

### 4.1 删掉引擎内置集合

- `07_build_vendor/files.go`：删除 `builtinFileOps` 整段（84–148 行）与其注释；
  `fileOpsFromManifest` 未声明时返回 nil 并打
  `[warn] 基线清单未声明 vendor.files：本机型不做厂商取件（文件覆盖 / 整替换 / 删除都不执行）`。
- `07_build_vendor/plan.go`：`buildPlan` 不再回退到内置集合，`Plan.FileOps` 只来自清单；
  模式描述回"三种模式（添加 / 取代 / 删除）"。
- `files_test.go`：删 `TestBuiltinFileOpsAllOfficial`，新增
  `TestFileOpsFromManifestAbsentIsNil`（未声明 → nil + warn，不报错）。

### 4.2 数据迁移：来源归位

- 基线 `vendor.files`：10 → **16 条**——原 10 条不动，新增 6 条
  `{ "from": "official", "source": "etc/media_codecs*.xml", "mode": "replace" }`。
- 删掉静态树里 6 个与官方逐字节相同的 `media_codecs*.xml`（`vendor/` 22 → 16 个文件）。
- `media_codecs_c2.xml`（`c46a2a6839bf5907feca7a253f04810c`，29400 B）**整份保留**在
  `manifest/xiaomi/gold/vendor/etc/`，由步骤 4 落位；基线里不为该路径写条目。
- `primary_audio_policy_configuration.xml` 同理留在静态树（官方分区无此文件）。

### 4.3 代码：步骤 5 只有三种模式

`files.go` 落盘语义只剩 add / replace / delete（`placeFileOp` / `mergeVendorDir` /
`deleteVendorPath`），无 `patch` 字段、无补丁路径解析、无 `Plan.ManifestDir`；
`applyVendorFiles` 的标题回到 `厂商文件操作（添加 / 取代 / 删除）`。

### 4.4 文档

- `07_build_vendor/README.md`：步骤总览与步骤 5 标题、`vendor.files` 未声明 → 空表 +
  `[warn]`、"引擎没有内置集合"、`mode` 表三行；新增一段**放置规则**（改过的件整份放静态树，
  且不要在表里为同一路径写条目，否则 `replace` 会盖掉）+ 不引入 `patch` 的取舍；
  代码地图去掉 `patch.go`；测试表回到 15 文件 / 约 2960 行。
- `manifest/xiaomi/gold/README.md`：目录表去掉 `patches/`、步骤 5 改 16 条、
  "media_codecs 7 个"改为"6 个已无副本 + c2 仍有整份副本"、维护约定改成
  "要改内容就把改过的件整份放回本目录，并从 `vendor.files` 删掉该路径的条目"。
- `manifest/Template.md`：`vendor.files` 字段表删 `patch` 并写明"不提供补丁模式"；
  新建流程第 5 步去掉"可按 `.patch` 打增量"；`vendor/` 静态树生效判据补一句。

## 五、验证

### 5.1 gold 实跑 07（`--only 07_build_vendor`）

```text
==> 文件操作集合: 基线清单 vendor.files（16 项）
==> [步骤 5] 厂商文件操作（添加 / 取代 / 删除）
    输入: 厂商 vendor（文件操作 16 项）
[replace] … ×16（10 个原条目 + 6 个 etc/media_codecs*.xml，c2 不在其中）
```

c2 由步骤 4 写入（日志第 227 行）：
`写 out/devices/xiaomi/gold/OS3.0.10.0.VNQCNXM/vendor/etc/media_codecs_c2.xml（0644）`。

### 5.2 与迁移前四项对账

| 检查项 | 结果 |
| --- | --- |
| 8 个 media/audio 文件 md5 | **全部相同**（c2 仍 `c46a2a68…`，primary_audio 仍 `d306eac3…`） |
| 装配树条目 | 801 行 `find \| sort` 与迁移前 `diff` **为空** |
| 元数据规则表 | `vendor-meta-rules.txt` 821 行，与迁移前 **0 行差异** |
| `vendor.img` | **170676224 B**（与迁移前同字节数） |
| 官方件对账 | 6 个直取件与 `super/unpack_vendor/etc/` 逐字节相同；c2 与官方件 `cmp` 不同（带增量，符合预期） |

### 5.3 单测与静态检查

```text
$ gofmt -l 07_build_vendor/     # 无输出
$ go vet ./07_build_vendor      # 无输出
$ go test ./07_build_vendor -count=1
ok  cloud-phone-hub/engine/build/07_build_vendor   2.990s
$ ls 07_build_vendor/*_test.go | wc -l
15
```

## 六、遗留

- **`primary_audio_policy_configuration.xml` 仍是静态树整份件**：官方 `vendor_a.img` 没有
  这个路径，基底是 lineage 的 AOSP primary 模块 + `Remote Submix In`，与 c2 同属"改过的件"，
  口径一致（步骤 4 落位、步骤 5 不碰）。
- **其它机型的 media 配置没逐台对账**：m22 / 15r / cph2797 / ss9380 / popsicle 的静态树里
  很可能也有同类官方件抄件，各自与厂商解包树 `cmp` 一遍才能判定"删副本改直取"还是
  "整份留"，是逐机型的活。
- **09 → 11 未重跑**：`vendor.img` 已是新产物（字节数一致），`patch.img` 与盘集仍是上一轮。
- **证据只到构建层**：v4l2 codec 在实例内的可用性本轮未复测（内容逐字节相同，理论上无差异）。
- **不引入补丁机制的边界**：如果将来真出现"官方件要频繁小改、且换版需要自动跟随"的场景，
  再议；当前 13 台机只有 c2 / primary_audio 两处增量，整份静态树足够（见 3.4 的否决理由）。
