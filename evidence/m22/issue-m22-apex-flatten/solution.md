# 07 步骤 2b：底包 vendor APEX 摊平（Android 16 图形栈不再以散件发布）

- 设备：flyme/m22（MEIZU 22，QTI `sun`），基底 lineage-23.2（Android 16）
- 日期：2026-10-29
- 关联：[issue-m22-hal-exec-label](../issue-m22-hal-exec-label/solution.md)（#55，标签只认
  `/vendor/bin/hw/`）、[issue-m22-hal-gap-fill](../issue-m22-hal-gap-fill/solution.md)（#53，
  HAL 缺口补齐）、[issue-vendor-apex-health](../../gold/issue-vendor-apex-health/solution.md)（#30，
  另一条与 `/vendor/apex` 相关的早期条目）

## 1. 问题

m22 换到 lineage-23.2 基底后，图形显示栈在装配树里完全没有来源：

- 底包 `super/unpack_vendor/bin/hw` 只剩 2 个文件（`android.hardware.bluetooth-service.default`、
  `android.hardware.security.keymint-service`），lineage-22.0 时代的
  `android.hardware.composer.hwc3-service.drm`、`android.hardware.graphics.allocator-service.minigbm`
  已经不在散件里；
- 07 步骤 2 的白名单只放 `bin/etc/lib/lib64/firmware_mnt` 与 `build.prop`/`pci.ids`，
  `apex` 顶层被整棵丢弃，装配树里既没有图形散件、也没有 APEX 内容；
- 于是 m22 的 hwc3 / allocator 只能靠机型静态树（`manifest/flyme/m22/vendor`）手工落的
  lineage-22.0 旧件顶上——那是「借件」，不是基座自带的图形栈，换任何一个 Android 16
  厂商 ROM（把图形栈放进 vendor APEX 的）都会重复踩同一个坑。

需求因此不是「给 m22 补两个文件」，而是：**07 要能自己判断基座把 HAL 放进了
vendor/apex，并把选中的 APEX 摊平回 vendor 根下的散件形态**。

## 2. 根因

1. **发布形态变了**：Android 16 起 lineage（与公开的厂商 GSI/vendor）把 HAL、图形栈从
   「vendor 根下的散件」改成 vendor APEX。运行时 `/apex/<name>/bin/hw/`、`/apex/<name>/etc/`，
   `vendor/bin/hw` 自然就空了。
2. **装配模型没跟上**：07 的步骤 2 是「按白名单从解包树铺底」，`apex` 不在白名单里
   （`assemble.go:allowedVendorDirs`，以及 `unpackbase.go` 里写明「apex…留在解包目录不回填」）。
   白名单当年是对的：把整个 APEX 目录搬进 vendor 也没用，init 不会去 `/vendor/apex/<名>/`
   下 exec。
3. **摊平必须同时改 rc**：APEX 的 rc 写死 `/apex/<挂载名>/bin/hw/xxx`。挂载名取
   `apex_manifest` 的 name（`com.android.hardware.graphics.composer`），**不等于** APEX 文件名
   （`com.android.hardware.graphics.composer.drm_hwcomposer_upstream.apex`），所以不能按
   文件名推导前缀。不改写的后果两条：init 永远 exec 不到；步骤 6 的
   `ensureHalExecLabels` 只认 `/vendor/bin/hw/`（#55），也补不上 `hal_*_exec` 标签，
   进镜像后全是 `vendor_file` → `entrypoint` 被拒 → `status 127`。
4. **落位还有两处细节**：rc 可能放在 `etc` 根（`<名>.apex.rc` 或 `<名>.rc`），init 只扫
   `etc/init`；VINTF 片段可能放在 `etc/vintf` 根，而 daemon/系统读的是
   `etc/vintf/manifest`。

## 3. 证据

