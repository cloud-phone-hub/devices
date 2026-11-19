# #253：`02_prepare` 生成与对齐 `audio_config.xml` 强制保证包含 `r_submix` 解决 `audioserver` 死锁

- 设备：跨机型通用（首发定因并闭环于 `meizu/Meizu22` `16-AQ3A.250411.001-20260127`）
- 日期：2026-11-18
- 状态：**已闭环**（`02_prepare` 单测全绿，`Meizu22` 补齐 `modules="default,bluetooth,usb,r_submix"` 重构后 ~80s 完成冷启动进入 Flyme 桌面）

---

## 一、问题

`meizu/Meizu22`（`16-AQ3A.250411.001-20260127`，Android 16）在解除 Widevine VINTF 冲突后，`SurfaceFlinger` 以 56 FPS 播放开机动画，`zygote64` 与 `system_server` 均已启动，但运行超过 230 秒仍无法完成 `sys.boot_completed=1`，`system_server` 长期阻塞在 `StartAudioService`；同时 `qemu.log` 持续每 ~1 秒刷出 `Could not find 'aidl/android.hardware.audio.core.IModule/r_submix'`，`audioserver` 每 10 秒因 `TimerThread: TimeCheck ... AudioPolicyService_onFirstRef` 触发 SIGABRT 重启循环。

---

## 二、根因

1. **音频引擎与音频策略 XML 模块声明不一致**：
   - `Meizu22` 的 `/vendor/etc/audio_policy_configuration.xml` 与自研 [`android.hardware.audio-service.xml`](../../../hardware/android.hardware.audio/vendor/etc/vintf/manifest/android.hardware.audio.service.xml) 均声明了 `<xi:include href="r_submix_audio_policy_configuration.xml"/>` 与 `IModule/r_submix`；
   - 然而 `Meizu22-android16/vendor/etc/audio_config.xml` 被 `02_prepare` 写成 `<configuration version="1.0" modules="default,bluetooth,usb">`，遗漏了 `r_submix`。
   - `android.hardware.audio.service` 读取 `audio_config.xml` 后仅注册了 `IModule/{default,bluetooth,usb}`（缺 `r_submix`），导致 `audioserver` 初始化 `AudioPolicyService` 时调用 `AServiceManager_waitForService("android.hardware.audio.core.IModule/r_submix")` 阻塞超时被 `TimeCheck` 强杀，连带 `SystemServer StartAudioService` 死等超时。
2. **模板生成与存量对齐未兜底**：
   - [`buildAudioEngineConfigXML`](../../../agent/modules/pipeline/stages/02_prepare/vendor_etc_templates.go) 直接使用 DSH AI 回复的 `ae.Modules`（当真机未显式列出 `r_submix` 时直接丢掉 `r_submix`）；
   - [`alignVendorExtraFields`](../../../agent/modules/pipeline/stages/02_prepare/apply_profile_configs.go) 在 `hasReadme=true` 时跳过已落盘的 `etc/audio_config.xml`，无法自动自愈历史配置。

---

## 三、证据

- 失败现场：[`log-fail.txt`](log-fail.txt)（`Meizu22` 运行态抓包，显示 `AudioPolicyService::onFirstRef -> AServiceManager_waitForService(r_submix)` `TimeCheck` 崩溃与 224 次 `Could not find aidl/android.hardware.audio.core.IModule/r_submix`，且 `service list` 仅见 `bluetooth/default/usb`）。
- 修复与通过现场：[`log-pass.txt`](log-pass.txt)（`TestAudioEngineConfigEnsuresRSubmix` 通过、`Meizu22` `audio_config.xml` 归一化为 `modules="default,bluetooth,usb,r_submix"`、`task-683` ~80s 完成冷启动进入 `QuickstepLauncher` 桌面）。

---

## 四、改动

1. **[`agent/modules/pipeline/stages/02_prepare/vendor_etc_templates.go`](../../../agent/modules/pipeline/stages/02_prepare/vendor_etc_templates.go)**：
   - 新增 `normalizeAudioEngineModules(raw)`（保留原模块顺序与去重，并强制补全 `"default"`、`"bluetooth"`、`"r_submix"`）及 `ensureAudioEngineConfigModules(xmlContent)`；
   - 在 `buildAudioEngineConfigXML` 中调用 `normalizeAudioEngineModules`。
2. **[`agent/modules/pipeline/stages/02_prepare/apply_profile_configs.go`](../../../agent/modules/pipeline/stages/02_prepare/apply_profile_configs.go)**：
   - 在 `alignVendorExtraFields` 处理 `fileTrees` 时，对存量 `etc/audio_config.xml` 不受 `hasReadme`/`overwrite` 限制执行 `ensureAudioEngineConfigModules` 自愈。
3. **[`agent/modules/pipeline/stages/02_prepare/apply_profile_test.go`](../../../agent/modules/pipeline/stages/02_prepare/apply_profile_test.go)** 与 `Meizu22` 同步：
   - 新增 `TestAudioEngineConfigEnsuresRSubmix`，重编 `cdh-pipeline` 并执行 `cdh-pipeline device apply-profile meizu/Meizu22` 自动修复 [`audio_config.xml`](../../../devices/manifest/configs/meizu/Meizu22/Meizu22-android16/vendor/etc/audio_config.xml)。

---

## 五、验证

1. `cd /Volumes/work/cloud-phone-hub/agent && TMPDIR=/Volumes/work/cloud-phone-hub/out/agent/tmp go test -count=1 ./modules/pipeline/stages/02_prepare/...` 全绿通过。
2. 核查全仓已生成的 7 份 `audio_config.xml`，均已包含 `r_submix`。
3. `meizu/Meizu22` 重构（`vda-6cb992ba9f6c` / `patch.img 9da7ccfc7a8f`）冷启动 ~80 秒通过 `verify_boot.py`（`sys.boot_completed=1`、`init.svc.bootanim=stopped`、前台 `com.meizu.flyme.launcher/...QuickstepLauncher`）。

---

## 六、遗留

- 后续 DSH 新采集的所有机型若模型输出漏写 `r_submix`，均已由 `normalizeAudioEngineModules` 在生成期自动兜底补齐，不再复发同类阻塞。
