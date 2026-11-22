/*
 * cdh_opencl_stub.c — OpenCL 兼容桩库（libOpenCL.so / libOpenCL-pixel.so）
 *
 * 云机无物理 Mali/Adreno OpenCL 运行时：
 * 1. 默认线程（PortraitSegmenter / NativeDeblurFusion / HDR+ 等）调用
 *    clGetPlatformIDs 时返回 CL_PLATFORM_NOT_FOUND_KHR(-1001)，引导它们走
 *    原生稳定的 CPU / OpenGL 降级路径；
 * 2. GoogleCamera 长曝光（Long Exposure / Action Pan，Lasagna）的
 *    MotionBlurProc 线程在 CreatePaneerProcessor 失败后未判空即把 nullptr
 *    存入 LasagnaNativeProcessor 并在 setOptions 触发空指针 SIGSEGV。
 *    对 MotionBlurProc 线程提供标准 OpenCL 3.0 内存型虚拟平台/上下文/
 *    队列/程序/内核/缓冲对象，使其顺利初始化且不干扰其他线程的 CPU/GL 回退。
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dlfcn.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define CL_SUCCESS                      0
#define CL_DEVICE_NOT_FOUND             (-1)
#define CL_INVALID_VALUE                (-30)
#define CL_INVALID_CONTEXT              (-34)
#define CL_INVALID_MEM_OBJECT           (-38)
#define CL_INVALID_PROGRAM              (-44)
#define CL_INVALID_KERNEL               (-48)
#define CL_PLATFORM_NOT_FOUND_KHR       (-1001)

#define CL_FALSE                        0
#define CL_TRUE                         1

#define CL_DEVICE_TYPE_GPU              (1ULL << 2)
#define CL_MEM_USE_HOST_PTR             (1ULL << 3)
#define CL_MEM_COPY_HOST_PTR            (1ULL << 5)

/* Platform info */
#define CL_PLATFORM_PROFILE             0x0900
#define CL_PLATFORM_VERSION             0x0901
#define CL_PLATFORM_NAME                0x0902
#define CL_PLATFORM_VENDOR              0x0903
#define CL_PLATFORM_EXTENSIONS          0x0904
#define CL_PLATFORM_NUMERIC_VERSION     0x0906

/* Device info */
#define CL_DEVICE_TYPE                          0x1000
#define CL_DEVICE_VENDOR_ID                     0x1001
#define CL_DEVICE_MAX_COMPUTE_UNITS             0x1002
#define CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS      0x1003
#define CL_DEVICE_MAX_WORK_GROUP_SIZE           0x1004
#define CL_DEVICE_MAX_WORK_ITEM_SIZES           0x1005
#define CL_DEVICE_MAX_CLOCK_FREQUENCY           0x100C
#define CL_DEVICE_ADDRESS_BITS                  0x100D
#define CL_DEVICE_MAX_READ_IMAGE_ARGS           0x100E
#define CL_DEVICE_MAX_WRITE_IMAGE_ARGS          0x100F
#define CL_DEVICE_MAX_MEM_ALLOC_SIZE            0x1010
#define CL_DEVICE_IMAGE2D_MAX_WIDTH             0x1011
#define CL_DEVICE_IMAGE2D_MAX_HEIGHT            0x1012
#define CL_DEVICE_IMAGE3D_MAX_WIDTH             0x1013
#define CL_DEVICE_IMAGE3D_MAX_HEIGHT            0x1014
#define CL_DEVICE_IMAGE3D_MAX_DEPTH             0x1015
#define CL_DEVICE_IMAGE_SUPPORT                 0x1016
#define CL_DEVICE_MAX_PARAMETER_SIZE            0x1017
#define CL_DEVICE_MAX_SAMPLERS                  0x1018
#define CL_DEVICE_MEM_BASE_ADDR_ALIGN           0x1019
#define CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE     0x101D
#define CL_DEVICE_GLOBAL_MEM_CACHE_SIZE         0x101E
#define CL_DEVICE_GLOBAL_MEM_SIZE               0x101F
#define CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE      0x1020
#define CL_DEVICE_MAX_CONSTANT_ARGS             0x1021
#define CL_DEVICE_LOCAL_MEM_TYPE                0x1022
#define CL_DEVICE_LOCAL_MEM_SIZE                0x1023
#define CL_DEVICE_AVAILABLE                     0x1027
#define CL_DEVICE_COMPILER_AVAILABLE            0x1028
#define CL_DEVICE_QUEUE_PROPERTIES              0x102A
#define CL_DEVICE_NAME                          0x102B
#define CL_DEVICE_VENDOR                        0x102C
#define CL_DRIVER_VERSION                       0x102D
#define CL_DEVICE_PROFILE                       0x102E
#define CL_DEVICE_VERSION                       0x102F
#define CL_DEVICE_EXTENSIONS                    0x1030
#define CL_DEVICE_PLATFORM                      0x1031
#define CL_DEVICE_OPENCL_C_VERSION              0x103D
#define CL_DEVICE_LINKER_AVAILABLE              0x103E
#define CL_DEVICE_IMAGE_MAX_BUFFER_SIZE         0x1040
#define CL_DEVICE_IMAGE_MAX_ARRAY_SIZE          0x1041
#define CL_DEVICE_IMAGE_PITCH_ALIGNMENT         0x104A
#define CL_DEVICE_IMAGE_BASE_ADDRESS_ALIGNMENT  0x104B
#define CL_DEVICE_MAX_READ_WRITE_IMAGE_ARGS     0x104C
#define CL_DEVICE_NUMERIC_VERSION               0x105E
#define CL_DEVICE_EXTENSIONS_WITH_VERSION       0x1060
#define CL_DEVICE_OPENCL_C_ALL_VERSIONS         0x1066
#define CL_DEVICE_OPENCL_C_FEATURES             0x106F
#define CL_DEVICE_IMAGE_SLICE_PITCH_ALIGNMENT   0x1071

/* Context / Queue / Program / Kernel / Mem info */
#define CL_CONTEXT_REFERENCE_COUNT              0x1080
#define CL_CONTEXT_DEVICES                      0x1081
#define CL_CONTEXT_NUM_DEVICES                  0x1083
#define CL_QUEUE_CONTEXT                        0x1090
#define CL_QUEUE_DEVICE                         0x1091
#define CL_QUEUE_REFERENCE_COUNT                0x1092
#define CL_MEM_TYPE                             0x1100
#define CL_MEM_FLAGS                            0x1101
#define CL_MEM_SIZE                             0x1102
#define CL_MEM_HOST_PTR                         0x1103
#define CL_MEM_CONTEXT                          0x1106
#define CL_IMAGE_WIDTH                          0x1114
#define CL_IMAGE_HEIGHT                         0x1115
#define CL_IMAGE_DEPTH                          0x1116
#define CL_IMAGE_ROW_PITCH                      0x1112
#define CL_IMAGE_SLICE_PITCH                    0x1113
#define CL_PROGRAM_REFERENCE_COUNT              0x1160
#define CL_PROGRAM_CONTEXT                      0x1161
#define CL_PROGRAM_NUM_DEVICES                  0x1162
#define CL_PROGRAM_DEVICES                      0x1163
#define CL_PROGRAM_BINARY_SIZES                 0x1165
#define CL_PROGRAM_BINARIES                     0x1166
#define CL_PROGRAM_BUILD_STATUS                 0x1181
#define CL_PROGRAM_BUILD_LOG                    0x1183
#define CL_KERNEL_WORK_GROUP_SIZE               0x11B0
#define CL_KERNEL_COMPILE_WORK_GROUP_SIZE       0x11B1
#define CL_KERNEL_LOCAL_MEM_SIZE                0x11B2
#define CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE 0x11B3
#define CL_EVENT_COMMAND_EXECUTION_STATUS       0x11D3

#define CL_EXPORT __attribute__((visibility("default")))

#define CDH_CL_MAGIC 0x43444843u /* "CDHC" */

typedef struct {
    uint32_t channel_order;
    uint32_t channel_data_type;
} cdh_cl_image_format;

typedef struct {
    uint32_t image_type;
    size_t image_width;
    size_t image_height;
    size_t image_depth;
    size_t image_array_size;
    size_t image_row_pitch;
    size_t image_slice_pitch;
    uint32_t num_mip_levels;
    uint32_t num_samples;
    void *buffer;
} cdh_cl_image_desc;

typedef struct {
    uint32_t magic;
    uint32_t kind; /* 1=ctx, 2=queue, 3=prog, 4=kernel, 5=mem, 6=sampler, 7=event */
    int32_t refcount;
    void *context;
    void *data;
    size_t size;
    size_t width;
    size_t height;
    size_t depth;
    size_t row_pitch;
    size_t slice_pitch;
    uint64_t flags;
    int owns_data;
} cdh_cl_obj;

static uint32_t g_cdh_platform = CDH_CL_MAGIC;
static uint32_t g_cdh_device = CDH_CL_MAGIC;

static const char kCdhClExtensions[] =
    "cl_khr_global_int32_base_atomics cl_khr_global_int32_extended_atomics "
    "cl_khr_local_int32_base_atomics cl_khr_local_int32_extended_atomics "
    "cl_khr_byte_addressable_store cl_khr_fp16 cl_khr_image2d_from_buffer "
    "cl_khr_priority_hints cl_arm_import_memory_host "
    "cl_arm_non_uniform_work_group_size "
    "cl_arm_integer_dot_product_accumulate_int8";

static int cdh_is_motion_blur_thread(void) {
    char name[32];
    memset(name, 0, sizeof(name));
    if (pthread_getname_np(pthread_self(), name, sizeof(name)) != 0) {
        return 0;
    }
    return strncmp(name, "MotionBlur", 10) == 0;
}

