# 蜂窝网卡与 Connectivity 全栈对齐：双 PDN（`ccmni0` 常驻 IMS + `ccmni1` 双栈/纯 IPv6 上网）、网关 IPv6/DNS64/NAT64 数据面、广电及全球运营商 APN 预置闭环（`#329`）

- **设备/机型**：跨机型引擎通用（首发在线对拍与多运营商实测机型：`redmi/gold` Redmi Note 13 5G `15-OS3.0.11.0.VNQCNXM`，在线真机 `ec519c7d513a3ad3` × 云机 #1 `4c17e53fbaef35ca-cloud`）
- **日期**：2026-11-25

## 1. 问题

在对 `Redmi Note 13 5G #1`（`gold_15-OS3.0.11.0.VNQCNXM/1`）与同版本在线 USB 真机（`OS3.0.11.0.VNQCNXM`）对比蜂窝网卡、`ConnectivityManager`、IPv6 数据面及国内外运营商 APN 支持度时，暴露出 6 项核心问题：

1. **`ccmni0` 常驻 IMS IPv6 网络整体缺失（`aligner.network_full` `fatal #1`）**：真机即使关闭移动数据开关（`mobile_data=0`）并连接 Wi-Fi，`NetworkInterface` 仍暴露处于 `UP` 的 `ccmni0`（`mtu=1400`、无 MAC、携带运营商公网 `/64` 非 EUI-64 IPv6 与同后缀 `fe80::/64`），且 `ConnectivityManager` 同时存在 `wlan0`（`WIFI`）与 `ccmni0`（`MOBILE` `extra: ims`，含 IPv6 `DnsAddresses`、`PcscfAddresses`、`IMS&MMTEL&NOT_METERED&VALIDATED`，请求方 `com.mediatek.ims` `uid=1001`）；云机仅有一条 `WIFI` 网络。
2. **残留僵尸 `ccmni2` 接口暴露内网 IPv4（`aligner.network_full` `fatal #2`）**：早期拨号后 `deactivateDataCall` 仅清除了 `IFF_UP` 而未清空 IP 地址，导致无权限 App 调用 `NetworkInterface.getNetworkInterfaces()` 仍能扫出 `ccmni2 (up=false, host=100.64.155.15)`。
3. **单槽仅支持单条单栈 `DataCall` 且暴露 `.2` 私有网关 DNS**：原 `modem-host-controller` 将同槽活跃 `DataCall` 限制为 1 条，数据关闭时一刀切拒绝 `ims` APN，缺少 `PcscfAddresses` TLV 字段，且返回 `100.64.155.2` 虚拟网关作为 DNS。
4. **宿主用户态网关 `net-host-service` 缺乏 IPv6 数据面**：原 `gw/gateway.go` 直接丢弃以太网层 `0x86dd (IPv6)` 帧，不支持 NDP 邻居/路由发现、ICMPv6 Echo、DNSv6、DNS64/NAT64（`64:ff9b::/96`）及 IPv6 TCP/UDP 出口转发，导致 `ping6` 100% 丢包、纯 IPv6 字面量连接不可达，且无法承载美国 T-Mobile `fast.t-mobile.com` 等纯 `IPV6` 单栈 APN。
5. **国际卡硬锁 `CountryCode == "86"` 且缺中国广电（`46015`）与海外运营商画像**：`daemon/backend/modem.go`、`modem-host-controller/{model/identity.go,telephony/telephony.go}` 与客机 `android.hardware.radio/src/state.cpp` 均硬编码要求 `CountryCode == "86"` 与 11 位手机号，`registeredNet` 硬编码 `CountryISO: "cn"`，导致切换美国/英国/日本等海外 SIM 预设时被拦截回退为中国联通；同时缺少中国广电（`46015`）及美/英/日/韩/港澳台专属 IPv6 前缀、公网 DNS 与 P-CSCF 画像。
6. **跨机型海外/广电 APN 预置与换卡自动激活缺口**：部分国行或精简底包缺失中国广电（`46015`）或海外运营商的 `default`/`ims` APN 条目；且在同一云机实例上更换新 ICCID SIM 卡产生新 `subId`（如 `subId=2/3`）后，旧 `defaultDataSubId=1` 失效导致系统不自动拨号上网。

## 2. 根因

