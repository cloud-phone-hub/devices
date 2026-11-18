// SPIR-V 寄存器级 Float16 -> Float32 提升补丁（纯 C99，无 Android 头依赖，支持宿主单测）：
//
// 背景：
//   SwiftShader (vulkan.pastel.so) 未实现 SPIR-V Float16 / OpFConvert：遇到 OpFConvert 时
//   仅打印 WARNING 并跳过 DefineResult，导致后续 EmitCompositeConstruct 查找结果 SSA ID 时
//   命中空槽解引用 NULL 崩溃（HyperOS 4 桌面 MiGlassV13F16 材质着色器即触发此问题）。
//
// 改写规则（等字长原地改写，不改变任何分支偏移或字数）：
//   - OpCapability Float16 (17, 9) -> OpCapability Shader (17, 1)
//   - OpTypeFloat %id 16 (22, %id, 16) -> OpTypeFloat %id 32
//   - OpConstant / OpSpecConstant (%fp16_type) -> IEEE 754 binary16 转 binary32
//   - OpFConvert (115) -> OpCopyObject (83)
#ifndef CDH_SPV_FP16_H
#define CDH_SPV_FP16_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define CDH_SPV_MAGIC 0x07230203u
#define CDH_SPV_OP_CAPABILITY 17u
#define CDH_SPV_OP_TYPE_FLOAT 22u
#define CDH_SPV_OP_CONSTANT 43u
#define CDH_SPV_OP_SPEC_CONSTANT 50u
#define CDH_SPV_OP_COPY_OBJECT 83u
#define CDH_SPV_OP_FCONVERT 115u
#define CDH_SPV_CAP_SHADER 1u
#define CDH_SPV_CAP_FLOAT16 9u

typedef struct {
    size_t fp16_types;
    unsigned consts;
    unsigned fconverts;
} cdh_spv_patch_stats_t;

static inline int cdh_spv_has_fp16(const uint32_t *code, size_t word_count) {
    if (!code || word_count < 5 || code[0] != CDH_SPV_MAGIC) {
        return 0;
    }
    size_t off = 5;
    while (off < word_count) {
        uint32_t w0 = code[off];
        uint32_t wc = w0 >> 16;
        uint32_t op = w0 & 0xffffu;
        if (wc == 0 || off + wc > word_count) {
            break;
        }
        if ((op == CDH_SPV_OP_CAPABILITY && wc == 2 &&
             code[off + 1] == CDH_SPV_CAP_FLOAT16) ||
            (op == CDH_SPV_OP_TYPE_FLOAT && wc == 3 && code[off + 2] == 16u) ||
            (op == CDH_SPV_OP_FCONVERT && wc == 4)) {
            return 1;
        }
        off += wc;
    }
    return 0;
}

// 纯整数 IEEE 754 binary16 -> binary32 位级转换（跨架构一致，支持零、次正规数、无穷与 NaN）。
static inline uint32_t cdh_fp16_to_fp32_bits(uint32_t w) {
    uint32_t h = w & 0xffffu;
    uint32_t sign = (h & 0x8000u) << 16;
    uint32_t exp = (h >> 10) & 0x1fu;
    uint32_t mant = h & 0x03ffu;

    if (exp == 0u) {
        if (mant == 0u) {
            return sign;
        }
        int e = -1;
        do {
            e++;
            mant <<= 1;
        } while ((mant & 0x0400u) == 0u);
        mant &= 0x03ffu;
        uint32_t f_exp = (uint32_t)(127 - 15 - e);
        return sign | (f_exp << 23) | (mant << 13);
    }
    if (exp == 31u) {
        return sign | 0x7f800000u | (mant << 13);
    }
    return sign | ((exp + (127u - 15u)) << 23) | (mant << 13);
}

// 若 SPIR-V 模块含 Float16 则分配并返回修补后的副本（调用方负责 free），否则返回 NULL。
static inline uint32_t *cdh_spv_promote_fp16_copy(const uint32_t *code,
                                                  size_t code_size,
                                                  cdh_spv_patch_stats_t *stats) {
    if (stats) {
        memset(stats, 0, sizeof(*stats));
    }
    if (!code || code_size < 20 || (code_size & 3u) != 0) {
        return NULL;
    }
    size_t word_count = code_size >> 2;
    if (!cdh_spv_has_fp16(code, word_count)) {
        return NULL;
    }
    uint32_t *patched = (uint32_t *)malloc(code_size);
    if (!patched) {
        return NULL;
    }
    memcpy(patched, code, code_size);

    uint32_t fp16_types[16];
    size_t fp16_type_count = 0;
    unsigned fconvert_count = 0;
    unsigned const_count = 0;

    size_t off = 5;
    while (off < word_count) {
        uint32_t w0 = patched[off];
        uint32_t wc = w0 >> 16;
        uint32_t op = w0 & 0xffffu;
        if (wc == 0 || off + wc > word_count) {
            break;
        }
        if (op == CDH_SPV_OP_CAPABILITY && wc == 2 &&
            patched[off + 1] == CDH_SPV_CAP_FLOAT16) {
            patched[off + 1] = CDH_SPV_CAP_SHADER;
        } else if (op == CDH_SPV_OP_TYPE_FLOAT && wc == 3 &&
                   patched[off + 2] == 16u) {
            if (fp16_type_count < sizeof(fp16_types) / sizeof(fp16_types[0])) {
                fp16_types[fp16_type_count++] = patched[off + 1];
            }
            patched[off + 2] = 32u;
        } else if ((op == CDH_SPV_OP_CONSTANT ||
                    op == CDH_SPV_OP_SPEC_CONSTANT) &&
                   wc == 4) {
            uint32_t res_type = patched[off + 1];
            for (size_t i = 0; i < fp16_type_count; i++) {
                if (fp16_types[i] == res_type) {
                    patched[off + 3] = cdh_fp16_to_fp32_bits(patched[off + 3]);
                    const_count++;
                    break;
                }
            }
        } else if (op == CDH_SPV_OP_FCONVERT && wc == 4) {
            patched[off] = (4u << 16) | CDH_SPV_OP_COPY_OBJECT;
            fconvert_count++;
        }
        off += wc;
    }

    if (stats) {
        stats->fp16_types = fp16_type_count;
        stats->consts = const_count;
        stats->fconverts = fconvert_count;
    }
    return patched;
}

#endif  // CDH_SPV_FP16_H
