# #177：电话 RIL 双卡 TAC / MEID / SVN 与蓝牙默认适配器名称对齐

- **分组**：`common/`（`android.hardware.radio` 与 `android.service.daemon` 跨机型共用）
- **首发验证机型**：Xiaomi Redmi Note 13 5G（`gold` / `2312DRAABC`，`OS3.0.10.0.VNQCNXM`）

## 1. 问题

在双机网络、电话与蓝牙对比（[`network_telephony_bt_gnss.js`](../../../../../core/development/device-checker/checks/java/aligner/xiaomi/gold/network_telephony_bt_gnss.js)）中，`G3` 与 `G4` 两项检查未通过：

1. **`G3_telephonyTacMatchesGold86717107 = false`**：
   - `TelephonyManager.getTypeAllocationCode(0/1)` 云机返回 `"35948214"`（真机为小米 `gold` 官方 GSMA TAC `"86717107"`）；
   - `TelephonyManager.getManufacturerCode(0/1)` 云机返回 `null`（真机为 `"99002082"`，`logcat -b radio` 报 `ImeiMeidSource: invalid meid= slot=0/1`）；
   - `TelephonyManager.getDeviceSoftwareVersion(0/1)` 云机返回 `"01"`（真机为 `"78"`）。
2. **`G4_bluetoothAdapterHasDeviceName = false`**：
   - `BluetoothAdapter.getDefaultAdapter().getName()` 云机返回 `null`（真机为 `"Redmi Note 13 5G"`）。

## 2. 根因

1. **`RadioModem` 硬编码测试前缀且未回填 MEID**：
   - `android.hardware.radio/src/radio_modem.cpp` 的 `ImeiFromSnapshot()` 硬编码 `body = "35948214" + suffix` 与 `svn = "01"`，且按 SIM `iccid` 派生（无卡槽 `slot=1` 得到 `000000`）；
   - `getDeviceIdentity()` 固定将第 4 个字符串参数 `meid` 传空串 `""`，导致框架 `GsmCdmaPhone` / `ImeiMeidSource` 无法提取 8 位 `ManufacturerCode`（`meid.substring(0, 8)`），同时 MIUI 侧 `ro.ril.oem.*` / `ro.vendor.oem.*` 属性缺失。
2. **跳过首次激活向导导致 `BluetoothManagerService` 缓存名称未初始化**：
   - 真机开机向导会将 `ro.product.marketname` 写入 `Settings.Secure.bluetooth_name` 并完成蓝牙控制器地址登记（`bluetooth_addr_valid=1`）；
   - 云机未设置 `bluetooth.device.default_name` 属性，且新数据卷缺少 `bluetooth_addr_valid=1` 与 `bluetooth_name`，导致 `BluetoothManagerService` 在蓝牙默认关闭态（`Bluetooth never enabled!`）下 `getName()` 恒返回 `null`。

## 3. 证据

- 修复前取证日志见同目录 [`log-fail.txt`](log-fail.txt)（`run-00222` 基线：`tac0/1="35948214"`、`mfg0/1=null`、`svn0/1="01"`、`btName=null`）。
- 修复后取证日志见同目录 [`log-pass.txt`](log-pass.txt)（`run-00223` 实测：`tac0/1="86717107"`、`mfg0/1="99002082"`、`svn0/1="78"`、`btName="Redmi Note 13 5G"`）。

## 4. 改动

1. **RIL Modem 双槽身份参数化与 MEID 补齐（[`radio_modem.cpp`](../../../../vendor/hardware/android.hardware.radio/src/radio_modem.cpp)）**：
   - 新增 `DeviceIdentityFromSnapshot(snapshot, slot)`：支持通过只读属性 `ro.vendor.radio.imei.tac`、`ro.vendor.radio.meid.prefix`、`ro.vendor.radio.imei.svn` 及 `ro.vendor.oem.{imei,imei2,meid}` 按机型覆盖；`CDH_RADIO_MTK` 变体默认对齐 `86717107` / `99002082` / `78`；
   - 基于实例持久 `ro.boot.serialno` 的 FNV-1a 哈希生成稳定的双槽相邻 6 位序号（槽 0 偶数 `snr` + Luhn 校验位 `PRIMARY`，槽 1 奇数 `snr+1` + Luhn 校验位 `SECONDARY`）及 14 位 `meid`，在 `getImei()`、`onImeiMappingChanged()` 与 `getDeviceIdentity()` 中完整返回。
2. **实例启动身份与蓝牙默认名同步（[`boardid.rs`](../../../../vendor/hardware/android.service.daemon/src/rust/src/boardid.rs) + [`cph-setup-display`](../../../../vendor/hardware/android.service.daemon/vendor/bin/cph-setup-display)）**：
   - `boardid.rs` 新增 `ensure_bluetooth_default_name()`（从 `ro.product.marketname` / `ro.product.model` 补齐 `bluetooth.device.default_name`）与 `ensure_oem_modem_identity()`（在 MIUI 机型上按同一 `serialno` 算法写齐 `ro.vendor.oem.*`、`ro.ril.oem.*`、`ro.ril.miui.imei*`）；
   - `cph-setup-display` 新增首启蓝牙适配器名称与缓存初始化逻辑，幂等写入 `Settings.Secure.bluetooth_name` 与 `Settings.Global.device_name`，并在首次未初始化时完成一次控制器缓存刷新后恢复 `STATE_OFF`。

## 5. 验证

1. **镜像构建与启动验证**：重编 `android.hardware.radio@mtk` 与 `android.service.daemon`，完成 `gold` `07 -> 09 -> 11`（`patch.img` `sha256 59062137ad00`，`vda-5af788d1edad`）；重启实例 `1`，`boot_check.py` `[pass]`（平均亮度 `121.6`），`dumpsys bluetooth_manager` 显示 `state: OFF`、`name: Redmi Note 13 5G`，`logcat -b radio` 的 `ImeiMeidSource` 零报错。
2. **双机对账（`run-00223`）**：
   - 真机 `ec519c7d513a3ad3`：`32 / 32` 通过。
   - 云机 `df809591a4e085ae-cloud`：`G3_telephonyTacMatchesGold86717107: true`、`G4_bluetoothAdapterHasDeviceName: true`（维度 G 全部 4 项 `G1..G4` 全绿），总分由 **`20 / 32` 提升至 `22 / 32`**。

## 6. 遗留

- 非 MTK 机型若需对齐特定品牌 GSMA TAC 或 MEID 前缀，可在对应基线 `vendor.build-prop-sets` 中配置 `ro.vendor.radio.imei.tac`、`ro.vendor.radio.meid.prefix` 与 `ro.vendor.radio.imei.svn`，无需重编 `android.hardware.radio`。
