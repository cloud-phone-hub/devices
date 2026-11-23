// engine/vendor/hardware 线 —— android.hardware.light（ILights v2）自研服务实现。
//
// 注册 android.hardware.light.ILights/default，供 framework LightsService 枚举灯并
// 接收 setLightState。本 HAL 是**虚拟 lights HAL**：setLightState 只记录状态，不驱动
// 宿主屏幕亮度，也不写 guest sysfs LED。
//
// 机型差异（有哪些灯、id/ordinal/type 组合）外置到运行时配置文件
// /vendor/etc/light_config.xml（env LIGHT_CONFIG_XML 可覆盖路径），由设备集成层随镜像
// 下发；本引擎不内置任何机型灯清单。读不到或为空则回退空清单（getLights 返回空，
// framework 容忍无灯）。配置格式：
//   <lights-config>
//     <light id="0" ordinal="0" type="BACKLIGHT"/>
//   </lights-config>
// type ∈ BACKLIGHT KEYBOARD BUTTONS BATTERY NOTIFICATIONS ATTENTION BLUETOOTH WIFI
//        MICROPHONE CAMERA（见 LightType 枚举）。
//
// 闭包最小：只用 std + libxml2（按 staticLibs 静态编入），-fno-exceptions（用 strtol，不用 std::stoi）。

#include <android/binder_ibinder.h>
#include <android/binder_status.h>

extern "C" {
void ABinderProcess_setThreadPoolMaxThreadCount(uint32_t numThreads);
void ABinderProcess_joinThreadPool(void);
binder_status_t AServiceManager_addService(AIBinder *binder,
                                           const char *instance);
}

#include <aidl/android/hardware/light/BnLights.h>
#include <aidl/android/hardware/light/BrightnessMode.h>
#include <aidl/android/hardware/light/FlashMode.h>
#include <aidl/android/hardware/light/HwLight.h>
#include <aidl/android/hardware/light/HwLightState.h>
#include <aidl/android/hardware/light/LightType.h>
#if CDH_LIGHT_MEIZU
#include "meizu/light_meizu_ext.h"
#endif

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <cctype>
#include <cstdlib>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

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

using aidl::android::hardware::light::BnLights;
using aidl::android::hardware::light::BrightnessMode;
using aidl::android::hardware::light::FlashMode;
using aidl::android::hardware::light::HwLight;
using aidl::android::hardware::light::HwLightState;
using aidl::android::hardware::light::LightType;
using ndk::ScopedAStatus;

static constexpr const char *kLightsServiceName =
    "android.hardware.light.ILights/default";
static constexpr const char *kDefaultXmlPath = "/vendor/etc/light_config.xml";

static ScopedAStatus badValue() {
  return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
}

static HwLight makeLight(int32_t id, int32_t ordinal, LightType type) {
  HwLight light;
  light.id = id;
  light.ordinal = ordinal;
  light.type = type;
  return light;
}

static HwLightState defaultStateFor(LightType type) {
  HwLightState state;
  state.color = type == LightType::BATTERY ? 0xff00ff00 : 0x00000000;
  state.flashMode = FlashMode::NONE;
  state.flashOnMs = 0;
  state.flashOffMs = 0;
  state.brightnessMode = BrightnessMode::USER;
  return state;
}

