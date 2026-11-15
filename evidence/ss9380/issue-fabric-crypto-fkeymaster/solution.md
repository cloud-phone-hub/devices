# fabric_crypto 硬查不存在的 Fkeymaster 服务 → 每 ~5 s abort 一次

- 机型：samsung / Galaxy S25 Ultra（`SM-S9380`，`ss9380`，OneUI Android 16）
- 发现时间：2026-10-30
- 盘集：修复前 index 3 → build15 通过
- 状态：已解决（fabric_crypto 只启动一次即被停止，错误日志归零）

## 一、问题

开机后 `fabric_crypto` 服务反复重启，每次都在 main 里直接 abort：

```
F fabric_crypto: Check failed: fKeymaster !@ Error : Not found Fkeymaster service
F DEBUG   : Abort message: 'Check failed: fKeymaster !@ Error : Not found Fkeymaster service'
```

日志中同时可见它一直在等一个不存在的服务：

```
I libbinder.ServiceManagerCppClient: Waiting for service
  'vendor.samsung.hardware.security.fkeymaster.ISehFkeymaster/default' on '/dev/binder'...
```

整份日志里 `starting service 'fabric_crypto'` **185 次**、
`Not found Fkeymaster service` **347 条**、abort 174 次，约每 5 s 一轮，
持续刷屏并占用启动带宽。

## 二、根因

`fabric_crypto` 是三星的密钥派生/加密服务，启动时硬性要求

```
vendor.samsung.hardware.security.fkeymaster.ISehFkeymaster/default
```

官方实现（`vendor.samsung.hardware.security.fkeymaster-service`）依赖
QSEECom/TEE：实测在云机上直跑会在 `main+764` 处
`CHECK(status == STATUS_OK)` 直接 SIGABRT——QEMU 没有 `/dev/qseecom`，
也没有 TEE。因此：

- fkeymaster 服务在云机上**永远注册不上**；
- fabric_crypto 找不到依赖就 abort，`init` 按服务定义再拉起，形成死循环。

这不是权限或标签问题，补 sepolicy 或换 vendor 件都无法让它成功；该服务唯一的
业务就是在 TEE 里生成密钥，没有 TEE 时它没有可完成的工作。

## 三、证据

- 失败态：`log-fail.txt`
  - `Not found Fkeymaster service` 与 abort 栈原文；
  - 计数：185 次启动、347 条错误、174 次 abort，时间跨度约 15 分钟；
  - 启动时间序列（96.6 s → 137.9 s → … → 1018.4 s），约 5 s 一轮。
- 通过态：`log-pass.txt`
  - build15 中 `starting service 'fabric_crypto'` 仅 1 次（95.788 s）；
  - `on nonencrypted` 动作在 96.045 s 执行 `stop fabric_crypto`，
    96.047 s 收到 SIGKILL，之后**没有**新的启动；
  - `Not found Fkeymaster service` = 0。

## 四、改动

新增机型层 rc：
`devices/manifest/samsung/ss9380/vendor/etc/init/cdh-fabric-crypto.rc`

```
# class main 由 init.rc 的 `on nonencrypted` 拉起；本文件在系统 rc 之后解析，
# 同触发器里的 stop 紧跟在 class_start main 之后执行，把这条循环停掉。
on nonencrypted
    stop fabric_crypto
```

选择“停止服务”而不是伪造 Fkeymaster HAL：伪造一个没有 TEE 的 fkeymaster 会让
fabric_crypto 进入更深的密钥流程并产生错误密钥；停止它是云机（无 TEE）下唯一
语义正确的行为，且不修改系统分区、不影响其它 HAL。

## 五、验证

1. `07_build_vendor` → `11_pack_vda` 重跑后，从最终 `vendor.img`
   `debugfs cat etc/init/cdh-fabric-crypto.rc` 读回 `on nonencrypted /
   stop fabric_crypto`（681 B）；
2. build15 整份 qemu.log 中 fabric_crypto 启动 1 次、SIGKILL 1 次、
   `Not found Fkeymaster` 0 次；
3. `logcat -b crash` 中 `Not found Fkeymaster` = 0；
4. 该服务停止后未观察到对系统启动、锁屏或桌面的影响（见同 build 的
   `issue-knoxguard-china-region/pass-home.png`）。

## 六、遗留

- `fabric_crypto` 在停止前仍打印了两次 binder 等待与 `!@ start fabric_crypto : 2`
  的启动日志，属正常的一次性启动输出，不是循环。
- 云机没有 TEE，凡是强依赖 QSEECom 的三星安全服务（fkeymaster、部分 Knox
  Trustlet 路径）都不可用；本条目只处理会刷屏/拖慢启动的 fabric_crypto，
  其它服务的失败按各自影响单独评估。
