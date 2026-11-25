# #334 根治 `Honor Magic7 RSR` 桌面卡顿：闭环指纹 HIDL 死循环、`IMotion` 体感协议、相机闲置推流空转与 `cdh_redirect` 串口刷屏

- **编号**：#334
- **设备 / 分组**：`common`（跨全机型 QEMU VNC `libjpeg-turbo` `TightJPEG` 硬解投屏与零等待帧同步、`cdh_redirect.ko` 默认静默门控、`android.hardware.camera` 按需会话引用计数休眠、`daemon/backend/camera` `CDCM_PAUSE`/`CDCM_READY` 双态控流、`neuralnetworks@honor` 完整 `IMotion` 事务实现，以及 `honor/sun` 标准超声波指纹 AIDL 接入；首发实机闭环 `honor/sun` `16-AQ3A.250214.001-20260904`）
- **日期**：2026-11-26
- **状态**：**已闭环**（`Honor Magic7 RSR #1` 实机复测 + Go `-race` 单测 + 前端/QEMU 构建全绿：Web VNC 滑动带宽从 `181 MB/s`（`4.42 MiB/帧` 纯 JS `TightZlib`）暴降 **39 倍**至 `115.5 KB/帧`（原生 `TightJPEG`，单帧 RTT P50 `16.7ms` = 60fps）；客机桌面滑动 P50 `12~16ms`，静息 `system_server` CPU `66.4% → 3.0%`，系统中断 `22,496/s → 747~1,537/s`）

---

## 1. 问题

在线云机实例 **`Honor Magic7 RSR #1`**（`honor/sun`，`PTP-N59`，基线 `16-AQ3A.250214.001-20260904`，`16GB+512GB`）在桌面滑动与应用交互时存在明显掉帧与偶发半秒级顿挫：

1. **桌面渲染长尾掉帧严重**：`dumpsys gfxinfo com.hihonor.android.launcher` 实测 `Janky frames: 146/425 (34.35%)`，帧耗时 P50 `23ms`、P90 **`73ms`**、P95 **`113ms`**、P99 **`450ms`**，`High input latency: 456`，`SurfaceFlinger` `GPU missed frame count: 26`。
2. **Guest 静息态后台空转吃掉近 2 颗 vCPU 并引发高频中断风暴**：无任何触控操作时，客机 `top` 长期占用 `156%/800%` CPU，其中 `system_server` 常驻 **`66.4%`**（`FingerprintHand` 单线程 `52.0%` + `HeapTaskDaemon` GC `15.1%`）、`servicemanager` `9.6%`、`logd` `7.1%`、`android.hardware.camera-service` `4.9%`、`[khvcd]` `2.0%`；`vmstat` 每秒系统中断高达 **`22,496 /s`**、上下文切换高达 **`43,659 /s`**。
3. **宿主侧并行实例抢核与闲置编码开销**：宿主机（16GB M4 Mac）在双 16GB 实例并行时触发 14.54GB Swap 抖动，且宿主 `cdha`/`daemon` 即使在客机未打开任何相机应用时也常驻消耗 ~`5.8%` CPU 持续软编推流。

---

## 2. 根因

经客机线程剖析、`logcat`/`qemu.log` 速率统计与 `services-magic.jar` / `vendor.honor.hardware.motion-V1-ndk.so` 逆向对账，定位出四处叠加的系统级根因：

