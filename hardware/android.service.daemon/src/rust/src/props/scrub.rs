//! 虚拟化痕迹清理：引导期 ro.boot.* / EFI 挂载记录收敛 + boot_completed 后的
//! 晚挂载分区 dev.mnt.* 收敛。机型差异（theme 去留、boot_devices 路径、分区盘符
//! 映射表）全部来自 profile.rs 的机型档案，本文件不含机型分支。

use std::collections::HashSet;
use std::ffi::CString;

use crate::log::kmsg;

use super::fixups::vendor_build_prop_value;
use super::profile::DeviceProfile;
use super::writer::{delete_prop_if_exists, set_prop, set_prop_svc};

/// 清理引导器/内核虚拟化参数派生的公开属性（ro.boot.* 与早期 EFI 挂载记录），
/// 并将通用的 ro.boot.boot_devices=any 收敛为档案里的真实 UFS/MMC 控制器路径。
pub(super) fn scrub_early_virtual_props(p: &DeviceProfile) {
    for key in [
        "ro.boot.console",
        "ro.boot.first_stage_console",
        "ro.boot.partition_map",
        "ro.boot.hypervisor.vm.supported",
        "ro.boot.hypervisor.protected_vm.supported",
        "ro.boot.hypervisor.version",
    ] {
        delete_prop_if_exists(key);
    }

    // ro.boot.theme：是否保留按档案（gs101 真机 bootloader 传 theme=1；
    // MTK 等真机不暴露，grub 的 androidboot.theme 是虚拟化产物）。
    if !p.keep_boot_theme {
        delete_prop_if_exists("ro.boot.theme");
    }

    scrub_virtual_mount_props(p);

    let boot_devs = resetprop::get("ro.boot.boot_devices").unwrap_or_default();
    if boot_devs.trim() == "any" {
        set_prop("ro.boot.boot_devices", p.boot_devices);
    }

    align_tmpfs_superblocks(p);
}

/// 在后台等待 sys.boot_completed=1 后，清理启动期临时兼容属性并收敛晚挂载分区的 dev.mnt.*。
pub fn spawn_post_boot_scrub(p: DeviceProfile) {
    let _ = std::thread::Builder::new()
        .name("prop-scrub".into())
        .spawn(move || {
            for _ in 0..1200 {
                if resetprop::get("sys.boot_completed")
                    .unwrap_or_default()
                    .trim()
                    == "1"
                {
                    break;
                }
                std::thread::sleep(std::time::Duration::from_millis(500));
            }
            // 留出 1s 窗口供 init 的 on property:sys.boot_completed=1 触发器执行完毕
            std::thread::sleep(std::time::Duration::from_secs(1));

            for key in [
                "ro.hw_timeout_multiplier",
                "debug.sf.vsync_reactor_ignore_present_fences",
            ] {
                delete_prop_if_exists(key);
            }
            // 纯 AIDL 云机（0 个 HIDL HAL）启动后 hwservicemanager 会置 disabled=true 并自行退出；
            // 仅当 hwservicemanager 确实仍在运行时才清除其兼容开关，避免 Mainline APEX（如 com.android.bluetooth）
            // 误调已停止的 /dev/hwbinder 拿到 DEAD_OBJECT 并 SIGABRT 崩溃。
            if resetprop::get("init.svc.hwservicemanager")
                .unwrap_or_default()
                .trim()
                == "running"
            {
                delete_prop_if_exists("hwservicemanager.disabled");
                if resetprop::get("hwservicemanager.ready")
                    .unwrap_or_default()
                    .trim()
                    .is_empty()
                {
                    set_prop("hwservicemanager.ready", "true");
                }
            }
            scrub_virtual_mount_props(&p);
            align_tmpfs_superblocks(&p);
            if let Some(want) = vendor_build_prop_value("dalvik.vm.background-dex2oat-threads") {
                let want = want.trim();
                let cur = resetprop::get("dalvik.vm.background-dex2oat-threads").unwrap_or_default();
                if !want.is_empty() && cur.trim() != want {
                    set_prop_svc("dalvik.vm.background-dex2oat-threads", want);
                }
            }
            kmsg("post-boot virtual prop scrub completed");
        });
}

