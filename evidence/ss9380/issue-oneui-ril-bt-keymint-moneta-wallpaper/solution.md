# ss9380 打通 OneUI 蜂窝满格信号/4G、蓝牙秒开、KeyMint/HyPer 扩展、Moneta/AI Core 崩溃根治与 10-bit HEVC 动态壁纸硬解

- 设备：`samsung/pa3q`（Samsung Galaxy S25 Ultra 国行 `SM-S9380`，OneUI Android 16，基线 `16-AP3A.240905.015.A2-20260812` / `S9380ZCSDCZH1`）
- 日期：2026-11-26
- 最终盘集：`patch.img` sha256 `91e40043cb7580b46dd800d7b9223efc11860fa338b50714ac09d39d58fdf575`（`vendor.img` `933495b813e3a223f26b70c359d75378d6772189934af11d43dbca6fda766c84`，`vendor_dlkm.img` `58a0a2c013c7000aa72119a841543411555ac3154b9a6276bbd82df3942dc630`；官方 `super.img` 100% 零改动原样复用）
- 结果：冷启动（零手工修权限）106s 进系统，`Enforcing`；SIM `LOADED`（中国联通 `subId=1`）+ 状态栏 **4 格满格信号** + `ccmni1` 4G 拨通（`100.64.155.x/24`，亮起 `4G ↑↓`）；蓝牙开关毫秒级切换 `ON` 并扫出外设；主桌面 `1440×3120` 120fps 10-bit HEVC 动态壁纸经 `c2.v4l2.hevc.decoder` 自动硬解渲染；整机 `logcat -b crash` **0 崩溃**，两个前台崩溃弹窗与后台安全服务风暴全面清零。

## 一、问题

在 `ss9380` 前序桌面打通（见 `../issue-knoxguard-china-region/`、`../issue-ss9380-vendor-hal-configs/`）基础上，真机级交互验收暴露出四类影响核心可用性的系统级问题（见同目录 `log-fail.txt`、`fail-aicore-popup.png`、`fail-moneta-black-wallpaper.jpg`）：

1. **移动网络不可用、状态栏信号条恒为 0 格空心**：客机虽已加载 AOSP `IRadio*` 虚拟基带，但 OneUI 状态栏信号格始终为空心灰阶，SIM 卡无法完成订阅加载，关闭 Wi-Fi 开启移动数据后无法建立蜂窝数据连接。
2. **蓝牙开关 10 秒死锁与 `ctl.interface_start` 风暴**：在设置中开启蓝牙时界面卡顿，`dumpsys bluetooth_manager` 报 `SERVICE 'bluetooth_manager' DUMP TIMEOUT (10000ms) EXPIRED`，`init` 每秒刷屏 `Unable to set property "ctl.interface_start" to "aidl/vendor.samsung.hardware.bluetooth.ISehBluetooth/default"`。
3. **主桌面 1440×3120 10-bit HEVC 动态壁纸纯黑**：S25 Ultra 原厂默认动态壁纸（`com.samsung.android.wallpaper.live/.infinity.InfinityWallpaper`，1440×3120 @ 120fps Main10 HEVC 视频流）解码失败，主屏幕背景退化为纯黑底色（见 `fail-moneta-black-wallpaper.jpg`）。
4. **前台连续崩溃弹窗与后台安全/SELinux 风暴**：
   - 进入桌面及设置页连续弹出 **“要清除AI Core的缓存吗？”**（`com.samsung.android.offline.languagemodel.chn`）与 **“‘个人数据智能服务’屡次停止运行”**（`com.samsung.android.smartsuggestions:moneta`）；
   - `keystore2` 反复报错 `SehKeyMintExtension must be present: system/security/keystore2/src/globals.rs:633`；
   - Moneta 初始化数据库触发 `vold` / StrongBox `HARDWARE_TYPE_UNAVAILABLE (-68)` 3300+ 次及 `secdiscard` 风暴，连带拉起 58 次 `/system/bin/dumpstate` 吃满全机 CPU 与磁盘 I/O；
   - `UwbService` 每秒刷屏请求 `android.hardware.uwb.IUwb/default`（900+ 次），`HYPER_NATIVE_INTF` 刷屏 `Failed to get ISehHyPer`；
   - `com.samsung.android.themecenter`、`com.samsung.aasaservice` 因缺 vendor 侧 SELinux 规则崩溃，`bootanim` / `platform_app` 访问 `/omr` 持续刷 `avc: denied`。

