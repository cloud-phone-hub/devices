# #359 realme GT7（`realme/RMX6688` / `MT6991` 天玑 9400+ / Android 15）全维度对齐、NFC/纯蜂窝/蓝牙/6GiB ZRAM 与原生相机 APS 全链路打通

- **设备/机型**：realme GT7（`realme/RMX6688`，真我 GT7 国行，天玑 9400+ `mt6991`，Android 15 / ColorOS 15.0.2 / Realme UI 6.0）
- **基线版本**：`15-AP3A.240617.008-20250825`（`lineage-22.2` + `kernel 6.6.127-4k-cdh`）
- **状态**：已闭环（2026-11-27，全新干净实例全维度实测通过）
- **最终产物**：
  - `out/devices/dist/realme/RMX6688/15-AP3A.240617.008-20250825/patch.img`（`sha256: f6ce6431d4e242f6ffb9d08bafaa5cfa0eed43112eb389d5f915421c3ee60174`）
  - `out/devices/dist/realme/RMX6688/15-AP3A.240617.008-20250825/vda.img`（原位写入最新 `vendor_boot.img` 至 `vendor_boot_a`/`vendor_boot_b`，`sha256: 375ba0e30ac0137c28a60edfe6e5a92f26aa03ec0093f85a1764ac5f67aaf47c`）

---

## 一、问题

在对镜像库 `#28 realme/RMX6688`（`15-AP3A.240617.008-20250825`）进行修前基线全维度审计时，发现 6 项核心问题（详见同目录 [`log-fail.txt`](log-fail.txt) 与 [`home-before.png`](home-before.png)）：

1. **原生相机被遮蔽且未接入真彩色 APS 引擎**：历史 bring-up（[../issue-rmx6688-android15-bringup/solution.md](../issue-rmx6688-android15-bringup/solution.md) `#194`）通过 `cdh_redirect` 遮蔽了 `/product/app/OplusCamera`，桌面无原生相机入口；解除遮蔽并部署自编 `libAlgoProcess.so` 后，首次启动原生 `com.oplus.camera` 在 `APSClient_createClient` 触发 `SIGSEGV`，日志报 `vndksupport: Could not load libAlgoProcess.so from sphal namespace: dlopen failed: cannot locate symbol "eglGetNativeClientBufferANDROID" referenced by "/odm/lib64/libAlgoProcess.so"`。
2. **真机具备全方位 NFC 但云机缺失 NFC 全栈**：`RMX6688.json` 缺少 `"host-hvc-nfc": "hvc11"`，且 `/system_ext/app/NfcNci` 被遮蔽，系统无 `com.android.nfc` 进程、无 NFC 开关与 `/api/nfc/tap` 能力。
3. **纯蜂窝移动数据 `ccmni1` 100% 丢包**：关闭 Wi-Fi 后无法经由 `ccmni1` 访问外网 `223.5.5.5`。
4. **内存统计倒挂且缺失 ZRAM 交换分区**：修前 `/proc/meminfo` 呈现 `MemTotal=7760724 kB < MemAvailable=8789264 kB` 异常倒挂（默认运行 SKU 内存配置与 12 GB 真机规格不齐），且 `/proc/swaps` 完全为空，无 6 GiB `zram0`。
5. **违规预装 `kernelsu.ko`**：修前 `module/config.json` 仍包含 `kernelsu` 外部模块。
6. **蓝牙音频假占位风险**：`cdh_audio_placeholders.txt` 存在蓝牙音频工厂假占位，需收敛避免蓝牙开启时 `FetchAudioProvider` 致命崩溃。

---

## 二、根因

1. **`6.6` 内核的 `cdh_redirect.ko` 版本滞后，未包含普通 App UID 放行 `libAlgoProcess.so` 的规则**：
   - `devices/modules/cdh_redirect/core/rules.c`（2026-11-27 09:24 更新）已新增 `CDH_CARRIER_ALGO_PROCESS_64`（`mask=0x10`），对 `uid > max_uid(10000)` 的欧加相机进程（`com.oplus.camera`）放行 `/odm/lib64/libAlgoProcess.so` 与 `/vendor/lib64/libAlgoProcess.so` 重定向；
   - 然而 `out/devices/kernel/6.6/modules/cdh_redirect.ko` 停留在 2026-11-26 23:57 旧版本（仅 `5.15` 与 `6.12` 重编过），导致 `com.oplus.camera`（`u0_a151`，UID > 10000）命中 `uid_skipped` 而加载了官方闭源 `/odm/lib64/libAlgoProcess.so`（依赖 Mali/EGL 符号 `eglGetNativeClientBufferANDROID`），进而 `dlopen` 失败并空指针解引用崩溃。
