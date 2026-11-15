# Issue: oriole（Pixel 6 / Android 14）机型静态树不再放 .so——改由基线清单 `vendor.lifts` 声明借件

- 设备: `google/oriole`（Pixel 6），官方固件 `AP2A.240905.003.F1`（Android 14）；云机底包 `lineage-21.0`（A14）
- 阶段: `07_build_vendor`（新增步骤 5b「借件」+ 装配段现编 compat 库）
- 状态: **已闭环**（2026-11-01）：机型静态树 18 → **2 个文件**（只剩 `etc/init/hwc3-drm.rc` 1426 B 与 `etc/vintf/manifest/hwc3-drm.xml` 435 B，共 1861 字节）；15 件按清单取件、1 件按需现编；07 实跑 `EXIT=0`，装配树 **ELF 159 个、未解析依赖 0 项**；重跑 09 → 11 后新盘集起机 **`sys.boot_completed=1`**，`vendor.hwcomposer-3` 常驻、`SurfaceFlinger` 正常接管
- 关联: [#101 issue-zygote-bootstrap-ns](../issue-zygote-bootstrap-ns/solution.md)（载体落点）、[#102 issue-carrier-mount-unify](../../common/issue-carrier-mount-unify/solution.md)（载体统一）。本条主体是「预编译件从哪来、怎么进装配树」，运行期起机在 §5 末补验（#101 遗留的「SF 崩在 createHWComposer」已解）

---

## 1. 问题

`devices/manifest/google/oriole/vendor/` 是机型**静态树**：`07_build_vendor` 步骤 4 原样覆盖到装配树，语义是「这个机型要的文件长这样」，**不表达文件从哪来**。oriole 为了跑起 A14 的 hwc3 composer，把 16 个预编译件（2.2 MB）直接塞进了静态树：

```
bin/hw/android.hardware.composer.hwc3-service.drm                        242064
lib64/android.hardware.drm.common-V1-ndk.so                               51240
lib64/android.hardware.graphics.allocator@2.0.so                          92864
lib64/android.hardware.graphics.allocator@3.0.so                          92896
lib64/android.hardware.graphics.allocator@4.0.so                          84216
lib64/android.hardware.graphics.common-V6-ndk.so                          52056
lib64/android.hardware.graphics.composer3-V4-ndk.so                      252832
lib64/android.hardware.graphics.composer@2.1-resources.so                 36472
lib64/android.hardware.graphics.composer@2.1.so                          337464
lib64/android.hardware.graphics.composer@2.2-resources.so                 35192
lib64/android.hardware.graphics.composer@2.2.so                          255080
lib64/android.hardware.graphics.mapper@2.0.so                            105960
lib64/android.hardware.graphics.mapper@2.1.so                            107000
lib64/android.hardware.graphics.mapper@3.0.so                            119488
lib64/libcdh_composer_compat.so                                            3400
lib64/libui.so                                                           378032
```

三层问题：

1. **不可审阅**：仓库里躺的是二进制，谁也不知道它是哪份 ROM、哪个版本、哪个编译器产出的；
2. **换基座即失效且无感**：底包换一版后这些件要么还在、要么缺了，构建不会报错，只会在运行期炸；
3. **有一件根本没有上游**：`libcdh_composer_compat.so` 是我们自己写的两符号垫片（见 #101/#102 的 hwc3 方案），带进仓等于把「十来行 C++」变成不可追溯的 3400 字节。

目标：**静态树只留人能读懂的东西（rc / VINTF 片段）**，全部预编译件在机型基线清单里逐条声明来源，由构建现场取件。

## 2. 根因

- 静态树这份机制**没有来源字段**，只能承载「字节」，不能承载「出处」；而 oriole 这 16 件的出处恰恰是最需要记下来的信息（其中 8 件来自**另一个 Android 版本**的 lineage 底包）。
- `vendor.replaces` 已经解决了「从厂商固件取原件」的一半问题，但它绑定**唯一**来源（官方固件解包根），且语法上是「整表取代内置集合」；借件需要的是「多条、每条各自指定来源」，语义不同，不能复用。
- `libcdh_composer_compat.so` 不是「借来的件」，**没有可借的对象**，只能现编——`vendor/build/internal/halbuild` 里给 android14 编 HAL 时补 compat 对象的做法已经是现成路径，照搬到 07 即可。

## 3. 证据

### 3.1 16 件逐条溯源（sha256 实测，2026-11-01）

| 件 | 旧静态树 sha256（前 12） | lineage-21.0 | lineage-22.2 | lineage-23.2 | 官方 A14 `vendor_a.img` | 结论 |
| --- | --- | --- | --- | --- | --- | --- |
| hwc3 可执行文件 + composer3-V4-ndk + graphics.common-V6 + drm.common-V1 + composer@2.1(+res) + @2.2(+res)（8 件） | — | 无此件 | **逐字节相同** | — | — | 借 A15 底包（`lineage:22.2`） |
| `libui.so` | `df201104b6f9` | 无此件 | `586e4bc54a57` | `11948fccc611` | `683c68d98495` | **三处都对不上** |
| allocator@2.0 | `30ebccdb31bf` | 无此件 | `61644916f62e` | `4d2a054c9411` | `7ba456604acc` | 同上 |
| allocator@3.0 | `b05f27881f45` | 无此件 | `8b9c53157f3a` | — | `d533210d34dd` | 同上 |
| allocator@4.0 | `0625f8fbae41` | 无此件 | `dafc7d039833` | — | `6054a137db58` | 同上 |
| mapper@2.0 | `7ad3525eef7f` | 无此件 | `b716c1c59b3d` | `e260f2e1b35c` | `e24ea2cfea49`(前 12) | 同上 |
| mapper@2.1 | `0e038068bfd5` | 无此件 | `a1ef8baae348` | — | `d729d5e12618` | 同上 |
| mapper@3.0 | `56f7f109613d` | 无此件 | `9c61612525fd` | `5c40904845e7` | `f52de2accd63` | 同上 |
| `libcdh_composer_compat.so` | 3400 B | — | — | — | — | 自编，无上游 |

也就是说：**7 件既不是本机底包、也不是借来的 A15、也不是官方 A14**——是某次手工拷贝留下的陌生件。这正是「静态树承载不了出处」的代价。

### 3.2 官方 `vendor_a.img` 里 7 件齐全（`debugfs stat`）

`out/roms/google/oriole/AP2A.240905.003.F1/super/vendor_a.img`（ext4）逐条 `stat` 全部命中、均 `0644`；同一次探测里另外确认了 11 件相关库的存在（libui、allocator@2.0/3.0/4.0、mapper@2.0/2.1/3.0、composer@2.1(+res)、@2.2(+res)）。反面：`lib64/egl/libMEOW_data.so`、`/etc/security`、`/etc/md` 均 `File not found`（这也是本机型 `vendor.replaces` 只留 `libpng` 一条的取证依据）。

### 3.3 换成官方 A14 原件后丢了哪些符号，谁在用

| 件 | 旧件导出 | 官方件导出 | 旧有新无 | 装配树内是否被引用 |
| --- | --- | --- | --- | --- |
| `libui.so` | 692 | **776** | `_ZN7android9SingletonINS_19GraphicBufferMapperEE11getInstanceEv` | **否** |
| `mapper@2.0.so` | 98 | 95 | 3 个 libc++ 模板实例化（`std::function<HidlInstrumentor…>` 的 `operator()`、`vector<…>::vector(const&)`、`vector<void*>::__push_back_slow_path`） | **否** |

「装配树内是否被引用」不是只看可执行文件：把装配树 159 个 ELF 的**全部**未定义符号去重成 4000 条后逐个 `grep -x`，4 个丢失符号**一条都没被引用**。

### 3.4 借来的 A15 库有两个 A14 没有的符号，且不允许惰性绑定

`llvm-readelf --dynamic` 显示 A15 这批库全部带 `BIND_NOW`（无 `FLAGS_1 LAZY`），缺符号就是 `dlopen` 失败而不是首次调用失败。缺口符号：

- `AIBinder_Class_setTransactionCodeToFunctionNameMap`（A15 libbinder_ndk 新增，纯诊断用）
- `_ZNSt3__122__libcpp_verbose_abortEPKcz`（A15 libc++ 新增）

A14 的 `/system/lib64/libc++.so` 与 `libbinder_ndk.so` 都没有这两个符号 —— 与 `halbuild/libcxx_compat.go` 给 android14 编 HAL 时补的是同一对。

## 4. 改动

### 4.1 取件层抽通用来源（`replace.go`）

把写死「官方固件」的取件逻辑抽成 `liftSource{Label, Trees, Image}` + `extractSource`：

- 依次尝试 `Trees` 里的逐文件树根；都没有才读 `Image`；
- 镜像按**文件系统类型**分流：ext4 走 `debugfs -R "stat/dump/rdump"`，erofs 走 `fsck.erofs --extract=… --path=…`；
- 新增 `imageExtractHint(image, source)`：dry-run 的计划命令行据此打印真实可复制的取件命令（此前 ext4 也一律打印 `fsck.erofs`，误导排查）；
- `placeReplace` 增加 `origin` 参数，日志里的来源名由调用方给（`厂商 vendor` / `lineage-22.2 底包`）。

`vendor.replaces` 行为不变（同一套落位语义，`replaceOriginLabel`）。

### 4.2 新增步骤 5b：`vendor.lifts`（`lift.go`，约 315 行）

清单键 `vendor.lifts`，条目 `{from, source, target, mode, optional, note}`，落位语义完全复用 `replaceEntry`：

| `from` | 来源 |
| --- | --- |
| 省略 / `vendor` / `official` | 本机型官方固件解包根（`super/unpack_vendor` 式逐文件树，或 `super/vendor_a.img`） |
| `lineage` / `base` | 本机底包（`BaseImage` 上溯 `lineage-*`） |
| `lineage:<版本>` / `lineage-<版本>` | `out/roms/lineage/lineage-<版本>/super/unpack_vendor`（缺则 `super/vendor_a.img`） |

与 `replaces` 的两处刻意差异：**未声明该键 = 空集**（不回落任何内置表，避免悄悄借件）；来源语法在装配前校验（`system`、带 `..` 的版本名直接报错）。

副作用：`describe` 多一行 `借件: 15 条(…)`；dry-run 多 15 行 `计划命令 [步骤 5b]`；`vendor-info.txt` 多一行 `vendor-lift: 15 条(…)`。

### 4.3 装配段现编 compat 库（`compatbuild.go`，约 173 行）

`libcdh_composer_compat.so` 不再进仓：源码以常量形式随引擎维护（十来行，无 include），`buildComposerCompatLib` 在 `patchComposerCompatNeeded` 之前用 NDK 编一份：

```
clang++ --target=aarch64-linux-android21 -shared -fPIC -nostdlib \
        -fno-exceptions -fno-rtti -O2 -Wl,-soname,libcdh_composer_compat.so
```

`-nostdlib` 让 `abort`/`vdprintf` 留成未定义符号由运行期 bionic 解析。exe 不存在时整步跳过（gold/m22/ss9380 用厂商自己的 composer，不受影响）。`locateNDKClang` 顺序：`CDH_NDK` → `ANDROID_NDK_HOME` → `ANDROID_NDK_ROOT` → SDK 下 `ndk/*`；**目录名以数字开头者优先**（理由见 §5 的第二个失败）。

### 4.4 接线与清单

- `plan.go`：`Plan.Lifts` 字段、解析+校验、摘要行、dry-run 5b 行；顺手修掉 dry-run 容量算好用 `vendorReplaces` 而不是 `plan.Replaces` 的旧 bug（声明了 `replaces` 的机型会打印错条数）。
- `assemble.go`：`applyVendorLifts` 排在 `applyVendorReplaces` 之后（借件盖过厂商覆盖）、`buildComposerCompatLib` 排在 `patchComposerCompatNeeded` 之前。
- 基线清单 `manifest/google/oriole/baselines/oriole-ap2a.240905.003.f1-factory-655c44e7.json`：新增 15 条 `vendor.lifts`（8 × `lineage:22.2` + 7 × `vendor`），两组首条各带一条 `note` 说明「为什么借、为什么是这个版本」；同时重写 `note` 里关于 composer 的三段旧口径（静态树 → 借件 → 现编）。
- 机型静态树：删除 16 个二进制（原件先备份到 `/tmp/oriole-old/` 做对照），只留 `etc/init/hwc3-drm.rc`、`etc/vintf/manifest/hwc3-drm.xml`。
- `build/07_build_vendor/README.md`：步骤总览加 5b 行 + 「步骤 5b：借件」整节（输入/输出/时机、`from` 表、JSON 示例、oriole 实跑输出、`CDH_LINEAGE_BASES`、NDK 说明）+ 代码地图两行。

## 5. 验证

- `go build ./...`、`go vet ./07_build_vendor/` 通过；`go test ./07_build_vendor/` 全绿（新增 7 个用例：解析、缺键为空集、`from` 语法含 `system`/`..` 拒绝、从 lineage 树借件、缺源报错、compat 源含两符号、无 exe 时跳过）。
- `--dry-run`：`借件: 15 条(…)`，15 行 `计划命令 [步骤 5b]`；8 条指向 `lineage-22.2/super/unpack_vendor/…`，7 条打印 `debugfs -R "dump /lib64/… <目标>" out/roms/google/oriole/AP2A.240905.003.F1/super/vendor_a.img`。
- 07 两次实跑（第一次失败，见 `log-fail.txt` §D；修 NDK 排序后第二次通过，`log-pass.txt`）：静态树 2 个文件；步骤 5b 15 行 `[replace]`；compat 库现编（NDK `29.0.14206865`）。
- 逐字节对表：`lineage:22.2` 8 件里 7 件与底包**完全相同**，可执行文件因装配段改写 `DT_NEEDED` 而差 50 字节（同尺寸 242064，差异集中在偏移 17052~17102 的 dynstr）；`vendor` 7 件与官方 `vendor_a.img` **完全相同**。
- `DT_NEEDED`：`android.hardware.graphics.composer@2.2-resources.so` 已被换成 `libcdh_composer_compat.so`（原方案的空 `resources` 壳不再需要预加载）。
- compat 库：导出两个符号，只引用 `abort` / `vdprintf` 两个 bionic 符号，3400 字节。
- 依赖闭包：装配树 159 个 ELF，`NEEDED` 全部由「树内同名件 / A14 LLNDK 名单 / bionic 运行时（libc/libm/libdl/libc++）」解析，**未解析 0 项**。
- 产物指纹：`vendor-info.txt` 记 `vendor-lift: 15 条(…)`、`image_sha256: 56a36e541e4bb1d00e924376470b79ec4a8c8f266d06495c5c8ea05ad6af67cb`、`recipe: vendor-67855d254548`（首次装配为 `5185b732ee07…`，两者只差静态树 rc 的注释文字）。
- **起机验证**（09 → 11 重跑后）：`build-patch --refresh` 重建 `patch.img`（`d4c6d5c60c299e9cb9dcf81eda89d652afb8fe6e3d9b3454477c39c1a0c4334d`，recipe `patch-9f55592f1c43`），`pack-vda --refresh` 重刷 13 GiB 盘集；`POST /api/create {"index":1}` 起实例 `oriole_AP2A.240905.003.F1/7`（adb `127.0.0.1:5911`）后实测：
  - `getprop sys.boot_completed` = `1`（约 30 s 内完成，`init: processing action (sys.boot_completed=1)` 见 `qemu.log`）；
  - `ps -A` 有 `android.hardware.composer.hwc3-service.drm`（pid 715，`logcat` 全文只有 1 次 `starting service 'vendor.hwcomposer-3'`，无崩溃重启）；
  - `service list` 有 `android.hardware.graphics.composer3.IComposer/default`；
  - `qemu.log`：`drmhwc: hwc3-drm starting up` → `SurfaceFlinger: Using HWComposer service: default` → `HwcComposer: Loaded AIDL composer3 HAL service` → `HWComposer: Switching to generalized multi-display mode`；`CANNOT LINK` 计数 **0**；
  - 宿主侧 `cgl-headless 大帧 1080x2400 … 总=1.9 ms` 说明帧确实合成上屏；
  - guest 里三件落地尺寸与装配树一致：exe 242064、`libcdh_composer_compat.so` 3400、`libui.so` 433584。
  > `screencap` 在本机型会挂住（无 GPU readback），故未截屏，改以 `qemu.log` 的合成统计与 SF 服务状态取证。

复现命令与完整输出见 `log-pass.txt`（脚本落在 `engine/out/tmp/verify-lift.sh`）。

## 6. 遗留

1. ~~没有整机启动验证~~ → **已补**（见 §5）：09 → 11 重跑后新盘集起机通过，#101 遗留的「SF 崩在 createHWComposer」已解。仍未验的是业务级观感（launcher 渲染质量、相机/视频的 GPU 路径），`screencap` 在本机型不可用（会挂住），只能通过 VNC 端口（实例 7 为 5912）人工看。
2. **compat 库字节随 NDK 版本变，但不进配方**：`ComputeVendorRecipe` 只哈希「基底镜像 + HAL 产物根 + APEX 摊平模式」，07 也没有按配方跳过装配（每次都重跑、`copyFile` 内容相同才不动），所以换 NDK 会静默改变 compat 库字节而 `vendor-recipe.txt` 不变 —— 下游 08/09/10 是按配方判复用的，这里有口径裂缝。**同一裂缝还有一处实证**：本轮只改了静态树 rc 的注释文字（`「由机型静态树写入 /vendor」`→`「静态树只留纯文本配置，二进制由 vendor.lifts 借件」`），`recipe` 仍是 `vendor-67855d254548` 不变，而 `vendor.img` 的 sha256 从 `5185b732ee07…` 变成 `56a36e541e4b…`——说明配方没覆盖机型静态树，靠「07 每次全量重装配」兜底。
3. **A15 借件是「跨版本可用」而不是「等价」**：这批库带 `BIND_NOW`、依赖两个 compat 符号；将来若把底包换到 A15，`lineage:22.2` 就该改成 `base`，`libcdh_composer_compat.so` 也应随之停编（A15 libc++/libbinder_ndk 自带这两个符号）。
4. **官方 `libui.so` 少 1 个导出、`mapper@2.0` 少 3 个 libc++ 模板实例化**（§3.3）：当前装配树无引用，但若将来有件按旧符号链接，会在 `dlopen` 时报 undefined。
5. **只迁移了 oriole**：`grep '"lifts"' manifest/**` 仅命中本机型；gold / m22 / ss9380 的静态树若也带预编译件，需各自判断哪些能溯源后迁移。
6. **`vendor.lifts` 未声明 = 空集**（不回落内置表）：这是刻意的，但意味着新机型若想借件必须显式声明，没有任何默认行为兜底。
