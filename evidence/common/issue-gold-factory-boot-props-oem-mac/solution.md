# #213：补齐 `gold` 出厂引导属性、`gatekeeper` 标识与按实例派生 OEM 相邻 MAC

## 问题

`gold_15-OS3.0.11.0.VNQCNXM` 首轮 `aligner.build_props_full`（`4d5128bd8ca07462-cloud`）在非故意 64 位化区间外暴露出 5 项真机存在而云机 `<缺>` 的身份属性：

- `ro.boot.vbmeta.device`（真机 `PARTUUID=d722c721-0dee-4cb8-8a83-2c63cd1393c7`）
- `ro.hardware.gatekeeper`（真机 `mitee`）
- `ro.ril.factory_id`（真机 `29`）
- `ro.ril.oem.btmac`（真机 `3c:38:24:24:c3:65`）
- `ro.ril.oem.wifimac`（真机 `3c:38:24:24:c3:64`，且与 `btmac` 符合相邻偶数/奇数配对规律）

## 根因

1. `gold-android15.json` 的 `buildVendor.build-prop-sets` 未配置 `ro.hardware.gatekeeper`、vbmeta 分区设备标识 `ro.vendor.cdh.vbmeta.device`、工厂 ID `ro.vendor.ril.factory_id` 以及小米 OUI 前缀 `ro.vendor.cdh.oem.mac_oui`。
2. `android.service.daemon` 的 `props/fixups.rs` 尚未将 `ro.vendor.cdh.vbmeta.device` 写入 `ro.boot.vbmeta.device`，也未在 `ensure_oem_modem_identity()` 中透传 `ro.ril.factory_id` 并按实例 `serialno` 确定性生成合法的 OEM WiFi/BT 相邻 MAC 对。

## 证据

- 修复前：`4d5128bd8ca07462-cloud` `diff/build_props.json` 5 项 `<缺>` 记录见同目录 [`log-fail.txt`](log-fail.txt)。
- 修复后：`7d10bffa4bc09406-cloud` 实机 `getprop` 与重采 `diff/build_props.json` 结果见同目录 [`log-pass.txt`](log-pass.txt)。

## 改动

1. **[`gold-android15.json`](../../../manifest/configs/redmi/gold/gold-android15.json)**：
   - 在 `buildVendor.build-prop-sets` 补充：`ro.hardware.gatekeeper: "mitee"`、`ro.vendor.cdh.vbmeta.device: "PARTUUID=d722c721-0dee-4cb8-8a83-2c63cd1393c7"`、`ro.vendor.ril.factory_id: "29"`、`ro.vendor.cdh.oem.mac_oui: "3c:38:24"`。
2. **[`props/fixups.rs`](../../../vendor/hardware/android.service.daemon/src/rust/src/props/fixups.rs)**：
   - `ensure_boot_security_props()`：当 `ro.boot.vbmeta.device` 为空且 `ro.vendor.cdh.vbmeta.device` 非空时写入 `ro.boot.vbmeta.device`；
   - `ensure_oem_modem_identity()`：从 `ro.vendor.ril.factory_id` / `ro.vendor.cdh.ril.factory_id` 补齐 `ro.ril.factory_id`；新增 `fnv1a32()`、`normalize_mac_oui()` 与 `derive_oem_mac_pair(oui, serialno)`，按 OUI（默认 `"3c:38:24"`）与实例 `serialno` 确定性派生连续偶数 `wifimac` 与 `+1` `btmac`，同步写入 `ro.vendor.oem.{wifi,bt}mac` 与 `ro.ril.oem.{wifi,bt}mac`；
   - 新增单测 `mac_pair_is_deterministic_and_adjacent`。

## 验证

1. `service_daemon` ARM64 单测在 guest 内执行通过（`11 passed; 0 failed`，含 `mac_pair_is_deterministic_and_adjacent`）。
2. 实例 `gold_15-OS3.0.11.0.VNQCNXM/2`（`7d10bffa4bc09406-cloud`）重采 `aligner.build_props_full`：
   - `ro.boot.vbmeta.device=PARTUUID=d722c721-0dee-4cb8-8a83-2c63cd1393c7`、`ro.hardware.gatekeeper=mitee`、`ro.ril.factory_id=29` 3 项 `<缺>` 彻底清零（`build_props` 身份差异从 `19` 降至 `16`）；
   - `ro.ril.oem.wifimac=3c:38:24:80:07:c4` 与 `ro.ril.oem.btmac=3c:38:24:80:07:c5` 由 `<缺>` 变为完整合法的同小米 OUI `wifimac/+1=btmac` 每实例唯一值（跨设备实例存在差异属预期单机身份）。

## 遗留

1. `services` 域中 `android.hardware.gatekeeper.IGatekeeper/default` 桩服务仍属 P1 服务补齐项（待后续按 P1 服务桩清单统一补齐）。
2. `ro.boot.chipid` 为单机随机 128-bit ID，云机已按实例输出合法格式。
