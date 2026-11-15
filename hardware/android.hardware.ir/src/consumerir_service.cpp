// engine/vendor/hardware 线 —— android.hardware.ir（IConsumerIr v1）自研服务实现。
//
// 注册 android.hardware.ir.IConsumerIr/default，供 framework 的 ConsumerIrService
// 与上层遥控类应用使用。云端虚拟机没有红外发射器（无 /dev/lirc、无 GPIO 载波），
// 故按一份**设备无关的默认口径**实现：
//   - getCarrierFreqs() 返回 AOSP 参考实现同款载频覆盖区间，调用方据此挑选频率；
//   - transmit() 只记日志、不驱动硬件，保证调用链不因缺硬件而抛异常。
//
// 为什么要这个"空实现"：官方 vendor 的 etc/permissions 整目录替换会带来
// android.hardware.consumerir.xml，PackageManager 据此声明 FEATURE_CONSUMER_IR；
// SystemServer 的 ConsumerIrService 在特性存在而找不到 IConsumerIr/default 时会让
// 框架不稳定（详见 build/07_build_vendor/replace.go 的说明），因此特性与 HAL 必须
// 成对出现。真机红外能力将来接入时，替换 transmit() 的落地实现即可。
//
// SELinux：执行域沿用 lineage vendor 策略里已定义的 hal_ir_default，
// 文件标签由本模块 sepolicy/vendor_file_contexts 片段给出。

#include <android/binder_ibinder.h>
#include <android/binder_interface_utils.h>
#include <android/binder_parcel.h>
#include <android/binder_parcel_utils.h>
#include <android/binder_status.h>

extern "C" {
void ABinderProcess_setThreadPoolMaxThreadCount(uint32_t numThreads);
void ABinderProcess_joinThreadPool(void);
binder_status_t AServiceManager_addService(AIBinder *binder,
                                           const char *instance);
}

#include <aidl/android/hardware/ir/BnConsumerIr.h>
#include <aidl/android/hardware/ir/ConsumerIrFreqRange.h>

#include <android/log.h>
#include <sys/system_properties.h>
#include <unistd.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using aidl::android::hardware::ir::BnConsumerIr;
using aidl::android::hardware::ir::ConsumerIrFreqRange;
using ndk::ScopedAStatus;

