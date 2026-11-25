// engine/vendor/hardware 线 —— android.hardware.biometrics.fingerprint（IFingerprint v4）自研服务实现。
//
// 注册 android.hardware.biometrics.fingerprint.IFingerprint/default。指纹 HAL 通过
// getSensorProps 上报传感器、再按传感器 createSession 做录入/识别。
//
// 云端虚拟机默认（基础变体）如实上报空列表、createSession 直接返回 EX_UNSUPPORTED_OPERATION。
//
// 启用 CDH_FINGERPRINT_FAKE_SENSOR（fakefingerprint / fingerprint-only 变体）时：
//   - 支持从 /vendor/etc/fingerprint_config.xml（或环境变量 FINGERPRINT_CONFIG_XML）
//     按机型加载传感器形态（POWER_BUTTON / UDFPS_OPTICAL / REAR 等）、安全等级（STRONG /
//     CONVENIENCE）、最大指纹数、传感器坐标及初始模板列表；配置文件缺失时回退兼容默认
//     （REAR + CONVENIENCE + maxEnrollments=1），设备无关、零机型常量硬编码。
//   - 在 Session 内完整闭环 ISessionCallback 回调（onEnrollmentsEnumerated /
//     onEnrollmentsRemoved / onAuthenticatorIdRetrieved / onChallengeGenerated /
//     onSessionClosed / cancel->onError(CANCELED)），确保 Android 14/15 开机阶段的
//     FingerprintInternalCleanupClient 与业务风控 SDK / BiometricPrompt 正常收口不挂起。

#include <android/binder_ibinder.h>
#include <android/binder_status.h>

extern "C" {
void ABinderProcess_setThreadPoolMaxThreadCount(uint32_t numThreads);
void ABinderProcess_joinThreadPool(void);
binder_status_t AServiceManager_addService(AIBinder* binder,
                                           const char* instance);
}

#include <aidl/android/hardware/biometrics/common/BnCancellationSignal.h>
#include <aidl/android/hardware/biometrics/common/CommonProps.h>
#include <aidl/android/hardware/biometrics/common/OperationContext.h>
#include <aidl/android/hardware/biometrics/common/SensorStrength.h>
#include <aidl/android/hardware/biometrics/fingerprint/BnFingerprint.h>
#include <aidl/android/hardware/biometrics/fingerprint/BnSession.h>
#include <aidl/android/hardware/biometrics/fingerprint/Error.h>
#include <aidl/android/hardware/biometrics/fingerprint/FingerprintSensorType.h>
#include <aidl/android/hardware/biometrics/fingerprint/ISession.h>
#include <aidl/android/hardware/biometrics/fingerprint/ISessionCallback.h>
#include <aidl/android/hardware/biometrics/fingerprint/PointerContext.h>
#include <aidl/android/hardware/biometrics/fingerprint/SensorLocation.h>
#include <aidl/android/hardware/biometrics/fingerprint/SensorProps.h>
#include <aidl/android/hardware/keymaster/HardwareAuthToken.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

using aidl::android::hardware::biometrics::common::BnCancellationSignal;
using aidl::android::hardware::biometrics::common::ICancellationSignal;
using aidl::android::hardware::biometrics::common::OperationContext;
using aidl::android::hardware::biometrics::common::SensorStrength;
using aidl::android::hardware::biometrics::fingerprint::BnFingerprint;
using aidl::android::hardware::biometrics::fingerprint::BnSession;
using aidl::android::hardware::biometrics::fingerprint::Error;
using aidl::android::hardware::biometrics::fingerprint::FingerprintSensorType;
using aidl::android::hardware::biometrics::fingerprint::ISession;
using aidl::android::hardware::biometrics::fingerprint::ISessionCallback;
using aidl::android::hardware::biometrics::fingerprint::PointerContext;
using aidl::android::hardware::biometrics::fingerprint::SensorLocation;
using aidl::android::hardware::biometrics::fingerprint::SensorProps;
using aidl::android::hardware::keymaster::HardwareAuthToken;
using ndk::ScopedAStatus;

static constexpr const char* kFingerprintServiceName =
    "android.hardware.biometrics.fingerprint.IFingerprint/default";

