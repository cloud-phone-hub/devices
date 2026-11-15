/*
 * 字段顺序逆向自 -V1-ndk.so 的 ServiceData::writeToParcel：
 *   header(int32 占位，回填本结构体长度)
 *   String  [x21+0x00]   服务名
 *   String  [x21+0x18]   客户端包名
 *   int32   [x21+0x30]
 *   double  [x21+0x38]
 *   bool    [x21+0x40]
 * 结构体共 72 字节；字段名是语义推测，wire 格式只由「类型 + 顺序」决定。
 */
package vendor.qti.hardware.servicetrackeraidl;

@VintfStability
parcelable ServiceData {
    String serviceName;
    String clientPackage;
    int pid;
    double timestamp;
    boolean isBound;
}
