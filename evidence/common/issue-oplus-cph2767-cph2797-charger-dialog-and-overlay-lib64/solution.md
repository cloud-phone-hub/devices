# #311 OPPO `CPH2797` & OnePlus `CPH2767` 80% 充电弹窗根治、OPlus `/product/lib64` overlay 挂载修复与相机流突破

## 问题

OPPO `CPH2797`（ColorOS 16 / 天玑 MT6993，`vendor_sdk=36`）与 OnePlus `CPH2767`（OxygenOS 16 / 高通 canoe，`vendor_sdk=36`）存在两项跨 OPlus 共性阻塞：
1. **80% 充电保护弹窗反复抢占焦点**：两机冷启后 `com.oplus.battery` 均弹出「Set charging limit to 80%」对话框覆盖桌面与应用，且 `dumpsys battery` 显示 `ChargerTechnology: -1, UsbHwStatus: -1, BatteryHwStatus: -1, BatteryIcStatus: -1`；
2. **原生相机缺库崩溃/未建预览流**：`OnePlus CPH2767` 打开 `com.oplus.camera` 报缺 `libOplusStringJNI.so`、`libAPSClient-alog-jni.so`、`libCombineLutJni.so`（`UnsatisfiedLinkError` 退出），且未导出 `com.oplus.supported.cameraid.type` vendor tag。

## 根因

1. **80% 充电弹窗的双重根因（`CdhOplusCharger` 未注册 / 被空桩抢注）**：
   - `devices/hardware/android.hardware.health/src/oplus/CdhOplusCharger.cpp` 的 `DetectOplusChargerVintf()` 仅硬编码检查 4 个固定 XML 路径，漏掉了 `09_build_vendor` 合成的 `/vendor/etc/vintf/manifest/cdh_vendor_placeholders.xml`，导致 `CPH2767` 的 `health@oplus` 跳过注册，而被 `cdh_nn_placeholders.txt` 中的 `neuralnetworks-service` 空桩抢注 `vendor.oplus.hardware.charger.ICharger/default`（所有调用报 `Method is unimplemented`，`notifycode=-1` 误触发 80% 弹窗）；
   - `CPH2797` 虽已启用 `health@oplus` 且 `vendor_service_contexts` 声明了 `ICharger/default`，但 VINTF manifest 中缺少该项声明，导致 `ICharger/default: not found`（`OplusBatteryServiceFeature: getIChargerService fail`）同样触发弹窗。
2. **跨 OPlus `/product/lib64` overlay 挂载失败致 40 个相机 JNI 库不可见**：
   - `CPH2767` 与 `CPH2797` 的全部 40 个相机算法库（`libCombineLutJni.so`、`libOplusStringJNI.so`、`libAPSClient-alog-jni.so` 等）实际原样存在于 `/my_product/lib64/`，真机依赖 `fstab` 中的 `overlay-overlay /product/lib64` 将其叠加到 `/product/lib64` 供 `product-clns-9` 搜索；
   - `06_build_vendor_boot/fstab_merge.go` 剔除了 OTA 不含的裸逻辑分区 `my_preload`，却未同步裁剪 `overlay` 行 `lowerdir=` 中引用的 `/my_preload/lib64`、`/my_preload/app` 等死路径，导致 `on fs` 阶段内核 `overlayfs` 报 `failed to resolve '/my_preload/lib64': -2 (ENOENT)`，使 `/product/lib64` 等 4 条关键叠加全线失败。
3. **顺手发现并修复两处全仓共性隐患**：
   - 此前未重编 `out/agent/dist/cdh-pipeline` 导致新编 `android15/16` `libEGL_meow.so` 引用未定义符号 `cdh_ovr_eglChooseConfig`，已重编 `cdh-pipeline` 与全代 `android14..17` GPU 库并用 `nm -D` 验证清零；
   - `09_build_vendor/files_compat.go` 对 Lineage 基座残缺 `media_profiles_V1_0.xml`（含 `"720p "` 尾空格且缺 `1080p`）未触发官方回填，已增补 `isIncompleteMediaProfiles` 自动覆写检测。

## 证据

