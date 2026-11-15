# Issue: oriole A14 云机与真机 device-checker 对拍对齐（传感器/输入/电池/GPU/显示）

- 设备: Google Pixel 6（oriole，AP2A.240905.003.F1 + lineage-21.0，内核 `5.10.270-4k-cdh`）
- 宿主: macOS (darwin, QEMU + hvf)，实例 `oriole_14-AP2A.240905.003.F1-20240815/2`（ADB `127.0.0.1:5911`）
- 真机: Pixel 6（USB `4lmbvom7gifm5lbu` / 网络 `192.168.31.207:5555`，androidId `b8f75582353b320a`）
- 云机: androidId `02f9d9f965732349-cloud`（cloud flavor APK）
- 状态: **已闭环（本轮可修项）**（2026-11-12）：同批 63 项功能点对拍，传感器列表 **842→0 完全对齐**，
  输入/电池/GPU/显示 targeted 项全部收敛；总差异键 26063→25291（净 -772，targeted 合计约 -887，
  其余被 files.storage/fsid 等瞬时噪声抵消）。剩余差异均为硬件/基线根因的永久遗留（见六）。
- 背景: 「oriole Android 14 云机和真机对齐」任务。#197（起机稳定）、#198（假 Wi-Fi 5G 速率）、
  #199（checker WS 心跳）把云机送到「能稳定注册到 device-checker 服务端」之后，本条做第一次
  全量真机/云机对拍并把「清单可修 + 宿主可注入」的差异修掉。

---

## 一、问题

云机能稳定注册后，用 device-checker 把**同一批 63 个功能点**同时下发真机与云机（run-00001，
126/126 succeeded），把每个功能点 `result` 递归扁平化为叶子路径逐键比对，得到 **26063 / 41921**
个差异键。差异集中在五处机制：

- `hardware.sensor`：842/843 键几乎全差——云机传感器列表与真机完全对不上；
- `hardware.inputs`：31/42——云机触摸屏叫 `sec_touchscreen`（真机 `fts`），且多出虚拟输入设备；
- `hardware.battery`：13/20——电池静态字段（技术/容量/健康阈值）与真机不一致；
- `hardware.gles`：3/6——GL renderer/vendor/version 是 virgl 默认值，非 Mali-G78；
- `screen.display`：12/88——云机只有单一 60fps 模式，真机支持 60/90 双模。

## 二、根因

五处差异各有机制，但都落在「清单可声明 + 宿主/模块可注入」范围内：

1. **传感器**：云机 sensors HAL 用的是 cuttlefish 默认传感器集，没有按 oriole 真机列表配置；
   需要把真机实测的传感器清单写成 `sensors_config.xml` 随 vendor 落位，HAL 按它枚举。
2. **输入**：QEMU `virtio-multitouch` 默认名 `sec_touchscreen`，且 `cdh_input_identity.ko` 默认走
   `gold` profile（小米口径），与 oriole 的 `fts` 触摸屏不符；虚拟键盘/指纹等设备未排除。
3. **电池**：电池静态身份字段无机型配置，走默认值；需 `battery_config.xml`。
4. **GPU**：virgl 上报默认 GL 身份；真机是 Mali-G78。身份串可由 `ro.vendor.cdh.gpu.*` 属性声明，
   07 抽离进 `gpu_id.cfg` 供 EGL 加载器读取（**不能**安全暴露完整 Mali 扩展集，见六）。
5. **显示**：宿主 QEMU `virtio-gpu` 的 `refresh-rates` 写死 60，没有按机型透传 60/90 的通道。

**本轮还挖出一个构建期 bug**：清单 `modules.options`（按机型的模块加载参数，如
`input_identity → profile=oriole`）在 `internal/manifest/manifest.go` 的 `parse()` 里**被丢弃**——
该函数逐字段解析 `modules` 段（external/in-tree/cpufreq/thermal），唯独没解析 `options`，
`doc.Modules.Options` 永远为 nil。即便 `db_bridge.go` 已把 options 透传进物化基线 JSON，
08 的 `manifestModuleOptions` 仍读到空集，`profile=oriole`  never 下发。这是「输入」修复链路
真正生效的前置闸门。

## 三、证据

- `log-fail.txt`：run-00001 五处 targeted 差异原文（传感器/输入/电池/GPU/显示），以及构建期
  bug 现场——08 日志 `模块加载参数: options cdh_media allow_userptr=1`（缺 input_identity）、
  产物 `modules.options` 仅 1 行、而物化基线 `modules.options={"input_identity":"profile=oriole"}`。
- `log-pass.txt`：run-00002 收敛表（sensor 842→0 等）、修复后 targeted 剩余差异原文、
  构建侧 `modules.options: 2 行` + 镜像内 debugfs 回读、运行态 QEMU cmdline 与 guest 探针。
