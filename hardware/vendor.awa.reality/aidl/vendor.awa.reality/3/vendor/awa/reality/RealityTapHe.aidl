/*
 * vendor.awa.reality —— RealityTap 的「触感波形」入参（v3）。
 *
 * 逆向来源：原厂 lib64/vendor.awa.reality-V3-ndk.so 的
 *   RealityTapHe::writeToParcel / readFromParcel（AArch64 反汇编，见
 *   engine/evidence/m22/issue-m22-realitytap-diaghal-aidl/solution.md）。
 * 写序：[int32 size 占位] → int[] values（AParcel_writeInt32Array，null 记 -1）
 *       → [0x18][0x1c][0x20][0x24] 四个 int32（逐个 AParcel_writeInt32）
 *       → 回填 size。
 * 故字段序为「一个 int[] + 四个 int」。原厂 .so 里没有字段名的调试符号，
 * 下面四个名字是占位（arg1..arg4），wire 上只认顺序与类型。
 */
package vendor.awa.reality;

@VintfStability
parcelable RealityTapHe {
  int[] values;
  int arg1;
  int arg2;
  int arg3;
  int arg4;
}
