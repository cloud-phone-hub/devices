// engine/vendor/hardware —— Android 13 HIDL android.hardware.thermal@2.0 自研服务。
//
// 读取 /vendor/etc/thermal_config.xml（缺省内置 CPU/GPU/BATTERY/SKIN 四温区，防止 ASUS 等
// 机型 ThermalManager 在空温区数组下崩溃），并按 /proc/stat CPU 负载更新 SKIN 温度模型。

#define LOG_TAG "CdhThermalHidl"

#include <android/hardware/thermal/2.0/IThermal.h>
#include <android/hardware/thermal/2.0/IThermalChangedCallback.h>
#include <hidl/HidlTransportSupport.h>
#include <log/log.h>

#include "thermal_model.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <mutex>
#include <string>
#include <vector>

namespace {

using ::android::sp;
using ::android::hardware::hidl_array;
using ::android::hardware::hidl_death_recipient;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::thermal::V1_0::CpuUsage;
using ::android::hardware::thermal::V1_0::ThermalStatus;
using ::android::hardware::thermal::V1_0::ThermalStatusCode;
using V1_0_CoolingDevice = ::android::hardware::thermal::V1_0::CoolingDevice;
using V1_0_CoolingType = ::android::hardware::thermal::V1_0::CoolingType;
using V1_0_Temp = ::android::hardware::thermal::V1_0::Temperature;
using V1_0_TempType = ::android::hardware::thermal::V1_0::TemperatureType;
using ::android::hardware::thermal::V2_0::CoolingDevice;
using ::android::hardware::thermal::V2_0::CoolingType;
using ::android::hardware::thermal::V2_0::IThermal;
using ::android::hardware::thermal::V2_0::IThermalChangedCallback;
using ::android::hardware::thermal::V2_0::Temperature;
using ::android::hardware::thermal::V2_0::TemperatureThreshold;
using ::android::hardware::thermal::V2_0::TemperatureType;
using ::android::hardware::thermal::V2_0::ThrottlingSeverity;

float nanVal() { return std::numeric_limits<float>::quiet_NaN(); }

bool readProp(const xmlNode *node, const char *name, std::string *out) {
  if (node == nullptr) return false;
  xmlChar *v = xmlGetProp(const_cast<xmlNode *>(node), BAD_CAST name);
  if (v == nullptr) return false;
  out->assign(reinterpret_cast<const char *>(v));
  xmlFree(v);
  return true;
}

float parseFloatOrNan(const std::string &s) {
  if (s.empty() || s == "nan" || s == "NaN") return nanVal();
  char *end = nullptr;
  float v = std::strtof(s.c_str(), &end);
  return (end == s.c_str()) ? nanVal() : v;
}

std::array<float, 7> parseThresholds(const std::string &spec) {
  std::array<float, 7> out;
  out.fill(nanVal());
  size_t pos = 0;
  size_t idx = 0;
  while (pos <= spec.size() && idx < 7) {
    size_t semi = spec.find(';', pos);
    std::string tok = (semi == std::string::npos) ? spec.substr(pos) : spec.substr(pos, semi - pos);
    out[idx++] = parseFloatOrNan(tok);
    if (semi == std::string::npos) break;
    pos = semi + 1;
  }
  return out;
}

ThrottlingSeverity toSeverity(cdh::thermal::Level lv) {
  switch (lv) {
    case cdh::thermal::Level::None: return ThrottlingSeverity::NONE;
    case cdh::thermal::Level::Light: return ThrottlingSeverity::LIGHT;
    case cdh::thermal::Level::Moderate: return ThrottlingSeverity::MODERATE;
    case cdh::thermal::Level::Severe: return ThrottlingSeverity::SEVERE;
    case cdh::thermal::Level::Critical: return ThrottlingSeverity::CRITICAL;
    case cdh::thermal::Level::Emergency: return ThrottlingSeverity::EMERGENCY;
    case cdh::thermal::Level::Shutdown: return ThrottlingSeverity::SHUTDOWN;
  }
  return ThrottlingSeverity::NONE;
}

struct SensorEntry {
  TemperatureType type;
  std::string name;
  float baseValue;
  std::array<float, 7> hot;
  std::array<float, 7> cold;
};

struct CoolingEntry {
  CoolingType type;
  std::string name;
  uint64_t value;
};

struct CallbackReg {
  sp<IThermalChangedCallback> cb;
  bool filterType;
  TemperatureType type;
};

class ThermalHidlImpl : public IThermal, public hidl_death_recipient {
 public:
  ThermalHidlImpl()
      : model_(cdh::thermal::ModelConfig{28.5f, 48.0f, 20.0f, 45.0f,
                                         {39.0f, 41.0f, 43.0f, 45.0f, 47.0f, 50.0f}}) {
    loadConfig();
  }