- 对拍口径脚本 [`compare.py`](../../../out/tmp/oriole-align/compare.py)：result 递归扁平化
  （dict `.key`、list `[i]`）按位置逐键比对；在 run-00001 上**精确复现 26063/41921**，证明口径一致。
- 原始数据：`out/tmp/oriole-align/run-00001-merged.json`、`run-00002-merged.json`、
  `diff-recompute-00001.txt`、`diff-00002-merged.txt`。

## 四、改动

**A. 引擎/宿主代码（跨机型机制）**

1. [`internal/manifest/manifest.go`](../../../build/internal/manifest/manifest.go)：`parse()` 补
   `modules.options` 解析（新增 `stringMapField` helper，键按 `CheckName` 校验、值须为字符串），
   `doc.Modules.Options` 不再恒 nil。**这是本轮关键修复**。
2. [`internal/device/db_bridge.go`](../../../build/internal/device/db_bridge.go)：
   `SynthesizeBaselineJSON` 把 `buildModules.options` 透传进基线 `modules.options`；
   `SynthesizeDeviceJSON` 已透传 `buildModules.display`。
3. **显示透传链（8 文件，仿 input 口径）**：`config buildModules.display` →
   [`modules.go`](../../../build/internal/device/modules.go) `ModuleSection.Display` →
   [`device.go`](../../../build/internal/device/device.go) `Device.Display`（modules.display 优先于顶层）→
   [`11_pack_vda/device_json.go`](../../../build/stages/11_pack_vda/device_json.go) dist 顶层 `display` →
   [`core/host/daemon/config/device.go`](../../../../core/host/daemon/config/device.go)
   `DeviceSpec.RefreshRates/EdidNativeOnly` →
   [`core/host/daemon/run/qemu.go`](../../../../core/host/daemon/run/qemu.go) `gpuArg`
   （`refresh-rates` 冒号拼接、`edid-native-only=on`）。
4. [`devices/modules/input_identity/cdh_input_identity.c`](../../../modules/input_identity/cdh_input_identity.c)：
   新增 `module_param(profile, charp, 0444)`（默认 `gold`），`profile=oriole` 分支按 oriole 真机
   输入身份上报。
5. [`android.service.daemon/src/rust/src/miui.rs`](../../../vendor/hardware/android.service.daemon/src/rust/src/miui.rs)：
   `fix()` 加品牌闸门——读 `ro.product.brand`（回退 vendor.brand），非 `xiaomi/redmi/poco` 直接
   跳过 `ro.miui.product.home` 注入（避免给 Google 机型写小米属性）。
6. sensors HAL 重编（flags 扩展，按 `sensors_config.xml` 枚举）。

**B. 清单（oriole 专属，[`configs/google/oriole/oriole-android14.json`](../../../manifest/configs/google/oriole/oriole-android14.json)）**

- `buildVendor.fileTrees`：新增 `etc/sensors_config.xml`（真机 35 传感器）、`etc/battery_config.xml`、
  `etc/excluded-input-devices.xml`（排除虚拟输入设备）；
- `buildVendor.props`：6 项 GPU 身份（`gl_renderer=Mali-G78` / `gl_vendor=ARM` /
  `gl_version=OpenGL ES 3.2 v1.r47p0-…` / `gl_sl_version` / `gl_extra_exts` / `gl_extra_exts2`，
  只补安全的 ARM 扩展）；
- `buildModules.input.touch.name`：`sec_touchscreen → fts`（bus=28，abs-to-mt=true）；
- `buildModules.display`：`{"refresh-rates":[60,90],"edid-native-only":true}`；
- `buildModules.options`：`{"input_identity":"profile=oriole"}`。

**C. 测试（与被测代码同目录，AGENTS.md §4）**

- [`internal/manifest/manifest_test.go`](../../../build/internal/manifest/manifest_test.go)
  `TestModuleOptionsParsing`（options 读入 / 未声明返回 nil / 非字符串报错）；
- [`internal/device/db_test.go`](../../../build/internal/device/db_test.go)
  `TestSynthesizeBaselineModuleOptions`（options 透传基线）；
- [`internal/device/display_test.go`](../../../build/internal/device/display_test.go)
  `TestModulesDisplayPassthrough`（5 子测试：透传/顶层兼容/优先级/缺省/非对象报错）。

**D. 重建与重启**：`go build` cdh + `cdh-vendor hal build android.service.daemon/android.hardware.sensors`
+ `bash build/core/host-main build`；`cdh device build oriole --manifest 14-AP2A.240905.003.F1-20240815
--from 03_build_modules`（修 options 后再 `--from 08_build_vendor_dlkm`）；停实例 2 → 重启 host daemon
（加载新二进制）→ 启动实例 2 → `adb connect 127.0.0.1:5911`。

## 五、验证

