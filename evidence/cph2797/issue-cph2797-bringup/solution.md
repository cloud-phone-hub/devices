# CPH2797 首轮起机：SurfaceFlinger 10bit 断言回环 + audioserver 死等 IModule/virtual

- 设备：oppo/cph2797（OPPO Find X9，ColorOS `16.0.8.301_EX01`，MTK `mt6993`）
- 日期：2026-10-30
- 关联：[issue-cph2797-baseline-align-replaces](../issue-cph2797-baseline-align-replaces/solution.md)（#76，
  清单对齐与 07/08–11 全链构建；本轮是它的运行期续篇）

## 1. 问题

#76 把 cph2797 的盘集从「07 步骤 5 报 `[err] 厂商 vendor 缺少该路径: etc/md`、整阶段 0 产物」
拉到 `EXIT=0` 全链跑通（`vendor.img` 205,885,440 B / `super.img` 11.08 GiB / vda 13.00 GiB），
但当时没有起实例（24 GiB 主机上 m22 #45 与 gold #7 各占 8 GiB）。

本轮在实例 6 上起机，`sys.boot_completed` 始终为空，且出现两个互相放大的症状：

1. **SurfaceFlinger 每 ~5 s 崩一次**：`RenderEngine: input buffer not gpu readable` +
   `Fatal signal 6`。整机会话里累计 `Abort message: 'input buffer not gpu readable'` 412 次、
   `Service 'surfaceflinger' (pid ...)` 422 行。首次启动时 init 还会因此判
   `process with updatable components 'surfaceflinger' exited 4 times before boot completed`。
2. **audioserver 卡死不返回**：每秒刷
   `ServiceManagerCppClient: Waited one second for android.hardware.audio.core.IModule/virtual`，
   永不 `addService(media.audio_flinger)`；system_server 在 `AudioSystem.setMasterMono`
   拿 `IAudioPolicyService` 时被 Watchdog 判阻塞。会话里 `IModule/virtual` 6561 次。
3. 两者叠加的后果是 **boot 永远到不了 `boot_completed`**，于是 init 的
   「updatable 服务在 boot 完成前退出 4 次」判据被反复触发
   （`vendor-dolby-media-c2-hal`、`vendor.nfc_hal_service`、`netdiag`、
   `oplus_esepower_hal_service`、`secure_element-1-0` …），置 `sys.init.updatable_crashing=1`
   并处理 `flags_health_check.rc:10`，整机进入重启循环。

## 2. 根因

### 2.1 图形：OPPO 的 primeCache 预编译画到 10bit target，而 virtio 只上报 8bit

ColorOS 把 `SkiaRenderEngine` 的 shader 预编译做成了外挂实现
`/system_ext/lib64/librenderengineextimpl.so`（SF 里只有裸名 `librenderengineextimpl.so`，
由 SF 自己 `dlopen`）。它的 `SkiaRenderEngineExtImpl::precompile` →
`TemporaryPrecompile::draw` 会在 `Cache::primeShaderCache` 期间额外画两块 buffer
（`128 * 128`，以及屏幕尺寸 `1080 * 2340`），随后在
`SkiaRenderEngine::drawLayersInternal` 上断言：

```
Abort message: 'input buffer not gpu readable'
```

断言成立的直接原因是 buffer **根本没分配成功**。allocator 是
`android.hardware.graphics.allocator-service.minigbm_upstream`，它把
`RGBA_1010102 / DRM_FOURCC_AB30`、`RGBA_FP16 / DRM_FOURCC_AB4H` 判为
`Unsupported combination`（会话累计 1039 次），因为内核 virtio-gpu 的
`virtio_gpu_formats[]` 只登记 8 个 8bit 格式
（`devices/kernels/lineage-23-6.12/drivers/gpu/drm/virtio/virtgpu_plane.c:32-41`），
`virtio_gpu_translate_format()` 也只映射这 8 个。

对照 m22（Flyme，无 OPPO 图形外挂）的 `qemu.log`：`Unsupported combination` **0 次**、
SF 是 `SkiaGL Backend (Ganesh)` + `renderer: virgl (Mali-G78)`。
即「请求 10bit」是 ColorOS 外挂独有的行为，不是 AOSP SF 的默认路径。

