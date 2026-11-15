# #190：最后 5 项精细字段级指纹对齐（`mtk-kpd` 键盘类型 / GPU 上限与 ARM 扩展 / Audio 音效模式与 17 阶流音量 / Wi-Fi P2P & 5GHz `11ac` `433Mbps` / 屏幕精确 `xDpi·yDpi` 与 EDID `DeviceProductInfo`）

## 1. 问题

在完成 `gold`（`15-OS3.0.10.0.VNQCNXM`）`#172..#189` 主干与深水区闭环后，对真机与云机的全量 JSON 基线做逐字段细粒度扫描，盘点出最后 **5 项精细字段级指纹差异**：

1. **`mtk-kpd` 键盘类型与全局 `Configuration` 差异**：
   - 真机 `mtk-kpd`（联发科侧边音量/电源矩阵按键）的 `InputDevice.getKeyboardType()` 为 **`1` (`KEYBOARD_TYPE_NON_ALPHABETIC`)**，全局 `Configuration` 为 `keyboard = 1 (KEYBOARD_NOKEYS)`、`hardKeyboardHidden = 2 (HARDKEYBOARDHIDDEN_YES)`、`reqInputFeatures = 0`；
   - 云机 `virtio-keyboard` 经 `cdh_input_identity` 改名为 `mtk-kpd` 后仍保留了 `KEY_A..KEY_Z` 全键盘位图，被 Android `EventHub` 判定为 **`2` (`KEYBOARD_TYPE_ALPHABETIC` / QWERTY 物理全键盘)**，连带污染 `reqKeyboardType = 2`、`hardKeyboardHidden = 1`、`reqInputFeatures = 1`。
2. **GPU `glGetIntegerv` 6 项上限对普通 App 未生效及安全 `GL_ARM_*` 扩展少 3 项**：
   - 真机 Mali-G57 MC2（Valhall r49p1）的 6 项上限为 `maxTextureSize = 16383`、`maxCubeMapTextureSize = 16383`、`maxRenderbufferSize = 16383`、`maxVertexAttribs = 32`、`maxTextureImageUnits = 16`、`maxCombinedTextureImageUnits = 96`；
   - 云机在 [`#187`](../issue-gpu-vulkan-pnext-uuid-and-gles-limits/solution.md) 中虽写入了 `ro.vendor.cdh.gpu.max_*`，但普通 App（`untrusted_app`）在 `Enforcing` SELinux 下无法读取默认的 `vendor_default_prop`，导致 `cdh_gl_strings.c` 静默回退至硬编码默认值（`16384` / `16` / `32` / `192`）；同时相比真机少了 `GL_ARM_debug_assert`、`GL_ARM_shader_core_properties`、`GL_ARM_texture_unnormalized_coordinates` 3 项非着色器查询类 ARM 扩展。
3. **Audio 23 项音效 `connectMode`/UUID 与 MIUI 17 阶流音量及无权限闹钟最小音量差异**：
   - 真机 23 个音效中包含 3 个 `"Pre Processing"`（`Acoustic Echo Canceler`、`Automatic Gain Control V2`、`Noise Suppression`）与 2 个 `"Auxiliary"`（`Environmental Reverb`、`Preset Reverb`），且 `Extension Effect` UUID 尾字节为 `...0242ac120002`；云机 `EffectFactoryImpl` 硬编码全部返回 `EFFECT_FLAG_TYPE_INSERT`，且 `gold-android15.json` 中 `ExtensionEffectExample` UUID 误写为 `...0242ac120001`；
   - 真机普通 App 调用 `AudioManager.getStreamMaxVolume()` 得到 `music` / `ring` / `notification` **`maxVolume = 17`**（MIUI 超大音量特性），且无 `MODIFY_AUDIO_SETTINGS` 特权时 `STREAM_ALARM` **`minVolume = 6`**；云机返回 `maxVolume = 15`、`alarm.minVolume = 5`。
4. **Wi-Fi Direct (`isP2pSupported`) 缺失及默认连接频段/标准/速率差异**：
   - 真机 `WifiManager.isP2pSupported() = true`（系统态存在 `p2p0` 接口），关联 5 GHz 信道 36（`5180 MHz`），`Wi-Fi standard: 11ac`（`wifiStandard = 5`），`connectedLinkSpeedMbps = 433`；
   - 云机 `isP2pSupported = false`（`wlan1` 闲置未重命名为 `p2p0`、Wifi HAL 未上报 `P2P` 特性与 `createP2pIface`），且默认连接 2.4 GHz `2437 MHz`、`Wi-Fi standard: legacy`、`connectedLinkSpeedMbps = 18`。
