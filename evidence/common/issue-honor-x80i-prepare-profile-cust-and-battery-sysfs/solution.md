# #341 闭环 `Honor X80i 5G` `02_prepare` 真机规格对齐、荣耀 `dload`/`cust` 身份基带提取、虚拟电池 `sysfs_power` 节点及 `nfc_sysfs` 门控防 `RescueParty`

- **编号**：#341
- **设备 / 分组**：`common`（跨机型管线 `02_prepare` 半成品占位拦截、跨节点 `FirmwareMeasured` 无损继承、荣耀 `version`/`product_h`/嵌套 `dload` 基带提取、`cdh_device_info.ko` 虚拟电池 `charge_full_design` + `sysfs_power` 策略、无 NFC HAL 机型 `/dev/pn544` 门控与首启 `UniHomeLauncher` OOBE 遮罩刷新，实机闭环 `Honor X80i 5G #1` `16-TP1A.220624.014-20260422`）
- **日期**：2026-11-26
- **状态**：**已闭环**（`Honor X80i 5G #1` 擦除 overlay 纯净首次冷启零触碰直达完整图标桌面、持续运行 >135s 零崩溃零 `RescueParty` + `关于手机` 全项真机对齐 + `02_prepare` 全包单测全绿）

---

## 1. 问题

继 [../issue-honor-x80i-yv12-v4l2-video-and-oobe-skip/](../issue-honor-x80i-yv12-v4l2-video-and-oobe-skip/)（#340）闭环欢迎页绿屏与设置引导跳过后，在线实例 **`Honor X80i 5G #1`**（`honor/HNLNA-M`，`16-TP1A.220624.014-20260422`）进入「设置 -> 关于手机」暴露多项硬件身份未对齐问题，并在对齐推进中暴露两项运行态连锁卡点：

1. **`02_prepare` 未完成真机规格对齐（「关于手机」多处缺失/错位）**（见 [about-fail.png](about-fail.png)）：
   - **分辨率错位**：卡片显示「荣耀绿洲护眼屏：`2600 x 1200`」，但实例实际运行分辨率仍为模板默认 `1080×2400 @ 480dpi`；
   - **电池显示「未知」**、**型号显示「`unknown`」**、**基带版本缺失**，且 `/vendor_dlkm/lib/modules/cdh_device_info.cpufreq` 未生成。
2. **首次冷启动 90.8s 触发 `RescueParty` 软重启并卡在二次开机动画**：
   - 刷入新版 `cdh_device_info.ko` 后，实例首次冷启动进入桌面约 18s（`90.8s`）即触发 `sys.powerctl=reboot,RescueParty`（`WARM_REBOOT for package com.android.nfc`）；客机内软重启后因宿主 `secure_env` 单向 `EARLY_BOOT_ENDED` 状态机封死，二次启动停在 `Waited one second for android.system.keystore2.IKeystoreService/default`。
3. **首启跳过向导后 `UniHomeLauncher` 停在纯壁纸遮罩态**：
   - `cph-setup-display` 置位 `device_provisioned=1` 后，在 `device_provisioned=0` 阶段已被预创建的 `UniHomeLauncher` 仍保持 OOBE 纯壁纸遮罩模式（隐藏全部桌面卡片/图标与顶栏状态栏）。

---

## 2. 根因

1. **`measured-only` 半成品档案被 `HasManifestDeviceProfile` 误判为已完成 + 控制机无解包树时丢失远端实测字段**：
   - `devices/manifest/devices/honor/HNLNA-M/device-profile.json` 存的是远端在 `CDH_PREPARE_MEASURED_ONLY=1` 下生成的占位文件（`sessionId: "measured-only"`，`available-frequencies: []`，`aiSpec` 全空），但 `HasManifestDeviceProfile` 仅检查文件存在性即跳过完整采集；
   - 当在无本地解包目录（`vendorDir == ""`）的控制机上触发 `ApplyProfile` 升级档案时，`extractFirmwareMeasuredFacts` 返回空结构，把远端已实测入库的 20 个温区、VINTF 占位、官方库清单与 `odm/etc/permissions` 实测事实全部覆盖丢失。
2. **荣耀固件专属分区（`version` / `product_h`）与双层嵌套 `dload` 包未被 `02_prepare` 提取**：
   - 荣耀 MagicOS 通过内核 cmdline `devmodel=<型号> vendorcountry=<区域>` 驱动 `libhwcust_init` 挂载 `/product_h/hn_oem/<型号>` 与 `/version/special_cust/<型号>/<区域>`，缺失这两个参数时 `ro.product.model` 回退为 `unknown`；且 `isSafeAndroidbootKey` 此前过滤了非 `androidboot.*` 前缀的 `devmodel` 与 `vendorcountry`；
   - 真机屏幕规格（`msc.config.new_screen_aspect=2600:2522:1200` -> `1200×2600`、`ro.sf.lcd_density=520`）、营销名（`ro.config.marketing_name=荣耀X80i`）与明文 `power_profile.xml` 的 23+18 档 CPU OPP 频点均位于 `product_h` 分区；
   - 基带镜像 `image/md1img.img`（`MOLY.NR17.R2.TC36.PR1.SP.V1.0.3.P107_2026/04/15`）位于外层固件包内嵌的 `File/ROM/dload/update_sd_base.zip` 中，单层 zip 扫描无法触达。