namespace {

constexpr const char *kTag = "cdh-consumerir";
constexpr const char *kServiceName = "android.hardware.ir.IConsumerIr/default";
constexpr const char *kMlipayServiceName =
    "vendor.xiaomi.hardware.mlipay.IMlipayService/default";
constexpr const char *kMlipayDescriptor =
    "vendor.xiaomi.hardware.mlipay.IMlipayService";
constexpr int32_t kMlipayVersion = 1;
constexpr const char *kMlipayHash =
    "d0490861d2dfbb90d050835773bb9e2ce113a0b7";

// 云机不挑频：声明 30k~56kHz 的单段覆盖，与 AOSP 参考实现给出的载频面一致。
constexpr int32_t kMinCarrierHz = 30000;
constexpr int32_t kMaxCarrierHz = 56000;

// 真机 IFAA TEE invoke_command 56B 标准响应载荷（默认兜底，Base64:
// AAAAADAAAAAQAAIACAABAKo6LCieTebE3yF3QFESAr7PjWODCqInwCQG7l8D1pQiaUKCAzOCtzA=）。
// 优先按机型读取 /vendor/etc/cdh_ifaa_response.hex 或属性 ro.vendor.cdh.ifaa.response_b64。
const std::vector<uint8_t> kDefaultIfaaResponse = {
    0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x10, 0x00, 0x02, 0x00,
    0x08, 0x00, 0x01, 0x00, 0xaa, 0x3a, 0x2c, 0x28, 0x9e, 0x4d, 0xe6, 0xc4,
    0xdf, 0x21, 0x77, 0x40, 0x51, 0x12, 0x02, 0xbe, 0xcf, 0x8d, 0x63, 0x83,
    0x0a, 0xa2, 0x27, 0xc0, 0x24, 0x06, 0xee, 0x5f, 0x03, 0xd6, 0x94, 0x22,
    0x69, 0x42, 0x82, 0x03, 0x33, 0x82, 0xb7, 0x30,
};

int HexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

std::vector<uint8_t> DecodeHex(const char *s) {
  std::vector<uint8_t> out;
  int hi = -1;
  for (const char *p = s; *p != '\0'; ++p) {
    if (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') continue;
    int v = HexNibble(*p);
    if (v < 0) return {};
    if (hi < 0) {
      hi = v;
    } else {
      out.push_back(static_cast<uint8_t>((hi << 4) | v));
      hi = -1;
    }
  }
  if (hi >= 0) return {};
  return out;
}

int Base64Val(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

std::vector<uint8_t> DecodeBase64(const char *s) {
  std::vector<uint8_t> out;
  uint32_t acc = 0;
  int bits = 0;
  for (const char *p = s; *p != '\0'; ++p) {
    if (*p == '=' || *p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
      continue;
    }
    int v = Base64Val(*p);
    if (v < 0) return {};
    acc = (acc << 6) | static_cast<uint32_t>(v);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out.push_back(static_cast<uint8_t>((acc >> bits) & 0xFFu));
    }
  }
  return out;
}

std::vector<uint8_t> LoadIfaaResponse() {
  if (FILE *fp = fopen("/vendor/etc/cdh_ifaa_response.hex", "r")) {
    char buf[1024] = {0};
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    if (n > 0) {
      buf[n] = '\0';
      auto decoded = DecodeHex(buf);
      if (!decoded.empty()) return decoded;
    }
  }
  char prop[PROP_VALUE_MAX] = {0};
  if (__system_property_get("ro.vendor.cdh.ifaa.response_b64", prop) > 0 &&
      prop[0] != '\0') {
    auto decoded = DecodeBase64(prop);
    if (!decoded.empty()) return decoded;
  }
  return kDefaultIfaaResponse;
}

const std::vector<uint8_t> &GetIfaaResponse() {
  static const std::vector<uint8_t> kCached = LoadIfaaResponse();
  return kCached;
}

binder_status_t MlipayService_onTransact(AIBinder *_aidl_binder,
                                         transaction_code_t _aidl_code,
                                         const AParcel *_aidl_in,
                                         AParcel *_aidl_out) {
  (void)_aidl_binder;
  binder_status_t _aidl_ret_status = STATUS_UNKNOWN_TRANSACTION;
  switch (_aidl_code) {
    case (FIRST_CALL_TRANSACTION + 0 /*ifaa_get_idlist*/): {
      int32_t in_param = 0;
      _aidl_ret_status = ::ndk::AParcel_readData(_aidl_in, &in_param);
      if (_aidl_ret_status != STATUS_OK) break;

      ScopedAStatus _aidl_status = ScopedAStatus::ok();
      _aidl_ret_status =
          AParcel_writeStatusHeader(_aidl_out, _aidl_status.get());
      if (_aidl_ret_status != STATUS_OK) break;

      std::vector<int32_t> _aidl_return = {1};
      _aidl_ret_status = ::ndk::AParcel_writeData(_aidl_out, _aidl_return);
      break;
    }
    case (FIRST_CALL_TRANSACTION + 1 /*ifaa_key_dump*/): {
      ScopedAStatus _aidl_status = ScopedAStatus::ok();
      _aidl_ret_status =
          AParcel_writeStatusHeader(_aidl_out, _aidl_status.get());
      if (_aidl_ret_status != STATUS_OK) break;

      std::string _aidl_return;
      _aidl_ret_status = ::ndk::AParcel_writeData(_aidl_out, _aidl_return);
      break;
    }
    case (FIRST_CALL_TRANSACTION + 2 /*ifaa_key_get_version*/): {
      ScopedAStatus _aidl_status = ScopedAStatus::ok();
      _aidl_ret_status =
          AParcel_writeStatusHeader(_aidl_out, _aidl_status.get());
      if (_aidl_ret_status != STATUS_OK) break;

      int32_t _aidl_return = 1;
      _aidl_ret_status = ::ndk::AParcel_writeData(_aidl_out, _aidl_return);
      break;
    }
    case (FIRST_CALL_TRANSACTION + 3 /*ifaa_key_load*/): {
      std::string in_arg0;
      std::string in_arg1;
      _aidl_ret_status = ::ndk::AParcel_readData(_aidl_in, &in_arg0);
      if (_aidl_ret_status != STATUS_OK) break;
      _aidl_ret_status = ::ndk::AParcel_readData(_aidl_in, &in_arg1);
      if (_aidl_ret_status != STATUS_OK) break;

      ScopedAStatus _aidl_status = ScopedAStatus::ok();
      _aidl_ret_status =
          AParcel_writeStatusHeader(_aidl_out, _aidl_status.get());
      if (_aidl_ret_status != STATUS_OK) break;

      int32_t _aidl_return = 0;
      _aidl_ret_status = ::ndk::AParcel_writeData(_aidl_out, _aidl_return);
      break;
    }
    case (FIRST_CALL_TRANSACTION + 4 /*ifaa_key_prepare*/): {
      ScopedAStatus _aidl_status = ScopedAStatus::ok();
      _aidl_ret_status =
          AParcel_writeStatusHeader(_aidl_out, _aidl_status.get());
      if (_aidl_ret_status != STATUS_OK) break;

      std::string _aidl_return;
      _aidl_ret_status = ::ndk::AParcel_writeData(_aidl_out, _aidl_return);
      break;
    }
    case (FIRST_CALL_TRANSACTION + 5 /*invoke_command*/): {
      std::vector<uint8_t> in_param;
      int32_t in_len = 0;
      _aidl_ret_status = ::ndk::AParcel_readData(_aidl_in, &in_param);
      if (_aidl_ret_status != STATUS_OK) break;
      _aidl_ret_status = ::ndk::AParcel_readData(_aidl_in, &in_len);
      if (_aidl_ret_status != STATUS_OK) break;

      ScopedAStatus _aidl_status = ScopedAStatus::ok();
      _aidl_ret_status =
          AParcel_writeStatusHeader(_aidl_out, _aidl_status.get());
      if (_aidl_ret_status != STATUS_OK) break;

      _aidl_ret_status =
          ::ndk::AParcel_writeData(_aidl_out, GetIfaaResponse());
      break;
    }
    case (FIRST_CALL_TRANSACTION + 16777214 /*getInterfaceVersion*/): {
      ScopedAStatus _aidl_status = ScopedAStatus::ok();
      _aidl_ret_status =
          AParcel_writeStatusHeader(_aidl_out, _aidl_status.get());
      if (_aidl_ret_status != STATUS_OK) break;

      int32_t _aidl_return = kMlipayVersion;
      _aidl_ret_status = ::ndk::AParcel_writeData(_aidl_out, _aidl_return);
      break;
    }
    case (FIRST_CALL_TRANSACTION + 16777213 /*getInterfaceHash*/): {
      ScopedAStatus _aidl_status = ScopedAStatus::ok();
      _aidl_ret_status =
          AParcel_writeStatusHeader(_aidl_out, _aidl_status.get());
      if (_aidl_ret_status != STATUS_OK) break;

      std::string _aidl_return(kMlipayHash);
      _aidl_ret_status = ::ndk::AParcel_writeData(_aidl_out, _aidl_return);
      break;
    }
  }
  return _aidl_ret_status;
}

AIBinder_Class *gMlipayClass = ::ndk::ICInterface::defineClass(
    kMlipayDescriptor, MlipayService_onTransact, nullptr, 0);

class IMlipayService : public ::ndk::ICInterface {
public:
  IMlipayService() = default;
  ~IMlipayService() override = default;
};

class MlipayService : public ::ndk::BnCInterface<IMlipayService> {
public:
  MlipayService() = default;
  ~MlipayService() override = default;

protected:
  ::ndk::SpAIBinder createBinder() override {
    AIBinder *binder = AIBinder_new(gMlipayClass, static_cast<void *>(this));
#ifdef BINDER_STABILITY_SUPPORT
    AIBinder_markVintfStability(binder);
#endif
    return ::ndk::SpAIBinder(binder);
  }
};

class ConsumerIrService : public BnConsumerIr {
public:
  ScopedAStatus getCarrierFreqs(
      std::vector<ConsumerIrFreqRange> *out) override {
    if (out == nullptr) {
      return ScopedAStatus::fromExceptionCode(EX_NULL_POINTER);
    }
    ConsumerIrFreqRange range;
    range.minHz = kMinCarrierHz;
    range.maxHz = kMaxCarrierHz;
    out->clear();
    out->push_back(range);
    return ScopedAStatus::ok();
  }

  ScopedAStatus transmit(int32_t carrierFreqHz,
                         const std::vector<int32_t> &pattern) override {
    __android_log_print(ANDROID_LOG_INFO, kTag,
                        "transmit: %d Hz, %zu slots (no hardware, ignored)",
                        carrierFreqHz, pattern.size());
    return ScopedAStatus::ok();
  }
};

bool HasMlipayVintfDeclaration() {
  return access("/vendor/etc/vintf/manifest/manifest_vendor.xiaomi.hardware.mlipay.xml",
                F_OK) == 0 ||
         access("/odm/etc/vintf/manifest/manifest_vendor.xiaomi.hardware.mlipay.xml",
                F_OK) == 0;
}

}  // namespace

int main() {
  ABinderProcess_setThreadPoolMaxThreadCount(1);
  auto service = ndk::SharedRefBase::make<ConsumerIrService>();
  if (AServiceManager_addService(service->asBinder().get(), kServiceName) !=
      STATUS_OK) {
    __android_log_print(ANDROID_LOG_ERROR, kTag, "addService failed: %s",
                        kServiceName);
    return 2;
  }
  __android_log_print(ANDROID_LOG_INFO, kTag, "registered %s", kServiceName);

  std::shared_ptr<MlipayService> mlipayService;
  if (HasMlipayVintfDeclaration()) {
    mlipayService = ndk::SharedRefBase::make<MlipayService>();
    if (AServiceManager_addService(mlipayService->asBinder().get(),
                                   kMlipayServiceName) == STATUS_OK) {
      __android_log_print(ANDROID_LOG_INFO, kTag, "registered %s",
                          kMlipayServiceName);
    } else {
      __android_log_print(ANDROID_LOG_WARN, kTag, "addService skipped: %s",
                          kMlipayServiceName);
    }
  }

  ABinderProcess_joinThreadPool();
  return 0;
}