- **单测**：`go test ./internal/manifest/ ./internal/device/ ./stages/08_build_vendor_dlkm/` 全绿；
  host 侧 `config`/`run` 包测试通过。
- **构建**：03→11 全 EXIT=0；08 `modules.options: 2 行`，debugfs 回读镜像含
  `options cdh_input_identity profile=oriole`；07 三个 XML + `gpu_id.cfg`（6 项）落位；
  11 dist `device.json` 含 `display={refresh-rates:[60,90],edid-native-only:true}`、`touch.name=fts`。
- **运行态**：`boot_completed=1` @30s；QEMU cmdline `refresh-rates=60:90`、`edid-native-only=on`、
  `input-name=fts`；guest `profile=oriole`、`/proc/bus/input/devices` 名 `fts`、
  `ro.miui.product.home` 空（闸门跳过）、`gpu_id.cfg` 6 项 Mali 身份、`sensors-service` 在跑
  （35 h/w sensors）、`dumpsys display` 支持 60/90 双模。
- **复测收敛**（同批 63 项，run-00002-merged，126/126 succeeded）：

  | 功能点 | 修复前 | 修复后 | 增减 |
  | :--- | :--- | :--- | :--- |
  | hardware.sensor | 842/843 | **0/843** | **-842（完全对齐）** |
  | hardware.inputs | 31/42 | 10/22 | -21 |
  | hardware.battery | 13/20 | 5/20 | -8 |
  | hardware.gles | 3/6 | 1/6 | -2 |
  | screen.display | 12/88 | 10/88 | -2 |
  | sim.telephony | 78/1147 | 71/1140 | -7 |
  | properties.getprop | 459/901 | 457/900 | -2 |
  | properties.native | 537/1275 | 535/1274 | -2 |
  | settings.provider | 139/391 | 138/391 | -1 |
  | **总计** | **26063** | **25291** | **-772** |

  targeted 合计约 -887，被瞬时噪声抵消：`files.storage` 154→271(+117，空闲空间随时间变)、
  `id.partition_fsid` 2→6(+4，fsid 每次格式化重生)、`dev.test_mount` +5。这些非本次修复目标。
  真机首轮 `hardware.sensor`/`sim.telephony` 执行超时（瞬时），重跑后成功，已并入 merged。

## 六、遗留

**永久遗留（硬件/基线根因，清单不可修，记录备查）**：

- `hardware.gles` 剩 1：`gles.extensions` 完整 Mali 扩展串。virgl 无法安全暴露 Mali 的
  AEP/geometry/tessellation 等扩展（暴露会让 CTS/应用走不支持的路径崩溃），只补了安全的 ARM 扩展；
  renderer/vendor/version 已对齐。
- `screen.display` 剩 10：`xdpi/ydpi`（真机 409.4/411.9 vs 云机 457.2/435.4，EDID 厘米量化误差）、
  `Hdr`/`WideColorGamut`/`PreferredWideGamutColorSpace`（drm_hwcomposer 硬编码不支持）、
  `State` 1 vs 2（瞬时屏幕状态）、`AppVsyncOffsetNanos` 1ns 舍入、`SupportedRefreshRates[0]`
  60 vs 60.0000038（vsync 浮点精度）。刷新率集合本身已对齐（60/90 双模）。
- `hardware.inputs` 剩 10：设备**枚举顺序**差异（真机 fts 在 index 3、云机在 index 1，启动时序决定）
  与 fts descriptor 哈希（`fd5f1429…`）。设备集合与名字已对齐（fts 在列、sec_touchscreen 消失）。
- `hardware.battery` 剩 5：`charge_counter`/`metadata` 内嵌库仑计/seq、充电状态交替——电池模拟动态量，
  静态身份字段已对齐。
- 大额结构性差异（非本轮目标）：`camera.camera_new` 11248（虚拟 camera HAL）、`files.app_paths` 4255、
  `files.vendor_file` 2985、`app.list` 2027（GMS/预置应用差异）、`dev.test_mount`/`dev.test_vendor`、
  `android.services` 370 等，属基线/产品选型层面。

**工程遗留**：

- `modules.options` 解析修复是通用闸门，但**无运行期回归基线**——「云机 input profile 必须等于清单声明」
  宜做成 daemon 自检项；本轮靠 debugfs 回读 + guest `/sys/module/.../parameters/profile` 双重确认。
- 显示透传链 `edid-native-only` 只影响暴露的模式集，EDID 厂商串仍是 `QEMU Monitor`/`RHT`
  （`deviceProductInfo`），真机为面板厂商；如需对齐须定制 EDID blob，属下一步。
- 对拍口径按数组**位置**逐键比对，会把「集合相同但顺序不同」（feature/input/app）计为差异；
  收敛统计含这部分顺序噪声，解读 targeted 项时需结合设备集合本身判断。