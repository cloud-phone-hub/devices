# 07_build_vendor 步骤 5 硬编码 MTK 覆盖集合，QTI 机型必挂

- 设备：flyme/m22（MEIZU 22，Flyme 12.5.0.0A / Android 16 / QTI `sun`）
- 组件：`engine/build/07_build_vendor`（`replace.go` / `plan.go`）
- 产物：`engine/out/devices/flyme/m22/12.5.0.0A/img/vendor.img`（180,748,288 B）

## 问题

07 的 13 个 HAL 全部编译通过、76 项产物写入装配树后，步骤 5「厂商 vendor 原件覆盖」
直接失败：

```
[err] 阶段 build_vendor 失败: 厂商 vendor 缺少该路径: lib64/egl/libGLES_mali.so
```

m22 是高通 QTI 机型，官方 vendor 里根本没有 Mali/MEOW 这套 GPU 文件——那是
gold（MTK）机型的身份扫描面。

## 根因

`vendorReplaces` 是引擎内置的**单一常量集合**（gold/MTK 口径）：Mali/MEOW 五个 GPU
库 + `libOpenCL.so` + `libpng.so` + `etc/permissions` + `etc/security` + `etc/md`。
集合里除 `Optional` 条目外，缺任一路径即整阶段失败。机型侧没有任何覆盖入口，
于是"换一个厂商就挂"。

## 证据

`log-fail.txt`：

1. 首次失败 `lib64/egl/libGLES_mali.so`（QTI 无 Mali）；
2. 把 `etc/security`、`etc/md` 一并写进机型清单后再跑，改成 `etc/security` 失败——
   用 `dump.erofs --ls --path=/etc` 核官方 m22 vendor，实际只有
   `permissions`（80 个文件）与 `selinux` 两个目录，没有 `security`/`md`；
   lineage 基座 vendor 同样没有这两个目录。

m22 官方 `/etc` 实测（节选）：

```
66062848    2  permissions
66063209    2  selinux
```

## 改动

- `07_build_vendor/plan.go`：`Plan` 新增 `Replaces []replaceEntry`；把原先在
  `buildPlan` 里提前执行的 `validateReplaceSet(vendorReplaces)` 移到集合确定之后。
- `07_build_vendor/replace.go`：新增 `replacesFromManifest`（读基线清单
  `vendor.replaces`，声明即整表取代内置集合、未声明返回 nil 沿用内置、显式空数组
  表示本机型不做覆盖）；`applyVendorReplaces` 与 `replaceInputText` 改用
  `plan.Replaces`。
- 机型清单 `manifest/flyme/m22/baselines/12.5.0.0A.json` 的 `vendor` 段加
  `replaces`：只保留官方确实存在的两项——`lib64/libpng.so`（LLNDK 外，vendor 进程
  取不到 `/system/lib64` 那份）与 `etc/permissions`（整目录替换，清掉 lineage 的
  `*_prebuilt.xml` 幽灵特性）。每条带 `note` 说明取舍。

## 验证

`log-pass.txt`：步骤 5 输出「覆盖集合 2 项」，`libpng.so` 文件覆盖 +
`etc/permissions` 80 个文件整目录替换；随后步骤 6 属性收口、步骤 7 打包，
587 条目（354 文件 / 199 链接 / 34 目录）元数据写入镜像、`resize2fs -M` 收紧到
180,748,288 B，步骤 8 写 `SHA256SUMS`。

`devices/manifest/flyme/m22/baselines/12.5.0.0A.json` 的 `note` 里也记了一条反向约束：
`etc/selinux` **不能**换厂商原件（策略编译输入 `vendor_sepolicy.cil` 与
`plat_pub_versioned.cil` 必须同源，混用会让 init 在 PropertyInit 阶段
`setxattr` 未知类型失败）。

## 遗留

- `libOpenCL.so` 没纳入：它是 GPU 身份面，m22 走 virtio-gpu/MEOW interposer 路线，
  保留基座件不会被 Flyme 侧探测到差异（若 bring-up 出现 OpenCL 探测失败再补）。
- 覆盖集合的正确性靠 `dump.erofs` 人工核对官方分区；没有自动化"清单里每条路径是否
  在厂商 vendor 内存在"的预检（`Optional: true` 可绕过，但静默版更危险）。

## 后续修正（2026-10-28）：摘要行仍打内置表

**现象**：全量 `cdh device build m22` 跑完后，`img/vendor-info.txt` 写
`vendor-replace: 10 条（Mali/MEOW/…）`，但同阶段日志明确是
「覆盖集合: 基线清单 vendor.replaces（2 项）」——实际打包只做了 2 项，摘要却说 10 项。

**根因**：`replaceEntry` 的整表切换只落在 `applyVendorReplaces` / `replaceInputText`，
摘要函数 `replaceInfoText(plan)` 仍直接遍历包级常量 `vendorReplaces`，与
`plan.Replaces` 脱钩。

**改动**：`07_build_vendor/replace.go` 的 `replaceInfoText` 改读 `plan.Replaces`
（空则输出「无」），并在注释里写明"必须读 plan.Replaces，否则会把整机机型写成
gold/MTK 口径"。

**验证**：`go build` 后 `--only 07_build_vendor --refresh`，`vendor-info.txt` 变为
`vendor-replace: 2 条（lib64/libpng.so（替换）、etc/permissions（替换）；来源
out/roms/flyme/m22/12.5.0.0A/super/vendor_a.img，已存在）`；装配树
588 条目（文件 355 / 链接 199 / 目录 34），`vendor.img` 180,752,384 B
（当次 sha256 `e4b26d23fab7`）。随后 09 出 `patch.img` 792 条目（源 790 + 固定 2，
回读 792/792 一致，当次 sha256 `72fe85d2317f`）、11 重跑把 dist 的 `patch.img` 一并刷新。

> `vendor.img` / `patch.img` 的 sha256 只在本条快照内有效：两者都由 `mke2fs -d`
> 生成，superblock 的 mkfs / last-write 时间每次重跑都变，同配方（`vendor-6f0bef2b9afe`
> / `patch-3ebb0511e28c`）也会得到不同字节。核对以当次 `img/SHA256SUMS` 为准。

> 与上文 587 条目 / 180,748,288 B 的差异来自 05 阶段现在提供合入后的
> `fstab.qcom`（`vendor` 装配树多 1 个文件），不是覆盖集合变化。
