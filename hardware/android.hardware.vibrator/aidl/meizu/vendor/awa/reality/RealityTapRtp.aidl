/*
 * vendor.awa.reality —— RealityTap 的「实时播放(rtp)」入参（v3）。
 *
 * 逆向来源：原厂 lib64/vendor.awa.reality-V3-ndk.so 的
 *   RealityTapRtp::writeToParcel / readFromParcel。
 * 写序：[int32 size 占位] → ParcelFileDescriptor（AParcel_writeParcelFileDescriptor；
 *       非空，读到负值即 0x80000008 = STATUS_UNEXPECTED_NULL）→ [0x4] 一个 int32
 *       → 回填 size。
 * 故字段序为「一个 ParcelFileDescriptor + 一个 int」。fd 承载波形数据，云机不解析。
 */
package vendor.awa.reality;

@VintfStability
parcelable RealityTapRtp {
  ParcelFileDescriptor fd;
  int arg1;
}
