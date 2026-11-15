# gold 清单 lineage 版本对齐 22.2

- 日期：2026-10-30
- 设备：`xiaomi/gold`（Redmi Note 13 5G，mt6833，Android 15）
- 阶段：清单（`manifest/xiaomi/gold/baselines/*.json`）+ 01 解包产物目录名
- 关联：#70 / #71（同一天 m22 的两次收敛，本条是 gold 侧的记录类不一致）

---

## 1. 问题

清单首行 `rom.lineage.version` 写的是 `lineage-22.0`：

```json
    "lineage":{
      "version": "lineage-22.0",
```

但只要对这份清单做任何一次路径推导，引擎就会顶一条告警：

```
$ ./out/bin/cdh env manifest/xiaomi/gold/baselines/gold_images_OS3.0.10.0.VNQCNXM_20260715.0000.00_15.0_cn_a9729c6694.json
[warn] 清单声明 lineage-22.0，实际底包为 UTM-VM-lineage-22.2-20261010-UNOFFICIAL-virtio_arm64only.zip
...
lineage 底包                    roms/lineage/UTM-VM-lineage-22.2-20261010-UNOFFICIAL-virtio_arm64only.zip   存在
lineage 解包根                  out/roms/lineage/lineage-22.0                                             存在
```

即：**清单说 22.0，实际吃的底包一直是 22.2**。这与仓库里其它 15 份基线清单的填法相反——它们填的都是真实底包版本（`lineage-22.2` / `lineage-23.2` / `lineage-24.0`），只有 gold 这一份是 `lineage-22.0`。

## 2. 根因

`rom.lineage.version` 不是纯注释，它有两个用途（`build/internal/env/env.go`）：

1. **解包目录名**：`lineageDir = out/roms/lineage/<rom.lineage.version>`（`:308` 取字段、`:334` 决定 `lineageName`、`:353` 拼目录）。
2. **底包匹配线索**：`resolveLineageZip(version, …)`（`:537`）先按完整版本号匹配 zip 名，不命中才**退级到主版本**，再不命中才取目录内唯一的 zip。

gold 的来历是：早期第一次解包时显式传了 `CDH_LINEAGE_VERSION=lineage-22.0`（这个值后来被抄进 02/03/04/05 的 README 示例，一直沿用），目录因此固化成 `out/roms/lineage/lineage-22.0`；底包后来换成 22.2（`UTM-VM-lineage-22.2-20261010`），但目录名没动。

于是每次解析都走"降级兜底"：完整串 `lineage-22.0` 在 `roms/lineage/` 里 **0 命中**（该目录只有 21.0 / 22.2-20261010 / 23.2 / 24.0），退到主版本 `lineage-22` 才命中 22.2 那个包 —— 这就是那条 `[warn]` 的来源，也是它一直"能跑"的原因。记录与事实不符，但被兜底逻辑盖住了。

## 3. 证据

**(1) 镜像内自述**（`system_a.img` 的 `/system/build.prop`，boot/vendor 之外的唯一版本真源）：

```
$ debugfs -R "cat /system/build.prop" out/roms/lineage/lineage-22.2/super/system_a.img
ro.build.version.incremental=1791608345
ro.build.version.release=15
ro.lineage.version=22.2-20261010-UNOFFICIAL-virtio_arm64only
```

**(2) `roms/lineage/` 内没有 22.0 的底包**：

```
UTM-VM-lineage-21.0-20260804-UNOFFICIAL-virtio_arm64only.zip        878,196,404
UTM-VM-lineage-22.2-20261010-UNOFFICIAL-virtio_arm64only.zip      1,003,458,732
UTM-VM-lineage-23.2-20260922-UNOFFICIAL-virtio_arm64only.zip      1,169,728,685
UTM-VM-lineage-24.0-20261010-UNOFFICIAL-virtio_arm64.zip          1,332,680,296
```

仓库根 `lineage-rom/` 里还留着一份真的 `UTM-VM-lineage-22.0-20260804-….zip`，那是下载中转目录，`resolveLineageZip` 只扫 `roms/lineage/`，不参与解析（否则金标目录会被 22.0 污染）。