namespace {

// LightType 枚举名 → 值（与 aidl LightType 一致，0..9）
bool lightTypeFromName(const std::string &name, LightType *out) {
  static const std::pair<const char *, LightType> kMap[] = {
      {"BACKLIGHT", LightType::BACKLIGHT},
      {"KEYBOARD", LightType::KEYBOARD},
      {"BUTTONS", LightType::BUTTONS},
      {"BATTERY", LightType::BATTERY},
      {"NOTIFICATIONS", LightType::NOTIFICATIONS},
      {"ATTENTION", LightType::ATTENTION},
      {"BLUETOOTH", LightType::BLUETOOTH},
      {"WIFI", LightType::WIFI},
      {"MICROPHONE", LightType::MICROPHONE},
      {"CAMERA", LightType::CAMERA},
  };
  for (const auto &kv : kMap) {
    if (name == kv.first) {
      *out = kv.second;
      return true;
    }
  }
  return false;
}

// 内置默认灯清单：当机型未下发 /vendor/etc/light_config.xml 时回退，避免定制 ROM
// （如 ASUS ROG UI 的 NotificationManagerService.updateLightsLocked）因 NOTIFICATIONS
// 或 ATTENTION 灯缺失为空直接调用 LogicalLight.turnOff() 触发 NullPointerException。
std::vector<HwLight> defaultLights() {
  return {
      makeLight(0, 0, LightType::BACKLIGHT),
      makeLight(1, 0, LightType::KEYBOARD),
      makeLight(2, 0, LightType::BUTTONS),
      makeLight(3, 0, LightType::BATTERY),
      makeLight(4, 0, LightType::NOTIFICATIONS),
      makeLight(5, 0, LightType::ATTENTION),
      makeLight(6, 1, LightType::BLUETOOTH),
      makeLight(7, 1, LightType::WIFI),
  };
}

// 运行时读 <light id ordinal type> 清单；文件不存在或解析失败时回退内置默认灯清单。
std::vector<HwLight> loadLights(const std::string &path) {
  std::vector<HwLight> lights;
  xmlDocPtr doc = loadXmlDoc(path);
  if (doc == nullptr) return defaultLights();
  xmlNode *root = xmlDocGetRootElement(doc);
  if (root == nullptr) {
    xmlFreeDoc(doc);
    return defaultLights();
  }

  std::string id;
  std::string ordinal;
  std::string type;
  for (xmlNode *e = firstChild(root, "light"); e != nullptr;
       e = nextNamed(e, "light")) {
    if (!readProp(e, "id", &id) || !readProp(e, "ordinal", &ordinal) ||
        !readProp(e, "type", &type)) {
      continue;
    }
    char *end = nullptr;
    const long idValue = std::strtol(id.c_str(), &end, 10);
    if (end == id.c_str() || *end != '\0') continue;
    const long ordinalValue = std::strtol(ordinal.c_str(), &end, 10);
    if (end == ordinal.c_str() || *end != '\0') continue;
    LightType lightType;
    if (!lightTypeFromName(type, &lightType)) continue;
    lights.push_back(makeLight(static_cast<int32_t>(idValue),
                               static_cast<int32_t>(ordinalValue), lightType));
  }
  xmlFreeDoc(doc);
  return lights;
}

std::string cfgPath() {
  const char *env = std::getenv("LIGHT_CONFIG_XML");
  if (env != nullptr && env[0] != '\0') return env;
  return kDefaultXmlPath;
}

class TargetLights : public BnLights {
public:
  TargetLights() : lights_(loadLights(cfgPath())) {
    for (const auto &light : lights_) {
      states_[light.id] = defaultStateFor(light.type);
    }
  }

  ScopedAStatus setLightState(int32_t id,
                              const HwLightState &state) override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (states_.find(id) == states_.end()) {
      return badValue();
    }
    states_[id] = state;
    return ScopedAStatus::ok();
  }

  ScopedAStatus getLights(std::vector<HwLight> *out) override {
    *out = lights_;
    return ScopedAStatus::ok();
  }

private:
  const std::vector<HwLight> lights_;
  std::mutex mutex_;
  std::unordered_map<int32_t, HwLightState> states_;
};

binder_status_t registerAsService(
    const std::shared_ptr<ndk::ICInterface> &service, const char *name) {
  return AServiceManager_addService(service->asBinder().get(), name);
}

}  // namespace

int main() {
  ABinderProcess_setThreadPoolMaxThreadCount(2);
  auto lights = ndk::SharedRefBase::make<TargetLights>();

  if (registerAsService(lights, kLightsServiceName) != STATUS_OK) {
    return 2;
  }
#if CDH_LIGHT_MEIZU
  // 魅族变体：同进程注册 vendor.meizu.hardware.aw_light.IAwLight/default。
  auto meizuAwLight = cdh::light::RegisterMeizuAwLight();
  (void)meizuAwLight;
#endif

  ABinderProcess_joinThreadPool();
  return 0;
}
