// 纯 C 源码实现 Android 13 QSSI 跨代运行 Wi-Fi AIDL NDK 库所需的 APersistableBundle_* 兼容桩。
//
// Android 13 官方 /system/lib64/libbinder_ndk.so 尚未导出 LIBBINDER_NDK35 的
// APersistableBundle_* 符号；通过 init rc 的 setenv LD_PRELOAD 为 wpa_supplicant / hostapd
// 预加载本库即可满足动态链接符号解析，无需修补任何 ELF 的 .dynsym。

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct APersistableBundle {
    uint32_t reserved;
} APersistableBundle;

typedef struct AParcel AParcel;
typedef int32_t binder_status_t;

enum {
    STATUS_OK = 0,
    STATUS_BAD_VALUE = -22,
};

__attribute__((visibility("default")))
APersistableBundle *APersistableBundle_new(void) {
    return (APersistableBundle *)calloc(1, sizeof(APersistableBundle));
}

__attribute__((visibility("default")))
void APersistableBundle_delete(APersistableBundle *bundle) {
    free(bundle);
}

__attribute__((visibility("default")))
APersistableBundle *APersistableBundle_dup(const APersistableBundle *bundle) {
    if (bundle == NULL) {
        return NULL;
    }
    return APersistableBundle_new();
}

__attribute__((visibility("default")))
binder_status_t APersistableBundle_readFromParcel(const AParcel *parcel, APersistableBundle **outBuffer) {
    (void)parcel;
    if (outBuffer == NULL) {
        return STATUS_BAD_VALUE;
    }
    *outBuffer = APersistableBundle_new();
    return STATUS_OK;
}

__attribute__((visibility("default")))
binder_status_t APersistableBundle_writeToParcel(const APersistableBundle *bundle, AParcel *parcel) {
    (void)bundle;
    (void)parcel;
    return STATUS_OK;
}