> 2026-11-03 更正：仓库根 `lineage-rom/` 现为指向 `engine/out/roms/lineage` 的相对软链，
> 里面已无 zip；同日 `cdh-vendor` 的 lineage 路径改为直接取 `engine/out/roms/lineage/<版本>`
> （`CDH_LINEAGE_BASES` 可改父目录），不再经过该软链。

**(3) 其它机型口径**：15 份基线清单里 `rom.lineage.version` 全部等于真实底包版本，gold 是唯一的 `lineage-22.0`。

**(4) 文档自述**：`build/01_unpack/README.md` 第 4 条原文已经写明"本项目流水线显式给了 `CDH_LINEAGE_VERSION=lineage-22.0`，所以目录是 `out/roms/lineage/lineage-22.0`（目录名与底包实际 22.2 **不要求**一致）"——机制没错，但作为记录该对齐了。

## 4. 改动

**（1）目录改名（同盘 rename，不动内容）**

```bash
cd engine/out/roms/lineage && mv lineage-22.0 lineage-22.2
```

`mv` 在同一文件系统内是元数据操作，26 GB 内容不复制、不重写；`unpack_vendor/`（453 文件）随之迁走。

**（2）清单**

`manifest/xiaomi/gold/baselines/gold_images_OS3.0.10.0.VNQCNXM_20260715.0000.00_15.0_cn_a9729c6694.json`：

- `rom.lineage.version`：`lineage-22.0` → **`lineage-22.2`**
- 新增 `rom.lineage.measured-from`，把实测依据与改名来历写进清单（防止后来人再按目录名反推版本）：

```
out/roms/lineage/lineage-22.2/super/system_a.img 的 /system/build.prop：
ro.lineage.version=22.2-20261010-UNOFFICIAL-virtio_arm64only、ro.build.version.release=15；
底包 roms/lineage/UTM-VM-lineage-22.2-20261010-UNOFFICIAL-virtio_arm64only.zip。
2026-10-30 由 lineage-22.0 对齐而来（gold 早期解包时显式传了 CDH_LINEAGE_VERSION=lineage-22.0，
目录名沿用到今天，内容一直是 22.2）。
```

**（3）文档同步**（只改"目录路径"与 `CDH_LINEAGE_VERSION=` 示例，共 43 处）

| 文件 | 处数 | 内容 |
| --- | --- | --- |
| `build/01_unpack/README.md` | 7 | 示例命令 `--out`、dry-run 回显、产物树、两条校验 `ls`；第 4 条机制说明重写为"早期给了 22.0、2026-10-30 已改名对齐" |
| `build/02_build_kernel/README.md` | 3 | 独立运行示例 2 处 + 编排器注入值说明 |
| `build/03_build_modules/README.md` | 2 | 独立运行示例 |
| `build/04_build_boot/README.md` | 9 | 示例命令、`--base-boot`、日志回显、`boot-info.txt` 实样、排错表"目录名不是 …" |
| `build/05_build_vendor_boot/README.md` | 9 | 示例命令、`--lineage-dir`、日志回显、`vendor_boot-info.txt` 实样、复跑校验 |
| `build/07_build_vendor/README.md` | 13 | 步骤表输入/输出、默认参数表 `--base-vendor`、日志回显、产物清单 |
| `build/07_build_vendor/unpackbase.go` | 1 | `unpackDirName` 注释里的举例路径 |

**（4）测试同步**（读真实产物的两处，不改会直接失败）

- `build/test/real_artifacts_test.go`：`const realLineage = "../../out/roms/lineage/lineage-22.2"`
- `build/test/build_boot_test.go`：`baseBoot` 路径 + `LineageVersion: "lineage-22.2"`

**（5）刻意保留的 `lineage-22.0`**

| 位置 | 理由 |
| --- | --- |
| `build/02_build_kernel/README.md:114` | `KernelTag(lineage 版本, 内核主次)` 的示例，讲的是 `22.x + 6.6 → lineage-22-6.6`，与具体小版本无关 |
| `build/07_build_vendor/README.md:209` | "底包本身没有 `vendor/apex`（`lineage-22.0` 及更早）"——讲的是**版本特性**，22.2 起才有 `vendor/apex`（gold 实跑走的就是步骤 2b 摊平 APEX），改了反而错 |
| `build/internal/env/env.go:28`、`build/01_unpack/plan.go:256` | 讲退级匹配/文件名解析机制的举例 |
| `build/test/unpack_test.go`、`plan_test.go`、`kernel_test.go`、`modules_test.go` | 用 `TempDir` 或任意字符串作参数，断言的是"输入什么就输出什么"，自洽 |
| `manifest/nothing/3a/baselines/Asteroids_V3.2-251013-1406.json` | 该机型自己的基线描述，与 gold 无关 |
| gold 清单新增的 `measured-from` | 记录改名前的历史值，属证据 |

