# #355 OPPO Find X9（`oppo/CPH2797`）真机规格与电池对齐、MTK 纯蜂窝/NXP NFC/蓝牙补齐、误注入 `kernelsu.ko` 引擎修复及 `OplusCamera` 拍录全链路打通

- **设备 / 机型**：OPPO Find X9（`oppo/CPH2797`，ColorOS 16.0.8 / `16-BP2A.250605.015-20260529` / 天玑 9500 `MT6993` / Android 16，lineage-23.2 + kernel `6.12.23`）
- **日期**：2026-11-27
- **最终产物**：`out/devices/dist/oppo/CPH2797/16-BP2A.250605.015-20260529/patch.img`（`sha256: 8dc2f202766677ce043b8e8fdeecd61e9881a33c9942ed8b3c798dde3b6683ad`）

---

## 一、问题

对 `#24 oppo/CPH2797` 进行全维度审计（见 [log-fail.txt](log-fail.txt) 与 [home-before.png](home-before.png)、[camera-before.png](camera-before.png)）发现 6 类核心缺陷：

1. **规格与电池单位失真**：`battery_config.xml` 误将 `full_uah` 写成 `7025`（应为 `7025000` µAh，7025 mAh）且插电状态为满电；`vendor/config.json` 仍残余 `ro.soc.model=vsoc_arm64`，未对齐真机 `MT6993` 与商业名 `OPPO Find X9`。
2. **纯蜂窝移动数据完全不通**：旧版 `patch.img` 中的 `cph-netmgr-setup` 将真实 `virtio-net` 网卡重命名为 `ccmni2` 而非 `ccmni1`，导致关闭 Wi-Fi 后纯蜂窝 `ccmni1` 100% 丢包。
3. **NFC 被长期屏蔽且恢复后立即崩溃**：`CPH2797.json` 缺少 `"host-hvc-nfc": "hvc11"`，且历史条目 [issue-cph2797-vintf-powerhal-nfc-radio-touch](../issue-cph2797-vintf-powerhal-nfc-radio-touch/solution.md) 遮蔽了 `/system/priv-app/NfcNci`；解除遮蔽后 `com.android.nfc` 因缺少 NXP/Oplus AIDL 占位服务与 `hal_oplus_nfc_service` SELinux 绑定，在 `SN100HalHelper::halGetProperty` 触发 `SIGSEGV (0x0)`。
4. **跨机型引擎 Bug：`vendor_dlkm` 漏网注入 `kernelsu.ko` 锁死 6.12 内核**：重跑 `10_build_vendor_dlkm` 时，共享内核模块目录 `out/devices/kernel/6.12/modules/` 下由其他机型编译的 `kernelsu.ko`（`inject=vendor_boot` 第一阶段模块）未被排除，被误打包进 `CPH2797` 的 `vendor_dlkm.img` 并在开机 31.7s `exec zygote` 阶段延迟加载，触发 `6.12` 内核 `WARNING at __queue_work` 死锁整台 VM。
5. **原生相机 `OplusCamera` 三道硬卡点**：
   - 第一层：ColorOS 16 将 `com.oplus.camera` 置于自定义 SELinux 域 `opluscamera_app`（目录标签 `opluscamera_app_data_file`），缺失策略放行导致启动即抛 `SQLiteCantOpenDatabaseException: Permission denied`；
   - 第二层：`OplusCamera` 成功打开 `Camera ID 0` 后始终停留在 `Device status: UNCONFIGURED / No output streams configured` 黑屏——打开明文日志（移除 `com.oplus.camera.encrypt.log.flag`）后查明 `camera_unit_config` 的 MTK `usecase_info`（如 `sat_photo_full_size_case`）一次请求 20 条 MTK 专有流（`raw_output`、`tuning_data_yuv`、`tuning_data_raw`、`capture_raw10_dol`、`capture_raw10_fullsize` 及不存在的 `rear_tele` `physicalId=-1`），且 `oplus_camera_algo_switch_config` 需按 `FullApsImpl`（`aps_mode=1` + `aps_capture_configs` `enable=0`）收口；
   - 第三层：拍照打通后，切换 `VIDEO` 录像在 `MediaRecorder._prepare()` 抛 `IllegalStateException`（`StagefrightRecorder: Output file descriptor is invalid`），内核日志显示 `binder: translate fd failed` -> `transaction call to mediaserver failed -1 (EPERM)`。

---

## 二、根因

