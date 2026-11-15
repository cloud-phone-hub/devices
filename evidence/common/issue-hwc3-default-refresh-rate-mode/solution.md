# #214：HWC3 按配置刷新率选定物理屏 `defaultModeId` 与 Boot Display Config 接口

## 问题

`gold_15-OS3.0.11.0.VNQCNXM` 首轮 `aligner.display_full`（`4d5128bd8ca07462-cloud`）在 `display.fields.mDisplayInfo` 中存在 `capability` 差异：

- 真机（`07cf5fc1784783d5`）：`mode 2, renderFrameRate 120.00001, defaultMode 2`
- 云机（`4d5128bd8ca07462-cloud`）：`mode 2, renderFrameRate 120.00001, defaultMode 1`（`dumpsys display` 中 `defaultModeId 1`、`mDefaultModeId=1`）

## 根因

1. `gold-android15.json` 中 `ro.vendor.cdh.display.refresh_rates="60,120,90"` 将 `60Hz` 排在首位（`configId=1`），`120Hz` 为 `configId=2`；
2. `CdhComposerClient::EnsurePhysicalDisplay()` 此前无条件将 `built.configs.front()`（即 `configId=1` / 60Hz）设为初始 `active_config_id` / `active_vsync_period`，且未记录 `default_config_id`；
3. 同时 `CdhComposerClient` 的 `setBootDisplayConfig` / `clearBootDisplayConfig` / `getPreferredBootDisplayConfig` 直接透传底层 `inner_`，未在物理屏上返回或覆盖首选用默认刷新率对应的 `configId`。

## 证据

- 修复前：`4d5128bd8ca07462-cloud` `diff/display.json` `display.fields.mDisplayInfo` 差异见同目录 [`log-fail.txt`](log-fail.txt)。
- 修复后：`7d10bffa4bc09406-cloud` `dumpsys display` 输出与重采 `diff/display.json` 结果见同目录 [`log-pass.txt`](log-pass.txt)。

## 改动

1. **[`gold-android15.json`](../../../manifest/configs/redmi/gold/gold-android15.json)**：
   - 在 `buildVendor.build-prop-sets` 新增 `"ro.vendor.cdh.display.default_refresh_rate": "120"`。
2. **[`CdhDisplayConfig.h`](../../../vendor/hardware/android.hardware.graphics.composer3/src/CdhDisplayConfig.h) 与 [`CdhDisplayConfig.cpp`](../../../vendor/hardware/android.hardware.graphics.composer3/src/CdhDisplayConfig.cpp)**：
   - 结构体 `CdhDisplayConfig` 新增 `int32_t default_refresh_rate = 0`，`LoadCdhDisplayConfig()` 解析 `ro.vendor.cdh.display.default_refresh_rate`（限 `30..240` Hz 合法范围）。
3. **[`CdhComposer.h`](../../../vendor/hardware/android.hardware.graphics.composer3/src/CdhComposer.h) 与 [`CdhComposer.cpp`](../../../vendor/hardware/android.hardware.graphics.composer3/src/CdhComposer.cpp)**：
   - `PhysicalDisplayState` 新增 `default_config_id` 与 `std::optional<int32_t> boot_config_override`，新增辅助函数 `FindDisplayConfigById()`；
   - `EnsurePhysicalDisplay()` 若配置了 `cfg_.default_refresh_rate > 0`，按 `std::lround(1e9 / vsyncPeriod)` 匹配目标配置（`gold` 上命中 `120Hz` 即 `configId=2`），设为 `default_config_id`、`active_config_id` 与 `active_vsync_period`；
   - 对非虚拟屏拦截实现 `setBootDisplayConfig`、`clearBootDisplayConfig` 与 `getPreferredBootDisplayConfig`（返回 `boot_config_override.value_or(state.default_config_id)`）。

## 验证

1. `gold_15-OS3.0.11.0.VNQCNXM/2`（`7d10bffa4bc09406-cloud`）实机复测：
   - `dumpsys display` 实测 `modeId 2, renderFrameRate 120.00001, defaultModeId 2`、`mActiveModeId=2`、`mDefaultModeId=2`；
   - `aligner.display_full` 重采确认 `display.fields.mDisplayInfo` 差异彻底清零（`display` `capability: 6 -> 5`、`fatal: 6 -> 5`）。

## 遗留

1. `display` 域剩余 P1 差异主要为 `gles.extensions` 列表仍为宿主桌面扩展剔除黑名单后的子集而非真机 Mali 扩展全集；另两条 `configuration.mnc` / `seq` 分别属 SIM 运营商差异与系统变更计数。
