/*
 * 三星 SEH 健康回调（无 AOSP 官方源，随库维护）。
 *
 * 取证（官方 vendor.samsung.hardware.health-V1-ndk.so）：
 *   - descriptor："vendor.samsung.hardware.health.ISehHealthInfoCallback"
 *   - 接口 hash："3bab6273a5491102b29c9d7a1f0efa749533f46d"
 *   - 唯一方法 healthInfoChanged(SehHealthInfo)，transaction code = 1
 *   - 不是 oneway：BpSehHealthInfoCallback::healthInfoChanged 的
 *     AIBinder_transact flags 实测为 0x10000000（无 FLAG_ONEWAY），即同步调用。
 *
 * 本云机的自研健康 HAL 只做该接口的**客户端**（服务端在 OneUI framework 侧），
 * 用来把 SehHealthInfo 推给三星 BatteryService。
 */
package vendor.samsung.hardware.health;

import vendor.samsung.hardware.health.SehHealthInfo;

@VintfStability
interface ISehHealthInfoCallback {
    void healthInfoChanged(in SehHealthInfo info);
}
