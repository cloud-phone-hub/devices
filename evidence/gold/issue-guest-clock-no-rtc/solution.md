# guest 没有 RTC：时钟停在镜像构建时刻（gold 云机，落后宿主 108 天）

- 设备：`xiaomi/gold`（Redmi Note 13 5G），云机实例 2，`c46c631991f2196b-cloud`
- 日期：2026-10-31
- 结果：宿主 daemon 在 guest-daemon 上线时注入宿主 UTC，**无需任何手工步骤**，
  每次 boot 自动对齐（本轮 4 次 boot 各 2 行日志），实测 guest 与宿主 epoch **完全相等**（差 0 s）

## 一、问题

云机开机后 `date -u` 显示的是**镜像构建那天**：

| | 值 |
| :--- | :--- |
| 宿主（macOS） | `2026-10-31 10:20:37 UTC`（epoch `1793442037`） |
| 云机（修前） | 与 `ro.build.date.utc = 1784120585` 一致，即 `2026-07-15 …`，**落后 108 天** |
| 真机 | 与宿主一致（有 RTC + 网络 NTP） |

后果不是「时间难看」这么简单：

1. `/data`、`/vendor` 里所有文件的 `atime/ctime/mtime` 都以错误时间为基准，
   `device-checker` 的 `ultimate.cloud_emulator` 命中 `U-DATA-ATIME`（`ageDays=0`）——
   真机 `ageDays=170` 不命中，属于**假阳性**，直接把云机判成"新装的伪造设备"。
2. logcat、`dmesg`、应用侧日志时间轴与宿主对不上，跨端排障时对不齐事件。

## 二、根因

- QEMU 这套构建**没有 RTC 设备**：`/dev/rtc*` 不存在、`/proc/driver/rtc` 不存在，
  所以 daemon 里 `-rtc base=localtime` 是空转；内核只能用编译期常量 `ro.build.date.utc` 起钟。
- 云机没有可用外网（emulated RIL `SETUP_DATA_CALL` 返回 `INSUFFICIENT_RESOURCES`，
  slirp `restrict=on`），`system_server` 的 NTP 也补不回来（`W BestClock: Missing network time fix`）。
- 因此只能在**宿主侧**把时间推给 guest，且要挑一个足够早的时机：
  guest 的 `system_server` 还没起、但 guest-daemon（root, `u:r:pioneer:s0`）已经能执行命令。

## 三、证据

- RTC 缺失（云机）：`ls: /dev/rtc*: No such file or directory`、
  `cat: /proc/driver/rtc: No such file or directory`、`getprop ro.build.date.utc` = `1784120585`。
- 时钟对齐（当前实例，宿主与 guest 同一秒抓）：
  `host-utc: 2026-10-31 10:20:37 (1793442037)` vs guest `2026-10-31 10:20:37 UTC` / `1793442037`。
- 注入时机（宿主 daemon 日志 `out/tmp/daemon-web.log`，每次 boot 两行，间隔约 15 s）：
  `daemon: 已把宿主时间 2026-10-31 09:57:40 UTC 写入 guest` /
  `daemon: 已把宿主时间 2026-10-31 09:57:56 UTC 写入 guest`（本轮 4 次 boot 共 8 行，见 `log-pass.txt`）。
- 修前设备侧证据见 `log-fail.txt`（`date -u` 停在 `2026-07-16`、`U-DATA-ATIME ageDays=0`）。

## 四、改动

1. `core/host/daemon/run/channel.go`
   - 新增 `syncGuestClock()`：把宿主 UTC 格式化成 `2006-01-02 15:04:05`，拼成
     `{"type":"js","script":"cmd.run(\"date -u -s '<UTC>'\")"}` 走已有 js 帧下发。
   - `onGuestEvent()`：guest 主动上报 `{"type":"status","daemon":"up"}` 时立刻注入一次
     （这是 guest-daemon 刚上线、`settimeofday` 可用的最早时机）。
   - `syncGuestClockAfter(6s)`：event 通道刚连上也补一次（兜底，防止 `status` 帧丢失）。
2. `core/host/daemon/run/manage.go`：`launchQemu` 成功后立刻 `channelFor(inst.Dir)`，
   让 event 通道尽早建立、尽早接上 `status{up}`。
3. 为什么用 `date -u -s` 而不是 `cmd alarm set-time`：后者要等 `system_server` 起来
   （boot 30 s 之后），而且 `cmd` 在 guest-daemon 的 SELinux 域里不一定可用；
   `date` 是 toybox 自带、root 下直接 `settimeofday`。

## 五、验证

- 每次 boot 的 daemon 日志都有两行 `已把宿主时间 … 写入 guest`，无需人工干预。
- 当前实例（已跑 20+ 分钟、跨一次重启）：host epoch `1793442037` = guest epoch `1793442037`。
- device-checker `run-00039`（18:10）云机 `ultimate.cloud_emulator` 的 diff 由 3 项 `U-DATA-ATIME`
  相关字段（`ageDays` / `earliestAtime` / `earliestAtimeSec`）收敛为**仅剩文件年龄差**
  （云机镜像构建于 2026-07-15、真机 2026-05-14，属真实差异，不是"时间错"）。
- 真机与云机 `date -u` 同时抓取：`Sat Oct 31 10:16:16 UTC 2026` / `Sat Oct 31 10:20:37 UTC 2026`（同一时区、同一天）。

## 六、遗留

- 只对齐"起点"，不做持续校时：guest 内核无 RTC，长期运行会按自身晶振漂移（本次 2 h 内未观察到秒级偏差）；
  若需要长期稳定，可在 daemon 里加周期任务（例如每小时重复一次 `syncGuestClock`）。
- 注入口子依赖 guest-daemon 的 event 通道；若该通道起不来（例如 daemon 二进制被换回旧版），
  时间会退回"停在上次写入值"，表现为 `date -u` 落后。此时看 daemon 日志有没有那两行即可定位。
- guest 的 `persist.sys.timezone` 是 `Asia/Shanghai`，与真机一致；未做时区对齐动作。
