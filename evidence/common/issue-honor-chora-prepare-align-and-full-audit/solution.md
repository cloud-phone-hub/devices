# #343 — Honor 400 Lite (`honor/chora`) `02_prepare` 硬件全量对齐、EROFS 基带解析、6.12 5GHz Wi-Fi 补齐与五大子系统闭环验收

- **设备 / 机型**：Honor 400 Lite（`honor/chora`，海外版真机型号 `BSN-NX1` / `devmodel=BSN-N31`，MagicOS 10 / Android 16 初版 `BP2A.250605.031.A3`，`lineage-23.2` + `kernel 6.12`）
- **构建规格**：`chora_16-BQ2A.250913.001-BP2A.250605.031.A3-20260804`（实例 `#1`，槽位 `1`，ADB `127.0.0.1:5919`）
- **日期**：2026-11-26
- **状态**：**已闭环**

---

## 一、问题

对运行中的 `Honor 400 Lite #1` 执行 `02_prepare` / `apply-profile` 硬件规格对齐及 Camera、Radio、Wi-Fi、NFC 与系统静息健康度全项检查时，暴露出以下问题链：

1. **硬件画像与「关于手机」多处失真（见 `log-fail.txt` 与 `about-before-1.png` / `about-before-2.png`）**：
   - `ro.soc.model` 被旧模版误写为 `SM6375-AC`，而非真机 `SM6850`（`Snapdragon 6 Gen 5`）；
   - 「关于手机」中 `Device name` 显示内部工程缩写 `BSN` 而非市场名 `Honor 400 Lite`；
   - `Silicon-Carbon Battery` 显示 `Unknown`（`/sys/class/power_supply` 为空目录，缺 `11000 mAh` 设计容量节点）；
   - `Baseband version` 为空（`gsm.version.baseband=""`）；
   - CPU 簇拓扑未按真机 `Chora SoC` DTB 对齐为 `4+4`（28 档频点）。
2. **外设与后台静息空转**：
   - 未启用 `biometrics@fingerprint-only` HAL，`FingerprintServiceEx` / `HidlToAidlSensorAdapter` 高频重试导致桌面静息态 `system_server` 长期占用 `46.6%` CPU（整机 8 核闲置仅 `643%/800%`）；
   - 旧运行镜像未同步 `#334`/`#335`/`#340` 的相机 `CDCM_PAUSE` 休眠、`opkey0`/APN 双 PDN（`ccmni0`/`ccmni1` 均 `DOWN`、APN 查询为空）、`/proc/{vip,fas}/ioctl` 及 `/dev/video0..1` `0660 media:camera` 权限。
3. **增量重打 `patch.img` 过程中的三处环境与策略卡点**：
   - 首次单阶段重打 `09_build_vendor` 时，因本机 `out/devices/roms/honor/chora/.../super/` 未放入官方 `system_a.img` / `system_ext_a.img` / `product_a.img`，`pruneUnsupportedPlatformPermsCIL` 未能扫描平台 CIL 裁剪 `lineage-23.2`（A16 QPR2）新增的 `nlmsg` 权限，导致二阶段 `/system/bin/secilc` 报 `Failed to resolve permission nlmsg` 触发 `InitFatalReboot` 循环重启；
   - 本机未跑过 `06_build_vendor_boot` 时缺 `vendor_boot/fstab.<board>` 缓存，且手工提取 `super` 子分区时未生成含全部 12 个荣耀逻辑分区（`preas`/`product_h`/`cust`/`version`/`preload` 等）的 `super/.super-layout.json`，导致 `fstab.chora` 缺失或误裁 `/preas/apex` overlay `lowerdir`；
   - 复用旧内核构建卷 `cdh-kernel-6.12-cdh-752b1df969f5` 编译 `04_build_modules` 时，卷内 `drivers/net/wireless/virtual/mac80211_hwsim.c` 早于 `../issue-hwsim-5g-cck-rate-all-kernels/`（#301），缺少 `rates_5g` 纯 OFDM 速率集，导致 5GHz 虚拟 AP `Marriott-15F-WiFi-37`（5220 MHz）关联时报 `Network not found event received`。

---

## 二、根因

