// engine/vendor/hardware 线 —— android.hardware.sensors（ISensors v2）自研服务。
//
// 注册 android.hardware.sensors.ISensors/default，供 framework SensorService 使用。
// 底座无 sensors HAL 时接管：上报机型传感器清单，并通过 FMQ（见 FmqSupport.cpp）
// 按 sensor_event_model 生成虚拟事件流。
//
// 机型差异（传感器清单：handle/name/vendor/type/量程/延迟/FIFO/wakeUp/reportingMode）
// 外置到运行时配置 /vendor/etc/sensors_config.xml（env SENSORS_CONFIG_XML 覆盖），
// 由设备集成层随镜像下发；本引擎不内置机型数值。读不到即回退空清单（无传感器） 。
// 配置格式：
//   <sensors-config>
//     <sensor handle="1" name="lsm6dso_acc" vendor="InvenSense" version="1"
//             type-id="1" type="android.sensor.accelerometer" max-range="78.45"
//             resolution="0.0012" power="0.001" min-delay="2500"
//             fifo-reserved="3000" fifo-max="4500" max-delay="80000"
//             wake-up="false" reporting="0"
//             additional-info="true" direct-report="1" direct-channel="2"/>
//   </sensors-config>
//
// 闭包最小：只用 std + libxml2（按 staticLibs 静态编入），-fno-exceptions（strtof/strtol，不用 stoi）。

#include <android/binder_ibinder.h>
#include <android/binder_status.h>

extern "C" {
void ABinderProcess_setThreadPoolMaxThreadCount(uint32_t numThreads);
void ABinderProcess_joinThreadPool(void);
binder_status_t AServiceManager_addService(AIBinder *binder,
                                           const char *instance);
}

#include <aidl/android/hardware/sensors/BnSensors.h>
#include <aidl/android/hardware/sensors/Event.h>
#include <aidl/android/hardware/sensors/ISensors.h>
#include <aidl/android/hardware/sensors/ISensorsCallback.h>
#include <aidl/android/hardware/sensors/SensorInfo.h>
#include <fmq/AidlMessageQueue.h>

#include "sensor_event_model.h"
#if CDH_SENSORS_MEIZU
#include "meizu/sensors_meizu_ext.h"
#endif

#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <string>
#include <sys/system_properties.h>
#include <thread>
#include <unordered_map>
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

using aidl::android::hardware::common::fmq::MQDescriptor;
using aidl::android::hardware::common::fmq::SynchronizedReadWrite;
using aidl::android::hardware::sensors::BnSensors;
using aidl::android::hardware::sensors::Event;
using aidl::android::hardware::sensors::ISensors;
using aidl::android::hardware::sensors::ISensorsCallback;
using aidl::android::hardware::sensors::SensorInfo;
using aidl::android::hardware::sensors::SensorStatus;
using aidl::android::hardware::sensors::SensorType;
using android::AidlMessageQueue;
using cdh::sensors::EventPayloadKind;
using cdh::sensors::SensorEventModel;
using cdh::sensors::SensorEventSpec;
using cdh::sensors::VirtualSensorEvent;
using ndk::ScopedAStatus;

static constexpr const char *kSensorsServiceName =
    "android.hardware.sensors.ISensors/default";
static constexpr const char *kDefaultXmlPath = "/vendor/etc/sensors_config.xml";

namespace {

std::string trim(const std::string &s) {
  size_t b = 0, e = s.size();
  while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
  while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
  return s.substr(b, e - b);
}

bool parseInt(const std::string &raw, int64_t *out) {
  std::string v = trim(raw);
  const char *c = v.c_str();
  char *end = nullptr;
  long x = std::strtol(c, &end, 10);
  if (end == c || *end != '\0') {
    return false;
  }
  *out = static_cast<int64_t>(x);
  return true;
}

bool parseFloat(const std::string &raw, float *out) {
  std::string v = trim(raw);
  const char *c = v.c_str();
  char *end = nullptr;
  float x = std::strtof(c, &end);
  if (end == c || *end != '\0') {
    return false;
  }
  *out = x;
  return true;
}

bool parseBool(const std::string &raw, bool *out) {
  std::string s = trim(raw);
  for (auto &c : s) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  if (s == "true" || s == "1") {
    *out = true;
    return true;
  }
  if (s == "false" || s == "0") {
    *out = false;
    return true;
  }
  return false;
}

}  // namespace

