// GLSL ES 3.10/3.20 -> 3.00 源码降级与 fma() 兼容宏补丁（纯 C，不依赖 Android 运行时，便于宿主单测）。
#ifndef CDH_GL_SHADER_PATCH_H
#define CDH_GL_SHADER_PATCH_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define CDH_GLSL_VER_LEN 15

static const char CDH_FMA_POLYFILL[] =
    "#ifndef fma\n"
    "#define fma(a, b, c) ((a) * (b) + (c))\n"
    "#endif\n";

static const char *cdh_find_glsl_310_or_320_es(const char *s, size_t len) {
    if (!s || len < CDH_GLSL_VER_LEN) {
        return NULL;
    }
    const char *m320 = (const char *)memmem(s, len, "#version 320 es", CDH_GLSL_VER_LEN);
    const char *m310 = (const char *)memmem(s, len, "#version 310 es", CDH_GLSL_VER_LEN);
    if (m320 && (!m310 || m320 < m310)) {
        return m320;
    }
    return m310;
}

// 对单个着色器分片执行版本降级并在首条被降级的 #version 行后注入 fma() 预处理宏。
// 未命中 #version 310/320 es 时返回 NULL；命中时返回新分配的以 '\0' 结尾的缓冲区，
// 并通过 *out_len 返回新长度（不含末尾 '\0'）。调用方负责 free() 返回值。
static inline char *cdh_patch_glsl_es_chunk(const char *src, size_t slen, size_t *out_len) {
    const char *first_hit = cdh_find_glsl_310_or_320_es(src, slen);
    if (!first_hit) {
        return NULL;
    }

    // 定位首个 #version 310/320 es 所在行结束位置（紧随 '\n' 之后插入宏定义）。
    size_t hit_off = (size_t)(first_hit - src);
    const char *nl = (const char *)memchr(first_hit + CDH_GLSL_VER_LEN, '\n',
                                          slen - (hit_off + CDH_GLSL_VER_LEN));
    size_t insert_off = nl ? (size_t)(nl - src) + 1 : slen;
    int need_leading_nl = (nl == NULL);

    size_t poly_len = sizeof(CDH_FMA_POLYFILL) - 1;
    size_t extra_len = (need_leading_nl ? 1 : 0) + poly_len;
    size_t total_len = slen + extra_len;
    char *buf = (char *)malloc(total_len + 1);
    if (!buf) {
        return NULL;
    }

    memcpy(buf, src, insert_off);
    size_t cur = insert_off;
    if (need_leading_nl) {
        buf[cur++] = '\n';
    }
    memcpy(buf + cur, CDH_FMA_POLYFILL, poly_len);
    cur += poly_len;
    if (insert_off < slen) {
        memcpy(buf + cur, src + insert_off, slen - insert_off);
    }
    buf[total_len] = '\0';

    // 将缓冲区内所有 "#version 320 es" / "#version 310 es" 版本号改为 "300"。
    char *p = buf;
    size_t rem = total_len;
    while (rem >= CDH_GLSL_VER_LEN) {
        char *m = (char *)cdh_find_glsl_310_or_320_es(p, rem);
        if (!m) {
            break;
        }
        m[10] = '0';
        size_t adv = (size_t)(m - p) + CDH_GLSL_VER_LEN;
        p += adv;
        rem -= adv;
    }

    if (out_len) {
        *out_len = total_len;
    }
    return buf;
}

typedef struct {
    int patched;
    const char **ptrs;
    int *lens;       // 原 length != NULL 时非空，确保变长分片不被旧 length 截断
    char **owned;
    int count;
} cdh_patched_sources_t;

static inline void cdh_free_patched_sources(cdh_patched_sources_t *res) {
    if (!res) {
        return;
    }
    if (res->owned) {
        for (int i = 0; i < res->count; i++) {
            free(res->owned[i]);
        }
        free(res->owned);
    }
    free((void *)res->ptrs);
    free(res->lens);
    memset(res, 0, sizeof(*res));
}

// 检查并修补 glShaderSource 传入的分片数组；若需要补丁则返回 patched=1 并填充 ptrs/lens，
// 调用方结束后需调用 cdh_free_patched_sources()。若分配失败则返回 patched=0（优雅退回原参数）。
static inline void cdh_prepare_patched_sources(int count,
                                               const char *const *string,
                                               const int *length,
                                               cdh_patched_sources_t *out) {
    memset(out, 0, sizeof(*out));
    if (count <= 0 || !string) {
        return;
    }

    int needs_patch = 0;
    for (int i = 0; i < count; i++) {
        if (!string[i]) {
            continue;
        }
        size_t slen = (length && length[i] >= 0) ? (size_t)length[i] : strlen(string[i]);
        if (cdh_find_glsl_310_or_320_es(string[i], slen) != NULL) {
            needs_patch = 1;
            break;
        }
    }
    if (!needs_patch) {
        return;
    }

    out->count = count;
    out->ptrs = (const char **)malloc((size_t)count * sizeof(const char *));
    out->owned = (char **)calloc((size_t)count, sizeof(char *));
    if (length) {
        out->lens = (int *)malloc((size_t)count * sizeof(int));
    }
    if (!out->ptrs || !out->owned || (length && !out->lens)) {
        cdh_free_patched_sources(out);
        return;
    }

    for (int i = 0; i < count; i++) {
        out->ptrs[i] = string[i];
        if (length) {
            out->lens[i] = length[i];
        }
        if (!string[i]) {
            continue;
        }
        size_t slen = (length && length[i] >= 0) ? (size_t)length[i] : strlen(string[i]);
        size_t new_len = 0;
        char *copy = cdh_patch_glsl_es_chunk(string[i], slen, &new_len);
        if (copy) {
            out->owned[i] = copy;
            out->ptrs[i] = copy;
            if (out->lens) {
                out->lens[i] = (int)new_len;
            }
        }
    }
    out->patched = 1;
}

#endif  // CDH_GL_SHADER_PATCH_H
