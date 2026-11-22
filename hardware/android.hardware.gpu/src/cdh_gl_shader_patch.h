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
    "#endif\n"
    "#ifndef ldexp\n"
    "#define ldexp(x, e) ((x) * exp2(float(e)))\n"
    "#endif\n"
    "#ifndef GL_EXT_YUV_target\n"
    "#define __samplerExternal2DY2YEXT sampler2D\n"
    "#define yuvCscStandardEXT int\n"
    "#define itu_601 0\n"
    "#define itu_601_full_range 1\n"
    "#define itu_709 2\n"
    "#define rgb_2_yuv(c, s) (c)\n"
    "#define yuv_2_rgb(c, s) (c)\n"
    "#endif\n";

static const char CDH_SSO_EXTENSION[] =
    "#extension GL_EXT_separate_shader_objects : enable\n"
    "#extension GL_OES_shader_io_blocks : enable\n";

static const char CDH_GL_PER_VERTEX_DECL[] =
    "#ifdef GL_OES_shader_io_blocks\n"
    "out gl_PerVertex { highp vec4 gl_Position; };\n"
    "#endif\n";

static inline int cdh_chunk_has_layout_in(const char *s, size_t len) {
    if (!s || len < 10) return 0;
    return memmem(s, len, "layout", 6) != NULL &&
           (memmem(s, len, ") in ", 5) != NULL ||
            memmem(s, len, ") highp in ", 11) != NULL ||
            memmem(s, len, ") mediump in ", 13) != NULL ||
            memmem(s, len, ") lowp in ", 10) != NULL ||
            memmem(s, len, ") flat in ", 10) != NULL ||
            memmem(s, len, ") smooth in ", 12) != NULL);
}

static inline int cdh_chunk_has_layout_out(const char *s, size_t len) {
    if (!s || len < 11) return 0;
    return memmem(s, len, "layout", 6) != NULL &&
           (memmem(s, len, ") out ", 6) != NULL ||
            memmem(s, len, ") highp out ", 12) != NULL ||
            memmem(s, len, ") mediump out ", 14) != NULL ||
            memmem(s, len, ") lowp out ", 11) != NULL ||
            memmem(s, len, ") flat out ", 11) != NULL ||
            memmem(s, len, ") smooth out ", 13) != NULL);
}

static const char CDH_TEXTURE_GATHER_POLYFILL[] =
    "highp vec4 textureGather(highp sampler2D s, highp vec2 p) { return texture(s, p).rrrr; }\n"
    "highp vec4 textureGather(highp sampler2D s, highp vec2 p, int c) { return texture(s, p).rrrr; }\n"
    "highp uvec4 textureGather(highp usampler2D s, highp vec2 p) { return texture(s, p).rrrr; }\n"
    "highp uvec4 textureGather(highp usampler2D s, highp vec2 p, int c) { return texture(s, p).rrrr; }\n";

static inline int cdh_needs_texture_gather_polyfill(const char *s, size_t len) {
    if (!s || len < 14) return 0;
    if (memmem(s, len, "textureGather(", 14) == NULL) return 0;
    // 若着色器自身已声明 vec4/uvec4 textureGather(...) 软实现，则不重复定义。
    if (memmem(s, len, "vec4 textureGather(", 19) != NULL) return 0;
    return 1;
}

static inline int cdh_is_ident_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}

static inline int cdh_is_space_char(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static inline void cdh_blank_preserve_nl(char *p, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (p[i] != '\n') {
            p[i] = ' ';
        }
    }
}

// 判定单个 layout 限定词项是否为 GLSL ES 3.00 不支持的项（binding = N / yuv；
// 若紧跟 uniform 声明，ES 3.00 亦不支持 location = N）。
static inline int cdh_is_es300_unsupported_layout_item(const char *start, const char *end,
                                                       int is_uniform_decl) {
    while (start < end && cdh_is_space_char(*start)) start++;
    while (end > start && cdh_is_space_char(*(end - 1))) end--;
    size_t n = (size_t)(end - start);
    if (n == 3 && memcmp(start, "yuv", 3) == 0) {
        return 1;
    }
    if (n >= 7 && memcmp(start, "binding", 7) == 0) {
        const char *q = start + 7;
        while (q < end && cdh_is_space_char(*q)) q++;
        if (q < end && *q == '=') {
            return 1;
        }
    }
    if (is_uniform_decl && n >= 8 && memcmp(start, "location", 8) == 0) {
        const char *q = start + 8;
        while (q < end && cdh_is_space_char(*q)) q++;
        if (q < end && *q == '=') {
            return 1;
        }
    }
    return 0;
}