static int cdh_is_obj(const void *p) {
    if (!p) return 0;
    const cdh_cl_obj *o = (const cdh_cl_obj *)p;
    return o->magic == CDH_CL_MAGIC;
}

static cdh_cl_obj *cdh_obj_new(uint32_t kind, void *ctx) {
    cdh_cl_obj *o = (cdh_cl_obj *)calloc(1, sizeof(cdh_cl_obj));
    if (!o) return NULL;
    o->magic = CDH_CL_MAGIC;
    o->kind = kind;
    o->refcount = 1;
    o->context = ctx;
    return o;
}

static int32_t cdh_obj_retain(void *p) {
    if (!cdh_is_obj(p)) return CL_INVALID_VALUE;
    cdh_cl_obj *o = (cdh_cl_obj *)p;
    __atomic_add_fetch(&o->refcount, 1, __ATOMIC_RELAXED);
    return CL_SUCCESS;
}

static int32_t cdh_obj_release(void *p) {
    if (!cdh_is_obj(p)) return CL_INVALID_VALUE;
    cdh_cl_obj *o = (cdh_cl_obj *)p;
    if (__atomic_sub_fetch(&o->refcount, 1, __ATOMIC_ACQ_REL) <= 0) {
        if (o->owns_data && o->data) {
            free(o->data);
            o->data = NULL;
        }
        o->magic = 0;
        free(o);
    }
    return CL_SUCCESS;
}

static int32_t cdh_copy_info(const void *src, size_t src_len,
                             size_t param_value_size, void *param_value,
                             size_t *param_value_size_ret) {
    if (param_value_size_ret) {
        *param_value_size_ret = src_len;
    }
    if (param_value) {
        if (param_value_size < src_len) {
            return CL_INVALID_VALUE;
        }
        if (src_len > 0) {
            memcpy(param_value, src, src_len);
        }
    }
    return CL_SUCCESS;
}

static int32_t cdh_copy_str(const char *s, size_t param_value_size,
                            void *param_value, size_t *param_value_size_ret) {
    return cdh_copy_info(s, strlen(s) + 1, param_value_size, param_value,
                         param_value_size_ret);
}

static size_t cdh_pixel_bytes(const cdh_cl_image_format *fmt) {
    size_t ch = 4;
    size_t comp = 4;
    if (!fmt) return 16;
    switch (fmt->channel_order) {
        case 0x10B0: case 0x10B1: case 0x10B8: case 0x10B9: case 0x10BB: ch = 1; break;
        case 0x10B2: case 0x10B3: case 0x10BA: ch = 2; break;
        case 0x10B4: ch = 3; break;
        default: ch = 4; break;
    }
    switch (fmt->channel_data_type) {
        case 0x10D0: case 0x10D2: case 0x10D7: case 0x10DA: comp = 1; break;
        case 0x10D1: case 0x10D3: case 0x10D8: case 0x10DB: case 0x10DD: comp = 2; break;
        default: comp = 4; break;
    }
    return ch * comp;
}

static void cdh_set_event(void **event, void *ctx) {
    if (event) {
        *event = cdh_obj_new(7, ctx);
    }
}

CL_EXPORT int32_t clGetPlatformIDs(uint32_t num_entries, void **platforms, uint32_t *num_platforms) {
    if (!cdh_is_motion_blur_thread()) {
        if (num_platforms) *num_platforms = 0;
        return CL_PLATFORM_NOT_FOUND_KHR;
    }
    if (num_platforms) *num_platforms = 1;
    if (platforms && num_entries >= 1) {
        platforms[0] = &g_cdh_platform;
    }
    return CL_SUCCESS;
}

CL_EXPORT int32_t clIcdGetPlatformIDsKHR(uint32_t num_entries, void **platforms, uint32_t *num_platforms) {
    return clGetPlatformIDs(num_entries, platforms, num_platforms);
}

CL_EXPORT int32_t clGetPlatformInfo(void *platform, uint32_t param_name,
                                    size_t param_value_size, void *param_value,
                                    size_t *param_value_size_ret) {
    if (platform != &g_cdh_platform) return CL_DEVICE_NOT_FOUND;
    switch (param_name) {
        case CL_PLATFORM_PROFILE:
            return cdh_copy_str("FULL_PROFILE", param_value_size, param_value, param_value_size_ret);
        case CL_PLATFORM_VERSION:
            return cdh_copy_str("OpenCL 3.0 v1.r38p1-01eac0", param_value_size, param_value, param_value_size_ret);
        case CL_PLATFORM_NAME:
            return cdh_copy_str("ARM Platform", param_value_size, param_value, param_value_size_ret);
        case CL_PLATFORM_VENDOR:
            return cdh_copy_str("ARM", param_value_size, param_value, param_value_size_ret);
        case CL_PLATFORM_EXTENSIONS:
            return cdh_copy_str(kCdhClExtensions, param_value_size, param_value, param_value_size_ret);
        case CL_PLATFORM_NUMERIC_VERSION: {
            uint32_t v = (3u << 22);
            return cdh_copy_info(&v, sizeof(v), param_value_size, param_value, param_value_size_ret);
        }
        default: {
            uint64_t zero = 0;
            size_t n = (param_value && param_value_size < sizeof(zero)) ? param_value_size : sizeof(uint32_t);
            return cdh_copy_info(&zero, n, param_value_size, param_value, param_value_size_ret);
        }
    }
}

CL_EXPORT int32_t clGetDeviceIDs(void *platform, uint64_t device_type, uint32_t num_entries,
                                 void **devices, uint32_t *num_devices) {
    (void)device_type;
    if (platform != &g_cdh_platform && !cdh_is_motion_blur_thread()) {
        if (num_devices) *num_devices = 0;
        return CL_DEVICE_NOT_FOUND;
    }
    if (num_devices) *num_devices = 1;
    if (devices && num_entries >= 1) {
        devices[0] = &g_cdh_device;
    }
    return CL_SUCCESS;
}

