# Issue: 逐机验收的「截图非黑」判据没有可用工具，RFB 客户端连写四轮才通

- 设备：跨机型（宿主运行侧）；本轮在 `xiaomi/gold` 实例上定位与验证
- 阶段：`core/host/daemon` 运行验收（QEMU `-vnc ...,websocket=` 抓帧），不涉及构建阶段代码
- 状态：**已闭环**（2026-11-04，gold 实例出帧 + 工具固化到 `core/host/daemon/tools/` + m22 用固化件复验）
- 关联：[#146 issue-in-tree-extras-stale-cdh-virt-wifi](../../gold/issue-in-tree-extras-stale-cdh-virt-wifi/solution.md)
  （同一轮逐机验收里 gold 撞上的另一件事）

## 一、问题

2026-11-04 定的逐机验收判据是「**引导完成 + 截图非黑**」，单台上限 10 分钟。第 1 台 gold
的 `qemu.log` 在 guest 89.29 s 就打出 `init: processing action (sys.boot_completed=1)`，
QEMU 侧也在持续出帧（`cgl-headless 大帧 1080x2400 … 总=1.9 ms`），但**截图这一半始终拿不到**：

1. 仓库里没有任何可复用的 websocket-VNC 抓帧件——`grep -rn "RFB 003"` 在非 `out/` 目录零命中。
   唯一线索是 [core/host/daemon/Display.md:215](../../../core/host/daemon/Display.md#L215)
   里那句「VNC 帧缓存即可截图（`out/tmp/vnc_grab.py`）」：**结论留档了，手段留在临时目录里，
   随 `out/tmp` 清理丢了**。
2. 现场重写一份（`engine/out/tmp/ws_shot.py`，纯标准库）后，依次撞上四种失败，
   每种都只把卡点往后挪一格（逐条原文见 [log-fail.txt](log-fail.txt)）：
   - 直连 TCP `5912`（不走 websocket）→ ServerInit 正常、帧恒全黑；
   - 假设 QEMU 的 `binary` 子协议每帧带 4 字节长度前缀 → `RuntimeError: 不是 RFB: b'\x82\x0cRFB 003.00'`；
   - WS 原始字节与解出的 RFB 字节共用一个缓冲 → 帧头漏进 RFB 流；
   - **修完前三条后**：RFB 握手全程正常，但发出 `FramebufferUpdateRequest` 后服务端
     15 s 内一帧不回（`recv` 超时）。最后一轮才是真卡点，前两处错在同一段代码里。

## 二、根因

四处全是**客户端侧协议写错**，与 QEMU、daemon、guest 都无关（探针逐帧输出见 log-fail §失败 4）：

| # | 错在哪 | 正确口径 |
| :--- | :--- | :--- |
| 1 | 走 TCP VNC 口 | `-display cgl-headless,gl=on,readback=on` 只在 GL scanout 路径出帧，必须连 **websocket 口 = `running[].vnc + 1`**（`run/qemu.go` 起的是 `-vnc 127.0.0.1:<N>,websocket=<N+1>,share=force-shared`） |
| 2 | 以为 `binary` 子协议帧载荷前有 4 字节长度 | 选了 `binary` 后 WS 帧载荷就是**裸 RFB 字节**；实测首帧 `\x82\x0c` + `RFB 003.008\n` |
| 3 | 一个缓冲同时装「未解析 WS 字节」和「已解出 RFB 字节」 | 两者必须分开：`raw` 等帧头凑齐再切载荷，`queue` 只放解出的 RFB 字节 |
| 4 | `FramebufferUpdateRequest` 消息类型写成 **0**，且没先发 `SetEncodings` | 客户端方向是 **3**（`0` 是服务端方向的 FramebufferUpdate）；QEMU 收到未知客户端消息**不回**，表象就是 recv 超时。请求前先发类型 2 的 `SetEncodings`，编码表 `[0 /* raw */]` |
| 5 | `PIXEL_FORMAT` 偏移整体错一位 | 16 字节布局是 `bpp, depth, bigEndian, trueColour, redMax, greenMax, blueMax, redShift, greenShift, blueShift, pad[3]`——**trueColour 之后没有 3 字节 pad**；三个 shift 落在整包第 14~16 字节（实测 `10 08 00` = 16/8/0）。按 `init[15:18]` 读会得到 `[8,0,0]` |

第 4、5 条同时解释了「黑屏」的另一半：即便出帧，shift 取错也会把通道映射搞反。

## 三、证据

- 失败原文（四种现象 + 逐帧探针输出）：[log-fail.txt](log-fail.txt)
- 通过原文（boot_completed 行、出帧尺寸与亮度、`[pass]` 判定、固化后工具的自检）：[log-pass.txt](log-pass.txt)
- 实测帧尺寸 **1088x2400** 而机型标称 1080x2400：`device.json` 的 width 是 guest 显示模式，
  QEMU/VNC 报的是按 16 像素对齐后的 scanout 宽，两者不是一回事。
- 同一份 ServerInit 在探针里是 28 字节的单个 WS 帧（24 字节 ServerInit + 4 字节 name 同帧到达），
  说明按「消息边界」读 WS 帧再按「字节流」读 RFB 才是对的拆法。

## 四、改动

只加运行侧工具，不动 QEMU、不动 daemon Go 代码、不动构建阶段：

1. 新增 [core/host/daemon/tools/vnc_grab.py](../../../core/host/daemon/tools/vnc_grab.py)：
   与本轮跑通的临时脚本同源，另补 `SetEncodings`、`setColourMapEntries` / `Cursor` 跳过分支、
   32bpp + trueColour 守卫，并把上面五条协议要点写进模块 docstring（**结论和手段放一起，
   下次不必再猜**）。
2. 新增 [core/host/daemon/tools/boot_check.py](../../../core/host/daemon/tools/boot_check.py)：
   一条命令做完「从 `/api/state` 找实例 → 轮询 `qemu.log` 的 `sys.boot_completed=1` →
   websocket 抓帧 → 按平均亮度判非黑」，退出码 0/1/2 分别对应 pass / fail / 实例找不到。
   端口只从 `running[]` 读（slot 与端口段不单调对应）。
3. 修正 [core/host/daemon/Display.md:215](../../../core/host/daemon/Display.md#L215) 里指向
   `out/tmp/vnc_grab.py` 的那句，改指固化后的路径。

**为什么不留在 `engine/out/tmp/`**：AGENTS.md 第 7 条约束的是「临时件」，而逐机验收每轮都要用它、
且它承载的是协议结论。放临时目录等于把同一个坑再埋一次——本轮的起点正是
`Display.md` 指着一份已经不存在的脚本。

## 五、验证

- gold 实例（`spec=gold_cn_OS3.0.10.0.VNQCNXM` `index=1`，websocket 5913）出帧：
  `1088x2400 bpp=32 平均亮度=121.1 峰值=255`，real 1.08 s；
- 人眼核对 PNG：完整 MIUI launcher（状态栏 5:27 / 4G / WiFi / 100% + 12 个图标 + dock 4 个），
  颜色正常，说明 shift 通道映射对；
- 正式判定一条命令过：`[pass] 引导完成（日志 15711 行）+ 截图已落 …/gold-desktop.png`，
  PNG 3 442 376 B；
- 固化后的两份脚本 `python3 -m py_compile` 通过；`boot_check.py no_such_spec` 走
  实例找不到的分支返回 2；`vnc_grab.py` 无参打印用法。
- **固化件在活实例上复验**（第 2 台 m22，本轮唯一一次用 `tools/` 里的成品跑验收）：
  `python3 core/host/daemon/tools/boot_check.py m22_12.5.0.0A --timeout 600` →
  `[pass] 引导完成（日志 7869 行）+ 截图非黑（平均亮度=147.9 峰值=252）`，
  退出码 0，PNG 落 `engine/out/tmp/m22-desktop.png`（画面是 Flyme 锁屏，非黑判据成立）。
  即：`out/tmp` 的临时件与 `tools/` 的固化件行为一致，下一台不必再回临时目录。
- 逐机验收的 pass 判据本轮第一次具备**可机检**形态（亮度阈值 `--min-bright`，默认 2）。

## 六、遗留

- **黑屏阈值是拍的**：`--min-bright 2` 只区分「全黑 vs 有内容」。guest 灭屏（`mWakefulness=Dozing`）
  会判 fail，但那是另一回事——`docker/init/README.md` 的排错表里就有「截图全黑 → 唤醒重抓」
  （`adb shell input keyevent 224` + `wm dismiss-keyguard`）。要不要把「灭屏」和「引导没到 launcher」
  分开判，等逐机跑完再定。
- **只支持 security type None + raw 编码**：QEMU 这条链路没口令、推的就是 raw；若以后给 VNC 加
  认证或换 tight/zrle，需要扩 `rfb_init` 与解码路径（宿主无 PIL，压缩编码得自己解）。
- **没做进 `cdh` 子命令**：抓帧仍是 python 脚本。要不要在 `engine/out/bin/cdh` 加一个
  `device check <spec>`（Go 侧直接实现 WS+RFB），等 7 台跑完看使用频率再定，避免为一次性动作加面。
- **帧宽 1088 未按 1080 裁**：判据只关心非黑，右侧 8 列是 scanout 对齐 padding。真要逐像素对账
  （比如比对 UI 布局），得先确认 guest 侧该出 1080 还是 1088 的模式。
- **`Display.md` 其余提到 `out/tmp` 的历史件**没逐条复核，本条只改了 M1 那一行。
