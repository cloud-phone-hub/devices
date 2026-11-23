// note13-5G-15 GL 身份覆写：接管 glGetString 的四个「身份」枚举 + 过滤扩展串，
// 其余枚举与全部其它 GL 入口一律原样转发给基座 mesa 驱动（转发表见 table_<tag>.c）。
//
// 覆写项的真机值全部逐字取自 device-checker hardware.gles 在 gold 上的实测返回
// （run-00112/00113），见 config/gpu.json 的 source-of-truth.captured-values。
//
// GL_SHADING_LANGUAGE_VERSION **故意不覆写**（曾经覆写过，是事故）：
// Skia 用它决定 shader 前缀版本。真机返回 "OpenGL ES GLSL ES 3.20"，覆写后 Skia 会
// 发 `#version 320 es`，而基座 mesa 的编译器实际只支持到 3.00（宿主 Apple GL 4.1
// 封顶）→ 全部 shader 编译失败 → App 内容区整片空白（Settings 复现，logcat 有
// "GLSL ES 3.20 is not supported. Supported versions are: 1.00 ES, and 3.00 ES"）。
// 真机能真编译 3.20，我们不能，所以这个字段必须留 mesa 真值。
// 代价：version 声称 3.2 而 GLSL 报 3.00 的内部矛盾保留——两害相权，空白界面
// 远比一个字符串矛盾严重。已记入 config/gpu.json 的 not-aligned。
//
// 为什么还需在 libEGL_meow.so 接管 eglGetProcAddress：
// 当 EGL 导出 EGL_KHR_get_all_proc_addresses 时，Chromium WebView (libmonochrome_64.so)
// 在 ui/gl/init/gl_initializer_android.cc 中优先通过 eglGetProcAddress("glGetString") 解析
// 核心 GL 入口。Android 系统侧 libEGL.so 的 eglGetProcAddressImpl 对非 egl* 符号直接转发给
// cnx->egl.eglGetProcAddress（即 libEGL_meow.so）。若不接管，Mesa 的 eglGetProcAddress 会返回
// libgpud.so 内部的原始 _mesa_GetString（"Mesa" / "virgl"），触发 Chromium 的软件渲染黑名单
// （ContextResult::kFatalFailure: WebGL1 blocklisted）。

#include <dlfcn.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef CDH_GL_STRINGS_HOST_TEST
#include "cdh_gpu_targets.inc"
#endif

#include "cdh_gl_shader_patch.h"
#include "cdh_gpu_cfg.h"

#ifdef CDH_HAS_EGLCREATECONTEXT
void *cdh_ovr_eglCreateContext __attribute__((visibility("hidden")));
typedef void *(*cdh_eglCreateContext_t)(void *, void *, void *, const int *);

#define CDH_EGL_NONE 0x3038
#define CDH_EGL_CONTEXT_MAJOR_VERSION 0x3098
#define CDH_EGL_CONTEXT_MINOR_VERSION 0x30FB
#define CDH_EGL_CONTEXT_FLAGS_KHR 0x30FC
#define CDH_EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT 0x30BF
#define CDH_EGL_CONTEXT_PRIORITY_LEVEL_IMG 0x3100
#define CDH_EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT 0x3138
#define CDH_EGL_PROTECTED_CONTENT_EXT 0x32C0

__attribute__((visibility("default")))
void *eglCreateContext(void *dpy, void *config, void *share_context,
                       const int *attrib_list) {
    if (!cdh_ovr_eglCreateContext) {
        return NULL;
    }
    cdh_eglCreateContext_t orig = (cdh_eglCreateContext_t)cdh_ovr_eglCreateContext;
    void *ctx = orig(dpy, config, share_context, attrib_list);
    if (ctx != NULL || attrib_list == NULL) {
        return ctx;
    }
    // 宿主 Apple GL 4.1 / VirGL 封顶支持 GLES 3.0：当调用方请求 GLES 3.1/3.2
    // （如 GoogleCamera fast_moments_hdr）或携带 VirGL 不支持的扩展属性导致
    // EGL_BAD_MATCH 时，降级 minor version -> 0 并剔除扩展属性后重试。
    int sanitized[64];
    int out_idx = 0;
    int major = 0;
    for (int i = 0; i < 60 && attrib_list[i] != CDH_EGL_NONE; i += 2) {
        int key = attrib_list[i];
        int val = attrib_list[i + 1];
        if (key == CDH_EGL_CONTEXT_MAJOR_VERSION) {
            major = val;
        }
        if (key == CDH_EGL_CONTEXT_PRIORITY_LEVEL_IMG ||
            key == CDH_EGL_PROTECTED_CONTENT_EXT ||
            key == CDH_EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT ||
            key == CDH_EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT ||
            key == CDH_EGL_CONTEXT_FLAGS_KHR) {
            continue;
        }
        if (key == CDH_EGL_CONTEXT_MINOR_VERSION && val > 0) {
            val = 0;
        }
        if (out_idx + 3 < 64) {
            sanitized[out_idx++] = key;
            sanitized[out_idx++] = val;
        }
    }
    sanitized[out_idx] = CDH_EGL_NONE;
    ctx = orig(dpy, config, share_context, sanitized);
    if (ctx != NULL) {
        return ctx;
    }
    if (major >= 3) {
        const int fallback_es3[] = {CDH_EGL_CONTEXT_MAJOR_VERSION, 3, CDH_EGL_NONE};
        ctx = orig(dpy, config, share_context, fallback_es3);
    }
    return ctx;
}
#endif

