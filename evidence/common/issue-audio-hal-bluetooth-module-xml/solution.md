# #220：Audio HAL 支持 `<configuration modules="...">` 与 `Role::Bluetooth` 对齐 `IModule/bluetooth`

## 问题

`gold_15-OS3.0.11.0.VNQCNXM` 改前 `aligner.services_full` 与实机 `service list` 对比显示音频 AIDL HAL 服务实例名与真机存在双向差异：

- 云机额外暴露了 `android.hardware.audio.core.IModule/primary`（真机不存在）；
- 云机缺少真机注册的 `android.hardware.audio.core.IModule/bluetooth`（真机为 `default`、`bluetooth`、`r_submix` 三个实例）。

## 根因

1. 自研 AIDL HAL [`android.hardware.audio`](../../../vendor/hardware/android.hardware.audio/) 原先在 [`AudioServiceMain.cpp`](../../../vendor/hardware/android.hardware.audio/src/AudioServiceMain.cpp) 固定注册 `IModule/primary` 等实例，缺少从 XML 读取启用模块列表与按蓝牙音频策略角色构造 `IModule/bluetooth` 的能力；
2. [`gold-android15.json`](../../../manifest/configs/redmi/gold/gold-android15.json) 尚未为 `gold` 声明 `modules="default,bluetooth,r_submix"` 及配套的 VINTF 服务清单替换规则。

## 证据

- 改前实机 `service list` 含 `IModule/primary` 与 `diff/services.json` 双向差异摘录见 [`log-fail.txt`](log-fail.txt)。
- 改后 `4abff687a02e421f-cloud` 实机 `service list` 与 `services_full` 重采验证摘录见 [`log-pass.txt`](log-pass.txt)。

## 改动

1. **[`android.hardware.audio`](../../../vendor/hardware/android.hardware.audio/)**：
   - [`EngineXml.h`](../../../vendor/hardware/android.hardware.audio/src/EngineXml.h) / `EngineXml.cpp`：新增 `loadConfiguredModules(path)` 解析 `/vendor/etc/audio_config.xml` 根节点 `<configuration modules="...">`；
   - [`ModuleImpl.h`](../../../vendor/hardware/android.hardware.audio/src/ModuleImpl.h) / `ModuleImpl.cpp`：扩展 `ModuleImpl::Role { Primary, RemoteSubmix, Bluetooth }`，新增 `buildBluetoothConfig()` 与 `getBluetooth` / `getBluetoothA2dp` / `getBluetoothLe` AIDL 接口实现；
   - [`AudioServiceMain.cpp`](../../../vendor/hardware/android.hardware.audio/src/AudioServiceMain.cpp)：按 `loadConfiguredModules("/vendor/etc/audio_config.xml")` 循环注册 `IModule/<instance>`（`bluetooth -> Role::Bluetooth`、`r_submix -> Role::RemoteSubmix`、其余共用 `Role::Primary`）。
2. **[`gold-android15.json`](../../../manifest/configs/redmi/gold/gold-android15.json)**：
   - 在 `etc/audio_config.xml` 声明 `<configuration version="1.0" modules="default,bluetooth,r_submix">`；
   - 配置 `vintf-drop` 并下发 `etc/vintf/manifest/manifest_android.hardware.audio.service.xml` 声明 `IModule/{default,bluetooth,r_submix}`。

## 验证

1. `cdh-vendor hal build android.hardware.audio --android 15` 编译产出新服务二进制，全量流水线 `03 -> 05 -> 07 -> 08 -> 09 -> 11` 写入载体镜像并在干净实例 `gold_15-OS3.0.11.0.VNQCNXM/2`（`4abff687a02e421f-cloud`）冷启验证。
2. 实机 `service list` 显示 `IModule/bluetooth`、`IModule/default`、`IModule/r_submix`（原 `IModule/primary` 彻底消失），`audioserver` 运行正常。
3. 19 域 `device-checker` 重采：`services` 域中 `IModule/primary` 与 `IModule/bluetooth` 两条双向能力差异 **全部清零**（`fatal 91 -> 89`，`onlyReal: 85 -> 84`，`onlyCloud: 4 -> 3`）。

## 遗留

1. `services` 域其余 84 个仅真机 binder 桩服务及 `android.hardware.media.c2.IComponentStore/default1` / `vendor.xiaomi.hardware.mlipay.IMlipayService/default` 实例命名仍在 P1 `#12` 后续桩服务规划中。
