# Infinix X6876 V4L2 硬件视频解码、6GiB ZRAM、`/proc/bootprof` 与宿主帧日志抖动根治（#346）

- **设备/机型**：Infinix GT 30 / X6876（`infinix/Infinix-X6876`，SoC `MT6878`，GRF `vendor=14 / API 34` + `system=16`，`16-UP1A.231005.007-20260423`，`lineage-21.0` + `kernels/6.1`）
- **状态**：已闭环（2026-11-26，实例 `Infinix X6876 #1`，`SELinux=Enforcing`）

## 一、问题

在完成五大外设子系统与 `/proc/ged` 修复（见 [../issue-x6876-subsystems-audit-and-jank-fix/solution.md](../issue-x6876-subsystems-audit-and-jank-fix/solution.md)）后，深度压测 `Infinix X6876 #1`（视频播放、后台长稳与内存压力场景）仍存在五类导致卡顿与后台异常风暴的隐蔽问题：

1. **YouTube 视频完全退回纯 CPU 软解且每 ~8 秒断流重切**：
   - 播放 YouTube Shorts（720p/1080p H.264 竖屏流）时，虽宿主机已拉起 `vhost-device-media`（VideoToolbox 硬解后端）且客机 `android.hardware.media.c2@1.2-service-v4l2-64` 已运行，但应用始终只看到 `"software"` 服务，被迫创建软解组件 `c2.android.avc.decoder`（`media.swcodec` 持续吃满 `25~45%` CPU）；
   - 同时 YouTube `YT.codecreuse` 因缺少帧率/竖屏互换能力声明与热区探测失败（`Cacheable false`），约每 8 秒销毁并重建解码器，且 `MediaCodecsXmlParser` 每次初始化都报错找不到 `media_codecs_mediatek_audio.xml`。
2. **`/proc/meminfo` 伪报 `SwapFree: 0 kB`（6GiB Swap 100% 假满）且真实 ZRAM 未激活**：
   - `/proc/swaps` 为空、`zram0 disksize=0`，而 `device_info/meminfo.c` 仅将 `SwapTotal:` 固定替换为 `6291452 kB`、未同步修正 `SwapFree: 0 kB`；
   - `lmkd` 与 Android `CachedAppOptimizer` 误判 6GiB Swap 已 100% 耗尽，后台频繁触发页面回收与进程查杀。
3. **AMS 进程拉起热路径 `/proc/bootprof` 异常风暴与 `StucklogoHandle` 报错**：
   - 每次拉起应用或服务进程，MTK `system_server`（`ProcessList.handleProcessStartedLocked -> MtkSystemServerImpl.addBootEvent`）都在 AMS 锁内尝试写 `/proc/bootprof`，因节点不存在抛 20 行 `FileNotFoundException`（单次运行刷出 `1604` 行 `E/BOOTPROF`），同时 `StucklogoHandle` 每 30 秒报 `parseBootProf open /proc/bootprof failed`。
4. **无硬件 `vtservice` 空转、`INvram` 桩缺失与 `/tranfs` `unlabeled:s0` 审计风暴**：
   - MTK 视频通话服务 `vtservice` 在无基带硬件环境被拉起，持续打开 `/dev/VT` 刷出 `904+` 行 `E/AVSync`；
   - `system_server`（`MtkSecurityHelper`）与 `radio` 查询 `vendor.mediatek.hardware.nvram.INvram/default` 触发 SELinux 拒绝与异常堆栈；
   - 根分区 `/tranfs` 目录原始 xattr `u:object_r:tranfs_file:s0` 未在策略中声明，被内核降级为 `u:object_r:unlabeled:s0`，引发 `system_server`、`stucklogo`、`vold`、`keystore` 与应用进程连锁 `{ search }` 拒绝，叠加 `sysfs_thermal` / `IMtkPower` 高频探测刷满 `auditd`（触发 `audit: rate limit exceeded`）。
5. **宿主机 QEMU `cgl-headless` 每帧同步写移动硬盘 `qemu.log` 引发渲染主线程 I/O 抖动**：
   - `qemu/source/ui/cgl-headless.c` 无条件对每帧 `>=640x480` 画面调用 `fprintf(stderr, "cgl-headless 大帧 ...")`，单实例运行不到一小时即向移动硬盘写入 `109,131` 行（`12.68 MB`）日志，外置盘元数据刷新偶发阻塞 QEMU 渲染线程造成 UI 瞬时卡顿。

