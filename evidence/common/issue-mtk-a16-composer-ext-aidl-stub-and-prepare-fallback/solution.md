# #272：MTK Android 16 `IComposerExt` AIDL 协议级兼容桩、`02_prepare` DSH 降级兜底与 GRF `mapper` 标签自动保留

- **设备/机型**：跨机型 / 引擎级（首发验证：`infinix/Infinix-X6876`、`motorola/aito`；惠及全部 MTK Android 16 GRF 机型、传音系机型与 `02_prepare` 新入库机型）
- **状态**：`已闭环`

## 问题

在推进 MTK Android 16 GRF 机型（`infinix/Infinix-X6876`、`motorola/aito`）与批量新机型 `02_prepare` 自动化入库时，暴露出 4 个跨机型共性阻塞点：
1. **MTK Android 16 `SurfaceFlinger` 硬依赖 `IComposerExt` 协议交互**：
   - Motorola / 传音等 Android 16 官方 `/system/bin/surfaceflinger` 在 `initializeDisplays()` 中对 `ComposerExt::ClientInterface*` 无空指针判空（或阻塞重试 `Can't get Composer Ext`）；
   - 官方 `/system_ext/lib64/libcomposer_ext.so`（链接 `vendor.mediatek.hardware.composer_ext-V1-ndk.so`）在 `ClientImplAidl::Init` 中调用 `registerClient` 校验 `AStatus_isOk()`，随后紧接着调用 `syncPerform(hnd, op=1 /*checkOpCodeMax*/, ...)` 并强校验返回 `int32 err == 0` 且 `vector<int8> output.size() == 8`（否则报 `checkOpCodeMax failed because of result output size 0 is not 8` 后空指针 `SIGSEGV`）；
   - 原 `neuralnetworks_service.cpp` 的 `OnPlaceholderTransact` 固定返回 `STATUS_UNKNOWN_TRANSACTION`，且当主 `IDevice/default` 未声明而未注册时 `main()` 直接 `return EXIT_SUCCESS` 退出，导致占位服务失效。
2. **`02_prepare` 调用 DSH 遇 `400 data_inspection_failed` 中断 `ApplyProfile`**：
   - DSH 联网搜索工具抓回外部网页含触发内容审查词条时返回 `HTTP 400 data_inspection_failed`，导致 `02_prepare` 跳过 `ApplyProfile`，机型缺失纯 64 位锁、GPU 后端与 `audio_config.xml` 等基础对齐。
3. **MTK 官方 `etc/audio_param` 未纳入 `02_prepare` 自动保留目录**：
   - MTK `/system_ext/lib64/libaudioutilmtk.so` 解析 `MTK_AUDIO_NUMBER_OF_SPEAKER` 时因缺 `/vendor/etc/audio_param` 得到空串，触发 `std::stoi` `SIGABRT` 崩溃循环。
4. **GRF 跨代机型（`vendor=33.0/34.0` + `system=15/16`）误删基底 `mapper/minigbm*` 服务上下文标签**：
   - `09_build_vendor/sepolicy_cil.go` 对 `targetVer != "202504"` 一刀切过滤基底 `vendor_service_contexts` 的 `mapper/minigbm*`，而 Android 15/16 官方 `plat_sepolicy.cil` 实际已声明 `hal_graphics_mapper_service`，导致每台 GRF 机型均需手工补 `vendor_service_contexts`。

## 根因

1. 反汇编 `vendor.mediatek.hardware.composer_ext-V1-ndk.so` 确证 `IComposerExt`（版本 `1`，hash `022c728d211f6b275911cb0602f6641afdd275d6`）的 NDK Binder 布局：
   - 客户端写入标准 `AIBinder` interface header（因此该 `AIBinder_Class` **不得**调用 `AIBinder_Class_disableInterfaceTokenHeader`，否则客户端传入的参数偏移错乱）；
   - `FIRST_CALL_TRANSACTION + 0` (`code=1`): `asyncPerform` -> `AParcel_writeStatusHeader(out, OK)`；
   - `FIRST_CALL_TRANSACTION + 1` (`code=2`): `syncPerform(int64 hnd, int32 op, ...)` -> 读取 `(hnd, op)` 后写入 `Status::OK`、`Result.err`（对 `op == 1` 返回 `0` + 长度 `8` 的 `int64_t(1)` 表示最大支持 opCode 为 1；对 `op > 1` 返回 `-38 (-ENOSYS)` + 空数组使客户端优雅跳过硬件扩展调用）；
   - `FIRST_CALL_TRANSACTION + 2` (`code=3`): `registerClient` -> 返回 `Status::OK`、`int32 ret = 0`、`int64 hnd = 1`。
