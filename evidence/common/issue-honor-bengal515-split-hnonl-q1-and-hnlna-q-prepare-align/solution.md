# #344 拆分混挂 `honor/bengal_515` 为 `HNONL-Q1`（HONOR Pad X7b）与 `HNLNA-Q`（HONOR X8d）、补齐 `02_prepare` 荣耀公版拆机与 `light_config.xml` `BATTERY` 逻辑灯保底

- **编号**：#344
- **设备 / 分组**：`common`（跨机型管线 `02_prepare` 荣耀公版 `vendor` 身份按 `product_h` `ro.product.device` 自动拆分 + `light_config.xml` 强制保底 `BACKLIGHT/BATTERY/NOTIFICATIONS/ATTENTION` 逻辑灯；实机闭环 **`HONOR Pad X7b #1`** `honor/HNONL-Q1` `16-TKQ1.240227.001-20260922` 与 **`HONOR X8d #1`** `honor/HNLNA-Q` `16-TKQ1.240227.001-20260708`）
- **日期**：2026-11-26
- **状态**：**已闭环**（双机型独立盘集纯净冷启均一次达成 `boot_completed=1`、零崩溃、`am_crash_count=0`、「关于平板 / 关于手机」全项真机对齐 + `02_prepare` / `09_build_vendor` 单测全绿）

---

## 1. 问题

排查 `Honor Pad X7b #1` 是否与 `02_prepare` 对齐时，发现两类叠加问题，并在拆分后实机验证中暴露一例 `02_prepare` 生成配置的致命回归：

1. **两款不同物理形态机型混挂在同一个 `honor/bengal_515` 下**：
   - `16-TKQ1.240227.001-20260708`（原 `Honor Pad X7b #1` 正在运行的版本，源包 `Huawei-P60-LNA-L32.zip`）实际为手机 **HONOR X8d**（`ro.product.device=HNLNA-Q`、`devmodel=LNA-L32`、`LNA-LX2`、`Snapdragon 6s 4G Gen 2`、`1080×2392 @ 480dpi`、`7000 mAh`、双卡 `dsds`）；
   - `16-TKQ1.240227.001-20260922`（源包 `Huawei-Honor-Pad-X7b-ONL-W09.zip`）才是平板 **HONOR Pad X7b**（`ro.product.device=HNONL-Q1`、`devmodel=ONL-L09`、`Snapdragon 685 4G Mobile Platform`、`1200×1920 @ 280dpi`、`8000 mAh`、单卡 LTE `ss`）。
2. **`bengal_515` 清单与运行盘集完全未与最新 `02_prepare`（含 #341）对齐**：
   - `device-profile.json` 仍为 `sessionId="measured-only"` / `rawReply="CDH_PREPARE_MEASURED_ONLY=1"` 半成品占位；
   - 缺少 `devmodel` / `vendorcountry` cmdline，导致 `libhwcust_init` 报 `can not get OEMINFO_TMP_VENDORCOUNTRY`、`ro.build.fingerprint` 出现 `HONOR//HNLNA-Q:...preload format error.`、`ro.config.marketing_name` 停留在板级缩写 `LNAL`；
   - 分辨率/DPI 停留在默认 `1080×2400 @ 440dpi`，无 `cdh_device_info.cpufreq`、无虚拟电池 `/sys/class/power_supply/battery/charge_full_design`、无基带版本，`media_config.xml` 残留 MTK 模板。
3. **拆分后 `HNONL-Q1` 首次冷启 `system_server` 陷入 BootPhase 550 NPE 崩溃循环**：
   - AI 采集为 `HNONL-Q1` 生成的 `vendor/etc/light_config.xml` 仅含 `BACKLIGHT / NOTIFICATIONS / ATTENTION`，缺失 `BATTERY` 逻辑灯，导致 MagicOS `HwBatteryService$HwLed.setLightColorImpl()` 对 `null` 调 `LogicalLight.setColor()` 连续 NPE。
4. **`HNLNA-Q`（海外 GMS 版 HONOR X8d）首次冷启进入纯壁纸桌面、无任何卡片与应用图标**：
   - `UniHomeLauncher` View 树内已加载全部小组件与图标，但 `cph-setup-display` 在 `[65.8s]` 刚进入 `boot_completed=1` 时过早重启 Launcher，彼时海外 GMS 向导 `com.google.android.setupwizard` / `com.aura.oobe.honor` 尚未禁用，且后半段电话进程重启（MCC/MNC 配置变更）、共享存储主题包挂载及末尾 `dismiss-keyguard` 均未完成，导致 `DragLayer` 的 `MultiValueAlpha` 被锁在 `0.0` 全透明遮罩态。

