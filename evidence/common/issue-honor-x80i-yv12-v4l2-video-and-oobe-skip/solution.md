# #340 闭环 `Honor X80i 5G` 欢迎页 `YV12`/V4L2 视频半屏绿屏、`/dev/video*` 权限缺失与荣耀 `HnStartupGuide` 开机设置引导自动跳过

- **编号**：#340
- **设备 / 分组**：`common`（跨机型构建管线 `09_build_vendor` `/dev/video*` `ueventd` 节点权限兜底、`android.service.daemon` `cph_disable_provision.rc` 抢跑竞态消除与荣耀/华为 `HnStartupGuide` 开机引导自动跳过，同步覆盖 `honor/{HNLNA-M,sun,chora}` 并实机闭环 `Honor X80i 5G #1` `16-TP1A.220624.014-20260422`）
- **日期**：2026-11-26
- **状态**：**已闭环**（`Honor X80i 5G #1` 零干预冷启 `38.8s` 直达 `UniHomeLauncher` 桌面 + 欢迎页 `1080×1080` H.264 视频经 `c2.v4l2.avc.decoder` 硬解零报错还原暖白渐变 + `09_build_vendor` 全包单测全绿）

---

## 1. 问题

在线云机实例 **`Honor X80i 5G #1`**（`honor/HNLNA-M`，`LNA-AN10`，MT6835，GRF `vendor=13 / system=16`，基线 `16-TP1A.220624.014-20260422`）开机后暴露两项串联问题：

1. **欢迎页下半屏刺眼亮绿失真（`y = 1200` 水平硬断层）**：
   - 停留在 MagicOS 开机向导首页（`MagicOS` + 手写 `hello` + 蓝色圆形箭头按钮）时，前景文字与按钮图层正常，但从屏幕正中 `y = 1200`（`2400 / 2`）往下整片背景变成荧光亮绿 `RGB ≈ [45, 255, 0]`（见 [hello-fail.png](hello-fail.png)）。
2. **开机设置引导（`com.hihonor.hnstartupguide`）未自动跳过**：
   - 实例完成启动后 `device_provisioned=0`、`user_setup_complete=0`，始终停留在 `com.hihonor.hnstartupguide`（点击下一步后进入 `EmbeddingActivity` 并拉起荣耀账号短信登录页 `com.hihonor.id/.../LoginBySMSActivity`），无法自动进入 `UniHomeLauncher` 桌面。

---

## 2. 根因

经客机 `SurfaceFlinger` 图层/缓冲剖析、`dumpsys settings` 写入溯源、`jadx` 反编译 `HnStartupGuide.apk` 及 V4L2/Minigbm 链路逐层对账，定位出四层根因：

### 2.1 视频半屏绿屏：`patch.img` 未同步 `#328` `YV12` 分配器修复 + `/vendor/etc/ueventd.rc` 缺失 `/dev/video*` 权限致 `c2.v4l2.avc.decoder` 打开失败

1. **欢迎页背景实为 `1080×1080` H.264 视频纹理**：
   - 反编译 `HnStartupGuide.apk` 的 `NewWelcomePage`（`com.hihonor.hnstartupguide.page.g3`）证实：首页通过 `android.media.MediaPlayer` 播放内置 `res/GJ.mp4`（`1080×1080` H.264 High Profile `yuv420p` 30fps 暖白渐变动画）渲染至全屏 `TextureView (0,0-1080,2400)`。
2. **存量 `patch.img`（`2026-11-22` 构建）缺失 `YV12` 步幅同步与 `media_codecs_c2.xml`**：
   - `HNLNA-M` 属远端同步盘集，其 `patch.img` 早于 [../issue-virgl-yv12-plane-layout-and-v4l2-hevc-wallpaper/](../issue-virgl-yv12-plane-layout-and-v4l2-hevc-wallpaper/)（#328）：既缺少 `/vendor/etc/media_codecs_c2.xml`（及顶层 `media_codecs.xml` 的 `<Include href="media_codecs_c2.xml" />`）使 `MediaPlayer` 回退至软解 `c2.android.avc.decoder`，其 `/vendor/bin/hw/android.hardware.graphics.allocator-V1-service.minigbm` 与 `/vendor/lib64/hw/mapper.mediatek.so` 也未包含 `cdh_fixup_yv12_bo_layout` / `VIRGL_CCMD_PIPE_RESOURCE_SET_TYPE` 显式三平面布局同步；
   - `1080` 按 32 对齐为 `1088` 后，旧 `minigbm` 将 `YV12` 色度步幅错设为全宽 `1088` 且宿主 `res->yuv_width == 0`，导致下半屏色度采样落入未初始化的全 `0x00` 区域（`Cb = -128, Cr = -128` 叠加白底 `Y ≈ 245`，经 YUV→RGB 转换恰为荧光绿 `[45, 255, 0]`）。
