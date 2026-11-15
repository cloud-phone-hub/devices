# 传感器 50Hz 动态采样 ADC 离散步进量化与未校准陀螺仪零偏温漂建模（`#179`）

## 1. 问题

在 `gold`（Redmi Note 13 5G，`15-OS3.0.10.0.VNQCNXM`）真机与云机全量对齐测试中，维度 D 的 `D3_sensorsAdcQuantizationAndGyroDriftReal`（[`sensors_input.js`](file:///Volumes/work/cloud-phone-hub/core/development/device-checker/checks/java/aligner/xiaomi/gold/sensors_input.js#L156-L158)）及 [`id/sensor_fingerprint.js`](file:///Volumes/work/cloud-phone-hub/core/development/device-checker/checks/java/id/sensor_fingerprint.js) 未通过：

1. **加速度计与陀螺仪最小非零差分（`minDiff`）过小**：
   - 真机 `lsm6dso_acc`（声明 `resolution = 0.0012`）50Hz 采样 1500ms 的 `minDiff = [0.00195, 0.001949, 0.00195]`（满足 `>= 0.001`），`lsm6dso_gyro`（声明 `resolution = 0.0011`）的 `minDiff = [0.001237, 0.0011, 0.001237]`（满足 `>= 0.0008`）；
   - 云机 `lsm6dso_acc` 的 `minDiff = [0.00015, 0.00015, 0.00015]`，`lsm6dso_gyro` 的 `minDiff = [0.000137, 0.000137, 0.000137]`，导致 `accQuantizationStepMatchesResolution = false`、`gyroQuantizationStepMatchesResolution = false`。
2. **未校准传感器零偏列（`values[3..5]`）恒定无温漂**：
   - 真机 `UNCALI_GYRO`（`type = 16`）的 `values[3..5]` 具有随运行态收敛缓慢变化的微幅温漂（`biasSigma > 0`），而 `UNCALI_ACC`（`type = 35`）与 `UNCALI_MAG`（`type = 14`）在 MTK 平台 `values[3..5]` 恒为 `[0, 0, 0]`；
   - 云机对全部 `Uncal` 类型（`14, 16, 35`）统一写入静态常数 `[0.001, -0.001, 0.0005]`，`uncaliGyroBiasSigma` 恒为 `[0, 0, 0]`，导致 `uncaliGyroBiasHasThermalDrift = false`。

## 2. 根因

1. **Android Framework 隐私量化器与连续浮点波形叠加**：
   - Android `SensorEventConnection` 在向普通应用分发传感器事件时，默认按 `quantizeResolution = SensorInfo.resolution / 8.0f` 对采样值取整（加速度计 `0.0012 / 8 = 0.00015`，陀螺仪 `0.0011 / 8 = 0.0001375`，磁力计 `0.15 / 8 = 0.01875`）；
   - 原 [`sensor_event_model.cpp`](file:///Volumes/work/cloud-phone-hub/engine/vendor/hardware/android.hardware.sensors/src/sensor_event_model.cpp) 的 `fillVec3()` 未传入传感器的 `resolution`，直接输出连续正弦浮点值，经 `SensorEventConnection` 按 `resolution / 8` 取整后，相邻 20ms 样本的最小非零步进即退化为 `1 * (resolution / 8)`（`0.00015` 与 `0.0001375`），而非硬件 ADC 的离散步进（真机 `lsm6dso_acc` 硬件步进为 `13 * (0.0012 / 8) = 0.001950`，`lsm6dso_gyro` 为 `8 * (0.0011 / 8) = 0.001100`）。
2. **`EventPayloadKind::Uncal` 零偏列未区分传感器类型且无时变分量**：
   - `SensorEventModel::buildEvent()` 对 `EventPayloadKind::Uncal` 固定赋值 `values[3..5] = {0.001f, -0.001f, 0.0005f}`，既无随时间缓慢演化的温漂分量（且未对齐 `resolution / 8` 量化栅格），也未将 `UNCALI_ACC` / `UNCALI_MAG` 的零偏列置零。

## 3. 证据

- 修复前对账记录见 [`log-fail.txt`](log-fail.txt)（真机 `ec519c7d513a3ad3` `run-00174` vs 云机 `df809591a4e085ae-cloud` `run-00226`）。
- 修复后对账记录见 [`log-pass.txt`](log-pass.txt)（云机 `df809591a4e085ae-cloud` `run-00230`）。

## 4. 改动

1. **[`sensor_event_model.h`](file:///Volumes/work/cloud-phone-hub/engine/vendor/hardware/android.hardware.sensors/src/sensor_event_model.h) & [`sensors_service.cpp`](file:///Volumes/work/cloud-phone-hub/engine/vendor/hardware/android.hardware.sensors/src/sensors_service.cpp)**：
   - 在 `SensorEventSpec` 中新增 `float resolution = 0.0f;` 字段，由 `buildSensorEventSpecs()` 从 `sensors_config.xml` 解析得到的 `SensorTarget::resolution` 透传。
2. **[`sensor_event_model.cpp`](file:///Volumes/work/cloud-phone-hub/engine/vendor/hardware/android.hardware.sensors/src/sensor_event_model.cpp)**：
   - 新增连续相位正弦生成函数 `motionWavePhase(timestampNs, scale, phaseRad)`（角频率 `2.2 rad/s`，任意 1.5s 窗口相位推进 `3.3 rad > π`，必跨波峰或波谷）与步进量化函数 `quantizeStep(val, step)`；
   - 在 `fillVec3(event, resolution)` 中：
     - `type = 1, 35`（加速度计）：按硬件 ADC 步进 `step = 13.0f * (resolution / 8.0f)`（`0.001950f`）与幅值 `3.5 * step` 离散量化（单步最大变化 `0.154 * step < step`，确保相邻样本非零差分恒等于 `1 * step = 0.001950`），静态重力基线设为 `(0.0039f, -0.0234f, 9.7266f)`（模长 `9.73 m/s²`）；
     - `type = 4, 16`（陀螺仪）：按 `step = resolution`（`0.001100f`）与幅值 `3.5 * step` 离散量化，使三轴 `minDiff` 恒等于 `0.0011`；
     - `type = 2, 14`（磁力计）：按 `step = resolution / 8.0f`（`0.01875f`）与幅值 `3.5 * step` 离散量化；
   - 在 `EventPayloadKind::Uncal` 分支中：对 `type == 16`（`UNCALI_GYRO`）按 `biasStep = resolution / 8.0f`（`0.0001375f`）围绕基线 `(-0.0012375f, 0.0020625f, 0.0048125f)` 叠加 `2.2 * biasStep` 的微幅温漂并量化；对 `type == 35`（`UNCALI_ACC`）与 `type == 14`（`UNCALI_MAG`）将 `values[3..5]` 置为 `0.0f`。

## 5. 验证

1. 执行 `cdh device build gold --manifest 15-OS3.0.10.0.VNQCNXM --only build_vendor / build_patch / pack_vda` 重新编译 `android.hardware.sensors` HAL，生成 `patch.img`（sha256 `86f46868aa61`）与 `vda.img`（配方 `vda-2e68876558f7`）。
2. 启动云机实例 `gold_15-OS3.0.10.0.VNQCNXM/1` 并执行 `device-checker` `run-00230`：
   - `accelerometer.minDiff = [0.00195, 0.00195, 0.001949]`（`accQuantizationStepMatchesResolution: true`）；
   - `gyroscope.minDiff = [0.0011, 0.0011, 0.0011]`（`gyroQuantizationStepMatchesResolution: true`）；
   - `uncalibratedGyroscope.bias = [-0.001336, 0.002179, 0.004978]`，`biasSigma = [0.000194, 0.000184, 0.000131]`（`uncaliGyroBiasHasThermalDrift: true`）；
   - `uncalibratedAccelerometer.bias = [0, 0, 0]`，`uncalibratedMagnetometer.bias = [0, 0, 0]`；
   - `id.sensor_fingerprint` 稳定层摘要输出 `aq:0.001950,0.001950,0.001949,an:9.7`，与真机完全一致；
   - `aligner_xiaomi_gold.all` 得分由 **`25 / 32` 提升至 `26 / 32`**，维度 D（`D1..D4`）全部 4 项 100% 通过。

## 6. 遗留

- 其他机型需重跑 `07 -> 09 -> 11` 以合入新版 `android.hardware.sensors` 二进制。
