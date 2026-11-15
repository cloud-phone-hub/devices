# #231 云机未装 `device-checker-cloud.apk` 与连 `101.33.73.101` 旧连接竞态掉线修复

## 问题

1. 干净重建盘云机 `gold_15-OS3.0.11.0.VNQCNXM/3`（`127.0.0.1:5911`，`98a058b2b0618483-cloud`）未预装最新 `device-checker-cloud.apk`；
2. 安装后云机与多台实例并发/重连至检测服务端 `http://101.33.73.101` 时，频繁出现连上数秒后从 `/api/devices` 消失的现象（用户反馈「云机都连不上 101 的服务器」）。

## 根因

1. **云端包缺失**：重建 data 分区后云机尚未部署带最新脚本支持（及服务端地址配置）的 `device-checker-cloud.apk`。
2. **服务端按 `android_id` 字符串无条件踢除（竞态删新）**：此前 [`issue-checker-ws-silent-disconnect`](../issue-checker-ws-silent-disconnect/solution.md) 已解决半开 NAT 保活问题，但 `core/development/device-checker/server/src/adapter.rs` 的 `handle_device` 在 WebSocket 任务退出时仍按 `&key`（`android_id`）直接执行 `devices.remove(&key)`。当客户端快速重启（如 `force-stop`、Activity 重建或网络切换）建立新连接并覆盖注册表条目后，旧连接的 TCP 半关闭通道若延迟数秒才触发 EOF/超时退出，其清理分支就会**把刚上线的新连接记录从在线表中误删**。
3. **客户端握手打断与延迟 `FIN` 放大竞态**：`WsClient.kt` 在首次注册 `ConnectivityManager` 回调时，`onNetworkAvailable()` 无条件调用 `loopThread?.interrupt()`，可能打断正在进行的握手；且关闭 socket 未设置 `SO_LINGER(true, 0)`，导致网关/服务端侧半关闭状态持续 3~5 秒。

## 证据

见同目录 [`log-fail.txt`](log-fail.txt) 与 [`log-pass.txt`](log-pass.txt)。

## 改动

1. **`core/development/device-checker/server/src/adapter.rs`**：
   - 新增 `Devices::remove_if_same(&key, &dev)`：通过 `Arc::ptr_eq` 校验当前注册表内的 `Device` 指针与退出协程持有的是否为同一连接；若已被新连接接替，仅记录 `[ws] 旧连接关闭（已被新连接接替，保留在线）`，不再移除在线条目；
   - 新增异步单测 `adapter::tests::stale_connection_disconnect_does_not_remove_replacement`，验证旧连接延迟 `drop` 时新连接仍保持在线，新连接自身 `drop` 后才移除。
2. **`core/development/device-checker/server/src/main.rs`**：
   - 将 `run_checks` 日志打印适配异步 `st.devices.ids().await.join(",")`。
3. **`core/development/device-checker/app/src/main/java/dev/checker/WsClient.kt`**：
   - 新增 `@Volatile private var connectingOrConnected = false` 与 `Thread.interrupted()`，仅在未连接态响应 `onNetworkAvailable()` 中断唤醒；
   - `connectOnce()` 连上后调用 `s.setSoLinger(true, 0)`，在进程退出或重启时直接发 `RST` 减少半关闭窗口；
   - 心跳线程连上后在 `4s` / `9s`（及此后每 `60s`）补发幂等 `hello` 帧，确保即使出现极端网关延迟也能快速自愈注册。
4. **编包与云机部署**：
   - 执行 `./app/gradlew -p app assembleDebug` 产物拷贝至 `out/core/development/dist/darwin/device-checker/device-checker-cloud.apk`（`9,010,495` 字节）；
   - 通过本地 `daemon`（`127.0.0.1:9050`）root 通道 `POST /api/channel/cmd?spec=gold_15-OS3.0.11.0.VNQCNXM&index=3` 执行 `pm install -r -g /data/local/tmp/dc-cloud.apk && rm -f /data/local/tmp/dc-cloud.apk` 并启动 `dev.checker/.MainActivity`。

## 验证

1. **宿主单元测试**：
   - `cd core/development/device-checker/server && cargo test` -> `1 passed; 0 failed`（`stale_connection_disconnect_does_not_remove_replacement ... ok`，耗时 `0.28s`）。
2. **真机与云机在线实测**：
   - 安装后 `force-stop` 12s 压测在 `t+1s..t+12s` 全程在线不掉线；
   - `adb devices` 显示真机 `4lmbvom7gifm5lbu` 与云机 `127.0.0.1:5911` 均为 `device`，`GET http://101.33.73.101/api/devices` 同时列出 `07cf5fc1784783d5` 与 `98a058b2b0618483-cloud`；
   - 随后连续完成 `aligner.*` 全 19 域（`run-00255..00275`，`38/38 succeeded`）与 `ultimate.*` 全 14 套（`run-00279..00292`，`28/28 succeeded`）采数，全程零断线。

## 遗留

1. 当前干净云机已装包并常驻 `dev.checker/.MainActivity`；后续若切换新版 APK，需同步通过 `daemon` 通道重装；若重编云机镜像 `data` 模板，可将对应 APK 纳入首启安装/预置流程。
