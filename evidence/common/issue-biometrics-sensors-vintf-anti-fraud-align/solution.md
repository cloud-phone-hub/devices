# 硬件风控特征闭环：虚拟指纹 `fingerprint-only` 全栈、传感器单机种子防聚类、内嵌 ODM VINTF 占位与守护进程保活（`#327`）

- **设备/机型**：跨机型引擎通用（首发在线对拍机型：`redmi/gold` Redmi Note 13 5G `15-OS3.0.11.0.VNQCNXM`，在线真机 `ec519c7d513a3ad3` × 云机 #1 `4c17e53fbaef35ca-cloud`）
- **日期**：2026-11-25

## 1. 问题

在对 `Redmi Note 13 5G #1`（`gold_15-OS3.0.11.0.VNQCNXM/1`）与同版本在线 USB 真机（`OS3.0.11.0.VNQCNXM`）执行 19 域 `aligner.*` 与 14 套 `ultimate.*` 全量风控与硬件特征对拍时，暴露出 6 项易被风控 SDK 用于真伪识别、跨实例聚类或导致运行异常的问题：

1. **`android.hardware.fingerprint` 全栈缺失**：`app` 域唯一 1 项 `fatal` 为缺失 `android.hardware.fingerprint` 特性（`92` vs 真机 `93`），`services` 域缺失 `fingerprint`（`IFingerprintService` / `FingerprintManager`）；且既有 `biometrics` 模块默认携带 `face-service` 与名称含 `virtualhal` 的 `IVirtualHal/virtual` 服务，`fakefingerprint` 变体硬编码 `REAR` + `CONVENIENCE` 且未回调 `ISessionCallback`，直接套用会暴露 `virtualhal` 服务名并挂起 Android 15 `FingerprintInternalCleanupClient`。
2. **传感器二供型号错配与多实例姿态/零偏同源聚类隐患**：`sensors` 域 `handle=2` 磁力计配置为 `mmc5603/Memsic`（在线真机为二供 `qmc6308/QST`），产生 `sensors` 域 2 项、`ids` 域 2 项 `fatal`；更深层地，`sensor_event_model.cpp` 将静态重力 `(0.142, 0.268, 9.80196)`、地磁基线与陀螺零偏写死为全局常数并用单频正弦波抖动，多台云机并发运行时静态桌面倾角、地磁矢量与频谱完全雷同，极易被风控后台同源聚类。
3. **`ultimate.hardware_env` 唯一 `cloud_only` 命中 `U-HW-WIDEVINE`**：云机运行自研 `android.hardware.drm-service.widevine`（默认支持 `L1`），但 `gold` 清单 `vendor/config.json` 覆写了 `"ro.vendor.drm.widevine.security_level": "L3"`，与 `ro.boot.verifiedbootstate="green"` 形成矛盾。
4. **22 项 Radio 占位服务因 VINTF 缺失被 `servicemanager` 拒注**：`cdh_radio_placeholders.txt` 声明的 16 项 `android.hardware.radio.*` 与 6 项 `vendor.mediatek.hardware.mtkradioex.*` 未进入 `cdh_vendor_placeholders.xml`，`dmesg` 报 `Could not find ... in the VINTF manifest`，导致 `services` 域少 22 个服务。
5. **`ro.frp.pst` 与 `oem_lock` / `persistent_data_block` 启动期被误删**：`ro.frp.pst` 运行态缺失，`oem_lock` 与 `persistent_data_block` 两个系统服务未注册，`sys.oem_unlock_allowed` 缺失。
6. **宿主 `cdha` 重启导致存量云机 `secure_env` / `net-host-service` 失联**：`cdha` 重启时旧 `net-host-service` 因匿名管道关闭触发 `SIGPIPE` 退出，且新 `cdha` 随机分配本地 SOCKS5 端口使存量实例 `wifi-aps.json` 失效；`secure_env` 断连则导致 `keystore2` 与 `attestation_integrity` 探针阻塞超时。

## 2. 根因