- `log-fail.txt`（改动前）：23.2 底包 `bin/hw` 只剩 2 项、`apex` 下 12 个 APEX 的完整清单、
  步骤 2 日志的 `丢弃 apex / …`、两个图形 APEX 解出的完整文件树、payload 的 `file` 结果
  （ext2/ext4，UUID 同源）、rc 里 `/apex/…` 原文与 `apex_manifest.pb` 的挂载名。
- `log-pass.txt`（改动后）：单测/vet/build 通过、步骤 0 摘要
  （`vendor APEX: graphics（预计摊平 2/12…）`）、步骤 2b 实跑日志（新增 10 / 覆盖 38 /
  跳过 4，rc 改写 2 文件 2 行）、产物 rc 全文与 sha256、`vendor.img` 内 `debugfs ls` 与
  `ea_get -V` 的 SELinux 标签、`vendor_file_contexts` 命中的三条规则。

## 4. 改动

1. 新增 `engine/build/07_build_vendor/apex.go`（步骤 2b），**不硬编码 lineage-23.2**：
   - 选择走模式：`graphics`（默认）/ `all` / `none` / 逗号分隔 glob（大小写不敏感）。
     `graphics` 的特征是文件名子串 `graphics.composer`、`graphics.allocator`、`gralloc`、
     `mapper.`、`display.composer`——换厂商 ROM 只要特征还在就能命中，不在就 `--apex-flatten all`
     或给 glob；
   - payload 按 magic 自动分流：ext4（偏移 1080 `0xEF53`）走 `debugfs -R "rdump /"`，
     erofs（偏移 1024 `0xE0F5E1E2`）走 `fsck.erofs --extract=`，不写死文件系统类型；
   - 路径映射（`apexTargetPath`）：`bin/lib/lib64/usr` 原样；`etc/init/*.rc` 原样；
     `etc/<名>[.apex].rc` → `etc/init/<名>.rc`；`etc/vintf/*.xml` →
     `etc/vintf/manifest/*.xml`；`lost+found`、`apex_manifest.pb`、`apex_pubkey`、
     `apex_build_info.pb` 丢弃；
   - rc 就地改写：正则 `/apex/[^/\s"]+/?` → `/vendor/`（不依赖文件名）；
   - 解包现场留 `super/apex_unpack/<apex 文件名>/root/`，便于排查。
2. `plan.go`：`Options`/`Plan` 增 `ApexFlatten`，新增 flag `-apex-flatten`（默认 `graphics`），
   `--dry-run` 摘要多一行 `vendor APEX: …` 与 `[步骤 2b]` 计划命令；
   `ComputeVendorRecipe` 拼入 ` apex:<mode>`——换模式重算配方，不误命中旧缓存。
3. `execute.go`：步骤 2 之后、步骤 3 之前调用 `materializeVendorApex`。位置很关键：摊平属于
   「基底还原」，模块产物（3/4）、机型静态树、厂商覆盖（5）都是后层，仍可覆盖摊平结果。
4. 新增 `engine/build/07_build_vendor/apex_test.go`：模式选择、路径映射（含 gralloc 无 `.apex`
   中缀的 rc）、rc 前缀改写（挂载名 ≠ 文件名、无 `/apex` 前缀不改）、整树落位（4 写 /
   2 跳过 / rc 1 文件 1 行 / 幂等重跑记 Overwrote）。
5. 撤掉机型静态树里 lineage-22.0 的 11 个图形件（`manifest/flyme/m22/vendor`）：
   `bin/hw/{android.hardware.composer.hwc3-service.drm, android.hardware.graphics.allocator-service.minigbm}`、
   `etc/init/{hwc3-drm.rc, allocator.rc}`、
   `etc/vintf/manifest/{hwc3-drm.xml, allocator.xml, mapper.minigbm.xml}`、
   `lib64/hw/{gralloc.minigbm.so, mapper.minigbm.so}`、
   `lib64/{libminigbm_gralloc.so, libminigbm_gralloc4_utils.so}`。撤件原因：两套并存时
   `allocator.xml` 与 `allocator.minigbm_upstream.xml` 都声明 `IAllocator/default`（同版本 @2）、
   两条 rc 也各起一个 composer 服务，属 #35 那类会让整份 device manifest 解析失败的形态。
   撤件后 mapper 的 native 声明由底包自带的 `mapper.minigbm_upstream.xml`
   （`@5.0/minigbm_upstream`）承接，不空缺。
