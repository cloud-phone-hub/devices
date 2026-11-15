# device-checker 云机 App 静默断线后永不重连（全机型通用）

## 问题

oriole A14 对齐复测前发现：云机（实例 2，ADB `127.0.0.1:5911`）的 device-checker
App 进程在跑、MainActivity 在前台，但服务器 `GET /api/devices` 只剩真机
`b8f75582353b320a`，云机 `02f9d9f965732349-cloud` 掉线。用户报告「云机的
device-checker app 都无法连接服务器」。

## 根因

`WsClient.kt` 的读帧循环 `ins.read()` **无读超时、无客户端主动心跳**：

1. TCP 半开连接（宿主 Wi-Fi NAT 表项过期、服务端重启未发 FIN 等）时，
   `read()` 永远阻塞、永不抛异常 → `loop()` 的 catch-重连分支永远走不到；
2. 重连的另一触发源是 `ConnectivityManager` 网络回调（`onLost`/`onAvailable`），
   但 guest 的 wlan0 一直 `CONNECTED/VALIDATED`（`192.168.60.100`，ping 服务端
   0% 丢包），网络层从未变化，回调永不触发；
3. 客户端只被动回服务端 ping（`0x9 → pong`），自己从不发 ping；服务端
   `adapter.rs` 对 `Message::Ping` 直接 `continue`，两端都没有探活流量。

三者叠加：链路静默死亡后 App 自认为「已连接」，无限期挂死，只能靠重启进程恢复。

## 证据

- 掉线时：`pidof dev.checker` = 18377、`dumpsys activity services` 有
  `DeviceCheckerService` ServiceRecord，但 `logcat -d | grep checker-ws` 零条
  （读循环挂死在最后一次成功连接上，无任何新日志）；
- guest 网络完好：`ping -c 2 101.33.73.101` 0% 丢包（rtt ~0.75 ms）、
  `ip route` 默认路由经 `192.168.60.1`、`dumpsys connectivity` 显示
  WIFI `CONNECTED` 且 `IS_VALIDATED`；
- 服务器侧正常：宿主 `curl /api/checks` = 200，`/api/devices` 只含真机；
- `am force-stop dev.checker` + `am start` 后 12 s 内重新握手注册成功
  （log-pass.txt 第 1 段）——证明不是服务器/网络问题，是客户端挂死。

## 改动

| 文件 | 要点 |
| :--- | :--- |
| `core/development/device-checker/app/src/main/java/dev/checker/WsClient.kt` | ① `s.soTimeout = 65000`：静默断链最多 65 s 抛 `SocketTimeoutException` 走既有重连；② 新增 `checker-ws-ping` 守护线程每 30 s 发 ping 帧（保活 NAT 映射 + 制造回流量），写失败即关 socket 触发重连；③ 读循环补 `0xA`（pong）分支；连接退出时 `pinger.interrupt()` |
| `core/development/device-checker/server/src/adapter.rs` | `Message::Ping(p)` 由 `continue` 改为显式回 `Message::Pong(p)`（split 后库内自动 pong 不一定被冲刷），发送失败即断会话 |

App 重编 `assembleCloudDebug` 并 `adb install -r` 到云机；服务端 `cargo check`
通过（远端 101.33.73.101 现部署的 tungstenite 自动 pong 已够用，显式回 pong
的改动随下次部署上线）。

## 验证

1. 新 APK 装机重启后 10 s 内握手注册成功（log-pass.txt 第 2 段）；
2. **心跳周期验证**：静置 75 s（> 65 s 读超时阈值）后 `logcat -s checker-ws`
   无重连/失败日志、连接保持稳定——证明 30 s ping 有 pong 回执、读超时不误判；
3. `/api/devices` 两端同时在线：`02f9d9f965732349-cloud` 与 `b8f75582353b320a`。

## 遗留

1. 服务端显式 pong 改动未部署到 101.33.73.101（无现成部署通道；现网
   tungstenite 自动 pong 已验证够用），下次服务端发版时一并带上；
2. 真机侧 App 仍是旧版（无心跳），若真机链路静默死亡同样需手工重启；
   真机装的是 real flavor，需另编 `assembleRealDebug` 推送；
3. 掉线的最初诱因（NAT 表项过期还是服务端曾重启）无日志可考——旧版客户端
   挂死前不落任何日志，新版心跳上线后此类事件会留下「连接失败/重连」痕迹。