---

## 2. 根因

1. **`02_prepare` 仅读 `vendor/odm` `build.prop` 推导设备标识，未处理荣耀高通公版 `bengal_515` 碰撞**：
   - 荣耀高通中端平台多个不同形态机型共用公版 `vendor`（`ro.product.vendor.device=bengal_515`），且 `odm/etc/build.prop` 只写 `ro.product.odm.brand=HONOR` 不写 `ro.product.odm.device`，导致 `ResolvePrepareIdentity` 退回公版代号 `bengal_515`；真机唯一设备代号实际写在 `product_h` 的 `/etc/prop/local.prop`（`HNLNA-Q`）或 `/region_comm/oversea/prop/local.prop`（`HNONL-Q1`）。
2. **海外版荣耀 `region_comm/oversea/prop/local.prop` 的完整 SoC 商用名未覆盖短编号**：
   - `/product_h/etc/prop/local.prop` 仅写 `SM6225` / `SM6225Pro`，完整商用名 `Snapdragon 685 4G Mobile Platform` / `Snapdragon 6s 4G Gen 2` 位于 `region_comm/oversea/prop/local.prop`。
3. **`buildVendorEtcFileTreeCandidates` 直接透传 AI 的 `LightEntries` 缺少必选逻辑灯保底**：
   - `apply_profile_configs.go` 在 `len(LightEntries) > 0` 时直接原样写出 `etc/light_config.xml`，当大模型省略 `BATTERY` 逻辑灯时直接触发 MagicOS `HwBatteryService$HwLed` 空指针崩溃。

---

## 3. 证据

- **修复前失败现场**（见 [log-fail.txt](log-fail.txt)）：
  - 原 `Honor Pad X7b #1`（`bengal_515_16-TKQ1.240227.001-20260708`）`adb` 实测 `ro.product.device=HNLNA-Q`、`ro.config.marketing_name=LNAL`、`ro.build.fingerprint=HONOR//HNLNA-Q:16/...preload format error.:user/release-keys`、`wm size=1080x2400 @ 440dpi`、无 `charge_full_design`、无 `cdh_device_info.cpufreq`；
  - `HNONL-Q1` 在缺失 `BATTERY` 逻辑灯时 `logcat -b crash` 连续报 `Failed to boot service com.android.server.HwBatteryService: onBootPhase threw an exception during phase 550 ... LogicalLight.setColor(int) on a null object reference`。
- **修复后通过现场**（见 [log-pass.txt](log-pass.txt) 及 4 张实机截屏）：
  - **`HONOR X8d #1`**（`honor/HNLNA-Q`，`127.0.0.1:5911`，[HNLNA-Q-home-pass.png](HNLNA-Q-home-pass.png) / [HNLNA-Q-about-pass.png](HNLNA-Q-about-pass.png)）：`boot_completed=1`、连续运行 `>415s`、`am_crash_count=0`、`wm size=1080x2392 @ 480dpi`、`ro.product.model=LNA-LX2`、`ro.config.marketing_name=HONOR X8d`、`ro.build.display.id=LNA-L32 10.0.0.179(C185E9R201P2)`、`ro.soc.model=Snapdragon 6s 4G Gen 2`、`battery_design_uah=7000000`（关于手机显示 `7000 mAh`、`2392 x 1080`）、`policy0(18档)+policy4(21档)`、`thermal_zones=7`；
  - **`HONOR Pad X7b #1`**（`honor/HNONL-Q1`，`127.0.0.1:5919`，[HNONL-Q1-home-pass.png](HNONL-Q1-home-pass.png) / [HNONL-Q1-about-pass.png](HNONL-Q1-about-pass.png)）：`boot_completed=1`、`system_server_pid=844`、`am_crash_count=0`、`wm size=1200x1920 @ 280dpi`（横屏平板渲染 `1920 x 1200`）、`ro.product.model=ONL-L09`、`ro.config.marketing_name=HONOR Pad X7b`、`ro.build.display.id=ONL-L09 10.0.0.129(C185E1R102P1)`、`ro.soc.model=Snapdragon 685 4G Mobile Platform`、`persist.radio.multisim.config=ss`、`battery_design_uah=8000000`（关于平板显示 `8000 mAh`、`1920 x 1200`）、`policy0(15档)+policy4(19档)`、`thermal_zones=9`。

---

## 4. 改动

