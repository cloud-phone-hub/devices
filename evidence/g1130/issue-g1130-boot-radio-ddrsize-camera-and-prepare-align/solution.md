# #336 BLU G65L（`blu/G1130`）`02_prepare` 真机规格对齐与无线电 `/dev/hvc4` 启动竞态、RAM 属性、`tool_service` AVC 及 `DreamCamera2` 原生相机全链修复

- **设备 / 机型**：BLU G65L（`blu/G1130`，版本 `14-UP1A.231005.007-20260511`，Unisoc UMS9230T，GRF `system=A14` + `vendor=A13`，配置 `G1130-android13`，`lineage-20.0` + `kernel 5.15`）
- **日期**：2026-11-26
- **状态**：已闭环

---

## 一、问题

`BLU G65L #1`（`G1130_14-UP1A.231005.007-20260511/1`）在 `02_prepare` 真机规格采集与运行态启动验收中暴露五类耦合问题：

1. **`02_prepare` 真机规格采集与落盘偏差**：
   - 官方固件 `BLU-G65L-G1130.zip` 内嵌展锐 `.pac` 包（`ums9230_1h10_Natv-user-gms_SIGN.pac`），旧 `baseband.go` 不解析 `.pac` 头部及内嵌 `SC9600_sharkl5pro_pubcp_modem.dat` 的 `BASE Version`，导致真机基带版本缺失（`gsm.version.baseband` 为 `,`）；
   - AI 规格采集偶发尾逗号与分组名 `"BLU Products"`（应归一为 `"BLU"`），大核 `Cortex-A75` 的 ARM Part 落成小核 `0xd05`（应为 `0xd0a`），屏幕刷新率与分辨率未被原厂 `blu.*` 属性与 `dtbo.img`（`720x1600 @ 90Hz`）纠偏；
   - 展锐 `ums9230` DTB 中升频/降频档位分散在 `sprd,power-scene` 与 `cooling-min-level` 节点，旧 `dtb_cpufreq.go` 未合并 `1820000`/`2028000` 顶频与 `614400`/`1228800` 底频；
   - 展锐 GRF 平台在 `apply-profile` 时误注入空桩 `srmi` HAL 声明、未自动生成屏蔽展锐调试守护进程的 `modules.redirect` 规则、EDID 厂商码写出 4 字节 `"SPRD"`（标准 EDID 限 3 大写字母 `"SPR"`）、`vibrator` 缺默认 `id="1"`，且 `vendor_boot` cmdline 缺 `androidboot.ddrsize=4096M`。
2. **设置「关于手机」`RAM` 显示 `null`**：原厂 `Settings.apk` 读取 `ro.boot.ddrsize` 渲染内存容量；云机未注入该引导属性且 `service_daemon` 未按 `/proc/meminfo` 动态同步，导致界面显示 `RAM: null`。
3. **电话栈彻底瘫痪（无 SIM、无基带、无蜂窝数据）**：`gsm.sim.state` 恒为 `ABSENT,ABSENT`，`mVoiceRegState=1(OUT_OF_SERVICE)`，`ccmni` 无 IP，`About device` 中 `SIM status` 显示 `Not available`。
4. **后台高频 SELinux AVC 刷屏**：`/system_ext/bin/tool_service` 未纳入 `cdh_redirect` 屏蔽列表，单次运行刷出 45,621 条 `avc: denied` 拒绝日志（承接 `g1130` `#297` [../issue-g1130-ppp-mppe-and-unisoc-restarting-daemons/solution.md](../issue-g1130-ppp-mppe-and-unisoc-restarting-daemons/solution.md)）。
5. **展锐原生相机 `DreamCamera2`（`/system_ext/app/DreamCamera2/DreamCamera2.apk`）冷启动即崩溃且快门无法拍照**：授予 `CAMERA` 权限后，打开相机即在 `SprdAndroidCamera2Capabilities.buildFlashLevels` 等多处抛出 `NullPointerException` 闪退；补齐静态能力后点击快门仍报 `zoom is not sync complete from hal,can not perform onShutterbuttonClick` 无法落盘照片。

---

## 二、根因

