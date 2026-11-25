// engine/vendor/hardware 线 —— sensors 虚拟事件模型实现（设备无关）。
//
// 按传感器类型生成虚拟事件流；当提供非零 instanceSeed 时，为每个云机实例确定性派生：
//   1. 满足局部重力模长守恒（~9.80665 m/s^2）的自然桌面微倾角 (gx, gy, gz)；
//   2. 与微倾角几何自洽的地磁三分量、欧拉方向角与旋转矢量四元数；
//   3. 单机专属的 UNCALI_GYRO 出厂零偏（保持 UNCALI_ACC / UNCALI_MAG 偏置为 0 与 MTK 真机一致）；
//   4. 确定性无状态 Box-Muller 宽带高斯白噪声 + 慢变温漂游走 + 真实 ADC 步进量化，
//      消除正弦单频尖峰与多实例静态姿态/零偏聚类特征。

#include "sensor_event_model.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace cdh::sensors {
namespace {

constexpr int64_t kNanosecondsPerMicrosecond = 1000;
constexpr int64_t kDefaultSamplingPeriodNs = 20'000'000;
constexpr int32_t kDevicePrivateBase = 0x10000;
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 6.28318530717958647692;

uint64_t splitMix64(uint64_t *state) {
  uint64_t z = (*state += 0x9e3779b97f4a7c15ULL);
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
  return z ^ (z >> 31);
}

// 映射到 (0, 1) 开区间，供 Box-Muller 变换安全取对数。
double uniformOpen01(uint64_t raw) {
  return (static_cast<double>(raw >> 11) + 0.5) * (1.0 / 9007199254740992.0);
}

double uniformRange(uint64_t *state, double lo, double hi) {
  return lo + (hi - lo) * uniformOpen01(splitMix64(state));
}

// 基于 (seed, channelTag, seq) 的无状态标准正态采样 N(0, 1)。
float statelessGaussian(uint64_t seed, uint64_t channelTag, uint64_t seq) {
  uint64_t st = seed ^ (channelTag * 0xd6e8feb86659fd93ULL) ^
                (seq * 0xa0761d6478bd642fULL);
  const double u1 = uniformOpen01(splitMix64(&st));
  const double u2 = uniformOpen01(splitMix64(&st));
  const double r = std::sqrt(-2.0 * std::log(u1));
  const double z = r * std::cos(kTwoPi * u2);
  // 限幅 [-3.5, 3.5] 防止极端离群值超出物理 ADC 抖动包络
  const double clamped = std::max(-3.5, std::min(3.5, z));
  return static_cast<float>(clamped);
}

float motionWavePhase(int64_t timestampNs, float scale, double phaseRad = 0.0) {
  const double seconds = static_cast<double>(timestampNs) * 1.0e-9;
  const double angle = std::fmod(seconds * 2.2 + phaseRad, kTwoPi);
  return static_cast<float>(std::sin(angle) * static_cast<double>(scale));
}

float motionWave(int64_t timestampNs, float scale) {
  return motionWavePhase(timestampNs, scale, 0.0);
}

float quantizeStep(float val, float step) {
  if (step <= 1.0e-7f) {
    return val;
  }
  return std::round(val / step) * step;
}

EventPayloadKind payloadKindForType(int32_t type) {
  switch (type) {
  case 1:
  case 2:
  case 3:
  case 4:
  case 9:
  case 10:
    return EventPayloadKind::Vec3;
  case 15:
    return EventPayloadKind::Vec4;
  case 14:
  case 16:
  case 35:
    return EventPayloadKind::Uncal;
  case 11:
  case 20:
    return EventPayloadKind::Data;
  case 19:
    return EventPayloadKind::StepCount;
  default:
    return type >= kDevicePrivateBase ? EventPayloadKind::Data
                                      : EventPayloadKind::Scalar;
  }
}

void fillVec3(VirtualSensorEvent &event, float resolution,
              const SensorEventModel::InstanceCalibration &cal,
              uint64_t sampleSeq) {
  const float wave = motionWave(event.timestampNs, 1.0f);
  const bool seeded = (cal.seed != 0);
  const double sec = static_cast<double>(event.timestampNs) * 1.0e-9;

  switch (event.sensorType) {
  case 1:
  case 35: {
    const float step =
        resolution > 1.0e-7f ? (13.0f * (resolution / 8.0f)) : 0.00195f;
    if (!seeded) {
      event.values[0] = quantizeStep(
          cal.gravityBase[0] + motionWavePhase(event.timestampNs, 9.5f * step, 0.0),
          step);
      event.values[1] = quantizeStep(
          cal.gravityBase[1] + motionWavePhase(event.timestampNs, 9.5f * step, 1.1),
          step);
      event.values[2] = quantizeStep(
          cal.gravityBase[2] + motionWavePhase(event.timestampNs, 9.5f * step, 2.3),
          step);
    } else {
      // 宽带 MEMS 热噪声（sigma ~ 4.5 * step ≈ 0.0088 m/s^2）叠加慢变呼吸微颤
      for (int axis = 0; axis < 3; ++axis) {
        const float white =
            statelessGaussian(cal.seed, 0x100U + static_cast<uint64_t>(axis),
                              sampleSeq) *
            (4.5f * step);
        const float drift = static_cast<float>(
            std::sin(sec * (0.37 + 0.11 * axis) +
                     static_cast<double>((cal.seed >> (axis * 8)) & 0xffU) * 0.024)) *
            (1.6f * step);
        event.values[axis] =
            quantizeStep(cal.gravityBase[axis] + white + drift, step);
      }
    }
    break;
  }
  case 2:
  case 14: {
    const float step = resolution > 1.0e-7f ? (resolution / 8.0f) : 0.01875f;
    if (!seeded) {
      const float jx = static_cast<float>(
          0.58 * std::sin(sec * 5.3 + 0.4) + 0.26 * std::sin(sec * 11.7 + 1.1));
      const float jy = static_cast<float>(
          0.64 * std::sin(sec * 4.7 + 1.7) + 0.29 * std::sin(sec * 10.3 + 2.4));
      const float jz = static_cast<float>(
          0.72 * std::sin(sec * 6.1 + 3.0) + 0.31 * std::sin(sec * 13.1 + 0.7));
      event.values[0] = quantizeStep(cal.magBase[0] + jx, step);
      event.values[1] = quantizeStep(cal.magBase[1] + jy, step);
      event.values[2] = quantizeStep(cal.magBase[2] + jz, step);
    } else {
      // 宽带磁热噪声 + 环境工频次谐波微扰，使 1.5s 采样窗口各轴标准差稳定在 0.40~0.68 uT
      const float scales[3] = {0.44f, 0.48f, 0.54f};
      for (int axis = 0; axis < 3; ++axis) {
        const float white =
            statelessGaussian(cal.seed, 0x200U + static_cast<uint64_t>(axis),
                              sampleSeq) *
            scales[axis];
        const float ripple = static_cast<float>(
            0.14 * std::sin(sec * (1.9 + 0.43 * axis) +
                            static_cast<double>((cal.seed >> (16 + axis * 5)) & 0x1fU)));
        event.values[axis] =
            quantizeStep(cal.magBase[axis] + white + ripple, step);
      }
    }
    break;
  }
  case 3:
    event.values[0] = cal.orientBase[0] + wave * 0.18f;
    event.values[1] = cal.orientBase[1] + wave * 0.04f;
    event.values[2] = cal.orientBase[2] + wave * 0.04f;
    break;
  case 4:
  case 16: {
    // 硬件陀螺仪真实 ADC 离散步进为 8 * (resolution / 8) = resolution（如 lsm6dso_gyro 0.0011 rad/s），
    // 对齐 Android SensorEventConnection 的 resolution/8 隐私量化栅格且保证非零 minDiff >= 0.0008。
    const float step = resolution > 1.0e-7f ? resolution : 0.0011f;
    if (!seeded) {
      event.values[0] = quantizeStep(
          motionWavePhase(event.timestampNs, 3.5f * step, 0.2), step);
      event.values[1] = quantizeStep(
          motionWavePhase(event.timestampNs, 3.5f * step, 1.5), step);
      event.values[2] = quantizeStep(
          motionWavePhase(event.timestampNs, 3.5f * step, 2.8), step);
    } else {
      // 校准陀螺围绕 0 附近呈宽带高斯白噪声（sigma ~ 0.85 * step ≈ 0.000935 rad/s，对齐真机 0.0007~0.00095）
      for (int axis = 0; axis < 3; ++axis) {
        const float white =
            statelessGaussian(cal.seed, 0x400U + static_cast<uint64_t>(axis),
                              sampleSeq) *
            (0.85f * step);
        event.values[axis] = quantizeStep(white, step);
      }
    }
    break;
  }
  case 9:
    event.values[0] = cal.gravityBase[0] + wave * 0.004f;
    event.values[1] = cal.gravityBase[1] - wave * 0.003f;
    event.values[2] = cal.gravityBase[2];
    break;
  case 10:
    event.values[0] = wave * 0.022f;
    event.values[1] = -wave * 0.018f;
    event.values[2] = wave * 0.014f;
    break;
  default:
    break;
  }
  event.values[3] = 3.0f;
}

void fillScalar(VirtualSensorEvent &event) {
  const float wave = motionWave(event.timestampNs, 1.0f);
  switch (event.sensorType) {
  case 5:
    event.values[0] = 120.0f + wave * 2.0f;
    break;
  case 6:
    event.values[0] = 1008.25f + wave * 0.08f;
    break;
  case 8:
    event.values[0] = 5.0f;
    break;
  case 27:
    event.values[0] = 0.0f;
    break;
  default:
    event.values[0] = 0.0f;
    break;
  }
}

void fillData(VirtualSensorEvent &event,
              const SensorEventModel::InstanceCalibration &cal) {
  const float wave = motionWave(event.timestampNs, 1.0f);
  if (event.sensorType == 5) {
    event.values[0] = 120.0f + wave * 2.0f;
    return;
  }
  if (event.sensorType == 8) {
    event.values[0] = 5.0f;
    return;
  }
  if (event.sensorType == 11 || event.sensorType == 20) {
    event.values[0] = cal.rotQuat[0] + wave * 0.0008f;
    event.values[1] = cal.rotQuat[1] - wave * 0.0008f;
    event.values[2] = cal.rotQuat[2];
    event.values[3] = cal.rotQuat[3];
    event.values[4] = 0.01f;
    return;
  }
  switch (event.sensorHandle) {
  case 23:
    event.values[0] = 31.25f + wave * 0.08f;
    break;
  case 24:
    event.values[0] = 34.5f + wave * 0.08f;
    break;
  case 29:
  case 31:
  case 35:
    event.values[0] = 120.0f + wave * 2.0f;
    break;
  default:
    event.values[0] = 0.0f;
    break;
  }
}

} // namespace

