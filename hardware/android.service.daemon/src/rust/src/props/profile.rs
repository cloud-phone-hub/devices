//! 机型档案：props 各属性兜底/清理逻辑里**随机型变化的部分**全部收敛到本文件。
//!
//! 结构：
//!   * `PROFILES`：按 `ro.board.platform` 精确匹配的静态档案表，一行一个平台口径
//!     （注释标明对应机型）。新增机型 = 加一行；平台口径相同则复用既有行；
//!   * `resolve()`：静态档案 + 运行期 UI 特征（MIUI / 品牌）合成最终档案。
//!     UI 特征必须运行期判定：同一平台可能承载不同 UI 的机型，且未知平台
//!     也要按真实属性走兜底，不能在表里写死。
//!
//! 执行逻辑（scrub.rs / fixups.rs / mod.rs）只读档案字段，不再出现机型分支；
//! 机型专属的**属性值**（vbmeta.size、imei.tac、meid.prefix、baseband 等）仍走
//! 清单 build-prop-sets 注入的 `ro.vendor.cdh.*` / `ro.vendor.radio.*` 属性，
//! 档案里只放无法经清单注入的结构性差异（引导器路径、分区盘符映射、平台闸门）。

use crate::log::kmsg;

/// virtio 盘符 → 真机 UFS 分区名映射表（dev.mnt.{blk,dev,rootdisk}.* 收敛用）。
/// 其他尚未登记精确拓扑的机型共用此 18 条通用映射表。
pub(super) const MOUNT_REWRITES_COMMON: &[(&str, &str, &str)] = &[
    ("cust", "sdc56", "sdc56"),
    ("metadata", "sdc14", "sdc14"),
    ("mnt.rescue", "sdc57", "sdc57"),
    ("mnt.vendor.nvcfg", "sdc6", "sdc6"),
    ("mnt.vendor.nvdata", "sdc7", "sdc7"),
    ("mnt.vendor.persist", "sdc15", "sdc15"),
    ("mnt.vendor.protect_f", "sdc18", "sdc18"),
    ("mnt.vendor.protect_s", "sdc19", "sdc19"),
    ("data", "sdc66", "dm-81"),
    ("data.user.0", "sdc66", "dm-81"),
    ("data_mirror.cur_profiles", "sdc66", "dm-81"),
    ("data_mirror.data_ce.null", "sdc66", "dm-81"),
    ("data_mirror.data_ce.null.0", "sdc66", "dm-81"),
    ("data_mirror.data_de.null", "sdc66", "dm-81"),
    ("data_mirror.misc_ce.null", "sdc66", "dm-81"),
    ("data_mirror.misc_de.null", "sdc66", "dm-81"),
    ("data_mirror.ref_profiles", "sdc66", "dm-81"),
    ("data_mirror.storage_area", "sdc66", "dm-81"),
];

/// gold（Redmi Note 13 5G，15-OS3.0.11.0.VNQCNXM）真机实测全量 27 个挂载后缀（81 项 dev.mnt.* 属性）。
pub(super) const MOUNT_REWRITES_GOLD: &[(&str, &str, &str)] = &[
    ("cust", "sdc56", "sdc56"),
    ("metadata", "sdc14", "sdc14"),
    ("mnt.rescue", "sdc57", "sdc57"),
    ("mnt.vendor.nvcfg", "sdc6", "sdc6"),
    ("mnt.vendor.nvdata", "sdc7", "sdc7"),
    ("mnt.vendor.persist", "sdc15", "sdc15"),
    ("mnt.vendor.protect_f", "sdc18", "sdc18"),
    ("mnt.vendor.protect_s", "sdc19", "sdc19"),
    ("root", "sdc65", "dm-10"),
    ("system_ext", "sdc65", "dm-11"),
    ("vendor", "sdc65", "dm-12"),
    ("product", "sdc65", "dm-13"),
    ("mi_ext", "sdc65", "dm-14"),
    ("mnt.vendor.mi_ext", "sdc65", "dm-14"),
    ("vendor_dlkm", "sdc65", "dm-15"),
    ("odm_dlkm", "sdc65", "dm-16"),
    ("system_dlkm", "sdc65", "dm-17"),
    ("data", "sdc66", "dm-58"),
    ("data.user.0", "sdc66", "dm-58"),
    ("data_mirror.cur_profiles", "sdc66", "dm-58"),
    ("data_mirror.data_ce.null", "sdc66", "dm-58"),
    ("data_mirror.data_ce.null.0", "sdc66", "dm-58"),
    ("data_mirror.data_de.null", "sdc66", "dm-58"),
    ("data_mirror.misc_ce.null", "sdc66", "dm-58"),
    ("data_mirror.misc_de.null", "sdc66", "dm-58"),
    ("data_mirror.ref_profiles", "sdc66", "dm-58"),
    ("data_mirror.storage_area", "sdc66", "dm-58"),
];

