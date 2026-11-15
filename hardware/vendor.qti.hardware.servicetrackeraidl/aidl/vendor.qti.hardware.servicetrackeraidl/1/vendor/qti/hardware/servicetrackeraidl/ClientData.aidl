/*
 * 字段顺序逆向自 -V1-ndk.so 的 ClientData::writeToParcel：
 *   header(int32) → String [x21+0x00] → int32 [x21+0x18]
 * 结构体共 32 字节（std::string 24 + int 4 + 对齐 4）。
 */
package vendor.qti.hardware.servicetrackeraidl;

@VintfStability
parcelable ClientData {
    String clientPackage;
    int uid;
}
