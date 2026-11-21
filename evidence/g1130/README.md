# g1130：BLU G65L（`blu/G1130` / Unisoc UMS9230T / GRF vendor=13，system=14，kernel 5.15）

1 条机型条目：#297 记录 `blu/G1130`（`14-UP1A.231005.007-20260511`，`lineage-20.0` + `kernel 5.15`）接入 `#295` 的 `CONFIG_PPP_MPPE=y` 5.15 新内核彻底消除「There's an internal problem with your device.」系统弹窗，并屏蔽循环重启的展锐后台进程（`linkturbonative`、`srmi_proxyd`、`ylog`、`slogmodem`）及补齐 `android.hardware.neuralnetworks@1.0` 占位宿主。

## 条目

| # | 问题标识 | 说明 | 状态 |
| :--- | :--- | :--- | :--- |
| 297 | [issue-g1130-ppp-mppe-and-unisoc-restarting-daemons](issue-g1130-ppp-mppe-and-unisoc-restarting-daemons/solution.md) | 运行旧 5.15 内核（`CONFIG_PPP_MPPE=m`）触发 Android 14 VINTF 系统内部错误弹窗；同时 `/system_ext/bin/{linkturbonative,srmi_proxyd}` 无限重启（各退出 511 次）、`ylog`/`slogmodem` 刷 9,800+ 次 AVC、漏配 `neuralnetworks@1.0` 占位宿主。**改动**：`module/config.json` 屏蔽上述 4 个展锐后台二进制，`hardware/config.json` 补入 `neuralnetworks@1.0`，基于 `#295` 新 5.15 内核全链重打包并原子替换盘集 | **已闭环**（2026-11-21）：冷启动 `Enforcing`、`CONFIG_PPP_MPPE=y`、`VintfObject` 0 错误、`restarting=none`、13 项展锐占位服务全在线、`qemu.log` 降 88%（AVC 降 85%），桌面审图系统内部错误弹窗 100% 消失 |
