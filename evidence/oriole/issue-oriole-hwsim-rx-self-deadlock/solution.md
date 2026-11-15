# Issue: oriole (5.10) wifi 数据面在 kthread 里直接调 netif_receive_skb，导致 TCP 锁单核自死锁 → 整机卡死

- **设备/机型**: Google Pixel 6（`google/oriole`，官方固件 `AP2A.240905.003.F1` + `lineage-21.0` 底座，内核 `5.10.270-4k-cdh`）
- **首次暴露**: 2026-11-04 实例 1（8 vCPU / 8 GB），冷启动 27.98 s 进桌面后持续运行至 `uptime = 822.15 s` 整机卡死（`qemu-system-aarch64` 占满 402.8% CPU，`adb shell` 全部超时挂起）
- **状态**: **已闭环**（2026-11-04，`5.10/0002-hwsim-fake-wifi.patch` 改用 `netif_rx_ni(skb)`，`6.1`/`6.6` 同步改用 `netif_rx(skb)`）

---

## 一、问题

`oriole_AP2A.240905.003.F1` 在 #150 闭环后冷启动 27.98 s 正常进 Pixel Launcher 桌面，但挂机运行到 `uptime ≈ 822 s` 时整机无响应：

- 宿主侧 `ps aux` 显示 `qemu-system-aarch64`（PID 19671）持续占用 **402.8% CPU**（4 个 vCPU 在 guest 内核里 100% 空转）；
- `adb -s 127.0.0.1:5911 shell` 任何命令全部卡住不返回；
- 串口日志 `qemu.log` 从 `822.15 s` 起先报 `rcu: INFO: rcu_preempt self-detected stall on CPU 0`（运行任务 `cdh-wifi-rx:5332`），随后从 `828.00 s` 起每 12~24 s 循环报 `watchdog: BUG: soft lockup`：
  - `CPU#0 stuck for 22s! [cdh-wifi-rx:5332]`
  - `CPU#2 stuck for 23s! [CronetNet:16363]`
  - `CPU#4 stuck for 23s! [netd:719]`

---

## 二、根因

此前把 `0002-hwsim-fake-wifi.patch` 移植到 `devices/kernels/patches/5.10/` 时，以 `6.1/0002-hwsim-fake-wifi.patch` 为底稿，而 `6.1` 与 `6.6` 尚未合入 15R（`6.12`）在 [../../15r/issue-hwsim-rx-self-deadlock/](../../15r/issue-hwsim-rx-self-deadlock/solution.md)（#132）定位出的 `cdh_rx_inject()` 修复，`cdh_rx_inject()` 末尾仍是：

```c
netif_receive_skb(skb);
```

`qemu.log` 第 22950~22993 行的 CPU#0 RCU stall 栈完整还原了单核自死锁过程：

```text
Task dump for CPU 0:
task:cdh-wifi-rx     state:R  running task     stack:    0 pid: 5332 ppid:     2 flags:0x0000000a
Call trace:
 ...
 queued_spin_lock_slowpath+0x204/0x384
 tcp_delack_timer+0x30/0x17c          <-- 5. timer 软中断抢同一把 sk->sk_lock.slock，自旋死锁
 call_timer_fn+0x3c/0x25c
 __run_timers+0x1e0/0x338
 run_timer_softirq+0x2c/0x54
 __do_softirq+0x108/0x410
 __irq_exit_rcu+0xc4/0xc8             <-- 4. 硬中断退出：因 BH 未禁用，就地执行软中断
 irq_exit+0x18/0x2c
 __handle_domain_irq+0xa4/0xf0
 gic_handle_irq+0x64/0x148
 el1_irq+0xe4/0x1c0                   <-- 3. 持锁期间本核收到时钟硬中断
 __wake_up_sync_key+0xa8/0x164
 sock_def_readable+0xb8/0x158
 tcp_data_ready+0x90/0xe8
 tcp_rcv_established+0x5cc/0x68c
 tcp_v4_do_rcv+0xa0/0x208
 tcp_v4_rcv+0x848/0x8e8               <-- 2. bh_lock_sock_nested(&sk->sk_lock.slock) 仅 spin_lock（假设 BH 已关）
 ip_protocol_deliver_rcu+0xdc/0x208
 ip_local_deliver+0xc0/0x15c
 ip_rcv+0x80/0x110
 __netif_receive_skb_core+0xbac/0xe48
 __netif_receive_skb+0x48/0x100
 netif_receive_skb+0x44/0x1e4         <-- 1. 违反“仅限软中断上下文调用”约定
 cdh_rx_thread+0x338/0x35c [mac80211_hwsim]
 kthread+0x140/0x214
```

