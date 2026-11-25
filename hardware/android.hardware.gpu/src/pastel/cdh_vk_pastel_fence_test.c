// cdh_vk_pastel_fence.h 宿主单元测试：
// 校验 VK_KHR_external_fence_fd 设备扩展剥离/补全与导入信号态 Fence 状态表语义。
#include "cdh_vk_pastel_fence.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_filter_without_fence_fd(void) {
    const char *exts[] = {"VK_KHR_swapchain", "VK_KHR_external_semaphore_fd"};
    const char **out = NULL;
    uint32_t out_count = 0;
    int changed = cdh_vk_filter_device_extensions(exts, 2u, &out, &out_count);
    assert(changed == 0);
    assert(out == NULL);
    assert(out_count == 2u);
}

static void test_filter_strips_fence_fd_and_injects_base_fence(void) {
    const char *exts[] = {
        "VK_KHR_swapchain",
        "VK_KHR_external_fence_fd",
        "VK_KHR_external_semaphore_fd",
    };
    const char **out = NULL;
    uint32_t out_count = 0;
    int changed = cdh_vk_filter_device_extensions(exts, 3u, &out, &out_count);
    assert(changed == 1);
    assert(out != NULL);
    assert(out_count == 3u);
    assert(strcmp(out[0], "VK_KHR_swapchain") == 0);
    assert(strcmp(out[1], "VK_KHR_external_semaphore_fd") == 0);
    assert(strcmp(out[2], "VK_KHR_external_fence") == 0);
    free(out);
}

static void test_filter_preserves_existing_base_fence(void) {
    const char *exts[] = {
        "VK_KHR_external_fence",
        "VK_KHR_external_fence_fd",
    };
    const char **out = NULL;
    uint32_t out_count = 0;
    int changed = cdh_vk_filter_device_extensions(exts, 2u, &out, &out_count);
    assert(changed == 1);
    assert(out != NULL);
    assert(out_count == 1u);
    assert(strcmp(out[0], "VK_KHR_external_fence") == 0);
    free(out);
}

static void test_fence_table_lifecycle(void) {
    cdh_vk_fence_table_t tab;
    memset(&tab, 0, sizeof(tab));

    assert(cdh_vk_fence_table_contains(&tab, 0x1001u) == 0);
    cdh_vk_fence_table_mark(&tab, 0x1001u);
    cdh_vk_fence_table_mark(&tab, 0x1001u);  // 幂等去重
    cdh_vk_fence_table_mark(&tab, 0x2002u);
    assert(tab.count == 2u);
    assert(cdh_vk_fence_table_contains(&tab, 0x1001u) == 1);
    assert(cdh_vk_fence_table_contains(&tab, 0x2002u) == 1);

    cdh_vk_fence_table_unmark(&tab, 0x1001u);
    assert(cdh_vk_fence_table_contains(&tab, 0x1001u) == 0);
    assert(cdh_vk_fence_table_contains(&tab, 0x2002u) == 1);
    assert(tab.count == 1u);
}

int main(void) {
    test_filter_without_fence_fd();
    test_filter_strips_fence_fd_and_injects_base_fence();
    test_filter_preserves_existing_base_fence();
    test_fence_table_lifecycle();
    printf("[pass] cdh_vk_pastel_fence_test: 4/4 passed\n");
    return 0;
}
