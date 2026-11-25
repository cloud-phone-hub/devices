//! 属性兜底补齐：Verified Boot / bootimage 指纹 / 基带与语言 / 蓝牙默认名 /
//! OEM modem 身份。机型差异（是否设蓝牙名、是否派生 IMEI/MEID、TAC/MEID 缺省值）
//! 全部来自 profile.rs 的机型档案；机型专属属性值（vbmeta.size、imei.tac、
//! meid.prefix、baseband）优先读清单 build-prop-sets 注入的 ro.vendor.* 属性。

use super::profile::DeviceProfile;
use super::writer::{set_prop, set_prop_svc};

/// 兜底补齐引导链可能遗漏的 Verified Boot / 启动模式只读属性（bootloader_prop 对所有 App 可读）。
/// 若 vendor_boot / grub cmdline 已显式注入非空值，则保留原值不覆盖；
/// vbmeta.size 优先读取机型配置属性 ro.vendor.cdh.vbmeta.size。
pub(super) fn ensure_boot_security_props() {
    let mut vbmeta_size = resetprop::get("ro.vendor.cdh.vbmeta.size")
        .unwrap_or_default()
        .trim()
        .to_string();
    if vbmeta_size.is_empty() {
        vbmeta_size = "11904".to_string();
    }
    for (key, default_val) in [
        ("ro.boot.veritymode", "enforcing"),
        ("ro.boot.mode", "normal"),
        ("ro.boot.vbmeta.avb_version", "1.2"),
        ("ro.boot.vbmeta.hash_alg", "sha256"),
        ("ro.boot.vbmeta.size", vbmeta_size.as_str()),
    ] {
        let cur = resetprop::get(key).unwrap_or_default();
        if cur.trim().is_empty() {
            set_prop(key, default_val);
        }
    }
    if resetprop::get("ro.boot.vbmeta.device")
        .unwrap_or_default()
        .trim()
        .is_empty()
    {
        let vbmeta_dev = resetprop::get("ro.vendor.cdh.vbmeta.device")
            .unwrap_or_default()
            .trim()
            .to_string();
        if !vbmeta_dev.is_empty() {
            set_prop("ro.boot.vbmeta.device", &vbmeta_dev);
        }
    }
}

/// 同步 bootimage 分区构建属性（ro.bootimage.build.*）：
/// 云机的 boot ramdisk 取自通用基座，未携带厂商 boot.img 内的 prop.default，导致
/// `ro.bootimage.build.fingerprint` 等 10 项缺失；当同机已有官方 `ro.vendor.bootimage.build.*`
/// （或回退 `ro.system.build.*`）时，用 set_direct 补齐对应只读属性，消除分区指纹缺口。
pub(super) fn ensure_bootimage_fingerprint_props() {
    if !resetprop::get("ro.bootimage.build.fingerprint")
        .unwrap_or_default()
        .trim()
        .is_empty()
    {
        return;
    }
    const SUFFIXES: &[&str] = &[
        "date",
        "date.utc",
        "fingerprint",
        "id",
        "tags",
        "type",
        "version.incremental",
        "version.release",
        "version.release_or_codename",
        "version.sdk",
    ];
    for suffix in SUFFIXES {
        let target = format!("ro.bootimage.build.{suffix}");
        if !resetprop::get(&target).unwrap_or_default().trim().is_empty() {
            continue;
        }
        let vendor_src = format!("ro.vendor.bootimage.build.{suffix}");
        let vendor_build_src = format!("ro.vendor.build.{suffix}");
        let sys_src = format!("ro.system.build.{suffix}");
        let build_src = format!("ro.build.{suffix}");
        for src in [&vendor_src, &vendor_build_src, &sys_src, &build_src] {
            let val = resetprop::get(src).unwrap_or_default();
            let trimmed = val.trim();
            if !trimmed.is_empty() {
                set_prop(&target, trimmed);
                break;
            }
        }
    }
}

