# m22 机型层 SELinux 片段下放：gnss / sensor_aux 进模块层，exec 标签交给 rc seclabel

> 2026-10-29，m22（MEIZU 22 / Flyme 12.6 / Android 16 / qcom `sun`）。
> 起因是「`manifest/flyme/m22/sepolicy` 里的是否可以下放到具体的 HAL 里」——
> 逐条核对归属后，凡是「有模块实体、规则与机型无关」的都搬走，搬不了的写清为什么。
> 产物：`vendor.img` `6e20f34c239dad2f574f3f594cef7b6ce7124c3e0ba79bf7086e41b111132c6f`（205,877,248 B）。

## 1. 问题

机型层 `devices/manifest/flyme/m22/sepolicy/` 有三份文件、约 12 KB：

| 文件 | 大小 | 内容 |
| --- | --- | --- |
| `vendor_sepolicy.cil` | 7,996 B | ① gnss 读 `/data/vendor/gps/fix`；② Flyme 查询的 5 个 vendor HAL 服务名 + `realitytap`；③ perfservice 读 hwservicemanager 属性；④ sensor_aux 服务名 + 桩授权；⑤ `allow init vendor_file execute`；⑥ FRP misc 块设备 |
| `vendor_service_contexts` | 1,862 B | 上述 6 个服务名 + sensor_aux 的标签（共 7 条） |
| `vendor_file_contexts` | 2,527 B | 8 条 `bin/hw` 可执行文件标签（health / radio / lights / gnss / thermal / vibrator / media c2 / dolby dax） |

问题不是「能不能跑」，而是**契约放错了层**：这些规则里有相当一部分属于 HAL 自己
（换机型不该重写），却全部写在只服务 m22 的机型层；其中 8 条 exec 标签更是与
07 步骤 6b 已有的「从 rc 的 `seclabel` 反推 `<域>_exec`」机制重复。

判据（`build/07_build_vendor/sepolicy.go` 头注释既定）：模块层放 HAL 自身契约、
机型层放只属于本机的东西。

## 2. 根因

### 2.1 exec 标签的正确出口是 rc 的 `seclabel`，不是手写 file_contexts

步骤 6b（`halexec.go`）会扫 `etc/init/*.rc` 的 `service … seclabel u:r:<域>:s0`，
推出 `u:object_r:<域>_exec:s0` 并补进 `vendor_file_contexts`（镜像 xattr 与运行态
restorecon 同源）。8 个目标二进制里 7 个的 rc 本来就有 `seclabel`（`hal_radio_default`、
`hal_light_default`、`hal_gnss_default`、`hal_thermal_default`、`hal_vibrator_default`、
`mediacodec` ×2），手写规则只是 6b 之前的历史兜底，**6b 看到规则已存在就跳过**——
两份东西描述同一件事。

### 2.2 唯一的例外是 health：它的 rc 没有 `seclabel`

`vendor/hardware/android.hardware.health/vendor/etc/init/android.hardware.health-service.rc`
只有 `class/user/group/capabilities`，域转换靠 exec 文件标签 + `typetransition`。
所以「整份 file_contexts 删掉」的前提，是给这个 rc 补一行 `seclabel`。

### 2.3 gnss 的 `/data/vendor/gps/fix` 是模块契约

目录由该模块 rc 自己 `mkdir /data/vendor/gps 0770 system shell` 建立，读它的是该
模块 HAL（写入即强制下发一次 fix）。与机型、板级无关；gold 也声明同一个
`android.hardware.gnss` 模块，但它**没有**这条规则（同样的 rc、同样的读文件），
说明这条本来就该在模块层——搬过去 gold 自动补齐。

### 2.4 sensor_aux 现在有实体了，机型层那份是「无实体时代」的补丁

机型层注释、README「缺口 3」都写着「引擎侧没有这个桩模块，实体仍缺」。实际
`vendor/build/modules/vendor.meizu.sensor_aux.json` + `vendor/hardware/vendor.meizu.sensor_aux/`
（AIDL + `sensor_aux_service.cpp` + rc）早已存在，`out/vendor/android16/bin/hw/`
里有 `vendor.meizu.sensor_aux-service`，`device.json` 也声明了。所以它和 #58 的
perf2 完全同构，该按同一口径归位。

### 2.5 剩下的三段确实没有归属

