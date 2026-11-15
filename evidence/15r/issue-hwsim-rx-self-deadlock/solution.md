# 15R 云机：wifi 数据面在 kthread 里直接 netif_receive_skb，导致 TCP 锁自死锁 → 整机卡死

- **设备/机型**：OnePlus 15R（CPH2769 / ColorOS 16.0.10.500_EX01 / SM8750 `canoe` /
  Android 16 + lineage-23.2 底座，内核 `6.12.81-4k-cdh`）
- **首次暴露**：实例 13（8 vCPU）uptime **136 s** 起报 5 个 CPU 的 `soft lockup`；实例 14
  （4 vCPU）延迟到 **696 s** 才爆发，两者症状一致（`adb shell` 全超时、qemu `%CPU` 400–540）
- **结论**：`mac80211_hwsim` 的 wifi 数据面直桥（`0002-hwsim-fake-wifi.patch` 里的
  `cdh-wifi-rx` 内核线程）在**进程上下文**直接调 `netif_receive_skb()`，而该 API 约定调用者
  已禁用 BH。于是 `tcp_v4_rcv` 的 `bh_lock_sock` 只做 `spin_lock`（不禁 BH），硬中断退出时
  timer 软中断在**同一个 CPU** 上重入 `tcp_delack_timer`，去抢本上下文正持有的
  `sk_lock`——自死锁。改用 `netif_rx()`（skb 进 per-CPU backlog，由 NET_RX 软中断处理）即解。

---

## 一、问题

| 实例 | vCPU | 首个 `soft lockup` | 表现 |
| :--- | :--- | :--- | :--- |
| 13 | 8 | **136 s** | 5 个 CPU 各报 7 波，26 s→160 s；`adb shell` 单条命令全部超时 |
| 14 | 4 | **696 s** | 4 个 CPU 各报 32 波，26 s→719 s；qemu `%CPU` 394.7；日志停写但 `adb devices` 仍 `device` |

共同点：卡住时 qemu 进程 `%CPU` 接近满核（4 vCPU→394%、8 vCPU→493%），说明**所有 vCPU 都在
guest 内核里空转**，而不是在等 IO；`soft lockup` 报告的 CPU 利用率是 `100% softirq / 0% idle`。
栈分两类，指向同一把锁：原发者 `queued_spin_lock_slowpath ← _raw_spin_lock ←
tcp_delack_timer`（在软中断里抢自己在进程上下文持有的 `sk_lock`），其余 CPU 是等待者
`queued_spin_lock_slowpath ← _raw_spin_lock_bh ← __lock_sock_fast ← tcp_get_info`。

## 二、根因

两个实例的原发栈逐帧相同（同一函数、同一偏移），与 vCPU 数无关。以实例 13 的 CPU#7
（uptime 136 s，起报第一条 lockup 时）为例：**它不是「等锁」，而是「持锁后自己撞自己」**。

```
[  136.020524][    C7] watchdog: BUG: soft lockup - CPU#7 stuck for 26s! [cdh-wifi-rx:7075]
[  136.020527][    C7] CPU#7 Utilization every 4s during lockup:
[  136.020528][    C7]  #1:   1% system,         98% softirq,     3% hardirq,    0% idle
Call trace:
 queued_spin_lock_slowpath+0x8c/0x444
 _raw_spin_lock+0x64/0x6c
 tcp_delack_timer+0x28/0x158          <-- 定时器要拿 sk->sk_lock.slock
 call_timer_fn
 __run_timer_base
 run_timer_softirq
 handle_softirqs
 __do_softirq
 irq_exit_rcu                          <-- 硬中断退出，此时 BH 计数为 0
 el1_interrupt
 __wake_up_sync_key                   <-- 下面这条栈正处在 tcp_data_queue 里
 sock_def_readable
 tcp_data_ready
 tcp_data_queue
 tcp_rcv_established
 tcp_v4_do_rcv
 tcp_v4_rcv                           <-- bh_lock_sock 在这里拿的锁
 ...
 netif_receive_skb
 cdh_rx_thread+0x238/0x370 [mac80211_hwsim]     <-- 我们的 kthread
 kthread
```

`tcp_v4_rcv()` 用的是 `bh_lock_sock_nested()`，它展开成**纯 `spin_lock`**——因为上游约定
「进 `netif_receive_skb()` 时调用者必须已禁用 BH」。网卡驱动都在 NAPI 软中断里调用，所以这条
假设成立。

而 `cdh_rx_inject()`（`cdh-wifi-rx` 线程读宿主帧后注入）是在 **kthread（进程上下文）**里调
`netif_receive_skb()`，BH 是开着的：

```
netif_receive_skb(skb);   /* 旧代码：BH 未禁用 */
```

于是 `tcp_data_queue()` 持着 `sk_lock.slock` 走到 `__wake_up_sync_key()` 时，一个硬中断进来，
退出路径 `irq_exit_rcu → do_softirq` 发现 BH 计数为 0，就在**同一 CPU** 上跑 timer 软中断；
`tcp_delack_timer` 恰好属于同一个 socket，去拿同一把 `sk_lock.slock`——自旋到天荒地老。

