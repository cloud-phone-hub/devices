// 宿主单元测试：验证加速度计静态模长、步进量化、陀螺仪细分步长及 50Hz 无漂移调度。
// 必须由 CDH_SENSORS_HOST_TEST 宏保护，避免 halbuild 扫描 src/*.cpp 时重复编译 main。
#ifdef CDH_SENSORS_HOST_TEST

#include "sensor_event_model.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {

using cdh::sensors::SensorEventModel;
using cdh::sensors::SensorEventSpec;
using cdh::sensors::VirtualSensorEvent;

void test_acc_and_gyro_stats_at_50hz() {
  std::vector<SensorEventSpec> specs{
      SensorEventSpec{.handle = 1,
                      .type = 1,
                      .reportingMode = 0,
                      .minDelayUs = 5000,
                      .resolution = 0.0012f},
      SensorEventSpec{.handle = 2,
                      .type = 2,
                      .reportingMode = 0,
                      .minDelayUs = 5000,
                      .resolution = 0.15f},
      SensorEventSpec{.handle = 4,
                      .type = 4,
                      .reportingMode = 0,
                      .minDelayUs = 5000,
                      .resolution = 0.0011f},
  };
  SensorEventModel model(specs);
  constexpr int64_t kStartNs = 10'000'000'000LL;
  constexpr int64_t kPeriodNs = 20'000'000LL;  // 50Hz
  constexpr int64_t kTickNs = 1'000'000LL;     // 模拟动态 1ms 检查唤醒粒度
  assert(model.setSamplingPeriod(1, kPeriodNs));
  assert(model.setSamplingPeriod(2, kPeriodNs));
  assert(model.setSamplingPeriod(4, kPeriodNs));
  assert(model.setActive(1, true, kStartNs));
  assert(model.setActive(2, true, kStartNs));
  assert(model.setActive(4, true, kStartNs));

  std::vector<VirtualSensorEvent> accSamples;
  std::vector<VirtualSensorEvent> magSamples;
  std::vector<VirtualSensorEvent> gyroSamples;
  int64_t t = kStartNs;
  const int64_t kEndNs = kStartNs + 1'500'000'000LL;  // 1500ms 窗口
  while (t < kEndNs) {
    const auto due = model.collectDue(t);
    for (const auto &ev : due) {
      if (ev.sensorHandle == 1) {
        accSamples.push_back(ev);
      } else if (ev.sensorHandle == 2) {
        magSamples.push_back(ev);
      } else if (ev.sensorHandle == 4) {
        gyroSamples.push_back(ev);
      }
    }
    const int64_t wait = model.nextWaitNs(t);
    assert(wait >= 0 && wait <= 5'000'000LL);
    t += (wait > 0 ? wait : kTickNs);
  }

  assert(accSamples.size() >= 75 && accSamples.size() <= 76);
  assert(magSamples.size() >= 75 && magSamples.size() <= 76);
  assert(gyroSamples.size() >= 75 && gyroSamples.size() <= 76);

  double sumX = 0, sumY = 0, sumZ = 0;
  float minAccDiff = 100.0f;
  for (size_t i = 0; i < accSamples.size(); ++i) {
    sumX += accSamples[i].values[0];
    sumY += accSamples[i].values[1];
    sumZ += accSamples[i].values[2];
    if (i > 0) {
      for (int axis = 0; axis < 3; ++axis) {
        const float d = std::fabs(accSamples[i].values[axis] -
                                   accSamples[i - 1].values[axis]);
        if (d > 1e-6f && d < minAccDiff) {
          minAccDiff = d;
        }
      }
    }
  }
  const double meanX = sumX / static_cast<double>(accSamples.size());
  const double meanY = sumY / static_cast<double>(accSamples.size());
  const double meanZ = sumZ / static_cast<double>(accSamples.size());
  const double norm = std::sqrt(meanX * meanX + meanY * meanY + meanZ * meanZ);
  assert(meanX > 0.10 && meanY > 0.20);
  assert(std::fabs(norm - 9.8066) < 0.02);
  assert(minAccDiff > 0.00190f && minAccDiff < 0.00200f);

  double magSum[3] = {0, 0, 0};
  for (const auto &ev : magSamples) {
    for (int axis = 0; axis < 3; ++axis) {
      magSum[axis] += ev.values[axis];
    }
  }
  double magVar[3] = {0, 0, 0};
  double magSigma[3] = {0, 0, 0};
  const double magN = static_cast<double>(magSamples.size());
  for (const auto &ev : magSamples) {
    for (int axis = 0; axis < 3; ++axis) {
      const double diff = ev.values[axis] - (magSum[axis] / magN);
      magVar[axis] += diff * diff;
    }
  }
  float minMagDiff = 100.0f;
  for (size_t i = 1; i < magSamples.size(); ++i) {
    for (int axis = 0; axis < 3; ++axis) {
      const float d = std::fabs(magSamples[i].values[axis] -
                                 magSamples[i - 1].values[axis]);
      if (d > 1e-6f && d < minMagDiff) {
        minMagDiff = d;
      }
    }
  }
  for (int axis = 0; axis < 3; ++axis) {
    magSigma[axis] = std::sqrt(magVar[axis] / magN);
    assert(magSigma[axis] >= 0.35 && magSigma[axis] <= 0.80);
  }
  assert(minMagDiff > 0.0180f && minMagDiff < 0.0200f);

  float minGyroDiff = 100.0f;
  for (size_t i = 1; i < gyroSamples.size(); ++i) {
    for (int axis = 0; axis < 3; ++axis) {
      const float d = std::fabs(gyroSamples[i].values[axis] -
                                 gyroSamples[i - 1].values[axis]);
      if (d > 1e-7f && d < minGyroDiff) {
        minGyroDiff = d;
      }
    }
  }
  assert(minGyroDiff > 0.000130f && minGyroDiff < 0.000145f);
  std::printf(
      "PASS test_acc_and_gyro_stats_at_50hz: n=%zu norm=%.4f meanXY=(%.3f,%.3f) magSigma=(%.3f,%.3f,%.3f) minAccDiff=%.6f minMagDiff=%.6f minGyroDiff=%.6f\n",
      accSamples.size(), norm, meanX, meanY, magSigma[0], magSigma[1], magSigma[2], minAccDiff, minMagDiff, minGyroDiff);
}