1. **指纹构建裁剪与会话回调缺失**：`09_build_vendor/assemble.go` 在未检测到指纹 HAL 二进制时自动移除 `android.hardware.fingerprint.xml`；而 `android.hardware.biometrics` 缺少仅编译发布 `IFingerprint/default`（剥离 `face-service` 与 `virtualhal`）的纯净变体，且 `fingerprint_service.cpp` 的占位会话未实现 `onEnrollmentsEnumerated` / `onEnrollmentsRemoved` / `onAuthenticatorIdRetrieved` / `onSessionClosed` 等异步回调。
2. **传感器无实例种子且陀螺量化步长退化**：`sensors_service.cpp` 未向 `SensorEventModel` 注入单机持久身份种子；同时陀螺仪硬件 ADC 离散步进应为 `8 * (resolution / 8) = resolution`（`0.0011 rad/s`，见 `../issue-sensors-adc-quantization-and-gyro-drift/`），若按 `resolution / 8`（`0.0001375`）量化会导致 `minDiff < 0.0008` 触发 `D3_sensorsAdcQuantizationAndGyroDriftReal = false`。
3. **内嵌 `vendor/odm` 被误判为直通独立 `odm` 分区**：`09_build_vendor/files_compat.go` 的 `synthesizePlaceholderVintfManifest` 在 `odmPassThrough == true` 时扫描了 `filepath.Join(root, "odm", "etc", "vintf")`（即官方 `unpack_vendor/odm/etc/vintf/manifest_dsds.xml`）。但无独立 `odm` 分区的机型（如 `gold`）运行态已由新 `vendor.img` 整盘覆盖 `/vendor`，官方内嵌 `vendor/odm` 片段根本不在运行态生效，误将其计入 `existingInstances` 导致 22 个 Radio 服务跳过合成。
4. **`frp` 软链时机晚于 `service_daemon` 早启动清理**：`frp_symlink.rc` 原在 `on post-fs-data` 才创建 `/dev/block/by-name/frp -> /dev/block/vda3`，晚于 `class core` 的 `service_daemon` `scrub_early_virtual_props()` 检查 `Path::new(frp_trimmed).exists()`，导致 `ro.frp.pst` 被当作无效残留属性清除。
5. **伴生网关走父进程管道与随机代理端口**：`daemon/backend/net.go` 使用自定义 `io.Writer`（`multiFileWriter`）承接 `net-host-service` 日志，Go `exec.Cmd` 为非 `*os.File` 自动创建父子进程 pipe，父进程 `cdha` 退出即关闭读端触发子进程 `SIGPIPE`；同时 `proxy.go` 默认传 `127.0.0.1:0` 随机端口且启动时未回刷存量运行实例。

## 3. 证据

- 修复前实测失败记录与差异明细见 [`log-fail.txt`](log-fail.txt)（`patch.img` `a34a8915dce6...`）。
- 修复后编译单测、开机冒烟与在线双机对拍通过记录见 [`log-pass.txt`](log-pass.txt)（`patch.img` `09c8a3dbce59...`，`run-00378` ~ `run-00404`）。
- 相关历史参考：`../../gold/issue-gold-os3-0-11-real-device-align/`、`../issue-sensors-adc-quantization-and-gyro-drift/`。

## 4. 改动

全程严格遵循 `AGENTS.md §8`，零修改 `super.img` / `system.img`，所有改动均收敛于自研 HAL、机型配置、构建管线与宿主 `daemon`：

