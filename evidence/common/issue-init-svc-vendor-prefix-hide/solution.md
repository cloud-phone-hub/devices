# #206：`init.svc.*` 暴露 9 个云机专属服务（`vendor.` 前缀收口与 `mdnsd` 屏蔽）

## 问题

`gold_15-OS3.0.11.0.VNQCNXM` 首轮 `aligner.build_props_full` 对比真机发现，普通 App 无需任何权限即可从 `SystemProperties` / `getprop` 读到 **9 个云机专属 `init.svc.*` 属性**：

- `init.svc.cph_storage_align`（`stopped`）
- `init.svc.cph_wifi_bootstrap`（`stopped`）
- `init.svc.keep_awake`（`running`）
- `init.svc.suspend_blocker`（`running`）
- `init.svc.auto_provision`（`stopped`）
- `init.svc.seriallogging`（`running`）
- `init.svc.logcat_dump`（`running`）
- `init.svc.mdnsd`（`running`）
- `init.svc.android-hardware-media-c2-v4l2-hal-1-2`（`running`）

`cph_*`、`keep_awake`、`suspend_blocker`、`auto_provision`、`seriallogging`、`logcat_dump`、`android-hardware-media-c2-v4l2-hal-1-2` 在真实量产机上均不存在，且云机不开启 `adb mdns` 却常驻 `mdnsd=running`，直接暴露云机指纹。

## 根因

1. Android `init` 在解析 `.rc` 的 `service <name>` 时会自动维护只读系统属性 `init.svc.<name>`。Android 平台 SELinux `property_contexts` 中，无 `vendor.` 前缀的服务默认落在 `init_svc_prop`（多数进程可读，或未按厂商私有属性域隔离），而以 `vendor.` 开头的服务属性会归属 `vendor_default_prop`（禁止 `untrusted_app` 读取）；上述 8 个自有 / 基底 / Codec2 服务命名均未加 `vendor.` 前缀。
2. lineage 基底 `init.virtio.rc` 中的 `suspend_blocker`、`setup_wifi`、`seriallogging` 也未加 `vendor.` 前缀，构建阶段 `07_build_vendor` 此前仅改写屏幕密度兜底值而未统一重写服务名。
3. `/system/etc/init/mdnsd.rc` 由官方系统镜像携带（按规则禁止修改 `system.img` 本体），且 APEX `adbd` 启动时默认未传 `ADB_MDNS=0`，仍会触发拉起 `mdnsd`。

## 证据

- 修复前：`engine/out/tmp/p0_fail_extract.txt` 与 `report/xiaomi/gold-aligner-20261114/` 首采快照（`4503ac70a1c898f3-cloud`）见同目录 [`log-fail.txt`](log-fail.txt)。
- 修复后：实机 shell 核验（`p0_smoke_pass.txt`）+ `07_build_vendor` 单测 + 实例 `gold_15-OS3.0.11.0.VNQCNXM/2`（`4d5128bd8ca07462-cloud`）`aligner.build_props_full` 重采结果见同目录 [`log-pass.txt`](log-pass.txt)。

## 改动

1. **CDH 自有与 v4l2 Codec2 服务 `.rc` 统一加 `vendor.` 前缀**：
   - `engine/vendor/hardware/android.service.daemon/vendor/etc/init/cph_storage_align.rc`：`cph_storage_align` → `vendor.cph_storage_align`；
   - `engine/vendor/hardware/android.service.daemon/vendor/etc/init/cph_wifi_bootstrap.rc`：`cph_wifi_bootstrap` → `vendor.cph_wifi_bootstrap`；
   - `engine/vendor/hardware/android.service.daemon/vendor/etc/init/cph_logcat_dump.rc`：`logcat_dump` → `vendor.logcat_dump`；
   - `engine/vendor/hardware/android.service.daemon/vendor/etc/init/cph_disable_provision.rc`：`auto_provision` → `vendor.auto_provision`、`keep_awake` → `vendor.keep_awake`；
   - `engine/vendor/hardware/android.hardware.media.v412/` 下 `vendor/etc/init/android.hardware.media.c2@1.2-service-v4l2-64.rc` 与 `src/v4l2_codec2/service/android.hardware.media.c2@1.2-service-v4l2-{32,64}.rc`：`android-hardware-media-c2-v4l2-hal-1-2` → `vendor.media.c2-v4l2-hal-1-2`。
