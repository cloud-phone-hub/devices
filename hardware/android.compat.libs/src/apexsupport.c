// 纯 C 源码实现非 APEX 摊平运行态下的 libapexsupport.so 桩函数。
//
// 当官方 system.img 未包含 /system/lib64/libapexsupport.so（如 Android 13 QSSI 机型）
// 而 vendor 基底 libbinder.so 带有 DT_NEEDED: libapexsupport.so 时，由本库原样满足链接契约，
// 零字节修改任何预编译 ELF 二进制。

#include <dlfcn.h>
#include <stddef.h>
#include <stdint.h>

typedef struct AApexInfo AApexInfo;

enum {
    AAPEXINFO_NO_APEX = 1,
};

__attribute__((visibility("default")))
int32_t AApexInfo_create(AApexInfo **out) {
    if (out != NULL) {
        *out = NULL;
    }
    return AAPEXINFO_NO_APEX;
}

__attribute__((visibility("default")))
void AApexInfo_destroy(AApexInfo *info) {
    (void)info;
}

__attribute__((visibility("default")))
const char *AApexInfo_getName(const AApexInfo *info) {
    (void)info;
    return "";
}

__attribute__((visibility("default")))
int64_t AApexInfo_getVersion(const AApexInfo *info) {
    (void)info;
    return 0;
}

__attribute__((visibility("default")))
void *AApexSupport_loadLibrary(const char *name, const AApexInfo *info, int flags) {
    (void)info;
    if (name == NULL) {
        return NULL;
    }
    return dlopen(name, flags);
}
