# issue-m22-instance31-binder-stall — m22 实例 31 界面卡死（binder async 池被打满）

- **设备**：`flyme/m22` 12.5.0.0A，云机实例 31（console 5918 / adb 5919 / vnc 5920）
- **内核**：cloud-phone-hub 6.12.81-4k-cdh（`#1 SMP PREEMPT Thu Oct 29 02:52:55 UTC 2026`，DMIID 已关）
- **状态**：已恢复；根因落到 **binder 层**，并在运行态用**开关自动亮度**做了双向 A/B 验证
  （关后新投递归零、开回 90 秒内恢复）；串口日志背压是**加速器**。镜像层落地待做
- **日期**：2026-10-29（11:02 首次处置 / 11:44 推翻 loglevel 单因 / 12:44 自动亮度 A/B 闭环）

## 1. 问题

实例 31 在 noVNC 里**卡死**：画面停在一帧（首轮停在「设置 → 我的手机」），点击无响应。
`adb shell` 仍能进去，但系统服务已不再正常应答：

```
$ adb shell dumpsys SurfaceFlinger
Error with service 'SurfaceFlinger' while dumping: FAILED_TRANSACTION
$ adb shell ps -A -o PID,NAME | grep -E 'system_server|surfaceflinger'
861 surfaceflinger
972 system_server
```

两个进程都在、宿主 QEMU 400%+ CPU（8 vCPU 里约 4 核跑满），但界面冻结。

**这是间歇性问题，不是每次必现**：同一镜像的实例 29 连跑 28 分钟干净退出
（`spamming oneway` 0 次、`DeadSystem` 0 条），而 28/30/31 都卡过。

## 2. 根因

一句话：**开启自动亮度后，surfaceflinger 的内容采样线程（`RegionSampling`）会以每秒一对的节奏，
向 system_server 投递 two 个 oneway 事务（`code1 size92` + `code2 size436`，事务体携带 SF 自己的
binder 对象）；system_server 侧这段 async 队列长期积压 ~900 条、队首常驻 20 分钟以上。
"净增长"一旦为正（system_server 被拖慢、消费落后），队列就单调增长，涨到 ≈985 对
（512 KiB async 空间）后池子打满 → 所有 async `-28 (ENOSPC)` → 客户端把 system_server
判成 dead → 全线 `DeadSystemException` → 画面冻在最后一帧。**

### 2.1 binder 层现场（2026-10-29 第 5 轮，12:09:17 起；12:28–12:44 连续取样）

挂 debugfs 直接读 `/sys/kernel/debug/binder/`（原始输出见 `binder-dump-769.txt`）：

```
# /sys/kernel/debug/binder/stats —— 156 个进程里只有 system_server 的 async 池不是满额
     155   free async space 520192
       1   free async space 273888      ← pid 769 system_server（约 47% 已用）
# /sys/kernel/debug/binder/proc/769 （941 条 pending，全部同形）
pending async transaction 105409: 0000000000000000 from 582:682 to 769:0 code 1 flags 11 pri 0:120 r0 elapsed 1254470ms node 9312 size 92:0  offset 22a18
pending async transaction 105410: 0000000000000000 from 582:682 to 769:0 code 2 flags 11 pri 0:120 r0 elapsed 1254470ms node 9312 size 436:24 offset 22a78
```

- **发送方 `582:682`**：pid 582 = `surfaceflinger`，**tid 682 的线程名 = `RegionSampling`**。
- **目标 `769:0`**：pid 769 = `system_server`，线程 0 = 任意线程。
- `flags 11` = `ONE_WAY | ASYNC`；`code 1 size 92` 与 `code 2 size 436` 严格成对，条数相等。
- 按发送者汇总（942 条 = 471 对）：**100% 来自 surfaceflinger，没有第二个来源**。
- `elapsed 1254470ms` ≈ 开机至今（20.9 min）：**队首这条从开机起就没被消费过**。
- `node 9312 … ls 943 … proc 582`：该 node 属 surfaceflinger，每个未消费事务各持 1 个
  strong ref，所以 `ls` 恒等于 pending 条数（943 : 942）。这既是"引用泄漏"，也是池子
  被占用的记账口径：**队列深度 × 528 B = 已用 async 空间**（450 对 ≈ 246 KB，实测
  520192−273888 = 246304 ✓）。

### 2.2 队列在动，但永远追不上

对同一份 `proc/769` 相隔 30 秒取两次快照、按事务 id 做差：

```
取样 4-a  918 条（队首 id 104269）
取样 4-b  912 条（间隔 30 s：消费 12 条，id ≤ 104527，全在队首；新增 6 条，id 148475）
```

