# 小米澎湃 OS 处理器/相机参数展示、宿主代理国内分流直连与 02_prepare 基带版本全链路反馈

## 问题

在 `Xiaomi 17 Pro Max #1`（`xiaomi/popsicle`，`16-OS3.0.319.0.WPBCNXM`，HyperOS 3 / Android 16）运行实例中，打开系统「设置 → 我的设备 → 全部参数与信息」存在两项硬件信息缺失：

1. **处理器营销名与前后置相机规格留空**：页面不显示「第五代骁龙®8至尊版移动平台」与「前置50MP 后置50MP+50MP+50MP」，`logcat` 反复报 `DeviceParamsManager: Failed to get basic device info`。
2. **基带版本未从 `02_prepare` 反馈到云机制作与运行流程**：实例内 `getprop gsm.version.baseband` 为 `,`（设置页显示基带未知），`devices/manifest/configs/xiaomi/popsicle/popsicle-android16/vendor/config.json`、`prepare.json` 及 `out/devices/dist/xiaomi/popsicle/*/device.json` 均缺少 `gsm.version.baseband` / `baseband-version`。

## 根因

### 一、处理器与相机规格缺失（在线分流阻塞 + 离线未预置）

1. **HyperOS `Settings` 动态拉取机制**：反编译 `Settings.apk`（`DeviceParamsManager`、`DeviceBasicInfoPresenter`、`DeviceParamsInitHelper`）确认，澎湃 OS 不会从本地 `build.prop` 拼装处理器营销名与相机像素，而是跨进程委托 `com.android.htmlviewer`（`RemoteParamsService`）向国内接口 `https://port.sec.miui.com/myDevClound/{devInfoNew,allparamInfo}` 发起 HTTPS 请求，拿到 JSON 后持久化至 `/data/user_de/0/com.android.settings/shared_prefs/device_params_pref.xml`（键 `basic_info_key`、`camera_info_key`，要求 `BasicInfoToggle="1"`）。
2. **宿主代理未解析 Clash 分流规则 + L2 网关丢失域名上下文**：
   - `qemu/backend/net-host-service/gw` 在转发 Guest TCP 连接时仅向宿主 SOCKS5 发送裸目的 IPv4（`ATYP=0x01`），未保留 Guest DNS 查询的 `IP -> 域名` 映射；
   - `agent/modules/daemon/backend/proxy` 仅解析了 `doc/90G.yaml` 的 `proxies:` 节点段，忽略了 `rules:` 分流段，将国内 `miui.com` 流量一律送往海外 Trojan 节点导致连接 15 秒超时。
3. **离线冷启动缺少预置兜底**：在纯内网/无外网或首次进入设置尚未完成云端拉取时，`02_prepare` 未从 `device-profile.json` 生成对应的 `device_params_pref.xml` 预置模板，首启脚本 `cph-setup-display` 也未向 DE 存储幂等种子化。

### 二、基带版本在 `01_unpack` → `02_prepare` → `13_pack_vda` / `09_build_vendor` / `daemon` 断链

1. **`.tgz` 归档与高通 SM8850 `MODEM-FW.bin` 未覆盖**：
   - `02_prepare/baseband.go` 的 `extractBasebandFromRomArchive` 此前硬编码仅处理 `.zip` 后缀（`!strings.HasSuffix(lower, ".zip")` 直接返回空），导致全部小米 `.tgz` 线刷包以及扩展名伪装为 `.zip` 的 gzip-tar 固件包跳过提取；
   - 高通骁龙 8 至尊版 Gen 5（`SM8850` / `Kaanapali`）将 MPSS 固件拆分至独立镜像 `MODEM-FW.bin`（`NON-HLOS.bin` 内无 `MPSS.` 串），且 `QC_IMAGE_VERSION_STRING=MPSS.DE.9.0-01805.10-KAANAPALI_GEN_PACK-2.121444.55.124572.67` 位于 `4 MiB` 偏移处，超出了旧 `2 MiB` 头部读取上限且文件名未纳入 `isModemImageName`。