其余 CPU 是纯等待者（`tcp_get_info` 来自 `sock_diag` 的 `ss`/`dumpsys` 类遍历、
`tcp_recvmsg` 来自 nfd/GApps 的读），它们把「一个 CPU 自死锁」放大成「整机锁死」：

- `khugepaged` 卡在 `lru_add_drain_all → flush_work`（等不响应的 CPU）；
- system_server 的 `OomAdjuster` 卡在 `percpu_down_write → synchronize_rcu`（RCU grace
  period 因为多个 CPU 在自旋而永远不完成）；
- 最终 AMS 五条线程堵 75 s → `WATCHDOG KILLING SYSTEM PROCESS` → `sysrq: Resetting`。

**为什么 4 vCPU 比 8 vCPU 晚 5 倍**：自死锁需要「持锁期间硬中断到来且该 socket 有 delack
timer 待处理」这个窗口，并发度越高命中越快；vCPU 多少不影响根因。

## 三、证据

失败侧（`log-fail.txt`）：

- 实例 13：uptime 136.02 s 首条 `soft lockup - CPU#7 stuck for 26s! [cdh-wifi-rx:7075]`，
  同波 CPU#0/#3/#5/#6 分别是 `netd` / `NetworkMonitor` / `binder:825_1` / `CronetNet`，
  5 个 CPU 各 7 波（26→48→71→93→115→138→160 s），共 35 条；等待者栈是
  `__lock_sock_fast ← tcp_get_info ← inet_sk_diag_fill ← inet_diag_dump_icsk`（netd 走 netlink）；
- 实例 14：uptime 696 s 起 4 CPU 全 stuck，各 32 波直到 719 s（128 条），CPU#2 是原发者，
  CPU#0/#1 在 `tcp_diag_dump → tcp_get_info → __lock_sock_fast` 等同一把锁，CPU#3 在
  `vfs_writev → sock_write_iter → netlink_unicast` 等 netlink 锁；
- 实例 13：`INFO: task khugepaged:83 blocked for more than 61 seconds`
  （`lru_add_drain_all ← khugepaged`）、`INFO: task OomAdjuster:1113 blocked for more than
  61 seconds`（`percpu_down_write ← cgroup1_procs_write` → `synchronize_rcu`）；实例 13 共
  10 条 hung_task，实例 14 为 0 条（4 个 CPU 全在软中断自旋，还没轮到 D 状态）；
- 代码位置：`devices/kernels/patches/lineage-23-6.12/0002-hwsim-fake-wifi.patch` 的
  `cdh_rx_inject()`：`+	netif_receive_skb(skb);`
- 内核源码依据：`net/core/dev.c` 的 `netif_receive_skb()` 文档写明「must be called with
  BH disabled」；`include/net/sock.h` 的 `bh_lock_sock()` 就是 `spin_lock(&sk->sk_lock.slock)`。

通过侧（`log-pass.txt`，实例 15，8 vCPU，与实例 13 同规格，最严格对照）：

| 指标 | 实例 13（改前，8 vCPU） | 实例 15（改后，8 vCPU） |
| :--- | :--- | :--- |
| `soft lockup - CPU` | 136 s 起，5 CPU × 7 波（共 35 条） | **0**（1269 s） |
| `blocked for more than` | 有 | **0** |
| `tcp_delack_timer` 出现在栈里 | 有 | **0** |
| load（稳定期） | 14.5 → 20.9（持续攀升） | 0.05 – 0.29 |
| qemu `%CPU` | 480–537 | **112.7** |
| `screencap` / `dumpsys` | 全超时 | `dumpsys window` **57 ms** |
| wifi 数据面 | 卡死 | `Wifi is connected to "CDH-A"`，192.168.60.100，RSSI −41 |
| `softnet_stat` 丢包列 | — | 各 CPU **全 0** |

## 四、改动

**`devices/kernels/patches/lineage-23-6.12/0002-hwsim-fake-wifi.patch`**（一行 + 九行注释）：

```c
-	netif_receive_skb(skb);
+	/* cdh: netif_receive_skb() 约定调用者已处于 BH 禁用上下文（网卡驱动在
+	 * NAPI 软中断里调用）。本函数跑在 cdh-wifi-rx 内核线程里，BH 是开着的，
+	 * 而 tcp_v4_rcv 的 bh_lock_sock 只做 spin_lock（不禁 BH）——于是硬中断
+	 * 退出时会在同一个 CPU 上重入 timer 软中断，tcp_delack_timer 去抢本上
+	 * 下文正持有的 sk_lock，形成自死锁……
+	 */
+	netif_rx(skb);
```

补丁的 8 个 hunk 头（`+` 起点与行数）同步 +9，`git apply --check` 通过。

为什么不选「在注入点前后加 `local_bh_disable()/enable()`」：那样虽然也能挡住本 CPU 的重入，
但把整条 TCP 收包路径搬进了 kthread 的长临界区，`local_bh_enable()` 时还会就地冲刷 backlog，
与直接 `netif_rx()` 相比没有收益；`netif_rx()` 是驱动在进程上下文的官方用法。