/// 一个平台/机型的属性口径档案。字段即差异点，执行侧按字段取值，不写机型分支。
pub(super) struct DeviceProfile {
    /// 档案标识（日志用）
    pub name: &'static str,
    /// MTK bootloader/efuse 口径：真机有 ro.boot.chipid / ro.boot.cpuid。
    /// Tensor/高通等平台真机没有这两项（Pixel 6 实测无），不得凭空造。
    pub has_chipid: bool,
    /// 保留 ro.boot.theme：Pixel（gs101）真机 bootloader 传 theme=1；
    /// MTK 等真机不暴露（grub 的 androidboot.theme 是虚拟化产物），需删除。
    pub keep_boot_theme: bool,
    /// ro.boot.boot_devices=any 时收敛到的真实 UFS/MMC 控制器路径
    pub boot_devices: &'static str,
    /// dev.mnt.* virtio 盘符收敛表（suffix, blk 值, dev 值；rootdisk 统一 sdc）
    pub mount_rewrites: &'static [(&'static str, &'static str, &'static str)],
    /// 是否强制全量对齐 mount_rewrites（含缺失的 super 逻辑分区与 data* 属性），
    /// 并仅清理 EFI/vendorpatch 载体痕迹；false 时保持传统删减模式。
    pub enforce_exact_mounts: bool,
    /// 是否兜底 bluetooth.device.default_name（resolve 按 UI/品牌判定：
    /// MIUI/HyperOS 与 Google Pixel 真机都不定义该属性，名称由
    /// Settings.Secure.bluetooth_name 承载，设了反而暴露独有属性）
    pub bluetooth_default_name: bool,
    /// 是否按实例 serialno 派生 OEM/RIL IMEI 与 MEID（resolve 按 MIUI 判定）
    pub oem_modem_identity: bool,
    /// IMEI TAC 缺省（清单 ro.vendor.radio.imei.tac 优先；gold 官方 GSMA TAC）
    pub imei_tac_default: &'static str,
    /// MEID 前缀缺省（清单 ro.vendor.radio.meid.prefix 优先；gold 官方口径）
    pub meid_prefix_default: &'static str,
    /// 根 tmpfs 超级块容量/inode 上限对齐（Some((size_kb, nr_inodes))）：
    /// 真机 first_stage_init 在 free_initmem() 前挂载 /dev、/mnt、/apex 等 tmpfs，
    /// 使 sbinfo->max_blocks 固定为早期 totalram_pages()/2，free_initmem() 后因与新的
    /// shmem_default_max_blocks() 不等而在 /proc/mounts 显式打印 size=...k,nr_inodes=...。
    pub tmpfs_limits: Option<(u64, u64)>,
}

/// 静态档案表：按 ro.board.platform 精确匹配，未命中走 DEFAULT_PROFILE。
/// 新增机型在此加行；同平台多机型口径一致时共用一行（如 gold 与 note13-5G-15）。
const PROFILES: &[(&str, DeviceProfile)] = &[
    (
        // gold / note13-5G-15（Redmi Note 13 5G，MTK Dimensity 700 系）
        "mt6833",
        DeviceProfile {
            name: "mt6833",
            has_chipid: true,
            keep_boot_theme: false,
            boot_devices: "bootdevice, soc/11230000.mmc, 11230000.mmc, soc/11230000.msdc, 11230000.msdc, soc/11270000.ufshci, 11270000.ufshci",
            mount_rewrites: MOUNT_REWRITES_GOLD,
            enforce_exact_mounts: true,
            bluetooth_default_name: false, // resolve 按运行期 UI/品牌复核
            oem_modem_identity: false,     // resolve 按运行期 MIUI 复核
            imei_tac_default: "86717107",
            meid_prefix_default: "99002158",
            tmpfs_limits: Some((3_859_852, 964_963)),
        },
    ),
    (
        // oriole（Pixel 6，Tensor gs101）
        "gs101",
        DeviceProfile {
            name: "gs101",
            has_chipid: false,
            // Pixel 6 真机 bootloader 传 theme=1（云机实测亦为 1，与真机一致）
            keep_boot_theme: true,
            // Pixel 6 真机实测 ro.boot.boot_devices=14700000.ufs（Tensor UFS 控制器）
            boot_devices: "14700000.ufs",
            mount_rewrites: MOUNT_REWRITES_COMMON,
            enforce_exact_mounts: false,
            bluetooth_default_name: false,
            oem_modem_identity: false,
            imei_tac_default: "86717107",
            meid_prefix_default: "99002082",
            tmpfs_limits: None,
        },
    ),
];

