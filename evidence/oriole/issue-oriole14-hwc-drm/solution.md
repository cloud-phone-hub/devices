# Issue: oriole A14 显示栈改回 lineage-21 时代（HIDL composer 2.4 + 自编 drm_hwcomposer）—— 解决方案

- 设备: Google Pixel 6 (oriole)，AP2A.240905.003.F1
- 基座: lineage-21.0 virtio 底包（`rom.lineage.version = 21.0`，A14）
- 状态: **已闭环**（2026-11-01，实例 14，patch.img sha256 `06e93c8f36a5`）
  `sys.boot_completed=1`、`init.svc.vendor.hwcomposer-2-4 = running`、
  SurfaceFlinger 稳定输出、桌面完整可见；显示栈三件（`hwcomposer.gs101.so`、
  `composer@2.1-resources.so`、`composer@2.4-service`）**全部改为构建期现编**，
  `vendor.lifts` 零条取官方 A14，机型静态树零字节

---

## 1. 问题

按「android14 只用 lineage-21 自己的件」的约定，把此前从 A15（`lineage:22.2`）
借来的 graphics 整套撤掉后，oriole A14 起机没有可用的 HWC：

- 第一次：`vendor.hwcomposer-2-4` 以 **status 127** 反复退出（4 分钟 4 次），
  屏幕上没有任何显示。
- 第二次（执行位修好后）：服务能常驻，RenderEngine 也初始化成功
  （Mesa / virgl / GLES 3.0），但 SurfaceFlinger 拿不到热插拔事件，5 秒一次
  `F SurfaceFlinger: Initial display configuration failed: HWC did not hotplug`，
  `init` 反复 `restart surfaceflinger`，整机停在黑屏。

需要回答的是：A14 的 oriole 到底该用哪一代 graphics，缺的件应该从哪里来。

## 2. 根因

三层，彼此独立：

**(a) lineage-21 里根本没有 composer 服务。**
逐分区清扫确认：`system_a.img` 的 `/system/apex` 30 个 APEX 里没有任何
graphics/hwcomposer 载荷；vendor 分区（cuttlefish 风格，`CuttlefishWifiOverlay`
+ `lineage_virtio_arm64only` RRO）只有
`bin/hw/android.hardware.graphics.allocator-service.minigbm` 与
`lib64/hw/{gralloc,mapper}.minigbm.so`。官方 A14 vendor 里确实带着
`android.hardware.graphics.composer@2.4-service`，但它 dlopen 的
`hwcomposer.gs101.so` 是 Exynos 绑定件，在 QEMU + virtio-gpu 上不可用。

**(b) 官方镜像里 `debugfs dump` 会丢执行位。** 借来的服务壳在镜像里是 `Mode: 0755`，
落盘后变成 `0644`（dump 按宿主 umask 写），init `execv` 直接 `EACCES` —— 这就是
status 127。装配树的元数据来自"装配后文件的真实模式"，于是 0644 被如实写进
`vendor-meta-rules.txt`，问题被固化进镜像。

**(c) 自编 `hwcomposer.drm.so` 的 buffer 元数据通路被桩掉。** drm_hwcomposer 的
`BufferInfoGetter::GetInstance()` 有两条路：`BufferInfoMapperMetadata`
（IMapper-4.0 元数据，需要 AIDL/HIDL 生成头，编译门槛高）与
`BufferInfoLibdrm`（老路）。此前为了先编过，把前者换成了返回 nullptr 的桩。结果
`GetInstance()` 退到 libdrm 老路，而它只接受名为 `"GBM Memory Allocator"` /
`"DRM Memory Allocator"` 的 gralloc；本机 minigbm 的模块名是 `"CrOS Gralloc"`：

```
INFO: Using CrOS Gralloc gralloc module: Chrome OS
ERR: Gralloc name isn't valid: Expected: "GBM Memory Allocator" or "DRM Memory Allocator", Actual: "CrOS Gralloc"
ERR: Failed to initialize BufferInfoGetter
```

`ResourceManager::Init()` 因此在 attach 任何 connector 之前就 return —— 服务端一个
显示都没有，自然不会给 SF 发 hotplug。这也解释了「服务 running、RenderEngine 正常、
但 SF 报 HWC did not hotplug」这个反直觉组合。

顺带排除的误判：drm_hwcomposer 自带 `utils/Log.h` 把 `ALOGx` 宏重定向到
`printf("ERR: " …)`，所以它**从不出现在 logcat** 里 —— 一开始「服务在跑却一点日志都
没有」不是被吞了，而是走了 stderr。

## 3. 证据

- 失败链与三段日志：`log-fail.txt`（status 127 / `HWC did not hotplug` /
  探针的 `Failed to initialize BufferInfoGetter`）。
