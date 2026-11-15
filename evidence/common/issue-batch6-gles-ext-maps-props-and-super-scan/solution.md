# #232 Batch-6 对齐收口（桌面 GLES 扩展剥离、GPU/SO `maps` 归一化、全量 `dev.mnt.*`/`tmpfs` 对齐、`super_scan` `NaN` 修复与 `ultimate.*` 零云机独有命中）

## 问题

Batch-5 收敛后，`gold`（`OS3.0.11.0.VNQCNXM`）在普通 App 与风控检测视角下仍残留以下关键特征与阻塞项：

1. **`display`**：`GL_RENDERER` 已对齐 `Mali-G57 MC2`，但 `gles.extensions` 仍漏出 24 项桌面 GPU 独有扩展；
2. **`native_env` / `proc`**：`loadedVendorSo` 仍含仅云机独有项（`libgpudataproducer.so`、`libpng.so` 等），普通应用 `/proc/*/maps` 与 `readlink` 中 `/dev/dri/{renderD128,card0}`、`vendor_mesa_prop`/`vendor_minigbm_debug_prop` 尚未归一；
3. **`proc` / `build_props`**：`tmpfs_limits` 与真机差 `3776kB` 致 13 条根 `tmpfs` `superOptions` 能力偏离，且 `dev.mnt.*` 仍缺 `super` 逻辑分区与 `data*` 完整映射；
4. **`ids`（传感器拟真）**：加速度计静置重心接近水平零偏，磁力计单频波形规律易被时序特征识别；
5. **`ultimate.super_scan`**：`super_scan.js` 在无法读取功率或 `/proc/uptime` 时写出 JS `NaN`/`Infinity`，被 `Checker.kt` 序列化为非法 JSON 导致全扫描超时失败；且 14 套并发下发时 `U-BINDER-TRANSACT` 易受 VM CPU 调度抖动触发误报。

## 根因

1. `cdh_gl_strings.c` 剥离函数 `cdh_stripped()` 仅做前缀比较，直接列入 `GL_EXT_draw_buffers` 会误伤真机原生的 `GL_EXT_draw_buffers_indexed`；
2. `gold-android15.json` 内 3 条 `maps` 规则指向真机当前进程未加载的 `libgpudataproducer.so` / `libpng.so`，且 `cdh_redirect` 尚未接管虚拟 DRM 设备节点与 Mesa 属性节点 VMA；
3. `profile.rs` 中 `MOUNT_REWRITES_GOLD` 未对逻辑分区与 `data*` 启用全量强制补齐，`tmpfs_limits` 沿用旧值 `(3_863_628, 965_907)`；
4. 参照 [`issue-aligner-media-int-ctor`](../issue-aligner-media-int-ctor/solution.md) 的同源序列化根因，`Checker.kt` `JsJson.rec` 对 `Double.isNaN()` / `Double.isInfinite()` 未按 RFC 8259 转 `"null"`，且 `super_scan.js` 对 `pwr.powerW`、`upSec`/`elapSec` 未做 `isFinite` 守卫。

## 证据

见同目录 [`log-fail.txt`](log-fail.txt) 与 [`log-pass.txt`](log-pass.txt)。另参照前序关联条目：[`issue-sphal-caller-gate-and-maps-inode-align`](../issue-sphal-caller-gate-and-maps-inode-align/solution.md)、[`issue-dev-mnt-props-and-mountinfo-inlinecrypt-align`](../issue-dev-mnt-props-and-mountinfo-inlinecrypt-align/solution.md)。

## 改动

1. **桌面 GLES 扩展精确剥离**：
   - `engine/vendor/hardware/android.hardware.gpu/src/cdh_gl_strings.c`：在 `cdh_stripped()` 新增 `'='` 前缀精确全名匹配与 `CDH_GL_STRINGS_HOST_TEST` 单测开关，同步补齐 `cdh_gl_strings_test.c`；
   - `engine/vendor/build/modules/android.hardware.gpu.json`：追加 24 条 `'='` 精确规则剥离全部桌面 GPU 独有扩展，保留真机子集匹配项。
2. **GPU 设备 / 属性 VMA 与 `loadedVendorSo` 归一化**：
   - `devices/modules/cdh_redirect/hook/maps.c`：新增 `cdh_dev_map_rules[]`，在 `uid > max_uid` 下将 `/dev/dri/renderD128`、`/dev/dri/card0` 改写为 `/dev/mali0`（固定回退 `CDH_FAKE_MALI0_INO = 1562UL`），并将 `vendor_mesa_prop`、`vendor_minigbm_debug_prop` 分别改写为 `vendor_mtk_soc_prop`、`vendor_mtk_camera_prop`，同步扩展预热表与缓冲区变长搬移逻辑；
   - `devices/modules/cdh_redirect/hook/readlink.c`：对 `/dev/dri/{renderD128,card0}` 反向改写为 `/dev/mali0` 并约束 `data->buflen` 拷出边界；
   - `devices/manifest/configs/redmi/gold/gold-android15.json`：将原 3 条 `maps` 规则目标替换为真机已加载的原厂 `/vendor/lib64/mt6833/libdpframework.so`、`/vendor/lib64/libunwindstack.so`、`/vendor/lib64/liblzma.so` 并随包注入官方 `.so`。