- 失败日志：[log-fail.txt](log-fail.txt)
- 通过日志：[log-pass.txt](log-pass.txt)
- 对比截图：
  - `CPH2767` 修复前 80% 充电弹窗抢焦点：[cph2767-fail-80pct-dialog.png](cph2767-fail-80pct-dialog.png)
  - `CPH2767` 修复后干净桌面无弹窗：[cph2767-pass-home-clean.png](cph2767-pass-home-clean.png)
  - `CPH2767` 原生相机突破缺库崩溃并建立 8 条流（`Stream[0]` 4143 帧）：[cph2767-camera-active-8streams.png](cph2767-camera-active-8streams.png)
  - `CPH2797` 修复后干净桌面无弹窗：[cph2797-pass-home-clean.png](cph2797-pass-home-clean.png)

## 改动

1. **`devices/hardware/android.hardware.health/src/oplus/CdhOplusCharger.cpp`**：
   - `DetectOplusChargerVintf()` 改为扫描 `/odm/etc/vintf/manifest/` 与 `/vendor/etc/vintf/manifest/` 下全部 `.xml` 文件，且 `ParseOplusChargerVintfVersion()` 在匹配 `vendor.oplus.hardware.charger` 的当前 `<hal>...</hal>` 块内解析 `<version>`，完整兼容多 HAL 合并清单 `cdh_vendor_placeholders.xml`。
2. **`agent/modules/pipeline/stages/09_build_vendor/files_compat.go`**：
   - 当检测到启用了 `android.hardware.health@oplus` 时，自动从 `etc/cdh_nn_placeholders.txt` 剔除 `vendor.oplus.hardware.charger.ICharger/default` 避免空桩抢注，并确保其以 `hal_health_default` 域写入 VINTF manifest；
   - 升级 `media_profiles` 回填条件：基座存在但缺失 `1080p` 或含 `"720p "` 时强制以官方 vendor 媒体配置覆盖。
3. **`agent/modules/pipeline/stages/06_build_vendor_boot/fstab_merge.go` & `vendor_boot_test.go`**：
   - 新增 `pruneOverlayLowerdirs()` 同步裁剪 `overlay` 挂载行 `lowerdir=` 中引用已剔除逻辑分区的失效路径段，补齐 `TestMergeCloudFstabPrunesDroppedOverlayLowerdir` 单元测试；
   - 重编 `cdh-pipeline` 与 `android15/16` `health@oplus`、全代 `android14..17` `camera` 与 `gpu` 库，并通过 `out/devices/tmp/oplus_patch_fix.py` 刷新两机 `patch.img`。

## 验证

1. `cd agent && go test ./modules/pipeline/stages/06_build_vendor_boot ./modules/pipeline/stages/09_build_vendor` 全绿；
2. **`OnePlus CPH2767` 重启实测**：
   - `ICharger/default: found`，`ChargerTechnology=0, UsbHwStatus=0, BatteryHwStatus=1, BatteryIcStatus=0`，零 `ICharger ... unimplemented`，桌面无 80% 充电弹窗；
   - `overlay-overlay on /product/lib64` 挂载成功，`libCombineLutJni.so` / `libOplusStringJNI.so` / `libAPSClient-alog-jni.so` 全部成功加载；
   - `dumpsys media.camera` 导出 `com.oplus.supported.cameraid.type`（0/1/2/3），`com.oplus.camera` 零崩溃进入拍照界面并建立 8 条流，`Stream[0]`（`1600x1200 YUV_420_888`）持续产出 4143+ 帧；
3. **`OPPO CPH2797` 重启实测**：
   - `ICharger/default: found`，`ChargerTechnology=0, UsbHwStatus=0, BatteryHwStatus=1, BatteryIcStatus=0`，80% 充电弹窗彻底消失；
   - `overlay-overlay on /product/lib64` 挂载成功，`com.oplus.camera` 突破缺库阶段进入 `buildSession`。

## 遗留

1. **`OnePlus CPH2767` 预览渲染上屏与录像档位**：PHOTO 模式 `Stream[0]` 已持续产出 4143+ 帧，但 OPlus 闭源 `OCAM_CameraScreenNail` 报 `onFrameAvailable, surfaceTexture does not correspond` 且 `OplusCaptureResult` 缺 `com.oplus.preview.skip.draw.frame` 致预览黑屏；VIDEO 模式需补齐该盘集完整 `media_profiles_V1_0.xml`；
2. **`OPPO CPH2797` `HdrAllRoutePresenter` 空指针**：`buildSession` 阶段在 `HdrAllRoutePresenter.g1` 报 `List.contains` `NullPointerException`，后续需反编译确认其查询的 HDR Vendor Tag / 特性列表。
