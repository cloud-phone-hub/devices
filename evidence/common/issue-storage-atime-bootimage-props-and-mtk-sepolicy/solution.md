# #186：存储老化时间戳、`ro.bootimage.build.*` 属性补齐、MTK 公开属性 SELinux 授权与 `/vendor/etc` 对齐

## 1. 问题

在对 `gold`（Xiaomi Redmi Note 13 5G，`15-OS3.0.10.0.VNQCNXM`）开展双机 86 套件深水区全量对比时，暴露出以下存储、属性与 SELinux 细节差异：

1. **`ultimate.cloud_emulator` 命中 `U-DATA-ATIME`（云机独有风控项）**：新创建的云机可写数据盘 `/data` 与 `/storage/emulated/0` 各级目录 `atime`/`mtime` 停留在建盘当天（`ageDays = 1`），触发新盘/快照克隆机年龄告警（真机 `ageDays >= 105`）。
2. **`/storage/emulated/0` 标准共享目录不完整**：首次启动早期缺少 `Documents`、`Audiobooks`、`Recordings` 等目录，导致 `files.storage` 20 个探测路径未对齐。
3. **10 项 `ro.bootimage.build.*`、区域与基带默认属性缺失**：云机 UEFI+GRUB 引导链未经过原厂 `bootimage` 属性合入，`ro.bootimage.build.{date,date.utc,description,fingerprint,id,tags,type,version.incremental,version.release,version.release_or_codename}` 全为空；同时缺少 `persist.sys.locale="zh-CN"` 与 `gsm.version.ril-impl="android reference-ril 1.0"`。
4. **普通 App（`untrusted_app`）无法读取 5 项 MTK 公开平台属性**：`ro.vendor.mediatek.rsc_name`、`ro.vendor.soc.model`、`ro.vendor.mediatek.platform`、`ro.vendor.mtk_camera_app_version`、`ro.vendor.have_aeev_feature` 在真机上对普通 App 可读，而云机落入默认 `vendor_default_prop` 被 SELinux 拦截返回空串。
5. **`/vendor/etc/public.libraries.txt` 行数与 `/vendor/etc/md` 标签不齐，且重复 `property_contexts` 前缀触发冲突**：基底 `public.libraries.txt` 缺少官方厂商 NDK 公开库行（真机 21 行）；`/vendor/etc/md` 落成 `unlabeled`；机型配置覆盖已有 `vendor_property_contexts` 前缀时因未去重导致 `secilc` 报重复条目错误。

## 2. 根因

1. **新盘 `qcow2` 首启无历史使用痕迹**：`vold` 初始化 `/data/media/0` 时以当前挂钟时间写入目录 inode 时间戳，`U-DATA-ATIME` 取 `/storage/emulated/0` 及标准子目录最早 `atime` 计算设备使用天数，`ageDays < 3` 即判定为新建沙箱实例。
2. **`boardid.rs` 未为 `ro.bootimage.build.*` 建立回退链**：云机 `ro.vendor.bootimage.build.*` 未必存在于所有 ROM 变体，当缺失时未回退读取 `ro.vendor.build.*` / `ro.system.build.*`；同时未设置 `persist.sys.locale` 与 `gsm.version.ril-impl` 缺省值。
3. **Lineage 基底 `vendor_sepolicy.cil` 不含联发科专有 `vendor_mtk_*_prop` 公开读属性**：真机 vendor SELinux 将上述 MTK 平台标识属性标为 `mtk_default_prop` / 公开可读类型并允许 `domain` 读取；云机基底仅保留通用 AOSP 属性上下文，导致这些 `ro.vendor.*` 前缀回退至 `vendor_default_prop`（`neverallow untrusted_app vendor_default_prop:file read`）。
4. **`07_build_vendor` 合并 `vendor_property_contexts` 时直接追加未按前缀去重**：当机型 `gold-android15.json` 为已存在于模块层的前缀重映射类型时，同一属性前缀在合并后的 `vendor_property_contexts` 中出现两次，`secilc` / `property_service` 拒绝重复定义。

## 3. 证据

- 失败日志见 [`log-fail.txt`](log-fail.txt)：
  - `run-00002` 中 `ultimate.cloud_emulator` 的 `U-DATA-ATIME` `hit: true`（`ageDays: 1`，`earliestAtime: "2026-11-10T03:50:40.000Z"`）。
  - `adb shell` 实测 `ro.bootimage.build.*` 为空、普通 App 读 `ro.vendor.soc.model` 等为空、`/vendor/etc/md` 为 `unlabeled`。
