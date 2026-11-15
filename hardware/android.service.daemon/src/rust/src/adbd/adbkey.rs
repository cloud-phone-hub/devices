//! ADB 公钥下发（B 方案：移植自 pixel6 `vendor/adb-auth` 的 adb_auth_loader）。
//!
//! host 经 virtio-serial `/dev/hvc2` 完成 ADBKEY1 握手，把宿主 ADB 公钥
//! 下发到 guest 并原子写入 `/data/misc/adb/adb_keys`，供 APEX adbd
//! （`ro.adb.secure=1`）做 RSA 验签。协议与 pixel6 逐字节一致：
//!
//! ```text
//! guest: "ADBKEY1 READY\n"
//! host : "ADBKEY1 <len>\n" + [700B base64 公钥 + 可选注释] + "\n"
//! guest: "ADBKEY1 OK\n"（校验/写入失败则 "ADBKEY1 ERR\n"）
//! ```
//!
//! 与 pixel6 差异：握手成功后**不 exec 链**，而是由调用方（main.rs 编排线程）
//! 转交 `crate::adbd::adbd_launch::launch_forever` fork+setns+exec APEX adbd（daemon
//! 本身保持存活，继续跑 hvc7 事件/心跳）。

use crate::channel::make_raw;
use crate::log::kmsg;
use std::fs::{File, OpenOptions};
use std::io::{ErrorKind, Write};
use std::os::unix::fs::{OpenOptionsExt, PermissionsExt};
use std::os::unix::io::AsRawFd;
use std::time::Duration;

/// hvc2 在 v3 顺延布局里原为 null 预留；B 方案改为 ADBKEY 通道（host 侧
/// denseVirtconArgs 在 `ADBKeyHVC2` 时挂 `cdh-adbkey` chardev）。
pub const HVC_ADBKEY_DEFAULT: &str = "/dev/hvc2";

const ADB_AUTH_MAX: usize = 1024;
const ADB_KEY_BIN: usize = 524;
const ADB_B64_LEN: usize = 700;
const ADB_WORDS: usize = 64;

const ADB_KEYS_DIR: &str = "/data/misc/adb";
const ADB_KEYS_PATH: &str = "/data/misc/adb/adb_keys";
const ADB_KEYS_LABEL: &str = "u:object_r:adb_keys_file:s0";

// aarch64 asm-generic fcntl 常量（O_CLOEXEC/O_NOFOLLOW/O_NOCTTY）
const O_CLOEXEC: i32 = 0o2000000;
const O_NOFOLLOW: i32 = 0o400000;
const O_NOCTTY: i32 = 0o400;

// ---------------- 纯解析（对应 adb_auth.c） ----------------

fn get_le32(v: &[u8], off: usize) -> u32 {
    u32::from_le_bytes([v[off], v[off + 1], v[off + 2], v[off + 3]])
}

fn b64_value(v: u8) -> i32 {
    match v {
        b'A'..=b'Z' => (v - b'A') as i32,
        b'a'..=b'z' => (v - b'a') as i32 + 26,
        b'0'..=b'9' => (v - b'0') as i32 + 52,
        b'+' => 62,
        b'/' => 63,
        _ => -1,
    }
}

/// base64 解码 700B → 524B（末组仅 2 字节，尾部 '='）。
fn b64_decode(input: &[u8], output: &mut [u8]) -> Result<usize, ()> {
    if input.len() != ADB_B64_LEN || input.len() % 4 != 0 || input[input.len() - 1] != b'=' {
        return Err(());
    }
    let mut produced = 0usize;
    let mut offset = 0usize;
    while offset < input.len() {
        let a = b64_value(input[offset]);
        let b = b64_value(input[offset + 1]);
        let c = b64_value(input[offset + 2]);
        let d = b64_value(input[offset + 3]);
        let last = offset + 4 == input.len();
        if a < 0 || b < 0 || c < 0 || (!last && d < 0)
            || (last && input[offset + 3] != b'=')
            || (last && (c & 3) != 0)
        {
            return Err(());
        }
        let take = if last { 2 } else { 3 };
        if produced + take > output.len() {
            return Err(());
        }
        output[produced] = ((a as u8) << 2) | ((b as u8) >> 4);
        output[produced + 1] = ((b as u8) << 4) | ((c as u8) >> 2);
        if !last {
            output[produced + 2] = ((c as u8) << 6) | (d as u8);
        }
        produced += take;
        offset += 4;
    }
    Ok(produced)
}

