# #264 Android 16+ 同版本 SELinux `202504+` 类型合并/未声明权限裁剪与必选 `android.hardware.power` HAL 保底（含 `honor/chora` 全链路收口）

- 编号：`#264`
- 分组：`common`（引擎级 `02_prepare` / `09_build_vendor` 跨机型修复，兼记 `honor/chora` 首台实证收口）
- 机型：通用 Android 16/17 机型（实证首发 `honor/chora`，Honor 400 Lite `BSN-NX1`，MagicOS 10 / `16-BQ2A.250913.001-BP2A.250605.031.A3-20260804`，`lineage-23.2` + `kernel 6.12`；同步防护 `redmagic/NX809J`、`oppo/PME110` 等 A16/A17 机型）
- 状态：**已闭环**（2026-11-19）

## 1. 问题

在制作并冷启动 `#14 honor/chora`（Android 16，`202504` SELinux 基线）时，依次暴露出 5 类引擎/配置问题：

1. **Android 16+（`202504+`）同版本 SELinux 类型导出同步受阻**：`09_build_vendor` 仅在 `parseSepolicyMajor(officialVers) <= 202404` 时同版本合并官方 `plat_pub_versioned.cil`、公共 vendor 属性与自定义 vendor 服务，导致 Android 16 厂商同版本 `system` / `system_ext` / `product` 引用的自定义平台类型或公共 vendor 属性无法同步。
2. **早期 A16 官方系统镜像缺少 QPR2 `lineage-23.2` 底包新增权限导致 `secilc` 启动崩盘**：第 1 次冷启动 300s 超时，`qemu.log` 报 `init: /system/bin/secilc: Failed to resolve permission nlmsg`（于 `vendor_sepolicy.cil:2423` `ot_rcp.te:21`）与未声明 `system:firmware_load`，退出码 254 并触发 `InitFatalReboot`。
3. **Android 16 缺失 `android.hardware.power` HAL 引发 `system_server` `HintManagerService` 空指针循环崩溃**：第 2 次冷启动通过 `secilc` 后，`system_server` 在第 35s 循环抛出 `Failed to create service com.android.server.power.hint.HintManagerService` -> `NullPointerException: Attempt to read from field '...SupportInfo.headroom' on a null object reference`（排重检查全仓 36 份 `hardware/config.json` 发现 `honor/chora`、`redmagic/NX809J`、`oppo/PME110` 3 个 A16+ 配置均漏配该 HAL）。
4. **荣耀 Android 16 传感器 `type 36` `Tag::data` 与 `vintf-drop` 非法项**：荣耀 `libsensorservice.so` 对 `type 5, 8, 36` 均要求 `EventPayload::Tag::data`；AI 采风偶发将 fqname 写入 `vintfDrop` 被 `02_prepare` 直接透传至 `vendor/config.json`，触发 `09_build_vendor` 报错退出。
5. **`honor/chora` 嵌套固件三包与原生参数收口**：官方 `Huawei-Honor-400-Lite-BSN-NX1.zip` 嵌套 `update_sd_base.zip` + `cust` + `preload` 三包（12 个逻辑分区），且原生屏幕为 `1280x2788 @ 560dpi`（非预估的 `1080x2412 @ 440`），需配合 `BSN-N31` `def/meafnaf` cmdline、`persist.sys.chr.chipsettype=1` 与 `hwfactoryinterface-service`。

## 2. 根因

1. [sepolicy_cil.go](../../../../agent/modules/pipeline/stages/09_build_vendor/sepolicy_cil.go) `alignSepolicyVersion` 硬编码 `parseSepolicyMajor(officialVers) <= 202404` 门限，未涵盖 Android 16（`202504`）及 Android 17+。
2. `lineage-23.2` 底包 `vendor_sepolicy.cil` 基于较新 A16 QPR2 编译（含 `netlink_route_socket:nlmsg` 与 `system:firmware_load`），而早期 Android 16 BP2A 厂商官方 `plat_sepolicy.cil` 的 `(class ...)` / `(common ...)` 尚未声明上述权限；二阶段 `init` 现场编策略时 `secilc` 必报未定义权限错误。
3. Android 16 `HintManagerService.<init>` 直接读取 `IPower.getSupportInfo().headroom`（`SupportInfo` 为空即抛 NPE）；`lineage-23.2` 底包默认无可用 power HAL，需显式启用本仓自研 `android.hardware.power@1.0`。而 `02_prepare` `alignHardwareHALList` 此前只兜底图形 HAL，未为 A16+ 幂等补齐 `android.hardware.power`。注：`PowerStatsService` 缺 `power.stats` 仅记 E 日志不崩溃，且部分 OPPO/Realme 机型 `odm` 自带 `IPowerStats` 片段，因此**不可**全局强加 `android.hardware.power.stats`。
4. [vendor_etc_templates.go](../../../../agent/modules/pipeline/stages/02_prepare/vendor_etc_templates.go) 此前仅对 `honor` 的 `type 5, 8` 写 `payload="data"`，漏掉 `type 36`；[apply_profile_configs.go](../../../../agent/modules/pipeline/stages/02_prepare/apply_profile_configs.go) 未校验 `vintfDrop` 为 `.xml` 纯文件名。

## 3. 证据

