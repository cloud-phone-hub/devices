# #270：Nothing Phone (3a) (`nothing/Asteroids`) Android 15 GRF (`vendor=14 / system=15`) 三层配置修复闭环

- 编号：`#270`
- 分组：`asteroids`（Nothing Phone (3a)，`nothing/Asteroids`）
- 机型：`nothing/Asteroids` `14-UKQ1.241011.001-20251013`（`Asteroids_V3.2-251013-1406.zip`，`system=15` / `vendor=14 (volcano)`，`lineage-22.2` + 借用 `kernel 6.1.138-4k-cdh`）
- 状态：**已闭环**（2026-11-19）

## 1. 问题

`nothing/Asteroids` `14-UKQ1.241011.001-20251013` 通过 GRF 代际校准（见 `../../common/issue-grf-system-android-version-config-reconcile/`）切入 `Asteroids-android15` 并全套编出 `vda.img` 后，冷启动依次暴露三层卡死/崩溃循环（全程未改动任何源码，仅定位并补齐 manifest 配置）：

1. **首轮卡死**：`hwc3` 与 `SurfaceFlinger` 启动报 `gralloc-mapper is missing`，伴随 `SELinux: avc: denied { find } ... name=android.hardware.graphics.mapper.IMapper/qti`（`tcontext=u:object_r:default_android_service:s0`）。
2. **次轮系统崩溃循环**：`PowerManagerService` 每 5 秒触发 `*** FATAL EXCEPTION IN SYSTEM PROCESS: PowerManagerService` -> `NullPointerException` at `BrightnessMappingStrategy$SimpleMappingStrategy.setBrightnessConfiguration(BrightnessMappingStrategy.java:887)`，同时伴随 `avc: denied { read } ... vendor_display_notch_prop`。
3. **第三轮系统崩溃循环**：解决亮度 NPE 后，`system_server` 又触发 `*** FATAL EXCEPTION IN SYSTEM PROCESS: NtRefreshRateConfig` -> `NullPointerException` at `NtRefreshRateController.qos`（`ArrayMap.containsKey` 空引用）。

## 2. 根因

1. **`mapper/qti` 缺 service context**：GRF `vendor` 源自 Android 14（`plat_sepolicy_vers=34.0`），官方 `plat_pub_versioned.cil` 未导出 `hal_graphics_mapper_service_34_0` 导致 `09_build_vendor` 跳过同步官方 `IMapper/qti`；后续重命名 `mapper/minigbm* -> mapper/qti` 时因缺少上下文使服务落入 `default_android_service` 被 SELinux 拒绝。
2. **`ro.vendor.display.*` 落入 `vendor_default_prop` 被静默拒读**：
   - 官方 `vendor_a.img/build.prop` 确含 `ro.vendor.display.array.auto.nits=7,7,...`，但官方策略将 `vendor.display.`/`ro.vendor.display.`/`persist.vendor.display.` 划归受限类型 `vendor_display_prop`（不在 `vendor_public_property_type`），故未自动同步进 `lineage-22.2` 的 `vendor_property_contexts`。
   - 缺失上下文使 `ro.vendor.display.array.auto.nits` 落入 `vendor_default_prop_34_0`，而 `lineage-22.2` 存在 `(dontaudit system_server_202404 vendor_default_prop_202404 (file (read)))`，`system_server` 静默读到空串；反汇编 `nt-framework.jar`/`nt-services.jar`/`services.jar` 证实：`NtFeaturesUtils` 特性 72 启用且 `useGmsAdaptiveBrightness()==true` 时，`SimpleMappingStrategy.<init>` 读到空串将 `mDefaultConfig` 留为 `null`，随后 `setBrightnessConfiguration(null)` 调 `v7.equals(mConfig)` 直接抛出 NPE。同因 `vendor_display_notch_prop` 也缺少 `system_server` 读权限。
3. **缺少 `/vendor/etc/display_refresh_rate_config.json`**：反汇编 `nt-services.jar` `NtRefreshRateController$pjy.ncb` 确认 Nothing OS 3.2 启动期无防御读取 `/vendor/etc/display_refresh_rate_config.json` 来初始化 `qos` `ArrayMap`，文件存在于官方 `unpack_vendor/etc/` 但此前未纳入 `vendor/config.json` `"files"`，导致读失败留空触发 NPE。（Nothing OS 4.1 / A16 对上述两项做了空指针防御，故 `#268` 未触发。）

## 3. 证据

