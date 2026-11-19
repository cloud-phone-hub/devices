#include <android/binder_ibinder.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <android/binder_status.h>
#include <android/log.h>

#include <aidl/android/hardware/gnss/BnAGnss.h>
#include <aidl/android/hardware/gnss/BnAGnssRil.h>
#include <aidl/android/hardware/gnss/BnGnss.h>
#include <aidl/android/hardware/gnss/BnGnssAntennaInfo.h>
#include <aidl/android/hardware/gnss/BnGnssBatching.h>
#include <aidl/android/hardware/gnss/BnGnssConfiguration.h>
#include <aidl/android/hardware/gnss/BnGnssDebug.h>
#include <aidl/android/hardware/gnss/BnGnssGeofence.h>
#include <aidl/android/hardware/gnss/BnGnssMeasurementInterface.h>
#include <aidl/android/hardware/gnss/BnGnssNavigationMessageInterface.h>
#include <aidl/android/hardware/gnss/BnGnssPowerIndication.h>
#include <aidl/android/hardware/gnss/BnGnssPsds.h>
#include <aidl/android/hardware/gnss/ElapsedRealtime.h>
#include <aidl/android/hardware/gnss/GnssConstellationType.h>
#include <aidl/android/hardware/gnss/GnssLocation.h>
#include <aidl/android/hardware/gnss/GnssPowerStats.h>
#include <aidl/android/hardware/gnss/IAGnssCallback.h>
#include <aidl/android/hardware/gnss/IAGnssRilCallback.h>
#include <aidl/android/hardware/gnss/IGnssBatchingCallback.h>
#include <aidl/android/hardware/gnss/IGnssCallback.h>
#include <aidl/android/hardware/gnss/IGnssGeofenceCallback.h>
#include <aidl/android/hardware/gnss/IGnssNavigationMessageCallback.h>
#include <aidl/android/hardware/gnss/IGnssPowerIndicationCallback.h>
#include <aidl/android/hardware/gnss/measurement_corrections/BnMeasurementCorrectionsInterface.h>
#include <aidl/android/hardware/gnss/measurement_corrections/IMeasurementCorrectionsInterface.h>
#include <aidl/android/hardware/gnss/measurement_corrections/MeasurementCorrections.h>
#include <aidl/android/hardware/gnss/visibility_control/BnGnssVisibilityControl.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <errno.h>
#include <cstdio>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <time.h>
#include <vector>

#include <libxml/parser.h>
#include <libxml/tree.h>

extern "C" {
void AIBinder_markVintfStability(AIBinder *binder);
void AIBinder_Class_disableInterfaceTokenHeader(AIBinder_Class *clazz);
}

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

using aidl::android::hardware::gnss::BnAGnss;
using aidl::android::hardware::gnss::BnAGnssRil;
using aidl::android::hardware::gnss::BnGnss;
using aidl::android::hardware::gnss::BnGnssAntennaInfo;
using aidl::android::hardware::gnss::BnGnssBatching;
using aidl::android::hardware::gnss::BnGnssConfiguration;
using aidl::android::hardware::gnss::BnGnssDebug;
using aidl::android::hardware::gnss::BnGnssGeofence;
using aidl::android::hardware::gnss::BnGnssMeasurementInterface;
using aidl::android::hardware::gnss::BnGnssNavigationMessageInterface;
using aidl::android::hardware::gnss::BnGnssPowerIndication;
using aidl::android::hardware::gnss::BnGnssPsds;
using aidl::android::hardware::gnss::ElapsedRealtime;
using aidl::android::hardware::gnss::GnssConstellationType;
using aidl::android::hardware::gnss::GnssLocation;
using aidl::android::hardware::gnss::GnssPowerStats;
using aidl::android::hardware::gnss::IAGnss;
using aidl::android::hardware::gnss::IAGnssCallback;
using aidl::android::hardware::gnss::IAGnssRil;
using aidl::android::hardware::gnss::IAGnssRilCallback;
using aidl::android::hardware::gnss::IGnss;
using aidl::android::hardware::gnss::IGnssAntennaInfo;
using aidl::android::hardware::gnss::IGnssBatching;
using aidl::android::hardware::gnss::IGnssBatchingCallback;
using aidl::android::hardware::gnss::IGnssCallback;
using aidl::android::hardware::gnss::IGnssConfiguration;
using aidl::android::hardware::gnss::IGnssDebug;
using aidl::android::hardware::gnss::IGnssGeofence;
using aidl::android::hardware::gnss::IGnssGeofenceCallback;
using aidl::android::hardware::gnss::IGnssMeasurementCallback;
using aidl::android::hardware::gnss::IGnssMeasurementInterface;
using aidl::android::hardware::gnss::IGnssNavigationMessageCallback;
using aidl::android::hardware::gnss::IGnssNavigationMessageInterface;
using aidl::android::hardware::gnss::IGnssPowerIndication;
using aidl::android::hardware::gnss::IGnssPowerIndicationCallback;
using aidl::android::hardware::gnss::IGnssPsds;
using aidl::android::hardware::gnss::IGnssPsdsCallback;
using aidl::android::hardware::gnss::PsdsType;
using aidl::android::hardware::gnss::measurement_corrections::
    BnMeasurementCorrectionsInterface;
