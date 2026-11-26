# ss9380 恢复 OneUI 设置“移动网络 / SIM 管理器”入口、挂载国行 CSC 分区并打通 40 个三星原装内置应用首启铺设

- 设备：`samsung/pa3q`（Samsung Galaxy S25 Ultra 国行 `SM-S9380`，OneUI Android 16，基线 `16-AP3A.240905.015.A2-20260812` / `S9380ZCSDCZH1`）
- 日期：2026-11-26
- 最终盘集：`patch.img` sha256 `e7e65647de3272d0e355b74e94055924b3af376aec48cf0ddf1e7e3b25b738be`（`vendor.img` `5bb97fa82ddbb84c07771a3ecd566bea076b716e2164bbcd9619045cda355a68`，`vendor_boot.img` `c5ad6d4c61a394d35ed0b541f0e5f77cc1d9a6b943a54666dc93e0fff9d4a801`，`samsung_preload.img` `b5c19c03d64eb4e243b7b2099596250c9768d857556e71144ecd323179efdf27`；官方 `super.img`、`prism.img`、`optics.img`、`omr.img` 100% 零改动原样直通）
- 结果：清空可写层冷启动 104s 达 `sys.boot_completed=1`，`Enforcing`；`/prism`、`/optics`、`/omr`、`/oem` 四分区全部挂载就位，`scs` 进程零拒权置位 `ro.csc.sales_code=CHC`、`persist.sys.omc_etcpath=/prism/etc/carriers/CHC`、`persist.sys.omc_path=/optics/configs/carriers/CHC/conf`；设置 → 连接中 **「SIM 管理器」** 与 **「移动网络」** 入口恢复显示并可正常进入二级配置页；三星原生 PDP 流程在 `post-fs-data` 阶段 `5.55s` 完成 **40 个原装内置应用**（`2.13 GiB`）铺设，OneUI 主屏幕 `HoneySpace.DataParser` 加载国行 `/prism/etc/carriers/CHC/default_workspace_homeonly.xml`，首屏完整呈现天气/三星健康/搜索小组件及底部「应用商店、相册、主题商店、三星生活助手」4 图标，次屏呈现 Samsung/Microsoft 文件夹及全套内置应用，整机 `logcat -b crash` **0 崩溃**。

## 一、问题

在 `ss9380` 打通蜂窝满格信号与 4G 数据拨号（见 `../issue-oneui-ril-bt-keymint-moneta-wallpaper/`）后，真机交互进一步发现三项功能与布局缺失（见同目录 `log-fail.txt`、`fail-connections.png`、`fail-home.png`、`fail-home-page1.png`）：

1. **设置 → 连接页面缺失「移动网络」与「SIM 管理器」入口**：尽管状态栏蜂窝信号满格且 4G 数据可正常上网，但打开 `com.android.settings/.Settings$ConnectionsSettingsActivity` 时，「飞行模式」下方直接跳到「流量监控」，没有「移动网络」和「SIM 管理器」菜单入口。
2. **缺失三星第一方内置应用**：系统内完全找不到时钟、计算器、日历、三星笔记、录音机、三星健康、SmartThings、三星社区、游戏中心、提醒等三星原装内置应用。
3. **主屏幕首屏无应用图标（布局回退海外 GMS 残缺模板）**：即使内置应用安装就位后，若 `persist.sys.omc_etcpath` 未置位，主屏幕第 1 屏仍只有右上角单个天气小组件而无任何应用图标，40 个国行应用被无分类平铺挤到第 2~3 屏。

## 二、根因