2. **`07_build_vendor` 清洗基底 `init` rc 服务名**：
   - `engine/build/stages/07_build_vendor/fstab.go` 新增 `sanitizeCloudInitRC`（由 `placeVendorInitRC` 对 `init.virt.rc` 及 `init.virtio.rc -> init.<board>.rc` 调用），自动将 `suspend_blocker`、`setup_wifi`、`seriallogging` 及其 `start` 指令重命名为 `vendor.*`；
   - `engine/build/stages/07_build_vendor/fstab_test.go` 新增单测 `TestSanitizeCloudInitRCDropsEFIAndPrefixesServices`。
3. **不改 `system.img` 屏蔽 `mdnsd`**：
   - 新增空 init 配置 `engine/vendor/hardware/android.service.daemon/vendor/etc/init/cph_empty.rc`（随 `patch.img` 落 `/mnt/vendorpatch/vendor/etc/init/cph_empty.rc`）；
   - `devices/manifest/configs/redmi/gold/gold-android15.json` 的 `modules.redirect`（`cdh_redirect` 规则第 11 条）追加 `{"type": "file", "from": "/system/etc/init/mdnsd.rc", "to": "/mnt/vendorpatch/vendor/etc/init/cph_empty.rc"}`（`init` pid=1, `uid=0 <= max_uid` 会读到空文件，从而不注册 `mdnsd`）；
   - `engine/vendor/hardware/android.service.daemon/src/rust/src/adbd/adbd_launch.rs` 的 `build_exec()` 为 APEX `adbd` 固定注入环境变量 `ADB_MDNS=0`，防止 `adbd` 属性触发或自行拉起 `mdnsd`。

## 验证

1. 单测：`cd /Volumes/work/cloud-phone-hub/engine/build && go test -v -run 'TestSanitizeCloudInitRCDropsEFIAndPrefixesServices' ./stages/07_build_vendor` 通过（`0.00s`）。
2. 全链路重建并重启 `gold_15-OS3.0.11.0.VNQCNXM/2`：
   - shell（`uid=2000`）`getprop | grep init.svc.`：旧 9 个服务名命中 `0`（`OLD_NAMES_NONE`），内部已改为 `init.svc.vendor.keep_awake`、`init.svc.vendor.media.c2-v4l2-hal-1-2`、`init.svc.vendor.seriallogging` 等；
   - `/proc/cdh_redirect` 确认第 11 条规则 `file /system/etc/init/mdnsd.rc -> /mnt/vendorpatch/vendor/etc/init/cph_empty.rc` 已挂载生效（`rules: 11/32`）；
   - `aligner.build_props_full` 重采对拍（`4d5128bd8ca07462-cloud` vs 真机 `07cf5fc1784783d5`）：App 侧可见的 `init.svc.*` 中旧 9 项与 `init.svc.vendor.*` 全部不可见（仅留 1 条与云机脚本无关的 AOSP `init.svc.cppreopts=stopped`；`services` 域的 `mdns` 仅是真机也存在的 Android `NsdManager` 系统服务）。

## 遗留

1. `gold-android15.json` 之外其余机型的 `modules.redirect` 尚未统一追加 `/system/etc/init/mdnsd.rc -> /mnt/vendorpatch/vendor/etc/init/cph_empty.rc`，后续随各机型对齐轮次补齐（规则上限 `CDH_RULES_MAX = 32` 需确认余量）。
2. `build_props` 中真机存在但云机缺失的 58 个厂商专属 `init.svc.*`（modem / 定位 / 温控 / TEE 等）属 P1 能力/服务对齐范围，不在本条目范围。