using aidl::android::hardware::gnss::measurement_corrections::
    IMeasurementCorrectionsCallback;
using aidl::android::hardware::gnss::measurement_corrections::
    IMeasurementCorrectionsInterface;
using aidl::android::hardware::gnss::measurement_corrections::
    MeasurementCorrections;
using aidl::android::hardware::gnss::visibility_control::BnGnssVisibilityControl;
using aidl::android::hardware::gnss::visibility_control::IGnssVisibilityControl;
using aidl::android::hardware::gnss::visibility_control::
    IGnssVisibilityControlCallback;
using ndk::ScopedAStatus;

static constexpr const char *kGnssServiceName =
    "android.hardware.gnss.IGnss/default";

// 设备值（默认 fix / 能力位 / systemInfo / 控制文件路径）外置为运行时配置
// /vendor/etc/gnss_config.xml（env GNSS_CONFIG_XML 可覆盖），对齐 audio/thermal/sensors
// 的 XML 外置做法——换机型/调默认定位只编辑该 xml，不必重编 HAL。缺项回退下面的保守默认。
struct GnssConfig {
  std::string controlFile = "/data/vendor/gps/fix";
  double defaultLongitude = 120.008549;
  double defaultLatitude = 30.289452;
  double defaultAltitudeMeters = 15.5;
  double defaultSpeedMetersPerSecond = 3.2;
  double defaultHorizontalAccuracyMeters = 1.0;
  int defaultSatellites = 7;
  int32_t gnssCapabilities = 1;
  int32_t powerCapabilities = 1;
  int32_t measurementCorrectionsCapabilities = 0;
  int32_t yearOfHw = 0;
  std::string systemInfoName;
};

static void sleepMilliseconds(int delayMs) {
  if (delayMs <= 0) {
    return;
  }
  struct timespec remaining{};
  remaining.tv_sec = delayMs / 1000;
  remaining.tv_nsec = static_cast<long>(delayMs % 1000) * 1000000L;
  while (nanosleep(&remaining, &remaining) == -1 && errno == EINTR) {
  }
}

static int64_t nowMillis() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(system_clock::now().time_since_epoch())
      .count();
}

static int64_t elapsedRealtimeNanos() {
  using namespace std::chrono;
  return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch())
      .count();
}

static int64_t statMtimeToken(const struct stat &st) {
  return static_cast<int64_t>(st.st_mtim.tv_sec) * 1000000000LL +
         static_cast<int64_t>(st.st_mtim.tv_nsec);
}

static double parseDouble(const std::map<std::string, std::string> &values,
                          const std::string &key, double fallback) {
  auto it = values.find(key);
  if (it == values.end()) {
    return fallback;
  }
  char *end = nullptr;
  const double parsed = std::strtod(it->second.c_str(), &end);
  return end != it->second.c_str() ? parsed : fallback;
}

static int parseInt(const std::map<std::string, std::string> &values,
                    const std::string &key, int fallback, int minValue,
                    int maxValue) {
  auto it = values.find(key);
  if (it == values.end()) {
    return fallback;
  }
  char *end = nullptr;
  const long parsed = std::strtol(it->second.c_str(), &end, 10);
  if (end == it->second.c_str()) {
    return fallback;
  }
  return static_cast<int>(
      std::max<long>(minValue, std::min<long>(maxValue, parsed)));
}

static GnssLocation buildLocation(double longitude, double latitude,
                                  double altitude, double speed,
                                  double horizontalAccuracy, int64_t timeMs) {
  GnssLocation location;
  location.gnssLocationFlags =
      GnssLocation::HAS_LAT_LONG | GnssLocation::HAS_ALTITUDE |
      GnssLocation::HAS_SPEED | GnssLocation::HAS_BEARING |
      GnssLocation::HAS_HORIZONTAL_ACCURACY |
      GnssLocation::HAS_VERTICAL_ACCURACY |
      GnssLocation::HAS_SPEED_ACCURACY | GnssLocation::HAS_BEARING_ACCURACY;
  location.latitudeDegrees = latitude;
  location.longitudeDegrees = longitude;
  location.altitudeMeters = altitude;
  location.speedMetersPerSec = speed;
  location.bearingDegrees = 0.0;
  location.horizontalAccuracyMeters = horizontalAccuracy;
  location.verticalAccuracyMeters = 1.0;
  location.speedAccuracyMetersPerSecond = 0.1;
  location.bearingAccuracyDegrees = 1.0;
  location.timestampMillis = timeMs;
  location.elapsedRealtime.flags =
      ElapsedRealtime::HAS_TIMESTAMP_NS |
      ElapsedRealtime::HAS_TIME_UNCERTAINTY_NS;
  location.elapsedRealtime.timestampNs = elapsedRealtimeNanos();
  location.elapsedRealtime.timeUncertaintyNs = 1000.0;
  return location;
}

