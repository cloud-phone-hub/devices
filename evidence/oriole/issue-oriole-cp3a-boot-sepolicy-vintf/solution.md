# #164 oriole（CP3A.261005.002.A1-20260905）：lineage-24.0 基底与官方 202604 系统混搭的三层启动卡死（SELinux 版本对齐、VINTF 冲突裁剪与 ashmem_libcutils_device 授权）

## 1. 问题

`oriole_CP3A.261005.002.A1-20260905`（Google Pixel 6，官方 Android 16 QPR2 / `CP3A` 固件，搭配 `lineage-24.0` 基底与 `lineage-23-6.1` 内核）在完成镜像构建（见 [`../../common/issue-lineage24-erofs-apex-and-v412-v2/`](../../common/issue-lineage24-erofs-apex-and-v412-v2/solution.md)）后启动实例卡住，依次暴露出三层阻断整机开机的故障：

1. **第一层（0.29s `selinux_setup` 阶段 `InitFatalReboot`）**：`init` 调用 `/system/bin/secilc` 编译策略时报错 `Could not open file: /system/etc/selinux/mapping/202704.cil: No such file or directory`，直接触发 `InitFatalReboot` 重启。
2. **第二层（0.96s `servicemanager` 报 `NULL VINTF MANIFEST!: device`，`/data` 挂载与 `keystore2` 卡死）**：`libvintf` 解析设备清单时因 `android.hardware.audio.effect`（`@2 vs @4`）、`android.hardware.wifi`（`@3 vs @4`）及 `android.hardware.camera.provider`（`@3 vs @4`）冲突返回空 Manifest，导致 `vold`/`keystore2` 查不到 `IKeyMintDevice/default`，无法完成 FBE 密钥派生与 `/data` 挂载。
3. **第三层（19.6s `system_server` 在 `ApplicationSharedMemory.nativeCreate` 循环崩溃）**：`audioserver` 与 `system_server` 打开 `/dev/ashmem<boot_id>` 时被 SELinux 拒绝（`avc: denied { open } ... tcontext=u:object_r:ashmem_libcutils_device:s0`），`system_server` 抛出 `RuntimeException: Failed to create ashmem: Permission denied` 退出并连带 `zygote` 每 5 秒循环重启。

## 2. 根因

### 2.1 第一层：基底 `plat_sepolicy_vers.txt`（`202704`）高于官方系统支持的最高映射版本（`202604`）
- `lineage-24.0` 基底 `vendor_a.img` 的 `/vendor/etc/selinux/plat_sepolicy_vers.txt` 为 `202704`，其 `plat_pub_versioned.cil` 与 `vendor_sepolicy.cil` 中的平台公有类型后缀均为 `_202704`。
- 而官方 `CP3A.261005.002.A1` 的 `system` 与 `vendor` 分区冻结在 `202604`，`/system/etc/selinux/mapping/` 下最高仅提供 `202604.cil`，不存在 `202704.cil`。

### 2.2 第二层：`lineage-24.0` 基底残留 legacy audio effect 服务 + 官方 `/vendor/apex` 内置 VINTF 片段冲突
- **根因 A（audio effect）**：`lineage-24.0` 基底 `vendor_a.img` 在非 APEX 顶层新增了 `etc/vintf/manifest/android.hardware.audio.effect.service-aidl.legacy.xml`（声明 `IFactory/default @4`）、`etc/init/android.hardware.audio.effect.service-aidl.legacy.rc` 和 `bin/hw/android.hardware.audio.effect.service-aidl.legacy`，与自研模块 `android.hardware.audio` 提供的 `android.hardware.audio.service.xml`（`IFactory/default @2`）冲突。
- **根因 B（wifi 与 camera APEX）**：官方 `CP3A` 的 `/vendor/apex/com.google.pixel.wifi.ext.apex` 与 `/vendor/apex/com.google.pixel.camera.hal.apex` 分别内置了 `/etc/vintf/manifest_wifi_ext_aidl.xml`（声明 `IWifi/default @4`）和 `/etc/vintf/android.hardware.camera.provider@2.7-service-google.xml`（声明 `ICameraProvider/internal/0 @4`），在不重定向 `/vendor/apex` 的前提下，与 `/vendor/etc/vintf/manifest/` 下的 `android.hardware.wifi-service.xml`、`android.hardware.camera-service.xml` 发生重复声明冲突。