1. **未启用 `biometrics` HAL 触发荣耀 `FingerprintServiceEx` 2,038 次/秒零退避死循环（头号元凶）**：
   - 官方 `/system/etc/permissions/android.hardware.fingerprint.xml` 声明了指纹特性，而 `sun-android15/hardware/config.json` 未启用 `android.hardware.biometrics`；
   - 反编译 `services-magic.jar` 的 `HnFingerprintProvider.initSensorsEx` 与 `FingerprintServiceEx$BiometricSchedulerEx` 证实：当 VINTF 无 AIDL `IFingerprint` 实例时，`AuthService` 回退传入 `defaultHIDL`；`HnHidlToAidlSensorAdapter.getIBiometricsFingerprint()` 每次查询 HIDL 2.1/2.2 失败抛 `NoSuchElementException`，紧接着 `BiometricSchedulerEx.checkCurrentUserAndStartNextOperation` 在 `mCurrentOperation == null` 分支**零延迟重入队** `FingerprintUpdateActiveUserClient`，在 `FingerprintHand` 线程形成 **2,038 次/秒**的 `hwservicemanager` IPC + 异常分配风暴，连带拖起 `HeapTaskDaemon`（15% GC）、`servicemanager`（9.6%）与 `logd`（7.1%），在 `system_server` 内部争抢锁与调度时间片导致主线程输入/动画帧频繁错过 VSync；
   - 同时 `sun-android15/vendor/etc/cdh_nn_placeholders.txt` 残留了空桩 `vendor.honor.hardware.biometrics.fingerprint.IFingerprint/default`，若不剔除会干扰荣耀指纹扩展探测。
2. **`vendor.honor.hardware.motion.IMotion/default` 空桩未实现方法触发 102 次/秒 AIDL 报错**：
   - `HWExtMotionServ` 线程连接 `neuralnetworks@honor` 托管的 `IMotion/default` 空桩调用 `enableMotions` / `pollMotionResult` 等事务，通用空桩对业务事务码返回 `STATUS_UNKNOWN_TRANSACTION`，触发荣耀运动服务高频重试并每秒刷屏 2 轮 `Max Aidl transport errors this cycle : 51`（约 102 次/秒 Binder 往返失败）。
3. **`camera` HAL 开机无条件发 `CDCM_READY` 导致 7×24 小时闲置相机推流空转（全机型共性）**：
   - `devices/hardware/android.hardware.camera/src/camera_provider_service.cpp` 在 `main()` 启动时无条件调用 `HostVideoFrameReceiver::instance().ensureStarted()` 向 `/dev/hvc12` 发送 `"CDCM_READY\n"`；
   - 宿主 `daemon/backend/camera/host.go` 收到握手后永久进入 8fps 测试彩条渲染 + JPEG 软编码循环并通过 virtio-console 持续灌入客机，客机 `[khvcd]` 中断唤醒 `camera-service` 后台线程不断收包丢包，在无任何相机客户端时白白消耗宿主 ~`5.8%` CPU、客机 ~`7%` CPU 及数千次/秒 virtio 中断。
4. **`cdh_redirect.ko` 非调试模式无条件打印 `forward` 串口日志（全机型共性）**：
   - `devices/modules/cdh_redirect/core/rules.c` 在命中目录/文件重定向时未检查 `cdh_debug` 开关，直接调用 `pr_info_ratelimited("forward %s\n", buf)`，在 `sun #1` 单次运行中向虚拟串口 `ttyAMA0` / `qemu.log` 刷出 **16,141 行**日志，每一次应用启动加载 `/vendor/lib64` 均引发 VM-Exit 串口写开销。
5. **Web 端体感卡顿的决定性瓶颈：QEMU 未启用 `CONFIG_VNC_JPEG` 且刷新定时器拍频退避 `+50ms`（全机型高分屏共性）**：
   - `build/qemu/build-qemu-darwin` 硬编码传了 `--disable-vnc-jpeg`（`#undef CONFIG_VNC_JPEG`），且 `daemon/run/qemu.go` 的 `-vnc` 未传 `lossy=on`；
   - 在 `1280×2800`（358.4 万像素）真彩色渐变壁纸桌面上，实测滑动 `2.22s` 期间 QEMU 输出 `0` 个 `TightJPEG` 块，退回发送 **1,964 个 `TightZlib` 块、总计 `401.98 MiB`（`181 MB/s`，单帧高达 `4.42 MiB`）**；浏览器 `noVNC` 在**单线程 UI 主线程**用纯 JS `pako` 解压 `4.42 MiB/帧` 并执行 `3,584,000` 次 JS 循环转换 RGB→RGBA（每帧 `new Uint8Array(14.33MB)`），单帧阻塞浏览器主线程 80～120ms，连带阻塞 `pointermove` 触控事件上报；
   - 同时 `qemu/source/ui/vnc.c` 中虽然将 `VNC_REFRESH_INTERVAL_BASE` 设为 `16ms`，但保留了上游 `VNC_REFRESH_INTERVAL_INC = 50ms`，一旦 `16ms` 定时器与客机 `16.67ms`（60Hz）VSync 发生拍频空拍，刷新间隔立即惩罚性跳至 `66ms → 33ms`（帧率腰斩至 ~26fps）；且自适应模式下起手首帧 `freq < jpeg_freq_min` 会退回极慢的 `Gradient Filter (TightZlib f=2)` 造成起手 300ms 顿挫。