SensorEventModel::InstanceCalibration
SensorEventModel::makeCalibration(uint64_t instanceSeed) {
  InstanceCalibration c;
  c.seed = instanceSeed;
  if (instanceSeed == 0) {
    return c;
  }
  uint64_t st = instanceSeed;
  // 1. 局部重力模长 ~9.80665 ± 0.0055 m/s^2，桌面微倾角分正负象限且避开死零点
  const double gNorm = 9.80665 + uniformRange(&st, -0.0055, 0.0055);
  const double signX = (splitMix64(&st) & 1ULL) ? 1.0 : -1.0;
  const double signY = (splitMix64(&st) & 1ULL) ? 1.0 : -1.0;
  const double gx = signX * uniformRange(&st, 0.075, 0.295);
  const double gy = signY * uniformRange(&st, 0.095, 0.335);
  const double gz = std::sqrt(std::max(1.0, gNorm * gNorm - gx * gx - gy * gy));
  c.gravityBase[0] = static_cast<float>(gx);
  c.gravityBase[1] = static_cast<float>(gy);
  c.gravityBase[2] = static_cast<float>(gz);

  // 2. 地磁水平方位角随机化（水平模长 21~28 uT，垂直分量 -43~-35 uT）
  const double magH = uniformRange(&st, 21.0, 28.0);
  const double yawRad = uniformRange(&st, 0.0, kTwoPi);
  const double mz = uniformRange(&st, -43.5, -35.0);
  c.magBase[0] = static_cast<float>(magH * std::cos(yawRad));
  c.magBase[1] = static_cast<float>(magH * std::sin(yawRad));
  c.magBase[2] = static_cast<float>(mz);

  // 3. 与重力/地磁几何自洽的欧拉角与旋转四元数
  const double pitchRad = std::atan2(-gy, gz);
  const double rollRad = std::atan2(gx, gz);
  double azimDeg = std::fmod((yawRad * 180.0 / kPi) + 360.0, 360.0);
  c.orientBase[0] = static_cast<float>(azimDeg);
  c.orientBase[1] = static_cast<float>(pitchRad * 180.0 / kPi);
  c.orientBase[2] = static_cast<float>(rollRad * 180.0 / kPi);

  const double cy = std::cos(yawRad * 0.5);
  const double sy = std::sin(yawRad * 0.5);
  const double cp = std::cos(pitchRad * 0.5);
  const double sp = std::sin(pitchRad * 0.5);
  const double cr = std::cos(rollRad * 0.5);
  const double sr = std::sin(rollRad * 0.5);
  c.rotQuat[0] = static_cast<float>(sr * cp * cy - cr * sp * sy);
  c.rotQuat[1] = static_cast<float>(cr * sp * cy + sr * cp * sy);
  c.rotQuat[2] = static_cast<float>(cr * cp * sy - sr * sp * cy);
  c.rotQuat[3] = static_cast<float>(cr * cp * cy + sr * sp * sy);

  // 4. 单机专属 UNCALI_GYRO 出厂零偏（±0.0008..0.0055 rad/s，避开全零）
  for (int axis = 0; axis < 3; ++axis) {
    const double sgn = (splitMix64(&st) & 1ULL) ? 1.0 : -1.0;
    c.gyroUncalBias[axis] =
        static_cast<float>(sgn * uniformRange(&st, 0.00085, 0.00550));
  }
  return c;
}

