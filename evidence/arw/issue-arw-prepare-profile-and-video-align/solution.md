# #339 HMD Crest Max（`hmd/ARW`）`02_prepare` 真机规格重采集全量对齐与原生相机录像打通：T760 二进制 Overlay 三丛集调频、HMD OneImage 身份闭环、`.rar` 基带流式提取与 `cdh-mediaserver` 1080p 录像落盘

- **设备 / 机型**：HMD Crest Max（`hmd/ARW`，版本 `14-QP1A.190711.020-20250620`，Unisoc UMS9620 / T760，GRF `system=A14` + `vendor=A13`，`lineage-20.0` + `kernel 5.15`）
- **日期**：2026-11-26
- **前置条目**：[`#304`](../issue-arw-boot-radio-deadlock-wifi-baseband/solution.md)、[`#337`](../../g1130/issue-g1130-camera-video-mediaserver/solution.md)
- **状态**：已闭环

---

## 一、问题

排查 `HMD Crest Max #1`（`ARW_14-QP1A.190711.020-20250620/1`）内置预装应用完整性与 `02_prepare` 对齐状态时，发现官方 `.pac` 虽无遗漏独立应用分区（全机 227 个系统包均已随官方只读镜像挂载），但 `hmd/ARW` 的真机规格档案与清单配置停留在早期 `measured-only` 半成品状态，导致运行态存在 6 项失真与功能阻塞：

1. **CPU 拓扑与调频完全失真**：云机暴露联发科模板 `mt-cpufreq` / `sugov_ext`、`6+2` 双丛集假频点（`2000000` / `2400000`），而展锐 UMS9620（T760）真机为 `sprd-cpufreq` / `sprd_sugov`、`4×A55 (0xd05, 2.002 GHz) + 3×A76 (0xd0b, 2.210 GHz) + 1×A76 (0xd0b, 2.548 GHz)` 三丛集 32 档频点。
2. **`OneImage` 身份属性落回普通版 `HMD CREST`**：`getprop ro.product.model` 为 `HMD CREST`、`ro.product.device` 为 `ARW`、`ro.product.name` 为 `Arrow_0AIN`，「关于手机」设备名称、产品名称及机身渲染图均未对齐 `HMD Crest Max`（`ARP` / `ArrowPro_0AWW`）。
3. **原生相机切换「视频」模式（或 `VIDEO_CAPTURE`）无限卡死无法录像**：打开 `com.hmdglobal.app.camera` 拍照正常，但发起 `android.media.action.VIDEO_CAPTURE` 或切入录像模式时，客户端线程无限阻塞在 `ServiceManagerCppClient: Waiting for service 'media.player' on '/dev/binder'...`，无法录制视频。
4. **屏幕刷新率、存储规格与硬件描述 XML 未对齐**：QEMU EDID 仅下发单 `60Hz`（缺 `120Hz` 高刷模式）；`ARW.json` 内存/存储仅填 `6GB+128GB`（缺 `8GB+128GB` / `8GB+256GB`）；`vendor/etc/` 下的 `sensors_config.xml`、`gnss_config.xml`、`media_codecs_c2.xml`、`thermal` 温区与 `focaltech_ats` I2C 总线身份尚未按 `prepare` 档案全量生成对齐。
5. **`.rar` 固件包未支持流式提取真机基带**：`02_prepare/baseband.go` 此前仅支持 `.pac` / `.zip` / `.tar`，遇到 `roms/devices/nokia/Hmd-Crest-Max.rar`（内嵌 6.8 GiB `.pac`）直接返回空，导致重跑 AI 采集时基带被猜成假值 `UMS9620_MODEM_V1.0.0`。
6. **单盘重建实例系统盘触发 FBE 密钥失配重启**：更新 `vda.img` 后若仅删除实例 `writable-base.qcow2`（含 `vda4` `/metadata` 分区）而保留旧 `writable-data.qcow2`（`vdb` 已加密 `/data`），新格式化的 `/metadata` 丢失 `/metadata/vold/metadata_encryption/key`，导致 `vold` `cryptfs mountFstab` 失败触发 `init_user0_failed -> reboot,recovery` 循环重启。

---

## 二、根因

