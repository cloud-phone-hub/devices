# #185 BLU G75（blu/G1170）Android 15 云机全链路制作、硬件身份对齐与 Coosea 像素插值相机修复

- **设备/机型**：BLU G75（`blu/G1170`，MT6768 / Helio G81，Coosea ODM `k68v1_64` / Android 15，基线 `15-AP3A.240905.015.A2-20260209`，`lineage-22.2` + `kernel 6.6`）
- **状态**：已闭环（2026-11-11）

## 一、问题

基于官方固件包 `roms/devices/blu/BLU-G75-G1170.zip` 制作 BLU G75（`blu/G1170`）Android 15 云机并进行全功能验证过程中，依次解决以下 5 个问题：

1. **初版配置处于占位态，硬件规格与虚拟通道未对齐真机**：
   - `G1170.json` 中 `platform` 为空、显示参数为占位值 `1080x2400 @ 440dpi`（真机面板为 `panel-jingtai-gc7272` `720×1640 @ 320dpi`）、缺少 `hostHvc` 7 条主机虚拟串口通道与 `verifiedBootHash`；
   - `G1170-android15.json` 缺少 MT6768（6×Cortex-A55 @ 1.8GHz + 2×Cortex-A75 @ 2.0GHz）16 档 CPU 频点表、13 个 MT6768 温区、触控屏 `gcore` + `mtk-kpd` 输入身份、`Mali-G52 MC2` GPU 指纹、双卡 MTK Radio / 17 颗真机传感器（无物理陀螺仪）/ Widevine CDM v19 L3 / FRP 块设备软链与蓝牙默认名称。
2. **原厂 `netdiag` 服务每 5 秒退出一次触发 `updatable_crashing` 告警**：
   - 二阶段启动后 `init` 持续打印 `process with updatable components 'netdiag' exited 4 times in 4 minutes`。
3. **原厂 `media_codecs_c2.xml` 含 UTF-8 BOM 且引用缺失的杜比解码表**：
   - 原厂 `/vendor/etc/media_codecs_c2.xml` 头部带 3 字节 BOM（`EF BB BF`），原厂 `media_codecs.xml` 包含 `<Include href="media_codecs_dolby_audio.xml" />`，直接复用会导致 Codec2 XML 解析失败。
4. **引入原厂单体 `/vendor/lib64/egl/libGLES_meow.so` 导致 `surfaceflinger` 空指针崩溃循环**：
   - 将原厂 `/vendor/lib64/egl/libGLES_meow.so` 列入 `vendor.files` 后，`surfaceflinger` 与 `zygote` 在 `dlopen` 图形驱动构造器阶段触发 `MEOW::Loader::load_driver` 空指针解引用（`SIGSEGV, fault addr 0x0`），无法进入桌面。
5. **系统相机 `com.mediatek.camera` 启动即闪退（`IllegalArgumentException: sizes was empty`）**：
   - 进入桌面后启动原厂相机 `com.mediatek.camera/.CameraActivity`，在 `FeatureSetting-Handler Thread` 调用 `CameraCharacteristics.getKeys()` 时于 `MandatoryStreamCombination$Builder.generateAvailableCombinations(MandatoryStreamCombination.java:2009)` → `getMaxSize(MandatoryStreamCombination.java:2202)` 抛出 `java.lang.IllegalArgumentException: sizes was empty`。

## 二、根因

1. **真机硬件真值提取**：
   - 解包官方固件 `super.img`（`system` / `system_ext` / `product` / `vendor` / `vendor_dlkm` / `odm`）与 `vendor_boot.img` DTB（`mt6768.dtb` + Coosea overlay），实测确认真机 SoC 为 `MT6768`（Helio G81），屏幕为 `720×1640 @ 320dpi`（物理尺寸 `70×156 mm`，支持 `60/90/120Hz`），GPU 为 `Mali-G52 MC2`（`OpenGL ES 3.2 v1.r32p1-01eac0.19498e0ae1d5dac223383c39a2e58f04`），纯 64 位 `arm64-v8a`，传感器共 17 项（加速度计 `mc34x9`、磁力计 `mmc5603`、光感 `stk6a7x_l`、距感 `stk6a7x_p` 及 13 项 MTK 融合传感器，无物理陀螺仪），内核 release 为 `6.6.66-android15-8-g1a5efa12ec93-ab13070261-4k`。
2. **`netdiag` 权限拒绝与服务关停**：
   - 原厂 `/system/etc/init/netdiag.rc` 启动的 MTK 网络诊断守护进程依赖 MTK 专有内核 netlink / vendor_default_prop 权限，云机环境无需运行，在 `/vendor/etc/init/cdh_g1170.rc` 的 `on post-fs-data` 与 `on property:sys.boot_completed=1` 中 `stop netdiag` 即可干净停用。
