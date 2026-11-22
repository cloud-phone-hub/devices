# #299：全栈打通 Host Radio/Wi-Fi SOCKS5 VPN、Camera 视频输入、Bluetooth 与 NFC 硬件模拟并完成全仓 45 盘集验收

- **设备/机型**：全仓 40 台 Android 机型 / 45 个 Android 盘集（A13~A17 五代底座；另有 2 个华为纯血鸿蒙包不属 Android 范围）
- **日期**：2026-11-22
- **状态**：已闭环

## 一、问题

改造前，宿主侧（`agent/modules/daemon` 与 `qemu/backend`）及 Guest HAL（`devices/hardware`）存在五项核心通道缺口（对应根目录 `TODO.md` 第 1~6 项）：

1. **无统一 Trojan→SOCKS5 代理引擎**：`doc/90G.yaml` 中的 33 个 Trojan VPN 节点未被 `host-daemon` 消费，无法自动测活选路，也无法在运行期热切换出口节点。
2. **Radio 蜂窝数据通道未打通**：`modem.go` 默认 `CellularData=false`、`modem-data-gateway` 未接线到 QEMU `net2`（`-netdev stream`）；`modem-host-controller` 上报的接口名与 MTU/DNS 偏移未对齐 `ccmni2`；Guest `android.hardware.radio` 在 `setupDataCall` 返回 `CONNECTED` 后并未拉起 `ccmni2` 网卡；`wifi-host-service/gw` 在 120s 无流量后会主动断开流连接（触发 QEMU `-netdev stream` 断开后不转发新连接的限制），且 `captiveDomains` 误劫持 `www.google.com` 导致 HTTPS 204 探测超时。
3. **Wi-Fi 数据通道未按实例接入 SOCKS5 VPN**：默认 `wifi-aps.json` 指向宿主机静态端口（`7891`/`1080`）或直连，且 `gw` 的 SOCKS5 客户端缺少认证协商、RFC 1928 动态地址解析、SOCKS5 DNS 与 UDP ASSOCIATE 完整支持。
4. **Bluetooth 宿主后端缺少自包含运行库与虚拟外设仿真**：`out/qemu/dist/darwin/libexec/lib64` 为指向本机 SDK 的软链接，迁移即失效；停机未直杀 `netsimd` 存在 2s 延迟；`bthci` 仅做透明转发，扫描列表无虚拟蓝牙外设可被发现。
5. **NFC 与 Camera 仍为本地空壳/占位**：Guest `android.hardware.nfc` 与 `android.hardware.camera` 未接通 QEMU `virtio-serial`（`hvc11`/`hvc12`），宿主机无 NCI 2.0 虚拟控制器、无视频源采集与帧推流后端；且早期向共享 `virtio-serial` 过早推大帧会堵塞同总线上的 `hvc2..hvc11` 通道。

## 二、根因

1. **代理与网关层**：`wifi-host-service/gw` 作为 Wi-Fi 与 Radio 数据面共用栈，缺少完整的 RFC 1928 SOCKS5 TCP/UDP/DNS 转发实现，其默认 120s 空闲超时会触发 QEMU `-netdev stream,server=on` 永久失活；同时 `captiveDomains` 将 `www.google.com` 劫持到 `172.16.0.1:80`，破坏了 HTTPS 204 连通性探测。
2. **Guest Radio 数据面**：Android Telephony 依赖 RIL HAL 在 `setupDataCall` 阶段配置并拉起蜂窝网卡（`ccmni2`），此前 `radio_data.cpp` 仅回复响应包而未执行 `ioctl(SIOCSIFADDR/SIOCSIFNETMASK/SIOCSIFMTU/SIOCSIFFLAGS)`，且 `hal_radio_default` 缺少 `capability net_admin` 与 `udp_socket` SELinux 权限。
3. **共享 `virtio-serial` 总线与 NCI 状态机**：
   - `hvc2..hvc12` 共用同一个 QEMU `stage-vserial` 设备，Host 若在 Guest 未开启 `/dev/hvc12` 前推送 500KB 级视频帧会填满宿主机到 Guest 的共享缓冲导致 `hvc4`/`hvc5`/`hvc11` 全部卡死；而单次握手包又可能在 QEMU socket 建链前丢失，需 Guest 空闲周期重发 `"CDCM_READY\n"` 心跳。
   - Android `NfcNciApexGoogle` 在 `CORE_INIT` 后严格要求 `NFCEE_DISCOVER_NTF`、路由表确认、厂商扩展 `2F 0C` 以及静态 HCI `ConnID=1` Admin Gate 应答，否则 90s 内触发 Watchdog 崩溃。