2. `ai_profile.go` 原逻辑仅在启用 `enable_search=true` 模式下单次请求 DSH，未针对外部网页内容污染做关闭搜索纯推理重试，也无纯实测 `measured-fallback` 保底档案合成。
3. `firmware_measured_extra.go` 的 `officialKeepDirs` 漏列 `"etc/audio_param"`；`apply_profile_configs.go` 的 `alignBuildOdm` 未覆盖传音系 `/odm/etc/build.prop` 四项 `vfy_boot` 防篡改开关。
4. `sepolicy_cil.go` 的 `filterBaseVendorServiceContexts` 仅用 `targetVer == "202504"` 判断是否保留 `mapper/minigbm*`，未检查官方平台策略是否已导出 `hal_graphics_mapper_service`。

## 证据

- 失败日志 [`log-fail.txt`](log-fail.txt)：
  - DSH `HTTP 400: data_inspection_failed` 导致跳过 `ApplyProfile`；
  - `libcomposer_ext: checkOpCodeMax failed because of result output size 0 is not 8` 与 `SurfaceFlinger::initializeDisplays()` `SIGSEGV` 崩溃栈。
- 通过日志 [`log-pass.txt`](log-pass.txt)：
  - `go test -count=1 ./modules/pipeline/stages/02_prepare/... ./modules/pipeline/stages/09_build_vendor/...` 全绿；
  - `Infinix-X6876` 与 `motorola/aito` 启用 `OnComposerExtTransact` 后 `init.svc.surfaceflinger=running` 持续零崩溃，`Infinix-X6876` 冷启动 `passed=true` 直入桌面。

## 改动

1. [`devices/hardware/android.hardware.neuralnetworks/src/neuralnetworks_service.cpp`](../../../hardware/android.hardware.neuralnetworks/src/neuralnetworks_service.cpp)：
   - 新增 `OnComposerExtTransact`，实现 `vendor.mediatek.hardware.composer_ext.IComposerExt` 的 `asyncPerform` / `syncPerform`（`op=1` 8 字节返回、`op>1` `-ENOSYS`）/ `registerClient` / `getInterfaceVersion` / `getInterfaceHash` 完整协议响应，并保持原生 interface token 头解析；
   - 修复 `main()`：当主 `IDevice/default` 未声明而跳过注册时，只要注册了 `cdh_nn_placeholders.txt` 占位服务，继续常驻 `ABinderProcess_joinThreadPool()`。
2. [`agent/modules/pipeline/stages/02_prepare/ai_profile.go`](../../../../agent/modules/pipeline/stages/02_prepare/ai_profile.go) & [`ai_profile_test.go`](../../../../agent/modules/pipeline/stages/02_prepare/ai_profile_test.go)：
   - 捕获 `data_inspection_failed` 时自动关闭联网搜索重试纯模型推理；若 DSH 仍不可达或设置 `CDH_PREPARE_MEASURED_ONLY=1`，自动通过 `buildMeasuredFallbackArchive` 合成实测保底 `device-profile.json`，确保 `ApplyProfile` 100% 执行；新增对应单元测试。
3. [`agent/modules/pipeline/stages/02_prepare/firmware_measured_extra.go`](../../../../agent/modules/pipeline/stages/02_prepare/firmware_measured_extra.go) & [`apply_profile_configs.go`](../../../../agent/modules/pipeline/stages/02_prepare/apply_profile_configs.go)：
   - 官方 vendor 保留目录新增 `"etc/audio_param"`；`nnSet` 过滤高危无桩厂商服务并保留已具完整协议桩的 `IComposerExt`；`alignBuildOdm` 对传音系自动置零四项 `vfy_boot` 属性。
4. [`agent/modules/pipeline/stages/09_build_vendor/sepolicy_cil.go`](../../../../agent/modules/pipeline/stages/09_build_vendor/sepolicy_cil.go)：
   - 新增 `officialPlatSepolicyDeclares(plan, "hal_graphics_mapper_service")`，GRF 跨代机型自动保留基底 `mapper/minigbm*` 服务标签。

## 验证

1. 执行 `go test -count=1 ./modules/pipeline/stages/02_prepare/... ./modules/pipeline/stages/09_build_vendor/...` 全部通过，重编 `cdh-pipeline` 与 `android15`/`android16` `android.hardware.neuralnetworks`。
2. `infinix/Infinix-X6876` 执行 `apply-profile` 自动完成全套对齐，冷启动 `SurfaceFlinger` 与 `audioserver` 零崩溃直入桌面（`meanBrightness=56.88`）；`motorola/aito` 冷启动 `SurfaceFlinger` 同样零崩溃点亮 Motorola 开机画面（`meanBrightness=8.58`）。

## 遗留

- 无。