namespace {

#if defined(CDH_FINGERPRINT_FAKE_SENSOR)

static constexpr const char* kDefaultFpConfigPath =
    "/vendor/etc/fingerprint_config.xml";

struct FakeFingerprintConfig {
  int32_t sensorId = 0;
  FingerprintSensorType sensorType = FingerprintSensorType::REAR;
  SensorStrength strength = SensorStrength::CONVENIENCE;
  int32_t maxEnrollmentsPerUser = 1;
  bool hasLocation = false;
  int32_t locX = 0;
  int32_t locY = 0;
  int32_t locRadius = 0;
  int64_t authenticatorId = 0;
  std::vector<int32_t> enrolledIds;
};

std::string trimAscii(const std::string& s) {
  size_t b = 0;
  size_t e = s.size();
  while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
  while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
  return s.substr(b, e - b);
}

std::string toLowerAscii(std::string s) {
  for (char& c : s) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return s;
}

bool parseInt64Attr(const std::string& raw, int64_t* out) {
  std::string v = trimAscii(raw);
  if (v.empty()) return false;
  char* end = nullptr;
  long long parsed = std::strtoll(v.c_str(), &end, 10);
  if (end == v.c_str() || *end != '\0') return false;
  *out = static_cast<int64_t>(parsed);
  return true;
}

// 从 XML 节点文本提取属性 key="value"（跳过 <!-- ... --> 注释段，零外部依赖）。
std::string stripXmlComments(const std::string& xml) {
  std::string out;
  out.reserve(xml.size());
  size_t pos = 0;
  while (pos < xml.size()) {
    size_t start = xml.find("<!--", pos);
    if (start == std::string::npos) {
      out.append(xml, pos, std::string::npos);
      break;
    }
    out.append(xml, pos, start - pos);
    size_t end = xml.find("-->", start + 4);
    if (end == std::string::npos) break;
    pos = end + 3;
  }
  return out;
}

bool extractXmlAttr(const std::string& tagBody, const char* key,
                    std::string* out) {
  const std::string needle = std::string(key) + "=";
  size_t pos = 0;
  while (pos < tagBody.size()) {
    size_t idx = tagBody.find(needle, pos);
    if (idx == std::string::npos) return false;
    if (idx > 0) {
      unsigned char prev = static_cast<unsigned char>(tagBody[idx - 1]);
      if (!std::isspace(prev)) {
        pos = idx + needle.size();
        continue;
      }
    }
    size_t qpos = idx + needle.size();
    while (qpos < tagBody.size() &&
           std::isspace(static_cast<unsigned char>(tagBody[qpos]))) {
      ++qpos;
    }
    if (qpos >= tagBody.size() ||
        (tagBody[qpos] != '"' && tagBody[qpos] != '\'')) {
      pos = qpos;
      continue;
    }
    char quote = tagBody[qpos];
    size_t end = tagBody.find(quote, qpos + 1);
    if (end == std::string::npos) return false;
    *out = tagBody.substr(qpos + 1, end - (qpos + 1));
    return true;
  }
  return false;
}

FakeFingerprintConfig loadFingerprintConfig() {
  FakeFingerprintConfig cfg;
  const char* envPath = std::getenv("FINGERPRINT_CONFIG_XML");
  const char* path =
      (envPath != nullptr && envPath[0] != '\0') ? envPath : kDefaultFpConfigPath;

  FILE* fp = std::fopen(path, "rb");
  if (fp == nullptr) {
    return cfg;
  }
  std::string raw;
  char buf[1024];
  size_t n = 0;
  while ((n = std::fread(buf, 1, sizeof(buf), fp)) > 0) {
    raw.append(buf, n);
    if (raw.size() > 65536) break;
  }
  std::fclose(fp);

  const std::string clean = stripXmlComments(raw);
  size_t tagStart = clean.find("<fingerprint-config");
  if (tagStart == std::string::npos) {
    return cfg;
  }
  size_t tagEnd = clean.find('>', tagStart);
  if (tagEnd == std::string::npos) {
    return cfg;
  }
  const std::string tag = clean.substr(tagStart, tagEnd - tagStart + 1);

  std::string v;
  int64_t num = 0;
  if (extractXmlAttr(tag, "sensor-id", &v) && parseInt64Attr(v, &num)) {
    cfg.sensorId = static_cast<int32_t>(num);
  }
  if (extractXmlAttr(tag, "sensor-type", &v)) {
    const std::string s = toLowerAscii(trimAscii(v));
    if (s == "power_button" || s == "side") {
      cfg.sensorType = FingerprintSensorType::POWER_BUTTON;
    } else if (s == "udfps_optical" || s == "under_display_optical") {
      cfg.sensorType = FingerprintSensorType::UNDER_DISPLAY_OPTICAL;
    } else if (s == "udfps_ultrasonic" || s == "under_display_ultrasonic") {
      cfg.sensorType = FingerprintSensorType::UNDER_DISPLAY_ULTRASONIC;
    } else if (s == "home_button" || s == "front") {
      cfg.sensorType = FingerprintSensorType::HOME_BUTTON;
    } else if (s == "unknown") {
      cfg.sensorType = FingerprintSensorType::UNKNOWN;
    } else {
      cfg.sensorType = FingerprintSensorType::REAR;
    }
  }
  if (extractXmlAttr(tag, "strength", &v)) {
    const std::string s = toLowerAscii(trimAscii(v));
    if (s == "strong") {
      cfg.strength = SensorStrength::STRONG;
    } else if (s == "weak") {
      cfg.strength = SensorStrength::WEAK;
    } else {
      cfg.strength = SensorStrength::CONVENIENCE;
    }
  }
  if (extractXmlAttr(tag, "max-enrollments", &v) && parseInt64Attr(v, &num) &&
      num > 0) {
    cfg.maxEnrollmentsPerUser = static_cast<int32_t>(num);
  }
  if (extractXmlAttr(tag, "authenticator-id", &v) && parseInt64Attr(v, &num)) {
    cfg.authenticatorId = num;
  }
  if (extractXmlAttr(tag, "loc-x", &v) && parseInt64Attr(v, &num)) {
    cfg.locX = static_cast<int32_t>(num);
    cfg.hasLocation = true;
  }
  if (extractXmlAttr(tag, "loc-y", &v) && parseInt64Attr(v, &num)) {
    cfg.locY = static_cast<int32_t>(num);
    cfg.hasLocation = true;
  }
  if (extractXmlAttr(tag, "loc-radius", &v) && parseInt64Attr(v, &num)) {
    cfg.locRadius = static_cast<int32_t>(num);
    cfg.hasLocation = true;
  }
  if (extractXmlAttr(tag, "enrolled-ids", &v)) {
    size_t start = 0;
    while (start < v.size()) {
      size_t comma = v.find(',', start);
      std::string item =
          (comma == std::string::npos) ? v.substr(start) : v.substr(start, comma - start);
      int64_t id = 0;
      if (parseInt64Attr(item, &id) && id > 0) {
        cfg.enrolledIds.push_back(static_cast<int32_t>(id));
      }
      if (comma == std::string::npos) break;
      start = comma + 1;
    }
  }
  return cfg;
}

class SessionCancellationSignal : public BnCancellationSignal {
 public:
  explicit SessionCancellationSignal(std::shared_ptr<ISessionCallback> cb)
      : cb_(std::move(cb)) {}

