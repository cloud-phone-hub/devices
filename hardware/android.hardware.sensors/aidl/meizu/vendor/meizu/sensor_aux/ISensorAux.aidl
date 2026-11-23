// MEIZU 22（Flyme 12.6）屏幕取色辅助 HAL 的接口面 —— 云机侧自研**桩**实现所用。
//
// 为什么要它（issue-m22-sf-oneway-async-saturation §V V3-8）：system_server 内
// `libsensorservice.so` 的 `android::Listener::onSampleResult` 会在 binder 工作线程上同步
// `AServiceManager_getService("vendor.meizu.sensor_aux.ISensorAux/default")`；我们替换官方
// vendor 时把该 HAL 的实现连同 manifest 一起丢了 ⇒ 每轮 `Waiting for service…` 白睡
// **5.018 s** 才返回 NULL，而这条回调正是该 binder 节点 oneway 闸门的持有者
// ⇒ 放行上限 1/5.018 s < surfaceflinger 成对发射速率 ⇒ 异步预算 520,192 B 贴顶
// ⇒ 全系统发往 system_server 的 oneway `-ENOSPC` → networkstack 自杀 → system_server 崩。
// 给这个名字一个**秒回**的实体即可解除（落法论证见 solution.md §4，方案①(a)）。
//
// 接口面全部来自静态取证（log-fail.txt §V7-5，官方 NDK 桩
// `vendor.meizu.sensor_aux-V1-ndk.so` + 实现库 `vendor.meizu.sensor_aux@1.0-default-impl.so`）：
//   * 方法**顺序**＝反汇编里 `AIBinder_transact` 的 code 实参 1..10（= AIDL 声明顺序）；
//   * 参数/回值类型＝每个方法对 AParcel 的 read/write 调用序列；
//   * `SAuxStatus` 无 read/writeToParcel 导出、回话只对应一次 int32 ⇒ 判为 **enum**；
//   * parcelable 形参/回值前都多一次 int32 ⇒ 判为 **@nullable**（presence 位）。
// 字段名与枚举取值名静态读不到，用中性名占位；线上兼容性只看类型与顺序，与名字无关。
package vendor.meizu.sensor_aux;

import vendor.meizu.sensor_aux.IScreenDataProviderCallback;
import vendor.meizu.sensor_aux.SAuxStatus;
import vendor.meizu.sensor_aux.SensorAuxArgs;
import vendor.meizu.sensor_aux.SensorAuxConfig;
import vendor.meizu.sensor_aux.SensorAuxDisplayInfo;

@VintfStability
interface ISensorAux {
    // 1
    int getColor();

    // 2
    @nullable SensorAuxDisplayInfo getDisplayInfo();

    // 3
    int registerScreenDataProvider(in IScreenDataProviderCallback provider);

    // 4
    int sensorAuxCtrl(in boolean enable);

    // 5
    int sensorAuxExecuteCmd(in int cmd, in @nullable SensorAuxArgs args);

    // 6
    int setSensorAuxConfig(in @nullable SensorAuxConfig config);

    // 7
    SAuxStatus unregisterScreenDataProvider(in int token);

    // 8
    SAuxStatus updateRGBAvg(in int value1, in int value2);

    // 9
    SAuxStatus updateSensorStatus(in int value1, in boolean flag);

    // 10
    void writeFPSToPromixity(in float fps);
}
