# #348 itel A6612L（`itel/itel-A6612L`）启动崩溃根治、`02_prepare` 真机规格全量对齐与四大子系统（Wi-Fi / Radio / Bluetooth / NFC）验收闭环

- **设备 / 机型**：itel A300C / A6612L（`itel/itel-A6612L`，版本 `16-BP2A.250605.031.A3-20260823`，Unisoc SC9863A / `sp9863a`，GRF `system=A16` + `vendor=A13`，`lineage-20.0` + `kernel 5.15`）
- **日期**：2026-11-26
- **前置条目**：[`#304`](../../arw/issue-arw-boot-radio-deadlock-wifi-baseband/solution.md)、[`#337`](../../g1130/issue-g1130-camera-video-mediaserver/solution.md)、[`#339`](../../arw/issue-arw-prepare-profile-and-video-align/solution.md)
- **状态**：已闭环

---

## 一、问题

`itel A6612L #1`（`itel-A6612L_16-BP2A.250605.031.A3-20260823/1`）无法启动进入桌面，且存量清单停留在早期 `measured-only` 半成品状态，具体表现为 4 项阻塞与失真：

1. **`SurfaceFlinger` 循环 `SIGABRT` 触发框架无限重启**：开机动画卡死，`logcat` 每 5 秒报一次 `SurfaceFlinger` `Abort message: 'gralloc-mapper is missing'`，累计退出 4 次后触发原厂 `/product/bin/fwkreboot.sh` 重启整套 Android 框架，`zygote` / `audioserver` / `cameraserver` / `netd` / `wificond` 全部处于 `restarting` 状态，永远无法到达 `sys.boot_completed=1`。
2. **传音 Android 16 展锐专有守护进程未屏蔽**：后台 `init.svc.srmi_proxyd` 无限 `restarting`，`init.svc.ylog` 与 `init.svc.tool_service` 持续运行并狂刷 4,429+ 条 `tool_service` SELinux `avc: denied` 审计日志；同时 32 位 `/system/bin/mediaserver` 被旧规则重定向至不存在的 `/mnt/vendorpatch/system/bin/mediaserver`。
3. **存量设备规格严重失真**：`devices/manifest/devices/itel/itel-A6612L.json` 与 `device-profile.json` 仍填着模板假值——分辨率 `1080×2400 @ 420dpi`（真机实为 `720×1612 @ 320dpi`）、内存 `[4096, 8192]`（真机实为 `3GB/4GB` 入门档位）、缺真机基带版本与完整传感器/温区/编解码 XML，且版本级 `vendor/config.json` 残留过期的 `"ro.hardware.vulkan": "virtio"` 覆盖项。
4. **四大无线子系统待验收**：修复启动并完成 `prepare` 真机对齐后，需逐项实测 **Wi-Fi、Radio（蜂窝）、NFC、Bluetooth** 的运行状态与数据连通性。

---

## 二、根因

1. **旧盘集 `mapper.minigbm.so` 引用 Android 13 `sphal` `libc++.so` 不存在的符号**：
   - 旧 `patch.img`（2026-11-23 构建）内的 `/vendor/lib64/hw/mapper.minigbm.so`（376,728 B）未包含内聚的 `_ZNSt3__122__libcpp_verbose_abortEPKcz` 定义；
   - `itel-A6612L` 为 GRF 跨代机型（`system=Android 16` + `vendor=Android 13`，`VNDK=33`），`SurfaceFlinger` 通过 `vndksupport` 从 Android 13 `sphal` 命名空间加载 `/vendor/lib64/hw/android.hardware.graphics.mapper@4.0-impl.so` → `mapper.minigbm.so` 时报 `dlopen failed: cannot locate symbol "_ZNSt3__122__libcpp_verbose_abortEPKcz"`，导致 `GraphicBufferMapper` 断言 `gralloc-mapper is missing` 自杀。
2. **`02_prepare` 展锐守护进程扫描未覆盖 `Spreadtrum` 厂商名与 `/product/bin` 分区**：
   - `itel-A6612L` 的 `ro.soc.manufacturer` 为 `Spreadtrum`（`ro.board.platform=sp9863a`），且传音在 Android 16 系统镜像中将 `srmi_proxyd`、`ylog`、`tool_service` 从传统的 `/system_ext/bin/` 迁移到了 `/product/bin/`（仅 `slogmodem` 留在 `/system_ext/bin/`）；
   - `02_prepare/firmware_measured_extra.go` 原逻辑仅扫描 `system_ext(_a).img`，漏掉了 `/product/bin/{srmi_proxyd,ylog,tool_service}` 的空桩重定向生成。
