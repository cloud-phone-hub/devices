# m22 服务型 SELinux 类型名去 `cdh` 前缀（去改造特征）

> 设备：`devices/flyme/m22`（QTI 12.5.0.0A，Android 16 / QTI `sun`）
> 关联：#58（perf2 的 find 授权迁到模块层，本轮在其成果上改名）、
> #56（sensor_aux 等模块落位）、#53（perf2 空桩初建）
> 日期：2026-10-29。§1–§6 为第一阶段（8 个服务类型，实例 39），
> §7 为第二阶段（wifi 类型 + rc 服务名 + 配置文件名，实例 41/42）

## 1. 问题

### 1.1 用户提出：`cdh` 前缀会留在镜像与日志里

> 「vendor.qti.hardware.perf2 不应该带 cdh 开头，容易留下特征。」

指向 `engine/vendor/hardware/vendor.qti.hardware.perf2/sepolicy/`：perf2 的服务类型
叫 `cdh_perf2_service`。`cdh` 是本项目（cloud-phone-hub）的缩写，一旦写进 SELinux
类型名，它会在三个可观测面以明文出现，等于在成品镜像里签名：

| 可观测面 | 表现 |
| :--- | :--- |
| 镜像静态 | `/vendor/etc/selinux/vendor_sepolicy.cil` 的 `(type cdh_perf2_service)`、`vendor_service_contexts` 的 `u:object_r:cdh_perf2_service:s0`；任意 `strings`/`grep cdh_` 即可筛出整个 vendor 分区 |
| 运行期进程与文件 | `ls -Z` / `ps -Z` 直接打 `u:object_r:cdh_*_service` |
| 运行期日志 | avc 记录里 `tcontext=u:object_r:cdh_perf2_service:s0`（实例 37 实测 1653 条，见 `log-fail.txt` A 段） |

### 1.2 范围：不止 perf2 一个

同一批「Flyme 会查、本云 vendor 侧没实现」的服务名，在机型层也用了同一套前缀。
本轮清点出 **8 个带 `cdh_` 前缀的服务类型**，其中 1 个在模块层、7 个在机型层：

| 旧名 | 归属层 | 服务名（本轮不变） |
| :--- | :--- | :--- |
| `cdh_perf2_service` | 模块层 `vendor.qti.hardware.perf2/sepolicy` | `vendor.qti.hardware.perf2.IPerf/default` |
| `cdh_servicetracker_service` | 机型层 | `vendor.qti.hardware.servicetrackeraidl.IServicetracker/default` |
| `cdh_meizu_charging_service` | 机型层 | `vendor.meizu.hardware.meizu_charging.IMeizuCharging/default` |
| `cdh_aw_light_service` | 机型层 | `vendor.meizu.hardware.aw_light.IAwLight/default` |
| `cdh_awa_reality_service` | 机型层 | `vendor.awa.reality.IRealityTap/default` |
| `cdh_idiag_service` | 机型层 | `vendor.qti.diaghal.Idiag/default` |
| `cdh_realitytap_service` | 机型层 | `realitytap`（框架服务） |
| `cdh_sensor_aux_service` | 机型层 | `vendor.meizu.sensor_aux.ISensorAux/default` |

## 2. 根因

### 2.1 命名沿用了设备脚手架，而脚手架本身是「内部代号」

这 8 个类型最早是照 `devices/flyme/m22/build/vendor/build/sepolicy.py`（v1 旧链路）
的命名习惯写的，该脚本里 15 处都用 `cdh_` 前缀。engine 侧（07 片段注入）照抄了同一
套名字，于是前缀从「构建期内部标识」变成了「成品镜像里的类型名」。

### 2.2 类型名不是内部细节，它是镜像的一部分

SELinux 类型名会原样进入 `vendor_sepolicy.cil` 文本（不混淆、不哈希），并在每次
拒绝发生时落到 avc 的 `tcontext`。所以它和文件路径、属性名一样，属于「可见特征」。
AOSP 对 HAL 服务的命名惯例是 `hal_<name>_service`（见 `hal_attribute_service` 宏
生成的类型族），改成同风格后既不撞名、也看不出改造来源。