重建链：`02 build_kernel`（13 分钟，含 2 个补丁，新卷 `…-0531d3a1f739`）→
`03 build_modules`（9 个 .ko，`mac80211_hwsim.ko` 203 个符号版本一致）→
`08 build_vendor_dlkm`（注入 14 个文件、回收 106 个基座模块）→ `09 build_patch`
（patch.img 384.0 MiB，sha256 `68afb224e0d3`）→ `11 pack_vda`（vda.img 五分区刷新）。

产物校验：`strings mac80211_hwsim.ko | grep -c '^netif_rx$'` = **3**，
`grep -c '^netif_receive_skb$'` = **0**。

## 五、验证

实例 15（2026-11-03 11:29:40 启动，11:30:59 `boot_completed=1`，8 vCPU / 8 GB）：

```
[01] 11:31:46 up=110.57s load=[30.74 8.90 3.08] lockup=0 hung=0 crash=13
[02] 11:33:32 up=216.53s load=[12.34 9.59 4.01] lockup=0 hung=0 crash=14
[03] 11:35:18 up=321.83s load=[2.93 7.01 3.67]  lockup=0 hung=0 crash=13
[04] 11:37:03 up=427.13s load=[0.56 4.97 3.29]  lockup=0 hung=0 crash=7
[05] 11:38:49 up=532.75s load=[0.19 3.52 2.94]  lockup=0 hung=0 crash=6
[06] 11:40:34 up=638.08s load=[0.06 2.49 2.63]  lockup=0 hung=0 crash=6
[07] 11:42:19 up=743.39s load=[0.05 1.77 2.35]  lockup=0 hung=0 crash=5
[08] 11:44:05 up=848.73s load=[0.29 1.30 2.12]  lockup=0 hung=0 crash=5
[09] 11:45:50 up=954.04s load=[0.13 0.95 1.90]  lockup=0 hung=0 crash=5
[10] 11:47:35 up=1059.31s load=[0.18 0.79 1.74] lockup=0 hung=0 crash=3
手工 11:48:  up=1103.09s  load=0.15 0.69 1.66  lockup=0  hung=0  delack=0  watchdog=0
[11] 11:49:21 up=1164.59s load=[0.86 0.81 1.64] lockup=0 hung=0 crash=2
[12] 11:51:06 up=1269.79s load=[1.30 0.95 1.60] lockup=0 hung=0 crash=1
```

`dumpsys window` 每次取样都 29–57 ms 返回 `mCurrentFocus=…com.android.launcher/.Launcher`，
`surfaceflinger`(1031) / `system_server`(947) 全程同一 PID；`logcat` 里 `WATCHDOG KILLING` 为 0。

数据面：`/proc/net/softnet_stat` 8 个 CPU 全在收包（processed 26635–39225），dropped 列**全 0**；
`cmd wifi status` = `Wifi is connected to "CDH-A"`，`wlan0` = 192.168.60.100/24。

画面：`screencap` 虽超时，但从宿主侧连 qemu 的 VNC（5912）抓到 1280x2800 真实扫描输出，
主动执行 `cmd statusbar expand-notifications` 后帧 md5 由 `88921791ce7b134b` 变为
`1d8f47659b300c87`，存图目视为完整桌面（状态栏 11:50、wifi 满格、电量 100%）——
合成与显示链路活着，卡死的只是 screencap 的 readback 路径。

## 六、遗留

1. **`screencap` 仍超时**（`adb shell screencap -p` 25–45 s 不返回，但系统其余命令 20–60 ms
   正常）。卡住的 screencap 进程状态是 `futex_wait_queue`（等 composer 回调），同时
   `dumpsys SurfaceFlinger` **正常返回但内容为 0 行**（112 ms），`logcat` 里每秒一条
   `E HwcComposer: getLuts failed Status(-8, EX_SERVICE_SPECIFIC): '8: '` 与
   `HWComposer: getLuts: getLuts failed for display 4632256312697818880: UNSUPPORTED (8)`。
   但 SF 的 44 个线程全 `sleeping`、CPU 0%（`top -H`），VNC 画面正常更新——坏的是
   `drmhwc` 的 LUT / readback 路径与 SF 的 dump 入口，不是整机卡死，不影响开机、桌面与网络，
   下一篇单独定位。
2. **`devices/manifest/oneplus/15r/device.json` 的 `smp` 会被回写成 8**：本轮曾改成 4 并生效
   （实例 14 实测 `nproc=4`），但跑完 02/11 后回到 8（文件时间 11:19）。回写者待定位
   （`11_pack_vda` 的 device.json 发布链或 02 的清单规范化），在那之前不要指望改 manifest
   能长期生效；好在根因已与本参数无关，smp=8 下同样稳定。
3. 自死锁窗口依赖「持锁期间同一 socket 有 delack timer 待处理」，`netif_rx` 只是把工作搬回
   软中断，并非消除并发；后续若在别的路径（如 tx 方向）也出现类似栈，按同一判据检查是否
   还有「进程上下文调 netif_*」的调用点。
