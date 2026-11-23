/*
 * vendor.awa.reality —— MEIZU/awa 触感（RealityTap）HAL v3。
 *
 * 逆向来源：原厂 lib64/vendor.awa.reality-V3-ndk.so（服务端实现
 * lib64/vendor.awa.realitytap.impl.so，二进制 bin/hw/vendor.awa.realitytap.service）。
 * 事务表（AArch64 反汇编，engine/out/tmp/rev2/rev_any.py 抽取）：
 *   code 1 performTap(RealityTapHe, EffectStrength, IVibratorCallback) -> int
 *          写序：int32(1) → RealityTapHe.writeToParcel → int8(EffectStrength，@Backing byte)
 *                → IVibratorCallback.writeToParcel
 *   code 2 performRtp(RealityTapRtp, EffectStrength, IVibratorCallback) -> int   （同上）
 *   code 3 cancel()    -> int
 *   code 4 calibrate() -> int
 *   code 5 getF0()     -> int
 * 接口 hash = 054e790771427edda2465b91648721f3d1033d4b（.so 内 40 位十六进制串，
 * 与 vibrator v2 的 ea8742d6… 区分后确认），逐字写进 3/.hash。
 *
 * 与 android.hardware.vibrator 的关系：v3 起本接口不再自造强度枚举，直接复用
 * 平台 android.hardware.vibrator.EffectStrength（@Backing(type="byte")，wire 上是
 * 1 字节）与 IVibratorCallback（oneway onComplete）。故本模块 spec 同时声明
 * aidlRootModule 与 depsHal=vibrator，跨包取平台 AIDL 快照（见 cdh-vendor 的
 * aidlRoots()）。
 */
package vendor.awa.reality;

import android.hardware.vibrator.EffectStrength;
import android.hardware.vibrator.IVibratorCallback;
import vendor.awa.reality.RealityTapHe;
import vendor.awa.reality.RealityTapRtp;

@VintfStability
interface IRealityTap {
  int performTap(in RealityTapHe he, in EffectStrength strength, in IVibratorCallback callback);
  int performRtp(in RealityTapRtp rtp, in EffectStrength strength, in IVibratorCallback callback);
  int cancel();
  int calibrate();
  int getF0();
}
