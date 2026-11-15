# Issue: oriole A14 云机与真机 device-checker 第二轮对齐（cmdline 硬件身份 / gsm 属性 / 属性写通道 / mali sepolicy）

- 设备: Google Pixel 6（oriole，AP2A.240905.003.F1 + lineage-21.0，内核 `5.10.198-android13-4` cdh）
- 宿主: macOS (darwin, QEMU + hvf)，实例 `oriole_14-AP2A.240905.003.F1-20240815/2`（ADB `127.0.0.1:5911`）
- 真机: Pixel 6（androidId `b8f75582353b320a`，本轮走 checker 服务端网络通道，USB 口当期被 gold 占用）
- 云机: androidId `02f9d9f965732349-cloud`（cloud flavor APK）
- 状态: **已闭环（本轮可修项）**（2026-11-13）：同批 63 项×2 设备复测 run-00004-merged 126/126 succeeded，
  总差异键 **25291 → 24213（净 -1078）**；本轮全部 target 键残留 0；冷启动 + `adb reboot` 全程 0 次 kernel panic。
- 背景: 「oriole Android 14 云机和真机对齐」任务第二轮。第一轮见
  [../issue-oriole14-checker-align/](../issue-oriole14-checker-align/solution.md)（#200，26063→25291）。
  本轮把剩余差异分诊为「可修 / 永久」两类，修掉全部可修项；期间第一版 gsm 修复引入 init 崩溃循环，
  根因定位后新建纯 property_service 写通道彻底解决（该通道为全机型共用 daemon 的通用修复）。

---

## 一、问题

第一轮收敛后剩 25291/41898 差异键。第二轮分诊发现可修项集中在四处：

1. **`ro.boot.*` 硬件身份组缺失**：真机 bootloader 向 kernel cmdline 注入的 sku / revision / ddr /
   ufs / color / coo / devcfg / pcbcfg / bootloader / soc / dtbo_idx / bootdevice 等约 20 键，
   云机全缺或为默认值，连带 `properties.getprop` / `properties.native` / `network.cell` /
   `sim.telephony` 等功能点读到的身份不一致；
2. **`gsm.version.baseband` / `gsm.version.ril-impl` 运行态缺失**：两键已写进 `/vendor/build.prop`，
   但 guest `getprop` 为空——值在文件里、没进属性系统；
3. **boardid 通用逻辑的平台错配**：`ro.boot.chipid` / `ro.boot.cpuid` 是 MTK 专属键却被注入 gs101；
   `ro.boot.boot_devices` 未按 SoC 映射（gs101 应为 `14700000.ufs`）；bluetooth `default_name`
   给 google 品牌加了不该有的品牌前缀；
4. **`vendor.mali.*` 5 键对 app 身份不可见**：shell `getprop` 能看到，checker（app 身份）却报「缺」。

## 二、根因

1. **cmdline 硬件身份**：真机 `ro.boot.*` 派生自 bootloader 注入的 `androidboot.*` cmdline；
   云机 grub 动态段没有这组声明。修法是清单 `buildVendorBoot.cmdline` 声明、11 经
   `kernel_cmdline_dynamic` 注入。硬约束：arm64 `COMMAND_LINE_SIZE=2048`；注入后总长 1811、余量 237。
   四组键**不能**注入（记永久遗留）：verified-boot 状态组（云机 locked/green 是 keymint 链口径，禁动）、
   `androidboot.mode`（05 `rewriteCmdline` 固定 normal 且 vendor_boot cmdline 先于动态段、`ro.` set-once
   无法覆盖）、`boottime`/`boottime_extra`（两项 317 字符，加后 2165 超限会被截断）、
   `theme`（grub 先传 `androidboot.theme=0`，set-once 先值生效）。
2. **gsm 两键**：init 加载 `/vendor/build.prop` 时被 SELinux 拒绝——`gsm.*` 按 plat_property_contexts
   前缀规则落 `u:object_r:radio_prop:s0`，lineage vendor 策略没给 `vendor_init` 该上下文的 set 权限，
   init 打「Do not have permissions to set … SELinux permission check failed」后**跳过**这两键。
   需要 daemon 起机后补写。
3. **boardid 平台闸门**：通用注入逻辑不区分 SoC/品牌，需按 `platform.starts_with("mt")`（chipid/cpuid）、
   gs101→`14700000.ufs`（boot_devices）、google 品牌跳过前缀（bt name）加闸门。