// 走 C stdio 而不是 std::ifstream：android14 线链接的是 lineage-21.0 的
// /system/lib64/libc++.so，那份 libc++ 不导出 iostream 的类（实测 2159 个导出
// 符号里没有 basic_filebuf / basic_ifstream，连 stringbuf 也没有），用
// std::ifstream 会在链接期报一堆 undefined symbol（本文件 2026-10-31 实测）。
static std::map<std::string, std::string> readKeyValueFile(const char *path) {
  std::map<std::string, std::string> values;
  FILE *input = fopen(path, "re");
  if (input == nullptr) {
    return values;
  }
  char *raw = nullptr;
  size_t capacity = 0;
  ssize_t length = 0;
  while ((length = getline(&raw, &capacity, input)) >= 0) {
    std::string line(raw, static_cast<size_t>(length));
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
      line.pop_back();
    }
    const size_t pos = line.find('=');
    if (pos == std::string::npos) {
      continue;
    }
    values[line.substr(0, pos)] = line.substr(pos + 1);
  }
  free(raw);
  fclose(input);
  return values;
}

static bool readControlFix(GnssLocation *out, int *satellitesOut,
                           const GnssConfig &cfg) {
  const auto values = readKeyValueFile(cfg.controlFile.c_str());
  if (values.empty()) {
    return false;
  }
  const double longitude =
      parseDouble(values, "longitude", cfg.defaultLongitude);
  const double latitude = parseDouble(values, "latitude", cfg.defaultLatitude);
  const double altitude =
      parseDouble(values, "altitude", cfg.defaultAltitudeMeters);
  const double speed =
      parseDouble(values, "velocity", cfg.defaultSpeedMetersPerSecond);
  const double accuracy =
      parseDouble(values, "accuracy", cfg.defaultHorizontalAccuracyMeters);
  const double timeMsDouble = parseDouble(values, "time_ms", nowMillis());
  const int satellites =
      parseInt(values, "satellites", cfg.defaultSatellites, 1, 64);
  *out = buildLocation(longitude, latitude, altitude, speed, accuracy,
                       static_cast<int64_t>(timeMsDouble));
  *satellitesOut = satellites;
  return true;
}

// 配置路径：env GNSS_CONFIG_XML 优先，否则 /vendor/etc/gnss_config.xml。
static std::string cfgPath() {
  const char *env = getenv("GNSS_CONFIG_XML");
  if (env && *env) {
    return env;
  }
  return "/vendor/etc/gnss_config.xml";
}

// 载入设备值：逐属性覆盖 GnssConfig 默认；缺文件/缺属性回退默认（服务仍可起）。
static GnssConfig loadConfig(const std::string &path) {
  GnssConfig cfg;
  xmlDocPtr doc = loadXmlDoc(path);
  if (doc == nullptr) {
    return cfg;
  }
  xmlNode *root = xmlDocGetRootElement(doc);
  if (root == nullptr) {
    xmlFreeDoc(doc);
    return cfg;
  }

  std::string v;
  auto str = [&](const char *attr, std::string *dst) {
    if (readProp(root, attr, &v) && !v.empty()) {
      *dst = v;
    }
  };
  auto number = [&](const char *attr, double *dst) {
    if (readProp(root, attr, &v)) {
      char *end = nullptr;
      const double parsed = std::strtod(v.c_str(), &end);
      if (end != v.c_str()) {
        *dst = parsed;
      }
    }
  };
  auto integer = [&](const char *attr, int *dst, long lo, long hi) {
    if (readProp(root, attr, &v)) {
      char *end = nullptr;
      const long parsed = std::strtol(v.c_str(), &end, 10);
      if (end != v.c_str()) {
        *dst = static_cast<int>(std::max<long>(lo, std::min<long>(hi, parsed)));
      }
    }
  };

  str("control-file", &cfg.controlFile);
  str("system-info-name", &cfg.systemInfoName);
  number("default-longitude", &cfg.defaultLongitude);
  number("default-latitude", &cfg.defaultLatitude);
  number("default-altitude-meters", &cfg.defaultAltitudeMeters);
  number("default-speed-meters-per-second", &cfg.defaultSpeedMetersPerSecond);
  number("default-horizontal-accuracy-meters",
         &cfg.defaultHorizontalAccuracyMeters);
  integer("default-satellites", &cfg.defaultSatellites, 1, 64);
  integer("gnss-capabilities", &cfg.gnssCapabilities, 0, INT32_MAX);
  integer("power-capabilities", &cfg.powerCapabilities, 0, INT32_MAX);
  integer("measurement-corrections-capabilities",
          &cfg.measurementCorrectionsCapabilities, 0, INT32_MAX);
  integer("year-of-hw", &cfg.yearOfHw, 0, 9999);
  xmlFreeDoc(doc);
  return cfg;
}

class NoopGnssConfiguration : public BnGnssConfiguration {
public:
  ScopedAStatus setSuplVersion(int32_t) override { return ScopedAStatus::ok(); }
  ScopedAStatus setSuplMode(int32_t) override { return ScopedAStatus::ok(); }
  ScopedAStatus setLppProfile(int32_t) override { return ScopedAStatus::ok(); }
  ScopedAStatus setGlonassPositioningProtocol(int32_t) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus setEmergencySuplPdn(bool) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus setEsExtensionSec(int32_t) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus setBlocklist(
      const std::vector<aidl::android::hardware::gnss::BlocklistedSource> &)
      override {
    return ScopedAStatus::ok();
  }
};

class NoopGnssMeasurement : public BnGnssMeasurementInterface {
public:
  ScopedAStatus setCallback(
      const std::shared_ptr<IGnssMeasurementCallback> &, bool, bool) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus close() override { return ScopedAStatus::ok(); }
  ScopedAStatus setCallbackWithOptions(
      const std::shared_ptr<IGnssMeasurementCallback> &,
      const IGnssMeasurementInterface::Options &) override {
    return ScopedAStatus::ok();
  }
};

