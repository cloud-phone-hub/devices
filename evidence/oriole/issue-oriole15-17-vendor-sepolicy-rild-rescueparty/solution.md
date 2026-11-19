# #256 oriole Android 15~17：`vendor/sepolicy` 缺同步致 `com.shannon.imsservice` 写 `vendor_rild_prop` 触发 `RescueParty` 重启与 `hal_drm_widevine_exec` 未映射

## 1. 问题

`google/oriole` Android 15（`15-BP1A.250505.005-20250326`）解黑屏问题后复查实例状态，发现虚拟机在开机约 `54.49s` 时反复触发 `init: Received sys.powerctl='reboot,RescueParty'` 强制重启，且重启瞬间 `init` 停掉 `surfaceflinger` 后宿主帧缓存回落到 `fbcon` 控制台画面。同时内核启动期伴随：
`SELinux: Context u:object_r:hal_drm_widevine_exec:s0 is not valid (left unmapped)` 导致 `vendor.drm-widevine-hal` 启动失败。

检查同机型配置目录发现 `oriole-android15`、`oriole-android16`、`oriole-android17` 的 `vendor/sepolicy/` 均缺失（而 `oriole-android14/vendor/sepolicy/` 早已补齐对应规则）。

## 2. 根因

1. **常驻 telephony 服务权限拒绝对照触发 `RescueParty`**：`oriole-android15~17` 缺少 `oriole-android14/vendor/sepolicy/` 中已固化的 `vendor_property_contexts` 与 `vendor_sepolicy.cil`（关联历史条目 [#197](../issue-oriole14-telephony-app-rild-prop-rescueparty/solution.md) 与 [#201](../issue-oriole14-checker-align-r2/solution.md)），导致系统属性 `persist.vendor.radio.call_waiting_for_sync_0` 无法映射为 `vendor_rild_prop`；常驻 `priv_app` `com.shannon.imsservice` 写该属性遭 SELinux 拒绝，连续抛出 `RuntimeException: Permission denied` 快速崩溃重启，最终触发 `system_server` 拉起 `reboot,RescueParty`。
2. **Widevine APEX 上下文未映射**：同目录 `vendor_file_contexts` 与 `vendor_sepolicy.cil` 缺失导致 `/apex/com.google.android.widevine/bin/hw/android.hardware.drm-service.widevine` 对应的 `hal_drm_widevine_exec` 类型无效并回退 `u:object_r:unlabeled:s0`，`init` 拒绝启动 Widevine DRM HAL。
3. **验证脚本假阳性盲区**：旧版 `verify_boot.py` 仅在一次 `sys.boot_completed=1` 后抓 VNC 帧，未在抓帧后复核进程与重启状态，若恰逢 `RescueParty` 停掉 `surfaceflinger`，VNC 抓到的 `fbcon` 控制台白字黑底文本平均亮度（`~24.6`）会混过 `>= 2.0` 的阈值。

## 3. 证据

- 失败日志见同目录 [log-fail.txt](log-fail.txt)：
  - `qemu.log`：`[   54.496888][  T129] init: Received sys.powerctl='reboot,RescueParty' from pid: 675 (system_server)`；
  - `qemu.log` AVC 拒绝：`avc: denied { write } for comm="pool-10-thread-" name="property_service" ... scontext=u:r:priv_app:s0:c512,c768 tcontext=u:object_r:property_socket:s0 tclass=sock_file permissive=0 app=com.shannon.imsservice`；
  - `logcat`：`com.shannon.imsservice` 持续抛出 `java.lang.RuntimeException: failed to set system property "persist.vendor.radio.call_waiting_for_sync_0" to "0": Permission denied` 直到 `am_process_crashed_too_much`；
  - `qemu.log`：`SELinux: Context u:object_r:hal_drm_widevine_exec:s0 is not valid (left unmapped).` 与 `Could not start service 'vendor.drm-widevine-hal'`。

## 4. 改动

1. 将 `/Volumes/work/cloud-phone-hub/devices/manifest/configs/google/oriole/oriole-android14/vendor/sepolicy/` 三件套（`vendor_file_contexts`、`vendor_property_contexts`、`vendor_sepolicy.cil`）同步复制到：
   - `devices/manifest/configs/google/oriole/oriole-android15/vendor/sepolicy/`
   - `devices/manifest/configs/google/oriole/oriole-android16/vendor/sepolicy/`
   - `devices/manifest/configs/google/oriole/oriole-android17/vendor/sepolicy/`
2. 针对 Android 16 起 Google `system_ext` 将常驻电话栈应用从 `priv_app` 迁入独立 SELinux 域（`oemrilservice_app`、`vendor_ims_app`、`vendor_rcs_app`），在 `oriole-android16/vendor/sepolicy/vendor_sepolicy.cil` 与 `oriole-android17/vendor/sepolicy/vendor_sepolicy.cil` 中比对官方 `gs101-sepolicy` 补齐版本化属性声明及 `vendor_rild_prop` / `radio_prop` / `property_socket` / `init`、`vendor_init` 授权规则。
3. 加固对话级验证脚本 `scratch/verify_boot.py`：抓帧之后二次轮询核对 `postBootCompleted == "1"`、`surfaceFlingerState == "running"` 且 `ro.boot.bootreason` 不含 `rescueparty`，防止把 `RescueParty` 重启瞬间的 `fbcon` 画面误判通过。

## 5. 验证

1. `google/oriole` Android 15（`15-BP1A.250505.005-20250326`）重跑 `build_hardware -> pack_vda` 后冷启动并用加固版 `verify_boot.py` 验收（见 [log-pass.txt](log-pass.txt) 与 [pass-screen.png](pass-screen.png)）：
   - `bootCompleted=true`、`postBootCompleted="1"`、`surfaceFlingerState="running"`、`bootReason="reboot,factory_reset"`、`topActivity=...NexusLauncherActivity`、`vnc.meanBrightness=91.4`、`peak=255`；
   - `getprop -Z persist.vendor.radio.call_waiting_for_sync_0` 返回 `u:object_r:vendor_rild_prop:s0`，`com.shannon.imsservice` 稳定存活，`RescueParty=0`、`hal_drm_widevine_exec=0`。
2. `google/oriole` Android 16（`16-CP1A.260405.005-20260310`，联合 [#257](../../common/issue-ext4-partial-dir-cache-handheld-permissions/solution.md)）重建后冷启动并挂机复核至 `proc_uptime=91.69s`（见 [log-pass.txt](log-pass.txt)）：
   - `bootCompleted=1`、`surfaceFlingerState=running`、`bootReason=reboot,factory_reset`、`vnc.meanBrightness=86.75`、`peak=255`，截图完整 Pixel 16 桌面；
   - `vendor.radio.allowed_types_loaded0=1`、`persist.vendor.radio.call_waiting_for_sync_0=0`（标签均 `u:object_r:vendor_rild_prop:s0`），`com.shannon.imsservice`（`vendor_ims_app`）与 `com.samsung.slsi.telephony.oemril`（`oemrilservice_app`）均稳定存活、`RescueParty=0`。

## 6. 遗留

- `oriole-android17` 已预先同步并补齐 `oemrilservice_app` / `vendor_ims_app` / `vendor_rcs_app` 规则，待 Tier 4 构建 `17-CP3A` 时实测确认兼容性。
