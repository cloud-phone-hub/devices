/*
 * 字段顺序逆向自 -V1-ndk.so 的 ClientConnection::writeToParcel：
 *   header(int32) → String [x21+0x00] → int32 [x21+0x18] → int32 [x21+0x1c]
 * 结构体 32 字节；ClientRecord 里的数组步长就是 32（反汇编 lsr x2, x8, #5）。
 */
package vendor.qti.hardware.servicetrackeraidl;

@VintfStability
parcelable ClientConnection {
    String serviceName;
    int pid;
    int uid;
}