// 检查 layout(...) 之后的首个关键字是否为 uniform。
static inline int cdh_followed_by_uniform(const char *buf, size_t len, size_t after_paren) {
    size_t p = after_paren;
    while (p < len && cdh_is_space_char(buf[p])) p++;
    return (p + 7 <= len && memcmp(buf + p, "uniform", 7) == 0 &&
            (p + 7 == len || !cdh_is_ident_char(buf[p + 7])));
}

// 原地用空格擦除 GLSL ES 3.00 不支持的 layout 限定词（binding = N、yuv）；
// 若括号内全部被擦空则连同 layout(...) 一并擦空。保留换行符与总长度不变。
static inline void cdh_strip_es310_layout_qualifiers(char *buf, size_t len) {
    if (!buf || len < 8) {
        return;
    }
    size_t i = 0;
    while (i + 6 <= len) {
        if (memcmp(buf + i, "layout", 6) != 0 ||
            (i > 0 && cdh_is_ident_char(buf[i - 1])) ||
            (i + 6 < len && cdh_is_ident_char(buf[i + 6]))) {
            i++;
            continue;
        }
        size_t j = i + 6;
        while (j < len && cdh_is_space_char(buf[j])) j++;
        if (j >= len || buf[j] != '(') {
            i += 6;
            continue;
        }
        size_t open_paren = j;
        size_t close_paren = open_paren + 1;
        int depth = 1;
        while (close_paren < len && depth > 0) {
            if (buf[close_paren] == '(') depth++;
            else if (buf[close_paren] == ')') depth--;
            if (depth > 0) close_paren++;
        }
        if (depth != 0) {
            break;
        }

        const int is_uniform = cdh_followed_by_uniform(buf, len, close_paren + 1);

        // 解析括号内逗号分隔的限定词项
        size_t item_starts[16];
        size_t item_ends[16];
        int item_drop[16];
        int item_count = 0;
        int any_drop = 0;
        int keep_count = 0;
        size_t cur = open_paren + 1;
        while (cur <= close_paren && item_count < 16) {
            size_t seg = cur;
            while (seg < close_paren && buf[seg] != ',') seg++;
            item_starts[item_count] = cur;
            item_ends[item_count] = seg;
            int drop = cdh_is_es300_unsupported_layout_item(buf + cur, buf + seg, is_uniform);
            item_drop[item_count] = drop;
            if (drop) {
                any_drop = 1;
            } else {
                // 跳过纯空白项
                const char *a = buf + cur;
                const char *b = buf + seg;
                while (a < b && cdh_is_space_char(*a)) a++;
                if (a < b) keep_count++;
            }
            item_count++;
            if (seg == close_paren) break;
            cur = seg + 1;
        }

        if (any_drop) {
            if (keep_count == 0) {
                cdh_blank_preserve_nl(buf + i, (close_paren + 1) - i);
            } else {
                // 逐项擦除需移除的项及多余逗号：
                // 优先擦除被删项后面紧邻的逗号；若它是最后一项，则擦除前一个保留项后面的逗号。
                int last_kept = -1;
                for (int k = 0; k < item_count; k++) {
                    if (item_drop[k]) {
                        cdh_blank_preserve_nl(buf + item_starts[k],
                                              item_ends[k] - item_starts[k]);
                        if (item_ends[k] < close_paren && buf[item_ends[k]] == ',') {
                            buf[item_ends[k]] = ' ';
                        } else if (last_kept >= 0 &&
                                   item_ends[last_kept] < close_paren &&
                                   buf[item_ends[last_kept]] == ',') {
                            buf[item_ends[last_kept]] = ' ';
                        }
                    } else {
                        last_kept = k;
                    }
                }
            }
        }
        i = close_paren + 1;
    }
}

