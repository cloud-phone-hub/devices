# CPH2797 基线清单按 m22 结构重排 + 机型声明 vendor.replaces 整表

- 设备：`oppo/cph2797`（OPPO Find X9 / ColorOS 16.0.8.301_EX01 / Android 16 / MediaTek mt6993）
- 阶段：清单层（影响 02/03/05/07 全链）
- 日期：2026-10-30

## 1. 问题

按 m22 / gold 范式接入 11 阶段流水线时，`manifest/oppo/cph2797/baselines/CPH2797_16.0.8.301_EX01_A11_full.json`
仍是旧工作区 `devices/oppo/findx9-16/build/baselines/` 的 legacy 形态（`cdh device sync` 原样搬过来的），
两处直接后果：

1. `03_build_modules` 报 `内核模块数量异常: cdh_virt_wifi.ko count=0（exit status 9）`；
2. `07_build_vendor` 步骤 5 报 `[err] 阶段 build_vendor 失败: 厂商 vendor 缺少该路径: etc/md`，整阶段 0 产物。

与 m22（`manifest/flyme/m22/baselines/12.5.0.0A.json`）的结构差异：

| 字段 | legacy（CPH2797 原样） | m22 / gold 口径 |
| --- | --- | --- |
| `version-description.board` | 缺 | 有（+ `board-note` 说明取 fstab 后缀） |
| `version-description.factory-dir` / `measured-from` | 缺 | 有 |
| `rom.lineage.kernel` | 缺 | 有（`version` / `vermagic` / `source` / `measured-from` / `in-tree-modules`） |
| `vendor.build-prop-sets` | `[[k, v], …]` 数组 | `{k: v}` 对象 |
| `vendor.build-prop-unsets` | `[]` | `{}` |
| `vendor.vintf-version` | 缺 | 有（取官方实测） |
| `vendor.replaces` | 缺（吃引擎内置表） | 机型声明整表 |
| `vendor.lib64-backfills` | 有 | **不存在该字段**（新引擎零代码读取） |
| 顶层 `auto-sync` | 有（迁移记录） | 无 |
| `comment` / `note` | 指向 `roms/CPH2797_…/`、`out/dist/lineage/lineage-23.2`、`build/vendor/build/props.py` | 指向新引擎阶段与产物 |

## 2. 根因

- **`03` 失败**：`build/internal/kbuild/kbuild.go` 的 `InTreeModules` 默认表是 6.6 口径，含 6.6 把
  `virt_wifi.o` 改名产出的 `cdh_virt_wifi.ko`；CPH2797 与 m22 同跑 6.12.81 底包，无线走
  `mac80211_hwsim`（CDH 补丁：hvc9 控制面 + hvc10 数据面直桥），该模块不存在 → 计数 0 → 阶段直接失败。
  解药是清单 `rom.lineage.kernel.in-tree-modules`（声明即整表取代默认表）。
- **`07` 失败**：未声明 `vendor.replaces` 时用内置的 `vendorReplaces`（gold/MTK 口径，10 项，
  见 `build/07_build_vendor/replace.go`），其中 `etc/md`（联发科 meta 数据目录）在 CPH2797 官方
  vendor 里**不存在**；覆盖段是"最终裁决"语义，取不到就报错而不是跳过。声明 `vendor.replaces`
  会整表取代内置集合 —— 这是机型唯一能裁掉本机不存在条目的口子。
- **格式未对齐的实质影响**不只在可读性：`board` 缺省时 05 只能"从厂商 vendor_boot 探测"，
  引擎无法在清单层固化；`lib64-backfills` 是 legacy python 流水线（`build/vendor/source/…`）
  的私有字段，新引擎不读，留着等于把"V5 图形库回填"这条必要动作静默丢掉。

## 3. 证据

官方 OTA（`roms/devices/oppo/cph2797/CPH2797_16.0.8.301_EX01_A11_full.zip`，payload 全量包）
经 `dump.erofs` / `debugfs` 直读：