1. **虚拟指纹 HAL 与纯净 `fingerprint-only` 变体**：
   - `devices/hardware/android.hardware.biometrics/src/fingerprint_service.cpp`：新增零外部依赖的 `/vendor/etc/fingerprint_config.xml` 解析器（缺省回退原 `REAR` + `CONVENIENCE` 保持向后兼容），支持配置 `sensor-type`（`POWER_BUTTON` / `UNDER_DISPLAY_OPTICAL` 等）、`strength`（`STRONG`）、`max-enrollments`、`SensorLocation` 及初始模板；升级 `FakeSession` 完整闭环 `ISessionCallback`（`onEnrollmentsEnumerated`、`onEnrollmentsRemoved`、`onAuthenticatorIdRetrieved`、`onChallengeGenerated`、`onSessionClosed` 及 `cancel -> onError(CANCELED, 0)`）。
   - `agent/modules/pipeline/stages/08_build_hardware/modules/android.hardware.biometrics.json` 与 `face_service.cpp` / `*_virtualhal_service.cpp`：新增 `"fingerprint-only"` 变体（`-DCDH_BIOMETRICS_FINGERPRINT_ONLY=1` + `vendorTreeExclude`），仅编译并发布 `android.hardware.biometrics.fingerprint-service`，彻底阻断 `IFace/default` 与 `virtualhal` 暴露。
   - `09_build_vendor/files_compat.go` & `assemble.go`：补齐主服务二进制 `hal_fingerprint_default_exec` 标签，并在启用 `fakefingerprint` / `fingerprint-only` 时自动兜底生成 `vendor/etc/permissions/android.hardware.fingerprint.xml`。
   - `devices/manifest/configs/redmi/gold/gold-android15/`：启用 `android.hardware.biometrics@fingerprint-only` 并新增侧边电源键电容指纹配置 `vendor/etc/fingerprint_config.xml`（`power_button` + `strong` + `max-enrollments=5`）。
2. **传感器静态对齐与单机种子动态防聚类模型**：
   - `devices/manifest/configs/redmi/gold/gold-android15/vendor/etc/sensors_config.xml`：`handle=2` 对齐在线真机 `qmc6308`（`vendor="QST"`）。
   - `devices/hardware/android.hardware.sensors/src/sensor_event_model.{h,cpp}` & `sensors_service.cpp`：引入 64-bit `instanceSeed`（由单机持久身份属性 `ro.vendor.oem.*` / `ro.ril.oem.*` / `ro.boot.serialno` FNV-1a 派生，并在 `initialize()` 二次刷新防早启动竞态）及 `sepolicy_cil.go` `reconcileSensorsIdentityPropsCIL` 只读授权；按单机种子派生球面模长守恒（$\approx 9.80665\text{ m/s}^2$）桌面微倾角、自洽地磁方位/四元数、单机 `UNCALI_GYRO` 零偏（保持 `UNCALI_ACC`/`UNCALI_MAG` 零偏为 `0`）与无状态 Box-Muller 高斯白噪声，保持加速度计 `13*(res/8)=0.00195`、陀螺仪 `res=0.0011`、磁力计 `res/8=0.01875` 真实硬件 ADC 量化步进；同步补齐 `sensor_event_model_test.cpp` 单测。
3. **Widevine、FRP 时序与内嵌 ODM VINTF 合成修复**：
   - `gold` `vendor/config.json`：`"ro.vendor.drm.widevine.security_level"` 由 `"L3"` 改为 `"L1"`。
   - `gold` `vendor/etc/init/frp_symlink.rc`：提前至 `on early-init` 与 `on init` 建立 `/dev/block/by-name/frp -> /dev/block/vda3`。
   - `09_build_vendor/files_compat.go` & `files_test.go`：将 `plan.VendorDir/odm/etc/vintf` 纳入 `existingInstances`，移除对已失效内嵌 `root/odm/etc/vintf` 的误去重（仅保留独立 `unpack_odm` 与 `odm(_a).img`），补齐 `hal_radio_default -> hal_radio_service` 判定及回归单测 `TestSynthesizePlaceholderVintfManifestIgnoresEmbeddedOfficialVendorOdm`。
4. **宿主守护进程伴生服务抗重启加固**：
   - `daemon/backend/tee/secure_env.go` & `daemon/backend/net.go`：设置 `Setpgid: true`；`net.go` 移除匿名管道 `multiFileWriter`，改为直接将 `*os.File` 传给 `cmd.Stdout/Stderr` 根除父进程退出引发的 `SIGPIPE`，并抽出 `RefreshInstanceNetConfigs` + `net_test.go` 单测。
   - `daemon/run/proxy.go` & `http.go`：未指定监听地址时优先绑定固定本地端口（WiFi `127.0.0.1:19051` / Cellular `127.0.0.1:19052`，冲突自动回退动态端口），并在 `Serve` 启动时执行 `reconcileRunningNetBackends` 自动回刷代理配置并恢复失联 `net-host-service`。