2. **欧加保留官方 `/odm` 分区时的 NFC VINTF 冲突与 SN100 回退栈**：
   - `RMX6688` 未整树重定向 `/odm`，官方 `/odm/etc/vintf/manifest/manifest_nfc_thn31.xml` 已声明 `android.hardware.nfc::INfc/default`；若从 `vendor/config.json` 的 `vintf-drop` 移除 `"android.hardware.nfc-service.xml"`，会触发 `Conflicting FqInstance` 导致 `NULL VINTF MANIFEST` 卡死 `keystore2`；
   - 同时云机无物理 TMS THN31 芯片时，欧加多芯片 `NfcNci` 会回退加载 NXP SN100 栈，因此需同步补齐 NXP/TMS 占位、`vendor_service_contexts` 映射与 SELinux 放行规则。
3. **官方 ODM 相机配置加密与 APS 模式**：
   - 官方 `/odm/etc/camera/config/` 下的 5 份相机配置文件经 `libOplusSecurity.so` 加密，需用原厂 `libOplusSecurity.so` + `dec_sec` 解密后修改 `oplus_camera_algo_switch_config`（`aps_mode=2`, `camera_id_list=["0","1","2","3"]`）并重定向覆盖。

---

## 三、证据

- **修前失败基线**：[`log-fail.txt`](log-fail.txt) 与 [`home-before.png`](home-before.png)（桌面无相机图标、无 `com.android.nfc`、`kernelsu.ko` 在载、`MemTotal=7760724 < MemAvailable=8789264`、`/proc/swaps` 为空、纯蜂窝 `ccmni1` 100% 丢包）。
- **修中根因实证**：
  - `adb shell readelf -d /odm/lib64/libAlgoProcess.so`（UID 0）显示自编库仅依赖 `libnativewindow.so / liblog.so / libm.so / libdl.so / libc.so`；
  - `strings out/devices/kernel/6.6/modules/cdh_redirect.ko | grep AlgoProcess` 为空，而 `5.15`/`6.12` 均包含 `/odm/lib64/libAlgoProcess.so`；
  - 重编 6.6 `cdh_redirect.ko`（`vermagic=6.6.127-4k-cdh`，与 `out/devices/kernel/6.6/modules/Module.symvers` MD5 `f7375ca08c9bfbafcdac85b72546e53e` 严格一致）并打入 `vendor_boot` 后，内核日志立即输出 `cdh_redirect: 载体 OpenCL 桩库就绪：mask=0x10`。
- **修后全绿证据**：[`log-pass.txt`](log-pass.txt) 及全部经人工核验的实屏截图与实拍原图：
  - [`home-pass.png`](home-pass.png)：干净 Realme UI 桌面，底部 Dock 栏恢复原生相机图标，Wi-Fi/4G 信号与 78% 未插电电量显示正常；
  - [`about-pass.png`](about-pass.png)：关于本机对齐 `realme RMX6688`、`MediaTek 天玑 9400+`、`7200 mAh (典型值)`、`12.0 GB`、`256 GB`、`6.8 英寸`；
  - [`camera-rear-preview.png`](camera-rear-preview.png) / [`camera-front-preview.png`](camera-front-preview.png)：原生 `OplusCamera` 后摄与前摄真彩色实时预览（无灰度偏色）；
  - [`capture-rear.jpg`](capture-rear.jpg)（4096×3072, 269 KB）/ [`capture-front.jpg`](capture-front.jpg)（4096×3072, 273 KB）：前后摄实拍真彩色 JPEG 原图；
  - [`gallery-view.png`](gallery-view.png) / [`video-play.png`](video-play.png)：原生相册查图与内置播放器通过 `c2.v4l2.avc.decoder` 硬解流畅播放 1080p H.264 实录视频（`00:02/00:04`）。

---

## 四、改动

1. **重编 `6.6` 内核最新 `cdh_redirect.ko`**：
   - 使用 `cdh-kernel-6.6-cdh-657d516eaa4f` 构建卷（与当前 `6.6.127-4k-cdh` 内核 `Module.symvers` 100% 一致）重新编译最新 `devices/modules/cdh_redirect`，产出 `out/devices/kernel/6.6/modules/cdh_redirect.ko` 并更新同目录 `SHA256SUMS`。
