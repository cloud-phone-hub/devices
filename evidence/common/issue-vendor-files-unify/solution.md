# issue-vendor-files-unify：把 replaces / lifts 合成一张带 mode 的文件操作表，并收紧借件来源

- 机型：oriole（Pixel 6 / lineage-21.0）为主，m22、cph2797、ss9380 同步迁移清单
- 时间：2026-11-02
- 动机（用户原话）：「要求不能跨版本跨分区借用，这两个都是file操作，应该添加一个模式，代表是添加还是取代，或者删除」

## 一、问题

`07_build_vendor` 的「从别处拿件摆进 vendor」有两套并行写法，语义重叠但字段与执行路径完全不同：

| 旧写法 | 清单键 | 语义 | 执行 |
| --- | --- | --- | --- |
| 厂商原件覆盖 | `vendor.replaces` | 「最终裁决」：取不到即整阶段失败；目录先删后拷 | `replace.go` `applyVendorReplaces` |
| 逐条借件 | `vendor.lifts` | 只补缺，工具名 `lift`，与 replaces 各走一条 | `lift.go` `applyVendorLifts` |

三处直接后果：

1. **同一件事两种写法**。oriole 的显示栈 12 件写成 `lifts`，m22 的 libpng / etc/permissions 写成 `replaces`，ss9380 又混着写；读清单的人必须先去代码里确认哪个键先执行、谁盖过谁。
2. **「借件」没有来源约束**。`from` 接受 `lineage:<任意版本>` 与 `lineage:<任意分区>`，实际使用中出现过 `lineage:22.2`（跨版本）和 `lineage:system`（跨分区）。跨版本把别代的库塞进本代 vendor、跨分区把 system 分区里非 LLNDK 的件摆进 vendor，运行期都是一次 `dlopen` 失败，而清单里看不出它依赖哪一代平台库。
3. **没有「删除」这一档**。要么覆盖、要么借件，想删掉 lineage 带进来的某个文件/目录只能在步骤 6 或机型静态树里另写一段代码。

## 二、根因

`lift` 与 `replaces` 是历史上两次需求先后加进来的：`replaces` 先有（贴厂商原件口径），`lifts` 后加（贴「只补个别件」），加时沿用了「再来一个清单键 + 再来一个 apply 函数」的做法，没有回头合并。`from` 字段同时承担了「从哪个来源拿」与「从哪一代/哪个分区拿」两件事，格式是自由字符串 `分区[:额外段]`，解析处只做了 `strings.Split`，没有白名单——所以任何 `lineage:<任意值>` 都能通过。

换句话说：**问题不在取件能力，而在清单 schema 没有把「落盘方式」与「来源合法性」表达出来。**

## 三、证据

### 3.1 两套写法并存（改造前形状）

- `manifest/google/oriole/baselines/oriole-ap2a…json`：`vendor.replaces` 1 条 + `vendor.lifts` 12 条，其中 12 条是 `from: lineage:system`。
- `manifest/flyme/m22/…/12.5.0.0A.json`：`vendor.replaces` 2 条（libpng 缺省 mode、etc/permissions 显式 replace）。
- `manifest/oppo/cph2797/…json`：`vendor.replaces` 4 条。
- `manifest/samsung/ss9380/…json`：`vendor.replaces` 7 条。
- 零清单机型（gold 等）走内置 `vendorReplaces` 10 项（Mali/MEOW 五件、libOpenCL、libpng、etc/permissions、etc/security、etc/md）。

### 3.2 oriole 那 12 件跨分区借件的真实出处（`vendor-a-stat.txt`）

用 `debugfs stat` 逐件读官方 Pixel A14 `vendor_a.img`（`out/roms/google/oriole/AP2A.240905.003.F1/super/vendor_a.img`，ext4）：

```
/lib64/libpng.so                                   Inode: 2433  Mode: 0644
/lib64/android.hardware.graphics.composer@2.1.so    Inode: 2112  Mode: 0644
… @2.2 2114 / @2.3 2115 / @2.4 2116 …
/lib64/libfmq.so                                    Inode: 2364  Mode: 0644
/lib64/libui.so                                     Inode: 2479  Mode: 0644
/lib64/android.hardware.graphics.allocator@2.0/3.0/4.0.so  Inode: 2100/2101/2102  Mode: 0644
/lib64/android.hardware.graphics.mapper@2.0/2.1/3.0.so      Inode: 2117/2118/2119  Mode: 0644
```

13 件全部存在、全部 `0644`、inode 落在 2100~2479 的连续区间（同一批打包进镜像）。对照 lineage-21.0 底包 `vendor_a.img` 的 `/lib64` 共 81 项，命中这 13 件的行数为 **0**。

结论：这 12 件不需要跨分区，官方 vendor 分区里就有同代原件——`from: vendor` 同时满足「同代」与「同分区」。也正是这条证据，让「禁止跨分区」不是一句口号，而是有替代出路的硬约束。