struct SensorTarget {
  int32_t handle = 0;
  std::string name;
  std::string vendor;
  int32_t version = 0;
  int32_t type = 0;
  std::string typeString;
  float maximumRange = 0.0f;
  float resolution = 0.0f;
  float power = 0.0f;
  int32_t minDelayUs = 0;
  int32_t fifoReservedEventCount = 0;
  int32_t fifoMaxEventCount = 0;
  int32_t maxDelayUs = 0;
  bool wakeUp = false;
  int32_t reportingMode = 0;
  // flags 附加位（可选，缺省 0 与旧配置行为一致）：真机 mFlags 除
  // reporting/wakeUp 外还带 additional-info / data-injection / direct 位。
  bool additionalInfo = false;
  bool dataInjection = false;
  int32_t directReportRate = 0;   // 0=stop，按 SENSOR_FLAG_SHIFT_DIRECT_REPORT 入位
  int32_t directChannelType = 0;  // 1=ashmem 2=gralloc，按 SHIFT_DIRECT_CHANNEL 入位
  bool hasPayloadOverride = false;
  EventPayloadKind payloadKind = EventPayloadKind::Scalar;
};

namespace {

bool parsePayloadKind(const std::string &raw, EventPayloadKind *out) {
  std::string s = trim(raw);
  for (auto &c : s) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  if (s == "vec3") {
    *out = EventPayloadKind::Vec3;
    return true;
  }
  if (s == "vec4") {
    *out = EventPayloadKind::Vec4;
    return true;
  }
  if (s == "uncal") {
    *out = EventPayloadKind::Uncal;
    return true;
  }
  if (s == "scalar") {
    *out = EventPayloadKind::Scalar;
    return true;
  }
  if (s == "step_count" || s == "stepcount") {
    *out = EventPayloadKind::StepCount;
    return true;
  }
  if (s == "data") {
    *out = EventPayloadKind::Data;
    return true;
  }
  return false;
}

// 解析 <sensor handle name vendor ... reporting>：缺属性保留结构体默认（字段与
// SensorInfo 一一对应，字段名见 sensors_config.xml 注释）。
std::vector<SensorTarget> loadSensors(const std::string &path) {
  std::vector<SensorTarget> out;
  xmlDocPtr doc = loadXmlDoc(path);
  if (doc == nullptr) {
    return out;  // 读不到回退空清单
  }
  xmlNode *root = xmlDocGetRootElement(doc);
  if (root == nullptr) {
    xmlFreeDoc(doc);
    return out;
  }
  for (xmlNode *e = firstChild(root, "sensor"); e != nullptr;
       e = nextNamed(e, "sensor")) {
    SensorTarget t;
    std::string v;
    int64_t n = 0;
    float f = 0.0f;
    bool b = false;
    if (readProp(e, "handle", &v) && parseInt(v, &n)) {
      t.handle = static_cast<int32_t>(n);
    }
    if (readProp(e, "name", &v)) t.name = v;
    if (readProp(e, "vendor", &v)) t.vendor = v;
    if (readProp(e, "version", &v) && parseInt(v, &n)) {
      t.version = static_cast<int32_t>(n);
    }
    if (readProp(e, "type-id", &v) && parseInt(v, &n)) {
      t.type = static_cast<int32_t>(n);
    }
    if (readProp(e, "type", &v)) t.typeString = v;
    if (readProp(e, "max-range", &v) && parseFloat(v, &f)) {
      t.maximumRange = f;
    }
    if (readProp(e, "resolution", &v) && parseFloat(v, &f)) {
      t.resolution = f;
    }
    if (readProp(e, "power", &v) && parseFloat(v, &f)) {
      t.power = f;
    }
    if (readProp(e, "min-delay", &v) && parseInt(v, &n)) {
      t.minDelayUs = static_cast<int32_t>(n);
    }
    if (readProp(e, "fifo-reserved", &v) && parseInt(v, &n)) {
      t.fifoReservedEventCount = static_cast<int32_t>(n);
    }
    if (readProp(e, "fifo-max", &v) && parseInt(v, &n)) {
      t.fifoMaxEventCount = static_cast<int32_t>(n);
    }
    if (readProp(e, "max-delay", &v) && parseInt(v, &n)) {
      t.maxDelayUs = static_cast<int32_t>(n);
    }
    if (readProp(e, "wake-up", &v) && parseBool(v, &b)) {
      t.wakeUp = b;
    }
    if (readProp(e, "reporting", &v) && parseInt(v, &n)) {
      t.reportingMode = static_cast<int32_t>(n);
    }
    if (readProp(e, "additional-info", &v) && parseBool(v, &b)) {
      t.additionalInfo = b;
    }
    if (readProp(e, "data-injection", &v) && parseBool(v, &b)) {
      t.dataInjection = b;
    }
    if (readProp(e, "direct-report", &v) && parseInt(v, &n)) {
      t.directReportRate = static_cast<int32_t>(n);
    }
    if (readProp(e, "direct-channel", &v) && parseInt(v, &n)) {
      t.directChannelType = static_cast<int32_t>(n);
    }
    EventPayloadKind pk = EventPayloadKind::Scalar;
    if (readProp(e, "payload", &v) && parsePayloadKind(v, &pk)) {
      t.hasPayloadOverride = true;
      t.payloadKind = pk;
    }
    out.push_back(std::move(t));
  }
  xmlFreeDoc(doc);
  return out;
}

std::string cfgPath() {
  const char *env = std::getenv("SENSORS_CONFIG_XML");
  if (env != nullptr && env[0] != '\0') return env;
  return kDefaultXmlPath;
}

}  // namespace