## 二、根因

1. **OneUI `SemRIL` 绕过 AOSP `IRadioSim` 直调三星私有 `ISehRadio*` 全家桶**：
   反编译客机 `telephony-common.jar` 与 `framework.jar` 确认：OneUI 的 `SemRIL.getIccCardStatus` 不走 AOSP `IRadioSim.getIccCardStatus`，而是经 `SehRadioSimProxy` 直接连接 `vendor.samsung.hardware.radio.sim.ISehRadioSim/slot1..2`，由 `SehSimResponse.getIccCardStatusResponse(SehRadioResponseInfo, SehCardStatus)` 构造卡状态喂给 `UiccController`；同时状态栏信号格并非读 AOSP `SignalStrength`，而是消费 `ISehRadioNetworkIndication` 的 TX `14`（`signalBar` 推送 `SehSignalBar{gsmLevel, wcdmaLevel, tdscdmaLevel, lteLevel, nrLevel}`）。缺失精确冻结版 `VERSION` / `HASH` 与 Parcel 序列化实现时，`SemRIL` 判定 `mHalVersion=-1.-1`，SIM 恒处于未就绪态、信号格恒为 0。
2. **OneUI `BluetoothManagerService` 同步阻塞等待 `ISehBluetooth/default`**：
   客机 `/apex/com.android.bt/javalib/service-bluetooth.jar` 与 `system_server` 在开启蓝牙路径先查找 `vendor.samsung.hardware.bluetooth.ISehBluetooth/default`（AIDL v1），未注册时每次 `waitForService` 阻塞 1 秒并重试 10 轮，直接拖死 `bluetooth_manager` 锁并触发 `ctl.interface_start` 风暴。
3. **2K+ 10-bit HEVC 动态壁纸硬解卡在 `virtio_media` 影子缓冲上限与 `ueventd.rc` 加载路径两处**：
   - **内核 `VIRTIO_SHADOW_BUF_SIZE` 过小**：`c2.v4l2.hevc.decoder` 解码 `1440×3120` 10-bit HEVC 时分配 `8 MiB` `USERPTR` 码流缓冲，离散物理页约 `2048` 页，构造 `virtio_media_sg_entry`（`16 B/页`）需 `32 KiB`，超过 `devices/modules/virtio_media/session.h` 上游默认的 `0x4000`（`16 KiB`），导致 `VIDIOC_QBUF` 直接失败返回 `-ENOMEM (-12)`；
   - **基座 `/vendor/etc/ueventd.rc` 不 `import ueventd.qcom.rc`**：`lineage-22.2` 基座的 `/vendor/etc/ueventd.rc` 无 `import /vendor/etc/ueventd.${ro.hardware}.rc` 语句，`ueventd` 冷启动仅解析 `/vendor/etc/ueventd.rc` 本体，此前写在 `ueventd.qcom.rc` 的节点规则从未生效，导致 `/dev/video0`、`/dev/video1` 保持 `0600 root:root`，以 `media` 用户运行的 `android.hardware.media.c2@1.2-service-v4l2-64` 打开设备报 `EACCES`；此外官方顶层 `/vendor/etc/media_codecs.xml` 未 `<Include href="media_codecs_c2.xml"/>`，需在装配阶段自动兜底注入以确保 `MediaCodecList` 暴露 `c2.v4l2.*`。