### 2.3 改名的唯一技术风险是「重复类型」

vendor CIL 只能追加不能删：若新名在 plat/system_ext/product/vendor 已有定义，或
新旧名并存，secilc 会因 duplicate type 失败 → init `InitFatalReboot`。所以改名
必须同时满足两条：**新名四处策略里不存在** + **旧名在装配树里归零**。

## 3. 证据

### 3.1 新名撞名预检：底座策略里全 0

把 07 注入的 marker 块剥掉，只看底座自带的 plat / system_ext / product / vendor 四处
策略，8 个新名的出现次数全为 0（可复现命令与实测输出见 `log-fail.txt` D 段）：

```
hal_perf2_service  …  hal_sensor_aux_service      base_hits=0 ×8
```

即底座既没有同名 `(type hal_x_service)`，也没有别处引用这些名字（AOSP 的
`hal_attribute_service` 宏只对它自己声明的模块生成类型族，perf2 不在其中）。

### 3.2 旧名在运行期的实际暴露（实例 37）

| 旧名 | 37 日志出现次数 |
| :--- | ---: |
| `cdh_perf2_service` | 1653 |
| `cdh_realitytap_service` | 9 |
| `cdh_sensor_aux_service` | 6 |
| 其余 5 个（servicetracker / meizu_charging / aw_light / awa_reality / idiag） | 0（无实体、无人查询被拒，故只存在于静态策略里） |

（37 日志：29,377,845 B / 213,606 行 / sha256 `62e9ab4a…`，与 #58 用的是同一份。）

### 3.3 改名后：装配树与设备侧旧名归零

- 装配树 `vendor/etc/selinux/vendor_sepolicy.cil`：`cdh_[a-z0-9_]*_service` **0**、
  8 个新类型各 `(type …)` **1** 次、`{{vers}}` **0**；
- 设备侧同名文件 sha256 `f83e798e6a24…`（= 09 改写后的 `patch/stage` 原件），
  同上三项同为 0 / 1 / 0；
- `vendor_service_contexts`：`cdh_` **0**，8 行标签全为 `hal_*_service`。

## 4. 改动

只改名字，不动授权面（谁 add / 谁 find 与 #58 完全一致）。

### 4.1 模块层：perf2

`engine/vendor/hardware/vendor.qti.hardware.perf2/sepolicy/vendor_sepolicy.cil`

```cil
;; 类型名用 hal_perf2_service：跟 AOSP hal_*_service 命名一致，不带项目缩写，
;; 镜像里 /vendor/etc/selinux 的标签与 avc 日志都看不出改造来源。
(type hal_perf2_service)
(roletype object_r hal_perf2_service)
(typeattributeset service_manager_type (hal_perf2_service))
(allow hal_sensors_default hal_perf2_service (service_manager (add find)))
(allow domain hal_perf2_service (service_manager (find)))
```

`.../perf2/sepolicy/vendor_service_contexts`

```
# 类型名 hal_perf2_service 与 AOSP hal_*_service 同风格，不带项目缩写。
vendor.qti.hardware.perf2.IPerf/default	u:object_r:hal_perf2_service:s0
```

### 4.2 机型层：其余 7 个类型

`devices/manifest/flyme/m22/sepolicy/vendor_sepolicy.cil`（第 58–98 行的两段）与
`devices/manifest/flyme/m22/sepolicy/vendor_service_contexts`（第 15–19、22、27 行）
按同一张表逐个改名（`type` / `roletype` / `typeattributeset` / 两处 `allow` 同步），
并在两段注释里写明命名口径：类型名一律按 AOSP `hal_*_service` 风格，不带项目缩写。

### 4.3 构建

