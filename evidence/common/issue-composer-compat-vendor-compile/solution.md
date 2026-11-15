# composer compat 垫片库改由 vendor 侧编译

- 日期：2026-11-02
- 位置：`engine/build/07_build_vendor`（原现编实现）、`engine/vendor/build`（新 `kind=shared`）、
  `engine/vendor/hardware/{common/libcxx-compat,libcdh_composer_compat}`、`devices/manifest/xiaomi/gold/device.json`
- 状态：已完成（构建层 + 两侧测试）；gold 实跑待随下一次 07 复验

## 1. 问题

07 装配段自己定位 NDK、自己拼 `clang++` 命令行，现编出一块垫片库
`lib64/libcdh_composer_compat.so`（3.4 KiB，只定义两个符号），再改写 composer 可执行文件的
`DT_NEEDED` 把它接上。

用户给出的边界是：

> vendor 的需要三方编译都交给 vendor 编译，build 只根据配置调用 vendor 编译然后集成产物

按这条边界，07（09~11 这一段的装配/打包职责）里不该出现「找工具链 / 调编译器 / 链共享库」；
它只应「按 `device.json` 的 `hardware` 清单调 `cdh-vendor hal build`，再把产物叠进装配树」。
本次要迁走的就是这块编译动作，装配侧只留 DT_NEEDED 改写。

## 2. 根因

2026-11-01 定位 gold 的 composer 缺符号问题时，垫片库是「顺着现象补出来」的：装配树里出现了
`bin/hw/android.hardware.composer.hwc3-service.drm`，就在同一步现编一个同名 `.so` 塞进 `lib64/`。
当时 `cdh-vendor` 侧只有 `kind=hidl` 的 `sharedLibs` 分支（要求 HIDL 头与 ABI 目录），没有
「只编一个 .so、不产服务」的口子，所以垫片库被放进了 07。

后果有三条：

1. 同一件事有两套实现。`engine/vendor/hardware/common/libcxx-compat/` 的两份 `.cc` 既被
   `cdh-vendor hal build` 拿去给 HAL 补 compat 对象（`libcxxCompatSource`），又被 07 拿去现编垫片库；
   命令行两份，改一边忘一边不会报错（符号对不上要到设备上 BIND_NOW 加载才暴露）。
2. 07 要承担工具链探测：`CDH_NDK` / `ANDROID_NDK_HOME` / `ANDROID_NDK_ROOT` → `$ANDROID_HOME`、
   `$ANDROID_SDK_ROOT`、`~/Library/Android/sdk` 下版本号最大的 `ndk/*`，这一整套与
   `vendor/build/internal/toolchain` 重复。
3. 编译产物没有进模块体系：它不在 `device.json` 的 `hardware` 清单里，因此不受
   「模块产物清单 → 步骤 4 叠加层」这套搬运约束，也不参与复用判据（源码指纹）。

## 3. 证据

### 3.1 三条来源先分清（决定「谁该编」）

| 机型 | 装配树里的 composer | 谁产的 | 缺两个 A15 符号？ |
| --- | --- | --- | --- |
| oriole | `android.hardware.graphics.composer@2.4-service`（367928 B） | 自编（`kind=hidl` 模块，`cdh-vendor hal build`） | 否（`llvm-nm -D -u` 零命中） |
| m22 / cph2797 / ss9380 | `…hwc3-service.drm_upstream` | 底包 vendor APEX 摊平（步骤 2b） | 是（带 `U __libcpp_verbose_abort`） |
| gold | `…hwc3-service.drm` | lineage-22.2 底包 vendor 分区铺底（步骤 2） | 是 |

也就是：垫片库要补的从来不是「composer 模块的产物」，而是「别处编出来、目标运行库缺符号的可执行件」。
这件事仍然是「编译」，所以归 `cdh-vendor`；07 只该做「按配置调它 + 集成」。

### 3.2 改造前的越界（原文见 `log-fail.txt`）

