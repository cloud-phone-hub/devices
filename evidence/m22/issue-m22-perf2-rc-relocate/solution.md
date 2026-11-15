# issue-m22-perf2-rc-relocate — perf2 的 rc 从机型静态树归位到模块

- 设备：`flyme/m22`（Android 16 / lineage-23.2 基底）
- 日期：2026-10-29
- 关联：#58（perf2 find 授权）、#59（服务类型名去品牌化）、《HAL 模块 rc 与机型静态树 rc 合并的权衡决策》

## 1. 问题

`vendor.qti.hardware.perf2` 的服务定义 `vendor/etc/init/vendor.qti.hardware.perf2-hal-service.rc`
一直放在机型静态树 `devices/manifest/flyme/m22/vendor/`，而模块目录
`engine/vendor/hardware/vendor.qti.hardware.perf2/` 只有 `aidl/ sepolicy/ src/`，没有 `vendor/`。

两处不一致：

- 仓库 39 份模块 rc 里，它是唯一「模块有实现、rc 却在机型层」的例外；
- `vendor/build/README.md` 的切分口径写明「`etc/init/*.rc`（服务定义）属设备无关，放模块 `vendor/`」。

后果不只是形式问题：任何新机型只要在 `device.json` 声明该模块，07 步骤 4 会复制到二进制却没有
rc，服务静默不启动——正是 #58 里那类「实例缺失 → 383 条 Could not find」的缺口复现路径。

## 2. 根因

历史决策的理由是「接口实现归模块，『在哪些机型上、以什么实例名注册』归设备集成层」。这个理由在
当前管线下不成立：

- **上不上机由 `device.json` 控制**，不由机型静态树有没有 rc 控制。07 步骤 4 只复制声明模块的
  `Targets` + `VendorTree`（`assemble.go`），未声明 perf2 的机型（gold）根本拿不到这份 rc；
- **这份 rc 没有任何机型数值**：无通道号、无设备节点、实例名 `vendor.perf2-default` 与二进制
  `vendor.qti.hardware.perf2-hal-service` 都是引擎侧约定，`seclabel` 复用的是底座标准域
  `hal_sensors_default`（本仓虚拟 HAL 统一约定）；
- 分离反而制造了「两份来源」风险：模块若将来补了同名 rc，就与静态树副本撞车，后层覆盖且难追溯。

## 3. 证据

- 改动前 `find vendor/hardware -path "*/vendor/etc/init/*.rc"` 共 38 个，不含 perf2；perf2 模块
  目录无 `vendor/`（见 `log-fail.txt` 第一、二节）；
- 旧 rc 自述「为什么由机型层提供而不是模块自带」，但其理由是「在哪些机型上注册」——该职责已由
  `device.json` 声明承担（`log-fail.txt` 第四节）；
- `manifest/flyme/m22/device.json` 是唯一声明方（gold 无 perf2 静态树、无声明）；
- 07 复用判据已把模块 vendor 静态树算进待落盘项：本次重编日志打印
  `待落盘 1/2 项（编译产物 1 + vendor 静态树 1）-> 重编`（`log-pass.txt` 第二节）。

## 4. 改动

1. 新增 `engine/vendor/hardware/vendor.qti.hardware.perf2/vendor/etc/init/vendor.qti.hardware.perf2-hal-service.rc`：
   **服务定义、`seclabel`、`interface`、`disabled`、`on post-fs-data start` 逐行照搬**，只重写头部注释
   ——说明「为什么归模块」（无机型数值 + 上机由 `device.json` 控制），保留原有三条理由
   （必须补实体、disabled + 显式 start、不注入 VINTF 片段）。
2. 删除机型静态树 `devices/manifest/flyme/m22/vendor/etc/init/vendor.qti.hardware.perf2-hal-service.rc`。
   静态树 `etc/init` 仅剩设备相关的 `cph_logcat_dump_early.rc`、`frp_symlink.rc` 与板级入口
   `hw/init.qcom.rc`。
3. `engine/vendor/build/README.md`：切分口径表 `etc/init/*.rc` 一行改为「各机型声明模块均已就位
   （gold 的 17 个 + 仅 m22 声明的 `vendor.qti.hardware.perf2`）」，表后补一段「『在哪些机型上启动』
   不靠机型 rc 门控」。
4. 未改：模块 `vendor.qti.hardware.perf2.json`（`publishVendor` 遍历 `vendor/**` 全树，无需登记）、
   `sepolicy/` 片段、`device.json` 声明。

## 5. 验证

| 项 | 结果 |
| --- | --- |
| 07 自动重编 | `vendor.qti.hardware.perf2` 判为「待落盘 1/2 项 → 重编」，`写 etc/init/vendor.qti.hardware.perf2-hal-service.rc（0644）`，`合并 vendor 静态树：新增 1` |
| 三层落位 | 产物根 `out/vendor/android16/etc/init/…rc`、装配树 `<版本目录>/vendor/etc/init/…rc`、镜像内 `/etc/init/…rc` 均为 2,296 B |
| 服务定义不变 | `service vendor.perf2-default /vendor/bin/hw/vendor.qti.hardware.perf2-hal-service`、`seclabel u:r:hal_sensors_default:s0`、`interface aidl …IPerf/default`、`disabled`、`on post-fs-data start` 与改动前逐行一致 |
| 标签 | rc = `u:object_r:vendor_configs_file:s0`（底座规则），二进制 = `u:object_r:hal_sensors_default_exec:s0`（步骤 6b 按 rc 的 seclabel 自动补，日志 `vendor.qti.hardware.perf2-hal-service -> hal_sensors_default_exec`） |
| 模块层 SELinux | `sepolicy/vendor_sepolicy.cil`(2318 B) 与 `vendor_service_contexts`(636 B) 照旧追加 |
| 其它模块无回归 | `android.service.daemon`、`vendor.meizu.sensor_aux` 仍 `[skip]`（复用判据未误伤），31 个模块产物全量重写 |
| 产物 | `vendor.img` 205,897,728 B，sha256 `1cc87515c5…`（体积与上一版相同，差异只在 rc 注释） |

## 6. 遗留

- **运行期未起实例复验**：本次只动 rc 注释与文件归属，服务名/二进制/域/interface 全部未变，
  风险低；下次起实例时确认 `init.svc.vendor.perf2-default=running` 即可闭环。
- 若将来某机型需要**不同的域或实例名**，模块 rc 无法按机型参数化。届时正确做法是给 `device.json`
  加配置项、由构建模板渲染 rc（或保留机型覆盖的合法例外），**不要**把 rc 拆回机型静态树默认存两份。
- 历史证据 `issue-m22-hal-gap-fill/log-pass.txt`、`issue-m22-hardware-list-align/solution.md` 里
  「机型层给 perf2 一个实体」的描述是当时的真实状态，保留不改；以本条为最新分层结论。
- `out/dist/flyme/m22/12.5.0.0A` 未重发，仍是 17:33 那版（不含本次 `vendor.img`）。
