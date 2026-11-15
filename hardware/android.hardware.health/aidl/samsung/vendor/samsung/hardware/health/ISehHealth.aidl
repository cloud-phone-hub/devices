/*
 * 三星 SEH 健康扩展接口（无 AOSP 官方源，随库维护）。
 *
 * 来源：三星官方 vendor 的 lib64/vendor.samsung.hardware.health-V1-ndk.so
 * （SM-S9380 官方 vendor_a.img 抽取件）。三星没有公开 .aidl，本文件是按该 .so
 * 的 AIDL 生成代码逐项取证后重写的等价定义：
 *
 *   - descriptor：.rodata 里 "vendor.samsung.hardware.health.ISehHealth"；
 *   - 接口 hash：.rodata 里 "632113d2738773fc3ee67a40cf37aa99a91f8bac"
 *     （BpSehHealth::getInterfaceHash 从 ISehHealth::hash 取，即该常量）；
 *   - 版本：BnSehHealth::getInterfaceVersion 反汇编为 "mov w8, #0x1" → version 1；
 *   - 方法顺序 = transaction code（BpSehHealth 各方法 AIBinder_transact 的 w1）：
 *       registerCallback=1, unregisterCallback=2, update=3, sehWriteEnableToParam=4
 *     4 个方法都无返回值（transact 后只读异常码，不读结果段）。
 *
 * 关于 interface hash：本包的 codegen 不传 --hash（hasHash=false，三星不公开 .hash），
 * 生成代码里没有 hash 常量、也没有 getInterfaceHash 方法。这在本机型的链路里是安全的：
 * 客户端是 OneUI 的 Java 侧，走 `ISehHealth.Stub.asInterface(binder)` 生成 $Stub$Proxy
 * （崩溃栈里的类名就是 `ISehHealth$Stub$Proxy`），该方法只做 queryLocalInterface，
 * 不校验 hash；跨进程只依赖 descriptor（AIBinder_associateClass 校验）＋版本＋
 * transaction code，三者都和官方对齐。官方件自带的 hash 仅用于 native 客户端
 * （BpSehHealth::getInterfaceHash），而本方案里不存在 native 客户端。
 * 若以后出现 native 客户端：在 aidl/samsung 根下放 `.hash`（内容即上面的常量）
 * 并把模块 variants.samsung.extraAidlRoots 的 hasHash 改成 true——注意现在的 codegen
 * 还没实现 extraAidlRoots 的 --hash，需要同步补 halbuild/codegen.go。
 *
 * 为什么必须有这份定义：OneUI 的 system_server 先取
 * android.hardware.health.IHealth/default，再对同一个 binder 调
 * AIBinder_getExtension() 拿 ISehHealth（三星官方服务用 AIBinder_setExtension
 * 把 ISehHealth 挂在 IHealth binder 上，不是独立注册的命名服务，所以
 * servicemanager 里看不到 ISehHealth 实例，官方 VINTF 片段也不声明它）。
 * 拿不到 extension 时 HealthRegCallbackAidl.onRegistration 对 null 代理调
 * registerCallback → NPE → BatteryService 起不来 → system_server 自杀循环。
 *
 * 本机型由自研健康 HAL 自己实现该接口并用 AIBinder_setExtension 挂到自研的
 * IHealth binder 上，从而不再依赖三星二进制（也就保住了 battery_config.xml
 * 的模拟电量能力）。
 */
package vendor.samsung.hardware.health;

import vendor.samsung.hardware.health.ISehHealthInfoCallback;

@VintfStability
interface ISehHealth {
    void registerCallback(in ISehHealthInfoCallback callback);
    void unregisterCallback(in ISehHealthInfoCallback callback);
    void update();
    void sehWriteEnableToParam(int param, boolean enable);
}