struct SensorState {
  bool active = false;
  int64_t samplingPeriodNs = 0;
  int64_t maxReportLatencyNs = 0;
};

static SensorInfo toSensorInfo(const SensorTarget &target) {
  SensorInfo info;
  info.sensorHandle = target.handle;
  info.name = target.name;
  info.vendor = target.vendor;
  info.version = target.version;
  info.type = SensorType(target.type);
  info.typeAsString = target.typeString;
  info.maxRange = target.maximumRange;
  info.resolution = target.resolution;
  info.power = target.power;
  info.minDelayUs = target.minDelayUs;
  info.fifoReservedEventCount = target.fifoReservedEventCount;
  info.fifoMaxEventCount = target.fifoMaxEventCount;
  info.requiredPermission = "";
  info.maxDelayUs = target.maxDelayUs;
  info.flags =
      (target.reportingMode << SensorInfo::SENSOR_FLAG_SHIFT_REPORTING_MODE) |
      (target.wakeUp ? SensorInfo::SENSOR_FLAG_BITS_WAKE_UP : 0) |
      (target.additionalInfo ? SensorInfo::SENSOR_FLAG_BITS_ADDITIONAL_INFO : 0) |
      (target.dataInjection ? SensorInfo::SENSOR_FLAG_BITS_DATA_INJECTION : 0) |
      ((target.directReportRate << SensorInfo::SENSOR_FLAG_SHIFT_DIRECT_REPORT) &
       SensorInfo::SENSOR_FLAG_BITS_MASK_DIRECT_REPORT) |
      ((target.directChannelType << SensorInfo::SENSOR_FLAG_SHIFT_DIRECT_CHANNEL) &
       SensorInfo::SENSOR_FLAG_BITS_MASK_DIRECT_CHANNEL);
  return info;
}

static std::vector<SensorInfo>
buildSensorList(const std::vector<SensorTarget> &targets) {
  std::vector<SensorInfo> sensors;
  sensors.reserve(targets.size());
  for (const auto &target : targets) {
    sensors.push_back(toSensorInfo(target));
  }
  return sensors;
}