CL_EXPORT int32_t clGetDeviceInfo(void *device, uint32_t param_name,
                                  size_t param_value_size, void *param_value,
                                  size_t *param_value_size_ret) {
    if (device != &g_cdh_device) return CL_DEVICE_NOT_FOUND;
    switch (param_name) {
        case CL_DEVICE_TYPE: {
            uint64_t v = CL_DEVICE_TYPE_GPU;
            return cdh_copy_info(&v, sizeof(v), param_value_size, param_value, param_value_size_ret);
        }
        case CL_DEVICE_VENDOR_ID: {
            uint32_t v = 0x13B5u;
            return cdh_copy_info(&v, sizeof(v), param_value_size, param_value, param_value_size_ret);
        }
        case CL_DEVICE_MAX_COMPUTE_UNITS: {
            uint32_t v = 20;
            return cdh_copy_info(&v, sizeof(v), param_value_size, param_value, param_value_size_ret);
        }
        case CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS: {
            uint32_t v = 3;
            return cdh_copy_info(&v, sizeof(v), param_value_size, param_value, param_value_size_ret);
        }
        case CL_DEVICE_MAX_WORK_GROUP_SIZE: {
            size_t v = 512;
            return cdh_copy_info(&v, sizeof(v), param_value_size, param_value, param_value_size_ret);
        }
        case CL_DEVICE_MAX_WORK_ITEM_SIZES: {
            size_t v[3] = {512, 512, 512};
            return cdh_copy_info(v, sizeof(v), param_value_size, param_value, param_value_size_ret);
        }
        case CL_DEVICE_MAX_CLOCK_FREQUENCY: {
            uint32_t v = 848;
            return cdh_copy_info(&v, sizeof(v), param_value_size, param_value, param_value_size_ret);
        }
        case CL_DEVICE_ADDRESS_BITS: {
            uint32_t v = 64;
            return cdh_copy_info(&v, sizeof(v), param_value_size, param_value, param_value_size_ret);
        }
        case CL_DEVICE_MAX_READ_IMAGE_ARGS:
        case CL_DEVICE_MAX_WRITE_IMAGE_ARGS:
        case CL_DEVICE_MAX_READ_WRITE_IMAGE_ARGS: {
            uint32_t v = 128;
            return cdh_copy_info(&v, sizeof(v), param_value_size, param_value, param_value_size_ret);
        }
        case CL_DEVICE_MAX_MEM_ALLOC_SIZE: {
            uint64_t v = 512ULL * 1024ULL * 1024ULL;
            return cdh_copy_info(&v, sizeof(v), param_value_size, param_value, param_value_size_ret);
        }
        case CL_DEVICE_IMAGE2D_MAX_WIDTH:
        case CL_DEVICE_IMAGE2D_MAX_HEIGHT: {
            size_t v = 16384;
            return cdh_copy_info(&v, sizeof(v), param_value_size, param_value, param_value_size_ret);
        }
        case CL_DEVICE_IMAGE3D_MAX_WIDTH:
        case CL_DEVICE_IMAGE3D_MAX_HEIGHT:
        case CL_DEVICE_IMAGE3D_MAX_DEPTH:
        case CL_DEVICE_IMAGE_MAX_ARRAY_SIZE: {
            size_t v = 2048;
            return cdh_copy_info(&v, sizeof(v), param_value_size, param_value, param_value_size_ret);
        }
        case CL_DEVICE_IMAGE_MAX_BUFFER_SIZE: {
            size_t v = 65536;
            return cdh_copy_info(&v, sizeof(v), param_value_size, param_value, param_value_size_ret);
        }
        case CL_DEVICE_IMAGE_SUPPORT:
        case CL_DEVICE_AVAILABLE:
        case CL_DEVICE_COMPILER_AVAILABLE:
        case CL_DEVICE_LINKER_AVAILABLE: {
            uint32_t v = CL_TRUE;
            return cdh_copy_info(&v, sizeof(v), param_value_size, param_value, param_value_size_ret);
        }
        case CL_DEVICE_MAX_PARAMETER_SIZE: {
            size_t v = 1024;
            return cdh_copy_info(&v, sizeof(v), param_value_size, param_value, param_value_size_ret);
        }
        case CL_DEVICE_MAX_SAMPLERS: {
            uint32_t v = 16;
            return cdh_copy_info(&v, sizeof(v), param_value_size, param_value, param_value_size_ret);
        }
        case CL_DEVICE_MEM_BASE_ADDR_ALIGN: {
            uint32_t v = 1024;
            return cdh_copy_info(&v, sizeof(v), param_value_size, param_value, param_value_size_ret);
        }
        case CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE:
        case CL_DEVICE_IMAGE_PITCH_ALIGNMENT:
        case CL_DEVICE_IMAGE_BASE_ADDRESS_ALIGNMENT:
        case CL_DEVICE_IMAGE_SLICE_PITCH_ALIGNMENT: {
            uint32_t v = 64;
            return cdh_copy_info(&v, sizeof(v), param_value_size, param_value, param_value_size_ret);
        }
        case CL_DEVICE_GLOBAL_MEM_CACHE_SIZE: {
            uint64_t v = 512ULL * 1024ULL;
            return cdh_copy_info(&v, sizeof(v), param_value_size, param_value, param_value_size_ret);
        }
        case CL_DEVICE_GLOBAL_MEM_SIZE: {
            uint64_t v = 2ULL * 1024ULL * 1024ULL * 1024ULL;
            return cdh_copy_info(&v, sizeof(v), param_value_size, param_value, param_value_size_ret);
        }
        case CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE: {
            uint64_t v = 65536ULL;
            return cdh_copy_info(&v, sizeof(v), param_value_size, param_value, param_value_size_ret);
        }
        case CL_DEVICE_MAX_CONSTANT_ARGS: {
            uint32_t v = 8;
            return cdh_copy_info(&v, sizeof(v), param_value_size, param_value, param_value_size_ret);
        }
        case CL_DEVICE_LOCAL_MEM_TYPE: {
            uint32_t v = 1; /* CL_LOCAL */
            return cdh_copy_info(&v, sizeof(v), param_value_size, param_value, param_value_size_ret);
        }
        case CL_DEVICE_LOCAL_MEM_SIZE: {
            uint64_t v = 32768ULL;
            return cdh_copy_info(&v, sizeof(v), param_value_size, param_value, param_value_size_ret);
        }
        case CL_DEVICE_QUEUE_PROPERTIES: {
            uint64_t v = 2ULL; /* CL_QUEUE_PROFILING_ENABLE */
            return cdh_copy_info(&v, sizeof(v), param_value_size, param_value, param_value_size_ret);
        }
        case CL_DEVICE_PLATFORM: {
            void *p = &g_cdh_platform;
            return cdh_copy_info(&p, sizeof(p), param_value_size, param_value, param_value_size_ret);
        }
        case CL_DEVICE_NAME:
            return cdh_copy_str("Mali-G78", param_value_size, param_value, param_value_size_ret);
        case CL_DEVICE_VENDOR:
            return cdh_copy_str("ARM", param_value_size, param_value, param_value_size_ret);
        case CL_DRIVER_VERSION:
            return cdh_copy_str("38.1", param_value_size, param_value, param_value_size_ret);
        case CL_DEVICE_PROFILE:
            return cdh_copy_str("FULL_PROFILE", param_value_size, param_value, param_value_size_ret);
        case CL_DEVICE_VERSION:
            return cdh_copy_str("OpenCL 3.0 v1.r38p1-01eac0", param_value_size, param_value, param_value_size_ret);
        case CL_DEVICE_OPENCL_C_VERSION:
            return cdh_copy_str("OpenCL C 3.0 ", param_value_size, param_value, param_value_size_ret);
        case CL_DEVICE_EXTENSIONS:
            return cdh_copy_str(kCdhClExtensions, param_value_size, param_value, param_value_size_ret);
        case CL_DEVICE_NUMERIC_VERSION: {
            uint32_t v = (3u << 22);
            return cdh_copy_info(&v, sizeof(v), param_value_size, param_value, param_value_size_ret);
        }
        case CL_DEVICE_EXTENSIONS_WITH_VERSION:
        case CL_DEVICE_OPENCL_C_ALL_VERSIONS:
        case CL_DEVICE_OPENCL_C_FEATURES: {
            if (param_value_size_ret) *param_value_size_ret = 0;
            return CL_SUCCESS;
        }
        default: {
            uint64_t zero = 0;
            size_t n = (param_value && param_value_size <= sizeof(zero)) ? param_value_size : sizeof(uint32_t);
            return cdh_copy_info(&zero, n, param_value_size, param_value, param_value_size_ret);
        }
    }
}

CL_EXPORT void *clCreateContext(const intptr_t *properties, uint32_t num_devices,
                                const void **devices, void *pfn_notify, void *user_data,
                                int32_t *errcode_ret) {
    (void)properties; (void)pfn_notify; (void)user_data;
    if (!cdh_is_motion_blur_thread() && !(num_devices > 0 && devices && devices[0] == &g_cdh_device)) {
        if (errcode_ret) *errcode_ret = CL_DEVICE_NOT_FOUND;
        return NULL;
    }
    cdh_cl_obj *ctx = cdh_obj_new(1, NULL);
    if (errcode_ret) *errcode_ret = ctx ? CL_SUCCESS : CL_DEVICE_NOT_FOUND;
    return ctx;
}

CL_EXPORT void *clCreateContextFromType(const intptr_t *properties, uint64_t device_type,
                                        void *pfn_notify, void *user_data, int32_t *errcode_ret) {
    (void)properties; (void)device_type; (void)pfn_notify; (void)user_data;
    if (!cdh_is_motion_blur_thread()) {
        if (errcode_ret) *errcode_ret = CL_DEVICE_NOT_FOUND;
        return NULL;
    }
    cdh_cl_obj *ctx = cdh_obj_new(1, NULL);
    if (errcode_ret) *errcode_ret = ctx ? CL_SUCCESS : CL_DEVICE_NOT_FOUND;
    return ctx;
}

CL_EXPORT int32_t clGetContextInfo(void *context, uint32_t param_name,
                                   size_t param_value_size, void *param_value,
                                   size_t *param_value_size_ret) {
    if (!cdh_is_obj(context)) return CL_INVALID_CONTEXT;
    cdh_cl_obj *o = (cdh_cl_obj *)context;
    switch (param_name) {
        case CL_CONTEXT_REFERENCE_COUNT:
            return cdh_copy_info(&o->refcount, sizeof(o->refcount), param_value_size, param_value, param_value_size_ret);
        case CL_CONTEXT_NUM_DEVICES: {
            uint32_t n = 1;
            return cdh_copy_info(&n, sizeof(n), param_value_size, param_value, param_value_size_ret);
        }
        case CL_CONTEXT_DEVICES: {
            void *d = &g_cdh_device;
            return cdh_copy_info(&d, sizeof(d), param_value_size, param_value, param_value_size_ret);
        }
        default: {
            uint32_t z = 0;
            return cdh_copy_info(&z, sizeof(z), param_value_size, param_value, param_value_size_ret);
        }
    }
}

CL_EXPORT void *clCreateCommandQueue(void *context, void *device, uint64_t properties,
                                     int32_t *errcode_ret) {
    (void)device; (void)properties;
    if (!cdh_is_obj(context)) {
        if (errcode_ret) *errcode_ret = CL_INVALID_CONTEXT;
        return NULL;
    }
    cdh_cl_obj *q = cdh_obj_new(2, context);
    if (errcode_ret) *errcode_ret = q ? CL_SUCCESS : CL_DEVICE_NOT_FOUND;
    return q;
}

CL_EXPORT void *clCreateCommandQueueWithProperties(void *context, void *device,
                                                   const uint64_t *properties,
                                                   int32_t *errcode_ret) {
    (void)properties;
    return clCreateCommandQueue(context, device, 0, errcode_ret);
}

CL_EXPORT int32_t clGetCommandQueueInfo(void *queue, uint32_t param_name,
                                        size_t param_value_size, void *param_value,
                                        size_t *param_value_size_ret) {
    if (!cdh_is_obj(queue)) return CL_INVALID_VALUE;
    cdh_cl_obj *q = (cdh_cl_obj *)queue;
    switch (param_name) {
        case CL_QUEUE_CONTEXT:
            return cdh_copy_info(&q->context, sizeof(q->context), param_value_size, param_value, param_value_size_ret);
        case CL_QUEUE_DEVICE: {
            void *d = &g_cdh_device;
            return cdh_copy_info(&d, sizeof(d), param_value_size, param_value, param_value_size_ret);
        }
        case CL_QUEUE_REFERENCE_COUNT:
            return cdh_copy_info(&q->refcount, sizeof(q->refcount), param_value_size, param_value, param_value_size_ret);
        default: {
            uint64_t z = 0;
            size_t n = (param_value && param_value_size <= sizeof(z)) ? param_value_size : sizeof(uint32_t);
            return cdh_copy_info(&z, n, param_value_size, param_value, param_value_size_ret);
        }
    }
}

CL_EXPORT void *clCreateBuffer(void *context, uint64_t flags, size_t size,
                               void *host_ptr, int32_t *errcode_ret) {
    if (!cdh_is_obj(context)) {
        if (errcode_ret) *errcode_ret = CL_INVALID_CONTEXT;
        return NULL;
    }
    cdh_cl_obj *m = cdh_obj_new(5, context);
    if (!m) {
        if (errcode_ret) *errcode_ret = CL_DEVICE_NOT_FOUND;
        return NULL;
    }
    size_t alloc_size = size > 0 ? size : 16;
    m->size = size;
    m->flags = flags;
    if ((flags & CL_MEM_USE_HOST_PTR) && host_ptr) {
        m->data = host_ptr;
        m->owns_data = 0;
    } else {
        m->data = calloc(1, alloc_size);
        m->owns_data = 1;
        if (m->data && (flags & CL_MEM_COPY_HOST_PTR) && host_ptr && size > 0) {
            memcpy(m->data, host_ptr, size);
        }
    }
    if (errcode_ret) *errcode_ret = m->data ? CL_SUCCESS : CL_DEVICE_NOT_FOUND;
    if (!m->data) {
        cdh_obj_release(m);
        return NULL;
    }
    return m;
}

