# #301 全内核线（5.15 / 6.1 / 6.12 / 6.18）假 Wi-Fi 补丁 5 GHz CCK 速率集补齐（Pixel 6 A17 实测验证）

## 问题

1. **5 GHz 默认开放热点连不上**：Pixel 6 Android 17（`oriole`, `17-CP3A.261005.002.A1-20260905`，运行 `6.1` 内核）启用 Wi-Fi 后，能扫到宿主下发的 4 个假热点（含 5180 MHz 开放热点 `TP-LINK_2464`），但 `cph-wifi-bootstrap` 连续 15 轮发起连接均失败，`WifiClientModeImpl` 反复上报 `Network not found event received: network: "TP-LINK_2464"`，`wlan0` 停留在 `DOWN`。
2. **附带现象（Settings「Internet」黑屏 16 秒）**：此前重启服务后旧实例 `#3` 的宿主 `secure_env` / `vhost-modem-controller` 随旧 owner 进程退出而 QEMU 孤儿存活，导致点击 Settings「Internet」黑屏 16 秒才进入，且 `dumpsys wifi` / `cmd wifi` 永久卡死。

## 根因

1. **5.15 / 6.1 / 6.12 / 6.18 假 Wi-Fi 补丁漏同步 5 GHz 纯 OFDM 速率集**：
   - `#190`（6.6）与 `#198`（5.10，见 `../../oriole/issue-oriole14-hwsim510-5g-cck-rate/`）已修复 5 GHz Beacon / AssocResp 携带 2.4 GHz CCK Basic Rate（`0x82` = 1 Mbps）的问题，但 `5.15`、`6.1`、`6.12`、`6.18` 四份 `devices/kernels/patches/<ver>/0002-hwsim-fake-wifi.patch` 仍硬编码 2.4G 速率表 `{ 0x82, 0x84, 0x8b, 0x96, 0x0c, 0x12, 0x18, 0x24 }` 并在 `hwsim_fake_tx_assoc` 里强发 `DS_PARAMS` 和 `EXT_SUPP_RATES`。
   - 宿主 `wifi-aps.json` 的默认开放热点 `TP-LINK_2464` 位于 5 GHz（CH36 / 5180 MHz）。`wpa_supplicant` 收到 5 GHz Beacon 携带 2.4 GHz CCK Basic Rate 后判定速率集不兼容并过滤该 BSS，表现为反复 `Network not found event received`。
2. **「Internet」页黑屏 16 秒的级联死锁链**：
   - `host-daemon` 子进程（`secure_env`、`vhost-modem-controller`、`media-decoder`、`media-encoder`）监听父进程（owner）生命周期；重启 `cdha serve` 后旧实例 `#3` 的 `secure_env` 与 `vhost-modem-controller` 自动退出，仅 QEMU 孤儿存活。
   - Guest KeyMint HAL（`/dev/hvc3`）无限等待回复 → 阻塞 `keystore2` → 永久阻塞 `system_server` 唯一的 `WifiHandlerThread`（现场 `/dev/binderfs/binder_logs` 实测阻塞 `15,637,534 ms`）。Settings 打开「Internet」页时主线程同步调用 4 次 `IWifiManager` Binder，每次触发 `WifiThreadRunner` 4000 ms 超时（`4 × 4005 ms`），导致 `performCreate = 16,140 ms` 黑屏。

## 证据

- 修前失败日志见 [log-fail.txt](log-fail.txt)：
  - `#4`（旧 6.1 内核，后端健康）：`cmd wifi list-scan-results` 可见 `74:39:89:0b:24:63 5180 -33 TP-LINK_2464 [ESS]`，但 `cph-wifi-bootstrap` 第 13~15 轮均报 `Network not found event received`，`wlan0 DOWN`；
  - `#3`（孤儿 QEMU，`secure_env` 已死）：`performCreate = 16,140 ms`，`WifiHandlerThread` 阻塞在 `keystore2 -> KeyMint` 达 4.3 小时。