## 二、根因

1. **GRF 跨代机型（`system=16` + `vendor=14`）缺失 `media.c2.hal.selection=aidl` 致系统侧屏蔽全部 AIDL Codec2 Store**：
   - 反汇编客机 `/system/lib64/libcodec2_client.so`（`Codec2Client::CacheServiceNames`）与 `/system/lib64/libcodec2_aidl_client.so`（`android::IsCodec2AidlHalSelected`）证实：Android 16 系统在 `com_android_media_codec_flags_aidl_hal() == true` 时，仍会调用 `GetProperty("media.c2.hal.selection", "hidl")` 且默认值为 **`"hidl"`**；
   - 原生 Android 15/16 vendor 在 `vendor/build.prop` 中自带 `media.c2.hal.selection=aidl`，而 `Infinix X6876` 属于 Android 14 vendor（`ro.vendor.api_level=34`）升级 Android 16 system 的 GRF 机型，官方 `vendor/build.prop` 无该属性；
   - 云机 `android14+` HAL 引擎（`android.hardware.media.v412` 与 `android.hardware.media`）统一注册 **AIDL** `android.hardware.media.c2.IComponentStore/{default,default1,default9}`（仅 `android13` 走 `hardware-hidl`），`IsCodec2AidlHalSelected()` 返回 `false` 后客户端跳过 AIDL 服务枚举、只查 HIDL `hwservicemanager`，导致 `Available Codec2 services: "software"`；
   - 此外 `09_build_vendor/media_codecs_c2.go` 生成的 `c2.v4l2.{avc,hevc}.decoder` 原先缺少 `frame-rate`、`measured-frame-rate-*` 与 `can-swap-width-height`（1080x1920 竖屏流能力匹配依赖该 Feature），顶层 `vendor/etc/media_codecs.xml` 残留悬空 `<Include href="media_codecs_mediatek_audio.xml" />`，且 `V4L2Decoder.cpp` 在每帧 `QBUF`/`DQBUF` 热路径遗留了 9 处 `ALOGI`（60fps 下每秒 ~480 行 `logd` 写入）。
2. **`device_info` 仅覆写 `SwapTotal` 未联动真实 Swap 用量，且缺少云机通用 ZRAM 初始化脚本**：
   - 内核已内置 `zram`（支持 `lz4`），但 lineage virtio 基底 `fstab` 未配置 `zramsize`；`meminfo.c` 将 `SwapTotal:` 改为 `6291452 kB` 时保留了内核原始 `SwapFree: 0 kB`，形成 `Used = 6291452 kB (100%)` 的假象。
3. **GKI 内核无 MTK 专有 `/proc/bootprof` 节点**：MTK 框架在 Java/Native 多处硬编码读写 `/proc/bootprof`。
4. **传音/MTK 专有服务与分区标签缺口**：
   - 官方默认开启 `persist.vendor.vilte_support=1` 拉起 `vtservice`；
   - `hal_neuralnetworks_default` 虽托管了 `INvram/default`，但 `nn_mtk_ext.cpp` 未实现 `vendor.mediatek.hardware.nvram.INvram` 事务应答，且 `system_server`/`radio` 缺 SELinux `find`/`call` 权限；
   - `tranfs_file` 未在 `vendor_sepolicy` 声明（且若拆成 `@when-not-declared` + `@when-declared` 两段会被单遍符号表解析跳过后半段 `allow`）。
5. **`cgl-headless.c` 调试日志未加环境变量与慢帧阈值门控**。

## 三、证据

- 修复前故障现场详见 [log-fail.txt](log-fail.txt)：
  - `SwapTotal: 6291452 kB` / `SwapFree: 0 kB`，`zram0 disksize=0`；
  - YouTube 播放仅见 `Available Codec2 services: "software"`，回退 `c2.android.avc.decoder`；
  - `/tranfs` 为 `u:object_r:unlabeled:s0`，`dmesg`/`logcat` 持续刷屏 `sysfs_thermal`、`unlabeled:s0`、`E/BOOTPROF`、`E/AVSync`；
  - 宿主机 `qemu.log` 达 `109,131` 行 / `12,679,519` 字节（满屏 `cgl-headless 大帧 1080x2436`）。
