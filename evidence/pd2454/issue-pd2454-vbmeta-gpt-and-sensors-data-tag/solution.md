# #262 vivo X200 Ultra (`vivo/PD2454`) 一阶段 `VMountVerifyPartitions` 真块设备 `vbmeta_a` 与二阶段 `SensorService` `Tag::data` / `media_config.xml` / `tango` 闭环

- **编号**：#262
- **设备 / 机型**：`vivo/PD2454`（基线 `15-AP3A.240905.015.A2-20250416`，`lineage-22.2` + `kernel 6.6`）
- **固件包**：`roms/devices/vivo/（X200 Ultra）PD2454_MA_15.0.10.24.W10.V000L1.zip`
- **状态**：已闭环（2026-11-19）

## 1. 问题

`vivo/PD2454` 在首个冷启动轮次接连遇到一阶段阻断与二阶段崩溃循环：

1. **一阶段 `VMountVerifyPartitions` 失败重启到 bootloader**：
   - `init` 执行完 `FirstStageMount` 并切根 `/system` 后，在 `SetupSelinux` 之前执行 `VMountVerifyPartitions`，报错：
     - `Device path not found: /dev/block/by-name/vbmeta_a`
     - `avb_slot_verify failed, result: 2`
     - `VMountVerifyPartitionsError: open AvbHandle fail, reboot into fastboot!: No such file or directory`
   - 若将 `/dev/block/by-name/vbmeta_a` 重定向为 `/mnt/vendorpatch` 下的普通文件，会因普通文件不支持块设备 `ioctl` 而失败。
2. **二阶段 `system_server` `SensorService` `SIGABRT` 循环崩溃与守护进程重启风暴**（150s / 360s 均无法达到 `sys.boot_completed=1`）：
   - `system_server` 启动约 2 秒即在 `SensorService` 线程触发 `SIGABRT`：
     `Event.h:695: ... EventPayload::get<Tag::data>() ... assertion "bad access: a wrong tag" failed`（`convertToSensorEvent` + `pollFmq`）；
   - `android-hardware-media-c2-hal`、`vendor-dolby-media-c2-hal`、`netboosterd` 每 ~5 秒退出（`status 1`），`zygote_tango` 每 ~5 秒触发 `signal 4` 生成 158 KiB tombstone，连带反复触发 `flags_health_check UPDATABLE_CRASHING`。

## 2. 根因

1. **一阶段 `vbmeta_a` 与 `AvbHandle::Open()` 依赖**：
   - vivo 原厂 `/system/bin/init` 的 `VMountVerifyPartitions` 会扫描系统/厂商校验分区并打开 `AvbHandle`；此前 `06_build_vendor_boot` 剥离了全部 `avb` 挂载标记且云机 GPT 盘缺失 `vbmeta_a` 真块设备，导致 `libfs_avb` 在 `Device path not found: /dev/block/by-name/vbmeta_a` 报错并强制重启到 `bootloader`；
   - 通过在 `06_build_vendor_boot` 为 `/dev/block/vdc /mnt/vendorpatch` 附带 `,avb=vbmeta`（`fstab-rev: merge-v14`）使 `AvbHandle::Open()` 走标准 vbmeta 流程（配合 `androidboot.vbmeta.digest` 与禁用校验描述符，使 `VMountVerifyPartitions` 读取到空 verify fstab 而放行），同时在 `13_pack_vda` 为 `vda.img` 真实追加 `vbmeta_a` GPT 分区并写入 4096 B `avb.DisabledVBMetaImage()`（供 `ioctl`/`BLKGETSIZE64` 与回读校验）。