/// 兜底设置基带版本/RIL 版本说明与系统默认语言属性：
/// - `gsm.version.baseband` / `gsm.version.ril-impl`：清单经 build-prop-sets 写进
///   /vendor/build.prop，但两键属 radio_prop 上下文，部分机型（如 oriole/gs101）
///   sepolicy 不允许 vendor_init 装载（init 报 SELinux permission check failed），
///   故属性为空时先取 build.prop 里的清单值经 property_service 补写（pioneer 域
///   permissive 放行；不能用直写，见 writer::set_prop_svc 注释）；ril-impl 无清单值
///   时再回退 "android reference-ril 1.0"（真机 RILD 启动后写入的口径）；
/// - `persist.sys.locale`：新 userdata 首次启动若为空，回退取 `ro.product.locale`（如 "zh-CN"）。
pub(super) fn ensure_telephony_and_locale_defaults() {
    for key in ["gsm.version.baseband", "gsm.version.ril-impl"] {
        if !resetprop::get(key).unwrap_or_default().trim().is_empty() {
            continue;
        }
        if let Some(val) = vendor_build_prop_value(key) {
            set_prop_svc(key, &val);
        }
    }
    if resetprop::get("gsm.version.ril-impl")
        .unwrap_or_default()
        .trim()
        .is_empty()
    {
        set_prop_svc("gsm.version.ril-impl", "android reference-ril 1.0");
    }
    if resetprop::get("persist.sys.locale")
        .unwrap_or_default()
        .trim()
        .is_empty()
    {
        let locale = resetprop::get("ro.product.locale").unwrap_or_default();
        let trimmed = locale.trim();
        if !trimmed.is_empty() {
            set_prop_svc("persist.sys.locale", trimmed);
        }
    }
}

/// 从 /vendor/build.prop 读取指定键的清单注入值（跳过注释/空行，值保留原样不 trim，
/// ril-impl 等值内含空格）。文件不存在或键缺失返回 None。
pub(super) fn vendor_build_prop_value(key: &str) -> Option<String> {
    let data = std::fs::read_to_string("/vendor/build.prop").ok()?;
    let prefix = format!("{key}=");
    for line in data.lines() {
        let line = line.trim();
        if line.is_empty() || line.starts_with('#') {
            continue;
        }
        if let Some(val) = line.strip_prefix(&prefix) {
            if !val.is_empty() {
                return Some(val.to_string());
            }
        }
    }
    None
}

/// 同步 /vendor/build.prop（叠加 /odm/etc/build.prop）中声明的只读硬件/品牌展示属性：
/// 1) Android init 先加载 /system、/product 再加载 /vendor，若官方系统分区预置了只读键
///    （如华硕内测底包预置 `ro.vendor.asus.beta=1`、`ro.vendor.build.asus.sku=ZWW`），
///    或某属性受厂商私有上下文限制（如 `vendor_asus_sp_system_prop` 仅允许 `/vendor/bin/sp`
///    写入而拒绝 `vendor_init`），`/vendor/build.prop` 中的键值会被静默跳过；
///    此处通过 `set_prop`（`resetprop::set_direct`）对齐至清单落盘值；
/// 2) 若机型声明了 `ro.vendor.memsize`（如华硕 About phone 内存展示键），按当前实例
///    `/proc/meminfo` 的真实 `MemTotal` 吸附标准物理容量档位（如 16GB / 24GB），保证多 SKU
///    切换内存启动时无需重打镜像即展示准确容量。
pub(super) fn sync_vendor_display_and_brand_props() {
    let mut merged: Vec<(String, String)> = Vec::new();
    for path in ["/vendor/build.prop", "/odm/etc/build.prop", "/odm/build.prop"] {
        if let Ok(text) = std::fs::read_to_string(path) {
            collect_syncable_ro_props(&text, &mut merged);
        }
    }
    for (key, val) in &merged {
        let cur = resetprop::get(key).unwrap_or_default();
        if cur != *val {
            set_prop(key, val);
        }
    }

    let cur_memsize = resetprop::get("ro.vendor.memsize").unwrap_or_default();
    let cur_ddrsize = resetprop::get("ro.boot.ddrsize").unwrap_or_default();
    let board_plat = resetprop::get("ro.board.platform").unwrap_or_default();
    let soc_mfg = resetprop::get("ro.soc.manufacturer").unwrap_or_default();
    let need_ddrsize = is_unisoc_ddrsize_sync_needed(&cur_ddrsize, &board_plat, &soc_mfg);
    if !cur_memsize.trim().is_empty() || need_ddrsize {
        if let Ok(meminfo) = std::fs::read_to_string("/proc/meminfo") {
            if let Some(gb) = detect_ram_tier_gb_from_meminfo(&meminfo) {
                if !cur_memsize.trim().is_empty() {
                    let target = format!("{gb}GB");
                    if cur_memsize.trim() != target {
                        set_prop("ro.vendor.memsize", &target);
                    }
                }
                if need_ddrsize {
                    let target_ddr = format!("{}M", gb * 1024);
                    if cur_ddrsize.trim() != target_ddr {
                        set_prop("ro.boot.ddrsize", &target_ddr);
                    }
                }
            }
        }
    }
}

