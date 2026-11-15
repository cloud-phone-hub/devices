# 宿主 WiFi → 云机 guest WiFi 数据通路与稳定性（gold / Redmi Note 13 5G，Android 15）

- 设备：Xiaomi Redmi Note 13 5G（gold-15，`gold_cn_OS3.0.10.0.VNQCNXM`）
- 宿主：macOS（darwin, QEMU + hvf），实例 5
- 组件：`qemu/backend/wifi-host-service`（Go，宿主侧 L2 网关）
- 关联：#18 [issue-wifi-connection-unstable](../issue-wifi-connection-unstable/solution.md)
  （guest 侧内核假 AP / beacon / ack 伪造，本记录的输入前提）

## 问题

v8 模拟 Wi-Fi 的链路层已经打通（guest 能连 CDH-A、拿到 192.168.60.100、能走
DHCP/DNS/HTTP），但作为「宿主 WiFi 出口」长期不可用，表现为四类互相独立的故障：

| # | 现象 | 观测 |
| :--- | :--- | :--- |
| A | 外网 TCP 间歇不可达 | 60 轮 DNS+HTTP 采样 **52/60** 成功，`nc: Timeout`；30 分钟长稳 **169/180**，失败轮次全是 `baidu=` 空 |
| B | 大流量下载永久卡死 | 20MB 下载只到约 15KB 就停；宿主侧 `[link] 发送队列满，丢帧（累计 12200）` |
| C | 建连偶发无响应 | 约 0.5% 的 SYN 无 SYN-ACK 回包（guest 卡 SYN_SENT 直到超时）；captive 校验偶发空响应（100 轮 1 次、修复后 120 轮第 107 轮 1 次） |
| D | 响应被截断 | 修复流控后下载拿到 **206 字节**（仅 HTTP 响应头）即 EOF；另一轮拿到 **65,906 字节 / 10s** 后停住 |
| E | 空闲后链路静默 | guest 侧 2 分钟无帧，宿主 `[link] 静默 2m0s 无帧，重置连接（看门狗）` 自愈 |
| F | 上行数据静默缺失 | 20MB 上传宿主只收到 **20,968,304** 字节（少 3,216），sha256 与源不一致，网关无任何报错 |

## 根因

1. **DNS 上游地理错配**（→ A）。默认上游是 `8.8.8.8`，对 CN 域名返回香港/海外
   CDN 节点（实测 `www.baidu.com`：`@8.8.8.8 → 103.235.46.96 / 103.235.47.188`（HK），
   `@223.5.5.5 → 180.101.49.44 / 180.101.51.73`（CN））。宿主直连这些海外 IP 的
   SYN 会间歇超时（修复当日探测 HK 节点 19/20、CN 节点 20/20；该丢包率随网络
   状态波动，2026-10-28 复测两者均 20/20）。表现为「DNS 成功、TCP 随机失败」，
   且失败面板与 DNS 返回的节点归属一一对应。
2. **下行没有端到端流控**（→ B）。宿主链路 `Link.Send` 是非阻塞入队，队列满即丢帧
   （`outQueueLen=1024`）；网关的 TCP 精简 server 不做重传。泵上游时若灌得比
   guest 消费快，队列溢出丢帧 → 该连接永久停顿（seq 出现空洞，永远等不到）。
3. **没有 SYN-ACK 重传**（→ C）。网关只在出口拨号成功时发一次 SYN-ACK；这一帧在
   hvc10 上丢掉，guest 只能卡在 SYN_SENT。
4. **SYN-ACK 重发用 `mySeq-1` 推导序号**（→ D）。只要数据已经泵出去，`mySeq-1`
   就不再是 ISN；guest 收到序号错误的 SYN-ACK 会重设接收窗口，把后续数据整段当
   重复丢弃——握手"成功"但应用收到 0 字节。
5. **握手 ACK 到达前就开始泵下游**（→ D）。guest 在 SYN_SENT 状态会丢弃数据段，
   而已经从上游 `Read` 出来的字节无法回退，响应开头被永久丢掉。
6. **guest FIN 被当成全关闭**（→ D）。HTTP 客户端（尤其 `toybox nc`）发完请求后
   `shutdown(SHUT_WR)` 发 FIN；网关收到 FIN 立即关上游并回 FIN，响应只剩已经发出
   的头（206 字节）。
7. **hvc10 双向静默**（→ E）。QEMU chardev 侧会偶发长时间不投递字节；无法从流内
   恢复，只能靠读超时重置连接（`idleTimeout=120s`）。
