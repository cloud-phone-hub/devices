# #346 Blackview XPLORE_X1 桌面动态壁纸半屏荧光绿/错色断层与 V4L2 视频硬编解码全链路修复

- **编号**：#346
- **设备/机型**：`blackview/XPLORE_X1`（`15-AP3A.240905.015.A2-20251126`，Doke OS 5 / MT6877 / `lineage-22.2` + `kernel 6.6`，`1080x2460`）
- **日期**：2026-11-26

---

## 一、问题

`Blackview Xplore 1 #1`（ADB `127.0.0.1:5911`）在完成 `prepare` 设备信息对齐后，桌面与视频编解码链路存在三项直观异常及一项渲染热路径隐患：

1. **桌面动态壁纸半屏荧光绿与水平断层**：前景图标与状态栏正常，背景动态壁纸（`/system/etc/VideoWallpaperScene01.mp4`，`1080x2460` H.264 60fps 深宝石蓝几何水晶）上半屏错色发红橙（均值 `RGB=(84.0, 54.0, 47.3)`），屏幕正中 `y=1230` 出现刀切状水平硬断层，下半屏爆出刺眼荧光亮绿（`RGB=(45.6, 201.2, 51.9)`，荧光绿像素占比达 `30.63%`）。
2. **竖屏录屏与相机视频编码降级/卡死**：
   - 竖屏 `1080x2460` 运行 `screenrecord` 无法命中 V4L2 硬件编码器，回退软编 `c2.android.avc.encoder`，3 秒仅产出 3 帧（约 1 fps）；
   - 原生相机 `com.mediatek.camera` 录像因缺少硬编码器 FHD 能力声明，被迫降级至 `640x480`（VGA）软编录制。
3. **V4L2 设备节点无权限与顶层 XML 悬空阻断**：`/dev/video0`（解码）与 `/dev/video1`（编码）为 `0600 root:root`，`mediacodec` 域 `c2-v4l2` 服务无法打开；官方 `/vendor/etc/media_codecs.xml` 顶部悬空引用不存在的 `media_codecs_mediatek_audio.xml`，导致 `MediaCodecsXmlParser` 提前中止。
4. **渲染热路径 `/proc/ged` 刷屏**：旧 `vendor_dlkm` 模块早于 `#345`，`logcat` 持续刷出 `Failed to open /proc/ged` 与 `[onQueue]: ged queue fail`。

---

## 二、根因

1. **旧载体 `patch.img` 缺失 `YV12` 半宽步幅修正与 V4L2 权限**：
   - 运行目录 `out/devices/dist/blackview/XPLORE_X1/15-AP3A.240905.015.A2-20251126/patch.img` 仍停留在 `2026-11-23`，未包含 `#328`（[../../common/issue-virgl-yv12-plane-layout-and-v4l2-hevc-wallpaper/solution.md](../../common/issue-virgl-yv12-plane-layout-and-v4l2-hevc-wallpaper/solution.md)）的 `minigbm` `DRM_FORMAT_YVU420_ANDROID` 半宽色度步幅修正（`1080` 宽对应 `yStride=1088`、`cStride=544`）及 `VIRGL_CCMD_PIPE_RESOURCE_SET_TYPE` 同步，宿主按整宽误读 UV 平面导致上半屏错色、`y=1230` 断层、下半屏读全 `0x00` 爆荧光绿；
   - 旧 `/vendor/etc/ueventd.rc` 未配置 `/dev/video* 0660 media camera`。
2. **官方顶层 `/vendor/etc/media_codecs.xml` 悬空 `<Include>` 截断解析**：
   - MTK 官方 `etc/media_codecs.xml` 首行写有 `<Include href="media_codecs_mediatek_audio.xml" />` 与 `<Include href="media_codecs_google_audio.xml" />`，但原厂 `/vendor/etc/` 下根本不存在这两个文件。AOSP `MediaCodecsXmlParser::parseTopLevelXMLFile` 在首个缺失 `Include` 处直接返回 `NO_INIT`，导致追加在末尾的 `<Include href="media_codecs_c2.xml" />` 整表不生效。
