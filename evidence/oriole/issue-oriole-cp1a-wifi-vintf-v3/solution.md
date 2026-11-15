# #161 oriole（CP1A.260405.005-20260310）：官方 `com.google.pixel.wifi.ext.apex` 新增 `IWifi/default (@3)` VINTF 声明与自研 `@2` Wi-Fi HAL 冲突致 `NULL VINTF MANIFEST` 崩显示栈

## 1. 问题

`out/core/daemon/runtime/oriole_CP1A.260405.005-20260310`（Pixel 6，Android 16 / `lineage-23.2` 底座）启动后永远卡在黑屏，无法进入桌面（`sys.boot_completed` 始终为空）：
- `qemu.log` 中 `init.svc.vendor.hwcomposer-3`（`status 234`）与 `vendor.graphics.allocator-default`（`minigbm`，`signal 6 SIGABRT`）每 5 秒循环崩溃退出；
- 二者 `.rc` 中配置了 `onrestart restart surfaceflinger`，而 `surfaceflinger.rc` 配置了 `onrestart restart --only-if-running zygote`，形成每 5 秒一次的 `hwcomposer-3 / minigbm → surfaceflinger → zygote` 连环重启风暴。

## 2. 根因

1. **`servicemanager` 在 `post-fs-data` 重载 VINTF 时返回 `NULL VINTF MANIFEST!: device`**：
   - 启动早期（`t ≈ 1.48s`）`apexd` 尚未挂载各分区 APEX 时，`servicemanager` 首次加载 `/vendor/etc/vintf` 成功，`hwcomposer-3` 与 `minigbm` 能正常注册。
   - `t ≈ 16.01s` `post-fs-data` 触发 `servicemanager` 执行 `getDeviceHalManifest: Reloading VINTF information.`，此时 `libvintf` 扫描已挂载的 `/apex/*/etc/vintf/` 片段并报：
     ```text
     vintf_object: VINTF parse error: Conflicting FqInstance: android.hardware.wifi.IWifi/default (@2) in /vendor/etc/vintf/manifest/android.hardware.wifi-service.xml vs. android.hardware.wifi.IWifi/default (@3) in /apex/com.google.pixel.wifi.ext/etc/vintf/manifest_wifi_ext_aidl.xml
     servicemanager: NULL VINTF MANIFEST!: device
     ```
   - 设备 VINTF manifest 变为 `NULL` 后，任何 HAL 声明校验（`isDeclared`）均返回未声明：`hwcomposer-3` 因 `Could not get passthrough or binderized IComposer` 以 `status 234` 退出；`minigbm` 因 `Assertion 'status == STATUS_OK' failed` 触发 `SIGABRT`，进而触发每 5 秒一次的连环杀进程。

2. **为什么 `AP2A`（Android 14）与 `BP1A`（Android 15）无此冲突，而 `CP1A` / `CP3A`（Android 16）会触发**：
   - 经 `debugfs` 对照四份官方 `vendor_a.img:/apex/com.google.pixel.wifi.ext.apex` 的 `apex_payload.img`：
     - `AP2A` 与 `BP1A` 的 `com.google.pixel.wifi.ext.apex` 内 `/etc/` 下仅有 `vendor.google.wifi_ext-service-vendor.rc`，**不含 `/etc/vintf/` 目录**（其 VINTF 声明在真机 `/vendor/etc/vintf/manifest/` 下，已被 `cdh_redirect` 的 `/vendor/etc` 重定向遮蔽）；
     - `CP1A` 与 `CP3A`（Android 16）将 VINTF 声明移入了 APEX 内的 `/etc/vintf/manifest_wifi_ext_aidl.xml`，声明 `android.hardware.wifi` `IWifi/default` `<version>3</version>`。
   - **不能通过 `cdh_redirect` 重定向 `/vendor/apex` 屏蔽**：`/vendor/apex` 下还有官方 `com.google.android.widevine-*.apex` 等组件需要由 `apexd` 正常挂载；
   - **两层必须同时对齐**：
     1. 自研 `android.hardware.wifi` HAL 在 Android 16/17 下此前仍编译为 AIDL v2（`@2`），必须升级到 AIDL v3（`@3`）并补齐 `IWifiChip::createApOrBridgedApIfaceWithParams`，使服务运行期 `getInterfaceVersion()` 返回 `3`，与官方 APEX 声明的 `@3` 对齐；
     2. `libvintf` 的唯一性检查要求同一 `FqInstance`（`IWifi/default`）不能在两个不同的 manifest fragment 中重复声明（即使两者均为 `@3` 也会报 `Conflicting FqInstance: IWifi/default (@3) vs. IWifi/default (@3)`），因此需按 `07_build_vendor/vintfdrop.go` 标准机制，在 `devices.db` 中为 `oriole` Android 16 版本配置 `vendor.vintf-drop: ["android.hardware.wifi-service.xml"]`，裁掉装配树 `/vendor/etc/vintf/manifest/android.hardware.wifi-service.xml` 中重复的声明，让官方 `/apex/com.google.pixel.wifi.ext/etc/vintf/manifest_wifi_ext_aidl.xml` 的 `@3` 声明唯一生效。

