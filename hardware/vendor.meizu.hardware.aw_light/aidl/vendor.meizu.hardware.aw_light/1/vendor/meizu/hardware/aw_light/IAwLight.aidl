// vendor.meizu.hardware.aw_light.IAwLight v1 —— 逆向件。
//
// 来源：原厂 flyme-m22 vendor.img 的 lib64/vendor.meizu.hardware.aw_light-V1-ndk.so
// （MEIZU 私有 HAL，AOSP/LineageOS 无公开 AIDL 源码）。方法名、参数形状、事务码
// 与回包顺序全部由该 .so 反解得到，证据见
// engine/evidence/m22/issue-m22-aw-light-aidl/。
//
// 反解口径（llvm-objdump -d + llvm-nm -D）：
//   * BpAwLight 的 11 个方法各有一条 AIBinder_transact，w1=事务码（1..11）、
//     w4=0x10000000（同步，非 oneway）；
//   * 写侧调用序列即参数顺序，读侧 = [StatusHeader][返回值]（没有 out 参数，
//     因为 bool/int 不能作 out）；
//   * aw_set_light_over_callback 的入参写的是 IAwLightCallback 的
//     writeToParcel（接口类型 → 强 binder，与 aidl 生成的 helper 一字不差）；
//   * 接口 hash 取自 .so 里的 40 位常量字符串，逐字写入 1/.hash。
//
// 参数类型判定：
//   aw_music_phase(int[])  ← Bp 侧 AParcel_writeInt32Array
//   aw_set_Semaphore_letter(byte) ← AParcel_writeByte（mangled 为 signed char）
//   其余整型 ← AParcel_writeInt32

package vendor.meizu.hardware.aw_light;

// 同包跨文件也要显式 import（aidl 只在 import 路径里按全限定名解析类型）。
import vendor.meizu.hardware.aw_light.IAwLightCallback;

@VintfStability
interface IAwLight {
    // code 1：设三色通道相位，返回是否接受。
    boolean aw_color_set_phase(int r, int g, int b);
    // code 2：某相位的持续时长（ms）。无硬件时给 0。
    int aw_get_duration_time(int phase);
    // code 3：初始化。
    boolean aw_light_init();
    // code 4：开始灯效。
    boolean aw_light_start();
    // code 5：停止灯效。
    boolean aw_light_stop();
    // code 6：切换灯效编号。
    boolean aw_light_switchEffect(int effect);
    // code 7：音乐律动相位序列。
    boolean aw_music_phase(in int[] phases);
    // code 8：信号灯字符（byte 是 AIDL 的 int8，不是 char）。
    boolean aw_set_Semaphore_letter(byte letter);
    // code 9：CL 亮度相位。
    boolean aw_set_cl_brightness_phase(int phase);
    // code 10：登记停止通知回调。
    boolean aw_set_light_over_callback(in IAwLightCallback callback);
    // code 11：设置进度（0..100）。
    boolean aw_set_progress(int progress);
}