3. **多媒体配置清理**：
   - 去除 `media_codecs_c2.xml` 首部 UTF-8 BOM 并裁剪不可用硬件 C2 组件，移除 `media_codecs.xml` 中的 `media_codecs_dolby_audio.xml` 引用后交由虚拟 `cdh_media` C2 HAL 承载编解码。
4. **单体 `libGLES_meow.so` 与 CDH 虚拟 GPU 三件套冲突**：
   - Android `libEGL` 在 `ro.hardware.egl=meow` 时优先尝试加载单体 `/vendor/lib64/egl/libGLES_meow.so`，仅在其不存在时才加载分体三件套 `libEGL_meow.so` + `libGLESv1_CM_meow.so` + `libGLESv2_meow.so`；原厂单体 `libGLES_meow.so` 在全局构造器 `load_file() -> MEOW::DDKHook::load() -> MEOW::Loader::load_driver()` 中硬编码查找真机 Mali DDK 符号失败后解引用空函数指针崩溃。移除单体 `libGLES_meow.so` 后，框架正常加载 CDH 虚拟 GPU 三件套。
5. **Coosea/BLU `framework.jar` 像素插值原地覆写 `SCALER_AVAILABLE_STREAM_CONFIGURATIONS` 末尾槽位**：
   - 官方 `odm/etc/build.prop` 配置了像素插值属性：`ro.odm.camera.insert.resolution=1`、`ro.odm.camera.rear.resolution=50M`、`ro.odm.camera.front.resolution=13M`、`ro.odm.camerafoursizes=1`。
   - 反编译 G1170 `/system/framework/framework.jar` 的 `android.hardware.camera2.CameraManager.getCameraCharacteristics()`（第 389~570 行）确认：当调用方包名命中白名单（三方验机软件覆写 `configurationsInsert[0]`；系统相机 `com.mediatek.camera` 覆写数组末尾 `configurationsInsert[length - 1]` 至 `configurationsInsert[length - 5]`）时，框架直接在原数组副本上把末尾 5 个四元组覆写为插值 `BLOB (33)` 输出尺寸（如 `8192×6144`）。
   - 此前虚拟相机 HAL `camera_provider_service.cpp` 在 `buildStaticMetadata()` 中将单条 `kRawSensor (32)` 输出流放在倒数第 2 槽、单条 `kYCbCr420888 (35)` **输入流**放在最后 1 槽（`length - 1`）。`com.mediatek.camera` 调用时末尾 5 槽被原地改成 `BLOB` 输出流，导致唯一的 `YUV_420_888` 输入流与 `RAW_SENSOR` 输出流被彻底抹掉，而 `REQUEST_AVAILABLE_CAPABILITIES` 仍包含 `YUV_REPROCESSING (7)` 与 `RAW (3)`；随后 `MandatoryStreamCombination` 在第 2009 行执行 `getMaxSize(mStreamConfigMap.getInputSizes(35))` 时拿到空数组，直接抛出 `IllegalArgumentException: sizes was empty`。
   - 将多档位（后摄 50 项、前摄 29 项）且不参与 `MandatoryStreamCombination` 构建的 `kYV12` 输出流移至 `streamConfigurations` 数组末尾后，首槽仍为 `kBlob` 最大档位，末尾 5 槽均为低分辨率冗余 `YV12` 档位（`320×240` ~ `160×96`），单条 `kRawSensor` 输出流与 `kYCbCr420888` 输入流安全处于数组中部，既保留 Coosea 50MP/13MP 插值特性，又对所有标准 AOSP 机型无副作用。

## 三、证据

- 失败与通过日志：[log-fail.txt](log-fail.txt)、[log-pass.txt](log-pass.txt)
- 实测截图：
  - 冷启动原生 Launcher3 桌面（`720×1640 @ 320dpi`）：[desktop.png](desktop.png)
  - 定制关于本机页上半屏（`Device name: G75`、`CPU: MediaTek Helio G81`、`128GB`、`8GB+8GB RAM`、SIM1 号码）：[about-device-1.png](about-device-1.png)
  - 定制关于本机页上滑下半屏（`SIM status: CU`、`Model: G75`、双槽 IMEI、`Android version: 15`）：[about-device-2.png](about-device-2.png)
  - 系统相机 `com.mediatek.camera` 正常启动进入拍摄界面：[camera-preview.png](camera-preview.png)

## 四、改动