- 失败现场摘录：[log-fail.txt](log-fail.txt)（含第 1 次 `secilc: Failed to resolve permission nlmsg` / `InitFatalReboot` 及第 2 次 `HintManagerService.<init>` `SupportInfo.headroom` NPE）
- 通过验证日志：[log-pass.txt](log-pass.txt)（含 `02_prepare` / `09_build_vendor` Go 单测、`vendor-log` `202504` 合并 21+24+175 项与裁剪 2 项 4 条规则日志、第 3 次冷启动 PASS 及桌面实测数据）
- 审阅截图：[pass-screen.png](pass-screen.png)（荣耀 `UniHomeLauncher` `1280x2788 @ 560dpi` 完整渲染）

## 4. 改动

1. **`202504+` 同版本 SELinux 合并与未声明权限裁剪**（[sepolicy_cil.go](../../../../agent/modules/pipeline/stages/09_build_vendor/sepolicy_cil.go)、[sepolicy_test.go](../../../../agent/modules/pipeline/stages/09_build_vendor/sepolicy_test.go)）：
   - 将 `alignSepolicyVersion` 同版本合并条件改为 `if officialVers == baseVers`（覆盖 `202504+`）；
   - 新增 `pruneUnsupportedPlatformPermsCIL` 并在 `reconcilePlatformGenfscon` 末尾调用：解析官方 `plat_sepolicy.cil` 的 `(common / class / classcommon)`，自动裁剪 `vendor_sepolicy.cil` 引用但官方平台未声明的权限及 `allowx` / `permissionx` 语句；
   - 新增单测 `TestAlignSepolicyVersionSameVersion202504MergesOfficialPub` 与 `TestPruneUnsupportedPlatformPermsCIL`。
2. **Android 16+ 必选 `android.hardware.power@1.0` 保底与 `vintf-drop`/传感器扩展**（[apply_profile.go](../../../../agent/modules/pipeline/stages/02_prepare/apply_profile.go)、[apply_profile_configs.go](../../../../agent/modules/pipeline/stages/02_prepare/apply_profile_configs.go)、[vendor_etc_templates.go](../../../../agent/modules/pipeline/stages/02_prepare/vendor_etc_templates.go) 及同目录单测）：
   - `alignHardwareHALList` 对 `major == "16" || major == "17"` 幂等补齐 `android.hardware.power@1.0`（不强加 `power.stats`）；
   - 新增 `isValidVintfDropFileName` 对存量与 AI 新增 `vintf-drop` 过滤非法非 `.xml` / 含路径项；
   - `sensorPayloadAttr` 的 `"honor"` 分支扩展至 `case 5, 8, 36:`。
3. **`honor/chora` 配置收口**（[chora.json](../../../../devices/manifest/devices/honor/chora.json)、[chora-android16](../../../../devices/manifest/configs/honor/chora/chora-android16)）：
   - 按 `product_h_a.img:/etc/prop/local.prop` 真值锁定 `1280x2788 @ 560dpi`；补齐 12 个逻辑分区与 `cust.img`，同步写入 `vendorcountry=def/meafnaf devmodel=BSN-N31` 等 6 项 cmdline；
   - 补齐 `android.hardware.power@1.0`、`persist.sys.chr.chipsettype=1`、`hwfactoryinterface-service` 6 件套、`frp_symlink.rc`、`ueventd.rc`、屏蔽 `/vendor/app`（总 redirect `14 <= 32`）及荣耀 SELinux 规则。

## 5. 验证

1. Go 单测与构建：
   - `TMPDIR=/Volumes/work/cloud-phone-hub/out/agent/tmp go test -count=1 ./modules/pipeline/stages/02_prepare/... ./modules/pipeline/stages/09_build_vendor/...` 全部通过；
   - 重编 `out/agent/dist/cdh-pipeline`。
2. `honor/chora` 重打构建与冷启动验收：
   - `09_build_vendor` 日志实证：`替换官方 plat_pub_versioned.cil 并补齐跨代兼容 typeattribute 21 项`、`同步官方公共/系统可见 vendor 属性类型 24 项`、`同步官方自定义 vendor 服务类型 175 项`、`裁剪平台未声明 SELinux 权限 2 项（移除规则 4 条）`，装配写入 `android.hardware.power@default`；
   - 第 3 次独占 CPU 冷启动验收：一次穿透 `secilc` 与 `system_server`，`bootCompleted=true`、`bootanim=stopped`、`serviceCount=406`、`surfaceFlingerState=running`、`bootReason=reboot,factory_reset`、`topActivity=com.hihonor.android.launcher/.unihome.UniHomeLauncher`；
   - VNC 抓帧审阅 `pass-screen.png`（`1280x2788`，`meanBrightness=183.78`，`6.29 MB`）：荣耀 MagicOS 10 官方紫色主题桌面、时钟、AI Suggestions、Gallery、Play Store 及底座 Dock 完整清晰渲染，运行超 270s 零 `RescueParty`。另为 `verify_boot.py` 增补了首启小体积纯白过渡帧（`< 50KB && brightness > 245`）延迟 `8s` 二次复抓防护。

## 6. 遗留

- 存量配置的 `redmagic/NX809J` 与 `oppo/PME110` 需运行一次 `02_prepare`（或手动同步）触发 `android.hardware.power@1.0` 保底；
- `vendor.honor.hardware.hwfactoryinterface-service` 与 `honor/sun`（`#259`）一致，因未引入真机闭源 `liboeminfo.so` 仅在 `dlopen` 时打提示并安全返回，不影响起机及核心服务。