```sh
cd engine/build
../out/bin/cdh device build m22 --only build_vendor     # 07：模块层+机型层片段注入
../out/bin/cdh device build m22 --from build_patch      # 09→10→11：patch / super / vda
```

产物（`engine/out/devices/flyme/m22/12.5.0.0A/img/SHA256SUMS` 同源）：

| 产物 | sha256 | 大小 | 备注 |
| :--- | :--- | ---: | :--- |
| `vendor.img` | `1824c88ec116effb…` | 206,680,064 B | 本轮重建 |
| `patch.img` | `c02a86866eb2f962…` | 402,653,184 B | recipe `patch-3a93fcd5dc95`，vendor 子树来自上面这张 vendor.img |
| `super.img` | `3eb133bbc4afd07d…` | 9.39 GiB | 10 阶段 `[skip]`（7 个源镜像内容未变），沿用上一版 |
| `vda.img` | — | 13.00 GiB | recipe `vda-876908d30aec`，16:44:22 |

改动的 4 个文件（均在 `engine/` 下，纯文本片段，无代码逻辑）：

1. `vendor/hardware/vendor.qti.hardware.perf2/sepolicy/vendor_sepolicy.cil`
2. `vendor/hardware/vendor.qti.hardware.perf2/sepolicy/vendor_service_contexts`
3. `manifest/flyme/m22/sepolicy/vendor_sepolicy.cil`
4. `manifest/flyme/m22/sepolicy/vendor_service_contexts`

## 5. 验证

### 5.1 静态编译：secilc 按 init 同序 RC=0

在实例 39 内以 root 用 guest 的 `secilc` 编译**设备上实际生效**的
`/vendor/etc/selinux/vendor_sepolicy.cil`（sha256 `f83e798e6a24…`，与 09 改写后的
`patch/stage/root/vendor/etc/selinux/vendor_sepolicy.cil` 完全一致）：

```
plat_sepolicy_vers=202504 genfs_labels_version=202504
secilc_rc=0
-rw------- 1 root root 869026 /data/local/tmp/policy.new
54d483a973d9ae73491779239772863636c86a040468f12ac55d99aa072d6ca2  /data/local/tmp/policy.new
```

RC=0 说明 8 个 `hal_*_service` 既没有被 AOSP 宏重复定义，也不存在新旧名并存。

### 5.2 装配树与设备侧

| 判据 | 期望 | 实测 |
| :--- | :--- | :--- |
| 装配树/设备 CIL 里 8 个 `(type hal_*_service)` | 各 1 | 各 1（设备侧 4609/4678/4681/4684/4687/4690/4693/4712 行） |
| 设备 CIL `cdh_[a-z0-9_]*_service` | 0 | 0 |
| 设备 `vendor_service_contexts` 的 `cdh_` | 0 | 0（8 行标签全 `hal_*_service`） |
| 设备 CIL `{{vers}}` 占位 | 0 | 0（已按 202504 展开，如 `system_server_202504`） |

### 5.3 运行期（实例 39，开机后 ≈8 min 窗口）

| 判据 | 实测 |
| :--- | :--- |
| `sys.boot_completed` | `1` |
| `init.svc.vendor.perf2-default` / 进程 | `running` / `pidof vendor.qti.hardware.perf2-hal-service` = 513 |
| `service check vendor.qti.hardware.perf2.IPerf/default` | `found` |
| `service list` 里的名字 | `vendor.qti.hardware.perf2.IPerf/default`、`realitytap` 原样（**只改标签，不改服务名**） |
| 开机 `Compiling SELinux policy` | 1 次 |
| `InitFatalReboot` / `Failed to resolve` | 0 / 0 |
| 结构完整 avc 记录 | 5379（kernel 379 + auditd 5000） |
| 含 `perf2` 的 avc 记录 | **0**（对照实例 37：1500+ 条） |
| 含旧名的 avc 记录（`cdh_*_service`） | **0** |
| 含 8 个新类型的 avc 记录 | 21 条，全部 `scontext=u:r:pioneer:s0`、`permissive=1`、pid 8292–8298 连号 —— 即本轮验证时我自己跑的 `service check` 循环；pioneer 是 permissive 域，只记不拦 |

