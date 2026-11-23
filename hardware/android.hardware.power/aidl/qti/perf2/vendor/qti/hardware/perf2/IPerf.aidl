/*
 * 3a-15 自研 QTI perf2 HAL 主接口（vendor.qti.hardware.perf2.IPerf，版本 1）。
 *
 * 接口不是 AOSP 定义，逆向自官方 vendor.qti.hardware.perf2-V1-ndk.so：
 *   * 方法顺序即 transaction code（1..16），不可调整；
 *   * BpPerf 的 C++ 符号（...Pi / ...PSB_）表明末尾 int/String 是**返回值**
 *     （NDK stub 把返回值实现为最后一个 out 形参），因此此处写返回类型；
 *   * perfEvent / perfLockCmd / perfLockRelease 无返回值，Bp 侧 transact 后不读 reply；
 *   * getInterfaceVersion 返回 1，getInterfaceHash 由 .hash 固化
 *     （c5d67f27386b8feb13a185a4cdd647afbeb758ce，与官方包一致）。
 *
 * 云机语义见 ../build.py 头注释：所有调用立即成功，返回 0 / 入参默认串。
 */
package vendor.qti.hardware.perf2;

import vendor.qti.hardware.perf2.IPerfCallback;

@VintfStability
interface IPerf {
    int perfAsyncRequest(int in_a, in String in_b, in int[] in_c);                       // code 1
    int perfCallbackDeregister(in IPerfCallback in_cb, int in_a);                        // code 2
    int perfCallbackRegister(in IPerfCallback in_cb, int in_a);                          // code 3
    void perfEvent(int in_a, in String in_b, in int[] in_c);                             // code 4
    int perfGetFeedback(int in_a, in String in_b, in int[] in_c);                        // code 5
    String perfGetProp(in String in_key, in String in_default);                          // code 6
    int perfHint(int in_a, in String in_b, int in_c, int in_d, in int[] in_e);           // code 7
    int perfHintAcqRel(int in_a, int in_b, in String in_c, int in_d, int in_e,           // code 8
                       in int[] in_f);
    int perfHintRenew(int in_a, int in_b, in String in_c, int in_d, int in_e,            // code 9
                      in int[] in_f);
    int perfLockAcqAndRelease(int in_a, int in_b, in int[] in_c, in int[] in_d);         // code 10
    int perfLockAcquire(int in_a, int in_b, in int[] in_c, in int[] in_d);               // code 11
    void perfLockCmd(int in_a, int in_b);                                                // code 12
    void perfLockRelease(int in_a, in int[] in_b);                                       // code 13
    int perfProfile(int in_a, int in_b, int in_c);                                       // code 14
    int perfSetProp(in String in_key, in String in_value);                               // code 15
    String perfSyncRequest(int in_a, in String in_b, in int[] in_c);                     // code 16
}