1. **`10_build_vendor_dlkm/modules.go` 模块过滤盲区**：`resolveModuleSet` 此前仅跳过**当前机型 `external` 列表内**标记为 `first-stage` 的模块，未全局扫描 `devices/modules/*/module.json` 排除其他机型产出的 `inject == vendor_boot` 模块和未声明模块，导致共享内核产物目录中的 `kernelsu.ko` 混入非 KernelSU 机型的 `vendor_dlkm.img`。
2. **ColorOS 16 `opluscamera_app` / `opluscamera_app_data_file` 策略缺口**：
   - `plat_sepolicy.cil` 仅向 `mediaserver` / `mediaextractor` / `mediaswcodec` 开放 `app_data_file` 与 `privapp_data_file`，而 `CPH2797` 的 `system_ext_sepolicy.cil` 仅声明了类型 `opluscamera_app_data_file` 却缺失对 `opluscamera_app` 自身及 `mediaserver` 的读写/fd 传递授权，导致数据库打开失败与录像 `setOutputFile(fd)` 跨进程 Binder 传递文件描述符被 SELinux 拒绝返回 `-EPERM`。
3. **MTK 旗舰 `camera_unit_config` 多流与 HDR 依赖**：`mt6993` 原厂 `camera_unit_config` 在 `usecase_info` 中为 `sat_photo_full_size_case` 等场景硬编码了 20 条 MTK ISP 专有 RAW/tuning/3DOL 流（含云机不存在的 `rear_tele` 物理 ID），在构造 `OutputConfiguration` 阶段即被客户端校验打回，从未下发至 `cameraserver`。

---

## 三、证据

- 修前基线日志：[log-fail.txt](log-fail.txt)
- 修前桌面/相机截图：[home-before.png](home-before.png)、[camera-before.png](camera-before.png)
- 终版全维度通过日志：[log-pass.txt](log-pass.txt)
- 实屏审阅证据（均已人工审阅确认）：
  - 干净桌面（1256×2760，Wi-Fi/蜂窝/未插电 85% 电量）：[home-pass.png](home-pass.png)
  - 关于本机（`CPH2797` / `ColorOS 16.0.8` / `256 GB` / `7025 mAh (TIP)` / `12,0 GB` / `Sistem Kamera LUMO`）：[about-pass.png](about-pass.png)
  - 原生相机后摄真彩色实时预览：[camera-rear-preview.png](camera-rear-preview.png)
  - 原生相机前摄真彩色实时预览（0.8x/1x）：[camera-front-preview.png](camera-front-preview.png)
  - 原生相机 `VIDEO` 模式 1080p 实时录像（`00:02` 计时真彩色）：[camera-video-recording.png](camera-video-recording.png)
  - 相册全屏查验实拍真彩色照片：[gallery-photo-pass.png](gallery-photo-pass.png)
  - 相册 `c2.v4l2.avc.decoder` 硬解播放 `VID20261127142811.mp4`（`00:05 / 00:06`）：[video-playback-pass.png](video-playback-pass.png)

---

## 四、改动

### 4.1 跨机型引擎加固（全仓生效）
1. `agent/modules/pipeline/stages/10_build_vendor_dlkm/modules.go`：`resolveModuleSet` 扫描 `ws.ModulesRoot()` 下全部 `module.json`，凡 `inject == vendor_boot`（第一阶段模块，如 `kernelsu.ko`）或未在当前机型 `external` 声明的外部模块产物一律过滤跳过，防止共享内核目录串模块；通过 `go test ./stages/10_build_vendor_dlkm/...` 并重编 `out/agent/dist/pipeline`。

