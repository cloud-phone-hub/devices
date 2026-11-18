// CDH GPU 身份与参数配置读取器（GL/EGL 与 Vulkan interposer 共用，消除重复解析代码）。
//
// 读取优先级：
//   1. /vendor/lib64/egl/gpu_id.cfg（由 09_build_vendor 从清单 ro.vendor.cdh.{gpu,vulkan}.* 生成，
//      绕开普通 App SELinux 对私有 vendor 属性的读取限制）；
//   2. Android 系统属性（__system_property_get 兜底）。
#ifndef CDH_GPU_CFG_H
#define CDH_GPU_CFG_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef CDH_GL_STRINGS_HOST_TEST
#ifndef PROP_VALUE_MAX
#define PROP_VALUE_MAX 92
#endif
static inline int cdh_sys_prop_get(const char *name, char *value) {
    (void)name;
    if (value) {
        value[0] = '\0';
    }
    return 0;
}
#else
#include <sys/system_properties.h>
static inline int cdh_sys_prop_get(const char *name, char *value) {
    return __system_property_get(name, value);
}
#endif

#ifndef CDH_GPU_CFG_PATH
#if defined(__LP64__)
#define CDH_GPU_CFG_PATH "/vendor/lib64/egl/gpu_id.cfg"
#else
#define CDH_GPU_CFG_PATH "/vendor/lib/egl/gpu_id.cfg"
#endif
#endif

#define CDH_CFG_MAX_ENTRIES 64
#define CDH_CFG_BUF_SIZE 4096

typedef struct {
    char raw[CDH_CFG_BUF_SIZE];
    const char *keys[CDH_CFG_MAX_ENTRIES];
    const char *vals[CDH_CFG_MAX_ENTRIES];
    size_t count;
    int loaded;
} cdh_gpu_cfg_store_t;

#ifdef CDH_GL_STRINGS_HOST_TEST
static cdh_gpu_cfg_store_t g_cdh_gpu_cfg;
#else
__attribute__((weak, visibility("hidden"))) cdh_gpu_cfg_store_t g_cdh_gpu_cfg;
#endif

static inline void cdh_load_cfg_once(void) {
    if (g_cdh_gpu_cfg.loaded) {
        return;
    }
#ifdef CDH_GL_STRINGS_HOST_TEST
    FILE *fp = fopen(CDH_GPU_CFG_PATH, "r");
#else
    FILE *fp = fopen(CDH_GPU_CFG_PATH, "re");
#endif
    if (!fp) {
        g_cdh_gpu_cfg.loaded = 1;
        return;
    }
    size_t n = fread(g_cdh_gpu_cfg.raw, 1, sizeof(g_cdh_gpu_cfg.raw) - 1, fp);
    fclose(fp);
    g_cdh_gpu_cfg.raw[n] = '\0';

    size_t cnt = 0;
    char *cur = g_cdh_gpu_cfg.raw;
    while (*cur && cnt < CDH_CFG_MAX_ENTRIES) {
        char *nl = strchr(cur, '\n');
        if (nl) {
            *nl = '\0';
        }
        while (*cur == ' ' || *cur == '\t') {
            cur++;
        }
        if (*cur != '\0' && *cur != '#') {
            char *eq = strchr(cur, '=');
            if (eq) {
                *eq = '\0';
                char *val = eq + 1;
                size_t vlen = strlen(val);
                while (vlen > 0 && (val[vlen - 1] == '\r' || val[vlen - 1] == ' ' || val[vlen - 1] == '\t')) {
                    val[--vlen] = '\0';
                }
                g_cdh_gpu_cfg.keys[cnt] = cur;
                g_cdh_gpu_cfg.vals[cnt] = val;
                cnt++;
            }
        }
        if (!nl) {
            break;
        }
        cur = nl + 1;
    }
    g_cdh_gpu_cfg.count = cnt;
    g_cdh_gpu_cfg.loaded = 1;
}

static inline int cdh_cfg_get(const char *key, char *out, size_t cap) {
    if (!key || !out || cap == 0) {
        return 0;
    }
    cdh_load_cfg_once();
    for (size_t i = 0; i < g_cdh_gpu_cfg.count; i++) {
        if (strcmp(g_cdh_gpu_cfg.keys[i], key) == 0) {
            if (g_cdh_gpu_cfg.vals[i][0] == '\0') {
                out[0] = '\0';
                return 0;
            }
            strncpy(out, g_cdh_gpu_cfg.vals[i], cap - 1);
            out[cap - 1] = '\0';
            return (int)strlen(out);
        }
    }
    char prop_buf[PROP_VALUE_MAX] = {0};
    int len = cdh_sys_prop_get(key, prop_buf);
    if (len > 0 && prop_buf[0] != '\0') {
        strncpy(out, prop_buf, cap - 1);
        out[cap - 1] = '\0';
        return (int)strlen(out);
    }
    out[0] = '\0';
    return 0;
}

static inline int cdh_prop_u32(const char *key, uint32_t *out) {
    char buf[PROP_VALUE_MAX] = {0};
    if (cdh_cfg_get(key, buf, sizeof(buf)) <= 0 || buf[0] == '\0') {
        return 0;
    }
    char *end = NULL;
    unsigned long v = strtoul(buf, &end, 0);
    if (end == buf || *end != '\0') {
        return 0;
    }
    *out = (uint32_t)v;
    return 1;
}

#endif  // CDH_GPU_CFG_H
