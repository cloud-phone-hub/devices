# #209：移除 `persist.security.adbinput / adbinstall` 并对齐锁屏、常亮等 7 项默认设置

## 问题

`gold_15-OS3.0.11.0.VNQCNXM` 首轮 `aligner.build_props_full` 与 `aligner.settings_full` 对比真机暴露出明显的云机自动控屏 / 调试标志残留：

- `build_props`：仅云机存在 `persist.security.adbinput = "1"`、`persist.security.adbinstall = "1"`；
- `settings`（`provider`）：
  - `secure.lockscreen.disabled`：真机 `"0"`，云机 `"1"`（强行关闭锁屏）
  - `global.stay_on_while_plugged_in`：真机 `"3"`，云机 `"15"`（`svc power stayon true` 与 `put 7` 叠加导致全 4 bit 置位）
  - `system.time_12_24`：真机 `"24"`，云机 `"12"`
  - `system.accelerometer_rotation`：真机 `"1"`，云机 `"0"`
  - `system.long_press_power_key`：真机 `"launch_voice_assistant"`，云机 `"null"`
  - `secure.color_scheme`：真机 `"1"`，云机 `"3"`
  - `global.hide_gesture_line`：真机 `"0"`，云机 `"1"`

## 根因

1. `engine/vendor/hardware/android.service.daemon/vendor/etc/init/cph_start_adbd.rc` 在 `sys.boot_completed=1` 触发器里无条件执行 `setprop persist.security.adbinput 1` 与 `setprop persist.security.adbinstall 1`，实际上管理侧装包与操作可通过 root 通道完成，不需要在用户可读的 `persist.security.*` 属性里打开这两个开关。
2. `engine/vendor/hardware/android.service.daemon/vendor/bin/cph-setup-display` 为了防休眠直接写了 `stay_on_while_plugged_in 7`、`svc power stayon true`（写入 `15`）及 `lockscreen.disabled 1`（而后续已有 `vendor.keep_awake` 周期调 `cmd window dismiss-keyguard` 兜底解锁，无需置 `lockscreen.disabled=1`），且冷启动 provisioning 未补齐 24 小时制、自动旋转与 HyperOS/MIUI 默认显示交互项。

## 证据

- 修复前：`4503ac70a1c898f3-cloud` `build_props` 含 `persist.security.adbinput/adbinstall=1` 且 7 项 `settings.provider` 与真机相反，见同目录 [`log-fail.txt`](log-fail.txt)。
- 修复后：实例 `2`（`4d5128bd8ca07462-cloud`）实机检查 + `aligner.build_props_full` / `aligner.settings_full` 重采对拍见同目录 [`log-pass.txt`](log-pass.txt)。

## 改动

1. **`engine/vendor/hardware/android.service.daemon/vendor/etc/init/cph_start_adbd.rc`**：
   - 删除 `on property:sys.boot_completed=1` 下的 `setprop persist.security.adbinput 1` 与 `setprop persist.security.adbinstall 1`（保留注释说明管理侧通过 root 通道完成装包）。
2. **`engine/vendor/hardware/android.service.daemon/vendor/bin/cph-setup-display`**：
   - 删除 `svc power stayon true`；
   - 将 `stay_on_while_plugged_in` 从 `7` 改为 `3`，`secure.lockscreen.disabled` 从 `1` 改为 `0`；
   - 仅当冷启动 `device_provisioned != "1"` 时幂等写入 `system.time_12_24=24`、`system.accelerometer_rotation=1`；当检测到 MIUI（`getprop ro.miui.ui.version.name` 非空）时同步写入 `system.long_press_power_key=launch_voice_assistant`、`secure.color_scheme=1`、`global.hide_gesture_line=0`（不覆盖运行后人工调整）。

## 验证

1. 重编 `android.service.daemon` 并重跑 `07 → 09 → 11` 后，新建（新 `userdata`）实例 `gold_15-OS3.0.11.0.VNQCNXM/2`（`4d5128bd8ca07462-cloud`）启动实测：
   - shell 验证无 `persist.security.adbinput` / `persist.security.adbinstall`（`NO_PERSIST_SECURITY_ADB`），7 项设置读取全部与真机相等；
   - `aligner.build_props_full` 重采确认 `persist.security.adbinput` 与 `persist.security.adbinstall` 在 `build_props` 中均为 `False (None)`，diff 条目 `[]`；
   - `aligner.settings_full` 重采确认上述 7 项 `settings.provider` 值与真机 `07cf5fc1784783d5` **7/7 完全匹配**（且 `global.adb_enabled="0"`、`development_settings_enabled` 缺失，不暴露调试状态）。

## 遗留

1. 关闭 `persist.security.adbinput` 后，普通 `adb shell input keyevent ...` 不再有权限注入按键（管理侧需走事件注入或 virtio-input，本设计已在 `.rc` 注释说明）。
2. `settings` 仍有约 260+ 项真机长期日常使用积累项（小米穿戴、蓝牙半开、应用商店使用记录等），属 P2 使用态模板问题。