### 4.2 `oppo/CPH2797` 机型配置改动
1. `devices/manifest/devices/oppo/CPH2797.json`：补齐 `"host-hvc-nfc": "hvc11"`（并同步更新 `dist/.../device.json` 与 `vda.img` 内 `boot`/`vendor_boot`）。
2. `devices/manifest/configs/oppo/CPH2797/CPH2797-android16/`：
   - `module/config.json`：移除 `/system/priv-app/NfcNci` 遮蔽；
   - `vendor/config.json`：修正 `ro.soc.model=MT6993`、`ro.vendor.cdh.device.market_name="OPPO Find X9"`；刷新 `vendor.img` 接入新版 `cph-netmgr-setup`（`ccmni1` 绑定真实 `virtio-net`）；
   - `vendor/etc/battery_config.xml`：修正 `full_uah="7025000"`、`level="85"`、`ac="0"`、`wireless="0"`；
   - NXP SN220 NFC 全套：`vendor/etc/cdh_nn_placeholders.txt`（补 4 条 NFC AIDL 占位：`INxpNfc`、`pnaidl.INxpNfc`、`IOplusNfc`、`INfcExtns`）、`vendor/etc/{libnfc-nci.conf,libnfc-nxp.conf}`、`odm/etc/permissions/{com.oplus.nfc_feature.xml,oplus_nfc_default_grant_permissions_list.xml}`、`vendor/sepolicy/vendor_service_contexts`；
   - `vendor/sepolicy/vendor_sepolicy_rules.cil`：放行 `hal_neuralnetworks_default` / `nfc_{{vers}}` 对 `hal_oplus_nfc_service` 的服务与 Binder 访问；放行 `opluscamera_app_{{vers}}` 读写 `opluscamera_app_data_file_{{vers}}`、`cameraserver_{{vers}}` `setsched`、以及 `mediaserver_{{vers}}` / `mediaextractor_{{vers}}` / `mediaswcodec_{{vers}}` 使用 `opluscamera_app_{{vers}}` fd 与读写 `opluscamera_app_data_file_{{vers}}`；
   - `odm/etc/camera/`：通过 `dec_sec` 解密官方 20 份 `config/*` + `CameraHWConfiguration.config` + `fwk_config.json`，对齐 `oplus_camera_algo_switch_config`（`file_version=1005.0, aps_mode=1`, `aps_capture_configs` `enable=0`）、`oplus_camera_config`（`file_version=1005.0`、移除 `encrypt.log.flag`、关闭 HDR/10bit RAW）、`camera_unit_config`（`version_code=99.9.11`、`camera_id_list=["0","1","2","3"]`、剔除 `usecase_info` 全部 MTK 专有 `raw`/`tuning`/`meta`/`depth`/`dol`/`rear_tele` 多流），配置 `vendor/etc/init/cph_cph2797_setup.rc` 与 `vendor/bin/cph_cph2797_setup.sh` 注入沙箱。

---

## 五、验证

在冷启全新构建的 `CPH2797_16-BP2A.250605.015-20260529`（`12GB+256GB`，`patch.img sha256: 8dc2f2027666`）上完成全维度实证（详见 [log-pass.txt](log-pass.txt)）：

| 维度 | 实测结果 |
| :--- | :--- |
| 规格与电池 | `CPH2797` / `OPPO Find X9` / `MT6993` / `1256x2760 @ 560dpi` / `12GB+256GB` / `6143M ZRAM` / `7025000 µAh` / `level=85` 未插电放电态 |
| 系统健康 | `getenforce=Enforcing`、`sys.boot_completed=1`、`restarting_count=0`、0 崩溃、`lsmod` 无 `kernelsu`、静置 `top` `781% idle / 800% cpu`（**97.6% 空闲**） |
| 蓝牙 | `enabled: true, state: ON, name: OPPO Find X9, Bluetooth crashed 0 times` |
| Wi-Fi 外网 | `wlan0 (192.168.20.100/24)` `ping -c 3 223.5.5.5` → **0% packet loss, avg 0.592 ms** |
| 纯蜂窝（Wi-Fi OFF） | `ccmni1 (100.64.155.15/24 -> virtio7)` `ping -c 3 223.5.5.5` → **0% packet loss, avg 0.222 ms** |
| NFC 全链路 | `mState=on, mScreenState=ON_UNLOCKED`，`POST /api/nfc/tap` 返回 `ok: true, initialized: true, discovering: true, cardUid: "04112233445566", cardPayload: "cloud-phone-hub-nfc-ok", rxCommands: 99, txResponses: 118` |
| 原生相机拍照与预览 | 后摄/前摄 100% 真彩色实时预览；实拍落盘 `IMG20261127141238.jpg`（279 KB）与 `IMG20261127141246.jpg`（112 KB），0 崩溃 |
| 录像与硬解回放 | `VIDEO` 模式录 6 秒生成 `VID20261127142811.mp4`（677 KB）0 崩溃；相册硬解播放 `c2.v4l2.avc.decoder [1920*1080] inputFps=21, outputFps=21, renderFps=18, discardFps=0` |
| 后台摄像头休眠 | 退回桌面宿主 `framesSent` 严格冻结：`592 -> 592`（+4s 零增长） |

---

## 六、遗留

1. 本条沉淀的 MTK ColorOS 16 全套方案（`camera_unit_config` 清理 MTK 专有 RAW/tuning 多流 + `oplus_camera_algo_switch_config` `aps_mode=1` + `opluscamera_app`/`mediaserver` fd 传递 SELinux 策略 + `NXP SN220 NFC` + `battery_config.xml` µAh 对齐）可直接无缝复用于下一台同平台旗舰 `#25 oppo/PME110`（OPPO Find X9s Pro，同 `mt6993`）与 `#28 realme/RMX6688`。
