# oriole-A14 显示栈三件从 07 手写现编迁到 cdh-vendor 的 kind=hidl

- 机型：google / oriole（Pixel 6），android 14（lineage-21.0 底座）
- 日期：2026-11-01（终态实例 17，patch.img `e8567516ac7f`；首轮实例 15 / 二轮实例 16
  的差异见 §5 的轮次说明）
- 前序：#105（自编 drm_hwcomposer 打通显示栈）、#103（静态树不放字节）

## 1. 问题

#105 让 A14 云机的显示栈从源码编起来后，编译逻辑落成了**引擎之外的一份特例实现**：

- `engine/vendor/aosp/a14/`：658 文件 / 7.0 MB 的私有源码树（`external/drm_hwcomposer`
  全树 + 自写服务壳 + 手工裁剪的「头文件闭包」6 个目录）；
- `engine/build/07_build_vendor/hwcbuild.go`：418 行的现编配方（27 个编译单元、
  编译/链接参数、产物名按 `ro.board.platform` 现算、NDK 版本挑选）。

它和仓库里其它 16 个 HAL 的形态完全不同：那些都是
`engine/vendor/hardware/<模块>/{src, vendor}` + `build/modules/<模块>.json`，由
`cdh-vendor hal build` 统一编译，07 只负责「按 device.json 的清单调编译器，再把
产物清单 copy 进装配树」。显示栈这一条却绕开引擎，把编译、依赖准备、产物落盘全部
塞回 07。

## 2. 根因

1. **cdh-vendor 的 spec 缺少这一类模块**。现有 kind 共 6 种（`aidl` / `pure-c` /
   `v412` / `gpu` / `cargo` / `prebuilt`），没有一种能表达「HIDL 头 + 上游实现树 +
   多个 .so 产物」。显示栈三件（1 个服务 + 2 个 .so）恰好都落在缝里，所以只能写进 07。
2. **写进 07 的代价是脱离引擎的三套机制**：
   - 依赖自愈（`deps.Prepare*`）：a14 得靠人先把头闭包摆好；
   - 源码指纹（`needsBuild`）：07 只按 device.json 的模块算指纹，a14 里的头是生成物，
     换 NDK / 换头就悄悄用旧件；
   - 产物清单（`moduleTargets`）：`.so` 不进清单，装配段 copy 不到就得再写一段特例。
3. **两份生成物只能手工维护**：HIDL 头（`include-composer/hidl-gen`）是当年在容器里
   用 hidl-gen 编出来的文本，`graphics.common` 的 AIDL 头（`include-drm/graphics-common`）
   是 `tools/gen_hidl_graphics.py` 从 `.hal` 还原的枚举副本 —— AOSP 一升版就得重做一遍。
   而 A14 的 `frameworks/native/libs/ui/include/ui/GraphicTypes.h` 直接 include
   `<aidl/android/hardware/graphics/common/BlendMode.h>`，绕不过去。
4. **绑死主机 NDK 26.x**：a14 的 `00-libcxx-config` 是自己写的 `__config_site`
   （把 libc++ 命名空间从 `__ndk1` 改回 `__1`），并且只能挑「没有
   `_LIBCPP_HARDENING_MODE`」的 NDK；NDK 28+ 直接编不了，步骤 4c 只能报错停下。

一句话：**HAL 的位置、依赖的位置、自写代码的位置都没有错，错在这条链路不在引擎里。**

## 3. 证据

失败与通过日志见同目录 `log-fail.txt`（9 条）与 `log-pass.txt`（5 段）。关键几条：

- `log-fail.txt ⑦`：07 全绿却装出 `hwcomposer.virt.so` —— 步骤 3 读的是步骤 2 铺的
  lineage `build.prop`（`ro.board.platform=virt`），而把它改成 `gs101` 的是步骤 6 的
  属性收口；运行期 `libhardware` 按 `gs101` 找，名字对不上。
- `log-fail.txt ⑧`：`.so` 里 `U std::__1::__libcpp_verbose_abort`，而 lineage-21 的
  `libc++.so` 2159 个动态导出里没有这个符号 —— 旧的 `compatObjects` 只给服务补，
  `.so` 走另一条链接函数，BIND_NOW 下会被直接拒绝加载。
- `log-fail.txt ③`：`libc.so / libm.so / libdl.so` 在 lineage 的 `system_a.img` 里是
  符号链接（`debugfs stat` → `Type: symlink`），`dump` 取不到实体。
- `log-pass.txt ④`：实例 16 `init: started service 'vendor.hwcomposer-2-4' has pid 711`、
  `HidlServiceManagement: Registered android.hardware.graphics.composer@2.4::IComposer/default`、
  `boot_completed=1` @ 33.86 s、`CANNOT LINK` 0 次。
