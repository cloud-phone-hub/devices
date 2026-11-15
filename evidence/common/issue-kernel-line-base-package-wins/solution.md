# Issue: 三台机型的顶层 `kernel` 抄了厂商 uname 主次，02 据此拼出不存在的内核树

- 设备：`flyme/m22`、`samsung/ss9380`、`google/oriole`（ap2a 基线）——同一口径错在三份基线；
  `xiaomi/gold` 是同代特例，本轮实跑验证落在 m22
- 阶段：`02_build_kernel`（`resolveSource` / `kbuild.KernelTag`）与 `internal/manifest.Kernel()` 的填写口径
- 状态：**已闭环**（2026-11-04，m22 与 oriole 各实跑 02→11 全绿；ss9380 同口径已改、待轮到时代跑）
- 关联：[#46 issue-m22-kernel-source-6.12](../../m22/issue-m22-kernel-source-6.12/solution.md)
  （m22 该用哪棵树，旧流水线已定）、
  [#146 issue-in-tree-extras-stale-cdh-virt-wifi](../../gold/issue-in-tree-extras-stale-cdh-virt-wifi/solution.md)
  （同一轮逐机验收里 gold 撞上的另一件清单过期）

## 一、问题

逐机验收第 2 台 `flyme/m22` 全量构建，01 unpack 正常导出 16 个分区，进到
`[2/11] build_kernel` 直接失败：

```
==> 内核线: 6.6（清单 kernel；与厂商固件内核同代）
[err] 阶段 build_kernel 失败: 无法确定内核源码：请用 --source 或 CDH_KERNEL_SOURCE 指定
     （kernels/ 下可用: kernels/lineage-21-6.1, kernels/lineage-22-6.12,
       kernels/lineage-22-6.6, kernels/lineage-23-6.12）
```

`kernels/` 下明明有 `lineage-23-6.12`，而 m22 的基线写的就是 `lineage-23.2` 底包——02 却在找
一条 6.6 的 23 代树。同一份基线的 `rom.lineage.kernel.measured-from` 甚至自己写着
「云机跑底包内核（6.12.81），官方 ROM 内核是 6.6.66-android15，两者不同代，**镜像标签与源码
目录名跟底包走**」，机型 README 的 `02_build_kernel` 一行也早就记成
`源码 kernels/lineage-23-6.12 … release 6.12.81-4k-cdh`。字段与它自己的文档互相矛盾。

按同一口径静态推演，本轮 7 台里还有两台会撞同一面墙：`samsung/ss9380`（`kernel: 6.6`，
底包 6.12.81）与 `google/oriole` 的 ap2a 基线（`kernel: 5.10`，底包 6.1）。

## 二、根因

`02` 的树名是 `kbuild.KernelTag(lineage 代, mf.Kernel())` 拼出来的
（`02_build_kernel/plan.go:335`），而 `mf.Kernel()` 的优先级是
**顶层 `kernel` > `rom.lineage.kernel.version` > … > `rom.vendor.kernel.uname-version`**
（`internal/manifest/manifest.go:477~492`）。顶层字段一旦写错，底包实测值就被压过去：

| 机型 | 顶层 `kernel`（改前） | 底包 `rom.lineage.kernel.version` | 厂商 `rom.vendor.kernel.uname-version` | 推导树名 | 存在 |
| :--- | :--- | :--- | :--- | :--- | :--- |
| m22 | `6.6` | `6.12.81` | `6.6.66-android15-…-4k` | `lineage-23-6.6` | ✗ |
| ss9380 | `6.6` | `6.12.81` | `6.6.98-a…` | `lineage-23-6.6` | ✗ |
| oriole（ap2a） | `5.10` | `6.1` | `5.10.1…` | `lineage-21-5.10` | ✗ |
| gold | `6.6` | `6.6` | `6.6.89…` | `lineage-22-6.6` | ✓ |
| 15r / cph2797 / popsicle | `6.12` | `6.12.81`（15r/popsicle 未写） | `6.12.38` / `6.12.23` | `lineage-23-6.12` | ✓ |

树名拼不出来时 `resolveSource` 会退到「`kernels/` 下唯一可用内核」——四棵树都在，
不唯一，于是报「请用 `--source`」。这个报错本身是对的（宁缺不猜），
**错的是数据与写数据的口径**：

`Kernel()` 的注释把「抄 `rom.vendor.kernel.uname-version` 的主次」当成通则
（原文见 [log-fail.txt](log-fail.txt) 末节）。但厂商线与底包线**不同代**时，云机跑的是底包那颗：
`boot.img` 由 04 用 02 的 `Image` 重打、`vendor_dlkm` 与各 `.ko` 也按底包内核配
（m22 / ss9380 的 `rom.lineage.images` 里就有 `vendor_dlkm_a.img`，官方那颗根本不用）。
gold 之所以看起来「抄厂商线也对」，纯粹因为它两代同。注释最后一句还把借用登记的对象说反了——
`verifyKernelBase` 比对的是**编出来的 release 与底包 `boot_a.img`**
（`plan.go:527~546`），跟厂商 uname 无关。

## 三、证据

- 失败原文（含三台静态推演对照、`kernels/` 与 `out/kernel/` 实际清单、三台 README 自证）：
  [log-fail.txt](log-fail.txt)
- 通过原文（m22 改后 02 的选树、底包同代校验、配方与起编）：[log-pass.txt](log-pass.txt)
- 判定链代码：`02_build_kernel/plan.go:198~212`（打印内核线并 `resolveSource`）、
  `plan.go:335~356`（树名推导 + 不唯一即报错）、`plan.go:527~546`（底包同代校验 / 借用登记）；
  `internal/manifest/manifest.go:477~507`（`Kernel()` 优先级与 `KernelSource()` 标注）。

## 四、改动

**改数据 + 改口径说明，不改 02 的行为**（它的报错与「宁缺不猜」是对的）：

1. 三份基线顶层 `kernel` 按「云机实际跑的那条线」改正：
   - `manifest/flyme/m22/baselines/12.5.0.0A.json`：`6.6` → **`6.12`**
   - `manifest/samsung/ss9380/baselines/SM-S9380_3_20260813083211_652iw49crl_fac.json`：`6.6` → **`6.12`**
   - `manifest/google/oriole/baselines/oriole-ap2a.240905.003.f1-factory-655c44e7.json`：`5.10` → **`6.1`**
2. 重写 `internal/manifest/manifest.go` 里 `Kernel()` 的口径注释：顶层 `kernel` =
   「02 要编、guest 要 boot 的那条线」，取值必须能拼出真实存在的
   `kernels/lineage-<代>-<主次>`；**厂商与底包不同代时以底包为准**，并把三台的实测值写成反例；
   借用登记的正确对象改为「编出来的 release 与底包不同代」。
3. 本轮不动 `15r` / `cph2797` / `popsicle`（已是 `6.12`）、不动 gold（同代，抄谁都一样）。

## 五、验证

m22 改后重跑 `cdh device build m22`（`engine/out/tmp/m22-build.log` 第 187 行起）：

```
==> 内核线: 6.12（清单 kernel；厂商固件内核 6.6，不同代）
==> 内核源码: kernels/lineage-23-6.12（清单推导 lineage-23-6.12）
==> 内核基线: 底包 6.12.81-4k-g4f6bf47200d9（out/roms/lineage/lineage-23.2/boot_a.img），与本机 6.12.81-4k-cdh 同代
    输入: 缓存配方 lineage-23-6.12-cdh-0531d3a1f739（输入哈希）
```

02 不再要求 `--source`，且 `[步骤 4] 容器内编译内核` 正常起编（编译结果与后续 03→11 见
[log-pass.txt](log-pass.txt)）。口径注释改动过 `go test ./internal/manifest`（`ok 1.043s`）。

第 3 台 `google/oriole`（ap2a 基线，`5.10` → `6.1`）同轮实跑，走的是**借用分支**——
底包 `lineage-21.0` 自带 6.12.23，而 virtio 源码线是 6.1.138：

```
==> 内核线: 6.1（清单 kernel；厂商固件内核 5.10，不同代）
==> 内核源码: kernels/lineage-21-6.1（清单推导 lineage-21-6.1）
==> 内核借用: lineage-21（底包 out/roms/lineage/lineage-21.0/boot_a.img 是 6.12.23-4k-gafb0ad713e3c-dirty，本机 6.1.138-4k-cdh）
[ok] 内核构建完成: out/kernel/lineage-21-6.1
[ok] 模块构建完成: out/kernel/lineage-21-6.1/modules（10 个 .ko）
[ok] 已完成
    产物: out/dist/google/oriole/AP2A.240905.003.F1（vda.img 13.00 GiB，5 个分区已刷新）
```

改前它同样会在 02 报「无法确定内核源码」（推导名 `lineage-21-5.10`）。
`samsung/ss9380`（`6.6` → `6.12`）**未实跑**，只做了静态一致性核对（基线
`rom.lineage.kernel.version` + 机型 README 的「lineage-23-6.12 内核树」+ `kernels/` 实际树名
三者一致），按逐机验收顺序轮到它时再复核。

## 六、遗留

- **`resolveSource` 的报错文案不指路**：它只列「`kernels/` 下可用」四棵树，不说
  「顶层 `kernel` 与 `rom.lineage.kernel.version` 不同代」这个最可能的原因。本轮靠人读
  基线才发现。要不要在推导失败时把「lineage 代 + 顶层 kernel + 底包实测 + 厂商实测」一起打出来，
  等 7 台跑完再定（改文案要同时补用例）。
- **m22 README 记的配方 hash 与新引擎不同**：README 写 `lineage-23-6.12-cdh-b4fe1c987b3a`，
  本轮实跑是 `lineage-23-6.12-cdh-0531d3a1f739`（旧流水线口径 + 补丁/片段变过）。
  本轮只改 `kernel` 字段，README 的阶段表未回写；跑完整机后按实据更新。
- **其余 9 份基线没逐台核**：`3a` / `ace3` / 三台 `realme` / `pme110` 与 oriole 另外三份基线
  都没写顶层 `kernel`（走回退链，取 `rom.lineage.kernel.version`，实测值多为空 → 02 会报
  「无法推导」）。它们不在本轮 7 台范围内，等纳入验收时按同一口径补声明。
- **同代时抄谁看不出来**：gold 两代相同，写厂商线或底包线结果一致，所以「抄厂商 uname」这个
  错口径在同代机型上永远不会暴露。若要在构建期强制校验，可让 02 在 `Kernel()` 取的是厂商线
  且与底包不同代时直接 `[err]`——本轮没做，避免把「借用厂商线」的合法场景一起堵死。