1. **`/dev/hvc4` 无线电通道启动竞态与 `hvc_console` 翻转缓冲死锁（电话栈瘫痪真因）**：
   - `hvc4` 是全机唯一由宿主侧（`vhost-modem-controller` 每 250ms 主动下发二进制 `HELLO` 帧）先于客户机灌数据的 `virtconsole`。客户机 `service_daemon` 在 `t ≈ 7.4s` 完成 `hvc2` `ADBKEY1 READY` 握手打开宿主闸门，而 `vendor.radio-vhal` 在 `t ≈ 9.03s` 才启动执行 `open("/dev/hvc4")`。
   - Linux 5.15 内核 `hvc_open` 在返回用户态前即调用 `hvc_kick()`，8 核 SMP 下 `khvcd` 内核线程先于用户态 `tcsetattr(cfmakeraw)` 将积压的二进制 `HELLO` 帧推入仍处于默认规范模式（`ICANON | ICRNL | IXON | ECHO`）的 `N_TTY` 行规程（QMP 实测 `virtio-serial` `q=11` TX `used=269` 即 `ECHO` 回显字节），把帧内 `0x0d` 变形为 `0x0a` 或触发 `IXON` 软流控停滞。
   - 旧 `ConfigureBinaryTTY()` 既未清理 `IXON|IXOFF|IXANY` 与 `tcflow(TCOON|TCION)`，也未执行 `tcflush(fd, TCIOFLUSH)` 清空竞态窗口期已变形的输入队列；旧 `ReadFrame()` 无魔数滑窗重同步，`DispatcherLoop` 首帧校验失败即调用 `SetStopped() -> close(/dev/hvc4)`，进而使 `hvc_console` 在 `itty == NULL` 状态下塞满 64 KiB `hp->port.buf`（QMP `q=10` `last_avail=shadow_avail=used=2031`）永久锁死端口。
   - 同时 Android 13 vendor / Android 14 system 跨代场景下，`android.hardware.radio` 的 `radio_config.cpp` 与 `radio_network.cpp` 存在 AIDL V1/V2 方法宏守卫错位，需按实际链接的 AIDL 版本隔离。
2. **`ro.boot.ddrsize` 缺失**：原厂 `Settings` 依赖 `ro.boot.ddrsize` 格式化 `4.0 GB` 规格；既要在 `02_prepare` 为展锐机型生成 `androidboot.ddrsize=<MB>M`，也要在 `service_daemon` 启动早期按实机 `/proc/meminfo` 动态兜底同步 `ro.boot.ddrsize` 以兼容不同内存规格实例。
3. **`DreamCamera2` 三层展锐私有 Vendor Tag 缺口**：
   - **路径识别遗漏**：`G1130` 的原生相机位于 `/system_ext/app/DreamCamera2/DreamCamera2.apk`，`camera_unisoc_ext.cpp` 的 `HasUnisocDreamCameraApp()` 仅探测了 `priv-app` 与 `/system/app`、`/product/app`，漏掉 `/system_ext/app/DreamCamera2`，导致 `capMode`、`meteringMode`、`sprdZslEnabled` 未切换为 `DreamCamera2` 所需的 `TYPE_INT32`。
   - **静态特征与帧结果未判空解引用**：字节码反编译全扫描证实，`SprdCameraCharacteristics` / `SprdAndroidCamera2Capabilities` 对 `availableSensorType`（`0xC2000017`）、`availableSprdFlashLevel`（`0xC2000019`）、`sprd3availableAuto3dnr`（`0xC2000024`）、`sprdAvailableAutoHdr`（`0xC200002E`）、`sprdAvailableGesturedetect`（`0xC200002F`）、`sprdAvailableLogoWatermark`（`0xC2000030`）、`sprdAvailableTimeWatermark`（`0xC2000031`）、`sprdCamIdSensorRoleType`（`0xC2000034`）直接调用 `.intValue()` / `.byteValue()` 未判空；`PhotoModule` 与 `SprdAndroidCamera2AgentImpl` 又对每帧 `CaptureResult` 中的 `sprdWaterMarkFlag`（`0xC2000052`）、`sprdSwitchCaptureRequestEnable`（`0xC200004A`）、`sprdGesturedetectCurrent`（`0xC200003B`）、`sprdFaceAttributes`（`0xC2000038`）、`sprdDualCamCoveredVal`（`0xC2000035`）、`sprdStabilityCurrent`（`0xC2000048`）、`sprdAfStateEnable`（`0xC2000029`）未判空直接解引用。
   - **快门变焦同步门控**：`PhotoModule.onShutterButtonClick` 检查帧结果 `sprdSwitchCaptureRequestEnable`（`0xC200004A` -> `mZoomCaptureResult`）必须为 `1` 才允许触发拍照，且拍照后由 `sprdNotifyNextCapture`（`0xC2000041` = `1`）触发 `onNextCaptureReady()` 重新解锁快门（对比 `arw` `#304` [../../arw/issue-arw-boot-radio-deadlock-wifi-baseband/solution.md](../../arw/issue-arw-boot-radio-deadlock-wifi-baseband/solution.md) 的 `HMDCamera` 分支）。

---

## 三、证据

