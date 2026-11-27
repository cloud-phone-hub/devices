# aito：Motorola Razr 2024（`motorola/aito` / `mt6878` / GRF vendor=14, system=16）

2 条机型条目：#271 记录 Android 16 版本早期排查修正 `platform=mtk`、`mapper/minigbm*` 与 `etc/audio_param`；#350 记录 `prepare` 真机规格全链对齐、补 `ro.vendor.hw.dualsim=true`（`exported_config_prop`）激活双卡 RRO 根治 `com.android.phone` 越界死循环崩溃、关闭 GRF LE Audio Client 根治蓝牙 `SIGABRT`、自愈 `media_config.xml` 打通 `c2.v4l2.avc.{encoder,decoder}` 硬件编解码并完成六大硬件子系统闭环验收。

## 条目

| # | 问题标识 | 说明 | 状态 |
| :--- | :--- | :--- | :--- |
| 271 | [issue-aito-a16-composer-ext-null-deref-and-audioparam](issue-aito-a16-composer-ext-null-deref-and-audioparam/solution.md) | 修正 `aito.json` `"mtk"`、补 `mapper/minigbm*` 与 `etc/audio_param`；反汇编确证 `/system/bin/surfaceflinger` 在 `initializeDisplays()+2460` (`0x2c07fc`) 裸解引用 `[x19, #0x2168]` (`ComposerExt::ClientInterface*`) 致空指针 `SIGSEGV`，且 `/system_ext/lib64/libcomposer_ext.so` `ClientImplAidl::Init` 要求 `registerClient` 返回 `AStatus_isOk()`，纯 manifest 配置无法绕过 | **已定位根因（待源码桩支持）**（2026-11-19）：已完成配置收口与 `log-fail.txt` 固证，未改任何仓库源码；待统一补实现 `IComposerExt` 桩后复测 |
| 350 | [issue-aito-dualsim-rro-bt-leaudio-v4l2-and-full-audit](issue-aito-dualsim-rro-bt-leaudio-v4l2-and-full-audit/solution.md) | `prepare` 对齐 `Motorola Razr 2024`（`12GB` RAM / `6GiB` ZRAM / `4200mAh` 电池 / `/proc/bootprof`）；补 `ro.vendor.hw.dualsim=true` + `exported_config_prop` 激活双卡 RRO 消除 `UiccController` `length=1; index=1` 循环崩溃（静置从 `90% idle` 恢复至 `793% idle`）；`cph-zram-setup` 关闭无 Offload 的 `bluetooth.profile.bap.*` 消除蓝牙 `SIGABRT`；补 `media.c2.hal.selection=aidl` 与 `media_config.xml` `rank=512/1024` 打通 `c2.v4l2` 编解码 | **已闭环**（2026-11-27，实例 1）：`40.7s` `Enforcing`、`restarting=0`、`793% idle`；BT `ON` 0 崩溃、Wi-Fi 与双 PDN 纯蜂窝 `0% loss`、NFC NDEF 发现、`MotoCamera` 前后摄拍照 + 1080p `.mp4` `c2.v4l2.avc.{encoder,decoder}` 编解码 + `CDCM_PAUSE` 休眠全通，滑动 `P99=17ms` |
