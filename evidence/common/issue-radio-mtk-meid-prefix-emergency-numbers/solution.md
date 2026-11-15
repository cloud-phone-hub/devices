# #215：对齐 MTK MEID 厂商码 `99002158` 与虚拟 Modem 紧急号码主动上报

## 问题

`gold_15-OS3.0.11.0.VNQCNXM` 首轮 `aligner.telephony_full`（`4d5128bd8ca07462-cloud`）除两端 SIM 在网状态外，存在 9 条明显的虚拟 modem 能力失真：

1. **MEID Manufacturer Code 不匹配（3 条）**：`telephony.ManufacturerCode` 及双槽 `sim[0,1].ManufacturerCode` 云机为 `"99002082"`，真机（`07cf5fc1784783d5`）为 `"99002158"`；
2. **Modem 紧急号码表缺失（6 条）**：真机每槽由 modem 上报 `10` 条紧急号码（`112 / 911` 为 `src=mdm def`，`000 / 08 / 110 / 118 / 119 / 120 / 122 / 999` 为 `src=mdm`），加上数据库 3 条共计 `13` 条；云机因未触发 modem 紧急号码上报，仅剩 `5` 条（缺失全部 `mdm` 条目，`src=` 为空）。

## 根因

1. `radio_modem.cpp`、`android.service.daemon` `props/profile.rs` 与 `gold-android15.json` 此前使用的 MTK 默认 MEID 前缀是旧值 `"99002082"`；
2. `radio_voice.cpp` 实现了 `RadioVoice` 响应，但从未调用 `IRadioVoiceIndication::currentEmergencyNumberList()` 向 Android Telephony 框架推送 ECC（Emergency Number List）指示。

## 证据

- 修复前：`4d5128bd8ca07462-cloud` `diff/telephony.json` 3 条 `ManufacturerCode` 与 6 条 `EmergencyNumberList` 差异见同目录 [`log-fail.txt`](log-fail.txt)。
- 修复后：`7d10bffa4bc09406-cloud` `aligner.telephony_full` 重采结果与清零核验见同目录 [`log-pass.txt`](log-pass.txt)。

## 改动

1. **对齐 MTK MEID 厂商码 `99002158`**：
   - 更新 [`gold-android15.json`](../../../manifest/configs/redmi/gold/gold-android15.json) 中 `"ro.vendor.radio.meid.prefix": "99002158"`；
   - 更新 [`props/profile.rs`](../../../vendor/hardware/android.service.daemon/src/rust/src/props/profile.rs) 中 `PROFILES` 对应项与 `MTK_PROFILE.meid_prefix_default = "99002158"`（含单测断言）；
   - 更新 [`radio_modem.cpp`](../../../vendor/hardware/android.hardware.radio/src/radio_modem.cpp) 中 `kDefaultMeidPrefix = "99002158"`。
2. **补充 Modem 紧急号码主动上报（[`radio_voice.cpp`](../../../vendor/hardware/android.hardware.radio/src/radio_voice.cpp)）**：
   - 新增 `BuildEmergencyNumberList(slot, snapshot)`：在主槽有卡时附带 `mcc="460", mnc="00"`，无卡/副槽置空，生成与 MTK 真机一致的 10 条紧急号码（`112`/`911` 标 `SOURCE_MODEM_CONFIG | SOURCE_DEFAULT`，`000 / 08 / 110(POLICE) / 118 / 119(FIRE_BRIGADE) / 120(AMBULANCE) / 122 / 999` 标 `SOURCE_MODEM_CONFIG`）；
   - 新增 `PushEmergencyNumberList()`，在 `setResponseFunctions()`（强制首推）以及 `Snapshot` 状态变更观察回调（按 `last_ecc_with_plmn_` 去重）调用 `indication->currentEmergencyNumberList(UNSOLICITED, ...)`。

## 验证

1. 重建 `android.hardware.radio`、`android.service.daemon` 并重打包冷启动 `gold_15-OS3.0.11.0.VNQCNXM/2`（`7d10bffa4bc09406-cloud`）：
   - `getprop ro.vendor.radio.meid.prefix` 与 `sim[0].ManufacturerCode` 均为 `"99002158"`；
   - `telephony.EmergencyNumberList` 共 13 条，完整包含 `db` 3 条与 `mdm def`（`112/911`）+ `mdm` 8 条；
   - `aligner.telephony_full` 重采结果：上述 **3 条 `ManufacturerCode` 与 6 条 `EmergencyNumberList` 差异共 9 项全部清零**（`telephony` 域 `capability: 123 -> 115`、`fatal: 125 -> 117`，未新增任何紧急号码或厂商码差异）。

## 遗留

1. `telephony` 域剩余 115 项能力差异均来源于双端 SIM 运营商与在网状态差异（真机为无服务的中国电信卡片，云机为已注册的中国联通 LTE；含重采时额外出现的 `DataConnectivityPossible: false -> true`）。