## 5. 验证

| 项 | 结果 |
| --- | --- |
| `cdh env <gold 清单>` | **无 `[warn]`**；`lineage 底包 = roms/lineage/UTM-VM-lineage-22.2-20261010-…zip`、`lineage 解包根 = out/roms/lineage/lineage-22.2`、`vendor_a.img` / `vendor_dlkm_a.img` 全部 `存在` |
| `cdh build-vendor <gold> --dry-run` | `vendor 基底: out/roms/lineage/lineage-22.2/super/vendor_a.img（清单 lineage 推导）`；`解包目录: …/lineage-22.2/super/unpack_vendor（已有 453 个文件）`——`mv` 前的解包产物完整跟过来了 |
| `go test ./test/ -run "TestSuperMetadataReal\|TestBuildBootDryRun"` | `ok cloud-phone-hub/engine/build/test 0.299s`（两处改路径的测试均通过；改前会 skip/失败） |
| 目录完整性 | 顶层 10 个 img（`BIOS/EFI/boot_a/firmware/metadata/misc/persist/super/vendor_boot_a`）+ `super/`，共 271 个文件 |
| 镜像内自述 | `ro.lineage.version=22.2-20261010-UNOFFICIAL-virtio_arm64only`、`release=15`、`incremental=1791608345`（与改名前逐字一致） |
| 内核 tag | `KernelTag("lineage-22.2","6.6")` 只取主版本 → 仍是 `lineage-22-6.6`，内核源码目录 `out/kernel/lineage-22-6.6` 与模块构建卷名**不变** |

## 6. 遗留

- **未跑 01/04/05/07 全量**：`mv` 不改字节，`vendor_a.img` / `boot_a.img` 内容与改名前同一份，重跑只会把同样的输入再产出一次。产物侧的旧路径快照（`out/devices/xiaomi/gold/OS3.0.10.0.VNQCNXM/img/boot-info.txt`、`vendor-info.txt` 等里的 `out/roms/lineage/lineage-22.0/…`）会在下次跑到对应阶段时自动刷成 22.2；`patch.img` / `out/dist` 仍是旧版。
- **`roms/lineage/` 只保留在用的底包**：真 22.0 的 zip 留在仓库根 `lineage-rom/`（下载中转），引擎不扫它。若将来要真跑 22.0，需要把 zip 放进 `roms/lineage/` 并另起 `--lineage-version lineage-22.0` 解包到独立目录，别覆盖 22.2。
- **退级兜底逻辑不再被这条清单触发**：`resolveLineageZip` 的"完整版本 0 命中 → 退主版本"分支现在没有实际用例覆盖（gold 已精确命中），该分支的测试只在 `unpack_test.go` 里用 `TempDir` 造数据验证。
- **回退**：`mv out/roms/lineage/lineage-22.2 out/roms/lineage/lineage-22.0`，清单 `version` 改回 `lineage-22.0`（`measured-from` 可留），然后 `git checkout -- build/ docs` 里那 43 处文档与 2 处测试。
- **全量 `go test ./...` 两处既有 FAIL，与本条无关**（本轮顺带发现，未修）：
  - `TestBuildVendorBootMergeLineage`：`注入的 fstab 缺版本标记`——`build/05_build_vendor_boot/fstab.go:51` 已把 `fstabMergeRevision` 改成 `# fstab-rev: merge-v5`，而 `build/test/build_vendor_boot_test.go:27` 的 `fstabRevMarker` 仍是 `merge-v4`，测试常量没跟着升版。
  - `TestBuildVendorBootCacheReuse`：TempDir 夹具里没准备 `vendor/vendor_boot.img`，报「找不到 vendor vendor_boot 镜像」。
  判定依据：`build_vendor_boot_test.go` 本轮零改动（`git status` 为空）、文件内不含任何 `out/roms` / `lineage-22` / `realLineage` 引用，且 05 README 的文档改动不参与 Go 测试。