4. **并发编排缺陷**：`daemon/run/qemu.go:allocateSlot` 无互斥锁与端口占用探活，导致多路并发 `/api/create` 冲突分配 `slot=0`；`peripherals.go:lookupActiveHosts` 走全目录 `ScanInstances` 在并发下易失配。

## 三、证据

详见同目录 [log-fail.txt](log-fail.txt) 与 [log-pass.txt](log-pass.txt)，核心对照如下：

- **改造前排查关键现象**：
  - `setupDataCall` 成功后 `dumpsys connectivity` 显示 `CONNECTED`，但 `ip link show ccmni2` 仍为 `DOWN` 且无 IP；
  - 经 `www.google.com` 探测 `https://www.google.com/generate_204` 被 `captiveDomains` 劫持至 `172.16.0.1` 超时；
  - `NfcNciApexGoogle` 在收到第 4 条命令后因缺少 `NFCEE_DISCOVER_NTF` / HCI Admin Gate 触发 90s Watchdog；
  - Camera 未握手直推 15fps 阻塞同总线 `hvc2..11`；
  - 全仓批量测试中因宿主机 `adb server` 残留已退出实例 `127.0.0.1:5911` 死连接导致单并发 11 台误报 `boot timeout`。

## 四、改动

### 1. 内置 Trojan→SOCKS5 代理引擎与热切换 API
- 新增 `agent/modules/daemon/backend/proxy/{nodes.go,trojan.go,socks5.go,proxy_test.go}`：零依赖解析 `doc/90G.yaml`（33 个 Trojan 节点），并发 204 测活优选低延迟节点，支持 SOCKS5 TCP CONNECT + UDP ASSOCIATE，支持运行期热切上游节点。
- 新增 `agent/modules/daemon/run/proxy.go` 并在 `run.go` / `http.go` 挂载 `GET /api/proxy/status` 与 `POST /api/proxy/select`。

### 2. Wi-Fi 与 Radio 数据通道 + SOCKS5 对接
- 修改 `qemu/backend/wifi-host-service/gw/{backend.go,dns.go,udp.go,ipv4.go,link.go}`：补齐 SOCKS5 用户名密码协商、RFC 1928 动态地址解析、TCP/UDP DNS 转发、UDP ASSOCIATE 及 `Link.SetIdleTimeout(d)`。
- 修改 `qemu/backend/modem-data-gateway/component/main.go`：调用 `link.SetIdleTimeout(0)` 禁用无流量断连看门狗。
- 修改 `qemu/backend/modem-host-controller/simulated/{data.go,provider_test.go}`：对齐 `interfaceName="ccmni2"`、`mtu=1400`、`netDNSOffset=2`。
- 重编并更新 `out/qemu/dist/darwin/libexec/{wifi-host-service,modem-data-gateway,vhost-modem-controller}`。
- 修改 `agent/modules/daemon/backend/{modem.go,modem_data.go,wifi.go}`：默认开启 `CellularData: true`，按实例动态生成 `modem-data.json` 与 `wifi-aps.json` 指向本地 Trojan-SOCKS5 监听地址，并从 `captiveDomains` 移除 `www.google.com`。
- 修改 `devices/hardware/android.hardware.radio/{src/radio_data.cpp,sepolicy/vendor_sepolicy.cil}`：在 `setupDataCall`/`deactivateDataCall` 自动配置/关闭 `ccmni2` 并补齐 `hal_radio_default` 的 `net_admin`/`udp_socket` 授权；重编 Android 13~17 全代（AOSP & MTK 变体）。