### 2.2 音频：/odm 的 VINTF 声明了一个不存在的 HAL，而 waitForService 无超时

`debuggerd -b audioserver` 的主线程栈是决定性的：

```
#07 AServiceManager_waitForService
#08 getServiceInstance<android::hardware::audio::core::IModule>(std::string const&)
#09 DevicesFactoryHalAidl::openDevice(char const*, sp<DeviceHalInterface>*)
#10 AudioFlinger::loadHwModule_ll(char const*)
#11 AudioFlinger::getAudioPolicyConfig(media::AudioPolicyConfig*)
#12 AudioPolicyService::AudioPolicyClient::getAudioPolicyConfig(...)
#13 createAudioPolicyManager(...)
#14 AudioPolicyService::onFirstRef()
#15 main
```

即 audioserver 在 `AudioPolicyService::onFirstRef()` 里同步枚举 audio module，
`DevicesFactoryHalAidl` 认为 `virtual` 是**已声明实例**，于是走
`AServiceManager_waitForService`（NDK 实现无超时）死等一个永远不会出现的 binder 服务。

声明来源是官方 `/odm` 的两个 VINTF 片段（不是我们装配出来的 vendor 件）：

- `/odm/etc/vintf/manifest/manifest_module_virtual_audio_v3.xml` → `<fqname>IModule/virtual</fqname>`
- `/odm/etc/vintf/manifest/manifest_oplus_virtualdevice_audio_aidl.xml` → `<name>vendor.oplus.hardware.virtual_device.audio</name>`

这两份配套的 HAL 二进制在官方 `/odm/bin/hw` 里**并不存在**（`/odm/bin/hw` 只有
esepower/secure_element/nfc/power.stats 等 OPPO 件），`/odm/etc/virtual_audio_policy_configuration.xml`
（83 行、`<module name="virtual" halVersion="3.0">`）也只是一个 policy 片段。
真机上该 HAL 由 `vendor.oplus.hardware.virtual_device.audio` 提供，我们的 `/vendor`
没有它，于是「声明存在 / 实现缺失」→ 死等。

已排除的两条错误假设：

- **不是 policy 的问题**：`/vendor/etc/audio_policy_configuration.xml` 只 include
  primary / r_submix / bluetooth，`/odm/etc/audio/audio_policy_configuration_a2dp_offload_enable_cg_enable.xml`
  也只有 primary / binaural_record，两处都没有 `virtual` module。
- **不是 `/system` 的问题**：`ro.build.fingerprint` 是
  `OPPO/CPH2797/OP5E8BL1:16/BP2A.250605.015/...`，本轮 super 里 `/system`、`/system_ext`、
  `/product` 仍是官方 ColorOS（10 阶段只替换 vendor / odm / vendor_dlkm），所以 audioserver
  是 ColorOS 定制版（日志里 `AudioFlingerExtImpl`、`AudioPolicyServiceExtImpl` 就是证据），
  不能用 AOSP 行为去推断它。

### 2.3 为什么运行态改了没用：servicemanager 的 VINTF 缓存比 audioserver 早 22 秒

`ro.boottime.servicemanager = 1.117 s`，而 `ro.boottime.audioserver = 23.766 s`、
`init` 处理 `post-fs-data` 在 4.8 s。运行态 `mount --bind` 覆盖这两份 odm VINTF 片段后
`setprop ctl.restart audioserver`，audioserver **仍然死等**：VINTF 数据库在 servicemanager
里已经建立，且 `/odm` 的片段在它首次查询时就被读入缓存，遮蔽不回溯。

## 3. 证据

- 失败现场：同目录 [log-fail.txt](log-fail.txt)（SF abort 栈、allocator 拒绝、
  audioserver 死等、updatable_crashing 判据、内核版本不一致）。
- 运行态验证：同目录 [log-pass.txt](log-pass.txt)（bind mount 遮蔽后 100 s 内
  `input buffer not gpu readable` 0 次、`TemporaryPrecompile` 0 次；debuggerd 栈；
  VINTF 运行时遮蔽无效的时序原因；patch 重定向白名单；init mount 支持 bind）。