日志快照：`out/core/daemon/runtime/m22_12.5.0.0A/39/qemu.log`，
12,514,453 B / 90,373 行 / sha256 `371edc6f…` / kernel ts 0.0–478.4 s。

### 5.4 验证口径

沿用 #58 确立的两条：①avc 统计按**记录**而不是按行（串口会把别的输出拼到 avc 行前面，
按行首 `scontext` 归属会造出假域）；②静态编译校验必须用 09 改写后的 CIL，装配树原文件
会在 lineage 的 `ot_rcp nlmsg` 上编不过（RC=254）。复现命令见 `log-pass.txt` F 段。

## 6. 遗留

1. **marker 仍是 `cdh-patch-begin/end`**：`vendor_sepolicy.cil` 23 处、
   `vendor_service_contexts` 7 处。它是 07 的注入锚点（常量在
   `engine/build/07_build_vendor/sepolicy.go:30-31`），本轮按约定不动；要改需同时改
   常量 + 已有装配树（幂等块会被重写），属独立改动。
2. ~~**wifi 两个类型仍带前缀**~~：已在第二阶段清掉，见 §7（`wifi_scan_param` /
   `wifi_hvc_data`）。
3. ~~**vendor 分区里的其它 `cdh` 痕迹**~~：配置文件名、rc 服务名已在第二阶段清掉，见 §7。
   **订正**：§6.3 早先把 `cdh_redirect` 记成「运行期属性」是错的 ——
   `getprop | grep -c cdh_redirect` 实测 **0**；它是**内核 printk 前缀/函数名**
   （`cdh_redirect_vendor_path()`、`CONFIG_CDH_PATH_REDIRECT`），只出现在串口
   /`dmesg` 里（实例 37 日志 337 次、实例 39 337 次、实例 41 614 次），
   不能用 `getprop` 清点，见 §7.6。
4. **旧链路脚本未同步**：`devices/flyme/m22/build/vendor/build/sepolicy.py` 仍有 15 处
   `cdh_*_service`。engine 未引用该目录（最后改动 10/21），改它不影响当前产物；
   若将来回退到 v1 链路需一起改。
5. **`hal_perf2_service` 与 AOSP 宏的关系**：本机 secilc RC=0 已证不撞，但若将来到
   带 `hal_attribute_service(hal, perf2)` 的底座上，宏会再定义同名族 → duplicate type。
   迁移到那种底座时需改回 `hal_perf2_default_service` 之类的名字（届时按 §3.1 重新预检）。
6. **07 之后仍需人工确认块内注释**：片段里的中文注释（含「不带项目缩写」等）会原样进
   镜像，属于说明性文字而非标识；如需彻底去中文/去出处，得连同注释一起精简。
   §7.6 补充了这条的实际规模：改完后设备上 `/vendor/etc/selinux/vendor_sepolicy.cil`
   里仍有 **39** 处含 `cdh` 的行，全部是注释（marker 行、`;; cdh：…` 授权说明、
   基线改写规则写进去的 `;; cdh: …`），不是类型名。

## 7. 第二阶段：wifi 类型 / rc 服务名 / 配置文件（同日 17:00–17:40）

### 7.1 问题：同一个暴露面还有三类出口

§1.1 只点了 SELinux 类型名。但「项目缩写进成品」这条在 vendor 分区里还有三类载体，
而且都比类型名更容易被 `ls` / `strings` 一眼看到：

| 载体 | 出现在哪里 | 第二阶段处理 |
| :--- | :--- | :--- |
| SELinux 类型名 | `vendor_sepolicy.cil`、`ls -Z`、avc `tcontext` | wifi 两个类型改名 |
| 配置文件名 | `ls /vendor/etc`、HAL 的 `open` 失败日志 | 17 个 `cdh_<模块>.xml` 改名 |
| rc 服务名 / rc 文件名 | `ps -Z`、`getprop init.svc.*`、`service list` | 5 个服务名 + 1 个 rc 文件名 |