3. **`02_prepare` / `09_build_vendor` 编解码声明与 `media_config.xml` 优先级缺陷**：
   - `02_prepare` 此前生成静态树 `etc/media_codecs_c2.xml` 时用 31 行 `c2.v4l2.*` 空壳覆盖了原厂 544 行文件，丢失全部 21 个 `c2.mtk.*` 硬编解码器能力声明（同时原厂将 `c2.android.avc.encoder` 限制为 `1808x1808`）；
   - `c2.v4l2.avc.encoder` 此前上限写死 `max="1920x1088"` 且无 `can-swap-width-height`（而宿主 `VideoToolbox` 实际支持 `4096x4096`），无法匹配竖屏 `1080x2460` 录屏；
   - `buildMediaConfigXML` 此前将假 store 组件设为 `rank="1"`（Codec2 `rank` 越小优先级越高，`c2.v4l2.*` 为 `128`，软编解码为 `256/512`），且 `codec2_store_service.cpp` 将 `video/hevc` encoder 误映射到不存在的 `"c2.v4l2.hevc.encoder"`，一旦恢复 `media_codecs_c2.xml` 会抢占无后端格式导致编解码失败。

---

## 三、证据

- **修前取证日志与实屏截图**：[log-fail.txt](log-fail.txt)、[home-fail.png](home-fail.png)（上半屏红橙错色、`y=1230` 断层、下半屏 `30.63%` 荧光绿，`/dev/video*` `0600 root:root`，`screenrecord` 3s 仅 3 帧，相机降级 `640x480`）。
- **修后验证日志与实屏/抽帧截图**：[log-pass.txt](log-pass.txt)、[home-pass.png](home-pass.png)（桌面深宝石蓝几何水晶完整还原）、[screenrecord-pass.png](screenrecord-pass.png)（`1080x2460` 58.6fps 硬编录屏抽帧）、[camera-video-pass.png](camera-video-pass.png)（原生相机 FHD `1920x1080` 硬编录像抽帧）。

---

## 四、改动

严守 `AGENTS.md` §7（不使用系统 `/tmp`）与 §8（不触碰官方 `super.img`/`system.img`），改动涵盖通用管线/HAL 与 `XPLORE_X1` 清单：

1. **Codec2 Store 桥接修正**：
   - [codec2_store_service.cpp](../../../../hardware/android.hardware.media/src/codec2_store_service.cpp)：移除 `video/hevc` encoder 到不存在的 `"c2.v4l2.hevc.encoder"` 的错误转发；
   - [vendor_sepolicy.cil](../../../../hardware/android.hardware.media.v412/sepolicy/vendor_sepolicy.cil)：补充 `mediacodec` 读取可选 `default_prop` 的 `dontaudit` 规则。
2. **`02_prepare` 与 `09_build_vendor` 编解码配置自动对齐增强**：
   - `agent/modules/pipeline/stages/02_prepare/`（`ai_profile.go`、`firmware_measured_extra.go`、`apply_profile_configs.go`、`media_codecs_c2_align.go`、`vendor_etc_templates.go`、`gold_reference.go`、`media_codecs_c2_align_test.go`）：提取原厂 `etc/media_codecs_c2.xml` 全文并与 `c2.v4l2.*` 幂等合并；将 `c2.v4l2.avc.encoder` 升级至 `max="4096x4096"` + `measured-frame-rate-*` + `can-swap-width-height`；将 `buildMediaConfigXML` 的 `rank` 修正为 `512`（有 V4L2 桥接）与 `1024`（无后端登记组件）；
   - `agent/modules/pipeline/stages/09_build_vendor/`（`media_codecs_c2.go`、`media_codecs_c2_test.go`）：新增 `pruneDanglingMediaCodecsIncludes` 自动剥除顶层 `vendor/etc/media_codecs.xml` 中不存在的同目录悬空 `<Include>`，幂等补入/升级 `c2.v4l2.*` 的 `4096x4096`、`measured-frame-rate-*`、`can-swap-width-height` 与 `ueventd.rc` `/dev/video* 0660 media camera` 规则。
