// getDisplayInfo 的出参包。
//
// 字段面取自官方桩 SensorAuxDisplayInfo::readFromParcel（0x11860，324 B）的调用序列，
// 实测 int32 ×2 → float ×2（§V7-5）。字段名静态取证读不到，用中性名。
package vendor.meizu.sensor_aux;

@VintfStability
parcelable SensorAuxDisplayInfo {
    int field1;
    int field2;
    float field3;
    float field4;
}
