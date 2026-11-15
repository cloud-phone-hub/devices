# `#229` QEMU 网络栈参数对齐（SLIRP IPv6 关闭、蜂窝接口初始 `DOWN` 态与 `ethtool_get_drvinfo` 收敛）

- 设备/机型：`common`（跨机型通用 QEMU 与 `cdh_device_info` 网络栈能力；`redmi/gold` `15-OS3.0.11.0.VNQCNXM` 首次实测闭环）
- 关联报告：[`gold-aligner-20261114/network`](../../../../report/xiaomi/gold-aligner-20261114/network.md)、前置条目 [`#208`](../issue-ccmni2-non-qemu-mac/solution.md)
- 状态：已解决（`2026-11-15`）

## 问题

`gold_15-OS3.0.11.0.VNQCNXM`（及共用 QEMU SLIRP 网络与 `cdh_device_info` 模块的云机）在 `network` 域及接口元数据采集中存在三项虚拟化环境特征差异：

1. **SLIRP IPv6 `fec0::` 路由广播**：QEMU `-netdev user` 默认开启 IPv6 RA，导致 `ccmni2` 自动获取 `fec0::c036:b4ff:fe12:3458/64`（SLIRP 保留前缀 `fec0::`）站点地址。
2. **无真实蜂窝数据连接下 `ccmni0`/`ccmni2` 呈 `UP` 态**：真机在 Wi-Fi 主网且电信卡无服务时，蜂窝接口 `ccmni0`/`ccmni2` 不挂入激活枚举表且初始为 `DOWN` 态；而云机 `cph-netmgr-setup` 主动拉起 `ccmni0`/`ccmni2` 并写入 `240e::` 全局 IPv6 地址，导致 `network.interfaces` 多出 2 个活跃接口（`network.capability = 2`）。
3. **`ethtool -i` 暴露虚拟化驱动信息**：`wlan0`/`p2p0` 上报 `driver: mac80211_hwsim`、`bus-info: hwsim0/hwsim1`，`ccmni2` 上报 `driver: virtio_net`、`version: 1.0.0`、`bus-info: 0000:00:08.0`，`dummy0`/`ccmni0` 上报构建核版本 `6.6.127-4k-cdh`，与真机驱动模型（`wlan0=wlan`、`dummy0=dummy`、`ccmni*`/`p2p*` 返回 `-EOPNOTSUPP`）不一致。

## 根因

1. [`core/host/daemon/run/qemu.go`](../../../../core/host/daemon/run/qemu.go) 构造 `id=net0`（`eth0`）与 `id=net2`（`ccmni2`）两个 QEMU user-mode netdev 参数时未传递 `ipv6=off`，SLIRP 默认广播 `fec0::/64` Router Advertisement。
2. [`cph-netmgr-setup`](../../../vendor/hardware/android.service.daemon/vendor/bin/cph-netmgr-setup) 在对齐分支为 `ccmni0` 静态配置 `240e:...` IPv6 并使 `ccmni0`/`ccmni2` 常驻 `UP`，未模拟 Wi-Fi 优先待机时的初始 `DOWN` 态；同时即使关停接口，Linux 内核在收到 RA 前的 SLAAC 配置也未统一禁用。
3. [`devices/modules/device_info`](../../../modules/device_info/core/main.c) 此前已对齐 `/proc/version` 与 `uname` 字符串及隐藏 `mgmt0`，但尚未接管内核 `ethtool_get_drvinfo`（`ETHTOOL_GDRVINFO`）路径，内核直接返回底层 `virtio_net`/`mac80211_hwsim` 驱动与 PCI BDF 地址。

## 证据

- 修复前基线摘录见 [`log-fail.txt`](log-fail.txt)：`vm-shell-baseline.txt` 中 `ccmni2` 含 `fec0::`、`ccmni0` 含 `240e::`，`ethtool` 输出 `mac80211_hwsim` / `virtio_net` / `0000:00:08.0` / `6.6.127-4k-cdh`，`network.interfaces` 多出 `ccmni0`/`ccmni2`。
- 修复后实测见 [`log-pass.txt`](log-pass.txt)。