- 修后通过日志见 [log-pass.txt](log-pass.txt)：
  - 全 6 棵内核树（`5.10` / `5.15` / `6.1` / `6.6` / `6.12` / `6.18`）`git apply --check` 全部通过；
  - 新镜像 `#5`（通过长驻 `cdha serve` 拉起，7/7 后端在线）开机 12 秒内自动连通 5180 MHz `TP-LINK_2464`，`wlan0 UP 192.168.60.100/24`，`successfulTxPackets=861` / `successfulRxPackets=2347`，宿主 `wifi-host-service` 转发 `#3200+` 帧；
  - Settings「Internet」（`WIFI_SETTINGS`）冷启动 `performCreate = 368 ms`（总耗时 `865 ms`，热切 `130 ms`），`WifiThreadRunner`/`IRadioData`/`KeyMint` 零超时；
  - `gsm.version.baseband = g5123b-145971-260721-B-15910359`。

## 改动

1. 同步 5 GHz 纯 OFDM 速率表 `{ 0x8c, 0x12, 0x98, 0x24, 0xb0, 0x48, 0x60, 0x6c }`（6/12/24 Mbps Basic）至全部剩余 4 条内核线补丁，与 `5.10`（#198）/ `6.6`（#190）口径一致：
   - `devices/kernels/patches/5.15/0002-hwsim-fake-wifi.patch`
   - `devices/kernels/patches/6.1/0002-hwsim-fake-wifi.patch`
   - `devices/kernels/patches/6.12/0002-hwsim-fake-wifi.patch`
   - `devices/kernels/patches/6.18/0002-hwsim-fake-wifi.patch`
   - 两处修改点：
     - `hwsim_fake_tx_assoc`：当 `channel->center_freq >= 5000` 时仅写 `WLAN_EID_SUPP_RATES` (`rates_5g`)，不发 `DS_PARAMS` 与 `EXT_SUPP_RATES`；
     - `hwsim_fake_build_beacon`：当 `ap->frequency >= 5000` 时写 `rates_5g`，否则写 2.4G `rates`。
2. 重编 `google/oriole` Android 17（`17-CP3A.261005.002.A1-20260905`）全链路（`01_unpack` → `03_build_kernel` 6.1 配方 `6.1-cdh-271dd5a8161d` → `04_build_modules` → `10_build_vendor_dlkm` → `11_build_patch` → `13_pack_vda`），验证 `vendor_dlkm` 清空基座 `.ko` 并仅注入新编 `mac80211_hwsim.ko`（`vermagic=6.1.138-4k-cdh`）。

## 验证

- **补丁可打性**：`5.10` / `5.15` / `6.1` / `6.6` / `6.12` / `6.18` 全部 6 棵内核源码树 `git apply --check` 通过。
- **镜像构建**：`cdh-pipeline device build oriole --manifest 17-CP3A.261005.002.A1-20260905` 13/13 阶段 EXIT=0。
- **上机实测（`oriole` Android 17 `#5`，7/7 宿主后端在线）**：
  - 5 GHz Wi-Fi 自动连通 `TP-LINK_2464`（`5180 MHz`，`192.168.60.100/24`，`Supplicant state: COMPLETED`，`score: 100`，`isUsable: true`），数据面双向收发正常；
  - Settings「Internet」冷启 `performCreate = 368 ms`（从 `16,140 ms` 下降 97.7%），无任何 Binder 超时；
  - `getprop gsm.version.baseband` = `g5123b-145971-260721-B-15910359`，Web `/api/daemon/instance/config` 完整返回 Wi-Fi 已连接、DSDS 双卡 + eSIM 与代理状态。

## 遗留

- 其余使用 `5.15` / `6.1` / `6.12` / `6.18` 内核的存量已发布盘集需在下次重编内核/模块（`03 -> 04 -> 10 -> 11 -> 13`）时自然吸收该补丁；此前基于旧 `vda.img` 创建的旧实例目录需重建实例或刷新 `patch.img` 生效。
- 宿主机 `cdha serve` 重启时若存在旧运行中实例，其 `secure_env`/`vhost-modem-controller` 会因父进程退出而终止但 QEMU 可能残留为孤儿；建议后续在 `cdha serve` 优雅退出或启动恢复逻辑中自动清理/接管遗留 QEMU 实例，避免再次触发孤儿实例 KeyMint 级联死锁。
