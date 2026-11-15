# Issue: 07 装配树的 `vendor/build.prop` 以机型静态树副本为基底，厂商原件只当"留档"

- 设备：全仓 13 台机型（机制改动落在 07，gold 实跑验证）
- 阶段：`07_build_vendor` 步骤 4（静态树叠加）/ 步骤 6（`props.go`）
- 状态：**已闭环**（2026-11-03，gold `--only 07_build_vendor` 实跑）
- 关联：[#134 issue-vendor-build-prop-object-only](../issue-vendor-build-prop-object-only/solution.md)（同一文件另一侧的口径：属性表只认对象）

## 一、问题

`devices/manifest/README.md` 当时规定每台机型要有 `manifest/<vendor>/<device>/vendor/build.prop`
（这条已在本次收尾中改为「不放」），内容是厂商 `super/vendor_a.img:/build.prop` 的逐字节副本。
但 07 读它时是**当基底**用的：

1. 步骤 4 把 `manifest/<vendor>/<device>/vendor/**` 整棵叠加到装配树，而
   `allowedVendorFiles = {build.prop, pci.ids}` 明确放行 `build.prop`
   → 静态树副本**直接覆盖**了步骤 2 从 lineage 镜像带出的那份；
2. 步骤 6 的 `applyVendorBuildProps` 再按基线 `vendor.build-prop-sets` / `build-prop-unsets`
   在这份副本上打补丁。

于是「厂商原件」与「云机基底」是同一个文件，而它同时又是人工 `dump.erofs --cat` 摘出来 `cp`
过去的——**没有任何环节阻止有人在里面就地改属性**，改完即视为收口起点，台账（基线）里查不到。

gold 就是这么漂的：本目录副本 774 行 / md5 `c44f5ee2…`，厂商原件 767 行 / md5 `a23c8617…`，
差的 4 条属性与 6 行 `# cdh:` 注释一条都不在基线 `build-prop-sets` 里（见 `log-fail.txt`）。
后果有两层：① 换机重做 01 后新摘的原件一覆盖，这些改动**静默消失**（abilist32 变回非空 →
framework 硬连不存在的 32 位 zygote）；② 反过来若副本被当成"厂商原值"对账，
`ro.vendor.product.cpu.abilist` 的官方取值就再也查不到了。

## 二、根因

07 从来没有"厂商属性真值"这一概念，只有"装配树里当前那份文件"。静态树副本承担了两个互斥的
角色——**厂商原件留档**与**装配基底**——而这两个角色的更新节奏完全相反：留档要冻结在出厂那一刻，
基底要每次重建。把两者放在同一个路径上，就只能靠人不去改它，约束写进 README 也拦不住构建。

真正需要的真值有两份且必须分开：厂商原值（从 `vendor_a.img` 现解）+ 云机增量（基线 sets/unsets）。
装配树 = 原件 ⊕ 增量，这个等式得让代码每次重算，而不是靠"上次谁手改过"。

## 三、证据

1. **漂移实测**（`log-fail.txt` 第 1 节，`diff --unified=1` 原件 vs 旧副本）：
   `ro.vendor.product.cpu.abilist` 由 `arm64-v8a,armeabi-v7a,armeabi` 收窄成 `arm64-v8a`、
   `abilist32` 由 `armeabi-v7a,armeabi` 清空、`ro.hwui.use_vulkan` true→false、
   追加 `ro.hw_timeout_multiplier=10` 与 6 行说明（含空行）。
2. **台账无记录**（`log-fail.txt` 第 2 节）：变更前的基线 `build-prop-sets` 15 条、`build-prop-unsets`
   1 条，这 5 个键**一条都没有**；`vendor.note` 也不存在。
3. **路径确实会覆盖装配树**：`assemble.go` 的 `allowedVendorFiles` 含 `build.prop`，
   步骤 4 的静态树整棵叠加在步骤 2 之后、步骤 6 之前，所以副本必然是 `applyVendorBuildProps`
   的输入。
4. **旁证（历史踩坑）**：`../gold/issue-vendor-buildprop-props` 与
   `../gold/issue-zygote-secondary-missing` 记录过同一条链路——abilist32 非空导致
   `bootCompleted` 强连 32 位 zygote 起不来，当时的修法就是"直接改 manifest 里那份 build.prop"。

## 四、改动

**07（`build/07_build_vendor`）**

- `props.go` 新增 `placeVendorBuildProp`，作为步骤 6 的**前半**（在 `applyVendorBuildProps` 之前）：
  - 用 `officialFileSource(plan)`（厂商树 + `plan.OfficialVendorImg`）走 `extractSource` 解
    `/build.prop`（EROFS 走 `dump.erofs`、ext4 走 `debugfs`，与步骤 5 同一套抽件逻辑）；
  - 先落 `<厂商解包根>/super/unpack_vendor/build.prop`（解包件留档，可直接人工对账），
    再落到装配树 `<版本目录>/vendor/build.prop`；
  - 打印三行：来源（厂商原件，`N 行 / sha256 xxxxxxxx`）、解包落点、装配落点。
- 两个例外仍回退静态树副本，并各打一条 `[warn]`：未提供基线清单时（副本常带手改的 `ro.zygote`，
  换成纯原件会让 init 去拉不存在的 `app_process32`）、厂商 vendor 镜像不在位时；
  两处都没有则只告警、不写文件。
- 新增 `warnBuildPropDrift`：静态树副本与厂商原件不一致时打两条 `[warn]`，提示把改动移进
  基线 `build-prop-sets` / `build-prop-unsets`，**不阻断**构建（副本只作核对基准）。
- 辅助 `copyVendorBuildProp` / `buildPropSummary`（内容与权限相同则跳过，保持幂等）。
- `assemble.go` 在 `applyVendorBuildProps` 前调用 `placeVendorBuildProp`；
- `plan.go` 的 `dryRunCommands` 增步骤 6 现解行、`describe` 增 `vendor build.prop:` 行；两者判据与
  运行态统一为 `stagekit.IsFile`（镜像与副本都不在位时明说“保留步骤 2 那份”）。
- `props_test.go` 新增 4 个用例：回退静态树、双缺只告警、漂移告警（一致时静默）、摘要格式。

**机型层（gold，样板）**

- 那 5 条本地改动迁进基线 `build-prop-sets`（共 20 条），因果写进新加的 `vendor.note`
  （abilist32 置空、`ro.hwui.use_vulkan=false`、`ro.hw_timeout_multiplier=10` 各自的证据路径；
  gold 是唯一有实跑证据的机型，故不做跨机型批量迁移）。
- `manifest/xiaomi/gold/vendor/build.prop` 先用原件覆盖回 767 行 `a23c8617…`；确认 07 不再读它后，
  **按新口径直接删除**（同日删除 ss9380 424 行 `df0c82b0…`、m22 324 行 `4a012482…`，
  其余 10 台本来就没有），机型目录从此不放这份抄件。

**文档**：`manifest/README.md`（约束节改为「机型目录**不放** `vendor/build.prop`」+「已删除的副本」
表 + Why/How）、`manifest/Template.md`（骨架去掉该文件、第 2 步改为“不放”、第 6 节与自检表同步）、
`manifest/xiaomi/gold/README.md`（小节改为“本目录不放/已删除”）、
`build/07_build_vendor/README.md`（步骤总览 + 收尾注入 4 项 + 回退分支改口径），
以及检查器 `out/tmp/check_manifest_template.py` 删掉该检查。

## 五、验证

`cdh device build gold --only 07_build_vendor`，rc=0，日志 `out/tmp/gold-07-buildprop.log`
（484 行，`[ok] 已完成`，全文无 `[warn]`）。详见同目录 `log-pass.txt`。

1. **日志**（371~392 行）：`==> [步骤 6] 厂商 build.prop 解包 -> 装配树` + 来源（767 行 /
   sha256 `12d7e632`）/ 解包落点 / 装配落点三行，随后 `applyVendorBuildProps` 打出 17 条
   `[prop]`（`abilist64` 因原件同值而跳过，说明收口幂等）。
2. **三层对账**：厂商解包件 767 行 `a23c8617…`（= 原件）→ 装配树 776 行 `d9ea34b4…`
   → 镜像内 `debugfs -R 'cat /build.prop'` 776 行 `d9ea34b4…`，`diff` 为空。
3. **增量就是台账那一份**：`diff 原件 装配树` 恰好是基线 `build-prop-sets` / `build-prop-unsets`
   的 14 处（abilist / abilist32 / `ro.hwui.use_vulkan` / `ro.hw_timeout_multiplier` / `ro.zygote` /
   `ro.hardware.hwcomposer` / `persist.system.powerhal.applist_enable` / `ro.frp.pst` 删除 +
   9 条 SKU/`ro.soc.*`），没有一条来自副本手改。
4. **行为不回退**：装配树 `ro.vendor.product.cpu.abilist=arm64-v8a`、`abilist32=`（空）、
   `ro.hwui.use_vulkan=false`、`ro.hw_timeout_multiplier=10`、`ro.zygote=zygote64`，
   与变更前出货镜像一致。
5. **静态树副本合规**：`manifest/xiaomi/gold/vendor/build.prop` 767 行 `a23c8617…`，与原件逐字节一致
   → 07 的漂移核对静默（`TestWarnBuildPropDrift` 另有用例覆盖"不一致时告警"方向）。
6. **静态检查**：`gofmt -l ./07_build_vendor` 无输出、`go vet` 通过、
   `go test ./07_build_vendor -count=1` → `ok … 13.986s`（含本次新增 4 个用例）。

## 六、遗留

- **09 / 11 未重跑**：本轮只出了 `img/vendor.img`，`patch.img` 与发布盘集仍是上一轮，
  需按依赖链 `07 → 09 → 11` 出镜像后才算整链闭合。
- **其余 12 台机型未重跑 07**：机制对所有机型生效，回退链保证旧清单仍能构建；但各机
  基线是否也有"副本里手改、台账无记录"的情况未逐一核对（核对手法见 `log-fail.txt` 第 1、2 节，
  可对每台 `diff` 原件与副本后逐键比对 `build-prop-sets`）。
- **`vendor.note` 仍未纳入检查器**：检查器删掉副本核对后不再看这一节；某条 `build-prop-sets`
  缺因果只能人工看，而 `[warn]` 本身不阻断构建、靠日志发现。
- **模板曾把副本列为必需（已改）**：`manifest/Template.md` 与 `README.md` 现已改为「机型目录
  **不放** `build.prop`」，检查器也删了这条检查；若日后有人重新放回副本，07 仍会比对并告警，
  但需要人收尾删除。
- **`unpack_vendor/build.prop` 是每轮覆盖的留档**：不参与配方缓存，删掉不影响复现
  （下一轮 07 重新解）。

