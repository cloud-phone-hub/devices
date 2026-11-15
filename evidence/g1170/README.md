# g1170：BLU G75（MT6768 / Helio G81 / Android 15）

1 条机型条目：#185 记录 BLU G75（`blu/G1170`，基线 `15-AP3A.240905.015.A2-20260209`，`lineage-22.2` + `kernel 6.6`）从官方固件 `BLU-G75-G1170.zip` 解包取证、MT6768 全栈硬件身份对齐（`720×1640 @ 320dpi` 120Hz、`Mali-G52 MC2`、大小核各 16 档频点、13 个温区、17 颗无陀螺仪传感器、双卡 Radio、Widevine CDM v19 L3、FRP 软链）、移除原厂单体 `libGLES_meow.so` 修复 `surfaceflinger` 空指针崩溃，到逆向 Coosea `framework.jar` 像素插值逻辑并调整虚拟相机 `streamConfigurations` 数组尾部顺序修复系统相机 `com.mediatek.camera` 闪退的全链路闭环。

## 条目

| # | 问题标识 | 说明 | 状态 |
| :--- | :--- | :--- | :--- |
| 185 | [issue-g1170-android15-bringup](issue-g1170-android15-bringup/solution.md) | BLU G75（`blu/G1170`）Android 15 云机全链路制作、硬件身份对齐与系统相机修复：① 提取官方 `super.img` 与 `mt6768.dtb` 硬件真值，对齐 `720×1640 @ 320dpi`（`70×156 mm`，`60/90/120Hz`）、`Mali-G52 MC2`、MT6768 16 档 `cpufreq`、13 个温区、`gcore` 触控、17 颗传感器、Widevine v19 L3 与 FRP；② 在 `cdh_g1170.rc` 停用原厂 `netdiag`，清理 `media_codecs_c2.xml` UTF-8 BOM 与杜比解码引用；③ 剔除抢占 CDH 三件套的原厂单体 `/vendor/lib64/egl/libGLES_meow.so`，解决 `MEOW::Loader::load_driver` 空指针崩 `surfaceflinger`；④ 反编译 `framework.jar` 查明 Coosea 像素插值对 `com.mediatek.camera` 原地覆写 `SCALER_AVAILABLE_STREAM_CONFIGURATIONS` 末尾 5 槽抹掉 `YUV_420_888` 输入流致 `MandatoryStreamCombination` 抛 `sizes was empty`，在 `camera_provider_service.cpp` 将多档位 `kYV12` 移至数组末尾兼容插值 | **已闭环**（2026-11-11，实例 1）：冷启动 `boot_check.py [pass]` 进入原生 Launcher3 桌面，触控滑动、关于本机、120Hz 显示、Mali-G52 MC2、双卡基带、Wi-Fi 外网、音频、17 传感器、Widevine L3、系统相机 `com.mediatek.camera` 全部验证通过且零崩溃 |