- 队列是 FIFO、**确实在推进**，只是极慢（≈0.4 条/s）；深度常年停在 **900 条上下**，
  队首永远是开机时刻的事务。
- 也就是说：system_server 的消费速度只是"勉强持平"，**没有任何余量**。生产略快于消费
  （负载升高、调度抖动、日志背压）→ 深度单调增长 → 约 985 对时打满。
- 因此崩溃时间不是常数，而是**净增长率的函数**：本轮（12:09:17 起）到 12:44 仍存活，
  深度稳定在 900 附近，`free async space` 反而从 240848 涨到 273888（队列在被消化）。

### 2.3 决定性 A/B：自动亮度就是开关

guest 里 `settings get system screen_brightness_mode` = **1**（自动亮度开）。以「最新一条 pending
的 `elapsed`」作为生产速率的指纹（生产停了，它就会跟着墙钟长；生产在跑，它就会重新变小）：

| 操作 | 观察窗 | 最新一条 `elapsed` | pending 条数 | 含义 |
| :--- | :--- | :--- | :--- | :--- |
| 自动亮度开（1） | 基线 | 29 407 ms | 941 | 每秒一对在持续投递 |
| `settings put system screen_brightness_mode 0` | 后 2 min | 171 001 → **291 042** | 762 → 714 | **恰好 +120 s，零新增** → 生产停了 |
| 再改回 1 | 后 90 s | **50 419 ms**（重新变小） | 702 → 676 | 生产立刻恢复 |
| 再改回 0（收尾） | — | — | 666，`free async space 333248` | 队列继续被消化 |

→ **自动亮度 = 内容采样开关**：关掉后 SF 的 `RegionSampling` 再没有一条新投递，
开回来 90 秒内又恢复「每秒一对」。云机没有环境光传感器，自动亮度本无意义，
却正好是这条 oneway 洪泛的唯一来源。

### 2.4 串口日志背压是加速器，不是唯一触发源

**A/B 对照（11:20–11:37，同一实例同一镜像，只改 console 日志级别）**：

| 阶段 | `printk` | `cannot allocate buffer` | `spamming oneway` | crash FATAL |
| :--- | :--- | :--- | :--- | :--- |
| 基线（开机 3 分钟才设） | 7 → 4 | 1（10 分钟内不增长） | 0 | 1 |
| **恢复 7，加压 30 s** | → 7 | **1 → 26** | **0 → 148** | **1 → 534** |
| 再压回 4（不重启） | → 4 | 60 → 80 → 99 → **118** | 167 → 191 | 1634 → 4036 |
| **boot+53 s 就设 4，重启跑同负载** | 4 | **72（11:44 仍崩）** | **384** | — |

- loglevel=7 时 `printk` 在 `console_lock` 上同步排队（串口另一端是 QEMU
  `-chardev stdio,mux=on`，落盘到移动硬盘的 `qemu.log`），把 system_server 的消费速度
  进一步压低，30 秒内就把净增长打成明显正数。
- **但 loglevel=4 不是解药**：第 4 行那轮在 boot+53 s 就把 console_loglevel 压到 4，
  仍在 11:44 崩（ENOSPC 72 / spam 384 / DeadSystem 1442）。它只把崩点从开机 **323 s
  拖到 434 s 以上**，属于延缓。
- 池子一旦打满就**不可逆**（第 3 行：压回 4 仍 60→118→118），只能重启进程。

日志洪泛源（把上面那点"余量"吃掉的东西）：

1. servicemanager 对三个**云机上不存在**的 HAL 每秒重试、永不超时：
   `android.hardware.biometrics.fingerprint.virtualhal.IVirtualHal/virtual`、
   `…face.virtualhal.IVirtualHal/virtual`、`android.hardware.nfc.INfc/default`
   （`*virtualhal*` 是 Cuttlefish 专用虚拟 HAL，Flyme 真机镜像注册了服务名，云机没有实现）。
   调用方 `ServiceManagerCppClient: Waited one second for …` 7 分钟累计 **442 / 441 / 439 次**，
   分别来自 system_server 的线程 1031、1032 和 **com.android.nfc 主线程**——后者被永久阻塞，
   正是 `ANR in com.android.nfc (Broadcast … BOOT_COMPLETED)` 的成因。
2. `/proc/cmdline` 带 **`androidboot.seriallogging=hvc1`**，logcat 整体镜像到同一个串口。
3. `cdh_redirect: /mnt/patch/vendor/lib64`（`CONFIG_CDH_PATH_REDIRECT`）在高频路径逐次打印，
   已触发内核限流（`cdh_redirect_vendor_path: 12 callbacks suppressed`）。

**排除项**：

