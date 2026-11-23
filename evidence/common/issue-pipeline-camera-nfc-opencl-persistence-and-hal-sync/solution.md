# #318 跨机型 Camera / NFC / OpenCL 流水线持久化与全代 HAL 同步重编收尾

## 一、问题

在 `#312`~`#317` 攻克 Honor `chora`、vivo `PD2454`、Nothing `Asteroids`、Motorola `aito`、RedMagic `NX809J`、ASUS `ASUSAI2501`、HMD `ARW`、Tecno `TECNO-CM5` 期间，部分 `patch.img` 修复（Honor NFC 配置文件/权限 XML/VINTF、vivo `IVivoCameraProvider` VINTF 与 SELinux、Nothing `INtCamService` SELinux、官方 `libOpenCL.so` 兼容桩替换、ASUS `vendor.camera.` 属性上下文与 `vendor.camera.lens_type`）最初以单机增量修补验证；若后续执行全量 `cdh-pipeline device build --refresh`，上述修补尚未全部落入 `09_build_vendor` 与 `halbuild/gpu.go`，且 `android13..17` 各代 `camera`/`neuralnetworks`/`gpu` 产物版本存在代际不同步。

## 二、根因

1. **`halbuild/gpu.go` 未编译官方 OpenCL 兼容桩**：`devices/hardware/android.hardware.gpu/src/cdh_opencl_stub.c` 尚未接入 `buildGpu()`，导致各代 `out/devices/vendor/android<N>/lib64/libOpenCL.so` 无产物；
2. **`09_build_vendor` 缺少跨机型相机/NFC/OpenCL 幂等收口 Pass**：
   - `backfillOfficialNfcConfigs` 未回填 `libnfc-{nci,nxp}-sz.conf`，未对 Honor NFC 机型自动补齐 `com.hihonor.nfc.xml` 及向 `cdh_nn_placeholders.txt` 追加 `INxpNfc/default` / `IHnNfc/default`；
   - 未在 `vendor_service_contexts` 含 `IVivoCameraProvider` 时自动向 `android.hardware.camera-service.xml` 注入 `vendor.vivo.hardware.vivocameraprovider` VINTF；
   - 未在 `vendor_sepolicy.cil` 声明 `vendor_set_camera_prop` 时自动补齐 `vendor_property_contexts` 的 `vendor.camera.` 映射，且 `ASUSAI2501` 的 `vendor.camera.lens_type` 未纳入配置与 `cdh_system_props.rc` 桥接；
   - 未对 `public.libraries.txt` 声明 `libOpenCL.so` 的机型自动覆写官方 OpenCL 兼容桩并绑定 `same_process_hal_file`；
   - `sepolicy_cil.go` 尚未内置 Honor NFC、Vivo Camera、Nothing Camera 的最小 SELinux CIL 授权；
3. **跨代 HAL 产物未统一刷新**：`android13/14/15/17` 的 `android.hardware.camera` 缺少 `#316` Nubia 标签与模板改动，`android13/16/17` 的 `android.hardware.neuralnetworks` 缺少最新 Honor/Vivo/Nothing 扩展接口，全 5 代 `android.hardware.gpu` 缺少 `lib64/libOpenCL.so` 产物。

## 三、证据

- 改动前缺口记录见同目录 `log-fail.txt`；
- 全仓单测、全 5 代 HAL 同步重编、产物校验与零孤儿进程核验日志见同目录 `log-pass.txt`。

## 四、改动

1. **`agent/modules/pipeline/internal/halcore/halbuild/gpu.go`**：
   - 在 `buildGpu()` 新增检测 `srcDir/cdh_opencl_stub.c`，通过新增的 `compileGpuLibSoname(..., "libOpenCL.so", nil)` 编译并落盘至 `lib64/libOpenCL.so`（带 `DT_SONAME=libOpenCL.so`、`0644`）。