- 代码侧证据：
  - `devices/kernels/lineage-23-6.12/drivers/gpu/drm/virtio/virtgpu_plane.c:32-113`
    —— `virtio_gpu_formats[]` 只有 8bit，`translate_format` 只映射 8bit。
  - `cuttlefish/source/system-core/init/builtins.cpp:468`
    —— `{ "bind", MS_BIND }`，init 的 `mount` 支持 bind。
  - `devices/kernels/lineage-22-6.6/fs/namei.c:189-219`
    —— patch.img 的 `cdh_redirect` 白名单不含 `/odm/etc/vintf`（所以遮蔽不能靠 patch，
    只能靠 init 自己的 mount）。
  - `cuttlefish/source/hardware-interfaces/audio/aidl/default/main.cpp:41-61` 与
    `.../Module.cpp:129-141` —— AOSP audio HAL 按 policy 的 module 名注册
    `IModule/<name>`，但 `typeFromString()` 只认
    `default / r_submix / stub / usb / bluetooth`，**不认 `virtual`**。因此
    「只在 policy 里加 virtual module」不能自造出 `IModule/virtual`。

## 4. 改动

全部改动都在**机型静态树**（`devices/manifest/oppo/cph2797/vendor/`，此前只有
`odm/etc/build.prop`，`vendor/` 目录尚未建立），不改内核、不改官方分区。

### 4.1 第一轮（实例 7 验证失败，暴露三个落地缺陷）

1. `vendor/etc/init/init.virtio_compat.rc` —— 新增，在 rc 里用 init 的
   `mount none <源> <目标> bind` 把两件「官方件但在本机不成立」的东西遮蔽掉：
   - `/system_ext/lib64/librenderengineextimpl.so` → 遮蔽成非 ELF，
     让 SF 的 `dlopen` 失败并回落到 AOSP SF 的预编译路径（m22/gold 都是这条路径）；
   - `/odm/etc/vintf/manifest/manifest_module_virtual_audio_v3.xml` 与
     `manifest_oplus_virtualdevice_audio_aidl.xml` → 遮蔽成「带 version 的空 fragment」
     （写法见 4.3，裸 `<manifest/>` 是错的）。
2. 随镜像落两个遮蔽源文件（内容不需要可执行能力，只需可被 bind 且不可解析）：
   - `vendor/etc/compat/renderengine_ext_unavailable.so`（20 B，`not-a-shared-object`）
   - `vendor/etc/compat/vintf_empty.xml`（`<manifest version="1.0" type="device"/>`）
3. `sepolicy/vendor_sepolicy.cil` —— 新增机型片段（见 4.2 第 3 条修正后的最终形态）。

### 4.2 第二轮修正（本轮实测逐条定位，三个缺陷叠加）

实例 7 上「rc 解析了、`post-fs-data` 也 processing 了，但三条 mount 全没生效、
卡点原样复现」。逐层拆出三个独立原因：

1. **source 路径错（ENOENT，被静默）**：改写为
   `mount none /mnt/patch/vendor/etc/compat/... <目标> bind wait`。
   本机内核 `cdh_redirect` 把 `/vendor/etc` 重定向到 `/mnt/patch/vendor/etc`，
   但该重定向不覆盖 `mount(2)` 里对 bind source 的 `kern_path()`；
   `do_mount` 用 `ErrnoErrorIgnoreEnoent`（`builtins.cpp:537`）静默 ENOENT，
   于是日志干净、挂载为空。运行态对照：
   `source=/vendor/etc/compat/...` → rc=1 `No such file or directory`；
   `source=/mnt/patch/vendor/etc/compat/...` → rc=0 且出现在 `/proc/1/mounts`。
2. **trigger 太晚（VINTF 缓存已成）**：`on post-fs-data` → `on early-init`。
   分区 0.24 s 就挂完（`/system_ext` 0.236 s、`/odm` 0.239 s、`/mnt/patch` 0.265 s），
   而 servicemanager 1.112 s 启动、**3.228 s 首次 `getDeviceHalManifest` 并缓存**；
   旧位置 `post-fs-data` 是 14.26 s，实测那次唯一的 VINTF 重载发生在 14.219 s，
   比我们的 bind 还早 37 ms —— 缓存里始终带着 `IModule/virtual`。