3. **`lineage-20.0` 基底及荣耀独立 `/vendor/etc/ueventd.rc` 缺失 `/dev/video*` 节点授权（跨机型缺口）**：
   - 补齐 `media_codecs_c2.xml` 后，`CCodec` 虽选中硬解 `c2.v4l2.avc.decoder`，但日志报 `E V4L2Decoder: Failed to open device for H264`；
   - 核查客机 `/dev/video0` 与 `/dev/video1` 发现权限为 `crw------- 1 root root`（`0600`）：AOSP `/system/etc/ueventd.rc` 不含 `/dev/video*`，`lineage-20.0`（Android 13 基底）与荣耀 `sun`/`chora` 手写覆盖的独立 `/vendor/etc/ueventd.rc` 均未配置 `/dev/video* 0660 media camera`，导致以 `user media (uid=1013)` 运行的 `android.hardware.media.c2@1.2-service-v4l2-64` 在启动枚举时因 `EACCES` 无法打开 `virtio-media` 设备。

### 2.2 设置引导未跳过：`cph_disable_provision.rc` 漏写 `disabled` 抢跑竞态 + `HnStartupGuide` 反写标志位为 `0`

1. **`vendor.auto_provision` 漏写 `disabled` 引发 `class_start main` 抢跑**：
   - `devices/hardware/android.service.daemon/vendor/etc/init/cph_disable_provision.rc` 中的 `vendor.auto_provision` 与 `vendor.keep_awake` 归属 `class main` 却未加 `disabled`（同目录 `cph_storage_align.rc`、`cph_wifi_bootstrap.rc` 均带 `disabled`）；
   - `qemu.log` 证实 `init` 在第 `33.6s`（`system_server`/`SettingsProvider` 尚未就绪）即提前拉起 `vendor.auto_provision`，空耗 `27.5s` 重试后在 `SettingsProvider` 刚初始化的瞬间写入设置，与 AMS 首启拉起 `HOME` 向导正面撞车。
2. **荣耀 `HnStartupGuide` 存活期间主动将 `device_provisioned` 与 `user_setup_complete` 覆写回 `0`**：
   - `dumpsys settings` 实测：`device_provisioned`（`generation:3`）与 `user_setup_complete`（`generation:2`）的最终写入者均为 `pkg:com.hihonor.hnstartupguide value:0`；
   - 反编译 `HnStartupGuide.apk` 的 `BaseActivity.onCreate -> n2.d0.Y1(context, false)` 与 `EmbeddingActivity` 证实：向导任一页面创建时若检测到未处于双 `1` 稳态即调用 `X1(context, 0)` 将两项重置为 `0`；且一旦跳转进入 `EmbeddingActivity`（或拉起 `com.hihonor.id` OOBE 登录页），不再注册 `mCloseOOBEObserver` 监听 `device_provisioned` 自动退出。

---

## 3. 证据

- **修复前失败现场**（详见 [log-fail.txt](log-fail.txt) 与 [hello-fail.png](hello-fail.png)）：
  - 像素实测：`top(300,100)=[234, 247, 251]`，`mid1(1195,100)=[255, 248, 255]`，`mid2(1205,100)=[50, 255, 0]`，`bot(1800,100)=[45, 255, 0]`（`y=1200` 处突变超 `250`）；
  - 编解码与节点：`/vendor/etc/media_codecs_c2.xml` 不存在，回退 `[c2.android.avc.decoder]`；补齐 XML 但未修 `ueventd.rc` 时 `/dev/video0..1` 为 `0600 root:root`，报 `E V4L2Decoder: Failed to open device for H264`；
  - 引导与设置：`vendor.auto_provision (pid 819)` 在 `61.13s`（早于 `74.01s` 的 `sys.boot_completed=1`）提前退出，`dumpsys settings` 记录 `_id:296 name:device_provisioned pkg:com.hihonor.hnstartupguide value:0`、`_id:261 name:user_setup_complete pkg:com.hihonor.hnstartupguide value:0`，前台卡在 `com.hihonor.hnstartupguide` / `com.hihonor.id`。
- **修复后通过现场**（详见 [log-pass.txt](log-pass.txt)、欢迎页硬解验证图 [hello-pass.png](hello-pass.png) 与冷启直达桌面图 [home-pass.png](home-pass.png)）：
  - **零干预冷启自动跳过引导**：冷启 `38.8s` 完成 `sys.boot_completed=1` 与 `vendor.auto_provision=stopped`，自动进入 `com.hihonor.android.launcher/.unihome.UniHomeLauncher`，`device_provisioned=1`、`user_setup_complete=1`、`oobe_statement_agreed=1`、`oobe_process_alive_state=0`、`buildversionrelease=LNA-AN10 10.0.0.126(C00E122)`，`com.hihonor.hnstartupguide` 已禁用；
  - **V4L2 硬件解码与欢迎页渲染复核**：开机 `/dev/video0` 与 `/dev/video1` 自动就位 `crw-rw---- 1 media camera u:object_r:video_device:s0`；拉起欢迎页实测 `CCodec` 创建 `[c2.v4l2.avc.decoder]` -> `V4L2Decoder: Set state Idle => Decoding` 硬解 `1080×1080` 视频零报错，全屏还原原片暖白柔光（`top=[252, 241, 223]`、`mid1=[254, 252, 253]`、`mid2=[254, 252, 253]`、`bot=[250, 246, 255]`），`y=1199->1200` 行差降至 **`0.04`**，半屏亮绿彻底消失。