SensorEventModel::SensorEventModel(std::vector<SensorEventSpec> specs,
                                   uint64_t instanceSeed)
    : cal_(makeCalibration(instanceSeed)) {
  handles_.reserve(specs.size());
  states_.reserve(specs.size());
  for (const auto &spec : specs) {
    State state;
    state.spec = spec;
    state.samplingPeriodNs = defaultSamplingPeriodNs(spec);
    if (states_.emplace(spec.handle, state).second) {
      handles_.push_back(spec.handle);
    }
  }
}

bool SensorEventModel::setActive(int32_t sensorHandle, bool enabled,
                                 int64_t nowNs) {
  auto found = states_.find(sensorHandle);
  if (found == states_.end()) {
    return false;
  }
  State &state = found->second;
  if (state.active == enabled) {
    return true;
  }
  state.active = enabled;
  state.initialSamplePublished = false;
  state.nextSampleNs = enabled ? nowNs : 0;
  return true;
}

bool SensorEventModel::setSamplingPeriod(int32_t sensorHandle,
                                         int64_t samplingPeriodNs) {
  auto found = states_.find(sensorHandle);
  if (found == states_.end() || samplingPeriodNs < 0) {
    return false;
  }
  const int64_t minimum = defaultSamplingPeriodNs(found->second.spec);
  found->second.samplingPeriodNs = std::max(samplingPeriodNs, minimum);
  return true;
}

