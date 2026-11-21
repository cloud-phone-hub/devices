# #293 ASUSAI2501 全屏 IMEI 防泄密水印浮层（`ImsWatermark`）遮盖所有应用界面

- **编号**：#293
- **设备/机型**：`asus/ASUSAI2501`（`15-AQ3A.240829.003-20250122`，`ZWWAI2501` / `lineage-22.2` + `kernel 6.6`）
- **日期**：2026-11-21

---

## 一、问题

`asus/ASUSAI2501` 开机后，在 `Settings` 及所有应用界面最上方始终叠加一层半透明竖排大号数字水印（显示当前云机双卡 IMEI：`358471234750645` / `358471234750652`），在深色界面下尤为刺眼并遮挡所有应用内容。

---

## 二、根因

1. `dumpsys window windows` 查明该浮层为全屏系统安全浮层窗口 `Window{... ImsWatermark}`（`2400x2400`，`ty=SECURE_SYSTEM_OVERLAY`），归属系统特权包 `com.asus.ims.watermark`（`/system/priv-app/ImsWatermarkService/ImsWatermarkService.apk`）。
2. 该官方固件包本身为华硕内测版（Beta）ROM，系统分区内置 `ro.vendor.asus.beta=1`；开机完成后 `MiscSetupService` 读取到 `ro.vendor.asus.beta=1`（见 `logcat`：`Watermark: MSG_BOOT_COMPLETE, ro.vendor.asus.beta= 1`），拉起 `ImsWatermarkService` 调用 `showIMEIView()` 在全屏铺上双卡 IMEI 防泄密水印。
3. 拆查 `ImsWatermarkService.apk` 的 `classes.dex` 确认其仅含 `ImsWatermarkService` / `WatermarkHandler` / `showIMEIView` / `removeIMEIView` 逻辑，不承载任何其他系统功能；按 `AGENTS.md` 第 8 条（禁止修改官方 `system.img`），应通过 `cdh_redirect` 目录屏蔽该特权应用。
4. 同期核查另一台华硕机型 `asus/ASUS_AI2205` 无此属性与水印；并实测验证 `ASUSAI2501` 官方 `payload.bin` 中 `xrom.img` / `asusfw.img` 均仅含版本文本（6 B / 21 B），无任何漏装应用，333 个原厂系统包中除屏蔽的 `com.asus.ims.watermark` 外其余 332 个全部正常安装。

---

## 三、证据

- **修前取证日志**：[log-fail.txt](log-fail.txt)（记录 `Window{640d0bc u0 ImsWatermark}`、`ro.vendor.asus.beta=1`、`MiscSetupService` 拉起链路与 `ImsWatermarkService.apk` `classes.dex` 关键字符串）。
- **修后验证日志**：[log-pass.txt](log-pass.txt)（冷启动 30s `Enforcing`、`pm list packages | grep watermark` 为空、`dumpsys window windows | grep ImsWatermark` 为空、总包数 332）。
- **修后审图截图**：
  - 深色 `Settings` 界面（与用户反馈同界面对比）：[settings-dark-pass.png](settings-dark-pass.png) — 纯黑背景干干净净，无任何水印浮层。
  - 桌面界面：[home-pass.png](home-pass.png) — 清晰纯净。

---

## 四、改动

在 [devices/manifest/configs/asus/ASUSAI2501/ASUSAI2501-android15/module/config.json](../../../../manifest/configs/asus/ASUSAI2501/ASUSAI2501-android15/module/config.json) 的 `redirect` 数组中新增 1 条目录屏蔽规则（规则总数 13 ≤ 32）：

```json
{
  "from": "/system/priv-app/ImsWatermarkService",
  "to": "/mnt/vendorpatch/system/priv-app/ImsWatermarkService",
  "type": "dir"
}
```

重跑 `ASUSAI2501` 全链（`01~12` + `13_pack_vda`）生成新盘集并原子替换至 `out/devices/dist/asus/ASUSAI2501/15-AQ3A.240829.003-20250122/`（`11_build_patch` 输出 `屏蔽落点校验通过（2 项均不存在）`）。

---

## 五、验证

在隔离测试 `host-daemon`（`127.0.0.1:19056`）上使用新版 `out/devices/dist/asus/ASUSAI2501/15-AQ3A.240829.003-20250122/` 冷启动：

1. **启动与安全状态**：30s 达成 `sys.boot_completed=1`，`getenforce=Enforcing`。
2. **包与窗口核查**：`pm list packages | grep -i watermark` 输出为空（系统总包数 `333 -> 332`）；`dumpsys window windows | grep -i ImsWatermark` 输出为空；`MiscSetupService` 无崩溃。
3. **人工审图**：切换深色模式并打开 `Settings` 经 WebSocket VNC 抓图（[settings-dark-pass.png](settings-dark-pass.png)）及桌面抓图（[home-pass.png](home-pass.png)），全屏 IMEI 水印浮层 100% 消失。

---

## 六、遗留

无。若用户本地原有旧实例正在使用修复前的磁盘快照，需停止旧实例并重新创建/重启实例以加载新版 `vda.img`。