static std::vector<SensorEventSpec>
buildSensorEventSpecs(const std::vector<SensorTarget> &targets) {
  std::vector<SensorEventSpec> specs;
  specs.reserve(targets.size());
  for (const auto &target : targets) {
    specs.push_back(SensorEventSpec{
        target.handle, target.type, target.reportingMode, target.minDelayUs,
        target.hasPayloadOverride, target.payloadKind, target.resolution});
  }
  return specs;
}

static int64_t bootTimeNanos() {
  timespec now{};
  if (clock_gettime(CLOCK_BOOTTIME, &now) != 0) {
    return 0;
  }
  return static_cast<int64_t>(now.tv_sec) * 1'000'000'000LL + now.tv_nsec;
}

static uint64_t fnv1aHash64(const std::string &material) {
  uint64_t h = 14695981039346656037ULL;
  for (unsigned char c : material) {
    h ^= static_cast<uint64_t>(c);
    h *= 1099511628211ULL;
  }
  return h == 0 ? 0x6364685f73656e73ULL : h;
}

// 计算单云机实例持久确定性种子（跨重启幂等，多实例互异）：
// 1. 优先读取环境变量 SENSORS_INSTANCE_SEED（测试注入）；
// 2. 依次采集实例专属属性（ro.vendor.oem.* / ro.ril.oem.* / ro.boot.serialno）；
// 3. 若早启动属性尚未就绪（*hasStableIdentity == false），回退读取内核 boot_id，
//    并在 SensorService 调用 initialize() 时再次尝试升级为持久身份种子。
static uint64_t resolveInstanceSeed(bool *hasStableIdentity) {
  if (hasStableIdentity != nullptr) {
    *hasStableIdentity = false;
  }
  if (const char *env = std::getenv("SENSORS_INSTANCE_SEED");
      env != nullptr && env[0] != '\0') {
    if (hasStableIdentity != nullptr) {
      *hasStableIdentity = true;
    }
    return fnv1aHash64(std::string("env:") + env);
  }

  static constexpr const char *kIdentityProps[] = {
      "ro.vendor.oem.serialno",
      "ro.vendor.oem.wifimac",
      "ro.vendor.oem.btmac",
      "ro.vendor.oem.imei",
      "ro.ril.oem.wifimac",
      "ro.ril.oem.btmac",
      "ro.boot.serialno",
  };
  std::string material;
  char propBuf[PROP_VALUE_MAX] = {};
  for (const char *key : kIdentityProps) {
    int len = __system_property_get(key, propBuf);
    if (len > 0) {
      material.append(key);
      material.push_back('=');
      material.append(propBuf, static_cast<size_t>(len));
      material.push_back(';');
    }
  }
  if (!material.empty()) {
    if (hasStableIdentity != nullptr) {
      *hasStableIdentity = true;
    }
    return fnv1aHash64(material);
  }

  if (FILE *fp = std::fopen("/proc/sys/kernel/random/boot_id", "rb");
      fp != nullptr) {
    char buf[128] = {};
    size_t n = std::fread(buf, 1, sizeof(buf) - 1, fp);
    std::fclose(fp);
    std::string bootId = trim(std::string(buf, n));
    if (!bootId.empty()) {
      return fnv1aHash64("boot_id:" + bootId);
    }
  }
  return 0x6364685f73656e73ULL;
}

