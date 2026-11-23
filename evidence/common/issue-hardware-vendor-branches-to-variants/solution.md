# #320 模拟硬件厂商分支全面改造成 Variant 变体架构（radio / neuralnetworks / camera）

## 一、问题

此前 `devices/hardware/` 下三大核心模拟硬件模块把大量跨厂商逻辑直接塞进了 `default` 主实现文件并通过运行期属性或接口名分支分发：
1. `android.hardware.camera/src/camera_provider_service.cpp` 膨胀至 3,417 行，在 `default` 里混杂 10 处厂商逻辑（`isPixelDualRearDevice`、`isOplusDevice`、`isSamsungDevice`、`isMeizuDevice`、`isHonorDevice`、`isVivoDevice` + 整套 `IVivoCameraProvider` AIDL 服务端、`isNothingDevice`、`isMotorolaDevice`、`isNubiaDevice`、`ro.hardware.camera == "unisoc"`）与 3 份厂商 `.inc` 巨表；
2. `android.hardware.neuralnetworks/src/neuralnetworks_service.cpp` 655 行，在 `default` 里硬编码 MTK `IComposerExt`、Honor `INxpNfc`、Vivo `IVASAlgoProvider`/`IVASAlgoContext`、Nothing `INtCamService`/`INtCamSession` 四家协议；
3. `android.hardware.radio/src/main.cpp` 807 行，虽已拆分 `mtk` 变体，但 `default` 仍混入高通 `IQtiOemHook` 与展锐 6 组 `IExtRadio*` 回调表。

## 二、根因

随着跨 15 大品牌机型适配推进，临时以 `default` 统一编入验证虽加快了首轮落地，但未及时沿用仓库既有 `health@{oplus,samsung}` / `composer3@oplus` / `radio@mtk` 的变体隔离范式（`08_build_hardware/modules/*.json` 的 `variants` + `extraSourceDirs` + 变体宏），导致 `default` 产物携带无关厂商代码与符号、耦合高且维护成本持续膨胀。

## 三、证据

- 重构前代码体积与厂商分支分布统计见同目录 `log-fail.txt`；
- 重构后主文件瘦身对比、全部 20 个变体目标编译链接日志、20/20 二进制跨厂商符号隔离校验及全仓单测结果见同目录 `log-pass.txt`。

## 四、改动

1. **`android.hardware.radio`**（`main.cpp` 807 → 311 行）：
   - 新增 `src/radio_placeholder_ext.h` 统一占位扩展接口（`default`/`mtk` 为零开销空内联）；
   - 新增 `src/qti/radio_qti_ext.cpp`（`-DCDH_RADIO_QTI=1`）承接高通 `IQtiOemHook` 与 `IQtiRadioStable` 规范化；
   - 新增 `src/unisoc/radio_unisoc_ext.cpp`（`-DCDH_RADIO_UNISOC=1`）承接展锐 6 组 `IExtRadio*` 与 `vendor.unisoc.hardware.radio.*` 契约应答；
   - 在 `agent/modules/pipeline/stages/08_build_hardware/modules/android.hardware.radio.json` 声明 `qti`、`unisoc` 变体。
2. **`android.hardware.neuralnetworks`**（`neuralnetworks_service.cpp` 655 → 254 行）：
   - 新增 `src/nn_vendor_ext.h` 统一扩展钩子（`default` 返回 `nullptr`）；
   - 拆分 `src/mtk/nn_mtk_ext.cpp`（`-DCDH_NN_MTK=1`）、`src/honor/nn_honor_ext.cpp`（`-DCDH_NN_HONOR=1`）、`src/vivo/nn_vivo_ext.cpp`（`-DCDH_NN_VIVO=1`）、`src/nothing/nn_nothing_ext.cpp`（`-DCDH_NN_NOTHING=1`）；
   - 在 `agent/modules/pipeline/stages/08_build_hardware/modules/android.hardware.neuralnetworks.json` 声明 `mtk`/`honor`/`vivo`/`nothing` 变体。
3. **`android.hardware.camera`**（`camera_provider_service.cpp` 3,417 → 2,386 行，净减 1,031 行）：
   - 新增 `src/camera_vendor_ext.h` 统一 6 项扩展钩子（`default` 全空内联）；
   - 拆分 10 个独立变体子目录：`src/pixel/`、`src/oplus/`、`src/samsung/`、`src/meizu/`、`src/honor/`、`src/vivo/`（含整套 `IVivoCameraProvider` 服务端）、`src/nothing/`（迁入 `cdh_nothing_vendor_tags.inc`）、`src/motorola/`（迁入 `cdh_motorola_vendor_tags.inc`）、`src/nubia/`（迁入 `cdh_nubia_vendor_tags.inc`）、`src/unisoc/`；
   - 清除 `camera_provider_service.cpp` 中全部 9 处运行期品牌/机型判断；
   - 在 `agent/modules/pipeline/stages/08_build_hardware/modules/android.hardware.camera.json` 声明全部 10 个变体。
4. **机型配置、自动推导与单测**：
   - 批量精确对齐全仓 40/42 份 Android 机型 `devices/manifest/configs/*/*/*/hardware/config.json`（剩余 2 份为纯血鸿蒙）；
   - 更新 `agent/modules/pipeline/stages/02_prepare/apply_profile.go` 按平台/品牌自动对齐 `radio`（`mtk`/`qti`/`unisoc`/`default`）、`camera` 与 `neuralnetworks` 变体；
   - 在 `agent/modules/pipeline/internal/halcore/spec/spec_test.go` 新增 `TestRealCameraRadioAndNNVariants` 白盒单测并重编 `out/agent/dist/cdh-pipeline`。

## 五、验证

1. **全仓单测**：`go -C agent test ./modules/pipeline/...` 全 31 包 PASS（含 `TestRealCameraRadioAndNNVariants` 与 `02_prepare`）；
2. **全 20 个变体编译验证（`android15`）**：
   - `android.hardware.radio`：`default`/`mtk`/`qti`/`unisoc` 4/4 编译链接通过；
   - `android.hardware.neuralnetworks`：`default`/`mtk`/`honor`/`vivo`/`nothing` 5/5 编译链接通过；
   - `android.hardware.camera`：`default`/`pixel`/`oplus`/`samsung`/`meizu`/`honor`/`vivo`/`nothing`/`motorola`/`nubia`/`unisoc` 11/11 编译链接通过；
3. **二进制跨厂商符号隔离校验**：20/20 目标 `[PASS]`，`default` 产物零跨厂商协议与标签泄漏，各变体仅包含自身厂商代码。

## 六、遗留

无。后续构建任意机型时，`09_build_vendor` 会按各机型 `hardware/config.json` 中声明的变体自动按需触发对应代际（`android13..17`）与变体的编译并从 `out/devices/vendor/android<N>/<variant>/` 精确装配。