1. **展锐 UMS9620 DTB OPP 表为微安电流值，真实频点藏于编译态二进制 Overlay APK**：
   - 固件 `dtb` 的 `opp-table0/1/2` 节点仅记录电压/电流，不含真实 `opp-hz`；而通用底包 `/vendor/etc/power_scene_config.xml` 写的是 T770 顶配 `2704000 kHz`，并非 T760 真值。
   - T760 的真实三丛集频点以编译后 Android Binary XML（`ResXMLTree` 魔数 `0x0003`）形式存放在 `/vendor/overlay/unisoc_overlay_power_cpuT760.apk` 的 `res/xml/power_profile.xml`（`cpu.clusters.cores = {4, 3, 1}`、`cpu.core_speeds.cluster0/1/2` 共 `10 + 11 + 11 = 32` 档频点），此前 `dtb_cpufreq.go` 无法解析二进制 `ResXMLTree` 且未扫描 `overlay/*cpu*.apk`。
2. **HMD `OneImage` 共包属性导入链缺 `skuid` / `nfc_support` 与载体 `/odm` 同步属性**：
   - 原厂包由 `ARW`（HMD Crest）与 `ARP`（HMD Crest Max）共用，`/system`、`/system_ext`、`/product`、`/odm` 的 `build.prop` 末尾均执行 `import /<part>/etc/one_image/${ro.boot.board_device}/${ro.boot.skuid}/${ro.boot.nfc_support}/odm_custom.prop`。
   - 此前 `vendor_boot` 仅写了 `androidboot.board_device=ARP`，缺少 `androidboot.skuid=100WW` 与 `androidboot.nfc_support=0`（注：选 `100WW` 而非 `100IN` 可同时规避原厂 `init.oneimage.rc` 挂载运营商推装包 `DTIgnite.apk`），导致 `import` 路径展开为空目录；同时云机将 `/odm` 重定向至载体 `odm.img`，需在 `odm/config.json` 同步补齐 `ro.product.odm.{device,model,name}`，才能闭环最高优先级的 `odm` 属性源与指纹 `HMD/ArrowPro_0AWW/ARP:14/...`。
3. **32 位原厂 `mediaserver` 被空桩屏蔽致 `media.player` 缺位**：
   - 同 [`#337`](../../g1130/issue-g1130-camera-video-mediaserver/solution.md) 机理：`ARW` 原厂 `/system/bin/mediaserver` 为 32 位 ELF，在纯 64 位云机（`ro.zygote=zygote64`）无法执行；此前重定向为空桩后 Binder 上无 `media.player` 服务，`HMDCamera` 初始化 `MediaRecorder` 时死等 `media.player`，且缺 `debug.stagefright.c2inputsurface=-1` 规避 GraphicBuffer 跨进程 GraphicBufferSource 兼容问题。
4. **`02_prepare` 对齐流水线 4 处边界缺陷**：
   - `baseband.go` 未识别 `Rar!` 魔数流式抽取内嵌 `.pac` 的 `Modem_NR` 分区；
   - `ai_profile.go` 未剥离 `"HMD Global"` 的 `" Global"` 后缀，且未在固件实测基带存在时强制压过 AI 猜测基带；
   - `apply_profile_configs.go` 在 `overwrite=true` 重刷 `vendor_boot` 时未保留存量非模板 `androidboot.*` 键与属于合法档位的 `androidboot.ddrsize=8192M`，且 `resolveVulkanAPIHexAndConformance` 未以固件 `permissions` 声明的 Vulkan `1.3` 为下限保护。

---

## 三、证据

- **修前失败日志**：见 [log-fail.txt](log-fail.txt)
  - `ro.product.model=HMD CREST`、`ro.product.device=ARW`、`ro.product.name=Arrow_0AIN`；
  - `policy0`（`0..5`）与 `policy6`（`6..7`）暴露 `scaling_driver=mt-cpufreq`、`2000000`/`2400000` 假频点；
  - 启动 `android.media.action.VIDEO_CAPTURE` 持续刷屏 `ServiceManagerCppClient: Waiting for service 'media.player' on '/dev/binder'...` 无法进入录像。
- **修后验收日志与实屏截图**：见 [log-pass.txt](log-pass.txt)、[pass-home.png](pass-home.png)、[pass-about.png](pass-about.png)、[pass-camera-photo.png](pass-camera-photo.png)、[pass-camera-video.png](pass-camera-video.png)。

---

## 四、改动