3. **MagicOS「关于手机 -> 电池」直读内核 `power_supply` sysfs 且受 AOSP `neverallow` 严格约束**：
   - MagicOS `Settings`（`u:r:system_app:s0`，属 `coredomain`）不读 `PowerProfile` 或 Health HAL，而是直读 `/sys/class/power_supply/battery/charge_full_design`（µAh 除以 1000 拼 `mAh`）；
   - `plat_sepolicy.cil` 对 `coredomain` 读普通 `sysfs` 与 `sysfs_batteryinfo` 均设有 `neverallow`，唯独公有类型 `sysfs_power` 无 `neverallow` 且被荣耀 `system_ext_sepolicy.cil` 原生放行。
4. **`cdh_device_info.ko` `nfc_sysfs.c` 无条件暴露 `/dev/pn544` 诱发无 NFC HAL 机型 `com.android.nfc` 死循环崩溃**：
   - 荣耀 MagicOS `NfcNci`（`PERSISTENT` 系统应用）在 `NfcApplication#onCreate` 中通过 `new File("/dev/pn544").exists()` 判断是否存在物理 NFC 驱动；
   - `nfc_sysfs.c` 此前无条件创建 `/dev/pn544` 与 `/sys/nfc/nfc_chip_type`，导致未装配 `android.hardware.nfc-service` 的机型启动 `NfcService` 失败后以约 8 次/秒死循环崩溃重启，90.8s 触发 `RescueParty`。
5. **预创建的 `UniHomeLauncher` 缓存首启 OOBE 壁纸遮罩状态**：
   - 首次冷启动早期 `device_provisioned=0` 时 `UniHomeLauncher` 已启动并隐藏桌面图标与状态栏；仅重发 `CATEGORY_HOME` Intent 不会清除其进程内 OOBE 遮罩状态。

---

## 3. 证据

- **修复前失败现场**（详见 [log-fail.txt](log-fail.txt) 与 [about-fail.png](about-fail.png)）：
  - `device-profile.json` 为 `sessionId="measured-only"`、`rawReply="CDH_PREPARE_MEASURED_ONLY=1"`；
  - 「关于手机」显示 `电池：未知`、`型号：unknown`、无基带版本行，实例运行分辨率 `1080×2400`；
  - 无门控加载新版 `cdh_device_info.ko` 时，`logcat` 连续刷出 `com.android.nfc` 重启，90.8s 触发 `handle boot error: sys.powerctl=reboot,RescueParty from system_server`（`Finished rescue level WARM_REBOOT for package com.android.nfc`）。
- **修复后通过现场**（详见 [log-pass.txt](log-pass.txt)、零触碰桌面图 [home-pass.png](home-pass.png) 及关于手机上下屏 [about-top-pass.png](about-top-pass.png) / [about-bottom-pass.png](about-bottom-pass.png)）：
  - `dmesg` 实测：`libhwcust_init: vendor_country=all/cn`、`get product name from oeminfo: LNA-AN10`；`cdh_device_info_cpufreq: created 2 cpufreq policies`（23+18 档）；`cdh_device_info_thermal: registered 20/20 thermal zones`；`cdh_device_info_nfc: nfc HAL service absent, skip virtual nfc nodes`（`/dev/pn544` 不存在）；`cdh_device_info_bat: registered /sys/class/power_supply/battery (design_uah=7000000)`（标签 `u:object_r:sysfs_power:s0`，零 `avc: denied`）；
  - 擦除 overlay 纯净首次冷启动零干预直接呈现完整桌面卡片/图标与顶栏状态栏（[home-pass.png](home-pass.png)），持续运行 >135.8s `am_crash` 为空、零重启；
  - 「关于手机」完整呈现：`设备名称：荣耀X80i`、`处理器：天玑6500 Elite`、`运行内存：8 GB (+16 GB 智慧运存)`、`电池：7000 mAh`、`荣耀绿洲护眼屏：2600 x 1200`（原生 `1200×2600`）、`型号：LNA-AN00`、`版本号：10.0.0.126(C00E122R102P1) GPU Turbo`、`基带版本：MOLY.NR17.R2.TC36.PR1.SP.V1.0.3.P107`。

---

## 4. 改动