4. **三星 `keystore2` 扩展强校验、不实真机 Feature XML 污染与强绑 NPU/Knox 的预装特权应用**：
   - 三星定制 `keystore2` 启动时调用 `ISehKeyMintExtension/default`（v4）的 `getLastFailedRot()`（TX `3`），且 `plat_sepolicy` 的 `neverallow` 严格限制只有 `hal_keymint_server` 域能注册 `hal_keymint_service`（通用占位进程无法代注册）；
   - `vendor/config.json` 整目录替换官方 `etc/permissions` 时一并带入了 `android.hardware.strongbox_keystore.xml`、`uwb.xml`、`ese.xml`、`satellite.xml`、`vulkan.*.xml` 等 14 份云机不存在的硬件特性声明，诱使 Moneta 请求 StrongBox 硬件密钥失败而陷入崩溃 + `dumpstate` 风暴；
   - `/system/priv-app/{AICoreCHN,SemanticSearchCore,SamsungSmartSuggestions}` 硬依赖端侧离线大模型 NPU、GMS `GoogleApiAvailability` 与 Knox 硬件密钥，在无物理 NPU/TEE 的云机上必然循环崩溃弹窗。
5. **官方 vendor 侧 SELinux 策略缺口**：
   三星将 `/data/overlays` 的 `themecenter_data_file` 标签授权、`aasa_service_app` 对 `app_api_service` / `asks_service` / `radio_service` 的 `find` 权限、`system_app`（`com.sec.imsservice`）与 `hal_radio_default` 的双向 Binder 调用、以及 `bootanim` / `platform_app` 对 `omr_file` 的读取权限均置于官方 `vendor_sepolicy.cil` 中，lineage 基底缺失导致对应组件权限拒绝或启动 NPE。

## 三、证据

修复前典型报错（完整采样见 `log-fail.txt`）：

```text
# 1) SemRIL ISehRadio* 缺失（mHalVersion=-1.-1）
D SehRadioSimProxy: AIDL initialized mHalVersion=-1.-1
E SEM_RILJ: getSehRadioServiceProxy: serviceProxy == null [PHONE0]
D SehRadioNetworkProxy: AIDL initialized mHalVersion=-1.-1

# 2) 蓝牙 10s 超时死锁与接口风暴
*** SERVICE 'bluetooth_manager' DUMP TIMEOUT (10000ms) EXPIRED ***
W libbinder.ServiceManagerCppClient: Waited one second for vendor.samsung.hardware.bluetooth.ISehBluetooth/default
W libc : Unable to set property "ctl.interface_start" to "aidl/vendor.samsung.hardware.bluetooth.ISehBluetooth/default": PROP_ERROR_HANDLE_CONTROL_MESSAGE (0x20)

# 3) keystore2 缺扩展、Moneta / AI Core / ThemeCenter 崩溃与 dumpstate 风暴
E keystore2: SehKeyMintExtension must be present: system/security/keystore2/src/globals.rs:633: In get_keymint_extension.
E AndroidRuntime: Process: com.samsung.android.smartsuggestions:moneta ... Unable to get provider com.samsung.android.moneta.app.MonetaInitializationProvider
E AndroidRuntime: Process: com.samsung.android.offline.languagemodel.chn ... java.lang.NoClassDefFoundError: Failed resolution of: Lcom/google/android/gms/common/GoogleApiAvailability;
E AndroidRuntime: Process: com.samsung.android.themecenter ... java.lang.IllegalStateException: openDatabase must be called before calling this method
I dumpstate: ... args='/system/bin/dumpstate -M -d -z', bugreport_mode= BUGREPORT_A_ERROR
```

从客机 DEX 与 `/system/lib64/vendor.samsung.hardware.*-ndk.so` 逆向取证对齐的接口契约摘要：

