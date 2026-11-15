# #208：虚拟蜂窝网卡 `ccmni2` 脱离 QEMU 默认 MAC 前缀 `52:54:00`

## 问题

`gold_15-OS3.0.11.0.VNQCNXM` 首轮 `aligner.network_full` 发现云机接口 `ccmni2` 的 SLAAC / link-local IPv6 地址为：

- `fe80::5054:ff:fe12:3458%ccmni2`
- `fec0::5054:ff:fe12:3458`

由 EUI-64（`50:54:00:ff:fe:12:34:58` 翻转 U/L bit）可直接反推出网卡硬件 MAC 为 **`52:54:00:12:34:58`**，其中 `52:54:00` 是 QEMU/KVM 默认 virtio 网卡 OUI 前缀，普通 App 通过 `NetworkInterface.getInetAddresses()` 即可识别虚拟化环境。

## 根因

1. 宿主机 daemon `core/host/daemon/config/device.go` 中 `CellularNetwork()` 硬编码 `net2` 网卡 MAC 为 `"52:54:00:12:34:58"`（未定义为非 QEMU 局部管理地址）。
2. guest 侧 `engine/vendor/hardware/android.service.daemon/vendor/bin/cph-netmgr-setup` 在 `fingerprint` 子命令中也写死只按 `"52:54:00:12:34:58"` 查找 `net2` 承载网卡并重命名为 `ccmni2`。

## 证据

- 修复前：`4503ac70a1c898f3-cloud` `network.interfaces` 中 `ccmni2` 携带 `fe80::5054:ff:fe12:3458%ccmni2` / `fec0::5054:ff:fe12:3458`，见同目录 [`log-fail.txt`](log-fail.txt)。
- 修复后：Go 单测（`TestCellularNetworkNonQEMUMAC`）+ 实例 `2`（`4d5128bd8ca07462-cloud`）`ip addr show ccmni2` 与 `aligner.network_full` 重采输出见同目录 [`log-pass.txt`](log-pass.txt)。

## 改动

1. **`core/host/daemon/config/device.go`**：
   - 新增导出常量 `DefaultCellularMAC = "c2:36:b4:12:34:58"`（符合 IEEE 802 局部管理单播地址规范：首字节 `0xc2` 次低位 bit1=1、最低位 bit0=0，非 QEMU `52:54:00` OUI）；
   - `DeviceSpec.CellularNetwork()` 改用 `DefaultCellularMAC`。
2. **`core/host/daemon/config/device_network_test.go`**：
   - 新增同包单测 `TestCellularNetworkNonQEMUMAC`，断言 `CellularNetwork().MAC == DefaultCellularMAC`、非 `52:54:00:` 前缀，且首字节满足 `hw[0]&0x03 == 0x02`。
3. **`engine/vendor/hardware/android.service.daemon/vendor/bin/cph-netmgr-setup`**：
   - `fingerprint` 分支匹配网口 MAC 改为同时识别 `c2:36:b4:12:34:58` 与兼容旧宿主的 `52:54:00:12:34:58`，确保改名 `ccmni2` 与设置 `mtu 1400` 正常生效。

## 验证

1. 单测：`cd /Volumes/work/cloud-phone-hub/core/host/daemon && go test -v -run 'TestCellularNetworkNonQEMUMAC' ./config` 通过（`0.00s`）。
2. 实例 `gold_15-OS3.0.11.0.VNQCNXM/2`（`4d5128bd8ca07462-cloud`）重建宿主机 daemon 与系统镜像并重启实测：
   - shell `ip link` / `ip addr`：`ccmni2` `link/ether c2:36:b4:12:34:58`、`inet6 fec0::c036:b4ff:fe12:3458/64`、`inet6 fe80::c036:b4ff:fe12:3458/64`；
   - `aligner.network_full` 重采对拍：`network` 全域扫描 `5054:ff` 与 `52:54:00` 均为 `False`，`ccmni2` 地址已变为 `fe80::c036:b4ff:fe12:3458%ccmni2` 与 `fec0::c036:b4ff:fe12:3458`。

## 遗留

1. 当前所有实例共用常量 `DefaultCellularMAC = "c2:36:b4:12:34:58"`，若后续需避免多台云机同局域网 IPv6 后缀相同，可进一步改为按实例 ID 派生局部管理单播 MAC。
2. 同域内蓝牙适配器名仍为 `2312DRAABC`（真机 `Redmi Note 13 5G`），留待 P1 收口。