class NoopGnssBatching : public BnGnssBatching {
public:
  ScopedAStatus init(const std::shared_ptr<IGnssBatchingCallback> &callback)
      override {
    callback_ = callback;
    return ScopedAStatus::ok();
  }
  ScopedAStatus getBatchSize(int32_t *out) override {
    // Expose a minimal nonzero FIFO so framework batching setup can complete
    // without claiming a real hardware batch buffer.
    *out = 1;
    return ScopedAStatus::ok();
  }
  ScopedAStatus start(const IGnssBatching::Options &) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus flush() override { return ScopedAStatus::ok(); }
  ScopedAStatus stop() override { return ScopedAStatus::ok(); }
  ScopedAStatus cleanup() override {
    callback_.reset();
    return ScopedAStatus::ok();
  }

private:
  std::shared_ptr<IGnssBatchingCallback> callback_;
};

class NoopGnssGeofence : public BnGnssGeofence {
public:
  ScopedAStatus setCallback(
      const std::shared_ptr<IGnssGeofenceCallback> &callback) override {
    callback_ = callback;
    return ScopedAStatus::ok();
  }
  ScopedAStatus addGeofence(int32_t geofenceId, double, double, double, int32_t,
                            int32_t, int32_t, int32_t) override {
    // Android 15 dereferences the geofence extension during GNSS init. Return
    // a real binder and complete unsupported operations with generic status.
    if (callback_ != nullptr) {
      callback_->gnssGeofenceAddCb(geofenceId,
                                   IGnssGeofenceCallback::ERROR_GENERIC);
    }
    return ScopedAStatus::ok();
  }
  ScopedAStatus pauseGeofence(int32_t geofenceId) override {
    if (callback_ != nullptr) {
      callback_->gnssGeofencePauseCb(geofenceId,
                                     IGnssGeofenceCallback::ERROR_GENERIC);
    }
    return ScopedAStatus::ok();
  }
  ScopedAStatus resumeGeofence(int32_t geofenceId, int32_t) override {
    if (callback_ != nullptr) {
      callback_->gnssGeofenceResumeCb(geofenceId,
                                      IGnssGeofenceCallback::ERROR_GENERIC);
    }
    return ScopedAStatus::ok();
  }
  ScopedAStatus removeGeofence(int32_t geofenceId) override {
    if (callback_ != nullptr) {
      callback_->gnssGeofenceRemoveCb(geofenceId,
                                      IGnssGeofenceCallback::ERROR_GENERIC);
    }
    return ScopedAStatus::ok();
  }

private:
  std::shared_ptr<IGnssGeofenceCallback> callback_;
};

class NoopGnssNavigationMessage : public BnGnssNavigationMessageInterface {
public:
  ScopedAStatus setCallback(
      const std::shared_ptr<IGnssNavigationMessageCallback> &callback)
      override {
    callback_ = callback;
    return ScopedAStatus::ok();
  }
  ScopedAStatus close() override {
    callback_.reset();
    return ScopedAStatus::ok();
  }

private:
  std::shared_ptr<IGnssNavigationMessageCallback> callback_;
};

class NoopMeasurementCorrections
    : public BnMeasurementCorrectionsInterface {
public:
  explicit NoopMeasurementCorrections(int32_t capabilities)
      : capabilities_(capabilities) {}

  ScopedAStatus setCorrections(const MeasurementCorrections &) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus setCallback(
      const std::shared_ptr<IMeasurementCorrectionsCallback> &callback)
      override {
    callback_ = callback;
    if (callback_ != nullptr) {
      // No LOS/excess-path/reflecting-plane corrections are synthesized yet.
      callback_->setCapabilitiesCb(capabilities_);
    }
    return ScopedAStatus::ok();
  }

private:
  int32_t capabilities_;
  std::shared_ptr<IMeasurementCorrectionsCallback> callback_;
};

class NoopGnssPowerIndication : public BnGnssPowerIndication {
public:
  explicit NoopGnssPowerIndication(int32_t capabilities)
      : capabilities_(capabilities) {}

  ScopedAStatus
  setCallback(const std::shared_ptr<IGnssPowerIndicationCallback> &callback)
      override {
    callback_ = callback;
    if (callback_ != nullptr) {
      // Android 15 always registers the power sub-HAL when it exists. Report
      // only total energy because that is the sole field this virtual GNSS
      // service can keep internally consistent.
      callback_->setCapabilitiesCb(capabilities_);
    }
    return ScopedAStatus::ok();
  }
  ScopedAStatus requestGnssPowerStats() override {
    if (callback_ != nullptr) {
      GnssPowerStats stats;
      stats.elapsedRealtime.flags = ElapsedRealtime::HAS_TIMESTAMP_NS;
      stats.elapsedRealtime.timestampNs = elapsedRealtimeNanos();
      stats.totalEnergyMilliJoule = 0.0;
      stats.singlebandTrackingModeEnergyMilliJoule = 0.0;
      stats.multibandTrackingModeEnergyMilliJoule = 0.0;
      stats.singlebandAcquisitionModeEnergyMilliJoule = 0.0;
      stats.multibandAcquisitionModeEnergyMilliJoule = 0.0;
      stats.otherModesEnergyMilliJoule.clear();
      callback_->gnssPowerStatsCb(stats);
    }
    return ScopedAStatus::ok();
  }

private:
  int32_t capabilities_;
  std::shared_ptr<IGnssPowerIndicationCallback> callback_;
};