| 变体模块 | 服务实例名 | 版本 / Interface Hash | 关键 Transaction 行为 |
| :--- | :--- | :--- | :--- |
| `radio@samsung` | `ISehRadioSim/{slot1,slot2}`、`ISehRadioData/{slot1,slot2}`、`ISehRadioNetwork/{slot1,slot2}`、`ISehRadioMessaging/{slot1,slot2}`、`ISehRadioBridge/{slot1,slot2}`、`ISehRadioChannel/{epdgd,epdgd2,imsd,imsd2}`、`ISehRadioSatellite/default`（共 15 个实例） | Sim v2 (`e94363ef…`)、Data v3 (`e3b3d94c…`)、Net v3 (`0fc15a1f…`)、Msg v1 (`c1a8596d…`)、Bridge v1 (`514415cd…`)、Sat v2 (`0fa24f61…`)、Channel v1 (`notfrozen`) | `ISehRadioSim` 实时绑定宿主 `StateStore` 回传 `SehCardStatus`（USIM AID + ATR）；`ISehRadioNetwork` 连接后主动经 Ind TX `14` 推送 4 格满格 `SehSignalBar` |
| `bluetooth@samsung` | `vendor.samsung.hardware.bluetooth.ISehBluetooth/default` | v1 (`notfrozen`) | 同进程实现 TX `1..4`（`writeFile` / `readFile` / `setProperty` / `getProperty` 内存态存取） |
| `keymint@samsung` | `vendor.samsung.hardware.keymint.ISehKeyMintExtension/default` | v4 (`1bd2a0e0a1dab05878a25215197138e81df073ec`) | 经 `-Wl,--wrap=ABinderProcess_joinThreadPool` 在 `hal_keymint_default` 同进程挂载：TX `1` `attestKey` 回 `-66`（优雅回退标准 KeyMint）、TX `3` `getLastFailedRot` 回空 `byte[0]` |
| `neuralnetworks@samsung` | `vendor.samsung.hardware.hyper.ISehHyPer/default` | v2 (`a441ea0d6fcf9a8d771c17914859ac86346259a5`) | 应答 TX `1..9`（`acquire`/`release` oneway 放行，频率/资源探针回空或 `false`） |

## 四、改动

所有改动均落在 HAL 变体、内核外部模块与 `pa3q` 机型清单层，严格遵守官方 `super.img` 零改动约束（通过 `patch.img` + `cdh_redirect` 生效）：

1. **新增四组三星专属 HAL 变体并接入流水线**：
   - `devices/hardware/android.hardware.radio/src/samsung/radio_samsung_ext.{h,cpp}` + `src/main.cpp`：实现 15 个 `ISehRadio*` 服务实例并与 `StateStore` 联动；
   - `devices/hardware/android.hardware.bluetooth/src/samsung/bt_samsung_ext.{h,cpp}` + `variants/samsung/...`：同进程注册 `ISehBluetooth/default` 及配套 VINTF/rc；
   - `devices/hardware/android.hardware.keymint/src/samsung/keymint_samsung_ext.cpp` + `variants/samsung/...`：在 `hal_keymint_default` 域内 wrap 注册 `ISehKeyMintExtension/default`；
   - `devices/hardware/android.hardware.neuralnetworks/src/samsung/nn_samsung_ext.cpp` + `variants/samsung/...`：注册 `ISehHyPer/default`；
   - `agent/modules/pipeline/stages/08_build_hardware/modules/android.hardware.{radio,bluetooth}.json` 与 `devices/manifest/configs/samsung/pa3q/pa3q-android15/hardware/config.json`：切入 `radio@samsung`、`bluetooth@samsung`、`keymint@samsung`、`neuralnetworks@samsung`。
2. **打通 1440×3120 10-bit HEVC 动态壁纸 V4L2 硬解链路**：
   - `devices/modules/virtio_media/session.h`：将 `VIRTIO_SHADOW_BUF_SIZE` 由 `0x4000`（16 KiB）扩容至 `0x20000`（128 KiB，支持最高 32 MiB 离散页 `USERPTR` 缓冲），重编 `cdh_media.ko`；
   - `devices/manifest/configs/samsung/pa3q/pa3q-android15/vendor/etc/ueventd.rc`：同名覆盖基座 `/vendor/etc/ueventd.rc`（删除无效的旧 `ueventd.qcom.rc`），补齐 `/dev/video*`、`/dev/media*`、`/dev/v4l-subdev*` `0660 media camera`；
   - `agent/modules/pipeline/stages/09_build_vendor/{media_codecs_c2.go,media_codecs_c2_test.go}` + `devices/hardware/android.hardware.media/src/mediaserver_64_service.cpp`：当厂商顶层 `media_codecs.xml` 未引用 `media_codecs_c2.xml` 时自动注入 `<Include>` 兜底，并修复 C++17 构建兼容。