```go
// engine/build/07_build_vendor/compatbuild.go（已删除）
const composerCompatTarget = "aarch64-linux-android21"
var ndkEnvVars = []string{"CDH_NDK", "ANDROID_NDK_HOME", "ANDROID_NDK_ROOT"}
func buildComposerCompatLib(plan *Plan, stdout io.Writer) error {
	...
	clang, err := locateNDKClang()
	argv := append([]string{clang, "--target=" + composerCompatTarget, "-shared", ...}, sources...)
	exec.Command(argv[0], argv[1:]...).CombinedOutput()
}
func ndkVersionsDesc(dir string) []string { ... }   // 连 SDK 目录扫描也在 07
```

### 3.3 触发判据只按文件名（本次未改，记入遗留）

`buildComposerCompatLib` / `patchComposerCompatNeeded` 都以
`exists(<vendor>/bin/hw/android.hardware.composer.hwc3-service.drm)` 为开关：

- m22 / cph2797 的 `_upstream` 件带同样两个未定义符号，名字不同 → 完全不处理；
- 若自编件哪天取名成同一个 `hwc3-service.drm`，07 会去改写一个不缺符号的 ELF。

### 3.4 vendor 侧已有的同源能力

- `internal/halbuild/libcxx_compat.go`：`libcxxCompatSource = "libcxx-compat/libcxx_verbose_abort.cc"`，
  `compatObjects(abiDirs)` 把它编进 HAL 的链接对象；但那条路要 HIDL 头与 ABI 目录，
  不适合「只要一个独立 .so」。
- `internal/spec` 的 `kind` 分支同理：`aidl` / `pure-c` / `v412` / `gpu` / `cargo` / `prebuilt` /
  `hidl`，没有「纯共享库」这一档。

结论：不是搬文件，而是给 vendor 侧补一档 `kind=shared`，再让 07 只做声明式调用。

## 4. 改动

### 4.1 vendor 侧：新增 `kind=shared`

| 文件 | 改动 |
| --- | --- |
| `engine/vendor/build/internal/spec/spec.go` | `Kind` 注释加 `shared`；`applyDefaults` 派生 `<短名>-shared-work` 工作区；`validate` 新增 `case "shared"`（要求 `sharedLibs` 每项有 name/dir/root/sources） |
| `engine/vendor/build/internal/halbuild/shared.go`（新增，121 行） | `buildShared` 逐库编译：`clang --target --sysroot -std -fPIC -O2 [-fno-exceptions] [-fno-rtti] -I… -c` → `ld.lld -nostdlib -fno-stack-protector -mno-outline-atomics -mbranch-protection=standard -shared -Wl,-soname,…`，产物落 `<产物根>/<dir>/<name>`；不链 libc++ 兼容对象（本库就是要补那两个符号，再链一份会重复定义），`-Wl,--allow-shlib-undefined` 把 `abort`/`vdprintf` 留给运行期 bionic |
| `engine/vendor/build/internal/halbuild/build.go` | 分派 `case "shared"`；`ResolvePath` 新增 `@common/<rel>` → `engine/vendor/hardware/common/<rel>`（`includes` 里的 `@` 前缀同样走它） |
| `engine/vendor/build/modules/libcdh_composer_compat.json`（新增） | `kind: shared`，`sharedLibs[0] = {name/soname: libcdh_composer_compat.so, dir: lib64, root: "@common/libcxx-compat", sources: [libcxx_verbose_abort.cc, binder_transaction_code_name.cc]}`，`compile: {std: c++17, exceptions: false, rtti: false}` |
| `engine/vendor/build/main.go` | `hal build` 对 `kind=shared` 不再要求 ABI 快照（与 v412 / pure-c 同列）；`hal list` 打印 `shared -> lib64/<name>` |
| `engine/vendor/hardware/libcdh_composer_compat/README.md`（新增） | 说明「只作为模块根存在」、源码走 `@common`、为什么不在装配段现编 |

### 4.2 07 侧：删掉现编，改成声明式集成