### 7.2 改动

| 类别 | 数量 | 旧 → 新 |
| :--- | ---: | :--- |
| SELinux 类型 | 2 | `cdh_wifi_scan_param` → `wifi_scan_param`；`cdh_wifi_data` → `wifi_hvc_data` |
| rc 服务名（HAL） | 4 | `vendor.cdh_gnss` → `vendor.gnss-vhal`；`vendor.cdh_gold_wifi` → `vendor.wifi-vhal`；`vendor.cdh_gold_radio` → `vendor.radio-vhal`；`vendor.cdh_gold_camera` → `vendor.camera-vhal` |
| rc 服务名（值守） | 1 | `cdh_daemon` → `vendor.service-daemon`（属性 `init.svc.cdh_daemon` 随之消失） |
| 配置文件 | 17 | `cdh_<模块>.xml` → `<模块>_config.xml`（m22 8 个 + gold 9 个） |
| rc 文件名 | 1 | `cdh_frp_symlink.rc` → `frp_symlink.rc` |

两条命名口径：

1. **类型名**沿用 §4 的 AOSP 风格：不带项目缩写；`wifi_hvc_data` 顺带把「hvc 数据面
   设备」写进名字（原 `cdh_wifi_data` 去前缀后只剩 `wifi_data` 过于笼统）。
2. **rc 服务名用 `vendor.<x>-vhal`，不用 AOSP 的 `vendor.<x>-default`**：底座 gold
   与机型层可能已有同名 `*-default` service，init 遇同名 service 只认先解析到的那份，
   撞车会静默丢服务。`-vhal` 在底座 55 个 service 名里 0 命中、代码里 0 引用。

文件级（`engine/` 下）：

| 文件 | 改动 |
| :--- | :--- |
| `vendor/hardware/android.hardware.wifi/sepolicy/vendor_sepolicy.cil` | 2 个类型 + 注释 |
| `vendor/hardware/android.hardware.wifi/sepolicy/vendor_file_contexts` | `/dev/hvc10` → `wifi_hvc_data` |
| `vendor/hardware/android.hardware.{gnss,wifi,radio,camera}/vendor/etc/init/*.rc` | 4 个服务名（含 `start` 行与注释） |
| `vendor/hardware/android.service.daemon/vendor/etc/init/cph_daemon_service.rc` | 1 个服务名 |
| `manifest/flyme/m22/vendor/etc/*.xml`（8 个）、`manifest/xiaomi/gold/vendor/etc/*.xml`（9 个） | 改名 |
| `manifest/flyme/m22/vendor/etc/init/frp_symlink.rc` | 改名 |
| 27 个文本文件（9 cpp + 3 h + 8 rc + 5 xml + 2 README） | 路径引用同步 |

环境变量一并去前缀：`CDH_<模块>_XML` → `<模块>_CONFIG_XML`（9 个：
`AUDIO_ENGINE/BATTERY/GNSS/LIGHT/MEDIA/SENSORS/THERMAL/VIBRATOR/WIFI`），
全仓 `CDH_*_XML` 残留 **0**。

### 7.3 构建

```sh
cd engine/build
../out/bin/cdh device build m22 --only build_vendor     # 07
../out/bin/cdh device build m22 --from build_patch      # 09→10→11
```

跑了两轮：第一轮 17:10 定稿改名；之后发现 wifi rc 注释里还引用了已改名的头文件
`src/cdh_wifi_config.h`（实际叫 `src/wifi_config.h`），修掉后 17:33 重跑一轮，
使**源码与镜像逐字节对应**。最终产物：

