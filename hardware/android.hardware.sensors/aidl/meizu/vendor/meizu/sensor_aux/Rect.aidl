// 取色区域。字段面取自官方桩 Rect::readFromParcel（0x11164，368 B）的调用序列，
// 实测 **int32 ×5（aidl 写法为 `int`，本机 SDK aidl 36.1.0 不认 `int32` 别名）**（§V7-5；本轮先用漏了 `_ZNK` 的正则取 write 侧，一度把 Rect 误判为
// 空 parcelable，已按修正后的正则重取）。字段名静态取证读不到，用中性名。
package vendor.meizu.sensor_aux;

@VintfStability
parcelable Rect {
    int field1;
    int field2;
    int field3;
    int field4;
    int field5;
}