/// 收敛 dev.mnt.{blk,dev,rootdisk}.* 中的 virtio 盘符（vda*/vdb*/vdc*）：
/// - 若 `p.enforce_exact_mounts` 为 true：仅删除 EFI 及载体 mnt.vendorpatch，并将档案里
///   全部挂载后缀（含缺失的 super 逻辑只读分区与 data*）幂等写入目标 blk/dev/rootdisk；
/// - 否则保持原有删减式兜底行为。
fn scrub_virtual_mount_props(p: &DeviceProfile) {
    const DROP_EXACT_ONLY_SUFFIXES: &[&str] = &["mnt.vendor.EFI", "mnt.vendorpatch"];
    const DROP_MOUNT_SUFFIXES: &[&str] = &[
        "mnt.vendor.EFI",
        "root",
        "product",
        "system_ext",
        "vendor",
        "vendor_dlkm",
        "odm_dlkm",
        "system_dlkm",
        "mi_ext",
        "mnt.vendor.mi_ext",
        "mnt.vendorpatch",
    ];
    let drop_list = if p.enforce_exact_mounts {
        DROP_EXACT_ONLY_SUFFIXES
    } else {
        DROP_MOUNT_SUFFIXES
    };
    for suf in drop_list {
        for prefix in ["dev.mnt.blk.", "dev.mnt.dev.", "dev.mnt.rootdisk."] {
            delete_prop_if_exists(&format!("{prefix}{suf}"));
        }
    }

    for (suf, blk_val, dev_val) in p.mount_rewrites {
        for (prefix, want) in [
            ("dev.mnt.blk.", *blk_val),
            ("dev.mnt.dev.", *dev_val),
            ("dev.mnt.rootdisk.", "sdc"),
        ] {
            let key = format!("{prefix}{suf}");
            let cur = resetprop::get(&key).unwrap_or_default();
            let trimmed = cur.trim();
            if p.enforce_exact_mounts {
                if trimmed != want {
                    set_prop(&key, want);
                }
            } else if is_virtio_disk_val(trimmed) {
                set_prop(&key, want);
            }
        }
    }
}

fn is_virtio_disk_val(v: &str) -> bool {
    let t = v.trim();
    t.starts_with("vd")
}

/// 按档案对全部分配根 tmpfs superblock 执行原位 MS_REMOUNT，注入 size=...k,nr_inodes=...
/// 并保留原挂载点的 seclabel/mode/gid 与 VFS 标志。
fn align_tmpfs_superblocks(p: &DeviceProfile) {
    let Some((size_kb, nr_inodes)) = p.tmpfs_limits else {
        return;
    };
    let Ok(mountinfo) = std::fs::read_to_string("/proc/1/mountinfo")
        .or_else(|_| std::fs::read_to_string("/proc/self/mountinfo"))
    else {
        return;
    };
    let targets = collect_tmpfs_remount_targets(&mountinfo, size_kb, nr_inodes);
    if targets.is_empty() {
        return;
    }
    let Ok(c_data) = CString::new(format!("size={size_kb}k,nr_inodes={nr_inodes}")) else {
        return;
    };
    let mut remounted = 0usize;
    for (mount_point, vfs_flags) in targets {
        let Ok(c_mp) = CString::new(mount_point.as_str()) else {
            continue;
        };
        let rc = unsafe {
            crate::syscall::mount(
                std::ptr::null(),
                c_mp.as_ptr(),
                std::ptr::null(),
                crate::syscall::MS_REMOUNT | vfs_flags,
                c_data.as_ptr(),
            )
        };
        if rc == 0 {
            remounted += 1;
        } else {
            kmsg(&format!(
                "tmpfs remount {mount_point} 失败 (errno={})",
                crate::syscall::errno()
            ));
        }
    }
    if remounted > 0 {
        kmsg(&format!(
            "tmpfs superblocks aligned: {remounted} remounted (size={size_kb}k,nr_inodes={nr_inodes})"
        ));
    }
}

/// 从 mountinfo 文本抽取需要 MS_REMOUNT 的根 tmpfs 目标（返回 `(mount_point, vfs_flags)`）：
/// - 仅保留 `fstype == "tmpfs"` 且 `root == "/"`；
/// - 按 `major:minor` 设备号去重（仅 remount 每个 superblock 首次出现的根挂载点）；
/// - 已同时带目标 `size=<k>k` 与 `nr_inodes=<n>` 时跳过。
fn collect_tmpfs_remount_targets(
    mountinfo: &str,
    size_kb: u64,
    nr_inodes: u64,
) -> Vec<(String, u64)> {
    let want_size = format!("size={size_kb}k");
    let want_inodes = format!("nr_inodes={nr_inodes}");
    let mut seen_devs: HashSet<&str> = HashSet::new();
    let mut out = Vec::new();

    for line in mountinfo.lines() {
        let Some((left_raw, right_raw)) = line.split_once(" - ") else {
            continue;
        };
        let left: Vec<&str> = left_raw.split_whitespace().collect();
        let right: Vec<&str> = right_raw.split_whitespace().collect();
        if left.len() < 6 || right.len() < 3 {
            continue;
        }
        let dev = left[2];
        let root = left[3];
        let mount_point = left[4];
        let vfs_opts = left[5];
        let fstype = right[0];
        let super_opts = right[2];

        if fstype != "tmpfs" || root != "/" {
            continue;
        }
        if !seen_devs.insert(dev) {
            continue;
        }
        if has_exact_tmpfs_limits(super_opts, &want_size, &want_inodes) {
            continue;
        }
        out.push((
            unescape_mount_path(mount_point),
            parse_vfs_mount_flags(vfs_opts),
        ));
    }
    out
}