1. Linux 内核 `net/core/dev.c:5679` 明确约定 `netif_receive_skb()` **只能在软中断（BH 禁用）上下文调用**（`"This function may only be called from softirq context and interrupts should be enabled"`）；
2. `cdh-wifi-rx` 是 `kthread_run()` 创建的内核线程（进程上下文，BH 开启）。直接调 `netif_receive_skb()` 进入 `tcp_v4_rcv()` 后，`bh_lock_sock_nested(sk)` 展开为纯 `spin_lock(&sk->sk_lock.slock)`，不会关 BH；
3. 当 CPU#0 持着 `sk->sk_lock.slock`（地址 `ffffff80b9b94148`）走到 `__wake_up_sync_key()` 时被硬件时钟中断打断，退出路径 `__irq_exit_rcu -> __do_softirq` 因 BH 计数为 0，在**同一个 CPU#0** 上执行 `run_timer_softirq -> tcp_delack_timer`，后者对同一个 socket 调用 `bh_lock_sock(sk)` 去抢 CPU#0 自己在进程上下文正持有的 `ffffff80b9b94148` —— **CPU#0 自己锁死自己**；
4. 随后 CPU#2（`CronetNet:16363`，`tcp_recvmsg -> lock_sock_nested`，`x0 = ffffff80b9b94148`）与 CPU#4（`netd:719`，`tcp_diag_get_info -> tcp_get_info -> lock_sock_fast`，`x0 = ffffff80b9b94148`）尝试获取同一把 socket 锁，全部卡在 `queued_spin_lock_slowpath` 自旋，引发多核 `soft lockup` 与整机冻死。

**5.10 与 6.1+ 的内核 API 差异**：

- 在 Linux 6.1+（上游 5.18 `commit baebdf48c360` 之后），`netif_rx_ni()` 已并入 `netif_rx()`：当在进程上下文调用 `netif_rx(skb)` 时，内部自动 `local_bh_disable() -> netif_rx_internal(skb) -> local_bh_enable()`；
- 但在 Linux **5.10**（`net/core/dev.c:4898-4926`）中，`netif_rx(skb)` **不会**关 BH 也不会触发 `do_softirq()`（仅限中断上下文调用）；进程上下文必须调用 **`netif_rx_ni(skb)`**（`preempt_disable() -> netif_rx_internal(skb) -> do_softirq() -> preempt_enable()`）。

---

## 三、证据

- 失败日志：[log-fail.txt](log-fail.txt)
  - `qemu.log:22947-22993`：`[  822.158919][    C0] rcu: INFO: rcu_preempt self-detected stall on CPU`，栈底 `cdh_rx_thread+0x338/0x35c [mac80211_hwsim] -> netif_receive_skb -> ... -> tcp_v4_rcv -> ... -> el1_irq -> __do_softirq -> tcp_delack_timer -> queued_spin_lock_slowpath`；
  - `qemu.log:23102-23138`、`52256-52350`：CPU#2（`CronetNet`）与 CPU#4（`netd`）寄存器 `x0 : ffffff80b9b94148` 指向同一把 `sk_lock.slock`；
