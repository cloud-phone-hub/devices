# gold 云机 × 真机 device-checker 对拍（卡点：MIUI 安装拦截 + guest 时钟停在镜像构建时刻）

- 设备：`xiaomi/gold`（Redmi Note 13 5G / MT6833，镜像基线 `OS3.0.10.0.VNQCNXM` 20260715）
- 真机：`07cf5fc1784783d5`（已 OTA 到 `OS3.0.11.0.VNQCNXM`）；云机：实例 2，`c46c631991f2196b-cloud`
- 日期：2026-10-31
- 结果：22 个功能点 × 双端同批跑通（`run-00034`）。4 个 `ultimate.*` 检测面里
  `cloud_emulator` / `probe_runtime` 命中集与真机**完全一致**；`hook_inject` 云机更干净；
  `root_selinux` 云机多 2 处（`U-CUSTOM-ROM` / `U-PEERGROUP-HOLE`）。期间打通 3 个前置卡点：
  装包被 MIUI 拒、数据面不通、**guest 时钟比宿主慢 108 天**。

## 一、问题

按要求「gold 启动实例、用 device-checker 与真机对拍」，链路上连续卡了三处：

1. 云机装不上 device-checker：`adb install` 一律返回
   `INSTALL_FAILED_USER_RESTRICTED: Install canceled by user`，且**屏幕上看不到任何确认弹窗**
   （6 张连拍截图逐字节相同），改 `adb_install_need_confirm` / `verifier_verify_adb_installs` /
   `security_adb_install_enable` / `persist.security.adbinstall` / `adb install -i <installer>`
   全无效。
2. 装上后服务端 `/api/devices` 里没有云机：guest 只有 `lo` + `mgmt0 10.0.2.15/24`，
   无 IPv4 默认路由，`ping 101.33.73.101` 报 `Network is unreachable`。
3. 联上对拍后出现**假差异**：云机 `ultimate.cloud_emulator` 多命中一条 `U-DATA-ATIME`
   （`ageDays=0`），而真机同项 `ageDays=170` 不命中。

## 二、根因

### 1. 装包拦截：MIUI 在 PM 里硬拦 shell 安装

反编译 `miui-services.jar` 确认调用链：
`PackageManagerServiceImpl.verifyInstallFromShell` → `PmInjector.installVerify(I)I` →
以 `"pm"` 身份 `startActivity(com.miui.securitycenter/.permcenter.install.AdbInstallActivity)`，
带 `observer`(IBinder) + `SESSION_ID`，然后 `wait(5000)` 等 `status`。
**只有 `status == 2` 才放行**，否则 `fatalIf(v1=(status!=2), -111, "PKMSImpl", …)` →
`Failure [INSTALL_CANCELED_BY_USER]` → session 标 `INSTALL_FAILED_USER_RESTRICTED`。

再看 `AdbInstallActivity.onActivityCreated`：读 `observer` → `IMessenger` → 若
`observer == null` 则 `finish()`；若 `!t6.c.t()` 则 `a=-1; finish()`（`t6.c.t()` 在
`ro.debuggable != 1` 时为真）；`sessionId == -1` / `getSessionInfo() == null` /
`Q0(pkg)`（包已存在）也各自 `finish()`。本机 `ro.secure=1`、`ro.build.type=user`、
`u:r:pioneer:s0`，没有 `su`——`adb shell` 的 uid 2000 走完整拦截，而弹窗进程
（securitycenter）在这套虚拟树上 15 ms 内被销毁，用户永远点不到确认，于是死锁在
「UI 没画出来 + 超时按拒绝处理」。

**结论：这是设计上的 uid 门禁，不是配置项能关的**。uid 0 走 `pm install` 根本不进
`verifyInstallFromShell`（日志只剩 `PKMSImpl: Check install restriction took 0ms`，
无 `assertCallerAndPackage`）。

### 2. 数据面：`ccmni2` 只有 IPv6，缺 IPv4 与默认路由

QEMU 侧两块用户态网卡：`net0`（`restrict=on`，只做 `hostfwd :5911→5555` 的 adb）与
`net2`（`net=100.64.155.0/24`，数据面）。guest 里 `ccmni2` 只被配了 link-local/`fec0::`
地址，`settings global mobile_data=0`、`svc data enable` 报
`Failure calling service phone: Failed transaction`——即没有 ModemData 走 setupDataCall，
路由不会自动下发。