// 原地将 GLSL ES 3.10+ 写法 "{highp,mediump,lowp} {in,out}" 调整为 ES 3.00 合法语序
// "{in,out} {highp,mediump,lowp}"（等长原地替换，保持行号与总长度不变）。
static inline void cdh_reorder_precision_storage_qualifiers(char *buf, size_t len) {
    static const struct {
        const char *from;
        const char *to;
        size_t flen;
    } kPairs[] = {
        {"highp in ", "in highp ", 9},
        {"highp out ", "out highp ", 10},
        {"mediump in ", "in mediump ", 11},
        {"mediump out ", "out mediump ", 12},
        {"lowp in ", "in lowp ", 8},
        {"lowp out ", "out lowp ", 9},
    };
    for (size_t k = 0; k < sizeof(kPairs) / sizeof(kPairs[0]); k++) {
        size_t flen = kPairs[k].flen;
        if (len < flen) continue;
        size_t pos = 0;
        while (pos + flen <= len) {
            char *hit = (char *)memmem(buf + pos, len - pos, kPairs[k].from, flen);
            if (!hit) break;
            size_t off = (size_t)(hit - buf);
            if (off == 0 || !cdh_is_ident_char(buf[off - 1])) {
                memcpy(hit, kPairs[k].to, flen);
            }
            pos = off + flen;
        }
    }
}

static inline int cdh_has_layout_keyword(const char *s, size_t len) {
    return s != NULL && len >= 6 &&
           (memmem(s, len, "layout", 6) != NULL ||
            memmem(s, len, "highp ", 6) != NULL ||
            memmem(s, len, "mediump ", 8) != NULL ||
            memmem(s, len, "lowp ", 5) != NULL);
}

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

// 将 GLSL ES 3.10+ 二维数组 `T name[3][3]` 平铺为一维数组 `T name[9]`，
// 并将后续出现的 `name[e1][e2]` 改写为 `name[(e1)*3+(e2)]`。
// 若源码不含 `[3][3]` 声明则返回 NULL（零开销）。
static inline char *cdh_flatten_3x3_arrays(const char *src, size_t len, size_t *out_len) {
    if (!src || len < 10 || memmem(src, len, "[3][3]", 6) == NULL) {
        return NULL;
    }
    // 收集所有带 [3][3] 声明的标识符（排除 mat/vec 索引 t[3][3]：声明前必有类型名与空白）
    char names[8][64];
    size_t name_lens[8];
    int name_count = 0;
    size_t scan = 0;
    while (scan + 6 <= len && name_count < 8) {
        const char *hit = (const char *)memmem(src + scan, len - scan, "[3][3]", 6);
        if (!hit) break;
        size_t off = (size_t)(hit - src);
        size_t id_end = off;
        size_t id_start = id_end;
        while (id_start > 0 && cdh_is_ident_char(src[id_start - 1])) id_start--;
        size_t id_len = id_end - id_start;
        // 检查标识符前是否有空白和前导类型标识符（如 `vec4 bayer_3x3[3][3]`）
        size_t sp = id_start;
        while (sp > 0 && (src[sp - 1] == ' ' || src[sp - 1] == '\t')) sp--;
        int has_type_before = (sp < id_start && sp > 0 && cdh_is_ident_char(src[sp - 1]));
        if (has_type_before && id_len > 0 && id_len < 63) {
            int dup = 0;
            for (int k = 0; k < name_count; k++) {
                if (name_lens[k] == id_len && memcmp(names[k], src + id_start, id_len) == 0) {
                    dup = 1;
                    break;
                }
            }
            if (!dup) {
                memcpy(names[name_count], src + id_start, id_len);
                names[name_count][id_len] = '\0';
                name_lens[name_count] = id_len;
                name_count++;
            }
        }
        scan = off + 6;
    }
    if (name_count == 0) {
        return NULL;
    }
    // 每次 `name[e1][e2]` -> `name[(e1)*3+(e2)]` 最多增加 5 字节，预留 2x 容量足够安全。
    size_t cap = len * 2 + 256;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    size_t out_pos = 0;
    size_t i = 0;
    while (i < len) {
        int matched = -1;
        for (int k = 0; k < name_count; k++) {
            size_t nl = name_lens[k];
            if (i + nl < len && memcmp(src + i, names[k], nl) == 0 &&
                (i == 0 || !cdh_is_ident_char(src[i - 1])) &&
                src[i + nl] == '[') {
                matched = k;
                break;
            }
        }
        if (matched < 0) {
            out[out_pos++] = src[i++];
            continue;
        }
        size_t nl = name_lens[matched];
        // 解析第一个 [ ... ]
        size_t b1_start = i + nl;
        size_t p = b1_start + 1;
        int d = 1;
        while (p < len && d > 0) {
            if (src[p] == '[') d++;
            else if (src[p] == ']') d--;
            if (d > 0) p++;
        }
        if (d != 0 || p + 1 >= len || src[p + 1] != '[') {
            memcpy(out + out_pos, src + i, nl);
            out_pos += nl;
            i += nl;
            continue;
        }
        size_t b1_end = p;
        size_t b2_start = p + 1;
        p = b2_start + 1;
        d = 1;
        while (p < len && d > 0) {
            if (src[p] == '[') d++;
            else if (src[p] == ']') d--;
            if (d > 0) p++;
        }
        if (d != 0) {
            memcpy(out + out_pos, src + i, nl);
            out_pos += nl;
            i += nl;
            continue;
        }
        size_t b2_end = p;
        // 检查是否为声明（`[3][3]` 且前面有类型名）
        size_t sp = i;
        while (sp > 0 && (src[sp - 1] == ' ' || src[sp - 1] == '\t')) sp--;
        int is_decl = (sp < i && sp > 0 && cdh_is_ident_char(src[sp - 1]) &&
                       (b1_end - b1_start == 2 && src[b1_start + 1] == '3') &&
                       (b2_end - b2_start == 2 && src[b2_start + 1] == '3'));
        // 写入标识符
        memcpy(out + out_pos, src + i, nl);
        out_pos += nl;
        if (is_decl) {
            memcpy(out + out_pos, "[9]", 3);
            out_pos += 3;
        } else {
            // `[(e1)*3+(e2)]`
            size_t e1_len = b1_end - (b1_start + 1);
            size_t e2_len = b2_end - (b2_start + 1);
            if (out_pos + e1_len + e2_len + 16 < cap) {
                out[out_pos++] = '[';
                out[out_pos++] = '(';
                memcpy(out + out_pos, src + b1_start + 1, e1_len);
                out_pos += e1_len;
                memcpy(out + out_pos, ")*3+(", 5);
                out_pos += 5;
                memcpy(out + out_pos, src + b2_start + 1, e2_len);
                out_pos += e2_len;
                out[out_pos++] = ')';
                out[out_pos++] = ']';
            }
        }
        i = b2_end + 1;
    }
    out[out_pos] = '\0';
    if (out_len) *out_len = out_pos;
    return out;
}