#ifdef CDH_HAS_EGLCHOOSECONFIG
void *cdh_ovr_eglChooseConfig __attribute__((visibility("hidden")));
typedef unsigned int (*cdh_eglChooseConfig_t)(void *, const int *, void **, int, int *);

#ifndef CDH_EGL_NONE
#define CDH_EGL_NONE 0x3038
#endif
#define CDH_EGL_ALPHA_SIZE 0x3021
#define CDH_EGL_BLUE_SIZE 0x3022
#define CDH_EGL_GREEN_SIZE 0x3023
#define CDH_EGL_RED_SIZE 0x3024
#define CDH_EGL_CONFIG_CAVEAT 0x3027
#define CDH_EGL_NATIVE_VISUAL_ID 0x302E
#define CDH_EGL_SAMPLES 0x3031
#define CDH_EGL_SAMPLE_BUFFERS 0x3032
#define CDH_EGL_SURFACE_TYPE 0x3033
#define CDH_EGL_RENDERABLE_TYPE 0x3040
#define CDH_EGL_CONFORMANT 0x3042
#define CDH_EGL_RECORDABLE_ANDROID 0x3142
#define CDH_EGL_FRAMEBUFFER_TARGET_ANDROID 0x3147
#define CDH_EGL_COLOR_COMPONENT_TYPE_EXT 0x3339
#define CDH_EGL_WINDOW_BIT 0x0004
#define CDH_EGL_PBUFFER_BIT 0x0001
#define CDH_EGL_OPENGL_ES2_BIT 0x0004

__attribute__((visibility("default")))
unsigned int eglChooseConfig(void *dpy, const int *attrib_list, void **configs,
                             int config_size, int *num_config) {
    if (!cdh_ovr_eglChooseConfig) {
        if (num_config) *num_config = 0;
        return 0;
    }
    cdh_eglChooseConfig_t orig = (cdh_eglChooseConfig_t)cdh_ovr_eglChooseConfig;
    int local_count = 0;
    int *cnt_ptr = num_config ? num_config : &local_count;
    unsigned int ok = orig(dpy, attrib_list, configs, config_size, cnt_ptr);
    if (ok && *cnt_ptr > 0) {
        return ok;
    }
    if (attrib_list != NULL) {
        // 第 1 级回退：HyperOS 3 (Xiaomi popsicle) 等相机请求 10-bit / FP16 / 广色域 /
        // EGL_RECORDABLE_ANDROID / 多重采样属性在 VirGL/Mesa 下返回 0 个 EGLConfig，
        // 触发 MIGLUtil Config chosen: R0 G0 B0 A0 -> EGL_BAD_CONFIG (12293)。
        // 这里将高色深收窄为 8-bit，剔除扩展/采样约束后重试。
        int sanitized[64];
        int out_idx = 0;
        for (int i = 0; i < 60 && attrib_list[i] != CDH_EGL_NONE; i += 2) {
            int key = attrib_list[i];
            int val = attrib_list[i + 1];
            if (key == CDH_EGL_COLOR_COMPONENT_TYPE_EXT ||
                key == CDH_EGL_RECORDABLE_ANDROID ||
                key == CDH_EGL_FRAMEBUFFER_TARGET_ANDROID ||
                key == CDH_EGL_SAMPLES ||
                key == CDH_EGL_SAMPLE_BUFFERS ||
                key == CDH_EGL_CONFORMANT ||
                key == CDH_EGL_CONFIG_CAVEAT ||
                key == CDH_EGL_NATIVE_VISUAL_ID) {
                continue;
            }
            if ((key == CDH_EGL_RED_SIZE || key == CDH_EGL_GREEN_SIZE ||
                 key == CDH_EGL_BLUE_SIZE || key == CDH_EGL_ALPHA_SIZE) &&
                val > 8) {
                val = 8;
            }
            if (key == CDH_EGL_RENDERABLE_TYPE) {
                val = CDH_EGL_OPENGL_ES2_BIT;
            }
            if (key == CDH_EGL_SURFACE_TYPE) {
                int masked = val & (CDH_EGL_WINDOW_BIT | CDH_EGL_PBUFFER_BIT);
                val = masked ? masked : CDH_EGL_WINDOW_BIT;
            }
            if (out_idx + 3 < 64) {
                sanitized[out_idx++] = key;
                sanitized[out_idx++] = val;
            }
        }
        sanitized[out_idx] = CDH_EGL_NONE;
        *cnt_ptr = 0;
        ok = orig(dpy, sanitized, configs, config_size, cnt_ptr);
        if (ok && *cnt_ptr > 0) {
            return ok;
        }
    }
    // 第 2 级兜底：标准 RGBA8888 + ES2 窗口配置
    const int fallback_rgba8[] = {
        CDH_EGL_SURFACE_TYPE, CDH_EGL_WINDOW_BIT,
        CDH_EGL_RENDERABLE_TYPE, CDH_EGL_OPENGL_ES2_BIT,
        CDH_EGL_RED_SIZE, 8,
        CDH_EGL_GREEN_SIZE, 8,
        CDH_EGL_BLUE_SIZE, 8,
        CDH_EGL_ALPHA_SIZE, 8,
        CDH_EGL_NONE,
    };
    *cnt_ptr = 0;
    return orig(dpy, fallback_rgba8, configs, config_size, cnt_ptr);
}
#endif

#ifdef CDH_HAS_EGLCREATEWINDOWSURFACE
void *cdh_ovr_eglCreateWindowSurface __attribute__((visibility("hidden")));
typedef void *(*cdh_eglCreateWindowSurface_t)(void *, void *, void *, const int *);

