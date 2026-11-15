# oriole 云机 zygote 崩溃循环：patch 载体挂 /mnt/patch 被 vold 当 stale mount 拆掉并杀掉 vndservicemanager，zygote 被提前拉进 bootstrap 挂载命名空间

## 1. 问题

设备 `google/oriole`（Pixel 6，AP2A.240905.003.F1），底座 lineage-21.0（Android 14）
+ 自编 `6.1.138-4k-cdh` 内核，patch.img 由 vdc 以 `first_stage_mount` 挂在 `/mnt/patch`，
内核 `CONFIG_CDH_PATH_REDIRECT` 把 `/vendor`、`/vendor_dlkm`、`/odm` 的白名单路径重定向过去。

引导到 6 s 就死：

```
[5.800424] init: starting service 'zygote'...
[6.110700] init: Service 'zygote' (pid 214) exited with status 1
...  E zygote64: CANNOT LINK EXECUTABLE "/system/bin/app_process64": library "libnativeloader.so" not found
```

之后 zygote 在 10.8 / 15.8 / 20.8 / 22.6 s 反复重试、反复失败，系统停在开机动画，
`sys.boot_completed` 永远等不到。三个「不该出现」的现象同时发生：

1. zygote 在 **5.8 s** 就启动了，而正规的 `on zygote-start` 要到 **22.6 s**；
2. 5.8 s 那批服务启动**没有 `processing action (...)` 抬头**（日志里 0.78 s~21.0 s
   一条 action 抬头都没有），像凭空冒出来；
3. `vold` 在 5.795 s 起连续报 `Found map /proc/<pid>/maps referencing /mnt/patch/...`
   并 `Sending Interrupt to pid <pid>`，前后共 **131** 次。

## 2. 根因

三层，逐层剥开（每层都有 AOSP 源码 + 运行时日志双证）：

**第一层：vold 把 `/mnt/patch` 当 stale mount 拆，拆前先杀进程。**

`system/vold` 的 `VolumeManager::start()` 调 `unmountAll()`，后者遍历 `/proc/mounts`，
把 `/mnt` 下**不在白名单**（`/mnt/scratch`〔仅 debuggable〕、`/mnt/vendor`、`/mnt/product`、
`/mnt/installer`、`/mnt/androidwritable`、`/mnt/vm`）的子挂载挨个 `ForceUnmount`。
`ForceUnmount` 不是纯 `umount2`：先 `umount2`，`EBUSY` 就按 `/proc/<pid>/maps`
杀引用该路径的进程，然后 SIGINT → 重试 → SIGTERM → 重试 → SIGKILL → 重试。
内核侧的 `ksys_umount` 对载体本身返回 `-EBUSY` 只拦住了**卸载**，拦不住**杀进程**——
这就是 131 次 `Found map` 的来源，也是 `/vendor` 下所有进程（各 HAL、vndservicemanager、
我们的 service_daemon）被杀的原因。

**第二层：被杀的首个进程是 vndservicemanager，它的 `onrestart` 把 zygote 提前拉起来。**

```
[5.795029] vold: Found map /proc/198/maps referencing /mnt/patch/vendor/bin/vndservicemanager
[5.796113] vold: Sending Interrupt to pid 198
[5.797646] init: starting service 'seriallogging'...
[5.800424] init: starting service 'zygote'...
...
init: Command 'class_restart main' action=onrestart (<Service 'vndservicemanager' onrestart>:1) took 133ms and succeeded
init: Command 'class_restart hal'  action=onrestart (<Service 'vndservicemanager' onrestart>:2) took 144ms and succeeded
```

`vendor/etc/init/vndservicemanager.rc`（lineage-21 原样）带
`onrestart class_restart main` / `class_restart hal` / `class_restart early_hal`；
`class_restart` 走 `Service::Restart()` → `Start()`，**绕过 `disabled` 标记**，
于是 class main 里所有服务（含 zygote、cph_storage_align、cph_wifi_bootstrap）一次性起来。
这批命令没有 action 抬头，是因为 AOSP `action_manager.cpp` 只在 `current_command_ == 0`
时打 `processing action (...)`，`onrestart` 动作早在第一层就已开始执行。

**第三层：init 把「早于 APEX 就绪」启动的服务永久钉在 bootstrap 挂载命名空间。**