fn word_cmp(left: &[u32; ADB_WORDS], right: &[u32; ADB_WORDS]) -> i32 {
    let mut index = ADB_WORDS;
    while index > 0 {
        index -= 1;
        if left[index] > right[index] {
            return 1;
        }
        if left[index] < right[index] {
            return -1;
        }
    }
    0
}

fn word_sub(value: &mut [u32; ADB_WORDS], modulus: &[u32; ADB_WORDS]) -> u32 {
    let mut borrow = 0u64;
    for index in 0..ADB_WORDS {
        let subtrahend = modulus[index] as u64 + borrow;
        let minuend = value[index] as u64;
        value[index] = (minuend.wrapping_sub(subtrahend)) as u32;
        borrow = (minuend < subtrahend) as u64;
    }
    borrow as u32
}

fn mod_double(value: &mut [u32; ADB_WORDS], modulus: &[u32; ADB_WORDS]) -> i32 {
    let mut carry = 0u64;
    for index in 0..ADB_WORDS {
        let doubled = ((value[index] as u64) << 1) | carry;
        value[index] = doubled as u32;
        carry = doubled >> 32;
    }
    if carry != 0 {
        if word_sub(value, modulus) != 1 {
            return -1;
        }
    } else if word_cmp(value, modulus) >= 0 && word_sub(value, modulus) != 0 {
        return -1;
    }
    0
}

/// 校验 524B Android 自有公钥格式（64 字 2048 位 RSA，指数 65537，
/// 以及 2^(2048) mod n 的预计算一致性）——对应 C 的 check_key()。
fn check_key(decoded: &[u8]) -> bool {
    if decoded.len() != ADB_KEY_BIN || get_le32(decoded, 0) != ADB_WORDS as u32 {
        return false;
    }
    let n0inv = get_le32(decoded, 4);
    let mut modulus = [0u32; ADB_WORDS];
    for index in 0..ADB_WORDS {
        modulus[index] = get_le32(decoded, 8 + index * 4);
    }
    let exponent = get_le32(decoded, 520);
    if (modulus[0] & 1) == 0
        || (modulus[ADB_WORDS - 1] & 0x8000_0000) == 0
        || exponent != 65537
        || (modulus[0].wrapping_mul(n0inv).wrapping_add(1) != 0)
    {
        return false;
    }
    let mut expected = [0u32; ADB_WORDS];
    expected[0] = 1;
    for _ in 0..ADB_WORDS * 64 {
        if mod_double(&mut expected, &modulus) != 0 {
            return false;
        }
    }
    for index in 0..ADB_WORDS {
        if expected[index] != get_le32(decoded, 264 + index * 4) {
            return false;
        }
    }
    true
}

/// 校验并规范密钥记录（token 段 700B base64 + 可选注释），输出带 '\n' 的落盘字节。
fn parse(record: &[u8]) -> Result<Vec<u8>, ()> {
    if record.len() < ADB_B64_LEN || record.len() > ADB_AUTH_MAX {
        return Err(());
    }
    let mut token_len = 0;
    while token_len < record.len() && record[token_len] != b' ' && record[token_len] != b'\t' {
        token_len += 1;
    }
    if token_len != ADB_B64_LEN {
        return Err(());
    }
    if token_len < record.len() {
        let mut have_comment = false;
        for &c in &record[token_len + 1..] {
            if c == b' ' || c == b'\t' {
                continue;
            }
            if !(0x21..=0x7e).contains(&c) {
                return Err(());
            }
            have_comment = true;
        }
        if !have_comment {
            return Err(());
        }
    }
    for &c in record {
        if c == 0 || c == b'\r' || c == b'\n' {
            return Err(());
        }
    }
    let mut decoded = [0u8; ADB_KEY_BIN];
    let n = b64_decode(&record[..token_len], &mut decoded)?;
    if !check_key(&decoded[..n]) {
        return Err(());
    }
    let mut out = record.to_vec();
    out.push(b'\n');
    Ok(out)
}

