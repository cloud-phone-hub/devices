// engine/vendor/hardware 线 —— vendor.qti.hardware.servicetrackeraidl（IServicetracker v1）
// 自研服务实现，替换此前直接下发原厂预编译二进制的做法。
//
// 背景：这是 QTI 私有 HAL（原厂编译自 vendor/qcom/proprietary/fwk-utils/
// servicetracker/aidl/1.0/default/），AOSP/LineageOS 无公开源码。接口是本模块
// aidl/ 下按原厂 -V1-ndk.so 反解出来的（见 IServicetracker.aidl 头注释），因此
// 这里可以用源码实现并保持与框架侧（Flyme system_server）wire 兼容。
//
// 原厂语义：框架把 bindService/unbindService/destroyService/startService 上报给
// 这个守护进程，它维护「服务 ↔ 客户端连接」表，供性能/功耗侧查询（公开日志里
// 可见 "bindService is called for service … total connections …"）。
//
// 云机语义：我们不接管任何应用服务，也没有可调优对象，故：
//   * 4 个上报 + killProcess 一律 no-op（返回 ok，绝不真的 kill）；
//   * 查询类返回空集合；单个 PID 查询返回 -1（未知）；isServiceB 返回 false。
// 这类返回值对框架侧是「表里没有」，与「服务不存在」同义，不会引出错误分支。
// 设备无关，不内置机型常量；rc / VINTF 在模块 vendor/ 静态树里。

#include <android/binder_auto_utils.h>
#include <android/binder_ibinder.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <android/binder_status.h>

#include <aidl/vendor/qti/hardware/servicetrackeraidl/BnServicetracker.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using aidl::vendor::qti::hardware::servicetrackeraidl::BnServicetracker;
using aidl::vendor::qti::hardware::servicetrackeraidl::ClientConnection;
using aidl::vendor::qti::hardware::servicetrackeraidl::ClientData;
using aidl::vendor::qti::hardware::servicetrackeraidl::ClientRecord;
using aidl::vendor::qti::hardware::servicetrackeraidl::ServiceConnection;
using aidl::vendor::qti::hardware::servicetrackeraidl::ServiceData;
using aidl::vendor::qti::hardware::servicetrackeraidl::ServiceRecord;
using ndk::ScopedAStatus;

namespace {

// kServiceName 必须与模块 vendor/etc/init/*.rc 的 interface 行、VINTF 的
// <fqname> 逐字一致；AIDL 实例名格式是 <包名>.<接口名>/<实例>。
constexpr char kServiceName[] =
        "vendor.qti.hardware.servicetrackeraidl.IServicetracker/default";

class Servicetracker final : public BnServicetracker {
  public:
    // code 1：oneway 上报绑定。原厂会记表，云机丢弃。
    ScopedAStatus bindService(const ServiceData& /*service*/,
                              const ClientData& /*client*/) override {
        return ScopedAStatus::ok();
    }

    // code 2：oneway 上报销毁。
    ScopedAStatus destroyService(const ServiceData& /*service*/) override {
        return ScopedAStatus::ok();
    }

    // code 3：某客户端的连接列表。表恒空。
    ScopedAStatus getClientConnections(const std::string& /*client*/,
                                       std::vector<ClientConnection>* connections) override {
        connections->clear();
        return ScopedAStatus::ok();
    }

    // code 4：单服务 PID。未知用 -1，调用方按「不在表里」处理。
    ScopedAStatus getPid(const std::string& /*service*/, int32_t* _aidl_return) override {
        *_aidl_return = -1;
        return ScopedAStatus::ok();
    }

    // code 5：批量 PID，长度与入参对齐、值全 -1（不能返回空数组，调用方按同序取）。
    ScopedAStatus getPids(const std::vector<std::string>& services,
                          std::vector<int32_t>* pids) override {
        pids->assign(services.size(), -1);
        return ScopedAStatus::ok();
    }

    // code 6：运行中服务 PID 列表，空。
    ScopedAStatus getRunningServicePid(std::vector<int32_t>* pids) override {
        pids->clear();
        return ScopedAStatus::ok();
    }

    // code 7：服务数与明细，均空（计数是返回值，明细走出参）。
    ScopedAStatus getServiceBCount(std::vector<ServiceRecord>* services,
                                   int32_t* _aidl_return) override {
        services->clear();
        *_aidl_return = 0;
        return ScopedAStatus::ok();
    }

    // code 8：某服务的连接列表，空。
    ScopedAStatus getServiceConnections(const std::string& /*service*/,
                                        std::vector<ServiceConnection>* connections) override {
        connections->clear();
        return ScopedAStatus::ok();
    }

    // code 9：客户端记录，返回空结构（包名回显入参便于日志对照）。
    ScopedAStatus getclientInfo(const std::string& client, ClientRecord* record) override {
        *record = ClientRecord{};
        record->clientPackage = client;
        return ScopedAStatus::ok();
    }

    // code 10：服务记录，返回空结构（服务名回显入参）。
    ScopedAStatus getserviceInfo(const std::string& service, ServiceRecord* record) override {
        *record = ServiceRecord{};
        record->serviceName = service;
        return ScopedAStatus::ok();
    }

    // code 11：是否后台服务，恒 false（我们没有该服务的信息）。
    ScopedAStatus isServiceB(const std::string& /*service*/, bool* _aidl_return) override {
        *_aidl_return = false;
        return ScopedAStatus::ok();
    }

    // code 12：oneway killProcess。原厂会杀进程，云机绝不能照做（可能命中宿主/框架），
    // 只回 ok 不执行任何动作。
    ScopedAStatus killProcess(int32_t /*pid*/) override { return ScopedAStatus::ok(); }

    // code 13：oneway 服务启动上报。
    ScopedAStatus startService(const ServiceData& /*service*/) override {
        return ScopedAStatus::ok();
    }

    // code 14：oneway 解绑上报。
    ScopedAStatus unbindService(const ServiceData& /*service*/,
                                const ClientData& /*client*/) override {
        return ScopedAStatus::ok();
    }
};

}  // namespace

int main() {
    // 单线程池足够：全部方法都是常数时间返回，不存在跨进程回调。
    ABinderProcess_setThreadPoolMaxThreadCount(1);
    std::shared_ptr<Servicetracker> service = ndk::SharedRefBase::make<Servicetracker>();
    binder_status_t status = AServiceManager_addService(service->asBinder().get(), kServiceName);
    if (status != STATUS_OK) {
        return 1;
    }
    ABinderProcess_joinThreadPool();
    return 0;
}