5. **屏幕精确物理 `xDpi / yDpi`、`uniqueId` 与 EDID `DeviceProductInfo` 缺失**：
   - 真机 `DisplayMetrics` 物理像素密度为 **`xdpi = 394.40994`、`ydpi = 393.39185`**，`dumpsys display` 中 `uniqueId = "local:4627039422300187648"`，且包含完整 `DeviceProductInfo{name=MTKDEV, manufacturerPnpId=MTK, productId=35638, manufactureDate=ManufactureDate{week=42, year=2021}}`；
   - 云机 `hwc3-service.drm` 按整数毫米宽高算得 `xdpi = ydpi = 440.0`，且 `HwcDisplay::GetDisplayIdentificationData` 因虚拟连接器无 DRM `EDID` blob 直接返回不支持，致 `DeviceProductInfo = null`、`uniqueId = "local:0"`。

## 2. 根因

1. **`EventHub` 键盘类型分类判据**：Android `EventHub::classifyInputDeviceClasses` 只要检测到 `keyBitmask` 中存在任意字母键（`KEY_Q..KEY_P` / `KEY_A..KEY_Z`）或 `EV_REP` 重复标志，即归类为 `KEYBOARD_TYPE_ALPHABETIC (2)`。`cdh_input_identity` 接管 `virtio-keyboard` 伪装 `mtk-kpd` 时未裁剪 `evbit`/`keybit` 至纯非字母按键集合。
2. **SP-HAL 进程内属性读取受限于调用方 App SELinux 域**：OpenGL ES 驱动库 `libGLESv2_meow.so`（伪装为 `libGLES_mali.so`）作为 SP-HAL 加载在 `untrusted_app` 进程空间内执行，`__system_property_get("ro.vendor.cdh.gpu.*")` 受 `untrusted_app` 策略约束；未声明专用公开属性域并向 `appdomain` 授权 `get_prop` 时，属性读取恒返回空串。此外需严格排除 `GL_ARM_shader_framebuffer_fetch*`，否则 Skia/Chromium 会生成 `#extension GL_ARM_shader_framebuffer_fetch` 着色器触发底层 Mesa GLSL 编译错误。
3. **AIDL Audio 音效标志位、MIUI `VolumeBoostHelper` 与 `mIndexMinNoPerm` 曲线阈值**：
   - `EffectFactoryImpl` 解析 `audio_effects_descriptors.xml` 时未按音效 `type` UUID 区分 `PRE_PROC`（AEC/AGC/NS）与 `AUXILIARY`（EnvReverb/PresetReverb）；
   - MIUI `framework/services` 的 `VolumeBoostHelper` 在 `system_server` 读取 `ro.vendor.audio.odmvolume=true`、`ro.vendor.audio.volume_super_index_add=20`、`ro.vendor.audio.volume_super_streamtype=44`（bit 2/3/5），但这些属性原落入 `vendor_default_prop` 被 SELinux 拒绝 `system_server` 读取，必须打上官方同款 `u:object_r:vendor_misound_ro_prop:s0` 标签；
   - 反汇编 `services.jar` (`AudioService`) 证实 `STREAM_ALARM` 的无权限最小音量 `mIndexMinNoPerm` 由 `AudioSystem.getStreamVolumeDB(STREAM_ALARM, idx, DEVICE_OUT_SPEAKER) < -36.0f` 决定；AIDL Audio HAL 从 `/vendor/etc/audio_config.xml` 加载 `DEFAULT_NON_MUTABLE_SPEAKER_VOLUME_CURVE`，原 `(20, -4000), (60, -1700)` 曲线在 `idx=5`（`28.57%`）处为 `-35.07 dB >= -36.0 dB`，导致阈值为 `5` 而非 `6`。