- 三轮失败日志（含 `gralloc-mapper`、`SimpleMappingStrategy` NPE、`NtRefreshRateController.qos` NPE 与实测 AVC）：[log-fail.txt](log-fail.txt)。
- 通过验证日志（`verify_boot.py --timeout 120`、`getprop -Z`、`hvc7` 桌面/显示/Radio/dropbox 核查与 `patch-info.txt`）：[log-pass.txt](log-pass.txt)。
- 实机 VNC 截图：[pass-screen.png](pass-screen.png)（`1088x2392` Nothing OS 3.2 主桌面完整渲染，`meanBrightness=112.99`）。

## 4. 改动

全程未修改任何仓库源码，仅增补 `Asteroids-android15` 清单配置：

1. 新增 [vendor_service_contexts](../../../../devices/manifest/configs/nothing/Asteroids/Asteroids-android15/vendor/sepolicy/vendor_service_contexts)：声明 `android.hardware.graphics.mapper.IMapper/minigbm_upstream` 与 `/minigbm` -> `u:object_r:hal_graphics_mapper_service:s0`（构建期由 `09_build_vendor` 自动重命名为 `mapper/qti`）。
2. 新增 [vendor_property_contexts](../../../../devices/manifest/configs/nothing/Asteroids/Asteroids-android15/vendor/sepolicy/vendor_property_contexts)：添加 `vendor.display.`、`ro.vendor.display.`、`persist.vendor.display.` -> `u:object_r:vendor_display_prop:s0`。
3. 新增 [vendor_sepolicy.cil](../../../../devices/manifest/configs/nothing/Asteroids/Asteroids-android15/vendor/sepolicy/vendor_sepolicy.cil)：声明 `vendor_display_prop` 属性集与官方对等读写权限（含 `system_server`、`surfaceflinger`、`bootanim`、`appdomain`、`vendor_init` 等），并补齐 `system_server` / `platform_app` 对 `vendor_display_notch_prop` 的读权限。
4. 更新 [vendor/config.json](../../../../devices/manifest/configs/nothing/Asteroids/Asteroids-android15/vendor/config.json)：在 `"files"` 中从官方 `vendor` 提取 `etc/display_refresh_rate_config.json`、`etc/advance_thermal_mitigation/nt_boost_adjuster.xml` 与 `etc/richtapresources`（`"mode": "replace"`）。
5. 同批补入兼容配置 [vendor/etc/init/cph_nothing_setup.rc](../../../../devices/manifest/configs/nothing/Asteroids/Asteroids-android15/vendor/etc/init/cph_nothing_setup.rc)（处理 `com.nothing.introduction*`/`engineerservice`/`vendor.qti.iwlan`）。

## 5. 验证

1. `TMPDIR=/Volumes/work/cloud-phone-hub/out/devices/tmp /Volumes/work/cloud-phone-hub/out/agent/dist/cdh-pipeline device build Asteroids --manifest 14-UKQ1.241011.001-20251013 --from build_vendor`：
   - `vendor/etc/display_refresh_rate_config.json`（`6617 B`，`u:object_r:vendor_configs_file:s0`）落盘；`patch.img`（`384.0 MiB`，`sha256 05731e2fe4ae`）回读 `837/837` 通过；`vda.img`（`13.00 GiB`）生成完毕。
2. 独占冷启动验收 `verify_boot.py --timeout 120 --keep`：
   - `passed=true`、`bootCompleted=true`、`bootanim=stopped`、`SurfaceFlinger=running`、`bootReason=reboot,factory_reset`；
   - `hvc7` 实测 `getprop -Z ro.vendor.display.array.auto.nits` -> `u:object_r:vendor_display_prop:s0`，完整读出曲线数组，`PowerManagerService` 与 `NtRefreshRateController` 正常初始化；
   - `wm size=1080x2392`、`wm density=388`、`mActiveModeId=2`、`IQtiRadioStable/slot1: found`、`service list` 共 `309` 项、`/data/system/dropbox` 无 `system_server` 崩溃；
   -  skip 引导进入 `com.nothing.launcher/com.android.searchlauncher.SearchLauncher` 后调用 `read_file` 审阅 VNC PNG（`1088x2392`，`meanBrightness=112.99`），完整渲染 Nothing OS 3.2 主桌面、点阵日期与系统状态栏。随后实例已停止并销毁（`running: []`）。

## 6. 遗留

- `logcat -b crash` 仍可见后台 `com.android.nfc`（`/system_ext/priv-app/Nfc_st/Nfc_st.apk` `StNativeNfcManager.initialize`）tombstone，因云机默认挂载 `android.hardware.nfc` AIDL 通用桩而非 ST21NFC 专属实现；不影响桌面启动与系统稳定性，后续如有 NFC 需求可按同机需求评估。