3. **`XPLORE_X1` 清单与载体镜像刷新**：
   - [media_codecs_c2.xml](../../../../manifest/configs/blackview/XPLORE_X1/XPLORE_X1-android15/vendor/etc/media_codecs_c2.xml)：合并原厂 21 个 `c2.mtk.*` 组件声明与 3 个 `4096x4096` `c2.v4l2.*` 硬件编解码声明；
   - [media_config.xml](../../../../manifest/configs/blackview/XPLORE_X1/XPLORE_X1-android15/vendor/etc/media_config.xml)：更新 22 个组件 `rank` 为 `512`/`1024`；
   - [XPLORE_X1.json](../../../../manifest/devices/blackview/XPLORE_X1.json) 及 `dist/.../device.json`：补齐 `"host-hvc-camera": "hvc12"` 与 `"host-hvc-nfc": "hvc11"`；
   - 重编 `6.6` 内核模块（含 `#345` [../../x6876/issue-x6876-subsystems-audit-and-jank-fix/solution.md](../../x6876/issue-x6876-subsystems-audit-and-jank-fix/solution.md) 的 `/proc/ged`）与 Android 15 HAL（`mapper.mediatek.so`、`android.hardware.media`），结合 `vendor_boot/fstab.mt6877` 合成新版 `patch.img`（`384 MiB`，`sha256 3ee1160e3616`，973 条目）部署至 `out/devices/dist/blackview/XPLORE_X1/15-AP3A.240905.015.A2-20251126/patch.img`。

---

## 五、验证

通过本机守护进程重启 `Blackview Xplore 1 #1`（ADB `127.0.0.1:5911`）实机全维度验收（详见 [log-pass.txt](log-pass.txt)）：

| 检查项 | 修前 | 修后 |
| :--- | :--- | :--- |
| 冷启动 / SELinux | 旧载体镜像 | `48s` `sys.boot_completed=1` / `Enforcing`，`am_crash=0` |
| `/dev/video0..1` 与 `/proc/ged` | `0600 root:root`；`/proc/ged` 缺失刷屏 | `0660 media:camera`；`/proc/ged` 在位，`ged_open_fail=0` |
| 桌面 `1080x2460` 动态壁纸解码 | 上半屏红橙，`y=1230` 断层，下半屏 `30.63%` 荧光绿 | 命中 `c2.v4l2.avc.decoder` 硬解，深宝石蓝还原，断层跳变 `13.6 -> 0.6`，荧光绿 **`0.00%`**（[home-pass.png](home-pass.png)） |
| 运行态 `MediaCodecList` | 缺硬编解码尺寸声明 | 共 `97` 个组件：`c2.v4l2.*` `3` 个（`1080x2460@60=true`）+ 原厂 `c2.mtk.*` `21` 个 |
| 竖屏 `1080x2460` `screenrecord` | 软编 `c2.android.avc.encoder`，3s 仅 3 帧 | 命中硬编 `c2.v4l2.avc.encoder`，4s 产出 `91` 帧（`58.56 fps`，`8.48 Mbps`，[screenrecord-pass.png](screenrecord-pass.png)） |
| 原生相机 `com.mediatek.camera` 录像 | 降级 `640x480` 软编 | 自动启用 **FHD `1920x1080`**，命中硬编 `c2.v4l2.avc.encoder` 录出 305 帧高清彩条视频（[camera-video-pass.png](camera-video-pass.png)） |
| 管线单测 | — | `stages/02_prepare` 与 `stages/09_build_vendor` 全绿通过 |

---

## 六、遗留

无。若在远端运行节点存在基于旧 `patch.img` 的存量实例，增量同步新版 `patch.img` 与 `device.json` 后重启实例即可生效。
