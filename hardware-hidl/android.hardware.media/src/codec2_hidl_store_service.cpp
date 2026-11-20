// engine/vendor/hardware —— Android 13 HIDL android.hardware.media.c2@1.2 辅助空 Store 服务。

#define LOG_TAG "CdhMediaC2StoreHidl"

#include <android/hardware/media/c2/1.2/IComponentStore.h>
#include <hidl/HidlTransportSupport.h>
#include <log/log.h>

#include <cstring>
#include <string>

namespace {

using ::android::sp;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::media::bufferpool::V2_0::IClientManager;
using ::android::hardware::media::c2::V1_0::Buffer;
using ::android::hardware::media::c2::V1_0::IComponentInterface;
using ::android::hardware::media::c2::V1_0::IComponentListener;
using ::android::hardware::media::c2::V1_0::IConfigurable;
using ::android::hardware::media::c2::V1_0::IInputSurface;
using ::android::hardware::media::c2::V1_0::ParamIndex;
using ::android::hardware::media::c2::V1_0::Status;
using ::android::hardware::media::c2::V1_0::StructDescriptor;
using V1_0_Comp = ::android::hardware::media::c2::V1_0::IComponent;
using V1_1_Comp = ::android::hardware::media::c2::V1_1::IComponent;
using V1_2_Comp = ::android::hardware::media::c2::V1_2::IComponent;
using ::android::hardware::media::c2::V1_2::IComponentStore;

class EmptyComponentStore : public IComponentStore {
 public:
  Return<void> createComponent(const hidl_string &, const sp<IComponentListener> &,
                               const sp<IClientManager> &, createComponent_cb _hidl_cb) override {
    _hidl_cb(Status::NOT_FOUND, nullptr);
    return Void();
  }

  Return<void> createComponent_1_1(const hidl_string &, const sp<IComponentListener> &,
                                   const sp<IClientManager> &, createComponent_1_1_cb _hidl_cb) override {
    _hidl_cb(Status::NOT_FOUND, nullptr);
    return Void();
  }

  Return<void> createComponent_1_2(const hidl_string &, const sp<IComponentListener> &,
                                   const sp<IClientManager> &, createComponent_1_2_cb _hidl_cb) override {
    _hidl_cb(Status::NOT_FOUND, nullptr);
    return Void();
  }

  Return<void> createInterface(const hidl_string &, createInterface_cb _hidl_cb) override {
    _hidl_cb(Status::NOT_FOUND, nullptr);
    return Void();
  }

  Return<void> listComponents(listComponents_cb _hidl_cb) override {
    hidl_vec<ComponentTraits> empty;
    _hidl_cb(Status::OK, empty);
    return Void();
  }

  Return<void> createInputSurface(createInputSurface_cb _hidl_cb) override {
    _hidl_cb(Status::OMITTED, nullptr);
    return Void();
  }

  Return<void> getStructDescriptors(const hidl_vec<ParamIndex> &,
                                    getStructDescriptors_cb _hidl_cb) override {
    hidl_vec<StructDescriptor> empty;
    _hidl_cb(Status::NOT_FOUND, empty);
    return Void();
  }

  Return<Status> copyBuffer(const Buffer &, const Buffer &) override {
    return Status::OMITTED;
  }

  Return<sp<IClientManager>> getPoolClientManager() override {
    return nullptr;
  }

  Return<sp<IConfigurable>> getConfigurable() override {
    return nullptr;
  }
};

}  // namespace

int main(int argc, char **argv) {
  std::string instance = "default1";
  if (argc > 0 && argv[0] != nullptr) {
    if (std::strstr(argv[0], "dolby") != nullptr) {
      instance = "default9";
    }
  }
  ::android::hardware::configureRpcThreadpool(4, true);
  sp<IComponentStore> store = new EmptyComponentStore();
  if (store->registerAsService(instance) != ::android::OK) {
    ALOGE("Failed to register IComponentStore@1.2/%s", instance.c_str());
    return 1;
  }
  ALOGI("IComponentStore@1.2/%s ready", instance.c_str());
  ::android::hardware::joinRpcThreadpool();
  return 1;
}