`Service::SetMountNamespace()` 注释写明 "Note that this marking is permanent"：
服务若在 `IsDefaultMountNamespaceReady()` 之前启动，就被固定到 `NS_BOOTSTRAP`；
该标志在 `perform_apex_config`（`builtins.cpp` → `SetDefaultMountNamespaceReady()`）
才置位。本机 `apexd.status=activated` 在 **22.04 s**、`perform_apex_config` 在 **22.11 s**，
而 zygote 在 **5.8 s** 就被拉起 → 永久 bootstrap：`apex/` 只有 bootstrap 那批、
`linkerconfig` 80 KB、`ld.config.txt` 里没有 `com_android_art` 段，
`libnativeloader.so` 自然解析不到 → `CANNOT LINK EXECUTABLE`。
22.6 s 那次正规 `zygote-start` 也一样失败：init 只在**首次**启动时决定命名空间，
后续 restart 沿用同一 ns。

**为什么之前以为「版本不匹配」**：lineage 底包是 21（Android 14），与 Pixel 的
AP2A（Android 14）同版本，`libnativeloader` 缺失与版本错配无关，是 APEX 没进
命名空间。

## 3. 证据

完整日志见同目录 `log-fail.txt` / `log-pass.txt`，要点：

- `log-fail.txt` ①（`/mnt/patch`，实例 `index 2`）：131 次 `Found map`、
  `class_restart main/hal` 无抬头执行、zygote 5.8 s 起 / 6.11 s exit 1、
  之后的 4 次重试全部同样失败。
- `log-fail.txt` ②（中间尝试 `/mnt/vendor/patch`，实例 `index 3`）：
  `avc: denied { search } name="vendor" dev="tmpfs" ino=2 tcontext=u:object_r:mnt_vendor_file:s0`
  对 zygote / vndservicemanager / hwservicemanager / servicemanager / surfaceflinger /
  各 `hal_*_default` / vdc / uncrypt 全域命中；`vndservicemanager` 退出码 1 →
  同样触发 `class_restart main` → zygote 1.33 s 起、1.67 s exit 1。
- AOSP 依据（android14-release）：`system/vold/VolumeManager.cpp` `unmountAll()`
  的白名单与前缀匹配、`system/vold/Utils.cpp` `ForceUnmount()`（477-505 行）
  的三轮 `KillProcessesWithOpenFiles` + `umount2`、`system/core/init/service.cpp`
  `SetMountNamespace()` 的永久标记、`system/core/init/action_manager.cpp`
  抬头只在 `current_command_ == 0` 打印、`system/core/init/builtins.cpp`
  `do_perform_apex_config` 末尾 `SetDefaultMountNamespaceReady()`、
  `system/core/init/first_stage_init.cpp` `SetupMounts()` 先建 `/mnt/vendor`（285 行）
  再 `DoFirstStageMount()`（403 行）、`system/core/fs_mgr/fs_mgr.cpp`
  的挂载点 `mkdir` 是**单层**（743、2266 行）。
- 标签规则：`plat_file_contexts` 的 `/mnt/vendor(/.*)? -> mnt_vendor_file`（720 行）、
  `/mnt -> tmpfs`（20 行）——这是第二版落点被判死刑的直接依据。

## 4. 改动

1. 内核（`devices/kernels/lineage-21-6.1`，两次提交 `990735a07df5`、`2589b949e987`）：
   载体落点 `/mnt/patch` → **`/mnt/vendorpatch`**，同步 9 条前向重定向表、
   9 条 `d_path()` 逆变换表、`ksys_umount` 的 `strcmp` 兜底与全部注释。
   落点必须同时满足两条外部约束，只剩这个平级名可行：
   - 以 `/mnt/vendor` 为**字符串前缀**（vold 白名单前缀匹配，跳过 = 不拆不杀）；
   - 不在 `/mnt/vendor/` 子目录下（避开 `mnt_vendor_file` 标签；实测
     `/mnt/vendor/patch` 会被全域拒 `search`）。
2. `engine/build/05_build_vendor_boot/fstab.go`：`lineVdc` 改
   `/dev/block/vdc /mnt/vendorpatch ext4 ro first_stage_mount`，合并版本
   `merge-v6` → **`merge-v8`**（缓存首行校验，旧缓存自动失效）。
3. 文档与测试同步：`02_build_kernel/lineage-kernel.config`、`09_build_patch/{layout,plan}.go`、
   `05_build_vendor_boot/README.md`、`11_pack_vda/README.md`、`06_build_odm/plan.go`、
   `main.go`、`test/build_vendor_boot_test.go`。
