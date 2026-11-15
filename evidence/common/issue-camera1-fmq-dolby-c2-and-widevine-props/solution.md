# Camera1 FMQ 共享内存授权、Dolby C2 服务上下文映射与 Widevine CDM v19 属性对齐（E1 / E2 / E3）

- 编号：`#178`
- 分组：`common`（`android.hardware.camera`、`android.hardware.media`、`android.hardware.drm` 三模块公共修复 + `gold-android15.json` 机型配置对齐）
- 对标机型：Xiaomi Redmi Note 13 5G（`gold` / `2312DRAABC`，`OS3.0.10.0.VNQCNXM`，真机 ID `ec519c7d513a3ad3`）
- 云机实例：`gold_15-OS3.0.10.0.VNQCNXM/1`（`df809591a4e085ae-cloud`，ADB `127.0.0.1:5911`）
- 状态：**已闭环**（`aligner_xiaomi_gold.all` 从 `22 / 32` 提升至 **`25 / 32`**，维度 E `E1 / E2 / E3 / E4` 全部 `PASS`）

---

## 1. 问题

在 `device-checker` 的 `aligner_xiaomi_gold.camera_media_drm_audio` 与 `aligner_xiaomi_gold.all`（`run-00223`）中，维度 E 的前三项均未通过：

1. **`E1_camera2AndCamera1Connectable = false`**：
   - `CameraManager.getCameraIdList()` 可枚举出 `["0", "1"]`，但 `Camera.open(0)` 抛出 `RuntimeException: Fail to connect to camera service`，`camera1OpenConnectable = false`。
2. **`E2_mediaCodecsHasMtkAndDolbyNoV4l2Leak = false`**：
   - `MediaCodecList` 中 `dolbyC2Count = 0`（真机为 2：`c2.dolby.ac4.decoder`、`c2.dolby.eac3.decoder`）；
   - 同时暴露了 3 个虚拟 V4L2 编解码器名 `c2.v4l2.avc.decoder`、`c2.v4l2.hevc.decoder`、`c2.v4l2.avc.encoder`（`v4l2LeakCount = 3`，真机为 0）。
3. **`E3_widevineVendorAndPropertiesAuthentic = false`**：
   - `MediaDrm(WIDEVINE_UUID)` 返回占位属性 `vendor="com.google.widevine.software.drm"`、`version="16"`、`systemId="0"`、`hdcpLevel="Unsecured"`、`maxNumberOfSessions="30"`，且 `algorithms`、`oemCryptoApiVersion`、`usageReportingSupport` 为空，与 Google Widevine CDM v19 标准属性不一致。

---

## 2. 根因

1. **`E1` 根因（`hal_camera_default` 缺少 `/dev/ashmem` SELinux 权限 + `mtk_hal_atms_service` 查詢被拒）**：
   - `engine/vendor/hardware/android.hardware.camera/src/camera_provider_service.cpp` 的 `Note13CameraSession` 在构造会话时调用 `createMetadataQueueFd()` 打开 `/dev/ashmem` 创建 request / result 两条 `AidlMessageQueue`（FMQ）。
   - 基底 vendor SELinux 策略未授予 `hal_camera_default` 对 `ashmem_device` 的访问权限（对比 `android.hardware.audio` 已在模块 `sepolicy/vendor_sepolicy.cil` 补充该权限），导致 `open("/dev/ashmem")` 返回 `EACCES (13)`，`session->isReady()` 为 `false`，`Note13CameraDevice::open()` 返回 `INTERNAL_ERROR`，`cameraserver` 报 `Could not open camera session: Function not implemented (-38)`。
   - 此外 `cameraserver` 在打开相机时查询 `vendor.mediatek.hardware.camera.atms.IATMs/default`（`mtk_hal_atms_service`）被 SELinux 拒绝 `find`，导致 `ServiceManagerCppClient` 每次重试等待 5 秒。
2. **`E2` 根因（`IComponentStore/default9` 未映射 `hal_codec2_service` + `media_codecs_c2.xml` 冗余声明 `c2.v4l2.*`）**：
   - 平台侧 `/system/etc/selinux/plat_service_contexts` 仅登记了 `IComponentStore` 的 `default`、`default1`、`default2`、`software` 四个实例，未登记 Dolby C2 store 使用的 `default9` 实例。真机在 `/vendor/etc/selinux/vendor_service_contexts` 中登记了 `android.hardware.media.c2.IComponentStore/default9 u:object_r:hal_codec2_service:s0`，而云机 `android.hardware.media` 模块缺少 `sepolicy/vendor_service_contexts`，导致 `default9` 落入 `default_android_service`，`mediaserver` 因 `avc: denied { find }` 无法访问 `default9`，`c2.dolby.*` 从 `MediaCodecList` 消失。
   - `gold-android15.json` 的静态树 `etc/media_codecs_c2.xml` 额外追加了 3 条 `<MediaCodec name="c2.v4l2.*">`，使得 `Codec2InfoBuilder` 将内部 V4L2 组件名直接暴露到公开 `MediaCodecList`。
3. **`E3` 根因（`drm_widevine_service.cpp` 硬编码旧版占位常量）**：
   - `engine/vendor/hardware/android.hardware.drm/src/drm_widevine_service.cpp` 中 `kVendor`、`kVersion`、`kSystemId`、`kMaxSessions` 及 `getPropertyString` 仍使用早期占位值，未返回标准 Widevine CDM v19 属性集。

---

## 3. 证据

详见同目录 [`log-fail.txt`](./log-fail.txt) 与 [`log-pass.txt`](./log-pass.txt)：