1. **高通 EROFS 格式 `NON-HLOS.bin` 基带提取盲区**：
   `Huawei-Honor-400-Lite-BSN-NX1.zip` 内嵌 `update_sd_base.zip!image/NON-HLOS.bin` 并非传统 FAT16 镜像，而是偏移 `1024` 字节处魔数为 `0xe0f5e1e2` 的 **EROFS 只读文件系统镜像**，基带版本号存放在镜像内 `/verinfo/ver_info.txt` 的 JSON 字段 `"Image_Build_IDs": {"modem": "MPSS.DE.3.1.7-00544.2-SKYROS_GEN_PACK-2.155179.122"}`。原 `baseband.go` 仅对 `NON-HLOS.bin` 做明文正则流扫，因 EROFS LZ4 块压缩无法命中明文串。
2. **荣耀海外版 `local.prop` 工程代号抢占市场名与 SoC 字段缺失**：
   海外版荣耀固件 `/product_h/etc/prop/local.prop` 中 `ro.config.marketing_name=BSN` 等于板级工程缩写 `ro.build.product=BSN`，真正的市场名在 `/product_h/region_comm/oversea/prop/local.prop` 或设备档案；同时高通荣耀机型在 `/product_h/etc/prop/local.prop` 以 `ro.config.cpu_info_display=SM6850` 声明 SoC 型号，海外版电池/CPU 曲线位于 `oversea/xml/power_profile.xml`。`cph-setup-display` 此前无条件信任非空的 `ro.config.marketing_name`，将 `Settings.Global.unified_device_name` 写成了 `BSN`。
3. **官方多 DTB 镜像中的真机拓扑定位**：
   官方 `vendor_boot.img` 内含 15 份 DTB，索引 0 为通用 `6+2` 模板，索引 13（`model = "Qualcomm Technologies, Inc. Chora SoC"`, `compatible = "qcom,chora"`）才是真机 DTB（`CPU0..3` 499.2MHz~2.016GHz 12 档 + `CPU4..7` 499.2MHz~2.6112GHz 16 档），与 `power_profile.xml` 的 `cpu.clusters.cores=[4,4]` 完全一致。
4. **条件式上下文规则求值顺序与 6.12 内核构建卷陈旧**：
   - `09_build_vendor/files_compat.go` 中 `reconcileVendorSystemCompat` 原先在 `applyVendorFallbackCopies` 与 `applyModulePlaceholderAdds` 之后才调用 `applyConditionalContextRules`，导致 HAL 变体声明的占位服务依赖新补入的 `vendor_service_contexts` 标签时顺序倒置；
   - `cdh-kernel-6.12-cdh-752b1df969f5` 卷建于 `2026-11-18`，未同步 `devices/kernels/patches/6.12/0002-hwsim-fake-wifi.patch` 的 5GHz 纯 OFDM 速率集（`rates_5g[]`）。

---

## 三、证据

- **修复前实测（`log-fail.txt`、`about-before-1.png`、`about-before-2.png`）**：
  - `ro.soc.model=SM6375-AC`、`gsm.version.baseband=""`、`persist.sys.opkey0=null`；
  - `About phone` 显示 `Device name: BSN`、`Silicon-Carbon Battery: Unknown`、`Baseband version` 空白；
  - `ccmni0` / `ccmni1` 均为 `DOWN`，`content://telephony/carriers` 返回 `No result found.`；
  - 静息 `top`：`system_server` `46.6%` CPU、`camera-service` `6.6%` CPU，`HidlToAidlSensorAdapter: Fingerprint HAL not available` 持续刷屏。
- **修复过程关键定位证据**：
  - `qemu.log` 初次重打 `patch.img` 报错：`init: /system/bin/secilc: Failed to resolve permission nlmsg`（补齐官方 `system_a`/`system_ext_a`/`product_a` 后 `09_build_vendor` 打印 `裁剪平台未声明 SELinux 权限 2 项（移除规则 4 条）`，`(nlmsg)` 计数归 `0`）；
  - 补齐 `super/.super-layout.json` 全部 12 个逻辑分区后，`06_build_vendor_boot` 合成 `fstab.chora` `5797 B`（保留 41 行，完整恢复 `/preas/apex` 等 overlay `lowerdir`）；
  - 同步 `0002-hwsim-fake-wifi.patch` 重编 `mac80211_hwsim.ko` 后，`verify-modules` 校验 `out/devices/kernel/6.12/modules/*.ko` 全部 13 个模块符号版本 `100%` 一致（`mac80211_hwsim.ko: 203 个符号版本一致`）。
