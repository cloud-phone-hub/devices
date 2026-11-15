/*
 * 字段顺序逆向自 -V1-ndk.so 的 ServiceConnection::writeToParcel：
 *   header(int32) → String [x21+0x00] → int32 [x21+0x18] → int32 [x21+0x1c]
 * 结构体 32 字节；ServiceRecord 里的数组步长同为 32。
 */
package vendor.qti.hardware.servicetrackeraidl;

@VintfStability
parcelable ServiceConnection {
    String clientPackage;
    int pid;
    int uid;
}
