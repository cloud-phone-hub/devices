// engine/vendor/hardware 线 —— thermal 温度模型实现（设备无关）。
//
// 承自设备树 note13-5G-15/src/thermal_model.cpp：一阶 RC 升/降温 + 阈值分级，
// 纯算法无机型数值。闭包最小：只用 std（<algorithm>/<cmath>），-fno-exceptions。

#include "thermal_model.h"

#include <algorithm>
#include <cmath>

namespace cdh::thermal {

Model::Model(ModelConfig config)
    : config_(config), sample_{config.baselineCelsius, Level::None} {
  sample_.level = LevelFor(sample_.celsius);
}

Sample Model::Current() const { return sample_; }

Sample Model::Update(float load, float elapsedSeconds) {
  if (!std::isfinite(load)) {
    load = 0.0f;
  }
  load = std::clamp(load, 0.0f, 1.0f);
  if (!std::isfinite(elapsedSeconds) || elapsedSeconds <= 0.0f) {
    return sample_;
  }
  elapsedSeconds = std::min(elapsedSeconds, 300.0f);
  const float target =
      config_.baselineCelsius +
      (config_.maximumCelsius - config_.baselineCelsius) * load;
  const float timeConstant = target >= sample_.celsius
                                 ? config_.heatingTimeSeconds
                                 : config_.coolingTimeSeconds;
  if (!std::isfinite(timeConstant) || timeConstant <= 0.0f) {
    return sample_;
  }
  const float response = 1.0f - std::exp(-elapsedSeconds / timeConstant);
  sample_.celsius += (target - sample_.celsius) * response;
  sample_.celsius = std::clamp(sample_.celsius, config_.baselineCelsius,
                               config_.maximumCelsius);
  sample_.level = LevelFor(sample_.celsius);
  return sample_;
}

Level Model::LevelFor(float celsius) const {
  for (int index = static_cast<int>(config_.hotThresholds.size()) - 1;
       index >= 0; --index) {
    if (celsius >= config_.hotThresholds[static_cast<size_t>(index)]) {
      return static_cast<Level>(index + 1);
    }
  }
  return Level::None;
}

} // namespace cdh::thermal