- 通过链：`log-pass.txt`（0755 规则行、09/11 EXIT=0、探针
  `HOTPLUG display=0 conn=1`、`boot_completed=1`、2164 帧、VNC 抓帧）。
- 画面：`boot-desktop.png`（实例 11）、`boot-desktop-selfbuilt.png`（实例 14，显示栈
  三件均为现编）。分别为 1088x2400，桌面 + 状态栏 + 导航栏，密度 420。
- 现编失败链（本题第二轮）：`log-fail.txt` ④ —— 过期 `cdh` 二进制把 `lineage:system`
  当成版本、`libsync.so` 取件路径漏 `system/` 前缀、NDK 29 的 `_LIBCPP_HARDENING_MODE`、
  链命令缺 `--target=` 落到 ld64、`-L` 顺序让 NDK 的 libc++ 抢了平台那份。
- 现编通过链：`log-pass.txt` ⑥ —— 三行 `现编 -> …`、服务壳 `DT_NEEDED` 里
  `hwc2on*` / `@2.2-resources` 命中 0、设备上三件 sha256 与装配树逐件相等。
- 关键对照：探针同一份二进制，修复前后只差 `hwcomposer.gs101.so` 的内容 ——
  修复前 `Failed to initialize BufferInfoGetter` 且无 hotplug；修复后两条消失，
  直接 `>>> HOTPLUG display=0 conn=1`。
- Chrome 对照（结论见遗留 3）：`aapt2 dump badging` 声明 `native-code: 'arm64-v8a'`、
  `alt-native-code: 'armeabi-v7a'`；但 apk 的 `lib/` 只有 `arm64-v8a/libplaceholder.so`（0 B）
  + `armeabi-v7a` 六个库，**两个 ABI 都没有 `libmonochrome.so`**；官方
  `system_a.img:/system/lib64` 与 `product_a.img:/lib64` 同样没有。lineage-21.0 的
  `product_a.img:/app` 只有 17 项，浏览器是 `Jelly`，压根没有 Chrome。
- 崩溃现场原文（实例 14 qemu.log）：`cr_LibraryLoader: Loading monochrome from within
  /data/app/…/Chrome.apk` → `UnsatisfiedLinkError: dlopen failed: library
  "libmonochrome.so" not found`。

## 4. 改动

1. **`engine/build/07_build_vendor/replace.go`**：`extractImageExt4` 在单文件
   `debugfs dump` 成功后，从同一份 `debugfs stat` 文本里取 `Mode:` 并
   `os.Chmod` 回目标文件（`imgStatInodeRe` 复用 `metacheck.go` 里已有的正则）。
   目录分支（`rdump`）不动 —— 目录里目前只放配置件，可执行件单独借。
   这一条对所有 ext4/erofs 借件生效，不只 composer。
2. **自编 drm_hwcomposer（AOSP android-14.0.0_r75，`external/drm_hwcomposer`）**：
   产出 `hwcomposer.drm.so`，27 个编译单元全过，`DT_NEEDED` 只有
   `libui/libcutils/libutils/libhardware/libdrm/libbase/libc++/libsync/liblog`
   这些平台库。编译脚本 `engine/out/tmp/build_dhc14.sh`，生成头脚本
   `engine/out/tmp/gen_hidl_graphics.py`。
   为了让 `BufferInfoMapperMetadata.cpp` 真正编过：AIDL 头用 SDK build-tools 的
   `aidl --lang=ndk --structured --stability=vintf` 生成（93 个头），HIDL 的
   `graphics/common/{1.0,1.1,1.2}/types.h` 用脚本从 `types.hal` 还原
   （枚举一律 `enum class` + `static_cast<底层类型>`，否则 `ColorMode`/`Dataspace`
   的 `STANDARD_BT709` 会撞名、`PixelFormat::VENDOR_MASK` 会窄化报错）。
3. **基线清单**：`vendor.lifts` 改为 17 条全部不从 A15 取，`lineage:22.2` 条目清零；
   服务壳 / VINTF 用 HIDL 2.4：
   `vendor/etc/init/hwc3-drm.rc`（`service vendor.hwcomposer-2-4
   /vendor/bin/hw/android.hardware.graphics.composer@2.4-service`）+
   `vendor/etc/vintf/manifest/hwc3-drm.xml`（`<version>2.4</version>`、
   `IComposer/default`）。
4. **机型静态树里的二进制已撤掉**：`manifest/google/oriole/vendor/lib64/hw/
   hwcomposer.gs101.so`（自编 328 KiB）删除，静态树只剩 `etc/init/hwc3-drm.rc` 与
   `etc/vintf/manifest/hwc3-drm.xml` 两份可读文本（#103 的「静态树不放字节」）。