- 反汇编 `/system/lib64/libcodec2_aidl_client.so` `0x1efc0 <android::IsCodec2AidlHalSelected()>` 实证其调用 `android::base::GetProperty("media.c2.hal.selection", "hidl")` 并与 `"aidl"`（`0x6c646961`）比对。
- 修复后全量冷启实测证据详见 [log-pass.txt](log-pass.txt) 与 V4L2 1080x1920 硬件解码播放截图 [youtube-v4l2-hwdec.png](youtube-v4l2-hwdec.png)。

## 四、改动

1. **打通 GRF 跨代 AIDL Codec2 选择与 V4L2 硬解能力表，收敛逐帧日志**：
   - `agent/modules/pipeline/stages/09_build_vendor/props.go` & `props_test.go`：对 `plan.Android >= 14` 自动在 `vendor/build.prop` 补齐 `media.c2.hal.selection=aidl`（`android13` 保持 `hardware-hidl` 不注入），新增单测 `TestApplyVendorBuildPropsBackfillsMediaC2HalSelectionAidl`；同步写入 `Infinix-X6876-android14/vendor/config.json`；
   - `agent/modules/pipeline/stages/09_build_vendor/media_codecs_c2.go` & `media_codecs_c2_test.go`：为 `c2.v4l2.{avc,hevc}.decoder` 补齐 `frame-rate`（`1-240`）、`measured-frame-rate-*`（至 `3840x2160`）与 `Feature name="can-swap-width-height"`（支持存量旧 XML 自动升级），新增 `pruneDanglingMediaCodecsIncludes` 自动剥除 `vendor/etc/media_codecs.xml` 中悬空的 `<Include>`；
   - `devices/hardware/android.hardware.media.v412/src/v4l2_codec2/v4l2/V4L2Decoder.cpp` & `sepolicy/vendor_sepolicy.cil`：将热路径 9 处逐帧 `ALOGI`（`QBUF`/`DQBUF`/`mOutputCb`）降级为 `ALOGV`（保留状态机切换 `Set state Idle => Decoding` 等会话级 `ALOGI`），并 `dontaudit` `mediacodec` 读取可选 `default_prop`。
2. **启用真实 6GiB `lz4` ZRAM 并修正 `/proc/meminfo` `SwapFree` 计算**：
   - 新增 `devices/hardware/android.service.daemon/vendor/bin/cph-zram-setup` 与 `vendor/etc/init/cph_zram_setup.rc`：`on boot` 阶段幂等配置 `/sys/block/zram0/comp_algorithm = lz4`、`disksize = 6442450944`（扣除 4KB swap 头后精确等于 `6291452 kB`）并执行 `mkswap` + `swapon`；
   - `devices/modules/device_info/meminfo/meminfo.c`：按内核真实 `swap_used = raw_swap_total - raw_swap_free` 计算并原位输出 `SwapFree: = CDH_SWAP_TOTAL_KB - swap_used`，彻底消除 `SwapFree: 0 kB` 假满状态。
3. **内核注册 `/proc/bootprof`、补齐 `INvram` 桩、禁启 `vtservice` 与收口 SELinux**：
   - `devices/modules/device_info/sched/sched_proc.c` & `android.service.daemon/sepolicy/vendor_sepolicy_rules.cil`：注册 `/proc/bootprof`（`0666`），自动声明 `proc_bootprof` + `(genfscon proc "/bootprof" ...)`，向 `init`、`vendor_init`、`system_server`、`vold`、`stucklogo`、应用及 HAL 域授予权限；同时在 `android.service.daemon/sepolicy/vendor_sepolicy.cil` 为 `mediaserver`、`mediametrics` 补齐 `pioneer` 反向 `fd`/`fifo_file`/`binder`/`unix_stream_socket` 授权，恢复 `Enforcing` 下 `dumpsys media.player` / `media.metrics`；
   - `devices/hardware/android.hardware.neuralnetworks/src/mtk/nn_mtk_ext.cpp`：新增 `OnMtkNvramTransact` 安全应答 `vendor.mediatek.hardware.nvram.INvram` 调用；
   - `Infinix-X6876-android14/{vendor,odm}/config.json`：设置 `"persist.vendor.vilte_support": "0"`、`"persist.vendor.viwifi_support": "0"` 阻止无硬件拉起 `vtservice`；
   - `Infinix-X6876-android14/vendor/sepolicy/vendor_sepolicy_rules.cil`：通过 `@ensure-type tranfs_file` 声明标签并向全域 `domain` 授予只读目录 `search`，授权 `system_server`/`radio` 访问 `mtk_hal_nvramagent_service`，并对 `sysfs_thermal`、`default_android_hwservice`（`IMtkPower`）、`mtk_hal_gpuserv_service`、`mtk_hal_mbrain_service` 等无硬件探测追加 `dontaudit`。