// 跳过头部预处理指令/注释/空行，返回首个非预处理语句起始位置（供注入函数体 polyfill）。
static inline size_t cdh_find_after_header_directives(const char *s, size_t len, size_t start) {
    size_t pos = start;
    while (pos < len) {
        size_t line = pos;
        while (line < len && (s[line] == ' ' || s[line] == '\t')) line++;
        if (line >= len) return len;
        if (s[line] == '\n' || s[line] == '\r' || s[line] == '#' ||
            (line + 1 < len && s[line] == '/' && s[line + 1] == '/')) {
            const char *nl = (const char *)memchr(s + line, '\n', len - line);
            pos = nl ? (size_t)(nl - s) + 1 : len;
            continue;
        }
        break;
    }
    return pos;
}

// 对单个着色器分片执行版本降级并在首条被降级的 #version 行后注入 fma() 预处理宏。
// 未命中 #version 310/320 es 时返回 NULL；命中时返回新分配的以 '\0' 结尾的缓冲区，
// 并通过 *out_len 返回新长度（不含末尾 '\0'）。调用方负责 free() 返回值。
static inline char *cdh_patch_glsl_es_chunk(const char *src, size_t slen,
                                            int inject_sso, int inject_per_vertex,
                                            size_t *out_len) {
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

    int need_sso = inject_sso ? 1 : 0;
    int need_per_vtx = inject_per_vertex ? 1 : 0;
    int need_gather = cdh_needs_texture_gather_polyfill(src, slen);
    int need_after_hdr = need_gather || need_per_vtx;
    size_t after_hdr_off =
        need_after_hdr ? cdh_find_after_header_directives(src, slen, insert_off) : insert_off;
    size_t poly_len = sizeof(CDH_FMA_POLYFILL) - 1;
    size_t sso_len = need_sso ? (sizeof(CDH_SSO_EXTENSION) - 1) : 0;
    size_t pv_len = need_per_vtx ? (sizeof(CDH_GL_PER_VERTEX_DECL) - 1) : 0;
    size_t gather_len = need_gather ? (sizeof(CDH_TEXTURE_GATHER_POLYFILL) - 1) : 0;
    size_t extra_len = (need_leading_nl ? 1 : 0) + poly_len + sso_len + pv_len + gather_len;
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
    if (need_sso) {
        memcpy(buf + cur, CDH_SSO_EXTENSION, sso_len);
        cur += sso_len;
    }
    if (need_after_hdr && after_hdr_off > insert_off) {
        memcpy(buf + cur, src + insert_off, after_hdr_off - insert_off);
        cur += (after_hdr_off - insert_off);
    }
    if (need_per_vtx) {
        memcpy(buf + cur, CDH_GL_PER_VERTEX_DECL, pv_len);
        cur += pv_len;
    }
    if (need_gather) {
        memcpy(buf + cur, CDH_TEXTURE_GATHER_POLYFILL, gather_len);
        cur += gather_len;
    }
    size_t tail_off = need_after_hdr ? after_hdr_off : insert_off;
    if (tail_off < slen) {
        memcpy(buf + cur, src + tail_off, slen - tail_off);
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

    cdh_strip_es310_layout_qualifiers(buf, total_len);
    cdh_reorder_precision_storage_qualifiers(buf, total_len);

    size_t flat_len = 0;
    char *flat = cdh_flatten_3x3_arrays(buf, total_len, &flat_len);
    if (flat) {
        free(buf);
        buf = flat;
        total_len = flat_len;
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
    int any_has_sso_ext = 0;
    int any_per_vertex = 0;
    int any_gl_pos = 0;
    int any_layout_in = 0;
    int any_layout_out = 0;
    for (int i = 0; i < count; i++) {
        if (!string[i]) {
            continue;
        }
        size_t slen = (length && length[i] >= 0) ? (size_t)length[i] : strlen(string[i]);
        if (cdh_find_glsl_310_or_320_es(string[i], slen) != NULL) {
            needs_patch = 1;
        }
        if (slen >= 30 && memmem(string[i], slen, "GL_EXT_separate_shader_objects", 30) != NULL) {
            any_has_sso_ext = 1;
        }
        if (slen >= 12 && memmem(string[i], slen, "gl_PerVertex", 12) != NULL) {
            any_per_vertex = 1;
        }
        if (slen >= 11 && memmem(string[i], slen, "gl_Position", 11) != NULL) {
            any_gl_pos = 1;
        }
        if (cdh_chunk_has_layout_in(string[i], slen)) {
            any_layout_in = 1;
        }
        if (cdh_chunk_has_layout_out(string[i], slen)) {
            any_layout_out = 1;
        }
    }
    if (!needs_patch) {
        return;
    }
    // Separate Shader Objects 按需注入：
    // 1) 顶点着色器（含 gl_Position）：仅当显式声明 gl_PerVertex 或使用了 ES 3.00 不支持的
    //    `layout(location=...) out`（如 rectiface/forward_warp.vert）时注入 SSO 扩展；
    //    若未声明 gl_PerVertex 则同时补写 `out gl_PerVertex { highp vec4 gl_Position; };`，
    //    防止宿主 VirGL/Apple GL 报 "gl_Position must be redeclared before use"。
    // 2) 非顶点着色器：仅在出现 `layout(...) in` 时注入 SSO 扩展。
    // 普通 Skia RenderThread 顶点着色器不含 layout(...) out 也不含 gl_PerVertex，完全不注入。
    int is_sso_vertex = any_per_vertex || (any_gl_pos && any_layout_out);
    int inject_sso = !any_has_sso_ext && (is_sso_vertex || (!any_gl_pos && any_layout_in));
    int inject_per_vertex = (inject_sso || any_has_sso_ext) && any_gl_pos && !any_per_vertex;

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
        char *copy =
            cdh_patch_glsl_es_chunk(string[i], slen, inject_sso, inject_per_vertex, &new_len);
        if (!copy && cdh_has_layout_keyword(string[i], slen)) {
            copy = (char *)malloc(slen + 1);
            if (copy) {
                memcpy(copy, string[i], slen);
                copy[slen] = '\0';
                cdh_strip_es310_layout_qualifiers(copy, slen);
                cdh_reorder_precision_storage_qualifiers(copy, slen);
                new_len = slen;
            }
        }
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
