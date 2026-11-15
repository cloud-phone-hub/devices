// setSensorAuxConfig 的入参包。
//
// 字段面取自官方桩 SensorAuxConfig::readFromParcel（0x11620，368 B）的调用序列，
// 实测 int32 ×5（§V7-5）。字段名静态取证读不到，用中性名；线上只看类型与顺序。
package vendor.meizu.sensor_aux;

@VintfStability
parcelable SensorAuxConfig {
    int field1;
    int field2;
    int field3;
    int field4;
    int field5;
}