- `log-pass.txt ⑤`：宿主 `cgl-headless 大帧` 1509 次，VNC 抓帧桌面完整（`boot-desktop.png`）。
- `log-fail.txt ⑨`：改了 halbuild 之后 07 仍打 `[skip] … 源码指纹一致`，`.so` 里
  `__libcpp_verbose_abort` 还是 `U` —— 复用判据只算模块源码与模块 JSON，不含
  cdh-vendor 自己的编译逻辑，且 `cdh device build --refresh` 没透传到 07。

## 4. 改动

**cdh-vendor 新增 `kind=hidl`（把 v412 那条「HIDL 头 + 上游树」的路泛化）**

| 文件 | 改动 |
| --- | --- |
| `vendor/build/internal/spec/spec.go` | `Hidl{packages, expectHeaders, aidlRoots, repos, linkLibs}`；`SharedLib{name, soname, dir, root, sources, includes, defines, linkLibs}`；`Service.soname`；`kind=hidl` 的必填校验 |
| `vendor/build/internal/deps/hidl.go`（新） | `PrepareHIDLModule` / `CheckHIDLModule`（仓 → hidl-gen → HIDL 头 → 链接期库，幂等）；`ModulePlatformRepos` / `PlatformReposAll`（模块声明的上游仓并入 `deps --platform`） |
| `vendor/build/internal/halbuild/hidl.go`（新） | 三段式：hidl-gen 头 + **上游 AIDL 现场生成**（多 `-I` 根、跳过 `aidl_api/` 冻结快照）→ 多共享库 → 服务；`expandProps` 展开产物名占位；`publishHidl` 按 `dir/name` 落位 |
| `vendor/build/internal/halbuild/libcxx_compat.go` | `compatObjects(abiDirs)` 改成由调用方给库目录；`buildSharedLibs` 也链入 compat 对象；兼容源码移出 Go 字符串，改读 `hardware/common/libcxx-compat/`（见下） |
| `vendor/build/main.go` | `--prop KEY=VALUE`（可多次），`hal list` 对 hidl 显示包数与库名 |

**模块目录（用户要求的形态：HAL 在 `hardware/`，依赖在 `platform/`，只放自写代码在 `src/`）**

```
engine/vendor/hardware/android.hardware.graphics.composer/
├── README.md          # 目录约定、产物→源码出处、头文件来源对照表
├── src/service.cpp    # 唯一自写代码（HIDL 2.4 服务壳，91 行）
└── （无 vendor/ 静态树：rc / VINTF 属设备集成，留在 manifest/google/oriole）
engine/vendor/hardware/common/libcxx-compat/libcxx_verbose_abort.cc
                       # 平台底包补洞用的 C++ 源码，与 bionic15 的 crt、platform-shim
                       # 的头同类；halbuild 只判定「要不要编、编到哪」
engine/vendor/build/modules/android.hardware.graphics.composer.json
    hidl.packages 13 个（hidl base/manager 1.0~1.2/memory/safe_union + graphics.common 1.0~1.2
                       + composer 2.1~2.4）、expectHeaders 104
    hidl.aidlRoots 2 个（hardware/interfaces/{common,graphics/common}/aidl）
    hidl.repos 6 个（drm_hwcomposer / libdrm / frameworks:native / libhardware / libhidl / libhwbinder）
    sharedLibs 2 个：hwcomposer.${ro.board.platform}.so（soname hwcomposer.drm.so，lib64/hw）
                     android.hardware.graphics.composer@2.1-resources.so（lib64）
    services 1 个：android.hardware.graphics.composer@2.4-service（bin/hw）
```

**07 侧退化**

| 文件 | 改动 |
| --- | --- |
| `07_build_vendor/hal.go` | `halCommand` 对 hidl 追加 `--prop`；`moduleTargets` 支持 hidl（多 .so + 占位展开）；`refreshModuleTargets` 在步骤 3 用真实属性重算产物清单；`moduleProps` 叠基线清单 `vendor.build-prop-sets` |
| `07_build_vendor/plan.go` | plan 阶段产物清单仍带占位（文件不存在），注释说明由步骤 3 重算 |
| `07_build_vendor/assemble.go` | 删掉步骤 4c `buildDisplayStack` 调用（4 与 4b 的 compat 库不变） |
| `07_build_vendor/hwcbuild.go` | **删除**（418 行手写编译逻辑） |
| `manifest/google/oriole/device.json` | `hardware` 增 `android.hardware.graphics.composer`（variant default，2.4） |
| `manifest/google/oriole/vendor/etc/{init,vintf/manifest}/hwc3-drm.{rc,xml}` | **移进模块**：`engine/vendor/hardware/android.hardware.graphics.composer/vendor/etc/…`，由 `publishVendor` 并入产物根、再由 07 按模块清单装配；机型静态树清空（0 个文件） |

