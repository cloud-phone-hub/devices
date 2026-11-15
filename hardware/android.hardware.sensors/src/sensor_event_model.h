// engine/vendor/hardware 线 —— sensors 虚拟事件模型（设备无关）。
//
// 承自设备树 note13-5G-15/src/sensor_event_model.h：按 SensorEventSpec
// （handle/type/reportingMode/minDelay）生成虚拟事件流，纯算法无机型传感器清单。
// 传感器清单由 sensors_service.cpp 从运行时配置注入。

#pragma once

#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace cdh::sensors {

enum class EventPayloadKind {
  Vec3,
  Vec4,
  Uncal,
  Scalar,
  StepCount,
  Data,
};

struct SensorEventSpec {
  int32_t handle;
  int32_t type;
  int32_t reportingMode;
  int32_t minDelayUs;
  bool hasPayloadOverride = false;
  EventPayloadKind payloadKind = EventPayloadKind::Scalar;
  float resolution = 0.0f;
};

struct VirtualSensorEvent {
  int64_t timestampNs = 0;
  int32_t sensorHandle = 0;
  int32_t sensorType = 0;
  EventPayloadKind payloadKind = EventPayloadKind::Scalar;
  std::array<float, 16> values{};
  int64_t stepCount = 0;
};

class SensorEventModel {
public:
  explicit SensorEventModel(std::vector<SensorEventSpec> specs);

  bool setActive(int32_t sensorHandle, bool enabled, int64_t nowNs);
  bool setSamplingPeriod(int32_t sensorHandle, int64_t samplingPeriodNs);
  int64_t samplingPeriodNs(int32_t sensorHandle) const;
  void disableAll();
  int64_t nextWaitNs(int64_t nowNs) const;
  std::vector<VirtualSensorEvent> collectDue(int64_t nowNs);

private:
  struct State {
    SensorEventSpec spec;
    bool active = false;
    bool initialSamplePublished = false;
    int64_t samplingPeriodNs = 0;
    int64_t nextSampleNs = 0;
  };

  static int64_t defaultSamplingPeriodNs(const SensorEventSpec &spec);
  static VirtualSensorEvent buildEvent(const SensorEventSpec &spec,
                                       int64_t nowNs);

  std::vector<int32_t> handles_;
  std::unordered_map<int32_t, State> states_;
};

} // namespace cdh::sensors
