# gold 云机 Wi-Fi 关闭：wlan0 恒 DOWN、活动网络是 Ethernet，与真机的 WIFI 形态不一致

## 1. 问题

device-checker（真机 Redmi Note 13 5G × 云机 gold）的 `network.connectivity` 在
「把能对齐的都对齐」之前的对拍里有 **13 项**差异，最刺眼的三条：

```
connectivity.transports[0]      真机: WIFI      云机: ETHERNET
connectivity.link.iface         真机: wlan0     云机: eth0
connectivity.capabilities[1]    真机: NET_CAPABILITY_VALIDATED=true
                                云机: NET_CAPABILITY_VALIDATED=false
```

连带 `network.ifaces` 36 项、`network.cell` 65 项（云机数据面只能退到 emulated RIL 的
`ccmni2`）。更实际的后果是：agent（`dev.checker`）拨不出到检查服务
`ws://101.33.73.101:80/ws` 的连接，`/api/devices` 里长期只有真机，双端对拍跑不起来。

此前为绕开这一点，做过一套临时手段（宿主 Python TCP 代理 8899 + `adb reverse` +
guest `OUTPUT` DNAT + 管理网卡改名 `eth0`）。这些手段治的是症：DNAT 让
`101.33.73.101:80` 落到 `127.0.0.1:8899`，可 `adb reverse` 一断（重连/重启即断）
整条链路就哑，且与真机的网络形态毫无关系。

## 2. 根因

**不是 Wi-Fi 栈缺失，是 Wi-Fi 从来没被打开。**

- `init.svc.vendor.wifi-vhal` = `running`、`init.svc.wificond` = `running`；
- 宿主 `wifi-host-service` 每 30 s 下发一次 `scan_profile`，
  `iw dev wlan0 scan` 能扫到 `VirtWifi` / `CDH-A`（-40 dBm @2437）；
- 但 `cmd wifi status` 恒为 `Wifi is disabled`，`settings get global wifi_on` = `0`。

真机的 `Settings.Global.WIFI_ON=1` 是**开机向导/首次开机流程**写进去的；云机的 data
镜像是「已经开好机」的快照，那份设置停在 0，框架于是把 Wi-Fi 关着，`Wlan0` 只有 DOWN。
数据面因此只能退到 emulated RIL：
`ccmni2` 反复 `SETUP_DATA_CALL → INSUFFICIENT_RESOURCES`（slirp `restrict=on`），
agent 连不上检查服务。

一句话：**云机缺的是「开机把 Wi-Fi 打开并接入 AP」这一步，不是缺 HAL。**

## 3. 证据

完整取证见同目录 `log-fail.txt` / `log-pass.txt`，要点：

- 失败态（`log-fail.txt` ②③，18:37 采集）：`wifi_on=0`、`cmd wifi list-networks` →
  `No networks`、`wlan0 DOWN`、`ip route get 101.33.73.101` → `Network is unreachable`、
  `Active default network: none`；同一时刻 HAL 与 4 个 AP 广播都在。
- 通过态（`log-pass.txt`）：人为把 Wi-Fi 打回「全新 data」状态后重启，新 patch 里的
  `cph-wifi-bootstrap` 在 `boot_completed` 后 9 s 内完成「开 Wi-Fi + 接入 CDH-A」：
  ```
  [10-31 18:39:07] Wi-Fi 处于关闭态，打开
  [10-31 18:39:13] 连接 CDH-A（第 1 轮）
  [10-31 18:39:16] wlan0：wlan0 UP 192.168.60.100/24 fe80::2c04:ddff:fe83:134b/64
  [10-31 18:39:16] 默认路由：101.33.73.101 via 192.168.60.1 dev wlan0 table wlan0 src 192.168.60.100
  ```
  `cmd wifi status` → `Wifi is connected to "CDH-A"`，NetworkCapabilities
  含 `Transports: WIFI ... VALIDATED`；guest 用 `nc` 直接 GET
  `http://101.33.73.101/api/devices` 拿到 `200 OK`，两台设备都在列表里。
- **这一轮没有任何临时手段**：无 `adb reverse`、无 DNAT、无宿主代理。

## 4. 改动

1. 新增开机脚本 `engine/vendor/hardware/android.service.daemon/vendor/bin/cph-wifi-bootstrap`
   （`/vendor/bin/cph-wifi-bootstrap`）：
   - 等 `sys.boot_completed=1`（90 × 2 s）；
   - 等 WifiService 就绪（`init.svc.wificond=running` 且 `cmd wifi status` 有回显，30 × 2 s）；
   - 若 `Wifi is disabled` → `cmd wifi set-wifi-enabled enabled`，再等最长 60 s；
   - 没连上目标 AP 时 `cmd wifi start-scan` + `cmd wifi connect-network <SSID> open`
     （15 轮 × 6 s），已连上直接跳过 —— 全程幂等；
   - 目标 AP 默认 `CDH-A`（`wifi-aps.json` 里唯一 `backend` 为空的直连 AP，
     与真机「家用路由器直连」最接近），可用 `CPH_WIFI_SSID` / `CPH_WIFI_SECURITY` 覆盖；
   - 诊断日志落 `/data/local/tmp/cph-wifi.log`（`/vendor` 只读、logcat 会轮转）。