**删除**：`engine/vendor/aosp/a14/`（658 文件 / 7.0 MB）。头文件闭包与生成物全部改为
构建期产出（HIDL 头由 `hidl-gen`、AIDL 头由 `aidl`）；来源对照表保留在模块 README 里。

## 5. 验证

1. **代码**：`gofmt -l` 干净，`go vet ./...` + `go test ./...`（vendor/build 5 个包、
   07_build_vendor 含新增 `TestModuleTargetsHidl`）全绿。
2. **cdh-vendor 单跑**：HIDL 头 104 个、上游 AIDL 3 + 28 个、三件产物一次编出；
   `hwcomposer.gs101.so` 的 SONAME 仍是 `hwcomposer.drm.so`，服务 `DT_NEEDED` 里
   没有 `hwc2on*` / `@2.2-resources`。
3. **流水线**：07 → 09 → 10 → 11 全部 EXIT=0，patch.img `e8567516ac7f`，691 条目
   回读一致；三件产物在「HAL 产物根」与「装配树」里 sha256 逐件相等
   （`0b93f744…` / `2bd37698…` / `7f305c06…`），rc / VINTF 两件同样逐字相等。
4. **起机（实例 17）**：`boot_completed=1` @ 32.23 s、`vendor.hwcomposer-2-4` 只启动
   一次（pid 714）、`Registered …composer@2.4::IComposer/default`、`CANNOT LINK` 0 次、
   `lshal` 里 composer 2.1~2.4 全在；设备上 `/vendor/etc/init/hwc3-drm.rc` 正是模块树
   那份（含 `publishVendor` 注释）。
5. **画面**：宿主 `cgl-headless 大帧` 1452 次，VNC 抓帧桌面 / 状态栏 / 图标 /
   导航栏完整（`boot-desktop.png`，1088x2400）。

> 三轮数字差异：首轮（实例 15，patch `d25f85d08df6`）装上去的 `.so` 其实是**改
> halbuild 之前**的旧产物（`__libcpp_verbose_abort` 仍是 `U`，靠服务端导出的同名
> 符号才加载得起来），见 §6 第一条；二轮（实例 16，patch `32a5213431d2`）重编后
> 两个 `.so` 都自带定义（`T`）；三轮（实例 17，patch `e8567516ac7f`）只是把 rc /
> VINTF 从机型静态树下放到模块树（注释润色），`.so` 与服务字节不变。

## 6. 遗留

- **07 的复用判据不含 cdh-vendor 的编译逻辑**（`needsBuild` 只算模块源码树 +
  模块 JSON 的 digest）。改 halbuild / spec / deps 之后 07 会打 `[skip] … 源码指纹
  一致` 并拷旧产物，而且 `cdh device build --refresh` 当前没有透传给 07
  （`RunStage` 不读 Refresh）。改完引擎必须手工跑一次
  `cdh-vendor hal build <模块>`（或删产物根里的目标文件）再走 07。
- **产物名占位只支持 `ro.board.platform` 一个键**（`propKeys`）。别的 HIDL 模块若要
  用别的属性命名，需要同时改 `propKeys` 与 `moduleProps`；也可以把键改成模块 JSON 声明。
- **`expectHeaders` 是精确值**（当前 104）。增删 HIDL 包要同步改这个数字，否则
  `deps` 会以「HIDL 生成数量错误」停下 —— 这是有意的守卫（防止 hidl-gen 静默少生成）。
- **rc / VINTF 下放后产物根成为唯一的「声明口径」**：`publishVendor` 按模块合并
  `<模块>/vendor/**` 到共享产物根，同路径异内容直接报 `vendor 树冲突`。将来另一个
  机型要不同的 rc（比如不同的 group / task profile）时，只能靠模块变体或换文件名，
  不能靠改一个文件两边通吃。另外 `pruneVendorDir` 只清顶层目录，改文件名会在装配树
  里留旧件 —— 所以本次保名迁移（`etc/init/hwc3-drm.rc`、`etc/vintf/manifest/hwc3-drm.xml`）。
- **只迁了 oriole-A14 这一份**。若 A15/A16 线也要自编显示栈，各版本的上游树差异
  （A15 起是 AIDL composer3）需要各自的模块 JSON 与 service 实现，未做。
- **`07` 的 plan 阶段产物清单仍打印 `${ro.board.platform}` 原样**（步骤 2 之前没有
  `build.prop`），只是显示问题，步骤 3 会重算；若要更早展开，需把属性来源前移到 plan。
- #105 的运维类遗留不变：`screencap` 在 oriole 上挂住（取证只走 VNC）、
  `dumpsys SurfaceFlinger` 直调返回 `FAILED_TRANSACTION`、`drmserver` 32 位
  `Exec format error` 每 5 分钟循环（与显示无关）。
- 本仓不是 git 仓库，`engine/vendor/aosp/a14` 的删除不可回退；其内容已可从
  `deps --platform` 与模块 `src/` 完整重建。
