# #216：对齐传感器静置重力模长 `9.8066`、陀螺仪细分步长与 50Hz 无漂移采样节拍

## 问题

首轮 `aligner.ids_full`（`4d5128bd8ca07462-cloud`）显示 `sensorFingerprint` 存在多项运动传感器动态特征与采样频度差异：

1. **静置重力模长偏低**：云机加速度计 `lsm6dso_acc` `norm = 9.728`（`normErr = -0.0787`、`fpStr` 尾巴出现 `an:9.7`），偏离标准重力 `9.80665`（真机 `an:9.9`，系桌面斜放）；
2. **陀螺仪量化步长过粗**：云机 `lsm6dso_gyro` `minDiff = [0.0011, 0.0011, 0.0011]`，而真机细粒度可达 `0.000137`（约为 `resolution` `0.0011 / 8`）；
3. **50Hz 采样频率漂移偏少**：1.5s 采样窗口（20ms 间隔理想约 75~76 点）云机仅采到 `62~63` 个点（`gyroscope.n` 报错 `76 vs 62` state 差异）。
4. **此前人工判读勘误**：复查原始 `ids.json` 数值确认，旧云机磁力计均值本已是正常地球磁场 `[24.769, -4.697, -39.456]` µT（模长 ~46.9 µT，sigma 0.02~0.04），反倒是实机桌面的真机 `07cf5fc1784783d5` 因周边磁场干扰出现 `[263.454, -17.651, -679.226]` µT（模长 ~729 µT，sigma 0.69~2.16），此前 `fill_analysis.py` 把 `base`/`target` 读反误判为云机磁力计失真。

## 根因

1. `sensor_event_model.cpp` 的 `fillVec3()` 对加速度计（type 1/35）使用 `9.7266f` 作为静置 Z 轴均值且波形振幅仅 `3.5f * step`，对陀螺仪（type 4/16）直接以 `resolution`（`0.0011f`）作为量化步长 `step`，未做 `1/8` 细分；
2. `sensors_service.cpp` 的 `runEventLoop()` 固定 `wait_for(5ms)`，且 `SensorEventModel::collectDue()` 每次将 `state.nextSampleNs = nowNs + state.samplingPeriodNs`。每次调度延迟 1~4ms 叠加进入下一周期，造成 20ms 目标周期实际拉长至 ~24ms，1.5s 内仅产出 ~62 个样本。

## 证据

- 修复前：`07cf5fc1784783d5` 与 `4d5128bd8ca07462-cloud` 原始 `sensorFingerprint` 数据对比见同目录 [`log-fail.txt`](log-fail.txt)。
- 修复后：宿主 C++ 单元测试结果与 `7d10bffa4bc09406-cloud` `aligner.ids_full` 重采结果见同目录 [`log-pass.txt`](log-pass.txt)。

## 改动

1. **[`sensor_event_model.cpp`](../../../vendor/hardware/android.hardware.sensors/src/sensor_event_model.cpp) 与 [`sensor_event_model.h`](../../../vendor/hardware/android.hardware.sensors/src/sensor_event_model.h)**：
   - 加速度计（type 1/35）静置 Z 轴中心调整为 `9.8066f`，波动幅值调整为 `9.5f * step`；
   - 陀螺仪（type 4/16）步长细分为 `resolution > 1e-7f ? (resolution / 8.0f) : 0.0001375f`，波动幅值设为 `8.0f * step`；
   - 新增 `SensorEventModel::nextWaitNs(int64_t nowNs)`（精确计算距下次到期采样时间并限幅 5ms）；
   - `collectDue()` 对连续型传感器改用累加推进 `state.nextSampleNs += state.samplingPeriodNs`（若滞后超过完整周期则整倍跳过追赶），消除累计时间漂移。
2. **[`sensors_service.cpp`](../../../vendor/hardware/android.hardware.sensors/src/sensors_service.cpp)**：
   - `runEventLoop()` 在正常激活态下改用 `eventModel_.nextWaitNs(bootTimeNanos())` 动态等待后再 `collectDue()`。
3. **新增 [`sensor_event_model_test.cpp`](../../../vendor/hardware/android.hardware.sensors/src/sensor_event_model_test.cpp)**：
   - 在 `CDH_SENSORS_HOST_TEST` 宏下校验 50Hz/1.5s 采样个数（`75..76`）、加速度计均值模长（`9.8066 ± 0.02`）、acc/gyro 最小量化步长以及 `nextWaitNs` 追赶边界。

## 验证

1. **宿主 C++ 单元测试**（2026-11-14 复测）：
   - `PASS test_acc_and_gyro_stats_at_50hz: n=75 norm=9.8147 minAccDiff=0.001949 minGyroDiff=0.000137`
   - `PASS test_next_wait_and_catchup`
2. **实例 `gold_15-OS3.0.11.0.VNQCNXM/2`（`7d10bffa4bc09406-cloud`）`aligner.ids_full` 重采**：
   - `acc.norm`: `9.728 -> 9.7958`（`normErr`: `-0.0787 -> -0.0108`，`fpStr` `an:9.7 -> an:9.8`），`acc.sigma` 增至 `[0.009775, 0.013062, 0.006471]`，样本数 `n`: `63 -> 76`；
   - `gyro.minDiff`: `[0.0011, 0.0011, 0.0011] -> [0.000137, 0.000137, 0.000137]`（精确对齐真机步长 `0.000137`），样本数 `n`: `62 -> 76`（精确匹配真机 `76`，`sensorFingerprint.gyroscope.n` 与 `sampled[2].n` 两条 state 差异清零）；
   - `mag.n`: `62 -> 75`，均值 `[24.823, -4.715, -39.52]` µT（正常地球磁场量级）。

## 遗留

1. 若后续希望每台云机实例呈现不同静置桌面摆放倾角或温和变化的地磁矢量，可在 `sensor_event_model.cpp` 读取实例级种子偏移进一步随机化。