CL_EXPORT void *clCreateSubBuffer(void *buffer, uint64_t flags, uint32_t create_type,
                                  const void *create_info, int32_t *errcode_ret) {
    (void)create_type;
    if (!cdh_is_obj(buffer)) {
        if (errcode_ret) *errcode_ret = CL_INVALID_MEM_OBJECT;
        return NULL;
    }
    cdh_cl_obj *parent = (cdh_cl_obj *)buffer;
    const size_t *region = (const size_t *)create_info;
    size_t origin = region ? region[0] : 0;
    size_t size = region ? region[1] : parent->size;
    if (origin + size > parent->size) {
        size = parent->size > origin ? parent->size - origin : 0;
    }
    cdh_cl_obj *m = cdh_obj_new(5, parent->context);
    if (!m) {
        if (errcode_ret) *errcode_ret = CL_DEVICE_NOT_FOUND;
        return NULL;
    }
    m->size = size;
    m->flags = flags;
    m->data = parent->data ? ((uint8_t *)parent->data + origin) : NULL;
    m->owns_data = 0;
    if (errcode_ret) *errcode_ret = CL_SUCCESS;
    return m;
}

CL_EXPORT void *clCreateImage(void *context, uint64_t flags,
                              const cdh_cl_image_format *image_format,
                              const cdh_cl_image_desc *image_desc,
                              void *host_ptr, int32_t *errcode_ret) {
    if (!cdh_is_obj(context)) {
        if (errcode_ret) *errcode_ret = CL_INVALID_CONTEXT;
        return NULL;
    }
    size_t w = (image_desc && image_desc->image_width) ? image_desc->image_width : 1;
    size_t h = (image_desc && image_desc->image_height) ? image_desc->image_height : 1;
    size_t d = (image_desc && image_desc->image_depth) ? image_desc->image_depth : 1;
    size_t bpp = cdh_pixel_bytes(image_format);
    size_t row_pitch = (image_desc && image_desc->image_row_pitch) ? image_desc->image_row_pitch : (w * bpp);
    size_t slice_pitch = (image_desc && image_desc->image_slice_pitch) ? image_desc->image_slice_pitch : (row_pitch * h);
    size_t total = slice_pitch * d;
    if (total == 0) total = 16;

    cdh_cl_obj *m = cdh_obj_new(5, context);
    if (!m) {
        if (errcode_ret) *errcode_ret = CL_DEVICE_NOT_FOUND;
        return NULL;
    }
    m->width = w;
    m->height = h;
    m->depth = d;
    m->row_pitch = row_pitch;
    m->slice_pitch = slice_pitch;
    m->size = total;
    m->flags = flags;
    if (image_desc && cdh_is_obj(image_desc->buffer)) {
        cdh_cl_obj *buf = (cdh_cl_obj *)image_desc->buffer;
        m->data = buf->data;
        m->owns_data = 0;
    } else if ((flags & CL_MEM_USE_HOST_PTR) && host_ptr) {
        m->data = host_ptr;
        m->owns_data = 0;
    } else {
        m->data = calloc(1, total);
        m->owns_data = 1;
        if (m->data && (flags & CL_MEM_COPY_HOST_PTR) && host_ptr) {
            memcpy(m->data, host_ptr, total);
        }
    }
    if (errcode_ret) *errcode_ret = m->data ? CL_SUCCESS : CL_DEVICE_NOT_FOUND;
    if (!m->data) {
        cdh_obj_release(m);
        return NULL;
    }
    return m;
}

CL_EXPORT void *clCreateImage2D(void *context, uint64_t flags,
                                const cdh_cl_image_format *image_format,
                                size_t image_width, size_t image_height,
                                size_t image_row_pitch, void *host_ptr,
                                int32_t *errcode_ret) {
    cdh_cl_image_desc desc;
    memset(&desc, 0, sizeof(desc));
    desc.image_width = image_width;
    desc.image_height = image_height;
    desc.image_depth = 1;
    desc.image_row_pitch = image_row_pitch;
    return clCreateImage(context, flags, image_format, &desc, host_ptr, errcode_ret);
}

CL_EXPORT void *clCreateImage3D(void *context, uint64_t flags,
                                const cdh_cl_image_format *image_format,
                                size_t image_width, size_t image_height, size_t image_depth,
                                size_t image_row_pitch, size_t image_slice_pitch,
                                void *host_ptr, int32_t *errcode_ret) {
    cdh_cl_image_desc desc;
    memset(&desc, 0, sizeof(desc));
    desc.image_width = image_width;
    desc.image_height = image_height;
    desc.image_depth = image_depth;
    desc.image_row_pitch = image_row_pitch;
    desc.image_slice_pitch = image_slice_pitch;
    return clCreateImage(context, flags, image_format, &desc, host_ptr, errcode_ret);
}

CL_EXPORT void *clImportMemoryARM(void *context, uint64_t flags,
                                  const intptr_t *properties, void *memory,
                                  size_t size, int32_t *errcode_ret) {
    (void)properties;
    return clCreateBuffer(context, flags | CL_MEM_USE_HOST_PTR, size, memory, errcode_ret);
}

CL_EXPORT int32_t clGetMemObjectInfo(void *memobj, uint32_t param_name,
                                     size_t param_value_size, void *param_value,
                                     size_t *param_value_size_ret) {
    if (!cdh_is_obj(memobj)) return CL_INVALID_MEM_OBJECT;
    cdh_cl_obj *m = (cdh_cl_obj *)memobj;
    switch (param_name) {
        case CL_MEM_FLAGS:
            return cdh_copy_info(&m->flags, sizeof(m->flags), param_value_size, param_value, param_value_size_ret);
        case CL_MEM_SIZE:
            return cdh_copy_info(&m->size, sizeof(m->size), param_value_size, param_value, param_value_size_ret);
        case CL_MEM_HOST_PTR:
            return cdh_copy_info(&m->data, sizeof(m->data), param_value_size, param_value, param_value_size_ret);
        case CL_MEM_CONTEXT:
            return cdh_copy_info(&m->context, sizeof(m->context), param_value_size, param_value, param_value_size_ret);
        default: {
            uint32_t z = 0;
            return cdh_copy_info(&z, sizeof(z), param_value_size, param_value, param_value_size_ret);
        }
    }
}

CL_EXPORT int32_t clGetImageInfo(void *image, uint32_t param_name,
                                 size_t param_value_size, void *param_value,
                                 size_t *param_value_size_ret) {
    if (!cdh_is_obj(image)) return CL_INVALID_MEM_OBJECT;
    cdh_cl_obj *m = (cdh_cl_obj *)image;
    switch (param_name) {
        case CL_IMAGE_WIDTH:
            return cdh_copy_info(&m->width, sizeof(m->width), param_value_size, param_value, param_value_size_ret);
        case CL_IMAGE_HEIGHT:
            return cdh_copy_info(&m->height, sizeof(m->height), param_value_size, param_value, param_value_size_ret);
        case CL_IMAGE_DEPTH:
            return cdh_copy_info(&m->depth, sizeof(m->depth), param_value_size, param_value, param_value_size_ret);
        case CL_IMAGE_ROW_PITCH:
            return cdh_copy_info(&m->row_pitch, sizeof(m->row_pitch), param_value_size, param_value, param_value_size_ret);
        case CL_IMAGE_SLICE_PITCH:
            return cdh_copy_info(&m->slice_pitch, sizeof(m->slice_pitch), param_value_size, param_value, param_value_size_ret);
        default: {
            size_t z = 0;
            return cdh_copy_info(&z, sizeof(z), param_value_size, param_value, param_value_size_ret);
        }
    }
}

CL_EXPORT int32_t clGetSupportedImageFormats(void *context, uint64_t flags,
                                             uint32_t image_type, uint32_t num_entries,
                                             cdh_cl_image_format *image_formats,
                                             uint32_t *num_image_formats) {
    (void)flags; (void)image_type;
    if (!cdh_is_obj(context)) return CL_INVALID_CONTEXT;
    static const uint32_t orders[] = {0x10B0, 0x10B2, 0x10B5}; /* R, RG, RGBA */
    static const uint32_t types[] = {0x10D2, 0x10D3, 0x10DA, 0x10DB, 0x10DC, 0x10DD, 0x10DE}; /* UNORM8/16, UINT8/16/32, HALF, FLOAT */
    const uint32_t total = (uint32_t)((sizeof(orders) / sizeof(orders[0])) * (sizeof(types) / sizeof(types[0])));
    if (num_image_formats) *num_image_formats = total;
    if (image_formats && num_entries > 0) {
        uint32_t idx = 0;
        for (size_t i = 0; i < sizeof(orders) / sizeof(orders[0]) && idx < num_entries; i++) {
            for (size_t j = 0; j < sizeof(types) / sizeof(types[0]) && idx < num_entries; j++) {
                image_formats[idx].channel_order = orders[i];
                image_formats[idx].channel_data_type = types[j];
                idx++;
            }
        }
    }
    return CL_SUCCESS;
}

