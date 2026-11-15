//! APEX adbd 启动器（B 方案：移植自 pixel6 `vendor/adbd-netns-launcher`）。
//!
//! 握手（adbkey.rs）成功、公钥已写入 adb_keys 后，本模块 fork 一个子进程：
//!
//! 1. `setns(CLONE_NEWNET)` 进入 `adb_net` 网络命名空间（`/mnt/run/adb_net`，
//!    由 netmgr_setup/netmgr.rc 创建）；
//! 2. 确保 selinuxfs 可读（netns 场景可能未挂载，必要时 `mount selinuxfs`）；
//! 3. 移除 `CAP_SYS_ADMIN`（bounding set / ambient / effective / permitted /
//!    inheritable，exec 前最小化能力）；
//! 4. `execve` 传入的 adbd 命令（`--adb-cmd` + `--adb-arg`，默认
//!    `/apex/com.android.adbd/bin/adbd`）。
//!
//! parent（daemon）waitpid 阻塞等待 adbd 退出，退出后重启子进程——与 pixel6
//! `adb_auth_loader → adbd_netns_launcher → adbd` 的单链 exec 语义等价，差别仅在
//! 于 daemon 必须保持存活（继续 hvc7 事件/心跳），故用 fork 而非整体 exec。
//!
//! 子进程分支只做 async-signal-safe 调用：所有 CString/argv/envp 在 fork 前构造。

use crate::log::kmsg;
use crate::syscall::*;
use std::ffi::{c_char, CString};
use std::time::Duration;

pub const ADBD_DEFAULT: &str = "/apex/com.android.adbd/bin/adbd";
const ADB_NET_NS: &str = "/mnt/run/adb_net";
const SELINUX_MOUNT_DIR: &str = "/sys/fs/selinux";
const SELINUX_ENFORCE: &str = "/sys/fs/selinux/enforce";

/// fork 前一次性构造好的 exec 素材（child 分支只读，不分配）
struct AdbdExec {
    argv: Vec<*const c_char>,
    envp: Vec<*const c_char>,
    // 以下 CString 用于保证 argv[0]/路径在 exec 期间存活
    _cmd: CString,
    _args: Vec<CString>,
    _env: Vec<CString>,
    netns: CString,
    selinux_enforce: CString,
    selinux_fstype: CString,
    selinux_mount_dir: CString,
}

fn build_exec(cmd: &str, args: &[String]) -> AdbdExec {
    let cmd_c = CString::new(cmd).expect("adbd cmd 含 NUL");
    let arg_c: Vec<CString> = args
        .iter()
        .map(|a| CString::new(a.as_str()).expect("adbd arg 含 NUL"))
        .collect();
    let mut argv: Vec<*const c_char> = Vec::with_capacity(args.len() + 2);
    argv.push(cmd_c.as_ptr());
    for a in &arg_c {
        argv.push(a.as_ptr());
    }
    argv.push(core::ptr::null());

    let mut env_c: Vec<CString> = Vec::new();
    for (k, v) in std::env::vars() {
        if k == "ADB_MDNS" {
            continue;
        }
        let mut s = k;
        s.push('=');
        s.push_str(&v);
        if let Ok(c) = CString::new(s) {
            env_c.push(c);
        }
    }
    // 禁用 APEX adbd 自启 mdnsd：否则即使屏蔽了 /system/etc/init/mdnsd.rc 仍会被 adbd 属性触发或拉起，导致 init.svc.mdnsd 暴露。
    env_c.push(CString::new("ADB_MDNS=0").expect("ADB_MDNS"));
    let mut envp: Vec<*const c_char> = env_c.iter().map(|c| c.as_ptr()).collect();
    envp.push(core::ptr::null());

    AdbdExec {
        argv,
        envp,
        _cmd: cmd_c,
        _args: arg_c,
        _env: env_c,
        netns: CString::new(ADB_NET_NS).expect("netns 路径"),
        selinux_enforce: CString::new(SELINUX_ENFORCE).expect("enforce 路径"),
        selinux_fstype: CString::new("selinuxfs").expect("fstype"),
        selinux_mount_dir: CString::new(SELINUX_MOUNT_DIR).expect("mount 路径"),
    }
}

const ADB_TCP_PORT_STR: &str = "5555";
const ADB_TCP_PORT_HEX_SUFFIX: &str = ":15b3"; // 5555 = 0x15B3
const WNOHANG_OPT: i32 = 1;

/// 检查子进程所在 netns 的 /proc/<pid>/net/tcp{,6} 是否已监听 5555 (st == 0A)。
fn is_adbd_listening_5555(pid: i32) -> bool {
    for table in ["tcp", "tcp6"] {
        let path = format!("/proc/{pid}/net/{table}");
        if let Ok(content) = std::fs::read_to_string(&path) {
            for line in content.lines().skip(1) {
                let mut cols = line.split_whitespace();
                let _sl = cols.next();
                let local = cols.next().unwrap_or("");
                let _rem = cols.next();
                let st = cols.next().unwrap_or("");
                if st.eq_ignore_ascii_case("0A")
                    && local.to_ascii_lowercase().ends_with(ADB_TCP_PORT_HEX_SUFFIX)
                {
                    return true;
                }
            }
        }
    }
    false
}

