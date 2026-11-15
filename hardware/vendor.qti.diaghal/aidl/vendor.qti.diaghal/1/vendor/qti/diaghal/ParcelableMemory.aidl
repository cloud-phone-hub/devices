/*
 * vendor.qti.diaghal —— 诊断数据的内存视图（v1）。
 *
 * 逆向来源：原厂 lib64/vendor.qti.diaghal-V1-ndk.so 的
 *   ParcelableMemory::writeToParcel / readFromParcel。
 * 写序：[int32 size 占位] → int32(1)（MappableFile 的非空标记）
 *       → android.hardware.common.MappableFile.writeToParcel（offset 0，24 B）
 *       → String name（offset 0x18，SSO 串）→ 回填 size。
 * 故字段序为「一个 MappableFile + 一个 String」。原厂 .so 未链接 MappableFile 的实现
 * （NEEDED 里有 android.hardware.common-V2-ndk.so），故该 parcelable 直接用平台
 * AOSP 定义（depsHal=common 的 common/android.hardware.common/2/MappableFile.aidl）。
 */
package vendor.qti.diaghal;

import android.hardware.common.MappableFile;

@VintfStability
parcelable ParcelableMemory {
  MappableFile file;
  String name;
}
