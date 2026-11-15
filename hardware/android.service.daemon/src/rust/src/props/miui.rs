//! MIUI 修复项（props 模块子项：品牌闸门内的 MIUI 专属属性修正）。

use crate::log::kmsg;

/// 设定 MIUI 桌面为主屏（com.miui.home）。
/// 品牌闸门：只对 Xiaomi/Redmi/POCO 生效——此前无条件 set，导致 Pixel 等
/// 非 MIUI 机型凭空多出 ro.miui.product.home，被 device-checker 属性对拍抓出。
pub fn fix() {
    let brand = resetprop::get("ro.product.brand")
        .or_else(|| resetprop::get("ro.product.vendor.brand"))
        .unwrap_or_default()
        .to_lowercase();
    if !matches!(brand.as_str(), "xiaomi" | "redmi" | "poco") {
        kmsg(&format!("miui::fix 跳过：品牌 {} 非 MIUI 系", brand));
        return;
    }
    // set_direct 直写属性共享内存（绕过 property_service 的 SELinux 检查），常规 set 兜底
    match resetprop::set_direct("ro.miui.product.home", "com.miui.home") {
        Ok(_) => kmsg("ro.miui.product.home set to com.miui.home via direct mmap"),
        Err(e) => {
            kmsg(&format!("set_direct ro.miui.product.home failed: {}", e));
            if resetprop::set("ro.miui.product.home", "com.miui.home").is_ok() {
                kmsg("ro.miui.product.home set to com.miui.home via api");
            }
        }
    }
}