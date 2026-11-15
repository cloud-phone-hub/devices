// engine/vendor/hardware 线 —— android.hardware.thermal（IThermal v2）自研服务。
//
// 注册 android.hardware.thermal.IThermal/default，供 framework ThermalManagerService
// 使用。底座无 thermal HAL 时接管：上报温度传感器/阈值/冷却设备，并按 /proc/stat 的
// CPU 负载对 SKIN 温度施加有界一阶升/降温模型（thermal_model）。
//
// 机型差异（传感器清单、阈值、冷却设备、模型参数）外置到运行时配置
// /vendor/etc/thermal_config.xml（env THERMAL_CONFIG_XML 覆盖），由设备集成层随镜像下发；
// 本引擎不内置机型数值。读不到即回退保守默认（空传感器清单、模型基线不上报分级）。
// 配置格式：
//   <thermal-config skin-name="SKIN" skin-celsius="27.8" model-maximum-celsius="50"
//                   model-heating-seconds="20" model-cooling-seconds="45"
//                   model-sample-interval-ms="1000">
//     <sensor id="0" name="CPU" value="29.1"
//             thresholds="nan;nan;nan;85;90;100;117"
//             shutdown="nan;nan;nan;nan;nan;nan;nan"/>
//     <cooling id="3" name="devfreq-..." value="0"/>
//   </thermal-config>
// thresholds / shutdown 数组按 ThrottlingSeverity 顺序（NONE..SHUTDOWN），未设置用 nan。
//
// 闭包最小：只用 std + libxml2（按 staticLibs 静态编入），-fno-exceptions（strtof/strtol，不用 stoi）。

#include <android/binder_ibinder.h>
#include <android/binder_status.h>

#include <cstdint>

extern "C" {
void ABinderProcess_setThreadPoolMaxThreadCount(uint32_t numThreads);
void ABinderProcess_joinThreadPool(void);
binder_status_t AServiceManager_addService(AIBinder *binder,
                                           const char *instance);
}

#include <aidl/android/hardware/thermal/BnThermal.h>
#include <aidl/android/hardware/thermal/CoolingDevice.h>
#include <aidl/android/hardware/thermal/CoolingType.h>
#include <aidl/android/hardware/thermal/ICoolingDeviceChangedCallback.h>
#include <aidl/android/hardware/thermal/IThermal.h>
#include <aidl/android/hardware/thermal/IThermalChangedCallback.h>
#include <aidl/android/hardware/thermal/Temperature.h>
#include <aidl/android/hardware/thermal/TemperatureThreshold.h>
#include <aidl/android/hardware/thermal/TemperatureType.h>
#include <aidl/android/hardware/thermal/ThrottlingSeverity.h>

#include "thermal_model.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <libxml/parser.h>
#include <libxml/tree.h>

// ---- libxml2 读取便利层（本模块自用，不进公共头）----
// 读整个文件成 DOM；失败返回 nullptr（调用方回退内置默认）。
static xmlDocPtr loadXmlDoc(const std::string &path) {
  return xmlReadFile(path.c_str(), nullptr,
                     XML_PARSE_NONET | XML_PARSE_NOBLANKS);
}

// 取属性值；缺失返回 false（值拷进 std::string，属性内存即时释放）。
static bool readProp(const xmlNode *node, const char *name, std::string *out) {
  if (node == nullptr) return false;
  xmlChar *v = xmlGetProp(const_cast<xmlNode *>(node), BAD_CAST name);
  if (v == nullptr) return false;
  out->assign(reinterpret_cast<const char *>(v));
  xmlFree(v);
  return true;
}

// 第一个同名子元素（之后用 nextNamed 顺序遍历）。
static xmlNode *firstChild(xmlNode *parent, const char *name) {
  if (parent == nullptr) return nullptr;
  for (xmlNode *c = parent->children; c != nullptr; c = c->next) {
    if (c->type == XML_ELEMENT_NODE && xmlStrcmp(c->name, BAD_CAST name) == 0) {
      return c;
    }
  }
  return nullptr;
}