4. **init 崩溃循环（本轮第一版修复引入，最重要根因）**：第一版用 resetprop 直写 mmap
   （`set_prop_direct_rw`）补 gsm 两键，值确实写进去了，但 `sys.boot_completed=1` 后整机反复
   `Kernel panic - not syncing: Attempted to kill init! exitcode=0x0000000b`（单个 qemu 日志实测 32 次）。
   机制：`set_prop_direct_rw` 写入前对目标 SELinux 上下文属性区做 `pa->reset()` + **全量重建**；
   `radio_prop` 是热区（framework/HAL 并发写），重建窗口与并发更新竞态 → 属性区 trie 损坏 → init SIGSEGV。
   进一步读 `resetprop.cpp` 发现**常规路径也不安全**：`resetprop::set` 成功后同样调
   `reset_properties()`→`reset_prop_file()` 做同一套全量重建。两条既有路径都会重建属性区，
   必须新建**只走 property_service、零重建**的纯通道。
5. **mali 5 键**：云机 `vendor_property_contexts`（lineage 基底）没有 `vendor.mali.` 条目 →
   落 `default_prop` → `untrusted_app` 无读权限（checker 的 `getprop.js` 以 app 身份跑
   `checker.runShell("getprop")`，故 shell 可见、checker 不可见）。真机把 `vendor.mali.` 映射到
   `vendor_arm_runtime_option_prop`（全 domain 可读、仅 vendor_init 可写），云机 sepolicy 缺该类型。

## 三、证据

失败侧（详见 [log-fail.txt](log-fail.txt)）：

- init 拒载 gsm 两键：`qemu-20261113-013212.log:930-931`
  「Do not have permissions to set 'gsm.version.baseband' … SELinux permission check failed」；
- 直写版崩溃循环：`qemu-20261113-014852.log` 先有 daemon kmsg「gsm.version.baseband=…（direct mmap）」
  （:3044/:3047），约 10 s 后「Kernel panic … Attempted to kill init! exitcode=0x0000000b」（:4749），
  全日志 32 次 panic、每次 boot_completed 后即崩；
- checker app 身份 mali 缺失：run-00003-merged `properties.getprop` 云机 `vendor.mali.*` 键数 0、
  真机 5（此时 gsm 已经 svc 通道补上，两侧 baseband 一致）。

通过侧（详见 [log-pass.txt](log-pass.txt)）：

- 最终镜像 `qemu.log` 0 次 panic，daemon kmsg「gsm.version.baseband=…（property_service）」；
- guest `getprop` 全部 target 键与真机一致（sku G9S9B、bootloader slider-14.5-12088207、
  boot_devices 14700000.ufs、chipid/cpuid 空、gsm 两键齐、mali 5 键齐）；
- run-00004-merged 126/126 succeeded，两侧 checker 视角 mali 均 5/5，总差异 24213/41871。

## 四、改动

**清单** `devices/manifest/configs/google/oriole/oriole-android14.json`：

- `buildVendorBoot.cmdline` 新增 29 项（run-00002 真机 `ro.boot.*` 取证），`note` 记录四组排除键
  与 1811/2048 长度预算；
- `buildVendor.build-prop-sets` 新增 gsm×2、`vendor.mali.*`×5、`dalvik.vm.*`×4（dex2oat 线程/CPU 集）；
- `buildVendor.sepolicy`：`vendor_sepolicy.cil` 新增 `vendor_arm_runtime_option_prop` 类型块
  （property_type / vendor_property_type / vendor_restricted_property_type 三属性集 +
  全 domain 读 + vendor_init set/读，照真机 run-00001 sepolicy 取证），
  `vendor_property_contexts` 新增 `vendor.mali. … prefix` 一行。

**guest daemon** `engine/vendor/hardware/android.service.daemon/`（Rust + C++，07 内按指纹重编）：

- `src/resetprop/src/resetprop.cpp` / `resetprop.hpp`：新增 `set_prop_svc()`——只调 dlsym 的
  `__system_property_set` 函数指针，**不做任何属性区 reset/重建**；`persist.*` 由 property_service 自行落盘；