2. 新增 rc `engine/vendor/hardware/android.service.daemon/vendor/etc/init/cph_wifi_bootstrap.rc`：
   ```rc
   service cph_wifi_bootstrap /system/bin/sh /vendor/bin/cph-wifi-bootstrap
       class main
       user root
       group root
       seclabel u:r:pioneer:s0
       oneshot
       disabled

   on property:sys.boot_completed=1
       start cph_wifi_bootstrap
   ```
   `cmd wifi …` 走 binder 调 system_server（uid 0 即 privileged shell），无需 capabilities；
   `disabled` 不能省 —— 与 `cph_storage_align` 同一坑：class main 的服务在 init main
   阶段（约 18 s）就跑起来了，那时 WifiService 还没起，`boot_completed` 的再次触发会被
   `requested start ... already running (flags: 6)` 忽略。
3. 产物：`07 build-vendor` → `09 build-patch` → `11 pack-vda`，
   `patch.img` sha256 由 `304f3a9c9bd2` 变为 **`ebb783007f84`**，已发布到
   `engine/out/dist/xiaomi/gold/OS3.0.10.0.VNQCNXM/`。
4. 撤掉所有临时手段：管理网卡 `eth0` 改回 `mgmt0`（`10.0.2.15/24`、local 表路由）、
   删除 `OUTPUT` DNAT 规则、`adb reverse --remove-all`、杀掉宿主代理进程。

## 5. 验证

- dmesg：`processing action (sys.boot_completed=1) from (/vendor/etc/init/cph_wifi_bootstrap.rc:20)`
  → `starting service 'cph_wifi_bootstrap'` → `exited with status 0 oneshot service took 7.527000 seconds`。
- 冷启动前把 Wi-Fi 打回出厂态（`wifi_on=0`、`forget-network` 掉 CDH-A、`wlan0 DOWN`），
  重启后**无人工干预**自动连上（见 `log-pass.txt` ②③）。
- `run-00041` 双端对拍（`/api/devices` 两台都在）：
  | check | 对齐前（run-00039 归并） | run-00040（手工开 Wi-Fi） | run-00041（本改动后） |
  | :--- | :--- | :--- | :--- |
  | network.connectivity | 13 项 | 5 项 | **5 项**（只差网段/IPv6 值） |
  | network.ifaces | 36 项 | 42 项 | 42 项（多出的是 ccmni0/ccmni2/mgmt0 模拟器内部接口） |
  | network.cell | 65 项 | 3 项 | 30 项（本轮真机 cells=[] 空集，见遗留） |
  | camera.camera_new | — | 570 项（键表位移） | **0 项** |
  | ultimate.probe_runtime | 35 项 | 35 项 | **25 项** |
  | 合计 | 5984 项 | 6523 项 | **5971 项** |
- `files.storage` 138 项差异中 `st_mode/st_uid/st_gid` **0 项**（全是 inode/时间戳噪声）。

## 6. 遗留

1. **网段值不能对齐**：真机 `192.168.31.159/24`、网关 `192.168.31.1`；云机
   `192.168.60.100/24`、网关 `192.168.60.1`。刻意不改 —— `192.168.31.0/24` 是宿主
   所在真实家用 LAN 的网段，云机内部网关若也用该网段会与真实主机地址撞车。
   要「看起来一样」可改 `wifi-aps.json` 的 `ipSubnet`，但代价是宿主 LAN 冲突，不做。
2. **IPv6 link-local 不可能一致**：由 `mac80211_hwsim` 分配的 AP/客户端 MAC 决定。
3. **`network.ifaces` 多 3 个接口**：`ccmni0`/`ccmni2`（emulated RIL 数据面）、`mgmt0`
   （宿主管理面，adb 与 daemon 通道都靠它）。它们是模拟器的运行基础设施，删掉就失去
   adb/回退数据面，不做对齐。
4. **`network.cell` 仍在漂**：真机本轮 `getAllCellInfo` 返回空集（Wi-Fi 为默认网络、
   蜂窝未注册），云机 cuttlefish `modem_simulator` 报 2 个 LTE cell（460/01）。这一项
   两侧都是「会随时间变的值」，且 modem 模拟器在宿主 cuttlefish 侧，非本仓库可改。
5. **真机侧 `hardware.sensor` 仍 `执行超时`**（检查脚本在该机型耗时过长），双端自动
   对拍这一项拿不到真机结果；传感器清单已用 `dumpsys sensorservice` 逐字节比对通过
   （见 #97）。
6. **`dev.checker` 自启不完整**：本轮重启后 `BootReceiver` 没把前台服务拉起来
   （运行时权限被系统回收，MainActivity 卡在授权页）。取证时用
   `pm grant` 八项运行时权限 + `am start-foreground-service -n dev.checker/.DeviceCheckerService`
   手工拉起；这是检查工具侧的体验问题，与云机镜像无关。另：一轮对拍跑完后服务端会把
   云机从 `/api/devices` 列表里移除（此时 guest 侧到 `101.33.73.101:80` 的 TCP 仍是
   `ESTAB`），重启前台服务即重新注册 —— 同样是服务端会话簿记，不是网络问题。
7. 实例 specId 在本次重出产物后由 `gold_OS3.0.10.0.VNQCNXM` 变为
   `gold_cn_OS3.0.10.0.VNQCNXM`（取 dist `device.json` 的 id），实例目录随之改名，
   `/data`（writable-data.qcow2）保留未重置。