| 产物 | sha256 | 大小 | 配方 |
| :--- | :--- | ---: | :--- |
| `vendor.img` | `e482c559cfa193cb…` | 206,680,064 B | `vendor-cccf0f8c247a` |
| `patch.img` | `3cdf4b16b5c7f1c3…` | 402,653,184 B | `patch-d7ebb01534d8` |
| `super.img` | `3eb133bbc4afd07d…` | 9.39 GiB | `super-2340ae080a5b`（10 阶段 `[skip]`，沿用） |
| `vda.img` | — | 13.00 GiB | `vda-79ec3f6cf770`，17:33 |

### 7.4 静态验证（装配树 + 设备侧，实例 42）

| 判据 | 期望 | 实测 |
| :--- | :--- | :--- |
| `/vendor/etc/*_config.xml` | 8 个自研 + 1 个底座 `audio_effects_config.xml` | 9 个 |
| `/vendor/etc/cdh_*`、`/vendor/etc/init/cdh_*` | 0 / 0 | 0 / 0 |
| 设备 CIL `cdh_` | 0 | 0 |
| 设备 `vendor_file_contexts` `cdh_` | 0 | 0 |
| 设备 CIL `wifi_hvc_data` / `wifi_scan_param` | 7 / 8 | 7 / 8 |
| 引用 `_config.xml` 的源码/rc/xml/README | — | 27 个文件，`CDH_*_XML` 残留 0 |

### 7.5 运行期验证（实例 42，开机后 ≈3 min）

```
vendor.service-daemon    running
vendor.gnss-vhal         running
vendor.wifi-vhal         running
vendor.radio-vhal        running
vendor.camera-vhal       running
vendor.perf2-default     running

getprop | grep -c cdh           = 0
ls -Z /dev/hvc10                = u:object_r:wifi_hvc_data:s0 /dev/hvc10
ls -Z …/mac80211_hwsim/parameters/scan_profile
                                = u:object_r:wifi_scan_param:s0 …
```

**配置真被读**（不是「文件在位」的间接推断）：

| HAL | 证据 |
| :--- | :--- |
| thermal | `dumpsys thermalservice` 报 8 个温区 `CPU/GPU/NPU/SOC/TPU/SKIN/BATTERY/POWER_AMPLIFIER`，与 `thermal_config.xml` 的 `name=` 逐条一致；`HAL Ready: true` |
| battery | `dumpsys battery` 报 `voltage: 4388`、`temperature: 290`（取自 XML 的假值，非真机） |
| sensors | `dumpsys sensorservice` = `Total 29 h/w sensors`，与 `sensors_config.xml` 的 29 条条目一致 |
| wifi | `ls -Z` 的两个标签 = 新类型名，且服务 `running`（配置读失败会起不来） |

### 7.6 avc 统计与残留

记录级口径同 §5.4。实例 42 快照
（`out/core/daemon/runtime/m22_12.5.0.0A/42/qemu.log`，4,347,391 B / 29,410 行 /
sha256 `002badad…` / kernel ts 0–约 60 s）：

```
结构完整 avc 记录 731 (ker 81 / aud 722)
10 个新名命中 0；含 perf2 的记录 0；含 cdh_ 的记录 0
含 cdh 的记录 5 = 3 个唯一事件（audit 100/101/103）双通道：
  service_daemon 建 /data/vendor/cdh 目录 + 写 /data/vendor/cdh/boot_identity
  scontext=u:r:pioneer:s0 permissive=1
Compiling SELinux policy 1 | InitFatalReboot 0 | Failed to resolve 0
duplicate definition of service 0
```

对照实例 41（同一批改动的上一轮镜像，快照 12,043,373 B / 83,789 行 /
sha256 `4e73cdb2…`）：4651 条 avc 里 10 个新名命中 **2** 条，是**我自己**跑
`ls -Z /dev/hvc10` 留下的 `getattr`（`comm="ls"`、pid 无、`permissive=1`）；
`cdh` 相关 6 条与上面同源（`/data/vendor/cdh/boot_identity`）。

