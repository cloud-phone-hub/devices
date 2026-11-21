// CDH Gralloc0 同进程单例转发层（gralloc_proxy.c）：
//
// 职责与设计背景：
//   上游 external/minigbm 的 cros_gralloc_driver::get_instance() 使用函数局部静态变量
//   维护进程内已导入 buffer_handle_t 的注册表（handles_）。
//   当脱离底包 libminigbm_gralloc.so、采用纯源码自编无 DT_SONAME 库时，若 mapper.<suffix>.so
//   与 gralloc.<suffix>.so 各自静态内聚一份 libminigbm_core.a，Bionic RTLD_LOCAL 会产生两份
//   独立的 cros_gralloc_driver 单例：libui 通过 IMapper::importBuffer 注册在 mapper 单例中，
//   而 Mesa 通过 gralloc0 perform(GRALLOC_DRM_GET_BUFFER_INFO) 查询 gralloc 单例时就会报
//   "Invalid reference (resource_info() called on unregistered handle)"。
//
// 零二进制修改根治方案：
//   1. 将上游 cros_gralloc/gralloc0/gralloc0.cc 直接编入 mapper.minigbm.so，使其同时导出
//      AIMapper_loadIMapper（v5）、HIDL_FETCH_IMapper（v4）与 HAL_MODULE_INFO_SYM（HMI），
//      共享进程内唯一一份 cros_gralloc_driver 单例；
//   2. gralloc.minigbm.so 由本纯 C 轻量代理编译产出（不链接 libminigbm_core.a），运行态读取
//      公开属性 ro.hardware.gralloc（exported_default_prop，全进程可读），dlopen 同目录下的
//      mapper.<suffix>.so（Bionic 按 inode 复用同一 soinfo），dlsym("HMI") 将全部 gralloc0
//      调用转发至 mapper 内的唯一实例。

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#define LOG_TAG "CdhGrallocProxy"

#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <cutils/properties.h>
#include <hardware/gralloc.h>
#include <log/log.h>

// 与 @platform/external/minigbm/cros_gralloc/gralloc0/gralloc0.cc 保持一致的操作码定义。
enum {
    CDH_GRALLOC_DRM_GET_STRIDE = 0,
    CDH_GRALLOC_DRM_GET_FORMAT = 1,
    CDH_GRALLOC_DRM_GET_DIMENSIONS = 2,
    CDH_GRALLOC_DRM_GET_BACKING_STORE = 3,
    CDH_GRALLOC_DRM_GET_BUFFER_INFO = 4,
    CDH_GRALLOC_DRM_GET_USAGE = 5,
};

static pthread_once_t g_target_once = PTHREAD_ONCE_INIT;
static const gralloc_module_t *g_target = NULL;

static void *try_open_mapper(const char *path) {
    void *h = dlopen(path, RTLD_NOW | RTLD_LOCAL | RTLD_NOLOAD);
    if (h != NULL) {
        return h;
    }
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}

static void *try_open_mapper_by_suffix(const char *self_dir, const char *const *dirs,
                                       size_t num_dirs, const char *cand) {
    char path[256];
    void *handle = NULL;
    if (cand == NULL || cand[0] == '\0') {
        return NULL;
    }
    if (self_dir != NULL && self_dir[0] != '\0') {
        snprintf(path, sizeof(path), "%s/mapper.%s.so", self_dir, cand);
        handle = try_open_mapper(path);
    }
    for (size_t i = 0; i < num_dirs && handle == NULL; ++i) {
        snprintf(path, sizeof(path), "%s/mapper.%s.so", dirs[i], cand);
        handle = try_open_mapper(path);
    }
    if (handle == NULL) {
        snprintf(path, sizeof(path), "mapper.%s.so", cand);
        handle = try_open_mapper(path);
    }
    return handle;
}