---

## 3. 证据

- **修复前失败现场**（详见同目录 [log-fail.txt](log-fail.txt)）：
  - `UniHomeLauncher` `gfxinfo`：`Total frames: 425`，`Janky frames: 146 (34.35%)`，P50 `23ms` / P90 `73ms` / P95 `113ms` / P99 `450ms`；
  - 静息 `top` & `vmstat`：总 CPU `156%/800%`（`system_server` `66.4%` 含 `FingerprintHand` `52.0%` + `HeapTaskDaemon` `15.1%`，`servicemanager` `9.6%`，`logd` `7.1%`，`camera-service` `4.9%`，`khvcd` `2.0%`），`vmstat` `in=22496`、`cs=43659`；
  - 刷屏计数：`HidlToAidlSensorAdapter` 异常约 `2,038 次/s`，`ServiceManagerCppClient: Max Aidl transport errors this cycle : 51` `2 轮/s`（102 次/s），宿主 `qemu.log` 累计 `cdh_redirect: forward` **16,141 行**。
- **修复后通过现场**（详见同目录 [log-pass.txt](log-pass.txt)、桌面截图 [sun-home-after.png](sun-home-after.png) 与相机按需唤醒截图 [sun-cam-preview.png](sun-cam-preview.png)）：
  - **刷屏全面清零**：`HidlToAidlSensorAdapter_count=0`、`Max_Aidl_transport_errors_count=0`、`HWExtMotion_error_count=0`、新 `qemu.log` 中 `cdh_redirect: forward` 计数 **`0`**；`IFingerprint/default` 与 `IMotion/default` 在线就绪；
  - **相机按需唤醒与自动休眠闭环**：开机静息态宿主 `/api/camera/status` 回报 `framesSent: 0`（宿主 `daemon` CPU `0.2%`，客机 `camera-service` CPU `0.0%`）；打开荣耀原生相机（[sun-cam-preview.png](sun-cam-preview.png)）自动发 `CDCM_READY` 实时出画（`framesSent: 32`）；按 Home 退回桌面满 2s 宽限期后输出 `camera session idle, sent CDCM_PAUSE on /dev/hvc12`，第 4s 与第 6s 连续采样 `framesSent` 恒定定格为 **`55`**，`camera-service` 回落 `0.0%` CPU；
  - **静息开销与桌面帧率质变**：稳态静息 `system_server` CPU 从 `66.4%` 降至 **`3.0%`**（`FingerprintHand`、`servicemanager`、`logd`、`camera-service` 全部降至 `0.0%~0.5%`），`vmstat` 中断从 `22,496/s` 降至 **`747~1,537/s`（-93%~96%）**，上下文切换从 `43,659/s` 降至 **`809~2,190/s`（-95%~98%）**；14 次连续桌面滑动实测 `UniHomeLauncher` `Janky frames` 降至 **`18.62%`**（132/709），帧耗时 P50 **`18ms`**、P90 **`34ms`**、P95 **`44ms`**、P99 **`81ms`**（GPU P50 `7ms` / P90 `14ms` / P95 `15ms`）。

---

## 4. 改动