对照 `../issue-cellular-data-no-setupdatacall/solution.md`（note13-5G-15）：那里 cold boot 后
框架会自己发起 SetupDataCall，ccmni2 拿到 `100.64.155.15/24`，默认网络是**已验证的**
`MOBILE[LTE]`。gold 上这条路没起来，只能手工 `ip` 补路由；代价是 **ConnectivityService 侧
仍认为“无活动网络”**（`hasActive=False`、无 `WIFI`/`CELLULAR` transport、无 DNS 链路），
见下（遗留 2）。

### 3. 时钟：guest 没有 RTC 设备，内核以 `ro.build.date.utc` 起钟

- `ls /dev/rtc*` → 不存在；本机 QEMU 构建里 `-device help` 也没有 `pl031`/`goldfish-rtc`
  之类 RTC 设备，`qemu.go:541` 的 `-rtc base=localtime` **没有可绑定的设备，等于空转**。
- `ro.build.date.utc = 1784120585` = `Wed Jul 15 21:03:05 CST 2026`；
  `stat /data` 的 mtime 也是 `2026-07-15 21:03:11`。开机瞬间时钟就落在「镜像被冻结的那一刻」，
  比宿主（2026-10-31）慢 108 天。
- `auto_time=1`、`ntp_server=pool.ntp.org` 都是开的，但数据面当时没通，
  `cmd network_time_update_service force_refresh` 返回 `false`。
- 这解释了假差异：`U-DATA-ATIME` 用「现在 − /data 最早 atime」算新鲜度，guest 认为
  「现在」就是 07-15，于是 `ageDays=0`；真机以同样的算法得 170 天，不命中。
  顺带解释了 logcat 行首时间戳全是 `07-15`。

## 三、证据

- 修复前：`log-fail.txt`（`run-00031` 状态表 + `ultimate.*` 命中对比 + 安装拦截栈 + 时钟取证 + 路由失败）
- 修复后：`log-pass.txt`（`run-00033/34/35` 状态表与命中对比 + root 装包 + 路由 + 对时实测）
- 原始结果：`out/tmp/gold-compare-run-00031.json`（修前）、`-00034.json`（修后）
- 工具：`out/tmp/jsrun.py`（经 `/api/channel/cmd` 以 root 下发 JS）、
  `out/tmp/compare_gold.py`（双端同批下发 + 扁平化 diff，噪声前缀按 SKILL §4）

关键单点：

| 项 | 真机 | 云机 |
|---|---|---|
| `ultimate.cloud_emulator` | hit=5（3 warn / 2 info） | 修前 hit=6（多 `U-DATA-ATIME`）→ **修后 5，一致** |
| `ultimate.probe_runtime` | hit=1（`U-STATUS-SANDBOX`） | hit=1，一致 |
| `ultimate.hook_inject` | hit=2（`H-113` **deadly**：`fd(deleted):3` + `H-21`） | hit=1（仅 `H-21`） |
| `ultimate.root_selinux` | hit=4（`H-40/U-DIRTY-POLICY/U-MNT-TMPFS-OPEN/H-01`） | hit=6（多 `U-CUSTOM-ROM` warn、`U-PEERGROUP-HOLE` **deadly**） |
| `android.feature` | — | 373 键**差异 0** |
| `camera.camera_new` | — | 8700 键**差异 0** |
| `hardware.sensor` | 29 项 | 29 项，仅磁力计 `mmc5603/Memsic` vs `qmc6308/QST` |

## 四、改动

云机侧（都是运行期操作，未改镜像/静态树）：

1. 装包：`adb push` 落地后用 root 通道执行
   `cmd.exec("nohup pm install -r -t /data/local/tmp/dc.apk > /data/local/tmp/ri.log 2>&1 &")`
   → `Success`；随后 `pm grant` 8 个运行时权限、`am start -n dev.checker/.MainActivity`。
2. 数据面（沿用 `issue-battery-align` 的既有配方，补 `onlink`）：
   `ip addr add 100.64.155.15/24 dev ccmni2`；
   `ip route add default via 100.64.155.2 dev ccmni2 onlink`（**不加 `onlink` 会
   `RTNETLINK answers: Network is unreachable`**）；`ip rule add pref 10500 lookup main`。
3. 对时：`cmd alarm set-time $(date +%s)000` → `date -u` 与宿主一致。
   `date -s` 在 guest 上直接 `Invalid argument`，走不通；`-anyway` 无 RTC 设备。
4. 对拍脚本：`out/tmp/compare_gold.py`（默认 22 个功能点；`--run` 可复算已有 run）。

