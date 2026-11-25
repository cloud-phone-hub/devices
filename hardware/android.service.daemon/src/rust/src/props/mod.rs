//! guest 属性修正与启动身份（原 boardid 模块 + miui 修复合并为 props）。
//!
//! 单实例稳定的启动身份：SoC（ro.boot.chipid / ro.boot.cpuid）与序号（serialno）。
//!
//! 真机链路（gold）：
//!   * chipid/cpuid 来自 MTK bootloader/efuse（硬件唯一只读）；`ro.boot.chipid` 命中
//!     plat 的 `ro.boot.` 前缀规则 → bootloader_prop，`ro.boot.cpuid` 命中 gold 规则
//!     → vendor_cpuid_prop。
//!   * serialno 一个源头三处落地：cmdline `androidboot.serialno` → init 设
//!     `ro.boot.serialno`，并据 ExportKernelBootProps 派生 `ro.serialno`；
//!     MIUI 的 mitee rc 再从 `ro.boot.serialno` 派生 `ro.vendor.oem.serialno`。
//!
//! 云机：没有对应硬件；宿主侧也注入不了（内核 cmdline 由镜像内 GRUB 拼装，QEMU 没有
//! -append 通道）。因此在 guest daemon 里按**实例**生成一次并落盘缓存（identity.rs）：
//! 值存 /data/vendor/cdh/boot_identity，跨重启复用，避免"每次重启换机"的指纹。
//!
//! 写入用 resetprop::set_direct（map 属性区 RW 直写，绕过 property_service 的 SELinux
//! 检查）：ro.boot.cpuid 的类型 vendor_cpuid_prop 只授 init 做 set，走 property_service
//! 会被拒 —— 与 daemon 现有的 ro.adb.secure / ro.miui.product.home 同一机制。属性区文件
//! 按 contexts 表命名，两侧的类型都有匹配规则，故直写可新增（统一入口见 writer.rs）。
//!
//! serialno 为什么三项都显式写：真机的两条派生链（init 的 cmdline 派生、MIUI 的 mitee rc）
//! 在本环境都不存在，故一次写齐 ro.boot.serialno / ro.serialno / ro.vendor.oem.serialno，
//! 与真机同值同口径。
//!
//! 模块拆分（机型多时按职责找文件，机型差异只改 profile.rs）：
//!   * profile.rs  —— 机型档案表：平台闸门 / boot_devices / 盘符映射 / IMEI 缺省等
//!                    随机型变化的口径，一行一平台；执行侧不含机型分支；
//!   * identity.rs —— 实例身份（chipid/cpuid/serialno）生成与落盘缓存，机型无关；
//!   * writer.rs   —— 属性写入/删除统一入口（ro. 直写、非 ro. 走 property_service）；
//!   * scrub.rs    —— 虚拟化痕迹清理（引导期 + boot_completed 后），按档案执行；
//!   * fixups.rs   —— 各属性兜底（VB / bootimage 指纹 / 基带语言 / 蓝牙名 / IMEI）；
//!   * miui.rs     —— MIUI 专属修复（品牌闸门，如 ro.miui.product.home 主屏设定）。

mod fixups;
mod identity;
pub mod miui;
mod profile;
mod scrub;
mod writer;

use writer::set_prop;

pub fn apply() {
    let p = profile::resolve();
    scrub::scrub_early_virtual_props(&p);
    fixups::sync_vendor_display_and_brand_props();
    fixups::ensure_boot_security_props();
    fixups::ensure_bootimage_fingerprint_props();
    fixups::ensure_telephony_and_locale_defaults();
    fixups::ensure_bluetooth_default_name(&p);
    let id = match identity::load_or_create() {
        Some(v) => v,
        None => return, // load_or_create 已记录原因
    };
    // chipid/cpuid 是 MTK bootloader/efuse 口径（见模块注释）：Tensor/高通等
    // 非 MTK 平台真机没有这两项（Pixel 6 实测无），按档案闸门跳过，避免
    // 给非 MTK 机型造出真机不存在的属性；serialno 各平台都有，不受闸门影响。
    if p.has_chipid {
        set_prop("ro.boot.chipid", &format!("0x{}", id.chipid));
        set_prop("ro.boot.cpuid", &format!("0x{}", id.cpuid));
    }
    set_prop("ro.boot.serialno", &id.serialno);
    set_prop("ro.serialno", &id.serialno);
    set_prop("ro.vendor.oem.serialno", &id.serialno);
    fixups::ensure_oem_modem_identity(&p, &id.serialno);
}

/// boot_completed 后的后台清理（独立线程，见 scrub.rs）。档案在此重新解析：
/// 与 apply 同一口径，且线程启动时属性已稳定，不依赖 apply 的调用时序。
pub fn spawn_post_boot_scrub() {
    scrub::spawn_post_boot_scrub(profile::resolve());
}