1. **OneUI `SecSettings.apk` 不读 AOSP `gsm.sim.state`，而强校验三星私有 SIM 属性三件套**：
   反编译客机 `/system/priv-app/SecSettings/SecSettings.apk` 的 `SecMobileNetworkPreferenceController.getAvailabilityStatus()`（`0x2b0b14`）与 `SecSimFeatureProviderImpl`（`0x2b1bc8`、`0x2b1f68`）确认：
   - `ConnectionsUtils.isHideMobileNetworks()` 调用 `ConnectionsUtils.isNoSIM()` → `SecSimFeatureProviderImpl.getEnabledSimCnt()`，后者逐槽读取系统属性 **`ril.ICC_TYPE0`** 与 **`ril.ICC_TYPE1`**（缺省 `"0"` 表示无卡，`1`=SIM，`2`=USIM）。自研 `radio@samsung` 未同步写入 `ril.ICC_TYPE0=2`，导致 `getEnabledSimCnt()` 返回 `0`，`SecMobileNetworkPreferenceController` 打印 `remove mobile network by HideMobildeNetwork` 并返回 `UNSUPPORTED_ON_DEVICE (4)` 隐藏菜单；
   - `SecSimManagerPreferenceController` 依赖 `SecSimFeatureProviderImpl.isMultiSimModel()` 检查 **`persist.radio.multisim.config`** 是否属于 `{"dsds","dsda","tsts"}`（自研 `android.hardware.radio` 已注册 `slot1` + `slot2` 双逻辑调制解调器，但 vendor 未声明 `dsds`）；
   - 开启双卡模型（`isDuosModel() == true`）后，`SecMobileNetworkPreferenceController` 还会读取 **`gsm.sim.currentcardstatus`**（缺省 `"9"`），要求至少一个卡槽值为 `"3"`（卡已激活就绪，如 `"3,9"`），否则走 `remove mobile network CDMA` 隐藏入口。
2. **国行 CSC 分区未随固件解包挂载，且三星将可卸载内置应用存放在 `userdata.img:/pdp_bkup/*.zip` 并依赖 `sec_system_init_shell` vendor SELinux 授权**：
   - 官方四件套 `SM-S9380_3_20260813083211_652iw49crl_fac.zip` 的 `CSC_CHC_*.tar.md5` 内含 `prism.img.lz4`（携带 `/etc/carriers/CHC/default_workspace.xml` 默认桌面布局、`sales_code.dat` 等）、`optics.img.lz4`（`cscfeature.xml`）与 `omr.img.lz4`，此前 `unpack/config.json` 与 `vda/config.json` 未纳入这三块分区，导致 `/prism`、`/optics`、`/omr` 未挂载，`/system/bin/scs` 无法置位 `ro.csc.sales_code=CHC` 与 `mdc.sys.omc_etcpath=/prism/etc/carriers/CHC`；
   - 官方 `AP_*.tar.md5` 将 27 个三星内置应用存放在 24 GiB 稀疏 F2FS `userdata.img` 的 `/pdp_bkup/pdp_ramload.zip` 与 `/pdp_bkup/apps_apks.zip` 内（另含 2.83 GiB 强绑高通 NPU 的 `AICoreLlmCHN.apk`/`AICoreLmmCHN.apk` 离线权重）。云机使用空白 214.23 GiB `data.qcow2`，不能直接灌入 24 GiB `userdata.img`；
   - 反编译 `services.jar` 确认：当 `ro.csc.sales_code=CHC` 时，`PmCustomInjector` 固定选用 `ChnPrePackageInstaller`，其 `prepareInstall()` 仅在 `mIsUpgrade == true`（OTA 升级）时解析 `chn_preload_package_list.xml`，首次开机（`mFirstBoot=true, mIsUpgrade=false`）完全交由三星原生 `/system/etc/init/sec_pdp.rc` 执行 `/system/etc/pdp_papps_copy.sh` 将 `/system/preload/*`（`.apk.gz`）与 `/system/carrier/preload/*`（`.apk`）拷入 `/data/app` 并调用 `pdp_bnr selinux_label` 统一 `restorecon`；
   - 三星在 `/system_ext/etc/selinux/system_ext_sepolicy.cil` 中定义了 `(typetransition init shell_exec process sec_system_init_shell)`，却将 `sec_system_init_shell` 读写 `pdp_data_file`（`/data/pdp_bkup`）、`apk_data_file`（`/data/app`）、`kmsg_device` 及执行 `toolbox_exec`/`shell_exec`/`system_file` 的全部权限放在官方 `vendor_sepolicy.cil`。使用 lineage vendor 基底时，`pdp_papps_copy.sh` 启动即 `exit(1)`，不仅导致 `/system/preload` 自带的 14 个预装包无法解压入 `/data/app`，还连累 `init` 阻塞等待 `/data/pdp_bkup/fsh_papps_cp_done` 超时 16.4 秒；
   - 此外，若将 `samsung_preload.img`（挂载于 `/oem`）直接写成 `dir /system/preloadFactoryResetOnly -> /oem`，`cdh_redirect` 的逆向钩子会使 `init` first-stage 校验 `realpath(/oem)` 返回虚拟路径而非 `/oem`，触发 `libfs_mgr: mount point is not canonical` 拒绝挂载。将 APK 收拢至镜像内 `/preload/<App>/<App>.apk` 并映射 `dir /system/carrier/preload -> /oem/preload`，既避开 `/oem` 挂载点逆向改写与根级 `lost+found`，又原生对接 `pdp_papps_copy.sh` 的 `/system/carrier/preload` 拷贝分支。
