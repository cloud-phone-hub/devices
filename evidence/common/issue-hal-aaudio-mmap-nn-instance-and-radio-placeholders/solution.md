# `#225` 补齐 `media.aaudio` MMAP、NN 实例定制、22 个 Radio 占位服务与 `gold` 6 项通用 HAL

- 设备/机型：`common`（Audio MMAP / NN CLI 实例名 / Radio 文件驱动占位为全机型通用能力，另含 `redmi/gold` 清单配置启用 6 个通用 HAL）
- 关联报告：[`gold-aligner-20261114`](../../../../report/xiaomi/gold-aligner-20261114/services.md)
- 状态：已解决（`2026-11-14`）

## 问题

`gold_15-OS3.0.11.0.VNQCNXM` 在 `aligner.services_full` 中云机 `service list` 仅 `390` 项（真机 `471` 项，缺 `84` 项），其中以下 4 类可通过引擎通用 HAL 能力或已有组件配置直接补齐：

1. **缺少 `media.aaudio` 服务**：`audioserver` 未向 `servicemanager` 注册 `media.aaudio`（影响 App 枚举 AAudio 服务存在性）。
2. **NeuralNetworks HAL 实例名偏离真机**：engine 自研 `android.hardware.neuralnetworks` 仅硬编码注册 `IDevice/default`，未支持按机型改名为 `IDevice/mtk-neuron_shim`，若直接启用会在 `gold` 注册出真机不存在的 `IDevice/default`。
3. **缺少 22 个 Radio / MtkRadioEx 多槽与 IMS/SAP/SE 占位实例**：`android.hardware.radio` 原仅按双卡主槽注册基本实例，缺失真机存在的 `IRadioIms/slot1..2`、`IRadio*/imsSlot1..2`、`IRadio*/se1..2`、`ISap/slot1..2` 与 `IMtkRadioEx{Dc,Ims,Rcs}/slot1..2` 共 22 个服务名。
4. **`gold` 清单未启用 6 个引擎已有通用 HAL**：`boot`（`IBootControl/default`）、`memtrack`（`IMemtrack/default`）、`power`（`IPower/default`）、`soundtrigger3`（`ISoundTriggerHw/default`）、`usb`（同时注册 `IUsb/default` 与 `IUsbGadget/default`）。

## 根因

1. `android.hardware.audio` 的 Primary `ModuleImpl::getMmapPolicyInfos` 返回空策略表，`audioserver` 因此判定 HAL 不支持 AAudio MMAP 而不注册 `media.aaudio`。
2. `android.hardware.neuralnetworks` `neuralnetworks_service.cpp` 固定使用 `kNeuralNetworksServiceName`（`IDevice/default`），未支持命令行实例参数与按服务前缀匹配的全量 SELinux/VINTF 策略（同时缺少真机 `neuralnetworks_hal_service_shim_mtk` 启动路径对默认 `.rc` 的抑制）。
3. `android.hardware.radio` 仅硬编码常规槽位，未支持由机型随包配置文件按行注册附加 Binder 占位服务，且 HAL 域缺少对应 `default_android_service` / `hal_radio_service` 注册权限或机型清单未下发。
4. `gold-android15.json` `buildVendor.hardware` 此前尚未勾选引擎已实现且经过联调的 6 个通用 AIDL HAL。

## 证据

- 修复前差异日志见同目录 [`log-fail.txt`](log-fail.txt)：`services` `fatal=89`、`serviceList.count` `471 对 390`，缺少 `IBootControl/default`、`IMemtrack/default`、`IPower/default`、`ISoundTriggerHw/default`、`IUsb/default`、`IUsbGadget/default`、`media.aaudio`、`IDevice/mtk-neuron_shim` 及上述 22 个 Radio/MtkRadioEx 服务。
- 修复后运行态与 `aligner` 对比结果见同目录 [`log-pass.txt`](log-pass.txt)。

## 改动

1. **`android.hardware.audio` 支持 Primary MMAP / `media.aaudio`**：
   - [`engine/vendor/hardware/android.hardware.audio/src/ModuleImpl.cpp`](../../../vendor/hardware/android.hardware.audio/src/ModuleImpl.cpp)：`ModuleImpl::getMmapPolicyInfos()` 对 Primary 模块返回 `AUTO` 策略信息，配合 AAudio burst 参数使 `audioserver` 正常注册 `media.aaudio`（见前序 [`issue-audio-hal-bluetooth-module-xml`](../issue-audio-hal-bluetooth-module-xml/solution.md) 同系列对齐）。