1. **设备清单与版本清单**：
   - `devices/manifest/devices/blu/G1170.json`：设置 `platform: "mtk"`、`resolution: "720x1640"`、`inch: "70x156"`、`density: 320`、`extra.board: "mt6768"`、7 条 `extra.hostHvc`（含 `hvc2` ~ `hvc8`）与官方 `verifiedBootHash`。
   - `devices/manifest/versions/blu/G1170/15-AP3A.240905.015.A2-20260209.json`：补充 `extra.fingerprint: "BLU/G75/G1170:15/AP3A.240905.015.A2/1770610308:user/release-keys"`。
2. **机型 Android 15 配置模版**：
   - `devices/manifest/configs/blu/G1170/G1170-android15.json`：
     - 配置 MT6768 大小核各 16 档 `cpufreq`、13 个 `thermal` 温区、`gcore`（`abs-to-mt: true`）+ `mtk-kpd` 输入映射、`/odm/etc/build.prop` 重定向；
     - 收口 44 项 `build-prop-sets`（`Mali-G52 MC2` GLES 版本串、`ro.hardware.egl=meow`、`ro.hardware.vulkan=mali`、`ro.hardware.hwcomposer=drm`、`ro.frp.pst=/dev/block/by-name/frp`、`bluetooth.device.default_name="BLU G75"`、`60,120,90` 刷新率、Widevine L3、纯 64 位 `arm64-v8a` 等）；
     - 装配 15 份 `fileTrees`（去 BOM `media_codecs_c2.xml`、去 Dolby `media_config.xml`、真机 17 传感器 `sensors_config.xml`、`gnss_config.xml`、`cdh_g1170.rc`、`frp_symlink.rc`、`ueventd.rc` 等）、12 项原厂 `vendor.files`（剔除单体 `libGLES_meow.so`，引入 Widevine v19 `libwvaidl.so`）、17 个虚拟 `hardware` 模块、`vintf-drop` 与 `vendor_sepolicy.cil`。
3. **虚拟相机 HAL 流配置顺序兼容加固**：
   - `engine/vendor/hardware/android.hardware.camera/src/camera_provider_service.cpp`（`buildStaticMetadata`）：将多档位 `kYV12` 输出流（及对应 `minFrameDurations` / `stallDurations`）移至 `kRawSensor` 与 `kYCbCr420888` 输入流之后作为数组尾部缓冲，避免 ODM 像素插值逻辑覆写单条输入流/RAW 流。

## 五、验证

- 单测回归：`cd engine/build && go test ./...` 全绿通过。
- 构建产物：`cdh device build G1170` 全阶段 `[ok]`，产出完整盘集至 `engine/out/dist/blu/G1170/15-AP3A.240905.015.A2-20260209/`。
- 实例冷启动与全栈功能实测（`G1170_15-AP3A.240905.015.A2-20260209/1`）：
  - `boot_check.py` 验收 `[pass]`（冷启动进入 Launcher3 桌面，VNC 抓帧 `720x1640` 平均亮度 `181.0`）；
  - 触控点击与滑动顺畅，`PriMyDeviceInfoActivity` 关于本机页完整展示 `G75`、`MediaTek Helio G81`、`128GB`、`8GB+8GB RAM`、双槽 IMEI 与 SIM1 联通在网；
  - 显示 `720×1640 @ 320dpi`，支持 `60/90/120Hz`（峰值 `120.00 Hz`），GPU 为 `ARM, Mali-G52 MC2, OpenGL ES 3.2 v1.r32p1-01eac0.19498e0ae1d5dac223383c39a2e58f04`；
  - 双卡基带在线（`LOADED,ABSENT`），`wlan0` 获取 `192.168.60.100/24` 且外网 `ping 223.5.5.5` `0% packet loss`；
  - 音频 `STREAM_MUSIC -> speaker(2)`、蓝牙名称 `BLU G75`、17 颗硬件传感器全部运行、CPU 大小核 16 档频点与 13 个温区正常读取、FRP `/dev/block/by-name/frp -> /dev/block/vda3` 正常就位；
  - `MediaDrm` Widevine CDM v19（`19.0.1@AV1A.240725.002`）L3 实例化成功；
  - `CameraManager` 前后双摄（`cameraIds=[0, 1]`）`getKeys()` 分别返回 130 / 125 项无异常，系统相机 `com.mediatek.camera/.CameraActivity` 正常打开进入拍摄界面、零崩溃。

## 六、遗留

- 跨机型虚拟相机 HAL（`Note13CameraSession::processCaptureRequest`）当前为无外部视频源的空缓冲回传实现（直接返回 `BufferStatus::OK` 的全零缓冲区，预览画面呈黑色背景，相机顶栏/底栏 UI 正常交互），属于引擎虚拟相机 stub 的既有通用行为。
