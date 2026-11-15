// engine/vendor/hardware 线 —— android.hardware.health（IHealth v3）自研服务实现。
//
// 注册 android.hardware.health.IHealth/default，供 framework BatteryService 取电池数据。
// 云端虚拟机没有真实电池硬件（/sys/class/power_supply 为空、内核无电池驱动），电池
// 数据由本服务模拟：BatteryProfile 的每一项（充电器在线状态、health/level/容量、
// charge_counter 与 voltage/temperature 区间等）都能被设备集成层配置
// /vendor/etc/battery_config.xml 覆盖；动态字段按开机时钟做挂充补电周期模拟，
// 并周期性调用 update() 推送回调。
//
// 机型差异化一律放 /vendor/etc/battery_config.xml（env BATTERY_CONFIG_XML 可覆盖路径，
// gold 的取值来自真机取证），本文件只保留一份可跨设备复用的模拟实现与通用缺省值；
// 配置缺失时仍按缺省口径运行。格式（各段与属性均可缺省，缺省即保留内置默认）：
//   <battery>
//     <identity technology="Li-poly" cycle_count="753" level="100" health="GOOD"
//               capacity_level="FULL" charging_state="INVALID" present="1"/>
//     <charger ac="1" usb="0" wireless="0" dock="0"
//              max_current_ua="0" max_voltage_uv="0"/>
//     <capacity full_uah="5000" counter_min="4947" counter_max="4972"/>
//     <cycle full_mv="4384" charge_mv="4450" temperature_full_decic="310"
//            temperature_charge_decic="380" seconds="600"/>
//     <health_data state_of_health="99" part_status="UNSUPPORTED"
//                  manufacturing_date_seconds="0" first_usage_seconds="0"/>
//   </battery>
//
// 闭包最小：只用 std + libxml2（按 staticLibs 静态编入），-fno-exceptions。
//
// 存储/磁盘统计：云机无 eMMC 健康与块设备统计需求，返回空数组。
//
// 厂商专属扩展分支不在本文件：
//   - 三星（OneUI）需在同一个 IHealth binder 上挂 vendor.samsung.hardware.health.ISehHealth
//     扩展，位于 src/samsung/，只在 `--variant samsung` 时编入（-DCDH_HEALTH_SAMSUNG=1）；
//   - 欧加（OPPO / OnePlus / realme）需按 VINTF 同进程注册 vendor.oplus.hardware.charger.ICharger，
//     位于 src/oplus/，只在 `--variant oplus` 时编入（-DCDH_HEALTH_OPLUS=1）；
//   其他云机走纯 AOSP 基础版，零跨厂商符号。

#include <android/binder_ibinder.h>
#include <android/binder_interface_utils.h>
#include <android/binder_status.h>
#include <android/log.h>

extern "C" {
void ABinderProcess_setThreadPoolMaxThreadCount(uint32_t numThreads);
void ABinderProcess_joinThreadPool(void);
binder_status_t AServiceManager_addService(AIBinder *binder,
                                           const char *instance);
}