fn is_unisoc_ddrsize_sync_needed(cur_ddrsize: &str, board_plat: &str, soc_mfg: &str) -> bool {
    let ddr = cur_ddrsize.trim();
    if !ddr.is_empty() && !ddr.eq_ignore_ascii_case("unconfig") {
        return true;
    }
    let b = board_plat.trim().to_ascii_lowercase();
    let m = soc_mfg.trim().to_ascii_lowercase();
    b.starts_with("ums")
        || b.starts_with("sp9")
        || b.starts_with("sc9")
        || b.starts_with("ud7")
        || b.starts_with("uis")
        || b.starts_with("sharkl")
        || b.starts_with("qogir")
        || m == "unisoc"
        || m == "spreadtrum"
}

fn is_syncable_readonly_vendor_key(key: &str) -> bool {
    key.starts_with("ro.vendor.cdh.")
        || key.starts_with("ro.vendor.asus.")
        || key.starts_with("ro.vendor.build.asus.")
        || key.starts_with("ro.vendor.build.csc.")
        || key.starts_with("ro.vendor.config.")
        || key.starts_with("ro.vendor.atd.")
        || key == "ro.vendor.cpufreq"
        || key == "ro.vendor.memsize"
        || key.starts_with("ro.soc.")
        || key.starts_with("ro.vendor.soc.")
}

fn collect_syncable_ro_props(content: &str, out: &mut Vec<(String, String)>) {
    for raw_line in content.lines() {
        let line = raw_line.trim();
        if line.is_empty() || line.starts_with('#') {
            continue;
        }
        let Some((raw_k, raw_v)) = line.split_once('=') else {
            continue;
        };
        let key = raw_k.trim();
        if !is_syncable_readonly_vendor_key(key) {
            continue;
        }
        let val = raw_v.trim().to_string();
        if let Some(existing) = out.iter_mut().find(|(k, _)| k == key) {
            existing.1 = val;
        } else {
            out.push((key.to_string(), val));
        }
    }
}

fn detect_ram_tier_gb_from_meminfo(meminfo: &str) -> Option<u32> {
    for line in meminfo.lines() {
        let trimmed = line.trim();
        let Some(rest) = trimmed.strip_prefix("MemTotal:") else {
            continue;
        };
        let kb_str = rest.split_whitespace().next()?;
        let kb: u64 = kb_str.parse().ok()?;
        if kb < 512 * 1024 {
            return None;
        }
        const TIERS_GB: &[u32] = &[2, 3, 4, 6, 8, 10, 12, 16, 18, 24, 32, 48, 64];
        for &tier in TIERS_GB {
            let tier_kb = (tier as u64) * 1024 * 1024;
            // 真机 MemTotal 因保留内存略小于标称颗粒容量，留 4% 上浮容差防止微小超出跳档
            if kb <= tier_kb + (tier_kb / 25) {
                return Some(tier);
            }
        }
        return Some(((kb + (1024 * 1024 - 1)) / (1024 * 1024)) as u32);
    }
    None
}

/// 兜底设置蓝牙默认适配器名称（bluetooth.device.default_name）：
/// 是否执行由档案决定（MIUI/HyperOS 与 Google Pixel 真机都不定义该属性，
/// 适配器名称由 Settings.Secure.bluetooth_name 承载，设了反而暴露独有属性）。
pub(super) fn ensure_bluetooth_default_name(p: &DeviceProfile) {
    if !p.bluetooth_default_name {
        return;
    }
    let cur_bt = resetprop::get("bluetooth.device.default_name")
        .unwrap_or_default()
        .trim()
        .to_string();
    let model_code = resetprop::get("ro.product.model")
        .unwrap_or_default()
        .trim()
        .to_string();
    if !cur_bt.is_empty() && cur_bt != model_code {
        return;
    }
    for candidate in [
        "ro.product.marketname",
        "ro.product.vendor.marketname",
        "ro.vendor.cdh.device.market_name",
        "ro.vendor.asus.product.mkt_name",
        "ro.product.model",
    ] {
        let val = resetprop::get(candidate).unwrap_or_default();
        let trimmed = val.trim();
        if !trimmed.is_empty() {
            if cur_bt != trimmed {
                set_prop("bluetooth.device.default_name", trimmed);
            }
            return;
        }
    }
}

