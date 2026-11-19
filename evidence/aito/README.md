# aito：Motorola Razr 2024（`motorola/aito` / `mt6878` / GRF vendor=14, system=16）

1 条机型条目：#271 记录 Android 16 版本（`14-W1UCS36H.96-35-8-9-20260801`，`lineage-23.2` + `kernels/6.1`）在纯只读排查中修正 `platform=mtk`、`mapper/minigbm*`、引入官方 `etc/audio_param` 修复 `audioserver` `stoi` 崩溃诱因，并确证 `SurfaceFlinger::initializeDisplays()+2460` 对 `ComposerExt` 缺空指针保护且 `ClientImplAidl::Init` 要求真实 `registerClient` AIDL OK 响应（需后续源码桩/重定向库支持）。

## 条目

| # | 问题标识 | 说明 | 状态 |
| :--- | :--- | :--- | :--- |
| 271 | [issue-aito-a16-composer-ext-null-deref-and-audioparam](issue-aito-a16-composer-ext-null-deref-and-audioparam/solution.md) | 修正 `aito.json` `"mtk"`、补 `mapper/minigbm*` 与 `etc/audio_param`；反汇编确证 `/system/bin/surfaceflinger` 在 `initializeDisplays()+2460` (`0x2c07fc`) 裸解引用 `[x19, #0x2168]` (`ComposerExt::ClientInterface*`) 致空指针 `SIGSEGV`，且 `/system_ext/lib64/libcomposer_ext.so` `ClientImplAidl::Init` 要求 `registerClient` 返回 `AStatus_isOk()`，纯 manifest 配置无法绕过 | **已定位根因（待源码桩支持）**（2026-11-19）：已完成配置收口与 `log-fail.txt` 固证，未改任何仓库源码；待统一补实现 `IComposerExt` 桩后复测 |