3. **裁剪不实硬件 Feature XML 并通过 `cdh_redirect` 屏蔽 NPU/Knox 必崩应用**：
   - `devices/manifest/configs/samsung/pa3q/pa3q-android15/vendor/config.json`：在 `etc/permissions` 替换后新增 14 条 `delete`（移除 `strongbox_keystore`、`secure_key_service`、`app_attest_key`、`device_id_attestation`、2 项 `uwb`、2 项 `ese`、`satellite` 及 5 项 `vulkan` XML）；
   - `devices/manifest/configs/samsung/pa3q/pa3q-android15/module/config.json`：`redirect` 表追加 `/system/priv-app/AICoreCHN`、`/system/priv-app/SemanticSearchCore`、`/system/priv-app/SamsungSmartSuggestions` 指向 `/mnt/vendorpatch/system/priv-app/...` 空目录（总规则数 18 条 ≤ `CDH_RULES_MAX = 32`）。
4. **补齐三星 vendor SELinux 策略片段**：
   - `devices/manifest/configs/samsung/pa3q/pa3q-android15/vendor/sepolicy/{vendor_sepolicy.cil,vendor_file_contexts}`：新增第十二段（`system_app` ↔ `hal_radio_default` Binder 互调）与第十三段（`/data/overlays` `themecenter_data_file` 类型及目录转换、`bootanim`/`platform_app`/`priv_app`/`system_app`/`system_server` 访问 `omr_file`、`aasa_service_app` `find` `app_api_service`/`asks_service`/`radio_service`）。

## 五、验证

1. **单测与镜像静态回读**：
   - `go test ./stages/06_build_vendor_boot ./stages/08_build_hardware ./stages/09_build_vendor` 全部通过；
   - `go run ./cmd/pipeline device build -from 09_build_vendor pa3q` 完成构建发布，`debugfs` 回读 `vendor.img` 确认 `/etc/ueventd.rc` 含 `/dev/video* 0660 media camera` 且旧 `/etc/ueventd.qcom.rc` 已移除；`vendor_dlkm.img` `/lib/modules` 仅含 11 个 `6.6.127-4k-cdh` 自编 `.ko`（含 76,976 B 新 `cdh_media.ko`）及全量重建索引。
2. **清空可写层冷启动实机终验**（`pa3q_16-AP3A.240905.015.A2-20260812` 实例 `#1`，删除 `writable-*.qcow2` 与 `data.qcow2`，零人工改权限，完整输出见 `log-pass.txt`）：
   - **起机与节点权限**：106s 达 `sys.boot_completed=1`，`getenforce=Enforcing`；`/dev/video0`、`/dev/video1` 开机即为 `crw-rw---- 1 media camera u:object_r:video_device:s0`；`/data/overlays` 与 `/data/overlays/preferences` 标签分别为 `themecenter_data_file` 与 `themecenter_sensitive_data_file`。
   - **10-bit HEVC 动态壁纸硬解**（`pass-home.png`）：进入 OneUI 桌面即由 `c2.v4l2.hevc.decoder` 硬解渲染 `1440×3120` 120fps 钛黑流线型动态壁纸，`V4L2Decoder: Copied USERPTR CAPTURE staging to YV12 graphic 1440x3120 format=NV12`，`dmesg` 中 `virtio-media` 错误数为 **0**。
   - **满格蜂窝信号与 4G 数据拨号**（`pass-home.png`、`pass-4g.png`）：15 个 `ISehRadio*` 实例全部在线，`gsm.sim.state=LOADED`、`gsm.operator.alpha=中国联通`（`subId=1`），`SignalBarInfo{gsmLevel=4 wcdmaLevel=4 lteLevel=4 nrLevel=4}` 状态栏 **4 格满格**；关闭 Wi-Fi 开启移动数据时 `ccmni1` 自动拨通获取 `100.64.155.15/24`，状态栏亮起 **`4G ↑↓`**；恢复开启 Wi-Fi 后自动连回热点且双网状态正常。
   - **蓝牙无死锁秒开**（`pass-bluetooth.png`）：`ISehBluetooth/default` 在位，打开蓝牙开关毫秒级切换 `enabled: true / state: ON`，无 10s 超时，蓝牙设置页正常扫描显示 `CDH-Headset-Pro`、`CDH-Speaker-Mini`、`CDH-Watch-S3`。