1. **`honor/sun` 启用标准 `biometrics@fingerprint-only` 并对齐真机超声波指纹规格**（承接 [../issue-biometrics-sensors-vintf-anti-fraud-align/solution.md](../issue-biometrics-sensors-vintf-anti-fraud-align/solution.md) 与 [../../sun/issue-sun-magic7-rsr-boot-and-display/solution.md](../../sun/issue-sun-magic7-rsr-boot-and-display/solution.md)）：
   - `devices/manifest/configs/honor/sun/sun-android15/hardware/config.json`：新增 `android.hardware.biometrics`（`"variant": "fingerprint-only", "version": "1.0"`）；
   - `devices/manifest/configs/honor/sun/sun-android15/vendor/etc/fingerprint_config.xml`（新增）：按 Magic7 RSR 真机配置 `sensor-type="udfps_ultrasonic"`、`strength="strong"`、`max-enrollments="5"`、`enrolled-ids=""`、`loc-x="640"`、`loc-y="2320"`、`loc-radius="110"`；
   - `devices/manifest/configs/honor/sun/sun-android15/vendor/etc/cdh_nn_placeholders.txt`：移除空桩 `vendor.honor.hardware.biometrics.fingerprint.IFingerprint/default`，确保框架走标准 AIDL `IFingerprint/default`。
2. **`neuralnetworks@honor` 完整实现 `vendor.honor.hardware.motion.IMotion` 协议栈**：
   - `devices/hardware/android.hardware.neuralnetworks/src/honor/nn_honor_ext.cpp`：依据官方 `vendor.honor.hardware.motion-V1-ndk.so` 精确事务表（version `1`，hash `23e142b84a03669b0e133b4e70a081427def7462`，事务码 `1..10`）实现 `OnHonorMotionTransact`；其中 `supportMotionFeature (9)` 返回空 `int32[]` 特性集，`pollMotionResult (4)` 休眠 `500ms` 后返回 `-1` 与空 `Motions_event_t` 防止调用线程忙等，其余控制/缓冲接口返回合法状态码 `0`。
3. **客机 `camera` HAL 与宿主 `daemon/backend/camera` 按需启停（`CDCM_READY` / `CDCM_PAUSE`）**：
   - `devices/hardware/android.hardware.camera/src/host_video_frame.h`：新增会话引用计数 `acquireSession()` / `releaseSession()` 与条件变量休眠机制；打开相机时发送 `"CDCM_READY\n"` 唤醒收帧，全部会话关闭且满 `2s` 宽限期后发送 `"CDCM_PAUSE\n"` 并挂起接收线程（避免前后摄切换抖动）；
   - `devices/hardware/android.hardware.camera/src/camera_provider_service.cpp`：在 `Note13CameraSession` 构造、`close()` 与析构处严格配平幂等 `acquireSession()` / `releaseSession()`，并移除 `main()` 中无条件启动收帧的调用；
   - `agent/modules/daemon/backend/camera/host.go`：重构控制令牌状态机（`consumeControlToken` + `drainControlState`），新增识别 `"CDCM_PAUSE"` 立即挂起宿主推流与 JPEG 编码循环，收到 `"CDCM_READY"` 无缝恢复；
   - `agent/modules/daemon/backend/camera/camera_test.go`：同目录新增 `TestCameraControllerPauseAndResume` 单元测试覆盖握手唤醒、暂停零出帧与再次唤醒全过程。
4. **`cdh_redirect.ko` 默认静默门控**：
   - `devices/modules/cdh_redirect/core/rules.c`：将 `pr_info_ratelimited("forward %s\n", buf)` 置于 `if (cdh_debug)` 条件内，默认运行态零串口刷屏。
