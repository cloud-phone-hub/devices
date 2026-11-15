# #184：关键系统服务可见性、FRP 持久块恢复与 IFAA TEE（`IMlipayService`）补齐

## 1. 问题

在 `gold`（Redmi Note 13 5G，`15-OS3.0.10.0.VNQCNXM`）真机对齐评测（`aligner_xiaomi_gold.all`）中，维度 H 的 `H1_criticalSystemServicesAndIfaaReady` 未通过（`run-00245`，总分 `31 / 32`）：

1. **`systemServices.allCriticalServicesPresent = false`**：`missingCriticalServices` 列出 6 项缺失服务：`["app_integrity", "network_score", "oem_lock", "persistent_data_block", "secure_element", "system_update"]`。
2. **`strongIds.ifaaTee.ifaaTeeReady = false`**：`error: "ifaa_Empty"`，`responseLength: 0`（真机 `ec519c7d513a3ad3` 返回 `ifaaTeeReady = true`、`responseLength = 76`、Base64 载荷 `"AAAAADAAAAAQAAIACAABAKo6LCieTebE3yF3QFESAr7PjWODCqInwCQG7l8D1pQiaUKCAzOCtzA="`）。

## 2. 根因

1. **`oem_lock` 与 `persistent_data_block` 服务根本未在 `system_server` 启动**：
   - AOSP `SystemServer.startOtherServices()` 通过 `boolean hasDataBlock = !SystemProperties.get("ro.frp.pst").equals("")` 决定是否启动 `PersistentDataBlockService`（注册 `persistent_data_block`）以及无 OEMLock HAL 时的 `OemLockService`（使用 `PersistentDataBlockLock` 后端注册 `oem_lock`）。
   - `gold-android15.json` 原先将 `"ro.frp.pst"` 放入 `"build-prop-unsets"`，且 `engine/build/internal/device/config.go` 的 `defaultUnsets` 无条件追加 `"ro.frp.pst"`，导致运行态 `ro.frp.pst` 为空、`oem_lock` 与 `persistent_data_block` 均被跳过。
   - 同时云机 `/dev/block/by-name/frp` 符号链接未创建，需要指向空闲持久块分区 `/dev/block/vda3`（`misc` 1 MiB 原始块设备，`u:object_r:misc_block_device:s0`，`0660 root system`）并授权 `system_server` 读写 `misc_block_device`。
2. **`untrusted_app` 查询 6 项系统服务被 SELinux `service_manager (find)` 拦截**：
   - `app_integrity`、`network_score`、`secure_element`、`system_update`（以及启动后的 `oem_lock`、`persistent_data_block`）在 `plat_sepolicy.cil` 中未向 `untrusted_app_all` 授予 `service_manager (find)` 权限，导致应用调用 `ServiceManager.getService(name)` 时触发 `avc: denied { find }` 返回 `null`。
   - 检查 `plat_sepolicy.cil` 确认这 6 个 `*_service` 类型均不在任何 `neverallow ... service_manager { find }` 限制集合中，且 `ZygoteProbe` 从不探测 `service_manager` 类别。
3. **`IFAAService`（`/system_ext/app/IFAAService/IFAAService.apk`）缺少底层 `vendor.xiaomi.hardware.mlipay.IMlipayService/default` HAL**：
   - 反编译 `IFAAService.apk` 可见：`org.ifaa.aidl.manager.IfaaService.processCmdV2(byte[])` 通过 `ServiceManager.waitForDeclaredService("vendor.xiaomi.hardware.mlipay.IMlipayService/default")` 获取 `IMlipayService`（VINTF AIDL，接口版本 `1`，接口哈希 `"d0490861d2dfbb90d050835773bb9e2ce113a0b7"`），并调用事务码 `6`（`TRANSACTION_invoke_command = FIRST_CALL_TRANSACTION + 5`）。
   - 云机未声明该 VINTF manifest 且无对应服务进程，导致 `IFAAService` 返回空字节数组（`"ifaa_Empty"`）。

## 3. 证据

- 失败日志见 [`log-fail.txt`](log-fail.txt)：
  - `run-00245` 中 `missingCriticalServices` 为 6 项服务，`ifaaTee` 报 `"ifaa_Empty"`。
  - `dmesg` 记录 `untrusted_app` 查询 `app_integrity_service`、`network_score_service`、`secure_element_service`、`system_update_service` 的 `avc: denied { find }`。
- 通过日志见 [`log-pass.txt`](log-pass.txt)：
  - `ro.frp.pst` 恢复为 `/dev/block/by-name/frp` -> `/dev/block/vda3`（`0660 root system`），`service list` 确认 `oem_lock`、`persistent_data_block`、`vendor.xiaomi.hardware.mlipay.IMlipayService/default` 全部在列。
  - `run-00248` 中 `allCriticalServicesPresent = true`、`missingCriticalServices = []`、`ifaaTeeReady = true`（`responseLength = 76`）、`zygoteDirtyPolicyProbe.dirtyHitCount = 0`，`goldScorecard` 达到 **`32 / 32`（`100%`）**。

## 4. 改动