#ifndef CDH_EGL_NONE
#define CDH_EGL_NONE 0x3038
#endif
#define CDH_EGL_RENDER_BUFFER 0x3086
#define CDH_EGL_GL_COLORSPACE_KHR 0x309D
#define CDH_EGL_SURFACE_PROTECTED_EXT 0x32C0
#define CDH_EGL_SMPTE2086_MIN 0x3341
#define CDH_EGL_SMPTE2086_MAX 0x334A
#define CDH_EGL_CTA861_3_MAX_CONTENT 0x3360
#define CDH_EGL_CTA861_3_MAX_FRAME 0x3361

__attribute__((visibility("default")))
void *eglCreateWindowSurface(void *dpy, void *config, void *win,
                             const int *attrib_list) {
    if (!cdh_ovr_eglCreateWindowSurface) {
        return NULL;
    }
    cdh_eglCreateWindowSurface_t orig =
        (cdh_eglCreateWindowSurface_t)cdh_ovr_eglCreateWindowSurface;
    void *surf = orig(dpy, config, win, attrib_list);
    if (surf != NULL || attrib_list == NULL) {
        return surf;
    }
    // 剔除广色域/HDR 元数据/受保护内容等 Mesa VirGL 不支持属性后重试
    int sanitized[64];
    int out_idx = 0;
    for (int i = 0; i < 60 && attrib_list[i] != CDH_EGL_NONE; i += 2) {
        int key = attrib_list[i];
        int val = attrib_list[i + 1];
        if (key == CDH_EGL_GL_COLORSPACE_KHR ||
            key == CDH_EGL_SURFACE_PROTECTED_EXT ||
            key == CDH_EGL_RENDER_BUFFER ||
            (key >= CDH_EGL_SMPTE2086_MIN && key <= CDH_EGL_SMPTE2086_MAX) ||
            key == CDH_EGL_CTA861_3_MAX_CONTENT ||
            key == CDH_EGL_CTA861_3_MAX_FRAME) {
            continue;
        }
        if (out_idx + 3 < 64) {
            sanitized[out_idx++] = key;
            sanitized[out_idx++] = val;
        }
    }
    sanitized[out_idx] = CDH_EGL_NONE;
    surf = orig(dpy, config, win, out_idx > 0 ? sanitized : NULL);
    if (surf != NULL) {
        return surf;
    }
    return orig(dpy, config, win, NULL);
}
#endif

#ifdef CDH_HAS_EGLGETPROCADDRESS
void *cdh_ovr_eglGetProcAddress __attribute__((visibility("hidden")));
typedef void *(*cdh_eglGetProcAddress_t)(const char *);

static void *cdh_gles2_ovr_handle;

static void *cdh_lookup_gles2_ovr(const char *name) {
    if (!cdh_gles2_ovr_handle) {
        cdh_gles2_ovr_handle = dlopen("/vendor/lib64/egl/libGLESv2_meow.so", RTLD_NOW | RTLD_LOCAL);
    }
    if (cdh_gles2_ovr_handle) {
        return dlsym(cdh_gles2_ovr_handle, name);
    }
    return NULL;
}

__attribute__((visibility("default")))
void *eglGetProcAddress(const char *procname) {
    if (procname) {
        if (strcmp(procname, "glGetString") == 0 ||
            strcmp(procname, "glGetStringi") == 0 ||
            strcmp(procname, "glGetIntegerv") == 0 ||
            strcmp(procname, "glShaderSource") == 0 ||
            strcmp(procname, "glFramebufferTexture2D") == 0) {
            void *fn = cdh_lookup_gles2_ovr(procname);
            if (fn) {
                return fn;
            }
        }
#ifdef CDH_HAS_EGLCREATECONTEXT
        if (strcmp(procname, "eglCreateContext") == 0) {
            return (void *)&eglCreateContext;
        }
#endif
#ifdef CDH_HAS_EGLCHOOSECONFIG
        if (strcmp(procname, "eglChooseConfig") == 0) {
            return (void *)&eglChooseConfig;
        }
#endif
#ifdef CDH_HAS_EGLCREATEWINDOWSURFACE
        if (strcmp(procname, "eglCreateWindowSurface") == 0) {
            return (void *)&eglCreateWindowSurface;
        }
#endif
        if (strcmp(procname, "eglGetProcAddress") == 0) {
            return (void *)&eglGetProcAddress;
        }
    }
    if (cdh_ovr_eglGetProcAddress) {
        return ((cdh_eglGetProcAddress_t)cdh_ovr_eglGetProcAddress)(procname);
    }
    return NULL;
}
#endif

#ifdef CDH_HAS_GLGETSTRING
// mesa 的真实现，由 table_<tag>.c 的构造函数 dlsym 后写入
void *cdh_ovr_glGetString __attribute__((visibility("hidden")));
void *cdh_ovr_glGetStringi __attribute__((visibility("hidden")));
void *cdh_ovr_glGetIntegerv __attribute__((visibility("hidden")));
#ifdef CDH_HAS_GLSHADERSOURCE
void *cdh_ovr_glShaderSource __attribute__((visibility("hidden")));
#endif
#ifdef CDH_HAS_GLFRAMEBUFFERTEXTURE2D
void *cdh_ovr_glFramebufferTexture2D __attribute__((visibility("hidden")));
#endif

#define CDH_GL_VENDOR 0x1F00
#define CDH_GL_RENDERER 0x1F01
#define CDH_GL_VERSION 0x1F02
#define CDH_GL_EXTENSIONS 0x1F03
#define CDH_GL_NUM_EXTENSIONS 0x821D
#define CDH_GL_SHADING_LANGUAGE_VERSION 0x8B8C