#include <aidl/android/hardware/health/BatteryCapacityLevel.h>
#include <aidl/android/hardware/health/BatteryChargingPolicy.h>
#include <aidl/android/hardware/health/BatteryChargingState.h>
#include <aidl/android/hardware/health/BatteryHealth.h>
#include <aidl/android/hardware/health/BatteryHealthData.h>
#include <aidl/android/hardware/health/BatteryPartStatus.h>
#include <aidl/android/hardware/health/BatteryStatus.h>
#include <aidl/android/hardware/health/BnHealth.h>
#include <aidl/android/hardware/health/DiskStats.h>
#include <aidl/android/hardware/health/HealthInfo.h>
#if CDH_ANDROID_VERSION >= 16
#include <aidl/android/hardware/health/HingeInfo.h>
#endif
#include <aidl/android/hardware/health/IHealthInfoCallback.h>
#include <aidl/android/hardware/health/StorageInfo.h>
// 欧加变体（ColorOS / Realme UI 需要 ICharger/default）：只在 --variant oplus 时编入，
// 见 src/oplus/CdhOplusCharger.h 与 modules/android.hardware.health.json 的 variants。
#if CDH_HEALTH_OPLUS
#include "oplus/CdhOplusCharger.h"
#endif
// 三星变体（OneUI 需要 ISehHealth 旁路）：只在 --variant samsung 时编入，
// 见 src/samsung/seh_health.h 与 modules/android.hardware.health.json 的 variants。
#if CDH_HEALTH_SAMSUNG
#include "samsung/seh_health.h"
#endif

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using aidl::android::hardware::health::BatteryCapacityLevel;
using aidl::android::hardware::health::BatteryChargingPolicy;
using aidl::android::hardware::health::BatteryChargingState;
using aidl::android::hardware::health::BatteryHealth;
using aidl::android::hardware::health::BatteryHealthData;
using aidl::android::hardware::health::BatteryPartStatus;
using aidl::android::hardware::health::BatteryStatus;
using aidl::android::hardware::health::BnHealth;
using aidl::android::hardware::health::DiskStats;
using aidl::android::hardware::health::HealthInfo;
#if CDH_ANDROID_VERSION >= 16
using aidl::android::hardware::health::HingeInfo;
#endif
using aidl::android::hardware::health::IHealthInfoCallback;
using aidl::android::hardware::health::StorageInfo;
using ndk::ScopedAStatus;

static constexpr const char *kHealthServiceName =
    "android.hardware.health.IHealth/default";

namespace {

constexpr const char *kTag = "cdh_health";

// 设备集成层提供的机型电池口径；不存在时用下方通用缺省值。
constexpr const char *kDefaultXmlPath = "/vendor/etc/battery_config.xml";

// 动态模拟的推送周期。真机挂充时 BatteryService 的 seq 随补电上报持续增长，
// 这里按 30s 推送一次，节奏接近真机 health HAL 的轮询。
constexpr int32_t kUpdateIntervalSeconds = 30;

// 一个挂充周期内用于补电的相位占比：其余时间为满电静置。
// 真机取证为「满电静置（FULL/4384mV/31.0°C/4962）↔ 补电（CHARGING/
// 4450mV/38.0°C/4947）」两种相位交替。
constexpr double kChargePhaseFraction = 0.25;

// 通用云机电池口径。下面每个字段都能被 /vendor/etc/battery_config.xml 的对应属性覆盖，
// 此处只是「没有该配置」时的兜底缺省，不代表任何具体机型。
struct BatteryProfile {
  // —— 静态身份 ——
  bool acOnline = true;
  bool usbOnline = true;
  bool wirelessOnline = false;
  bool dockOnline = false;
  int32_t maxChargingCurrentUa = 0;
  int32_t maxChargingVoltageUv = 0;
  BatteryHealth health = BatteryHealth::GOOD;
  bool present = true;
  int32_t level = 100;
  int32_t cycleCount = 0;
#if CDH_ANDROID_VERSION >= 16
  // 仅兜底：android16 对标设备的 6000 mAh（单位 µAh）口径应写在对应设备的
  // battery_config.xml（<capacity full_uah counter_min counter_max>）。
  int32_t fullChargeUah = 6000000;
  int32_t chargeCounterMin = 5960000;
  int32_t chargeCounterMax = 5970000;
#else
  // 仅兜底：gold 真机 intent extra 同量级（49xx µAh），实际取值见
  // engine/manifest/xiaomi/gold/vendor/etc/battery_config.xml。
  int32_t fullChargeUah = 5000;
  int32_t chargeCounterMin = 4967;
  int32_t chargeCounterMax = 4972;
#endif
  std::string technology = "Li-poly";
  BatteryCapacityLevel capacityLevel = BatteryCapacityLevel::FULL;
  BatteryChargingState chargingState = BatteryChargingState::INVALID;
  int64_t manufacturingDateSeconds = 0;
  int64_t firstUsageSeconds = 0;
  int64_t stateOfHealth = 99;
  std::string serialNumber;
  BatteryPartStatus partStatus = BatteryPartStatus::UNSUPPORTED;