  void serviceDied(uint64_t, const ::android::wp<::android::hidl::base::V1_0::IBase> &who) override {
    std::lock_guard<std::mutex> lk(mu_);
    sp<::android::hidl::base::V1_0::IBase> dead = who.promote();
    callbacks_.erase(
        std::remove_if(callbacks_.begin(), callbacks_.end(),
                       [&](const CallbackReg &r) {
                         return r.cb == nullptr ||
                                ::android::hardware::interfacesEqual(r.cb, dead);
                       }),
        callbacks_.end());
  }

  Return<void> getTemperatures(getTemperatures_cb _hidl_cb) override {
    std::vector<V1_0_Temp> out;
    const auto sample = currentSkinSample();
    for (const auto &s : sensors_) {
      const int32_t t = static_cast<int32_t>(s.type);
      if (t < static_cast<int32_t>(V1_0_TempType::UNKNOWN) ||
          t > static_cast<int32_t>(V1_0_TempType::SKIN)) {
        continue;
      }
      V1_0_Temp item{};
      item.type = static_cast<V1_0_TempType>(t);
      item.name = s.name;
      item.currentValue = (s.name == skinName_) ? sample.celsius : s.baseValue;
      item.throttlingThreshold = s.hot[static_cast<size_t>(ThrottlingSeverity::SEVERE)];
      item.shutdownThreshold = s.hot[static_cast<size_t>(ThrottlingSeverity::SHUTDOWN)];
      item.vrThrottlingThreshold = s.hot[static_cast<size_t>(ThrottlingSeverity::SEVERE)];
      out.push_back(std::move(item));
    }
    _hidl_cb(okStatus(), out);
    return Void();
  }

  Return<void> getCpuUsages(getCpuUsages_cb _hidl_cb) override {
    std::vector<CpuUsage> out;
    std::FILE *fp = std::fopen("/proc/stat", "r");
    if (fp != nullptr) {
      char line[256];
      while (std::fgets(line, sizeof(line), fp) != nullptr) {
        if (std::strncmp(line, "cpu", 3) != 0 || line[3] < '0' || line[3] > '9') {
          continue;
        }
        char name[16] = {0};
        unsigned long long user = 0, nice = 0, sys = 0, idle = 0, iowait = 0, irq = 0, softirq = 0;
        if (std::sscanf(line, "%15s %llu %llu %llu %llu %llu %llu %llu",
                        name, &user, &nice, &sys, &idle, &iowait, &irq, &softirq) >= 5) {
          CpuUsage u{};
          u.name = name;
          u.active = user + nice + sys + irq + softirq;
          u.total = u.active + idle + iowait;
          u.isOnline = true;
          out.push_back(std::move(u));
        }
      }
      std::fclose(fp);
    }
    if (out.empty()) {
      out.push_back(CpuUsage{"cpu0", 100, 1000, true});
    }
    _hidl_cb(okStatus(), out);
    return Void();
  }

  Return<void> getCoolingDevices(getCoolingDevices_cb _hidl_cb) override {
    hidl_vec<V1_0_CoolingDevice> empty;
    _hidl_cb(okStatus(), empty);
    return Void();
  }