// 下一个同名兄弟元素（跳过文本/注释与异名元素）。
static xmlNode *nextNamed(xmlNode *node, const char *name) {
  if (node == nullptr) return nullptr;
  for (xmlNode *c = node->next; c != nullptr; c = c->next) {
    if (c->type == XML_ELEMENT_NODE && xmlStrcmp(c->name, BAD_CAST name) == 0) {
      return c;
    }
  }
  return nullptr;
}

using aidl::android::hardware::thermal::BnThermal;
using aidl::android::hardware::thermal::CoolingDevice;
using aidl::android::hardware::thermal::CoolingType;
using aidl::android::hardware::thermal::ICoolingDeviceChangedCallback;
using aidl::android::hardware::thermal::IThermalChangedCallback;
using aidl::android::hardware::thermal::Temperature;
using aidl::android::hardware::thermal::TemperatureThreshold;
using aidl::android::hardware::thermal::TemperatureType;
using aidl::android::hardware::thermal::ThrottlingSeverity;
using ndk::ScopedAStatus;

static constexpr const char *kThermalServiceName =
    "android.hardware.thermal.IThermal/default";
static constexpr const char *kDefaultXmlPath = "/vendor/etc/thermal_config.xml";

static float nanValue() { return std::numeric_limits<float>::quiet_NaN(); }

static bool isTemperatureType(TemperatureType value, TemperatureType want) {
  return value == want;
}

static ThrottlingSeverity throttlingStatus(cdh::thermal::Level level) {
  switch (level) {
  case cdh::thermal::Level::None:
    return ThrottlingSeverity::NONE;
  case cdh::thermal::Level::Light:
    return ThrottlingSeverity::LIGHT;
  case cdh::thermal::Level::Moderate:
    return ThrottlingSeverity::MODERATE;
  case cdh::thermal::Level::Severe:
    return ThrottlingSeverity::SEVERE;
  case cdh::thermal::Level::Critical:
    return ThrottlingSeverity::CRITICAL;
  case cdh::thermal::Level::Emergency:
    return ThrottlingSeverity::EMERGENCY;
  case cdh::thermal::Level::Shutdown:
    return ThrottlingSeverity::SHUTDOWN;
  }
  return ThrottlingSeverity::NONE;
}

static Temperature makeTemperature(const char *name, int32_t type,
                                   float value) {
  Temperature temperature;
  temperature.type = static_cast<TemperatureType>(type);
  temperature.name = name;
  temperature.value = value;
  temperature.throttlingStatus = ThrottlingSeverity::NONE;
  return temperature;
}

static TemperatureThreshold makeThreshold(const char *name, int32_t type,
                                          std::vector<float> hot,
                                          std::vector<float> cold) {
  TemperatureThreshold threshold;
  threshold.type = static_cast<TemperatureType>(type);
  threshold.name = name;
  threshold.hotThrottlingThresholds = std::move(hot);
  threshold.coldThrottlingThresholds = std::move(cold);
  return threshold;
}

static CoolingDevice makeCoolingDevice(const char *name, int32_t type,
                                       int64_t value) {
  CoolingDevice device;
  device.type = static_cast<CoolingType>(type);
  device.name = name;
  device.value = value;
  return device;
}

class CPULoadReader {
public:
  bool Read(float *out) {
    std::FILE *input = std::fopen("/proc/stat", "r");
    if (input == nullptr) {
      return false;
    }
    char label[8]{};
    unsigned long long values[10]{};
    const int count = std::fscanf(
        input, "%7s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu", label,
        &values[0], &values[1], &values[2], &values[3], &values[4], &values[5],
        &values[6], &values[7], &values[8], &values[9]);
    std::fclose(input);
    if (count < 5 || std::strcmp(label, "cpu") != 0) {
      return false;
    }
    const int valueCount = count - 1;
    uint64_t total = 0;
    for (int index = 0; index < valueCount; ++index) {
      total += values[index];
    }
    uint64_t idle = values[3];
    if (valueCount > 4) {
      idle += values[4];
    }
    if (!initialized_) {
      previousTotal_ = total;
      previousIdle_ = idle;
      initialized_ = true;
      return false;
    }
    if (total <= previousTotal_ || idle < previousIdle_) {
      previousTotal_ = total;
      previousIdle_ = idle;
      return false;
    }
    const uint64_t totalDelta = total - previousTotal_;
    const uint64_t idleDelta = std::min(idle - previousIdle_, totalDelta);
    previousTotal_ = total;
    previousIdle_ = idle;
    *out = static_cast<float>(totalDelta - idleDelta) /
           static_cast<float>(totalDelta);
    return std::isfinite(*out);
  }

private:
  bool initialized_ = false;
  uint64_t previousTotal_ = 0;
  uint64_t previousIdle_ = 0;
};

