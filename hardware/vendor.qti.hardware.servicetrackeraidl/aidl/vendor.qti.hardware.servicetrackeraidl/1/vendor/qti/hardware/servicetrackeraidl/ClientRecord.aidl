/*
 * 字段顺序逆向自 -V1-ndk.so 的 ClientRecord::writeToParcel：
 *   header(int32) → String [x21+0x00] → int32 [x21+0x18] → ClientConnection[]（数组步长 32）
 * 数组元素类型由 .rela.dyn 的 GLOB_DAT 证明：
 *   0x14748 → AParcel_writeStdVectorParcelableElement<ClientConnection>
 */
package vendor.qti.hardware.servicetrackeraidl;

import vendor.qti.hardware.servicetrackeraidl.ClientConnection;

@VintfStability
parcelable ClientRecord {
    String clientPackage;
    int uid;
    ClientConnection[] connections;
}