1. **IMS 豁免数据开关机制未触发**：Android `DataNetworkController` 对 `NET_CAPABILITY_IMS` 请求豁免 `mobile_data` 开关并免探测置 `VALIDATED`；云机未运行闭源 IMS 守护进程持有该请求。
2. **Linux 下线接口保留地址表**：内核在 `IFF_UP` 清零后仍保留 `in_ifaddr`/`inet6_ifaddr`，被 Java `NetworkInterface` 枚举暴露。
3. **网关仅实现 ARP/IPv4 状态机**：缺少 `ipv6.go`（RFC 4861 NDP NS/NA/RS/RA、ICMPv6 Echo、DNSv6、RFC 6052/6147 DNS64+NAT64 `64:ff9b::/96`）及 `tcp.go`/`udp.go` 的双栈伪首部校验和与 MSS 适配（蜂窝 `MTU=1400` 下 IPv6 TCP `MSS=1340`）。
4. **多处中国区号码长度硬编码与 `app_process` Binder 限制**：宿主/客机四处校验锁死 `+86` + 11 位号码；此外 `app_process` 进程无权读 `cache_key.*` 属性导致 `SubscriptionManager` 缓存恒返回 `-1`，需直调 `ISub` Binder 并在换卡后自动迁移 `defaultDataSubId`，且 `uid=1001` 需经 `IActivityManager.getContentProvider(null, pkg, "telephony", 0, false)` 访问 `TelephonyProvider`。

## 3. 证据

- 修复前 `aligner.network_full` 致命差异、`ccmni2` 僵尸地址泄露与单网快照见 [`log-fail.txt`](log-fail.txt)。
- 修复后宿主/客机单测、在线真机双机 `device-checker` 终验（`run-00405..00406`）、以及美国 T-Mobile（`310260`）、中国广电（`46015` `cbnet`）、中国联通（`46001` `3gnet`）三套 SIM 预设的 IPv4/IPv6 双栈与字面量 IPv6 端到端实测见 [`log-pass.txt`](log-pass.txt)（`patch.img` `d44f963bcbc9...`）。
- 关联前置证据：`../issue-biometrics-sensors-vintf-anti-fraud-align/`（`#327`）、`../../gold/issue-gold-os3-0-11-real-device-align/`。

## 4. 改动

全程严守 `AGENTS.md §4/§7/§8`，零触碰 `super.img` / `product.img` / `system.img`：

1. **宿主 `qemu/backend/net-host-service` + `agent/modules/daemon/backend/proxy`（真 IPv6 / DNS64 / NAT64 全栈数据面）**：
   - 新增 `gw/ipv6.go`，升级 `gw/{gateway.go,ipv4.go,dns.go,tcp.go,udp.go}`：实现 RFC 4861 Proxy-NDP（`NS->NA` `HopLimit=255`，跳过 `::` DAD 防冲突）与 `RS->RA`（`fe80::2`）、ICMPv6 Echo 应答、IPv4/IPv6 运营商公网 DNS `:53` 透明劫持、Captive AAAA（`64:ff9b::ac10:1`）与 RFC 7050 `ipv4only.arpa` 应答、DNS64（纯 IPv4 域名自动合成 `64:ff9b::<v4>`）、`A`+`AAAA` 双栈域名反查缓存，以及适配 `MTU=1400`（`tcpMSSV6=1340`）的 IPv6 TCP/UDP 出口转发；在 `gateway_test.go` 补齐 `TestCarrierDNSAndAAAA` 全栈 IPv6 单测。
   - `daemon/backend/proxy/rules.go`：为 `GEOIP,CN` 与默认直连规则补入国内主流公网 IPv6 段（`2400:3200::/32`、`2402:4e00::/32`、`2408:8000::/20`、`2409:8000::/20`、`240e::/18`）。
2. **宿主 `qemu/backend/modem-host-controller` + `agent/modules/daemon`（双 PDN + 国际卡解锁 + 广电/全球运营商画像）**：
   - `model/{types.go,validate.go,identity.go}`、`telephony/telephony.go`、`protocol/{tlv.go,messages.go}`、`daemon/backend/modem.go`：新增 `DataCall.PcscfAddresses`（TLV `tag=8`），支持同槽多 PDN（`ccmni0` IMS + `ccmni1` 上网）并存与 IMS 豁免数据开关；全面放开 `CountryCode`（1~3 位国际区号）与 7~14 位号码（ITU-T E.164 `<=15` 位）校验；新增 `TestConcurrentImsAndDefaultPDNOnSameSlot` 与 `TestBroadnetAndInternationalSIMIdentity`。
   - `simulated/{provider.go,data.go,provider_test.go}` & `daemon/{config/sim.go,run/sim_config.go}`：按 MCC/PLMN 动态派生 `CountryISO`（`cn`/`us`/`gb`/`jp`/`kr`/`hk`/`tw`/`sg` 等），内置中国移动、联通、电信、**中国广电（`46015` `2409:8c20::` / DNS `117.50.10.10`）**、**美国 T-Mobile（`310260` `2607:fb90::`）/ AT&T（`310410` `2600:380::`）/ Verizon（`311480` `2600:1000::`）** 及英/欧/日/韩/港澳台运营商 IPv6/DNS/P-CSCF 画像与一键 SIM 预设（`cn-cbn-single`、`cn-dsds-cbn`、`us-dsds-esim`、`us-verizon-single`、`gb-dsds-esim`、`jp-dsds-esim`）；`ccmni1` 下发 `fe80::2` IPv6 下一跳；新增 `TestImsAndDefaultCoexistAndCarrierIPv6` 与 `TestBroadnetAndUSCarriers`。
