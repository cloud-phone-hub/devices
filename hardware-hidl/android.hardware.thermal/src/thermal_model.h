// engine/vendor/hardware 线 —— thermal 温度模型（设备无关的一阶升/降温算法）。
//
// 承自设备树 note13-5G-15/src/thermal_model.h：纯算法，不含任何机型数值。
// 阈值/基线/上界等参数由 thermal_service.cpp 从运行时配置注入 ModelConfig。

#pragma once

#include <array>

namespace cdh::thermal {

enum class Level {
  None = 0,
  Light = 1,
  Moderate = 2,
  Severe = 3,
  Critical = 4,
  Emergency = 5,
  Shutdown = 6,
};

struct ModelConfig {
  float baselineCelsius;
  float maximumCelsius;
  float heatingTimeSeconds;
  float coolingTimeSeconds;
  std::array<float, 6> hotThresholds;
};

struct Sample {
  float celsius;
  Level level;
};

class Model {
public:
  explicit Model(ModelConfig config);

  Sample Current() const;
  Sample Update(float load, float elapsedSeconds);

private:
  Level LevelFor(float celsius) const;

  ModelConfig config_;
  Sample sample_;
};

} // namespace cdh::thermal
