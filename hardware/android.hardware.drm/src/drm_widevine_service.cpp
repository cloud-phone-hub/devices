// engine/vendor/hardware 线 —— android.hardware.drm（IDrmFactory v1）Widevine 检测模拟服务。
//
// 注册 android.hardware.drm.IDrmFactory/widevine，与同模块的 clearkey 桩并存（不同实例名）。
// 本服务只做「检测与会话模拟」：返回标准 Widevine 的属性、安全级别、会话计数、HDCP 信息、
// 本地随机会话与受保护的持久 deviceUniqueId；供证、许可、加解密、签名、离线密钥等明确回
// EX_UNSUPPORTED_OPERATION，不生成许可、密钥或解密帧。这些应答不证明物理 TEE、真实 OEMCrypto、
// Widevine 认证或 HDCP 链路协商。
//
// 设备无关：不内置任何具体真机的唯一身份或厂商采集值。属性取中立通用默认；deviceUniqueId 由
// Profile（/vendor/etc/mediadrm-model-id，制作者显式提供）或本实例 userdata 持久随机身份提供，
// 原始身份不入仓、不写日志。移植自 cloud-droid-hub 的 Widevine 检测模型（pixel6-15 / N4a），
// 改写为本仓 AIDL Bn* codegen 风格，可直接被 cdh-vendor 编译。

#include <android/binder_ibinder.h>
#include <android/binder_status.h>
#include <android/log.h>

extern "C" {
void ABinderProcess_setThreadPoolMaxThreadCount(uint32_t numThreads);
void ABinderProcess_joinThreadPool(void);
binder_status_t AServiceManager_addService(AIBinder* binder, const char* instance);
AIBinder* AServiceManager_checkService(const char* instance);
int __system_property_get(const char* name, char* value);
}

#include <aidl/android/hardware/drm/BnCryptoPlugin.h>
#include <aidl/android/hardware/drm/BnDrmFactory.h>
#include <aidl/android/hardware/drm/BnDrmPlugin.h>
#include <aidl/android/hardware/drm/CryptoSchemes.h>
#include <aidl/android/hardware/drm/DecryptArgs.h>
#include <aidl/android/hardware/drm/DrmMetricGroup.h>
#include <aidl/android/hardware/drm/HdcpLevel.h>
#include <aidl/android/hardware/drm/HdcpLevels.h>
#include <aidl/android/hardware/drm/ICryptoPlugin.h>
#include <aidl/android/hardware/drm/IDrmPlugin.h>
#include <aidl/android/hardware/drm/IDrmPluginListener.h>
#include <aidl/android/hardware/drm/KeyRequest.h>
#include <aidl/android/hardware/drm/KeySetId.h>
#include <aidl/android/hardware/drm/KeyType.h>
#include <aidl/android/hardware/drm/KeyValue.h>
#include <aidl/android/hardware/drm/LogMessage.h>
#include <aidl/android/hardware/drm/NumberOfSessions.h>
#include <aidl/android/hardware/drm/OfflineLicenseState.h>
#include <aidl/android/hardware/drm/OpaqueData.h>
#include <aidl/android/hardware/drm/ProvideProvisionResponseResult.h>
#include <aidl/android/hardware/drm/ProvisionRequest.h>
#include <aidl/android/hardware/drm/SecureStop.h>
#include <aidl/android/hardware/drm/SecureStopId.h>
#include <aidl/android/hardware/drm/SecurityLevel.h>
#include <aidl/android/hardware/drm/SharedBuffer.h>
#include <aidl/android/hardware/drm/SupportedContentType.h>
#include <aidl/android/hardware/drm/Uuid.h>

#include <fcntl.h>
#include <linux/unistd.h>
#include <sys/file.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace drm = aidl::android::hardware::drm;
using ndk::ScopedAStatus;

