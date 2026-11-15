// engine/vendor/hardware 线 —— android.hardware.vibrator（IVibrator/IVibratorManager v2）自研服务。
//
// 注册 android.hardware.vibrator.IVibrator/default（及按需 IVibratorManager/default），
// 供 framework Vibrator/VibratorManagerService 使用。本 HAL 是**能力/接口虚拟服务**：
// on/perform 立即成功（不驱动宿主振动、不影响 guest 输入设备），不宣称 amplitude control。
//
// 机型差异（是否注册 manager、能力位、组合上限、vibrator id 清单、支持效果清单）外置到
// 运行时配置 /vendor/etc/vibrator_config.xml（env VIBRATOR_CONFIG_XML 覆盖），由
// 设备集成层随镜像下发；本引擎不内置机型数值。读不到即回退保守默认（不宣称能力/效果）。
// 配置格式：
//   <vibrator-config register-manager="true" capabilities="0"
//                    manager-capabilities="0" composition-delay-max="1000"
//                    composition-size-max="256">
//     <vibrator id="1"/>
//     <effect name="CLICK"/>       <!-- 可重复，见 Effect 枚举 -->
//   </vibrator-config>
//
// 闭包最小：只用 std + libxml2（按 staticLibs 静态编入），-fno-exceptions（strtol，不用 stoi）。

#include <android/binder_ibinder.h>
#include <android/binder_status.h>

extern "C" {
void ABinderProcess_setThreadPoolMaxThreadCount(uint32_t numThreads);
void ABinderProcess_joinThreadPool(void);
binder_status_t AServiceManager_addService(AIBinder *binder,
                                           const char *instance);
}

#include <aidl/android/hardware/vibrator/BnVibrator.h>
#include <aidl/android/hardware/vibrator/BnVibratorManager.h>
#include <aidl/android/hardware/vibrator/Braking.h>
#include <aidl/android/hardware/vibrator/CompositeEffect.h>
#include <aidl/android/hardware/vibrator/CompositePrimitive.h>
#include <aidl/android/hardware/vibrator/Effect.h>
#include <aidl/android/hardware/vibrator/EffectStrength.h>
#include <aidl/android/hardware/vibrator/IVibrator.h>
#include <aidl/android/hardware/vibrator/IVibratorCallback.h>
#include <aidl/android/hardware/vibrator/IVibratorManager.h>
#include <aidl/android/hardware/vibrator/PrimitivePwle.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <cctype>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using aidl::android::hardware::vibrator::Braking;
using aidl::android::hardware::vibrator::BnVibrator;
using aidl::android::hardware::vibrator::BnVibratorManager;
using aidl::android::hardware::vibrator::CompositeEffect;
using aidl::android::hardware::vibrator::CompositePrimitive;
using aidl::android::hardware::vibrator::Effect;
using aidl::android::hardware::vibrator::EffectStrength;
using aidl::android::hardware::vibrator::IVibrator;
using aidl::android::hardware::vibrator::IVibratorCallback;
using aidl::android::hardware::vibrator::IVibratorManager;
using aidl::android::hardware::vibrator::PrimitivePwle;
using ndk::ScopedAStatus;

static constexpr const char *kVibratorServiceName =
    "android.hardware.vibrator.IVibrator/default";
static constexpr const char *kVibratorManagerServiceName =
    "android.hardware.vibrator.IVibratorManager/default";
static constexpr const char *kDefaultXmlPath = "/vendor/etc/vibrator_config.xml";

static ScopedAStatus unsupported() {
  return ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

static ScopedAStatus badValue() {
  return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
}

static void
completeCallback(const std::shared_ptr<IVibratorCallback> &callback) {
  if (callback != nullptr) {
    callback->onComplete();
  }
}

static void completeCallbackAfter(
    const std::shared_ptr<IVibratorCallback> &callback, int32_t timeoutMs) {
  if (callback == nullptr) {
    return;
  }
  std::thread([callback, timeoutMs]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));
    callback->onComplete();
  }).detach();
}

