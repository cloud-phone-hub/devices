# a6612l：itel A300C / A6612L（`itel/itel-A6612L` / Unisoc SC9863A `sp9863a` / GRF vendor=13，system=16，kernel 5.15）

1 条机型条目：#348 记录 `itel/itel-A6612L`（`16-BP2A.250605.031.A3-20260823`，`lineage-20.0` + `kernel 5.15`）根治旧 `mapper.minigbm.so` 缺 `__libcpp_verbose_abort` 符号引发的 `SurfaceFlinger` `gralloc-mapper is missing` 无限崩溃重启循环，扩展 `02_prepare` 扫描传音 `/product/bin/{srmi_proxyd,ylog,tool_service}` 展锐守护进程屏蔽，重采对齐 `720×1612 @ 320dpi`、8×A55 `sprd-cpufreq` 12 档频点、基带 `4G_MODEM_22B_W26.24.1_P2` 与 64 位 `cdh-mediaserver`，并完成 Wi-Fi、Radio、Bluetooth、NFC 四大子系统实测验收。

## 条目

| # | 问题标识 | 说明 | 状态 |
| :--- | :--- | :--- | :--- |
| 348 | [issue-a6612l-boot-gralloc-prepare-align-and-subsystems](issue-a6612l-boot-gralloc-prepare-align-and-subsystems/solution.md) | `itel A6612L #1` 启动卡死且清单规格失真：① 旧盘集 `mapper.minigbm.so` 缺 `_ZNSt3__122__libcpp_verbose_abortEPKcz` 致 Android 13 `sphal` 加载失败，`SurfaceFlinger` 循环报 `gralloc-mapper is missing` 触发 `fwkreboot.sh` 重启框架；② 传音 A16 将 `srmi_proxyd`/`ylog`/`tool_service` 放在 `/product/bin/` 未被 `02_prepare` 扫描屏蔽，狂刷 4,429+ 条 AVC；③ 存量清单误填 `1080×2400`、`8192MB` 假值且 32 位 `mediaserver` 重定向目标缺失。**改动**：`02_prepare` 扩展支持 `Spreadtrum` 与 `product(_a).img` 展锐守护进程自动重定向并补单测；重跑 `01_unpack` → `02_prepare --refresh-profile` → `apply-profile -overwrite` 对齐真值（`720×1612 @ 320dpi`、`3GB/4GB + 64GB/128GB`、`sprd-cpufreq` `1.6GHz`、基带 `4G_MODEM_22B_W26.24.1_P2`、`hasNfc=false`、`cdh-mediaserver`）并同步 `rom-main`；全量重编 `android13` 22 个 HAL 与发布盘集（`patch.img` `9105e5314463`） | **已闭环**（2026-11-26，实例 1，`SELinux=Enforcing`）：31.4s 冷启动直达 `720×1612` 桌面，`restarting=0`、`anr=0`；Wi-Fi 5GHz `"JiHotel-9F-WiFi-E9"`（`192.168.20.100/24`）外网 `0%` 丢包；Radio 中国联通 LTE `IN_SERVICE`（`ccmni0` IMS IPv6 + `ccmni1` `100.64.155.15/24`）纯蜂窝外网 `0%` 丢包；Bluetooth `ON`（`/dev/hvc5`，0 崩溃，3 对端就绪）；NFC 与真机无硬件一致（不启用且零报错） |
