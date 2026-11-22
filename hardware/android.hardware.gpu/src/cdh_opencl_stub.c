/*
 * cdh_opencl_stub.c — 极简 OpenCL 优雅降级桩库（libOpenCL.so）
 *
 * 云机无物理 Mali/Adreno OpenCL 运行时；部分厂商相机（如 Pixel GoogleCamera
 * NativeDeblurFusion）在 cl_loader 未加载到 libOpenCL.so 时未对 clGetPlatformIDs
 * 函数指针判空即直接 blr 导致 SIGSEGV(pc=0x0)，而收到 CL_PLATFORM_NOT_FOUND_KHR(-1001)
 * 或 num_platforms=0 后可正常走无 OpenCL 降级路径。
 * 本桩库提供零依赖的标准 OpenCL 查询入口，安全回报无可用平台。
 */
#include <stddef.h>
#include <stdint.h>

#define CL_DEVICE_NOT_FOUND         (-1)
#define CL_INVALID_VALUE            (-30)
#define CL_PLATFORM_NOT_FOUND_KHR   (-1001)

#define CL_EXPORT __attribute__((visibility("default")))

CL_EXPORT int32_t clGetPlatformIDs(uint32_t num_entries, void **platforms, uint32_t *num_platforms) {
    (void)num_entries;
    (void)platforms;
    if (num_platforms) {
        *num_platforms = 0;
    }
    return CL_PLATFORM_NOT_FOUND_KHR;
}

CL_EXPORT int32_t clIcdGetPlatformIDsKHR(uint32_t num_entries, void **platforms, uint32_t *num_platforms) {
    return clGetPlatformIDs(num_entries, platforms, num_platforms);
}

CL_EXPORT int32_t clGetDeviceIDs(void *platform, uint64_t device_type, uint32_t num_entries, void **devices, uint32_t *num_devices) {
    (void)platform;
    (void)device_type;
    (void)num_entries;
    (void)devices;
    if (num_devices) {
        *num_devices = 0;
    }
    return CL_DEVICE_NOT_FOUND;
}

CL_EXPORT void *clGetExtensionFunctionAddress(const char *func_name) {
    (void)func_name;
    return NULL;
}

CL_EXPORT void *clGetExtensionFunctionAddressForPlatform(void *platform, const char *func_name) {
    (void)platform;
    (void)func_name;
    return NULL;
}

#define STUB_INT(name) \
    CL_EXPORT int32_t name(void) { return CL_DEVICE_NOT_FOUND; }

#define STUB_PTR(name) \
    CL_EXPORT void *name(void *a, void *b, void *c, void *d, void *e, int32_t *errcode_ret) { \
        (void)a; (void)b; (void)c; (void)d; (void)e; \
        if (errcode_ret) *errcode_ret = CL_DEVICE_NOT_FOUND; \
        return NULL; \
    }

STUB_INT(clGetPlatformInfo)
STUB_INT(clGetDeviceInfo)
STUB_INT(clGetContextInfo)
STUB_INT(clGetCommandQueueInfo)
STUB_INT(clGetMemObjectInfo)
STUB_INT(clGetImageInfo)
STUB_INT(clGetProgramInfo)
STUB_INT(clGetProgramBuildInfo)
STUB_INT(clGetKernelInfo)
STUB_INT(clGetKernelWorkGroupInfo)
STUB_INT(clGetKernelArgInfo)
STUB_INT(clGetEventInfo)
STUB_INT(clGetEventProfilingInfo)
STUB_INT(clGetSamplerInfo)
STUB_INT(clGetSupportedImageFormats)
STUB_INT(clBuildProgram)
STUB_INT(clCompileProgram)
STUB_INT(clSetKernelArg)
STUB_INT(clSetKernelArgSVMPointer)
STUB_INT(clSetKernelExecInfo)
STUB_INT(clEnqueueNDRangeKernel)
STUB_INT(clEnqueueReadBuffer)
STUB_INT(clEnqueueWriteBuffer)
STUB_INT(clEnqueueCopyBuffer)
STUB_INT(clEnqueueFillBuffer)
STUB_INT(clEnqueueReadImage)
STUB_INT(clEnqueueWriteImage)
STUB_INT(clEnqueueCopyImage)
STUB_INT(clEnqueueUnmapMemObject)
STUB_INT(clEnqueueBarrierWithWaitList)
STUB_INT(clEnqueueMarkerWithWaitList)
STUB_INT(clFlush)
STUB_INT(clFinish)
STUB_INT(clWaitForEvents)
STUB_INT(clReleaseContext)
STUB_INT(clRetainContext)
STUB_INT(clReleaseCommandQueue)
STUB_INT(clRetainCommandQueue)
STUB_INT(clReleaseMemObject)
STUB_INT(clRetainMemObject)
STUB_INT(clReleaseProgram)
STUB_INT(clRetainProgram)
STUB_INT(clReleaseKernel)
STUB_INT(clRetainKernel)
STUB_INT(clReleaseEvent)
STUB_INT(clRetainEvent)
STUB_INT(clReleaseSampler)
STUB_INT(clRetainSampler)
STUB_INT(clReleaseDevice)
STUB_INT(clRetainDevice)
STUB_INT(clUnloadCompiler)
STUB_INT(clUnloadPlatformCompiler)

STUB_PTR(clCreateContext)
STUB_PTR(clCreateContextFromType)
STUB_PTR(clCreateCommandQueue)
STUB_PTR(clCreateCommandQueueWithProperties)
STUB_PTR(clCreateBuffer)
STUB_PTR(clCreateSubBuffer)
STUB_PTR(clCreateImage)
STUB_PTR(clCreateImage2D)
STUB_PTR(clCreateImage3D)
STUB_PTR(clCreateProgramWithSource)
STUB_PTR(clCreateProgramWithBinary)
STUB_PTR(clCreateProgramWithIL)
STUB_PTR(clLinkProgram)
STUB_PTR(clCreateKernel)
STUB_PTR(clCloneKernel)
STUB_PTR(clCreateSampler)
STUB_PTR(clCreateSamplerWithProperties)
STUB_PTR(clCreateUserEvent)
STUB_PTR(clEnqueueMapBuffer)
STUB_PTR(clEnqueueMapImage)