static void init_target(void) {
    char suffix[PROPERTY_VALUE_MAX] = {0};
    char self_suffix[PROPERTY_VALUE_MAX] = {0};
    char prop_suffix[PROPERTY_VALUE_MAX] = {0};
    char self_dir[192] = {0};
    void *handle = NULL;

#if defined(__LP64__)
    const char *dirs[] = {"/vendor/lib64/hw", "/system/vendor/lib64/hw"};
#else
    const char *dirs[] = {"/vendor/lib/hw", "/system/vendor/lib/hw"};
#endif
    const size_t num_dirs = sizeof(dirs) / sizeof(dirs[0]);

    // 1. 优先用 dladdr 取当前 gralloc.<suffix>.so 自身所在目录与文件名：
    //    - 若自身为 gralloc.<vendor>.so（非 default），优先尝试同目录/系统目录的 mapper.<vendor>.so；
    //    - 若 gralloc 后缀与 mapper 后缀不同（如 MTK 机型 gralloc.common.so + mapper.mediatek.so），
    //      打开失败时继续向下走属性、已知厂商列表与目录扫描兜底，不得卡死在不存在的 mapper.common.so。
    Dl_info dl_info;
    if (dladdr((const void *)&init_target, &dl_info) != 0 && dl_info.dli_fname != NULL) {
        const char *slash = strrchr(dl_info.dli_fname, '/');
        const char *base = slash ? (slash + 1) : dl_info.dli_fname;
        if (slash != NULL) {
            size_t dir_len = (size_t)(slash - dl_info.dli_fname);
            if (dir_len > 0 && dir_len < sizeof(self_dir)) {
                memcpy(self_dir, dl_info.dli_fname, dir_len);
                self_dir[dir_len] = '\0';
            }
        }
        size_t base_len = strlen(base);
        if (strncmp(base, "gralloc.", 8) == 0 && base_len > 11 &&
            strcmp(base + base_len - 3, ".so") == 0) {
            size_t suf_len = base_len - 11;
            if (suf_len < sizeof(self_suffix) && strncmp(base + 8, "default", suf_len) != 0) {
                memcpy(self_suffix, base + 8, suf_len);
                self_suffix[suf_len] = '\0';
                handle = try_open_mapper_by_suffix(self_dir, dirs, num_dirs, self_suffix);
                if (handle != NULL) {
                    snprintf(suffix, sizeof(suffix), "%s", self_suffix);
                }
            }
        }
    }

    // 2. 属性探测（若机型保留了官方 ro.hardware.gralloc 且与自身文件名后缀不同）
    if (handle == NULL) {
        property_get("ro.hardware.gralloc", prop_suffix, "");
        if (prop_suffix[0] != '\0' && strcmp(prop_suffix, self_suffix) != 0) {
            handle = try_open_mapper_by_suffix(self_dir, dirs, num_dirs, prop_suffix);
            if (handle != NULL) {
                snprintf(suffix, sizeof(suffix), "%s", prop_suffix);
            }
        }
    }

    // 3. 确定性常见厂商 mapper 后缀直连探测（纯文件 open，兼容无目录读权限的 isolated_app 沙箱）
    if (handle == NULL) {
        static const char *const kKnownSuffixes[] = {
            "mediatek", "qti", "pixel", "arm", "exynos", "minigbm"
        };
        for (size_t k = 0; k < sizeof(kKnownSuffixes) / sizeof(kKnownSuffixes[0]) && handle == NULL; ++k) {
            const char *cand = kKnownSuffixes[k];
            if (strcmp(cand, self_suffix) == 0 || strcmp(cand, prop_suffix) == 0) {
                continue;
            }
            handle = try_open_mapper_by_suffix(self_dir, dirs, num_dirs, cand);
            if (handle != NULL) {
                snprintf(suffix, sizeof(suffix), "%s", cand);
            }
        }
    }

    // 4. 目录扫描兜底（针对不在常见列表中的自定义厂商后缀）
    if (handle == NULL) {
        for (size_t i = 0; i < num_dirs && handle == NULL; ++i) {
            DIR *d = opendir(dirs[i]);
            if (d == NULL) {
                continue;
            }
            struct dirent *ent = NULL;
            while ((ent = readdir(d)) != NULL && handle == NULL) {
                const char *name = ent->d_name;
                size_t nlen = strlen(name);
                if (strncmp(name, "mapper.", 7) == 0 && nlen > 10 &&
                    strcmp(name + nlen - 3, ".so") == 0) {
                    size_t slen = nlen - 10;
                    if (slen > 0 && slen < sizeof(suffix) &&
                        strncmp(name + 7, "default", slen) != 0 &&
                        strncmp(name + 7, "minigbm", 7) != 0) {
                        char cand[PROPERTY_VALUE_MAX] = {0};
                        memcpy(cand, name + 7, slen);
                        cand[slen] = '\0';
                        handle = try_open_mapper_by_suffix(self_dir, dirs, num_dirs, cand);
                        if (handle != NULL) {
                            snprintf(suffix, sizeof(suffix), "%s", cand);
                        }
                    }
                }
            }
            closedir(d);
        }
    }

    if (handle == NULL) {
        ALOGE("failed to dlopen mapper (self=%s prop=%s): %s",
              self_suffix[0] ? self_suffix : "<none>",
              prop_suffix[0] ? prop_suffix : "<none>",
              dlerror());
        return;
    }

    g_target = (const gralloc_module_t *)dlsym(handle, HAL_MODULE_INFO_SYM_AS_STR);
    if (g_target == NULL) {
        ALOGE("failed to dlsym(%s) in mapper.%s.so: %s",
              HAL_MODULE_INFO_SYM_AS_STR, suffix, dlerror());
    }
}

