#pragma once

// 零外部共享库依赖的精简版 BufferAllocator（对齐 AOSP system/memory/libdmabufheap 接口）：
//
// 背景：
//   上游 external/minigbm 在定义 HAS_DMABUF_SYSTEM_HEAP 时，通过 BufferAllocator::Alloc(
//   kDmabufSystemHeapName, reserved_region_size) 从 /dev/dma_heap/system 分配图形缓冲区
//   元数据区（SELinux 标签为 dmabuf_system_heap_device，所有应用域与编解码域均可 mmap）；
//   若未启用 HAS_DMABUF_SYSTEM_HEAP，则会退回 memfd_create_reserved_region()，产生的匿名文件
//   继承 hal_graphics_allocator_default_tmpfs 标签，在 Android 16 上会被 platform_app_36
//   （如 HyperOS 4 Impeller AHBSwapchainVK 的 ASurfaceTransaction 元数据读写）与 mediaswcodec
//   触发 SELinux map 拒绝。
//   本头文件直接通过 Linux UAPI DMA_HEAP_IOCTL_ALLOC 操作系统 dmabuf heap，避免为
//   mapper.<vendor>.so 引入对载体私有库 libdmabufheap.so 的 DT_NEEDED 依赖，保持 SP-HAL 100% 自包含。

#include <errno.h>
#include <fcntl.h>
#include <linux/ioctl.h>
#include <linux/types.h>
#include <stddef.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <mutex>
#include <string>
#include <unordered_map>

static constexpr char kDmabufSystemHeapName[] = "system";
static constexpr char kDmabufSystemUncachedHeapName[] = "system-uncached";

#ifndef DMA_HEAP_IOC_MAGIC
struct dma_heap_allocation_data {
  __u64 len;
  __u32 fd;
  __u32 fd_flags;
  __u64 heap_flags;
};
#define DMA_HEAP_IOC_MAGIC 'H'
#define DMA_HEAP_IOCTL_ALLOC _IOWR(DMA_HEAP_IOC_MAGIC, 0x0, struct dma_heap_allocation_data)
#endif

class BufferAllocator {
 public:
  BufferAllocator() = default;

  ~BufferAllocator() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& kv : heap_fds_) {
      if (kv.second >= 0) {
        close(kv.second);
      }
    }
  }

  BufferAllocator(const BufferAllocator&) = delete;
  BufferAllocator& operator=(const BufferAllocator&) = delete;

  int Alloc(const std::string& heap_name, size_t len,
            unsigned int flags = (O_RDWR | O_CLOEXEC),
            size_t /*legacy_align*/ = 0) {
    if (heap_name.empty() || len == 0) {
      return -EINVAL;
    }
    int heap_fd = GetOrOpenHeapFd(heap_name);
    if (heap_fd < 0) {
      return heap_fd;
    }

    struct dma_heap_allocation_data data;
    memset(&data, 0, sizeof(data));
    data.len = static_cast<__u64>(len);
    data.fd_flags = flags;
    data.heap_flags = 0;

    int ret = TEMP_FAILURE_RETRY(ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &data));
    if (ret < 0) {
      return -errno;
    }
    return static_cast<int>(data.fd);
  }

 private:
  int GetOrOpenHeapFd(const std::string& heap_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = heap_fds_.find(heap_name);
    if (it != heap_fds_.end() && it->second >= 0) {
      return it->second;
    }
    std::string path = "/dev/dma_heap/" + heap_name;
    int fd = TEMP_FAILURE_RETRY(open(path.c_str(), O_RDONLY | O_CLOEXEC));
    if (fd < 0) {
      return -errno;
    }
    heap_fds_[heap_name] = fd;
    return fd;
  }

  std::mutex mutex_;
  std::unordered_map<std::string, int> heap_fds_;
};