class NoopGnssPsds : public BnGnssPsds {
public:
  ScopedAStatus injectPsdsData(PsdsType, const std::vector<uint8_t> &) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus setCallback(const std::shared_ptr<IGnssPsdsCallback> &)
      override {
    return ScopedAStatus::ok();
  }
};

class NoopAGnss : public BnAGnss {
public:
  ScopedAStatus setCallback(const std::shared_ptr<IAGnssCallback> &) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus dataConnClosed() override { return ScopedAStatus::ok(); }
  ScopedAStatus dataConnFailed() override { return ScopedAStatus::ok(); }
  ScopedAStatus setServer(IAGnssCallback::AGnssType, const std::string &,
                          int32_t) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus dataConnOpen(int64_t, const std::string &,
                             IAGnss::ApnIpType) override {
    return ScopedAStatus::ok();
  }
};

class NoopAGnssRil : public BnAGnssRil {
public:
  ScopedAStatus setCallback(const std::shared_ptr<IAGnssRilCallback> &)
      override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus
  setRefLocation(const IAGnssRil::AGnssRefLocation &) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus setSetId(IAGnssRil::SetIdType, const std::string &) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus
  updateNetworkState(const IAGnssRil::NetworkAttributes &) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus injectNiSuplMessageData(const std::vector<uint8_t> &,
                                        int32_t) override {
    return ScopedAStatus::ok();
  }
};

class NoopGnssDebug : public BnGnssDebug {
public:
  explicit NoopGnssDebug(const GnssLocation *location) : location_(location) {}

  ScopedAStatus getDebugData(IGnssDebug::DebugData *out) override {
    out->position.valid = true;
    out->position.latitudeDegrees = location_->latitudeDegrees;
    out->position.longitudeDegrees = location_->longitudeDegrees;
    out->position.altitudeMeters =
        static_cast<float>(location_->altitudeMeters);
    out->position.speedMetersPerSec =
        static_cast<float>(location_->speedMetersPerSec);
    out->position.horizontalAccuracyMeters =
        location_->horizontalAccuracyMeters;
    out->position.verticalAccuracyMeters = location_->verticalAccuracyMeters;
    out->position.speedAccuracyMetersPerSecond =
        location_->speedAccuracyMetersPerSecond;
    out->position.bearingAccuracyDegrees = location_->bearingAccuracyDegrees;
    out->position.ageSeconds = 0.0f;
    out->time.timeEstimateMs = location_->timestampMillis;
    out->time.timeUncertaintyNs = 1000.0f;
    out->time.frequencyUncertaintyNsPerSec = 0.0f;
    out->satelliteDataArray.clear();
    return ScopedAStatus::ok();
  }

private:
  const GnssLocation *location_;
};

class NoopGnssVisibilityControl : public BnGnssVisibilityControl {
public:
  ScopedAStatus enableNfwLocationAccess(
      const std::vector<std::string> &) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus
  setCallback(const std::shared_ptr<IGnssVisibilityControlCallback> &)
      override {
    return ScopedAStatus::ok();
  }
};

class NoopGnssAntennaInfo : public BnGnssAntennaInfo {
public:
  ScopedAStatus
  setCallback(const std::shared_ptr<
              aidl::android::hardware::gnss::IGnssAntennaInfoCallback> &)
      override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus close() override { return ScopedAStatus::ok(); }
};

class GnssService : public BnGnss {
public:
  explicit GnssService(GnssConfig cfg)
      : cfg_(std::move(cfg)),
        location_(buildLocation(cfg_.defaultLongitude, cfg_.defaultLatitude,
                                cfg_.defaultAltitudeMeters,
                                cfg_.defaultSpeedMetersPerSecond,
                                cfg_.defaultHorizontalAccuracyMeters,
                                nowMillis())),
        satellites_(cfg_.defaultSatellites),
        configuration_(ndk::SharedRefBase::make<NoopGnssConfiguration>()),
        measurement_(ndk::SharedRefBase::make<NoopGnssMeasurement>()),
        batching_(ndk::SharedRefBase::make<NoopGnssBatching>()),
        geofence_(ndk::SharedRefBase::make<NoopGnssGeofence>()),
        navigation_(ndk::SharedRefBase::make<NoopGnssNavigationMessage>()),
        measurementCorrections_(
            ndk::SharedRefBase::make<NoopMeasurementCorrections>(
                cfg_.measurementCorrectionsCapabilities)),
        power_(ndk::SharedRefBase::make<NoopGnssPowerIndication>(
            cfg_.powerCapabilities)),
        psds_(ndk::SharedRefBase::make<NoopGnssPsds>()),
        agnss_(ndk::SharedRefBase::make<NoopAGnss>()),
        agnssRil_(ndk::SharedRefBase::make<NoopAGnssRil>()),
        debug_(ndk::SharedRefBase::make<NoopGnssDebug>(&location_)),
        visibility_(ndk::SharedRefBase::make<NoopGnssVisibilityControl>()),
        antenna_(ndk::SharedRefBase::make<NoopGnssAntennaInfo>()) {
    poller_ = std::thread([this] { pollControlFile(); });
  }

