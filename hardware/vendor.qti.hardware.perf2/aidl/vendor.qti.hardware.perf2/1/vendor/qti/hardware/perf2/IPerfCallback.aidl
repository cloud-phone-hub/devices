/*
 * 3a-15 自研 QTI perf2 HAL 回调接口（vendor.qti.hardware.perf2.IPerfCallback，版本 1）。
 *
 * 逆向自官方 vendor.qti.hardware.perf2-V1-ndk.so 的 BpPerfCallback：
 * 仅一个方法 notifyCallback（transaction code 1），参数
 * (int, String, int, int, int[]) 与官方 parcel 写入序列一致。
 * 本 HAL 只登记回调，不主动回调（云机上无 perf 事件来源）。
 */
package vendor.qti.hardware.perf2;

@VintfStability
interface IPerfCallback {
    void notifyCallback(int in_a, in String in_b, int in_c, int in_d, in int[] in_e);
}
