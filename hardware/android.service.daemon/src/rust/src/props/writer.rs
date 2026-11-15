//! 属性写入/删除统一入口（props 各子模块共用）。

use crate::log::kmsg;

/// 属性写入统一入口：ro. 只读键走 direct mmap（绕 set-once）；非 ro. 键一律走
/// property_service 常规通道（daemon 域 pioneer 为 permissive，SELinux 放行）。
/// 禁止对非 ro. 键用 set_direct：直写会 reset+重建整个上下文属性区，而
/// radio_prop 等热区有框架/HAL 并发写，重建窗口内并发更新会破坏属性区 trie，
/// init 后续读属性即 SIGSEGV（oriole 实测 boot_completed 后反复 panic 重启）。
pub(super) fn set_prop(key: &str, value: &str) {
    if !key.starts_with("ro.") {
        set_prop_svc(key, value);
        return;
    }
    match resetprop::set_direct(key, value) {
        Ok(_) => kmsg(&format!("{key}={value}（direct mmap）")),
        Err(e) => {
            kmsg(&format!("set_direct {key} 失败: {e}"));
            if resetprop::set(key, value).is_ok() {
                kmsg(&format!("{key} 经 property_service 写入"));
            }
        }
    }
}

/// 经 property_service 写非只读属性（见 set_prop 注释）；用 set_svc 而非 set，
/// 后者成功后仍会 reset+重建上下文属性区，热区并发不安全。
pub(super) fn set_prop_svc(key: &str, value: &str) {
    if resetprop::set_svc(key, value).is_ok() {
        kmsg(&format!("{key}={value}（property_service）"));
    } else {
        kmsg(&format!("set {key} 经 property_service 失败"));
    }
}

pub(super) fn delete_prop_if_exists(key: &str) {
    if resetprop::get(key).is_some() {
        if let Err(e) = resetprop::delete(key) {
            kmsg(&format!("delete {key} 失败: {e}"));
        } else {
            kmsg(&format!("{key} deleted"));
        }
    }
}