/// 按实例 serialno 派生并补齐 OEM/RIL IMEI 与 MEID 只读属性（是否执行由档案决定，
/// 当前仅 MIUI/HyperOS 机型；与 android.hardware.radio 的 radio_modem.cpp 采用
/// 相同 FNV-1a + Luhn 规则，保证系统属性与 RIL getImei / getDeviceIdentity
/// 返回值逐位一致）。TAC / MEID 前缀优先取清单注入属性，缺省用档案值。
pub(super) fn ensure_oem_modem_identity(p: &DeviceProfile, serialno: &str) {
    if !p.oem_modem_identity {
        return;
    }

    let mut tac = resetprop::get("ro.vendor.radio.imei.tac")
        .unwrap_or_default()
        .trim()
        .to_string();
    if tac.len() != 8 || !tac.bytes().all(|b| b.is_ascii_digit()) {
        tac = p.imei_tac_default.to_string();
    }

    let mut meid_prefix = resetprop::get("ro.vendor.radio.meid.prefix")
        .unwrap_or_default()
        .trim()
        .to_string();
    if meid_prefix.len() != 8 || !meid_prefix.bytes().all(|b| b.is_ascii_hexdigit()) {
        meid_prefix = p.meid_prefix_default.to_string();
    }

    let hash = fnv1a32(serialno.as_bytes());
    let base_snr = 100_000 + ((hash % 449_999) * 2);
    let snr0 = format!("{base_snr:06}");
    let snr1 = format!("{:06}", base_snr + 1);

    let body0 = format!("{tac}{snr0}");
    let imei0 = format!("{body0}{}", luhn_check_digit(&body0));
    let body1 = format!("{tac}{snr1}");
    let imei1 = format!("{body1}{}", luhn_check_digit(&body1));
    let meid = format!("{meid_prefix}{snr0}");

    for (key, val) in [
        ("ro.vendor.oem.imei", imei0.as_str()),
        ("ro.vendor.oem.imei2", imei1.as_str()),
        ("ro.vendor.oem.meid", meid.as_str()),
        ("ro.ril.oem.imei", imei0.as_str()),
        ("ro.ril.oem.imei1", imei0.as_str()),
        ("ro.ril.oem.imei2", imei1.as_str()),
        ("ro.ril.oem.meid", meid.as_str()),
        ("ro.ril.miui.imei0", imei0.as_str()),
        ("ro.ril.miui.imei1", imei1.as_str()),
    ] {
        if resetprop::get(key).unwrap_or_default().trim().is_empty() {
            set_prop(key, val);
        }
    }

    if resetprop::get("ro.ril.factory_id")
        .unwrap_or_default()
        .trim()
        .is_empty()
    {
        for src in ["ro.vendor.ril.factory_id", "ro.vendor.cdh.ril.factory_id"] {
            let val = resetprop::get(src).unwrap_or_default();
            let trimmed = val.trim();
            if !trimmed.is_empty() {
                set_prop("ro.ril.factory_id", trimmed);
                break;
            }
        }
    }

    let raw_oui = resetprop::get("ro.vendor.cdh.oem.mac_oui")
        .unwrap_or_default()
        .trim()
        .to_string();
    let oui = if raw_oui.is_empty() {
        "3c:38:24"
    } else {
        raw_oui.as_str()
    };
    if let Some((wifimac, btmac)) = derive_oem_mac_pair(oui, serialno) {
        for (key, val) in [
            ("ro.vendor.oem.wifimac", wifimac.as_str()),
            ("ro.ril.oem.wifimac", wifimac.as_str()),
            ("ro.vendor.oem.btmac", btmac.as_str()),
            ("ro.ril.oem.btmac", btmac.as_str()),
        ] {
            if resetprop::get(key).unwrap_or_default().trim().is_empty() {
                set_prop(key, val);
            }
        }
    }
}

fn fnv1a32(bytes: &[u8]) -> u32 {
    let mut hash: u32 = 2_166_136_261;
    for &b in bytes {
        hash ^= b as u32;
        hash = hash.wrapping_mul(16_777_619);
    }
    hash
}

/// 由指定 OUI（3 段十六进制如 "3c:38:24"）与实例 serialno 确定性派生连续配对的
/// WiFi MAC（末位偶数）与 BT MAC（wifimac + 1，对齐小米出厂相邻 MAC 规律）。
fn derive_oem_mac_pair(oui: &str, serialno: &str) -> Option<(String, String)> {
    let norm = normalize_mac_oui(oui)?;
    let seed = format!("{serialno}:oem_mac");
    let h = fnv1a32(seed.as_bytes());
    let b3 = ((h >> 16) & 0xff) as u8;
    let b4 = ((h >> 8) & 0xff) as u8;
    let b5_wifi = (h & 0xfe) as u8;
    let b5_bt = b5_wifi | 0x01;
    Some((
        format!("{norm}:{b3:02x}:{b4:02x}:{b5_wifi:02x}"),
        format!("{norm}:{b3:02x}:{b4:02x}:{b5_bt:02x}"),
    ))
}

