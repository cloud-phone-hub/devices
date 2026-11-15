# #188：Audio 23 项音效描述符、`48000` Hz / `256` 帧原生输出、DRM ClearKey、GNSS 年份与 Health 微安电流对齐

## 1. 问题

在 `gold`（`15-OS3.0.10.0.VNQCNXM`）多媒体、DRM、电池与定位维度的深水区逐字段对比中，云机与真机存在以下差异：

1. **Audio 音效数量与原生输出采样率/缓冲帧数不齐**：
   - `AudioEffect.queryEffects()` 返回 `effectCount = 0`（真机为 `23` 项，含 Dolby DAX、MiSound、NXP Bundle 等厂商音效描述符）；
   - `AudioManager.getProperty(PROPERTY_OUTPUT_SAMPLE_RATE)` 返回 `"44100"`（一度因 AIDL 转换报 `BAD_VALUE` 退化为 `null`），`PROPERTY_OUTPUT_FRAMES_PER_BUFFER` 返回 `"2048"`，而真机为 **`"48000"`** 与 **`"256"`**；
   - 超声波能力属性 `SUPPORT_AUDIO_SOURCE_UNPROCESSED` / `SUPPORT_MIC_NEAR_ULTRASOUND` / `SUPPORT_SPEAKER_NEAR_ULTRASOUND` 未与真机（`"false"` / `"true"` / `"true"`）对齐。
2. **DRM Scheme UUID 数量缺失**：真机 `MediaDrm.getSupportedCryptoSchemes()` 返回 **4 个** UUID（Widevine `edef8ba9-...` + ClearKey `e2719d58-...` + Common PSSH `1077efec-...` + DASH-IF ClearKey `3d5e6d35-...`），云机仅暴露 Widevine 1 个。
3. **电池实时电流恒为 0**：`BatteryManager.BATTERY_PROPERTY_CURRENT_NOW` 与 `BATTERY_PROPERTY_CURRENT_AVERAGE` 返回 `0`（真机亮屏放电态为负值微安级动态电流，如 `-120000` ~ `-60000` uA）。
4. **GNSS 硬件年份未对齐**：`LocationManager.getGnssYearOfHardware()` 未返回 MT6833 基线硬件年份 `2020`。

## 2. 根因

1. **Audio AIDL HAL 两级加载链路问题（导致 `AudioPolicyManager` 回退 `44100` Hz 默认配置）**：
   - **第一级（`libaudio_aidl_conversion_common_cpp.so` 报 `BAD_VALUE`）**：在 AOSP `aidl2legacy_AudioDeviceDescription_audio_devices_t` 中，`AudioDeviceType::IN_SUBMIX` 与 `OUT_SUBMIX` 强制要求 `connection == AudioDeviceDescription::CONNECTION_VIRTUAL`（`"virtual"`）。旧 `ModuleImpl.cpp` 对所有内置端口统一写 `connection = ""`，导致 `AudioPolicyManager::loadFromAidl()` 转换失败并回退到 `AudioPolicyConfig::setDefault()`（硬编码请求 `44100` Hz）；
   - **第二级（`Hal2AidlMapper::initialize()` 动态连接与试连断开）**：将 `IN_SUBMIX` / `OUT_SUBMIX` 设为 `CONNECTION_VIRTUAL` 且初始 `address = ""`（模板设备口）后，`libaudiohal@aidl.so` 的 `Hal2AidlMapper::initialize()` 在启动期依次调用 `IModule::connectExternalDevice` 连接 `IN_SUBMIX(address="0")`、连接 `OUT_SUBMIX(address="0")`、再立即调用 `IModule::disconnectExternalDevice` 断开 `OUT_SUBMIX` 试连端口！旧 `ModuleImpl` 的 `connectExternalDevice` / `disconnectExternalDevice` 硬编码返回 `EX_ILLEGAL_ARGUMENT`（`-22`），导致 `loadHwModule("primary")` 失败；
   - **缓冲帧数与音效配置**：`ModuleImpl` / `VdacStream` 原默认块帧数为 `2048`，且 `EffectFactoryImpl` 仅返回硬编码空表，未解析 `/vendor/etc/audio_effects_descriptors.xml`（或厂商音效 XML）。
2. **DRM HAL 仅注册了 Widevine 单实例或 ClearKey Scheme 列表不完整**：`drm_service.cpp` 未完整实现 W3C Common PSSH（`1077efec-c0b2-4d02-ace3-3c1e52e2fb4b`）、ClearKey（`e2719d58-a985-b3c9-781a-b030af78d30e`）与 DASH-IF ClearKey（`3d5e6d35-9b9a-41e8-b843-dd3c6e72c42c`）三项标准 UUID 支持。
3. **Health AIDL HAL 未合成微安级动态电流，GNSS AIDL HAL 硬件年份写死旧默认值**：`health_service.cpp` 将 `currentNowMicroamps` / `currentAverageMicroamps` 置 0；`gnss_service.cpp` 未对齐 `yearOfHw = 2020`。

## 3. 证据

- 失败日志见 [`log-fail.txt`](log-fail.txt)：
  - 修复前 `outputSampleRate = "44100"`、`outputFramesPerBuffer = "2048"`、`effectCount = 0`；DRM 仅 1 个 UUID；电池 `CURRENT_NOW = 0`。