| 文件 | 改动 |
| --- | --- |
| `engine/build/07_build_vendor/compatbuild.go` | **删除**（含 `locateNDKClang` / `ndkClangCandidates` / `ndkVersionsDesc` / 内嵌源码常量） |
| `engine/build/07_build_vendor/assemble.go` | 去掉 `buildComposerCompatLib` 调用；`patchComposerCompatNeeded` 保留并注明「库由 `kind=shared` 模块编出」 |
| `engine/build/07_build_vendor/composercompat.go` | 注释与报错改口径：缺库时提示「device.json 的 hardware 里声明 libcdh_composer_compat」 |
| `engine/build/07_build_vendor/hal.go` | 新增 `sharedModuleKind`；`moduleTargets` 的 `case hidlModuleKind` 扩成 `case hidlModuleKind, sharedModuleKind`；`moduleSourceDigest` 增加 `@common/<rel>` 源码根（否则改 `.cc` 不会让指纹变化、会复用旧 .so），抽出 `mixTree` / `commonSourceRoots` |
| `engine/build/07_build_vendor/composercompat_test.go` | 删掉两个已无对应实现的测试（源码解析、跳过分支），只留 `replaceDynNeeded` 三例 |
| `engine/build/07_build_vendor/hal_test.go` | 新增 `TestModuleTargetsShared`（shared 产物 = `lib64/<name>`）、`TestModuleSourceDigestCoversCommon`（改 `@common` 下的 .cc 指纹必须变） |
| `engine/vendor/build/internal/halbuild/shared_test.go`（新增） | 从 07 搬来的符号断言：模块配置可加载、`root` 必须是 `@common/`、两份 `.cc` 仍带 `__libcpp_verbose_abort` / `AIBinder_Class_setTransactionCodeToFunctionNameMap` |
| `devices/manifest/xiaomi/gold/device.json` | `hardware` 追加 `{"libcdh_composer_compat": {"variant": "default", "version": "1.0"}}` |
| 文档 | `engine/vendor/build/README.md`（kind 六种 → 七种，特殊模块要点加一条）、`engine/build/07_build_vendor/README.md`（步骤 5 配套段、文件表、测试表去掉 `compatbuild.go`，补 NDK 定位已迁走） |

产物链路因此变成：`device.json hardware` → 步骤 3 `cdh-vendor hal build libcdh_composer_compat`
→ `out/vendor/android15/lib64/libcdh_composer_compat.so` → 步骤 4 按模块产物清单上树
→ `composercompat.go` 改写 `DT_NEEDED`。

## 5. 验证

全部命令与原始输出见 `log-pass.txt`。

| 项 | 结果 |
| --- | --- |
| `cdh-vendor hal list --android 15` | 43 个模块，含 `libcdh_composer_compat  shared -> lib64/libcdh_composer_compat.so` |
| `cdh-vendor hal build libcdh_composer_compat --dry-run` | 2 个编译单元取自 `hardware/common/libcxx-compat/`，链接命令 `-nostdlib … -Wl,-soname,libcdh_composer_compat.so … -Wl,--allow-shlib-undefined` |
| 实编 | `写 lib64/libcdh_composer_compat.so`，3512 B（旧 07 现编为 3400 B，差在 clang/目标版本不同） |
| `llvm-nm -D --defined-only` | `T AIBinder_Class_setTransactionCodeToFunctionNameMap`、`T _ZNSt3__122__libcpp_verbose_abortEPKcz` |
| `llvm-nm -D -u` | 只剩 `U abort`、`U vdprintf`（运行期 bionic 解析，与旧实现一致） |
| `llvm-readelf -d` | `SONAME: libcdh_composer_compat.so`、`FLAGS: BIND_NOW`，**无 DT_NEEDED**（未因改用 A35 sysroot 带进 libc/libc++ 依赖） |
| `llvm-readelf -h` | `ELF64 / DYN / AArch64` |
| 07 里还有编译器调用吗 | `grep -E "clang|NDK|ndk" engine/build --include=*.go` 仅剩注释与库名片段，无 `exec` |
| 07 干跑 gold | `编译产物: … libcdh_composer_compat@default 1 项`；`计划命令 [步骤 3]: cdh-vendor hal build libcdh_composer_compat --android 15 --out-dir …`；步骤 4 叠加层含该模块 |
| 测试 | `engine/vendor/build`：`gofmt -l` 空、`go build ./...`、`go vet ./...`、`go test ./internal/halbuild ./internal/spec` 全绿；`engine/build`：`gofmt -l ./07_build_vendor` 空、`go build ./...`、`go vet ./07_build_vendor`、`go test ./07_build_vendor`（14.06s）全绿 |