1. **`02_prepare` 荣耀固件实测增强、嵌套 `dload` 基带提取与跨节点档案无损合并**：
   - 新增 `agent/modules/pipeline/stages/02_prepare/honor_measured.go`：从 `unpack_version` / 外层 zip `dload/<model>_<vendor>_<country>/` 提取 `devmodel` 与 `vendorcountry` 注入 `BootCmdline`；从 `unpack_product_h` 提取原生分辨率、逻辑密度、营销名及明文 `power_profile.xml` CPU 频点；
   - 更新 `agent/modules/pipeline/stages/02_prepare/baseband.go`：支持双层嵌套 `*base*.zip`（`SectionReader` 或 `bsdtar -xOf` 流式零落盘）提取 `md1img.img` 基带并剥离 `_YYYY/MM/DD` 斜杠日期尾巴；
   - 更新 `agent/modules/pipeline/stages/02_prepare/{apply_profile.go,ai_profile.go,firmware_measured_extra.go,apply_profile_configs.go}`：拦截非 `measured-only` 模式下的半成品占位档案；新增 `mergeRemoteFirmwareMeasuredIntoLocal` 与 `sanitizeMeasuredVendorOfficialFiles`，在控制机无本地解包树时完整继承远端 `FirmwareMeasured` 并剔除历史残留 `official libOpenCL.so`；`battery_config.xml` 兼容 mAh/µAh 双量纲；`isSafeAndroidbootKey` 放行 `devmodel`/`vendorcountry`；
   - 新增同目录单元测试 `agent/modules/pipeline/stages/02_prepare/honor_prepare_test.go`，重编 `out/agent/dist/pipeline`。
2. **`cdh_device_info.ko` 虚拟电池节点、`nfc_sysfs` 存在性门控、SELinux `sysfs_power` 策略与首启桌面刷新**：
   - 新增 `devices/modules/device_info/power/battery_sysfs.c`（接入 `Kbuild`、`cdh_device_info.h`、`core/main.c`）：解析 `battery_config.xml` 的 `full_uah` 注册只读 `/sys/class/power_supply/battery/charge_full_design`；
   - 更新 `devices/modules/device_info/nfc/nfc_sysfs.c`：新增 `cdh_nfc_hal_present()`（通过 GKI `kernel_read_file_from_path` 检测 `android.hardware.nfc-service.rc`），未装配 NFC HAL 时跳过注册 `/dev/pn544`、`/dev/nq-nci` 与 `/sys/nfc/*`；重编发布 `out/devices/kernel/5.15/modules/cdh_device_info.ko`；
   - 新增 `devices/hardware/android.hardware.health/sepolicy/vendor_sepolicy.cil`：将 `/devices/virtual/power_supply/battery` 与 `/class/power_supply/battery` 标为 `sysfs_power` 并授予 `system_app` 只读权限；
   - 更新 `devices/hardware/android.service.daemon/vendor/bin/cph-setup-display`：首启固定 `unified_device_name` 为 `/product_h/etc/prop/local.prop` 基础营销名，并在置位向导完成标志与 `dismiss-keyguard` 后重启一次 `com.hihonor.android.launcher` 退出 OOBE 壁纸遮罩态。
3. **`honor/HNLNA-M` 清单入库与本地 `dist` 盘集无损同步**（严守 `AGENTS.md §8/§9`）：
   - 校准并执行 `pipeline device apply-profile HNLNA-M --overwrite`，更新 `devices/manifest/devices/honor/HNLNA-M.{json,/device-profile.json,/device-profile.md}` 与 `configs/honor/HNLNA-M/HNLNA-M-android13/**`；
   - 原位更新本地 `out/devices/dist/honor/HNLNA-M/16-TP1A.220624.014-20260422/{device.json,patch.img,vda.img}`（仅经 `mcopy` 更新 `vda.img` EFI `refind.conf` 主 `options`，未触碰 `super`）。

---

## 5. 验证

1. **自动化单测与编译门禁**：
   - `cd agent/modules/pipeline && go test ./stages/02_prepare/... ./stages/09_build_vendor/...` 全部通过；
   - `sh -n devices/hardware/android.service.daemon/vendor/bin/cph-setup-display` 通过；
   - `pipeline device build HNLNA-M --only build_modules` 零告警完成，`cdh_device_info.ko` 86 个符号版本全部一致，`patch.img` `e2fsck -fn` 零错。
2. **`Honor X80i 5G #1` 纯净首次冷启动与「关于手机」端到端验收**：
   - 删除 `writable-base.qcow2` 与 `writable-data.qcow2` 后冷启动，零人工干预自动跳过引导并渲染完整桌面图标首屏（[home-pass.png](home-pass.png)），持续运行 >135s 零崩溃、零 `RescueParty`；
   - 「关于手机」全项对齐（`1200×2600`、`荣耀X80i`、`天玑6500 Elite`、`7000 mAh`、`LNA-AN00`、`10.0.0.126(C00E122R102P1) GPU Turbo`、`MOLY.NR17.R2.TC36.PR1.SP.V1.0.3.P107`）。

---

## 6. 遗留

- `devices/modules/device_info/` 的虚拟电池节点与 NFC HAL 存在性门控源码已统一兼容 `5.10~6.18` 全系 GKI 内核；本次已重编 `5.15` 模块并刷新 `HNLNA-M` 本地盘集，其余内核线（`6.1`/`6.6`/`6.12`）在下次执行 `build_modules` 时将自动生效。