- **修复后全项实测（`log-pass.txt` 及 `about-after-*.png`、`apn-after.png`、`wifi-after.png`、`camera-*-after.png`、`camera-video-mode.png`）**：见第五节。

---

## 四、改动

1. **流水线 `02_prepare` 增强**：
   - `agent/modules/pipeline/stages/02_prepare/baseband.go` & `baseband_test.go`：新增 `reVerInfoModem`、`isErofsSuperblockAt1024`、`extractBasebandFromErofsImage` 与 `extractBasebandFromErofsStream`，支持本地及嵌套 zip 流内的 EROFS 格式 `NON-HLOS.bin` / `modem.img` 提取 `/verinfo/ver_info.txt` 基带版本；
   - `agent/modules/pipeline/stages/02_prepare/honor_measured.go` & `honor_prepare_test.go`：新增 `isHonorInternalBoardMarketingName` 跳过等于 `ro.build.product` 的工程缩写（如 `BSN`），读取 `ro.config.cpu_info_display` 回填 `SoCModel`，兼容 `oversea/xml/power_profile.xml`；
   - `devices/manifest/prepare_catalog.json` & `agent/modules/pipeline/stages/02_prepare/prepare_catalog_default.json`：新增 `honor-nfc-services` 条件规则项。
2. **流水线 `09_build_vendor` 与客机守护脚本**：
   - `agent/modules/pipeline/stages/09_build_vendor/files_compat.go`：将 `applyConditionalContextRules` 前置到 `applyVendorFallbackCopies` 与 `applyModulePlaceholderAdds` 之前执行；
   - `devices/hardware/android.service.daemon/vendor/bin/cph-setup-display`：当 `ro.config.marketing_name`（或存量 `unified_device_name`）等于 `ro.build.product` 时自动回退采用 `ro.vendor.cdh.device.market_name`。
3. **`honor/chora` 清单对齐与 `6.12` 内核模块刷新**：
   - 校准 `devices/manifest/devices/honor/chora/device-profile.json` 并运行 `pipeline device apply-profile chora --overwrite`，对齐 `chora.json`、`chora-android16/{hardware,module,vendor,vendor_boot,vda}/config.json`（含 `SM6850`、`1280x2788@560dpi`、`4+4` 28 档频点、`11000 mAh`、基带 `MPSS.DE.3.1.7-00544.2-SKYROS_GEN_PACK-2.155179.122`、`biometrics@fingerprint-only`、`fingerprint_config.xml`、`odm/etc/libnfc-*-sz.conf`）；
   - 将 `devices/kernels/patches/6.12/0002-hwsim-fake-wifi.patch`（含 `rates_5g` 纯 OFDM 速率集）同步进 `cdh-kernel-6.12-cdh-752b1df969f5` 构建卷，重编发布 `out/devices/kernel/6.12/modules/mac80211_hwsim.ko` 并更新 `SHA256SUMS`；
   - 重打并校验部署 `out/devices/dist/honor/chora/16-BQ2A.250913.001-BP2A.250605.031.A3-20260804/patch.img`（`sha256: 43dc4267caf7`，`e2fsck -fn` 839 文件零错通过，零触碰官方 `super` 分区）。

---

## 五、验证

1. **自动化单元测试与模块符号校验**：
   - `go test -count=1 ./stages/02_prepare/... ./stages/09_build_vendor/...` 全部 `PASS`；
   - `pipeline verify-modules out/devices/kernel/6.12/modules/Module.symvers out/devices/kernel/6.12/modules/*.ko` 全部 13 个 `.ko` 100% `[ok]`。