### 3.3 新 schema 的负路径实测（`log-fail.txt`）

四份 oriole 基线的改造副本，全部在**步骤 0 计划期**被拒绝（`exit=1`，不写任何产物）：

| 案例 | 报错 |
| --- | --- |
| `legacy-replaces.json`（保留 `vendor.replaces`） | `[err] 清单 vendor.replaces / vendor.lifts 已废弃：合并成 vendor.files（每条用 mode: add / replace / delete 表示落盘方式）` |
| `legacy-lifts.json`（保留 `vendor.lifts`） | 同上 |
| `cross-partition.json`（`from: lineage:system`） | `[err] 文件操作来源 "lineage:system" 非法：只允许 official（官方固件）或 lineage/base（本机底包，同版本、vendor 分区）；不允许跨版本借件（如 lineage:22.2）或跨分区借件（如 lineage:system）` |
| `cross-version.json`（`from: lineage:22.2`） | `[err] 文件操作来源 "lineage:22.2" 非法：…（同上，两种违规同一条文案点名）` |

### 3.4 正路径（`log-pass.txt`）

- `gofmt -l 07_build_vendor/` 无输出；`go build ./...`、`go vet ./07_build_vendor/` 通过；`go test -count=1 ./...` **20 个包全绿**。
- `build-vendor --dry-run` 实跑三机型，步骤 5 的计划命令与旧行为逐条对得上：
  - m22 2 项（`fsck.erofs --path=…` 取官方 erofs 镜像）；
  - cph2797 4 项（含 `graphics.common-V5-ndk.so` 回填）；
  - oriole 13 项，12 条借件全部变成 `debugfs -R "dump /lib64/… <目标>" out/roms/google/oriole/AP2A.240905.003.F1/super/vendor_a.img`。

## 四、改动

### 4.1 引擎（`engine/build/07_build_vendor/`）

| 文件 | 改动 |
| --- | --- |
| `files.go`（新增） | 新模型 `fileOp{From,Source,Target,Mode,Optional}` + `fileMode`（`add`/`replace`/`delete`）；`fileOpsFromManifest` 读 `vendor.files`，遇到旧键 `replaces`/`lifts` 直接报错；`validateFileOps` 校验字段组合；`parseFileFrom` 白名单收紧；`fileOpSource` 选来源；`applyVendorFiles` 主循环；`fileOpsInputText`/`fileOpsInfoText`/`fileOpHint`/`modeLabel` 负责日志与 dry-run 文案；`builtinFileOps` 承接原内置 10 项 |
| `replace.go`（瘦身为执行侧） | 保留 `fileSource`/`extractSource`（逐文件树 → ext4 `debugfs` / erofs `fsck.erofs`）/`placeFileOp`/`mergeVendorDir`/`replaceVendorDir`/`copyVendorTree`；删掉 `replaceEntry` 与 `applyVendorReplaces`/`applyReplaceSet` |
| `lift.go` | **删除**（能力并入 `files.go` + `replace.go`） |
| `plan.go` | `Plan.Replaces` → `Plan.Files`；`FileOps []fileOp` 由清单或内置集合求值，再 `validateFileOps`；dry-run 列 `plan.FileOps` |
| `assemble.go` / `execute.go` | 调 `applyVendorFiles`；`vendor-info.txt` 的键改为 `vendor-files: ` |

三种 mode 的落盘语义（`files.go` 注释与 README 步骤 5 一致）：

| `mode` | 文件 | 目录 |
| --- | --- | --- |
| `add` | 覆盖写（对文件而言与 `replace` 同义） | 合并：同名覆盖、其余保留 |
| `replace`（缺省） | 覆盖写（内容与权限以来源原件为准） | 先删后拷（最终裁决，不保留旧文件） |
| `delete` | 删除目标（文件、符号链接或整目录，不取来源） | 同左；目标不存在只打 `[skip]`，不算失败 |

来源只认两种：`""`/`vendor`/`official` → 官方固件；`lineage`/`base` → 本机底包（版本取 `rom.lineage.version`，**分区固定 `vendor`**）。`lineage:22.2`、`lineage:system`、`system`、`lineage:../etc` 一律在计划期报错。

`optional` 的边界也一并收紧：只容忍「来源在、但没这个路径」（布局差异）；连逐文件树带分区镜像都没有仍然硬失败（那是底包没解包，不是布局差异）。这条是 `replace.go` `extractSource` 里 `len(src.Trees) > 0` 分支决定的。

### 4.2 清单（4 份，`replaces`/`lifts` → `files`）