namespace {

// 中立通用默认值：不绑定具体真机；Widevine CDM v19 标准公开属性（支持 ro.vendor.drm.widevine.* 覆盖）。
constexpr const char* kWidevineFactoryName = "android.hardware.drm.IDrmFactory/widevine";
constexpr const char* kVendor = "com.google.android.widevine";
constexpr const char* kVersion = "19.0.1@AV1A.240725.002";
constexpr const char* kDescription = "Widevine CDM";
constexpr const char* kSystemId = "33098";
constexpr const char* kOemCryptoApiVersion = "18";
constexpr const char* kAlgorithms = "AES/CBC/NoPadding,HmacSHA256";
constexpr const char* kUsageReportingSupport = "True";
constexpr const char* kHdcpLevelStr = "Unprotected";
constexpr const char* kSecurityLevelStr = "L1";  // 检测模拟，不证明物理 L1。
constexpr int32_t kMaxSessions = 16;

std::string getPropOrDefault(const char* propName, const char* defaultValue) {
  char buf[92] = {};
  if (__system_property_get(propName, buf) > 0 && buf[0] != '\0') {
    return std::string(buf);
  }
  return std::string(defaultValue);
}

// Widevine 主 UUID 与 2970 别名（协议标识，非设备身份）。AIDL Uuid.uuid 为定长 16 字节数组。
const std::array<uint8_t, 16> kWidevineUuid = {
    0xed, 0xef, 0x8b, 0xa9, 0x79, 0xd6, 0x4a, 0xce,
    0xa3, 0xc8, 0x27, 0xdc, 0xd5, 0x1d, 0x21, 0xed};
const std::array<uint8_t, 16> kWidevineAlias = {
    0x29, 0x70, 0x1f, 0xe4, 0x3c, 0xc7, 0x4a, 0x34,
    0x8c, 0x5b, 0xae, 0x90, 0xc7, 0x43, 0x9a, 0x47};

bool isWidevineUuid(const std::array<uint8_t, 16>& uuid) {
  return uuid == kWidevineUuid || uuid == kWidevineAlias;
}

// 供证 / 许可 / 加解密 / 签名等明确不支持；不生成任何密钥或解密帧。
ScopedAStatus unsupported() {
  return ScopedAStatus::fromExceptionCodeWithMessage(
      EX_UNSUPPORTED_OPERATION,
      "Widevine detection simulation: provisioning, licensing and cryptography unsupported");
}

// 会话不存在、已关闭或属于其它调用者。
ScopedAStatus closed() {
  return ScopedAStatus::fromServiceSpecificErrorWithMessage(
      3, "Session is unknown, closed, or owned by another client");
}

// 本地会话表：随机会话 id → {创建 UID, 安全级别}；与具体插件对象无关，跨对象按 UID 隔离。
struct Session {
  uid_t uid;
  drm::SecurityLevel level;
};
std::mutex g_gate;
std::map<std::vector<uint8_t>, Session> g_sessions;

// ---------------------------------------------------------------------------
// 持久身份：Profile 优先，其次 userdata 随机持久化，均不落原始身份到日志。
// ---------------------------------------------------------------------------
namespace identity {
constexpr const char* kProfile = "/vendor/etc/mediadrm-model-id";
constexpr const char* kStateDir = "/data/vendor/mediadrm/model";
constexpr const char* kStateName = "device-id";

std::mutex g_gate;
std::vector<uint8_t> g_value;

bool logFail(const char* stage) {
  const int saved = errno;
  __android_log_print(ANDROID_LOG_ERROR, "DrmWidevine",
                      "identity stage=%s errno=%d (%s)", stage, saved, strerror(saved));
  errno = saved;
  return false;
}

struct Fd {
  int fd = -1;
  explicit Fd(int v) : fd(v) {}
  Fd(const Fd&) = delete;
  Fd& operator=(const Fd&) = delete;
  ~Fd() {
    const int saved = errno;
    if (fd >= 0) close(fd);
    errno = saved;
  }
};

bool randomBytes(uint8_t* p, size_t n) {
  size_t at = 0;
  while (at < n) {
    const ssize_t used = getrandom(p + at, n - at, 0);
    if (used < 0 && errno == EINTR) continue;
    if (used <= 0) return false;
    at += static_cast<size_t>(used);
  }
  return true;
}

// 校验并读取普通独立文件：0600、单硬链、32 字节、属主与当前进程一致。
bool readExact(int fd, std::array<uint8_t, 32>* out) {
  struct stat st;
  if (fstat(fd, &st) != 0) return false;
  if (!S_ISREG(st.st_mode) || st.st_nlink != 1 || st.st_size != 32 ||
      (st.st_mode & 0777) != 0600) {
    errno = EINVAL;
    return false;
  }
  size_t at = 0;
  while (at < out->size()) {
    const ssize_t used = read(fd, out->data() + at, out->size() - at);
    if (used < 0 && errno == EINTR) continue;
    if (used <= 0) { errno = EINVAL; return false; }
    at += static_cast<size_t>(used);
  }
  return true;
}

bool stateId(int dir, std::array<uint8_t, 32>* out) {
  Fd file(openat(dir, kStateName, O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
  return file.fd >= 0 && readExact(file.fd, out);
}

// 原子不覆盖改名写入随机身份。
bool createId(int dir, std::array<uint8_t, 32>* out) {
  char tmp[128];
  std::array<uint8_t, 8> salt;
  if (!randomBytes(salt.data(), salt.size())) return false;
  const char hex[] = "0123456789abcdef";
  char suffix[17];
  for (size_t i = 0; i < salt.size(); ++i) {
    suffix[i * 2] = hex[salt[i] >> 4];
    suffix[i * 2 + 1] = hex[salt[i] & 15];
  }
  suffix[16] = 0;
  std::snprintf(tmp, sizeof(tmp), "%s.tmp-%ld-%s", kStateName,
                static_cast<long>(getpid()), suffix);
  Fd file(openat(dir, tmp, O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600));
  if (file.fd < 0) return false;
  if (fchmod(file.fd, 0600) != 0) return false;
  if (!randomBytes(out->data(), out->size())) return false;
  size_t at = 0;
  while (at < out->size()) {
    const ssize_t used = write(file.fd, out->data() + at, out->size() - at);
    if (used < 0 && errno == EINTR) continue;
    if (used <= 0) return false;
    at += static_cast<size_t>(used);
  }
  if (fsync(file.fd) != 0) return false;
  if (syscall(__NR_renameat2, dir, tmp, dir, kStateName, RENAME_NOREPLACE) != 0) {
    if (errno == EEXIST) return stateId(dir, out);
    return false;
  }
  if (fsync(dir) != 0) return false;
  return stateId(dir, out);
}

// 仅启动阶段选择一次来源；Profile 不存在才启用 userdata 持久随机身份。
bool load() {
  std::lock_guard<std::mutex> lock(g_gate);
  if (!g_value.empty()) return true;
  std::array<uint8_t, 32> bytes;
  Fd profile(open(kProfile, O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
  if (profile.fd >= 0) {
    if (!readExact(profile.fd, &bytes)) return logFail("profile-read");
    g_value.assign(bytes.begin(), bytes.end());
    return true;
  }
  if (errno != ENOENT) return logFail("profile-open");
  Fd dir(open(kStateDir, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
  if (dir.fd < 0) return logFail("state-open");
  struct stat info;
  if (fstat(dir.fd, &info) != 0) return logFail("state-stat");
  if (!S_ISDIR(info.st_mode) || (info.st_mode & 0777) != 0700) {
    errno = EACCES;
    return logFail("state-metadata");
  }
  int r;
  do { r = flock(dir.fd, LOCK_EX); } while (r != 0 && errno == EINTR);
  if (r != 0) return logFail("state-lock");
  if (!stateId(dir.fd, &bytes)) {
    if (errno != ENOENT) return logFail("state-read");
    if (!createId(dir.fd, &bytes)) return logFail("state-create");
  }
  g_value.assign(bytes.begin(), bytes.end());
  return true;
}

const std::vector<uint8_t>& get() { return g_value; }
}  // namespace identity

// ---------------------------------------------------------------------------
// ICryptoPlugin：MediaCrypto 元数据；不做真实解密。
// ---------------------------------------------------------------------------
class CryptoPlugin : public drm::BnCryptoPlugin {
 public:
  CryptoPlugin() = default;

  ScopedAStatus setMediaDrmSession(const std::vector<uint8_t>& in_sessionId) override {
    if (in_sessionId.empty()) return ScopedAStatus::ok();
    std::lock_guard<std::mutex> lock(g_gate);
    const auto pos = g_sessions.find(in_sessionId);
    if (pos == g_sessions.end() || pos->second.uid != AIBinder_getCallingUid())
      return closed();
    session_ = in_sessionId;
    return ScopedAStatus::ok();
  }

  ScopedAStatus requiresSecureDecoderComponent(const std::string& in_mime,
                                               bool* _aidl_return) override {
    const bool video = in_mime.rfind("video/", 0) == 0;
    *_aidl_return = video;  // 视频按需要安全解码器（检测语义），音频与未知放行。
    return ScopedAStatus::ok();
  }

  ScopedAStatus notifyResolution(int32_t, int32_t) override { return ScopedAStatus::ok(); }
  ScopedAStatus setSharedBufferBase(const drm::SharedBuffer&) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus getLogMessages(std::vector<drm::LogMessage>* r) override {
    r->clear();
    return ScopedAStatus::ok();
  }
  ScopedAStatus decrypt(const drm::DecryptArgs&, int32_t*) override { return unsupported(); }

 private:
  std::vector<uint8_t> session_;
};

// ---------------------------------------------------------------------------
// IDrmPlugin：检测属性、随机会话、HDCP 信息、持久 deviceUniqueId。
// ---------------------------------------------------------------------------
class DrmPlugin : public drm::BnDrmPlugin {
 public:
  ScopedAStatus setListener(const std::shared_ptr<drm::IDrmPluginListener>&) override {
    return ScopedAStatus::ok();
  }

  ScopedAStatus openSession(drm::SecurityLevel in_securityLevel,
                            std::vector<uint8_t>* _aidl_return) override {
    const int32_t lv = static_cast<int32_t>(in_securityLevel);
    if (lv < static_cast<int32_t>(drm::SecurityLevel::SW_SECURE_CRYPTO) ||
        lv > static_cast<int32_t>(drm::SecurityLevel::DEFAULT)) {
      return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    if (identity::get().empty()) {
      return ScopedAStatus::fromServiceSpecificErrorWithMessage(
          4, "DRM identity not initialized");
    }
    std::lock_guard<std::mutex> lock(g_gate);
    if (g_sessions.size() >= static_cast<size_t>(kMaxSessions)) {
      return ScopedAStatus::fromServiceSpecificErrorWithMessage(
          8, "Simulated session limit reached");
    }
    std::vector<uint8_t> id(32);
    do {
      if (!identity::randomBytes(id.data(), id.size())) {
        return ScopedAStatus::fromServiceSpecificErrorWithMessage(
            4, "Session random source failed");
      }
    } while (g_sessions.find(id) != g_sessions.end());
    // DEFAULT 归一到 HW_SECURE_ALL；其余按请求级别保存。
    const auto stored =
        lv == static_cast<int32_t>(drm::SecurityLevel::DEFAULT)
            ? drm::SecurityLevel::HW_SECURE_ALL
            : in_securityLevel;
    g_sessions.emplace(id, Session{AIBinder_getCallingUid(), stored});
    *_aidl_return = id;
    return ScopedAStatus::ok();
  }

  ScopedAStatus closeSession(const std::vector<uint8_t>& in_sessionId) override {
    std::lock_guard<std::mutex> lock(g_gate);
    const auto pos = g_sessions.find(in_sessionId);
    if (pos == g_sessions.end() || pos->second.uid != AIBinder_getCallingUid())
      return closed();
    g_sessions.erase(pos);
    return ScopedAStatus::ok();
  }

  ScopedAStatus getSecurityLevel(const std::vector<uint8_t>& in_sessionId,
                                 drm::SecurityLevel* _aidl_return) override {
    std::lock_guard<std::mutex> lock(g_gate);
    const auto pos = g_sessions.find(in_sessionId);
    if (pos == g_sessions.end() || pos->second.uid != AIBinder_getCallingUid())
      return closed();
    *_aidl_return = pos->second.level;
    return ScopedAStatus::ok();
  }

  ScopedAStatus getNumberOfSessions(drm::NumberOfSessions* _aidl_return) override {
    std::lock_guard<std::mutex> lock(g_gate);
    _aidl_return->currentSessions = static_cast<int32_t>(g_sessions.size());
    _aidl_return->maxSessions = kMaxSessions;
    return ScopedAStatus::ok();
  }

  // 虚拟显示的信息模型独立于厂商许可与 HDCP 协商：一律报无 HDCP，不声称已协商。
  ScopedAStatus getHdcpLevels(drm::HdcpLevels* _aidl_return) override {
    _aidl_return->connectedLevel = drm::HdcpLevel::HDCP_NONE;
    _aidl_return->maxLevel = drm::HdcpLevel::HDCP_NONE;
    return ScopedAStatus::ok();
  }

  ScopedAStatus getPropertyString(const std::string& in_propertyName,
                                  std::string* _aidl_return) override {
    const std::string& n = in_propertyName;
    if (n == "vendor") {
      *_aidl_return = getPropOrDefault("ro.vendor.drm.widevine.vendor", kVendor);
      return ScopedAStatus::ok();
    }
    if (n == "version") {
      *_aidl_return = getPropOrDefault("ro.vendor.drm.widevine.version", kVersion);
      return ScopedAStatus::ok();
    }
    if (n == "description") { *_aidl_return = kDescription; return ScopedAStatus::ok(); }
    if (n == "systemId") {
      *_aidl_return = getPropOrDefault("ro.vendor.drm.widevine.system_id", kSystemId);
      return ScopedAStatus::ok();
    }
    if (n == "securityLevel") {
      *_aidl_return = getPropOrDefault("ro.vendor.drm.widevine.security_level", kSecurityLevelStr);
      return ScopedAStatus::ok();
    }
    if (n == "algorithms") { *_aidl_return = kAlgorithms; return ScopedAStatus::ok(); }
    if (n == "oemCryptoApiVersion") {
      *_aidl_return = getPropOrDefault("ro.vendor.drm.widevine.oemcrypto_version", kOemCryptoApiVersion);
      return ScopedAStatus::ok();
    }
    if (n == "usageReportingSupport") { *_aidl_return = kUsageReportingSupport; return ScopedAStatus::ok(); }
    if (n == "hdcpLevel") { *_aidl_return = kHdcpLevelStr; return ScopedAStatus::ok(); }
    if (n == "maxHdcpLevel") { *_aidl_return = kHdcpLevelStr; return ScopedAStatus::ok(); }
    if (n == "maxNumberOfSessions") {
      *_aidl_return = std::to_string(kMaxSessions);
      return ScopedAStatus::ok();
    }
    if (n == "numberOfOpenSessions") {
      std::lock_guard<std::mutex> lock(g_gate);
      *_aidl_return = std::to_string(g_sessions.size());
      return ScopedAStatus::ok();
    }
    *_aidl_return = "";  // 未知字符串属性回空，不伪造真机专有值。
    return ScopedAStatus::ok();
  }

  ScopedAStatus getPropertyByteArray(const std::string& in_propertyName,
                                     std::vector<uint8_t>* _aidl_return) override {
    if (in_propertyName == "deviceUniqueId") {
      *_aidl_return = identity::get();
      return ScopedAStatus::ok();
    }
    _aidl_return->clear();
    return ScopedAStatus::ok();
  }

  ScopedAStatus setPropertyString(const std::string&, const std::string&) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus setPropertyByteArray(const std::string&,
                                     const std::vector<uint8_t>&) override {
    return ScopedAStatus::ok();
  }

  ScopedAStatus requiresSecureDecoder(const std::string& in_mime,
                                      drm::SecurityLevel in_level,
                                      bool* _aidl_return) override {
    const bool video = in_mime.rfind("video/", 0) == 0;
    const int32_t lv = static_cast<int32_t>(in_level);
    const bool secure = lv >= static_cast<int32_t>(drm::SecurityLevel::HW_SECURE_CRYPTO) &&
                        lv <= static_cast<int32_t>(drm::SecurityLevel::HW_SECURE_ALL);
    *_aidl_return = video && secure;
    return ScopedAStatus::ok();
  }

  ScopedAStatus setCipherAlgorithm(const std::vector<uint8_t>&, const std::string&) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus setMacAlgorithm(const std::vector<uint8_t>&, const std::string&) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus setPlaybackId(const std::vector<uint8_t>&, const std::string&) override {
    return ScopedAStatus::ok();
  }

  ScopedAStatus queryKeyStatus(const std::vector<uint8_t>&,
                               std::vector<drm::KeyValue>* r) override {
    r->clear();
    return ScopedAStatus::ok();
  }
  ScopedAStatus getLogMessages(std::vector<drm::LogMessage>* r) override {
    r->clear();
    return ScopedAStatus::ok();
  }
  ScopedAStatus getMetrics(std::vector<drm::DrmMetricGroup>* r) override {
    r->clear();
    return ScopedAStatus::ok();
  }
  ScopedAStatus getOfflineLicenseKeySetIds(std::vector<drm::KeySetId>* r) override {
    r->clear();
    return ScopedAStatus::ok();
  }
  ScopedAStatus getSecureStopIds(std::vector<drm::SecureStopId>* r) override {
    r->clear();
    return ScopedAStatus::ok();
  }
  ScopedAStatus getSecureStops(std::vector<drm::SecureStop>* r) override {
    r->clear();
    return ScopedAStatus::ok();
  }

  ScopedAStatus releaseAllSecureStops() override { return ScopedAStatus::ok(); }
  ScopedAStatus removeAllSecureStops() override { return ScopedAStatus::ok(); }
  ScopedAStatus releaseSecureStop(const drm::SecureStopId&) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus removeSecureStop(const drm::SecureStopId&) override {
    return ScopedAStatus::ok();
  }
  ScopedAStatus releaseSecureStops(const drm::OpaqueData&) override {
    return ScopedAStatus::ok();
  }

  // 以下涉及供证 / 许可 / 加解密 / 签名 / 离线密钥，明确不支持，不产生任何密钥或解密帧。
  ScopedAStatus getKeyRequest(const std::vector<uint8_t>&, const std::vector<uint8_t>&,
                              const std::string&, drm::KeyType,
                              const std::vector<drm::KeyValue>&,
                              drm::KeyRequest*) override {
    return unsupported();
  }
  ScopedAStatus provideKeyResponse(const std::vector<uint8_t>&, const std::vector<uint8_t>&,
                                   drm::KeySetId*) override {
    return unsupported();
  }
  ScopedAStatus restoreKeys(const std::vector<uint8_t>&, const drm::KeySetId&) override {
    return unsupported();
  }
  ScopedAStatus removeKeys(const std::vector<uint8_t>&) override { return unsupported(); }
  ScopedAStatus getProvisionRequest(const std::string&, const std::string&,
                                    drm::ProvisionRequest*) override {
    return unsupported();
  }
  ScopedAStatus provideProvisionResponse(
      const std::vector<uint8_t>&, drm::ProvideProvisionResponseResult*) override {
    return unsupported();
  }
  ScopedAStatus getOfflineLicenseState(const drm::KeySetId&,
                                       drm::OfflineLicenseState*) override {
    return unsupported();
  }
  ScopedAStatus removeOfflineLicense(const drm::KeySetId&) override { return unsupported(); }
  ScopedAStatus getSecureStop(const drm::SecureStopId&, drm::SecureStop*) override {
    return unsupported();
  }
  ScopedAStatus encrypt(const std::vector<uint8_t>&, const std::vector<uint8_t>&,
                        const std::vector<uint8_t>&, const std::vector<uint8_t>&,
                        std::vector<uint8_t>*) override {
    return unsupported();
  }
  ScopedAStatus decrypt(const std::vector<uint8_t>&, const std::vector<uint8_t>&,
                        const std::vector<uint8_t>&, const std::vector<uint8_t>&,
                        std::vector<uint8_t>*) override {
    return unsupported();
  }
  ScopedAStatus sign(const std::vector<uint8_t>&, const std::vector<uint8_t>&,
                     const std::vector<uint8_t>&, std::vector<uint8_t>*) override {
    return unsupported();
  }
  ScopedAStatus signRSA(const std::vector<uint8_t>&, const std::string&,
                        const std::vector<uint8_t>&, const std::vector<uint8_t>&,
                        std::vector<uint8_t>*) override {
    return unsupported();
  }
  ScopedAStatus verify(const std::vector<uint8_t>&, const std::vector<uint8_t>&,
                       const std::vector<uint8_t>&, const std::vector<uint8_t>&,
                       bool*) override {
    return unsupported();
  }
};

// ---------------------------------------------------------------------------
// IDrmFactory/widevine：按 UUID 派发 Widevine 插件与加密插件。
// ---------------------------------------------------------------------------
class WidevineFactory : public drm::BnDrmFactory {
 public:
  ScopedAStatus createDrmPlugin(const drm::Uuid& in_uuid, const std::string&,
                                std::shared_ptr<drm::IDrmPlugin>* _aidl_return) override {
    if (!isWidevineUuid(in_uuid.uuid)) {
      _aidl_return->reset();  // 非 Widevine：交由其它工厂处理。
      return ScopedAStatus::ok();
    }
    *_aidl_return = ndk::SharedRefBase::make<DrmPlugin>();
    return ScopedAStatus::ok();
  }

  ScopedAStatus createCryptoPlugin(const drm::Uuid& in_uuid,
                                   const std::vector<uint8_t>& in_initData,
                                   std::shared_ptr<drm::ICryptoPlugin>* _aidl_return) override {
    if (!isWidevineUuid(in_uuid.uuid)) {
      _aidl_return->reset();
      return ScopedAStatus::ok();
    }
    auto crypto = ndk::SharedRefBase::make<CryptoPlugin>();
    if (!in_initData.empty()) {
      std::lock_guard<std::mutex> lock(g_gate);
      const auto pos = g_sessions.find(in_initData);
      if (pos != g_sessions.end() && pos->second.uid == AIBinder_getCallingUid()) {
        // initData 携带已存在的会话 id：留给 setMediaDrmSession 绑定，这里不重复校验。
      }
    }
    *_aidl_return = crypto;
    return ScopedAStatus::ok();
  }

  ScopedAStatus getSupportedCryptoSchemes(drm::CryptoSchemes* _aidl_return) override {
    _aidl_return->uuids.clear();
    _aidl_return->mimeTypes.clear();
    drm::Uuid u;
    u.uuid = kWidevineUuid;
    _aidl_return->uuids.push_back(u);
    u.uuid = kWidevineAlias;
    _aidl_return->uuids.push_back(u);
    static constexpr const char* kMimes[] = {
        "cenc",       "cbcs",       "cens",      "cbc1",      "video/mp4",
        "video/webm", "video/3gpp", "audio/mp4", "audio/webm", "audio/3gpp",
        "application/mp4",
    };
    for (const char* m : kMimes) {
      drm::SupportedContentType ct;
      ct.mime = m;
      ct.minLevel = drm::SecurityLevel::SW_SECURE_CRYPTO;
      ct.maxLevel = drm::SecurityLevel::HW_SECURE_ALL;
      _aidl_return->mimeTypes.push_back(std::move(ct));
    }
    return ScopedAStatus::ok();
  }
};

// ---------------------------------------------------------------------------
// IResourceManagerService ("media.resource_manager") 轻量级兜底服务：
// 在纯 64 位宿主（如 Apple Silicon HVF）上，若固件 /system/bin/mediaserver 为 32 位
// ELF 而无法 exec，系统将缺少 media.resource_manager，导致 libmediadrm 在
// openSession / closeSession 时卡在 DrmSessionManager::getResourceManagerService_l()
// 的 AServiceManager_waitForService("media.resource_manager")。
// 这里在 media.resource_manager 未上线时注册 NDK Binder 兜底桩。
// ---------------------------------------------------------------------------
constexpr const char* kResourceManagerServiceName = "media.resource_manager";
constexpr const char* kResourceManagerDescriptor = "android.media.IResourceManagerService";

void* onResourceManagerCreate(void* args) {
  return args;
}

void onResourceManagerDestroy(void* /*userData*/) {}

binder_status_t onResourceManagerTransact(AIBinder* /*binder*/, transaction_code_t code,
                                          const AParcel* /*in*/, AParcel* out) {
  ScopedAStatus ok = ScopedAStatus::ok();
  binder_status_t status = AParcel_writeStatusHeader(out, ok.get());
  if (status != STATUS_OK) {
    return status;
  }
  // reclaimResource (FIRST_CALL_TRANSACTION + 4) 返回 bool 出参。
  if (code == FIRST_CALL_TRANSACTION + 4) {
    status = AParcel_writeBool(out, false);
  }
  return status;
}

void registerResourceManagerFallbackIfNeeded() {
  AIBinder* existing = AServiceManager_checkService(kResourceManagerServiceName);
  if (existing != nullptr) {
    AIBinder_decStrong(existing);
    return;
  }
  AIBinder_Class* cls = AIBinder_Class_define(
      kResourceManagerDescriptor, onResourceManagerCreate,
      onResourceManagerDestroy, onResourceManagerTransact);
  if (cls == nullptr) {
    __android_log_print(ANDROID_LOG_WARN, "DrmWidevine",
                        "failed to define %s", kResourceManagerDescriptor);
    return;
  }
  static int sToken = 1;
  AIBinder* binder = AIBinder_new(cls, &sToken);
  if (binder == nullptr) {
    return;
  }
  if (AServiceManager_addService(binder, kResourceManagerServiceName) == STATUS_OK) {
    __android_log_print(ANDROID_LOG_INFO, "DrmWidevine",
                        "registered fallback %s", kResourceManagerServiceName);
  } else {
    __android_log_print(ANDROID_LOG_WARN, "DrmWidevine",
                        "failed to register fallback %s", kResourceManagerServiceName);
  }
}

}  // namespace

int main() {
  if (!identity::load()) {
    __android_log_print(ANDROID_LOG_ERROR, "DrmWidevine",
                        "DRM identity initialization failed; using in-memory only");
  }
  ABinderProcess_setThreadPoolMaxThreadCount(4);
  auto svc = ndk::SharedRefBase::make<WidevineFactory>();
  if (AServiceManager_addService(svc->asBinder().get(), kWidevineFactoryName) != STATUS_OK) {
    __android_log_print(ANDROID_LOG_ERROR, "DrmWidevine",
                        "failed to register %s", kWidevineFactoryName);
    return 2;
  }
  registerResourceManagerFallbackIfNeeded();
  __android_log_print(ANDROID_LOG_INFO, "DrmWidevine",
                      "widevine detection factory ready");
  ABinderProcess_joinThreadPool();
  return 0;
}
