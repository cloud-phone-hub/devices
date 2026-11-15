# Issue: oriole A14（5.10 内核）假 Wi-Fi 在 5 GHz 上广播 CCK 1 Mbps 基础速率，`wpa_supplicant` 拒连 → 云机无网

- 设备: Google Pixel 6（oriole，AP2A.240905.003.F1 + lineage-21.0，内核 `5.10.270-4k-cdh`）
- 宿主: macOS (darwin, QEMU + hvf)，实例 `oriole_14-AP2A.240905.003.F1-20240815/2`（ADB `127.0.0.1:5911`）
- 状态: **已闭环**（2026-11-12）：5.10 `0002-hwsim-fake-wifi.patch` 的 beacon 与 assoc resp 按频段分速率集（5G 纯 OFDM），重跑 `02 → 11`，干净盘起机自动连上 `TP-LINK_2464`（5180 MHz，`192.168.60.100/24`），可 ping 通 device-checker 服务端。
- 背景: 「oriole Android 14 云机和真机对齐」任务的第二个卡点（#197 解决起机稳定后，云机仍连不上 device-checker 服务端）。

---

## 一、问题

#197 之后云机能稳定运行，但 `wlan0` 一直 `DOWN`、没有任何路由，`ping` 报 `Network is unreachable`，
device-checker 云机版装好后注册不到服务端。扫描能看到 4 个假 AP，`TP-LINK_2464`（开放）也已由
`cph-wifi-bootstrap` 存成网络 0，但框架报 `NETWORK_SELECTION_DISABLED_NETWORK_NOT_FOUND`，切开关后
`CMD_START_CONNECT` 仍然失败。

## 二、根因

gold #190 为了对齐真机「5 GHz 802.11ac」，把共用的 `wifi-aps.json` 里默认开放 AP `TP-LINK_2464`
改到了 **channel 36（5180 MHz）**，并且只修了 **6.6** 的 hwsim 补丁：5G 只发纯 OFDM 速率。

5.10 的 `0002-hwsim-fake-wifi.patch` 没跟上：假 beacon 和假 assoc resp 不分频段，一律发 2.4G 速率集
`{0x82, 0x84, 0x8b, 0x96, …}`，其中 `0x82` = **1 Mbps 且为 Basic Rate（CCK）**。5 GHz 不支持 CCK，
`wpa_supplicant` 做 BSS 选择时判 `hardware does not support required rate 1.0 Mbps (freq=5180 …)`
→ `skip - rate sets do not match`，于是框架那边就是「网络找不到」。

## 三、证据

- `log-fail.txt`：无网现场、扫描结果与已存网络、`wifi-aps.json` 的 ch36 配置、verbose 日志里的
  `rate sets do not match` 原句，以及 5.10 / 6.6 补丁速率表的对照。
- `log-pass.txt`：修复后 `Wifi is connected to "TP-LINK_2464"`、`wlan0 UP 192.168.60.100/24`、路由与 ping，
  以及构建日志里 `mac80211_hwsim.o` 重编、`186 个符号版本一致`。

## 四、改动

[`kernels/patches/5.10/0002-hwsim-fake-wifi.patch`](../../../kernels/patches/5.10/0002-hwsim-fake-wifi.patch)
（对 `drivers/net/wireless/mac80211_hwsim.c` 打补丁后改源码再 `git diff` 重新生成，只改两处，与 6.6 同口径）：

1. `hwsim_fake_build_beacon`：新增 `rates_5g[] = {0x8c, 0x12, 0x98, 0x24, 0xb0, 0x48, 0x60, 0x6c}`
   （6/12/24 Mbps 为 Basic），`ap->frequency >= 5000` 时发它，否则沿用 2.4G 速率表；两组都是 8 字节，长度预检不变。
2. `hwsim_fake_tx_assoc`：`center_freq >= 5000` 时 assoc resp 只带 `rates_5g`，不带 DS Parameter Set 与
   Extended Supported Rates；2.4G 分支与原来一致。skb 长度按分支计算。

没有移植 6.6 的 HT/VHT IE（`hwsim_fake_append_ht_vht_ies`）：本次只解决「连不上」，链路标准与速率对齐留作后续条目。

## 五、验证

1. `cdh device build oriole --manifest 14-AP2A.240905.003.F1-20240815 --from 02_build_kernel` EXIT=0
   （`CC [M] drivers/net/wireless/mac80211_hwsim.o` 重编，`mac80211_hwsim.ko: 186 个符号版本一致`）；
2. 删实例后干净盘重建：起机 152 s 时 `cmd wifi status` 为 `Wifi is connected to "TP-LINK_2464"`、
   `Frequency: 5180MHz`、`IP: /192.168.60.100`，`wlan0 UP`，`ping 101.33.73.101` 0% 丢包。

## 六、遗留

- **其它内核线同样有问题**：`5.15` 与 `6.1` 的 `0002-hwsim-fake-wifi.patch` 也是不分频段的旧速率表；
  `wifi-aps.json` 是所有机型共用的，所以这两条线的机型（5 GHz 默认 AP）应该一样连不上。代码改法相同，
  但本次没有在那些机型上起机验证，**还没改**，等用到时按本条移植并复测。
- **链路形态与真机不一致**：现在是 `Wi-Fi standard: 1`（legacy 11a）、`Link speed: 54Mbps`；Pixel 6 真机是
  11ax/11ac，速率高得多。需要把 6.6 的 HT/VHT（以及 HE）IE 移植到 5.10，归入后续对拍差异项。