3. **SELinux 只给了一半**：`bind mount` 有两道检查，第一轮只给了 target inode 的
   `file { mounton }`，缺 target 所在 superblock 的 `filesystem { mount }`
   （`selinux_sb_mount` → `superblock_has_perm(..., FILESYSTEM__MOUNT)`，
   sb label 取分区根 label：`/system_ext` → `system_file`、`/odm` → `vendor_file`）。
   缺这道会 EACCES（init 会打印 `mount() failed: Permission denied`）。
   第一轮没暴露纯粹因为 source 先 ENOENT，根本没走到 SELinux。

最终 `vendor_sepolicy.cil` 的授权面（`patch-log` 记为「清单 4 条规则」）：

```
(allow init system_lib_file (file (mounton)))
(allow init vendor_configs_file (file (mounton)))
(allow init system_file (filesystem (mount)))
(allow init vendor_file (filesystem (mount)))
```

注：运行时 init 的真实域是 `u:r:init:s0`（avc 的 `scontext` 实证），
所以写 `init` 而非编译期的版本化名 `init_202504` 是正确的。

### 4.3 第三轮修正：空 manifest fragment 必须带 `version`

第二轮把 trigger 提前到 `early-init` 后，`input buffer not gpu readable` /
`TemporaryPrecompile` / `IModule/virtual` 同时归零，但系统卡在 keystore2：

```
servicemanager: getDeviceHalManifest: -22 VINTF parse error: Illformed file:
  /odm/etc/vintf/manifest/manifest_module_virtual_audio_v3.xml:
  Could not find/parse attr with name "version" and value "" for element <manifest>
servicemanager: NULL VINTF MANIFEST!: device
vold: Waited one second for android.system.keystore2.IKeystoreService/default...
```

即：libvintf 对**每个 fragment** 都校验 `<manifest>` 的 `version` 属性，
裸 `<manifest/>` 不是「空但合法」，而是「文件畸形」→ 整个 device manifest 变成
NULL → 所有 `isDeclared` 判定失效（keystore2 也找不到）→ vold 死等。

正确写法（保持「有 version、无 hal」）：

```xml
<?xml version="1.0" encoding="UTF-8"?>
<manifest version="1.0" type="device"/>
```

这个坑的教训：**遮蔽只要做到「该 HAL 不声明」，不要动 fragment 的合法性**；
这也是为什么本轮把三件事分成「source / 时机 / 内容合法性」三层各自取证据，
否则很容易把「NULL manifest」误读成「遮蔽成功」。

## 5. 验证

运行态（本轮已完成，见 [log-pass.txt](log-pass.txt)）：

- 遮蔽 `librenderengineextimpl.so` 后 `setprop ctl.restart surfaceflinger`：
  标记点之后 100 s 内 `input buffer not gpu readable` **0** 次、
  `TemporaryPrecompile` **0** 次（对照：标记点前 412 / 621 次）；
  `init.svc.surfaceflinger=running`、`pidof surfaceflinger` 稳定不自增；
  VNC 抓到 OPPO「Find X」开机动画（1264×2760 全覆盖）。
- `debuggerd -b audioserver` 拿到 2.2 节完整调用栈，把「等 virtual」从推断变成实证。
- 运行态遮蔽 VINTF **无效**（预期，因为 servicemanager 已缓存），这条失败本身
  确定了必须把遮蔽前移到 `post-fs-data`。

镜像落地后的验证（本轮已完成，三轮实例对照）：

| 轮次 | 镜像状态 | `input buffer not gpu readable` | `TemporaryPrecompile` | `Unsupported combination` | `IModule/virtual` | 三条 bind 在 `/proc/1/mounts` |
|---|---|---|---|---|---|---|
| 实例 7 | 第一轮（source `/vendor/...`、post-fs-data、仅 mounton） | 14 | 21 | 35 | 46 | **无** |
| 实例 8 | 第二轮（source `/mnt/patch/...`、post-fs-data、+filesystem mount） | **0** | **0** | **0** | 157 | **3 条全在** |
| 实例 9 | 第三轮（trigger=early-init，但 fragment 是裸 `<manifest/>`） | 0 | 0 | 0 | 0 | 3 条全在 |

