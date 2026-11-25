// SwiftShader (vulkan.pastel.so) 外置同步扩展 VK_KHR_external_fence_fd 纯 C 辅助头：
//
// 背景：
//   澎湃 OS 3/4 自研 Rust + Flutter 应用（如相册 com.miui.gallery、天气 com.miui.weather2）
//   内置的 libhyper_os_flutter.so 强制开启 Impeller Vulkan 后端，并在 capabilities_vk.cc
//   中硬性要求物理设备支持 VK_KHR_external_fence_fd（SYNC_FD 句柄类型）。
//   Lineage 底包自带的 vulkan.pastel.so（SwiftShader）仅实现了 VK_KHR_external_semaphore_fd
//   与 VK_KHR_external_fence，未实现 VK_KHR_external_fence_fd，导致 Impeller 报
//   "Device does not support required Android extension: VK_KHR_external_fence_fd" 并直接退出。
//
// 本头文件提供与平台无关的扩展过滤与导入信号态 Fence 状态表，便于宿主 C 单测直接覆盖。
#ifndef CDH_VK_PASTEL_FENCE_H
#define CDH_VK_PASTEL_FENCE_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define CDH_VK_EXT_EXTERNAL_FENCE_NAME "VK_KHR_external_fence"
#define CDH_VK_EXT_EXTERNAL_FENCE_FD_NAME "VK_KHR_external_fence_fd"
#define CDH_VK_EXT_EXTERNAL_FENCE_FD_SPEC_VERSION 1u

#define CDH_VK_FENCE_TABLE_CAP 128u

typedef struct {
    uint64_t handles[CDH_VK_FENCE_TABLE_CAP];
    size_t count;
} cdh_vk_fence_table_t;

static inline void cdh_vk_fence_table_mark(cdh_vk_fence_table_t *tab,
                                           uint64_t fence_bits) {
    if (!tab || fence_bits == 0u) {
        return;
    }
    for (size_t i = 0; i < tab->count; i++) {
        if (tab->handles[i] == fence_bits) {
            return;
        }
    }
    if (tab->count < CDH_VK_FENCE_TABLE_CAP) {
        tab->handles[tab->count++] = fence_bits;
    } else {
        // 容量满时淘汰最旧槽位，避免泄漏阻塞后续导入。
        memmove(&tab->handles[0], &tab->handles[1],
                (CDH_VK_FENCE_TABLE_CAP - 1u) * sizeof(uint64_t));
        tab->handles[CDH_VK_FENCE_TABLE_CAP - 1u] = fence_bits;
    }
}

static inline void cdh_vk_fence_table_unmark(cdh_vk_fence_table_t *tab,
                                             uint64_t fence_bits) {
    if (!tab || fence_bits == 0u) {
        return;
    }
    for (size_t i = 0; i < tab->count; i++) {
        if (tab->handles[i] == fence_bits) {
            tab->handles[i] = tab->handles[tab->count - 1u];
            tab->count--;
            return;
        }
    }
}

static inline int cdh_vk_fence_table_contains(const cdh_vk_fence_table_t *tab,
                                              uint64_t fence_bits) {
    if (!tab || fence_bits == 0u) {
        return 0;
    }
    for (size_t i = 0; i < tab->count; i++) {
        if (tab->handles[i] == fence_bits) {
            return 1;
        }
    }
    return 0;
}

// 过滤 vkCreateDevice 请求的扩展名列表：
//   - 若未请求 VK_KHR_external_fence_fd，不分配内存并返回 0；
//   - 若请求了 VK_KHR_external_fence_fd，剥离该扩展（防止 SwiftShader 报
//     VK_ERROR_EXTENSION_NOT_PRESENT），并在缺失时自动补入前置扩展 VK_KHR_external_fence，
//     通过 *out_exts / *out_count 返回堆分配的新数组（调用方负责 free(*out_exts)），返回 1。
static inline int cdh_vk_filter_device_extensions(
    const char *const *in_exts, uint32_t in_count,
    const char ***out_exts, uint32_t *out_count) {
    if (!out_exts || !out_count) {
        return 0;
    }
    *out_exts = NULL;
    *out_count = in_count;
    if (!in_exts || in_count == 0u) {
        return 0;
    }

    int has_fence_fd = 0;
    int has_ext_fence = 0;
    for (uint32_t i = 0; i < in_count; i++) {
        if (!in_exts[i]) {
            continue;
        }
        if (strcmp(in_exts[i], CDH_VK_EXT_EXTERNAL_FENCE_FD_NAME) == 0) {
            has_fence_fd = 1;
        } else if (strcmp(in_exts[i], CDH_VK_EXT_EXTERNAL_FENCE_NAME) == 0) {
            has_ext_fence = 1;
        }
    }
    if (!has_fence_fd) {
        return 0;
    }

    const char **filtered =
        (const char **)malloc(((size_t)in_count + 1u) * sizeof(const char *));
    if (!filtered) {
        return 0;
    }
    uint32_t dst = 0;
    for (uint32_t i = 0; i < in_count; i++) {
        if (in_exts[i] &&
            strcmp(in_exts[i], CDH_VK_EXT_EXTERNAL_FENCE_FD_NAME) == 0) {
            continue;
        }
        filtered[dst++] = in_exts[i];
    }
    if (!has_ext_fence) {
        filtered[dst++] = CDH_VK_EXT_EXTERNAL_FENCE_NAME;
    }
    *out_exts = filtered;
    *out_count = dst;
    return 1;
}

#endif  // CDH_VK_PASTEL_FENCE_H