2. **`agent/modules/pipeline/stages/09_build_vendor/files_compat.go`**：
   - 扩展 `backfillOfficialNfcConfigs` 回填 `libnfc-nci-sz.conf` / `libnfc-nxp-sz.conf`，新增 `alignHonorNfcCompat` 幂等补齐 `etc/permissions/com.hihonor.nfc.xml`、`-sz.conf` 兜底及向 `cdh_nn_placeholders.txt` 追加 `INxpNfc/default` / `IHnNfc/default`（串联 `synthesizePlaceholderVintfManifest` 自动生成 VINTF）；
   - 新增 `alignVendorCameraVintfCompat`：对含 `IVivoCameraProvider` 的机型自动向 `android.hardware.camera-service.xml` 注入 AIDL VINTF 声明；
   - 新增 `alignVendorCameraPropertyContexts`：对声明了 `vendor_set_camera_prop` 的机型幂等补齐 `vendor.camera.` 与 `ro.vendor.camera.sound.forced` 属性上下文；
   - 新增 `alignVendorOpenCLStub`：对 `etc/public.libraries.txt` 声明了 `libOpenCL.so` 的机型自动以 `HalRoot/lib64/libOpenCL.so` 覆写 `vendor/lib64/libOpenCL.so` 并写入 `same_process_hal_file` 上下文规则；
   - 扩展 `isSystemDomainPropKey` 纳入 `vendor.camera.lens_type`，并在 `devices/manifest/configs/asus/ASUSAI2501/ASUSAI2501-android15/vendor/config.json` 持久化 `"vendor.camera.lens_type": "0:std;1:std"`。
3. **`agent/modules/pipeline/stages/09_build_vendor/sepolicy_cil.go`**：
   - 新增 `reconcileVendorCameraNfcCompatCIL` 并接入 `reconcilePlatformGenfscon`，幂等补齐 Honor NFC（`nfc` 读 `sysfs`、`hal_neuralnetworks_default` 注册与 `nfc` 访问 `hal_nxpnfc_service`/`hal_hnnfc_service`）、Vivo Camera（`platform_app` 访问 `hal_camera3rd_service`/`hal_cameralog_service`/`hal_camera_service` 及 binder 调用）、Nothing Camera（`platform_app` 访问 `vendor_hal_noth_camera_service` 及 binder 调用）的最小 CIL 规则。
4. **`agent/modules/pipeline/stages/09_build_vendor/camera_nfc_compat_test.go`**：
   - 补齐 6 项同目录白盒单测（覆盖 Honor NFC + VINTF 串联、Vivo Camera VINTF、ASUS `vendor_set_camera_prop`、OpenCL 兼容桩自动装配、三机型 CIL 绑定及幂等性、`vendor.camera.lens_type` 属性桥接）。
5. **重编 `cdh-pipeline` 与同步重编全 5 代 HAL**：
   - 重编 `out/agent/dist/cdh-pipeline`；
   - 执行 `build-hardware -android <13..17> -module android.hardware.camera -module android.hardware.neuralnetworks -module android.hardware.gpu -refresh`，全 5 代统一刷新。
6. **最终清理**：
   - 停止所有测试 VM 与 `host-daemon`；清理 `out/devices/tmp/` 中临时反编译目录（`*_inspect`）与过程截屏（`*.png` 等，释放 2.3+ GB），保留 `5b56c8b31b1ad.mp4`、`patch_hal_refresh.py`、`oplus_patch_fix.py`、`qmp_tap.py`、`official_libOpenCL.so` 等复测工具。

## 五、验证

1. **新增白盒单测与全仓单测**：
   - `go -C agent test -v ./modules/pipeline/stages/09_build_vendor -run 'TestAlignHonorNfc|TestAlignVendorCamera|TestAlignVendorOpenCL|TestReconcileVendorCameraNfc|TestIsSystemDomainPropKeyCameraLensType'`：6/6 PASS；
   - `go -C agent test ./modules/pipeline/... ./modules/daemon/backend/nfc/...`：全仓 32 个测试包全部 PASS。
2. **全 5 代 HAL 与 OpenCL 产物核验**：
   - `out/devices/vendor/android{13,14,15,16,17}/` 均已刷新生成 `bin/hw/android.hardware.camera-service`、`bin/hw/android.hardware.neuralnetworks-service`、`lib64/egl/libGLESv2_meow.so` 及 `lib64/libOpenCL.so`（43,536 B，`DT_SONAME=libOpenCL.so`，导出 `clGetPlatformIDs`/`clCreateContext` 等标准入口）。
3. **无孤儿进程与清理复核**：
   - `ps aux | grep -E 'host-daemon|qemu-system' | grep -v grep` 输出为空；`out/devices/tmp/*_inspect` 与临时截屏已清零，复测工具均完整保留。

## 六、遗留

无。本轮 `TODO.md` 所列全部 P0 / P1 / P2 机型适配、流水线持久化、全代 HAL 与 6.6 内核模块重编及清理任务已 100% 闭环。