/// 未收录平台的兜底档案：boot_devices 用通用 UFS 口径，其余与 MTK 通用一致；
/// UI 相关闸门仍由 resolve 按运行期属性判定。
const DEFAULT_PROFILE: DeviceProfile = DeviceProfile {
    name: "default",
    has_chipid: false,
    keep_boot_theme: false,
    boot_devices: "bootdevice,soc/1d84000.ufshci",
    mount_rewrites: MOUNT_REWRITES_COMMON,
    enforce_exact_mounts: false,
    bluetooth_default_name: false,
    oem_modem_identity: false,
    imei_tac_default: "86717107",
    meid_prefix_default: "99002082",
    tmpfs_limits: None,
};

/// MTK 通用档案：未精确收录的 mt* 平台（mt6989 / mt6993 等）共用。
const MTK_PROFILE: DeviceProfile = DeviceProfile {
    name: "mtk",
    has_chipid: true,
    keep_boot_theme: false,
    boot_devices: "bootdevice,soc/112b0000.ufshci,112b0000.ufshci",
    mount_rewrites: MOUNT_REWRITES_COMMON,
    enforce_exact_mounts: false,
    bluetooth_default_name: false,
    oem_modem_identity: false,
    imei_tac_default: "86717107",
    meid_prefix_default: "99002158",
    tmpfs_limits: None,
};

/// 解析当前实例的机型档案：静态表按 ro.board.platform 匹配（精确优先，
/// 其次 mt* 前缀，最后兜底），再叠加运行期 UI 特征。
pub(super) fn resolve() -> DeviceProfile {
    let platform = resetprop::get("ro.board.platform")
        .unwrap_or_default()
        .trim()
        .to_string();
    let mut p = match_profile(&platform);

    // UI 特征运行期判定（不随平台写死）：
    // - MIUI/HyperOS：派生 OEM/RIL IMEI/MEID，且不设蓝牙默认名；
    // - google 品牌：同样不设蓝牙默认名（Pixel 真机无此属性）。
    let is_miui = !resetprop::get("ro.miui.ui.version.name")
        .unwrap_or_default()
        .trim()
        .is_empty();
    let brand = resetprop::get("ro.product.brand")
        .unwrap_or_default()
        .trim()
        .to_lowercase();
    p.oem_modem_identity = is_miui;
    p.bluetooth_default_name = !is_miui && brand != "google";

    kmsg(&format!("boot identity: 机型档案 {}（platform={platform}）", p.name));
    p
}

/// 平台 → 静态档案（纯函数，便于单测）：精确匹配 > mt* 前缀 > 兜底。
fn match_profile(platform: &str) -> DeviceProfile {
    for (key, profile) in PROFILES {
        if *key == platform {
            return clone_profile(profile);
        }
    }
    if platform.starts_with("mt") {
        return clone_profile(&MTK_PROFILE);
    }
    clone_profile(&DEFAULT_PROFILE)
}

/// DeviceProfile 全是 &'static 数据，逐字段拷贝即深拷贝。
fn clone_profile(p: &DeviceProfile) -> DeviceProfile {
    DeviceProfile {
        name: p.name,
        has_chipid: p.has_chipid,
        keep_boot_theme: p.keep_boot_theme,
        boot_devices: p.boot_devices,
        mount_rewrites: p.mount_rewrites,
        enforce_exact_mounts: p.enforce_exact_mounts,
        bluetooth_default_name: p.bluetooth_default_name,
        oem_modem_identity: p.oem_modem_identity,
        imei_tac_default: p.imei_tac_default,
        meid_prefix_default: p.meid_prefix_default,
        tmpfs_limits: p.tmpfs_limits,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn exact_platform_wins() {
        let p = match_profile("mt6833");
        assert_eq!(p.name, "mt6833");
        assert!(p.has_chipid);
        assert!(!p.keep_boot_theme);
        assert!(p.boot_devices.contains("11230000.mmc"));
        assert!(p.enforce_exact_mounts);
        assert_eq!(p.mount_rewrites.len(), 27);
        assert_eq!(p.meid_prefix_default, "99002158");
        assert_eq!(p.tmpfs_limits, Some((3_859_852, 964_963)));
    }

    #[test]
    fn gs101_keeps_theme_and_no_chipid() {
        let p = match_profile("gs101");
        assert!(!p.has_chipid);
        assert!(p.keep_boot_theme);
        assert!(!p.enforce_exact_mounts);
        assert_eq!(p.boot_devices, "14700000.ufs");
    }

    #[test]
    fn unknown_mtk_falls_to_mtk_profile() {
        let p = match_profile("mt6993");
        assert_eq!(p.name, "mtk");
        assert!(p.has_chipid);
        assert!(!p.enforce_exact_mounts);
        assert_eq!(p.meid_prefix_default, "99002158");
    }

    #[test]
    fn unknown_platform_falls_to_default() {
        let p = match_profile("exynos2100");
        assert_eq!(p.name, "default");
        assert!(!p.has_chipid);
        assert!(!p.keep_boot_theme);
        assert!(!p.enforce_exact_mounts);
    }
}