## 6. 遗留

1. **gold 未跑完整 07**：本次只做 dry-run + 单独 `cdh-vendor hal build`，装配树里
   `lib64/libcdh_composer_compat.so` 与改写后的 `DT_NEEDED` 要等下一次 07 实跑（含实机起机）复验。
   gold 装配树里现有的 `.so` 与改写过的那份 exe 仍是旧 07 现编的产物（内容等价，md5 会因编译器不同而变）。
2. **触发判据仍是文件名**：`patchComposerCompatNeeded` 只看
   `bin/hw/android.hardware.composer.hwc3-service.drm` 是否存在。应改成「ELF 的 `DT_NEEDED` 里还剩空壳
   `…@2.2-resources.so`」——m22 / cph2797 的 `_upstream` 件同样缺符号，现在无人处理；自编件若同名会被误改。
   一并处理时还要决定：`_upstream` 机型是否声明本模块（APEX 摊平的 composer 与 gold 同源，理应同样补）。
3. **`kind=shared` 目前只有一个使用者**：`libcdh_composer_compat`。若将来出现别的垫片库（如给 A14 跑
   A16 件的 binder 兼容层），直接加模块即可；不要退回「装配段现编」。
4. **目标三元组变了**：旧实现 `--target=aarch64-linux-android21` 且不带 sysroot；`cdh-vendor` 统一用
   `aarch64-linux-android35` + NDK sysroot。因链接加 `-nostdlib`，产物 `DT_NEEDED` 为空、`U` 符号仍是
   `abort`/`vdprintf`，与旧件等价；但若哪天有人给这类库加真实链接库，要先确认不会带进 A35 专有依赖。
5. **`@common/` 前缀只加在 `sharedLibs.root` / `includes`**：`staticLibs.root` 与 `externalSources.root`
   走的是 `ResolvePath`，同样能吃 `@common/`，但没有实际使用者，未加测试覆盖。

## 7. 后续结论修正（2026-11-02，原 `#118 issue-composer-compat-removal`，该条已随代码整链删除而移除，见 `../../README.md` 的「已移除条目」表）

本条遗留 2（触发判据）与遗留 3（`kind=shared` 的使用者）的结论是 **整链删除**，不是收紧判据：

- 按精确文件名逐机核对，唯一命中改写的是 **gold**，而 gold 是 A15 件 + A15 底座
  （`system/vendor` 两份 `libc++.so` 均导出 `__libcpp_verbose_abort`，`libbinder_ndk.so` 导出那个 A15 接口），
  **本来不缺符号**；真实为 0 的只有 lineage-21.0（A14）的 `libc++.so`。
- oriole（唯一 A14 机型）在 `#105/#106` 后改同代自编，缺符号在编译期由 `halbuild` 的 compat 对象补，
  不再走这条改写；m22 / cph2797 / ss9380 的 `_upstream` 件文件名不同，从未命中且各自 A16 库都导出。
- 因此 07 的 `composercompat.go`、gold `device.json` 的模块声明、vendor 侧的 `kind=shared` 与 `@common/`
  前缀、模块 json 与模块根全部删除；本文档保留为「这条通路曾经怎么成立」的取证。

运行态复验（gold 装配树回到厂商原样后起机）原记在 `#118` 的遗留 1；`#118` 已移除，条目去向见 `../../README.md` 的「已移除条目」表。
