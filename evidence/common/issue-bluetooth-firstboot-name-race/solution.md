# #212：修复蓝牙首启 Adapter 名未落盘与 `service_daemon` 启动期并发竞态

## 问题

1. `gold_15-OS3.0.11.0.VNQCNXM` 上 `aligner.network_full` 显示 `bluetooth.name` 在云机（`4d5128bd8ca07462-cloud`）返回 `"2312DRAABC"`（`ro.product.model`），而真机（`07cf5fc1784783d5`）为市场名 `"Redmi Note 13 5G"`（触发 `network` 域 `identity=1` 差异）。
2. 在将蓝牙首启名称同步加固进首次开机初始化脚本的过程中，进一步暴露并一并解决了 `android.service.daemon` 的两处底层启动竞态：
   - **`hwservicemanager` 误激活竞态**：`props/scrub.rs` 在 `boot_completed` 后无条件删除 `hwservicemanager.disabled` 并置 `hwservicemanager.ready=true`，即使当时 `init.svc.hwservicemanager == "stopped"`。在 HyperOS Mainline 蓝牙 APEX（`com.android.bluetooth`）初始化时，`HalVersionManager` 读到 `ready=true` 去连接未运行的 `/dev/hwbinder`，导致蓝牙无法完成 `STATE_ON` 并报错崩溃；
   - **`mgmt_bridge` 与 `reap_children` 子进程抢收竞态**：`main.rs` 紧随 `mgmt_bridge::spawn()` 进入 `reap_children()`（循环 `waitpid(-1, WNOHANG)`），与 `mgmt_bridge` 后台线程里执行 `Command::output()` 的 `run_ip()` 并发抢收子进程退出码，偶发 `ECHILD(10)` 导致 `mgmt0`（`10.0.2.15/24`）隔离网络命名空间与 `0.0.0.0:5555 -> 127.0.0.1:5555` adb 桥接中断。

## 根因

1. 旧版 `cph-setup-display` 仅静态执行 `settings put secure bluetooth_name` 并短暂开关蓝牙，未等待蓝牙栈进入 `STATE_ON` 后由小米 `AdapterProperties.onBluetoothReady()` 把名称持久化写入 Bluedroid 栈（`/data/misc/bluedroid/bt_config.conf` `[Adapter] Name`），导致 `BluetoothAdapter.getName()` 读回默认的 `ro.product.model`。
2. `props/scrub.rs` 未区分精简了旧 HIDL `hwservicemanager` 的纯 AIDL 机型，误对已停止的服务发出就绪信号。
3. `mgmt_bridge::spawn()` 未设置线程内全部 `ip` 命令执行完成与 `setns` 恢复主命名空间前的同步屏障，导致 `main()` 线程提前调用 `waitpid(-1)` 回收了 `run_ip()` 的子进程 PID。

## 证据

- 修复前：`4d5128bd8ca07462-cloud` `diff/network.json` 中 `bluetooth.name` 差异与中间调试阻断现象见同目录 [`log-fail.txt`](log-fail.txt)。
- 修复后：guest 内 `service_daemon` ARM64 单元测试结果、`dmesg` `mgmt_bridge`、`dumpsys bluetooth_manager`、`/data/misc/bluedroid/bt_config.conf` 与 `7d10bffa4bc09406-cloud` 重采 `diff/network.json` 见同目录 [`log-pass.txt`](log-pass.txt)。

## 改动

1. **[`cph-setup-display`](../../../vendor/hardware/android.service.daemon/vendor/bin/cph-setup-display)**：
   - 重写 `sync_bt_name_once()`：先设置 `settings secure bluetooth_name` / `settings global device_name`，再执行 `cmd bluetooth_manager enable` 并轮询 `dumpsys bluetooth_manager` 的 `name:` 行（最多 `15 × 0.3s`）等待 `STATE_ON` 与名称落盘；
   - 若栈仍返回旧名称，提供跨 ROM 兜底直接修正 `/data/misc/bluedroid/bt_config.conf` `[Adapter] Name` 并执行 `chown/chmod/restorecon` 后二次重开校验，最终关闭蓝牙恢复默认 `STATE_OFF` 态。
2. **[`props/scrub.rs`](../../../vendor/hardware/android.service.daemon/src/rust/src/props/scrub.rs)**：
   - 仅当 `init.svc.hwservicemanager == "running"` 时才删除 `hwservicemanager.disabled` 与写入 `hwservicemanager.ready=true`，避免没有运行 `hwservicemanager` 的 ROM 被 Mainline APEX 撞死。
3. **[`mgmt_bridge.rs`](../../../vendor/hardware/android.service.daemon/src/rust/src/mgmt_bridge.rs)**：
   - 在 `spawn()` 增加 `std::sync::mpsc::sync_channel(1)`（`ready_tx/ready_rx`）就绪屏障：`spawn()` 阻塞等待后台线程完成全部 `run_ip()`、`TcpListener::bind("0.0.0.0:5555")` 及 `setns(main_ns_fd)` 返回主命名空间后再返回，使 `main()` 的 `reap_children()` 不会抢收 `ip` 子进程；并在 `run_ip()` 中对 `ECHILD`（`raw_os_error() == Some(10)`）做防御性兼容。

## 验证

1. **ARM64 单测**：交叉编译 `service_daemon` 测试二进制并在 guest `gold` 实例 2 执行，输出 `test result: ok. 11 passed; 0 failed`。
2. **新开干净实例 `gold_15-OS3.0.11.0.VNQCNXM/2`（`7d10bffa4bc09406-cloud`）实机复测**：
   - `adb connect 127.0.0.1:5911` 一次直连在线；`dmesg` 显示 `mgmt_bridge: mgmt0 (10.0.2.15/24) 已隔离并监听 0.0.0.0:5555 -> 127.0.0.1:5555`，主 netns `ip link` 无 `mgmt0`；
   - `init.svc.hwservicemanager=stopped`、`hwservicemanager.disabled=true`、`hwservicemanager.ready=`；
   - `dumpsys bluetooth_manager` 显示 `enabled: false`、`state: OFF`、`name: Redmi Note 13 5G`、`Bluetooth crashed 0 times`，`logcat -b crash` 无蓝牙崩溃；`/data/misc/bluedroid/bt_config.conf` 包含 `Name = Redmi Note 13 5G`；
   - `aligner.network_full` 重采结果：`bluetooth.name` 差异彻底清零（`network` 域 `identity: 1 -> 0`、`fatal: 3 -> 2`，仅剩双端蜂窝网口在线态差异）。

## 遗留

1. `Bluetooth Status` 中的默认模拟蓝牙地址仍为 `00:00:00:DE:17:00`（本次仅对齐 `bluetooth.name`，BT MAC 伪装需结合虚拟 BT HAL 或随 [`#213`](../issue-gold-factory-boot-props-oem-mac/solution.md) 派生 MAC 进一步透传给 BT 栈时统一处理）。