- 通过日志见 [`log-pass.txt`](log-pass.txt)：
  - 全新干净盘双机复测（`run-00014..00020`）中，`camera_media_drm_audio.audio` 实测 **`effectCount = 23`**、**`outputSampleRate = "48000"`**、**`outputFramesPerBuffer = "256"`**、`isVolumeFixed = false`、`music.maxVolume = 15`、`supportAudioSourceUnprocessed = "false"`、`supportMicNearUltrasound = "true"`、`supportSpeakerNearUltrasound = "true"`，与真机 **100% 一致**。
  - `camera_media_drm_audio.drm` 中 `supportedCryptoSchemes` 完整返回 4 个 UUID（与真机集合完全相等）。
  - `battery_power_thermal.battery` 实测 `bmCurrentNowUa = -62500`、`bmCurrentAvgUa = -63100`、`instantPowerWatts = 0.2741`。
  - `network_telephony_bt_gnss.locationGnss` 实测 `gnssYearOfHardware = 2020`（与真机一致）。

## 4. 改动

1. **Audio AIDL HAL `48000` Hz / `256` 帧、动态虚拟子混音端口连接与 23 项音效描述符**：
   - [`engine/vendor/hardware/android.hardware.audio/src/ModuleImpl.h`](../../../vendor/hardware/android.hardware.audio/src/ModuleImpl.h) 与 [`ModuleImpl.cpp`](../../../vendor/hardware/android.hardware.audio/src/ModuleImpl.cpp)：
     - 主输出流采样率锁定 `48000` Hz，默认/最小缓冲帧数对齐 `256` 帧，超声波 `AudioPort` 频响范围对齐真机；
     - `makeDeviceExt()` 对 `IN_SUBMIX` / `OUT_SUBMIX` 设置 `connection = AudioDeviceDescription::CONNECTION_VIRTUAL` 且模板口 `address = ""`；
     - 完整实现 `connectExternalDevice`（支持幂等复用已连接 `(type, address)` 端口，动态分配 `port.id >= 100` 并自动从同类型模板口克隆/扩充 `mRoutes`）、`disconnectExternalDevice`（清理 `mConnectedExternalPorts`、`mPorts`、`mPortConfigs` 及关联 `mRoutes`）与 `prepareToDisconnectExternalDevice`。
   - [`engine/vendor/hardware/android.hardware.audio/src/VdacStream.cpp`](../../../vendor/hardware/android.hardware.audio/src/VdacStream.cpp)：对齐 `256` 帧周期读写时序。
   - [`engine/vendor/hardware/android.hardware.audio/src/EffectFactoryImpl.cpp`](../../../vendor/hardware/android.hardware.audio/src/EffectFactoryImpl.cpp)：支持解析 `/vendor/etc/audio_effects_descriptors.xml` 暴露全部 23 项音效 `Descriptor`。
   - [`devices/manifest/configs/redmi/gold/gold-android15.json`](../../../manifest/configs/redmi/gold/gold-android15.json)：在 `fileTrees` 提供 `gold` 23 项音效描述符配置。
2. **DRM ClearKey 3 UUID、GNSS 年份与 Health 微安电流**：
   - [`engine/vendor/hardware/android.hardware.drm/src/drm_service.cpp`](../../../vendor/hardware/android.hardware.drm/src/drm_service.cpp)：补齐 ClearKey、W3C Common PSSH、DASH-IF ClearKey 3 个 Scheme UUID（连同 Widevine 共 4 个）。
   - [`engine/vendor/hardware/android.hardware.gnss/src/gnss_service.cpp`](../../../vendor/hardware/android.hardware.gnss/src/gnss_service.cpp)：`setCallback` 上报 `GnssCapabilities` + `GnssSystemInfo{.yearOfHw = 2020}`（支持属性覆盖）。
   - [`engine/vendor/hardware/android.hardware.health/src/health_service.cpp`](../../../vendor/hardware/android.hardware.health/src/health_service.cpp)：按充放电状态合成带微幅抖动的合理微安级电流 `currentNowMicroamps` 与平滑均值 `currentAverageMicroamps`。

## 5. 验证

1. 重编 `android.hardware.audio`、`android.hardware.drm`、`android.hardware.gnss`、`android.hardware.health` 四大模块，`cd engine/vendor/build && go test -count=1 ./...` 全绿。
2. 重打 `gold` `15-OS3.0.10.0.VNQCNXM`（`patch.img` `196c697bb633`，`vda-4ab0452c965b`），全新干净盘冷启动一次亮屏，`audioserver` 零报错加载 AIDL `primary` 与 `r_submix` 模块。
3. 双机实测 `aligner_xiaomi_gold.camera_media_drm_audio`、`id.drm_uuid`、`hardware.battery`、`aligner_xiaomi_gold.network_telephony_bt_gnss` 全部一次通过。

## 6. 遗留

- 其他机型若需对齐厂商专属音效列表，只需在其机型配置中放入对应的 `/vendor/etc/audio_effects_descriptors.xml`，`EffectFactoryImpl` 会自动加载无需改 C++ 代码。