  Return<void> getCurrentTemperatures(bool filterType, TemperatureType type,
                                      getCurrentTemperatures_cb _hidl_cb) override {
    std::vector<Temperature> out;
    const auto sample = currentSkinSample();
    for (const auto &s : sensors_) {
      if (filterType && s.type != type) continue;
      Temperature item{};
      item.type = s.type;
      item.name = s.name;
      item.value = (s.name == skinName_) ? sample.celsius : s.baseValue;
      item.throttlingStatus = (s.name == skinName_) ? toSeverity(sample.level)
                                                    : ThrottlingSeverity::NONE;
      out.push_back(std::move(item));
    }
    _hidl_cb(okStatus(), out);
    return Void();
  }

  Return<void> getTemperatureThresholds(bool filterType, TemperatureType type,
                                        getTemperatureThresholds_cb _hidl_cb) override {
    std::vector<TemperatureThreshold> out;
    for (const auto &s : sensors_) {
      if (filterType && s.type != type) continue;
      TemperatureThreshold th{};
      th.type = s.type;
      th.name = s.name;
      for (size_t i = 0; i < 7; ++i) {
        th.hotThrottlingThresholds[i] = s.hot[i];
        th.coldThrottlingThresholds[i] = s.cold[i];
      }
      th.vrThrottlingThreshold = s.hot[static_cast<size_t>(ThrottlingSeverity::SEVERE)];
      out.push_back(std::move(th));
    }
    _hidl_cb(okStatus(), out);
    return Void();
  }

  Return<void> registerThermalChangedCallback(
      const sp<IThermalChangedCallback> &callback, bool filterType, TemperatureType type,
      registerThermalChangedCallback_cb _hidl_cb) override {
    if (callback == nullptr) {
      _hidl_cb(ThermalStatus{ThermalStatusCode::FAILURE, "Null callback"});
      return Void();
    }
    std::lock_guard<std::mutex> lk(mu_);
    for (const auto &r : callbacks_) {
      if (::android::hardware::interfacesEqual(r.cb, callback)) {
        _hidl_cb(ThermalStatus{ThermalStatusCode::FAILURE, "Callback already registered"});
        return Void();
      }
    }
    (void)callback->linkToDeath(this, 0);
    callbacks_.push_back(CallbackReg{callback, filterType, type});
    _hidl_cb(okStatus());
    return Void();
  }

  Return<void> unregisterThermalChangedCallback(
      const sp<IThermalChangedCallback> &callback,
      unregisterThermalChangedCallback_cb _hidl_cb) override {
    if (callback == nullptr) {
      _hidl_cb(ThermalStatus{ThermalStatusCode::FAILURE, "Null callback"});
      return Void();
    }
    std::lock_guard<std::mutex> lk(mu_);
    auto it = std::remove_if(callbacks_.begin(), callbacks_.end(),
                             [&](const CallbackReg &r) {
                               return ::android::hardware::interfacesEqual(r.cb, callback);
                             });
    if (it == callbacks_.end()) {
      _hidl_cb(ThermalStatus{ThermalStatusCode::FAILURE, "Callback not registered"});
      return Void();
    }
    for (auto p = it; p != callbacks_.end(); ++p) {
      if (p->cb != nullptr) (void)p->cb->unlinkToDeath(this);
    }
    callbacks_.erase(it, callbacks_.end());
    _hidl_cb(okStatus());
    return Void();
  }

  Return<void> getCurrentCoolingDevices(bool filterType, CoolingType type,
                                        getCurrentCoolingDevices_cb _hidl_cb) override {
    std::vector<CoolingDevice> out;
    for (const auto &c : coolings_) {
      if (filterType && c.type != type) continue;
      out.push_back(CoolingDevice{c.type, c.name, c.value});
    }
    _hidl_cb(okStatus(), out);
    return Void();
  }

 private:
  static ThermalStatus okStatus() {
    return ThermalStatus{ThermalStatusCode::SUCCESS, ""};
  }

  cdh::thermal::Sample currentSkinSample() {
    std::lock_guard<std::mutex> lk(mu_);
    return model_.Current();
  }