static Event toAidlEvent(const VirtualSensorEvent &sample) {
  using Payload = Event::EventPayload;
  Event event;
  event.timestamp = sample.timestampNs;
  event.sensorHandle = sample.sensorHandle;
  event.sensorType = SensorType(sample.sensorType);
  switch (sample.payloadKind) {
  case EventPayloadKind::Vec3: {
    Payload::Vec3 value;
    value.x = sample.values[0];
    value.y = sample.values[1];
    value.z = sample.values[2];
    value.status = SensorStatus::ACCURACY_HIGH;
    event.payload.set<Payload::Tag::vec3>(value);
    break;
  }
  case EventPayloadKind::Vec4: {
    Payload::Vec4 value;
    value.x = sample.values[0];
    value.y = sample.values[1];
    value.z = sample.values[2];
    value.w = sample.values[3];
    event.payload.set<Payload::Tag::vec4>(value);
    break;
  }
  case EventPayloadKind::Uncal: {
    Payload::Uncal value;
    value.x = sample.values[0];
    value.y = sample.values[1];
    value.z = sample.values[2];
    value.xBias = sample.values[3];
    value.yBias = sample.values[4];
    value.zBias = sample.values[5];
    event.payload.set<Payload::Tag::uncal>(value);
    break;
  }
  case EventPayloadKind::Scalar:
    event.payload.set<Payload::Tag::scalar>(sample.values[0]);
    break;
  case EventPayloadKind::StepCount:
    event.payload.set<Payload::Tag::stepCount>(sample.stepCount);
    break;
  case EventPayloadKind::Data: {
    Payload::Data value;
    value.values = sample.values;
    event.payload.set<Payload::Tag::data>(value);
    break;
  }
  }
  return event;
}

template <typename T, typename Flavor>
static bool
isUsableQueueDescriptor(const MQDescriptor<T, Flavor> &descriptor) {
  if (descriptor.grantors.empty() || descriptor.handle.fds.empty() ||
      descriptor.quantum <= 0) {
    return false;
  }
  for (const auto &grantor : descriptor.grantors) {
    if (grantor.fdIndex < 0 ||
        static_cast<size_t>(grantor.fdIndex) >= descriptor.handle.fds.size() ||
        grantor.offset < 0 || grantor.extent <= 0) {
      return false;
    }
  }
  return true;
}

class TargetSensors : public BnSensors {
public:
  explicit TargetSensors(std::vector<SensorTarget> targets)
      : targets_(std::move(targets)),
        eventModel_(buildSensorEventSpecs(targets_),
                    resolveInstanceSeed(&seedFromStableIdentity_)) {
    for (const auto &target : targets_) {
      sensorStates_.emplace(target.handle, SensorState{});
    }
    eventThread_ = std::thread([this] { runEventLoop(); });
  }

  ~TargetSensors() override {
    {
      std::lock_guard<std::mutex> guard(lock_);
      stopEventThread_ = true;
      eventCondition_.notify_all();
    }
    if (eventThread_.joinable()) {
      eventThread_.join();
    }
  }

  ScopedAStatus activate(int32_t sensorHandle, bool enabled) override {
    std::lock_guard<std::mutex> guard(lock_);
    if (!isKnownSensorLocked(sensorHandle)) {
      return badValue();
    }
    sensorStates_[sensorHandle].active = enabled;
    if (!eventModel_.setActive(sensorHandle, enabled, bootTimeNanos())) {
      return badValue();
    }
    eventCondition_.notify_all();
    return ScopedAStatus::ok();
  }

  ScopedAStatus batch(int32_t sensorHandle, int64_t samplingPeriodNs,
                      int64_t maxReportLatencyNs) override {
    std::lock_guard<std::mutex> guard(lock_);
    if (!isKnownSensorLocked(sensorHandle) || samplingPeriodNs < 0 ||
        maxReportLatencyNs < 0) {
      return badValue();
    }
    SensorState &state = sensorStates_[sensorHandle];
    state.samplingPeriodNs = samplingPeriodNs;
    state.maxReportLatencyNs = maxReportLatencyNs;
    if (!eventModel_.setSamplingPeriod(sensorHandle, samplingPeriodNs)) {
      return badValue();
    }
    eventCondition_.notify_all();
    return ScopedAStatus::ok();
  }

  ScopedAStatus configDirectReport(int32_t, int32_t, ISensors::RateLevel,
                                   int32_t *out) override {
    *out = EX_UNSUPPORTED_OPERATION;
    return ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
  }

