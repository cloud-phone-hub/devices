# arw：HMD Crest Max（`hmd/ARW` / Unisoc UMS9620 / GRF vendor=13，system=14，kernel 5.15）

1 条机型条目：#304 记录 `hmd/ARW`（`14-QP1A.190711.020-20250620`，`lineage-20.0` + `kernel 5.15`）解决展锐扩展 AIDL `IExtRadio*` 无回调与 `RadioInteractorHandler$SyncHandler` 遇错写 `null` 导致的 `com.android.phone` 主线程死锁 ANR、同步 `#301` 5 GHz OFDM Wi-Fi 补丁内核、回填真机基带版本、跳过 HMD `WelcomeSetupWizard` 引导页、屏蔽 `/system_ext/bin/tool_service` 高频 AVC 及修复 Android 13 下 `CdhAllocator.h` 编译兼容。

## 条目

| # | 问题标识 | 说明 | 状态 |
| :--- | :--- | :--- | :--- |
| 304 | [issue-arw-boot-radio-deadlock-wifi-baseband](issue-arw-boot-radio-deadlock-wifi-baseband/solution.md) | `HMD Crest Max #1` 存在 6 项启动与运行问题：① 展锐 `IExtRadio*` 占位缺异步回调、`RadioResponse.responseInts` 空指针及 `SyncHandler` 在 `getSimlockDummys` 遇错写 `null` 致 `com.android.phone` 主线程永久死锁 ANR 与 SIM 恒 `,ABSENT`；② 旧 5.15 内核未含 `#301` 5 GHz OFDM 速率补丁致 Wi-Fi 断联；③ 缺真机基带属性显示 `,`；④ 首启停在 `com.hmdglobal.app.setupwizardext/.WelcomeSetupWizard`；⑤ `/system_ext/bin/tool_service` 高频 AVC；⑥ `CdhAllocator.h` AIDL V2 方法缺 API>=34 守卫致 A13 编译失败。**改动**：`android.hardware.radio/src/main.cpp` 补齐 `OnUnisocExtTransact` 回调、非空空数组编码及 3 项防死锁查询 `error=0`；`CdhAllocator.h` 补 API 守卫；`cph-setup-display` 跳过 HMD SetupWizard 并拉起桌面；`ARW` 清单补真机基带 `5G_MODEM_V2_23B_W24.26.4_P11`、关 HDR、锁纯 64 位并屏蔽 `tool_service`；全链重编发布 `patch.img`（`2783574327d1`） | **已闭环**（2026-11-22）：全新冷启与二次重启零干预双验通过，`Enforcing`、SIM `LOADED,ABSENT`、中国联通 `IN_SERVICE`、`ccmni2` 与 5 GHz `TP-LINK_2464` 双通、基带正常、`anr_events=0`、`non_chrome_crashes=0`、`dri_image_err=0`，VNC 桌面与网络设置审图全部通过 |