CL_EXPORT void *clCreateProgramWithSource(void *context, uint32_t count,
                                          const char **strings, const size_t *lengths,
                                          int32_t *errcode_ret) {
    (void)count; (void)strings; (void)lengths;
    if (!cdh_is_obj(context)) {
        if (errcode_ret) *errcode_ret = CL_INVALID_CONTEXT;
        return NULL;
    }
    cdh_cl_obj *p = cdh_obj_new(3, context);
    if (errcode_ret) *errcode_ret = p ? CL_SUCCESS : CL_DEVICE_NOT_FOUND;
    return p;
}

CL_EXPORT void *clCreateProgramWithBinary(void *context, uint32_t num_devices,
                                          const void **device_list, const size_t *lengths,
                                          const unsigned char **binaries, int32_t *binary_status,
                                          int32_t *errcode_ret) {
    (void)device_list; (void)lengths; (void)binaries;
    if (!cdh_is_obj(context)) {
        if (errcode_ret) *errcode_ret = CL_INVALID_CONTEXT;
        return NULL;
    }
    if (binary_status) {
        for (uint32_t i = 0; i < num_devices; i++) binary_status[i] = CL_SUCCESS;
    }
    cdh_cl_obj *p = cdh_obj_new(3, context);
    if (errcode_ret) *errcode_ret = p ? CL_SUCCESS : CL_DEVICE_NOT_FOUND;
    return p;
}

CL_EXPORT void *clCreateProgramWithIL(void *context, const void *il, size_t length,
                                      int32_t *errcode_ret) {
    (void)il; (void)length;
    return clCreateProgramWithSource(context, 0, NULL, NULL, errcode_ret);
}

CL_EXPORT void *clLinkProgram(void *context, uint32_t num_devices, const void **device_list,
                              const char *options, uint32_t num_input_programs,
                              const void **input_programs, void *pfn_notify, void *user_data,
                              int32_t *errcode_ret) {
    (void)num_devices; (void)device_list; (void)options; (void)num_input_programs;
    (void)input_programs; (void)pfn_notify; (void)user_data;
    return clCreateProgramWithSource(context, 0, NULL, NULL, errcode_ret);
}

CL_EXPORT int32_t clBuildProgram(void *program, uint32_t num_devices, const void **device_list,
                                 const char *options, void *pfn_notify, void *user_data) {
    (void)num_devices; (void)device_list; (void)options; (void)pfn_notify; (void)user_data;
    return cdh_is_obj(program) ? CL_SUCCESS : CL_INVALID_PROGRAM;
}

CL_EXPORT int32_t clCompileProgram(void *program, uint32_t num_devices, const void **device_list,
                                   const char *options, uint32_t num_input_headers,
                                   const void **input_headers, const char **header_include_names,
                                   void *pfn_notify, void *user_data) {
    (void)num_devices; (void)device_list; (void)options; (void)num_input_headers;
    (void)input_headers; (void)header_include_names; (void)pfn_notify; (void)user_data;
    return cdh_is_obj(program) ? CL_SUCCESS : CL_INVALID_PROGRAM;
}

CL_EXPORT int32_t clGetProgramInfo(void *program, uint32_t param_name,
                                   size_t param_value_size, void *param_value,
                                   size_t *param_value_size_ret) {
    if (!cdh_is_obj(program)) return CL_INVALID_PROGRAM;
    cdh_cl_obj *p = (cdh_cl_obj *)program;
    switch (param_name) {
        case CL_PROGRAM_REFERENCE_COUNT:
            return cdh_copy_info(&p->refcount, sizeof(p->refcount), param_value_size, param_value, param_value_size_ret);
        case CL_PROGRAM_CONTEXT:
            return cdh_copy_info(&p->context, sizeof(p->context), param_value_size, param_value, param_value_size_ret);
        case CL_PROGRAM_NUM_DEVICES: {
            uint32_t n = 1;
            return cdh_copy_info(&n, sizeof(n), param_value_size, param_value, param_value_size_ret);
        }
        case CL_PROGRAM_DEVICES: {
            void *d = &g_cdh_device;
            return cdh_copy_info(&d, sizeof(d), param_value_size, param_value, param_value_size_ret);
        }
        case CL_PROGRAM_BINARY_SIZES: {
            size_t sz = 16;
            return cdh_copy_info(&sz, sizeof(sz), param_value_size, param_value, param_value_size_ret);
        }
        case CL_PROGRAM_BINARIES: {
            if (param_value_size_ret) *param_value_size_ret = sizeof(unsigned char *);
            if (param_value && param_value_size >= sizeof(unsigned char *)) {
                unsigned char **dst = (unsigned char **)param_value;
                if (dst[0]) {
                    memset(dst[0], 0, 16);
                    memcpy(dst[0], "CDHCLBIN", 8);
                }
            }
            return CL_SUCCESS;
        }
        default: {
            uint32_t z = 0;
            return cdh_copy_info(&z, sizeof(z), param_value_size, param_value, param_value_size_ret);
        }
    }
}

CL_EXPORT int32_t clGetProgramBuildInfo(void *program, void *device, uint32_t param_name,
                                        size_t param_value_size, void *param_value,
                                        size_t *param_value_size_ret) {
    (void)device;
    if (!cdh_is_obj(program)) return CL_INVALID_PROGRAM;
    if (param_name == CL_PROGRAM_BUILD_STATUS) {
        int32_t st = 0; /* CL_BUILD_SUCCESS */
        return cdh_copy_info(&st, sizeof(st), param_value_size, param_value, param_value_size_ret);
    }
    if (param_name == CL_PROGRAM_BUILD_LOG) {
        return cdh_copy_str("", param_value_size, param_value, param_value_size_ret);
    }
    uint32_t z = 0;
    return cdh_copy_info(&z, sizeof(z), param_value_size, param_value, param_value_size_ret);
}

CL_EXPORT void *clCreateKernel(void *program, const char *kernel_name, int32_t *errcode_ret) {
    (void)kernel_name;
    if (!cdh_is_obj(program)) {
        if (errcode_ret) *errcode_ret = CL_INVALID_PROGRAM;
        return NULL;
    }
    cdh_cl_obj *p = (cdh_cl_obj *)program;
    cdh_cl_obj *k = cdh_obj_new(4, p->context);
    if (errcode_ret) *errcode_ret = k ? CL_SUCCESS : CL_DEVICE_NOT_FOUND;
    return k;
}

CL_EXPORT void *clCloneKernel(void *source_kernel, int32_t *errcode_ret) {
    if (!cdh_is_obj(source_kernel)) {
        if (errcode_ret) *errcode_ret = CL_INVALID_KERNEL;
        return NULL;
    }
    cdh_cl_obj *sk = (cdh_cl_obj *)source_kernel;
    cdh_cl_obj *k = cdh_obj_new(4, sk->context);
    if (errcode_ret) *errcode_ret = k ? CL_SUCCESS : CL_DEVICE_NOT_FOUND;
    return k;
}

CL_EXPORT int32_t clSetKernelArg(void *kernel, uint32_t arg_index, size_t arg_size,
                                 const void *arg_value) {
    (void)arg_index; (void)arg_size; (void)arg_value;
    return cdh_is_obj(kernel) ? CL_SUCCESS : CL_INVALID_KERNEL;
}

CL_EXPORT int32_t clSetKernelArgSVMPointer(void *kernel, uint32_t arg_index, const void *arg_value) {
    (void)arg_index; (void)arg_value;
    return cdh_is_obj(kernel) ? CL_SUCCESS : CL_INVALID_KERNEL;
}

CL_EXPORT int32_t clSetKernelExecInfo(void *kernel, uint32_t param_name, size_t param_value_size,
                                      const void *param_value) {
    (void)param_name; (void)param_value_size; (void)param_value;
    return cdh_is_obj(kernel) ? CL_SUCCESS : CL_INVALID_KERNEL;
}

CL_EXPORT int32_t clGetKernelInfo(void *kernel, uint32_t param_name,
                                  size_t param_value_size, void *param_value,
                                  size_t *param_value_size_ret) {
    (void)param_name;
    if (!cdh_is_obj(kernel)) return CL_INVALID_KERNEL;
    uint32_t z = 0;
    return cdh_copy_info(&z, sizeof(z), param_value_size, param_value, param_value_size_ret);
}

CL_EXPORT int32_t clGetKernelWorkGroupInfo(void *kernel, void *device, uint32_t param_name,
                                           size_t param_value_size, void *param_value,
                                           size_t *param_value_size_ret) {
    (void)device;
    if (!cdh_is_obj(kernel)) return CL_INVALID_KERNEL;
    switch (param_name) {
        case CL_KERNEL_WORK_GROUP_SIZE: {
            size_t v = 256;
            return cdh_copy_info(&v, sizeof(v), param_value_size, param_value, param_value_size_ret);
        }
        case CL_KERNEL_COMPILE_WORK_GROUP_SIZE: {
            size_t v[3] = {0, 0, 0};
            return cdh_copy_info(v, sizeof(v), param_value_size, param_value, param_value_size_ret);
        }
        case CL_KERNEL_LOCAL_MEM_SIZE: {
            uint64_t v = 0;
            return cdh_copy_info(&v, sizeof(v), param_value_size, param_value, param_value_size_ret);
        }
        case CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE: {
            size_t v = 16;
            return cdh_copy_info(&v, sizeof(v), param_value_size, param_value, param_value_size_ret);
        }
        default: {
            uint64_t z = 0;
            size_t n = (param_value && param_value_size <= sizeof(z)) ? param_value_size : sizeof(size_t);
            return cdh_copy_info(&z, n, param_value_size, param_value, param_value_size_ret);
        }
    }
}

CL_EXPORT int32_t clGetKernelArgInfo(void *kernel, uint32_t arg_indx, uint32_t param_name,
                                     size_t param_value_size, void *param_value,
                                     size_t *param_value_size_ret) {
    (void)arg_indx; (void)param_name;
    if (!cdh_is_obj(kernel)) return CL_INVALID_KERNEL;
    uint32_t z = 0;
    return cdh_copy_info(&z, sizeof(z), param_value_size, param_value, param_value_size_ret);
}