  ScopedAStatus flush(int32_t sensorHandle) override {
    std::lock_guard<std::mutex> guard(lock_);
    if (!isKnownSensorLocked(sensorHandle) ||
        !sensorStates_[sensorHandle].active) {
      return badValue();
    }
    Event event;
    event.timestamp = bootTimeNanos();
    event.sensorHandle = sensorHandle;
    event.sensorType = SensorType::META_DATA;
    Event::EventPayload::MetaData meta;
    meta.what = Event::EventPayload::MetaData::MetaDataEventType::
        META_DATA_FLUSH_COMPLETE;
    event.payload.set<Event::EventPayload::Tag::meta>(meta);
    return writeEventsLocked(std::vector<Event>{event}) ? ScopedAStatus::ok()
                                                        : noMemory();
  }

  ScopedAStatus getSensorsList(std::vector<SensorInfo> *out) override {
    *out = buildSensorList(targets_);
    return ScopedAStatus::ok();
  }

  ScopedAStatus initialize(
      const MQDescriptor<Event, SynchronizedReadWrite> &eventQueueDescriptor,
      const MQDescriptor<int32_t, SynchronizedReadWrite> &wakeLockDescriptor,
      const std::shared_ptr<ISensorsCallback> &sensorsCallback) override {
    if (sensorsCallback == nullptr ||
        !isUsableQueueDescriptor(eventQueueDescriptor) ||
        !isUsableQueueDescriptor(wakeLockDescriptor)) {
      return badValue();
    }
    auto eventQueue =
        std::make_unique<EventQueue>(eventQueueDescriptor, true);
    auto wakeLockQueue =
        std::make_unique<WakeLockQueue>(wakeLockDescriptor, true);
    if (!eventQueue->isValid() || eventQueue->getEventFlagWord() == nullptr ||
        !wakeLockQueue->isValid() ||
        wakeLockQueue->getEventFlagWord() == nullptr) {
      return badValue();
    }
    std::lock_guard<std::mutex> guard(lock_);
    if (!seedFromStableIdentity_) {
      bool stableNow = false;
      uint64_t refreshedSeed = resolveInstanceSeed(&stableNow);
      if (stableNow) {
        eventModel_ =
            SensorEventModel(buildSensorEventSpecs(targets_), refreshedSeed);
        seedFromStableIdentity_ = true;
      }
    }
    eventQueue_ = std::move(eventQueue);
    wakeLockQueue_ = std::move(wakeLockQueue);
    callback_ = sensorsCallback;
    eventModel_.disableAll();
    for (auto &[handle, state] : sensorStates_) {
      static_cast<void>(handle);
      state.active = false;
    }
    initialized_ = true;
    eventQueueQuantum_ = eventQueueDescriptor.quantum;
    wakeLockQueueQuantum_ = wakeLockDescriptor.quantum;
    eventCondition_.notify_all();
    return ScopedAStatus::ok();
  }

  ScopedAStatus injectSensorData(const Event &event) override {
    std::lock_guard<std::mutex> guard(lock_);
    if (operationMode_ != ISensors::OperationMode::DATA_INJECTION ||
        !isKnownSensorLocked(event.sensorHandle)) {
      return badValue();
    }
    return writeEventsLocked(std::vector<Event>{event}) ? ScopedAStatus::ok()
                                                        : noMemory();
  }

  ScopedAStatus registerDirectChannel(const ISensors::SharedMemInfo &,
                                      int32_t *out) override {
    *out = EX_UNSUPPORTED_OPERATION;
    return ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
  }

  ScopedAStatus setOperationMode(ISensors::OperationMode mode) override {
    std::lock_guard<std::mutex> guard(lock_);
    if (mode != ISensors::OperationMode::NORMAL &&
        mode != ISensors::OperationMode::DATA_INJECTION) {
      return badValue();
    }
    operationMode_ = mode;
    eventCondition_.notify_all();
    return ScopedAStatus::ok();
  }

  ScopedAStatus unregisterDirectChannel(int32_t) override {
    return ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
  }

private:
  static ScopedAStatus badValue() {
    return ScopedAStatus::fromServiceSpecificError(ISensors::ERROR_BAD_VALUE);
  }