## 五、验证

- `run-00033`（只跑云机、时钟已修）：`ultimate.cloud_emulator` 的命中集从 6 回到 5，
  `U-DATA-ATIME` 消失，与真机逐条相同。
- `run-00034`（真机 + 云机、22 功能点同批）：44 条里 40 条 succeeded；
  `hardware.sensor` 真机超时属采样耗时，单跑 `run-00032` 成功（29 传感器）；
  真机 `ultimate.probe_runtime` 因中途掉线重跑 `run-00035` 成功（hit=1）。
- 附带结论：`android.feature`、`camera.camera_new`、`media.mediacodec` 三面**零差异**，
  说明 gold 静态树在「能力清单 / 相机 / 编解码」这条线上已经把真机口径复刻到位。

## 六、遗留

按影响排序，前 3 条建议进流水线而不是手工补：

1. **guest 无 RTC → 时钟永远停在镜像构建时刻**。建议在 `core/host/daemon` 里
   `boot_completed=1` 后经 hvc7 下发 `cmd alarm set-time <宿主毫秒>`（本次已人工验证有效），
   或在 QEMU 侧补一个 RTC 设备（当前构建没有可用 RTC device）。
   不修的话任何「时间类」探针（`U-DATA-ATIME`、证书校验、会话过期、服务器对时）都是假阳性。
2. **网络栈没注册进 ConnectivityService**（`network.connectivity` 一面云机全空：
   `hasActive=False`、无 transport/VALIDATED/DNS/链路）。现在的手工 `ip route` 只是“黑路由”，
   任何用 `ConnectivityManager` 判在线的 App（大量风控/检测脚本）会直接判定云机**离线**。
   正解是回到 `setupDataCall` 路线（参照 `../issue-cellular-data-no-setupdatacall/solution.md`
   在 note13-5G-15 上的已验证形态：默认网络 `MOBILE[LTE]` + `IS_VALIDATED`），
   而不是手工补路由。
3. **`hal_lineage_*` 策略泄漏**（`U-CUSTOM-ROM` warn）：`/vendor/etc/selinux/vendor_sepolicy.cil`
   262 处、`vendor_file_contexts` 12 处含 `lineage`（`hal_lineage_touch/powershare/livedisplay/
   fastcharge/health` 等 lineage 专有 HAL 域）。改法：`07_build_vendor` 落盘后对这两个文本做
   统一改名（如 `_lineage_` → `_vendor_`），需两文件同步、并确认 `precompiled_sepolicy` 不引用旧名。
4. **`U-PEERGROUP-HOLE`（deadly）**：`/proc/self/mountinfo` 的 peer group 编号有 2 个空洞
   （云机 groups=64，真机 72）。来自挂载集合差异，靠静态树补不了，需要在 init 阶段补齐对应挂载点。
5. **`/storage/emulated/0` 是空壳**：20 个探测路径里 17 个 `ENOENT`，`/storage/emulated`
   `mtime=1970-01-01`、`uid=0`、`st_size=60`。真机有完整标准目录（Alarms/DCIM/Download…）。
   影响 `files.storage`（300 键里 268 项差异）及一切媒体/存储类检测。
6. **网卡面**：云机没有 `wlan0`（真机 wifi `192.168.31.159`），且暴露 `mgmt0 10.0.2.15/24`
   ——QEMU 用户态网卡是教科书级云机特征，且 `NetworkInterface` 直接可读。建议隐藏/改名管理网卡，
   并补 `wlan0`（引擎已有 hvc9/hvc10 的 wifi 后端，但 guest 里没建出该接口）。
7. **`hardware.cpu.cpuFreq` 为空**（真机 2 个 policy、各 16 档频率）：VM 没暴露
   `/sys/devices/system/cpu/cpufreq/policy*`（该项 46 键里 44 项差异，主要来源）。
8. **磁力计型号对不上**：`mmc5603/Memsic`（真机）vs `qmc6308/QST`（云机）。
   `devices/manifest/xiaomi/gold/vendor/etc/sensors_config.xml:31-44`（`handle="2"`）里
   `name`/`vendor` 两个属性写成了 `qmc6308`/`QST`，其余字段（`max-range 4912.05`、
   `resolution 0.15`、`min-delay 10000`、fifo 600/4500）与真机**逐项一致**——
   即只需改这两个属性，改完 `hardware.sensor` 这一面就零差异。