4. **Wi-Fi P2P 接口拓扑与 `mac80211` 5 GHz VHT80 + WMM QoS 协商约束**：
   - `WifiChip` 未在芯片特性集中置位 `WIFI_FEATURE_P2P`、未实现 `createP2pIface`/`getP2pIface`/`removeP2pIface`，且 `mac80211_hwsim` 第二个无线电实例 `wlan1` 在 `post-fs` 阶段未被重命名为 `p2p0`（若仅在 `hal_wifi_default` 内调 `SIOCSIFNAME` `0x8923` 会被平台 `udp_socket` `allowxperm` 拦截）；
   - `mac80211_hwsim` 假 AP 在 5 GHz（`5180 MHz`）关联时：① 不能在 Supported Rates 中携带 1/2/5.5/11 Mbps CCK Basic Rate（5 GHz sband 无 CCK 速率会导致关联被拒）；② Beacon 与 AssocResp 必须同时携带 **Microsoft WMM Parameter IE**（`00:50:f2:02:01:01`）与 `HT Cap/Oper` + `VHT Cap/Oper (80 MHz)`——内核 `net/mac80211/mlme.c` 若发现 `!bss->wmm_used` 会强制打印 `disabling HT/VHT/HE as WMM/QoS is not supported by the AP` 并降级为 `legacy`；③ `HWSIM_COMMON_OPS` 需实现 `.sta_statistics` 上报 `VHT 80MHz MCS9 NSS1 SGI`（`433.3 Mbps` → Android `433 Mbps`）。
5. **`hwc3-service.drm` 物理 DPI 计算与 `GetDisplayIdentificationData` EDID 解析链**：
   - `ComposerClient::getDisplayConfigurations` 原逻辑仅按整数毫米宽度算一个 `s0` 并用 `stp s0, s0, [sp, #0xc]` 同时填入 `dpi.x` 和 `dpi.y`；
   - Android 15 `SurfaceFlinger` (`DisplayIdentification.cpp`) 通过 `HwcDisplay::GetDisplayIdentificationData` 读取 128 字节标准 EDID v1.4：从中解析 EISA 3 字母厂商码（`"MTK"` = `0x368b`）、小端 `productId`（`35638`）、制造周/年（`week=42, year=2021`）与显示器名称描述符（`"MTKDEV"`）构造 `DeviceProductInfo`，并由 `(hasStableEdid=1 << 62) | (manufacturerId=0x368b << 48) | port(0)` 合成稳定物理屏幕 ID `0x40368b0000000000 = 4627039422300187648`（即 `"local:4627039422300187648"`）。

## 3. 证据

- 修复前失败日志见 [`log-fail.txt`](log-fail.txt)：
  - `mtk-kpd` `keyboardType = 2`，`reqKeyboardType = 2`，`reqInputFeatures = 1`；
  - `gles.limits` 回退为 `maxTextureSize = 16384`、`maxVertexAttribs = 16`、`maxTextureImageUnits = 32`、`maxCombinedTextureImageUnits = 192`；`GL_ARM_*` 仅 3 项；
  - Audio 5 个预处理/辅助音效 `connectMode = "Insert"`、`Extension Effect` UUID 尾字节为 `...0001`，`music/ring/notification maxVolume = 15`、`alarm minVolume = 5`；
  - Wi-Fi `isP2pSupported = false`、`connectedFrequencyMhz = 2437`、`connectedLinkSpeedMbps = 18`；
  - `display.realMetrics` 中 `xdpi = 440`、`ydpi = 440`，无 `DeviceProductInfo`。
- 修复后通过日志见 [`log-pass.txt`](log-pass.txt)（全新干净盘实例 `gold_15-OS3.0.10.0.VNQCNXM/1`，`device-checker` ID `3d737b9b5d823eec-cloud`，批次 `run-00279` / `run-00280` / `run-00281`）：
  1. **输入设备**：`dumpsys input` 中 `mtk-kpd` `KeyboardType: 1`，`am get-config` 含 `nokeys`；App 视角 `mtk-kpd.keyboardType = 1`、`deviceConfigInfo = {reqInputFeatures: 0, reqKeyboardType: 1, reqNavigation: 1, reqTouchScreen: 3}`、`configuration = {keyboard: 1, hardKeyboardHidden: 2}`，与真机 **100% 一致**；
  2. **GPU 能力与安全扩展**：`getprop -Z ro.vendor.cdh.gpu.max_texture_size` 为 `u:object_r:cdh_gpu_public_prop:s0`；App 视角 `gles.limits` 6 项（`16383 / 16383 / 16383 / 32 / 16 / 96`）与真机字典 `==` 完全相等，6 项安全 `GL_ARM_*` 生效，WebView WebGL 正常渲染无任何 GLSL 报错；
  3. **Audio 音效与流音量**：`getprop -Z ro.vendor.audio.odmvolume` 为 `u:object_r:vendor_misound_ro_prop:s0`；`dumpsys audio` 中 `STREAM_ALARM` 为 `Min: 1 w/o perm:6`；App 视角整个 `audio` 对象（23 项音效 `connectMode`/UUID + 全部 8 路音量流 `music/ring/notification maxVolume = 17`、`alarm minVolume = 6`）与真机 `==` **逐字节完全相等**；
  4. **Wi-Fi Direct & 5 GHz `11ac` `433 Mbps`**：系统态 `ip link show p2p0` 存在（MAC `76:39:89:0b:24:65`，`wlan1` 已移除，零 `SIOCSIFNAME` AVC），`cmd wifi status` 关联 `TP-LINK_2464` @ `5180MHz`、**`Wi-Fi standard: 11ac`**、**`Link speed: 433Mbps`**；App 视角 `isP2pSupported = true`、`connectedFrequencyMhz = 5180`、`connectedLinkSpeedMbps = 433`、`activeIfaces` 保持干净 5 项；
  5. **屏幕精确 DPI & EDID**：`dumpsys display` 中 `uniqueId = "local:4627039422300187648"`、`394.40994 x 393.39185 dpi`、`DeviceProductInfo{name=MTKDEV, manufacturerPnpId=MTK, productId=35638, week=42, year=2021}`；App 视角 `xdpi = 394.41`、`ydpi = 393.392`，与真机 **100% 完全一致**；
  6. **全绿回归**：`aligner_xiaomi_gold.all` 保持 **`32 / 32` (`100%`)**（`failedItems = []`），全部 14 个 `ultimate.*` 终极风控探针 `cloudOnlyHits = []`（零云机独有命中）。