修复前后关键量化指标对比（已排除 `adbd` 命令回显）：

| 核验项 | 修复前（`log-fail.txt`） | 修复后（`log-pass.txt`） |
| :--- | :--- | :--- |
| SIM 状态 / 状态栏信号格 | 未就绪（`mHalVersion=-1.-1`）/ 0 格空心 | `LOADED`（中国联通 `subId=1`）/ **4 格满格 + `4G ↑↓`** |
| 蓝牙开启耗时与 `ISehBluetooth` `ctl.interface_start` 风暴 | 10000ms 超时死锁 / 持续刷屏 | **毫秒级 `state: ON`** / **0** |
| 主桌面 1440×3120 10-bit HEVC 动态壁纸 | 纯黑背景（`EACCES` + `VIDIOC_QBUF -12`） | **`c2.v4l2.hevc.decoder` 自动硬解正常渲染** |
| `keystore2` `SehKeyMintExtension must be present` | 报错触发异常 | **0**（`Successfully started Samsung Keystore service`） |
| `Failed to get ISehHyPer` / `IUwb` `ctl.interface_start` 风暴 | 刷屏 / 900+ 次 | **0** / **0** |
| StrongBox `-68` 风暴 / `dumpstate` 拉起次数 | 3300+ 次 / 58 次 | 仅启动期常规探测 9 次（无 `secdiscard` 风暴）/ **0 次** |
| 前台“AI Core”与“个人数据智能服务”崩溃弹窗 | 必现双弹窗 | **0 弹窗** |
| `logcat -b crash -d` 崩溃总行数 | 多进程循环 FATAL EXCEPTION | **0 行（整机零崩溃）** |
| `omr_file` / `video_device` / `themecenter` / `aasa_service_app` AVC | 持续拒绝 | **全部 0** |

## 六、遗留

1. **三星扩展密钥证明回退口径**：`keymint@samsung` 的 `ISehKeyMintExtension::attestKey`（TX `1`）当前按无三星私有 Knox 根证书状态返回 `ServiceSpecificError(-66, ATTESTATION_KEYS_NOT_PROVISIONED)`，使 `keystore2` 优雅回退至标准 AOSP Remote KeyMint 证书链（见 `../../common/issue-tee-attestation-tri-state-and-avb-rkp-v3/`）；若后续业务需通过三星专属 SAK（Samsung Attestation Key）校验，可进一步接入 SAK 证书链。
2. **端侧 NPU 离线 AI 组件屏蔽边界**：`AICoreCHN`、`SemanticSearchCore`、`SamsungSmartSuggestions` 因强耦合高通 Hexagon NPU 与 Knox 硬件密钥已通过 `cdh_redirect` 置空目录屏蔽，不影响桌面、设置、网络、蓝牙、相机与常规三方应用，但 OneUI“Galaxy AI 端侧语义搜索/智能建议”入口在云机中不提供服务。
3. **首次启动向导（`SecSetupWizard`）策略**：本次冷启动清空 `data.qcow2` 后通过 `device_provisioned=1` + `user_setup_complete=1` 进入桌面验收，与 `../issue-knoxguard-china-region/` 口径一致。