1. **`02_prepare` 荣耀公版 `vendor` 自动拆机与 `light_config.xml` 必选逻辑灯保底**：
   - 更新 `agent/modules/pipeline/stages/02_prepare/honor_measured.go`：新增 `honorGenericVendorDeviceRe`（覆盖 `bengal*`/`qssi*`/`k*`/`vext_*`/`mt*`/`vsoc_*`，不触碰既有 `sun`）与 `resolveHonorProductHDeviceCode()` 从 `product_h` 读取真机 `ro.product.device`；并在 `enrichHonorFirmwareMeasuredFacts()` 中补充从 `region_comm/{china,oversea}/prop/local.prop` 升级完整 SoC 商用名与默认分辨率；
   - 更新 `agent/modules/pipeline/stages/02_prepare/prop.go`：`loadPrepareProps()` 在 Honor 固件缺失 `ro.product.odm.device` 且 `ro.product.vendor.device` 命中公版板级占位时自动以 `product_h` 真机代号回填 `ro.product.odm.device`；
   - 更新 `agent/modules/pipeline/stages/02_prepare/apply_profile_configs.go`：`buildVendorEtcFileTreeCandidates()` 生成 `etc/light_config.xml` 时强制保底包含 `BACKLIGHT` / `BATTERY` / `NOTIFICATIONS` / `ATTENTION` 四个核心逻辑灯并按 ID 去重排序；
   - 同步 `agent/modules/pipeline/stages/02_prepare/prepare_catalog_default.json` 与 `devices/manifest/prepare_catalog.json`（新增 `hnonl-q1` 与 `hnlna-q` 映射）；
   - 新增单元测试 `TestHonorGenericBengalVendorSplitsByProductHDeviceCode` 与 `TestLightConfigAlwaysIncludesBatteryLogicalLight`（`agent/modules/pipeline/stages/02_prepare/honor_prepare_test.go`），重编 `out/agent/dist/pipeline`。
2. **拆分清单入库与 `apply-profile --recollect --overwrite` 全量对齐**：
   - 将 `devices/manifest/{devices,configs,versions}/honor/bengal_515*` 拆分为 `honor/HNONL-Q1`（`id=12`, `configId=79`, `versionId=17`, `16-TKQ1.240227.001-20260922`）与 `honor/HNLNA-Q`（`id=47`, `configId=81`, `versionId=16`, `16-TKQ1.240227.001-20260708`）；
   - 执行两台机型完整 `pipeline device apply-profile <dev> --recollect --overwrite`，校准两份 `battery_config.xml`（`8000000` / `7000000` µAh）、`HNONL-Q1` `persist.radio.multisim.config=ss`、Widevine `system_id=27176` 与 `light_config.xml`。
3. **双机型本地盘集重编刷新、海外荣耀首启桌面刷新时序闭环与旧残留清理**（严守 `AGENTS.md §8`）：
   - 更新 `devices/hardware/android.service.daemon/vendor/bin/cph-setup-display`：首启禁用海外版并存的 `com.google.android.setupwizard` 与 `com.aura.oobe.honor`，并将 `UniHomeLauncher` 重启刷新收口至脚本末尾电话进程重启、存储挂载及最终 `dismiss-keyguard` 落定之后；
   - 保留原厂 `vda.img` 内只读 `super` 分区零改动，仅经 `mcopy` 向 EFI `refind.conf` 主 `options` 分别注入 `devmodel=ONL-L09 vendorcountry=def/meafnaf` 与 `devmodel=LNA-L32 vendorcountry=def/meafnaf`；
   - 完整重跑 `07_build_odm -> 08_build_hardware -> 09_build_vendor -> 10_build_vendor_dlkm -> 11_build_patch` 生成两份新版 `patch.img` 与 `device.json`，清理旧 `bengal_515` 盘集、构建目录与运行态实例。

---

## 5. 验证

1. **单元测试**：`cd agent/modules/pipeline && go test ./stages/02_prepare/... ./stages/09_build_vendor/...` 全部通过（`1.163s`）。
2. **双机型纯净冷启动实机验收**：
   - `HONOR X8d #1`（`127.0.0.1:5911`）冷启进桌面并持续运行 `>415s`，`boot_completed=1`、`am_crash_count=0`、「关于手机」全项实机截屏核验通过；
   - `HONOR Pad X7b #1`（`127.0.0.1:5919`）冷启进横屏平板桌面，`boot_completed=1`、`system_server_pid=844`、`am_crash_count=0`、「关于平板」全项实机截屏核验通过。

---

## 6. 遗留

- 本机 `devices/manifest/` 已落盘更新，若后续需在远端 ROM 服务器（`rom-main`）上重打两台机型镜像，请先通过集群清单单向同步（`SyncLocalManifestToNode`，见 `AGENTS.md §9`）覆盖远端旧 `bengal_515` 清单。