## 4. 改动

1. **`cdh_input_identity` 裁剪 `mtk-kpd` 键位位图为非字母矩阵按键**：
   - [`devices/modules/input_identity/cdh_input_identity.c`](../../../modules/input_identity/cdh_input_identity.c)：当目标身份为 `mtk-kpd` 时，清除 `EV_REP` 与字母键位图，仅保留 `EV_SYN` / `EV_KEY` 及 `KEY_VOLUMEUP`、`KEY_VOLUMEDOWN`、`KEY_POWER` 等物理侧键位，使 `EventHub` 精确归类为 `KEYBOARD_TYPE_NON_ALPHABETIC (1)`。
2. **GPU 公开只读属性 SELinux 域与 3 项安全 `GL_ARM_*` 扩展**：
   - [`engine/vendor/hardware/android.service.daemon/sepolicy/vendor_property_contexts`](../../../vendor/hardware/android.service.daemon/sepolicy/vendor_property_contexts) 与 [`vendor_sepolicy.cil`](../../../vendor/hardware/android.service.daemon/sepolicy/vendor_sepolicy.cil)：定义 `cdh_gpu_public_prop` 将 `ro.vendor.cdh.gpu.` 前缀向 `appdomain` / `surfaceflinger` / `hal_graphics_composer` 开放只读映射（不带脏规则副作用）；
   - [`engine/vendor/hardware/android.hardware.gpu/src/cdh_gl_strings.c`](../../../vendor/hardware/android.hardware.gpu/src/cdh_gl_strings.c) 与 [`gold-android15.json`](../../../manifest/configs/redmi/gold/gold-android15.json)：配置 `16383 / 32 / 16 / 96` 精确上限，并在 `cdh_build_ext()` 追加 `GL_ARM_debug_assert`、`GL_ARM_shader_core_properties`、`GL_ARM_texture_unnormalized_coordinates`（保持过滤 `GL_ARM_shader_framebuffer_fetch*`）。
3. **Audio 音效连接模式分类、UUID 对齐与 MIUI 17 阶流音量 / 闹钟曲线调优**：
   - [`engine/vendor/hardware/android.hardware.audio/src/EffectFactoryImpl.cpp`](../../../vendor/hardware/android.hardware.audio/src/EffectFactoryImpl.cpp)：按标准音效 `type` UUID 为 AEC / AGC2 / NS 设置 `Flags::Type::PRE_PROC`，为 EnvReverb / PresetReverb 设置 `Flags::Type::AUXILIARY`；
   - [`devices/manifest/configs/redmi/gold/gold-android15.json`](../../../manifest/configs/redmi/gold/gold-android15.json)：修正 `ExtensionEffectExample` UUID 为 `fa81dd00-588b-11ed-9b6a-0242ac120002`；在机型 `vendor_property_contexts` 绑定 `ro.vendor.audio. -> u:object_r:vendor_misound_ro_prop:s0` 激活 MIUI 17 阶流音量；调整 `etc/audio_config.xml` 的 `DEFAULT_NON_MUTABLE_SPEAKER_VOLUME_CURVE` 拐点为 `(0,-5800),(33,-3350),(66,-1700),(100,0)`，使 `-36.0 dB` 阈值精确落在 `mIndexMinNoPerm = 6`。