3. **传感器静置微倾与磁力计非公度微抖动**：
   - `engine/vendor/hardware/android.hardware.sensors/src/sensor_event_model.cpp`（及 `sensor_event_model_test.cpp`）：加速度计均值校准为 `(0.142, 0.268, 9.80196)`（保持合重力 `9.80665 m/s²`），同步更新 `case 9` 重力模型；磁力计 `case 2` 改用非公度双频正弦合成，使 1.5s 采样窗口各轴标准差落在 `0.40~0.65 uT`。
4. **`service_daemon` `dev.mnt.*` 全量补齐与 `tmpfs_limits` 校准**：
   - `engine/vendor/hardware/android.service.daemon/src/rust/src/props/{profile.rs,scrub.rs}`：新增 `MOUNT_REWRITES_GOLD`（27 项，含静态分区、`sdc65/dm-10..17`、`sdc66/dm-58` `data*`）及 `enforce_exact_mounts = true`；将 `gold` `tmpfs_limits` 校准为 `(3_859_852, 964_963)`；
   - `engine/vendor/hardware/android.service.daemon/vendor/etc/init/cph_daemon_service.rc`：清理已由早期阶段接管的 `FIRST_STAGE_SNAPUSERD_*` 冗余导出块。
5. **`ultimate.super_scan` `NaN`/`Infinity` 修复**：
   - `core/development/device-checker/checks/java/ultimate/super_scan.js` L132–143：`H-57` 以 `isFinite(rawPowerW)` 守卫输出 `null`，`H-84` 对 `upSec` / `elapSec` 做有限正值校验并在不可读时返回 `null`；
   - `core/development/device-checker/app/src/main/java/dev/checker/Checker.kt` L41：`JsJson.rec` 对 `is Double` 增加 `if (v.isNaN() || v.isInfinite()) b.append("null")` 守卫。

## 验证

1. **宿主单测**：
   - `cdh_gl_strings_test`：`5/5 passed`；
   - `sensor_event_model_test`：`test_acc_and_gyro_stats_at_50hz`（`n=75, norm=9.8144, magSigma=(0.409,0.517,0.580)`）与 `test_next_wait_and_catchup` 均 `PASS`；
   - （注：`service_daemon` 的 `cargo test` 在 macOS 宿主因 `resetprop` 仅限 Android 目标而跳过，已随重编 `patch.img` 在云机实测闭合）。
2. **`aligner.*` 19 域全量对比（`run-00255..00275`，真机 `07cf5fc1784783d5` vs 云机 `98a058b2b0618483-cloud`）**：
   - **8 域已完全对齐（`fatal=0`）**：`inputs`、`sensors`、`camera`、`media`、`battery_power`、`cpu_memory`、`app`、`network`；
   - `display`：`gles.extensions` 云机仅剩 58 项且 `仅云机有 = []`（`capability=5, state=0`）；
   - `native_env`：`loadedVendorSo.{gpuSo,vendorSo}` 均达到 `仅云机有 = []`，`identity.base_apk` 差异清零；
   - `build_props`：`props.dev.mnt.*` 差异 `0`（`raw` 降至 `429`）；
   - `proc`：原 13 条根 `tmpfs` 能力差异清零（`capability` `18 -> 5`）。
3. **风控检测 `ultimate.*` 14 套 338 项全量串行验证（`run-00279..00292`）**：
   - `28/28 succeeded`（零失败、零超时），**全部 14 套 `cloud_only = []`**（338 个子检测项无一云机独占命中）；
   - `ultimate.super_scan`（`run-00291`）真机 `1653ms` / 云机 `871ms` 完成（< 5s 要求），`H-57` 与 `H-84` 均干净返回 `null` / `hit=false`；
   - 注：先前 `run-00277` 将全部 14 套并发同时下发时，`ultimate.hook_inject` 的 `U-BINDER-TRANSACT` 因单进程内 14 个 Rhino 线程并发抢占触发 10ms 量级调度抖动（云机 `ms=[10.2, 8.6, 52, 42.1, 26.4]`）；经 `run-00278` 孤立复测（`ms=[7.7, 7.8, 10.3, 8.4, 9.8], ratio=[1.013, 1.338, 1.091, 1.273], hit=false`）及 `run-00286` 串行实测（`ms=[7.6, 7.3, 7.7, 6.7, 7.5], ratio=[0.961, 1.013, 0.882, 0.987], hit=false`），确认该时序检查在正常无高并发抢占场景下真云一致通过，不弱化 `hook_inject.js` 检测阈值。

## 遗留

1. **刻意排除项（按规划保持）**：用户明确排除的第 11 项（ADB Shell 视角差异：`u:r:pioneer:s0`、shell `ro.vendor.cdh.*`、`hwsim0`）、32 位 ABI / `ro.hwui.use_vulkan=false` 策略排除、硬件熔丝 Key Attestation 及 51 项仅真机 Mali 硬件 GLES 扩展按兼容原则保留现状；
2. **低优先级结构性差异**：底层 `dm-*`/`loop*` 块设备号与 `fsid` 偏移、`/dev/usb-ffs/*`、`settings` 真机长期使用态累积键、SIM 运营商（真机电信 `46003` vs 云机联通虚拟卡 `46001`）留待后续基线迭代按需收口。
