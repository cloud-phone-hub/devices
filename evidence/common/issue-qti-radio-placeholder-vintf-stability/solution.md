# #266：高通 `QtiTelephony` `IQtiRadioStable` 占位服务 VINTF Stability / `InterfaceTokenHeader` 错配致连续 `RescueParty` 修复

- 编号：`#266`
- 分组：`common`（引擎级 `devices/hardware` 4 个 HAL 与 `02_prepare` 通用提取/对齐修复，首发实证 `redmagic/NX809J`）
- 机型：高通平台 QSSI16+（首发实证：`redmagic/NX809J` `16-BQ2A.250705.001-BP2A.250605.031.A3-20260527`）
- 状态：**已闭环**（2026-11-19）

## 1. 问题

在打通 `#15 redmagic/NX809J` 内核模块与串口通道（见 [../issue-system-dlkm-gki-ko-shadowing-virtio-console/solution.md](../issue-system-dlkm-gki-ko-shadowing-virtio-console/solution.md)）后，冷启动在第 1 轮与第 2 轮各自开机后约 `52s` / `32s` 连续两次触发 `reboot: Restarting system with command 'RescueParty'`，导致验收失败。

通过 `hvc7` 抓取 Boot 1 `/data/system/dropbox/system_app_crash@*.txt` 实证，进程 `com.qti.phone`（UID `10401`，`system_ext/priv-app/QtiTelephony/QtiTelephony.apk`）在启动期间 1 秒内连续崩溃 6 次并呈现两阶段不同异常：

1. **修复前（第 1 轮）**：`ExtTelephonyService.onCreate` -> `QtiRadioFactory.initQtiRadioAidl(QtiRadioFactory.java:87)` 抛出 `NullPointerException: Attempt to get length of null array`；
2. **仅补 `qtiradio-saidl.xml` 与 `AIBinder_markVintfStability` 后（第 2 轮）**：NPE 消失，但同一进程在 `QtiRadioAidl.initQtiRadio(QtiRadioAidl.java:157)` -> `IQtiRadio$Stub$Proxy.getInterfaceVersion(IQtiRadio.java:1126)` -> `BinderProxy.transactNative` 抛出未捕获的 `java.lang.IllegalArgumentException`，再次连续崩溃 6 次触发 `RescueParty`。

## 2. 根因

反汇编官方 `QtiTelephony.apk` (`classes.dex`) 并结合现场 `servicemanager` 与 `android.hardware.radio` 源码，锁定双层叠加根因：

1. **VINTF 声明缺失 + 占位 Binder 缺少 VINTF 稳定性标记**：
   - `QtiRadioFactory.isAidlAvailable()` 在 `ro.board.api_level` (`202504`) / `ro.product.first_api_level` (`36`) 下调用 `ServiceManager.isDeclared("vendor.qti.hardware.radio.qtiradio.IQtiRadioStable/slot1")`。因 `02_prepare` 未从官方 `unpack_vendor/etc/vintf/manifest/` 提取 `qtiradio-saidl.xml`，`isDeclared()` 返回 `false`，`makeQtiRadio()` 退入 HIDL 分支致使 `mQtiRadioAidl` 为 `null`；随后 `QtiRadioProxy.init()` 无条件调 `QtiRadioFactory.initQtiRadioAidl()` 解引用 `mQtiRadioAidl.length` 触发 NPE。
   - 同时，`android.hardware.{radio,gnss,neuralnetworks,soundtrigger3}` 中的 `RegisterPlaceholdersFromFile` 调用 `AIBinder_new` 后未调用 `AIBinder_markVintfStability(binder)`，一旦服务在 VINTF manifest 中声明为 AIDL 即无法通过 `servicemanager` 稳定性校验。
2. **服务实例名与 AIDL 接口描述符不一致 + `libbinder_ndk` 默认强校验 `interfaceToken`**：
   - 官方 `qtiradio-saidl.xml` 将服务名声明为 `vendor.qti.hardware.radio.qtiradio.IQtiRadioStable/slot1..2`，但其真实 Java AIDL 接口描述符 `IQtiRadio.DESCRIPTOR`（字节码实证 `<clinit>`）为 **`"vendor.qti.hardware.radio.qtiradio.IQtiRadio"`**（不含 `Stable` 后缀）。
   - `RegisterPlaceholdersFromFile` 直接截断 `/` 前字符串作为 `AIBinder_Class_define` 的 `interfaceDescriptor`，且未调用 `AIBinder_Class_disableInterfaceTokenHeader(clazz)`。客户端 `IQtiRadio$Stub$Proxy.getInterfaceVersion()` 写入 token `...IQtiRadio` 时，`libbinder_ndk` 的 `ABBinder::onTransact` 校验不等直接返回 `BAD_TYPE/BAD_VALUE`，被 `BinderProxy` 转为 unchecked `IllegalArgumentException`（绕过了 `QtiRadioAidl.initQtiRadio` 对 `RemoteException` 的 `try/catch`）。

## 3. 证据