3. **三星 `scs`（`/system/bin/scs`）缺少官方 vendor SELinux 授权，导致 `persist.sys.omc_etcpath` 与 `persist.sys.omc_path` 未落盘，Launcher 回退海外 GMS 残缺布局**：
   - 反编译 `/system/priv-app/TouchWizHome_2017/TouchWizHome_2017.apk`（`HoneySpace.DataParser`）确认：OneUI 桌面在国行无抽屉模式（`HOME_ONLY`）下只从 **`persist.sys.omc_etcpath`**（而非早期临时属性 `mdc.sys.omc_etcpath`）拼接加载 `default_workspace_homeonly.xml` 与 `default_application_order.xml`；
   - `/system/etc/init/scs.rc` 在 `post-fs` 与 `on property:ro.persistent_properties.ready=true` 两阶段拉起 `/system/bin/scs`（`u:r:scs:s0`）。官方将 `scs` 读取 `persistent_properties_ready_prop`、`efs_file` 及设置 `csc_prop` / `locale_prop` / `system_prop` 的规则放在官方 `vendor_sepolicy.cil`。缺失时第二阶段 `scs` 报 `avc: denied { read } ... tcontext=u:object_r:persistent_properties_ready_prop:s0` 并打印 `[setDmRwProperties] persist properties set : False`，导致 `persist.sys.omc_etcpath` 与 `persist.sys.omc_path` 为空：
     1. `CscParser` 反复回退查找不存在的 `/system/csc/customer.xml` 报 `FileNotFoundException`；
     2. `HoneySpace.DataParser` 读不到 `/prism/etc/carriers/CHC/default_workspace_homeonly.xml`，回退 APK 内置海外 GMS 模板（首屏 Google 搜索栏/Play 商店/Google 文件夹因无 GMS 被滤空，仅剩右上角单个天气小组件，40 个国行应用被平铺挤到第 2~3 屏）。

## 三、证据

修复前实测日志（完整输出见 `log-fail.txt`）：