  // —— 动态模拟区间 ——
  int32_t voltageFullMv = 4389;
  int32_t voltageChargeMv = 4450;
  int32_t temperatureFullDeciC = 290;
  int32_t temperatureChargeDeciC = 380;
  int32_t cycleSeconds = 600;
  bool hasCustomCurrent = false;
  int32_t currentFullUa = 0;
  int32_t currentChargeUa = 900000;
};

BatteryProfile gProfile;

// 补电/静置相位下当前时刻的电池快照。
struct BatterySample {
  BatteryStatus status;
  int32_t voltageMv;
  int32_t temperatureDeciC;
  int32_t currentUa;
  int32_t currentAvgUa;
  int32_t chargeCounter;
};

std::string trim(const std::string &text) {
  const char *ws = " \t\r\n";
  const size_t begin = text.find_first_not_of(ws);
  if (begin == std::string::npos) {
    return "";
  }
  const size_t end = text.find_last_not_of(ws);
  return text.substr(begin, end - begin + 1);
}

bool parseInt(const std::string &text, int32_t *out) {
  char *end = nullptr;
  const long value = std::strtol(text.c_str(), &end, 10);
  if (end == text.c_str() || *end != '\0') {
    return false;
  }
  *out = static_cast<int32_t>(value);
  return true;
}

bool parseInt64(const std::string &text, int64_t *out) {
  char *end = nullptr;
  const long long value = std::strtoll(text.c_str(), &end, 10);
  if (end == text.c_str() || *end != '\0') {
    return false;
  }
  *out = static_cast<int64_t>(value);
  return true;
}

// 布尔键支持 1/0、true/false、yes/no、on/off 四种写法。
bool parseBool(const std::string &text, bool *out) {
  if (text == "1" || text == "true" || text == "yes" || text == "on") {
    *out = true;
    return true;
  }
  if (text == "0" || text == "false" || text == "no" || text == "off") {
    *out = false;
    return true;
  }
  return false;
}

// 枚举属性按名字解析（XML 里写 GOOD / FULL 这类），不用数字以免 AIDL 版本间错位。
template <size_t N, typename Enum>
bool parseEnum(const std::string &text, const std::pair<const char *, Enum> (&table)[N],
               Enum *out) {
  for (const auto &item : table) {
    if (text == item.first) {
      *out = item.second;
      return true;
    }
  }
  return false;
}

// 各枚举的合法名字表；XML 属性值必须是表里的名字。
const std::pair<const char *, BatteryHealth> kHealthTable[] = {
    {"UNKNOWN", BatteryHealth::UNKNOWN},
    {"GOOD", BatteryHealth::GOOD},
    {"OVERHEAT", BatteryHealth::OVERHEAT},
    {"DEAD", BatteryHealth::DEAD},
    {"OVER_VOLTAGE", BatteryHealth::OVER_VOLTAGE},
    {"UNSPECIFIED_FAILURE", BatteryHealth::UNSPECIFIED_FAILURE},
    {"COLD", BatteryHealth::COLD},
    {"FAIR", BatteryHealth::FAIR},
};

const std::pair<const char *, BatteryCapacityLevel> kCapacityLevelTable[] = {
    {"UNSUPPORTED", BatteryCapacityLevel::UNSUPPORTED},
    {"UNKNOWN", BatteryCapacityLevel::UNKNOWN},
    {"CRITICAL", BatteryCapacityLevel::CRITICAL},
    {"LOW", BatteryCapacityLevel::LOW},
    {"NORMAL", BatteryCapacityLevel::NORMAL},
    {"HIGH", BatteryCapacityLevel::HIGH},
    {"FULL", BatteryCapacityLevel::FULL},
};

const std::pair<const char *, BatteryChargingState> kChargingStateTable[] = {
    {"INVALID", BatteryChargingState::INVALID},
    {"NORMAL", BatteryChargingState::NORMAL},
    {"TOO_COLD", BatteryChargingState::TOO_COLD},
    {"TOO_HOT", BatteryChargingState::TOO_HOT},
    {"LONG_LIFE", BatteryChargingState::LONG_LIFE},
    {"ADAPTIVE", BatteryChargingState::ADAPTIVE},
};

const std::pair<const char *, BatteryPartStatus> kPartStatusTable[] = {
    {"UNSUPPORTED", BatteryPartStatus::UNSUPPORTED},
    {"ORIGINAL", BatteryPartStatus::ORIGINAL},
    {"REPLACED", BatteryPartStatus::REPLACED},
};

// ---- libxml2 读取便利层（本模块自用，不进公共头）----
// 读整个文件成 DOM；失败返回 nullptr（调用方回退内置默认）。
xmlDocPtr loadXmlDoc(const std::string &path) {
  return xmlReadFile(path.c_str(), nullptr, XML_PARSE_NONET | XML_PARSE_NOBLANKS);
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

// 第一个同名子元素。
xmlNode *firstChild(xmlNode *parent, const char *name) {
  if (parent == nullptr) return nullptr;
  for (xmlNode *c = parent->children; c != nullptr; c = c->next) {
    if (c->type == XML_ELEMENT_NODE && xmlStrcmp(c->name, BAD_CAST name) == 0) {
      return c;
    }
  }
  return nullptr;
}

// 以下 read*Attr 在属性缺失或值非法时返回 false，调用方保留缺省值。
bool readIntAttr(const xmlNode *node, const char *name, int32_t lo, int32_t hi,
                 int32_t *out) {
  std::string text;
  if (!readProp(node, name, &text)) return false;
  int32_t value = 0;
  if (!parseInt(trim(text), &value) || value < lo || value > hi) return false;
  *out = value;
  return true;
}

bool readInt64Attr(const xmlNode *node, const char *name, int64_t lo, int64_t hi,
                   int64_t *out) {
  std::string text;
  if (!readProp(node, name, &text)) return false;
  int64_t value = 0;
  if (!parseInt64(trim(text), &value) || value < lo || value > hi) return false;
  *out = value;
  return true;
}

bool readBoolAttr(const xmlNode *node, const char *name, bool *out) {
  std::string text;
  if (!readProp(node, name, &text)) return false;
  return parseBool(trim(text), out);
}

// 字符串属性允许显式写空串（如 serial_number=""），语义是“清空”。
bool readStrAttr(const xmlNode *node, const char *name, std::string *out) {
  std::string text;
  if (!readProp(node, name, &text)) return false;
  *out = trim(text);
  return true;
}

template <size_t N, typename Enum>
bool readEnumAttr(const xmlNode *node, const char *name,
                  const std::pair<const char *, Enum> (&table)[N], Enum *out) {
  std::string text;
  if (!readProp(node, name, &text)) return false;
  return parseEnum(trim(text), table, out);
}

std::string cfgPath() {
  const char *env = std::getenv("BATTERY_CONFIG_XML");
  if (env != nullptr && env[0] != '\0') {
    return env;
  }
  return kDefaultXmlPath;
}

// 读取 XML 形式的机型口径；文件缺失或元素/属性缺失时保留对应字段的缺省值。
void loadProfile(BatteryProfile *profile) {
  const std::string path = cfgPath();
  xmlDocPtr doc = loadXmlDoc(path);
  if (doc == nullptr) {
    __android_log_print(ANDROID_LOG_INFO, kTag, "未找到 %s，使用通用缺省口径", path.c_str());
    return;
  }
  xmlNode *root = xmlDocGetRootElement(doc);
  if (root == nullptr || xmlStrcmp(root->name, BAD_CAST "battery") != 0) {
    __android_log_print(ANDROID_LOG_WARN, kTag, "%s 根元素不是 <battery>，使用通用缺省口径",
                        path.c_str());
    xmlFreeDoc(doc);
    return;
  }

  int applied = 0;
  auto take = [&applied](bool ok) {
    if (ok) {
      ++applied;
    }
  };

  // <identity>：设备身份与电量
  const xmlNode *identity = firstChild(root, "identity");
  take(readStrAttr(identity, "technology", &profile->technology));
  take(readIntAttr(identity, "cycle_count", 0, INT32_MAX, &profile->cycleCount));
  take(readIntAttr(identity, "level", 0, 100, &profile->level));
  take(readEnumAttr(identity, "health", kHealthTable, &profile->health));
  take(readEnumAttr(identity, "capacity_level", kCapacityLevelTable, &profile->capacityLevel));
  take(readEnumAttr(identity, "charging_state", kChargingStateTable, &profile->chargingState));
  take(readBoolAttr(identity, "present", &profile->present));

  // <charger>：充电器在线状态与充电上限
  const xmlNode *charger = firstChild(root, "charger");
  take(readBoolAttr(charger, "ac", &profile->acOnline));
  take(readBoolAttr(charger, "usb", &profile->usbOnline));
  take(readBoolAttr(charger, "wireless", &profile->wirelessOnline));
  take(readBoolAttr(charger, "dock", &profile->dockOnline));
  take(readIntAttr(charger, "max_current_ua", 0, INT32_MAX, &profile->maxChargingCurrentUa));
  take(readIntAttr(charger, "max_voltage_uv", 0, INT32_MAX, &profile->maxChargingVoltageUv));

  // <capacity>：额定容量与 charge_counter 区间
  const xmlNode *capacity = firstChild(root, "capacity");
  take(readIntAttr(capacity, "full_uah", 1, INT32_MAX, &profile->fullChargeUah));
  take(readIntAttr(capacity, "counter_min", 0, INT32_MAX, &profile->chargeCounterMin));
  take(readIntAttr(capacity, "counter_max", 0, INT32_MAX, &profile->chargeCounterMax));

  // <cycle>：挂充/静置两相位的电压、温度、实时电流与周期
  const xmlNode *cycle = firstChild(root, "cycle");
  take(readIntAttr(cycle, "full_mv", 0, INT32_MAX, &profile->voltageFullMv));
  take(readIntAttr(cycle, "charge_mv", 0, INT32_MAX, &profile->voltageChargeMv));
  take(readIntAttr(cycle, "temperature_full_decic", 0, INT32_MAX,
                   &profile->temperatureFullDeciC));
  take(readIntAttr(cycle, "temperature_charge_decic", 0, INT32_MAX,
                   &profile->temperatureChargeDeciC));
  take(readIntAttr(cycle, "seconds", 1, INT32_MAX, &profile->cycleSeconds));
  const bool gotFullCur =
      readIntAttr(cycle, "current_full_ua", INT32_MIN, INT32_MAX, &profile->currentFullUa);
  const bool gotChargeCur =
      readIntAttr(cycle, "current_charge_ua", INT32_MIN, INT32_MAX, &profile->currentChargeUa);
  if (gotFullCur || gotChargeCur) {
    profile->hasCustomCurrent = true;
    take(true);
  }

  // <health_data>：getBatteryHealthData 的档案字段（serial_number 写空串即清空）
  const xmlNode *healthData = firstChild(root, "health_data");
  take(readInt64Attr(healthData, "state_of_health", 0, 100, &profile->stateOfHealth));
  take(readInt64Attr(healthData, "manufacturing_date_seconds", 0, INT64_MAX,
                     &profile->manufacturingDateSeconds));
  take(readInt64Attr(healthData, "first_usage_seconds", 0, INT64_MAX,
                     &profile->firstUsageSeconds));
  take(readStrAttr(healthData, "serial_number", &profile->serialNumber));
  take(readEnumAttr(healthData, "part_status", kPartStatusTable, &profile->partStatus));

  xmlFreeDoc(doc);

  if (profile->chargeCounterMin > profile->chargeCounterMax) {
    __android_log_print(ANDROID_LOG_WARN, kTag,
                        "counter_min > counter_max，回退到较大值");
    profile->chargeCounterMin = profile->chargeCounterMax;
  }

  __android_log_print(ANDROID_LOG_INFO, kTag,
                      "已加载 %s（%d 项）：level=%d health=%s ac=%d usb=%d "
                      "present=%d full_charge=%duAh counter=%d..%d",
                      path.c_str(), applied, profile->level,
                      aidl::android::hardware::health::toString(profile->health).c_str(),
                      profile->acOnline, profile->usbOnline, profile->present,
                      profile->fullChargeUah, profile->chargeCounterMin,
                      profile->chargeCounterMax);
  __android_log_print(ANDROID_LOG_INFO, kTag,
                      "动态区间：voltage=%d..%dmV temperature=%d..%d(0.1C) cycle=%ds "
                      "cycle_count=%d technology=%s",
                      profile->voltageFullMv, profile->voltageChargeMv,
                      profile->temperatureFullDeciC, profile->temperatureChargeDeciC,
                      profile->cycleSeconds, profile->cycleCount, profile->technology.c_str());
}

int64_t bootSeconds() {
  timespec now{};
  clock_gettime(CLOCK_BOOTTIME, &now);
  return static_cast<int64_t>(now.tv_sec);
}

// 挂充周期：当任一充电器在线时，前 kChargePhaseFraction 补电（counter 升、电压/温度升、
// 有充电电流），其余时间满电静置（counter 缓降、温度回落、电流为 0）；
// 当所有充电器均离线（未插电口径，如 ac=0/usb=0/wireless=0/dock=0）时，恒报
// DISCHARGING（或满电时 NOT_CHARGING）与平稳待机放电参数，避免无充电器却上报
// CHARGING/FULL 的物理状态矛盾。
BatterySample sampleBattery(const BatteryProfile &profile) {
  const int64_t cycle =
      profile.cycleSeconds > 0 ? profile.cycleSeconds : 600;
  const int64_t now = bootSeconds();
  const double phase = static_cast<double>(now % cycle) / cycle;

  BatterySample sample{};
  auto lerp = [](int32_t from, int32_t to, double t) {
    return from + static_cast<int32_t>(std::lround((to - from) * t));
  };

  const bool anyChargerOnline = profile.acOnline || profile.usbOnline ||
                                profile.wirelessOnline || profile.dockOnline;
  if (!anyChargerOnline) {
    sample.status = (profile.level >= 100) ? BatteryStatus::NOT_CHARGING
                                           : BatteryStatus::DISCHARGING;
    sample.chargeCounter =
        lerp(profile.chargeCounterMax, profile.chargeCounterMin, phase);
    sample.voltageMv =
        profile.voltageFullMv + static_cast<int32_t>((now / 7) % 3) - 1;
    sample.temperatureDeciC = profile.temperatureFullDeciC;
    const int32_t jitter = (static_cast<int32_t>((now / 5) % 9) - 4) * 150;
    const int32_t baseUa =
        profile.hasCustomCurrent ? profile.currentFullUa : -185000;
    sample.currentUa = baseUa + jitter;
    sample.currentAvgUa =
        baseUa - 1300 + static_cast<int32_t>((now / 11) % 5) * 100;
    return sample;
  }

  if (phase < kChargePhaseFraction) {
    const double t = phase / kChargePhaseFraction;
    sample.status = BatteryStatus::CHARGING;
    sample.chargeCounter = lerp(profile.chargeCounterMin,
                                profile.chargeCounterMax, t);
    sample.voltageMv = lerp(profile.voltageFullMv, profile.voltageChargeMv, t);
    sample.temperatureDeciC =
        lerp(profile.temperatureFullDeciC, profile.temperatureChargeDeciC, t);
    if (profile.hasCustomCurrent) {
      const int32_t jitter = (static_cast<int32_t>((now / 5) % 9) - 4) * 150;
      sample.currentUa = lerp(profile.currentChargeUa, profile.currentFullUa, t) + jitter;
      sample.currentAvgUa = sample.currentUa - 1100;
    } else {
      // 补电电流随充满度下降：900mA → 250mA
      sample.currentUa = lerp(900000, 250000, t);
      sample.currentAvgUa = sample.currentUa;
    }
  } else {
    const double t =
        (phase - kChargePhaseFraction) / (1.0 - kChargePhaseFraction);
    sample.status = BatteryStatus::FULL;
    sample.chargeCounter = lerp(profile.chargeCounterMax,
                                profile.chargeCounterMin, t);
    sample.voltageMv = profile.voltageFullMv;
    sample.temperatureDeciC =
        lerp(profile.temperatureChargeDeciC, profile.temperatureFullDeciC, t);
    if (profile.hasCustomCurrent) {
      // 满电挂充微安级浮充电流及微小平滑差（如真机 -62200 µA / avg -63500 µA）
      const int32_t jitter = (static_cast<int32_t>((now / 5) % 9) - 4) * 150;
      sample.currentUa = profile.currentFullUa + jitter;
      sample.currentAvgUa =
          profile.currentFullUa - 1300 + static_cast<int32_t>((now / 11) % 5) * 100;
    } else {
      sample.currentUa = 0;
      sample.currentAvgUa = 0;
    }
  }

  // 真机满电静置时电压有 ±1mV 的低频抖动
  sample.voltageMv += static_cast<int32_t>((now / 7) % 3) - 1;
  return sample;
}

void fillBattery(const BatteryProfile &profile, const BatterySample &sample,
                 BatteryChargingPolicy policy, HealthInfo *out) {
  out->chargerAcOnline = profile.acOnline;
  out->chargerUsbOnline = profile.usbOnline;
  out->chargerWirelessOnline = profile.wirelessOnline;
  out->chargerDockOnline = profile.dockOnline;
  out->maxChargingCurrentMicroamps = profile.maxChargingCurrentUa;
  out->maxChargingVoltageMicrovolts = profile.maxChargingVoltageUv;
  out->batteryStatus = sample.status;
  out->batteryHealth = profile.health;
  out->batteryPresent = profile.present;
  out->batteryLevel = profile.level;
  out->batteryVoltageMillivolts = sample.voltageMv;
  out->batteryTemperatureTenthsCelsius = sample.temperatureDeciC;
  out->batteryCurrentMicroamps = sample.currentUa;
  out->batteryCurrentAverageMicroamps = sample.currentAvgUa;
  out->batteryCycleCount = profile.cycleCount;
  out->batteryFullChargeUah = profile.fullChargeUah;
  out->batteryChargeCounterUah = sample.chargeCounter;
  out->batteryTechnology = profile.technology;
  out->batteryCapacityLevel = profile.capacityLevel;
  out->batteryChargeTimeToFullNowSeconds = 0;
  out->batteryFullChargeDesignCapacityUah = profile.fullChargeUah;
  out->chargingState = profile.chargingState;
  out->chargingPolicy = policy;
  out->diskStats.clear();
  out->storageInfos.clear();
}

class HealthService : public BnHealth {
public:
  ScopedAStatus registerCallback(
      const std::shared_ptr<IHealthInfoCallback> &callback) override {
    if (callback == nullptr) {
      return ScopedAStatus::fromExceptionCode(EX_NULL_POINTER);
    }
    std::lock_guard<std::mutex> lock(mutex_);
    callbacks_.push_back(callback);
    return ScopedAStatus::ok();
  }

  ScopedAStatus unregisterCallback(
      const std::shared_ptr<IHealthInfoCallback> &callback) override {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = callbacks_.begin(); it != callbacks_.end(); ++it) {
      if (it->get() == callback.get()) {
        callbacks_.erase(it);
        break;
      }
    }
    return ScopedAStatus::ok();
  }

  ScopedAStatus update() override {
    HealthInfo info;
    fill(&info);
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = callbacks_.begin(); it != callbacks_.end();) {
      if (!it->get()->healthInfoChanged(info).isOk()) {
        it = callbacks_.erase(it);
      } else {
        ++it;
      }
    }
    return ScopedAStatus::ok();
  }

