# #296 Blackview XPLORE_X1 `com.android.phone` 闪退、`Not Write Google Key / Not Write RKP` 红字水印及缺失 DRM/NN HAL 修复

- **编号**：#296
- **设备/机型**：`blackview/XPLORE_X1`（`15-AP3A.240905.015.A2-20251126`，Doke OS 5 / MT6877 / `lineage-22.2` + `kernel 6.6`）
- **日期**：2026-11-21

---

## 一、问题

`blackview/XPLORE_X1` 冷启动进入 Doke OS 5 后出现两类直观异常及两项后台隐患：

1. **应用闪退弹窗**：`com.android.phone has stopped` 反复弹出，堆栈指向 `com.wtk.factory.AtCommandService$1.handleMessage(AtCommandService.java:268)` 的 `NullPointerException`。
2. **右上角红字水印**：状态栏右上角常驻两行红色重叠文字 **`Not Write Google Key`** 与 **`Not Write RKP`**。
3. **后台 HAL 缺失与日志暴涨**：`hardware/config.json` 漏配 `android.hardware.neuralnetworks@1.0`（导致 `cdh_nn_placeholders.txt` 中 51 个 MTK/Blackview 占位服务未注册）与 `android.hardware.drm@1.0`（导致 `rkpdapp` 疯狂重试原厂 `com.google.android.widevine.nonupdatable.apex` 的 `IDrmFactory/widevine`，刷出 71,496 条失败日志，把 `qemu.log` 写到 924,672 行）。

---

## 二、根因

1. **`com.android.phone` 闪退**：`/system/app/OdmFactory/OdmFactory.apk`（`com.wtk.factory`，闻泰 ODM 产线工厂测试包）通过 `BootReceiver` 监听 `BOOT_COMPLETED` 拉起运行在 `com.android.phone` 进程中的 `AtCommandService`。该服务向基带下发产线私有 AT 命令 `EVENT_GET_SUPPORT_BRAND`，在模拟 `radio@mtk` 下收到 `null` 的 `AsyncResult`，在 `AtCommandService.java:268` 裸读 `ar.exception` 抛出空指针直接带崩电话进程。
2. **`Not Write Google Key` / `Not Write RKP` 红字水印**：反汇编 `/system/framework/services.jar`（`classes3.dex`）的 `WindowManagerService.createWatermark()` 确认，当 `/system/build.prop` 中 `ro.mtk_tee_check_support=1`、`ro.mtk_trustkernel_tee_support=1`、`ro.mtk_trustkernel_rkp_support=1` 时，系统通过 `SystemProperties.get(..., "false")` 读取 `vendor.trustkernel.keybox.deployed` 与 `vendor.trustkernel.rkp.uploaded`；任一为 `"false"` 即构造 `Watermark("trustkernel")` 绘制红字。
   - **关键 SELinux 约束（首轮复测踩坑实证）**：仅把两个属性写入 `vendor/build.prop` 并给 `domain` 开读权限仍不会生效——Android `init` 加载 `/vendor/build.prop` 时校验 `vendor_init` 的 `property_service (set)` 权限，缺少 `(allow vendor_init_{{vers}} vendor_trustkernel_prop (property_service (set)))` 会导致这两个属性在开机初始化时被静默拒写。
3. **Widevine / NN HAL 缺失**：原配置未声明 `android.hardware.neuralnetworks@1.0` 与 `android.hardware.drm@1.0`（配 `vintf-drop: ["android.hardware.drm-service.widevine.xml"]` 及 `hal_drm_widevine` SELinux 域，与 `#254` 同理）。

---

## 三、证据

- **修前取证日志**：[log-fail.txt](log-fail.txt)（含 `am_crash` / `AtCommandService` 堆栈、`WindowManagerService.createWatermark` 逻辑、71,496 条 Widevine 失败统计）。
- **修后验证日志**：[log-pass.txt](log-pass.txt)（冷启动 45s `Enforcing`、`vendor.trustkernel.keybox.deployed=true`、`vendor.trustkernel.rkp.uploaded=true`、`OdmFactory` 屏蔽、`IDrmFactory/widevine` 与 `neuralnetworks` 占位服务在线、`widevine_spam=0`、`am_crash=0`、多款内置应用拉起零崩溃）。
- **修后审图截图**：[about-phone-pass.png](about-phone-pass.png)（与用户反馈同屏的 Doke OS 5 `About Phone` 界面：右上角红字水印 100% 消失，无任何 `com.android.phone has stopped` 弹窗）。

