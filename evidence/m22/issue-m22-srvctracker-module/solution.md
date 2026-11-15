# servicetracker 收成 prebuilt 模块：机型层最后一条「有实体」的 vendor HAL 标签下放

> 2026-10-29，m22（MEIZU 22 / Flyme 12.6 / Android 16 / qcom `sun`）。
> 起因是用户追问「`sepolicy` 里剩下的还能不能下放到 HAL」——上一轮把 gnss / sensor_aux /
> exec 标签搬走时，判定这 6 条 vendor 服务名「vendor 侧没有实现可挂」。复核原厂
> vendor 镜像后发现：**实现是有的，只是没被引擎收进来**。本轮把其中最小的
> `vendor.qti.hardware.servicetrackeraidl` 收成模块（新增 `kind=prebuilt` 通道），
> 它的域、exec 标签、服务名标签全部随模块走。
> 产物：`vendor.img` `f50893a7c39e916b277a5e2f02f90d23c13a43234ac654249290f852ea08392e`
> （206,073,856 B；末次重跑于 22:42，因修正机型层 service_contexts 注释 +3 行；
> 注释修正前的同构产物为 206,069,760 B / `b592f0be…`。sha 每次不同见 5.4）。

## 1. 问题

机型层 `manifest/flyme/m22/sepolicy/` 的段 1) 声明了 5 个 Flyme 会查询的 vendor HAL
服务名，但本云 vendor 里一个都没装：

```
vendor.qti.hardware.servicetrackeraidl.IServicetracker/default
vendor.meizu.hardware.meizu_charging.IMeizuCharging/default
vendor.meizu.hardware.aw_light.IAwLight/default
vendor.awa.reality.IRealityTap/default
vendor.qti.diaghal.Idiag/default
```

上一轮的结论是「vendor 侧无实现 → 只能空挂标签，留机型层」。用户追问后复核原厂
`out/tmp/unpack-vendor_a/flyme-m22/`，事实相反：

- `bin/hw/` 有 4 个二进制（`vendor.awa.realitytap.service`、`vendor.meizu.hardware.aw_light-service`、
  `vendor.meizu.hardware.meizu_charging-service`、`vendor.qti.hardware.servicetrackeraidl-service`）；
- `lib64/` 有对应 `*-V1-ndk.so` / `*-impl.so` / `@1.x.so`；
- `etc/init/*.rc` 与 `etc/vintf/manifest/*.xml` 齐全；
- 原厂 `etc/selinux/vendor_sepolicy.cil`（13,208 行）里有约 235 行域策略
  （aw_light 69 / meizu_charging 65 / realitytap 59 / diaghal 33 / servicetrackeraidl 9）。

所以「不能下放」不是归属问题，而是**宿主实现没被引擎收进来**；没有模块，sepolicy
片段就没有落点（`sepolicySources()` 只遍历 `device.json` 声明的模块）。

servicetracker 是这批里最小的一个：策略 9 行、无设备节点、纯 AIDL 客户端只有
system_server，适合做流程试点。

## 2. 根因

### 2.1 引擎只有「编译型」模块，没有「厂商预编译」通道

`vendor/build/internal/spec/spec.go` 的 kind 白名单是 `aidl / pure-c / v412 / gpu / cargo`，
未知 kind 直接报错；`halbuild.Build` 的每条分支都要先 `toolchain.Discover`（要 NDK）。
厂商私有 HAL 只有二进制、没有源码，按现有通道无法接入。

### 2.2 07 的产物判据只认编译产物，不认识「静态树即产物」

`moduleTargets` 按 kind 推 `bin/hw/<service>` 之类的期望产物，`len(Targets)==0` 时
`buildModules` 会在日志里报「产物清单无法判定」并照常调 `cdh-vendor hal build`。
预编译模块的全部文件都在 `vendor/**` 静态树里，需要显式清单才能参与「产物齐全」判据。

### 2.3 原厂策略不能照抄：版本与 attribute 都是机型/全量策略的编排

原厂域 `vendor_hal_srvctracker_default` 挂在 `protected_service_202404` /
`protected_hwservice_202404` / `halserverdomain` 等大 attribute 行里，且平台域是
`init_202404`。本云底座 `plat_sepolicy_vers.txt = 202504`，直接搬会
`Failed to resolve`（#58 的教训）。模块层片段必须用 `{{vers}}`，并舍弃 Flyme
全量策略的 attribute 编排，只留最小授权。

### 2.4 原厂 rc 没有 seclabel

4 个原厂 rc 都只有 `class/user/group/interface`，域转换靠 file_contexts 的
`vendor_hal_srvctracker_default_exec`。收进模块后若不给 rc 补 `seclabel`，07 步骤 6b
扫不到域、不会派生 exec 标签（与本轮 health 的退化路径同构）。

## 3. 证据

### 3.1 原厂资产与依赖闭包

