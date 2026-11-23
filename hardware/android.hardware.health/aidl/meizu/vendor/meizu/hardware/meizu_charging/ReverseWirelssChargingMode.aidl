// AIDL enum（wire = int32）。原厂成员名/取值不可得（.so 里没有 toString/成员符号），
// 这里只为通过类型检查：本云实现不解释枚举值，且接口 hash 由 .hash 固定，成员名不进 wire。

package vendor.meizu.hardware.meizu_charging;

@VintfStability
enum ReverseWirelssChargingMode {
    UNKNOWN = 0,
}