- 5 个 vendor HAL 服务名 + `realitytap`：Flyme framework 的查询名单，本云 vendor
  装配树里没有实现可挂；gold 是 MIUI 名单，两边完全不同 → 机型专属。
  > **2026-10-29 复核修正**：这条只管本云（lineage-23.2 底座）。原厂 flyme vendor
  > 里这 5 个 HAL 全都有实体——`out/tmp/unpack-vendor_a/flyme-m22/` 下
  > `bin/hw/` 有 `vendor.awa.realitytap.service`、`vendor.meizu.hardware.aw_light-service`、
  > `vendor.meizu.hardware.meizu_charging-service`、`vendor.qti.hardware.servicetrackeraidl-service`，
  > 配套 `lib64/*-V1-ndk.so`、`etc/init/*.rc`、`etc/vintf/manifest/*.xml`；原厂
  > `etc/selinux/vendor_sepolicy.cil`（13,208 行）里有约 235 行域策略
  > （aw_light 69 / meizu_charging 65 / realitytap 59 / diaghal 33 / servicetrackeraidl 9）。
  > 所以它们不是「无归属」，而是「宿主实现尚未收进引擎」：可下放，前提是给每个
  > HAL 建模块（spec + 静态树 + sepolicy），标签与域策略随模块走；模块存在前，
  > 机型层这 6 条只是空挂标签（维持 framework 按不可用降级、不出 avc 噪音）。
  > 例外：`realitytap` 由 system_server 的 RealityTapManagerService 发布，宿主是
  > framework，永远不能随 vendor HAL 模块走。
- perfservice 读 `hwservicemanager.disabled`：主体是 Flyme 的 `vendor_perfservice`
  （system_ext 域），不是 HAL。
- FRP misc 块设备：绑本机 GPT（本盘无 frp 分区）+ 机型静态树的 `frp_symlink.rc`。

## 3. 证据

### 3.1 改前：手写规则与 6b 的产出重复

```
$ grep -n "cdh-patch-begin" out/devices/.../vendor/etc/selinux/vendor_file_contexts
6          # 5 个模块层 + 1 个机型层（机型层就是手写的 8 条）
$ sed -n '380,398p' …/vendor_file_contexts   # 机型层块（含 health/radio/lights/gnss/thermal/vibrator/c2/dolby 8 条）
```

### 3.2 7 个 `_exec` 类型都能被 6b 认出来（`definedPolicyTypes` 只认文本）

```
$ grep -n "(type hal_health_default_exec)" …/vendor_sepolicy.cil    → 389
$ grep -n "(type hal_radio_default_exec)"  …/vendor_sepolicy.cil    → 469
$ grep -n "(type hal_light_default_exec)"  …/vendor_sepolicy.cil    → 429
$ grep -n "(type hal_gnss_default_exec)"   …/vendor_sepolicy.cil    → 375
$ grep -n "(type hal_thermal_default_exec)" …/vendor_sepolicy.cil   → 499
$ grep -n "(type hal_vibrator_default_exec)" …/vendor_sepolicy.cil  → 547
$ grep -n "(type mediacodec_exec)"         …/vendor_sepolicy.cil    → 575
```

### 3.3 8 个 rc 的 seclabel 现状（health 缺，其余齐全）

```
android.hardware.radio-service                  seclabel u:r:hal_radio_default:s0
android.hardware.lights-service                 seclabel u:r:hal_light_default:s0
android.hardware.gnss-service                   seclabel u:r:hal_gnss_default:s0
android.hardware.thermal-service                seclabel u:r:hal_thermal_default:s0
android.hardware.vibrator-service               seclabel u:r:hal_vibrator_default:s0
android.hardware.media.c2-mediatek              seclabel u:r:mediacodec:s0
vendor.dolby.media.c2-default-service-dax       seclabel u:r:mediacodec:s0
android.hardware.health-service                 （无 seclabel）
```

### 3.4 模块层片段只在 device.json 声明该模块时生效

`sepolicySources()` 遍历 `plan.Modules`（= `device.json` 的 `hardware`）再追加机型层；
marker 对模块层追加 `@<模块名>`，两层同名片段不会互相整块替换。dry-run 的
`SELinux 补丁:` 行改前 15 个来源、改后仍是 15 个（gnss、sensor_aux 从「机型层内部」
变成独立来源，机型层文件数 3 → 2）。

## 4. 改动

| # | 文件 | 动作 |
| --- | --- | --- |
| 1 | `vendor/hardware/android.hardware.gnss/sepolicy/vendor_sepolicy.cil` | **新建**：原机型层①（两条 `hal_gnss_default vendor_data_file_{{vers}}`）搬入 |
| 2 | `vendor/hardware/vendor.meizu.sensor_aux/sepolicy/vendor_sepolicy.cil` | **新建**：原机型层④（`hal_sensor_aux_service` 类型 + `system_server` find + `hal_sensors_default` add/find） |
| 3 | `vendor/hardware/vendor.meizu.sensor_aux/sepolicy/vendor_service_contexts` | **新建**：`vendor.meizu.sensor_aux.ISensorAux/default` 标签 |
| 4 | `vendor/hardware/vendor.meizu.sensor_aux/vendor/etc/init/vendor.meizu.sensor_aux-service.rc` | 注释「机型层需已授…」→「由模块根目录的 sepolicy/ 给出」 |
| 5 | `vendor/hardware/android.hardware.health/vendor/etc/init/android.hardware.health-service.rc` | 加 `seclabel u:r:hal_health_default:s0`（让 6b 能派生 `hal_health_default_exec`） |
| 6 | `manifest/flyme/m22/sepolicy/vendor_file_contexts` | **整份删除**（8 条 exec 标签交由 6b） |
| 7 | `manifest/flyme/m22/sepolicy/vendor_sepolicy.cil` | 删①④两段并重排编号（2→1、3→2、5→3、6→4）；头注释改写；顺手修两处排版 |
| 8 | `manifest/flyme/m22/sepolicy/vendor_service_contexts` | 删 sensor_aux 一条 + 注释；「第 2/4 段」→「第 1 段」 |
| 9 | `manifest/flyme/m22/README.md` | sepolicy 小节整节改写（下放结论 + 模块层清单 + 两份文件新内容）；缺口 3 标已解决 |