- **修前失败现场**：见 [log-fail.txt](log-fail.txt)、[before-about-top.png](before-about-top.png)、[before-about-ram.png](before-about-ram.png)
  - `gsm.sim.state=ABSENT,ABSENT`、`gsm.version.baseband=,`、`SIM status (sim slot 1): Not available`；
  - `ro.boot.ddrsize` 为空，`About device` 显示 `RAM: null`；
  - `dmesg` 中 `/system_ext/bin/tool_service` 累计 45,621 条 `avc: denied`；
  - QMP `x-debug-virtio-queue-status` 证实 `/dev/hvc4` 对应 `virtio-serial` RX `q=10` `last_avail=shadow_avail=used=2031`（64 KiB 缓冲塞满停摆）、TX `q=11` `used=269`（`N_TTY` 规范模式 `ECHO` 回显残留）。
- **修后实机验收日志与实屏截图**：见 [log-pass.txt](log-pass.txt)、[after-about-top.png](after-about-top.png)、[after-about-ram.png](after-about-ram.png)、[after-android-version.png](after-android-version.png)、[after-camera-preview.png](after-camera-preview.png)。

---

## 四、改动

1. **`02_prepare` 展锐真机规格提取与清单生成全链对齐**：
   - `agent/modules/pipeline/stages/02_prepare/baseband.go` & `baseband_test.go`：支持解析展锐 `.pac` 及 `.zip` 内嵌 `.pac` 分区表，从 modem 载荷提取 `BASE Version`（`4G_MODEM_22B_W26.02.5_P2`）。
   - `agent/modules/pipeline/stages/02_prepare/ai_profile.go`：增强宽松 JSON 清洗、商业品牌组名规整（`"BLU Products"` -> `"BLU"`）、`Cortex-A75` 标准 `part=0xd0a` 映射，以及结合原厂 `blu.*` 属性与 `dtbo.img` 面板时序纠偏 `720x1600 @ 90Hz`。
   - `agent/modules/pipeline/stages/02_prepare/dtb_cpufreq.go`：解析并合并展锐 `sprd,power-scene` 升频档位（`1820000`/`2028000`）与 `cooling-min-level` 底频档位（`614400`/`1228800`）。
   - `agent/modules/pipeline/stages/02_prepare/{grf_reconcile.go,register_parse.go,firmware_measured_extra.go,apply_profile.go,apply_profile_configs.go}` & `unisoc_profile_align_test.go`：展锐平台禁空桩 `srmi`、自动生成 `system_ext/bin/{linkturbonative,srmi_proxyd,ylog,slogmodem,tool_service}` 屏蔽重定向规则（遵循 `AGENTS.md §8` 不改官方 `super.img`）、EDID 3 字母厂商码 `"SPR"` 规整、`vibrator id="1"` 保底及 `androidboot.ddrsize=4096M` 注入；重跑 `apply-profile -recollect -overwrite blu/G1130` 刷新 `devices/manifest/devices/blu/G1130*` 与 `devices/manifest/configs/blu/G1130/G1130-android13/**`。
2. **`service_daemon` 动态同步 `ro.boot.ddrsize`**：
   - `devices/hardware/android.service.daemon/src/rust/src/props/fixups.rs`：按 `/proc/meminfo` 总内存档位自动计算并同步 `ro.boot.ddrsize`（4 GiB 实例落为 `4096M`）。
3. **`android.hardware.radio` `/dev/hvc4` 竞态免疫与 AIDL V1/V2 守卫修复**：
   - `devices/hardware/android.hardware.radio/src/channel.cpp` & 同目录单测 `channel_test.cpp`：
     - `ConfigureBinaryTTY()` 显式清除 `IXON|IXOFF|IXANY`、置 `CLOCAL|CREAD`、调用 `tcflow(TCOON|TCION)` 解除潜在软流控停滞，并执行 `tcflush(fd, TCIOFLUSH)` 清空 `open -> tcsetattr` 窗口期被 `N_TTY` 污染的残留字节；
     - 新增 `BufferedChannelReader` 与 `ReadFrameResync()` 128 KiB 魔数滑窗重同步状态机（失配仅滑动 1 字节保留预读窗口，绝不丢弃相邻真魔数）；
     - `DispatcherLoop()` 在收到首个合法 `HELLO` 完成握手前遇到残帧仅记录告警继续等待下一拍（250ms），不再误关 fd 锁死 `hvc_console` 缓冲。
   - `devices/hardware/android.hardware.radio/src/{radio_config.cpp,radio_network.cpp}`、`agent/modules/pipeline/stages/08_build_hardware/modules/android14/android.hardware.radio.json`、`agent/modules/pipeline/internal/halcore/profile/profile.go`：对齐 Android 13/14 Radio AIDL V1/V2 条件编译守卫。