| 清单 | 迁移结果 |
| --- | --- |
| `manifest/flyme/m22/baselines/12.5.0.0A.json` | 2 条：libpng（缺省 replace）、etc/permissions（`mode: replace`） |
| `manifest/oppo/cph2797/baselines/CPH2797_16.0.8.301_EX01_A11_full.json` | 4 条：libpng、etc/permissions、etc/security、graphics.common-V5-ndk.so |
| `manifest/samsung/ss9380/baselines/SM-S9380_3_20260813083211_652iw49crl_fac.json` | 7 条（原 replaces 整表） |
| `manifest/google/oriole/baselines/oriole-ap2a.240905.003.f1-factory-655c44e7.json` | 13 条：libpng + 12 条 `from: vendor`、`mode: add`；`source` 去掉 `system/` 前缀回 `lib64/…`；首条 note 记录「借件来源收紧（2026-11-02）」与逐件 inode |

清单 note 里的口径文字同步改：m22/cph2797 的「走 replaces 取件」→「走 vendor.files 取代取件」，cph2797 的「replaces 整表由机型声明」→「文件操作整表由机型声明」，ss9380 的「内置 vendorReplaces」→「内置 builtinFileOps」。

### 4.3 测试与文档

- 删 `lift_test.go`、`replace_test.go`；`files_test.go` 成为唯一文件操作测试文件，覆盖：清单解析（含旧键拒绝）、来源选择器收紧（`lineage:22.2`/`lineage:system`/`lineage:22.2:system` 必须报错且文案同时含「跨版本」「跨分区」）、三种 mode 落盘、`delete` 幂等、`optional` 容忍、`fileOp.paths()`。
- 两个 composer compat 用例移入 `composercompat_test.go`（原本与 `files_test.go` 重复定义，`go vet` 报 redeclared）。
- `07_build_vendor/README.md`：步骤总览把 5/5b 合成一行「文件操作（清单 `vendor.files`：添加 / 取代 / 删除）」；原「步骤 5：厂商原件覆盖」与「步骤 5b：借件（lift.go）」两节合并重写为「步骤 5：文件操作（files.go / replace.go）」，含不跨版本/不跨分区说明、`builtinFileOps` 表、mode 语义表、optional 新语义、oriole 新写法示例与跨分区撤销说明；代码地图 `lift.go` 行删除、`replace.go` 职责改为执行侧；测试表 `replace_test.go / lift_test.go` → `files_test.go`。

## 五、验证

1. **静态**：`gofmt -l 07_build_vendor/` 无输出；`go build ./...`、`go vet ./07_build_vendor/` 通过。
2. **单测**：`go test -count=1 ./...` 20 包全绿（`07_build_vendor` 17.07 s）。
3. **清单完整性**：`json.load` 复核 4 份基线，`files` 条数 m22=2 / cph2797=4 / ss9380=7 / oriole=13，且都不再含 `replaces`/`lifts` 键。
4. **正路径 dry-run**：m22 / cph2797 / oriole 三机型步骤 5 计划命令与旧版逐条等价（见 `log-pass.txt`）；oriole 12 条由 `fsck.erofs`/跨分区取件变成 `debugfs dump` 官方 `vendor_a.img`。
5. **负路径**：旧键 ×2、跨分区、跨版本共 4 例均在计划期 `exit=1`（见 `log-fail.txt`）。
6. **同代同分区可得性**：`debugfs stat` 逐件证明 oriole 12 件在官方 A14 `vendor_a.img` 存在且 `0644`（见 `vendor-a-stat.txt`）。

## 六、遗留

1. **oriole 起机未复验**：12 件从「lineage-21.0 的 system 分区」改成「官方 A14 vendor 分区」后，库文件内容变了（同代但不同 ROM）。需按 07→09→11 重跑并起实例，用 `logcat` 确认 `composer@2.1~2.4` / `mapper` / `allocator` 无 `CANNOT LINK`、SurfaceFlinger 不崩。这是本方案唯一的运行期风险点，也是下一步要做的第一件事。
2. **零清单机型仍走内置表**：gold 等未声明 `vendor.files` 的机型沿用 `builtinFileOps`（gold/MTK 口径 10 项）。语义与旧 `vendorReplaces` 一致，但「内置默认值」本身仍是隐藏行为——建议后续让 `cdh device init` 把内置表落成显式清单，或至少在计划摘要里打一行「未声明 vendor.files，沿用内置 10 项」。
   *（2026-11-03 已按「删掉默认值」解决：`builtinFileOps` 整段删除，未声明即空表 + 一行 `[warn]`，见 [#142](../issue-vendor-files-no-builtin/solution.md)。）*
3. **`delete` 尚无真实使用者**：三种 mode 里 `delete` 只有单测覆盖，四个机型清单都没用它。真正用它替代机型静态树里的零散删除（如步骤 6 的 fstab 清理）是独立任务。
4. **跨版本/跨分区禁令只有静态校验**：`parseFileFrom` 拦的是字面写法，`from: lineage` 的版本仍取自 `rom.lineage.version`——如果清单把 `rom.lineage.version` 写成与底包实际不符的值，校验就失去意义（该字段的正确性由 `--base-vendor` 路径与 01 解包共同保证，属既有约束）。
5. **本证据只到 dry-run**：三机型都没有起实例（24 GiB 机器上三实例会挤爆），运行期结论以第 1 条复验为准。