刻意没做：

- 没给任何模块新增 `vendor_file_contexts`。exec 标签一律走「rc `seclabel` → 6b 派生」，
  避免同一件事两种机制（模块层已有 ir / power / wifi / daemon / media.v412 各带一份
  是历史形态，不再加新的）。
- 没删机型层段⑤ `(allow init vendor_file (file (execute)))`。它是权限放大，但删它要
  先确认底座是否对 `*_exec` 给了 init `execute`——只做构建层验证不够，留待实机。
- 没动 `patch.img`（09 未重跑）与 `out/dist`。

## 5. 验证

| 项 | 改前 | 改后 |
| --- | --- | --- |
| 机型层文件数 | 3（cil / service_contexts / file_contexts） | **2**（cil 6,925 B / service_contexts 1,545 B） |
| 模块层来源数 | 13 | **15**（+gnss +sensor_aux） |
| `vendor_file_contexts` 的 marker | 6（含机型层手写块） | **5**（全为模块层） |
| 步骤 6b 补充 exec 标签 | 手写 8 条 + 6b 其余（手写规则挡在前面） | **26 条一次补齐**（含 health / lights / gnss / radio / thermal / vibrator / c2 / dolby） |
| `{{vers}}` 残留 | 0 | 0 |
| `hal_sensor_aux_service` 在 cil 里 | 机型层 5 处 | **模块层 5 处**（机型层 0） |
| gnss fix 规则 | 机型层 marker 块内 | **模块层 marker 块内**（`vendor_data_file_202504`） |
| 装配树条目 | 656（425/199/32） | **655（424/199/32）**，−1 是 `init.virtio.rc` 不再保留（机型静态树已提供 `init.qcom.rc`，见步骤 6 fstab 收口行），与本次 sepolicy 改动无关 |
| `vendor.img` | `94df88de3a6acb…` / 205,877,248 B | **`6e20f34c239dad2f…` / 205,877,248 B** |
| 镜像内标签回读 | health/lights/c2/dolby 正确 | health `hal_health_default_exec`、lights `hal_light_default_exec`、c2-mediatek 与 dolby `mediacodec_exec`、sensor_aux `hal_sensors_default_exec` |
| VINTF / 元数据 / fsck | 37 片段 0 待改写 / 655 条 | 同（本轮不涉及） |

构建：`gofmt -l` 干净、`go vet ./...`、`go build ./...` 通过（本轮未改 Go 代码，仅
资产与文档，另跑一次确认无回归）。

## 6. 遗留

1. **运行期未复验**：本轮只到构建层。下次起 m22 实例要确认：`ls -Z /vendor/bin/hw/`
   里 8 个二进制仍是 `*_exec`；`vendor.meizu.sensor_aux` 服务 `running` 且
   `service check` 找得到名字；gnss 读 `/data/vendor/gps/fix` 无 `denied`；
   `dmesg | grep "Failed to resolve"` 为 0（secilc 合并成功）。
2. **段⑤可删性待评估**：8 条标签正确后，`allow init vendor_file (file (execute))`
   理论上多余。删之前要先在实例上确认 `avc { execute }` 仍为 0（底座对
   `*_exec` 的 execute 授权口径）。
3. **gold 会跟着变**：`android.hardware.gnss` 模块新增了 gps/fix 读授权，gold 下次
   构建 vendor 时会带上（预期是补齐缺失，但要在 gold 实例上看一眼 `denied`）。
4. **`patch.img` 与 `out/dist` 仍是旧版**（09/11 未重跑）；`patch.img` 里的
   `/vendor/etc/selinux/*` 会带着旧机型层块，需重跑 09 刷新。
5. **scenario「模块层 vendor_file_contexts」仍未收敛**：ir / power / wifi / daemon /
   media.v412 各带一份 exec 标签片段，与 6b 的派生机制并存。要不要也收敛成 rc
   seclabel，另立条目评估。