void test_next_wait_and_catchup() {
  SensorEventModel model({SensorEventSpec{.handle = 1,
                                          .type = 1,
                                          .reportingMode = 0,
                                          .minDelayUs = 20000,
                                          .resolution = 0.0012f}});
  assert(model.nextWaitNs(1000) == 5'000'000LL);
  assert(model.setActive(1, true, 1'000'000LL));
  assert(model.nextWaitNs(1'000'000LL) == 0);
  auto first = model.collectDue(1'000'000LL);
  assert(first.size() == 1);
  // 下次在 21'000'000，当前 18'000'000 应返回 3'000'000
  assert(model.nextWaitNs(18'000'000LL) == 3'000'000LL);
  // 滞后超过 2 个周期（到 65'000'000）收集后，下一次应追赶到 81'000'000，相差 16'000'000，限幅为 5ms
  auto catchup = model.collectDue(65'000'000LL);
  assert(catchup.size() == 1);
  assert(model.nextWaitNs(65'000'000LL) == 5'000'000LL);
  assert(model.nextWaitNs(79'000'000LL) == 2'000'000LL);
  std::printf("PASS test_next_wait_and_catchup\n");
}

}  // namespace

int main() {
  test_acc_and_gyro_stats_at_50hz();
  test_next_wait_and_catchup();
  return 0;
}

#endif  // CDH_SENSORS_HOST_TEST
