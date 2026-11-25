//! 管理网卡独立网络命名空间隔离与 ADB TCP 5555 环回桥接。
//!
//! 背景：
//! - QEMU `net0`（SLIRP `10.0.2.15/24`，MAC `52:54:00:12:34:56`）仅承载宿主 `hostfwd`
//!   到实例 `5555` 端口的 ADB 管理流量；
//! - 若将 `mgmt0` 留在主网络命名空间，应用层调用 `NetworkInterface.getNetworkInterfaces()`
//!   会枚举出带 IPv4/IPv6 地址的 `mgmt0`；
//! - 若把 `adbd` 整体放入隔离命名空间，`adb shell` 也将被困在该命名空间内，无法访问主命名
//!   空间中的 `wlan0` / `ccmni*` 业务网络。
//!
//! 方案：
//! 1. 主线程（位于主 netns）打开 `/proc/1/ns/net` 保存主命名空间句柄；
//! 2. 工作线程调用 `unshare(CLONE_NEWNET)` 创建匿名管理网络命名空间（不挂载任何路径，不在
//!    `mountinfo` 产生额外挂载点），通过通道将自身 `tid` 回传主线程；
//! 3. 主线程在主 netns 执行 `/system/bin/ip link set mgmt0 netns <tid>` 将管理网卡迁入管理
//!    netns，主命名空间内彻底不再存在 `mgmt0`；
//! 4. 工作线程在管理 netns 内拉起 `lo` 与 `mgmt0`（`10.0.2.15/24` + 默认路由 `10.0.2.2`），
//!    并在管理 netns 内绑定 `TcpListener("0.0.0.0:5555")`；
//! 5. 工作线程调用 `setns(main_ns_fd, CLONE_NEWNET)` 切回主 netns（已绑定的 `TcpListener`
//!    套接字永久驻留管理 netns 并持有其内核引用计数），随后循环 `accept()` 来自 SLIRP 的
//!    管理连接，转发至主 netns 的 `127.0.0.1:5555`（APEX `adbd`）。

use crate::log::kmsg;
use crate::syscall::{close, errno, gettid, open, setns, unshare, CLONE_NEWNET, O_CLOEXEC, O_RDONLY};
use std::ffi::CString;
use std::io;
use std::net::{Shutdown, SocketAddr, TcpListener, TcpStream};
use std::process::Command;
use std::sync::mpsc;
use std::time::Duration;

const INIT_NET_NS_PATH: &str = "/proc/1/ns/net";
const MGMT_IFACE: &str = "mgmt0";
const MGMT_MAC: &str = "52:54:00:12:34:56";
const MGMT_CIDR: &str = "10.0.2.15/24";
const MGMT_GW: &str = "10.0.2.2";
const ADB_LISTEN_ADDR: &str = "0.0.0.0:5555";
const ADB_UPSTREAM_ADDR: &str = "127.0.0.1:5555";

/// 在主命名空间查找管理网卡名（优先 `mgmt0`，兜底匹配 `net0` 默认 MAC）。
fn find_mgmt_iface_in_main_ns() -> Option<String> {
    if std::path::Path::new("/sys/class/net/mgmt0").exists() {
        return Some(MGMT_IFACE.to_string());
    }
    let entries = std::fs::read_dir("/sys/class/net").ok()?;
    for entry in entries.flatten() {
        let name = entry.file_name().to_string_lossy().into_owned();
        if matches!(name.as_str(), "lo" | "dummy0" | "ccmni0" | "ccmni1" | "ccmni2") {
            continue;
        }
        let addr_path = entry.path().join("address");
        if let Ok(mac) = std::fs::read_to_string(addr_path) {
            if mac.trim().eq_ignore_ascii_case(MGMT_MAC) {
                return Some(name);
            }
        }
    }
    None
}

