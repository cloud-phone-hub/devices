// vendor.meizu.hardware.meizu_charging.IMeizuCharging v1 —— 逆向件。
//
// 来源：原厂 flyme-m22 vendor.img 的 lib64/vendor.meizu.hardware.meizu_charging-V1-ndk.so
// （MEIZU 私有 HAL，AOSP/LineageOS 无公开 AIDL 源码）。事务表与参数形状由该 .so 反解，
// 证据见 engine/evidence/m22/issue-m22-meizu-charging-aidl/。
//
// 反解口径（llvm-objdump -d + llvm-nm -D）：
//   * BpMeizuCharging 的 11 个方法各一条 AIBinder_transact，w1=事务码 1..11，
//     w4 全部是 0x10000000（同步，非 oneway）；
//   * 读侧只有 [StatusHeader][返回值]，没有 out 参数；
//   * 6 个枚举入参在 wire 上都是 AParcel_writeInt32（.so 里没有它们的 writeToParcel，
//     说明是 AIDL enum 而不是 parcelable），故枚举声明只管类型，不参与 parcel 字段序；
//   * 返回值类型：Bool（set 系列）/ Int32（SOH、两个阈值读取）/ String（tx model）。

package vendor.meizu.hardware.meizu_charging;

// 同包跨文件也要显式 import（aidl 只在 import 路径里按全限定名解析类型）。
import vendor.meizu.hardware.meizu_charging.BatteryUPSMode;
import vendor.meizu.hardware.meizu_charging.CharingThresholdCtrl;
import vendor.meizu.hardware.meizu_charging.ReverseWirelssChargingMode;
import vendor.meizu.hardware.meizu_charging.WiredCharingLevel;
import vendor.meizu.hardware.meizu_charging.WirelessChargingLevel;
import vendor.meizu.hardware.meizu_charging.WirelessTxFanMode;

@VintfStability
interface IMeizuCharging {
    // code 1：无线充电发射端型号（字符串）。无硬件 → 空串。
    String getWirelessTxModel();
    // code 2：反向无线充电模式（手机当发射端）。
    boolean setReverseWirelssChargingMode(ReverseWirelssChargingMode mode);
    // code 3：有线充电档位。
    boolean setWiredCharingLevel(WiredCharingLevel level);
    // code 4：无线充电档位。
    boolean setWirelessChargingLevel(WirelessChargingLevel level);
    // code 5：无线发射端风扇模式。
    boolean setWirelessTxFanMode(WirelessTxFanMode mode);
    // code 6：电池健康度 SOH（百分比）。未知 → -1。
    int getBatterySOH();
    // code 7：UPS 模式（对外供电策略）。
    boolean setBatteryUPSMode(BatteryUPSMode mode);
    // code 8：充电阈值控制的当前状态。
    int getCharingThresholdCtrl();
    // code 9：设置充电阈值控制。
    boolean setCharingThresholdCtrl(CharingThresholdCtrl ctrl);
    // code 10：当前充电停止阈值（百分比）。
    int getCharingThreshold();
    // code 11：设置充电停止阈值（百分比）。
    boolean setCharingThreshold(int threshold);
}