  ~GnssService() override {
    stopPoller_ = true;
    if (poller_.joinable()) {
      poller_.join();
    }
  }

  ScopedAStatus setCallback(const std::shared_ptr<IGnssCallback> &callback)
      override {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      callback_ = callback;
    }
    if (callback != nullptr) {
      // Android framework calls setCallback() during init/cleanup probing.
      // Keep this phase side-effect-light: advertise only the scheduling
      // capability that is needed for a normal GPS provider, and defer status
      // and fixes until start() enters an actual GNSS session.
      callback->gnssSetCapabilitiesCb(cfg_.gnssCapabilities);
      IGnssCallback::GnssSystemInfo info;
      info.yearOfHw = cfg_.yearOfHw;
      info.name = cfg_.systemInfoName;
      callback->gnssSetSystemInfoCb(info);
    }
    return ScopedAStatus::ok();
  }

  ScopedAStatus close() override {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_.reset();
    running_ = false;
    return ScopedAStatus::ok();
  }

  ScopedAStatus getExtensionPsds(std::shared_ptr<IGnssPsds> *out) override {
    *out = psds_;
    return ScopedAStatus::ok();
  }
  ScopedAStatus getExtensionGnssConfiguration(
      std::shared_ptr<IGnssConfiguration> *out) override {
    *out = configuration_;
    return ScopedAStatus::ok();
  }
  ScopedAStatus getExtensionGnssMeasurement(
      std::shared_ptr<IGnssMeasurementInterface> *out) override {
    *out = measurement_;
    return ScopedAStatus::ok();
  }
  ScopedAStatus getExtensionGnssPowerIndication(
      std::shared_ptr<IGnssPowerIndication> *out) override {
    *out = power_;
    return ScopedAStatus::ok();
  }
  ScopedAStatus getExtensionGnssBatching(
      std::shared_ptr<IGnssBatching> *out) override {
    *out = batching_;
    return ScopedAStatus::ok();
  }
  ScopedAStatus getExtensionGnssGeofence(
      std::shared_ptr<IGnssGeofence> *out) override {
    *out = geofence_;
    return ScopedAStatus::ok();
  }
  ScopedAStatus getExtensionGnssNavigationMessage(
      std::shared_ptr<IGnssNavigationMessageInterface> *out) override {
    *out = navigation_;
    return ScopedAStatus::ok();
  }
  ScopedAStatus getExtensionAGnss(std::shared_ptr<IAGnss> *out) override {
    *out = agnss_;
    return ScopedAStatus::ok();
  }
  ScopedAStatus getExtensionAGnssRil(std::shared_ptr<IAGnssRil> *out)
      override {
    *out = agnssRil_;
    return ScopedAStatus::ok();
  }
  ScopedAStatus getExtensionGnssDebug(std::shared_ptr<IGnssDebug> *out)
      override {
    *out = debug_;
    return ScopedAStatus::ok();
  }
  ScopedAStatus getExtensionGnssVisibilityControl(
      std::shared_ptr<IGnssVisibilityControl> *out) override {
    *out = visibility_;
    return ScopedAStatus::ok();
  }

  ScopedAStatus start() override {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      running_ = true;
      pendingStart_ = true;
    }
    return ScopedAStatus::ok();
  }

  ScopedAStatus stop() override {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      running_ = false;
      pendingStop_ = true;
    }
    return ScopedAStatus::ok();
  }

  ScopedAStatus injectTime(int64_t, int64_t, int32_t) override {
    return ScopedAStatus::ok();
  }

  ScopedAStatus injectLocation(const GnssLocation &location) override {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      location_ = location;
      pendingForceLocation_ = true;
    }
    return ScopedAStatus::ok();
  }

  ScopedAStatus injectBestLocation(const GnssLocation &location) override {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      location_ = location;
      pendingForceLocation_ = true;
    }
    return ScopedAStatus::ok();
  }

  ScopedAStatus deleteAidingData(IGnss::GnssAidingData) override {
    return ScopedAStatus::ok();
  }

  ScopedAStatus setPositionMode(const IGnss::PositionModeOptions &) override {
    return ScopedAStatus::ok();
  }

  ScopedAStatus getExtensionGnssAntennaInfo(
      std::shared_ptr<IGnssAntennaInfo> *out) override {
    *out = antenna_;
    return ScopedAStatus::ok();
  }

  ScopedAStatus getExtensionMeasurementCorrections(
      std::shared_ptr<IMeasurementCorrectionsInterface> *out) override {
    *out = measurementCorrections_;
    return ScopedAStatus::ok();
  }

  ScopedAStatus startSvStatus() override {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      pendingSvStatus_ = true;
    }
    return ScopedAStatus::ok();
  }
  ScopedAStatus stopSvStatus() override { return ScopedAStatus::ok(); }
  ScopedAStatus startNmea() override {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      pendingNmea_ = true;
    }
    return ScopedAStatus::ok();
  }
  ScopedAStatus stopNmea() override { return ScopedAStatus::ok(); }

