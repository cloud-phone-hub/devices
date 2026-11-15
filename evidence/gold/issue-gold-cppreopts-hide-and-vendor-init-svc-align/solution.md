# #222：隐藏 AOSP `init.svc.cppreopts` 并补齐 `gold` 44 个纯厂商 `init.svc.*` 服务

## 问题

`gold_15-OS3.0.11.0.VNQCNXM` 改前 `aligner.build_props_full`（`7d10bffa4bc09406-cloud`）的 `init.svc.*` 运行态属性存在明显两端偏差：

1. 云机普通 App 可见 AOSP 占位服务属性 `init.svc.cppreopts = "stopped"`，而真机无该属性（`<缺>`）；
2. 真机存在 **57 个** `init.svc.*` 运行态属性而云机完全缺失（涵盖 `ccci_mdinit`、`nvram_daemon`、`nvram-aidl`、`mnld`、`agpsd`、`mtk_lbs_service`、`mi_thermald`、`thermal_manager`、`thermalloadalgod`、`camerahalserver`、`tee-supplicant`、`miteelog`、`fuelgauged`、`batterysecret` 等厂商核心守护进程与一次性初始化服务）。

## 根因

1. 官方 `/system/etc/init/cppreopts.rc` 定义的 `cppreopts` 在云机无意义启动流程中进入 `stopped` 并写入公开可见的 `init.svc.cppreopts` 属性；
2. 云机未为真机专属纯 vendor 后台/HAL 服务提供同名 init 占位定义，导致 init 未创建对应的 `init.svc.<name>` 属性。

## 证据

- 改前实机 `init.svc.cppreopts=[stopped]`、`mi_thermald=[]`、`nvram_daemon=[]` 及改前 58 条 `init.svc.*` 差异明细见 [`log-fail.txt`](log-fail.txt)。
- 改后实例 `4abff687a02e421f-cloud` 逐条校验 44 个服务（`running=34, stopped=10, mismatched=[]`）、`init.svc.cppreopts=[]` 与重采剩余 17 条明细见 [`log-pass.txt`](log-pass.txt)。

## 改动

1. **[`gold-android15.json`](../../../manifest/configs/redmi/gold/gold-android15.json)**：
   - 在 `modules.redirect` 追加文件重定向：`"/system/etc/init/cppreopts.rc"` → `"/mnt/vendorpatch/vendor/etc/init/cph_empty.rc"`，遵循 [`AGENTS.md §8`](../../../../AGENTS.md) 不篡改官方 `system.img`，启动期静默屏蔽 `cppreopts` 服务定义；
   - 在 `buildVendor.vendor-files` 新增 `etc/init/cph_gold_vendor_svc_align.rc`：经全分区 `.rc` 逐一审计确认无重名、无属性触发器冲突后，安全补齐 **44 个** 纯厂商 `init.svc.*` 服务（10 个 `oneshot` 使用 `/system/bin/true` → `stopped`，34 个常驻 HAL/守护进程使用 `/system/bin/sleep 2147483647` → `running`，`seclabel u:r:pioneer:s0`）。

## 验证

1. 重跑 `03 -> 05 -> 07 -> 08 -> 09 -> 11` 重打包并新建干净实例 `gold_15-OS3.0.11.0.VNQCNXM/2`（`4abff687a02e421f-cloud`）验证：
   - `getprop init.svc.cppreopts` 为空；
   - 实机逐条校验 44 个纯厂商服务：`running=34, stopped=10, mismatched=[]`（如 `init.svc.mi_thermald=[running]`、`init.svc.nvram_daemon=[stopped]`）；
   - 重采 `aligner.build_props_full`：`init.svc.cppreopts` 与全部 44 个纯厂商服务 **45 条差异全部清零**，`build_props.raw` 从 `533` 降至 `484`（含 [`#218`](../../common/issue-userdata-f2fs-geometry-and-metadata-opts/solution.md) `vold.has_compress` 与 [`#221`](../../common/issue-tmpfs-superblock-limits-and-dex2oat-threads/solution.md) `dex2oat-threads`）。

## 遗留

1. 经审计余下的 13 个真机多出服务（`aee-reinit`、`aee_aed`、`aee_aed64`、`boringssl_self_test32`、`boringssl_self_test32_vendor`、`boringssl_self_test_apex32`、`connsyslogger`、`gsid`、`snapuserd`、`snapuserd_proxy`、`update_verifier`、`vndservicemanager`、`zygote_secondary`）因涉及 `/system`/apex `.rc`、属性触发器或纯 64 位有意锁定口径暂未并入（另 4 项 `adbd`/`artd`/`hidl_memory`/`hwservicemanager` 为实时运行态变化）。