### 3. Bluetooth 宿主后端与虚拟外设模拟
- 将 `out/qemu/dist/darwin/libexec/lib64/` 转为自包含实体目录，内嵌 `libglib2_darwin-aarch64.dylib`、`libandroid-emu-base-logging.dylib`、`libabseil_dll.dylib`（`netsimd 0.3.40`）。
- 修改 `agent/modules/daemon/backend/bluetooth/bluetooth.go`：停机直接 `Kill()` 消除 `--no-shutdown` 2s 延迟并接入 `PeerRegistry`。
- 新增 `agent/modules/daemon/backend/bluetooth/bthci/{virtual_peers.go,virtual_peers_test.go}`：在 H4 代理层注入 Classic Inquiry EIR (`0x2f`)、Remote Name Complete (`0x07`) 与 BLE Legacy/Extended Advertising Report (`0x3e:0x02/0x0d`)，默认预置 3 台虚拟外设（`CDH-Headset-Pro`/`CDH-Speaker-Mini`/`CDH-Watch-S3`），经 `GET/POST /api/bluetooth/peers` 动态管理。

### 4. NFC 宿主后端 + QEMU `hvc11` + Guest HAL
- 新增 `agent/modules/daemon/backend/nfc/{nfcc.go,host.go,nfc_test.go}`：纯 Go 实现 NCI 2.0 虚拟 NFCC（`CORE_RESET`/`CORE_INIT`/`NFCEE_DISCOVER`/`MODE_SET`/路由表/`2F 0C`/静态 HCI `ConnID=1` Admin Gate/静态 RF `ConnID=0` ISO-DEP Type 4A NDEF 虚拟卡片），监听 `channels/nfc.sock` 并挂载 `/api/nfc/{status,tap,remove}`。
- 升级 `devices/hardware/android.hardware.nfc/{src/nfc_service.cpp,vendor/etc/init/android.hardware.nfc-service.rc,sepolicy/vendor_sepolicy.cil}`：以 raw 模式透传 `/dev/hvc11` 原始 NCI 帧，重编 Android 13~17 全代。

### 5. Camera 宿主视频输入 + QEMU `hvc12` + Guest HAL
- 新增 `agent/modules/daemon/backend/camera/{protocol.go,source.go,host.go,camera_test.go}`：`CDCM` 二进制帧协议（I420 + 真 JPEG 编码），支持 `testpattern`/`file` (ffmpeg)/`image`/`avfoundation` 四类源，经 `/api/camera/{status,source}` 热切换；收到 Guest `"CDCM_READY"` 后按 8fps 推流。
- 升级 `devices/hardware/android.hardware.camera/{src/host_video_frame.h,src/camera_provider_service.cpp,vendor/etc/init/android.hardware.camera-service.rc,sepolicy/vendor_sepolicy.cil}`：参考 `cloud-droid-hub` 使用 `libnativewindow.so` + `BufferCache` 渲染 `RGBA_8888`/`YCbCr_420_888`/`YV12`/`BLOB`（含 8B `camera3_jpeg_blob_t`），空闲 2s 周期重发 `"CDCM_READY\n"` 心跳；重编 Android 13~17 全代。

### 6. Daemon 并发接线与全仓 45 盘集增量刷新
- 修改 `agent/modules/daemon/run/{hosts.go,qemu.go,http.go,peripherals.go}`：接线 `modem-data`/`nfc`/`camera`，为 `allocateSlot` 增加互斥锁 + 20s 预留窗 + 4 端口探测，并将 `lookupActiveHosts` 改为 O(1) 内存查找；重编 `out/agent/dist/{host-daemon,cdha}`。
- 在严格遵守 `AGENTS.md §8`（零修改官方 `super.img`/`system.img`/`product.img`）前提下，增量刷新全仓 45 个 Android 盘集的 `patch.img`（同步新 Radio/Camera/NFC HAL、init `.rc` 与 `vendor_sepolicy.cil` 并完整保留 `uid/gid/mode/security.selinux`），45/45 通过 `e2fsck -fn` 校验。

## 五、验证

