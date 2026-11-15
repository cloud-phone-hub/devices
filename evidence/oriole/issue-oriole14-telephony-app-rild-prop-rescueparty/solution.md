# Issue: oriole A14 起机后 Pixel 电话栈常驻 App 写 `vendor_rild_prop` 前缀属性被拒 → 崩溃循环 → `reboot,rescueparty`

- 设备: Google Pixel 6（oriole，官方固件 AP2A.240905.003.F1 + lineage-21.0 vendor 底座，内核 `5.10.270-4k-cdh`）
- 宿主: macOS (darwin, QEMU + hvf)，实例 `oriole_14-AP2A.240905.003.F1-20240815/2`（ADB `127.0.0.1:5911`）
- 状态: **已闭环**（2026-11-12）：清单 `buildVendor.sepolicy` 补官方 `vendor_rild_prop` 7 条前缀 + priv_app 授权，重跑 `07 → 09 → 11` 后干净盘起机，运行 ≥ 298 s 无重启，RescueParty 0 次，两条触发属性均写入成功。
- 背景: 「oriole Android 14 云机和真机对齐」任务的第一个卡点——云机起不稳，没法装 device-checker 下发对拍。

---

## 一、问题

11-09 产物与当天重建产物起机后都能到 `sys.boot_completed=1`，但约 1.5 min 后整机重启，
`sys.boot.reason=reboot,rescueparty`，几轮后进 `recovery`（`persist.sys.boot.reason.history`
里 `recovery` 与 `reboot,rescueparty` 交替）。adb 每轮都掉成 `offline`，device-checker 云机版装上后
来不及注册到服务端。

## 二、根因

Pixel 6 官方 `/system_ext/priv-app` 里的电话栈常驻 App（`PERSISTENT` 标志）在启动后马上写厂商属性：

| App | 写的属性 | 调用点 |
| :--- | :--- | :--- |
| `com.samsung.slsi.telephony.oemril`（OemRilService） | `vendor.radio.allowed_types_loaded<slot>` | `PhoneConfig.setAllowedNetworkTypesLoaded` |
| `com.shannon.imsservice`（ShannonIms） | `persist.vendor.radio.call_waiting_for_sync_<slot>` | `ImsUt.setCallWaitingSyncProperty` |

真机上这几条由官方 vendor 策略兜住：`vendor_seapp_contexts` 把它们分到 `oemrilservice_app` /
`vendor_ims_app` 等专属域，`vendor_property_contexts` 第 138~144 行把 `vendor.radio.`、
`persist.vendor.radio.`、`vendor.ril.` 等前缀标成 `vendor_rild_prop`，`vendor_sepolicy.cil`
给这些域 `(property_service (set))`。

云机的 `/vendor` 是 lineage-21.0 底座的策略：**没有 `vendor_rild_prop` 类型、没有这些 seapp 条目**，
于是 App 落在通用 `priv_app` 域，属性落在 `vendor_default_prop`，`SystemProperties.set` 被
property_service 拒绝，Java 层抛 `RuntimeException: failed to set system property` 未捕获 →
常驻进程反复崩溃 → `PackageWatchdog` 判定失败 → RescueParty 重启。

只补 oemril 那一条（`vendor.radio.` 两行）后，ShannonIms 立刻接棒成为新的崩溃源（第 2 轮），
说明必须按官方 `vendor_rild_prop` 的整组前缀补齐，而不是逐个属性打补丁。

## 三、证据

- `log-fail.txt`：两轮崩溃栈、`dumpsys package` 的 `PERSISTENT` 标志、dex 里的属性名、
  云机 `getprop -Z` 与 `ps -AZ`（`vendor_default_prop` / `priv_app`）、官方 vendor 策略对应行。
- `log-pass.txt`：装配树策略文件复核、修复后运行态（uptime 298 s、两条属性已写入且标签为
  `vendor_rild_prop`、`RescueParty` 计数 0、剩余 app crash 只有 camera.services / chrome）。

## 四、改动

只改机型清单 [`oriole-android14.json`](../../../manifest/configs/google/oriole/oriole-android14.json)
的 `buildVendor.sepolicy`（07 步骤 6 按 marker 合入，`{{vers}}` 换成 `202404`）：

1. 新增 `vendor_property_contexts` 片段：逐行照抄官方 `vendor_rild_prop` 的 7 条前缀
   （`persist.vendor.debug_level`、`persist.vendor.ril.`、`persist.vendor.radio.`、`vendor.radio.ril.`、
   `vendor.sys.rild_reset`、`vendor.ril.`、`vendor.radio.`）；`ro.vendor.build.svn` 是只读身份属性，不改标签。
2. `vendor_sepolicy.cil` 追加：声明 `vendor_rild_prop`（`property_type` + `vendor_property_type`）；
   给 `priv_app` 补 `vendor_rild_prop` 与 `radio_prop`（官方 ims/rcs 域也有）的 set/read、
   `property_socket` 写与 `init` connectto；`radio` 域 set/read、`hal_radio_default` read。

不新造 `oemrilservice_app` 等专属域：CIL 无法继承 `priv_app` 的全量规则，复刻整套 app 域代价远大于收益；
代价是这组属性对所有 priv_app 可写（真机只对专属域开放），记入遗留。

## 五、验证

1. `cdh device build oriole --manifest 14-AP2A.240905.003.F1-20240815 --only 07_build_vendor` / `09_build_patch` / `11_pack_vda` 全部 EXIT=0；
2. `/api/delete` 删实例后 `/api/create` 干净盘重建，起机（首启 `reboot,factory_reset` 为新盘正常流程）；
3. 运行 298 s：`sys.boot_completed=1`，`logcat | grep -c RescueParty` = 0，dropbox 无新 `SYSTEM_BOOT` 循环；
   `vendor.radio.allowed_types_loaded0=1`、`persist.vendor.radio.call_waiting_for_sync_0=0`（`vendor_rild_prop`），
   oemril / imsservice 进程常驻不退。

## 六、遗留

- **SELinux 域与真机不一致**：这些 App 在真机是 `oemrilservice_app` / `vendor_ims_app` / `vendor_rcs_app` /
  `grilservice_app` / `euiccpixel_app` 等专属域，云机仍是 `priv_app`。普通 App 看不到别的进程的域，
  对对拍影响小，但 root 级取证看得出来。
- 其它非致命崩溃仍在，留给后续对齐条目：
  - `vendor.google.edgetpu_app_service@1.0-service` 每 5 s `Check failed: status == STATUS_OK` abort（无 EdgeTPU 设备）；
  - `com.google.android.apps.camera.services` `LyricConfigReceiver` NPE（Lyric 相机 HAL 缺位）；
  - `com.android.chrome` `Starting in 64-bit mode requires the 64-bit native library`——#149 的纯 64 位锁
    （`ro.zygote=zygote64`、abilist 只留 arm64）与真机 `arm64-v8a,armeabi-v7a,armeabi` 不一致，是对拍差异项之一。
- 官方 `vendor_modem_prop`（`euiccpixel_app` / `vendor_telephony_app` 会写）本次没补，目前没看到崩溃；
  如果后续出现 `persist.vendor.modem.*` 写失败再按同样方式补。