3. **存量档案早于 `02_prepare` 固件实测与 DSH AI 对齐增强**：
   - 解包查验原厂 `/product/media/bootanimation.zip` 内 `desc.txt` 首行为 `720 1612 25`，结合 `/product/etc/build.prop`（`ro.sf.lcd_density=320`）与原厂 DTB `sprd-cpufreq`（8×Cortex-A55 `768 MHz .. 1.6 GHz` 12 档频点）及 `.pac` 基带 `4G_MODEM_22B_W26.24.1_P2`，证实早期清单中的 `1080×2400`、`8192 MB` 均为占位模板值；
   - 同时核查原厂全分区（`unpack_system` / `unpack_product` / `unpack_vendor` / `unpack_odm`），无任何 `*nfc*.xml` 权限声明、无 `/odm/etc/vconfig/NFC`、无 NFC 固件，确认 `itel A6612L`（商业名 `itel A300C`）物理真机**无 NFC 硬件**（`hasNfc: false`）。

---

## 三、证据

- **修前失败日志**：见 [log-fail.txt](log-fail.txt)
  - `vndksupport: Could not load /vendor/lib64/hw/android.hardware.graphics.mapper@4.0-impl.so from sphal namespace: dlopen failed: cannot locate symbol "_ZNSt3__122__libcpp_verbose_abortEPKcz" referenced by "/vendor/lib64/hw/mapper.minigbm.so"`；
  - `SurfaceFlinger` 每 5 秒触发 `Abort message: 'gralloc-mapper is missing'`，伴随 `srmi_proxyd`、`audioserver`、`cameraserver`、`netd`、`wificond`、`zygote` 集体 `restarting`。
- **修后通过日志与桌面实屏截图**：见 [log-pass.txt](log-pass.txt)、[pass-home.png](pass-home.png)。

---

## 四、改动

1. **增强 `02_prepare` 传音/展锐守护进程自动识别与单测覆盖**：
   - 修改 `agent/modules/pipeline/stages/02_prepare/firmware_measured_extra.go`：新增 `extractUnisocDaemonRedirects` / `extractUnisocDaemonRedirectsWithProbe`，兼容 `Spreadtrum` / `Unisoc` / `sp9863a` 平台识别，同时探测 `system_ext(_a).img` 与 `product(_a).img` 下的 `slogmodem`、`srmi_proxyd`、`ylog`、`tool_service` 并生成 `/mnt/vendorpatch/vendor/bin/cdh_noop` 重定向规则；
   - 同目录 `agent/modules/pipeline/stages/02_prepare/unisoc_profile_align_test.go` 新增 `TestExtractUnisocDaemonRedirectsScansSystemExtAndProduct` 单测验证通过，重编 `out/agent/dist/pipeline`。
2. **执行 `01_unpack` → `02_prepare --refresh-profile` → `device apply-profile -overwrite` 全量对齐真机档案（严守 `AGENTS.md §8` 官方镜像零修改与 `§9` 本机真相源同步）**：
   - 重新解包 `roms/devices/itel/itel-A6612L.zip` 并结合固件实测 + DSH 会话采集生成 `devices/manifest/devices/itel/itel-A6612L/device-profile.{json,md}` 与 `itel-A6612L.json`：
     - 屏幕对齐为真值 `720×1612`、物理尺寸 `68×153 mm`、`320 dpi`、`60 Hz`；
     - 档位对齐为 `memory: [3072, 4096]`、`storage: [64000, 128000]`、`skuCombinations: ["3GB+64GB", "4GB+64GB", "4GB+128GB"]`；
     - 基带对齐为固件实测值 `4G_MODEM_22B_W26.24.1_P2`；`hasNfc: false`；
   - 清理 `devices/manifest/versions/itel/itel-A6612L/16-BP2A.250605.031.A3-20260823/vendor/config.json` 中过期的 `"ro.hardware.vulkan": "virtio"`；
   - 运行 `pipeline device apply-profile -overwrite itel/itel-A6612L` 刷新 `itel-A6612L-android13/{module,vendor,vendor_boot,vda}/config.json` 及 `vendor/etc/*.xml`：
     - `module/config.json`：`cpufreq` 写入 `sprd-cpufreq` / `sprd-dm` 8×Cortex-A55（`1600000 .. 768000` 12 档频点）；`redirect`（17/32 条）将 `/system/bin/mediaserver` 指向 64 位 `/mnt/vendorpatch/vendor/bin/hw/cdh-mediaserver`，并将 `/system_ext/bin/slogmodem` 与 `/product/bin/{srmi_proxyd,ylog,tool_service}` 全部重定向至 `cdh_noop`；
   - 通过 `cdha tool cluster.sync_sources` 将本机 `devices/manifest/` 单向同步至 `rom-main`（`192.168.31.164`）。
