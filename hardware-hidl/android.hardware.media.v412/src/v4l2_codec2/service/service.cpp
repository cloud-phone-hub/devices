// Copyright 2020 The Chromium Authors. All rights reserved.
// Android 13 HIDL IComponentStore@1.2 entry point for v4l2_codec2.

#ifdef V4L2_CODEC2_SERVICE_V4L2_STORE
#define LOG_TAG "android.hardware.media.c2@1.2-service-v4l2"
#else
#error "V4L2_CODEC2_SERVICE_V4L2_STORE has to be defined"
#endif

#include <C2Component.h>
#include <codec2/hidl/1.2/ComponentStore.h>
#include <hidl/HidlTransportSupport.h>
#include <log/log.h>
#include <minijail.h>

#ifdef V4L2_CODEC2_SERVICE_V4L2_STORE
#include <v4l2_codec2/v4l2/V4L2ComponentStore.h>
#endif

int main(int /* argc */, char** /* argv */) {
    ALOGD("HIDL 1.2 Service starting...");
    signal(SIGPIPE, SIG_IGN);

    android::hardware::configureRpcThreadpool(16, true /* callerWillJoin */);

    const std::shared_ptr<C2ComponentStore> componentStore =
            android::V4L2ComponentStore::Create();
    if (componentStore == nullptr) {
        ALOGE("Cannot create Codec2's V4L2 component store.");
        return 1;
    }

    android::sp<android::hardware::media::c2::V1_2::IComponentStore> store =
            new android::hardware::media::c2::V1_2::utils::ComponentStore(componentStore);
    if (store->registerAsService("default") != android::OK) {
        ALOGE("Cannot register Codec2's HIDL IComponentStore@1.2 service.");
        return 1;
    }
    ALOGI("Codec2's HIDL IComponentStore@1.2 service created.");

    android::hardware::joinRpcThreadpool();
    ALOGD("Service shutdown.");
    return 0;
}