namespace {

std::string trim(const std::string &s) {
  size_t b = 0, e = s.size();
  while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
  while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
  return s.substr(b, e - b);
}

std::vector<std::string> split(const std::string &s, char delim) {
  std::vector<std::string> out;
  size_t start = 0;
  while (true) {
    size_t pos = s.find(delim, start);
    if (pos == std::string::npos) {
      out.push_back(s.substr(start));
      break;
    }
    out.push_back(s.substr(start, pos - start));
    start = pos + 1;
  }
  return out;
}

bool parseFloat(const std::string &raw, float *out) {
  std::string v = trim(raw);
  if (v == "nan" || v == "NaN" || v == "null") {
    *out = nanValue();
    return true;
  }
  const char *c = v.c_str();
  char *end = nullptr;
  float x = std::strtof(c, &end);
  if (end == c || *end != '\0') {
    return false;
  }
  *out = x;
  return true;
}

bool parseInt(const std::string &raw, long *out) {
  std::string v = trim(raw);
  const char *c = v.c_str();
  char *end = nullptr;
  long x = std::strtol(c, &end, 10);
  if (end == c || *end != '\0') {
    return false;
  }
  *out = x;
  return true;
}

// 阈值数组：按 ';' 切段，逐段 parseFloat（失败或 nan 保序写入 NaN）。
std::vector<float> parseFloatList(const std::string &s) {
  std::vector<float> out;
  for (const auto &tok : split(s, ';')) {
    float f = nanValue();
    parseFloat(tok, &f);
    out.push_back(f);
  }
  return out;
}

struct SensorRaw {
  int32_t type = 0;
  std::string name;
  float value = 0.0f;
  std::vector<float> hot;
  std::vector<float> cold;
};

struct CoolingRaw {
  int32_t type = 0;
  std::string name;
  int64_t value = 0;
};

struct ThermalConfig {
  std::string skinName = "SKIN";
  float skinBaseline = 27.8f;
  float maximumCelsius = 50.0f;
  float heatingSeconds = 20.0f;
  float coolingSeconds = 45.0f;
  int64_t sampleIntervalMs = 1000;
  std::vector<SensorRaw> sensors;
  std::vector<CoolingRaw> coolings;
};

// 未提供 /vendor/etc/thermal_config.xml 或未配置 <sensor> 时，回退基础四温区（CPU/GPU/BATTERY/SKIN），
// 防止 OEM 系统服务（如华硕 FocusAppListener / GameGenie）直接取 getDeviceTemperatures(SKIN, CURRENT)[0]
// 触发 ArrayIndexOutOfBoundsException: length=0; index=0。
void ensureDefaultSensors(ThermalConfig *cfg) {
  if (!cfg->sensors.empty()) {
    return;
  }
  const float nan = nanValue();
  const std::vector<float> cold7(7, nan);
  cfg->sensors.push_back(SensorRaw{
      0, "CPU", 29.1f, {nan, nan, nan, 85.0f, 90.0f, 100.0f, 117.0f}, cold7});
  cfg->sensors.push_back(SensorRaw{
      1, "GPU", 28.5f, {nan, nan, nan, 85.0f, 90.0f, 100.0f, 117.0f}, cold7});
  cfg->sensors.push_back(SensorRaw{
      2, "BATTERY", 26.0f, {nan, nan, nan, 50.0f, 55.0f, 59.0f, 60.0f}, cold7});
  cfg->sensors.push_back(SensorRaw{
      3,
      cfg->skinName.empty() ? "SKIN" : cfg->skinName,
      cfg->skinBaseline,
      {nan, 43.0f, 45.0f, 50.0f, 70.0f, 80.0f, 90.0f},
      cold7});
}

ThermalConfig loadConfig(const std::string &path) {
  ThermalConfig cfg;
  xmlDocPtr doc = loadXmlDoc(path);
  if (doc == nullptr) {
    ensureDefaultSensors(&cfg);
    return cfg;  // 读不到回退保守默认
  }
  xmlNode *root = xmlDocGetRootElement(doc);
  if (root == nullptr) {
    xmlFreeDoc(doc);
    ensureDefaultSensors(&cfg);
    return cfg;
  }

  std::string v;
  float f = 0.0f;
  long n = 0;
  if (readProp(root, "skin-name", &v)) cfg.skinName = v;
  if (readProp(root, "skin-celsius", &v) && parseFloat(v, &f)) {
    cfg.skinBaseline = f;
  }
  if (readProp(root, "model-maximum-celsius", &v) && parseFloat(v, &f)) {
    cfg.maximumCelsius = f;
  }
  if (readProp(root, "model-heating-seconds", &v) && parseFloat(v, &f)) {
    cfg.heatingSeconds = f;
  }
  if (readProp(root, "model-cooling-seconds", &v) && parseFloat(v, &f)) {
    cfg.coolingSeconds = f;
  }
  if (readProp(root, "model-sample-interval-ms", &v) && parseInt(v, &n)) {
    cfg.sampleIntervalMs = n;
  }

  for (xmlNode *e = firstChild(root, "sensor"); e != nullptr;
       e = nextNamed(e, "sensor")) {
    SensorRaw s;
    if (readProp(e, "id", &v) && parseInt(v, &n)) {
      s.type = static_cast<int32_t>(n);
    }
    if (readProp(e, "name", &v)) s.name = v;
    if (readProp(e, "value", &v) && parseFloat(v, &f)) {
      s.value = f;
    }
    if (readProp(e, "thresholds", &v)) {
      s.hot = parseFloatList(v);
    }
    if (readProp(e, "shutdown", &v)) {
      s.cold = parseFloatList(v);
    }
    cfg.sensors.push_back(std::move(s));
  }

  for (xmlNode *e = firstChild(root, "cooling"); e != nullptr;
       e = nextNamed(e, "cooling")) {
    CoolingRaw c;
    if (readProp(e, "id", &v) && parseInt(v, &n)) {
      c.type = static_cast<int32_t>(n);
    }
    if (readProp(e, "name", &v)) c.name = v;
    if (readProp(e, "value", &v) && parseInt(v, &n)) {
      c.value = n;
    }
    cfg.coolings.push_back(std::move(c));
  }

  xmlFreeDoc(doc);
  ensureDefaultSensors(&cfg);
  return cfg;
}

std::string cfgPath() {
  const char *env = std::getenv("THERMAL_CONFIG_XML");
  if (env != nullptr && env[0] != '\0') return env;
  return kDefaultXmlPath;
}

cdh::thermal::ModelConfig buildModelConfig(const ThermalConfig &cfg) {
  cdh::thermal::ModelConfig m;
  m.baselineCelsius = cfg.skinBaseline;
  m.maximumCelsius = cfg.maximumCelsius;
  m.heatingTimeSeconds = cfg.heatingSeconds;
  m.coolingTimeSeconds = cfg.coolingSeconds;
  for (float &slot : m.hotThresholds) slot = nanValue();
  for (const auto &s : cfg.sensors) {
    if (s.name == cfg.skinName) {
      // 模型分级阈值取 skin 的 hot[1..6]（跳过 NONE 档），与设备树生成器一致
      for (int i = 0; i < 6; ++i) {
        size_t idx = static_cast<size_t>(i) + 1;
        m.hotThresholds[static_cast<size_t>(i)] =
            (idx < s.hot.size()) ? s.hot[idx] : nanValue();
      }
      break;
    }
  }
  return m;
}

std::vector<Temperature> toTemperatures(const ThermalConfig &cfg) {
  std::vector<Temperature> out;
  for (const auto &s : cfg.sensors) {
    out.push_back(makeTemperature(s.name.c_str(), s.type, s.value));
  }
  return out;
}

std::vector<TemperatureThreshold> toThresholds(const ThermalConfig &cfg) {
  std::vector<TemperatureThreshold> out;
  for (const auto &s : cfg.sensors) {
    out.push_back(
        makeThreshold(s.name.c_str(), s.type, s.hot, s.cold));
  }
  return out;
}

std::vector<CoolingDevice> toCoolingDevices(const ThermalConfig &cfg) {
  std::vector<CoolingDevice> out;
  for (const auto &c : cfg.coolings) {
    out.push_back(makeCoolingDevice(c.name.c_str(), c.type, c.value));
  }
  return out;
}

}  // namespace