3. **客机 `devices/hardware/android.hardware.radio` + `android.service.daemon`（Netlink IPv6、全球预置 APN 库与换卡自愈）**：
   - `radio/src/{state.cpp,data_policy.cpp,radio_data.cpp}` & `vendor_sepolicy.cil`：放开国际 E.164 身份校验；支持 `NETLINK_ROUTE` 配置公网 `/64` + 同后缀 `fe80::/64` IPv6、透传 `pcscf`，并在 `deactivateDataCall` 经 `RTM_DELADDR` 彻底清空接口全部 IPv4/IPv6 地址。
   - `service.daemon/vendor/bin/cph-netmgr-setup` & `mgmt_bridge.rs`：`net2` 命名为 `ccmni1`（`mtu=1400`），`ccmni0` 专供 IMS，补齐 `ccmni2..20` 与 `p2p1`。
   - 新增 `vendor/etc/cph-apns-preset.xml`（**139 条**国内外主流运营商 `default`+`mms`+`ims` 预置表）与 `ImsNetAgent.java`（`cph-ims-netmgr.jar`）：直调 `ISub` Binder 在换卡后自动将当前在槽活跃卡提升为 `defaultDataSubId`，经 `getContentProvider(null, pkg, "telephony", 0, false)` 检测并无侵入补齐缺失的广电/海外 APN 至 `telephony.db`，并持久维护 `NET_CAPABILITY_IMS` 请求驱动 `ccmni0` 常驻在线。

## 5. 验证

1. **全仓单元测试门禁全绿**：
   - `qemu/backend/net-host-service`：`go test ./...` 通过（含 NDP/ICMPv6/DNSv6/NAT64 TCP 204 单测）。
   - `qemu/backend/modem-host-controller`：全 7 个 Go 包单测通过（含双 PDN、中国广电 `46015`、美国 T-Mobile/Verizon、英国/香港国际号码单测）。
   - `agent/modules/daemon`：全 9 个 Go 包单测通过；客机 `radio` C++ `data_policy_test` 通过。
2. **多运营商（美区 T-Mobile + 中国广电 + 中国联通）与 IPv6 数据面实机终验（`patch.img` `d44f963bcbc9...`）**：
   - **美国 T-Mobile（`us-dsds-esim`，`310260`，`CountryISO=us`，自动切换 `mSubId=2`）**：冷启动自动拉起 `ccmni0`（`ims`，`2607:fb90:4eb1:4928:...`，P-CSCF `2607:fc20:1000::1`，60ms `VALIDATED`）；关 Wi-Fi 开数据后 `ccmni1`（`epc.tmobile.com`，双栈 `100.64.155.15/24 + 2607:fb90:4eb1:4929:...`，DNS `8.8.8.8, 2607:fb90:1800::1`，326ms `VALIDATED`）并发在线；`ping6 2400:3200::1` 与 `ping6 2001:4860:4860::8888` 均 **`0% packet loss`**（`0.19~0.25ms`），域名 HTTP（`www.baidu.com` `200 OK`、`www.gstatic.com/generate_204` `204 No Content`）与**纯 IPv6 字面量 TCP（`[2400:3200::1]:80`）** 全部打通。
   - **中国广电（`cn-cbn-single`，`46015`，`中国广电`，自动切换 `mSubId=3`）**：冷启动自动拉起 `ccmni0`（`ims`，`2409:8c20:daa9:955c:...`，P-CSCF `2409:8c00:9000::1`，29ms `VALIDATED`）；开数据后 `ccmni1`（`cbnet`，双栈 `100.64.155.15/24 + 2409:8c20:daa9:955d:...`，DNS `117.50.10.10, 2409:8c00::1`，316ms `VALIDATED`）并发在线；IPv4 `ping`、IPv6 `ping6`、国内外 HTTP 与纯 IPv6 字面量 TCP 全部 `0%` 丢包跑通。
   - **恢复中国联通稳态（`cn-dsds-esim`，`46001`）与在线真机 `device-checker` 对比（`run-00405..00406`）**：`CphImsNetAgent` 零告警 33ms 连上 `ccmni0` IMS；`aligner.network_full` **`fatal: 2 -> 0`（`exit=0`）**，`ultimate.location_network` **27/27 `cloud_only=[]`**，`gold` 总分 **`31/32`**（`G1~G4` 全 `True`）。

## 6. 遗留

1. 其他机型需启用本套全球预置 APN 库（`/vendor/etc/cph-apns-preset.xml`）、换卡自动默认卡槽接管与常驻 IMS 承载网时，无需修改机型清单，直接执行 `pipeline device build -from build_hardware -skip pack_super -skip pack_vda <device>` 刷新 `patch.img` 即可获得全套能力（宿主 `vhost-modem-controller`、`net-host-service` 与 `cdha` 已全局生效）。