// ---------------- 帧读取（对应 adb_runtime.c adb_read_frame） ----------------

fn read_full(fd: i32, out: &mut [u8]) -> Result<(), ()> {
    let mut offset = 0usize;
    while offset < out.len() {
        let n = unsafe { crate::syscall::read(fd, out[offset..].as_mut_ptr(), out.len() - offset) };
        if n > 0 {
            offset += n as usize;
            continue;
        }
        if n < 0 {
            let err = crate::syscall::errno();
            if err == 4 {
                continue; // EINTR
            }
        }
        return Err(());
    }
    Ok(())
}

/// 读一帧：头行 `ADBKEY1 <len>\n`（≤32B）→ len 字节记录 → 结尾 '\n'。
fn read_frame(fd: i32, out: &mut Vec<u8>) -> Result<(), ()> {
    let prefix = b"ADBKEY1 ";
    let mut header = [0u8; 32];
    let mut header_len = 0usize;
    while header_len < header.len() {
        if read_full(fd, &mut header[header_len..header_len + 1]).is_err() {
            return Err(());
        }
        header_len += 1;
        if header[header_len - 1] == b'\n' {
            break;
        }
    }
    if header_len <= prefix.len()
        || header[header_len - 1] != b'\n'
        || &header[..prefix.len()] != prefix
    {
        return Err(());
    }
    let mut idx = prefix.len();
    if header[idx] == b'0' {
        return Err(());
    }
    let mut record_len = 0usize;
    while idx + 1 < header_len {
        let c = header[idx];
        if !c.is_ascii_digit() {
            return Err(());
        }
        if record_len > (ADB_AUTH_MAX - (c - b'0') as usize) / 10 {
            return Err(());
        }
        record_len = record_len * 10 + (c - b'0') as usize;
        idx += 1;
    }
    if record_len < ADB_B64_LEN || record_len > ADB_AUTH_MAX {
        return Err(());
    }
    let mut record = vec![0u8; record_len];
    if read_full(fd, &mut record).is_err() {
        return Err(());
    }
    let mut term = [0u8; 1];
    if read_full(fd, &mut term).is_err() || term[0] != b'\n' {
        return Err(());
    }
    *out = parse(&record)?;
    Ok(())
}

// ---------------- 原子写 adb_keys（对应 adb_runtime.c adb_write_key） ----------------

fn set_label(path: &str) {
    // best-effort：设 security.selinux xattr；失败（域无 setattr 权限）仅记录，
    // 依赖 init 的 restorecon（file_contexts 的 /data/misc/adb → adb_keys_file）兜底。
    let c_path = std::ffi::CString::new(path).unwrap();
    let c_label = std::ffi::CString::new(ADB_KEYS_LABEL).unwrap();
    let rc = unsafe {
        crate::syscall::setxattr(
            c_path.as_ptr(),
            c"security.selinux".as_ptr(),
            c_label.as_ptr() as *const u8,
            c_label.as_bytes().len() + 1,
            0,
        )
    };
    if rc != 0 {
        kmsg(&format!("adbkey: 设 {path} label 失败(rc={rc})，依赖 restorecon 兜底"));
    }
}

fn ensure_data_dir() -> Result<(), String> {
    match std::fs::create_dir_all(ADB_KEYS_DIR) {
        Ok(_) => {}
        Err(e) => return Err(format!("mkdir {ADB_KEYS_DIR}: {e}")),
    }
    // 2750 / 1000:2000（system:shell），与 pixel6 adb_auth 校验一致
    let c_path = std::ffi::CString::new(ADB_KEYS_DIR).unwrap();
    if unsafe { crate::syscall::chmod(c_path.as_ptr(), 0o2750) } != 0 {
        return Err("chmod /data/misc/adb".into());
    }
    if unsafe { crate::syscall::chown(c_path.as_ptr(), 1000, 2000) } != 0 {
        kmsg("adbkey: chown /data/misc/adb 失败（继续）");
    }
    set_label(ADB_KEYS_DIR);
    Ok(())
}