private:
  void setLocation(const GnssLocation &location, bool forcePublish) {
    int satellites;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      satellites = satellites_;
    }
    setLocation(location, satellites, forcePublish);
  }

  void setLocation(const GnssLocation &location, int satellites,
                   bool forcePublish) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      location_ = location;
      satellites_ = std::max(1, std::min(64, satellites));
    }
    publishLocation(forcePublish);
  }

  void notifyStatus(IGnssCallback::GnssStatusValue status) {
    std::shared_ptr<IGnssCallback> callback;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      callback = callback_;
    }
    if (callback != nullptr) {
      callback->gnssStatusCb(status);
    }
  }

  void publishLocation(bool forcePublish = false) {
    std::shared_ptr<IGnssCallback> callback;
    GnssLocation location;
    bool running;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      callback = callback_;
      location = location_;
      running = running_;
    }
    if (callback != nullptr && (running || forcePublish)) {
      callback->gnssLocationCb(location);
    }
  }

  void publishSvStatus() {
    std::shared_ptr<IGnssCallback> callback;
    int satellites;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      callback = callback_;
      satellites = satellites_;
    }
    if (callback == nullptr) {
      return;
    }
    std::vector<IGnssCallback::GnssSvInfo> svs;
    for (int i = 0; i < satellites; ++i) {
      IGnssCallback::GnssSvInfo sv;
      sv.svid = i + 1;
      sv.constellation = GnssConstellationType::GPS;
      sv.cN0Dbhz = 35.0f + static_cast<float>(i);
      sv.basebandCN0DbHz = sv.cN0Dbhz;
      sv.elevationDegrees = 45.0f;
      sv.azimuthDegrees = static_cast<float>(i * 30);
      sv.carrierFrequencyHz = 1575420000;
      // AIDL generates GnssSvFlags as a scoped enum; combine flags through the
      // integer backing type so the callback still receives the official mask.
      sv.svFlag =
          static_cast<int32_t>(IGnssCallback::GnssSvFlags::HAS_EPHEMERIS_DATA) |
          static_cast<int32_t>(IGnssCallback::GnssSvFlags::HAS_ALMANAC_DATA) |
          static_cast<int32_t>(IGnssCallback::GnssSvFlags::USED_IN_FIX) |
          static_cast<int32_t>(
              IGnssCallback::GnssSvFlags::HAS_CARRIER_FREQUENCY);
      svs.push_back(sv);
    }
    callback->gnssSvStatusCb(svs);
  }

  void publishNmea() {
    std::shared_ptr<IGnssCallback> callback;
    GnssLocation location;
    int satellites;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      callback = callback_;
      location = location_;
      satellites = satellites_;
    }
    if (callback == nullptr) {
      return;
    }
    // NMEA 用 snprintf 拼：不用 ostringstream。Android 14 平台的 libc++.so 不导出
    // basic_stringbuf/stringstream 符号，14 线一旦引用就在链接期报未定义。
    char nmea[192];
    snprintf(nmea, sizeof(nmea),
             "$GPGGA,000000,%.6f,N,%.6f,E,1,%d,1.0,%.1f,M,0.0,M,,",
             std::fabs(location.latitudeDegrees),
             std::fabs(location.longitudeDegrees), satellites,
             location.altitudeMeters);
    callback->gnssNmeaCb(location.timestampMillis, nmea);
  }

  void pollControlFile() {
    int64_t lastMtime = 0;
    int tick = 0;
    while (!stopPoller_) {
      bool doStart = false;
      bool doStop = false;
      bool doForceLoc = false;
      bool doSvStatus = false;
      bool doNmea = false;
      {
        std::lock_guard<std::mutex> lock(mutex_);
        std::swap(doStart, pendingStart_);
        std::swap(doStop, pendingStop_);
        std::swap(doForceLoc, pendingForceLocation_);
        std::swap(doSvStatus, pendingSvStatus_);
        std::swap(doNmea, pendingNmea_);
      }
      if (doStart) {
        notifyStatus(IGnssCallback::GnssStatusValue::ENGINE_ON);
        notifyStatus(IGnssCallback::GnssStatusValue::SESSION_BEGIN);
        publishLocation();
      }
      if (doStop) {
        notifyStatus(IGnssCallback::GnssStatusValue::SESSION_END);
        notifyStatus(IGnssCallback::GnssStatusValue::ENGINE_OFF);
      }
      if (doForceLoc) {
        publishLocation(true);
      }
      if (doSvStatus) {
        publishSvStatus();
      }
      if (doNmea) {
        publishNmea();
      }

      if (tick % 10 == 0) {
        struct stat st {};
        if (stat(cfg_.controlFile.c_str(), &st) == 0 &&
            statMtimeToken(st) != lastMtime) {
          lastMtime = statMtimeToken(st);
          GnssLocation location;
          int satellites = cfg_.defaultSatellites;
          // fishbridge 写入的控制文件是 console geo fix 到 GNSS HAL 的唯一输入；
          // 这里消费后强制推送一次 HAL fix，让 adb emu geo fix 在无前台 listener
          // 时也能更新 framework 的 GPS provider 状态。
          if (readControlFix(&location, &satellites, cfg_)) {
            setLocation(location, satellites, true);
          }
        }
      }
      ++tick;
      sleepMilliseconds(25);
    }
  }

  std::mutex mutex_;
  std::shared_ptr<IGnssCallback> callback_;
  GnssConfig cfg_;
  GnssLocation location_;
  int satellites_;
  bool running_ = false;
  bool pendingStart_ = false;
  bool pendingStop_ = false;
  bool pendingForceLocation_ = false;
  bool pendingSvStatus_ = false;
  bool pendingNmea_ = false;
  bool stopPoller_ = false;
  std::thread poller_;
  std::shared_ptr<IGnssConfiguration> configuration_;
  std::shared_ptr<IGnssMeasurementInterface> measurement_;
  std::shared_ptr<IGnssBatching> batching_;
  std::shared_ptr<IGnssGeofence> geofence_;
  std::shared_ptr<IGnssNavigationMessageInterface> navigation_;
  std::shared_ptr<IMeasurementCorrectionsInterface> measurementCorrections_;
  std::shared_ptr<IGnssPowerIndication> power_;
  std::shared_ptr<IGnssPsds> psds_;
  std::shared_ptr<IAGnss> agnss_;
  std::shared_ptr<IAGnssRil> agnssRil_;
  std::shared_ptr<IGnssDebug> debug_;
  std::shared_ptr<IGnssVisibilityControl> visibility_;
  std::shared_ptr<IGnssAntennaInfo> antenna_;
};

