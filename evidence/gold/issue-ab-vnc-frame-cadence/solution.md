# VNC 帧节奏 A/B：engine 与 gold-15 等速（无可测差异）

- 设备：xiaomi/gold（Redmi Note 13 5G）
- 宿主：macOS (darwin, QEMU 11.0.4 + hvf)
- 日期：2026-10-26 晚
- 状态：**已闭环**（结论：两条线帧交付无可测差异；局限见第 6 节）
- A 臂：`gold_cn_OS3.0.10.0.VNQCNXM` 实例 1（gold-15 原厂 ROM，第二 daemon 9060，adb 5919）
- B 臂：`gold_OS3.0.10.0.VNQCNXM` 实例 38（engine，21:10 版 patch，md5 `f2a80ff1…`）
- 数据：`log-pass.txt`（各轮实测表 + 采样方法）
- 反向对照：`log-fail.txt`（为什么必须做 A/B，以及第一版手势脚本的干扰项）
- 关联：`../issue-mnt-patch-vold-teardown/`（B 臂所用镜像即那里的 merge-v5 分支；本 A/B 同时
  证明 composer 代际差异——A 臂 HIDL 2.4 vs B 臂 AIDL `hwc3-service.drm`——不影响帧节奏）

## 1. 问题

用户问「VNC 流畅了吗」。这个问题不能靠肉眼看 noVNC 画面回答：

- 「卡」既可以来自 guest 侧渲染（SurfaceFlinger/HWC 掉帧），也可以来自宿主侧
  readback、VNC 编码传输、noVNC 客户端解码；
- engine 与 gold-15 的**合成器代际不同**（engine 是 AIDL `hwc3-service.drm`，
  gold-15 是 HIDL `composer@2.4` + `hwcomposer.drm.so`），仅凭「看起来一样」
  无法判断这个差异是否影响帧节奏；
- 单次目测无法区分「两条线的差异」与「同一臂两轮之间的自然波动」。

因此需要一次**同脚本、双端采样、每臂两轮**的 A/B：把「手势输入」和「测量口径」
都固定住，只让镜像这一个变量变化。

## 2. 根因（测量设计）

要判定「是否等速」，必须同时抓住两端：

1. **guest 侧交付**：`dumpsys gfxinfo <pkg>` 的
   `Total frames rendered` / `Janky frames:` / `50th|90th|99th percentile`。
   它反映 App 线程与 RenderThread 的实际产出，但**不反映**帧是否真的送到了宿主机。
2. **宿主侧接收**：QEMU 的 `cgl-headless` 后端在每次大帧 readback 时打印
   `cgl-headless 大帧 … 总=<N> ms`，以及 `cgl-headless flush #<n>`。
   统计其累计条数随时间的增量，就是宿主真正拿到的帧率。

只用其中任一端都会误判：guest 掉帧但宿主正常 = 渲染问题；
guest 正常但宿主帧数少 = readback/VNC 传输问题。

## 3. 证据

### 3.1 采样方法

- `sampler.py`：每 0.3 s 对 qemu.log 跑一次 `grep -c "cgl-headless 大帧"`，
  输出 `t_sec \t cum_frames` 序列；
- `analyze.py`：把相邻采样点换算成瞬时帧率（`Δ帧 / Δt`），给出总帧、窗口、
  平均 fps、瞬时 p50/p10、空转采样数（瞬时帧率 <1 的采样点）、采样间隔 p90；
- `guest.sh`：经 daemon 的 `POST /api/channel/cmd` 下发 shell 命令，从
  `/api/channel/events` 回收输出（不依赖 adb，避免与另一路会话抢 adb 通道）。

### 3.2 三套手势脚本（逐次收敛干扰项）

| 脚本 | 手势 | 为什么改 |
|---|---|---|
| `run-arm.sh` | HOME → 打开设置 → 5 次滑动 | 首版；「回桌面再进设置」引入桌面搜索浮层等不确定状态 |
| `run-arm2.sh` | 假定设置页已在前台：reset → 5 次同距滑动 → dump | 去掉导航步骤，缩短计时段 |
| `run-arm3.sh` | HOME → 通知栏 `expand-notifications`/`collapse` ×5 | 通知栏动画路径固定、无导航风险，作为跨臂的稳定负载 |

最终对照数据以 `run-arm3.sh` 的通知栏动画口径为准：它不依赖 Settings 的滚动
容器状态，两臂负载等价。

### 3.3 实测结果（每臂两轮）

| 轮次 | 总帧 | 窗口 | 平均 | 瞬时 p50 |
|---|---|---|---|---|
| A gold-15 | 366 / 364 | 21.5 / 21.6 s | 17.0 / 16.9 fps | 10.1 / 6.1 |
| B engine | 392 / 367 | 21.7 / 22.0 s | 18.0 / 16.7 fps | 13.3 / 4.4 |

- 宿主 readback 中位 **2.1–2.5 ms**，最大 21 ms；
- guest `gfxinfo` 只采到 A 臂（195 / 184 帧，janky 1.54% / 3.26%），
  B 臂该轮未采到完整输出。

### 3.4 判读

- 两臂平均帧率差（17.0 vs 18.0、16.9 vs 16.7）**小于同臂两轮之间的波动**
  （A 臂 17.0→16.9 与 B 臂 18.0→16.7 的跨度都覆盖了对方）；
- 瞬时 p50 的差异（10.1/6.1 vs 13.3/4.4）同样在轮间波动量级内；
- **17 fps 是通知栏动画负载本身的量级**，不是 VNC 丢帧 —— 宿主 readback 中位
  仅 2.1–2.5 ms，说明每一帧都被及时取走，瓶颈在动画自身节奏。

## 4. 改动

无源码改动。本轮新增的是**测量工具**（保留在 `.tmp/ab/`，未进构建链）：

- `guest.sh` —— daemon 通道下发/回收
- `sampler.py` / `analyze.py` —— 宿主帧计数采样与换算
- `run-arm.sh` / `run-arm2.sh` / `run-arm3.sh` —— 三套等价负载脚本
- `shot.py` —— VNC 截图取帧

## 5. 验证

`log-pass.txt` 记录了：三套脚本的用途差异、`run-arm3.sh` 的完整命令、
两臂两轮的实测表、readback 中位/最大值，以及「差异小于轮间波动」的判读依据。

## 6. 遗留

1. **原始采样文件未留档**：`/tmp/ab_*.seg` 与 `sampler.py` 输出的 tsv 已随
   `/tmp` 清理，本轮只保留了汇总表（见 `log-pass.txt`）。要复现需重跑
   `sampler.py` + `run-arm3.sh`。
2. **noVNC 客户端未量化**：本次只测到「宿主取帧」为止，浏览器解码/合成/
   显示这一段完全没测。用户看到的「卡」若发生在客户端，本结论不覆盖。
3. **满帧场景未覆盖**：17 fps 是通知栏动画负载；视频播放、列表快速滚动等
   高负载场景、以及能否打满 60 fps 未测。
4. **B 臂只覆盖 21:10 的 patch**：结论不能外推到后续产物（后续为修 `cmd`/声卡
   重打过的镜像均未重跑本 A/B）。
5. **engine 在密集 `dumpsys` 压测下出现过 ANR**：同一实例上并发执行多条
   `dumpsys` 时 system_server 无响应，说明压测口径本身对 guest 有侵入性，
   后续采样应避免在计时段内跑重命令。
