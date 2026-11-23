// framework（system_server 内 libsensorservice）实现、由 sensor_aux HAL 回调的二级接口。
//
// 方法顺序与事务码取自官方 NDK 桩 BpScreenDataProviderCallback 反汇编里
// `AIBinder_transact` 的 code 实参（disable=1 / enable=2 / setArea=3，§V7-5）；
// 编解码序列：disable 无参无回值、enable 写 int32 读 bool、setArea 写 int32+Rect 读 int32。
// 本仓的桩**从不调用**该回调（只做注册与快速回话），此处声明只为与官方接口面对齐。
package vendor.meizu.sensor_aux;

import vendor.meizu.sensor_aux.Rect;
import vendor.meizu.sensor_aux.SAuxStatus;
import vendor.meizu.sensor_aux.ScreenDataProviderWorkMode;

@VintfStability
interface IScreenDataProviderCallback {
    void disable();

    boolean enable(in ScreenDataProviderWorkMode mode);

    SAuxStatus setArea(in @nullable Rect area);
}