  ScopedAStatus getChargeCounterUah(int32_t *out) override {
    *out = sampleBattery(gProfile).chargeCounter;
    return ScopedAStatus::ok();
  }

  ScopedAStatus getCurrentNowMicroamps(int32_t *out) override {
    *out = sampleBattery(gProfile).currentUa;
    return ScopedAStatus::ok();
  }

  ScopedAStatus getCurrentAverageMicroamps(int32_t *out) override {
    *out = sampleBattery(gProfile).currentAvgUa;
    return ScopedAStatus::ok();
  }

  ScopedAStatus getCapacity(int32_t *out) override {
    *out = gProfile.level;
    return ScopedAStatus::ok();
  }

  ScopedAStatus getEnergyCounterNwh(int64_t *out) override {
    // 无能量计，按 AOSP 口径返回不支持
    *out = 0;
    return ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
  }

  ScopedAStatus getChargeStatus(BatteryStatus *out) override {
    *out = sampleBattery(gProfile).status;
    return ScopedAStatus::ok();
  }

  ScopedAStatus getStorageInfo(std::vector<StorageInfo> *out) override {
    out->clear();
    return ScopedAStatus::ok();
  }

  ScopedAStatus getDiskStats(std::vector<DiskStats> *out) override {
    out->clear();
    return ScopedAStatus::ok();
  }