typedef const unsigned char *(*cdh_glGetString_t)(unsigned int);
typedef const unsigned char *(*cdh_glGetStringi_t)(unsigned int, unsigned int);
typedef void (*cdh_glGetIntegerv_t)(unsigned int, int *);
#ifdef CDH_HAS_GLSHADERSOURCE
typedef void (*cdh_glShaderSource_t)(unsigned int, int, const char *const *, const int *);
#endif

// ---------------------------------------------------------------------------
// 扩展串过滤：只删不加
//
// 为什么必须删：基座 mesa/virgl 的扩展串里带着直指宿主与驱动的条目——
//   GL_APPLE_sync / GL_APPLE_texture_max_level   → 宿主是 macOS
//   GL_MESA_*                                    → 驱动是 Mesa
//   GL_ANGLE_*                                   → 走了 ANGLE
// 这些在真机 Mali 上都不存在，App 一读就能定位到虚拟化 + 宿主操作系统。
//
// 为什么不能加：真机独有的 57 项里含 GL_EXT_geometry_shader /
// GL_EXT_tessellation_shader / GL_ANDROID_extension_pack_es31a 等，云机 virgl
// 实际不具备（宿主 Apple GL 4.1 封顶，AEP 14 项只命中 2 项）。谎报后 App 会
// 真去调这些入口 → 编译/绘制失败甚至崩溃。少报扩展最多让 App 走保守分支，
// 多报则直接崩，所以只做减法。
//
// 减法本身是安全的：被删的都是 Apple/Mesa/ANGLE 私有或桌面 GL 遗留扩展，
// Android App 不会依赖它们做功能判定。
// ---------------------------------------------------------------------------

#define CDH_EXT_MAX 512
#define CDH_EXT_BUF 16384

static char cdh_ext_joined[CDH_EXT_BUF];
static char cdh_ext_items_buf[CDH_EXT_BUF];
static const char *cdh_ext_list[CDH_EXT_MAX];
static unsigned int cdh_ext_count;
static int cdh_ext_ready;

// 前缀名单来自 config/gpu.json，构建期生成成逗号分隔串
static const char cdh_ext_strip[] = CDH_ID_GL_EXT_STRIP;

static int cdh_stripped(const char *name, size_t len) {
    const char *p = cdh_ext_strip;
    while (*p) {
        const char *comma = strchr(p, ',');
        size_t n = comma ? (size_t)(comma - p) : strlen(p);
        // '=' 前缀表示整名精确匹配，避免误伤同前缀的真机原生扩展（如 GL_EXT_draw_buffers_indexed）。
        if (n > 1 && p[0] == '=') {
            if (len == n - 1 && memcmp(name, p + 1, n - 1) == 0) {
                return 1;
            }
        } else if (n > 0 && n <= len && memcmp(name, p, n) == 0) {
            return 1;
        }
        if (!comma) {
            break;
        }
        p = comma + 1;
    }
    return 0;
}

static char cdh_gl_vendor_buf[PROP_VALUE_MAX];
static char cdh_gl_renderer_buf[PROP_VALUE_MAX];
static char cdh_gl_version_buf[PROP_VALUE_MAX];
static char cdh_gl_sl_version_buf[PROP_VALUE_MAX];
static int cdh_gl_id_ready;

static void cdh_init_gl_id(void) {
    if (cdh_gl_id_ready) {
        return;
    }
    if (cdh_cfg_get("ro.vendor.cdh.gpu.gl_vendor", cdh_gl_vendor_buf, sizeof(cdh_gl_vendor_buf)) <= 0) {
        strncpy(cdh_gl_vendor_buf, CDH_ID_GL_VENDOR, sizeof(cdh_gl_vendor_buf) - 1);
    }
    if (cdh_cfg_get("ro.vendor.cdh.gpu.gl_renderer", cdh_gl_renderer_buf, sizeof(cdh_gl_renderer_buf)) <= 0) {
        strncpy(cdh_gl_renderer_buf, CDH_ID_GL_RENDERER, sizeof(cdh_gl_renderer_buf) - 1);
    }
    if (cdh_cfg_get("ro.vendor.cdh.gpu.gl_version", cdh_gl_version_buf, sizeof(cdh_gl_version_buf)) <= 0) {
        strncpy(cdh_gl_version_buf, CDH_ID_GL_VERSION, sizeof(cdh_gl_version_buf) - 1);
    }
    if (cdh_cfg_get("ro.vendor.cdh.gpu.gl_sl_version", cdh_gl_sl_version_buf, sizeof(cdh_gl_sl_version_buf)) <= 0) {
#ifdef CDH_ID_GL_SL_VERSION
        strncpy(cdh_gl_sl_version_buf, CDH_ID_GL_SL_VERSION, sizeof(cdh_gl_sl_version_buf) - 1);
#else
        cdh_gl_sl_version_buf[0] = '\0';
#endif
    }
    cdh_gl_id_ready = 1;
}

static void cdh_append_ext_token(const char *ext, size_t len, size_t *used,
                                 size_t *items_used, unsigned int *count) {
    if (len == 0 || *count >= CDH_EXT_MAX || *used + len + 2 >= CDH_EXT_BUF ||
        *items_used + len + 1 >= CDH_EXT_BUF) {
        return;
    }
    for (unsigned int j = 0; j < *count; j++) {
        if (strlen(cdh_ext_list[j]) == len && memcmp(cdh_ext_list[j], ext, len) == 0) {
            return;
        }
    }
    if (*used) {
        cdh_ext_joined[(*used)++] = ' ';
    }
    memcpy(cdh_ext_joined + *used, ext, len);
    *used += len;
    cdh_ext_joined[*used] = '\0';

    char *item_ptr = cdh_ext_items_buf + *items_used;
    memcpy(item_ptr, ext, len);
    item_ptr[len] = '\0';
    *items_used += len + 1;
    cdh_ext_list[(*count)++] = item_ptr;
}