---

## 四、改动

遵循 `AGENTS.md` §8（不触碰官方 `system.img`/`super.img`），全部改动落在 `devices/manifest/configs/blackview/XPLORE_X1/XPLORE_X1-android15/`：

1. [module/config.json](../../../../manifest/configs/blackview/XPLORE_X1/XPLORE_X1-android15/module/config.json)：在 `redirect` 数组追加 1 条目录屏蔽规则（总数 `21 <= 32`）：
   ```json
   {
     "from": "/system/app/OdmFactory",
     "to": "/mnt/vendorpatch/system/app/OdmFactory",
     "type": "dir"
   }
   ```
2. [vendor/config.json](../../../../manifest/configs/blackview/XPLORE_X1/XPLORE_X1-android15/vendor/config.json)：
   - `build-prop-sets` 追加 `"vendor.trustkernel.keybox.deployed": "true"` 与 `"vendor.trustkernel.rkp.uploaded": "true"`；
   - 追加 `"vintf-drop": ["android.hardware.drm-service.widevine.xml"]`。
3. [hardware/config.json](../../../../manifest/configs/blackview/XPLORE_X1/XPLORE_X1-android15/hardware/config.json)：追加 `android.hardware.drm@1.0` 与 `android.hardware.neuralnetworks@1.0`。
4. 新建 [vendor/sepolicy/vendor_property_contexts](../../../../manifest/configs/blackview/XPLORE_X1/XPLORE_X1-android15/vendor/sepolicy/vendor_property_contexts) 与 [vendor/sepolicy/vendor_sepolicy.cil](../../../../manifest/configs/blackview/XPLORE_X1/XPLORE_X1-android15/vendor/sepolicy/vendor_sepolicy.cil)：
   - 声明 `vendor_trustkernel_prop`（`vendor_public_property_type`），授予 `vendor_init_{{vers}}` `property_service (set)` 与 `domain` `file (read getattr map open)`；
   - 补齐 `hal_drm_widevine` 域与服务注册规则及 `system_server_{{vers}} -> mtk_hal_gpuserv_service (find)`。

完成 `XPLORE_X1` 全链重打包（`vda.img` 14.64 GiB、`patch.img` 384 MiB `sha256 5b8cb697519d`）并原子替换至 `out/devices/dist/blackview/XPLORE_X1/15-AP3A.240905.015.A2-20251126/`。

---

## 五、验证

在隔离 `host-daemon` 上使用新版盘集冷启动验收（详见 [log-pass.txt](log-pass.txt) 与 [about-phone-pass.png](about-phone-pass.png)）：

| 检查项 | 修前 | 修后 |
| :--- | :--- | :--- |
| 启动 / SELinux | Enforcing | `45s` `sys.boot_completed=1` / `Enforcing` |
| `OdmFactory` / `com.android.phone` | `AtCommandService` NPE 崩溃弹窗 | 包已彻底屏蔽，`am_crash=0`、`crash_buf=(NONE)` |
| `vendor.trustkernel.*` 属性 | 未生效（默认 `"false"`） | `keybox.deployed=true`、`rkp.uploaded=true`，0 SELinux 拒绝 |
| 右上角红字水印 | `Not Write Google Key` / `Not Write RKP` | 审图 [about-phone-pass.png](about-phone-pass.png) **100% 消失** |
| DRM / NN 服务 | 均缺失，`widevine` 刷屏 71,496 条 | `IDrmFactory/{clearkey,widevine}`、`neuralnetworks`、`IHelper`、`IMtkPowerService` 全部在线，`widevine_spam=0` |
| `qemu.log` 总行数 | `924,672` 行 | `9,465` 行 |
| 预装应用拉起体检 | — | 计算器、时钟、系统管家、浏览器、相机 5 款全部正常进前台，0 闪退 |

---

## 六、遗留

无。用户控制台若仍有旧 `XPLORE_X1` 运行中实例，需停止并重建/重启实例以加载新版盘集。