| 事实 | 实测值 |
| --- | --- |
| `super/vendor_a.img:/etc` | 只有 `fstab.mt6993` / `fstab.emmc` / `fstab.enableswap` / `fstab.raid`（无 `etc/md`） |
| `super/vendor_a.img:/build.prop` | `ro.board.platform=common` 后覆盖成 `mt6993`；`ro.vendor.build.security_patch=2026-06-01`；`ro.hardware.egl=mali`；`ro.hardware.vulkan=mali`；`ro.hwui.use_vulkan=true`；`ro.hardware.hwcomposer=mtk_common`；**无** `ro.vendor.boot_security_patch` 行 |
| `super/vendor_a.img:/etc/vintf/manifest.xml` | `<manifest version="9.0" type="device" target-level="202504">` |
| `super/vendor_a.img:/etc/vintf/compatibility_matrix.xml` | `<compatibility-matrix version="9.0" type="device">` |
| `super/vendor_a.img:/lib64/egl` | `libGLES_mali.so`、`libGLES_meow.so`（**无** `libEGL_mesa` 三件） |
| `super/vendor_a.img:/lib64` | `graphics.common-V5-ndk.so` 51992 B、`V6`，另有 `@1.0/@1.1/@1.2` |
| `super/vendor_a.img:/etc/vulkan` | **不存在**（无 ICD） |
| `super/system_a.img:/system/build.prop:74` | `ro.opengles.version=196610` |
| lineage-23.2 基座 `vendor_a.img:/lib64`（ext4，debugfs） | 只有 `graphics.common-V7-ndk.so` 51992 B（无 V5） |
| lineage-23.2 基座 `vendor_a.img:/build.prop`（153 行） | `ro.vendor.build.security_patch=2026-08-01`、`ro.hwui.use_vulkan=`（空）、`ro.soc.model=virtio_arm64only`；**无** `ro.hardware.egl` / `hwcomposer` / `opengles.version` |
| 装配树 `vendor/etc/selinux/vendor_file_contexts` | 无 `egl/lib` 规则、无 `graphics.common` 规则（grep 0 命中）→ 兜底 `vendor_file` |
| 出厂 `firmware/boot.img`（boot v4，kernel_size=18830917） | `Linux version 6.12.23-android16-5-g82efd98459a2-ab14457512-4k (kleaf@build-host) (Android (14043575, +pgo, +bolt, +lto, based on r536225) clang version 19.0.1 (…llvm-project b3a530ec…), LLD …) #1 SMP PREEMPT Tue Nov 18 18:08:47 UTC 2025`（`LLD` 版本号被 MTK LK 压缩字典吞掉，原文只剩 `LLD`） |

失败现场见 `log-fail.txt`（07 步骤 5，覆盖集合 10 项 → `etc/md` 缺路径）。

## 4. 改动

只改清单，零代码。

1. `version-description` 按 m22 补齐 `board=mt6993` + `board-note`（取官方 fstab 后缀，并说明
   `ro.board.platform` 实为 `common`→`mt6993`）、`factory-dir=16.0.8.301_EX01`、`measured-from`；
   `android-version` 从 `16` 写成 `16.0.0`。
2. `rom.lineage.kernel` 按 m22 落地：`version=6.12.81`、`vermagic=6.12.81-4k-g4f6bf47200d9 SMP preempt mod_unload modversions aarch64`、
   `source`、`measured-from`、`in-tree-modules` 6 项（`virtio_console / rfkill / cfg80211 / mac80211 /
   mac80211_hwsim / libarc4`）、`in-tree-modules-note`、`config-note`（不声明机型片段，走统一
   `build/02_build_kernel/lineage-kernel.config`）。
3. `rom.vendor.kernel` 补 `measured-from`；`uname-version` 从"banner 被截断"的糊弄写法换成
   LK 压缩流里还原出的原文（缺口只标 `LLD …`）。