- 失败现场日志：[log-fail.txt](log-fail.txt)（含第 1 轮 `dropbox` `QtiRadioFactory.initQtiRadioAidl:87` NPE 与 `RescueParty` 日志，以及第 2 轮 `dropbox` `IQtiRadio$Stub$Proxy.getInterfaceVersion:1126` `IllegalArgumentException` 与 `RescueParty` 日志）。
- 通过验证日志：[log-pass.txt](log-pass.txt)（`service check vendor.qti.hardware.radio.qtiradio.IQtiRadioStable/slot1..2` 均 `found`、`/data/system/dropbox` 输出 `NO_DROPBOX_CRASH`、`verify_boot.py` 首次冷启动 `passed=true` 且 `bootReason=reboot,factory_reset`）。
- 运行截图：[pass-screen.png](pass-screen.png)（`redmagic/NX809J` `1216x2688` 完整桌面）。

## 4. 改动

1. **4 个 HAL 占位类补齐 VINTF Stability 与禁用严格 Interface Token 头校验**：
   - [devices/hardware/android.hardware.radio/src/main.cpp](../../../../devices/hardware/android.hardware.radio/src/main.cpp)：新增 `extern "C"` 声明 `AIBinder_markVintfStability` 与 `AIBinder_Class_disableInterfaceTokenHeader`；在 `RegisterPlaceholdersFromFile` 中将 `vendor.qti.hardware.radio.qtiradio.IQtiRadioStable` 映射为真实接口描述符 `vendor.qti.hardware.radio.qtiradio.IQtiRadio`；在 `AIBinder_Class_define` 后调用 `AIBinder_Class_disableInterfaceTokenHeader(it->second)`，并在 `AIBinder_new` 后调用 `AIBinder_markVintfStability(binder)`。
   - [gnss_service.cpp](../../../../devices/hardware/android.hardware.gnss/src/gnss_service.cpp)、[neuralnetworks_service.cpp](../../../../devices/hardware/android.hardware.neuralnetworks/src/neuralnetworks_service.cpp)、[android.hardware.soundtrigger3/src/main.cpp](../../../../devices/hardware/android.hardware.soundtrigger3/src/main.cpp)：同步补齐 `AIBinder_Class_disableInterfaceTokenHeader` 与 `AIBinder_markVintfStability`。
2. **`02_prepare` 自动提取并幂等补齐官方 `qtiradio-saidl.xml`**：
   - [firmware_measured_extra.go](../../../../agent/modules/pipeline/stages/02_prepare/firmware_measured_extra.go)：在 `extractMeasuredVendorFilesAndRedirects` 中探测官方 `unpack_vendor/etc/vintf/manifest/qtiradio-saidl.xml` 存在时自动添加 `{"from": "official", "source": "etc/vintf/manifest/qtiradio-saidl.xml"}`。
   - [apply_profile_configs.go](../../../../agent/modules/pipeline/stages/02_prepare/apply_profile_configs.go)：在 `alignVendorExtraFields` 对存量 `files` 列表增加 `qtiradio-saidl.xml` 幂等补入分支。
   - [apply_profile_test.go](../../../../agent/modules/pipeline/stages/02_prepare/apply_profile_test.go)：新增单测 `TestMeasuredQtiradioSaidlAndIdempotentFilesAlign`。
3. **`redmagic/NX809J` 配置同步**（[vendor/config.json](../../../../devices/manifest/configs/redmagic/NX809J/NX809J-android16/vendor/config.json)）：
   - 在 `files` 数组追加 `{"from": "official", "source": "etc/vintf/manifest/qtiradio-saidl.xml"}`。

## 5. 验证

1. Go 单测与构建：
   - `TMPDIR=/Volumes/work/cloud-phone-hub/out/agent/tmp go test -count=1 ./modules/pipeline/stages/02_prepare/... ./modules/pipeline/stages/09_build_vendor/...` 全部通过（含 `TestMeasuredQtiradioSaidlAndIdempotentFilesAlign`）；
   - 重编 `out/agent/dist/cdh-pipeline`。
2. `redmagic/NX809J` 实机冷启动：
   - 重编 `build_hardware`（`android.hardware.gnss` 与 `android.hardware.radio` 重编链接通过），重打 `build_vendor -> build_patch -> pack_vda`，确认 `patch/stage/root/vendor/etc/vintf/manifest/qtiradio-saidl.xml` 落盘；
   - 冷启动 `verify_boot.py` 一次通过（`passed=true`、`bootCompleted=true`、`bootanim=stopped`、`serviceCount=358`、`bootReason=reboot,factory_reset`）；
   - 运行态实测 `service check vendor.qti.hardware.radio.qtiradio.IQtiRadioStable/slot1` 与 `slot2` 均为 `found`，`/data/system/dropbox/*crash*` 输出 `NO_DROPBOX_CRASH`，全程零 `RescueParty`。

## 6. 遗留

- 本例实证在 `NX809J` 完成 `radio` 与 `gnss` 实编加载；`neuralnetworks` 与 `soundtrigger3` 虽已同步补齐同类占位源码修改，将在后续启用对应 HAL 的机型中自动触发重编验证。