1. **`agent/modules/pipeline/stages/02_prepare/dtb_cpufreq.go`**：
   - 新增 `mergeVendorOverlayPowerProfileFreqs` 与纯 Go Android Binary XML（`ResXMLTree` `0x0003` 字符串池 + UTF-8/UTF-16 解码 + `<array>`/`<item>` 状态机）解析器 `parseAndroidBinaryPowerProfileCPU`，自动从 `unpack_vendor/overlay/*power*cpu*.apk` 提取芯片专属丛集核数与频点表；当专属 overlay 已完整覆盖全部丛集时跳过通用底包 `power_scene_config.xml`，防止 T770 `2704000` 污染 T760。
2. **`agent/modules/pipeline/stages/02_prepare/baseband.go` & `ai_profile.go`**：
   - `extractBasebandFromRomArchive` 新增 `.rar` 后缀与 `Rar!` 魔数识别，实现 `extractBasebandFromRar` 通过 `bsdtar` 零落盘流式管道提取内嵌 `.pac` 的 `Modem_NR` 基带 `5G_MODEM_V2_23B_W24.26.4_P11`；
   - `sanitizeOfficialGroupName` 增加 `" Global"` 后缀归一（`"HMD Global"` → `"HMD"`），并在固件实测基带非空时强制优先于 AI 推断值。
3. **`agent/modules/pipeline/stages/02_prepare/apply_profile.go`、`apply_profile_configs.go` & `unisoc_profile_align_test.go`**：
   - `alignBuildVendorBoot` 保留存量非模板 `androidboot.*` 键及合法内存档位 `androidboot.ddrsize=8192M`，并为检测到 `/odm/etc/one_image/ARP/100WW/0/odm_custom.prop` 的 HMD OneImage 共包机型幂等写入 `androidboot.board_device=ARP`、`androidboot.product.model.display=TA-1635`、`androidboot.skuid=100WW`、`androidboot.nfc_support=0`；
   - `alignBuildOdm` 同步幂等补齐载体 `/odm` 的 `ro.product.odm.{device,model,name}`（`ARP` / `HMD CREST MAX` / `ArrowPro_0AWW`）；
   - `resolveVulkanAPIHexAndConformance` 以固件 `permissions` Vulkan 特性版本（`1.3`）为下限保护，保持 `0x00403116`（`1.3.8.0`）；
   - 同目录单测 `unisoc_profile_align_test.go` 新增 `TestUnisocHMDCrestMaxOverlayFreqsAndOneImageAlignment` 覆盖二进制 overlay 频点解析与 OneImage 对齐逻辑。
4. **`devices/manifest/` 下 `hmd/ARW` 真机规格档案与配置全量刷新（严守 `AGENTS.md §8` 官方镜像零修改）**：
   - 重采生成 `devices/manifest/devices/hmd/ARW/device-profile.{json,md}` 与 `ARW.json`（补齐 `memory: [6144, 8192]`、`storage: [128000, 256000]`、`baseband-version: 5G_MODEM_V2_23B_W24.26.4_P11`）；
   - 经 `pipeline device apply-profile -overwrite hmd/ARW` 全量刷新 `ARW-android13/{module,vendor,vendor_boot,odm}/config.json` 与 `vendor/etc/{sensors_config.xml,gnss_config.xml,media_codecs_c2.xml,media_codecs_performance_c2.xml}`：
     - `module/config.json`：`cpufreq` 切换为 `sprd-cpufreq` / `sprd_sugov` 4+3+1 三丛集 32 档频点与 MIDR `0xd05`/`0xd0b`；`display.refresh-rates` 设为 `[60, 120]`；`input.touch` 对齐 `focaltech_ats`（`bus=24`, `17280×38400`, `touch-keys=[116,143,354]`）；`thermal` 对齐 31 个展锐温区；`redirect` 将 `/system/bin/mediaserver` 前置指向 64 位 `/mnt/vendorpatch/vendor/bin/hw/cdh-mediaserver`（共 19/32 条规则）；
     - `vendor/config.json`：写入 `debug.stagefright.c2inputsurface=-1`、`ro.vendor.cdh.display.refresh_rates=60,120` 等收口属性；
   - 重编发布 `05_build_boot` → `13_pack_vda` 盘集（`patch.img` sha256 `6d6d4ceea8fb`，`vda-d3c50f63b408`），同步重置 `HMD Crest Max #1` 的 `writable-base.qcow2` + `writable-data.qcow2` + `efi-vars.fd` 并通过长驻 `cdha` 服务拉起实例。

---

## 五、验证