6. 第二批撤件：静态树里另有 7 个 lineage-22.0 的 composer/HIDL 依赖库（`lib64/`），
   文件名在 23.2 基座里**不存在**、内容与 22.0 基座逐字节一致，即当初为 22.0 的
   `hwc3-service.drm` 借来的依赖：
   `android.hardware.graphics.composer@2.1.so`（`da42d70b…`）、`@2.1-resources.so`（`e566b76d…`）、
   `@2.2.so`（`ee8b4814…`）、`@2.2-resources.so`（`b5f58ca1…`）、
   `android.hardware.graphics.composer3-V4-ndk.so`（`7f5f2f69…`）、
   `android.hardware.graphics.common-V6-ndk.so`（`d14f5667…`）、
   `android.hardware.drm.common-V1-ndk.so`（`de7d1a53…`）。
   撤件原因不只是「多余」：**步骤 4 的机型静态树是步骤 2b 的后层**，两个同名库会
   覆盖摊平结果——产物里 `composer3-V4-ndk.so` 实际是 22.0 的 `7f5f2f69…` 而不是 23.2
   APEX 的 `bc002b73…`，`drm.common-V1-ndk.so` 同理（`de7d1a53…` vs `3709aab3…`），
   且 `graphics.common` 出现 V6（22.0）与 V7（23.2）两代并存。HAL 二进制来自 23.2
   APEX、依赖库却来自 22.0 静态树，这才是真正的「基底混用」；撤掉后由步骤 2b 提供
   的 23.2 版本唯一胜出。撤前已确认这 7 件在产物全树无 HAL 消费者（只有彼此与
   22.0 版 `composer3-V4-ndk` 互相 NEEDED）。

## 5. 验证

| 项 | 结果 |
| --- | --- |
| `gofmt` / `go vet ./07_build_vendor/` / `go build` | 通过 |
| `go test ./07_build_vendor/` | 全包通过（含 4 个新增单测） |
| 步骤 0 摘要 | `vendor APEX: graphics（预计摊平 2/12：composer.drm_hwcomposer_upstream / gralloc.minigbm_upstream）` |
| 步骤 2b 实跑（m22，rc=0） | 合计新增 10 / 覆盖 38 / 跳过 4，rc 改写 2 个文件 2 行 |
| 装配树 `bin/hw` | `…hwc3-service.drm_upstream`(358,504 B) 与 `…allocator-service.minigbm_upstream`(52,648 B) 到位，权限 0755 |
| 装配树 `etc/init` | `hwc3-drm-upstream.rc`、`org.lineageos.device.gralloc.minigbm_upstream.rc`，内容均为 `/vendor/bin/hw/…` |
| 装配树 `etc/vintf/manifest` | `hwc3-drm-upstream.xml`（composer3 v4）、`allocator.minigbm_upstream.xml`（allocator v2） |
| `vendor.img`（撤件后 206,524,416 B，sha256 `143ad271…`） | `debugfs ls /bin/hw` 只剩 2 个 upstream 二进制；`/etc/init` 只剩 2 个 upstream rc |
| 镜像标签 | composer_upstream = `hal_graphics_composer_default_exec`，allocator_upstream = `hal_graphics_allocator_default_exec`，`libminigbm_gralloc_upstream.so` / `mapper.minigbm_upstream.so` = `same_process_hal_file`（命中底座 `(_upstream)?` / `.minigbm.*` 规则，不需要 #55 兜底） |
| VINTF 去重 | `vendor/etc/vintf/manifest` 里只剩 `IComposer/default`（v4）、`IAllocator/default`（v2）、`@5.0/minigbm_upstream` 各一处，无重复 fqname |
| 静态树来源分类（28 个文件，与两代基座逐文件 sha256 比对） | 与 23.2 同路径同内容 0；与 23.2 同名不同内容 3（`build.prop` 由 props.go 生成、`etc/excluded-input-devices.xml` 只排除 Power Button、`usr/idc/tablet2multitouch.idc` 追加 `touch.toolType = finger`，均为机型定制）；23.2 无而 22.0 同内容 0（第二批撤件后清零）；两基座均无 25（自研/手工 rc、配置 XML、VINTF 片段） |
| 第二批撤件后 `vendor.img`（205,897,728 B，sha256 `bce47cdb…`） | 镜像内 `/lib64` 只剩 23.2 APEX 版本：`composer3-V4-ndk.so`(252,448 B, `bc002b73…`)、`drm.common-V1-ndk.so`(51,312 B, `3709aab3…`)、`graphics.common-V7-ndk.so`(51,992 B)；`composer@2.1/2.2*.so` 与 `common-V6-ndk.so` 全部消失 |
| 基底来源唯一性 | 07 日志 8 处 lineage 路径全部指向 `lineage-23.2`（基底镜像 / 解包目录 / 摊平输入 / 打包取 UUID），全文无 `lineage-22` 字样；静态树写入量 32 → 25 |
| 静态检查 | `vendor_file_contexts`、`vendor_sepolicy.cil` 本轮未改动，无新增策略面 |

