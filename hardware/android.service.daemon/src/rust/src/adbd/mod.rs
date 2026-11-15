//! adb 链路：宿主公钥注入与 APEX adbd 的拉起/守护。
//!
//! - `adbkey`：hvc2 收宿主公钥（ADBKEY1 握手），原子写 `/data/misc/adb/adb_keys`；
//! - `adbd_launch`：fork 子进程进入 `adb_net` netns 并 exec APEX adbd，父进程
//!   waitpid 守护，退出即重启。
//!
//! 认证开关见 [`ADB_SECURE`]：0 = 免认证（云机调试期默认），1 = 公钥验签。

pub mod adbkey;
pub mod adbd_launch;

use crate::log::kmsg;

/// `ro.adb.secure`：0 免认证（不校验 adb_keys）；1 用宿主公钥做 RSA 验签。
///
/// 宿主 daemon 的 hvc2 ADBKEY1 通道已就绪（见 gold-15
/// issue-adb-offline-hvc2-adbkey），恢复 1：main 先跑 adbkey 握手把宿主公钥
/// 写进 /data/misc/adb/adb_keys，再拉起 adbd 做 RSA 验签。
/// 置 0 会跳过握手，adbd 拿不到 adb_keys，宿主连接恒 offline。
pub const ADB_SECURE: bool = true;

/// 在 adbd 拉起前写 `ro.adb.secure`（resetprop 直接改属性区，只读属性也生效）。
pub fn apply_props() {
    let value = if ADB_SECURE { "1" } else { "0" };
    if resetprop::set_direct("ro.adb.secure", value).is_err() {
        kmsg(&format!("set ro.adb.secure={value} 失败（继续）"));
    }
}
