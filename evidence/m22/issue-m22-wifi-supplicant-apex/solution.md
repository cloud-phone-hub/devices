# m22 WiFi 打不开：wpa_supplicant 在 vendor APEX 里，07 步骤 2b 默认集合没带它

- 设备：`flyme/m22`（MEIZU 22，QTI `sun`，底包 `lineage-23.2` / Android 16）
- 阶段：`07_build_vendor`（引擎侧：步骤 2b「摊平 vendor APEX」的默认集合）
- 现场实例：daemon 实例 44（失败）/ 实例 45（m22 通过）/ 实例 7（gold 对照通过）
- 日期：2026-10-30

## 1. 问题

接 #74（SELinux 域补齐）之后，m22 已能进 Flyme 桌面，但 **WiFi 开关打不开**：打开会自己弹回，
扫不到也连不上任何 AP。现场日志（`44/qemu.log`）：

```
servicemanager: Could not find android.hardware.wifi.supplicant.ISupplicant/default
               in the VINTF manifest. No alternative instances declared in VINTF.
I am_wtf: [0,935,system_server,-1,SupplicantStaIfaceHal,
           Failed to get internal ISupplicantStaIfaceHal instance.]
E SupplicantStaIfaceHal: Internal ISupplicantStaIfaceHal instance does not exist.
E WifiNative: Failed to start supplicant
E WifiActiveModeWarden: ClientModeManager start failed!
           ConcreteClientModeManager{iface=wlan0 role=ROLE_CLIENT_SCAN_ONLY}
```

而 init 侧 `vendor.wifi-vhal` 与 `wificond` 都是 `started`，唯独没有 `wpa_supplicant` 这个服务；
装配树里 `find vendor -iname "*supplicant*"` 是空的。

## 2. 根因

底包 `lineage-23.2`（Android 16）把 wifi 认证栈做成了 vendor APEX：

```
unpack_vendor/apex/com.android.hardware.wpa_supplicant.mainline_common.apex
    apex/bin/hw/wpa_supplicant                                3,451,208 B
    apex/etc/com.android.hardware.wpa_supplicant.rc
    apex/etc/vintf/manifest/android.hardware.wifi.supplicant.xml
```

而 07 的两个环节都没有覆盖它：

1. **步骤 2（`materializeVendorBase`）只回填白名单** `bin/etc/lib/lib64/firmware_mnt`，
   `apex` 顶层被显式丢弃 —— 这是设计如此（APEX 目录形态不能直接当 vendor 根用）。
2. **步骤 2b（`materializeVendorApex`）负责把选中的 APEX 摊平成散件**，但默认集合原先只有
   `graphicsApexHints`（`graphics.composer` / `gralloc` / `mapper.` / …），只服务图形显示栈。
   m22 的 hwc3/allocator 正是靠它救活的（#70/#71 之后能进桌面），**认证栈没有对应条目**。

缺件的后果链是单向的：树里没有 `bin/hw/wpa_supplicant` → 没有 `etc/init/*.rc` → init 不存在
该服务 → VINTF 清单里没有 `ISupplicant` → 框架 `SupplicantStaIfaceHal` 建不出客户端 →
`WifiNative: Failed to start supplicant` → `ClientModeManager start failed` → 开关打不开。

## 3. 证据

- `log-fail.txt`：七节。框架侧四层报错、init 侧服务缺失对照（vhal/wificond 在、supplicant 不在）、
  装配树空查、底包 APEX 内容清单、以及「图形栈早靠 2b 救活」的对照。
- `log-pass.txt`：七节。引擎改动 diff 锚点、m22 构建与镜像回读、m22 运行态、gold 对照、
  端到端连通性、host 侧 L2 网关统计，以及一个「残留 WifiConfigStore 假失败」的坑。

## 4. 改动

`engine/build/07_build_vendor/apex.go`（零新增依赖，复用 2b 已有的 ext4/erofs payload 解包、
`/apex/<名>/` → `/vendor/` 的 rc 改写、步骤 6b 的 exec 标签反推）：

```go
// wifiApexHints 是「wifi 认证栈」APEX 的文件名特征（小写子串）。
var wifiApexHints = []string{
	"wpa_supplicant",
	"hostapd",
}

// default（空）   图形显示栈 + wifi 认证栈（graphicsApexHints + wifiApexHints）
case "", apexFlattenDefault:
	return matchesApexHint(graphicsApexHints, lower) || matchesApexHint(wifiApexHints, lower)
```

配套：`apexFlattenDefault` 的常量注释与文件头「背景」段落补上 wifi 认证栈的由来；
`plan.go` 的 `-apex-flatten` 帮助文本同步为「default（默认：图形显示栈 + wifi 认证栈）」。
`graphics` 模式保留原语义（只图形），`all` / `none` / 自定义 glob 不变。

