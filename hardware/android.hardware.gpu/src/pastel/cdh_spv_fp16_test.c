// cdh_spv_fp16.h 宿主单元测试：
// 校验纯 Float32 SPIR-V 不触发改写，含 Float16 能力/类型/常量/OpFConvert 的模块被等长提升。
#include "cdh_spv_fp16.h"

#include <assert.h>
#include <stdio.h>

static void test_fp32_untouched(void) {
    const uint32_t spv[] = {
        CDH_SPV_MAGIC, 0x00010000u, 0u, 8u, 0u,
        (2u << 16) | CDH_SPV_OP_CAPABILITY, CDH_SPV_CAP_SHADER,
        (3u << 16) | CDH_SPV_OP_TYPE_FLOAT, 1u, 32u,
    };
    cdh_spv_patch_stats_t stats;
    uint32_t *out = cdh_spv_promote_fp16_copy(spv, sizeof(spv), &stats);
    assert(out == NULL);
    assert(stats.fp16_types == 0);
}

static void test_fp16_promoted_to_fp32(void) {
    const uint32_t spv[] = {
        CDH_SPV_MAGIC, 0x00010000u, 0u, 16u, 0u,
        (2u << 16) | CDH_SPV_OP_CAPABILITY, CDH_SPV_CAP_FLOAT16,
        (3u << 16) | CDH_SPV_OP_TYPE_FLOAT, 2u, 16u,
        // 0x3c00 = 1.0 in binary16 -> 0x3f800000 in binary32
        (4u << 16) | CDH_SPV_OP_CONSTANT, 2u, 3u, 0x00003c00u,
        // OpFConvert %res_type %id %operand -> OpCopyObject
        (4u << 16) | CDH_SPV_OP_FCONVERT, 2u, 4u, 3u,
    };
    cdh_spv_patch_stats_t stats;
    uint32_t *out = cdh_spv_promote_fp16_copy(spv, sizeof(spv), &stats);
    assert(out != NULL);
    assert(stats.fp16_types == 1);
    assert(stats.consts == 1);
    assert(stats.fconverts == 1);

    assert(out[6] == CDH_SPV_CAP_SHADER);
    assert(out[9] == 32u);
    assert(out[13] == 0x3f800000u);
    assert(out[14] == ((4u << 16) | CDH_SPV_OP_COPY_OBJECT));
    free(out);
}

int main(void) {
    test_fp32_untouched();
    test_fp16_promoted_to_fp32();
    printf("[pass] cdh_spv_fp16_test: 2/2 passed\n");
    return 0;
}