fn write_key(record: &[u8]) -> Result<(), String> {
    let pid = std::process::id();
    let mut last_err = String::new();
    for attempt in 0..8usize {
        let name = format!("adb_keys.tmp.{pid}.{attempt}");
        let tmp = format!("{ADB_KEYS_DIR}/{name}");
        let mut f = match OpenOptions::new()
            .write(true)
            .create_new(true)
            .mode(0o600)
            .custom_flags(O_CLOEXEC | O_NOFOLLOW)
            .open(&tmp)
        {
            Ok(f) => f,
            Err(e) if e.kind() == ErrorKind::AlreadyExists => continue,
            Err(e) => {
                last_err = format!("open {tmp}: {e}");
                continue;
            }
        };
        let fd = f.as_raw_fd();
        if unsafe { crate::syscall::fchown(fd, 1000, 2000) } != 0 {
            last_err = "fchown".into();
            drop(f);
            let _ = std::fs::remove_file(&tmp);
            continue;
        }
        if let Err(e) = f.set_permissions(std::fs::Permissions::from_mode(0o640)) {
            last_err = format!("fchmod: {e}");
            drop(f);
            let _ = std::fs::remove_file(&tmp);
            continue;
        }
        if let Err(e) = f.write_all(record) {
            last_err = format!("write: {e}");
            drop(f);
            let _ = std::fs::remove_file(&tmp);
            continue;
        }
        if let Err(e) = f.sync_all() {
            last_err = format!("fsync: {e}");
            drop(f);
            let _ = std::fs::remove_file(&tmp);
            continue;
        }
        drop(f);
        set_label(&tmp);
        if let Err(e) = std::fs::rename(&tmp, ADB_KEYS_PATH) {
            last_err = format!("rename: {e}");
            let _ = std::fs::remove_file(&tmp);
            continue;
        }
        if let Ok(d) = File::open(ADB_KEYS_DIR) {
            let _ = d.sync_all();
        }
        return Ok(());
    }
    Err(if last_err.is_empty() { "temp 名用尽".into() } else { last_err })
}

// ---------------- 握手（对应 adb_auth_main.c main） ----------------

/// 完成一轮 ADBKEY1 握手并写 /data/misc/adb/adb_keys。
/// 失败返回 Err（调用方决定重试）。
pub fn handshake() -> Result<(), String> {
    ensure_data_dir()?;
    let mut dev = OpenOptions::new()
        .read(true)
        .write(true)
        .custom_flags(O_CLOEXEC | O_NOCTTY | O_NOFOLLOW)
        .open(HVC_ADBKEY_DEFAULT)
        .map_err(|e| format!("open {HVC_ADBKEY_DEFAULT}: {e}"))?;
    make_raw(&dev, "adbkey");
    let fd = dev.as_raw_fd();

    let mut send = |s: &str| -> Result<(), String> {
        dev.write_all(s.as_bytes()).map_err(|e| format!("写 {s:?}: {e}"))
    };
    send("ADBKEY1 READY\n")?;
    kmsg("adbkey: 已发 READY，等待宿主公钥");

    let mut record = Vec::new();
    read_frame(fd, &mut record).map_err(|_| "收帧/校验失败".to_string())?;
    write_key(&record)?;
    send("ADBKEY1 OK\n")?;
    kmsg("adbkey: 公钥已写入 adb_keys，OK");
    Ok(())
}

/// 常驻编排：握手失败重试；成功一次后停在这里（真正的 adbd 启动由
/// main.rs 在握手成功路径上转交 adbd_launch，本函数仅用于首次失败重试）。
pub fn run_handshake_retry() -> Result<(), String> {
    let mut delay = Duration::from_secs(1);
    loop {
        match handshake() {
            Ok(()) => return Ok(()),
            Err(e) => {
                kmsg(&format!("adbkey: 握手失败 {e}，{delay:?} 后重试"));
                std::thread::sleep(delay);
                if delay < Duration::from_secs(10) {
                    delay *= 2;
                }
            }
        }
    }
}