2. **「关于手机」与硬件规格 100% 对齐（`about-after-1.png`、`about-after-2.png`）**：
   - `getenforce=Enforcing`、`sys.boot_completed=1`（冷启动零重启）；
   - `About phone` 实屏展示：`Device name: Honor 400 Lite`、`Processor: Snapdragon 6 Gen 5`（`ro.soc.model=SM6850`）、`RAM: 8 GB (+16 GB Honor RAM Turbo)`、`Silicon-Carbon Battery: 11000 mAh`（`charge_full_design=11000000`）、`Honor Eye Comfort Screen: 2788 x 1280`（`560 dpi`）、`Model: BSN-NX1`、`Baseband version: MPSS.DE.3.1.7-00544.2-SKYROS_GEN_PACK-2.155179.122`、`Kernel version: 6.12.58-android16-6-g50d6c2424b3a-ab15726793-4k`；
   - `cpufreq`：`policy0`（CPU `0..3`，12 档 `499.2MHz~2.016GHz`）+ `policy4`（CPU `4..7`，16 档 `499.2MHz~2.6112GHz`）全数在线。
3. **Camera（荣耀原生相机 `com.hihonor.camera`）**：
   - 后摄 `Device 0` 秒开实时预览（341+ 帧，0 丢帧，见 `camera-rear-after.png`），快门实拍生成 `IMG_20261126_145005_1.jpg`（16 KB）并渲染左下角缩略图；
   - 切换前摄 `Device 1` 实时出画（89+ 帧，见 `camera-front-after.png`）并实拍落盘；
   - 切换 `VIDEO` 模式（`1080 · 30`，见 `camera-video-mode.png`）录制生成 `VID_20261126_145600.mp4`（51 KB）；
   - 退回桌面后自动进入 `CDCM_PAUSE` 按需休眠（`framesSent: 118 -> 118` 零增长，`Device 0/1 is closed`）。
4. **Radio（蜂窝网络与双 PDN）**：
   - `gsm.sim.state=LOADED,ABSENT`、`China Unicom`（`46001`）、`persist.sys.opkey0=46001`、`mVoiceRegState=0(IN_SERVICE)`、`mDataRegState=0(IN_SERVICE)`（LTE 满格）；
   - 中国联通 3 条 APN（`3gnet`、`3gwap`、`ims`）自动写入并在设置页选中（见 `apn-after.png`）；
   - `ccmni1`（`100.64.155.15/24` + IPv6 `UP`）与 `ccmni0`（IMS IPv6）双栈就位；关闭 Wi-Fi（`svc wifi disable`）纯蜂窝移动数据下 `ping 223.5.5.5` `0.402 ms` 0% 丢包、HTTPS `200 OK`。
5. **Wi-Fi（5GHz 虚拟 AP）**：
   - 扫描发现 8 个虚拟 AP，开机自动关联 5GHz `Marriott-15F-WiFi-37`（`5220 MHz`, `192.168.20.100/24`, `Supplicant state: COMPLETED`，见 `wifi-after.png`），`ping -I wlan0 223.5.5.5` `0.552 ms` 0% 丢包、HTTPS `200 OK`。
6. **NFC（虚拟 NFCC 与刷卡交互）**：
   - `Service nfc: found`、`Nfc is enabled`、`mState=on`、`/dev/pn544` 与 `/sys/nfc/nfc_chip_type=sn220u` 就位；
   - 宿主 `/api/nfc/status` 返回 `connected=true, initialized=true, discovering=true`；调用 `/api/nfc/tap` 与 `/api/nfc/remove` 完整触发 `cardPresent: true -> false` 与客机 `NfcService` 标签检测流程，全程零崩溃。
7. **静息 CPU 与系统健康度**：
   - 静息 `top`：8 核总闲置 **`786%/800%`（整机总占用从 `19.6%` 降至 `1.75%`）**，`system_server` 从 `46.6%` 降至 `0.0%`；
   - `/proc/{vip,fas}/ioctl` 与 `/dev/video0..1`（`0660 media:camera`）就位，`HidlToAidlSensorAdapter`、`IMotion`、`FingerprintHand`、`FASProxy`、`RescueParty` 计数全部为 `0`。

---

## 六、遗留

- 无阻塞遗留。荣耀后台 `com.hihonor.dataupdateservice` 开机首次请求外网荣耀云端热更鉴权接口因无内网凭据抛出 1 次一次性 NPE 退出（非 persistent 服务，不重试、不影响任何系统功能，`RescueParty=0`）。