3. **全链重编与发布盘集**：
   - 执行 `pipeline device build itel-A6612L --from 03_build_kernel --refresh`，复用同代 `5.15` 内核与 13 个自建 `.ko`（`vendor_dlkm` 基座 `.ko` 清零且依赖闭包成立），全量重编 `android13` 22 个自研 HAL（含内聚定义 `__libcpp_verbose_abort` 的新版 `mapper.minigbm.so`、`radio@unisoc`、`cdh-mediaserver`），原样直通官方 `super.img`，生成新盘集（`patch.img` sha256 `9105e5314463`，`vda.img` 6.08 GiB，`data.qcow2` 53.56 GiB）。

---

## 五、验证

重建并冷启动 `itel A6612L #1`（`itel-A6612L_16-BP2A.250605.031.A3-20260823/1`，SKU `4GB+128GB`，ADB `127.0.0.1:5919`），实测结果全部通过（详见 [log-pass.txt](log-pass.txt) 与 [pass-home.png](pass-home.png)）：

1. **启动健康度与设备信息对齐**：
   - **31.4s** 到达 `sys.boot_completed=1`、`init.svc.bootanim=stopped`、`getenforce=Enforcing`，`restarting` 服务数为 **0**，`/data/anr` 与 `/data/tombstones` 均为空，`Service SurfaceFlinger: found`、`Service media.player: found`；
   - 设备身份与规格：`model=itel A6612L`、`device=itel-A6612L`、`fingerprint=itel/A6612L-OP64/itel-A6612L:16/BP2A.250605.031.A3/A6612L_260822V4:user/release-keys`、`Physical size: 720x1612`、`Physical density: 320`、`MemTotal: 3809724 kB`（4GB）、`/data` `113G`（128GB）、`sprd-cpufreq` 12 档频点 `1600000..768000`；
   - 桌面截图 [pass-home.png](pass-home.png) 正常渲染 Launcher 桌面，状态栏右上角 **蓝牙 + Wi-Fi 满格 + 4G 满格** 图标同时在线。
2. **四大子系统逐项实测**：
   - **Wi-Fi（正常）**：`Wifi is enabled`，`cmd wifi list-scan-results` 扫出全部 16 个 2.4GHz/5GHz 虚拟 AP，自动关联 5GHz `"JiHotel-9F-WiFi-E9"`（`5180 MHz`, RSSI `-34 dBm`, `Supplicant state: COMPLETED`, `score: 100`, `isUsable: true`），`wlan0` 获取 `192.168.20.100/24`；`ping -c 3 -I wlan0 223.5.5.5` **0% 丢包**（`avg 0.545 ms`），开关 Wi-Fi 后自动秒级回连。
   - **Radio / 蜂窝（正常）**：基带 `gsm.version.baseband=4G_MODEM_22B_W26.24.1_P2,4G_MODEM_22B_W26.24.1_P2`（`sprd-ril`），`gsm.sim.state=LOADED,ABSENT`，注册 `中国联通`（`46001`）`LTE` `IN_SERVICE`（`mDataConnectionState=2`）；双 PDN 网卡 `ccmni0`（IMS IPv6 `2408:8456:30:b52b:...`）与 `ccmni1`（`100.64.155.15/24`）在线；临时关闭 Wi-Fi 实测纯蜂窝外网 `ping -c 3 223.5.5.5` **0% 丢包**（`avg 0.300 ms`）。
   - **Bluetooth / 蓝牙（正常）**：`android.hardware.bluetooth@1.1-service.btlinux` 与 `com.android.bluetooth` 经 `/dev/hvc5` 在线运行，`dumpsys bluetooth_manager` 显示 `enabled: true`、`state: ON`、`name: itel A6612L`、`ScanMode: SCAN_MODE_CONNECTABLE`、`Bluetooth crashed 0 times`；宿主 3 台双模虚拟蓝牙对端（`CDH-Headset-Pro` / `CDH-Speaker-Mini` / `CDH-Watch-S3`）就绪。
   - **NFC（真机无硬件，符合预期）**：官方全分区无 NFC 权限 XML 与设备节点，`pm list features | grep -i nfc` 为空、`Service nfc: not found`、宿主不挂载 NFC 后端，零报错重试，与 `itel A6612L` 真机物理无 NFC 硬件完全一致。

---

## 六、遗留

- 同 [`#304`](../../arw/issue-arw-boot-radio-deadlock-wifi-baseband/solution.md) 全库既定口径：官方 `super.img` 预装的 `com.android.chrome` 仅含 32 位 `libmonochrome.so`，在纯 64 位 ARM64 云机后台自启探针时报一次 `UnsatisfiedLinkError`，不影响系统桌面与其他预装应用运行，遵守 `AGENTS.md §8` 不改动官方 `super.img`。