int64_t SensorEventModel::samplingPeriodNs(int32_t sensorHandle) const {
  const auto found = states_.find(sensorHandle);
  return found == states_.end() ? -1 : found->second.samplingPeriodNs;
}

void SensorEventModel::disableAll() {
  for (auto &[handle, state] : states_) {
    static_cast<void>(handle);
    state.active = false;
    state.initialSamplePublished = false;
    state.nextSampleNs = 0;
  }
}

int64_t SensorEventModel::nextWaitNs(int64_t nowNs) const {
  constexpr int64_t kMaxIdleWaitNs = 5'000'000;
  int64_t minNext = std::numeric_limits<int64_t>::max();
  for (const int32_t handle : handles_) {
    const State &state = states_.at(handle);
    if (!state.active || state.spec.reportingMode == 2 ||
        state.spec.reportingMode == 3) {
      continue;
    }
    if (state.spec.reportingMode == 1 && state.initialSamplePublished) {
      continue;
    }
    if (state.nextSampleNs < minNext) {
      minNext = state.nextSampleNs;
    }
  }
  if (minNext == std::numeric_limits<int64_t>::max()) {
    return kMaxIdleWaitNs;
  }
  if (minNext <= nowNs) {
    return 0;
  }
  return std::min(minNext - nowNs, kMaxIdleWaitNs);
}

