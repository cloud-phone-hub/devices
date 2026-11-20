# devices/hardware-hidl — HIDL HAL 模块独立源码树

本目录专门存放所有基于 HIDL（`hwbinder` / `hwservicemanager`）实现的 HAL 模块源码、静态树（`vendor/`）与 SELinux 策略片段（`sepolicy/`），与纯 AIDL 模块目录 `devices/hardware/` 物理隔离。

包含模块：
- `android.hardware.graphics.composer`（HIDL `IComposer@2.4` + `drm_hwcomposer`）
- Android 13（`lineage-20.0`）原生仍使用 HIDL 的模块：
  - `android.hardware.audio`
  - `android.hardware.bluetooth`（`IBluetoothHci@1.1`）
  - `android.hardware.boot`（`IBootControl@1.2`）
  - `android.hardware.confirmationui`（`IConfirmationUI@1.0`）
  - `android.hardware.fastboot`（`IFastboot@1.1`）
  - `android.hardware.media`（`IComponentStore@1.2` 辅助实例）
  - `android.hardware.media.v412`（`IComponentStore@1.2` V4L2 硬编解码实例）
  - `android.hardware.secure_element`（`ISecureElement@1.2`）
  - `android.hardware.thermal`（`IThermal@2.0`）
  - `android.hardware.wifi`（`IWifi@1.6`）