8. **guest→host 方向不校验 TCP seq**（→ F）。`ingest` 只按「收到多少」推进
   `guestSeq` 并乐观 ACK：链路丢掉一帧后，空洞之后的段被直接写进上游流，ACK 也
   越过空洞，guest 永远不重传——上游文件静默缺一段。FIN 同样未校验序号，提前
   确认会把缺段永久截掉。
9. **captive 204/FIN 发出即置 Closed**（→ C 残留）。204 或 FIN 在 hvc10 上丢帧时
   guest 会重传同一请求，但连接已 Closed，重传被完全忽略，guest 只能等到校验
   超时（120 轮长稳里偶发 1 次 `captive=` 空）。

> 根因 4/5/6 是同一处「TCP 状态机语义」问题：网关把 `sendSegment` 的副作用
> （`mySeq` 自增）当成了握手状态机，且没区分半关闭。

## 证据

### A. DNS 地理错配（修复前）

`out/tmp/wifi-evidence/pre-dns-diag.log`（60 轮，DNS 全部成功、HTTP 8 轮失败）：

```
9 00:24:15 dns=1 http='' err=''
25 00:25:53 dns=1 http='' err='nc: Timeout'
29 00:26:21 dns=1 http='' err='nc: Timeout'
...
60 00:29:39 dns=1 http='' err=''
```

`out/tmp/wifi-evidence/pre-dns-stab.log`：180 轮 **169** 轮全通过（11 轮 `baidu=` 空）。

修复后 `out/tmp/wifi-evidence/post-dns-stab.log`：180 轮 **179** 轮全通过；
唯一失败轮次 51（`00:59:50 captive=` 空）属根因 3（SYN-ACK 丢失），由 SYN 重传修复，
随后 `out/tmp/wifi-evidence/captive-100.log` **100/100**。

### B. 链路丢帧（修复前）

`out/core/daemon/runtime/gold_cn_OS3.0.10.0.VNQCNXM/5/runtime/wifi/wifi-host-service.log`：

```
2026/10/28 01:36:59 [link] 发送队列满，丢帧（累计 12200）：对端未排空
```

20MB 下载到此为止，guest `/data/local/tmp/dl.bin` 约 15KB。

### D. 截断 / 0 字节（流控与握手修复前后）

```
# 流控补丁但无 isn/半关闭修复：
round bytes=65906 elapsed=10s      # 停在 maxInflight=64KB 附近
round bytes=206   elapsed=0s       # 只剩响应头就 EOF
```

### E. 静默看门狗（guest 停止发帧）

```
2026/10/28 01:42:38 [stat] rx=482(+0) tx=445 drop=0 reset=0 队列=0/1024
2026/10/28 01:43:35 [link] 读帧失败: read unix ->/tmp/stage-007d12b0/wifi-data.sock: i/o timeout
2026/10/28 01:43:35 [link] 已连接 /tmp/stage-007d12b0/wifi-data.sock
2026/10/28 01:43:38 [stat] rx=529(+47) tx=488 drop=0 reset=1 队列=0/1024
```

### F. 上行静默丢字节（修复前）

`out/tmp/wifi-evidence/pre-ingest-upload.log`（旧二进制 `90fb3ac9…`）：

```
connected from ('192.168.31.230', 63245)
received 20968304
```

落盘 sha256 `c9f9513e4d4cbf221f4b1bd65e9dd530bf12a4c3e26c0b9eeebdc8f9ea657960`
≠ 源 `cd52d81e25f372e6fa4db2c0dfceb59862c1969cab17096da352b34950c973cc`
（20971520 字节）。同时段网关日志无 `非法以太帧`/`读帧失败`/`重同步`/`写出口失败`，
`[stat] drop=0 reset=0`——丢帧发生在 guest→host 链路上，网关未校验 seq 所以无感。

## 改动

源码：`qemu/backend/wifi-host-service/`