CL_EXPORT void *clCreateSampler(void *context, uint32_t normalized_coords,
                                uint32_t addressing_mode, uint32_t filter_mode,
                                int32_t *errcode_ret) {
    (void)normalized_coords; (void)addressing_mode; (void)filter_mode;
    if (!cdh_is_obj(context)) {
        if (errcode_ret) *errcode_ret = CL_INVALID_CONTEXT;
        return NULL;
    }
    cdh_cl_obj *s = cdh_obj_new(6, context);
    if (errcode_ret) *errcode_ret = s ? CL_SUCCESS : CL_DEVICE_NOT_FOUND;
    return s;
}

CL_EXPORT void *clCreateSamplerWithProperties(void *context, const uint64_t *sampler_properties,
                                              int32_t *errcode_ret) {
    (void)sampler_properties;
    return clCreateSampler(context, 0, 0, 0, errcode_ret);
}

CL_EXPORT int32_t clGetSamplerInfo(void *sampler, uint32_t param_name,
                                   size_t param_value_size, void *param_value,
                                   size_t *param_value_size_ret) {
    (void)param_name;
    if (!cdh_is_obj(sampler)) return CL_INVALID_VALUE;
    uint32_t z = 0;
    return cdh_copy_info(&z, sizeof(z), param_value_size, param_value, param_value_size_ret);
}

CL_EXPORT void *clCreateUserEvent(void *context, int32_t *errcode_ret) {
    if (!cdh_is_obj(context)) {
        if (errcode_ret) *errcode_ret = CL_INVALID_CONTEXT;
        return NULL;
    }
    cdh_cl_obj *ev = cdh_obj_new(7, context);
    if (errcode_ret) *errcode_ret = ev ? CL_SUCCESS : CL_DEVICE_NOT_FOUND;
    return ev;
}

CL_EXPORT int32_t clGetEventInfo(void *event, uint32_t param_name,
                                 size_t param_value_size, void *param_value,
                                 size_t *param_value_size_ret) {
    if (!cdh_is_obj(event)) return CL_INVALID_VALUE;
    if (param_name == CL_EVENT_COMMAND_EXECUTION_STATUS) {
        int32_t st = 0; /* CL_COMPLETE */
        return cdh_copy_info(&st, sizeof(st), param_value_size, param_value, param_value_size_ret);
    }
    uint32_t z = 0;
    return cdh_copy_info(&z, sizeof(z), param_value_size, param_value, param_value_size_ret);
}

CL_EXPORT int32_t clGetEventProfilingInfo(void *event, uint32_t param_name,
                                          size_t param_value_size, void *param_value,
                                          size_t *param_value_size_ret) {
    (void)param_name;
    if (!cdh_is_obj(event)) return CL_INVALID_VALUE;
    uint64_t z = 0;
    return cdh_copy_info(&z, sizeof(z), param_value_size, param_value, param_value_size_ret);
}

CL_EXPORT int32_t clEnqueueNDRangeKernel(void *queue, void *kernel, uint32_t work_dim,
                                         const size_t *global_work_offset,
                                         const size_t *global_work_size,
                                         const size_t *local_work_size,
                                         uint32_t num_events, const void **event_wait_list,
                                         void **event) {
    (void)work_dim; (void)global_work_offset; (void)global_work_size;
    (void)local_work_size; (void)num_events; (void)event_wait_list;
    if (!cdh_is_obj(queue) || !cdh_is_obj(kernel)) return CL_INVALID_VALUE;
    cdh_set_event(event, ((cdh_cl_obj *)queue)->context);
    return CL_SUCCESS;
}

CL_EXPORT int32_t clEnqueueReadBuffer(void *queue, void *buffer, uint32_t blocking_read,
                                      size_t offset, size_t size, void *ptr,
                                      uint32_t num_events, const void **wait_list,
                                      void **event) {
    (void)blocking_read; (void)num_events; (void)wait_list;
    if (!cdh_is_obj(queue) || !cdh_is_obj(buffer)) return CL_INVALID_VALUE;
    cdh_cl_obj *m = (cdh_cl_obj *)buffer;
    if (ptr && m->data && offset < m->size) {
        size_t n = (offset + size <= m->size) ? size : (m->size - offset);
        memcpy(ptr, (const uint8_t *)m->data + offset, n);
    }
    cdh_set_event(event, ((cdh_cl_obj *)queue)->context);
    return CL_SUCCESS;
}

CL_EXPORT int32_t clEnqueueWriteBuffer(void *queue, void *buffer, uint32_t blocking_write,
                                       size_t offset, size_t size, const void *ptr,
                                       uint32_t num_events, const void **wait_list,
                                       void **event) {
    (void)blocking_write; (void)num_events; (void)wait_list;
    if (!cdh_is_obj(queue) || !cdh_is_obj(buffer)) return CL_INVALID_VALUE;
    cdh_cl_obj *m = (cdh_cl_obj *)buffer;
    if (ptr && m->data && offset < m->size) {
        size_t n = (offset + size <= m->size) ? size : (m->size - offset);
        memcpy((uint8_t *)m->data + offset, ptr, n);
    }
    cdh_set_event(event, ((cdh_cl_obj *)queue)->context);
    return CL_SUCCESS;
}

CL_EXPORT int32_t clEnqueueCopyBuffer(void *queue, void *src_buffer, void *dst_buffer,
                                      size_t src_offset, size_t dst_offset, size_t size,
                                      uint32_t num_events, const void **wait_list,
                                      void **event) {
    (void)num_events; (void)wait_list;
    if (!cdh_is_obj(queue) || !cdh_is_obj(src_buffer) || !cdh_is_obj(dst_buffer)) return CL_INVALID_VALUE;
    cdh_cl_obj *s = (cdh_cl_obj *)src_buffer;
    cdh_cl_obj *d = (cdh_cl_obj *)dst_buffer;
    if (s->data && d->data && src_offset < s->size && dst_offset < d->size) {
        size_t n = size;
        if (src_offset + n > s->size) n = s->size - src_offset;
        if (dst_offset + n > d->size) n = d->size - dst_offset;
        memmove((uint8_t *)d->data + dst_offset, (const uint8_t *)s->data + src_offset, n);
    }
    cdh_set_event(event, ((cdh_cl_obj *)queue)->context);
    return CL_SUCCESS;
}

CL_EXPORT int32_t clEnqueueFillBuffer(void *queue, void *buffer, const void *pattern,
                                      size_t pattern_size, size_t offset, size_t size,
                                      uint32_t num_events, const void **wait_list,
                                      void **event) {
    (void)num_events; (void)wait_list;
    if (!cdh_is_obj(queue) || !cdh_is_obj(buffer)) return CL_INVALID_VALUE;
    cdh_cl_obj *m = (cdh_cl_obj *)buffer;
    if (m->data && pattern && pattern_size > 0 && offset < m->size) {
        size_t end = (offset + size <= m->size) ? (offset + size) : m->size;
        for (size_t pos = offset; pos < end; pos += pattern_size) {
            size_t n = (pos + pattern_size <= end) ? pattern_size : (end - pos);
            memcpy((uint8_t *)m->data + pos, pattern, n);
        }
    }
    cdh_set_event(event, ((cdh_cl_obj *)queue)->context);
    return CL_SUCCESS;
}

CL_EXPORT int32_t clEnqueueReadImage(void *queue, void *image, uint32_t blocking_read,
                                     const size_t *origin, const size_t *region,
                                     size_t row_pitch, size_t slice_pitch, void *ptr,
                                     uint32_t num_events, const void **wait_list,
                                     void **event) {
    (void)blocking_read; (void)origin; (void)region; (void)row_pitch; (void)slice_pitch;
    (void)num_events; (void)wait_list;
    if (!cdh_is_obj(queue) || !cdh_is_obj(image)) return CL_INVALID_VALUE;
    cdh_cl_obj *m = (cdh_cl_obj *)image;
    if (ptr && m->data && m->size > 0) {
        size_t copy_bytes = m->size;
        if (region && row_pitch) copy_bytes = row_pitch * region[1];
        if (copy_bytes > m->size) copy_bytes = m->size;
        memcpy(ptr, m->data, copy_bytes);
    }
    cdh_set_event(event, ((cdh_cl_obj *)queue)->context);
    return CL_SUCCESS;
}

CL_EXPORT int32_t clEnqueueWriteImage(void *queue, void *image, uint32_t blocking_write,
                                      const size_t *origin, const size_t *region,
                                      size_t input_row_pitch, size_t input_slice_pitch,
                                      const void *ptr, uint32_t num_events,
                                      const void **wait_list, void **event) {
    (void)blocking_write; (void)origin; (void)region; (void)input_row_pitch; (void)input_slice_pitch;
    (void)num_events; (void)wait_list;
    if (!cdh_is_obj(queue) || !cdh_is_obj(image)) return CL_INVALID_VALUE;
    cdh_cl_obj *m = (cdh_cl_obj *)image;
    if (ptr && m->data && m->size > 0) {
        size_t copy_bytes = m->size;
        if (region && input_row_pitch) copy_bytes = input_row_pitch * region[1];
        if (copy_bytes > m->size) copy_bytes = m->size;
        memcpy(m->data, ptr, copy_bytes);
    }
    cdh_set_event(event, ((cdh_cl_obj *)queue)->context);
    return CL_SUCCESS;
}