std::vector<VirtualSensorEvent>
SensorEventModel::collectDue(int64_t nowNs) {
  std::vector<VirtualSensorEvent> events;
  events.reserve(handles_.size());
  for (const int32_t handle : handles_) {
    State &state = states_.at(handle);
    if (!state.active || state.spec.reportingMode == 2 ||
        state.spec.reportingMode == 3 || nowNs < state.nextSampleNs) {
      continue;
    }
    if (state.spec.reportingMode == 1 && state.initialSamplePublished) {
      continue;
    }
    events.push_back(buildEvent(state.spec, nowNs, state.sampleSeq++));
    state.initialSamplePublished = true;
    if (state.spec.reportingMode == 0) {
      state.nextSampleNs += state.samplingPeriodNs;
      if (state.nextSampleNs <= nowNs) {
        const int64_t lag = nowNs - state.nextSampleNs;
        const int64_t skipped = lag / state.samplingPeriodNs + 1;
        state.nextSampleNs += skipped * state.samplingPeriodNs;
      }
    } else {
      state.nextSampleNs = std::numeric_limits<int64_t>::max();
    }
  }
  return events;
}

int64_t
SensorEventModel::defaultSamplingPeriodNs(const SensorEventSpec &spec) {
  if (spec.minDelayUs <= 0) {
    return kDefaultSamplingPeriodNs;
  }
  return static_cast<int64_t>(spec.minDelayUs) * kNanosecondsPerMicrosecond;
}

VirtualSensorEvent
SensorEventModel::buildEvent(const SensorEventSpec &spec, int64_t nowNs,
                             uint64_t sampleSeq) const {
  VirtualSensorEvent event;
  event.timestampNs = nowNs;
  event.sensorHandle = spec.handle;
  event.sensorType = spec.type;
  event.payloadKind =
      spec.hasPayloadOverride ? spec.payloadKind : payloadKindForType(spec.type);
  switch (event.payloadKind) {
  case EventPayloadKind::Vec3:
    fillVec3(event, spec.resolution, cal_, sampleSeq);
    break;
  case EventPayloadKind::Vec4: {
    const float wave = motionWave(nowNs, 0.0008f);
    event.values[0] = cal_.rotQuat[0] + wave;
    event.values[1] = cal_.rotQuat[1] - wave;
    event.values[2] = cal_.rotQuat[2];
    event.values[3] = cal_.rotQuat[3];
    break;
  }
  case EventPayloadKind::Uncal:
    fillVec3(event, spec.resolution, cal_, sampleSeq);
    if (spec.type == 16) {
      const float biasStep =
          spec.resolution > 1.0e-7f ? (spec.resolution / 8.0f) : 0.0001375f;
      if (cal_.seed == 0) {
        event.values[3] = quantizeStep(
            cal_.gyroUncalBias[0] + motionWavePhase(nowNs, 2.2f * biasStep, 0.5),
            biasStep);
        event.values[4] = quantizeStep(
            cal_.gyroUncalBias[1] + motionWavePhase(nowNs, 2.2f * biasStep, 1.8),
            biasStep);
        event.values[5] = quantizeStep(
            cal_.gyroUncalBias[2] + motionWavePhase(nowNs, 2.2f * biasStep, 3.1),
            biasStep);
      } else {
        const double sec = static_cast<double>(nowNs) * 1.0e-9;
        for (int axis = 0; axis < 3; ++axis) {
          // 慢变热温漂游走 + 零偏量化，并且令未校准测量值 = 已校准值 + 零偏
          const float drift =
              static_cast<float>(std::sin(
                  sec * (0.29 + 0.09 * axis) +
                  static_cast<double>((cal_.seed >> (24 + axis * 7)) & 0x7fU) *
                      0.05)) *
              (2.4f * biasStep);
          const float jitter =
              statelessGaussian(cal_.seed, 0x600U + static_cast<uint64_t>(axis),
                                sampleSeq) *
              (0.9f * biasStep);
          const float b =
              quantizeStep(cal_.gyroUncalBias[axis] + drift + jitter, biasStep);
          event.values[3 + axis] = b;
          event.values[axis] = quantizeStep(event.values[axis] + b, biasStep);
        }
      }
    } else {
      event.values[3] = 0.0f;
      event.values[4] = 0.0f;
      event.values[5] = 0.0f;
    }
    break;
  case EventPayloadKind::Scalar:
    fillScalar(event);
    break;
  case EventPayloadKind::StepCount:
    event.stepCount = 0;
    break;
  case EventPayloadKind::Data:
    fillData(event, cal_);
    break;
  }
  return event;
}

} // namespace cdh::sensors
