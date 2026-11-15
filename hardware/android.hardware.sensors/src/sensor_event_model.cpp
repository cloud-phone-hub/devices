// engine/vendor/hardware 线 —— sensors 虚拟事件模型实现（设备无关）。
//
// 承自设备树 note13-5G-15/src/sensor_event_model.cpp：按传感器类型生成波形/标量/
// 计步等虚拟事件，纯算法无机型传感器清单。闭包最小：只用 std，-fno-exceptions。

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

float motionWavePhase(int64_t timestampNs, float scale, double phaseRad = 0.0) {
  constexpr double kTwoPi = 6.283185307179586;
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

void fillVec3(VirtualSensorEvent &event, float resolution) {
  const float wave = motionWave(event.timestampNs, 1.0f);
  switch (event.sensorType) {
  case 1:
  case 35: {
    const float step =
        resolution > 1.0e-7f ? (13.0f * (resolution / 8.0f)) : 0.00195f;
    // 对齐桌面自然镜头凸起微倾角，保持三轴合重力模长为 9.80665 m/s^2
    event.values[0] = quantizeStep(
        0.142f + motionWavePhase(event.timestampNs, 9.5f * step, 0.0), step);
    event.values[1] = quantizeStep(
        0.268f + motionWavePhase(event.timestampNs, 9.5f * step, 1.1), step);
    event.values[2] = quantizeStep(
        9.80196f + motionWavePhase(event.timestampNs, 9.5f * step, 2.3), step);
    break;
  }
  case 2:
  case 14: {
    const float step = resolution > 1.0e-7f ? (resolution / 8.0f) : 0.01875f;
    const double sec = static_cast<double>(event.timestampNs) * 1.0e-9;
    // 双频非公度自然电磁微抖动，使 1.5s 采样窗口各轴标准差落在 0.40~0.65 uT
    const float jx = static_cast<float>(
        0.58 * std::sin(sec * 5.3 + 0.4) + 0.26 * std::sin(sec * 11.7 + 1.1));
    const float jy = static_cast<float>(
        0.64 * std::sin(sec * 4.7 + 1.7) + 0.29 * std::sin(sec * 10.3 + 2.4));
    const float jz = static_cast<float>(
        0.72 * std::sin(sec * 6.1 + 3.0) + 0.31 * std::sin(sec * 13.1 + 0.7));
    event.values[0] = quantizeStep(24.80625f + jx, step);
    event.values[1] = quantizeStep(-4.6875f + jy, step);
    event.values[2] = quantizeStep(-39.4875f + jz, step);
    break;
  }
  case 3:
    event.values[0] = 181.0f + wave * 0.2f;
    event.values[1] = 0.2f + wave * 0.05f;
    event.values[2] = -0.3f + wave * 0.05f;
    break;
  case 4:
  case 16: {
    const float step =
        resolution > 1.0e-7f ? (resolution / 8.0f) : 0.0001375f;
    event.values[0] = quantizeStep(
        motionWavePhase(event.timestampNs, 8.0f * step, 0.2), step);
    event.values[1] = quantizeStep(
        motionWavePhase(event.timestampNs, 8.0f * step, 1.5), step);
    event.values[2] = quantizeStep(
        motionWavePhase(event.timestampNs, 8.0f * step, 2.8), step);
    break;
  }
  case 9:
    event.values[0] = 0.142f + wave * 0.005f;
    event.values[1] = 0.268f - wave * 0.004f;
    event.values[2] = 9.80196f;
    break;
  case 10:
    event.values[0] = wave * 0.025f;
    event.values[1] = -wave * 0.02f;
    event.values[2] = wave * 0.015f;
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

void fillData(VirtualSensorEvent &event) {
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
    event.values[0] = wave * 0.001f;
    event.values[1] = -wave * 0.001f;
    event.values[2] = 0.0f;
    event.values[3] = 1.0f;
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

SensorEventModel::SensorEventModel(std::vector<SensorEventSpec> specs) {
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
    events.push_back(buildEvent(state.spec, nowNs));
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
SensorEventModel::buildEvent(const SensorEventSpec &spec, int64_t nowNs) {
  VirtualSensorEvent event;
  event.timestampNs = nowNs;
  event.sensorHandle = spec.handle;
  event.sensorType = spec.type;
  event.payloadKind =
      spec.hasPayloadOverride ? spec.payloadKind : payloadKindForType(spec.type);
  switch (event.payloadKind) {
  case EventPayloadKind::Vec3:
    fillVec3(event, spec.resolution);
    break;
  case EventPayloadKind::Vec4:
    event.values[0] = motionWave(nowNs, 0.001f);
    event.values[1] = -event.values[0];
    event.values[2] = 0.0f;
    event.values[3] = 1.0f;
    break;
  case EventPayloadKind::Uncal:
    fillVec3(event, spec.resolution);
    if (spec.type == 16) {
      const float biasStep =
          spec.resolution > 1.0e-7f ? (spec.resolution / 8.0f) : 0.0001375f;
      event.values[3] = quantizeStep(
          -0.0012375f + motionWavePhase(nowNs, 2.2f * biasStep, 0.5), biasStep);
      event.values[4] = quantizeStep(
          0.0020625f + motionWavePhase(nowNs, 2.2f * biasStep, 1.8), biasStep);
      event.values[5] = quantizeStep(
          0.0048125f + motionWavePhase(nowNs, 2.2f * biasStep, 3.1), biasStep);
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
    fillData(event);
    break;
  }
  return event;
}

} // namespace cdh::sensors