fn has_exact_tmpfs_limits(super_opts: &str, want_size: &str, want_inodes: &str) -> bool {
    let mut has_size = false;
    let mut has_inodes = false;
    for opt in super_opts.split(',') {
        if opt == want_size {
            has_size = true;
        } else if opt == want_inodes {
            has_inodes = true;
        }
    }
    has_size && has_inodes
}

fn parse_vfs_mount_flags(vfs_opts: &str) -> u64 {
    let mut flags = 0u64;
    for opt in vfs_opts.split(',') {
        match opt {
            "ro" => flags |= crate::syscall::MS_RDONLY,
            "nosuid" => flags |= crate::syscall::MS_NOSUID,
            "nodev" => flags |= crate::syscall::MS_NODEV,
            "noexec" => flags |= crate::syscall::MS_NOEXEC,
            "nosymfollow" => flags |= crate::syscall::MS_NOSYMFOLLOW,
            _ => {}
        }
    }
    flags
}

fn unescape_mount_path(raw: &str) -> String {
    let bytes = raw.as_bytes();
    let mut out = String::with_capacity(raw.len());
    let mut i = 0usize;
    while i < bytes.len() {
        if bytes[i] == b'\\' && i + 3 < bytes.len() {
            match &bytes[i + 1..i + 4] {
                b"040" => {
                    out.push(' ');
                    i += 4;
                    continue;
                }
                b"011" => {
                    out.push('\t');
                    i += 4;
                    continue;
                }
                b"012" => {
                    out.push('\n');
                    i += 4;
                    continue;
                }
                b"134" => {
                    out.push('\\');
                    i += 4;
                    continue;
                }
                _ => {}
            }
        }
        let rest = &raw[i..];
        if let Some(ch) = rest.chars().next() {
            out.push(ch);
            i += ch.len_utf8();
        } else {
            break;
        }
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::syscall::{MS_NODEV, MS_NOEXEC, MS_NOSUID, MS_RDONLY};

    #[test]
    fn collect_tmpfs_targets_filters_bind_dedups_and_preserves_flags() {
        let mountinfo = "\
18 1 0:18 / /dev rw,nosuid,noatime shared:1 - tmpfs tmpfs rw,seclabel,mode=755\n\
19 1 0:19 / /mnt rw,nosuid,nodev,noexec,relatime - tmpfs tmpfs rw,seclabel,mode=755,gid=1000\n\
20 19 0:19 /user/0 /mnt/user/0 rw,nosuid,nodev,noexec - tmpfs tmpfs rw,seclabel,mode=755,gid=1000\n\
21 1 0:19 / /apex/shared rw,nosuid,nodev - tmpfs tmpfs rw,seclabel,mode=755\n\
22 1 0:22 / /already ro,nosuid,nosymfollow - tmpfs tmpfs rw,seclabel,size=3863628k,nr_inodes=965907\n\
23 1 0:23 / /my\\040dir ro,nodev - tmpfs tmpfs rw,seclabel\n\
24 1 254:1 / /data rw,noatime - f2fs /dev/block/vdb rw,seclabel\n";

        let targets = collect_tmpfs_remount_targets(mountinfo, 3_863_628, 965_907);
        assert_eq!(targets.len(), 3);
        assert_eq!(targets[0], ("/dev".to_string(), MS_NOSUID));
        assert_eq!(targets[1], ("/mnt".to_string(), MS_NOSUID | MS_NODEV | MS_NOEXEC));
        assert_eq!(targets[2], ("/my dir".to_string(), MS_RDONLY | MS_NODEV));
    }

    #[test]
    fn gold_mount_rewrites_cover_all_vendor_and_rescue_partitions() {
        let rewrites = super::super::profile::MOUNT_REWRITES_GOLD;
        assert_eq!(rewrites.len(), 27);
        for (suf, blk, dev) in [
            ("mnt.rescue", "sdc57", "sdc57"),
            ("mnt.vendor.nvcfg", "sdc6", "sdc6"),
            ("mnt.vendor.nvdata", "sdc7", "sdc7"),
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
            ("data_mirror.storage_area", "sdc66", "dm-58"),
        ] {
            assert!(
                rewrites.contains(&(suf, blk, dev)),
                "MOUNT_REWRITES_GOLD 缺少 {suf} -> ({blk}, {dev})"
            );
        }
    }
}