// 过滤宿主 Apple GL 4.1 / VirGL 不具备完整硬件实现、且会诱发 Skia/HWUI/App
// 走错误 FBO 或着色器路径导致 0x8d56 黑屏或指令流中止的危险扩展（不改动 Mesa src 原生扩展）。
static int cdh_is_virgl_unsafe_extra_ext(const char *ext, size_t len) {
    static const char *const kUnsafeExts[] = {
        "GL_ANDROID_extension_pack_es31a",
        "GL_EXT_geometry_shader",
        "GL_OES_geometry_shader",
        "GL_EXT_tessellation_shader",
        "GL_OES_tessellation_shader",
        "GL_EXT_texture_cube_map_array",
        "GL_OES_texture_cube_map_array",
        "GL_EXT_disjoint_timer_query",
        "GL_EXT_multisampled_render_to_texture",
        "GL_EXT_multisampled_render_to_texture2",
        "GL_IMG_multisampled_render_to_texture",
        "GL_OVR_multiview",
        "GL_OVR_multiview2",
        "GL_OVR_multiview_multisampled_render_to_texture",
        "GL_EXT_shader_framebuffer_fetch",
        "GL_EXT_shader_framebuffer_fetch_non_coherent",
        "GL_ARM_shader_framebuffer_fetch",
        "GL_ARM_shader_framebuffer_fetch_depth_stencil",
        "GL_EXT_shader_pixel_local_storage",
        "GL_EXT_shader_pixel_local_storage2",
        "GL_KHR_blend_equation_advanced",
        "GL_KHR_blend_equation_advanced_coherent",
        "GL_EXT_buffer_storage",
        "GL_EXT_external_buffer",
        "GL_EXT_EGL_image_storage",
        "GL_EXT_protected_textures",
        "GL_EXT_YUV_target",
        "GL_EXT_clear_texture",
        "GL_EXT_clip_control",
        "GL_EXT_polygon_offset_clamp",
        "GL_EXT_sRGB",
        "GL_EXT_robustness",
        "GL_KHR_robustness",
        "GL_KHR_robust_buffer_access_behavior",
        "GL_OES_sample_shading",
        "GL_OES_gpu_shader5",
        "GL_EXT_gpu_shader5",
        "GL_OES_texture_view",
        "GL_EXT_texture_view",
        "GL_OES_texture_storage_multisample_2d_array",
        "GL_KHR_shader_subgroup",
        "GL_KHR_texture_compression_astc_hdr",
        "GL_KHR_texture_compression_astc_sliced_3d",
        "GL_OES_texture_compression_astc",
        "GL_QCOM_shader_framebuffer_fetch_noncoherent",
        "GL_QCOM_shader_framebuffer_fetch_rate",
    };
    const size_t n = sizeof(kUnsafeExts) / sizeof(kUnsafeExts[0]);
    for (size_t i = 0; i < n; i++) {
        if (strlen(kUnsafeExts[i]) == len && memcmp(ext, kUnsafeExts[i], len) == 0) {
            return 1;
        }
    }
    return 0;
}

static void cdh_append_filtered_extra_exts(const char *csv, size_t *used,
                                           size_t *items_used, unsigned int *count) {
    if (!csv) {
        return;
    }
    const char *ep = csv;
    while (*ep) {
        while (*ep == ' ' || *ep == ',') {
            ep++;
        }
        if (!*ep) {
            break;
        }
        const char *estart = ep;
        while (*ep && *ep != ' ' && *ep != ',') {
            ep++;
        }
        size_t elen = (size_t)(ep - estart);
        if (cdh_is_virgl_unsafe_extra_ext(estart, elen)) {
            continue;
        }
        cdh_append_ext_token(estart, elen, used, items_used, count);
    }
}

// 惰性构建过滤后的扩展表；只在首次查询时做一次，之后走缓存。
// 需要 cdh_ext_lock 之类的同步吗？—— glGetString 可能在多线程被调，但构建过程
// 是幂等的（同样的输入得同样的结果），最坏情况是重复构建一次，不会读到半成品：
// cdh_ext_ready 在所有字段填完后才置 1。
static void cdh_build_ext(void) {
    if (cdh_ext_ready) {
        return;
    }
    cdh_init_gl_id();
    const char *src = NULL;
    if (cdh_ovr_glGetString) {
        src = (const char *)((cdh_glGetString_t)cdh_ovr_glGetString)(CDH_GL_EXTENSIONS);
    }
    if (!src) {
        cdh_ext_ready = 1;
        return;
    }
    size_t used = 0;
    size_t items_used = 0;
    unsigned int count = 0;
    const char *p = src;
    while (*p && count < CDH_EXT_MAX && used + 1 < CDH_EXT_BUF) {
        while (*p == ' ') {
            p++;
        }
        if (!*p) {
            break;
        }
        const char *start = p;
        while (*p && *p != ' ') {
            p++;
        }
        size_t len = (size_t)(p - start);
        if (cdh_stripped(start, len)) {
            continue;
        }
        cdh_append_ext_token(start, len, &used, &items_used, &count);
    }
    char extra_prop[512] = {0};
    const char *extra_src = NULL;
    if (cdh_cfg_get("ro.vendor.cdh.gpu.gl_extra_exts", extra_prop, sizeof(extra_prop)) > 0 &&
        extra_prop[0] != '\0') {
        if (strcmp(extra_prop, "none") != 0) {
            extra_src = extra_prop;
        }
    } else if (strcmp(cdh_gl_vendor_buf, "ARM") == 0) {
        extra_src = "GL_ARM_mali_program_binary,GL_ARM_mali_shader_binary,GL_ARM_rgba8";
    }
    cdh_append_filtered_extra_exts(extra_src, &used, &items_used, &count);
    char extra_prop2[512] = {0};
    if (cdh_cfg_get("ro.vendor.cdh.gpu.gl_extra_exts2", extra_prop2, sizeof(extra_prop2)) > 0 &&
        extra_prop2[0] != '\0' && strcmp(extra_prop2, "none") != 0) {
        cdh_append_filtered_extra_exts(extra_prop2, &used, &items_used, &count);
    }
    cdh_ext_joined[used] = '\0';
    cdh_ext_count = count;
    cdh_ext_ready = 1;
}