9. **镜像版本落后真机一个 OTA**：`build.INCREMENTAL/FINGERPRINT/SECURITY_PATCH` 三处必然不同
   （`OS3.0.10.0` / `2026-07-01` vs `OS3.0.11.0` / `2026-09-01`）。baseline 目前钉在
   `gold_images_OS3.0.10.0.VNQCNXM_20260715…`，真机 OTA 后需同步换 baseline。
10. **检查脚本自身缺陷**：`ultimate.debug_integrity` 在两端都报
    `Wrapped java.lang.RuntimeException: 字段不存在: android.content.…`，属脚本 bug，
   与机型无关（两端一致，不影响对比结论）。

## 7. 追加：把能对齐的都对齐之后的复跑（run-00039 → run-00041）

2026-10-31 晚按「能对齐的都对齐」把上面遗留里的 2/5/6/8 四项做掉，逐项证据见
`../issue-guest-clock-no-rtc/`（#98）、`../issue-emulated-storage-unmounted/`（#99）、
`../issue-gold-wifi-disabled/`（#100）、`../issue-gold-sensors-magnetometer/`（#97）。
本目录另存本轮原始产物：`run-00041-log.txt`（执行状态 + 汇总）、
`run-00041-diff.txt`（逐键差异，已剔除 `build./ro./s_*` 噪声）。

三次复跑的对照（同一台真机 + 同一台云机；差异项数）：

| check | run-00039（对齐前） | run-00040（时钟/存储/磁力计 + 手工开 Wi-Fi） | run-00041（Wi-Fi 开机引导） |
| :--- | ---: | ---: | ---: |
| android.feature | 0 | 0 | 0 |
| camera.camera_new | 0 | 570 | **0** |
| dev.test_vendor | 5602 | 5602 | 5602 |
| files.storage | 139 | 138 | 138（`st_mode/st_uid/st_gid` **0**，全是 inode/时间戳） |
| hardware.battery | 6 | 5 | 6 |
| hardware.cpu | 44 | 44 | 44 |
| hardware.gles | 2 | 2 | 2 |
| hardware.inputs | 12 | 12 | 12 |
| hardware.meminfo | 1 | 1 | 1 |
| hardware.uname | 2 | 2 | 2 |
| media.mediacodec | 0 | 0 | 0 |
| network.cell | 65 | 3 | 30 |
| network.connectivity | 13 | 5 | **5** |
| network.ifaces | 36 | 42 | 42 |
| properties.build | 0 | 0 | 0 |
| screen.display | 17 | 17 | 17 |
| ultimate.cloud_emulator | 3 | 3 | 3 |
| ultimate.hook_inject | 18 | 18 | 18 |
| ultimate.probe_runtime | — | 35 | **25** |
| ultimate.root_selinux | 24 | 24 | 24 |
| **合计** | **5984** | **6523** | **5971** |

已闭环的遗留：

- 遗留 2（数据面 `hasActive=False`）：改用真机同形的 Wi-Fi 通道，`Active default network`
  变为 `WIFI`、`NET_CAPABILITY_VALIDATED=true`，agent 经 `wlan0` 直连注册。
- 遗留 5（`/storage/emulated/0` 空壳）：`emulated;0` 引导时挂载 + 14 个标准目录按真机
  `uid:gid:mode` 对齐，20/20 路径逐项相同。
- 遗留 6（没有 `wlan0`、暴露 `mgmt0`）：`wlan0` 已起（`192.168.60.100/24`）；
  `mgmt0` 仍需保留（adb/daemon 通道走它），但不再是活动网络 —— `connectivity` 里
  真机/云机都是 `wlan0`。
- 遗留 8（磁力计型号）：清单改 `mmc5603`/`Memsic`，`dumpsys sensorservice` 清单两侧逐字节一致。

仍未对齐（详见各 issue 的「遗留」与 `run-00041-diff.txt`）：`dev.test_vendor` 5602 项
（云机检测器自身输出）、`hardware.cpu.cpuFreq`（VM 无 cpufreq policy）、`hardware.uname`
（6.6.89 vs 6.6.118）、`hardware.battery`（实时电压/温度/电量计）、`screen.display`
（virtio-gpu 固定 60 Hz、整数 mm 推出的 xdpi）、`hardware.inputs`（7 个设备注册顺序）、
`ultimate.root_selinux`（lineage 策略串 `U-CUSTOM-ROM`、`U-PEERGROUP-HOLE`）、
`ultimate.hook_inject`（云机专有注入痕迹）、`network.cell`（两侧均时变；modem 模拟器在宿主）。
