# m22 新 HAL 二进制缺 SELinux 可执行标签（entrypoint 被拒 → 服务 status 127）

- 设备：flyme/m22（MEIZU 22，QTI `sun`），实例 33 / 34
- 日期：2026-10-29
- 关联：[issue-m22-hal-gap-fill](../issue-m22-hal-gap-fill/solution.md)（#53）、
  [issue-m22-bootctl-vold-deadlock](../issue-m22-bootctl-vold-deadlock/solution.md)（#54）

## 1. 问题

补完 HAL 并撤掉 boot 之后（实例 33），开机仍不完成。串口里 13 个新 HAL 服务每秒死一次：

```
init: Service 'vendor.memtrack-default' (pid 817) exited with status 127
init: Service 'vendor.usb-gadget' (pid 815) exited with status 127
avc: denied { entrypoint } for comm="init"
     path="/mnt/patch/vendor/bin/hw/android.hardware.memtrack-service"
     scontext=u:r:hal_memtrack_default:s0 tcontext=u:object_r:vendor_file:s0 tclass=file permissive=0
```

累计：`exited with status 127` 1773 次，`vendor_file` 上的 entrypoint 拒绝 8784 次。底座自带的老
HAL（`android.hardware.sensors-service` 等）不受影响——它们的二进制在 basal `vendor_file_contexts`
里本来就有规则。

## 2. 根因

init 在 `execve` 前检查「服务运行域 对 可执行文件标签」的 `entrypoint` 权限。文件被标成
`u:object_r:vendor_file:s0` 时，`hal_*_default` 域没有 `entrypoint vendor_file` 授权 → 拒绝 →
exec 失败 → 退出码 127 → init 每秒重试，框架侧表现为该 HAL 永远找不到。

为什么会被标成 `vendor_file`：07 的标签真值有四级（`setxattr.go`），这 13 个新二进制**四级都没命中**：

1. 装配树自带的 `vendor/etc/selinux/vendor_file_contexts`：只有底座老 HAL 的规则，且是按 `@版本`
   命名的（`android.hardware.nfc@1.0-service` 之类），与本轮新增的 `android.hardware.nfc-service`
   这类无版本名不匹配；
2. 官方 vendor 镜像 inode：这些自研二进制在官方镜像里不存在；
3. 机型基线 JSON 的 `vendor.selinux-labels`（精确路径）：没有登记；
4. `defaultLabelFor` 兜底：对 `bin/hw/*` 只特判 audio/radio，其余一律 `vendor_file`。

策略侧其实是齐的：`hal_memtrack_default_exec` 等 12 个 `_exec` 类型都在随附的
`vendor_sepolicy.cil` 里定义，每个域也都有
`(allow hal_X_default hal_X_default_exec (file (read getattr map execute open entrypoint)))`。
缺的**只是 file_contexts 规则 + 镜像 xattr**。

## 3. 证据

- `log-fail.txt`：实例 33 的 entrypoint 拒绝原文、13 个服务的 127 次数、涉及域统计，
  以及 `debugfs ea_list` 的对照（老 HAL 是 `hal_sensors_default_exec`，新 HAL 全是 `vendor_file`）。
- `log-pass.txt`：07 打印「补充 HAL 可执行标签 13 条」、`vendor.img` / `patch.img` 里的
  `security.selinux`、guest 内 `ls -Z`、实例 34 六项计数全 0、10 个服务的稳定 PID，
  以及 NFC HAL 被真正调通的日志（`HalOpenInternal: Open Error=OK`）。

## 4. 改动

1. 新增 `engine/build/07_build_vendor/halexec.go`：`ensureHalExecLabels` 从装配树
   `etc/init/*.rc` 反推标签——
   - 解析 `service <名> <路径>` 块内的 `seclabel u:r:<域>:s0`；
   - 只处理 `bin/hw` 下、且装配树里确实是普通文件的二进制；
   - 若该路径已命中非兜底规则（`!= "" && != vendor_file`）则跳过；
   - `<域>_exec` 必须在随附策略里真的定义过（扫 `etc/selinux/*` 的 `(type X)` 与
     `u:object_r:X:s0`），否则只打 `[warn]` 不落规则——造一个策略里不存在的标签，运行态等于
     unlabeled；
   - 通过后往 `vendor/etc/selinux/vendor_file_contexts` 追加
     `/(vendor|system/vendor)/bin/hw/<转义名> u:object_r:<域>_exec:s0`，并同步进程内规则表。
2. `engine/build/07_build_vendor/execute.go`：在 `assemble`（步骤 4-6）与 `packVendorImg`
   （步骤 7）之间插入「步骤 6b」调用。顺序很关键：09 的 `loadVendorMeta` 在步骤 7 里重读这份
   file_contexts，所以规则与镜像 xattr 天然同源，运行态 `restorecon` 也读同一份文件。
3. 新增 `engine/build/07_build_vendor/halexec_test.go`：补规则 / 三种跳过 / rc 解析三个单测。

## 5. 验证

| 项 | 结果 |
| --- | --- |
| `go build ./...` / `go vet` | 通过 |
| `go test ./07_build_vendor/` | 全包通过（含新增 3 个单测） |
| 07 构建输出 | `[ok] 补充 HAL 可执行标签 13 条`（清单见 log-pass.txt） |
| `vendor.img` xattr | 13 个 `bin/hw` 二进制带 `security.selinux`；老 HAL 标签未变 |
| `patch.img` xattr | `/vendor/bin/hw/android.hardware.nfc-service` = `u:object_r:hal_nfc_default_exec:s0` |
| guest `ls -Z` | 10 个抽查二进制逐个是 `hal_*_default_exec` |
| `denied { entrypoint }`（实例 34） | 0 |
| `exited with status 127` | 0 |
| `ignored duplicate definition of service` | 0 |
| 三项 `Waited one second`（nfc / 两个 virtualhal） | 0 / 0 / 0 |
| 进程 | 10 个服务 PID 824-849 稳定，开机至今未重启 |
| 功能面 | `INfcAidl::fromBinder returned ver(1)`、`HalOpenInternal: Open Error=OK`、`NfcService: Enabling NFC` |
| 开机 | `sys.boot_completed=1`（71.7 s） |

## 6. 遗留

- 13 条规则追加在 `# cdh-patch-end: vendor_file_contexts` 标记块**之后**（块外）。当前 09/11 不改写
  该文件，因此稳定；若以后改成整份重写策略块，需要把这批规则并入
  `sepolicy.go` 的 patch 块（或在块内追加），否则会被覆盖掉。
- 带 `@版本` 的旧名规则（`nfc@1.0/1.1/1.2-service` 等）仍在基线里，本轮新增的无版本名规则与它们
  并存、互不冲突；以后若改用版本化命名，会自动命中基线，不必再补。
- `dumpsys nfc`、`dumpsys media.camera` 仍返回 `FAILED_TRANSACTION`，原因是 vendor 域对
  pioneer(shell) 的 dump 管道 fd 的 `{ use }` 被拒（`permissive=0`），与 HAL 是否可用无关；本仓
  README 备注也记了「原厂实例同样如此，不能当判据」。若要可读，需要补 `hal_*_default` 对
  `pioneer` 的 `fd use` 授权（调试增强，非功能必需）。
- `com.android.nfc` 的应用层 watchdog 每 90 s 仍会 abort 一次并重启进程：`INfc` 目前是空实现，
  `enable()` 的 callback 不返回，属 #53 第 6 节记的「功能面待补齐」，不在本卡点范围内。
