/*
 * 三星 SEH 健康信息（无 AOSP 官方源，随库维护）。
 *
 * 字段顺序按官方 .so 的 SehHealthInfo::writeToParcel 反汇编逐条取证：
 *
 *   c42c  HealthInfo::writeToParcel(this, parcel)   ← 首字段就是 AOSP 的 HealthInfo
 *   c43c  ldr w1, [this, #0xf0] + writeInt32        ← ext[0]
 *   c44c  ldr w1, [this, #0xf4] + writeInt32        ← ext[1]
 *   c45c  ldr w1, [this, #0xf8] + writeInt32        ← ext[2]
 *   c46c  ldrb   [this, #0xfc] + writeBool          ← ext[3]
 *   c47c  ldrb   [this, #0xfd] + writeBool          ← ext[4]
 *   c48c  ldr w1, [this, #0x100] + writeInt32       ← ext[5]
 *   c49c  ldr w1, [this, #0x104] + writeInt32       ← ext[6]
 *   c4ac  ldr w1, [this, #0x108] + writeInt32       ← ext[7]
 *   c4bc  ldrb   [this, #0x10c] + writeBool         ← ext[8]
 *   c4cc  ldr w1, [this, #0x110] + writeInt32       ← ext[9]
 *
 * 即 AOSP HealthInfo（三星编译态占 0xf0 字节）后面跟 10 个私有基础类型字段。
 * 私有字段的语义没有公开资料，云机侧一律置 0：三星 BatteryService 的
 * 电量/充电状态全部取自前面的 HealthInfo，私有段只是增强上报（充电策略、
 * 老化数据等），缺省 0 与官方件在"无电池硬件"时的默认值一致。
 *
 * 注意：本文件只保证**序列化顺序与类型**与官方一致（客户端按此顺序解析）；
 * 内存布局不必对齐，因为我们自己生成自己消费，不与官方 .so 混链。
 */
package vendor.samsung.hardware.health;

import android.hardware.health.HealthInfo;

@VintfStability
parcelable SehHealthInfo {
    android.hardware.health.HealthInfo healthInfo;
    /** 私有扩展位 0..9：类型与顺序见文件头取证，语义未公开。 */
    int sehInt0;
    int sehInt1;
    int sehInt2;
    boolean sehBool3;
    boolean sehBool4;
    int sehInt5;
    int sehInt6;
    int sehInt7;
    boolean sehBool8;
    int sehInt9;
}