5. **借件来源细分到分区（`lift.go`）**：底包把这一代 graphics 库放在 system，
   而 vendor 侧进程看不到非 LLNDK 的 `/system/lib64`，所以新增 `lineage:<分区>` /
   `lineage:<版本>:<分区>` 选择器（`liftSelector{Kind,Version,Partition}` +
   `lineageLiftSource(plan, version, partition)`，分区 → `super/unpack_<分区>` 树
   → `super/<分区>_a.img`）。目标路径用 `target` 与来源分开（`system/lib64/x.so` ->
   `lib64/x.so`）。据此 12 条改为 `from: lineage:system`：composer@2.1/2.2/2.3/2.4、
   libfmq、libui、allocator@2.0/3.0/4.0、mapper@2.0/2.1/3.0，全部从本机底包
   `lineage-21.0/super/system_a.img` 抽取。
6. **AOSP A14 源码入库（`engine/vendor/aosp/a14`，658 文件 / 7.0 MB）**：
   `external/drm_hwcomposer` 全树（27 个编译单元）+ 自写 `sources/composer/service.cpp`
   + `ComposerResources.cpp` + 一份裁剪过的头文件闭包（22 个编号 `-I` 根、
   `include-composer/{hidl-gen,composer-utils}`、`include-drm/graphics-common`）。
   两份生成头（hidl-gen 产物、graphics.common 1.x 类型）作为可读文本随仓库落盘，
   构建期不再依赖主机侧 hidl-gen / aidl。来源对照见该目录 `README.md`。
7. **新增步骤 4c 显示栈现编（`build/07_build_vendor/hwcbuild.go`）**：装配段用 NDK
   clang++ 从上面那份源码编出三件并直接落装配树，产物名与 `DT_NEEDED` 全部现算：
   `lib64/hw/hwcomposer.<ro.board.platform>.so`（= `hwcomposer.gs101.so`）、
   `lib64/android.hardware.graphics.composer@2.1-resources.so`、
   `bin/hw/android.hardware.graphics.composer@2.4-service`（0755）。触发条件：装配树
   里没有 composer 服务壳（gold / m22 / ss9380 自带 composer，不受影响）。
8. **基线清单清空官方借件**：删掉原 17 条里的 5 条 `from: vendor`
   （服务壳 + `@2.1-resources` + `@2.2-resources` + `libhwc2on1adapter` +
   `libhwc2onfbadapter`），`vendor.lifts` 现为 12 条、全部 `lineage:system`。
   两个 adapter 与 `@2.2-resources` 是**彻底不需要**而非被替代：服务壳不走
   `HwcLoader::load()` 的 hwc1/fbdev 回落，源码里没有对它们的引用；
   `@2.2-resources` 实测 0 个导出符号。
9. **现编的三处工具链口径**（都是踩过坑才定下来的）：
   ① 编译器不能用「最新 NDK」——NDK 28 起 libc++ 强制 `_LIBCPP_HARDENING_MODE`，
   与 AOSP 14 的平台 `__config_site` 直接冲突，故 `locateDisplayStackClang()` 只挑
   `__config_site` 里没有该宏的 NDK（本机 26.1）；② 链命令必须带 `--target=`
   （裸 `clang++` 不带会落到宿主 `ld64`，报 `unknown argument '-soname'`）；
   ③ `-L` 顺序必须是「装配树平台库 → 底包 system → NDK sysroot」，否则 `-lc++`
   命中 NDK 的 `std::__ndk1`，服务壳里所有 `std::__1` 实例变成未定义符号。

## 5. 验证

- `07 --refresh` / `09 --refresh` / `11 --refresh` 全 `EXIT=0`；
  规则表 `bin/hw/android.hardware.graphics.composer@2.4-service file 0755`；
  patch 694 条目回读一致。
- 实例 11：`sys.boot_completed=1`（32.79 s）、`ro.build.version.release=14`、
  服务 `running`、`surfaceflinger`/`composer@2.4-service` 各一个 pid 且不变
  （无崩溃循环）、宿主 `cgl-headless 大帧` 2164 次、VNC 画面正常。
- guest 内探针：`HOTPLUG display=0 conn=1`，`Failed to initialize BufferInfoGetter` 0 次。
- **借件收敛后复验（实例 12，patch `3c5a75f9061a`）**：12 条件改从底包 `system_a.img`
  抽取后重跑 07→09→11，`sys.boot_completed=1`、`init.svc.vendor.hwcomposer-2-4=running`、
  SF pid 732、宿主大帧 1499 次、VNC 画面正常 —— 说明那批库与服务端同代可用。
