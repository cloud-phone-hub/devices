// engine/vendor/hardware 线 —— android.hardware.secure_element（ISecureElement v1）自研实现。
//
// 注册 android.hardware.secure_element.ISecureElement/eSE1。云机没有真实安全单元
// （无 QSEECom / 无 eSE 通路），故本服务**如实上报"无卡"**：isCardPresent=false、
// init 回 onStateChange(false)，openBasicChannel / openLogicalChannel / transmit /
// getAtr 一律回空、closeChannel / reset 直接成功，不宣称存在任何真实 eSE。
//
// 为什么只提供 eSE1：m22 官方 manifest 声明的是 SIM1/SIM2（无 eSE1），而框架侧
// Telephony/OMAPI 在本基底上查找的是 eSE1 实例；提供 eSE1 不与其冲突，也不引入
// 对 SIM 通道的错误承诺。
//
// 不落 VINTF 片段（设备集成层职责）：一旦集成层已声明同一 FqInstance，模块再自带
// 分件会触发 libvintf "Conflicting FqInstance" → NULL VINTF MANIFEST。
// 设备无关，不内置机型常量。

#include <android/binder_ibinder.h>
#include <android/binder_status.h>

extern "C" {
void ABinderProcess_setThreadPoolMaxThreadCount(uint32_t numThreads);
void ABinderProcess_joinThreadPool(void);
binder_status_t AServiceManager_addService(AIBinder *binder,
                                           const char *instance);
}

#include <aidl/android/hardware/secure_element/BnSecureElement.h>
#include <aidl/android/hardware/secure_element/ISecureElementCallback.h>
#include <aidl/android/hardware/secure_element/LogicalChannelResponse.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using aidl::android::hardware::secure_element::BnSecureElement;
using aidl::android::hardware::secure_element::ISecureElementCallback;
using aidl::android::hardware::secure_element::LogicalChannelResponse;
using ndk::ScopedAStatus;

namespace {
constexpr const char *kSecureElementServiceName =
    "android.hardware.secure_element.ISecureElement/eSE1";
// 无真实安全单元：isCardPresent 恒 false，init 回调也报 false。
constexpr bool kSecureElementPresent = false;
} // namespace

// 虚拟 eSE：如实上报「无安全单元」，所有 APDU 通道回空。
class SecureElementService : public BnSecureElement {
public:
  ScopedAStatus closeChannel(int8_t channelNumber) override {
    (void)channelNumber;
    return ScopedAStatus::ok();
  }

  ScopedAStatus getAtr(std::vector<uint8_t> *out) override {
    out->clear();
    return ScopedAStatus::ok();
  }

  ScopedAStatus
  init(const std::shared_ptr<ISecureElementCallback> &clientCallback) override {
    if (clientCallback != nullptr) {
      // onStateChange 是 oneway：通告「未连接」，不会阻塞调用方。
      clientCallback->onStateChange(kSecureElementPresent,
                                    "cdh virtual secure element");
    }
    return ScopedAStatus::ok();
  }

  ScopedAStatus isCardPresent(bool *out) override {
    *out = kSecureElementPresent;
    return ScopedAStatus::ok();
  }

  ScopedAStatus openBasicChannel(const std::vector<uint8_t> &aid, int8_t p2,
                                 std::vector<uint8_t> *out) override {
    (void)aid;
    (void)p2;
    out->clear();
    return ScopedAStatus::ok();
  }

  ScopedAStatus openLogicalChannel(const std::vector<uint8_t> &aid, int8_t p2,
                                   LogicalChannelResponse *out) override {
    (void)aid;
    (void)p2;
    out->channelNumber = 0;
    out->selectResponse.clear();
    return ScopedAStatus::ok();
  }

  ScopedAStatus reset() override { return ScopedAStatus::ok(); }

  ScopedAStatus transmit(const std::vector<uint8_t> &data,
                         std::vector<uint8_t> *out) override {
    (void)data;
    out->clear();
    return ScopedAStatus::ok();
  }
};

int main() {
  ABinderProcess_setThreadPoolMaxThreadCount(2);
  auto element = ndk::SharedRefBase::make<SecureElementService>();
  if (AServiceManager_addService(element->asBinder().get(),
                                 kSecureElementServiceName) != STATUS_OK) {
    return 2;
  }
  ABinderProcess_joinThreadPool();
  return 0;
}