static inline const gralloc_module_t *get_target(void) {
    pthread_once(&g_target_once, init_target);
    return g_target;
}

static int proxy_open(const struct hw_module_t *mod, const char *name, struct hw_device_t **dev) {
    (void)mod;
    const gralloc_module_t *t = get_target();
    if (t == NULL || t->common.methods == NULL || t->common.methods->open == NULL) {
        return -ENODEV;
    }
    return t->common.methods->open(&t->common, name, dev);
}

static int proxy_register_buffer(struct gralloc_module_t const *module, buffer_handle_t handle) {
    (void)module;
    const gralloc_module_t *t = get_target();
    if (t == NULL || t->registerBuffer == NULL) {
        return -ENODEV;
    }
    return t->registerBuffer(t, handle);
}

static int proxy_unregister_buffer(struct gralloc_module_t const *module, buffer_handle_t handle) {
    (void)module;
    const gralloc_module_t *t = get_target();
    if (t == NULL || t->unregisterBuffer == NULL) {
        return -ENODEV;
    }
    return t->unregisterBuffer(t, handle);
}

static int proxy_lock(struct gralloc_module_t const *module, buffer_handle_t handle, int usage,
                      int l, int top, int w, int h, void **vaddr) {
    (void)module;
    const gralloc_module_t *t = get_target();
    if (t == NULL || t->lock == NULL) {
        return -ENODEV;
    }
    return t->lock(t, handle, usage, l, top, w, h, vaddr);
}

static int proxy_unlock(struct gralloc_module_t const *module, buffer_handle_t handle) {
    (void)module;
    const gralloc_module_t *t = get_target();
    if (t == NULL || t->unlock == NULL) {
        return -ENODEV;
    }
    return t->unlock(t, handle);
}