CL_EXPORT int32_t clEnqueueCopyImage(void *queue, void *src_image, void *dst_image,
                                     const size_t *src_origin, const size_t *dst_origin,
                                     const size_t *region, uint32_t num_events,
                                     const void **wait_list, void **event) {
    (void)src_origin; (void)dst_origin; (void)region; (void)num_events; (void)wait_list;
    if (!cdh_is_obj(queue) || !cdh_is_obj(src_image) || !cdh_is_obj(dst_image)) return CL_INVALID_VALUE;
    cdh_cl_obj *s = (cdh_cl_obj *)src_image;
    cdh_cl_obj *d = (cdh_cl_obj *)dst_image;
    if (s->data && d->data) {
        size_t n = s->size < d->size ? s->size : d->size;
        memmove(d->data, s->data, n);
    }
    cdh_set_event(event, ((cdh_cl_obj *)queue)->context);
    return CL_SUCCESS;
}

CL_EXPORT void *clEnqueueMapBuffer(void *queue, void *buffer, uint32_t blocking_map,
                                   uint64_t map_flags, size_t offset, size_t size,
                                   uint32_t num_events, const void **wait_list,
                                   void **event, int32_t *errcode_ret) {
    (void)blocking_map; (void)map_flags; (void)size; (void)num_events; (void)wait_list;
    if (!cdh_is_obj(queue) || !cdh_is_obj(buffer)) {
        if (errcode_ret) *errcode_ret = CL_INVALID_VALUE;
        return NULL;
    }
    cdh_cl_obj *m = (cdh_cl_obj *)buffer;
    cdh_set_event(event, ((cdh_cl_obj *)queue)->context);
    if (errcode_ret) *errcode_ret = CL_SUCCESS;
    return m->data ? ((uint8_t *)m->data + (offset < m->size ? offset : 0)) : NULL;
}

CL_EXPORT void *clEnqueueMapImage(void *queue, void *image, uint32_t blocking_map,
                                  uint64_t map_flags, const size_t *origin, const size_t *region,
                                  size_t *image_row_pitch, size_t *image_slice_pitch,
                                  uint32_t num_events, const void **wait_list,
                                  void **event, int32_t *errcode_ret) {
    (void)blocking_map; (void)map_flags; (void)origin; (void)region;
    (void)num_events; (void)wait_list;
    if (!cdh_is_obj(queue) || !cdh_is_obj(image)) {
        if (errcode_ret) *errcode_ret = CL_INVALID_VALUE;
        return NULL;
    }
    cdh_cl_obj *m = (cdh_cl_obj *)image;
    if (image_row_pitch) *image_row_pitch = m->row_pitch;
    if (image_slice_pitch) *image_slice_pitch = m->slice_pitch;
    cdh_set_event(event, ((cdh_cl_obj *)queue)->context);
    if (errcode_ret) *errcode_ret = CL_SUCCESS;
    return m->data;
}

CL_EXPORT int32_t clEnqueueUnmapMemObject(void *queue, void *memobj, void *mapped_ptr,
                                          uint32_t num_events, const void **wait_list,
                                          void **event) {
    (void)mapped_ptr; (void)num_events; (void)wait_list;
    if (!cdh_is_obj(queue) || !cdh_is_obj(memobj)) return CL_INVALID_VALUE;
    cdh_set_event(event, ((cdh_cl_obj *)queue)->context);
    return CL_SUCCESS;
}

CL_EXPORT int32_t clEnqueueBarrierWithWaitList(void *queue, uint32_t num_events,
                                               const void **wait_list, void **event) {
    (void)num_events; (void)wait_list;
    if (!cdh_is_obj(queue)) return CL_INVALID_VALUE;
    cdh_set_event(event, ((cdh_cl_obj *)queue)->context);
    return CL_SUCCESS;
}

CL_EXPORT int32_t clEnqueueMarkerWithWaitList(void *queue, uint32_t num_events,
                                              const void **wait_list, void **event) {
    return clEnqueueBarrierWithWaitList(queue, num_events, wait_list, event);
}

CL_EXPORT int32_t clFlush(void *queue) {
    return cdh_is_obj(queue) ? CL_SUCCESS : CL_INVALID_VALUE;
}

CL_EXPORT int32_t clFinish(void *queue) {
    return cdh_is_obj(queue) ? CL_SUCCESS : CL_INVALID_VALUE;
}

CL_EXPORT int32_t clWaitForEvents(uint32_t num_events, const void **event_list) {
    (void)num_events; (void)event_list;
    return CL_SUCCESS;
}

CL_EXPORT int32_t clReleaseContext(void *ctx)      { return cdh_obj_release(ctx); }
CL_EXPORT int32_t clRetainContext(void *ctx)       { return cdh_obj_retain(ctx); }
CL_EXPORT int32_t clReleaseCommandQueue(void *q)   { return cdh_obj_release(q); }
CL_EXPORT int32_t clRetainCommandQueue(void *q)    { return cdh_obj_retain(q); }
CL_EXPORT int32_t clReleaseMemObject(void *m)      { return cdh_obj_release(m); }
CL_EXPORT int32_t clRetainMemObject(void *m)       { return cdh_obj_retain(m); }
CL_EXPORT int32_t clReleaseProgram(void *p)        { return cdh_obj_release(p); }
CL_EXPORT int32_t clRetainProgram(void *p)         { return cdh_obj_retain(p); }
CL_EXPORT int32_t clReleaseKernel(void *k)         { return cdh_obj_release(k); }
CL_EXPORT int32_t clRetainKernel(void *k)          { return cdh_obj_retain(k); }
CL_EXPORT int32_t clReleaseEvent(void *e)          { return cdh_obj_release(e); }
CL_EXPORT int32_t clRetainEvent(void *e)           { return cdh_obj_retain(e); }
CL_EXPORT int32_t clReleaseSampler(void *s)        { return cdh_obj_release(s); }
CL_EXPORT int32_t clRetainSampler(void *s)         { return cdh_obj_retain(s); }
CL_EXPORT int32_t clReleaseDevice(void *d)         { (void)d; return CL_SUCCESS; }
CL_EXPORT int32_t clRetainDevice(void *d)          { (void)d; return CL_SUCCESS; }
CL_EXPORT int32_t clUnloadCompiler(void)           { return CL_SUCCESS; }
CL_EXPORT int32_t clUnloadPlatformCompiler(void *p){ (void)p; return CL_SUCCESS; }

CL_EXPORT void *clGetExtensionFunctionAddress(const char *func_name) {
    if (func_name && strcmp(func_name, "clImportMemoryARM") == 0) {
        return (void *)(uintptr_t)clImportMemoryARM;
    }
    return NULL;
}

CL_EXPORT void *clGetExtensionFunctionAddressForPlatform(void *platform, const char *func_name) {
    (void)platform;
    return clGetExtensionFunctionAddress(func_name);
}

/* Pixel libOpenCL-pixel.so 专属入口（GoogleCamera LoadOpenCL() 强依赖） */
CL_EXPORT void enableOpenCL(void) {
}

CL_EXPORT void *loadOpenCLPointer(const char *func_name) {
    if (!func_name) return NULL;
    static void *self_handle = NULL;
    if (!self_handle) {
        Dl_info info;
        if (dladdr((void *)(uintptr_t)loadOpenCLPointer, &info) && info.dli_fname) {
            self_handle = dlopen(info.dli_fname, RTLD_NOLOAD | RTLD_LAZY);
        }
    }
    void *sym = self_handle ? dlsym(self_handle, func_name) : NULL;
    if (!sym) {
        sym = dlsym(RTLD_DEFAULT, func_name);
    }
    return sym;
}

