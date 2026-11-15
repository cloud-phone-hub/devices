# Issue: 12 台机型清单与 `Template.md` 脱节（README 三种形态、`brand`/`device` 混用、模板自身过期）

- 设备：`gold` 之外的 12 台全部（flyme/m22、google/oriole、nothing/3a、oneplus/15r、oneplus/ace3、
  oppo/cph2797、oppo/pme110、realme/rmx3823、realme/rmx5112、realme/rmx6688、samsung/ss9380、
  xiaomi/popsicle）；改动只落在清单目录与模板/索引，**没有阶段代码改动**
- 阶段：清单层（`manifest/<vendor>/<device>/`、`manifest/Template.md`、`manifest/README.md`）
- 状态：**已闭环**（2026-11-03，静态校验 + 全量单测 + `device list` 加载；未实跑 01→11）
- 关联：[#137 issue-redirect-comments-to-readme](../issue-redirect-comments-to-readme/solution.md)
  （注释串外移到机型 README，本次补的正是那些 README 的骨架）、
  [#138 issue-device-hardware-compact](../issue-device-hardware-compact/solution.md)（`hardware` 形态）、
  [#139 issue-vendor-buildprop-official-source](../issue-vendor-buildprop-official-source/solution.md)、
  [#142 issue-vendor-files-no-builtin](../issue-vendor-files-no-builtin/solution.md)（`builtinFileOps` 删除）、
  [#143 issue-odm-buildprop-baseline](../issue-odm-buildprop-baseline/solution.md)（基线 `odm` 段）

## 一、问题

`Template.md`（「一个新机型该长什么样」）与 gold 样板都在，但 12 台机型的落盘形态与之脱节，
四类偏差叠在一起：

1. **机型 README 三分裂**（模板 §3 要求「目录 / 07 装配顺序 / vendor 文件来源 / 维护约定 /
   已知缺口 / `modules.redirect`」六节）：
   - 5 台只有「规则表 + 原说明」两块（`oppo/cph2797`、`samsung/ss9380`、`oneplus/15r`、
     `xiaomi/popsicle`、`google/oriole`）——#137 只把注释搬出来，没补骨架；
   - 7 台只有旧说明或缺节（`nothing/3a`、`oneplus/ace3`、三台 realme、`oppo/pme110` 连文件都没有）；
   - `flyme/m22` 有**两个**「## 维护约定」节，其中旧那份还写着「未声明 `vendor.files` 沿用内置」
     这种已失效口径（`builtinFileOps` 在 #142 删了）。
2. **品牌键两套写法**：`oneplus/15r`、`oneplus/ace3`、`oppo/pme110` 的 `device.json` 写 `"brand"`，
   模板与其余 10 台写 `"device"`。`internal/device/hardware.go` 有别名，所以构建全绿、只在人读
   与脚本统计时分裂。
3. **模板自身过期**：§3 维护约定第 3 条仍写「引擎内置集合」（2026-11-03 删 `builtinFileOps` 后
   未声明就是空表 + 一行 `[warn]`）；§7 只列 `vendor_sepolicy.cil` / `vendor_file_contexts`
   两个文件名，而 `07_build_vendor/sepolicy.go:54-59` 的 `sepolicyPatches` 支持**四个**
   （`vendor_property_contexts`、`vendor_service_contexts` 无处可查）。
4. **索引过期**：`manifest/README.md` 两处仍写 `manifest/<vendor>/<brand>/`；「已知缺口」三条与
   事实相反——「仅 gold / m22 / cph2797 / ss9380 写了 `hardware`」（实际 7 台）、
   「`vendor/build.prop` 只 3 台有文件」（2026-11-03 已全删，13 台都没有）、
   「8 台 `vendor_a.img` 已就位」（`out/roms/` 是产物目录，本机已无解包树）。

另外模板 §3 骨架里的相对链接（`../../../modules/…`、`../../../../AGENTS.md`）是**以机型目录为
基准**写的，留在 `Template.md` 里点开全是 404——抄写者不知道，校验脚本也会误报。

## 二、根因

- **模板是单点产物，README 不进任何校验**：#137/#138/#139/#141~#143 一批改动陆续落在
  `device.json` 与基线 JSON 上，README 只在当时被顺手补几行；没有构建阶段读它，也没有脚本
  断言「这六节必须存在」，于是各台按各自理解写，长出三种形态。
- **文档与代码各演各的**：`builtinFileOps` 删除、`sepolicyPatches` 扩到四类都是代码侧变化，
  模板/索引没有守卫（既无测试也无检查器），过期表述不会被任何东西拦住。
- **键名靠别名兜住**：`device`/`brand` 双写不影响产物，就没有人统一；这与 #134「数组兜底读法
  掩盖写残数据」是同一类问题——**能跑**不等于**口径一致**。
- **相对链接的基准模糊**：骨架文本被设计成「抄过去就能用」，但没人写明它的解析基准。

## 三、证据

1. **改前形态**（`log-fail.txt` 第 1 节）：13 台 README 的节标题清单与行数，5 台无 §3 骨架、
   `m22` 两个「维护约定」。
2. **`brand` 键残留**（第 2 节）：`oneplus/15r`、`oneplus/ace3`、`oppo/pme110` 改前写 `brand`；
   改后 13 份 `brand` 键 0 处。
3. **模板过期**（第 3 节）：Template §3 / §7 改前原文；`grep -rn builtinFileOps` 全仓 0 处；
   `sepolicy.go:54-59` 四个文件名。
4. **索引过期**（第 4 节）：`manifest/README.md` 第 3 / 98 行 `<brand>`、第 210~212 行
   「仅 4 台有 hardware」、第 222 行「build.prop 只 3 台有文件」。
5. **现状矩阵**（第 5 节）：13 台的 `hardware` 项数 / `hardware-platform` / 规则条数 /
   `modules.input` / 基线份数 / `odm` 段 / `vendor.files` 条数 / 静态树件数，是本次写
   「对齐结果」表的原始数据。

## 四、改动

1. **12 台 README 按 §3 骨架重写或补节**（`gold` 补「已知缺口」）：每台五节齐全 + 保留原
   「原说明」`text` 块。差异化的部分逐台写实，不抄别机结论：
   - `flyme/m22`：`odm_a.img:/etc/build.prop` 已进基线（sets/unsets 为空，官方原件本身满足 ABI），
     删掉静态抄件；6 份 `media_codecs*.xml` 改 `from: official` 直取、`c2` 增量留静态树；
     合并两个「维护约定」，旧节里仍有效的两条（`vendor.files` 现覆盖 `lib64/libpng.so` +
     `etc/permissions`、`etc/selinux` 不入）并入新节。
   - `oppo/cph2797`：`etc/compat` 两件（`renderengine_ext_unavailable.so`、`vintf_empty.xml`）
     与 `init.virtio_compat.rc` 的来源/增量写清。
   - `samsung/ss9380`：8 份 `<模块>_config.xml`、6 份 `audio_effects*`（其中 3 份与
     `vendor.files` 同路径 → 步骤 5 覆盖，标为「留档」）、`ueventd.qcom.rc`、两件 sepolicy。
   - `oneplus/15r`：结论是 **odm 件不可迁**——要改的是**顶层** `/odm/build.prop`，而基线
     `odm` 段落点固定 `odm/etc/build.prop`，且本机 `/odm` 整棵重定向，静态副本即唯一真值。
   - `xiaomi/popsicle`：`/odm` 整棵重定向（`file /odm /odm` 护栏必须排在 `dir /odm → …` 之前，
     否则 `failed to realpath(/odm)` → `InitFatalReboot`）。
   - `google/oriole`：四份基线是**四条版本线**（ap2a / bp1a / cp1a / cp3a），只有 ap2a 声明了
     `vendor.files`；切版本 = 换基线 + 改 `rom.lineage.version`，不是四选一。
   - `oppo/pme110`：`device.json` 连 `modules` 段都没有 → 05 不注入 `cdh_redirect.ko`，
     规则表在 guest 侧不生效；README 只给补齐路径（先实测 `/odm` 布局），不臆造规则。
   - `nothing/3a`、`oneplus/ace3`、三台 realme：静态树为空、`vendor.files` 未声明、
     `hardware` 空，README 写「本机没有」并给出首次构建时的取件范本（指向 m22 的写法）。
2. **`brand` → `device`**：`oneplus/15r`、`oneplus/ace3`、`oppo/pme110` 三份 `device.json` 改键
   （`internal/device/hardware.go` 保留别名，旧清单仍可读）。
3. **`Template.md`**：§3 前言补「骨架内相对链接以机型目录为基准」；§3 维护约定第 3 条改为
   「未声明 `vendor.files` 什么都不做（无内置集合，只打 `[warn]`）」+ 新增「静态树与
   `vendor.files` 同路径」判据（举 ss9380 的三份 `audio_effects*` 为「留档」实例）；
   §7 改为四文件表格（文件名 / 追加到装配树 / marker 注释前缀）。
4. **`manifest/README.md`**：两处 `manifest/<vendor>/<brand>/` → `<device>`、「12 台补齐结果」
   表头 `brand` → `device`；新增「13 台按 [`Template.md`](Template.md) 对齐结果（2026-11-03）」
   表（README §3 / `device.json` 键 / `odm` 段 / `vendor.files` / 静态树件数 / 主要缺口）；
   「已知缺口」重写三条（7 台 `hardware` 的实际项数、pme110 无 `modules` 段、`out/roms/` 是
   产物目录、`vendor/build.prop` 13 台全无）。

## 五、验证

`log-pass.txt` 四节，均可复跑：

1. **JSON 全解析**：13 份 `device.json` + 17 份 `baselines/*.json` 全部 `json.load` 通过，
   `brand` 键残留 **0**。
2. **骨架齐全度**：13 台 README 六节（目录 / 07 装配顺序 / vendor 文件来源 / 维护约定 /
   已知缺口 / modules.redirect）**缺节 0**。
3. **相对链接**：13 台 README + `manifest/README.md` 的 `../` 链接逐个 `os.path.exists`，
   坏链 **0**（`Template.md` 骨架内那条 `../../../modules/…` 是抄写基准示例，已在 §3 前言注明）。
4. **代码与加载**：`go build ./...`、`go vet ./internal/device ./07_build_vendor` 干净；
   `go test -count=1 ./...` 全绿（含 `07_build_vendor` 1.875s、`internal/manifest` 2.535s）；
   `go run . device list` 13 台 / 17 份基线全部「就位」。

## 六、遗留

- **未实跑构建链**：`out/roms/` 无解包树（产物目录，不入库），所以 06/07 均未实跑；
  m22 删抄件后「06 只取原件 + 校验 ABI」这条路径只经过静态阅读与单测，未在真机构建里复验。
- **10 台 `odm` 段待迁**（cph2797 / ss9380 / popsicle / oriole / 3a / ace3 /
  rmx3823 / rmx5112 / rmx6688 / pme110）：静态树里还有 `odm/etc/build.prop` 抄件，迁移前必须 `dump.erofs --ls/--cat`
  实测「哪个镜像、镜像内哪个路径」，并补 abilist sets（漏写会让 06 的 ABI 校验直接失败）。
  `oneplus/15r` 不迁（顶层件 + `/odm` 整棵重定向），已在 README 写死结论。
- **6 台 `hardware` 为空**（3a / ace3 / pme110 / rmx3823 / rmx5112 / rmx6688）、
  **pme110 无 `modules` 段**：属 HAL 面与 redirect 规则待补，不是文档问题。
- **文档与代码的一致性仍无守卫**：建议后续加一条检查（测试或 `cdh` 子命令）断言
  「`Template.md` §7 的文件名集合 == `sepolicyPatches`」「§1 的目录清单 == 检查器的白名单」，
  否则下一次改代码仍会把模板再漂一次。