2. **`android.hardware.neuralnetworks` 实例参数化与 `gold` 接入**：
   - [`src/neuralnetworks_service.cpp`](../../../vendor/hardware/android.hardware.neuralnetworks/src/neuralnetworks_service.cpp)（L133–L140）：读取 `argv[1]` 动态拼接 `IDevice/<argv[1]>`，缺省保持 `IDevice/default`；
   - [`sepolicy/vendor_service_contexts`](../../../vendor/hardware/android.hardware.neuralnetworks/sepolicy/vendor_service_contexts)：按前缀 `android.hardware.neuralnetworks.IDevice` 授权 `hal_neuralnetworks_service`；
   - [`sepolicy/vendor_sepolicy.cil`](../../../vendor/hardware/android.hardware.neuralnetworks/sepolicy/vendor_sepolicy.cil)：声明 `hal_neuralnetworks_default`、`hal_neuralnetworks_default_exec` 域转换、`binder` 与 `service_manager (add find)` 权限（避开 `neverallow`）；
   - `gold-android15.json`：`buildVendor.hardware` 启用 `android.hardware.neuralnetworks@default`，`buildVendor.files` 删除默认 `etc/init/android.hardware.neuralnetworks-service.rc`，由 `etc/init/cph_gold_vendor_svc_align.rc` 启动 `service neuralnetworks_hal_service_shim_mtk /vendor/bin/hw/android.hardware.neuralnetworks-service mtk-neuron_shim`，配合 `vintf-drop` 与 `etc/vintf/manifest/manifest_android.hardware.neuralnetworks.xml` 注册唯一实例 `IDevice/mtk-neuron_shim`。
3. **`android.hardware.radio` 文件驱动占位服务**：
   - [`src/main.cpp`](../../../vendor/hardware/android.hardware.radio/src/main.cpp)：新增 `RegisterPlaceholdersFromFile("/vendor/etc/cdh_radio_placeholders.txt")`，按行读取并持强引用注册附加占位 `AIBinder` 实例；
   - [`sepolicy/vendor_sepolicy.cil`](../../../vendor/hardware/android.hardware.radio/sepolicy/vendor_sepolicy.cil)：补全 `hal_radio_default` 对 `servicemanager`、`default_android_service` 与 `hal_radio_service` 的注册权限；
   - `gold-android15.json`：`fileTrees` 新增 `etc/cdh_radio_placeholders.txt`，列出 `IRadioIms/slot{1,2}`、`IRadioModem/{imsSlot{1,2},se{1,2}}`、`IRadioNetwork/imsSlot{1,2}`、`ISap/slot{1,2}`、`IRadioSim/{imsSlot{1,2},se{1,2}}`、`IRadioVoice/imsSlot{1,2}`、`IMtkRadioEx{Dc,Ims,Rcs}/slot{1,2}` 共 22 个服务名。
4. **`gold` 清单启用通用 HAL**：
   - `gold-android15.json` `buildVendor.hardware` 追加 `android.hardware.boot`、`android.hardware.memtrack`、`android.hardware.power`、`android.hardware.soundtrigger3`、`android.hardware.usb`（含 `IUsb` + `IUsbGadget`）、`android.hardware.neuralnetworks`。

## 验证

1. **全链路重打与无崩溃冷启**：`03 -> 05 -> 07 -> 08 -> 09 -> 11` 编出上述 HAL 产物（`out/vendor/android15/{boot,memtrack,neuralnetworks,power,soundtrigger3,usb,audio}@default` + `mtk/radio`）并启动新实例 `gold_15-OS3.0.11.0.VNQCNXM/2`，`logcat -b crash -d` 零崩溃。
2. **设备端运行态直检**（`batch3-post-runtime.txt`）：
   - `service list` 总数由 `390` 升至 `419`；
   - `IBootControl/default`、`IMemtrack/default`、`IPower/default`、`ISoundTriggerHw/default`、`IUsb/default`、`IUsbGadget/default`、`media.aaudio`、`android.hardware.neuralnetworks.IDevice/mtk-neuron_shim` 全部在列，且确认无多余 `IDevice/default`；
   - `/vendor/etc/cdh_radio_placeholders.txt` 22/22 项全部就位（`missing=0`）。
3. **`aligner` 19 域重采（`dfc3bfc1d643037a-cloud`）**：
   - `services` 域 `fatal 89 -> 59`（`-30`），`raw 527 -> 497`（`-30`）；消失 30 项为：6 个通用 HAL + `media.aaudio` + 22 个 Radio 占位服务 + 运行态不再多出的 `artd`。

## 遗留

- `android.hardware.neuralnetworks.IDevice/mtk-neuron_shim` 已注册在列，但 `aligner` 在 `dfc3bfc1d643037a-cloud` 上对该实例读取的接口描述符列表为 `[]`（真机为 `['android.hardware.neuralnetworks.IDevice']`），仍占 `services` 1 项能力差异；
- `services` 域余 `54` 个仅真机有的深度厂商/定制 HAL（如 `ILbs/*` 24 个、`IMtkGnss`、`IGatekeeper`、`IMiKeyBagService`/`IMrm*`、PQ/Camera ISP/Dolby、`IBluetoothAudioProviderFactory` 等）与 `2` 个仅云机实例（`IComponentStore/default1`、`IMlipayService/default`），后续可继续沿 `cdh_radio_placeholders` 或专项桩思路收敛。
