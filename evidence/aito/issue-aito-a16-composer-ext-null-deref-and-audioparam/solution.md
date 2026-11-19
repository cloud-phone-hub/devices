# #271：`motorola/aito` Android 16 GRF `ComposerExt` 空指针解引用阻塞与 `audio_param` 配置修复（仅检查定位）

- **设备/机型**：`motorola/aito`（Motorola Razr 2024，`mt6878`，版本 `14-W1UCS36H.96-35-8-9-20260801`，GRF `vendor=14 / system=16`，`lineage-23.2` + `kernels/6.1`）
- **状态**：`已定位根因（待源码桩支持）`

## 问题

1. 首轮排查：`motorola/aito` 清单误标 `"platform": "qcom"` 致 MTK 平台覆盖层未生效，且 GRF `34.0` 的步骤 9.6 剔除了 `lineage-23.2` 的 `mapper/minigbm*` `vendor_service_contexts` 标签，冷启动约 18 s 崩溃熔断。
2. 修正 `platform="mtk"` 并补全 `mapper/minigbm*` 后，第二轮冷启动仍无法进入图形界面与桌面，120 s 超时熔断期间出现两条**独立原生崩溃循环**：
   - `audioserver`（`pid 748`）：`libaudioutilmtk.so` 中 `android::FeatureOption::getValues()+268` 调用 `std::__1::stoi` 抛出 `std::invalid_argument` 中止；
   - `surfaceflinger`（`pid 611/613`）：约每 5~6 s 在 `android::SurfaceFlinger::initializeDisplays()+2460`（`pc 0x2c07fc`）发生 `SIGSEGV (SEGV_MAPERR) fault addr 0x0`，伴随 `E/libcomposer_ext: Can't get Composer Ext, tried(22) times`、`E/SurfaceFlinger: ComposerExt HIDL not present`、`DisplayManagerService: Default display is null` 与 `SELinux: avc: denied { find } ... name=vendor.mediatek.hardware.composer_ext.IComposerExt/default`。

## 根因

1. **`audioserver` 崩溃（配置缺失，纯配置可修）**：
   - `/system_ext/lib64/libaudioutilmtk.so`（`0x153d0..0x15504`）通过 `dlopen` 加载 `libaudio_param_parser-sys@aidl.so` / `libaudio_param_parser-sys@8.1.so` / `libaudio_param_parser-sys.so`（`0x15f00`），调用 `appGetFeatureOptionValue("MTK_AUDIO_NUMBER_OF_SPEAKER")`，若返回非空指针则直接拷贝入 `std::string` 后在 `0x154dc` 调用 `std::__1::stoi(..., 10)`。
   - 核查公式产物 `out/devices/devices/motorola/aito/.../vendor/etc/audio_param` 完全缺失；而官方 `unpack_vendor/etc/audio_param/` 包含 `AudioParamOptions_mgvi.xml`（第 43 行 `<Param name="MTK_AUDIO_NUMBER_OF_SPEAKER" value="" />`）与 `AudioParamOptions_vext.xml`（第 43 行 `<Param name="MTK_AUDIO_NUMBER_OF_SPEAKER" value="2" />`）。缺失 `/vendor/etc/audio_param` 导致解析得到空值，触发 `stoi` 异常。