namespace {

// ---- libxml2 读取便利层（每个模块自带，不引跨模块头）----
// 读整个文件成 DOM；失败返回 nullptr（调用方回退内置默认）。
xmlDocPtr loadXmlDoc(const std::string &path) {
  return xmlReadFile(path.c_str(), nullptr,
                     XML_PARSE_NONET | XML_PARSE_NOBLANKS);
}

// 取属性值；缺失返回 false（值拷进 std::string，属性内存即时释放）。
bool readProp(const xmlNode *node, const char *name, std::string *out) {
  if (node == nullptr) return false;
  xmlChar *v = xmlGetProp(const_cast<xmlNode *>(node), BAD_CAST name);
  if (v == nullptr) return false;
  out->assign(reinterpret_cast<const char *>(v));
  xmlFree(v);
  return true;
}

// 第一个同名子元素（之后用 nextNamed 顺序遍历）。
xmlNode *firstChild(xmlNode *parent, const char *name) {
  if (parent == nullptr) return nullptr;
  for (xmlNode *c = parent->children; c != nullptr; c = c->next) {
    if (c->type == XML_ELEMENT_NODE && xmlStrcmp(c->name, BAD_CAST name) == 0) {
      return c;
    }
  }
  return nullptr;
}

// 下一个同名兄弟元素（跳过文本/注释节点与异名元素）。
xmlNode *nextNamed(xmlNode *node, const char *name) {
  if (node == nullptr) return nullptr;
  for (xmlNode *c = node->next; c != nullptr; c = c->next) {
    if (c->type == XML_ELEMENT_NODE && xmlStrcmp(c->name, BAD_CAST name) == 0) {
      return c;
    }
  }
  return nullptr;
}

bool parseBool(const std::string &v, bool *out) {
  std::string s = v;
  for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
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

bool parseInt(const std::string &v, int32_t *out) {
  const char *c = v.c_str();
  char *end = nullptr;
  long x = std::strtol(c, &end, 10);
  if (end == c || *end != '\0') return false;
  *out = static_cast<int32_t>(x);
  return true;
}

bool effectFromName(const std::string &name, Effect *out) {
  static const std::pair<const char *, Effect> kMap[] = {
      {"CLICK", Effect::CLICK},
      {"DOUBLE_CLICK", Effect::DOUBLE_CLICK},
      {"TICK", Effect::TICK},
      {"THUD", Effect::THUD},
      {"POP", Effect::POP},
      {"HEAVY_CLICK", Effect::HEAVY_CLICK},
      {"RINGTONE_1", Effect::RINGTONE_1},
      {"RINGTONE_2", Effect::RINGTONE_2},
      {"RINGTONE_3", Effect::RINGTONE_3},
      {"RINGTONE_4", Effect::RINGTONE_4},
      {"RINGTONE_5", Effect::RINGTONE_5},
      {"RINGTONE_6", Effect::RINGTONE_6},
      {"RINGTONE_7", Effect::RINGTONE_7},
      {"RINGTONE_8", Effect::RINGTONE_8},
      {"RINGTONE_9", Effect::RINGTONE_9},
      {"RINGTONE_10", Effect::RINGTONE_10},
      {"RINGTONE_11", Effect::RINGTONE_11},
      {"RINGTONE_12", Effect::RINGTONE_12},
      {"RINGTONE_13", Effect::RINGTONE_13},
      {"RINGTONE_14", Effect::RINGTONE_14},
      {"RINGTONE_15", Effect::RINGTONE_15},
      {"TEXTURE_TICK", Effect::TEXTURE_TICK},
  };
  for (const auto &kv : kMap) {
    if (name == kv.first) {
      *out = kv.second;
      return true;
    }
  }
  return false;
}

struct VibratorConfig {
  bool registerManager = true;
  int32_t ivibratorCapabilities = 0;
  int32_t ivibratorManagerCapabilities = 0;
  int32_t compositionDelayMax = 0;
  int32_t compositionSizeMax = 0;
  std::vector<int32_t> vibratorIds;
  std::vector<Effect> supportedEffects;
};

// 解析 XML 属性：缺属性保留编译期默认（配置缺失不能让服务起不来）。
VibratorConfig loadConfig(const std::string &path) {
  VibratorConfig cfg;
  xmlDocPtr doc = loadXmlDoc(path);
  if (doc == nullptr) return cfg;
  xmlNode *root = xmlDocGetRootElement(doc);
  if (root == nullptr) {
    xmlFreeDoc(doc);
    return cfg;
  }

  std::string v;
  int32_t n = 0;
  if (readProp(root, "register-manager", &v)) {
    parseBool(v, &cfg.registerManager);
  }
  if (readProp(root, "capabilities", &v) && parseInt(v, &n)) {
    cfg.ivibratorCapabilities = n;
  }
  if (readProp(root, "manager-capabilities", &v) && parseInt(v, &n)) {
    cfg.ivibratorManagerCapabilities = n;
  }
  if (readProp(root, "composition-delay-max", &v) && parseInt(v, &n)) {
    cfg.compositionDelayMax = n;
  }
  if (readProp(root, "composition-size-max", &v) && parseInt(v, &n)) {
    cfg.compositionSizeMax = n;
  }

  for (xmlNode *e = firstChild(root, "vibrator"); e != nullptr;
       e = nextNamed(e, "vibrator")) {
    if (readProp(e, "id", &v) && parseInt(v, &n)) {
      cfg.vibratorIds.push_back(n);
    }
  }
  for (xmlNode *e = firstChild(root, "effect"); e != nullptr;
       e = nextNamed(e, "effect")) {
    Effect effect;
    if (readProp(e, "name", &v) && effectFromName(v, &effect)) {
      cfg.supportedEffects.push_back(effect);
    }
  }

  xmlFreeDoc(doc);
  return cfg;
}

std::string cfgPath() {
  const char *env = std::getenv("VIBRATOR_CONFIG_XML");
  if (env != nullptr && env[0] != '\0') return env;
  return kDefaultXmlPath;
}

class TargetVibrator : public BnVibrator {
public:
  explicit TargetVibrator(VibratorConfig cfg) : cfg_(std::move(cfg)) {}

  ScopedAStatus getCapabilities(int32_t *out) override {
    *out = cfg_.ivibratorCapabilities;
    return ScopedAStatus::ok();
  }

  ScopedAStatus off() override { return ScopedAStatus::ok(); }

  ScopedAStatus on(int32_t timeoutMs,
                   const std::shared_ptr<IVibratorCallback> &callback) override {
    if (timeoutMs <= 0) {
      return badValue();
    }
    if ((cfg_.ivibratorCapabilities & IVibrator::CAP_ON_CALLBACK) != 0) {
      completeCallbackAfter(callback, timeoutMs);
    }
    return ScopedAStatus::ok();
  }

  ScopedAStatus perform(Effect, EffectStrength,
                        const std::shared_ptr<IVibratorCallback> &callback,
                        int32_t *out) override {
    *out = 0;
    if ((cfg_.ivibratorCapabilities & IVibrator::CAP_PERFORM_CALLBACK) != 0) {
      completeCallback(callback);
    }
    return ScopedAStatus::ok();
  }

  ScopedAStatus getSupportedEffects(std::vector<Effect> *out) override {
    *out = cfg_.supportedEffects;
    return ScopedAStatus::ok();
  }

  ScopedAStatus setAmplitude(float amplitude) override {
    if ((cfg_.ivibratorCapabilities & IVibrator::CAP_AMPLITUDE_CONTROL) == 0) {
      return unsupported();
    }
    if (amplitude <= 0.0f || amplitude > 1.0f) {
      return badValue();
    }
    return ScopedAStatus::ok();
  }

  ScopedAStatus setExternalControl(bool enabled) override {
    return enabled ? unsupported() : ScopedAStatus::ok();
  }

  ScopedAStatus getCompositionDelayMax(int32_t *out) override {
    *out = cfg_.compositionDelayMax;
    return ScopedAStatus::ok();
  }

  ScopedAStatus getCompositionSizeMax(int32_t *out) override {
    *out = cfg_.compositionSizeMax;
    return ScopedAStatus::ok();
  }

  ScopedAStatus
  getSupportedPrimitives(std::vector<CompositePrimitive> *out) override {
    out->clear();
    return ScopedAStatus::ok();
  }

  ScopedAStatus getPrimitiveDuration(CompositePrimitive, int32_t *out) override {
    *out = 0;
    return unsupported();
  }

  ScopedAStatus
  compose(const std::vector<CompositeEffect> &composite,
          const std::shared_ptr<IVibratorCallback> &callback) override {
    if (composite.empty()) {
      return badValue();
    }
    (void)callback;
    return ScopedAStatus::ok();
  }

  ScopedAStatus getSupportedAlwaysOnEffects(std::vector<Effect> *out) override {
    out->clear();
    return ScopedAStatus::ok();
  }

  ScopedAStatus alwaysOnEnable(int32_t, Effect, EffectStrength) override {
    return unsupported();
  }

  ScopedAStatus alwaysOnDisable(int32_t) override { return unsupported(); }

  ScopedAStatus getResonantFrequency(float *out) override {
    *out = 0.0f;
    return unsupported();
  }

  ScopedAStatus getQFactor(float *out) override {
    *out = 0.0f;
    return unsupported();
  }

  ScopedAStatus getFrequencyResolution(float *out) override {
    *out = 0.0f;
    return unsupported();
  }

  ScopedAStatus getFrequencyMinimum(float *out) override {
    *out = 0.0f;
    return unsupported();
  }

  ScopedAStatus getBandwidthAmplitudeMap(std::vector<float> *out) override {
    out->clear();
    return unsupported();
  }

  ScopedAStatus getPwlePrimitiveDurationMax(int32_t *out) override {
    *out = 0;
    return unsupported();
  }

  ScopedAStatus getPwleCompositionSizeMax(int32_t *out) override {
    *out = 0;
    return unsupported();
  }

  ScopedAStatus getSupportedBraking(std::vector<Braking> *out) override {
    out->clear();
    return ScopedAStatus::ok();
  }

  ScopedAStatus
  composePwle(const std::vector<PrimitivePwle> &,
              const std::shared_ptr<IVibratorCallback> &) override {
    return unsupported();
  }

private:
  VibratorConfig cfg_;
};

class TargetVibratorManager : public BnVibratorManager {
public:
  TargetVibratorManager(std::shared_ptr<IVibrator> vibrator, VibratorConfig cfg)
      : vibrator_(std::move(vibrator)), cfg_(std::move(cfg)) {}

  ScopedAStatus getCapabilities(int32_t *out) override {
    *out = cfg_.ivibratorManagerCapabilities;
    return ScopedAStatus::ok();
  }

  ScopedAStatus getVibratorIds(std::vector<int32_t> *out) override {
    *out = cfg_.vibratorIds;
    return ScopedAStatus::ok();
  }

  ScopedAStatus getVibrator(int32_t vibratorId,
                            std::shared_ptr<IVibrator> *out) override {
    bool known = false;
    for (const int32_t id : cfg_.vibratorIds) {
      if (id == vibratorId) {
        known = true;
        break;
      }
    }
    if (!known) {
      return badValue();
    }
    *out = vibrator_;
    return ScopedAStatus::ok();
  }

  ScopedAStatus prepareSynced(const std::vector<int32_t> &vibratorIds) override {
    const std::vector<int32_t> &known = cfg_.vibratorIds;
    if (vibratorIds.size() != known.size()) {
      return badValue();
    }
    for (size_t index = 0; index < known.size(); ++index) {
      if (vibratorIds[index] != known[index]) {
        return badValue();
      }
    }
    return ScopedAStatus::ok();
  }

  ScopedAStatus
  triggerSynced(const std::shared_ptr<IVibratorCallback> &callback) override {
    completeCallback(callback);
    return ScopedAStatus::ok();
  }

  ScopedAStatus cancelSynced() override { return ScopedAStatus::ok(); }

private:
  std::shared_ptr<IVibrator> vibrator_;
  VibratorConfig cfg_;
};

binder_status_t registerAsService(
    const std::shared_ptr<ndk::ICInterface> &service, const char *name) {
  return AServiceManager_addService(service->asBinder().get(), name);
}

}  // namespace

int main() {
  ABinderProcess_setThreadPoolMaxThreadCount(2);
  VibratorConfig cfg = loadConfig(cfgPath());
  auto vibrator = ndk::SharedRefBase::make<TargetVibrator>(cfg);

  if (registerAsService(vibrator, kVibratorServiceName) != STATUS_OK) {
    return 2;
  }
  if (cfg.registerManager) {
    auto manager =
        ndk::SharedRefBase::make<TargetVibratorManager>(vibrator, cfg);
    if (registerAsService(manager, kVibratorManagerServiceName) != STATUS_OK) {
      return 3;
    }
  }

  ABinderProcess_joinThreadPool();
  return 0;
}