- `src/resetprop/src/wrapper.h` / `wrapper.cpp`：FFI `resetprop_set_svc`；
- `src/resetprop/src/lib.rs`：`pub fn set_svc()`；
- `src/rust/src/boardid.rs`：
  - 新增 `vendor_build_prop_value()`（读 `/vendor/build.prop`，值不 trim——ril-impl 含空格）；
  - `ensure_telephony_and_locale_defaults()` 改为经 `set_prop_svc` 从 build.prop 补 gsm 两键 +
    ril-impl 兜底 + locale 兜底；
  - `set_prop` 统一分派：**非 `ro.` 键一律 svc 通道，`ro.` 键才走 direct mmap**（绕 set-once）；
  - chipid/cpuid 加 MTK 闸门、boot_devices gs101→`14700000.ufs`、bt default_name 跳过 google 品牌；
- `src/rust/src/adbd/adbd_launch.rs`：`service.adb.tcp.port` 两处写入同样切到 `set_svc`
  （同属非 ro 热键，避免重建竞态）。

**重建/重启**：`cdh device build oriole -manifest 14-AP2A.240905.003.F1-20240815 -from 07_build_vendor`
共 4 次（r2 首版 → r2b 直写 gsm → r2c 换 svc 通道 → r2d 加 mali sepolicy），每次停实例 2 → 重启 → 起机。

## 五、验证

- **稳定性**：最终镜像冷启动 + `adb reboot` 各一轮，`qemu.log` 与快照日志 `Kernel panic` 计数 0
  （对照直写版单日志 32 次）；
- **target 键**（guest shell 实测）：`sys.boot_completed=1`；baseband `g5123b-135085-240517-B-11857288`；
  ril-impl `Samsung S.LSI Vendor RIL 5300 V2.3 Build 2024-08-06 14:04:21`；sku `G9S9B`；
  bootloader `slider-14.5-12088207`；chipid/cpuid 空；boot_devices `14700000.ufs`；
  `vendor.mali.*` 5 键齐；`persist.sys.locale=en-US`；theme=1（与真机一致，无需注入）；
- **app 身份可见性**：run-00004 两侧 `properties.getprop` 的 `vendor.mali.*` 均 5/5（run-00003 云机为 0）；
- **收敛**：run-00004-merged 126/126 succeeded，总差异 **24213/41871**，较第一轮 25291 净 **-1078**。
  按功能点：files.app_paths -703、files.storage -120、app.list -118、properties.getprop -47、
  properties.native -46、dev.test_mount -28、network.cell -13、sim.telephony -7、
  id.partition_fsid -4、jvm.memory -1；瞬时噪声 +9（jvm.debugmemory +6、settings.provider +2、
  hardware.battery +1）。本轮全部 target 键（sku/bootloader/chipid/cpuid/gsm/bt-name/dalvik/
  boot_devices/theme/mali）残留 0。

## 六、遗留

**永久遗留（硬件/口径根因，不再追）**：

- verified-boot 组（`ro.boot.flash.locked` / `verifiedbootstate` 等）：云机 locked/green 是
  keymint 工厂链口径，真机 unlocked/orange，禁动；
- `ro.boot.boottime` / `boottime_extra`：两项 317 字符，加后超 arm64 cmdline 2048 上限；
- `ro.boot.mode`：05 固定 normal + vendor_boot cmdline 先于动态段、`ro.` set-once 先值生效；
- camera / app_paths / vendor_file / app.list 大额结构性差异：基线/产品选型（lineage vs 官方预装）；
- 运行时噪声：jvm.debugmemory、settings.provider、battery 动态字段等逐次漂移项。

**通用教训（全机型适用）**：

- **非 `ro.` 键一律走 property_service（`set_svc`），禁止 direct mmap**：resetprop 的直写与常规
  set 两条路径都会 reset+全量重建 SELinux 上下文属性区，热区并发写下必竞态损坏 trie、杀 init。
  direct mmap 只保留给 `ro.` 键绕 set-once 的场景；
- init 拒载（SELinux permission check failed）的属性不会进属性系统，`/vendor/build.prop` 里有值
  ≠ `getprop` 有值；排查时按失败关键词过滤完整日志，勿信被截断的 grep；
- checker 以 **app 身份**读属性：shell 可见不代表 checker 可见，缺键先查 property_contexts 落哪个
  类型、该类型对 `untrusted_app` 是否可读；
- checker 连接会静默掉线（进程活着但服务端通道断）：批量超时先 `am force-stop dev.checker` 再拉起。

**后续可做**：5.15/6.1 假 Wi-Fi 补丁同病未改（#198 遗留）；camera/预装 App 结构性对齐属产品选型决策。