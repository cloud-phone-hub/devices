// 屏幕取色辅助 HAL 的状态枚举。
//
// 依据（原 v1 设备侧证据 issue-m22-sf-oneway-async-saturation，§V7-5；未纳入 engine/evidence）：官方 NDK 桩
// vendor.meizu.sensor_aux-V1-ndk.so 里 **没有** SAuxStatus 的 readFromParcel/writeToParcel
// 导出，rodata 也没有 "vendor.meizu.sensor_aux.SAuxStatus" 类型名串，而它在方法签名里
// 始终作为最后一个指针参数出现、回话中对应**一次 AParcel_readInt32**
// ⇒ 判定它是 enum（NDK 里按 int32 编解码），不是 parcelable。
// 取值名未知（静态取证读不到），此处按「回话只写 0」的桩语义定义；线上只要求 int32。
package vendor.meizu.sensor_aux;

@VintfStability
enum SAuxStatus {
    OK = 0,
    ERROR = 1,
    INVALID_ARGUMENT = 2,
    NOT_SUPPORTED = 3,
}
