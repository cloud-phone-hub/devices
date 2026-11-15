# m22 加 boot HAL 后开机互锁（vdc checkpoint × vold waitForService × init 单线程）

- 设备：flyme/m22（MEIZU 22，QTI `sun`），实例 32 / 34
- 日期：2026-10-29
- 关联：[issue-m22-hal-gap-fill](../issue-m22-hal-gap-fill/solution.md)（#53）

## 1. 问题

按 #53 的缺口清单给 m22 的 `device.json` 加上 `android.hardware.boot`（第 26 条）并重跑
07→09→11 后，新起的实例 32 一直拿不到 `sys.boot_completed`（轮询到 200 s 仍为空），串口里
每秒一条：

```
ServiceManagerCppClient: Waited one second for android.hardware.boot.IBootControl/default
servicemanager: Caller(pid=269,uid=0,sid=u:r:vold:s0) Since 'android.hardware.boot.IBootControl/default'
                could not be found trying to start it as a lazy AIDL service.
```

122.9 s 起升级为 init 自身的背压告警：

```
init: Too many pending control messages, dropped 'interface_start'
```

同一份 patch 里其余新 HAL（memtrack / nfc / camera / usb.gadget / 两个 virtualhal）都已经在
servicemanager 注册，只有 boot 这一条把开机按死。对照实例 31（未加 boot）同一位置只有一句
`Could not find android.hardware.boot.IBootControl/default in the VINTF manifest` 就继续引导。

## 2. 根因

三件事叠成环：

1. **init 在 `late-fs` 用 `exec` 同步跑 vdc**：`Calling: /system/bin/vdc checkpoint needsCheckpoint`
   与 `restoreCheckpoint /dev/block/vdb` 都是 `SVC_EXEC`，init 主线程要 `wait()` 到子进程退出
   （实例 32 里 `markBootAttempt` 那条就明确写了 `started; waiting...`，20 s 后才 `exited with
   status 0`）。
2. **vold 因 VINTF 声明而阻塞**：只要 device manifest 里声明了
   `android.hardware.boot.IBootControl/default`，servicemanager 就会尝试把它当**懒启动**服务拉起来，
   vold 侧 `ServiceManagerCppClient` 于是进入 `waitForService`，每秒重试；没声明时它直接
   `Could not find` → `AIDL IBootControl not available, falling back to HIDL`，不阻塞。
3. **懒启动请求要由 init 处理**，而 init 此刻正被 `exec` 占着 → `ctl.start` 排进队列，
   队列打满后连 `interface_start` 一起丢（`Too many pending control messages`）→ vold 永远等不到
   → vdc 永远不返回 → 互锁。

即：**声明了一个「懒启动」的 boot HAL，却把它放在「init 会同步阻塞等待 vold」的路径上**。

## 3. 证据

- `log-fail.txt`：实例 32 串口节选（vdc 调用 → `Found ... in device VINTF manifest` →
  144 次 `Waited one second` → `Too many pending control messages`），并与实例 31（未声明 boot，
  `Too many pending` 0 次）对照。
- `log-pass.txt`：撤掉声明并重跑后的实例 34——`WaitForService` 0 次、`Too many pending` 0 次、
  `sys.boot_completed=1`（71.7 s 处理 `sys.boot_completed=1` 动作）。

## 4. 改动

- `devices/manifest/flyme/m22/device.json`：撤下 `android.hardware.boot` 条目（hardware 26 → 25 条）。
- 模块代码与产物不撤：`engine/vendor/hardware/android.hardware.boot` 保留（编译通过、可随时启用），
  07 的复用判据因清单里已无该模块而不再把它装进装配树。
- 07 步骤 7 之后装配树里 `bin/hw` / `etc/init` 已无任何 boot 相关文件（round2 构建日志确认 0 残留）。

## 5. 验证

| 项 | 结果 |
| --- | --- |
| 重跑 07→09→11（round2） | EXIT=0，patch.img 重新发布 |
| 装配树 boot 残留 | `bin/hw` 0 个、`etc/init` 0 个 |
| 实例 34 `sys.boot_completed` | 1（71.7 s 触发 `sys.boot_completed=1` 动作） |
| `Waited one second for ...IBootControl` | 0 |
| `Too many pending control messages` | 0 |
| vold 侧表现 | 单句 `Could not find ...` + `AIDL IBootControl not available, falling back to HIDL` |

## 6. 遗留

- 云机的数据盘可重建，不需要 Android checkpoint / boot control 能力，因此本轮选择「不声明」，
  没有去改 init/vold 的时序。
- 若将来确实要提供 `IBootControl`（例如需要 A/B 槽状态上报），必须让它**不被懒启动**：
  rc 里以 `class early_hal` 起、`oneshot` 且排在 vdc 之前，或者由内核/属性层把
  `vdc checkpoint` 整段跳过；否则同样的互锁会复现。
- 同理适用于任何「`late-fs` 同步 exec 的客户端 + 依赖懒启动 HAL」的组合，新增 HAL 之前先确认
  它是否出现在 `late-fs`/`post-fs-data` 的同步 `exec` 路径上。