static int proxy_perform(struct gralloc_module_t const *module, int op, ...) {
    (void)module;
    const gralloc_module_t *t = get_target();
    if (t == NULL || t->perform == NULL) {
        return -ENODEV;
    }

    va_list args;
    va_start(args, op);
    int ret = -EINVAL;

    switch (op) {
    case CDH_GRALLOC_DRM_GET_STRIDE: {
        buffer_handle_t handle = va_arg(args, buffer_handle_t);
        uint32_t *out_stride = va_arg(args, uint32_t *);
        ret = t->perform(t, op, handle, out_stride);
        break;
    }
    case CDH_GRALLOC_DRM_GET_FORMAT: {
        buffer_handle_t handle = va_arg(args, buffer_handle_t);
        int32_t *out_format = va_arg(args, int32_t *);
        ret = t->perform(t, op, handle, out_format);
        break;
    }
    case CDH_GRALLOC_DRM_GET_DIMENSIONS: {
        buffer_handle_t handle = va_arg(args, buffer_handle_t);
        uint32_t *out_width = va_arg(args, uint32_t *);
        uint32_t *out_height = va_arg(args, uint32_t *);
        ret = t->perform(t, op, handle, out_width, out_height);
        break;
    }
    case CDH_GRALLOC_DRM_GET_BACKING_STORE: {
        buffer_handle_t handle = va_arg(args, buffer_handle_t);
        uint64_t *out_store = va_arg(args, uint64_t *);
        ret = t->perform(t, op, handle, out_store);
        break;
    }
    case CDH_GRALLOC_DRM_GET_BUFFER_INFO: {
        buffer_handle_t handle = va_arg(args, buffer_handle_t);
        void *info = va_arg(args, void *);
        ret = t->perform(t, op, handle, info);
        break;
    }
    case CDH_GRALLOC_DRM_GET_USAGE: {
        uint32_t req_usage = va_arg(args, uint32_t);
        uint32_t *out_usage = va_arg(args, uint32_t *);
        ret = t->perform(t, op, req_usage, out_usage);
        break;
    }
    default:
        ret = -EINVAL;
        break;
    }

    va_end(args);
    return ret;
}

static int proxy_lock_ycbcr(struct gralloc_module_t const *module, buffer_handle_t handle,
                            int usage, int l, int top, int w, int h, struct android_ycbcr *ycbcr) {
    (void)module;
    const gralloc_module_t *t = get_target();
    if (t == NULL || t->lock_ycbcr == NULL) {
        return -ENODEV;
    }
    return t->lock_ycbcr(t, handle, usage, l, top, w, h, ycbcr);
}

static int proxy_lock_async(struct gralloc_module_t const *module, buffer_handle_t handle,
                            int usage, int l, int top, int w, int h, void **vaddr, int fence_fd) {
    (void)module;
    const gralloc_module_t *t = get_target();
    if (t == NULL || t->lockAsync == NULL) {
        return -ENODEV;
    }
    return t->lockAsync(t, handle, usage, l, top, w, h, vaddr, fence_fd);
}

static int proxy_unlock_async(struct gralloc_module_t const *module, buffer_handle_t handle,
                              int *fence_fd) {
    (void)module;
    const gralloc_module_t *t = get_target();
    if (t == NULL || t->unlockAsync == NULL) {
        return -ENODEV;
    }
    return t->unlockAsync(t, handle, fence_fd);
}

static int proxy_lock_async_ycbcr(struct gralloc_module_t const *module, buffer_handle_t handle,
                                  int usage, int l, int top, int w, int h,
                                  struct android_ycbcr *ycbcr, int fence_fd) {
    (void)module;
    const gralloc_module_t *t = get_target();
    if (t == NULL || t->lockAsync_ycbcr == NULL) {
        return -ENODEV;
    }
    return t->lockAsync_ycbcr(t, handle, usage, l, top, w, h, ycbcr, fence_fd);
}

static struct hw_module_methods_t g_proxy_methods = {
    .open = proxy_open,
};

__attribute__((visibility("default")))
gralloc_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = GRALLOC_MODULE_API_VERSION_0_3,
        .hal_api_version = 0,
        .id = GRALLOC_HARDWARE_MODULE_ID,
        .name = "CrOS Gralloc",
        .author = "Chrome OS",
        .methods = &g_proxy_methods,
    },
    .registerBuffer = proxy_register_buffer,
    .unregisterBuffer = proxy_unregister_buffer,
    .lock = proxy_lock,
    .unlock = proxy_unlock,
    .perform = proxy_perform,
    .lock_ycbcr = proxy_lock_ycbcr,
    .lockAsync = proxy_lock_async,
    .unlockAsync = proxy_unlock_async,
    .lockAsync_ycbcr = proxy_lock_async_ycbcr,
    .validateBufferSize = NULL,
    .getTransportSize = NULL,
};