- 内核源码对比：
  - `kernels/5.10/net/core/dev.c:4911`：`int netif_rx_ni(struct sk_buff *skb)`（进程上下文入口）；
  - `kernels/6.1/net/core/dev.c:5090`、`kernels/6.6/net/core/dev.c:5207`：`int netif_rx(struct sk_buff *skb)` 内含 `need_bh_off = !(hardirq_count() | softirq_count())`；
- 通过日志：[log-pass.txt](log-pass.txt)

---

## 四、改动

1. **`devices/kernels/patches/5.10/0002-hwsim-fake-wifi.patch`**：
   `cdh_rx_inject()` 末尾将 `netif_receive_skb(skb);` 改为 `netif_rx_ni(skb);`，并同步更新后续 8 个 hunk 行号偏移（`git -C kernels/5.10 apply --check` 通过）。
2. **`devices/kernels/patches/6.1/0002-hwsim-fake-wifi.patch`** 与 **`devices/kernels/patches/6.6/0002-hwsim-fake-wifi.patch`**：
   同步合入 #132 的修复，将 `cdh_rx_inject()` 末尾的 `netif_receive_skb(skb);` 改为 `netif_rx(skb);`（`git apply --check` 在 `6.1`、`6.6`、`6.12` 全通过），消除其余内核分支的同类隐患。
3. **重编流水线**：
   `02_build_kernel`（新配方 `5.10-cdh-549c734f9d36`）→ `03_build_modules`（`strings out/kernel/5.10/modules/mac80211_hwsim.ko` 确认 `netif_rx_ni` 存在、`netif_receive_skb` 为 0）→ `04`~`11_pack_vda`（`vda-32bfe3cfca6c`，`patch.img` sha256 `cd3c7ef4425f`）。

---

## 五、验证

清空旧实例 1（`vda-accc03338e83`）并基于新镜像（`vda-32bfe3cfca6c`、`patch.img` `cd3c7ef4425f`）冷启动实例 1（`PID 23218`，`adb=5911`，`vnc=5912`）：

1. **符号与模块校验**：
   - `strings out/kernel/5.10/modules/mac80211_hwsim.ko | grep -E "^netif_(rx|receive_skb)"` 仅输出 `netif_rx_ni` 与 `netif_rx`（`netif_receive_skb` 计数为 **0**）；
   - `uname -a`：`Linux localhost 5.10.270-4k-cdh #3 SMP PREEMPT Wed Nov 4 04:03:43 UTC 2026 aarch64 Toybox`。
2. **冷启动与 Wi-Fi 关联**：
   - `sys.boot_completed=1`、`init.svc.bootanim=stopped`；
   - `dmesg`：`mac80211_hwsim: cdh: bound virtio netdev 'eth1' for data path`、`wlan0: associated`（BSSID `02:00:00:00:01:00`）；
   - `dumpsys wifi`：`CurState=ConnectedState`、`SSID: "CDH-A"`、`mLinkAddresses=[192.168.60.100/24]`、`successfulTxPackets: 20274`。
3. **多核 backlog 软中断队列与稳定性**：
   - `/proc/net/softnet_stat` 显示 8 个 vCPU 的 `process_backlog` 均正常消费包（CPU0~CPU7 处理帧数 `0x1a23`~`0x2cf6`，累计 >74,000 帧），所有核心第 2 列 `dropped = 00000000`；
   - `dmesg | grep -E "soft lockup|rcu_preempt|self-detected stall|tcp_delack_timer"` 计数为 **0**；
   - 宿主 `qemu-system-aarch64` CPU 占用回落至正常水平（`%CPU = 84.2%`，`STAT = S`），不再出现 402.8% 死锁空转。

---

## 六、遗留

- `qemu.log` 中 `android-hardware-media-c2-hal` 与 `vendor-dolby-media-c2-hal` 每 5 s 以 `status 1` 退出重启（`oriole` 尚未配 `vendor/etc/media_config.xml`，与 15R 的 #129 同因），不影响桌面与网络，后续按需补齐 `media_config.xml`。