```text
=== [PRE-FIX] SIM & CSC Properties ===
gsm.sim.state=[LOADED]
ril.ICC_TYPE0=[]
ril.ICC_TYPE1=[]
gsm.sim.currentcardstatus=[]
persist.radio.multisim.config=[]
ro.csc.sales_code=[]
mdc.sys.omc_etcpath=[]
(persist.sys.omc_etcpath / persist.sys.omc_path unset even after mounting /prism & /optics)

=== [PRE-FIX] SecSettings ConnectionsUtils Logs ===
D SecSimFeatureProvider: isMultiSim :
D ConnectionsUtils: for mobile_networks -> phoneType: 1
D SecSimFeatureProvider: getTelephonyProperty : 0 : 0
D SecSimFeatureProvider: getTelephonyProperty : 1 : 0
D SecSimFeatureProvider: getEnabledSimCnt : 0
D ConnectionsUtils: isNoSIM : 0
D ConnectionsUtils: isHideMobileNetworks: return true
I SecMobileNetworkPreferenceController: remove mobile network by HideMobildeNetwork

=== [PRE-FIX] PDP Failure & scs SELinux Denials ===
init: SVC_EXEC service 'exec 16 (/system/bin/sh /system/etc/pdp_papps_copy.sh)' pid 953 ... exited with status 1 waiting took 16.412001 seconds
init: [PDP] apks size after preload-app copy : 0
audit: avc: denied { search } for pid=932 comm="scs" name="efs" scontext=u:r:scs:s0 tcontext=u:object_r:efs_file:s0 tclass=dir permissive=0
audit: avc: denied { read } for pid=932 comm="scs" name="u:object_r:persistent_properties_ready_prop:s0" scontext=u:r:scs:s0 tcontext=u:object_r:persistent_properties_ready_prop:s0 tclass=file permissive=0
D scs     : [setDmRwProperties] persist properties set : False
E CscParser: FileNotFoundException!! /system/csc/customer.xml: open failed: ENOENT
```

## 四、改动

严格遵守官方 `super.img` 与 CSC 镜像零改动约束，改动分层落位如下：

1. **三星 RIL 变体同步写入 SIM 类型与卡状态属性**：
   - `devices/hardware/android.hardware.radio/src/samsung/radio_samsung_ext.cpp`：新增 `SyncSamsungSimProperties(slot, ready)`，通过 `__system_property_set` 按槽位同步 `ril.ICC_TYPE0`（有卡供电时置 `"2"` USIM，否则 `"0"`）、`ril.ICC_TYPE1`（`"0"`）与 `gsm.sim.currentcardstatus`（卡槽 1 就绪时 `"3,9"`），在 `SehSimService` 构造、`NotifyReady()`、`getIccCardStatus`（TX `1`）与 `setSimOnOff`（TX `5`）四处联动刷新（`ril.*` 与 `gsm.*` 均属 `radio_prop`，`hal_radio_default` 已具备写权限）；
   - `devices/manifest/configs/samsung/pa3q/pa3q-android15/vendor/config.json`：`build-prop-sets` 新增 `"persist.radio.multisim.config": "dsds"` 与 `"ro.csc.sales_code": "CHC"`。
2. **稀疏 F2FS 零展开直读与三星内置应用镜像 `samsung_preload.img` 自动衍生**：
   - `agent/modules/pipeline/internal/fsimg/{sparse_reader.go,sparse_reader_test.go,f2fs.go,f2fs_extract.go}`：为 `fsimg.Open` 与 `fsimg.IsF2FS` 增加 Android Sparse v1.0（`0xed26ff3a`）随机读取支持，无需将 24 GiB 稀疏 `userdata.img` 展开落盘即可直接寻址读取 F2FS，并新增 `(v *Volume) OpenFileSection(inImage)` 返回 `*io.SectionReader`；
   - `agent/modules/pipeline/stages/01_unpack/official.go`：`ExtractMembersOnly` 对 `fsimg.IsF2FS` 可直读的稀疏镜像跳过冗余 `unsparse` 落盘；
   - `agent/modules/pipeline/stages/02_prepare/{samsung_preload.go,derive.go,place.go}`：支持 `samsung_preload.img:userdata.img` 衍生规范，流式读取 `/pdp_bkup/{pdp_ramload.zip,apps_apks.zip}` 提取 27 个三星内置应用（剔除 2.83 GiB NPU 离线大模型权重与纯第三方推广包）至 `/preload/<Stem>/<Base>.apk`，封装为带 `u:object_r:system_file:s0` 标签的只读 ext4 `samsung_preload.img`（`1.82 GiB`），并在 `place.go` 支持增量补抽新增 `keeps` 镜像且不重抽 `super.img`。