1. **全量 Go 单元测试回归（16/16 有测包 PASS）**：
   - `agent/modules/daemon/...`（`backend`、`bthci`、`camera`、`haptic`、`nfc`、`proxy`、`config`、`run`）全部 `ok`；
   - `qemu/backend/wifi-host-service/gw` `ok`；
   - `qemu/backend/modem-host-controller/...`（7 个包）全部 `ok`。
2. **单机深度 5 项全能 + VPN 节点运行期热切换实测**：
   - `google/oriole` (A15) Radio `ccmni2`：日本节点 HTTP 3/3 `150.249.241.56` (549ms) + HTTPS 204 2/2 (436ms)，调用 `POST /api/proxy/select` 热切美国节点后立即 HTTP 2/2 `38.46.218.20`；
   - `redmi/gold` (A15) Wi-Fi `wlan0`：日本节点 HTTP 3/3 `150.249.241.56` (580ms) + HTTPS 204 2/2 (430ms)，热切美国节点后立即 HTTP 2/2 `38.46.218.20`；
   - `vivo/PD2454`、`realme/RMX6688`、`samsung/pa3q`：同实例内完成 Wi-Fi + Radio 双网 VPN 实测通过（跨日本 `150.249.241.56` 与香港 `220.246.176.187` 节点）；
   - `google/oriole` (A14~A17)、`asus/ASUS_AI2205`、`meizu/Meizu22`、`tcl/ericanppro`：NFC 完成 NCI 2.0 初始化进入 `discovering: true`，触发 `POST /api/nfc/tap` 后 Guest `NfcService` 实时响应重选并匹配 `TagViewer (android.nfc.tech.Ndef)`；
   - Camera 跨 A13~A17 多台机型实测稳定接收 564~1736 帧、`framesDropped: 0`，并实测热切外部 MP4 (`POST /api/camera/source`) 零中断推流。
3. **全仓 45 Android 盘集端到端自动化验收（45/45 = 100% 开机实测）**：
   - 全部 45 个 Android 盘集 100% 冷启动达成 `sys.boot_completed=1`（`NOT_BOOTED=0`）；
   - 22 个盘集直接通过 Wi-Fi 或 Radio VPN HTTP 3/3 + HTTPS 204 2/2 实测（Wi-Fi 13、Radio 12 含 10 自动拨号 + 2 `pioneer` root 链路直通，覆盖日本/美国/香港 3 地域节点）；
   - 33 个盘集实测 Bluetooth `STATE_ON` + 3 台虚拟外设在线；
   - 24 个盘集实测 Camera `hvc12` 持续推帧（绝大多数 `framesDropped=0`）；
   - 7 个标准 Android NFC 盘集实测 NCI 2.0 `discovering=true` + 虚拟刷卡，29 个盘集经 `pm list features` 核实无系统 NFC 特性（`N/A`），其余 9 个为携带厂商专有 NFC 服务/未就绪的 OEM ROM。完整 45 盘集逐机数据见 [log-pass.txt](log-pass.txt)。

## 六、遗留

1. **首次开机向导未跳过对部分 OEM 自动联网的影响**：部分 OEM ROM（ColorOS/HyperOS/MagicOS/OneUI 等）在未跳过首次开机向导前不会自动关联开放 Wi-Fi 或发起上层 `setupDataCall`，但在完成向导或手动点击联网后均直接复用同一套已验证的 `wifi-host-service` / `modem-data-gateway` / Trojan-SOCKS5 底层链路。
2. **部分 OEM 专有 NFC 栈（9 个盘集）**：如 `ASUSAI2501`/`RMX6688`/`pa3q`/`CPH2767` 等自带厂商定制 NFC 服务（依赖厂商专有芯片固件或厂商 HIDL/AIDL 扩展），后续若需在这些机型启用虚拟 NFC，可按需在 `09_build_vendor` 替换为 AOSP `NfcNci` 体系或适配其厂商扩展命令。
3. **macOS 宿主 `avfoundation` 摄像头权限**：`camera` 引擎已支持 `avfoundation` 实时采集源，但在非交互式守护进程环境下调用真机摄像头需预先在 macOS 隐私设置中授予宿主进程摄像头权限。