选 `default` 而非常开 `all` 的理由：`all` 会把 audio/boot/cas 等也摊平，等于绕过厂商覆盖段的
后层语义；只加「装机即生效、引擎侧没有自研模块、缺了就起不来」的第二类，风险面最小。

## 5. 验证

**构建侧（m22，`out/tmp/m22-wifi-supp.log`，`EXIT=0`）**

- 摘要行：`vendor APEX: default（预计摊平 3/12：…composer.drm_hwcomposer_upstream /
  com.android.hardware.wpa_supplicant.mainline_common / org.lineageos.device.gralloc.minigbm_upstream）`
- 步骤 2b：`合计: 新增 27 / 覆盖 44 / 跳过 6，rc 改写 4 个文件 4 行（3 个 APEX）`
- 装配树 **669 → 687**（文件 455 / 链接 199 / 目录 33），`vendor.img` **206,700,544 → 213,262,336 B**
- 镜像回读：`/bin/hw/wpa_supplicant` 3,451,208 B（0755 0:2000）、`/etc/init/com.android.hardware.wpa_supplicant.rc`
  327 B、`/etc/vintf/manifest/android.hardware.wifi.supplicant.xml` 332 B；
  `vendor_file_contexts` 新增 `/(vendor|system/vendor)/bin/hw/wpa_supplicant u:object_r:hal_wifi_supplicant_default_exec:s0`
  （由步骤 6b 按 rc 的 `seclabel u:r:hal_wifi_supplicant_default:s0` 反推）

**运行侧（m22 实例 45，161,613 行日志）**

- `init: ... started service 'wpa_supplicant'` 出现，`Failed to start supplicant` **0**
- `Found android.hardware.wifi.supplicant.ISupplicant/default in device VINTF manifest` **10** 次
- 进程域是 `sid=u:r:hal_wifi_supplicant_default:s0`（不是 init），说明 exec 标签生效
- `cmd wifi status`：`Wifi is connected to "CDH-A"`，IP `192.168.60.100`，`Supplicant state: COMPLETED`；
  `wlan0 UP 192.168.60.100/24`

**gold 对照（实例 7，同引擎全量重建）**

- gold 底包是 `lineage-22.2`（Android 15），supplicant 仍是 vendor 根下散件，
  `vendor APEX: graphics（预计摊平 0/9）`、`没有需要摊平的 APEX` —— 说明改默认集合不误伤老底包
- 装配树 781（文件 544 / 链接 198 / 目录 39）、`vendor.img` 166,268,928 B
- `wifi-vhal` / `wificond` / `wpa_supplicant` 各 started 1 次，`cannot setexeccon` 0、`received signal 6` 0
- 连上 `CDH-A`（`192.168.60.100`），`ping 192.168.60.1` 与 `ping www.baidu.com` 均 **0% 丢包**，
  `dumpsys connectivity` 该网络 `INTERNET & VALIDATED`
- host 侧 `wifi-host-service.log`：`scan_profile_ack ok=true`、`rx=6987 tx=7929 drop=0`
- 截图 `.tmp/gold7-wifi-ok.png`（HyperOS 桌面，状态栏 WiFi 图标满格）

## 6. 遗留

1. **`graphics` 模式仍是单口子**：如果将来某机型清单显式写 `-apex-flatten=graphics`（gold 本轮
   构建就是这样跑的），Android 16 底包上会重现本问题。建议把「认证栈」从模式枚举里摘出来做成
   叠加修饰（如 `graphics+wifi`），或直接废掉 `graphics`、只留 `default/all/none/glob`。
2. **`hostapd` 已在 hints 里但未实测**：本轮的验证只覆盖 STA 侧（连 AP）；热点（SoftAp）需要
   另一套 host 侧通道，尚未跑过。
3. **残留 `WifiConfigStore` 会造成假失败**：同一 SSID 存在旧的 PSK 配置时，日志是
   `assoc key_mgmt 0x0 network key_mgmt 0x4` + `Network not found event received`，与缺 supplicant
   的报错完全不同。定位时先 `cmd wifi forget-network <id>` 排除干扰（本轮 gold 就踩了一次）。
4. **gold 新镜像仍有 v412 的 107 次 `CANNOT LINK`（`libchrome.so`）**：这是 #74 遗留的
   `hal.go:409` 两趟假设——冷启动第一趟必然漏 15 个 v412 运行库。建议把 `moduleTargets` 的
   求值挪到步骤 3 编译之后（属独立任务，不在本次改动范围）。
5. **无回归基线**：`Failed to start supplicant` 计数、`ISupplicant` 在 VINTF 里的可见性，
   宜做成 daemon 启动自检项（与 #74 的 `cannot setexeccon == 0` 同类）。