static binder_exception_t
registerAsService(const std::shared_ptr<ndk::ICInterface> &service,
                  const char *name) {
  return AServiceManager_addService(service->asBinder().get(), name);
}

static void *OnPlaceholderCreate(void * /*args*/) {
  return reinterpret_cast<void *>(1);
}

static void OnPlaceholderDestroy(void * /*user_data*/) {}

static binder_status_t OnPlaceholderTransact(AIBinder * /*binder*/,
                                             transaction_code_t /*code*/,
                                             const AParcel * /*in*/,
                                             AParcel * /*out*/) {
  return STATUS_UNKNOWN_TRANSACTION;
}

static std::string TrimWhitespace(const std::string &input) {
  const std::string::size_type first = input.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return "";
  }
  const std::string::size_type last = input.find_last_not_of(" \t\r\n");
  return input.substr(first, last - first + 1);
}

// 按行读取配置文件（C stdio，避免依赖高版本 libc++ iostream），为未实现 GNSS 扩展注册空桩。
static void RegisterPlaceholdersFromFile(const char *path) {
  FILE *input = fopen(path, "re");
  if (input == nullptr) {
    return;
  }
  static std::map<std::string, AIBinder_Class *> class_cache;
  static std::vector<AIBinder *> placeholder_refs;

  char *raw = nullptr;
  size_t capacity = 0;
  ssize_t length = 0;
  int registered = 0;
  while ((length = getline(&raw, &capacity, input)) >= 0) {
    const std::string name =
        TrimWhitespace(std::string(raw, static_cast<size_t>(length)));
    if (name.empty() || name[0] == '#') {
      continue;
    }
    if (AIBinder *existing = AServiceManager_checkService(name.c_str())) {
      AIBinder_decStrong(existing);
      continue;
    }
    const std::string::size_type slash = name.find('/');
    const std::string descriptor =
        slash == std::string::npos ? name : name.substr(0, slash);
    if (descriptor.empty()) {
      continue;
    }
    auto it = class_cache.find(descriptor);
    if (it == class_cache.end()) {
      it = class_cache.emplace(descriptor, nullptr).first;
      it->second = AIBinder_Class_define(
          it->first.c_str(), OnPlaceholderCreate, OnPlaceholderDestroy,
          OnPlaceholderTransact);
      if (it->second == nullptr) {
        __android_log_print(ANDROID_LOG_WARN, "cdh-gnss",
                            "define placeholder binder class failed: %s",
                            descriptor.c_str());
        class_cache.erase(it);
        continue;
      }
      AIBinder_Class_disableInterfaceTokenHeader(it->second);
    }
    AIBinder *binder = AIBinder_new(it->second, nullptr);
    if (binder == nullptr) {
      __android_log_print(ANDROID_LOG_WARN, "cdh-gnss",
                          "allocate placeholder binder failed: %s",
                          name.c_str());
      continue;
    }
    AIBinder_markVintfStability(binder);
    const binder_exception_t status =
        AServiceManager_addService(binder, name.c_str());
    if (status != EX_NONE) {
      __android_log_print(ANDROID_LOG_WARN, "cdh-gnss",
                          "register placeholder service failed: %s (%d)",
                          name.c_str(), status);
      AIBinder_decStrong(binder);
      continue;
    }
    placeholder_refs.push_back(binder);
    ++registered;
  }
  free(raw);
  fclose(input);
  __android_log_print(ANDROID_LOG_INFO, "cdh-gnss",
                      "registered %d gnss placeholder services from %s",
                      registered, path);
}

int main() {
  ABinderProcess_setThreadPoolMaxThreadCount(4);
  ABinderProcess_startThreadPool();
  auto gnss = ndk::SharedRefBase::make<GnssService>(loadConfig(cfgPath()));
  if (registerAsService(gnss, kGnssServiceName) != EX_NONE) {
    return 2;
  }
  RegisterPlaceholdersFromFile("/vendor/etc/cdh_gnss_placeholders.txt");
  ABinderProcess_joinThreadPool();
  return 0;
}