2. **二阶段三类运行态阻断**：
   - **vivo `libsensorservice.so` 私有事件载荷契约**：反汇编 `system_a.img:/system/lib64/libsensorservice.so` 的 `convertToSensorEvent`（`0x639cc`）及其 43 项跳转表（`0x17128`）证实，vivo OriginOS 5 将 `SensorType` `3` (`ORIENTATION`)、`5` (`LIGHT`)、`6` (`PRESSURE`)、`7` (`TEMPERATURE`)、`8` (`PROXIMITY`)、`36` (`HINGE_ANGLE`) 全部跳往 `0x64010` 分支，读取 `EventPayload::Tag::data (0xa)`；此外 `PD2454` `sensors_config.xml` 存在误标为 `type-id="34"` 的 `HEAD_TRACKER`（真 `HEAD_TRACKER=37` 对应 `Tag::headTracker (11)`，虚拟传感器 HAL 未实现）。
   - **Codec2 假表实例名错配**：引擎模块 `devices/hardware/android.hardware.media` 固定启动 `android.hardware.media.c2-mediatek`（`default1`）与 `vendor.dolby.media.c2-default-service-dax`（`default9`），并在 `codec2_store_service.cpp` 以 `executableBaseName()` 查 `<instance binary="..." store="..."/>`；`02_prepare` 生成器 `buildMediaConfigXML` 此前把实测官方名和 `store="rank=1"` 直接写入 `media_config.xml`，造成两进程查找自身失败以 `status 1` 退出。
   - **32 位 `tango` 与 `netboosterd`**：`/system/etc/init/tango-binfmt.rc` 拉起 32 位 `zygote_tango` 在纯 64 位云机环境发生 `SIGILL`（`signal 4`），`/system/etc/init/netboosterd.rc` 依赖缺失内核能力以 `status 1` 退出；需在 `vendor/config.json` 补齐纯 64 位 `ro.vendor.product.cpu.abilist*` 并通过 `/system/etc/init/*.rc` 重定向到 `/mnt/vendorpatch/vendor/etc/init/cph_empty.rc` 屏蔽。

## 3. 证据

- 一阶段失败日志与堆栈：见同目录 [`log-fail.txt`](log-fail.txt)（摘自 `pd2454-vmount-fail.log`、`pd2454-boot2-qemu.log` 与 `tombstone_00`）。
- 汇编验证（官方 `/system/lib64/libsensorservice.so`）：`0x64014: ldrb w9, [x0, #0x10]; cmp w9, #0xa; b.ne 0x64144`（跳转表项 `type 3/5/6/7/8/36` 均指向 `0x64010`）。
- 二阶段修复后打包与启动日志、验收截图：见同目录 [`log-pass.txt`](log-pass.txt) 与 [`pass-screen.png`](pass-screen.png)。

## 4. 改动

1. **一阶段 AVB 禁用态真块设备支持（跨机型基础设施）**：
   - [`agent/modules/pipeline/stages/02_prepare/apply_profile_configs.go`](../../../agent/modules/pipeline/stages/02_prepare/apply_profile_configs.go) & [`apply_profile_test.go`](../../../agent/modules/pipeline/stages/02_prepare/apply_profile_test.go)：移除旧 `/dev/block/by-name/vbmeta_a` 普通文件重定向方案；
   - [`agent/modules/pipeline/stages/06_build_vendor_boot/{fstab.go,fstab_merge.go,plan.go,vendor_boot_test.go}`](../../../agent/modules/pipeline/stages/06_build_vendor_boot/fstab.go)：当 `verified-boot-hash == DisabledVBMetaDigest` 时以 `merge-v14` 在 `vdc` 行追加 `,avb=vbmeta`；
   - [`agent/modules/pipeline/stages/13_pack_vda/{plan.go,vda.go,plan_describe.go,plan_test.go}`](../../../agent/modules/pipeline/stages/13_pack_vda/vda.go)：为禁用校验机型在 `vda.img` GPT 追加 `vbmeta_a`（1 MiB 对齐槽位，写入 4096 B `DisabledVBMetaImage()` 并回读校验）。