1. **单元测试**：`go -C agent/modules/pipeline vet ./stages/02_prepare/` 与 `go -C agent/modules/pipeline test -count=1 ./stages/02_prepare/` 全部 PASS（`1.102s`）。
2. **`HMD Crest Max #1`（`ARW_14-QP1A.190711.020-20250620/1`，ADB `127.0.0.1:5911`）实机全维度验收**（详见 [log-pass.txt](log-pass.txt)）：
   - **引导与健康度**：全新初始化 59s `sys.boot_completed=1`、`getenforce=Enforcing`、`/data/anr` 为 0、直达中文桌面并加载 Crest Max 专属壁纸（见 [pass-home.png](pass-home.png)）。
   - **OneImage 身份与关于手机**：
     - `ro.product.model=HMD CREST MAX`、`ro.product.device=ARP`、`ro.product.name=ArrowPro_0AWW`、`ro.product.model.display=TA-1635`、`Build fingerprint='HMD/ArrowPro_0AWW/ARP:14/QP1A.190711.020/00WW_1_590:user/release-keys'`、`gsm.version.baseband=5G_MODEM_V2_23B_W24.26.4_P11,5G_MODEM_V2_23B_W24.26.4_P11`；
     - 实屏 [pass-about.png](pass-about.png) 完整展示酒红色 `HMD Crest Max` 渲染图、`8GB` RAM、`128 GB` 存储、型号 `TA-1635`、相机 `正面：50MP / 背面：64MP + 5MP + 2MP`，且无 `100IN` 运营商推装包。
   - **CPU 三丛集调频与 MIDR**：
     - `policy0`（CPU `0..3`）：`sprd-cpufreq` / `sprd_sugov`，10 档频点 `2002000 .. 614400`，`CPU part=0xd05`；
     - `policy4`（CPU `4..6`）：`sprd-cpufreq` / `sprd_sugov`，11 档频点 `2210000 .. 614400`，`CPU part=0xd0b`；
     - `policy7`（CPU `7`）：`sprd-cpufreq` / `sprd_sugov`，11 档频点 `2548000 .. 614400`，`CPU part=0xd0b`。
   - **显示、传感器、GNSS、触控与双网**：
     - `dumpsys display` 暴露 `1080×2400 @ 60Hz`（id=1）与 `1080×2400 @ 120Hz`（id=2，默认模式）；
     - `dumpsys sensorservice` 注册全套 19 项硬件/融合传感器（`bmi323_acc/gyro`、`mmc5603`、`stk3338_l/p` 及 Unisoc 融合传感器）；`GNSS Hardware Model Name: SPRD_GNSS_default`；
     - `focaltech_ats` 命中原厂 `/vendor/usr/idc/focaltech_ats.idc`，QMP `input-send-event` 真实触控点击精准响应；
     - 5 GHz Wi-Fi `wlan0`（`5240 MHz`, `192.168.10.100/24`）与联通蜂窝 `ccmni1`（`100.64.155.15/24`, `SIM LOADED`）双通在线。
   - **`cdh-mediaserver` 与原生相机拍照 + 录像落盘**：
     - `service check media.player` 返回 `Service media.player: found`（`mediaserver` PID `571`）；`am start -W -a android.media.action.VIDEO_CAPTURE` 46ms 秒返回 `Status: ok`；
     - 拍照模式实拍落盘 `IMG_20261126_104048_HDR.jpg`（`282 KiB`，见 [pass-camera-photo.png](pass-camera-photo.png)）；
     - 切换「视频」模式点击录制并停止，成功落盘 `VID_20261126_104142.mp4`（`420 KiB`，见 [pass-camera-video.png](pass-camera-video.png)），经 `ffprobe` 校验为 `H.264 (avc1) 1920×1080 yuvj420p`、时长 `5.69s`、`175` 帧（`30.76 fps`）、`probe_score=100`，全程 `cameraserver` / `mediaserver` / `com.hmdglobal.app.camera` 零崩溃、零 ANR。

---

## 六、遗留

- 全新 `/data` 首次冷启动第 40 秒左右，`com.android.systemui` RenderThread 偶发过一次首启瞬态 `gralloc-mapper is missing` 后立即自愈拉起并全程稳定运行（后续桌面、设置、相机切换均未再复现）；同 [`#304`](../issue-arw-boot-radio-deadlock-wifi-baseband/solution.md) 既定口径，预装 32 位 `com.android.chrome` 在纯 64 位云机上保持原样不改动。