3. **挂载 CSC / 预装分区并补齐三星 PDP 与 `scs` SELinux 策略及路径映射**：
   - `devices/manifest/configs/samsung/pa3q/pa3q-android15/unpack/config.json` 与 `vda/config.json`：纳入 `prism.img`（→ `/prism` `ro`）、`optics.img`（→ `/optics` `ro`）、`omr.img`（→ `/omr` `rw`）与 `samsung_preload.img:userdata.img`（→ `/oem` `ro`）；
   - `devices/manifest/configs/samsung/pa3q/pa3q-android15/module/config.json` + `vendor/etc/chn_preload_package_list.xml`：新增 `dir /system/carrier/preload -> /oem/preload` 与 `file /prism/etc/carriers/CHC/sysconfig/chn_preload_package_list.xml -> /mnt/vendorpatch/vendor/etc/chn_preload_package_list.xml`（总规则数 `20/32`）；
   - `devices/manifest/configs/samsung/pa3q/pa3q-android15/vendor/sepolicy/{vendor_file_contexts,vendor_sepolicy.cil}`：补齐 `/data/pdp_bkup`（`pdp_data_file`）、`/cache/pdp_bkup`（`cache_pdp_file`）标签、第十四段 `init` / `sec_system_init_shell` 官方授权（使首启 PDP 脚本正常运行）以及第十五段 `scs` 官方授权（使 `scs` 读取 `persistent_properties_ready_prop` 并写入 `persist.sys.omc_etcpath` / `persist.sys.omc_path` 等 `csc_prop` / `locale_prop`）。

## 五、验证

1. **单元测试与完整流水线构建**：
   - `go test ./internal/fsimg ./internal/manifest ./stages/01_unpack ./stages/02_prepare ./stages/06_build_vendor_boot ./stages/08_build_hardware ./stages/09_build_vendor ./stages/13_pack_vda` 全部通过；
   - `02_prepare` 从官方稀疏 `userdata.img` 流式生成 `samsung_preload.img`（`1.82 GiB`，27 个 APK 位于 `/preload/` 子目录且 `e2fsck` 零告警）并自动清理临时 `userdata.img`；`device build -from 09_build_vendor pa3q` 生成 21.00 GiB `vda.img`（`vda-2f93b17f6115`）与最终 `patch.img`（`e7e65647de32…`）。
2. **清空可写层冷启动实机终验**（`pa3q_16-AP3A.240905.015.A2-20260812` 实例 `#1`，删除 `writable-*.qcow2`，完整日志见 `log-pass.txt`）：
   - **起机、挂载与国行 OMC 属性**：104s 达 `sys.boot_completed=1`，`getenforce=Enforcing`；`/prism`（`vda13`）、`/optics`（`vda14`）、`/omr`（`vda15`）、`/oem`（`vda16`）全部在 first-stage 正常挂载；`scs` 第二阶段（PID `984`）零拒权输出 `[setDmRwProperties] persist properties set : True`，置位 `ro.csc.sales_code=CHC`、`persist.sys.omc_etcpath=/prism/etc/carriers/CHC`、`persist.sys.omc_path=/optics/configs/carriers/CHC/conf`、`persist.sys.omc_respath=/omr/res`、`persist.sys.omc_support=true`、`SemCscParser: csc region: CHN`，`/system/csc/customer.xml` `ENOENT` 报错归 **0**。
   - **设置「SIM 管理器」与「移动网络」菜单恢复**（`pass-connections.png`、`pass-sim-manager.png`、`pass-mobile-network.png`）：`ril.ICC_TYPE0=2`、`ril.ICC_TYPE1=0`、`gsm.sim.currentcardstatus=3,9`、`persist.radio.multisim.config=dsds`；`SecSimFeatureProvider` 识别 `getEnabledSimCnt : 1` / `isMultiSim : dsds`，「SIM 管理器」（显示 `SIM 1 中国联通` 已启用）与「移动网络」入口及 `NetSettingsActivity` 二级页面正常展示无崩溃。
   - **40 个三星原装内置应用首启铺设与国行首屏/多页桌面布局还原**（`pass-home-page1.png`、`pass-home-page2.png`、`pass-minus-one-spage.png`、`pass-calculator.png`）：`pdp_papps_copy.sh` 耗时 `5.551s` 以 `status 0` 完成 `/system/preload`（14 个 `.apk.gz`）与 `/system/carrier/preload`（27 个 `.apk`）向 `/data/app` 的复制解压（`2,286,994,917 B`）；`HoneySpace.DataParser` 加载 `/prism/etc/carriers/CHC/default_workspace_homeonly.xml` 与 `default_application_order.xml`，**第 1 屏（主屏，`pass-home-page1.png`）** 完整呈现天气小组件（`16° 晴 余杭区`，`(0,2, 2×2)`）、三星健康每日活动小组件（`(2,3, 2×1)`）、全局搜索栏（`(0,4, 4×1)`）及底部「应用商店、相册、主题商店、三星生活助手」4 个应用图标与 4 个 Dock 图标，**第 2 屏（`pass-home-page2.png`）** 完整呈现 `Samsung`、`Microsoft`、`三星专属` 三大分类文件夹以及三星钱包、时钟、联系人、设置、日历、计算器、笔记、游戏中心、三星商城、智能搜索、三星阅读等全套内置应用，左滑负一屏（`pass-minus-one-spage.png`）正常渲染，整机 `logcat -b crash` 为 **0**。