2. **`02_prepare` 通用模板修复（`vivo`/`iqoo` 传感器载荷与 Codec2 实例规范化）**：
   - [`agent/modules/pipeline/stages/02_prepare/ai_profile.go`](../../../agent/modules/pipeline/stages/02_prepare/ai_profile.go)：为 `AISensorDetailSpec` 增加 `Payload` 字段；
   - [`agent/modules/pipeline/stages/02_prepare/vendor_etc_templates.go`](../../../agent/modules/pipeline/stages/02_prepare/vendor_etc_templates.go)：
     - 新增 `sensorPayloadAttr` / `formatSensorPayloadAttr`：对 `vivo`/`iqoo` 的 `type-id` `3, 5, 6, 7, 8, 36`（与 `honor` `5, 8`）输出 `payload="data"`，并在 `renderAISensorListXML` 过滤 `head_tracker` / `type 37`；
     - 在 `buildMediaConfigXML` 保底注册 `android.hardware.media.c2-mediatek -> default1` 与 `vendor.dolby.media.c2-default-service-dax -> default9`，清洗含 `c2pa` 或 `store="rank=1"` 的非法实例，并将未命中合法 store 的组件重映射到 `default1`/`default9`；
   - [`agent/modules/pipeline/stages/02_prepare/vendor_etc_configs_test.go`](../../../agent/modules/pipeline/stages/02_prepare/vendor_etc_configs_test.go)：新增 `TestVivoSensorsPayloadAndMediaInstancesNormalized`。
3. **`vivo/PD2454` 配置收敛**：
   - 经 `cdh-pipeline device apply-profile vivo/PD2454 --manifest 15-AP3A.240905.015.A2-20250416 --overwrite` 重新生成 [`vendor/etc/sensors_config.xml`](../../../devices/manifest/configs/vivo/PD2454/PD2454-android15/vendor/etc/sensors_config.xml) 与 [`vendor/etc/media_config.xml`](../../../devices/manifest/configs/vivo/PD2454/PD2454-android15/vendor/etc/media_config.xml)；
   - 在 [`vendor/config.json`](../../../devices/manifest/configs/vivo/PD2454/PD2454-android15/vendor/config.json) 补齐 `ro.vendor.product.cpu.abilist=arm64-v8a`、`abilist32=""`、`abilist64=arm64-v8a`；
   - 新增 [`vendor/etc/init/cph_empty.rc`](../../../devices/manifest/configs/vivo/PD2454/PD2454-android15/vendor/etc/init/cph_empty.rc) 并在 [`module/config.json`](../../../devices/manifest/configs/vivo/PD2454/PD2454-android15/module/config.json) 追加 `/system/etc/init/tango-binfmt.rc` 与 `/system/etc/init/netboosterd.rc` 文件重定向（共 14 条）。

## 5. 验证

1. **单元测试**：
   - `cd agent && TMPDIR=/Volumes/work/cloud-phone-hub/out/agent/tmp go test -count=1 ./modules/pipeline/internal/avb/... ./modules/pipeline/stages/02_prepare/... ./modules/pipeline/stages/06_build_vendor_boot/... ./modules/pipeline/stages/09_build_vendor/... ./modules/pipeline/stages/13_pack_vda/...` 全部 `ok`；
   - 更新 `vendor_etc_templates.go` 后复跑 `go test -count=1 ./modules/pipeline/stages/02_prepare/...` 通过（`0.474s`），含新增 `TestVivoSensorsPayloadAndMediaInstancesNormalized`。
2. **实机冷启动验收**（增量重打 `05/06/09/11/13`，独占 CPU 跑 `verify_boot.py`）：
   - 一阶段日志 `[1.166212] init: VMountVerifyPartitions: verify fstab empty`，顺利挂 `/data`；
   - 二阶段一次冷启动 `passed: true`、`bootCompleted: true`、`postBootCompleted: "1"`、`bootanim: "stopped"`、`surfaceFlingerState: "running"`、`serviceCount: "387"`、`bootReason: "reboot,factory_reset"`；
   - `topActivity`: `com.vivo.setupwizard/com.vivo.setupwizard.ui.SetupWizardActivity`；
   - VNC 截图 [`pass-screen.png`](pass-screen.png)（1440×3168，`meanBrightness: 225.25`）人工审图确认简体中文 OriginOS 向导页（「开启 OriginOS」按钮、状态栏与无障碍入口）渲染完整清晰。

## 6. 遗留

- 当前停留在 `com.vivo.setupwizard/.ui.SetupWizardActivity` 首启向导页，尚未模拟点击跳过向导进桌面；后续如需验证 vivo 预装桌面及应用可继续点击向导验证。