2. **`02_prepare/place.go` 覆写 `.unpacked.json` 漏拷 `Baseband`**：`writePlacedMarker` 从 `tmpDir/.unpacked.json` 生成 `VendorDir/.unpacked.json` 时遗漏拷贝 `marker.Baseband`，清理 `tmpDir` 后嗅探结果即丢失。
3. **多版本基带与构建阶段断链**：
   - 同机型不同版本固件基带不同（如 `popsicle` 的 `16-OS3.0.319.0.WPBCNXM` 为 `...124572.67`，`17-OS4.0.5.0.XPBCN` 为 `...166769.13`），原 `apply-profile` 未按版本回填 `versions/<vendor>/<device>/<buildId>`；
   - `internal/device/db_bridge.go` 与 `device.go` 从三表构造 `device.Device` 时未回填 `BasebandVersion`；`13_pack_vda` 的 `resolveManifestBasebandVersion` 忽略了 `vendor/brand/version` 参数，未读取 `02_prepare` 产出的 `prepare.json` / `device-profile.json`；
   - `09_build_vendor/files_compat.go` 的 `isSystemDomainPropKey` 未将 `gsm.*` 纳入 `cdh_system_props.rc` 桥接（`vendor_init` 因 Treble SELinux 无法从 `/vendor/build.prop` 直接写 `radio_prop`）；且对于超过 45 字节的高通长基带串（双槽拼接 `> 91B`），`android.hardware.radio` 返回 `REQUEST_NOT_SUPPORTED` 前若属性尚未写入单槽完整串，设置页即回退为空。

## 证据

- 修复前失败现场（详见 [log-fail.txt](log-fail.txt)）：
  - `logcat` 连续输出 `W DeviceParamsManager: Failed to get basic device info`；
  - Guest 内直测 `curl https://port.sec.miui.com/myDevClound/devInfoNew` 卡在 TLS Handshake `Connection timed out after 5001 milliseconds`；
  - Guest 内 `getprop gsm.version.baseband` 返回 `[,]`，`out/devices/dist/xiaomi/popsicle/16-OS3.0.319.0.WPBCNXM/device.json` 无 `baseband-version`。
- 修复后实机验证日志与截图（详见 [log-pass.txt](log-pass.txt)、[screen_my_device.png](screen_my_device.png)、[screen_detail_top.png](screen_detail_top.png)、[screen_detail_baseband.png](screen_detail_baseband.png)）。

## 改动

1. **宿主 SOCKS5 Clash 分流直连与网关 DNS 域名透传（在线路径）**：
   - `agent/modules/daemon/backend/proxy/rules.go`、`socks5.go`、`proxy_test.go`：新增 `LoadClashRules`、`MatchRules`、`DefaultCNDirectRules` 与 `peekClientHost` / `extractTLSClientHelloSNI`，按声明顺序解析 `doc/90G.yaml` 的 `DOMAIN` / `DOMAIN-SUFFIX` / `DOMAIN-KEYWORD` / `IP-CIDR` / `GEOIP,CN` / `MATCH` 规则，国内域名与私网走宿主 `DIRECT` 直连，裸 IP 请求在 `80/443/8080/8443` 端口无损预读首包 TLS SNI / HTTP `Host` 二次裁决并完整重放首包。
   - `qemu/backend/net-host-service/gw/gateway.go`、`dns.go`、`tcp.go`、`gateway_test.go`：新增线程安全的 DNS A 记录逆向缓存（`recordDNSAnswers` / `lookupDNSDomain`，兼容 CNAME 链），在 `dialTCPViaProxy` 中自动将已解析的目标 IP 还原为 `domain:port`（SOCKS5 `ATYP=0x03`）透传给宿主代理。
2. **`02_prepare` 与首启脚本澎湃 OS 硬件规格离线预置（离线路径）**：
   - `agent/modules/pipeline/stages/02_prepare/apply_profile_configs.go`、`apply_profile_test.go`：新增 `buildMiuiDeviceParamsXML`、`formatXiaomiSOCMarketingName`、`formatXiaomiCameraShort`，对 Xiaomi / Redmi / POCO 机型自动从 `device-profile.json` 合成 `etc/cdh_miui_device_params.xml`。
   - `devices/hardware/android.service.daemon/vendor/bin/cph-setup-display`：首启检测 `/vendor/etc/cdh_miui_device_params.xml`，在 `device_params_pref.xml` 缺失或不含 `BasicInfoToggle` 时原子写入，并继承父目录 `/data/user_de/0/com.android.settings` 的精确属主（`1000:1000`）与 SELinux 上下文（`chcon u:object_r:system_app_data_file:s0`，避免 `restorecon` 误标为 `system_data_file`）。