- **不是内存问题**：`MemAvailable` 4.8 GB；`top` 里 “Swap 6291452K used” 是显示 hook 的假值，
  `/proc/swaps` 为空、`free -m` 的 Swap 是 0。
- **与本次内核改动（`CONFIG_DMIID` 关闭）无关**：改动前的实例 28（10:05 起）与 30（10:55 起）
  的 `qemu.log` 里同样有 `spamming oneway` 686 / 881 次、`DeadSystem` 64656 / 66611 条。
- **不是动态壁纸单独造成**：首轮把 `com.flyme.alivewallpaper`（GLSL ES 3.20 编译失败，13 轮）
  当主因并禁用，但禁用后仍复发；它只是众多日志/崩溃源之一。
- **不是 overlay 抖动**：`ApplicationInfo updating` 的 3046 次风暴集中在 20:59:35–20:59:4x，
  **晚于** `DeadSystemException` 起点 20:57:35，是卡死后的连锁反应，不是触发源。

## 3. 证据

| 文件 | 内容 |
| :--- | :--- |
| `screen-stuck-31.png` | 卡死时的 VNC 帧（停在「我的手机」） |
| `screen-after-fix-31.png` | 关自动亮度后（12:49）的 RFB 抓帧：页面正常渲染、可交互 |
| `dmesg-31.txt` | 17,338 行全量内核日志：`spamming oneway` 147 条、`cannot allocate buffer` 136 条、`audit_lost=6884` |
| `logcat-31.txt` | 28,920 行（crash + system + main）：`DeadSystemException` 475 条 |
| `log-fail.txt` | 卡死现场的时间线摘录与「不是内存问题」的核对 |
| `log-pass.txt` | 首次处置 + A/B + binder 层取证（第 8 节为最终定因） |
| `binder-dump-769.txt` | system_server 的 async 池原始输出：`free async space`、941 条 pending、`node 9312`、30 秒差分 |

关键时间（内核 uptime；guest 墙钟走镜像 RTC，显示 06-25 20:5x）：

```
[ 323.3] binder_alloc: 972: pid 861 spamming oneway? 1486 buffers allocated for a total size of 416080
[ 366.1] binder: cannot allocate buffer: no space left        （共 134 条）
[ 456.4] binder: 861:986 transaction async to 972:0 failed 856391/29201/-28, code 1 size 92-0
[ 491.7] binder_alloc: 972: pid 861 spamming oneway? 1868 buffers allocated for a total size of 520096
```

各实例 `qemu.log` 的规模本身就是这个机制的旁证——日志越猛，越容易卡：

| 实例 | qemu.log | 运行时长 | 平均速率 | 结果 |
| :--- | :--- | :--- | :--- | :--- |
| 28 | 176.9 MB | 约 22 min | ≈133 KB/s | 卡死 |
| 30 | 148.4 MB | 约 25 min | ≈100 KB/s | 卡死 |
| 29 | 19.9 MB | 28 min | ≈12 KB/s | 干净退出 |

## 4. 改动

| 位置 | 改动 | 效果 |
| :--- | :--- | :--- |
| guest（root，经 daemon hvc7 通道） | `echo 4 > /proc/sys/kernel/printk` | **决定性**：验证期 10 分钟不复发 |
| guest（实例 31 的 userdata） | `pm disable-user --user 0 com.android.nfc` / `com.meizu.mznfcpay` / `vendor.qti.iwlan` | 无效：`com.android.nfc` 进程仍被 `NfcService` 拉起并等 HAL |
| guest（同上） | `pm disable-user --user 0 com.flyme.alivewallpaper` / `com.meizu.activeviewlivewallpaper` | 单独无效（首轮试过，仍复发） |
| guest（同上） | `pm disable-user --user 0 com.qualcomm.qti.cne` | **重启后被 PMS 恢复**（该包带 `PERSISTENT`），不可用此路禁用 |
| 实例 31 | `/api/stop` → `/api/start`（复用实例目录，保留 writable-data） | 每次崩溃后必须重启，池满不可自愈 |
| guest（只读） | `mount -t debugfs none /sys/kernel/debug` 后读 `binder/{stats,proc/<pid>,state}` | 定位到 SF `RegionSampling` → system_server 的 async 积压，见 2.1 |
| guest（实例 31，决定性） | `settings put system screen_brightness_mode 0`（关自动亮度） | **源头断流**：最新一条 pending 的 `elapsed` 从 171 s 长到 291 s（零新增），队列从 941 条一路降到 666 条 |

`/proc/sys/kernel/printk` 不是持久设置，**重启即失效**，所以当前手段只能算临时缓解。

## 5. 验证

- **基线（不动任何东西）**：开机后 5.5 分钟必崩——`spamming oneway` 从 0 起涨，
  同期 `DeadSystemException` 开始级联。
