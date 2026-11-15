// vendor.meizu.hardware.aw_light.IAwLightCallback v1 —— 逆向件。
//
// 来源：原厂 lib64/vendor.meizu.hardware.aw_light-V1-ndk.so。
// 反解口径：BpAwLightCallback::onStopNotify 只有一条 transact（code=1，
// w4=0x10000000 同步），写侧 1 个 AParcel_writeInt32、读侧只有 StatusHeader
// （无返回值、无出参）→ void onStopNotify(int)。
//
// 客户端（Flyme framework 的灯效控制侧）实现本接口、通过
// IAwLight.aw_set_light_over_callback 交给服务端；云机实现是 no-op，从不回调。

package vendor.meizu.hardware.aw_light;

@VintfStability
interface IAwLightCallback {
    void onStopNotify(int reason);
}