/// 清理瞬态启动参数 service.adb.tcp.port，保持运行期属性状态与标准 USB 设备一致。
fn clear_adb_tcp_port_prop() {
    if resetprop::delete("service.adb.tcp.port").is_ok() {
        kmsg("adbd: 5555 监听就绪，已移除 service.adb.tcp.port");
    } else if resetprop::set_svc("service.adb.tcp.port", "").is_ok() {
        kmsg("adbd: 5555 监听就绪，已置空 service.adb.tcp.port");
    }
}

/// 常驻：fork 子进程进入 netns 并 exec adbd；adbd 退出后自动重启。
/// 永不返回（`-> !`）。
pub fn launch_forever(cmd: &str, args: &[String]) -> ! {
    loop {
        // adbd 仅在进程初始化时读取 service.adb.tcp.port 决定绑定 TCP 5555；
        // 启动前写入瞬态配置，待子进程完成 bind/listen 后立即清理。
        // 非 ro. 键走 property_service 常规通道（pioneer 域 permissive 放行）；
        // 直写会 reset+重建整个上下文属性区，与并发写竞争可致 init 读属性崩溃。
        let _ = resetprop::set_svc("service.adb.tcp.port", ADB_TCP_PORT_STR);

        let ex = build_exec(cmd, args);
        let pid = unsafe { fork() };
        if pid < 0 {
            clear_adb_tcp_port_prop();
            kmsg("adbd: fork 失败，3s 后重试");
            std::thread::sleep(Duration::from_secs(3));
            continue;
        }
        if pid == 0 {
            child_exec(&ex);
        }
        kmsg(&format!("adbd: 子进程 pid={pid} 已拉起（{cmd}）"));

        let mut status = 0;
        let mut exited_early = false;
        for _ in 0..80 {
            let w = unsafe { waitpid(pid, &mut status, WNOHANG_OPT) };
            if w > 0 {
                exited_early = true;
                break;
            }
            if is_adbd_listening_5555(pid) {
                break;
            }
            std::thread::sleep(Duration::from_millis(25));
        }
        clear_adb_tcp_port_prop();

        if !exited_early {
            unsafe { waitpid(pid, &mut status, 0) };
        }
        kmsg("adbd: 子进程退出，2s 后重启");
        std::thread::sleep(Duration::from_secs(2));
    }
}

/// 子进程分支：setns → selinuxfs → 去 CAP_SYS_ADMIN → exec。不返回。
fn child_exec(ex: &AdbdExec) -> ! {
    unsafe {
        // 0) 清掉多余 fd（保留 0/1/2），避免 adbd 意外继承 daemon 的通道 fd
        for fd in 3..1024 {
            close(fd);
        }
        // 1) 进入 adb_net 网络命名空间
        let ns_fd = open(ex.netns.as_ptr(), O_RDONLY | O_CLOEXEC);
        if ns_fd < 0 || setns(ns_fd, CLONE_NEWNET) != 0 {
            if ns_fd >= 0 {
                close(ns_fd);
            }
            _exit(121); // parent 会 waitpid 到并重启
        }
        close(ns_fd);

        // 2) selinuxfs 可读性检查 + 按需挂载
        let mut c = 0u8;
        let ef = open(ex.selinux_enforce.as_ptr(), O_RDONLY);
        let ok = ef >= 0 && read(ef, &mut c, 1) > 0;
        if ef >= 0 {
            close(ef);
        }
        if !ok {
            mount(
                ex.selinux_fstype.as_ptr(),
                ex.selinux_mount_dir.as_ptr(),
                ex.selinux_fstype.as_ptr(),
                MS_NOSUID | MS_NODEV | MS_NOEXEC,
                core::ptr::null(),
            );
        }

        // 3) 移除 CAP_SYS_ADMIN（exec 前最小化能力）
        prctl(PR_CAPBSET_DROP, CAP_SYS_ADMIN as u64, 0, 0, 0);
        prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_CLEAR_ALL as u64, 0, 0, 0);
        let mut hdr = CapHdr {
            version: LINUX_CAPABILITY_VERSION_3,
            pid: 0,
        };
        let mut data = [CapData {
            effective: 0,
            permitted: 0,
            inheritable: 0,
        }; 2];
        if capget(&mut hdr, data.as_mut_ptr()) == 0 {
            let idx = (CAP_SYS_ADMIN / 32) as usize;
            let mask = 1u32 << (CAP_SYS_ADMIN % 32);
            data[idx].effective &= !mask;
            data[idx].permitted &= !mask;
            data[idx].inheritable &= !mask;
            let _ = capset(&hdr, data.as_ptr());
        }

        // 4) exec adbd（argv[0] = cmd）
        execve(ex.argv[0], ex.argv.as_ptr(), ex.envp.as_ptr());
    }
    unsafe { _exit(127) }
}