  void loadConfig() {
    const char *env = std::getenv("THERMAL_CONFIG_XML");
    const std::string path = (env != nullptr && env[0] != '\0') ? env : "/vendor/etc/thermal_config.xml";
    xmlDocPtr doc = xmlReadFile(path.c_str(), nullptr, XML_PARSE_NONET | XML_PARSE_NOBLANKS);
    if (doc != nullptr) {
      xmlNode *root = xmlDocGetRootElement(doc);
      if (root != nullptr) {
        std::string v;
        if (readProp(root, "skin-name", &v) && !v.empty()) skinName_ = v;
        float skinBase = 28.5f;
        if (readProp(root, "skin-celsius", &v)) {
          float f = parseFloatOrNan(v);
          if (!std::isnan(f)) skinBase = f;
        }
        model_ = cdh::thermal::Model(
            cdh::thermal::ModelConfig{skinBase, 48.0f, 20.0f, 45.0f,
                                      {39.0f, 41.0f, 43.0f, 45.0f, 47.0f, 50.0f}});
        for (xmlNode *c = root->children; c != nullptr; c = c->next) {
          if (c->type != XML_ELEMENT_NODE) continue;
          if (xmlStrcmp(c->name, BAD_CAST "sensor") == 0) {
            SensorEntry s{};
            int32_t id = 0;
            if (readProp(c, "id", &v)) id = static_cast<int32_t>(std::strtol(v.c_str(), nullptr, 10));
            s.type = static_cast<TemperatureType>(id);
            readProp(c, "name", &s.name);
            s.baseValue = 29.0f;
            if (readProp(c, "value", &v)) {
              float f = parseFloatOrNan(v);
              if (!std::isnan(f)) s.baseValue = f;
            }
            s.hot.fill(nanVal());
            s.cold.fill(nanVal());
            if (readProp(c, "thresholds", &v)) {
              s.hot = parseThresholds(v);
              s.cold = s.hot;
            }
            if (!s.name.empty()) sensors_.push_back(std::move(s));
          } else if (xmlStrcmp(c->name, BAD_CAST "cooling") == 0) {
            CoolingEntry ce{};
            int32_t id = 2;
            if (readProp(c, "id", &v)) id = static_cast<int32_t>(std::strtol(v.c_str(), nullptr, 10));
            ce.type = static_cast<CoolingType>(id);
            readProp(c, "name", &ce.name);
            ce.value = 0;
            if (readProp(c, "value", &v)) ce.value = static_cast<uint64_t>(std::strtoull(v.c_str(), nullptr, 10));
            if (!ce.name.empty()) coolings_.push_back(std::move(ce));
          }
        }
      }
      xmlFreeDoc(doc);
    }

    if (sensors_.empty()) {
      auto defHot = parseThresholds("nan;39;42;45;48;52;60");
      sensors_.push_back(SensorEntry{TemperatureType::CPU, "CPU", 31.2f, defHot, defHot});
      sensors_.push_back(SensorEntry{TemperatureType::GPU, "GPU", 30.0f, defHot, defHot});
      sensors_.push_back(SensorEntry{TemperatureType::BATTERY, "battery", 28.5f, defHot, defHot});
      sensors_.push_back(SensorEntry{TemperatureType::SKIN, skinName_, 28.5f, defHot, defHot});
    }
    if (coolings_.empty()) {
      coolings_.push_back(CoolingEntry{CoolingType::CPU, "thermal-cpufreq-0", 0});
    }
  }

  std::mutex mu_;
  std::string skinName_ = "SKIN";
  cdh::thermal::Model model_;
  std::vector<SensorEntry> sensors_;
  std::vector<CoolingEntry> coolings_;
  std::vector<CallbackReg> callbacks_;
};

}  // namespace

int main() {
  ::android::hardware::configureRpcThreadpool(4, true);
  sp<IThermal> svc = new ThermalHidlImpl();
  if (svc->registerAsService("default") != ::android::OK) {
    ALOGE("Failed to register IThermal@2.0/default");
    return 1;
  }
  ALOGI("IThermal@2.0/default service ready");
  ::android::hardware::joinRpcThreadpool();
  return 1;
}
