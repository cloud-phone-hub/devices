# android.hardware.graphics.composer（HIDL 2.4 显示栈）

云机的显示栈 HAL：HIDL composer 2.4 服务壳 + composer 资源库 + `hwcomposer`
实现体（drm_hwcomposer）。三者都由 `cdh-vendor hal build` 从源码编出，产物落
`engine/out/vendor/android<版本>/`，再由 07 装配进 vendor 分区。

## 目录里为什么只有一个 .cpp

| 产物 | 源码 | 出处 |
| --- | --- | --- |
| `android.hardware.graphics.composer@2.4-service` | `src/service.cpp` | **本模块自写**（见文件头注释） |
| `android.hardware.graphics.composer@2.1-resources.so` | `graphics/composer/2.1/utils/resources/ComposerResources.cpp` | `@platform/hardware/interfaces`（AOSP 上游） |
| `hwcomposer.${ro.board.platform}.so` | `external/drm_hwcomposer` 的 27 个编译单元 | `@platform/external/drm_hwcomposer`（AOSP 上游） |

上游源码不进仓库：`cdh-vendor deps` 按 `hidl.repos` 从 gitiles 拉到
`engine/vendor/out/android<版本>/aosp/platform/`，HIDL 头由 `hidl-gen` 现场生成
（包清单与期望头数写在 `build/modules/android.hardware.graphics.composer.json`）。
本目录只放「我们要维护的那部分代码」，即服务壳。

`ro.board.platform` 取自装配树的 `vendor/build.prop`，由 07 以
`--prop ro.board.platform=<值>` 传给 cdh-vendor：`libhardware` 的 `hw_get_module`
会按 `hwcomposer.<ro.hardware>.so` → `.<ro.product.board>.so` →
`.<ro.board.platform>.so` → `.default.so` 的顺序查找，取 platform（Pixel 6 是
`gs101`）与既有产物同名、且不随机型名变化。SONAME 保持 `hwcomposer.drm.so`。

## 静态树：rc 与 VINTF 跟着模块走

```
vendor/etc/init/hwc3-drm.rc              # service vendor.hwcomposer-2-4
vendor/etc/vintf/manifest/hwc3-drm.xml   # HIDL 2.4 IComposer/default
```

这两件是 HAL 本体的配套声明（服务名 / 接口 / 权限组、VINTF 注册），跟产物一荣俱荣，
所以放在模块里而不是机型静态树：`cdh-vendor hal build` 结束时 `publishVendor` 把它们
按相对路径并进产物根（`<产物根>/etc/…`），07 再按 `device.json` 的模块清单装配进
vendor 分区 —— 换机型、换底包不必再搬两份文件，也不会“装了 HAL 忘配 rc”。

产物根是全平台共用，同路径异内容会报 `vendor 树冲突`：将来若另一机型要不同 rc，
靠模块变体或换文件名解决，别靠手改。

## 依赖边界

- **代码依赖**（要编译的上游树）：`external/libdrm`、`external/drm_hwcomposer`、
  `frameworks/native`、`hardware/libhardware`、`hardware/interfaces`、
  `system/libhidl`、`system/libhwbinder`、`system/core`。
- **链接期库**（`hidl.linkLibs`）：从 lineage 底包镜像里提取到
  `abi/graphics-composer/system/lib64`，只用于链接、不发布。运行期由基线清单的
  `vendor.files`（`mode: add` + `from: official`，取官方固件同代 vendor 分区）把
  composer@2.1~2.4 等同代库借进 `/vendor/lib64`。
  （2026-11-02 前写的是 `vendor.lifts` + `from: lineage:system`；跨分区借件已禁止，
  且官方 `vendor_a.img` 逐件实测存在，见 `devices/evidence/common/issue-vendor-files-unify/`。）
- 两个 HWC1 adapter（`libhwc2on1adapter` / `libhwc2onfbadapter`）与
  `@2.2-resources` 都不需要：服务壳不走 `HwcLoader::load()` 的 hwc1/fbdev 回落，
  源码里零引用。

## 构建

```
cdh-vendor deps --android 14 --platform     # 拉上游仓 + hidl-gen + 链接期库
cdh-vendor hal build graphics-composer --android 14 --prop ro.board.platform=gs101
```

`hal build` 自带依赖自愈：缺上游仓 / HIDL 头 / 链接期库时会现场补齐。

## 头文件与库来源对照

模块 JSON 的 `includeDirs` / `repos` / `aidlRoots` 每一项都对应 platform 树里的
一个上游目录（全部来自 `android-14.0.0_r67`），排障时按这张表倒查：

| 需要的东西 | 模块 JSON 里的来源 |
| --- | --- |
| libc++ 的 ABI 命名空间 `std::__1` | 工具链的 `hardware/common/libcxx-platform/__config_site`（NDK 默认是 `__ndk1`，与设备 `libc++.so` 对不上） |
| `cutils/`、`utils/`、`log/`、`base/`、`sync/` | 工具链 `PlatformExtras`（`system/core/{libcutils,libutils,libsync,libsystem}`、`system/libbase`、`common/platform-shim` 的 `log/log.h`） |
| `hardware/{hwcomposer2,gralloc,hardware}.h` | `hardware/libhardware/include` |
| `hidl/*`（base/transport）、`hwbinder/*` | `system/libhidl/{base,transport}/include`、`system/libhwbinder/include` |
| `binder/*`、`ui/*`、`nativebase`、`nativewindow`、`arect`、`math` | `frameworks/native/libs/{binder,ui,nativebase,nativewindow,arect,math}/include` |
| `drm/*.h`、`xf86drm*.h`、`gralloc_handle.h` | `external/libdrm/{include/drm,android}` + 其根目录 |
| `aidl/android/hardware/graphics/common/*.h` | `aidlRoots` 现场生成：`hardware/interfaces/{common,graphics/common}/aidl`（A14 起 graphics.common 是 AIDL） |
| `android/hardware/graphics/composer/2.x/*.h`、`android/hidl/{base,manager,memory,safe_union}/*` | `hidl.packages` + `expectHeaders`，`hidl-gen` 现场生成 |
| `composer-hal/2.x`、`composer-command-buffer/2.x`、`composer-resources/2.x`、`composer-passthrough/2.x` | `hardware/interfaces/graphics/composer/2.{1,2,3,4}/utils/*/include` |

两份「生成物」（HIDL 头与 graphics.common 的 AIDL 头）不在仓库里，构建时由
`hidl-gen` / `aidl` 主机工具现做：`aidl` 取 SDK build-tools 那份，`hidl-gen` 由
`cdh-vendor deps` 现场编（AOSP prebuilts 里没有它的预编译件）。