__attribute__((visibility("default")))
const unsigned char *glGetString(unsigned int name) {
    cdh_init_gl_id();
    switch (name) {
    case CDH_GL_VENDOR:
        return (const unsigned char *)cdh_gl_vendor_buf;
    case CDH_GL_RENDERER:
        return (const unsigned char *)cdh_gl_renderer_buf;
    case CDH_GL_VERSION:
        return (const unsigned char *)cdh_gl_version_buf;
    case CDH_GL_SHADING_LANGUAGE_VERSION:
        if (cdh_gl_sl_version_buf[0] != '\0' && strcmp(cdh_gl_sl_version_buf, "passthrough") != 0) {
            return (const unsigned char *)cdh_gl_sl_version_buf;
        }
        break;
    case CDH_GL_EXTENSIONS:
        cdh_build_ext();
        if (cdh_ext_count) {
            return (const unsigned char *)cdh_ext_joined;
        }
        break;   // 没拿到 mesa 的串就退回转发，别返回空串
    default:
        break;
    }
    if (cdh_ovr_glGetString) {
        return ((cdh_glGetString_t)cdh_ovr_glGetString)(name);
    }
    return NULL;
}

// 过滤扩展串后，GL_NUM_EXTENSIONS 必须同步返回过滤后的条数，否则 Skia 按 mesa 的
// 原始条数（101）去 glGetStringi 逐条取，索引 92..100 会拿到 NULL。
// 同时支持按机型属性覆写 GLES 硬件能力上限（如 Mali-G57 MC2 的 maxTextureSize=16383、
// maxTextureImageUnits=16、maxCombinedTextureImageUnits=96、maxVertexAttribs=32）。
#define CDH_GL_MAX_TEXTURE_SIZE               0x0D33u
#define CDH_GL_MAX_RENDERBUFFER_SIZE          0x84E8u
#define CDH_GL_MAX_CUBE_MAP_TEXTURE_SIZE      0x851Cu
#define CDH_GL_MAX_VERTEX_ATTRIBS             0x8869u
#define CDH_GL_MAX_TEXTURE_IMAGE_UNITS        0x8872u
#define CDH_GL_MAX_COMBINED_TEX_IMAGE_UNITS   0x8B4Du

static int cdh_limits_ready;
static int cdh_lim_max_tex_size;
static int cdh_lim_max_rb_size;
static int cdh_lim_max_cube_size;
static int cdh_lim_max_vert_attribs;
static int cdh_lim_max_tex_units;
static int cdh_lim_max_comb_tex_units;

static int cdh_prop_pos_int(const char *key) {
    char buf[PROP_VALUE_MAX] = {0};
    if (cdh_cfg_get(key, buf, sizeof(buf)) <= 0 || buf[0] == '\0') {
        return 0;
    }
    char *end = NULL;
    long v = strtol(buf, &end, 10);
    if (end == buf || *end != '\0' || v <= 0 || v > 0x7fffffffL) {
        return 0;
    }
    return (int)v;
}

static void cdh_init_gl_limits(void) {
    if (cdh_limits_ready) {
        return;
    }
    cdh_lim_max_tex_size = cdh_prop_pos_int("ro.vendor.cdh.gpu.max_texture_size");
    cdh_lim_max_rb_size = cdh_prop_pos_int("ro.vendor.cdh.gpu.max_renderbuffer_size");
    cdh_lim_max_cube_size = cdh_prop_pos_int("ro.vendor.cdh.gpu.max_cube_map_texture_size");
    cdh_lim_max_vert_attribs = cdh_prop_pos_int("ro.vendor.cdh.gpu.max_vertex_attribs");
    cdh_lim_max_tex_units = cdh_prop_pos_int("ro.vendor.cdh.gpu.max_texture_image_units");
    cdh_lim_max_comb_tex_units =
        cdh_prop_pos_int("ro.vendor.cdh.gpu.max_combined_texture_image_units");
    cdh_limits_ready = 1;
}