  ScopedAStatus getHealthInfo(HealthInfo *out) override {
    fill(out);
    return ScopedAStatus::ok();
  }

  ScopedAStatus setChargingPolicy(BatteryChargingPolicy in_value) override {
    std::lock_guard<std::mutex> lock(mutex_);
    policy_ = in_value;
    return ScopedAStatus::ok();
  }

  ScopedAStatus getChargingPolicy(BatteryChargingPolicy *out) override {
    std::lock_guard<std::mutex> lock(mutex_);
    *out = policy_;
    return ScopedAStatus::ok();
  }

  ScopedAStatus getBatteryHealthData(BatteryHealthData *out) override {
    out->batteryManufacturingDateSeconds = gProfile.manufacturingDateSeconds;
    out->batteryFirstUsageSeconds = gProfile.firstUsageSeconds;
    out->batteryStateOfHealth = gProfile.stateOfHealth;
    if (!gProfile.serialNumber.empty()) {
      out->batterySerialNumber = gProfile.serialNumber;
    }
    out->batteryPartStatus = gProfile.partStatus;
    return ScopedAStatus::ok();
  }

#if CDH_ANDROID_VERSION >= 16
  ScopedAStatus getHingeInfo(std::vector<HingeInfo> *out) override {
    out->clear();  // 非折叠机：返回空表而非不支持，对齐对标设备
    return ScopedAStatus::ok();
  }
#endif

private:
  void fill(HealthInfo *out) {
    BatteryChargingPolicy policy;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      policy = policy_;
    }
    fillBattery(gProfile, sampleBattery(gProfile), policy, out);
  }

  std::mutex mutex_;
  std::vector<std::shared_ptr<IHealthInfoCallback>> callbacks_;
  BatteryChargingPolicy policy_ = BatteryChargingPolicy::INVALID;
};

}  // namespace