class TargetThermal final : public BnThermal {
public:
  explicit TargetThermal(ThermalConfig cfg)
      : cfg_(std::move(cfg)),
        baseTemperatures_(toTemperatures(cfg_)),
        thresholds_(toThresholds(cfg_)),
        coolingDevices_(toCoolingDevices(cfg_)),
        model_(buildModelConfig(cfg_)),
        sampler_(&TargetThermal::RunModel, this) {}

  ~TargetThermal() override {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      stopped_ = true;
    }
    wake_.notify_all();
    if (sampler_.joinable()) {
      sampler_.join();
    }
  }

  ScopedAStatus getCoolingDevices(std::vector<CoolingDevice> *out) override {
    *out = coolingDevices_;
    return ScopedAStatus::ok();
  }

  ScopedAStatus
  getCoolingDevicesWithType(CoolingType type,
                            std::vector<CoolingDevice> *out) override {
    out->clear();
    for (const auto &device : coolingDevices_) {
      if (device.type == type) {
        out->push_back(device);
      }
    }
    return ScopedAStatus::ok();
  }

  ScopedAStatus getTemperatures(std::vector<Temperature> *out) override {
    *out = CurrentTemperatures();
    return ScopedAStatus::ok();
  }

  ScopedAStatus
  getTemperaturesWithType(TemperatureType type,
                          std::vector<Temperature> *out) override {
    out->clear();
    for (const auto &temperature : CurrentTemperatures()) {
      if (isTemperatureType(temperature.type, type)) {
        out->push_back(temperature);
      }
    }
    return ScopedAStatus::ok();
  }

  ScopedAStatus
  getTemperatureThresholds(std::vector<TemperatureThreshold> *out) override {
    *out = thresholds_;
    return ScopedAStatus::ok();
  }

  ScopedAStatus getTemperatureThresholdsWithType(
      TemperatureType type, std::vector<TemperatureThreshold> *out) override {
    out->clear();
    for (const auto &threshold : thresholds_) {
      if (isTemperatureType(threshold.type, type)) {
        out->push_back(threshold);
      }
    }
    return ScopedAStatus::ok();
  }

  ScopedAStatus registerThermalChangedCallback(
      const std::shared_ptr<IThermalChangedCallback> &callback) override {
    return RegisterCallback(callback, false, static_cast<TemperatureType>(0));
  }

  ScopedAStatus registerThermalChangedCallbackWithType(
      const std::shared_ptr<IThermalChangedCallback> &callback,
      TemperatureType type) override {
    return RegisterCallback(callback, true, type);
  }

  ScopedAStatus unregisterThermalChangedCallback(
      const std::shared_ptr<IThermalChangedCallback> &callback) override {
    std::lock_guard<std::mutex> lock(mutex_);
    callbacks_.erase(std::remove_if(callbacks_.begin(), callbacks_.end(),
                                    [&callback](const Callback &entry) {
                                      return SameCallback(entry.callback,
                                                          callback);
                                    }),
                     callbacks_.end());
    return ScopedAStatus::ok();
  }

  ScopedAStatus registerCoolingDeviceChangedCallbackWithType(
      const std::shared_ptr<ICoolingDeviceChangedCallback> &,
      CoolingType) override {
    return ScopedAStatus::ok();
  }

  ScopedAStatus unregisterCoolingDeviceChangedCallback(
      const std::shared_ptr<ICoolingDeviceChangedCallback> &) override {
    return ScopedAStatus::ok();
  }

