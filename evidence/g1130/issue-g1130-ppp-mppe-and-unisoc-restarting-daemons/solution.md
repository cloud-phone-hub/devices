# #297 BLU G65L（`blu/G1130`）系统内部错误弹窗（`CONFIG_PPP_MPPE=m`）与展锐循环重启服务/AVC 收口

- **编号**：#297
- **设备/机型**：`blu/G1130`（BLU G65L，`14-UP1A.231005.007-20260511`，Unisoc UMS9230T，`lineage-20.0` + `kernel 5.15`）
- **日期**：2026-11-21

---

## 一、问题

`blu/G1130` 进入桌面后弹出系统告警框 **"Android System — There's an internal problem with your device. Contact your manufacturer for details."**；同时排查 `qemu.log`（96,147 行）与运行态进程发现：

1. 内核仍为旧版 5.15（`CONFIG_PPP_MPPE=m`），不满足 Android 14 `/system/etc/vintf/compatibility_matrix.7.xml` 的 `CONFIG_PPP_MPPE=y` 要求；
2. 两个展锐系统守护进程 `/system_ext/bin/linkturbonative` 与 `/system_ext/bin/srmi_proxyd` 每 5 秒崩溃重启一次（各退出 511 次，状态持续 `restarting`），伴随 `ISrmiManager/default` 找不到刷屏 2,531 条；
3. 展锐抓日志进程 `/system_ext/bin/ylog`（9,822 次）与 `/system_ext/bin/slogmodem`（773 次）合计刷出上万条 SELinux AVC；
4. `hardware/config.json` 漏配 `android.hardware.neuralnetworks@1.0`，致使 `vendor/etc/cdh_nn_placeholders.txt` 中 13 项展锐占位服务全部未注册。

---

## 二、根因

1. **弹窗根因**：与 `#295`（[issue-kernel-ppp-mppe-builtin-and-ai2205-selinux](../../common/issue-kernel-ppp-mppe-builtin-and-ai2205-selinux/solution.md)）同源，`blu/G1130` 尚未基于 `CONFIG_PPP_MPPE=y` 新版 5.15 内核重打包。
2. **后台无限重启与刷屏**：`linkturbonative`、`srmi_proxyd`、`ylog`、`slogmodem` 均为展锐平台私有后台诊断/调优进程，位于官方只读 `/system_ext/bin/`，在云机环境下无对应实体依赖与 SELinux 放行，应按 `AGENTS.md` §8 通过 `cdh_redirect` 屏蔽；同时补齐 `neuralnetworks@1.0` 承载既有占位清单。

---

## 三、证据

- **修前取证日志**：[log-fail.txt](log-fail.txt)
- **修后验证日志**：[log-pass.txt](log-pass.txt)
- **修后审图截图**：[home-pass.png](home-pass.png)（桌面干净清爽，**"There's an internal problem with your device." 弹窗已 100% 彻底消失**）

---

## 四、改动

1. [devices/manifest/configs/blu/G1130/G1130-android13/hardware/config.json](../../../../manifest/configs/blu/G1130/G1130-android13/hardware/config.json)：追加 `android.hardware.neuralnetworks@1.0`。
2. [devices/manifest/configs/blu/G1130/G1130-android13/module/config.json](../../../../manifest/configs/blu/G1130/G1130-android13/module/config.json)：在 `redirect` 数组追加 4 条文件屏蔽规则（规则总数 `18 <= 32`）：
   - `/system_ext/bin/linkturbonative`
   - `/system_ext/bin/srmi_proxyd`
   - `/system_ext/bin/ylog`
   - `/system_ext/bin/slogmodem`
3. 使用 `#295` 编好的 `CONFIG_PPP_MPPE=y` 5.15 内核全链重打包 `blu/G1130`（`01~13`），并原子替换至 `out/devices/dist/blu/G1130/14-UP1A.231005.007-20260511/`。

---

## 五、验证

在隔离 `host-daemon` 上使用新版盘集冷启动验收（详见 [log-pass.txt](log-pass.txt) 与 [home-pass.png](home-pass.png)）：

| 检查项 | 修前 | 修后 |
| :--- | :--- | :--- |
| SELinux / 内核 `PPP_MPPE` | Enforcing / `=m` | Enforcing / `CONFIG_PPP_MPPE=y`、`CONFIG_CRYPTO_LIB_ARC4=y` |
| `VintfObject` 与系统弹窗 | 弹出系统内部错误窗口 | **`(NONE)` 0 错误，`Window{Android System}` 与弹窗 100% 消失** |
| `init.svc.*restarting` | `linkturbonative`、`srmi_proxyd`（各退出 511 次） | **`(none)` 0 重启服务** |
| 展锐占位服务（`neuralnetworks`） | 13 项全部缺失 | `neuralnetworks` + 13 项展锐占位服务全部在线 |
| `qemu.log` 总行数 / AVC 总数 | `96,147` 行 / `15,614` 条 | `11,635` 行（降 88%）/ `2,368` 条（降 85%） |
| 非 Chrome 应用崩溃 | `com.android.phone` ANR | `(NONE)` |

---

## 六、遗留

1. 仓内其余 Android 14 + 5.15 内核机型（`hmd/ARW`、`nubia/P616F05`、`symphony/G27`、`symphony/MAX_10`）及高版本 5.15 GRF 机型可按需用新 5.15 内核重打包同步获得 `CONFIG_PPP_MPPE=y`。
2. 用户控制台若仍有旧 `BLU G65L #1` 运行中实例，需停止并重建/重启实例以加载新版盘集。