- **loglevel=4（开机后 3 分钟才设）**：11:23:40 → 11:32:16，**10 分钟**内
  `cannot allocate buffer` 停在 1、`spamming oneway` 停在 0；期间做了 30 次快速滑动、
  连续开关设置页、横滑桌面等真实交互，均不复发。
- **反证（恢复 7）**：仅 30 秒加压，ENOSPC 1→26、spam 0→148、FATAL 1→534；再压回 4 仍继续
  恶化到 118/191/4036——证明「7 是触发条件，且池满后不可逆」。
- 三次重启均用 `/api/start` 复用实例目录，端口恒定 5918/5919/5920，`boot_completed` 约 50 s。
- **第 5 轮（12:09:17 起，boot+59 s 设 loglevel=4）**：到 12:45 仍存活（>35 分钟，超过
  之前所有崩点），`cannot allocate buffer` / `spamming oneway` / `DeadSystem` **全为 0**，
  `mCurrentFocus` 正常切页；同期 system_server 的 async 用量稳定在 46–53%，
  `free async space` 从 240848 → 250368 → 252608 → 273888（**回升**，队列在被消化）。
  → 说明这不是「必崩」，而是「净增长率 > 0 才崩」。
- **决定性 A/B（12:37–12:44）**：关自动亮度后，最新一条 pending 的 `elapsed` 紧贴墙钟增长
  （171 001 → 291 042 ms，间隔 120 s **零新增**），队列 941 → 828 → 666 持续下滑、
  `free async space` 273888 → 333248；改回自动亮度 90 秒后最新 `elapsed` 骤降到 50 419 ms
  （生产立即恢复）。**同轮内双向可逆**，坐实因果关系。

## 6. 遗留

1. **已找到开关，但消费端的 bug 还在**：关自动亮度能彻底断流（见 2.3），所以
   **首选落地手段 = 镜像层默认关自动亮度**（云机没有环境光传感器，此项本就无意义）：
   - 首选：`device.json` / `guest` 层在首启时写 `settings put system screen_brightness_mode 0`
     （或镜像层 `defaults` 直接设初值），不动 framework。
   - 遗留风险：只要用户手动打开自动亮度，依然可能复发（system_server 侧消费只有 ≈0.4 条/s，
     而 SF 投递 ≈2 条/s，从机制上看这本就是个 bug）。彻底修需查清消费端为何这么慢
     （候选：每次回调都要同步回查 SF，而 `dumpsys SurfaceFlinger` 现在稳定返回
     `FAILED_TRANSACTION`，怀疑 SF 侧有同步路径故障形成反馈堵塞）。
   - code1/code2 对应的 AIDL 尚未查实（已确认发送线程名 `RegionSampling`、目标 system_server、
     尺寸 92/436）；已试 `grep ISamplingListener libgui.so` = 0，下一步查 `services.vdex`。
2. **loglevel=4 + 去掉 `androidboot.seriallogging=hvc1` 必须落镜像**（只是延缓，但可以
   把消费端余量腾出来）：
   - **首选**：在 boot.img 的 bootconfig / GRUB cmdline 里加 `loglevel=4`，并去掉
     **`androidboot.seriallogging=hvc1`**（logcat 不再灌串口，这是 5.8 KB/s 的主体）。
   - 次选：把 `cdh_redirect` 的 printk 降到 `pr_debug`（内核侧改动）。
   - 兜底：daemon 在拉起 QEMU 后、guest daemon 通道一可用就自动执行
     `echo 4 > /proc/sys/kernel/printk`（本次实测 boot 后 53–59 s 可执行）。
3. **三个缺失 HAL 每秒重试、永不超时**是日志洪泛的源头，也是 `ANR in com.android.nfc` 的成因。
   治本要在镜像层让这些服务"秒失败"（VINTF 里删掉这几条，或补一个空的虚拟 HAL 实现），
   而不是靠禁用包——`com.android.nfc` 是 `NfcService` 主动拉起的，`pm disable-user` 挡不住。
4. **`com.qualcomm.qti.cne` 带 `PERSISTENT`**，`pm disable-user` 重启后被 PMS 恢复；
   它因 `vendor.qti.iwlan` 的 JNI 库缺失持续崩（开机 16 轮），要治只能在 patch 层补库或去掉该 APK。
5. **没有在旧内核上复现**：严格排除「DMIID 关闭」的关联，需用旧 `vda`（`DMIID=y`）起一轮对比。
   当前依据是 28/30 旧盘同样崩，且崩溃栈全在用户态。
6. 实例 30 的目录（`runtime/m22_12.5.0.0A/30`，`qemu.log` 148 MB）与 28 的（176 MB）仍在，
   确认无用后可删。