修复前后关键指标对比（已排除 `adbd` 回显）：

| 核验项 | 修复前（`log-fail.txt`） | 修复后（`log-pass.txt`） |
| :--- | :--- | :--- |
| `ril.ICC_TYPE0` / `gsm.sim.currentcardstatus` / `multisim.config` | 空 / 空 / 空（`getEnabledSimCnt : 0`） | **`2` / `3,9` / `dsds`**（`getEnabledSimCnt : 1`） |
| 设置 → 连接「SIM 管理器」与「移动网络」入口 | 被 `HideMobildeNetwork` 移除隐藏 | **正常显示，二级页正常打开** |
| `/prism`、`/optics`、`/omr`、`/oem` 分区挂载与 `ro.csc.sales_code` | 未挂载 / 空 | **四分区全部挂载 / `CHC`** |
| `scs` AVC 拒绝数 / `persist.sys.omc_etcpath` / `CscParser` `ENOENT` | `3` 次拒绝 / 空 / 大量 `/system/csc/customer.xml: ENOENT` | **`0` / `/prism/etc/carriers/CHC` / `0`** |
| `pdp_papps_copy.sh` 执行耗时与拷入字节数 | `exit(1)` 并阻塞超时 `16.41s` / `0 B` | **`status 0` 耗时 `5.55s` / `2,286,994,917 B`（2.13 GiB）** |
| 主屏幕首屏与多页桌面布局 | 首屏无应用图标（海外残缺模板） | **加载国行 `default_workspace_homeonly.xml`，首屏小组件+4 图标、次屏文件夹+40 内置应用全量就位** |
| `logcat -b crash -d` 崩溃行数 | 0 | **0** |

## 六、遗留

1. **端侧离线大模型包刻意剔除**：官方 `apps_apks.zip` 内的 `AICoreLlmCHN.apk`（`1.83 GiB`）与 `AICoreLmmCHN.apk`（`1.00 GiB`）为高通 Hexagon NPU 离线权重包，与 `../issue-oneui-ril-bt-keymint-moneta-wallpaper/` 屏蔽 `AICoreCHN` 的边界保持一致，在 `samsung_preload.go` 白名单中显式排除以节省 `1.82 GiB` 之外的无用磁盘占用。
2. **双卡槽二状态**：当前宿主基带默认提供单张在线 USIM（卡槽 1 `ril.ICC_TYPE0=2`、卡槽 2 `ril.ICC_TYPE1=0`，`gsm.sim.currentcardstatus=3,9`），符合真机双卡单待插卡态；若后续宿主支持双 SIM 同时在线，可按槽位状态将 `slot2` 动态切换为 `ICC_TYPE1=2` 与 `"3,3"`。