## 改动

1. **QEMU SLIRP 关闭 IPv6 广播（[`qemu.go`](../../../../core/host/daemon/run/qemu.go)、[`qemu_net_test.go`](../../../../core/host/daemon/run/qemu_net_test.go)）**：
   - 在 `qemu.go`（L599–L612）为 `net0` 与 `net2` 的 `-netdev user,...` 参数统一追加 `,ipv6=off`，从宿主机用户态网络栈源头阻断 `fec0::` RA；
   - 在 `qemu_net_test.go` 新增单元测试 `TestQemuArgsNetdevIPv6Off` 校验两个 netdev 参数均包含 `ipv6=off`。
2. **蜂窝接口初始链路态与地址清理（[`cph-netmgr-setup`](../../../vendor/hardware/android.service.daemon/vendor/bin/cph-netmgr-setup)）**：
   - 在 `cph-netmgr-setup`（L46–L80）对 `ccmni2` 设置 `net.ipv6.conf.ccmni2.accept_ra=0` 与 `autoconf=0`，清除站点/非链路本地 IPv6（含旧 `fec0::`），随后将 `ccmni2` 与 `ccmni0` 设为 `mtu 1400 down` 且移除静态 `240e::` 地址注入。
3. **`cdh_device_info` 接管 `ETHTOOL_GDRVINFO`（[`ethtool_drvinfo.c`](../../../modules/device_info/net/ethtool_drvinfo.c)、[`core/main.c`](../../../modules/device_info/core/main.c)、[`cdh_device_info.h`](../../../modules/device_info/include/cdh_device_info.h)、[`Kbuild`](../../../modules/device_info/Kbuild)）**：
   - 新增 `net/ethtool_drvinfo.c`，通过 `kretprobe` 挂载内核 `ethtool_get_drvinfo`（并声明 `cdh_ethtool_devlink_compat`）；
   - 由 `cdh_ethtool_should_suppress()` 对 `virtio_net`、`ccmni*`、`p2p*`、`eth*`、`mgmt*` 统一返回 `-EOPNOTSUPP`（用户态表现 `Operation not supported on transport endpoint`）；
   - 对 `wlan*` / `mac80211_hwsim` 将 `info->driver` 归一化为 `"wlan"`、清空 `bus_info`、将 `version` 同步为已对齐的 `init_uts_ns.name.release`；对 `dummy*` 同步覆盖 `version` 为 `init_uts_ns.name.release`。

## 验证

1. **宿主机 Go 单元测试**：
   - `cd /Volumes/work/cloud-phone-hub/core/host/daemon && TMPDIR=/Volumes/work/cloud-phone-hub/engine/out/tmp go test ./run/... -v` -> `PASS ok cloud-phone-hub/core/daemon/run 1.912s`（`TestGpuArgRefreshRates`、`TestQemuArgsNetdevIPv6Off` 均通过）。
2. **实例 `gold_15-OS3.0.11.0.VNQCNXM/3`（`61f8a3a6a912df72-cloud`）运行时验证**：
   - `ip -6 addr` 仅剩 `lo`（`::1`）、`dummy0`（`fe80::...`）、`wlan0`（`fe80::...`），无任何 `fec0::` 或 `240e::` 地址；`ip link show ccmni0` 与 `ccmni2` 均呈 `state DOWN`；
   - shell 与 root 下 `ethtool -i` 均输出 `wlan0: driver=wlan version=6.6.118-android15-8-ge56cf6b09c bus-info=""`、`dummy0: driver=dummy version=6.6.118-android15-8-ge56cf6b09c`，`ccmni0`/`ccmni2`/`p2p0` 均返回 `Cannot get driver information: Operation not supported on transport endpoint`；
   - `aligner` `network_full`（`run-00237`）重采 `capability` 由 `2` 清零至 `0`。

## 遗留

- `network` 域剩余 2 项 `identity` 为新启实例首启时序引起的 `bluetooth` 名延迟同步字段，以及 1 项依赖无线热点实时连通态的 `availability`（`wifiConnected=False`），不涉及虚拟化驱动参数问题。