int main() {
  loadProfile(&gProfile);

  ABinderProcess_setThreadPoolMaxThreadCount(2);
  auto health = ndk::SharedRefBase::make<HealthService>();
  // binder 只取一次并全程持有。BnCInterface::asBinder() 只做**弱**缓存
  // （mWeakBinder），拿它的临时返回值（health->asBinder().get()）用过即丢时，
  // 底层 ABBinder 引用计数归零被销毁，下一次 asBinder() 会重建一个全新对象。
  // AIBinder_setExtension 的注释对此有明确警告：setExtension 与 addService
  // 必须作用在同一个 AIBinder 上，否则 extension 落在已销毁的那个临时对象上，
  // 客户端 getExtension() 只能拿到 null（表现为 BatteryService NPE 死循环）。
  ndk::SpAIBinder binder = health->asBinder();
#if CDH_HEALTH_SAMSUNG
  // 三星变体：先在 IHealth binder 上挂 ISehHealth 扩展，再发布服务：客户端拿到
  // binder 就立刻 getExtension，晚于 addService 会撞上“extension 还没设”的窗口。
  // 回调数据源就是本服务自己的模拟电池口径。
  cdh::health::AttachSehHealth(
      binder.get(), [health](HealthInfo *out) { health->getHealthInfo(out); },
      kUpdateIntervalSeconds);
#endif
  if (AServiceManager_addService(binder.get(), kHealthServiceName) != STATUS_OK) {
    return 2;
  }

#if CDH_HEALTH_OPLUS
  // 欧加变体：OPPO/OnePlus/realme 设备若声明了 ICharger/default，同进程注册以提供
  // notifycode=0 及一致的电池快照；未声明该 VINTF 片段的机型返回 nullptr。
  auto oplusCharger = cdh::health::RegisterOplusChargerIfDeclared(
      [health](HealthInfo *out, int32_t *outSoh) {
        health->getHealthInfo(out);
        if (outSoh != nullptr) {
          *outSoh = static_cast<int32_t>(gProfile.stateOfHealth);
        }
      });
  (void)oplusCharger;
#endif

  std::thread([health] {
    while (true) {
      std::this_thread::sleep_for(
          std::chrono::seconds(kUpdateIntervalSeconds));
      health->update();
    }
  }).detach();

  ABinderProcess_joinThreadPool();
  return 0;
}