## 3. 证据

- 失败日志见 [log-fail.txt](log-fail.txt)
- 通过日志见 [log-pass.txt](log-pass.txt)

## 4. 改动

1. **升级 Android 16/17 的 `android.hardware.wifi` HAL 至 AIDL v3（`@3`）**：
   - `engine/vendor/build/internal/profile/profile.go`（及 `profile_test.go`）：在 `halTable16()` 中将 `"wifi"` 的 `android.hardware.wifi` 包版本覆盖为 `"3"`（Android 14/15 保持 `"2"`）。
   - `engine/vendor/build/modules/android.hardware.wifi.json`：将 `-DCDH_WIFI_AIDL_VERSION=2` 从 `compile.extraFlags` 移至 `compile.defines`，支持按 Android 版本覆盖。
   - 新增 `engine/vendor/build/modules/android16/android.hardware.wifi.json` 与 `engine/vendor/build/modules/android17/android.hardware.wifi.json`：覆盖 `"aidlPackageVersions": {"android.hardware.wifi": "3"}` 与 `"compile": {"defines": ["-DCDH_WIFI_AIDL_VERSION=3"]}`。
   - `engine/vendor/hardware/android.hardware.wifi/src/wifi_service.cpp`：在 `WifiChip` 中以 `#if defined(CDH_WIFI_AIDL_VERSION) && CDH_WIFI_AIDL_VERSION >= 3` 补齐 AIDL v3 新增的 `createApOrBridgedApIfaceWithParams` 实现。
2. **数据库配置 `vendor.vintf-drop`（`engine/build/assets/devices.db`）**：
   - 保持 `oriole` 的 `modules.redirect` 为标准 10 条规则（不重定向 `/vendor/apex`）；
   - 在 `devices.db` 中为 `oriole` 的 Android 16 版本（`id=3` `CP1A.260405.005-20260310` 与 `id=4` `CP3A.261005.002.A1-20260905`）的 `versions.buildVendor` 设置 `"vintf-drop": ["android.hardware.wifi-service.xml"]`。

## 5. 验证

1. `engine/vendor/build` 下 `go test ./...` 全部通过；`cdh-vendor hal build android.hardware.wifi --android 14|15|16` 全部编译通过（14/15 为 v2 `a5a330d7dabd`，16 为 v3 `3b3e31a62743`）。
2. 重跑 `cdh device build oriole --manifest CP1A.260405.005-20260310 --only 05_build_vendor_boot / 07_build_vendor / 09_build_patch / 11_pack_vda`（07 步骤 6c 打印 `[vintf] 裁掉 .../vendor/etc/vintf/manifest/android.hardware.wifi-service.xml（官方同名声明保留，避免 fqname 冲突）`）。
3. 重建并启动 `oriole_CP1A.260405.005-20260310` 实例 1（`pid=80675`，ADB `127.0.0.1:5911`，VNC `127.0.0.1:5912`）：
   - `cdh_redirect` 加载标准 10 条规则，官方 `/vendor/apex`（含 `com.google.android.widevine-13130248.apex` 与 `com.google.pixel.wifi.ext.apex`）正常挂载；
   - `qemu.log` 中 `NULL VINTF MANIFEST` 与 `Conflicting FqInstance` 均为 0 次；
   - `sys.boot_completed=1`、`init.svc.bootanim=stopped`，桌面显示与 VNC 触控点击正常；
   - 自研 `android.hardware.wifi-service`（AIDL v3）正常运行（`CdhWifiHal: getStaIfaceNames created=1,0`），`wlan0` 关联 `"CDH-A"`（`192.168.60.100/24`，`curState=CompletedState`）。

## 6. 遗留

- 官方 `product_a.img` 预置的 Chrome 壳包因不含 `libmonochrome.so` 在首次开机时触发一次 `com.android.chrome` 崩溃弹窗（与 `#105` 遗留同源，属官方壳包问题，不影响桌面与系统服务）。
- `android-hardware-media-c2-hal`（`IComponentStore/default1`）被媒体扫描触发时以 `status 1` 退出 4 次（系统已有 `IComponentStore/software` 与 v4l2 codec2 正常工作，不影响开机与桌面）。