---

## 4. 改动

1. **修复 `auto_provision` 抢跑竞态并支持荣耀/华为开机向导自动跳过**：
   - `devices/hardware/android.service.daemon/vendor/etc/init/cph_disable_provision.rc`：为 `vendor.auto_provision` 与 `vendor.keep_awake` 补齐 `disabled`，确保仅在 `on property:sys.boot_completed=1` 系统服务就绪后触发；
   - `devices/hardware/android.service.daemon/vendor/bin/cph-setup-display`：新增 `com.hihonor.hnstartupguide` / `com.huawei.hwstartupguide` 处理分支——先 `am force-stop` + `pm disable` 向导包（首启或向导存活时同步清理被 OOBE 拉起的 `com.hihonor.id` / `com.huawei.hwid`），再对齐 `HnStartupGuide.finishWizard()` 标准标志位（`user_setup_complete=1`、`device_provisioned=1`、`oobe_statement_agreed=1`、`oobe_process_alive_state=0`、`ota_service_updated=0`、`buildversionrelease`）并拉起 `CATEGORY_HOME` 桌面；同步刷新 `out/devices/vendor/android{13..17}/`。
2. **构建管线 `09_build_vendor` 幂等补齐 `/dev/video*` 与 `/dev/media*` `ueventd` 权限**：
   - `agent/modules/pipeline/stages/09_build_vendor/media_codecs_c2.go`：新增 `ensureVendorUeventdV4L2Nodes()`，当装配树启用 `v4l2` Codec2 服务且独立 `vendor/etc/ueventd.rc`（及其 `import` 分件）未配置 `/dev/video` 规则时，幂等补入 `/dev/video* 0660 media camera` 与 `/dev/media* 0660 media camera`；
   - `agent/modules/pipeline/stages/09_build_vendor/media_codecs_c2_test.go`：同目录新增 `TestAlignVendorMediaCodecsC2_EnsuresUeventdVideoNodes` 覆盖独立 `ueventd.rc` 注入、二次运行幂等性及含 `ueventd.virt.rc` `import` 时跳过；重编更新 `out/agent/dist/pipeline`；
   - `devices/manifest/configs/honor/{HNLNA-M/HNLNA-M-android13,sun/sun-android15,chora/chora-android16}/vendor/etc/ueventd.rc`：同步落盘 `/dev/video*` 与 `/dev/media*` `0660 media:camera` 规则。
3. **`Honor X80i 5G`（`HNLNA-M`）`patch.img` 原位无损刷新**（严守 `AGENTS.md §8` 不触碰 `super.img`）：
   - 将 Android 13 最新自编 `allocator-V1-service.minigbm`、`mapper.mediatek.so`、`gralloc.default.so`、`media_codecs_c2.xml`、含 `<Include href="media_codecs_c2.xml" />` 的 `media_codecs.xml`、`ueventd.rc`、`cph-setup-display` 与 `cph_disable_provision.rc` 写入 `out/devices/dist/honor/HNLNA-M/16-TP1A.220624.014-20260422/patch.img`（`sha256: 63e320d4bac9f353 -> 1234513afa1ececd`），保留全部权限位与 `security.selinux` 扩展属性并通过 `e2fsck -fn` 零错校验。

---

## 5. 验证

1. **单元测试与语法门禁**：
   - `sh -n devices/hardware/android.service.daemon/vendor/bin/cph-setup-display` 通过；
   - `cd agent/modules/pipeline && go test ./stages/09_build_vendor` 全包通过（`1.337s`）。
2. **`Honor X80i 5G #1` 实机冷启与双项回归验证**：
   - 实例重启后 `38.8s` 自动跳过设置引导停在 `UniHomeLauncher` 桌面，各项 provisioning 标志位全部为预期稳态值；
   - `/dev/video0` 与 `/dev/video1` 开机自动呈现 `0660 media:camera`；临时拉起欢迎页验证 `c2.v4l2.avc.decoder` 硬解 `1080×1080` 视频无绿屏无断层（`y=1199->1200` 跳变 `0.04`），验证完毕后恢复禁用 `com.hihonor.hnstartupguide` 并保持实例运行在干净桌面态。

---

## 6. 遗留

- 其他尚未重打 `patch.img` 的 `lineage-20.0`（Android 13 基底）远端同步盘集在下次执行 `09_build_vendor -> 11_build_patch` 时，将自动继承本次 `ensureVendorUeventdV4L2Nodes`、`cph-setup-display` 与 `cph_disable_provision.rc` 的全部修复。
- `HNLNA-M` 当前尚未在 `hardware/config.json` 启用 `android.hardware.biometrics`（`fingerprint-only`），背景仍有少量荣耀指纹扩展探测日志；后续在 `rom-main` 在线重打整机 `vendor_sepolicy.cil` 时可按 [../issue-honor-sun-lag-fingerprint-motion-and-idle-camera/](../issue-honor-sun-lag-fingerprint-motion-and-idle-camera/) 同构接入以进一步压降待机 CPU。
