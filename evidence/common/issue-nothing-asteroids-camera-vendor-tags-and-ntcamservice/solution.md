# #314 — Nothing `Asteroids`（A16，`vendor_sdk=34`）原生相机专有 Vendor Tags 与 `INtCamService` 闭环

## 1. 问题

`nothing/Asteroids`（`Asteroids_16-UKQ1.250612.001-20260810`，`vendor_sdk=34`）拉起原生相机 `com.nothing.camera/.activity.CameraActivity` 时：
1. `CONNECT device 0` 并收到首帧预览回调后，`preview-frame-process-thread` 立即因缺少 `com.nothing.camera.sr.enable`（`CaptureResult.get` 未捕获异常）抛 `IllegalArgumentException` 闪退；
2. 伴生算法服务进程 `com.camera.ufs.service`（`UFSManager`）因 SELinux 拒绝 `platform_app` 查找 `vendor.noth.hardware.camera.INtCamService/default` 且占位空桩未返回 `INtCamSession`，随后对空对象调用 `INtCamSession.setParameters` 抛 NPE 崩溃。

## 2. 根因

1. **缺少 Nothing / QTI 专有 Vendor Tags**：反编译 `/system_ext/priv-app/NTCamera/NTCamera.apk` 中 `com.nothing.common.setting.SettingCharacteristics` 确认 Nothing 相机集中声明了 180 项 `com.nothing.camera.*` / `org.quic.*` / `com.vidhance.stabilizer.*` 专有标签（含 `com.nothing.camera.sr.enable` `TYPE_INT32`、`com.nothing.camera.glyphlight.enable`、`com.nothing.camera.captureNumber` 等），未在相机 HAL 注册。
2. **AOSP `VendorTagDescriptor` 最长前缀截断陷阱**：`VendorTagDescriptor::lookupTag()` 按无点边界前缀匹配 section 名；若同时注册 `"com.nothing.camera"` 与 `"com.nothing.camera.capture"`，查询 `"com.nothing.camera.captureNumber"` 会被前者长前缀吃掉 `"com.nothing.camera.capture"` 并跳过 `'N'`，报错 `Tag name 'umber' does not exist`。将所有标签归并至 14 个互不重叠的顶级 section（如 `"com.nothing.camera"`、`tagName="capture.timestamp"` / `"captureNumber"`）可实现零前缀碰撞。
3. **`INtCamService` 权限与协议缺失**：`vendor.noth.hardware.camera.INtCamService`（version=1, hash=`ec128e41418deb99810012bba5a2699bdae7534b`）的 `openSession()`（code 1）需返回非空 VINTF `INtCamSession`（其 5 个方法均为 `void`），且 `vendor_sepolicy.cil` 需放行 `platform_app_34_0` 查找 `vendor_hal_noth_camera_service`。

## 3. 证据

- 修复前主/次崩溃与前缀截断日志见 [log-fail.txt](log-fail.txt)。
- 修复后多模式交互与零崩溃验收日志见 [log-pass.txt](log-pass.txt)。
- 实屏视觉验收截图（经 `read_file` 逐张核对）：
  - 后摄 PHOTO 满屏高清蓝天白云预览（1877+ 帧）：[photo-rear.png](photo-rear.png)
  - 快门拍照后左下角生成清晰蓝天白云缩略图并切换前摄：[photo-front.png](photo-front.png)

## 4. 改动

1. **[cdh_nothing_vendor_tags.inc](file:///Volumes/work/cloud-phone-hub/devices/hardware/android.hardware.camera/src/cdh_nothing_vendor_tags.inc)**（新增）与 **[camera_provider_service.cpp](file:///Volumes/work/cloud-phone-hub/devices/hardware/android.hardware.camera/src/camera_provider_service.cpp)**：
   - 新增 `isNothingDevice()`，从 `NTCamera.apk` 自动提取并去重 180 项 Nothing/QTI 专有 Vendor Tags，归并至 14 个零前缀碰撞顶级 section（`0x8200xxxx..0x820dxxxx` 段），仅在 Nothing 设备运行时注册。
2. **[neuralnetworks_service.cpp](file:///Volumes/work/cloud-phone-hub/devices/hardware/android.hardware.neuralnetworks/src/neuralnetworks_service.cpp)**：
   - 新增 `OnNothingCamServiceTransact` 与 `OnNothingCamSessionTransact`，按精确版本（v1）与哈希实现 `INtCamService::openSession()` 返回有效 VINTF `INtCamSession` 占位对象，`registerCallback`/`setParameters`/`addMetaData`/`processImgRequest`/`closeSession` 均返回 `EX_NONE`。
3. **重编与刷写 `android14`（`vendor_sdk=34`）HAL**：
   - 执行 `cdh-pipeline build-hardware --android 14 --module android.hardware.camera --module android.hardware.neuralnetworks --refresh`；
   - 将新版 `camera-service`、`neuralnetworks-service` 及 `vendor_sepolicy.cil`（放行 `platform_app_34_0` 查找 `vendor_hal_noth_camera_service` 与调用 `hal_neuralnetworks_default`）刷入 `nothing/Asteroids` `patch.img`，经 `ea_get -V` 与 `e2fsck -fn` 校验通过。

## 5. 验证

冷启 `nothing/Asteroids`（`Asteroids_16-UKQ1.250612.001-20260810`，`+28.9s` 完成启动）：
- 冷启 `com.nothing.camera/.activity.CameraActivity` 耗时 **1482ms** 秒开；
- 后摄 PHOTO 建起 5 条输出流，`Stream[0]` 与 `Stream[1]` 均稳定产出 **1877+ 帧**，`photo-rear.png` 满屏超清蓝天白云无畸变伪影；
- 点击快门实拍，左下角相册缩略图成功渲染清晰蓝天白云成片；
- 切换前摄（Camera ID 1，ACTIVE 95 帧）与切回后摄（Camera ID 0，ACTIVE 96 帧）均零崩溃，全程 `com.nothing.camera` 与 `com.camera.ufs.service` 双进程稳态存活，日志零 `Could not find tag`、零 `INtCamService` 报错。

## 6. 遗留

1. `vendor_sepolicy.cil` 中对 `platform_app_34_0` 放行 `vendor_hal_noth_camera_service` 规则需在全仓收尾阶段固化至 `09_build_vendor`。
2. UFS 拍照后处理依赖真机 DSP，相册缩略图已正常渲染，`.pending-*.jpg` 异步提交存在轻微延迟（未影响预览与连续交互）。
