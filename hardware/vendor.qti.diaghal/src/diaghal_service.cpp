// engine/vendor/hardware 线 —— vendor.qti.diaghal（IIdiag v1）自研服务实现。
//
// 背景：高通 diag HAL 在 Flyme m22 上由 /vendor/bin/diag-router 注册
// （原厂 rc: service vendor.diag-router，class hal，capabilities KILL，disabled，
// on early-boot start）。QTI 未公开 AIDL 源码，接口按原厂
// lib64/vendor.qti.diaghal-V1-ndk.so 反解，见 aidl/.../1/ 与同目录 .hash。
//
// 原厂语义：diag-router 打开 /dev/diag，把上层（dpmd、qlogd、mdb 等）的读/写请求
// 翻译成 DIAG_IOCTL_* 下发给内核 diag 驱动；ParcelableMemory 用 MappableFile 把
// 用户态缓冲区以 fd 映射进来，data 经 IIdiagcallback::send_data 回推。
//
// 云机语义：底座（lineage-23.2 virtio）没有 /dev/diag 与高通 diag 驱动，故：
//   * open 保存客户端回调并回 0（= 已打开），close 回 0；
//   * ioctl / write 回 -1（= 失败），不 open /dev/diag、不映射 fd、不回调；
//   * 从不主动 send_data（没有数据源）。
// 这样任何 diag 客户端都会得到"设备不可用"的确定性结果，而不是拿 0 误以为成功。
// 设备无关，不内置机型常量；rc / VINTF 在模块 vendor/ 静态树里。

#include <android/binder_auto_utils.h>
#include <android/binder_ibinder.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <android/binder_status.h>

#include <aidl/vendor/qti/diaghal/BnIdiag.h>
#include <aidl/vendor/qti/diaghal/IIdiagcallback.h>
#include <aidl/vendor/qti/diaghal/ParcelableMemory.h>

#include <cstdint>
#include <memory>

using aidl::vendor::qti::diaghal::BnIdiag;
using aidl::vendor::qti::diaghal::IIdiagcallback;
using aidl::vendor::qti::diaghal::ParcelableMemory;
using ndk::ScopedAStatus;

namespace {

// 必须与模块 vendor/etc/init/*.rc 的 interface 行、VINTF 的 <fqname> 逐字一致。
constexpr char kServiceName[] = "vendor.qti.diaghal.Idiag/default";

class Idiag final : public BnIdiag {
  public:
    // code 1：关闭会话。
    ScopedAStatus close(int32_t* _aidl_return) override {
        callback_.reset();
        *_aidl_return = 0;
        return ScopedAStatus::ok();
    }

    // code 2：下发 DIAG_IOCTL_*。没有 /dev/diag → -1（EIO 语义上的失败）。
    ScopedAStatus ioctl(int32_t /*cmd*/, const ParcelableMemory& /*memory*/, int32_t /*flag*/,
                        int32_t* _aidl_return) override {
        *_aidl_return = -1;
        return ScopedAStatus::ok();
    }

    // code 3：打开会话并登记回调。回 0 表示"会话已建立"，不代表内核设备可用。
    ScopedAStatus open(const std::shared_ptr<IIdiagcallback>& callback,
                       int32_t* _aidl_return) override {
        callback_ = callback;
        *_aidl_return = 0;
        return ScopedAStatus::ok();
    }

    // code 4：写入 diag 请求。没有 /dev/diag → -1。
    ScopedAStatus write(const ParcelableMemory& /*memory*/, int32_t /*length*/,
                        int32_t* _aidl_return) override {
        *_aidl_return = -1;
        return ScopedAStatus::ok();
    }

  private:
    std::shared_ptr<IIdiagcallback> callback_;
};

}  // namespace

int main() {
    // 单线程池足够：全部方法常数时间返回，只持有一个客户端回调引用。
    ABinderProcess_setThreadPoolMaxThreadCount(1);
    std::shared_ptr<Idiag> service = ndk::SharedRefBase::make<Idiag>();
    binder_status_t status = AServiceManager_addService(service->asBinder().get(), kServiceName);
    if (status != STATUS_OK) {
        return 1;
    }
    ABinderProcess_joinThreadPool();
    return 0;
}