```
$ ls -l flyme-m22/bin/hw/vendor.qti.hardware.servicetrackeraidl-service \
        flyme-m22/lib64/hw/*servicetrackeraidl-impl.so flyme-m22/lib64/*servicetrackeraidl-V1-ndk.so
 52080  bin/hw/...-service
 85584  lib64/hw/...-impl.so
102544  lib64/...-V1-ndk.so

$ llvm-readelf -d bin/hw/...-service | grep NEEDED
liblog libbase libhardware libutils libbinder_ndk
vendor.qti.hardware.servicetrackeraidl-V1-ndk.so
vendor.qti.hardware.servicetrackeraidl-impl.so
libc++ libc libm libdl
```

依赖全部在底座 `vendor/lib64`（libbase / libhardware / libutils / libcutils 均在），
不需要把 lineage 的 ABI 库再捎一份。

### 3.2 原厂策略的实际内容（9 行 + 4 个 attribute 行）

```
$ grep -n "srvctracker" flyme-m22/etc/selinux/vendor_sepolicy.cil
831:(typeattributeset vendor_hal_srvctracker (vendor_hal_srvctracker_default))
832:(typeattributeset vendor_hal_srvctracker_client (system_server_202404 vendor_hal_perf_default))
833:(typeattributeset vendor_hal_srvctracker_server (vendor_hal_srvctracker_default))
2823-2826:(type/roletype vendor_hal_srvctracker_default[_exec])
2887:(type vendor_hal_srvctracker_hwservice)
9608-9611:(allow init_202404 …exec…) (allow init_202404 …transition…) (allow …entrypoint…) (dontaudit …)
```

### 3.3 原厂标签名与本轮采用的一致

```
$ grep -n servicetrackeraidl flyme-m22/etc/selinux/{vendor_service_contexts,vendor_file_contexts}
vendor_service_contexts:89: vendor.qti.hardware.servicetrackeraidl.IServicetracker/default u:object_r:vendor_hal_servicetrackeraidl_service:s0
vendor_file_contexts:301: /(vendor|system/vendor)/bin/hw/vendor\.qti\.hardware\.servicetrackeraidl-service \
                             u:object_r:vendor_hal_srvctracker_default_exec:s0
```

exec 类型名沿用原厂 `vendor_hal_srvctracker_default_exec`；服务类型名取
`vendor_hal_srvctracker_service`（与 exec 同前缀）。注意原厂的服务类型名是
`vendor_hal_servicetrackeraidl_service`——类型名只是自定义标签，不影响功能，若要
与原厂 avc 逐字对照可改名。

## 4. 改动

| # | 文件 | 动作 |
| --- | --- | --- |
| 1 | `vendor/build/internal/spec/spec.go` | 新增 `kind=prebuilt`：`PrebuiltFiles` 字段、校验（必须非空）、工作区名 `<x>-prebuilt-work` |
| 2 | `vendor/build/internal/halbuild/build.go` | `prebuilt` 在 `toolchain.Discover` **之前**短路，只执行 `publishVendor()`（不找 NDK） |
| 3 | `build/07_build_vendor/hal.go` | `moduleTargets` 读 `prebuiltFiles` 生成期望产物；新增 `prebuiltFiles()` helper |
| 4 | `vendor/hardware/vendor.qti.hardware.servicetrackeraidl/vendor/` | **新建**：原厂 `bin/hw` 二进制、`lib64/hw/*-impl.so`、`lib64/*-V1-ndk.so`、`etc/init/*.rc`、`etc/vintf/manifest/*.xml` |
| 5 | 同上 `vendor/etc/init/…-service.rc` | 加 `seclabel u:r:vendor_hal_srvctracker_default:s0`（原厂无），注释说明来源 |
| 6 | 同上 `sepolicy/vendor_sepolicy.cil` | **新建**：域 + exec + init 启动链 + AIDL 注册/查询 + 双向 binder，全部 `{{vers}}` |
| 7 | 同上 `sepolicy/vendor_service_contexts` | **新建**：`…IServicetracker/default` → `vendor_hal_srvctracker_service` |
| 8 | `vendor/build/modules/vendor.qti.hardware.servicetrackeraidl.json` | **新建** spec：`kind=prebuilt` + 3 项 `prebuiltFiles` |
| 9 | `manifest/flyme/m22/device.json` | hardware 31 → **32 项**，声明新模块 |
| 10 | `manifest/flyme/m22/sepolicy/vendor_sepolicy.cil` | 删 `hal_servicetracker_service` 的三行类型 + 一行 `allow system_server find`；头注释 5→4、分工段加 servicetracker |
| 11 | `manifest/flyme/m22/sepolicy/vendor_service_contexts` | 删 servicetracker 条目；注释 5→4 |
| 12 | `manifest/flyme/m22/README.md`、`vendor/build/README.md` | 记录第二批下放；模块数 28→29、kind 五种→六种 |

刻意没做：

- 没搬原厂的 `protected_service` / `halserverdomain` / `hwservice` 编排——本云不需要，
  搬了反而引入 202404 的影子类型。
- 没动另外 4 个 HAL（aw_light / meizu_charging / realitytap / diaghal）：它们的策略
  与设备节点标签（`/dev/rgb_led`、`/sys/.../rgb_led/*`、`/dev/awa_realitytap_dev`、
  `/dev/diag`，共 18 行）绑定板级，需要单独一轮。