/// 调用 `/system/bin/ip` 执行网络配置命令，返回成功与否并记录失败详情。
fn run_ip(args: &[&str]) -> bool {
    match Command::new("/system/bin/ip").args(args).output() {
        Ok(out) if out.status.success() => true,
        Ok(out) => {
            kmsg(&format!(
                "mgmt_bridge: ip {:?} 失败 code={:?} stderr={}",
                args,
                out.status.code(),
                String::from_utf8_lossy(&out.stderr).trim()
            ));
            false
        }
        Err(e) if e.raw_os_error() == Some(10) => {
            // 纵深防御：若并发 waitpid(-1) 抢先回收了 ip 子进程导致 ECHILD，子进程已完成退出。
            kmsg(&format!(
                "mgmt_bridge: 执行 ip {:?} Wait 被提前回收 (ECHILD)，按已执行继续",
                args
            ));
            true
        }
        Err(e) => {
            kmsg(&format!("mgmt_bridge: 执行 ip {:?} 异常: {e}", args));
            false
        }
    }
}

/// 启动管理网卡命名空间隔离与 TCP 5555 环回转发桥。
pub fn spawn() {
    let Some(src_iface) = find_mgmt_iface_in_main_ns() else {
        kmsg("mgmt_bridge: 主命名空间未发现 mgmt0 管理网卡，跳过隔离");
        return;
    };

    let init_ns_c = CString::new(INIT_NET_NS_PATH).expect("init netns path");
    let main_ns_fd = unsafe { open(init_ns_c.as_ptr(), O_RDONLY | O_CLOEXEC) };
    if main_ns_fd < 0 {
        kmsg(&format!(
            "mgmt_bridge: 打开 {INIT_NET_NS_PATH} 失败 errno={}",
            errno()
        ));
        return;
    }

    let (tid_tx, tid_rx) = mpsc::sync_channel::<Result<i32, String>>(1);
    let (moved_tx, moved_rx) = mpsc::sync_channel::<bool>(1);
    let (ready_tx, ready_rx) = mpsc::sync_channel::<bool>(1);

    let spawn_res = std::thread::Builder::new()
        .name("mgmt-bridge".into())
        .spawn(move || {
            if unsafe { unshare(CLONE_NEWNET) } != 0 {
                let _ = tid_tx.send(Err(format!("unshare(CLONE_NEWNET) errno={}", errno())));
                unsafe { close(main_ns_fd) };
                return;
            }
            let tid = unsafe { gettid() };
            if tid_tx.send(Ok(tid)).is_err() {
                unsafe { close(main_ns_fd) };
                return;
            }

            if !matches!(moved_rx.recv(), Ok(true)) {
                let _ = unsafe { setns(main_ns_fd, CLONE_NEWNET) };
                unsafe { close(main_ns_fd) };
                let _ = ready_tx.send(false);
                return;
            }

            // 当前线程位于隔离的管理 netns，子进程 ip 继承该 netns 完成接口与路由初始化。
            let _ = run_ip(&["link", "set", "lo", "up"]);
            if !run_ip(&["link", "set", MGMT_IFACE, "up"])
                || !run_ip(&["addr", "replace", MGMT_CIDR, "dev", MGMT_IFACE])
            {
                let _ = unsafe { setns(main_ns_fd, CLONE_NEWNET) };
                unsafe { close(main_ns_fd) };
                let _ = ready_tx.send(false);
                return;
            }
            let _ = run_ip(&["route", "replace", "default", "via", MGMT_GW, "dev", MGMT_IFACE]);

            let listener = match TcpListener::bind(ADB_LISTEN_ADDR) {
                Ok(l) => l,
                Err(e) => {
                    kmsg(&format!("mgmt_bridge: 绑定 {ADB_LISTEN_ADDR} 失败: {e}"));
                    let _ = unsafe { setns(main_ns_fd, CLONE_NEWNET) };
                    unsafe { close(main_ns_fd) };
                    let _ = ready_tx.send(false);
                    return;
                }
            };

            // TcpListener 已绑定在管理 netns 上；将线程自身切回主 netns，使得后续
            // TcpStream::connect 直接连接主 netns 的 127.0.0.1:5555 (adbd)。
            if unsafe { setns(main_ns_fd, CLONE_NEWNET) } != 0 {
                kmsg(&format!(
                    "mgmt_bridge: 切回主 netns 失败 errno={}",
                    errno()
                ));
                unsafe { close(main_ns_fd) };
                let _ = ready_tx.send(false);
                return;
            }
            unsafe { close(main_ns_fd) };

            kmsg(&format!(
                "mgmt_bridge: {MGMT_IFACE} ({MGMT_CIDR}) 已隔离并监听 {ADB_LISTEN_ADDR} -> {ADB_UPSTREAM_ADDR}"
            ));
            // 同步屏障：确保工作线程所有 ip 子进程已 wait 完成且转发监听就绪，再放行 main 进入 reap_children 循环。
            let _ = ready_tx.send(true);

            for incoming in listener.incoming() {
                match incoming {
                    Ok(client) => {
                        let _ = std::thread::Builder::new()
                            .name("mgmt-adb-conn".into())
                            .spawn(move || bridge_connection(client));
                    }
                    Err(e) => {
                        kmsg(&format!("mgmt_bridge: accept 异常: {e}"));
                        std::thread::sleep(Duration::from_millis(100));
                    }
                }
            }
        });

    if let Err(e) = spawn_res {
        kmsg(&format!("mgmt_bridge: 线程创建失败: {e}"));
        unsafe { close(main_ns_fd) };
        return;
    }

    let tid = match tid_rx.recv() {
        Ok(Ok(t)) => t,
        Ok(Err(msg)) => {
            kmsg(&format!("mgmt_bridge: {msg}"));
            return;
        }
        Err(e) => {
            kmsg(&format!("mgmt_bridge: 等待工作线程 tid 失败: {e}"));
            return;
        }
    };

    let _ = run_ip(&["link", "set", &src_iface, "down"]);
    if src_iface != MGMT_IFACE && !run_ip(&["link", "set", &src_iface, "name", MGMT_IFACE]) {
        let _ = moved_tx.send(false);
        return;
    }
    let tid_str = tid.to_string();
    let moved = run_ip(&["link", "set", MGMT_IFACE, "netns", &tid_str]);
    let _ = moved_tx.send(moved);
    if moved {
        let _ = ready_rx.recv();
    }
}