2. **`surfaceflinger` 崩溃（Motorola A16 系统二进制缺少空指针保护 + AIDL 占位事务不支持，属源码级阻断）**：
   - 对官方 `/system/bin/surfaceflinger` 反汇编确证：`[x19, #0x2168]` 为 `ComposerExt::ClientInterface*`，在 `0x3114d0` 函数（`0x311570`）调用 `ComposerExt::ClientInterface::create`；当 `create` 失败时（`0x311718` 打 `ComposerExt HIDL not present\n`），`0x311730` 将 `[x19, #0x2168]` 置 `NULL`。
   - 然而 `android::SurfaceFlinger::initializeDisplays()`（`0x2bfe60..0x2c09c0`）全路径必经的 `0x2c07f4..0x2c0804` 处**未做 `cbz x0` 空指针判断**，直接读取 `[x19, #0x2168]` 后解引用虚表 `[x0]` -> `[x8, #0x48]`（`0x2c07fc: ldr x8, [x0]`），导致确定性空指针段错误。其他已通过的 MTK 机型（如 `gold`、`CPH2797`、`XPLORE_X1`）中系统侧对空指针具备容错，且刻意不在 `cdh_nn_placeholders` 伪造 `IComposerExt/default`（见 [`#227`](../../gold/issue-gold-vendor-binder-placeholders-and-init-svc/solution.md)）。
   - 对 `/system_ext/lib64/libcomposer_ext.so` 反汇编确证：即使运行态 `cdh_nn_placeholders.txt` 列有 `vendor.mediatek.hardware.composer_ext.IComposerExt/default`，除了 GRF `34.0` 官方 `vendor_sepolicy.cil:994` 使用 `surfaceflinger_34_0` 导致 `avc: denied { find }` 外，即便开放 SELinux `find`/`binder call`，`ClientImplAidl::Init`（`0x246a0`）在获取 Binder 后仍会通过 `[x8, #0x40]` 发起 `registerClient` AIDL 事务（`0x24918`）并严格校验 `AStatus_isOk()`（`0x24b08`）；而现有 [`devices/hardware/android.hardware.neuralnetworks/src/neuralnetworks_service.cpp:143-148`](../../../hardware/android.hardware.neuralnetworks/src/neuralnetworks_service.cpp#L143-L148) 的 `OnPlaceholderTransact` 固定返回 `STATUS_UNKNOWN_TRANSACTION`，无法让 `ClientImplAidl::Init` 返回成功。遵循“只查调配置、不改任何仓库代码”约束，无法在纯 `devices/manifest/` 层解决。

## 证据

见同目录 [`log-fail.txt`](log-fail.txt)：
- **第 1 轮失败**：缺 `mtk` 平台覆盖与 `mapper/minigbm*` 标签（~18 s 熔断）；
- **第 2 轮失败**（`verify_boot.py --timeout 120` 熔断 + `hvc7` 取证）：
  - `audioserver` 崩溃栈 `#08 std::__1::stoi` <- `#09 /system_ext/lib64/libaudioutilmtk.so (android::FeatureOption::getValues()+268)`；
  - `surfaceflinger` 崩溃栈 `#00 pc 0x2c07fc /system/bin/surfaceflinger (android::SurfaceFlinger::initializeDisplays()+2460)`，`x0=0`、`fault addr 0x0`；
  - `logcat`: `E/libcomposer_ext: Can't get Composer Ext, tried(22) times`、`E/SurfaceFlinger: ComposerExt HIDL not present`；
  - `dmesg`: `SELinux: avc: denied { find } for pid=912 uid=1000 name=vendor.mediatek.hardware.composer_ext.IComposerExt/default scontext=u:r:surfaceflinger:s0 tcontext=u:object_r:mtk_hal_composer_ext_service:s0 tclass=service_manager permissive=0`。

## 改动

仅调整配置（不改动任何仓库源码）：
1. [`devices/manifest/devices/motorola/aito.json`](../../../manifest/devices/motorola/aito.json)：`"platform"` 由 `"qcom"` 更正为 `"mtk"`。
2. 新建 [`devices/manifest/configs/motorola/aito/aito-android16/vendor/sepolicy/vendor_service_contexts`](../../../manifest/configs/motorola/aito/aito-android16/vendor/sepolicy/vendor_service_contexts)：补入 `mapper/minigbm_upstream` 与 `mapper/minigbm` -> `u:object_r:hal_graphics_mapper_service:s0`。
3. [`devices/manifest/configs/motorola/aito/aito-android16/vendor/config.json`](../../../manifest/configs/motorola/aito/aito-android16/vendor/config.json)：`"files"` 追加 `{"from": "official", "mode": "replace", "source": "etc/audio_param"}`。
4. [`devices/manifest/configs/motorola/aito/aito-android16/vendor/etc/cdh_nn_placeholders.txt`](../../../manifest/configs/motorola/aito/aito-android16/vendor/etc/cdh_nn_placeholders.txt)：移除刻意排除的 `vendor.mediatek.hardware.composer_ext.IComposerExt/default`、`IATMs/default`、`IPictureQuality_AIDL/default`（对齐 `#227`）。

## 验证

- 第 1 轮改动后 `--from build_vendor` 成功，`debugfs` 读回 `vendor.img/etc/selinux/vendor_service_contexts` 已含 `mapper/mediatek -> hal_graphics_mapper_service`；平台覆盖层 logs 确认生效（4 个文件）。
- 第 2 轮冷启动后已核实 `cdha` 无运行实例（`running: []`）；第 3 项/第 4 项配置收尾后通过 `json.load` 校验配置格式合法，中间产物已按磁盘回收规范清理。

## 遗留

- 解除 `#18 motorola/aito` 阻塞需二选一代码级支持（待用户批准后统一实施）：
  1. 在 `devices/hardware/` 实现 `vendor.mediatek.hardware.composer_ext.IComposerExt/default` 的最小 AIDL no-op HAL 服务（需响应 `registerClient` 及虚表 `+0x48` 对应设置方法并返回 `STATUS_OK`），同时在 `aito-android16/vendor/sepolicy/` 补全 `surfaceflinger` 的 `mtk_hal_composer_ext_service` `find` 及 `binder call` 授权（以及 `AudioParamOptions` `etc/audio_param` 一并生效）；
  2. 或经 `patch.img` + `cdh_redirect` 重定向定制编译的 no-op `libcomposer_ext.so`（不改 `/system` 本体，符合 `AGENTS.md` #8）。
