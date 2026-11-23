// 屏幕取色回调的工作模式。
//
// 依据（§V7-5）：官方 BpScreenDataProviderCallback::enable 的编解码是
// 「AParcel_writeInt32 → 回话 AParcel_readBool」⇒ 形参是 int32 基的 enum、返回 bool。
// 取值名未知，桩侧不消费。
package vendor.meizu.sensor_aux;

@VintfStability
enum ScreenDataProviderWorkMode {
    NONE = 0,
    REGION_SAMPLING = 1,
}