__attribute__((visibility("default")))
void glGetIntegerv(unsigned int pname, int *params) {
    if (pname == CDH_GL_NUM_EXTENSIONS) {
        cdh_build_ext();
        if (params) {
            *params = (int)cdh_ext_count;
        }
        return;
    }
    if (cdh_ovr_glGetIntegerv) {
        ((cdh_glGetIntegerv_t)cdh_ovr_glGetIntegerv)(pname, params);
    }
    if (!params) {
        return;
    }
    cdh_init_gl_limits();
    switch (pname) {
    case CDH_GL_MAX_TEXTURE_SIZE:
        if (cdh_lim_max_tex_size > 0) {
            *params = cdh_lim_max_tex_size;
        }
        break;
    case CDH_GL_MAX_RENDERBUFFER_SIZE:
        if (cdh_lim_max_rb_size > 0) {
            *params = cdh_lim_max_rb_size;
        }
        break;
    case CDH_GL_MAX_CUBE_MAP_TEXTURE_SIZE:
        if (cdh_lim_max_cube_size > 0) {
            *params = cdh_lim_max_cube_size;
        }
        break;
    case CDH_GL_MAX_VERTEX_ATTRIBS:
        if (cdh_lim_max_vert_attribs > 0) {
            *params = cdh_lim_max_vert_attribs;
        }
        break;
    case CDH_GL_MAX_TEXTURE_IMAGE_UNITS:
        if (cdh_lim_max_tex_units > 0) {
            *params = cdh_lim_max_tex_units;
        }
        break;
    case CDH_GL_MAX_COMBINED_TEX_IMAGE_UNITS:
        if (cdh_lim_max_comb_tex_units > 0) {
            *params = cdh_lim_max_comb_tex_units;
        }
        break;
    default:
        break;
    }
}

// ES 3.x 上下文用 glGetStringi(GL_EXTENSIONS, i) 逐条取；必须与 glGetString
// 返回同一套过滤结果，否则两条路径自相矛盾。
// 该入口只存在于 GLESv2/v3 —— GLES1_CM 没有它，mesa 的 libGLESv1_CM 也不导出，
// 所以由 build.py 按各库的 overrides 决定是否定义 CDH_HAS_GLGETSTRINGI，
// 避免从 ES1.1 库里多导出一个不该有的符号（破坏与基座的符号一一对应）。
#ifdef CDH_HAS_GLGETSTRINGI
__attribute__((visibility("default")))
const unsigned char *glGetStringi(unsigned int name, unsigned int index) {
    if (name == CDH_GL_EXTENSIONS) {
        cdh_build_ext();
        if (index < cdh_ext_count) {
            return (const unsigned char *)cdh_ext_list[index];
        }
        return NULL;
    }
    if (cdh_ovr_glGetStringi) {
        return ((cdh_glGetStringi_t)cdh_ovr_glGetStringi)(name, index);
    }
    return NULL;
}
#endif

#ifdef CDH_HAS_GLSHADERSOURCE
// 将着色器源码中的 "#version 320 es" 或 "#version 310 es" 降级为 "#version 300 es"，
// 同时注入 fma(a, b, c) 预处理兼容宏并同步更新显式 length[] 数组（防止增广后源码被截断），
// 使 Skia/HWUI 生成的 RRect 等 GLSL ES 3.10+/3.20 着色器可被 Mesa GLSL ES 3.00 正常编译。
__attribute__((visibility("default")))
void glShaderSource(unsigned int shader, int count, const char *const *string, const int *length) {
    if (!cdh_ovr_glShaderSource) {
        return;
    }
    cdh_glShaderSource_t real_fn = (cdh_glShaderSource_t)cdh_ovr_glShaderSource;
    cdh_patched_sources_t patched;
    cdh_prepare_patched_sources(count, string, length, &patched);
    if (!patched.patched) {
        real_fn(shader, count, string, length);
        return;
    }
    real_fn(shader, count, patched.ptrs, length ? patched.lens : NULL);
    cdh_free_patched_sources(&patched);
}
#endif

#ifdef CDH_HAS_GLFRAMEBUFFERTEXTURE2D
// 部分高通/努比亚相机（如 RedMagic NX809J 的 HardwareBufferRenderer）在离屏辅助通道
// 依赖 GL_EXT_YUV_target，将 GL_TEXTURE_EXTERNAL_OES (0x8D65) 纹理直接挂到 FBO；
// Mesa VirGL 不支持外部纹理作为 FBO 颜色附件，原样调用会留下 GL_INVALID_ENUM 并导致
// glCheckFramebufferStatus 返回 GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT (0x8CD7) 崩溃。
// 当检测到底层拒绝 GL_TEXTURE_EXTERNAL_OES 挂载时，清掉该错误并改挂线程本地兜底
// GL_TEXTURE_2D (RGBA8) 纹理，确保离屏辅助 FBO 完整、主屏幕 FBO 0 预览零崩溃正常出画。
typedef void (*cdh_glFramebufferTexture2D_t)(unsigned int, unsigned int, unsigned int, unsigned int, int);
typedef unsigned int (*cdh_glGetError_t)(void);
typedef unsigned int (*cdh_glCheckFramebufferStatus_t)(unsigned int);
typedef void (*cdh_glGenTextures_t)(int, unsigned int *);
typedef unsigned char (*cdh_glIsTexture_t)(unsigned int);
typedef void (*cdh_glBindTexture_t)(unsigned int, unsigned int);
typedef void (*cdh_glTexImage2D_t)(unsigned int, int, int, int, int, int, unsigned int, unsigned int, const void *);
typedef void (*cdh_glTexParameteri_t)(unsigned int, unsigned int, int);

#define CDH_GL_TEXTURE_2D 0x0DE1u
#define CDH_GL_TEXTURE_EXTERNAL_OES 0x8D65u
#define CDH_GL_TEXTURE_BINDING_2D 0x8069u
#define CDH_GL_RGBA 0x1908u
#define CDH_GL_UNSIGNED_BYTE 0x1401u
#define CDH_GL_TEXTURE_MIN_FILTER 0x2801u
#define CDH_GL_TEXTURE_MAG_FILTER 0x2800u
#define CDH_GL_LINEAR 0x2601
#define CDH_GL_FRAMEBUFFER_COMPLETE 0x8CD5u