## 5. 验证

1. **单元测试门禁**：
   - C++ 宿主单测：`sensor_event_model_test`（`-Wall -Wextra -Werror`）3 项测试全部 `PASS`（`accDist=0.5359, biasDist=0.005104`，陀螺 `minDiff=0.0011`）。
   - Go 单元测试：`agent/modules/pipeline/...` 全 28 包与 `agent/modules/daemon/...` 全 9 包测试 100% 通过。
2. **载体镜像增量构建与重启冒烟**：
   - 执行 `pipeline device build gold --manifest 15-OS3.0.11.0.VNQCNXM` 产出并发布 `patch.img`（SHA-256 `09c8a3dbce593150398c0bc7dd3457be02d13544e8dabfa66e921c18f8997441`，未触碰 `super.img`/`vda.img`）。
   - `gold` #1 重启 22s 达成 `sys.boot_completed=1`：`ro.frp.pst=/dev/block/by-name/frp`（`exported_default_prop`）、`sys.oem_unlock_allowed=0`、`widevine=L1`、`init.svc.vendor.fingerprint-default=running`、`dumpsys fingerprint` 显示 `Type: 3 (POWER_BUTTON), Current operation: null, Pending operations: 0`，`service list` 无任何 `virtualhal` 或 `IFace` 暴露。
3. **在线真机 × 云机 `device-checker` 实测终验（`run-00378` ~ `run-00404`）**：
   - `aligner.app_full`（`run-00378`）：`fatal: 1 -> 0`、`availability: 1 -> 0`（93/93 系统特性 100% 零差异）；
   - `aligner.sensors_full`（`run-00402`）：`fatal: 2 -> 0`、`state: 0`、`availability: 0`（29/29 传感器全零差异，且两次重启间单机姿态均值 `[-0.157, -0.285, 9.803]` vs `[-0.156, -0.284, 9.801]` 严格幂等稳定，陀螺 `minDiff=[0.0011, 0.0011, 0.0011]`，`D1..D4` 全绿）；
   - `aligner.services_full`（`run-00380`）：`fatal: 40 -> 16 (-24)`、`availability: 4 -> 0`（`getService` 88/88、`getSystemService` 115/115 完全对齐真机，22 项 Radio 占位 + `fingerprint` + `oem_lock` + `persistent_data_block` 全部就位）；
   - `aligner.ids_full`（`run-00403`）：`fatal: 10 -> 9 (-1)`；
   - **14 套 `ultimate.*` 风控探针**：**14/14 全部双端 `succeeded`**，其中 `hardware_env`（`run-00397`，`U-HW-WIDEVINE` 已消除）、`hook_inject`（`run-00400`）、`cloud_emulator`、`native_env` 等 **13 套达成 `cloud_only = 0`**！

## 6. 遗留

1. `ultimate.attestation_integrity`（`run-00393`）3.4s 正常返回 `cloud_only = ['U-ATTEST-APPID', 'U-ATTEST-ROOT-KEY', 'U-ATTEST-SIGN-PROBE']`：系宿主 `secure_env` 使用本地自签测试根（`KEY_UNKNOWN`）而非 Google/OEM 硬件出厂私钥链的固有物理边界。
2. `aligner_xiaomi_gold.all`（`run-00404`）云机得 `31/32`（高于解锁 BL 真机的 `30/32`），唯一未勾选项 `E2_mediaCodecsHasMtkAndDolbyNoV4l2Leak` 系 `gold` 开启宿主硬件视频加速（`hardware-media: true`）刻意保留的 3 条 `c2.v4l2.*` 编解码器声明。
3. 其他需要启用侧边/屏下虚拟指纹或同步新版 `sensors` 防聚类模型的机型，按需在机型配置添加 `fingerprint_config.xml` 并重跑 `08 -> 09 -> 11` 刷新 `patch.img` 即可。