- **显示栈现编后复验（实例 14，patch `06e93c8f36a5`）**：
  - `07 --refresh` 里 `[步骤 5b] 输入: lineage-21.0 底包 system（借件集合 12 项）`，
    12 条全部 `[replace] … <- lineage-21.0 底包 system/system/lib64/…`；紧接着
    `[步骤 4c] 显示栈现编` 打出三行 `现编 -> …`，`EXIT=0`。
  - 三件的 `DT_NEEDED` 实测：服务壳 19 项（composer@2.1~2.4 + `@2.1-resources` +
    libhidlbase/libbinder/libutils/libcutils/libbase/libfmq/libhardware/libsync/libui/
    libc++/libc/libm/libdl/liblog），**`hwc2on1adapter` / `hwc2onfbadapter` /
    `@2.2-resources` 命中数 0**；`hwcomposer.gs101.so` 只靠
    libui/libcutils/libutils/libhardware/libdrm/libbase/libc++/libsync/liblog/libm/libdl/libc。
  - 09（691 条目回读一致）、11 均 `EXIT=0`；起机后设备上三件与装配树 sha256 前 16 位
    **逐件相等**（`549d3b1e13088545` / `74bcb26ede84e312` / `cb3d26650b60c967`）。
  - `sys.boot_completed=1`、`init.svc.vendor.hwcomposer-2-4=running`、SF pid 730、
    composer 服务 pid 716（各一个、无重启）、宿主 `cgl-headless 大帧` 1297 次；
    logcat 里 `cannot link` / `dlopen failed` 只有 Chrome 那条（见遗留 3）。
  - 画面：`boot-desktop-selfbuilt.png`（1088x2400，桌面/状态栏/导航栏，密度 420）。

## 6. 遗留

1. ~~静态树里仍有一个二进制~~ **已闭环**：`manifest/google/oriole/vendor/lib64/hw/
   hwcomposer.gs101.so` 已删除，改由步骤 4c 现编；静态树只剩 rc / VINTF。
2. ~~仍有 5 条取官方 A14~~ **已闭环**：服务壳与 `@2.1-resources` 现编，
   `@2.2-resources` 与两个 adapter 直接不要；`vendor.lifts` 现为 12 条、
   全部 `lineage:system`。
3. **Chrome 起不来（结论第二次修正：既不是「lineage 没带」，也不是「32 位包」）**：
   ① lineage-21.0 的 `product_a.img` 里确实没有 Chrome —— `/app` 下 17 项，浏览器是
   `Jelly`；
   ② 本机 `/product` 不是 lineage 的 product，而是官方 Pixel A14 固件的
   `product_a.img`（`super-info.txt` 记 `product_a <- out/roms/google/oriole/AP2A.240905.003.F1/
   super/product_a.img`），它把 Chrome 预置成 `/product/app/Chrome/Chrome.apk.gz`
   （解压 38 792 056 B，sha256 `583ea45ebff36c3d`），首启解到 `/data/app`；
   ③ 那份 Chrome 122.0.6225.0 是**壳包**：`aapt2 dump badging` 明写
   `native-code: 'arm64-v8a'`、`alt-native-code: 'armeabi-v7a'`，可 apk 里
   `lib/arm64-v8a/` 只有一个 **0 字节 `libplaceholder.so`**，`lib/armeabi-v7a/` 六件里也
   **没有 `libmonochrome.so`**（只有 `libmonochrome_{cablev2_authenticator,stack_unwinder,
   test_dummy}_partition.so` + `libchromium_android_linker.so` + `libelements.so` +
   `liboptimization_guide_internal.so`）；官方 `system_a.img:/system/lib64`、
   `product_a.img:/lib64` 同样都没有这个库。因此失败点不是 ABI（ABI 不匹配会报
   `is 32-bit instead of 64-bit`），而是 loader 直接从 apk 里取主库：
   `cr_LibraryLoader: Loading monochrome from within /data/app/…/Chrome.apk` →
   `UnsatisfiedLinkError: dlopen failed: library "libmonochrome.so" not found`。
   底包 arm64only（`abilist=arm64-v8a`、`abilist32` 空、`zygote64`）只是让
   「`/data/app/…/lib/arm64` 为空目录」这件事无从补救，并非首因。
   修法：离线装一份完整 arm64 Chrome、或让 Play / Chrome 模块下发主库、或干脆不预置
   Chrome（换 `Jelly` 或其他 arm64 浏览器）。属产品选型，与本次显示栈无关。
4. `screencap` 在本机型会挂住、`dumpsys SurfaceFlinger` 直调返回
   `FAILED_TRANSACTION`（drm_hwcomposer 无 readback / 主线程长等），取证只能走 VNC，
   脚本：`engine/out/tmp/vncshot.py <port> <out.png>`（RFB 3.8 裸抓一帧转 PNG）。
5. `/system/bin/drmserver` 以 `Exec format error` → status 127 每 5 分钟循环
   （32 位 media DRM server），与显示无关，待单独处理。
6. 现编环节仍依赖主机装了 NDK 26.x：若机器上只有 NDK 28+，步骤 4c 会直接报错停在
   「未找到 c++ 头与 AOSP 14 平台 libc++ 配套的 NDK clang++」，不会静默编出跑不起来的件。
