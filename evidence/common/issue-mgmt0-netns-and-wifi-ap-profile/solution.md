# #176：管理网卡 `mgmt0` 独立网络命名空间隔离与 Wi-Fi 热点扫描档案对齐

- **分组**：`common/`（`android.service.daemon`、`android.hardware.wifi` 与宿主 `wifi-host-service` 跨全机型共用）
- **首发验证机型**：Xiaomi Redmi Note 13 5G（`gold` / `2312DRAABC`，`OS3.0.10.0.VNQCNXM`）

## 1. 问题

在双机网络与无线连接对比（[`network_telephony_bt_gnss.js`](../../../../../core/development/device-checker/checks/java/aligner/xiaomi/gold/network_telephony_bt_gnss.js)）中，`G1` 与 `G2` 两项检查未通过：

1. **`G1_networkActiveAndNoSuspiciousIfaces = false`**：普通应用调用 `java.net.NetworkInterface.getNetworkInterfaces()` 时，在主网络命名空间内枚举出管理网卡 `mgmt0`（`10.0.2.15/24`、`fec0::5054:ff:fe12:3456/64`），`suspiciousInterfaceNames = ["mgmt0"]`（真机仅返回 `["ccmni0", "dummy0", "lo", "wlan0"]`）。
2. **`G2_wifiNoVirtWifiOrHwsimBssidLeak = false`**：`WifiManager.getConnectionInfo()` 返回 `SSID: "CDH-A"`、`BSSID: "02:00:00:00:00:11"`；`WifiManager.getScanResults()` 仅返回 `VirtWifi`（`02:00:00:00:00:01`）与 `CDH-A/B/C`（`02:00:00:00:00:11..13`）4 个开放热点，带明显虚拟测试前缀与 `02:00:00:00:00:*` 本地管理 MAC。

## 2. 根因

1. **`mgmt0` 与 `adbd` 命名空间耦合**：
   - QEMU `net0`（SLIRP `10.0.2.0/24`，MAC `52:54:00:12:34:56`）仅用于宿主 `hostfwd` 到实例 `5555` 端口的 ADB 转发。
   - 早期为了让 `adbd` 与 `adb shell` 运行在主网络命名空间（从而可见 `wlan0` / `ccmni*`），`cph-netmgr-setup` 将 `mgmt0` 直接留在主命名空间配置 `10.0.2.15/24`，导致 Bionic `getifaddrs()` 通过 `RTM_GETADDR` 向所有应用暴露 `mgmt0`。
2. **宿主网关与 Wi-Fi HAL 遗留 `virt_wifi` 档位约束**：
   - 宿主 `wifi-host-service/gw/rssi.go` 在 `encodeProfile()` 中无条件前置硬编码记录 `VirtWifi,02:00:00:00:00:01,5240,-50,open`，并在缺省 `bssid` 时合成 `02:00:00:00:00:1X`；`wifi-aps.json` 默认配置为 `CDH-A/B/C`。
   - 实例侧 `android.hardware.wifi/include/wifi_scan_model.h` 的 `ParseScanModel()` 保留了旧版 `virt_wifi` 驱动的校验 `primary.ssid != "VirtWifi" || primary.security != Security::kOpen`（而当前内核 `mac80211_hwsim` 的 `hwsim_fake_profile_set` 已无此限制）。
   - 此外，`mac80211_hwsim` 假 Beacon 固定携带 802.11b CCK 强制基础速率（`1/2/5.5/11 Mbps`），5GHz 频段扫描可见但 `wpa_supplicant` 关联时因 5GHz `sband` 不含 CCK 速率而拒连；因此直连开放主 AP 宜设在 2.4GHz 信道（如信道 6 `2437 MHz`），5GHz 热点作为 WPA2 邻区扫描项存在。

## 3. 证据

- 修复前取证日志见同目录 [`log-fail.txt`](log-fail.txt)（`run-00221` 基线：`hasSuspiciousMgmt0=true`、`connectedSsid="CDH-A"`、`hwsimBssidApCount=4`）。
- 修复后取证日志见同目录 [`log-pass.txt`](log-pass.txt)（`run-00222` 实测：`hasSuspiciousMgmt0=false`、`connectedSsid="TP-LINK_2464"`、`connectedBssid="74:39:89:0b:24:63"`、`hwsimBssidApCount=0`）。

## 4. 改动