  ScopedAStatus cancel() override {
    if (cb_ != nullptr) {
      cb_->onError(Error::CANCELED, 0);
    }
    return ScopedAStatus::ok();
  }

 private:
  std::shared_ptr<ISessionCallback> cb_;
};

std::shared_ptr<ISession> MakeFakeSession(
    const FakeFingerprintConfig& cfg,
    const std::shared_ptr<ISessionCallback>& cb) {
  class FakeSession : public BnSession {
   public:
    FakeSession(const FakeFingerprintConfig& cfg,
                std::shared_ptr<ISessionCallback> cb)
        : cb_(std::move(cb)),
          enrolledIds_(cfg.enrolledIds),
          authenticatorId_(cfg.authenticatorId) {}

    ScopedAStatus generateChallenge() override {
      int64_t ch = 0;
      {
        std::lock_guard<std::mutex> lock(mu_);
        ch = ++challengeCounter_;
      }
      if (cb_ != nullptr) {
        cb_->onChallengeGenerated(ch);
      }
      return ScopedAStatus::ok();
    }

    ScopedAStatus revokeChallenge(int64_t in_challenge) override {
      if (cb_ != nullptr) {
        cb_->onChallengeRevoked(in_challenge);
      }
      return ScopedAStatus::ok();
    }

    ScopedAStatus enroll(
        const HardwareAuthToken& /*in_hat*/,
        std::shared_ptr<ICancellationSignal>* _aidl_return) override {
      *_aidl_return = ndk::SharedRefBase::make<SessionCancellationSignal>(cb_);
      return ScopedAStatus::ok();
    }

    ScopedAStatus authenticate(
        int64_t /*in_operationId*/,
        std::shared_ptr<ICancellationSignal>* _aidl_return) override {
      *_aidl_return = ndk::SharedRefBase::make<SessionCancellationSignal>(cb_);
      return ScopedAStatus::ok();
    }

    ScopedAStatus detectInteraction(
        std::shared_ptr<ICancellationSignal>* _aidl_return) override {
      *_aidl_return = ndk::SharedRefBase::make<SessionCancellationSignal>(cb_);
      return ScopedAStatus::ok();
    }

    ScopedAStatus authenticateWithContext(
        int64_t /*in_operationId*/, const OperationContext& /*in_context*/,
        std::shared_ptr<ICancellationSignal>* _aidl_return) override {
      *_aidl_return = ndk::SharedRefBase::make<SessionCancellationSignal>(cb_);
      return ScopedAStatus::ok();
    }

    ScopedAStatus enrollWithContext(
        const HardwareAuthToken& /*in_hat*/,
        const OperationContext& /*in_context*/,
        std::shared_ptr<ICancellationSignal>* _aidl_return) override {
      *_aidl_return = ndk::SharedRefBase::make<SessionCancellationSignal>(cb_);
      return ScopedAStatus::ok();
    }

    ScopedAStatus detectInteractionWithContext(
        const OperationContext& /*in_context*/,
        std::shared_ptr<ICancellationSignal>* _aidl_return) override {
      *_aidl_return = ndk::SharedRefBase::make<SessionCancellationSignal>(cb_);
      return ScopedAStatus::ok();
    }

    ScopedAStatus enumerateEnrollments() override {
      std::vector<int32_t> snapshot;
      {
        std::lock_guard<std::mutex> lock(mu_);
        snapshot = enrolledIds_;
      }
      if (cb_ != nullptr) {
        cb_->onEnrollmentsEnumerated(snapshot);
      }
      return ScopedAStatus::ok();
    }

    ScopedAStatus removeEnrollments(const std::vector<int32_t>& ids) override {
      {
        std::lock_guard<std::mutex> lock(mu_);
        for (int32_t id : ids) {
          enrolledIds_.erase(
              std::remove(enrolledIds_.begin(), enrolledIds_.end(), id),
              enrolledIds_.end());
        }
      }
      if (cb_ != nullptr) {
        cb_->onEnrollmentsRemoved(ids);
      }
      return ScopedAStatus::ok();
    }

    ScopedAStatus getAuthenticatorId() override {
      int64_t authId = 0;
      {
        std::lock_guard<std::mutex> lock(mu_);
        authId = enrolledIds_.empty() ? 0 : authenticatorId_;
      }
      if (cb_ != nullptr) {
        cb_->onAuthenticatorIdRetrieved(authId);
      }
      return ScopedAStatus::ok();
    }

    ScopedAStatus invalidateAuthenticatorId() override {
      int64_t nextAuthId = 0;
      {
        std::lock_guard<std::mutex> lock(mu_);
        if (!enrolledIds_.empty()) {
          authenticatorId_ = (authenticatorId_ == 0) ? 1 : (authenticatorId_ + 1);
          nextAuthId = authenticatorId_;
        }
      }
      if (cb_ != nullptr) {
        cb_->onAuthenticatorIdInvalidated(nextAuthId);
      }
      return ScopedAStatus::ok();
    }

    ScopedAStatus resetLockout(const HardwareAuthToken& /*in_hat*/) override {
      if (cb_ != nullptr) {
        cb_->onLockoutCleared();
      }
      return ScopedAStatus::ok();
    }

    ScopedAStatus close() override {
      if (cb_ != nullptr) {
        cb_->onSessionClosed();
      }
      return ScopedAStatus::ok();
    }

    ScopedAStatus onPointerDown(int32_t /*pointerId*/, int32_t /*x*/,
                                int32_t /*y*/, float /*minor*/,
                                float /*major*/) override {
      return ScopedAStatus::ok();
    }

    ScopedAStatus onPointerUp(int32_t /*pointerId*/) override {
      return ScopedAStatus::ok();
    }

    ScopedAStatus onUiReady() override { return ScopedAStatus::ok(); }

    ScopedAStatus onPointerDownWithContext(
        const PointerContext& /*in_context*/) override {
      return ScopedAStatus::ok();
    }

    ScopedAStatus onPointerUpWithContext(
        const PointerContext& /*in_context*/) override {
      return ScopedAStatus::ok();
    }

#if !defined(CDH_ANDROID_VERSION) || CDH_ANDROID_VERSION >= 14
    ScopedAStatus onPointerCancelWithContext(
        const PointerContext& /*in_context*/) override {
      return ScopedAStatus::ok();
    }
#endif

    ScopedAStatus onContextChanged(
        const OperationContext& /*in_context*/) override {
      return ScopedAStatus::ok();
    }

#if !defined(CDH_ANDROID_VERSION) || CDH_ANDROID_VERSION >= 14
    ScopedAStatus setIgnoreDisplayTouches(bool /*shouldIgnore*/) override {
      return ScopedAStatus::ok();
    }
#endif

   private:
    std::mutex mu_;
    std::shared_ptr<ISessionCallback> cb_;
    std::vector<int32_t> enrolledIds_;
    int64_t authenticatorId_ = 0;
    int64_t challengeCounter_ = 0x63646800LL;
  };
  return ndk::SharedRefBase::make<FakeSession>(cfg, cb);
}

#endif  // CDH_FINGERPRINT_FAKE_SENSOR

class FingerprintService : public BnFingerprint {
 public:
#if defined(CDH_FINGERPRINT_FAKE_SENSOR)
  FingerprintService() : cfg_(loadFingerprintConfig()) {}
#endif

