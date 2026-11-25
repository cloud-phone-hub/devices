// 宿主单元测试：验证加速度计静态模长、步进量化、陀螺仪细分步长、50Hz 无漂移调度及多实例种子防聚类特性。
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

void verify_model_stream(uint64_t seed, double *outAccMean, double *outGyroBias) {
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
      SensorEventSpec{.handle = 16,
                      .type = 16,
                      .reportingMode = 0,
                      .minDelayUs = 5000,
                      .resolution = 0.0011f},
  };
  SensorEventModel model(specs, seed);
  constexpr int64_t kStartNs = 10'000'000'000LL;
  constexpr int64_t kPeriodNs = 20'000'000LL;  // 50Hz
  constexpr int64_t kTickNs = 1'000'000LL;
  for (int32_t h : {1, 2, 4, 16}) {
    assert(model.setSamplingPeriod(h, kPeriodNs));
    assert(model.setActive(h, true, kStartNs));
  }

  std::vector<VirtualSensorEvent> accSamples;
  std::vector<VirtualSensorEvent> magSamples;
  std::vector<VirtualSensorEvent> gyroSamples;
  std::vector<VirtualSensorEvent> uncalGyroSamples;
  int64_t t = kStartNs;
  const int64_t kEndNs = kStartNs + 1'500'000'000LL;
  while (t < kEndNs) {
    const auto due = model.collectDue(t);
    for (const auto &ev : due) {
      if (ev.sensorHandle == 1) accSamples.push_back(ev);
      else if (ev.sensorHandle == 2) magSamples.push_back(ev);
      else if (ev.sensorHandle == 4) gyroSamples.push_back(ev);
      else if (ev.sensorHandle == 16) uncalGyroSamples.push_back(ev);
    }
    const int64_t wait = model.nextWaitNs(t);
    assert(wait >= 0 && wait <= 5'000'000LL);
    t += (wait > 0 ? wait : kTickNs);
  }

  assert(accSamples.size() >= 75 && accSamples.size() <= 76);
  assert(magSamples.size() >= 75 && magSamples.size() <= 76);
  assert(gyroSamples.size() >= 75 && gyroSamples.size() <= 76);
  assert(uncalGyroSamples.size() >= 75 && uncalGyroSamples.size() <= 76);

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
  if (seed == 0) {
    assert(meanX > 0.10 && meanY > 0.20);
  } else {
    assert(std::fabs(meanX) > 0.05 && std::fabs(meanY) > 0.07);
  }
  assert(std::fabs(norm - 9.8066) < 0.025);
  assert(minAccDiff > 0.00190f && minAccDiff < 0.00200f);

  double magSum[3] = {0, 0, 0};
  for (const auto &ev : magSamples) {
    for (int axis = 0; axis < 3; ++axis) magSum[axis] += ev.values[axis];
  }
  double magVar[3] = {0, 0, 0};
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
      if (d > 1e-6f && d < minMagDiff) minMagDiff = d;
    }
  }
  for (int axis = 0; axis < 3; ++axis) {
    const double sigma = std::sqrt(magVar[axis] / magN);
    assert(sigma >= 0.30 && sigma <= 0.85);
  }
  assert(minMagDiff > 0.0180f && minMagDiff < 0.0200f);

  float minGyroDiff = 100.0f;
  for (size_t i = 1; i < gyroSamples.size(); ++i) {
    for (int axis = 0; axis < 3; ++axis) {
      const float d = std::fabs(gyroSamples[i].values[axis] -
                                 gyroSamples[i - 1].values[axis]);
      if (d > 1e-7f && d < minGyroDiff) minGyroDiff = d;
    }
  }
  assert(minGyroDiff > 0.00105f && minGyroDiff < 0.00115f);

  if (outAccMean != nullptr) {
    outAccMean[0] = meanX;
    outAccMean[1] = meanY;
    outAccMean[2] = meanZ;
  }
  if (outGyroBias != nullptr) {
    outGyroBias[0] = uncalGyroSamples[0].values[3];
    outGyroBias[1] = uncalGyroSamples[0].values[4];
    outGyroBias[2] = uncalGyroSamples[0].values[5];
  }
}

void test_acc_and_gyro_stats_at_50hz() {
  verify_model_stream(0, nullptr, nullptr);
  std::printf("PASS test_acc_and_gyro_stats_at_50hz (legacy seed=0)\n");
}

void test_multi_instance_anti_clustering() {
  double accA[3]{}, biasA[3]{};
  double accB[3]{}, biasB[3]{};
  double accA2[3]{}, biasA2[3]{};
  verify_model_stream(0x123456789abcdef0ULL, accA, biasA);
  verify_model_stream(0xfedcba9876543211ULL, accB, biasB);
  verify_model_stream(0x123456789abcdef0ULL, accA2, biasA2);

  // 同一种子重启必须完全幂等稳定
  for (int i = 0; i < 3; ++i) {
    assert(std::fabs(accA[i] - accA2[i]) < 1e-9);
    assert(std::fabs(biasA[i] - biasA2[i]) < 1e-9);
  }
  // 不同实例种子必须具有不同的微倾角与陀螺零偏（防跨实例风控聚类）
  const double accDist = std::hypot(accA[0] - accB[0], accA[1] - accB[1]);
  const double biasDist = std::hypot(biasA[0] - biasB[0], biasA[1] - biasB[1]);
  assert(accDist > 0.02);
  assert(biasDist > 0.0005);
  std::printf("PASS test_multi_instance_anti_clustering: accDist=%.4f biasDist=%.6f\n",
              accDist, biasDist);
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
  assert(model.nextWaitNs(18'000'000LL) == 3'000'000LL);
  auto catchup = model.collectDue(65'000'000LL);
  assert(catchup.size() == 1);
  assert(model.nextWaitNs(65'000'000LL) == 5'000'000LL);
  assert(model.nextWaitNs(79'000'000LL) == 2'000'000LL);
  std::printf("PASS test_next_wait_and_catchup\n");
}

}  // namespace

int main() {
  test_acc_and_gyro_stats_at_50hz();
  test_multi_instance_anti_clustering();
  test_next_wait_and_catchup();
  return 0;
}

#endif  // CDH_SENSORS_HOST_TEST