#if CDH_THERMAL_AIDL_VERSION >= 3
  ScopedAStatus forecastSkinTemperature(int32_t, float *out) override {
    for (const auto &temperature : CurrentTemperatures()) {
      if (temperature.type == TemperatureType::SKIN) {
        *out = temperature.value;
        return ScopedAStatus::ok();
      }
    }
    *out = nanValue();
    return ScopedAStatus::ok();
  }
#endif

private:
  struct Callback {
    std::shared_ptr<IThermalChangedCallback> callback;
    bool filtered;
    TemperatureType type;
  };

  static bool
  SameCallback(const std::shared_ptr<IThermalChangedCallback> &left,
               const std::shared_ptr<IThermalChangedCallback> &right) {
    if (left == nullptr || right == nullptr) {
      return left == right;
    }
    return left->asBinder().get() == right->asBinder().get();
  }

  std::vector<Temperature>
  MakeTemperatures(const cdh::thermal::Sample &sample) {
    auto temperatures = baseTemperatures_;
    for (auto &temperature : temperatures) {
      if (temperature.type == TemperatureType::SKIN) {
        temperature.value = sample.celsius;
        temperature.throttlingStatus = throttlingStatus(sample.level);
      }
    }
    return temperatures;
  }

  std::vector<Temperature> CurrentTemperatures() {
    std::lock_guard<std::mutex> lock(mutex_);
    return MakeTemperatures(model_.Current());
  }

  ScopedAStatus
  RegisterCallback(const std::shared_ptr<IThermalChangedCallback> &callback,
                   bool filtered, TemperatureType type) {
    if (callback == nullptr) {
      return ScopedAStatus::ok();
    }
    std::vector<Temperature> temperatures;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      callbacks_.push_back(Callback{callback, filtered, type});
      temperatures = MakeTemperatures(model_.Current());
    }
    Notify(Callback{callback, filtered, type}, temperatures);
    return ScopedAStatus::ok();
  }

  static void Notify(const Callback &entry,
                     const std::vector<Temperature> &temperatures) {
    for (const auto &temperature : temperatures) {
      if (!entry.filtered || temperature.type == entry.type) {
        entry.callback->notifyThrottling(temperature);
      }
    }
  }

  void RunModel() {
    CPULoadReader loadReader;
    const auto interval =
        std::chrono::milliseconds(cfg_.sampleIntervalMs > 0
                                      ? cfg_.sampleIntervalMs
                                      : 1000);
    auto previousTime = std::chrono::steady_clock::now();
    while (true) {
      {
        std::unique_lock<std::mutex> lock(mutex_);
        if (wake_.wait_for(lock, interval, [this] { return stopped_; })) {
          return;
        }
      }
      const auto now = std::chrono::steady_clock::now();
      float load = 0.0f;
      if (!loadReader.Read(&load)) {
        previousTime = now;
        continue;
      }
      const float elapsed =
          std::chrono::duration<float>(now - previousTime).count();
      previousTime = now;

      std::vector<Callback> callbacks;
      std::vector<Temperature> temperatures;
      {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto previous = model_.Current();
        const auto current = model_.Update(load, elapsed);
        if (std::fabs(current.celsius - previous.celsius) < 0.001f &&
            current.level == previous.level) {
          continue;
        }
        callbacks = callbacks_;
        temperatures = MakeTemperatures(current);
      }
      for (const auto &callback : callbacks) {
        Notify(callback, temperatures);
      }
    }
  }

  ThermalConfig cfg_;
  std::vector<Temperature> baseTemperatures_;
  std::vector<TemperatureThreshold> thresholds_;
  std::vector<CoolingDevice> coolingDevices_;
  std::mutex mutex_;
  std::condition_variable wake_;
  bool stopped_ = false;
  cdh::thermal::Model model_;
  std::vector<Callback> callbacks_;
  std::thread sampler_;
};

static binder_status_t
registerAsService(const std::shared_ptr<ndk::ICInterface> &service,
                  const char *name) {
  return AServiceManager_addService(service->asBinder().get(), name);
}

int main() {
  ABinderProcess_setThreadPoolMaxThreadCount(2);
  ThermalConfig cfg = loadConfig(cfgPath());
  auto thermal = ndk::SharedRefBase::make<TargetThermal>(std::move(cfg));
  if (registerAsService(thermal, kThermalServiceName) != STATUS_OK) {
    return 2;
  }
  ABinderProcess_joinThreadPool();
  return 0;
}