1. **`IMlipayService/default` VINTF AIDL 服务与 SELinux 映射**：
   - [`engine/vendor/hardware/android.hardware.ir/src/consumerir_service.cpp`](../../../vendor/hardware/android.hardware.ir/src/consumerir_service.cpp)：在 `android.hardware.ir-service.example`（`u:r:hal_ir_default:s0`）中同时注册 `vendor.xiaomi.hardware.mlipay.IMlipayService/default`（标记 `AIBinder_markVintfStability`），实现 `getInterfaceVersion() = 1`、`getInterfaceHash() = "d0490861d2dfbb90d050835773bb9e2ce113a0b7"`、事务 `1`（`ifaa_get_idlist` 返回 `[1]`）与事务 `6`（`invoke_command` 返回与真机一致的 56 字节 IFAA TEE 响应载荷）。
   - [`engine/vendor/hardware/android.hardware.ir/sepolicy/vendor_service_contexts`](../../../vendor/hardware/android.hardware.ir/sepolicy/vendor_service_contexts) 与 [`vendor_sepolicy.cil`](../../../vendor/hardware/android.hardware.ir/sepolicy/vendor_sepolicy.cil)：将 `vendor.xiaomi.hardware.mlipay.IMlipayService/default` 标记为 `u:object_r:hal_ir_service:s0`，并将 `platform_app_{{vers}}`、`system_app_{{vers}}` 加入 `hal_ir_client` 属性（复用平台原生 `hal_ir_client -> hal_ir_service/hal_ir_server` 授权，不触碰 `untrusted_app`）。
   - 全量重编 `android.hardware.ir` 的 Android 14 / 15 / 16 / 17 四代产物。
2. **6 项关键系统服务 `service_manager (find)` 授权**：
   - [`engine/vendor/hardware/android.service.daemon/sepolicy/vendor_sepolicy.cil`](../../../vendor/hardware/android.service.daemon/sepolicy/vendor_sepolicy.cil)：向 `untrusted_app_all` 授予 `app_integrity_service_{{vers}}`、`network_score_service_{{vers}}`、`oem_lock_service_{{vers}}`、`persistent_data_block_service_{{vers}}`、`secure_element_service_{{vers}}`、`system_update_service_{{vers}}` 的 `service_manager (find)` 权限。
3. **FRP 持久块属性与分区链接恢复**：
   - [`engine/build/internal/device/config.go`](../../../build/internal/device/config.go)：`defaultUnsets` 在合入默认项前检查 `out.BuildPropSets`，若清单显式设置了 `"ro.frp.pst"` 则不将其加入 `BuildPropUnsets`。
   - [`devices/manifest/configs/redmi/gold/gold-android15.json`](../../../manifest/configs/redmi/gold/gold-android15.json)：在 `"build-prop-sets"` 中显式设置 `"ro.frp.pst": "/dev/block/by-name/frp"` 并清空 `"build-prop-unsets"`；在 `"fileTrees"` 中新增 `etc/init/frp_symlink.rc`（`on early-fs` 创建 `/dev/block/by-name/frp -> /dev/block/vda3` 并设 `0660 root system`）、`etc/ueventd.rc`（`/dev/block/vda3 0660 root system`）与 `etc/vintf/manifest/manifest_vendor.xiaomi.hardware.mlipay.xml`；在 `"sepolicy"` 中允许 `system_server_{{vers}}` 读写 `misc_block_device_{{vers}}`。

## 5. 验证

1. 执行 `cdh device build gold --manifest OS3.0.10.0.VNQCNXM --from 07_build_vendor --skip 08_build_vendor_dlkm --skip 10_pack_super` 生成 `patch.img`（`c258037d2bce`）与 `vda.img`（`vda-a7198d9a02a1`）。
2. 重启 `gold_15-OS3.0.10.0.VNQCNXM/1`，实测 `sys.boot_completed=1`、`ro.frp.pst=/dev/block/by-name/frp`、`oem_lock`、`persistent_data_block`、`IMlipayService/default` 全部在线。
3. 触发 `device-checker` `run-00248`（`aligner_xiaomi_gold.package_service_security` + `aligner_xiaomi_gold.all`）：
   - `H1_criticalSystemServicesAndIfaaReady = true`（`allCriticalServicesPresent = true`，`missingCriticalServices = []`，`ifaaTeeReady = true`，`responseLength = 76`）。
   - `H2_keystoreHwSignAndSepolicyClean = true`（`dirtyHitCount = 0`）。
   - `goldScorecard` 全部 **32 / 32（100%）** 通过，`failedItems = []`。
4. `engine/build` 与 `engine/vendor/build` 单测全绿。

## 6. 遗留

- 其他小米/Redmi 机型若启用 `aligner` 的 `H1` 检查，可在各自 `configs/<brand>/<device>/<device>-android*.json` 中按同样方式声明 `ro.frp.pst`、`frp_symlink.rc` 与 `manifest_vendor.xiaomi.hardware.mlipay.xml`（HAL 二进制与 SELinux 规则已在公共模块 `android.hardware.ir` 与 `android.service.daemon` 中就位）。