2. **清单与机型配置修复（`devices/manifest/devices/realme/RMX6688.json` & `devices/manifest/configs/realme/RMX6688/RMX6688-android15/`）**：
   - 补齐 `"host-hvc-nfc": "hvc11"`、12 GB+256 GB 真机内存/存储 SKU 及 7200 mAh 未插电电池配置；
   - `module/config.json`：移除 `kernelsu` 外部模块，解除 `/system_ext/app/NfcNci` 与 `/product/app/OplusCamera` 遮蔽，保留 `/odm/lib64/libAlgoProcess.so` 与 `/odm/etc/camera/config` 重定向；
   - `vendor/config.json`：保留 `vintf-drop` 中的 `"android.hardware.nfc-service.xml"` 防与官方 ODM `manifest_nfc_thn31.xml` 撞车；
   - 新增解密并打补丁（`aps_mode=2`, `camera_id_list=["0","1","2","3"]`）的 5 份 ODM 相机配置至 `odm/etc/camera/config/`；
   - 补齐 NXP/TMS NFC 占位、`vendor_service_contexts`、`vendor_sepolicy_rules.cil`、`libnfc-nci.conf`/`libnfc-nxp.conf`；清空 `cdh_audio_placeholders.txt`；新增 `cph_rmx6688_setup.{rc,sh}` 建 NFC 目录、激活 6 GiB `zram0` 并预置相机沙盒。
3. **增量重打与盘集原位更新**：
   - 依次执行 `build_vendor_boot` → `build_odm` → `build_vendor` → `build_vendor_dlkm` → `build_patch`，将新 `vendor_boot.img` 原位写入 `dist/.../vda.img` 的 `vendor_boot_a`/`vendor_boot_b`，并更新 `dist/.../patch.img`。

---

## 五、验证

全新干净实例（`RMX6688_15-AP3A.240617.008-20250825` 实例 1，`12GB+256GB`）实测全绿（详见 [`log-pass.txt`](log-pass.txt)）：

- **系统健康与安全**：`getenforce=Enforcing`、`init.svc.*` `restarting=0`、无 `kernelsu.ko`；静置 10 秒 `top` 显示 `744%idle / 800%cpu`（93% 空闲率），无崩溃风暴。
- **真机规格、内存、ZRAM 与电池**：`MemTotal=11661120 kB`、`MemAvailable=7787244 kB`（彻底消除内存倒挂）；`/dev/block/zram0` 大小 `6291452 kB`（6 GiB）已激活并承接交换；`battery` 为 `AC/USB/Wireless=false, status=3, level=78, Charge counter=5613633`（7200 mAh 未插电自然电量）。
- **蓝牙、Wi-Fi 与纯蜂窝外网**：蓝牙 `enabled: true, state: ON` 连续稳定运行 `00:01:25+`，`Bluetooth crashed 0 times`；Wi-Fi 扫描发现 9+ AP；关闭 Wi-Fi 后纯蜂窝 `ccmni1` (`100.64.155.15/24`) `ping -c 4 223.5.5.5` **0% packet loss，avg 0.233 ms**。
- **NFC 全链路**：`dumpsys nfc` 显示 `mState=on, mScreenState=ON_UNLOCKED`；宿主机 `POST /api/nfc/tap` 返回 `ok: true, initialized: true, discovering: true, cardUid: "04112233445566", rxCommands: 48, txResponses: 73`。
- **原生相机全链路与硬编硬解**：
  - 后摄、前摄实时真彩色预览正常，宿主推流 `framesSent: 68 -> 84`（8 fps, `framesDropped: 0`）；
  - 后摄实拍 `IMG20261127174938.jpg`（4096×3072, 269 KB）、前摄实拍 `IMG20261127175411.jpg`（4096×3072, 273 KB）均真彩色清晰落盘；
  - 原生相机录像产出 `VID20261127174957.mp4`（1.0 MB, H.264 1920×1080, 3.85s）；
  - 退桌面触发 `CDCM_PAUSE`，宿主 `framesSent` 严格冻结（`638 -> 638`）；
  - 原生相册查图与内置播放器通过 `c2.v4l2.avc.decoder` 硬解流畅播放录像（`00:02/00:04`）。

---

## 六、遗留

无遗留问题。`6.6` 内核最新 `cdh_redirect.ko`（含 `libAlgoProcess.so` 普通 App UID 放行）已沉淀至 `out/devices/kernel/6.6/modules/`，后续所有 6.6 内核欧加机型重打 `vendor_boot`/`vendor_dlkm` 即可自动复用。
