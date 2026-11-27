# p616f05：ZTE Blade V70 / Z2459（`nubia/P616F05` / Unisoc UMS9230T `ums9230` / GRF vendor=13，system=14，kernel 5.15）

1 条机型条目：#352 记录 `nubia/P616F05`（`14-TP1A.220624.014-20260324`，`lineage-20.0` + `kernel 5.15`）重对齐 `720×1600 @ 320dpi`、`8GB+256GB`、25 档 `UMS9230T` `cpufreq` 与 `5000mAh` 电池，补齐真机 NFC HAL，纠偏 `camera@unisoc` + `radio@unisoc` + 64 位 `cdh-mediaserver`，屏蔽 `vendorcfgdeamon`/`tool_service`/`sprd_networkcontrol` 消除循环重启与 AVC 风暴，并完成蓝牙、Wi-Fi、纯蜂窝移动数据、NFC、原生相机前后摄拍照录像及 `V4L2Decoder` 硬解全维度实测验收。

## 条目

| # | 问题标识 | 说明 | 状态 |
| :--- | :--- | :--- | :--- |
| 352 | [issue-p616f05-prepare-720p-nfc-dreamcamera2-and-full-audit](issue-p616f05-prepare-720p-nfc-dreamcamera2-and-full-audit/solution.md) | `ZTE Blade V70 #1` 存量镜像误配 `1080×2400`、`camera@nubia`、漏配真机 `android.hardware.nfc`、缺 64 位 `cdh-mediaserver`、Radio 脱网、ZRAM 耗尽触发 `lmkd`、`vendorcfgd` 循环重启且 `tool_service` 刷 423 条 AVC。**改动**：对齐 `720×1600 @ 320dpi`、`4GB+128GB/8GB+256GB`、`5000mAh` 电池与 25 档频点；补齐 `nfc` HAL 并切 `camera@unisoc`/`radio@unisoc` + `cdh-mediaserver` + `c2inputsurface=-1`（保持 HIDL C2 默认选型）；重定向屏蔽 `vendorcfgdeamon`/`tool_service`/`sprd_networkcontrol` 并开机幂等禁用 `com.android.aftersaleservice`；刷编 `android13` 9 个 HAL 与 `5.15` 模块并重建盘集（`patch.img` `de7ac8faae0b`） | **已闭环**（2026-11-27，实例 1，`8GB+256GB`，`SELinux=Enforcing`）：`42.8s` 启动直达桌面，`restarting=0`、AVC 清零、静置 `746% idle`（修前 `424%`）、`6GiB` ZRAM `lmkd=0`；BT `ON` 0 崩溃、5GHz Wi-Fi 与纯蜂窝 `ccmni1` `ping 223.5.5.5` 均 `0%` 丢包、NFC 亮标且贴卡激活；`DreamCamera2` 前后摄拍照 + `1080p@30fps` `.mp4` 录像 + `V4L2Decoder` 硬解回放 + `CDCM_PAUSE` 休眠全通，桌面滑动 `P50=16ms, P99=29ms` |