static void *cdh_gles2_base_handle;
static cdh_glGetError_t cdh_base_glGetError;
static cdh_glCheckFramebufferStatus_t cdh_base_glCheckFramebufferStatus;
static cdh_glGenTextures_t cdh_base_glGenTextures;
static cdh_glIsTexture_t cdh_base_glIsTexture;
static cdh_glBindTexture_t cdh_base_glBindTexture;
static cdh_glTexImage2D_t cdh_base_glTexImage2D;
static cdh_glTexParameteri_t cdh_base_glTexParameteri;
static int cdh_gles2_fb_helpers_ready;

static void cdh_init_fb_helpers(void) {
    if (cdh_gles2_fb_helpers_ready) {
        return;
    }
    cdh_gles2_fb_helpers_ready = 1;
#ifdef CDH_BASE_PATH
    cdh_gles2_base_handle = dlopen(CDH_BASE_PATH, RTLD_NOLOAD | RTLD_LOCAL);
    if (!cdh_gles2_base_handle) {
        cdh_gles2_base_handle = dlopen(CDH_BASE_PATH, RTLD_NOW | RTLD_LOCAL);
    }
#endif
    if (!cdh_gles2_base_handle) {
        return;
    }
    cdh_base_glGetError = (cdh_glGetError_t)dlsym(cdh_gles2_base_handle, "glGetError");
    cdh_base_glCheckFramebufferStatus =
        (cdh_glCheckFramebufferStatus_t)dlsym(cdh_gles2_base_handle, "glCheckFramebufferStatus");
    cdh_base_glGenTextures = (cdh_glGenTextures_t)dlsym(cdh_gles2_base_handle, "glGenTextures");
    cdh_base_glIsTexture = (cdh_glIsTexture_t)dlsym(cdh_gles2_base_handle, "glIsTexture");
    cdh_base_glBindTexture = (cdh_glBindTexture_t)dlsym(cdh_gles2_base_handle, "glBindTexture");
    cdh_base_glTexImage2D = (cdh_glTexImage2D_t)dlsym(cdh_gles2_base_handle, "glTexImage2D");
    cdh_base_glTexParameteri = (cdh_glTexParameteri_t)dlsym(cdh_gles2_base_handle, "glTexParameteri");
}

static __thread unsigned int cdh_ext_fbo_fallback_tex;

__attribute__((visibility("default")))
void glFramebufferTexture2D(unsigned int target, unsigned int attachment,
                            unsigned int textarget, unsigned int texture, int level) {
    if (!cdh_ovr_glFramebufferTexture2D) {
        return;
    }
    cdh_glFramebufferTexture2D_t real_fn =
        (cdh_glFramebufferTexture2D_t)cdh_ovr_glFramebufferTexture2D;
    real_fn(target, attachment, textarget, texture, level);
    if (textarget != CDH_GL_TEXTURE_EXTERNAL_OES || texture == 0) {
        return;
    }
    cdh_init_fb_helpers();
    if (!cdh_base_glGetError || !cdh_base_glCheckFramebufferStatus ||
        !cdh_base_glGenTextures || !cdh_base_glIsTexture ||
        !cdh_base_glBindTexture || !cdh_base_glTexImage2D ||
        !cdh_base_glTexParameteri || !cdh_ovr_glGetIntegerv) {
        return;
    }
    unsigned int err = cdh_base_glGetError();
    unsigned int status = cdh_base_glCheckFramebufferStatus(target);
    if (err == 0 && status == CDH_GL_FRAMEBUFFER_COMPLETE) {
        return;
    }
    while (cdh_base_glGetError() != 0) {}

    int prev_tex2d = 0;
    ((cdh_glGetIntegerv_t)cdh_ovr_glGetIntegerv)(CDH_GL_TEXTURE_BINDING_2D, &prev_tex2d);
    if (cdh_ext_fbo_fallback_tex == 0 || !cdh_base_glIsTexture(cdh_ext_fbo_fallback_tex)) {
        cdh_ext_fbo_fallback_tex = 0;
        cdh_base_glGenTextures(1, &cdh_ext_fbo_fallback_tex);
        if (cdh_ext_fbo_fallback_tex != 0) {
            cdh_base_glBindTexture(CDH_GL_TEXTURE_2D, cdh_ext_fbo_fallback_tex);
            cdh_base_glTexParameteri(CDH_GL_TEXTURE_2D, CDH_GL_TEXTURE_MIN_FILTER, CDH_GL_LINEAR);
            cdh_base_glTexParameteri(CDH_GL_TEXTURE_2D, CDH_GL_TEXTURE_MAG_FILTER, CDH_GL_LINEAR);
            cdh_base_glTexImage2D(CDH_GL_TEXTURE_2D, 0, (int)CDH_GL_RGBA, 64, 64, 0,
                                  CDH_GL_RGBA, CDH_GL_UNSIGNED_BYTE, NULL);
            cdh_base_glBindTexture(CDH_GL_TEXTURE_2D, (unsigned int)prev_tex2d);
        }
    }
    if (cdh_ext_fbo_fallback_tex != 0) {
        real_fn(target, attachment, CDH_GL_TEXTURE_2D, cdh_ext_fbo_fallback_tex, 0);
        while (cdh_base_glGetError() != 0) {}
    }
}
#endif
#endif  // CDH_HAS_GLGETSTRING