  static ScopedAStatus noMemory() {
    return ScopedAStatus::fromServiceSpecificError(ISensors::ERROR_NO_MEMORY);
  }

  bool isKnownSensorLocked(int32_t sensorHandle) const {
    return sensorStates_.find(sensorHandle) != sensorStates_.end();
  }

  bool writeEventsLocked(const std::vector<Event> &events) {
    if (!initialized_ || eventQueue_ == nullptr || events.empty()) {
      return false;
    }
    constexpr int64_t kWriteTimeoutNs = 20'000'000;
    return eventQueue_->writeBlocking(
        events.data(), events.size(),
        static_cast<uint32_t>(ISensors::EVENT_QUEUE_FLAG_BITS_EVENTS_READ),
        static_cast<uint32_t>(
            ISensors::EVENT_QUEUE_FLAG_BITS_READ_AND_PROCESS),
        kWriteTimeoutNs);
  }

  void drainWakeLockQueueLocked() {
    if (wakeLockQueue_ == nullptr) {
      return;
    }
    int32_t handled = 0;
    while (wakeLockQueue_->availableToRead() > 0) {
      if (!wakeLockQueue_->read(&handled)) {
        return;
      }
    }
  }

  void runEventLoop() {
    std::unique_lock<std::mutex> lock(lock_);
    while (!stopEventThread_) {
      const int64_t waitNs =
          (!initialized_ || operationMode_ != ISensors::OperationMode::NORMAL)
              ? 5'000'000
              : eventModel_.nextWaitNs(bootTimeNanos());
      if (waitNs > 0) {
        eventCondition_.wait_for(lock, std::chrono::nanoseconds(waitNs));
      }
      if (stopEventThread_) {
        break;
      }
      drainWakeLockQueueLocked();
      if (!initialized_ || operationMode_ != ISensors::OperationMode::NORMAL) {
        continue;
      }
      const auto samples = eventModel_.collectDue(bootTimeNanos());
      if (samples.empty()) {
        continue;
      }
      std::vector<Event> events;
      events.reserve(samples.size());
      for (const auto &sample : samples) {
        events.push_back(toAidlEvent(sample));
      }
      static_cast<void>(writeEventsLocked(events));
    }
  }

  using EventQueue = AidlMessageQueue<Event, SynchronizedReadWrite>;
  using WakeLockQueue = AidlMessageQueue<int32_t, SynchronizedReadWrite>;

  std::vector<SensorTarget> targets_;
  std::mutex lock_;
  std::condition_variable eventCondition_;
  std::unordered_map<int32_t, SensorState> sensorStates_;
  std::shared_ptr<ISensorsCallback> callback_;
  SensorEventModel eventModel_;
  std::unique_ptr<EventQueue> eventQueue_;
  std::unique_ptr<WakeLockQueue> wakeLockQueue_;
  std::thread eventThread_;
  ISensors::OperationMode operationMode_ = ISensors::OperationMode::NORMAL;
  bool stopEventThread_ = false;
  bool initialized_ = false;
  bool seedFromStableIdentity_ = false;
  int32_t eventQueueQuantum_ = 0;
  int32_t wakeLockQueueQuantum_ = 0;
};

static binder_status_t
registerAsService(const std::shared_ptr<TargetSensors> &service) {
  const std::string instance = kSensorsServiceName;
  return AServiceManager_addService(service->asBinder().get(),
                                    instance.c_str());
}

int main() {
  ABinderProcess_setThreadPoolMaxThreadCount(1);
  auto service =
      ndk::SharedRefBase::make<TargetSensors>(loadSensors(cfgPath()));
  binder_status_t status = registerAsService(service);
  if (status != STATUS_OK) {
    return 2;
  }
#if CDH_SENSORS_MEIZU
  // 魅族变体：同进程注册 vendor.meizu.sensor_aux.ISensorAux/default。
  auto meizuSensorAux = cdh::sensors::RegisterMeizuSensorAux();
  (void)meizuSensorAux;
#endif
  ABinderProcess_joinThreadPool();
  return 0;
}
