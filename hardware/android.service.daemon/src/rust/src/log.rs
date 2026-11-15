//! 内核日志输出（/dev/kmsg）

use std::fs::OpenOptions;
use std::io::Write;

/// 写内核日志，<6> = KERN_INFO，dmesg/logcat 可见
pub fn kmsg(msg: &str) {
    if let Ok(mut f) = OpenOptions::new().write(true).open("/dev/kmsg") {
        let _ = writeln!(f, "<6>[room-daemon] {}", msg);
    }
}