3. **基带版本从 `01_unpack` / `02_prepare` 到云机制作与运行全链路打通**：
   - `agent/modules/pipeline/stages/01_unpack/official_extract.go`、`02_prepare/place.go`、`02_prepare/baseband.go`、`02_prepare/baseband_test.go`：按文件魔数识别 `.zip` / `.tgz` / `.tar.gz` / `.tar`（兼容伪装扩展名），纳入 `MODEM-FW.bin` / `modem_fw*.img` / `nr_modem` / `l_modem` 分块流式扫描前 `16 MiB`（命中高置信度串立即停止），并在 `writePlacedMarker` 中保留 `Baseband` 字段。
   - `agent/modules/pipeline/stages/02_prepare/apply_profile.go`：新增 `alignDeviceVersionsBaseband` 与 `syncPreparedAndDistBaseband`，按各固件版本精确提取基带并回填 `devices/*.json`、`configs/.../vendor/config.json`、`versions/<brand>/<device>/<buildId>`、`prepare.json` 与存量盘集 `dist/.../device.json`。
   - `agent/modules/pipeline/internal/device/db_bridge.go`、`device.go`、`stages/13_pack_vda/plan_describe.go`、`device_json_test.go`、`stages/09_build_vendor/files_compat.go`、`daemon/config/device.go`：全链路贯通版本级基带解析（`versions` → `prepare.json` → `configs` → `devices`），在 `09_build_vendor` 将 `gsm.*` 纳入 `cdh_system_props.rc` `pioneer` 属性桥接。
   - `devices/hardware/android.hardware.radio/src/radio_modem.cpp`、`sepolicy/vendor_sepolicy.cil`：当双槽拼接超 `91` 字节（单槽 `46..91` 字节）时，由 `hal_radio_default` 直接将完整单槽基带写入 `gsm.version.baseband` 后返回 `REQUEST_NOT_SUPPORTED`，防止 `com.android.phone` 截断或写空槽 `,`。

## 验证

1. **单元测试全绿**：
   - `go test ../modules/pipeline/internal/device ../modules/pipeline/stages/01_unpack ../modules/pipeline/stages/02_prepare ../modules/pipeline/stages/09_build_vendor ../modules/pipeline/stages/13_pack_vda ../modules/daemon/config ../modules/daemon/backend/proxy` 全部 PASS；
   - `qemu/backend/net-host-service`: `go test ./gw` 全部 PASS。
2. **多版本基带提取与清单反补验证**：
   - `pipeline device apply-profile xiaomi/popsicle` 自动从两个 `.tgz` 的 `MODEM-FW.bin` 提取并落盘：
     - `16-OS3.0.319.0.WPBCNXM` → `MPSS.DE.9.0-01805.10-KAANAPALI_GEN_PACK-2.121444.55.124572.67`
     - `17-OS4.0.5.0.XPBCN` → `MPSS.DE.9.0-01805.10-KAANAPALI_GEN_PACK-2.121444.286.166769.13`
3. **`Xiaomi 17 Pro Max #1` 冷启动双路径与 UI 实测**：
   - **离线首启预置**：`logcat` 记录 `cdh_provision: seeded MIUI/HyperOS device_params_pref.xml from /vendor/etc/cdh_miui_device_params.xml (ctx=u:object_r:system_app_data_file:s0)`，文件属主 `system:system`、权限 `0660`、SELinux 标签 `u:object_r:system_app_data_file:s0`。
   - **在线分流直连复拉**：Guest 内 `curl https://port.sec.miui.com/myDevClound/devInfoNew` 耗时 `0.389s` 返回 HTTP `200`（同时 `https://www.google.com/generate_204` 经 Trojan 耗时 `0.514s` 返回 `204`）；手动删除 `device_params_pref.xml` 后重进「我的设备」，`Settings` 实时在线重拉生成 `1313 B` 缓存文件（含云端 `2608×1200 超级像素`），全程零 `Failed to get basic device info`。
   - **基带与 UI 实屏展示**：`getprop gsm.version.baseband` 返回完整 61 字节 `MPSS.DE.9.0-01805.10-KAANAPALI_GEN_PACK-2.121444.55.124572.67`；实屏截图确认「我的设备」与「全部参数与信息」准确展示 `第五代骁龙®8至尊版移动平台 / 八核 最高 4.6GHz`、`7500mAh(typ)`、`前置50MP 后置50MP+50MP+50MP` 与完整基带版本。

## 遗留

- 无阻塞遗留。其余历史存量机型如需批量补刷离线 XML 或从远端 ROM 服务器补提基带，在 ROM 服务器在线时执行 `pipeline device apply-profile <机型>` 即可幂等落盘。