/* 补齐 OpenCL 2.0/3.0 与扩展导出符号，避免 cl_loader 解析为 NULL */
CL_EXPORT int32_t clCreateSubDevices(void *in_device, const intptr_t *properties,
                                     uint32_t num_devices, void **out_devices,
                                     uint32_t *num_devices_ret) {
    (void)in_device; (void)properties; (void)num_devices; (void)out_devices;
    if (num_devices_ret) *num_devices_ret = 0;
    return CL_DEVICE_NOT_FOUND;
}
CL_EXPORT int32_t clSetDefaultDeviceCommandQueue(void *ctx, void *dev, void *q) {
    (void)ctx; (void)dev; (void)q;
    return CL_SUCCESS;
}
CL_EXPORT int32_t clGetDeviceAndHostTimer(void *dev, uint64_t *dev_ts, uint64_t *host_ts) {
    (void)dev;
    if (dev_ts) *dev_ts = 0;
    if (host_ts) *host_ts = 0;
    return CL_SUCCESS;
}
CL_EXPORT int32_t clGetHostTimer(void *dev, uint64_t *host_ts) {
    (void)dev;
    if (host_ts) *host_ts = 0;
    return CL_SUCCESS;
}
CL_EXPORT void *clCreatePipe(void *ctx, uint64_t flags, uint32_t pkt_sz, uint32_t max_pkts,
                             const intptr_t *props, int32_t *errcode_ret) {
    (void)flags; (void)pkt_sz; (void)max_pkts; (void)props;
    return clCreateBuffer(ctx, 0, (size_t)pkt_sz * (size_t)max_pkts, NULL, errcode_ret);
}
CL_EXPORT int32_t clGetPipeInfo(void *pipe, uint32_t param_name, size_t sz, void *val, size_t *ret) {
    (void)pipe; (void)param_name;
    uint32_t z = 0;
    return cdh_copy_info(&z, sizeof(z), sz, val, ret);
}
CL_EXPORT int32_t clSetMemObjectDestructorCallback(void *mem, void *cb, void *user_data) {
    (void)mem; (void)cb; (void)user_data;
    return CL_SUCCESS;
}
CL_EXPORT void *clSVMAlloc(void *ctx, uint64_t flags, size_t size, uint32_t alignment) {
    (void)ctx; (void)flags; (void)alignment;
    return calloc(1, size > 0 ? size : 16);
}
CL_EXPORT void clSVMFree(void *ctx, void *svm_ptr) {
    (void)ctx;
    free(svm_ptr);
}
CL_EXPORT void *clCreateProgramWithBuiltInKernels(void *ctx, uint32_t num_dev, const void **devs,
                                                  const char *kernel_names, int32_t *errcode_ret) {
    (void)num_dev; (void)devs; (void)kernel_names;
    return clCreateProgramWithSource(ctx, 0, NULL, NULL, errcode_ret);
}
CL_EXPORT int32_t clCreateKernelsInProgram(void *prog, uint32_t num_kernels, void **kernels,
                                           uint32_t *num_kernels_ret) {
    (void)prog; (void)num_kernels; (void)kernels;
    if (num_kernels_ret) *num_kernels_ret = 0;
    return CL_SUCCESS;
}
CL_EXPORT int32_t clGetKernelSubGroupInfo(void *kernel, void *dev, uint32_t param_name,
                                          size_t in_sz, const void *in_val,
                                          size_t sz, void *val, size_t *ret) {
    (void)kernel; (void)dev; (void)param_name; (void)in_sz; (void)in_val;
    size_t v = 16;
    return cdh_copy_info(&v, sizeof(v), sz, val, ret);
}
CL_EXPORT int32_t clSetUserEventStatus(void *event, int32_t status) {
    (void)event; (void)status;
    return CL_SUCCESS;
}
CL_EXPORT int32_t clSetEventCallback(void *event, int32_t type, void *cb, void *user_data) {
    (void)event; (void)type; (void)cb; (void)user_data;
    return CL_SUCCESS;
}
CL_EXPORT int32_t clEnqueueReadBufferRect(void *q, void *buf, uint32_t blk,
                                          const size_t *bo, const size_t *ho, const size_t *reg,
                                          size_t brp, size_t bsp, size_t hrp, size_t hsp,
                                          void *ptr, uint32_t nev, const void **wl, void **ev) {
    (void)buf; (void)blk; (void)bo; (void)ho; (void)reg; (void)brp; (void)bsp;
    (void)hrp; (void)hsp; (void)ptr; (void)nev; (void)wl;
    if (!cdh_is_obj(q)) return CL_INVALID_VALUE;
    cdh_set_event(ev, ((cdh_cl_obj *)q)->context);
    return CL_SUCCESS;
}
CL_EXPORT int32_t clEnqueueWriteBufferRect(void *q, void *buf, uint32_t blk,
                                           const size_t *bo, const size_t *ho, const size_t *reg,
                                           size_t brp, size_t bsp, size_t hrp, size_t hsp,
                                           const void *ptr, uint32_t nev, const void **wl, void **ev) {
    return clEnqueueReadBufferRect(q, buf, blk, bo, ho, reg, brp, bsp, hrp, hsp,
                                   (void *)(uintptr_t)ptr, nev, wl, ev);
}
CL_EXPORT int32_t clEnqueueCopyBufferRect(void *q, void *sb, void *db,
                                          const size_t *so, const size_t *doff, const size_t *reg,
                                          size_t srp, size_t ssp, size_t drp, size_t dsp,
                                          uint32_t nev, const void **wl, void **ev) {
    (void)sb; (void)db; (void)so; (void)doff; (void)reg; (void)srp; (void)ssp;
    (void)drp; (void)dsp; (void)nev; (void)wl;
    if (!cdh_is_obj(q)) return CL_INVALID_VALUE;
    cdh_set_event(ev, ((cdh_cl_obj *)q)->context);
    return CL_SUCCESS;
}
CL_EXPORT int32_t clEnqueueFillImage(void *q, void *img, const void *color,
                                     const size_t *origin, const size_t *region,
                                     uint32_t nev, const void **wl, void **ev) {
    (void)img; (void)color; (void)origin; (void)region; (void)nev; (void)wl;
    if (!cdh_is_obj(q)) return CL_INVALID_VALUE;
    cdh_set_event(ev, ((cdh_cl_obj *)q)->context);
    return CL_SUCCESS;
}
CL_EXPORT int32_t clEnqueueCopyImageToBuffer(void *q, void *src_img, void *dst_buf,
                                             const size_t *so, const size_t *reg, size_t dst_off,
                                             uint32_t nev, const void **wl, void **ev) {
    (void)so; (void)reg; (void)nev; (void)wl;
    if (!cdh_is_obj(q) || !cdh_is_obj(src_img) || !cdh_is_obj(dst_buf)) return CL_INVALID_VALUE;
    cdh_cl_obj *s = (cdh_cl_obj *)src_img;
    cdh_cl_obj *d = (cdh_cl_obj *)dst_buf;
    if (s->data && d->data && dst_off < d->size) {
        size_t n = d->size - dst_off;
        if (s->size < n) n = s->size;
        memcpy((uint8_t *)d->data + dst_off, s->data, n);
    }
    cdh_set_event(ev, ((cdh_cl_obj *)q)->context);
    return CL_SUCCESS;
}
CL_EXPORT int32_t clEnqueueCopyBufferToImage(void *q, void *src_buf, void *dst_img,
                                             size_t src_off, const size_t *doff, const size_t *reg,
                                             uint32_t nev, const void **wl, void **ev) {
    (void)doff; (void)reg; (void)nev; (void)wl;
    if (!cdh_is_obj(q) || !cdh_is_obj(src_buf) || !cdh_is_obj(dst_img)) return CL_INVALID_VALUE;
    cdh_cl_obj *s = (cdh_cl_obj *)src_buf;
    cdh_cl_obj *d = (cdh_cl_obj *)dst_img;
    if (s->data && d->data && src_off < s->size) {
        size_t n = s->size - src_off;
        if (d->size < n) n = d->size;
        memcpy(d->data, (const uint8_t *)s->data + src_off, n);
    }
    cdh_set_event(ev, ((cdh_cl_obj *)q)->context);
    return CL_SUCCESS;
}
CL_EXPORT int32_t clEnqueueMigrateMemObjects(void *q, uint32_t n, const void **mems,
                                             uint64_t flags, uint32_t nev, const void **wl, void **ev) {
    (void)n; (void)mems; (void)flags; (void)nev; (void)wl;
    if (!cdh_is_obj(q)) return CL_INVALID_VALUE;
    cdh_set_event(ev, ((cdh_cl_obj *)q)->context);
    return CL_SUCCESS;
}
CL_EXPORT int32_t clEnqueueTask(void *q, void *k, uint32_t nev, const void **wl, void **ev) {
    (void)k; (void)nev; (void)wl;
    if (!cdh_is_obj(q)) return CL_INVALID_VALUE;
    cdh_set_event(ev, ((cdh_cl_obj *)q)->context);
    return CL_SUCCESS;
}
CL_EXPORT int32_t clEnqueueMarker(void *q, void **ev) {
    if (!cdh_is_obj(q)) return CL_INVALID_VALUE;
    cdh_set_event(ev, ((cdh_cl_obj *)q)->context);
    return CL_SUCCESS;
}
CL_EXPORT int32_t clEnqueueWaitForEvents(void *q, uint32_t nev, const void **wl) {
    (void)nev; (void)wl;
    return cdh_is_obj(q) ? CL_SUCCESS : CL_INVALID_VALUE;
}
CL_EXPORT int32_t clEnqueueBarrier(void *q) {
    return cdh_is_obj(q) ? CL_SUCCESS : CL_INVALID_VALUE;
}
CL_EXPORT int32_t clEnqueueSVMFree(void *q, uint32_t n, void **ptrs, void *cb, void *ud,
                                   uint32_t nev, const void **wl, void **ev) {
    (void)n; (void)ptrs; (void)cb; (void)ud; (void)nev; (void)wl;
    if (!cdh_is_obj(q)) return CL_INVALID_VALUE;
    cdh_set_event(ev, ((cdh_cl_obj *)q)->context);
    return CL_SUCCESS;
}
CL_EXPORT int32_t clEnqueueSVMMemcpy(void *q, uint32_t blk, void *dst, const void *src, size_t sz,
                                     uint32_t nev, const void **wl, void **ev) {
    (void)blk; (void)nev; (void)wl;
    if (!cdh_is_obj(q)) return CL_INVALID_VALUE;
    if (dst && src && sz > 0) memcpy(dst, src, sz);
    cdh_set_event(ev, ((cdh_cl_obj *)q)->context);
    return CL_SUCCESS;
}
CL_EXPORT int32_t clEnqueueSVMMemFill(void *q, void *ptr, const void *pat, size_t psz, size_t sz,
                                      uint32_t nev, const void **wl, void **ev) {
    (void)nev; (void)wl;
    if (!cdh_is_obj(q)) return CL_INVALID_VALUE;
    if (ptr && pat && psz > 0) {
        for (size_t pos = 0; pos < sz; pos += psz) {
            size_t n = (pos + psz <= sz) ? psz : (sz - pos);
            memcpy((uint8_t *)ptr + pos, pat, n);
        }
    }
    cdh_set_event(ev, ((cdh_cl_obj *)q)->context);
    return CL_SUCCESS;
}
CL_EXPORT int32_t clEnqueueSVMMap(void *q, uint32_t blk, uint64_t flags, void *ptr, size_t sz,
                                  uint32_t nev, const void **wl, void **ev) {
    (void)blk; (void)flags; (void)ptr; (void)sz; (void)nev; (void)wl;
    if (!cdh_is_obj(q)) return CL_INVALID_VALUE;
    cdh_set_event(ev, ((cdh_cl_obj *)q)->context);
    return CL_SUCCESS;
}
CL_EXPORT int32_t clEnqueueSVMUnmap(void *q, void *ptr, uint32_t nev, const void **wl, void **ev) {
    (void)ptr; (void)nev; (void)wl;
    if (!cdh_is_obj(q)) return CL_INVALID_VALUE;
    cdh_set_event(ev, ((cdh_cl_obj *)q)->context);
    return CL_SUCCESS;
}

