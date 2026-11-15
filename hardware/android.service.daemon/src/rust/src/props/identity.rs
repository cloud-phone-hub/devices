//! 实例启动身份（chipid/cpuid/serialno）的生成与落盘缓存——与机型无关的通用逻辑。
//!
//! 云机没有真机的 bootloader/efuse 与 cmdline serialno 来源，宿主侧也注入不了
//! （内核 cmdline 由镜像内 GRUB 拼装，QEMU 没有 -append 通道），故按**实例**
//! 生成一次并落盘缓存：
//!   * 值存 /data/vendor/cdh/boot_identity（每台实例有独立 userdata → per-instance
//!     唯一；与 android_id 同性质的"实例身份"文件）；
//!   * 之后每次启动先读缓存复用 → 跨重启稳定（随机值不落盘会变成"每次重启换机"
//!     的指纹，比不设更糟）。
//!
//! 缓存格式（逐行；缺项/坏项单独重生成）：
//!   chipid=<32 hex>
//!   cpuid=<72 hex>              # = chipid(16B) + 20B 随机，长度对齐真机 288 bit
//!   serialno=<16 位 [a-z0-9]>   # 形态对齐真机（如 fmvwkrheonugvgjb）

use std::fs;
use std::io::Read;

use crate::log::kmsg;

const CACHE_DIR: &str = "/data/vendor/cdh";
const CACHE_FILE: &str = "/data/vendor/cdh/boot_identity";
const CHIPID_BYTES: usize = 16;
const CPUID_BYTES: usize = 36; // 真机 cpuid = chipid 16B + 20B
const SERIAL_LEN: usize = 16;
const SERIAL_ALPHABET: &[u8] = b"abcdefghijklmnopqrstuvwxyz0123456789";

pub(super) struct Identity {
    pub chipid: String,
    pub cpuid: String,
    pub serialno: String,
}

/// 缓存三项都完好即复用；否则只重生成缺失项并回写缓存。
/// 写盘失败只告警（值本次仍生效），下次启动重新生成。
pub(super) fn load_or_create() -> Option<Identity> {
    let text = fs::read_to_string(CACHE_FILE).unwrap_or_default();
    let mut id = Identity {
        chipid: read_field(&text, "chipid=", |v| is_hex(v, CHIPID_BYTES * 2)).unwrap_or_default(),
        cpuid: read_field(&text, "cpuid=", |v| is_hex(v, CPUID_BYTES * 2)).unwrap_or_default(),
        serialno: read_field(&text, "serialno=", |v| is_serial(v)).unwrap_or_default(),
    };
    if !id.chipid.is_empty() && !id.cpuid.is_empty() && !id.serialno.is_empty() {
        kmsg("boot identity: 复用缓存");
        return Some(id);
    }

    let mut buf = [0u8; CPUID_BYTES + SERIAL_LEN];
    let mut f = match fs::File::open("/dev/urandom") {
        Ok(f) => f,
        Err(e) => {
            kmsg(&format!("boot identity: 打开 /dev/urandom 失败: {e}，跳过"));
            return None;
        }
    };
    if let Err(e) = f.read_exact(&mut buf) {
        kmsg(&format!("boot identity: 读 /dev/urandom 失败: {e}，跳过"));
        return None;
    }
    // chipid/cpuid 同源（cpuid = chipid + 20B），缺任一即整组重生成
    if id.chipid.is_empty() || id.cpuid.is_empty() {
        id.chipid = hex(&buf[..CHIPID_BYTES]);
        id.cpuid = hex(&buf[..CPUID_BYTES]);
    }
    if id.serialno.is_empty() {
        id.serialno = serial(&buf[CPUID_BYTES..]);
    }
    kmsg("boot identity: 生成缺失项并落盘");
    write_cache(&format!(
        "chipid={}\ncpuid={}\nserialno={}\n",
        id.chipid, id.cpuid, id.serialno
    ));
    Some(id)
}

/// 取一行 `key=<value>`，交给 ok 校验；不命中/不合法返回 None。
/// 缓存被截断或写坏时按未命中处理，避免把半截值设进属性。
fn read_field(text: &str, key: &str, ok: impl Fn(&str) -> bool) -> Option<String> {
    for line in text.lines() {
        if let Some(v) = line.strip_prefix(key) {
            let v = v.trim();
            return if ok(v) { Some(v.to_string()) } else { None };
        }
    }
    None
}

fn is_hex(s: &str, len: usize) -> bool {
    s.len() == len && s.bytes().all(|b| b.is_ascii_hexdigit())
}

fn is_serial(s: &str) -> bool {
    s.len() == SERIAL_LEN
        && s.bytes().all(|b| b.is_ascii_lowercase() || b.is_ascii_digit())
}

fn write_cache(raw: &str) {
    if let Err(e) = fs::create_dir_all(CACHE_DIR) {
        kmsg(&format!("boot identity: mkdir {CACHE_DIR} 失败: {e}"));
        return;
    }
    if let Err(e) = fs::write(CACHE_FILE, raw) {
        kmsg(&format!("boot identity: 写 {CACHE_FILE} 失败: {e}"));
    }
}

fn hex(bytes: &[u8]) -> String {
    let mut s = String::with_capacity(bytes.len() * 2);
    for b in bytes {
        s.push_str(&format!("{b:02x}"));
    }
    s
}

/// 随机字节映射到 [a-z0-9]（36 进制），长度即字节数：取模偏差对身份值无意义。
fn serial(bytes: &[u8]) -> String {
    bytes
        .iter()
        .map(|b| SERIAL_ALPHABET[(*b as usize) % SERIAL_ALPHABET.len()] as char)
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn serial_shape_matches_real_device() {
        let s = serial(&[0u8; SERIAL_LEN]);
        assert!(is_serial(&s));
        let s = serial(&[255u8; SERIAL_LEN]);
        assert!(is_serial(&s));
    }

    #[test]
    fn hex_roundtrip_length() {
        assert_eq!(hex(&[0xab, 0x01]), "ab01");
        assert!(is_hex(&hex(&[0u8; CHIPID_BYTES]), CHIPID_BYTES * 2));
    }

    #[test]
    fn read_field_rejects_bad_value() {
        let text = "chipid=zz\ncpuid=00\nserialno=abc\n";
        assert!(read_field(text, "chipid=", |v| is_hex(v, 32)).is_none());
        assert!(read_field(text, "serialno=", is_serial).is_none());
        assert!(read_field(text, "missing=", |_| true).is_none());
    }

    #[test]
    fn read_field_accepts_good_value() {
        let text = format!("serialno={}\n", serial(&[7u8; SERIAL_LEN]));
        assert!(read_field(&text, "serialno=", is_serial).is_some());
    }
}