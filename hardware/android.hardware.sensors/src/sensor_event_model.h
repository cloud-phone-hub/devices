// engine/vendor/hardware 线 —— sensors 虚拟事件模型（设备无关）。
//
// 承自设备树 note13-5G-15/src/sensor_event_model.h：按 SensorEventSpec
// （handle/type/reportingMode/minDelay）生成虚拟事件流，纯算法无机型传感器清单。
// 支持可选的实例确定性种子 instanceSeed：
//   - seed == 0：回退固定基准姿态（兼容历史确定性基线单测）；
//   - seed != 0：按单机种子确定性派生球面守恒微倾角、地磁方位、姿态四元数、
//     未校准陀螺出厂零偏与宽带高斯热噪声，消除多云机实例传感器特征同源聚类。

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
  explicit SensorEventModel(std::vector<SensorEventSpec> specs,
                            uint64_t instanceSeed = 0);

  bool setActive(int32_t sensorHandle, bool enabled, int64_t nowNs);
  bool setSamplingPeriod(int32_t sensorHandle, int64_t samplingPeriodNs);
  int64_t samplingPeriodNs(int32_t sensorHandle) const;
  void disableAll();
  int64_t nextWaitNs(int64_t nowNs) const;
  std::vector<VirtualSensorEvent> collectDue(int64_t nowNs);

  struct InstanceCalibration {
    uint64_t seed = 0;
    float gravityBase[3]{0.142f, 0.268f, 9.80196f};
    float magBase[3]{24.80625f, -4.6875f, -39.4875f};
    float gyroUncalBias[3]{-0.0012375f, 0.0020625f, 0.0048125f};
    float orientBase[3]{181.0f, 0.2f, -0.3f}; // azimuth, pitch, roll (deg)
    float rotQuat[4]{0.0f, 0.0f, 0.0f, 1.0f}; // x, y, z, w
  };

  const InstanceCalibration &calibration() const { return cal_; }

private:
  struct State {
    SensorEventSpec spec;
    bool active = false;
    bool initialSamplePublished = false;
    int64_t samplingPeriodNs = 0;
    int64_t nextSampleNs = 0;
    uint64_t sampleSeq = 0;
  };

  static InstanceCalibration makeCalibration(uint64_t instanceSeed);
  static int64_t defaultSamplingPeriodNs(const SensorEventSpec &spec);
  VirtualSensorEvent buildEvent(const SensorEventSpec &spec, int64_t nowNs,
                                uint64_t sampleSeq) const;

  InstanceCalibration cal_;
  std::vector<int32_t> handles_;
  std::unordered_map<int32_t, State> states_;
};

} // namespace cdh::sensors