4. **没有**改 `vndservicemanager.rc`（保持 lineage 原样）：触发源已消除，
   `onrestart class_restart` 不再被激活。
5. 内核侧 `ksys_umount` 的 `-EBUSY` 拦截保留，现在是纯兜底（第二道防线）。

## 5. 验证

复跑重编后的盘集（实例 `index 4`），逐条对照 `log-fail.txt` 的失败指标，数据见同目录
`log-pass.txt`：

| 指标 | 实例 2（`/mnt/patch`） | 实例 3（`/mnt/vendor/patch`） | 实例 4（`/mnt/vendorpatch`） |
| :--- | :--- | :--- | :--- |
| `Found map … referencing /mnt/…` | 131 | 0 | **0** |
| `Tearing down stale mount` | 有 | 0 | **0** |
| `Sending Interrupt to pid`（vold 杀进程） | 4 | 0 | **0** |
| `avc: … mnt_vendor_file` | 0 | 3924 | **0** |
| `vdc` 挂载结果 | 挂上后被拆 | 挂上后被拒 | **`__mount(…target=/mnt/vendorpatch…)=0: Success` @ 0.269 s** |
| 首次 `starting service 'zygote'` | 5.800 s（`onrestart` 拉起） | 1.333 s（`onrestart` 拉起） | **16.205 s（正规 `zygote-start`）** |

要点：

- 载体在 **0.269 s** 随 `first_stage_mount` 挂到 `/mnt/vendorpatch`，vold 全程零干预；
- `cdh_redirect:` 生效 **1250** 次，目标全是 `/mnt/vendorpatch/…`——内核里的前向表与
  fstab 的落点对上了，不再是"改名未同步"那种全树 ENOENT 的更糟错位；
- `apexd.status=activated` 在 **5.390 s** 就绪（`Wait … took 1763 ms`），zygote 首次
  启动即落在 APEX 就绪之后，不再被钉到 bootstrap 命名空间；
- `libnativeloader.so not found` 这类 `CANNOT LINK` 不再出现，`system_server` 已推进到
  `StartActivityManager`。

边界：本次只验证"载体落点"这一环，**没有**证明整机 `sys.boot_completed`（仍被
hwc3 composer 的符号缺失挡住，见下条遗留）。

口径外延：`/mnt/vendorpatch` 现在已是全库唯一口径——另外两代内核树与机型清单
同步迁移，并加了源码级守卫用例，详见 `../../common/issue-carrier-mount-unify/`。

## 6. 遗留

- ~~其它代内核（`lineage-22-6.6`、`lineage-23-6.12`，gold/m22/ss9380 用）的同一套
  `CDH_PATH_REDIRECT` 仍把载体挂在 `/mnt/patch`，同样的隐患未迁移。~~
  **已闭环**：`lineage-22-6.6`（`0192a6feab2d`）、`lineage-23-6.12`（`47d441573`）、
  `lineage-21-6.1` 的 Kconfig help（`128aa20763b6`）均已改挂 `/mnt/vendorpatch`，
  构建侧收成 `build/internal/carrier.MountPoint` 单一来源，并加 `TestCarrierMountPointUnified`
  / `TestCarrierNoLegacyPatchPath` 两只守卫用例；复盘见 `../../common/issue-carrier-mount-unify/`。
  12 台非 oriole 机型的盘集仍需按 02→11 重编才能生效。
- `vndservicemanager.rc` 的 `onrestart class_restart main/hal/early_hal` 依然是
  「任何 `/vendor` 进程在 APEX 就绪前死掉 → zygote 被提前拉起」的放大器；
  本次只消除触发源，未动这个放大机制。
- `vold` 的 `unmountAll` 仍会尝试拆 `/storage` 下的残留挂载（本次无影响，未验证）。
- 整机启动未通：`vendor.hwcomposer-3` 在 31.7 s 因
  `std::__1::__libcpp_verbose_abort`、`AIBinder_Class_setTransactionCodeToFunctionNameMap`
  两个符号缺失连崩 4 次 → init 判 `exited 4 times before boot completed` → 进 recovery，
  `sys.boot_completed` 始终为空。属独立卡点（07 的 composer 修好之后从未真正走过
  09/10/11，patch.img 里大概率还是旧二进制），另行闭环。