| 文件 | 改动 |
| :--- | :--- |
| `gw/config.go` | `UpstreamDNS` 支持逗号分隔多台（`UpstreamList()`），缺省改 `223.5.5.5:53,8.8.8.8:53`（国内优先） |
| `gw/gateway.go` | `upstream string` → `upstreams []string` |
| `gw/dns.go` | `forwardDNS` 逐台上游 3s 超时、失败继续；全部失败记日志丢弃（guest 自身会重试） |
| `gw/tcp.go` | ① 新增 `isn`，`resendSynAck` 复用 ISN；② SynAck 阶段收到握手 ACK 才启动下行泵（`waitInflight` 在 SynAck 阶段等待）；③ ACK/window 流控：`maxInflight=64KB`、`lastWin`、`ackCh`，泵每读一个 MSS 前检查在途字节；④ guest FIN 改半关闭（`CloseWrite` 上游 + ACK FIN），上游 EOF 才回 FIN；⑤ 流控停顿 >2s 记日志（诊断）；⑥ 出口写加 10s 超时（`ingest` 由读循环串行调用，上游不排空不能无限阻塞）；⑦ `ingest` 校验 seq：空洞不转发只回 dup ACK 促重传、重叠裁剪后只转发新增字节；FIN 需 `seq+len(data)==guestSeq` 才确认；⑧ captive 改为按 seq 幂等应答：204+FIN 保留原序号可重放，guest ACK 后才收尾（不再发完即 Closed） |
| `gw/gateway_test.go` | 新增 `TestSynRetransmitGetsSynAck`、`TestNoDataBeforeHandshakeAck`、`TestGuestFinHalfClose`、`TestUploadSeqGapNoCorruption`（空洞不写上游 + 缺段补齐按序 + 重叠裁剪 + FIN 序号）、`TestCaptiveReplay`（重传请求按原序号重放 204+FIN） |
| `wifi-aps.json` / `README.md` | 上游 DNS 改国内优先，补「为什么不用 8.8.8.8 直连」的说明 |

构建与部署（`build/qemu/build-qemu-darwin` 因缺 PyYAML 不能整体跑，按其中
`build_wifi()` 同款步骤手工执行）：

```sh
cd qemu/backend/wifi-host-service
go test ./...
go build -o /tmp/wifi-host-service.new ./component
cp /tmp/wifi-host-service.new out/qemu/dist/darwin/libexec/wifi-host-service.new
mv out/qemu/dist/darwin/libexec/wifi-host-service.new \
   out/qemu/dist/darwin/libexec/wifi-host-service     # 原子替换
```

产物 sha256：`55ca32e6b211960077b25c05f690e477208aa51662889a14e867df8588ef2441`
（含 ⑦ seq 校验）。此后又补了 §9 captive 幂等重放，最终产物
`83759fef020633b03d0ef210c693fab8dcfd7c4c1773294722bf56589a3f3737`
（PID 92706，2026-10-28 03:12:36 起，实例 5 未重启），长稳与空闲恢复均在该产物上复验。

## 验证

| # | 验证点 | 结果 |
| :--- | :--- | :--- |
| 1 | 单测 | `go test ./...` 全通过（含 4 个新增回归） |
| 2 | 20MB 下行 ×3 | 每次正文 `20971520` 字节、`2s`，sha256 与源 **一致**；`drop=0 reset=0` |
| 3 | 20MB 上行 ×3 | `received 20971520`/次，落盘 sha256 `cd52d81e…` 与源 **一致**（修复前 20,968,304） |
| 4 | 链路统计 | 复验期间 `rx=153675(+104741) tx=182122 drop=0 reset=0 队列=0/1024`，无 `流控停顿`/非法帧 |
| 5 | 30 分钟长稳（120 轮 ×15s） | **120/120 全过**（`stab-120-final.log`，每轮 ip=1 gw=1 captive=204 baidu=200）；同期宿主 `drop=0 reset=0` |
| 6 | 空闲恢复 | 静默 135s 触发看门狗重置（03:48:31），03:49:07 计数恢复、03:49:08 首轮检查全过 |
| 7 | SELinux | guest Enforcing、`/dev/hvc10` 标签 `cdh_wifi_data`，无相关 avc（沿用 #18 结论） |

## 遗留

1. **网关 TCP 无重传、无乱序缓存**：上行空洞只回 dup ACK，靠 guest 内核重传补齐
   （20MB×3 实测零缺口）；下行靠「在途 ≤64KB」流控避免队列溢出。若 guest 侧不再
   重传，连接会停顿，可用 `[tcp] 流控停顿` 日志定位（>2s）。
2. **CDH-B/C 切换未验**：见 #18 遗留 1（框架 autojoin 拉回 CDH-A，host 侧不跟随）。
3. **link speed 上限 18Mbps**：hwsim 管理帧样本升级到 2.4G legacy 上限；若要更高
   显示需假 beacon 带 HT capabilities（#18 遗留 2）。
4. 内核桥（guest 侧 `cdh-wifi-tx/rx`）源码不在本仓 `kernels/`（工作区为空），本次
   未改内核；E 类静默只能靠宿主看门狗兜底。