4. `vendor` 段按 m22 顺序重排：`vintf-version=9.0` → `selinux-labels`（mesa 三件 +
   `graphics.common-V5-ndk.so`，值 `same_process_hal_file`）→ `replaces`（4 项：`lib64/libpng.so`、
   `etc/permissions` 49 文件整替换、`etc/security` 171 文件整替换、`lib64/android.hardware.graphics.common-V5-ndk.so`
   回填）→ `build-prop-sets`（对象，7 条：两个 security_patch、`ro.hardware.egl=mesa`、
   `ro.hardware.hwcomposer=drm`、`ro.opengles.version=196608`、`ro.hwui.use_vulkan=false`、
   `ro.soc.model=vsoc_arm64`）→ `build-prop-unsets={}` → `note`（逐条给证据）。
5. 删除 legacy 私有字段：顶层 `auto-sync`、`vendor.lib64-backfills`（其唯一产物"V5 回填"改由
   `vendor.replaces` 条目承接，语义等价且引擎真的会执行）。

**刻意不含**：官方 `lib64/egl` 的 Mali 三件 / `libMEOW_*.so` / `libOpenCL.so`（图形走 mesa，
无 GPU 时 Mali 无落地价值）；`etc/selinux`（策略必须与 lineage 的 `plat_pub_versioned.cil` 同源）。

## 5. 验证

- 清单层：`python3 -c 'json.load'` 通过；`board=mt6993 | android=16.0.0 | factory-dir=16.0.8.301_EX01`，
  `props=7 | labels=4 | replaces=4 | in-tree=6`，`auto-sync` / `lib64-backfills` 已消失。
- `cdh device build cph2797 --dry-run`：11 阶段全识别，固件与 lineage 底包均在位。
- `--only build_vendor_boot`：`板级名: mt6993`、配方 `vboot-12aedb9feb6b` 与改前一致 → `[skip]`
  （证明 `board` 声明与 05 探测值同源，不会因清单固化而改产物）。
- `--only build_vendor`（见 `log-pass.txt`）：覆盖集合 4 项全部命中（含 V5 图形库回填）、`VINTF schema
  对齐到 9.0 已全部是 9.0`、7 条属性逐条落盘（追加/覆盖各 4/3 条）、12 个模块层 sepolicy 片段追加、
  装配树 **762** 条目（文件 531 / 链接 199 / 目录 32）、`vendor.img` **205,885,440 B**。
- 后续四阶段一链跑通：`vendor_dlkm.img` 15,831,040 B（sha256 `7908b453e00c`）、`patch.img`
  402,653,184 B（sha256 `298f3616b28b`）、`super.img` 11.08 GiB（16 个逻辑分区回读逐字节一致）、
  `out/dist/oppo/cph2797/16.0.8.301_EX01/vda.img` 13.00 GiB（5 个分区）。

## 6. 遗留

- **运行期未起实例**：本机 24 GiB RAM 上 m22 #45 与 gold #7 各占 8 GiB，第三个实例会挤爆；
  需先停一个槽位再跑 CPH2797，按 AGENTS.md「先固证再推进」补 boot 日志与截图。
- **`ro.soc.model=vsoc_arm64` 未经本机实测**：取自 findx9-16 旧案的 ColorOS QPerformance 口径，
  bring-up 若无线索需按日志裁决（m22 证明了"没有 libqti_performance.so 就不必设"这一边界）。
- **`replaces` 未纳入 Mali/MEOW/OpenCL**：若 ColorOS 侧（`my_product` / `my_stock`）有二进制硬
  `dlopen` 这些库，会在 logcat 暴露 `CANNOT LINK`，届时按证据逐条加回。
- **09 的 `/odm` 只来自 06 覆盖树**（当前内容：`odm/etc/build.prop` 一件）→ 官方 ColorOS
  `/odm` 整树（`lib64/libui.so`、`contexthub`、`power.stats` 等）在运行态被 patch 遮蔽；
  这是与 legacy `devices/oppo/findx9-16` 最大的路线差，是否改成"官方 odm 为底 + 覆盖件叠加"
  要等 boot 日志说话。
- **工具坑**：本轮对已存在的基线 JSON 用编辑类工具写入会"报成功但不落盘"（`mtime`/`size` 不变），
  可靠做法是"写新文件 + `mv` 覆盖"，改完必须用 `python3 -c json.load` 复核磁盘内容。