- 没把模块 VINTF 的 `version="8.0"` 改成 9.0：`alignVintfSchema` 只降不升，8.0 对
  libvintf@9.0 合法（见遗留 4）。
- 没刷新 `patch.img` / `out/dist`（09/11 未跑）。

## 5. 验证

### 5.1 模块与产物

```
$ out/bin/cdh-vendor hal list | grep servicetracker
  vendor.qti.hardware.servicetrackeraidl prebuilt

$ dry-run → HAL 模块: vendor.qti.hardware.servicetrackeraidl（…8 项齐全（编译产物 3 + vendor 静态树 5，源码指纹一致）-> 复用）
```

### 5.2 07 实跑（exit=0）

```
==> [步骤 4] …编译产物 vendor.qti.hardware.servicetrackeraidl@default: 写入 5
==> [步骤 6] 注入 SELinux 策略补丁
    输入[模块层 vendor.qti.hardware.servicetrackeraidl]: vendor/hardware/…/sepolicy
    追加 …/vendor_sepolicy.cil <- …/sepolicy/vendor_sepolicy.cil（2826 字节，marker ;; vendor_sepolicy.cil@vendor.qti.hardware.servicetrackeraidl）
    追加 …/vendor_service_contexts <- …/sepolicy/vendor_service_contexts（551 字节）
[ok] 补充 HAL 可执行标签 27 条
    vendor.qti.hardware.servicetrackeraidl-service -> vendor_hal_srvctracker_default_exec
==> [步骤 7] 装配树条目 660（文件 429 / 链接 199 / 目录 32）
```

对比上一轮 655 → 660（+5，全部来自本模块）。

### 5.3 镜像内标签（debugfs `ea_get -V`）

```
bin/hw/vendor.qti.hardware.servicetrackeraidl-service   u:object_r:vendor_hal_srvctracker_default_exec:s0
lib64/hw/…-impl.so                                      u:object_r:vendor_file:s0
lib64/…-V1-ndk.so                                       u:object_r:vendor_file:s0
etc/init/…-service.rc                                   u:object_r:vendor_configs_file:s0
etc/vintf/manifest/…-service.xml                        u:object_r:vendor_configs_file:s0
```

cil 里 `{{vers}}` 残留 0；`vendor_hal_srvctracker_*` 全部落在模块层 marker 块
（4683~4704 行），机型层为 0。

### 5.4 清空产物根后能重建（预编译模块的关键可靠性）

```
$ mv <5 个产物> /tmp/prebuilt-bak/ → dry-run: 待落盘 8/8 项（编译产物 3 + vendor 静态树 5）-> 重编
$ 07 实跑 exit=0 → 5 个文件回写，与备份逐字节相同（5/5 SAME）
```

**sha 不作为一致性判据**：同样内容连续两次构建得到
`3e5d8e86…` 与 `b592f0be…`（尺寸均为 206,069,760 B）。修正机型层注释后重跑
为 206,073,856 B / `f50893a7…`（多 1 个 4 KiB 块），装配树仍 660、6b 仍 27 条。mke2fs 全量生成镜像时
inode/时间戳有非确定性；判据用「装配树条目 + 逐文件内容 sha + 镜像内 xattr」。

### 5.5 质量门

```
$ cd engine/vendor/build && gofmt -l .        # 只剩既存的 internal/deps/v412abi.go（本轮未碰）
$ go vet ./... && go build -o ../../out/bin/cdh-vendor .      # 通过
$ cd engine/build && gofmt -l 07_build_vendor && go vet ./... && go build ./...   # 通过
```

## 6. 遗留

1. **运行期未复验**：起实例后确认 `service list | grep servicetrackeraidl` 为 running、
   `dmesg | grep "Failed to resolve"` 为 0、`logcat`/`avc` 里没有
   `vendor_hal_srvctracker_default` 的 denied。若服务反复重启，先看 impl.so 是否被
   链接器加载（`ls -Z /vendor/lib64/hw`）。
2. **`kind=prebuilt` 目前只服务这一个模块**：`hal build` 的手动路径、`hal list` 的
   输出格式、以及 `cdh-vendor` 侧没有 `prebuiltFiles` 的存在性校验，都还没沉淀成
   通用能力；再收第二个（aw_light）时一并补。
3. **另外 4 个 HAL 的策略与设备节点标签**：aw_light（69 行 + rgb_led sysfs）、
   meizu_charging（65 行）、realitytap（59 行 + `/dev/awa_realitytap_dev`）、diaghal
   （33 行 + `/dev/diag`）。设备节点/sysfs 路径属板级事实，按 keymint 的分层结论
   应留机型层——收模块也不会让机型层清零。
4. **VINTF 片段仍是 8.0**：07 的 `alignVintfSchema` 只把过高版本降下来，低版本不动
   （8.0 对 libvintf@9.0 合法）。若要求与其余 37 个片段一致，需要改 `alignVintfSchema`
   的升版语义，或让模块片段按基线生成。
5. **`patch.img` 与 `out/dist` 仍是旧版**（09/11 未重跑），其中的
   `/vendor/etc/selinux/*` 不带本模块的块。
