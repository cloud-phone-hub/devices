//! 直连 bionic 的 FFI 与常量（B 方案 adbkey / adbd_launch 共用）。
//!
//! 与 crate 内既有约定一致（`channel.rs`）：不引 libc crate，用到什么就在此
//! 声明什么；常量取 aarch64 asm-generic 取值。结构体按 bionic 布局手写。

use std::ffi::{c_char, c_int, c_uint};
use std::io;

// ---------------- open(2) / read(2) ----------------

pub const O_RDONLY: i32 = 0;
pub const O_CLOEXEC: i32 = 0o2000000;

// ---------------- 网络命名空间 ----------------

/// setns(2) 的 nstype：CLONE_NEWNET
pub const CLONE_NEWNET: c_int = 0x4000_0000;

// ---------------- mount(2) ----------------

pub const MS_RDONLY: u64 = 1;
pub const MS_NOSUID: u64 = 2;
pub const MS_NODEV: u64 = 4;
pub const MS_NOEXEC: u64 = 8;
pub const MS_REMOUNT: u64 = 32;
pub const MS_NOSYMFOLLOW: u64 = 256;

// ---------------- prctl(2) / capabilities ----------------

pub const PR_CAPBSET_DROP: c_int = 24;
pub const PR_CAP_AMBIENT: c_int = 47;
pub const PR_CAP_AMBIENT_CLEAR_ALL: c_uint = 4;
pub const CAP_SYS_ADMIN: c_uint = 21;
pub const LINUX_CAPABILITY_VERSION_3: c_uint = 0x2008_0522;

/// linux `cap_user_header_t`
#[repr(C)]
#[derive(Clone, Copy)]
pub struct CapHdr {
    pub version: c_uint,
    pub pid: c_int,
}

/// linux `cap_user_data_t`（本流程只用 32 位版本 3 的首组）
#[repr(C)]
#[derive(Clone, Copy)]
pub struct CapData {
    pub effective: c_uint,
    pub permitted: c_uint,
    pub inheritable: c_uint,
}

// ---------------- FFI ----------------

extern "C" {
    pub fn open(path: *const c_char, flags: i32, ...) -> i32;
    pub fn close(fd: i32) -> i32;
    pub fn read(fd: i32, buf: *mut u8, count: usize) -> isize;
    pub fn fchown(fd: i32, owner: u32, group: u32) -> i32;
    pub fn chmod(path: *const c_char, mode: u32) -> i32;
    pub fn chown(path: *const c_char, owner: u32, group: u32) -> i32;
    pub fn setxattr(
        path: *const c_char,
        name: *const c_char,
        value: *const u8,
        size: usize,
        flags: c_int,
    ) -> i32;

    pub fn setns(fd: i32, nstype: c_int) -> i32;
    pub fn unshare(flags: c_int) -> i32;
    pub fn gettid() -> i32;
    pub fn mount(
        source: *const c_char,
        target: *const c_char,
        fstype: *const c_char,
        flags: u64,
        data: *const c_char,
    ) -> i32;
    pub fn prctl(option: c_int, a2: u64, a3: u64, a4: u64, a5: u64) -> i32;
    pub fn capget(hdr: *mut CapHdr, data: *mut CapData) -> i32;
    pub fn capset(hdr: *const CapHdr, data: *const CapData) -> i32;

    pub fn fork() -> i32;
    pub fn execve(path: *const c_char, argv: *const *const c_char, envp: *const *const c_char) -> i32;
    pub fn waitpid(pid: i32, status: *mut i32, options: i32) -> i32;
    pub fn _exit(status: i32) -> !;
}

// ---------------- 小工具 ----------------

/// 最近的 errno
pub fn errno() -> i32 {
    io::Error::last_os_error().raw_os_error().unwrap_or(0)
}
