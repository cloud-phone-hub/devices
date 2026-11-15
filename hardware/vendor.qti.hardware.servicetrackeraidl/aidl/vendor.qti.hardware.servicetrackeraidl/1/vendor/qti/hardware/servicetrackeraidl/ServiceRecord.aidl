/*
 * 字段顺序逆向自 -V1-ndk.so 的 ServiceRecord::writeToParcel：
 *   header(int32) → String [x21+0x00] → String [x21+0x18] → int32 [x21+0x30]
 *   → bool [x21+0x34] → double [x21+0x38] → ServiceConnection[]（步长 32）
 * 数组元素类型由 .rela.dyn 的 GLOB_DAT 证明：
 *   0x14758 → AParcel_writeStdVectorParcelableElement<ServiceConnection>
 */
package vendor.qti.hardware.servicetrackeraidl;

import vendor.qti.hardware.servicetrackeraidl.ServiceConnection;

@VintfStability
parcelable ServiceRecord {
    String serviceName;
    String clientPackage;
    int pid;
    boolean isBound;
    double timestamp;
    ServiceConnection[] connections;
}