- **修复前（`log-fail.txt`，`run-00223`）**：
  - `cdh_note13_camera_provider: open(/dev/ashmem) for cdh-camera-request-fmq failed: 13`
  - `avc: denied { open } for pid=542 comm="android.hardwar" path="/dev/ashmem" scontext=u:r:hal_camera_default:s0 tcontext=u:object_r:ashmem_device:s0 tclass=chr_file permissive=0`
  - `SELinux : avc: denied { find } for pid=666 uid=1013 name=android.hardware.media.c2.IComponentStore/default9 scontext=u:r:mediaserver:s0 tcontext=u:object_r:default_android_service:s0 tclass=service_manager permissive=0`
  - `E1=false, E2=false, E3=false`（总分 `22 / 32`）。
- **修复后（`log-pass.txt`，`run-00226` / `run-00227`）**：
  - `camera1OpenConnectable: true`（`cam_0` `openSuccess: true, paramCount: 61`；`cam_1` `openSuccess: true`）
  - `dolbyC2Count: 2`（`["c2.dolby.ac4.decoder", "c2.dolby.eac3.decoder"]`），`v4l2LeakCount: 0`
  - `widevine`: `vendor="com.google.android.widevine"`, `version="19.0.1@AV1A.240725.002"`, `systemId="33098"`, `oemCryptoApiVersion="18"`, `algorithms="AES/CBC/NoPadding,HmacSHA256"`, `hdcpLevel="Unprotected"`, `maxHdcpLevel="Unprotected"`, `maxNumberOfSessions="16"`, `usageReportingSupport="True"`, `securityLevel="L1"`
  - `E1=true, E2=true, E3=true, E4=true`（总分 **`25 / 32`**）。

---

## 4. 改动

1. **Camera HAL FMQ 共享内存授权与 `memfd_create` 兜底（`E1`）**：
   - 新增 `engine/vendor/hardware/android.hardware.camera/sepolicy/vendor_sepolicy.cil`：授予 `hal_camera_default` 对 `ashmem_device_{{vers}}` 的 `ioctl read write getattr map open` 权限及与 `cameraserver_{{vers}}` 的 `fd (use)` 权限。
   - 修改 `engine/vendor/hardware/android.hardware.camera/src/camera_provider_service.cpp`：`createMetadataQueueFd()` 在 `/dev/ashmem` 不可用时自动回退 `syscall(__NR_memfd_create, name, MFD_CLOEXEC)` + `ftruncate`。
   - 修改 `devices/manifest/configs/redmi/gold/gold-android15.json`：在 `sepolicy/vendor_sepolicy.cil` 增加 `(allow cameraserver_{{vers}} mtk_hal_atms_service (service_manager (find)))`。
2. **Dolby C2 `default9` 服务上下文映射与 `c2.mtk.* -> c2.v4l2.*` 桥接（`E2`）**：
   - 新增 `engine/vendor/hardware/android.hardware.media/sepolicy/vendor_service_contexts`：映射 `android.hardware.media.c2.IComponentStore/default9 u:object_r:hal_codec2_service:s0`。
   - 修改 `engine/vendor/hardware/android.hardware.media/sepolicy/vendor_sepolicy.cil`：授予 `mediacodec` 对 `hal_codec2_service_{{vers}}` 的 `add find` 及域内 `binder (call transfer)`。
   - 修改 `engine/vendor/hardware/android.hardware.media/src/codec2_store_service.cpp`：`createComponent` 与 `getPoolClientManager` 支持将 `c2.mtk.{avc,hevc,vpx,vp9}.decoder` / `c2.mtk.avc.encoder` 转发至 `IComponentStore/default`（`v4l2_codec2` 对应组件），保证 `v4l2_codec2` 服务在线可用且无需在 `media_codecs_c2.xml` 暴露 `c2.v4l2.*` 名称。
   - 修改 `devices/manifest/configs/redmi/gold/gold-android15.json`：移除 `etc/media_codecs_c2.xml` 中 3 条 `c2.v4l2.*` 块，恢复与真机一致的编解码器声明表。
3. **Widevine CDM v19 标准属性集对齐（`E3`）**：
   - 修改 `engine/vendor/hardware/android.hardware.drm/src/drm_widevine_service.cpp`：更新默认属性常量与 `getPropertyString()`，支持 `vendor`、`version`、`systemId`、`oemCryptoApiVersion`、`algorithms`、`usageReportingSupport`、`hdcpLevel`、`maxHdcpLevel`、`maxNumberOfSessions` 标准返回及 `ro.vendor.drm.widevine.*` 属性覆盖。

---

## 5. 验证

1. 重新执行 `cdh device build gold --manifest 15-OS3.0.10.0.VNQCNXM --only build_vendor / build_patch / pack_vda`（`patch.img` sha256 `4d830eba40e9`，`vda-344033f7c706`），重启云机实例 `1`（保留 `writable-base.qcow2` 与 `writable-data.qcow2`）。
2. 双机运行 `device-checker`（`run-00226` / `run-00227`）：
   - 真机 `ec519c7d513a3ad3`：`32 / 32` 通过（`E1=True, E2=True, E3=True, E4=True`）。
   - 云机 `df809591a4e085ae-cloud`：**`25 / 32`** 通过（`E1=True, E2=True, E3=True, E4=True`，维度 E 四项 100% 绿灯）。

---

## 6. 遗留

- 维度 A、E、G 已全部 100% 绿灯；剩余 7 项待对齐：`D3`（传感器 ADC 步进量化与陀螺仪微漂移）、`F2`（`/sys/class/thermal/thermal_zone*` 温区节点）、`B5`（`/proc/self/maps` Mesa/Gallium 渲染库路径收敛）、`C4`/`C5`/`C2`（GLES GLSL 3.20 / WebView WebGL / 120Hz 模式）及 `H1`（系统服务与 IFAA）。