### 2.3 第三层：`202604` 平台策略移除全体 `domain` 打开 `ashmem_libcutils_device` 的默认授权
- 反汇编 `CP3A` 的 `/system/lib64/libcutils.so`（`_Z9use_memfdv`）可见：当 `sys.use_memfd` 未置 `true` 时，`use_memfd()` 检查 `/sys/fs/selinux/policy_capabilities/memfd_class` 是否存在且 `ro.vendor.api_level >= 202604`。
- `oriole` 采用 `6.1` 内核（无 `memfd_class` policy capability）且 `ro.vendor.api_level=31`，因此 `use_memfd()` 返回 `false`，`libcutils` 继续打开 `/dev/ashmem<boot_id>`（标签为 `u:object_r:ashmem_libcutils_device:s0`）。
- 从平台策略 `202604` 起，AOSP `plat_sepolicy.cil` 移除了对全体 `domain` 打开 `ashmem_libcutils_device` 的默认许可，改为由旧 shipping API 机型的厂商策略通过 `system/sepolicy/vendor/ashmem/ashmem_allow.te` 注入 `(allow domain ashmem_libcutils_device_202604 (chr_file (open)))`（官方 `CP3A` 的 `vendor_sepolicy.cil:13876` 含该条）；而 `lineage-24.0`（Cuttlefish `202704`）基底未编入 `ashmem_allow.te`。

## 3. 证据

- 失败日志见 [`log-fail.txt`](log-fail.txt)。
- 通过日志见 [`log-pass.txt`](log-pass.txt)。
- 开机桌面截图见 [`boot-pass.png`](boot-pass.png)。

## 4. 改动

1. **`engine/build/07_build_vendor/sepolicy.go`**：
   - 新增 `alignSepolicyVersion(plan, stdout)`：当基底 `plat_sepolicy_vers.txt` 高于厂商固件版本时，自动将 `plat_sepolicy_vers.txt`、`genfs_labels_version.txt` 及 `plat_pub_versioned.cil`、`vendor_sepolicy.cil` 中的版本后缀降级对齐为厂商固件版本（`202704 -> 202604`）。
2. **`engine/build/assets/devices.db`（`versions` 表 `buildId='CP3A.261005.002.A1-20260905'` 的 `buildVendor`）**：
   - `files` 新增 3 条 `mode: "delete"` 删除基底残留的 `etc/vintf/manifest/android.hardware.audio.effect.service-aidl.legacy.xml`、`etc/init/android.hardware.audio.effect.service-aidl.legacy.rc`、`bin/hw/android.hardware.audio.effect.service-aidl.legacy`。
   - `vintf-drop` 设置为 `["android.hardware.wifi-service.xml", "android.hardware.camera-service.xml"]`。
3. **`engine/vendor/hardware/android.service.daemon/sepolicy/vendor_sepolicy.cil`**：
   - 追加 `(allow domain ashmem_libcutils_device_{{vers}} (chr_file (open)))`，补齐 `system/sepolicy/vendor/ashmem/ashmem_allow.te` 兼容授权。

## 5. 验证

1. 执行 `cdh device build oriole --manifest CP3A.261005.002.A1-20260905 --only 07_build_vendor`、`09_build_patch`、`11_pack_vda` 全部 `EXIT=0`；`go test ./07_build_vendor` 通过。
2. 创建并启动实例 `oriole_CP3A.261005.002.A1-20260905/1`（`pid=5879`，ADB `127.0.0.1:5911`，VNC `127.0.0.1:5912`）：
   - `secilc` 编译通过，无 `InitFatalReboot`；
   - `servicemanager` 成功加载设备与框架 VINTF 清单，`NULL VINTF MANIFEST` 为 0 次；
   - `audioserver` 与 `system_server` 无 `ashmem_libcutils_device` 权限拒绝，35 秒内 `sys.boot_completed=1`、`init.svc.bootanim=stopped`；
   - `boot_check.py` 验证通过（`1088x2400`，平均亮度 `34.8` / 峰值 `255`），`sec_touchscreen` 触控、`IWifi/default`（`mIsStaConnected: true`）、`ICameraProvider/internal/0`、`IRadio*`、`IComponentStore/software` 全部正常工作。

## 6. 遗留

- `oriole` 的 `devices.db` 中 `modules.hardware` 同时包含 `android.hardware.media`（MediaTek HIDL c2）与 `android.hardware.media.v412`（Tensor V4L2 AIDL c2），前者在无 `/vendor/etc/media_config.xml` 时以 `status 1` 退出，不影响 `android.hardware.media.c2@1.2-service-v4l2-64` 提供 `IComponentStore/default` 与 `IComponentStore/software`，后续可在清理机型硬件模块列表时单独收敛。

