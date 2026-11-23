# #307 高通 `IQtiOemHook` 主线程死锁根治、`vivo/PD2454` NFC 补装及 Treble `vendor_sdk` 永久化防线

## 问题

承接 [#306](../issue-fleet-wifi-radio-nfc-camera-deep-audit-and-fixes/solution.md) 遗留的 3 项跨机型 P0 阻塞：

1. **高通 `PAKALA`/`KAANAPALI`/`MILOS` 平台 Radio `POWER_OFF`**：`meizu/Meizu22`、`asus/ASUSAI2501`、`redmagic/NX809J`、`nothing/Asteroids`、`xiaomi/popsicle` 开机后 `android.hardware.radio-service` 正常运行且 `dumpsys isub` 已读出联通 ICCID，但随即进入 `mVoiceRegState=3(POWER_OFF)`、`gsm.sim.state=,ABSENT`，`dumpsys activity service com.android.phone/.TelephonyDebugService` 超时（主线程卡死），日志伴随 `AnswerToReset: Unexpected bytes after the check byte`；
2. **`vivo/PD2454` 缺失 `android.hardware.nfc-service`**：系统声明 `android.hardware.nfc` 且 `com.android.nfc` 每秒轮询 `android.hardware.nfc.INfc/default`，但清单未勾选 `android.hardware.nfc`、`patch.img` 未打入 NFC HAL，导致 `INIT_FAIL`；
3. **Treble 跨代 `vendor_sdk` 缺乏全仓永久化防护**：增量刷新脚本 `patch_hal_refresh.py` 优先按系统目录名（如 `16-`）猜测 HAL 代际，且 `09_build_vendor` 缺少基于 `ro.vendor.build.version.sdk`/`ro.vndk.version` 的 ABI 硬闸门，跨代机型（全仓 28/45 盘集）存在误混高代 HAL 引发 `__libcpp_verbose_abort` 动态链接崩溃的风险。

## 根因

1. **`IQtiOemHook` 空桩未回调响应致 `com.android.phone` 主线程永久死锁**：
   - 在 `meizu/Meizu22` 上主动触发 `com.android.phone/.SimContacts` ANR 获取主线程 Java 堆栈，精准定位到主线程永久卡在 `QtiPhoneSwitcher.queryMaxDataAllowed() -> QtiRilInterface.getMaxDataAllowed() -> QcRilHook.sendQcRilHookMsg() -> IQcrilMsgTunnel.sendOemRilRequestRaw()`；
   - 反编译 `qcrilmsgtunnel.apk`（`QtiOemHookAidl.java` / `IQtiOemHook.java` / `IQtiOemHookResponse.java`）证实：`QcrilMsgTunnel` 连接 VINTF 声明的 `vendor.qti.hardware.radio.qcrilhook.IQtiOemHook/oemhook{0,1}`，先调 `setCallback(tx=1)` 注册 `IQtiOemHookResponse`，随后以 `oneway` 发送 `oemHookRawRequest(tx=2, serial, data)` 并同步阻塞等待 `IQtiOemHookResponse.oemHookRawResponse(tx=1, serial, error, byte[])`；
   - 原 `android.hardware.radio-service` 对 `IQtiOemHook` 仅挂了 `OnPlaceholderTransact`（回 `STATUS_UNKNOWN_TRANSACTION` 且不回调），使 `sendOemRilRequestRaw()` 永远等不到唤醒，冻结 `com.android.phone` 主线程 Looper；
   - 此外 `radio_sim.cpp` / `radio_config.cpp` 写入的模拟 ATR 串 `3B9F96801FC7AFC08031E073FE211B66D000000000000000` 为 24 字节，比 ISO 7816-3（6 字节头 + 15 字节历史 + 1 字节 TCK = 22 字节）多出 2 字节且未计算 XOR 校验字节。
2. **`02_prepare` 系统分区权限补采遗漏 `android.hardware.nfc`**：`vendor_etc_configs.go` 的 `enrichPermissionsFromSystemPartitions()` 仅补充扫描 `system`/`product`/`system_ext` 中的 `android.hardware.consumerir.xml`，未覆盖只在系统侧分区声明 `android.hardware.nfc*.xml` 的机型（`vivo/PD2454`）；全仓扫描确认 `xiaomi/popsicle` 因含 ST NFC 扩展被 `assemble.go` 正常裁除 NFC 特性属 `N/A`，全仓唯一漏装机型为 `vivo/PD2454`。
3. **脚本与流水线缺少 `ro.vendor.build.version.sdk` 强校验**：`patch_hal_refresh.py` 优先正则匹配目录名 `^(\d+)-`，且 `is_mtk_radio_device` 曾依赖废弃目录与通用 XML；`09_build_vendor/execute.go` 未在解包基底/官方 `vendor/build.prop` 后校验 `plan.Android` 与真实 `vendor_sdk` 一致。

## 证据

- 故障主线程死锁栈与异常日志：[log-fail.txt](log-fail.txt)
- 5 款高通机型冷启 Radio 验收、`vivo/PD2454` NFC 刷卡闭环与全仓 45/45 盘集校验日志：[log-pass.txt](log-pass.txt)

## 改动

1. **实现 `IQtiOemHook` 真实回调占位服务端并修正 ISO 7816-3 USIM ATR**：
   - 修改 [main.cpp](../../../devices/hardware/android.hardware.radio/src/main.cpp)：新增 `QtiOemHookPlaceholderState` / `OnQtiOemHookTransact`，在 `tx=1 (setCallback)` 保存 `IQtiOemHookResponse` 远端代理（按 `oemhook0`/`oemhook1` 实例隔离且保留标准 InterfaceToken 校验），在 `tx=2 (oemHookRawRequest)` 读出 `serial` 并立即以 `FLAG_ONEWAY` 回调 `oemHookRawResponse(serial, 6 /* REQUEST_NOT_SUPPORTED */, byte[0])`；
   - 修改 [radio_sim.cpp](../../../devices/hardware/android.hardware.radio/src/radio_sim.cpp#L86) 与 [radio_config.cpp](../../../devices/hardware/android.hardware.radio/src/radio_config.cpp#L43)：将 ATR 修正为合规 22 字节 USIM ATR `3B9F96801FC78031E073FE211B66D0017A0801000073`；
   - 重编全部 5 代（`android13..17`）`android.hardware.radio-service` 并安全同步至全仓 45/45 `patch.img`（MTK 盘集保持 `mtk` 变体）。
2. **补全 `02_prepare` 系统分区 NFC 自动探测并补装 `vivo/PD2454` NFC HAL**：
   - 修改 [vendor_etc_configs.go](../../../agent/modules/pipeline/stages/02_prepare/vendor_etc_configs.go#L625-L750)：扩展 `enrichPermissionsFromSystemPartitions()` 同时探测 `system`/`product`/`system_ext` 中的 `android.hardware.nfc*.xml` 与 `nfc_features.xml`；
   - 修改 [config.json](../../../devices/manifest/configs/vivo/PD2454/PD2454-android15/hardware/config.json)：补加 `android.hardware.nfc@1.0`；
   - 向 `vivo/PD2454` `patch.img` 补装 `android15` 的 `android.hardware.nfc-service`、`.rc`、VINTF `.xml`（严格设置 `uid/gid/mode/security.selinux`）并在 `vendor_sepolicy.cil` 补齐 `hal_nfc_default -> console_device_202404` 授权，通过 `ea_get -V` 与 `e2fsck -fn` 校验。
3. **永久化 Treble `vendor_sdk` 选型与校验防线**：
   - 修改 [execute.go](../../../agent/modules/pipeline/stages/09_build_vendor/execute.go#L68-L78) 与 [execute_test.go](../../../agent/modules/pipeline/stages/09_build_vendor/execute_test.go)：在 `09_build_vendor` 步骤 1c 新增 `validateVendorABIGuard()` + `detectVendorSDKAndroidMajor()`，强制比对 `plan.Android` 与基底/官方 `vendor/build.prop` 的 `ro.vendor.build.version.sdk` / `ro.vndk.version`，跨代错配直接拦截；
   - 重构 [patch_hal_refresh.py](../../../out/devices/tmp/patch_hal_refresh.py)：强制优先读 `/vendor/build.prop` 的 `ro.vendor.build.version.sdk` / `ro.vndk.version` 映射 `android13..17`（拒绝按系统目录名猜测），精确按三表 `hardware/config.json` 与 `ro.board.platform` 识别 MTK Radio 变体，且每个写入文件强制回读 `ea_get -V security.selinux` 与全镜像 `e2fsck -fn`。

## 验证

- **P0-1 高通 Radio 冷启实测 5/5 全通**：`meizu/Meizu22`、`asus/ASUSAI2501`、`nothing/Asteroids`、`redmagic/NX809J`、`xiaomi/popsicle` 全部冷启达成 `gsm.sim.state=LOADED,ABSENT`、`中国联通 (46001)`、`LTE`、`mVoiceRegState=0(IN_SERVICE)`、`mDataRegState=0(IN_SERVICE)`，且 `TelephonyDebugService` 秒返零超时、`AnswerToReset: Unexpected bytes after the check byte` 清零；
- **P0-2 `vivo/PD2454` NFC 冷启实测闭环 PASS**：`android.hardware.nfc-service` 在线、`dumpsys nfc` 返回 `mState=on`、`POST /api/nfc/tap` 后 `rxCommands` 由 138 增至 189、`apduCount=43`、`POST /api/nfc/remove` 恢复 `cardPresent=false`；
- **P0-3 全仓校验与单测全绿**：`patch_hal_refresh.py --only radio` 全仓 45/45 `patch.img` 全部 `[OK]`；`go test ./modules/pipeline/stages/02_prepare ./modules/pipeline/stages/09_build_vendor -count=1` 全绿（1.48s / 1.42s）。

## 遗留

三项 P0 跨机型共性阻塞已全部清零闭环；后续按 `TODO.md` 继续推进 P1/P2 各品牌原生相机与专属功能深度适配。