## 6. 遗留

- **运行期未验**：本轮只做到「产物与镜像内落位 + 标签正确 + VINTF 无重复声明」，没有起实例看
  `vendor.hwcomposer-3.drm_upstream` / `vendor.graphics.allocator.minigbm_upstream` 是否 running、
  surfaceflinger 是否真的用上 upstream 这套；也没有做 23.2 upstream 图形栈与 QTI 私有
  `libgralloc.qti` 的混合可行性验证（m22 的图形加速若要落到 QTI 侧，还要另议）。
- **静态树借件已全部撤掉（两批共 18 件，清单见第 4 节第 5、6 条）**。现在图形栈的**唯一来源**是
  步骤 2b，产物 `bin/hw` 只剩 `…drm_upstream` / `…minigbm_upstream` 两个二进制、`etc/init` 只剩两个
  upstream rc、VINTF 三个 fqname 各一处、`lib64` 只剩 23.2 APEX 的 `composer3-V4-ndk` /
  `drm.common-V1` / `common-V7`。若运行期发现 upstream 这套在 m22 上不可用，回退办法是把这 18 件
  从 git 恢复并同时 `--apex-flatten none`，不要两套并存。
- 静态树来源分类（第 5 节那行）建议当作换底包时的自检脚本模板：任何「23.2 无而旧版有且同内容」
  的件都是历史借件，换基底后必须复核，否则后层静态树会悄悄压掉新基底/摊平的内容。
- `graphics` 特征表是按 lineage-23.2 的实际文件名归纳的，换别家 Android 16 ROM 若命名不同
  （例如把 composer 放在 `com.android.hardware.graphics.composer.apex` 之外的包里），需要
  用 `--apex-flatten all` 或 glob 兜底；`all` 会把 audio/boot/cas 等也摊平，与自研模块
  （audio 等）可能撞车，届时按模块逐个排除。
- erofs payload 分支（`fsck.erofs --extract=`）本轮没有实跑对象（23.2 两个 APEX 都是 ext4），
  只在代码里按 magic 分流。
- 摊平的结果不参与缓存指纹（只有模式参与）：如果以后 APEX 内容变化而基底镜像不变，
  需要 `--refresh` 才会重摊。
