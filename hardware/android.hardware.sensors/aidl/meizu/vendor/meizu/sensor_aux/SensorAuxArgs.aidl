// sensorAuxExecuteCmd 的入参包。
//
// 字段面取自官方桩 SensorAuxArgs::readFromParcel（0x113a4，412 B）的调用序列，实测为
// int32 ×4 → boolean ×2（§V7-5）。**字段名静态取证读不到**，此处用中性名；
// 线上兼容性只取决于类型与顺序，与名字无关。
package vendor.meizu.sensor_aux;

@VintfStability
parcelable SensorAuxArgs {
    int field1;
    int field2;
    int field3;
    int field4;
    boolean flag1;
    boolean flag2;
}
