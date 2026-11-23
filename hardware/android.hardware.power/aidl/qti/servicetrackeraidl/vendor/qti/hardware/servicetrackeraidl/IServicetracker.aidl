/*
 * vendor.qti.hardware.servicetrackeraidl.IServicetracker（版本 1）。
 *
 * 接口不是 AOSP 定义，逆向自原厂 vendor 镜像里的
 *   lib64/vendor.qti.hardware.servicetrackeraidl-V1-ndk.so
 *   lib64/hw/vendor.qti.hardware.servicetrackeraidl-impl.so
 * 两条 .so 保留了自研所需的全部事实：
 *   * descriptor：vendor.qti.hardware.servicetrackeraidl.IServicetracker
 *   * 接口 hash ：fd4771abe6e1c694687c871ee5d888b6e9cbf7b0（见同目录 .hash）
 *   * 方法顺序  ：BpServicetracker::<方法> 里的 AIBinder_transact 立即数，1..14，
 *                 与下面声明顺序逐条对应，不可调整；
 *   * 方法方向  ：oneway 的 5 个方法（transact flags = 0x10000001）只能单向，
 *                 Bp 侧不读 reply；查询类 flags = 0x10000000，返回经状态头 + 返回值/出参；
 *   * 参数形状  ：取 Bp 侧 mangled 签名（ServiceData/ClientData、String、
 *                 String[]、int32_t*、bool*、数组/结构体指针）。
 *
 * 关于 int / boolean：AIDL 不允许基本类型做出参（aidl 报
 * "int can only be an in parameter"，34/35/36/37 各版本一致），所以 getPid、
 * isServiceB 以及 getServiceBCount 的计数只能是返回值。Bp 侧反汇编证实了这一点：
 * getServiceBCount 先 AParcel_readInt32（写入 *count）后
 * AParcel_readParcelableArray（写入服务数组），即回包序为
 * [返回值 count][出参 services]，与「返回值先写、出参后写」的生成规则吻合。
 * 数组/结构体类出参则确实用 out（回包只一项时两种写法 wire 完全相同）。
 *
 * 云机语义见 src/servicetracker_service.cpp：原厂实现维护的是「框架侧服务绑定
 * 关系表」，云机不接管任何应用服务，故所有登记调用为 no-op、所有查询返回空。
 */
package vendor.qti.hardware.servicetrackeraidl;

// 同包跨文件也要显式 import（aidl 只在 import 路径里按全限定名解析类型）。
import vendor.qti.hardware.servicetrackeraidl.ClientConnection;
import vendor.qti.hardware.servicetrackeraidl.ClientData;
import vendor.qti.hardware.servicetrackeraidl.ClientRecord;
import vendor.qti.hardware.servicetrackeraidl.ServiceConnection;
import vendor.qti.hardware.servicetrackeraidl.ServiceData;
import vendor.qti.hardware.servicetrackeraidl.ServiceRecord;

@VintfStability
interface IServicetracker {
    // code 1（oneway）：框架上报一次服务绑定。
    oneway void bindService(in ServiceData service, in ClientData client);
    // code 2（oneway）：服务销毁。
    oneway void destroyService(in ServiceData service);
    // code 3：某客户端包名持有的连接列表。
    void getClientConnections(in String client, out ClientConnection[] connections);
    // code 4：某服务的 PID（AIDL 不允许 out int，故为返回值）。
    int getPid(in String service);
    // code 5：批量查 PID（与入参数组同序）。
    void getPids(in String[] services, out int[] pids);
    // code 6：仍在运行的服务 PID 列表。
    void getRunningServicePid(out int[] pids);
    // code 7：服务明细经出参返回，计数为返回值（回包序 [count][services]）。
    int getServiceBCount(out ServiceRecord[] services);
    // code 8：某服务被哪些连接持有。
    void getServiceConnections(in String service, out ServiceConnection[] connections);
    // code 9：客户端记录（名称大小写沿用原厂 getclientInfo）。
    void getclientInfo(in String client, out ClientRecord record);
    // code 10：服务记录（名称大小写沿用原厂 getserviceInfo）。
    void getserviceInfo(in String service, out ServiceRecord record);
    // code 11：是否为后台服务（原厂命名 isServiceB，boolean 只能做返回值）。
    boolean isServiceB(in String service);
    // code 12（oneway）：终止某进程（原厂实现会真的 kill，云机为 no-op）。
    oneway void killProcess(int pid);
    // code 13（oneway）：服务启动。
    oneway void startService(in ServiceData service);
    // code 14（oneway）：解绑。
    oneway void unbindService(in ServiceData service, in ClientData client);
}