4. **门控宿主机 QEMU `cgl-headless` 帧日志**：
   - `qemu/source/ui/cgl-headless.c`：新增环境变量 `CDH_CGL_DEBUG=1` 开关，默认仅记录前 3 帧建连握手与 `>=50ms` 的限频慢帧告警；增量重编并带 `com.apple.security.hypervisor` 签名更新 `out/qemu/dist/darwin/bin/qemu-system-aarch64`。

## 五、验证

将新编 `6.1` 内核模块、HAL 与 `patch.img`（`sha256: 06122c486783ff61f70d3a7affda386c3fb53c2c8831bd01ffa9c17e1377eeab`，官方 `super.img` 保持原样直通）部署至 `Infinix X6876 #1` 冷启动实测（`SELinux=Enforcing`）：

1. **V4L2 VideoToolbox 硬件视频解码直通**：
   - 冷启后 `getprop media.c2.hal.selection` 为 `aidl`，`dumpsys media.player` 直接列出 `c2.v4l2.avc.decoder` 与 `c2.v4l2.hevc.decoder`（`hw-accelerated: 1`、`vendor: 1`、`rank: 128`）；
   - 打开 YouTube Shorts 播放 `1080x1920` 高清竖屏视频：日志显示 `Available Codec2 services: "default" "default1" "default9" "software"` → `Created component [c2.v4l2.avc.decoder]` → `YT.codecreuse: Codec created: c2.v4l2.avc.decoder. Cacheable true` → `V4L2Decoder: Set state Idle => Decoding`（`coded size: 1080x1920`），不再断流重切；
   - 播放期间 **`media.swcodec` CPU 降至 `0.0%`**，`com.google.android.youtube` CPU 仅 `3.4~4.0%`，整机空闲率保持 `721~743%idle / 800%cpu`，`V4L2_per_frame_QBUF_spam=0`。
2. **6GiB ZRAM 与内存状态**：
   - `/proc/meminfo` 显示 `MemAvailable: 8843108 kB`、`SwapTotal: 6291452 kB`、**`SwapFree: 6291452 kB`**；
   - `/proc/swaps` 显示 `/dev/block/zram0 partition 6291452 0 -2`，压缩算法为 `[lz4]`（`disksize=6442450944`）。
3. **异常风暴与 SELinux 拒绝清零**：
   - `/proc/bootprof` 标签为 `u:object_r:proc_bootprof:s0`，`/tranfs` 标签为 `u:object_r:tranfs_file:s0`；
   - `E_BOOTPROF=0`、`E_STUCKLOGO_bootprof=0`、`E_AVSync=0`、`E_media_codecs_audio_missing=0`、`avc_tranfs=0`、`avc_bootprof=0`、`avc_sysfs_thermal=0`；
   - 宿主机 `qemu.log` 中 `cgl-headless 大帧` 计数为 **`0`**（仅保留首屏 `flush #1..#3` 共 3 行握手记录）。
4. **桌面流畅度、静置负载与外设回归**：
   - `QuickstepLauncher` 连续 10 次快速左右滑动：`Total frames rendered: 1296`，**`Janky frames: 1 (0.08%)`**，`Missed Vsync: 0`（CPU `P50=6ms / P90=10ms / P95=12ms / P99=17ms`，GPU `P50=4ms / P99=11ms`）；
   - 桌面静置 CPU 空闲率达 `770~786%idle / 800%cpu`；
   - Wi-Fi（`wlan0 192.168.20.100/24`，外网 `ping 223.5.5.5` `0% loss / 0.62ms`）、Radio（`ccmni0` IMS IPv6 + `ccmni1 100.64.155.15/24`）、蓝牙（`state: ON`，`crashed 0 times`）、NFC（`Service nfc: found`）全部正常无回归。

## 六、遗留

无。`Infinix X6876` 的硬解管线、ZRAM/Swap、内核 `/proc/bootprof`、SELinux 审计噪声与宿主机 QEMU 帧日志 I/O 抖动已全部闭环固化。
