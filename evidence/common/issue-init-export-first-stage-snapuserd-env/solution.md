# #217：在 `on init` 注入 `FIRST_STAGE_SNAPUSERD_*` 环境变量对齐 Zygote 环境

## 问题

`gold_15-OS3.0.11.0.VNQCNXM` 首轮 `aligner.jvm_full`（`4d5128bd8ca07462-cloud`）通过 Java `Os.getenv()` 检出 2 项真机存在、云机 `<缺>` 的 `capability` 级环境变量差异：

- `env.FIRST_STAGE_SNAPUSERD_INFO`: 真机 `"socket"` / 云机 `<缺>`
- `env.FIRST_STAGE_SNAPUSERD_PID`: 真机 `"274"` / 云机 `<缺>`

## 根因

搭载 Virtual A/B 的真机（含 `gold`）在 first-stage init 启动 `snapuserd` 并向 second-stage init 移交 socket 时，会将 `FIRST_STAGE_SNAPUSERD_INFO` 与 `FIRST_STAGE_SNAPUSERD_PID` 保留在 `PID 1` 环境中，进而被 `zygote64` 与所有普通 App 进程继承。云机 QEMU 镜像不使用 Virtual A/B 块设备快照链，因此默认没有这两个环境变量。

## 证据

- 修复前：`4d5128bd8ca07462-cloud` `diff/jvm.json` 两条 `<缺>` 记录见同目录 [`log-fail.txt`](log-fail.txt)。
- 修复后：`gold_15-OS3.0.11.0.VNQCNXM/2`（`7d10bffa4bc09406-cloud`）`zygote64` 环境变量读取与重采 `diff/jvm.json` 结果见同目录 [`log-pass.txt`](log-pass.txt)。

## 改动

1. **[`cph_daemon_service.rc`](../../../vendor/hardware/android.service.daemon/vendor/etc/init/cph_daemon_service.rc)**：
   - 在 `on init` 动作段添加：
     ```rc
     on init
         export FIRST_STAGE_SNAPUSERD_INFO socket
         export FIRST_STAGE_SNAPUSERD_PID 274
     ```
   - 故意放在 `on init` 而非 `early-init`：此时 `ConnectEarlyStageSnapuserdAction` 已执行完毕，既不会触发 `init` 尝试连接并不存在的 snapuserd socket 导致 init crash，又能抢在 `zygote-start` 前写入 `PID 1` 继承环境。

## 验证

1. `gold_15-OS3.0.11.0.VNQCNXM/2`（`7d10bffa4bc09406-cloud`）实机复测：
   - 检查 `zygote64`（`pid=485`）`/proc/485/environ`，确含 `FIRST_STAGE_SNAPUSERD_INFO=socket` 与 `FIRST_STAGE_SNAPUSERD_PID=274`；
   - `aligner.jvm_full` 重采结果：上述两项缺失环境变量差异彻底清零（`jvm` 域 `capability: 4 -> 2`、`fatal: 4 -> 2`，`leavesReal/leavesCloud = 2128 / 2128`）。

## 遗留

1. `jvm` 域仅余 2 条能力差异：`ANDROID_SOCKET_zygote`（`29 vs 26`）与 `ANDROID_SOCKET_usap_pool_primary`（`30 vs 27`）的 init 分配 fd 序号偏移。