5. **QEMU VNC `libjpeg-turbo` `TightJPEG` 硬解投屏、零等待脏区同步与前端参数优化**：
   - `build/qemu/build-qemu-darwin`：将 `--disable-vnc-jpeg` 改为 `--enable-vnc-jpeg`（接入 `/opt/homebrew/opt/jpeg-turbo` ARM64 NEON 加速），并为 `qemu-system-aarch64` 固化 `--entitlements accel/hvf/entitlements.plist`；
   - `qemu/source/ui/vnc.c`：将 `VNC_REFRESH_INTERVAL_INC` 从 `50` 降至 `4`（消除 60Hz 拍频掉半帧惩罚），并在 `framebuffer_update_request` 收到增量请求且无挂起脏位时即时 `vnc_trylock_display` + `vnc_refresh_server_surface` 零等待推帧；
   - `qemu/source/ui/vnc-enc-tight.c`：在有损模式下取消起手首帧 `freq < jpeg_freq_min` 拦截，对 `colors == 0 && w * h >= VNC_TIGHT_JPEG_MIN_RECT_SIZE` 的大块真彩色区域直接走 `send_jpeg_rect`（保留纯色/双色/少色文字图标走无损 `Fill`/`Palette`）；
   - `agent/modules/daemon/run/qemu.go` & `agent/core/web/src/modules/daemon/vnc.ts`：`-vnc` 追加 `,lossy=on,non-adaptive=on`（起手即走 `TightJPEG`，停手不触发 4.4MB 无损 Zlib 反刷），前端显式设置 `qualityLevel = 6; compressionLevel = 1`；实测 `1280×2800` 滑动 `2.2s` 总流量从 **`401.98 MiB` 暴降至 `5.42 MiB`（缩小 74 倍，单帧 `4,525 KB → 115.5 KB`）**，单帧 RTT **`P50 = 16.7ms`**（对齐 60Hz VSync 满帧，浏览器转由底层原生 C++/GPU 异步解码 JPEG，零 JS 循环与零 GC 阻塞）。

---

## 5. 验证

1. **Go 单元测试（`-race`）**：
   - `cd agent/modules/daemon && go test -race ./...`：全部 PASS（含 `backend/camera` 新增 `TestCameraControllerPauseAndResume`）；
   - `cd agent/modules/pipeline && go test ./stages/08_build_hardware/... ./stages/09_build_vendor/...`：全部 PASS。
2. **镜像增量重编与宿主守护进程更新**：
   - 执行 `pipeline device build --from 04_build_modules sun` 完成 10 阶段构建（注入新版 `cdh_redirect.ko` 的 `vendor_boot.img` `vboot-cbabae4806bf`、含 24 个 HAL 模块的 `patch.img` `fe3a43315780` 与 `vda.img` `vda-f52ca0ccbb57`，官方 `super.img` 保持原样未重打）；
   - 重新编译更新 `out/agent/dist/{daemon,cdha}`。
3. **实机冷启动、相机启停与桌面滑动复测**（注：因 `vda.img` 刷新重建 `writable-base.qcow2` 时 `vda5` `/metadata` 会重新格式化生成新 FBE 密钥，需与 `writable-data.qcow2` 成对重建以避免旧加密头失配）：
   - `Honor Magic7 RSR #1` 一次冷启 `sys.boot_completed=1`（`SELinux=Enforcing`），`IFingerprint/default`、`IMotion/default`、`ICameraProvider/internal/0` 全部在线；
   - 复测报错计数、相机开启/退出帧计数与 CPU、静息 `top`/`vmstat` 及 14 轮桌面滑动 `gfxinfo` 指标全部符合预期（见第 3 节与 [log-pass.txt](log-pass.txt)）。

---

## 6. 遗留

- 全新初始化首次联网后，GMS 负一屏进程 `com.google.android.googlequicksearchbox:googleapp`（Chromium WebView `Chrome_InProcGp` 线程）在后台预热卡片期间偶发单核占用（约占 8 核总算力的 12%，`:search` 子进程已在数分钟内回落至 `0.5%`），不阻塞 `system_server` 主线程与 `SurfaceFlinger` 合成；后续若需进一步压降 GMS 待机功耗，可排查 WebView GPU 光栅化配置。
- `system_server` 运行期偶有荣耀调度代理打印 `[VipSchedProxy:]: Vip failed to open /proc/vip/ioctl, errno=2`（仅在应用切换时触发，稳态静息 `logd` CPU 已为 `0.0%`），后续如有需要可在 `cdh_device_info` 提供轻量 `/proc/vip` 兼容节点进一步静默日志。