- 通过日志见 [`log-pass.txt`](log-pass.txt)：
  - 全新干净盘冷启动后 `ultimate.cloud_emulator` 中 `U-DATA-ATIME` `hit: false`（`ageDays: 120`，`earliestAtime: "2026-07-14T09:51:26.000Z"`）。
  - `files.storage` 20 个路径全部就位；`properties.getprop`（普通 App 域）成功读出 `ro.bootimage.build.fingerprint="Redmi/gold/gold:15/AP3A.240905.015.A2/OS3.0.10.0.VNQCNXM:user/release-keys"`、`ro.vendor.mediatek.rsc_name="gold_cn_pa"`、`ro.vendor.soc.model="MT6833"`、`ro.vendor.mediatek.platform="MT6833"`、`ro.vendor.mtk_camera_app_version="3"`、`ro.vendor.have_aeev_feature="1"`、`persist.sys.locale="zh-CN"`、`gsm.version.ril-impl="android reference-ril 1.0"`。
  - `/vendor/etc/public.libraries.txt` 21 行与真机逐字节一致（`files.vendor_content` 0 差异），`ls -Zd /vendor/etc/md` 为 `u:object_r:vendor_etc_md_file:s0`。

## 4. 改动

1. **共享存储目录补齐与 120 天老化回拨**：
   - [`engine/vendor/hardware/android.service.daemon/vendor/bin/cph-storage-align`](../../../vendor/hardware/android.service.daemon/vendor/bin/cph-storage-align)：幂等补齐 `/storage/emulated/0` 下 `Alarms`、`Android`、`Audiobooks`、`DCIM`、`Documents`、`Download`、`Movies`、`Music`、`Notifications`、`Pictures`、`Podcasts`、`Recordings`、`Ringtones` 等标准共享目录，并将 `/data`、`/storage/emulated`、`/storage/emulated/0` 及其标准子目录的 `atime`/`mtime` 回拨约 120 天。
2. **`ro.bootimage.build.*`、区域与 RIL 属性补齐**：
   - [`engine/vendor/hardware/android.service.daemon/src/rust/src/boardid.rs`](../../../vendor/hardware/android.service.daemon/src/rust/src/boardid.rs)：新增 `ensure_bootimage_build_props()`，按 `ro.vendor.bootimage.build.{s}` → `ro.vendor.build.{s}` → `ro.system.build.{s}` → `ro.build.{s}` 顺序非覆盖式补齐 10 项 `ro.bootimage.build.*`；新增 `ensure_telephony_and_locale_defaults()` 补齐 `gsm.version.ril-impl` 与 `persist.sys.locale`。
3. **`vendor_property_contexts` 按前缀覆盖去重**：
   - [`engine/build/stages/07_build_vendor/sepolicy.go`](../../../build/stages/07_build_vendor/sepolicy.go) 与 [`sepolicy_test.go`](../../../build/stages/07_build_vendor/sepolicy_test.go)：合并 `vendor_property_contexts` 时以后入规则覆盖同前缀旧规则，消除重复前缀冲突并补充单测。
4. **`gold` 官方文件直取与 MTK 公开属性 SELinux 授权**：
   - [`devices/manifest/configs/redmi/gold/gold-android15.json`](../../../manifest/configs/redmi/gold/gold-android15.json)：`vendor.files` 直取官方 `etc/public.libraries.txt`（21 行）与 `etc/md`；声明 `vendor_etc_md_file` 文件上下文与 `vendor_mtk_public_prop` 属性类型（向 `appdomain` 开放 `property_service` 只读映射，严格避开 `vendor_default_prop` `neverallow` 边界），并在 `build-prop-sets` 设置 `ro.vendor.mediatek.rsc_name="gold_cn_pa"`。

## 5. 验证

1. `cd engine/build && go test -count=1 ./stages/07_build_vendor` 与 `cd engine/vendor/build && go test -count=1 ./...` 全部通过。
2. 全量重打 `gold` `15-OS3.0.10.0.VNQCNXM`（`patch.img` `196c697bb633`，`vda-4ab0452c965b`），清空 `writable-*.qcow2` 后全新干净盘冷启动实例 `1`，`boot_check.py` `[pass]`。
3. 双机实测（`0b7cd3333759d674-cloud` vs `ec519c7d513a3ad3`，`run-00014..00020`）：
   - `U-DATA-ATIME`: `hit = false`（`ageDays = 120`）；
   - `ro.bootimage.build.*` 10 项全齐，5 项 MTK 公开属性在 `untrusted_app` 域全部可读且与真机一致；
   - `zygoteDirtyPolicyProbe.dirtyHitCount = 0`，`aligner_xiaomi_gold.all` **`32 / 32 (100%)`**。

## 6. 遗留

- 其他联发科机型（如 `cph2797`、`g1170`）若需向普通 App 暴露同款 `ro.vendor.mediatek.*` 公开属性，可直接复用同一 `vendor_mtk_public_prop` SELinux 片段。