判读：改名、改文件名、改服务名都没有引入任何功能性拒绝；唯一与新名字相关的 avc
是验证动作自身造成的自伤（pioneer 是 permissive 域，只记不拦）。

### 7.7 订正与剩余特征

1. **订正 `cdh_redirect`**：§6.3 早前写成「运行期属性」是错的。`getprop | grep -c
   cdh_redirect` = 0；它是内核 printk 前缀/函数名（`cdh_redirect_vendor_path()`，
   `CONFIG_CDH_PATH_REDIRECT`），只在串口/`dmesg` 里（实例 41 日志 614 次、实例 42
   日志 185 次）。下文一律按「内核侧 printk」计。
2. **运行期数据目录 `/data/vendor/cdh/boot_identity`**（7.6 那 3 个事件的来源）：
   `engine/vendor/hardware/android.service.daemon/src/rust/src/boardid.rs` 的
   `CACHE_DIR`/`CACHE_FILE`（与 `core/guest/daemon/rust/src/boardid.rs` 同源双份）。
   它不进镜像，但 `ls /data/vendor`、`dmesg` 都能看到；改名的代价是已缓存 identity
   失效后**重新生成**（板级身份值变化），属身份语义变更，需单独评估后动，本轮未动。
3. **内核侧**（`devices/flyme/m22` 的 kernel/boot 产物，不在 vendor 分区）：
   模块文件名 `cdh_{media,device_info,goodix_fingerprint,input_identity}.ko` 与
   `modules.load`；`vermagic 6.12.81-4k-cdh`（`uname -r` 直接可见）；`cdh_redirect:`
   printk；`SELinux: cdh: pioneer (type 2605) forced permissive`（security-env patch
   打的一条，每次开机 1 行）；线程名 `cdh-wifi-tx/rx`。
4. **二进制符号与日志 tag**：`namespace cdh::audio`、`cdh_start_main`、log tag
   `cdh_audio` / `cdh_wifi` / `cdh_health`（`logcat -s cdh_wifi` 就能筛出来）。改名
   要动源码与日志筛选习惯，风险与收益比最差，留待单独决策。
5. **策略注释**：设备 CIL 仍有 **39** 行含 `cdh` —— 23 行 marker（`cdh-patch-*`）、
   3 行基线改写规则写入的 `;; cdh: …`（`devices/manifest/flyme/m22/baselines/12.5.0.0A.json`
   的 patch 规则）、8 行模块片段头 `;; cdh：…`（07 写的）、5 行其它说明
   （`cdh-wifi-tx/rx`、`lineage + cdh device 树` 等）。另三个 contexts 文件：
   `vendor_service_contexts` 8 行（7 marker + 1 注释）、`vendor_file_contexts` 16 行
   （12 marker + 4 注释）、`vendor_property_contexts` 2 行（2 marker）。**全部是注释**，
   不影响任何标签，但 `strings vendor.img | grep cdh` 仍能命中。
6. **marker 锚点**：`cdh-patch-begin/end`（07 常量，`engine/build/07_build_vendor/sepolicy.go:30-31`）
   与 11 写进 grub.cfg 的 `# CDH:`（`engine/build/11_pack_vda/grub.go`）。改它们要
   同步改常量与幂等判据（`strings.Contains(marker)`），否则会把已有装配树/grub 判成
   「非我注入」而重复写入或改坏，属独立改动。
7. **`cph_*` 文件名族**：`cph_daemon_service.rc`、`cph_logcat_dump_early.rc`。不是 `cdh`
   但同属内部代号；部分基线 JSON 以 `etc/init/cph_*.rc` 为键，改名要连基线一起改。
8. **本仓内的旧名残留**（不影响产物）：`devices/flyme/m22/build/vendor/build/sepolicy.py`
   15 处 `cdh_*_service`（v1 链路，engine 未引用）；`devices/evidence/*` 历史日志里的
   旧文件名（属证据原文，不回改）；host 侧程序名 `cdh`（不进镜像）。