4. **Wi-Fi Direct (`p2p0`) 与 5 GHz VHT80 + WMM QoS `433 Mbps` 全链路打通**：
   - [`devices/kernels/patches/6.6/0002-hwsim-fake-wifi.patch`](../../../kernels/patches/6.6/0002-hwsim-fake-wifi.patch)：在 `hwsim_fake_build_beacon` 与 `hwsim_fake_tx_assoc` 区分 5 GHz 纯 OFDM 速率与 2.4 GHz 速率，通过 `hwsim_fake_append_ht_vht_ies` 注入标准 24B WMM Parameter IE（置位 `bss->wmm_used = true`）+ `HT Cap/Oper` + 5 GHz `VHT Cap/Oper (80 MHz)`，并在 `HWSIM_COMMON_OPS` 挂载 `.sta_statistics` 上报 5 GHz `VHT 80MHz MCS9 NSS1 SGI`（`433.3 Mbps`）；
   - [`qemu/backend/wifi-host-service/wifi-aps.json`](../../../../qemu/backend/wifi-host-service/wifi-aps.json)：将主开放 AP `TP-LINK_2464` 切至 5 GHz ch 36（`5180 MHz`），保持 2.4G + 5G 双频 4 热点；
   - [`engine/vendor/hardware/android.service.daemon/vendor/bin/cph-netmgr-setup`](../../../vendor/hardware/android.service.daemon/vendor/bin/cph-netmgr-setup) 与 [`engine/vendor/hardware/android.hardware.wifi/src/wifi_service.cpp`](../../../vendor/hardware/android.hardware.wifi/src/wifi_service.cpp)：在 `post-fs` `fingerprint` 阶段将闲置 `wlan1` 重命名为 `p2p0` 并替换为局部管理 P2P MAC（保持默认 `DOWN`），同时在 `WifiChip` 实现 `WIFI_FEATURE_P2P` 与 `IWifiP2pIface` 接口族及动态 `getLinkLayerStats` 频点/RSSI 读取。
5. **`07_build_vendor` 修补 `hwc3-service.drm` 支持配置化精确 `xDpi / yDpi` 与 128B 标准 EDID 注入**：
   - [`engine/build/stages/07_build_vendor/elfcompat.go`](../../../build/stages/07_build_vendor/elfcompat.go) 与 [`elfcompat_test.go`](../../../build/stages/07_build_vendor/elfcompat_test.go)：扩展 `vendorDisplayConfig` 解析 `ro.vendor.cdh.display.{xdpi,ydpi,edid_pnp,edid_name,edid_product_id,edid_week,edid_year}`；原地修补 `ComposerClient::getDisplayConfigurations` 分别写入精确 `float32` `xdpi`/`ydpi`，并修补 `HwcDisplay::GetDisplayIdentificationData` 返回由 `buildStandardEdid128` 合成的 128 字节标准 EDID v1.4 数据块（含单元测试覆盖）。

## 5. 验证

1. `go test -count=1 ./stages/07_build_vendor/...` 单元测试全绿（含新增 `xDpi/yDpi` 与 128B EDID 机器码及校验和断言）；`git -C devices/kernels/6.6 status --short` 为空且 `0001`+`0002` 补丁 `git apply --check` 通过。
2. 全流水线重跑 `02_build_kernel`（`6.6-cdh-595096a3795b`）→ `03_build_modules` → `04..11`（`dlkm-45531ca7d3f8`、`patch.img` `c0e939dbf3bc`、`vda-7de3bfef388d`）全部 `[ok]`，`08` 四道全自建闸门通过。
3. 全新干净盘冷启动实例 `gold_15-OS3.0.10.0.VNQCNXM/1`，`boot_check.py` `[pass]`；系统态 `adb` 与普通 App `device-checker` 全量 27 项检查（`run-00279`、`run-00280`、`run-00281`）实测 5 项精细指标全部与真机 100% 对齐，`goldScorecard` **`32 / 32`**，14 个 `ultimate.*` 套件零云机独有命中。

## 6. 遗留

- 无。所有新增参数（`ro.vendor.cdh.gpu.max_*`、`ro.vendor.cdh.display.{xdpi,ydpi,edid_*}`、`ro.vendor.cdh.wifi.disable_he`）均为通用可选配置项，未声明该组属性的其余机型保持原有行为不变，后续新机型只需在各自清单填入真机实测参数即可直接复用。