4. **`android.hardware.camera` 展锐 `DreamCamera2` 全链适配**：
   - `devices/hardware/android.hardware.camera/src/{camera_vendor_ext.h,camera_provider_service.cpp}`：在 `#if CDH_CAMERA_UNISOC` 守卫下新增 `AppendVendorRequestAndResultKeys` 与 `AppendVendorResultMetadata` 钩子（对其他厂商变体为空内联零影响），将 `buildResultMetadata` 容量扩至 `128 entries / 8192 B`。
   - `devices/hardware/android.hardware.camera/src/unisoc/camera_unisoc_ext.cpp`：
     - `HasUnisocDreamCameraApp()` 增补 `/system_ext/app/DreamCamera2` 与 `/system_ext/app/DreamCamera2/DreamCamera2.apk`；
     - 实现 `AppendVendorStaticMetadata`、`AppendVendorCharacteristicKeys`、`AppendVendorRequestAndResultKeys`，注入 20 项展锐静态能力标签及请求/结果可用键列表；
     - 实现 `AppendVendorResultMetadata`，注入 20 项展锐 `SprdCaptureResult` 帧结果标签，其中置 `sprdSwitchCaptureRequestEnable`（`0xC200004A`）= `1` 与 `sprdNotifyNextCapture`（`0xC2000041`）= `1` 打通快门拍照与连拍解锁。
5. **增量构建与盘集发布**：执行 `pipeline device build -from 08_build_hardware G1130` 刷新 `out/devices/dist/blu/G1130/14-UP1A.231005.007-20260511/patch.img`（`sha256: 3dd9502c29052b4dad441cc1d06862966beb36aec6655477a16ad50b2da32186`）并重启 `BLU G65L #1`。

---

## 五、验证

1. **单元测试**：
   - `go test ./stages/02_prepare/...` 全量 PASS；
   - `devices/hardware/android.hardware.radio/src/channel_test.cpp`（含 `N_TTY` `0x0d->0x0a` 残帧 + 伪魔数滑窗恢复压测）与 `data_policy_test.cpp` 编译运行全量 PASS。
2. **实机 `BLU G65L #1`（ADB `127.0.0.1:5911`，稳态运行 `181.07s`）全维度验收**（详见 [log-pass.txt](log-pass.txt)）：
   - **引导与安全**：`sys.boot_completed=1`、`getenforce=Enforcing`、`anr_files=0`、`tool_service_avc_denied=0`（从 45,621 条降至 0）。
   - **无线电、SIM、基带与双网**：`gsm.sim.state=LOADED,ABSENT`、`gsm.operator.alpha=China Unicom,`、`gsm.operator.numeric=46001,`、`gsm.network.type=LTE,Unknown`、`gsm.version.baseband=4G_MODEM_22B_W26.02.5_P2,4G_MODEM_22B_W26.02.5_P2`、`mVoiceRegState=0(IN_SERVICE)`、`mDataRegState=0(IN_SERVICE)`、`mDataConnectionState=2(CONNECTED)`；`ccmni1` 获配 `100.64.155.15/24`、`wlan0` 获配 `192.168.10.100/24`。
   - **真机规格对齐**：`Device name=BLU G65L`、`Model=G65L`、`ro.boot.ddrsize=4096M`（`RAM: 4.0 GB`）、屏幕 `720x1600 @ 320dpi`、GPU `ARM Mali-G57 MP1`、小核 `614400..1820000` / 大核 `921600..2028000` 频率表、11 项传感器全部就绪；人工审图 [after-about-top.png](after-about-top.png)、[after-about-ram.png](after-about-ram.png)、[after-android-version.png](after-android-version.png) 确认状态栏双信号满格、`SIM status: China Unicom`、`RAM: 4.0 GB` 与 `Baseband version: 4G_MODEM_22B_W26.02.5_P2` 全部准确显示。
   - **原生相机 `DreamCamera2` 5/5 压测与连拍落盘**：
     - 连续 5 次 `am force-stop com.android.camera2` + 冷启动 **5/5（100%）通过**（PID `5083 / 6673 / 6929 / 7135 / 7380`），`cameraserver`（PID `508`）与 `android.hardware.camera-service`（PID `423`）全程 0 重启、0 崩溃；
     - 连续点击快门成功落盘 2 张 JPEG 照片（`/sdcard/DCIM/Camera/IMG_20261126_031933.jpg` 16 KB、`IMG_20261126_031936.jpg` 16 KB），人工审阅 [after-camera-preview.png](after-camera-preview.png) 及导出的 JPEG 原图确认实时取景画面与左下角圆形相册缩略图均清晰正常。

---

## 六、遗留

- 官方 `super.img` 预装的 `com.android.chrome` 仅携带 32 位 `libmonochrome.so`，在纯 64 位云机环境后台自启探针时会记录一次 `UnsatisfiedLinkError: library "libmonochrome.so" not found`（与 `arw` `#304` [../../arw/issue-arw-boot-radio-deadlock-wifi-baseband/solution.md](../../arw/issue-arw-boot-radio-deadlock-wifi-baseband/solution.md) 同属官方 32 位预装包既定预期行为，不影响桌面与任何系统功能）。