  ScopedAStatus getSensorProps(
      std::vector<SensorProps>* _aidl_return) override {
    _aidl_return->clear();  // 默认无指纹传感器
#if defined(CDH_FINGERPRINT_FAKE_SENSOR)
    SensorProps sensor;
    sensor.commonProps.sensorId = cfg_.sensorId;
    sensor.commonProps.sensorStrength = cfg_.strength;
    sensor.commonProps.maxEnrollmentsPerUser = cfg_.maxEnrollmentsPerUser;
    sensor.sensorType = cfg_.sensorType;
    if (cfg_.hasLocation) {
      SensorLocation loc;
      loc.sensorLocationX = cfg_.locX;
      loc.sensorLocationY = cfg_.locY;
      loc.sensorRadius = cfg_.locRadius;
      sensor.sensorLocations.push_back(std::move(loc));
    }
    _aidl_return->push_back(std::move(sensor));
#endif
    return ScopedAStatus::ok();
  }

  ScopedAStatus createSession(
      int32_t /*in_sensorId*/, int32_t /*in_userId*/,
      const std::shared_ptr<ISessionCallback>& in_cb,
      std::shared_ptr<ISession>* _aidl_return) override {
#if defined(CDH_FINGERPRINT_FAKE_SENSOR)
    *_aidl_return = MakeFakeSession(cfg_, in_cb);
    return ScopedAStatus::ok();
#else
    (void)in_cb;
    (void)_aidl_return;
    return ScopedAStatus::fromExceptionCodeWithMessage(
        EX_UNSUPPORTED_OPERATION, "no fingerprint sensor");
#endif
  }

#if defined(CDH_FINGERPRINT_FAKE_SENSOR)
 private:
  FakeFingerprintConfig cfg_;
#endif
};

}  // namespace

int main() {
  ABinderProcess_setThreadPoolMaxThreadCount(2);
  auto svc = ndk::SharedRefBase::make<FingerprintService>();
  if (AServiceManager_addService(svc->asBinder().get(), kFingerprintServiceName) !=
      STATUS_OK) {
    return 2;
  }
  ABinderProcess_joinThreadPool();
  return 0;
}