实例 9 的计数虽然全归零，但**不可采信**：`NULL VINTF MANIFEST` 让整个 device
manifest 失效，`IModule/virtual` 为 0 是「什么都查不到」的副作用（详见 4.3），
代价是 keystore2 不可达、vold 死等。这一行保留在表里就是为了避免把
「遮蔽过头」误读成「修好」。

实例 8 的关键事实：

- `/proc/1/mounts` 三条齐备，source 均为 `/dev/block/vdc`（patch.img）：
  `/system_ext/lib64/librenderengineextimpl.so`、
  `/odm/etc/vintf/manifest/manifest_module_virtual_audio_v3.xml`、
  `.../manifest_oplus_virtualdevice_audio_aidl.xml`。
- 图形三条计数**全部归零**，崩溃栈里的
  `librenderengineextimpl.so (TemporaryPrecompile::draw)` 不再出现。
- rc 侧无任何 `mount() failed`；仅有 ColorOS 自己的 24 条
  `denied { mounton }`（目标都是 `/vendor/app/mcRegistry`、`/data/persist_log/...`，
  与我们的路径无关），且 avc 的 `scontext=u:r:init:s0` 实证了 `allow init` 生效。
- `IModule/virtual` 仍 157 次 → 4.2 第 2 条：遮蔽本身正确
  （`grep -rl 'IModule/virtual' /odm/etc/vintf` 已无命中），
  但 bind 生效于 14.26 s，晚于 servicemanager 3.228 s 的 VINTF 缓存。
- `AudioFlinger::loadHwModule` 日志只到 primary（handle 10）与 r_submix（handle 18），
  第三个 module 正是 virtual —— 与策略文件
  `/odm/etc/audio/audio_policy_configuration_a2dp_offload_enable_cg_enable.xml`
  里含 `virtual` 吻合；policy 只决定「要打开哪些 module」，
  「是否 declared」只由 VINTF 决定，两者已分开验证。

第三轮（`on early-init`）的验证目标：
`IModule/virtual` 计数归零、`sys.boot_completed=1`、
`init.svc.surfaceflinger=running`、`sys.init.updatable_crashing` 不再置 1。

## 6. 遗留

1. **updatable 服务崩溃面还没收敛**：`vendor-dolby-media-c2-hal`、
   `vendor.nfc_hal_service`、`oplus_esepower_hal_service`、`secure_element-1-0`
   （后两个缺 `libMcClient.so`）、`android.hardware.nfc-service.nxp`（缺
   `libmemunreachable.so`）、`netdiag`（`Failed to bind socket: Permission denied`）、
   `/odm/bin/hw/android.hardware.power.stats-impl.oplus`（`Check failed: s == STATUS_OK`，
   与我们 vendor 侧 power.stats 的服务名冲突）。本轮只解决了「放大成整机重启」的
   前置条件（让 boot 尽快完成），这些服务本身的失败要单独一条 issue 收敛
   —— 缺库的补库、名字冲突的取舍（odm 侧或我们侧二选一）都要证据。
2. **实测内核与清单不一致**：`ro.kernel.version` 实测
   `6.6.89-android15-8-gbe8d201b0d27-ab13762941-4k`，清单声明
   `6.12.81` / 官方 `6.12.23-android16-5`。pack_vda 取到的 boot 不是清单声明的树，
   6.12 的 in-tree 模块（`cdh_*` 与无线）会因 vermagic 不符加载失败。这是独立缺陷，
   必须在下一轮先定「用哪棵树」再谈模块。
3. **图形修法是绕过而非修复**：真正让 10bit 可用的路是给 virtio-gpu 补
   `DRM_FORMAT_ABGR2101010` 等 10bit 格式（协议层 `VIRTIO_GPU_FORMAT_*` 是否支持待查），
   或让 minigbm 走 software fallback。遮蔽 ext 库等于放弃 ColorOS 的 shader 预编译，
   只影响首帧耗时，功能面无损，但属于「知道真因、暂时选择绕过」。
4. **`ro.soc.model=vsoc_arm64` 仍未复核**（#76 遗留），本轮没有 getprop 对照。
5. `/system`、`/system_ext`、`/product` 仍是官方 ColorOS，意味着类似
   「ColorOS 组件硬依赖官方 HAL / 官方 odm 件」的坑会继续出现；是否走
   「官方件 + 遮蔽」还是「换 lineage 对应分区」需要一条策略级结论。