fn normalize_mac_oui(oui: &str) -> Option<String> {
    let parts: Vec<&str> = oui.trim().split(':').collect();
    if parts.len() != 3 {
        return None;
    }
    let mut norm = Vec::with_capacity(3);
    for part in parts {
        if part.len() != 2 || !part.bytes().all(|b| b.is_ascii_hexdigit()) {
            return None;
        }
        norm.push(part.to_ascii_lowercase());
    }
    Some(norm.join(":"))
}

fn luhn_check_digit(body: &str) -> char {
    let mut sum = 0u32;
    let mut double_digit = true;
    for ch in body.bytes().rev() {
        let mut d = (ch - b'0') as u32;
        if double_digit {
            d *= 2;
            if d > 9 {
                d -= 9;
            }
        }
        sum += d;
        double_digit = !double_digit;
    }
    (b'0' + ((10 - (sum % 10)) % 10) as u8) as char
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn luhn_known_vectors() {
        // 标准 Luhn 向量（与 radio_modem.cpp 同规则，已用独立实现核对）
        assert_eq!(luhn_check_digit("49015420323751"), '8');
        assert_eq!(luhn_check_digit("7992739871"), '3');
    }

    #[test]
    fn luhn_result_is_digit() {
        for body in ["86717107100000", "86717107999998", "00000000000000"] {
            assert!(luhn_check_digit(body).is_ascii_digit());
        }
    }

    #[test]
    fn mac_pair_is_deterministic_and_adjacent() {
        let (wifi1, bt1) = derive_oem_mac_pair("3C:38:24", "4d5128bd8ca07462").unwrap();
        let (wifi2, bt2) = derive_oem_mac_pair("3c:38:24", "4d5128bd8ca07462").unwrap();
        assert_eq!(wifi1, wifi2);
        assert_eq!(bt1, bt2);
        assert!(wifi1.starts_with("3c:38:24:"));
        assert_eq!(wifi1.len(), 17);
        assert_eq!(bt1.len(), 17);
        let w_last = u8::from_str_radix(&wifi1[15..17], 16).unwrap();
        let b_last = u8::from_str_radix(&bt1[15..17], 16).unwrap();
        assert_eq!(w_last % 2, 0);
        assert_eq!(b_last, w_last + 1);
        assert!(derive_oem_mac_pair("bad-oui", "123").is_none());
    }

    #[test]
    fn syncable_props_and_ram_tier_detection() {
        let mut props = Vec::new();
        collect_syncable_ro_props(
            "ro.vendor.asus.beta=1\nro.vendor.build.asus.sku=ZWW\nro.product.model=KEEP\n",
            &mut props,
        );
        collect_syncable_ro_props(
            "ro.vendor.asus.beta=0\nro.vendor.build.asus.sku=WW\nro.vendor.cpufreq=4.3GHz\n",
            &mut props,
        );
        assert_eq!(
            props,
            vec![
                ("ro.vendor.asus.beta".to_string(), "0".to_string()),
                ("ro.vendor.build.asus.sku".to_string(), "WW".to_string()),
                ("ro.vendor.cpufreq".to_string(), "4.3GHz".to_string()),
            ]
        );
        assert_eq!(detect_ram_tier_gb_from_meminfo("MemTotal:        3872512 kB\n"), Some(4));
        assert_eq!(detect_ram_tier_gb_from_meminfo("MemTotal:        7760724 kB\n"), Some(8));
        assert_eq!(detect_ram_tier_gb_from_meminfo("MemTotal:       15639808 kB\n"), Some(16));
        assert_eq!(detect_ram_tier_gb_from_meminfo("MemTotal:       23592960 kB\n"), Some(24));
        assert!(is_unisoc_ddrsize_sync_needed("", "ums9230", ""));
        assert!(is_unisoc_ddrsize_sync_needed("unconfig", "", "Unisoc"));
        assert!(is_unisoc_ddrsize_sync_needed("4096M", "sun", "Qualcomm"));
        assert!(!is_unisoc_ddrsize_sync_needed("", "sun", "Qualcomm"));
    }
}