/// 连接主命名空间中的 `127.0.0.1:5555`（开机初期 `adbd` 尚未就绪时短暂重试），并双向转发。
fn bridge_connection(client: TcpStream) {
    let target: SocketAddr = ADB_UPSTREAM_ADDR.parse().expect("valid upstream socket addr");
    let mut upstream_opt = None;
    for attempt in 0..20 {
        match TcpStream::connect_timeout(&target, Duration::from_millis(250)) {
            Ok(s) => {
                upstream_opt = Some(s);
                break;
            }
            Err(_) if attempt < 19 => {
                std::thread::sleep(Duration::from_millis(100));
            }
            Err(e) => {
                kmsg(&format!("mgmt_bridge: 连接主命名空间 adbd 失败: {e}"));
                let _ = client.shutdown(Shutdown::Both);
                return;
            }
        }
    }
    let Some(upstream) = upstream_opt else {
        return;
    };

    let _ = client.set_nodelay(true);
    let _ = upstream.set_nodelay(true);

    let Ok(mut c_read) = client.try_clone() else {
        return;
    };
    let Ok(mut u_write) = upstream.try_clone() else {
        return;
    };
    let mut u_read = upstream;
    let mut c_write = client;

    let c2u = std::thread::Builder::new()
        .name("mgmt-c2u".into())
        .spawn(move || {
            let _ = io::copy(&mut c_read, &mut u_write);
            let _ = u_write.shutdown(Shutdown::Write);
        });

    let _ = io::copy(&mut u_read, &mut c_write);
    let _ = c_write.shutdown(Shutdown::Write);

    if let Ok(handle) = c2u {
        let _ = handle.join();
    }
}