1. **`mgmt0` 匿名网络命名空间隔离与 TCP 5555 环回桥接**：
   - [`cph-netmgr-setup`](../../../../vendor/hardware/android.service.daemon/vendor/bin/cph-netmgr-setup) 与 [`cph_netmgr.rc`](../../../../vendor/hardware/android.service.daemon/vendor/etc/init/cph_netmgr.rc)：`configure` 阶段仅识别 MAC `52:54:00:12:34:56` 改名为 `mgmt0` 并保持 `DOWN`，不在主命名空间分配 IP 或路由；`/mnt/run/adb_net` 仍指向 `/proc/1/ns/net`，保持 `adbd` 与 `adb shell` 位于主命名空间。
   - 新增 [`mgmt_bridge.rs`](../../../../vendor/hardware/android.service.daemon/src/rust/src/mgmt_bridge.rs)（并在 [`syscall.rs`](../../../../vendor/hardware/android.service.daemon/src/rust/src/syscall.rs) 声明 `unshare`/`gettid`、在 [`main.rs`](../../../../vendor/hardware/android.service.daemon/src/rust/src/main.rs) 启动早期调用 `mgmt_bridge::spawn()`）：工作线程 `unshare(CLONE_NEWNET)` 创建匿名管理命名空间（不产生任何 `mountinfo` 挂载点），主线程执行 `ip link set mgmt0 netns <tid>` 将 `mgmt0` 移出主命名空间；工作线程在管理命名空间内配置 `10.0.2.15/24`、默认路由 `10.0.2.2` 并绑定 `TcpListener("0.0.0.0:5555")`，随后 `setns` 切回主命名空间，将收到的 SLIRP ADB 连接双向转发至主命名空间 `127.0.0.1:5555`。
2. **Wi-Fi 热点档案与扫描解析对齐**：
   - [`wifi_scan_model.h`](../../../../vendor/hardware/android.hardware.wifi/include/wifi_scan_model.h)：移除 `ParseScanModel()` 对首条记录必须为 `VirtWifi` 的历史限制，仅校验列表非空及字段合法。
   - [`rssi.go`](../../../../../qemu/backend/wifi-host-service/gw/rssi.go) 与 [`control_test.go`](../../../../../qemu/backend/wifi-host-service/gw/control_test.go)：移除 `encodeProfile()` 硬编码前置的 `VirtWifi` 记录；缺省 BSSID 合成改为合法全局单播前缀 `74:39:89:0b:24:6X`。
   - [`wifi-aps.json`](../../../../../qemu/backend/wifi-host-service/wifi-aps.json)：将默认热点配置更新为标准双频路由器参数（主连接开放 AP `TP-LINK_2464` `74:39:89:0b:24:63` ch6，邻区 WPA2 热点 `TP-LINK_5G_2464` ch36、`Xiaomi_A490_5G` ch44、`ChinaNet-8F3A` ch1），同步发布至 `out/qemu/dist/darwin/libexec/`。
   - [`cph-wifi-bootstrap`](../../../../vendor/hardware/android.service.daemon/vendor/bin/cph-wifi-bootstrap)：自动从 `/sys/module/mac80211_hwsim/parameters/scan_profile` 解码首个 `,open` 接入点 SSID（兜底 `TP-LINK_2464`），并在连接前清理持久化卷中遗留的 `VirtWifi` / `CDH-*` 保存配置。

## 5. 验证

1. **单元测试**：`qemu/backend/wifi-host-service` 执行 `go test -v ./...`，13 项用例全部 `PASS`。
2. **镜像构建与冷启动亮屏**：重编 `android.service.daemon` 与 `android.hardware.wifi`，完成 `gold` `07_build_vendor -> 09_build_patch -> 11_pack_vda`（`patch.img` `sha256 eaff76e24905`，`vda-c97d6641407d`）；重启实例 `1`，`dmesg` 输出 `mgmt_bridge: mgmt0 (10.0.2.15/24) 已隔离并监听 0.0.0.0:5555 -> 127.0.0.1:5555`，`boot_check.py` `[pass]`（平均亮度 `121.5`），`adb connect 127.0.0.1:5911` 正常连通，`adb shell ip -br addr` 主命名空间无 `mgmt0` 且可见 `wlan0`（`192.168.60.100/24`）。
3. **双机对账（`run-00222`）**：
   - 真机 `ec519c7d513a3ad3`：`32 / 32` 通过（`G1=true`、`G2=true`）。
   - 云机 `df809591a4e085ae-cloud`：`G1_networkActiveAndNoSuspiciousIfaces: true`、`G2_wifiNoVirtWifiOrHwsimBssidLeak: true`（`suspiciousInterfaceNames=[]`、`connectedSsid="TP-LINK_2464"`、`connectedBssid="74:39:89:0b:24:63"`、`suspiciousScanAps=[]`），总分由 **`18 / 32` 提升至 `20 / 32`**。

## 6. 遗留

- 如后续需要将直连主接入点切换至 5GHz 频段（如 `5180 MHz`），可在内核补丁 `0002-hwsim-fake-wifi.patch` 的 `hwsim_fake_build_beacon` / `hwsim_fake_tx_probe_resp` 中按 `frequency >= 5000` 选用 OFDM 基础速率表（`6/12/24 Mbps`）替代 2.4GHz CCK 速率表。
- 其它机型需重跑 `07 -> 09 -> 11` 后合入最新的 `service_daemon` 与 `android.hardware.wifi-service` 产物。
