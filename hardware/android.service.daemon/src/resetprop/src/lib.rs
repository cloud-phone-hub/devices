//! Rust FFI bindings for resetprop
//! Provides system property manipulation on Android

use std::ffi::{CStr, CString};
use std::os::raw::c_char;

const PROP_BUF_SIZE: usize = 256;

unsafe extern "C" {
    fn resetprop_init();
    fn resetprop_get(name: *const c_char, out_buf: *mut c_char, buf_size: i32) -> i32;
    fn resetprop_set(name: *const c_char, value: *const c_char) -> i32;
    fn resetprop_set_svc(name: *const c_char, value: *const c_char) -> i32;
    fn resetprop_set_direct(name: *const c_char, value: *const c_char) -> i32;
    fn resetprop_delete(name: *const c_char) -> i32;
    fn resetprop_get_context(name: *const c_char, out_buf: *mut c_char, buf_size: i32) -> i32;
    fn resetprop_reset_file(filename: *const c_char);
    fn resetprop_load_file(filename: *const c_char);
}

/// Initialize the property system (map prop areas); must run before first use
pub fn init() {
    unsafe { resetprop_init() };
}

/// Get a system property value
pub fn get(name: &str) -> Option<String> {
    let c_name = CString::new(name).ok()?;
    let mut buf = vec![0u8; PROP_BUF_SIZE];
    let ret = unsafe {
        resetprop_get(
            c_name.as_ptr(),
            buf.as_mut_ptr() as *mut c_char,
            PROP_BUF_SIZE as i32,
        )
    };
    if ret == 0 {
        let c_str = unsafe { CStr::from_ptr(buf.as_ptr() as *const c_char) };
        Some(c_str.to_string_lossy().into_owned())
    } else {
        None
    }
}

/// Set a system property value (through property_service)
pub fn set(name: &str, value: &str) -> Result<(), String> {
    let c_name = CString::new(name).map_err(|e| e.to_string())?;
    let c_value = CString::new(value).map_err(|e| e.to_string())?;
    let ret = unsafe { resetprop_set(c_name.as_ptr(), c_value.as_ptr()) };
    if ret == 0 {
        Ok(())
    } else {
        Err(format!("set_prop failed with code {}", ret))
    }
}

/// Set a system property value, bypassing property_service
pub fn set_direct(name: &str, value: &str) -> Result<(), String> {
    let c_name = CString::new(name).map_err(|e| e.to_string())?;
    let c_value = CString::new(value).map_err(|e| e.to_string())?;
    let ret = unsafe { resetprop_set_direct(c_name.as_ptr(), c_value.as_ptr()) };
    if ret == 0 {
        Ok(())
    } else {
        Err(format!("set_prop_direct failed with code {}", ret))
    }
}

/// 仅经 property_service 写入，不 reset/重建属性区。
/// 非 ro. 键（尤其 radio_prop 等热区）必须用本接口而非 set/set_direct：
/// 后两者成功后会全量重建上下文属性区，与并发写竞争可致 init 读属性崩溃。
pub fn set_svc(name: &str, value: &str) -> Result<(), String> {
    let c_name = CString::new(name).map_err(|e| e.to_string())?;
    let c_value = CString::new(value).map_err(|e| e.to_string())?;
    let ret = unsafe { resetprop_set_svc(c_name.as_ptr(), c_value.as_ptr()) };
    if ret == 0 {
        Ok(())
    } else {
        Err(format!("set_prop_svc failed with code {}", ret))
    }
}

/// Delete a system property
pub fn delete(name: &str) -> Result<(), String> {
    let c_name = CString::new(name).map_err(|e| e.to_string())?;
    let ret = unsafe { resetprop_delete(c_name.as_ptr()) };
    if ret == 0 {
        Ok(())
    } else {
        Err(format!("delete_prop failed with code {}", ret))
    }
}

/// Get the SELinux context of a property
pub fn get_context(name: &str) -> Option<String> {
    let c_name = CString::new(name).ok()?;
    let mut buf = vec![0u8; PROP_BUF_SIZE];
    let ret = unsafe {
        resetprop_get_context(
            c_name.as_ptr(),
            buf.as_mut_ptr() as *mut c_char,
            PROP_BUF_SIZE as i32,
        )
    };
    if ret == 0 {
        let c_str = unsafe { CStr::from_ptr(buf.as_ptr() as *const c_char) };
        Some(c_str.to_string_lossy().into_owned())
    } else {
        None
    }
}

/// Reset properties in a context file
pub fn reset_file(filename: &str) {
    if let Ok(c_filename) = CString::new(filename) {
        unsafe { resetprop_reset_file(c_filename.as_ptr()) };
    }
}

/// Load properties from a file
pub fn load_file(filename: &str) {
    if let Ok(c_filename) = CString::new(filename) {
        unsafe { resetprop_load_file(c_filename.as_ptr()) };
    }
}
