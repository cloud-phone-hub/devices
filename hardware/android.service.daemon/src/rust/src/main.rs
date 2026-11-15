//! service_daemon: Android guest 侧守护进程
//!
//! 流程: init 属性系统 -> 启用 adb 认证 -> miui 修复 -> 启动身份(props:
//!      chipid/cpuid/serialno 按实例生成+缓存) -> virtio-serial 事件通道
//!      （--event 指定设备，默认 /dev/hvc7）-> hvc2 ADBKEY1 握手收宿主公钥
//!      （adbd/adbkey.rs，默认 /dev/hvc2）-> fork 子进程进入 adb_net netns 并 exec
//!      APEX adbd（adbd/adbd_launch.rs，--adb-cmd/--adb-arg 传入命令）-> 心跳循环
//!
//! B 方案：不再内置自研 Rust adbd（原 adbd/ 模块已删除），ADB 由平台 APEX adbd
//! 在 adb_net 网络命名空间内提供（监听 TCP 5555，host 经 SLIRP hostfwd 转发）；
//! 自研 adbd 的 RSA 验签缺口由「公钥注入 + ro.adb.secure=1」补齐；
//! 云机调试期 ro.adb.secure=0 免认证直接拉起（见 adbd/mod.rs）。
//!
//! 注：原 wifi AP 数据面模块（wifi/mod.rs，hostapd + hvc10 转发）已随 v8 方案退役——
//! wifi 列表/连接/数据面改由内核 mac80211_hwsim 承担（假扫描 + 假连接 + 以太帧直桥
//! /dev/hvc10），daemon 不再参与数据面，仅经事件通道代理 `wifi_connect` 连接指定。

mod adbd;
mod channel;
mod jsengine;
mod log;
mod mgmt_bridge;
mod props;
mod syscall;

use log::kmsg;

// bionic waitpid（不引 libc crate，与 channel.rs 的 tcgetattr 同款直连 FFI）:
// spawn 不等待的子进程（input keyevent/text）退出后由
// 内核挂成僵尸，daemon 是直接父进程，必须自己收尸
extern "C" {
    fn waitpid(pid: i32, status: *mut i32, options: i32) -> i32;
}
const WNOHANG: i32 = 1;

/// 回收所有已退出的直接子进程（WNOHANG 循环到无僵尸为止）
fn reap_children() {
    unsafe {
        while waitpid(-1, std::ptr::null_mut(), WNOHANG) > 0 {}
    }
}

fn main() {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let mut event_dev = channel::HVC_EVENT_DEFAULT.to_string();
    let mut adb_cmd = adbd::adbd_launch::ADBD_DEFAULT.to_string();
    let mut adb_args: Vec<String> = Vec::new();
    let mut i = 0;
    while i < args.len() {
        match args[i].as_str() {
            "run" => {} // 常驻模式（init 默认入口）
            "--event" => {
                i += 1;
                if i >= args.len() {
                    eprintln!("--event 缺少设备路径参数");
                    eprintln!("usage: service_daemon [run] [--event <device>] [--adb-cmd <path>] [--adb-arg <arg>]...");
                    std::process::exit(2);
                }
                event_dev = args[i].clone();
            }
            "--adb-cmd" => {
                i += 1;
                if i >= args.len() {
                    eprintln!("--adb-cmd 缺少命令路径参数");
                    eprintln!("usage: service_daemon [run] [--event <device>] [--adb-cmd <path>] [--adb-arg <arg>]...");
                    std::process::exit(2);
                }
                adb_cmd = args[i].clone();
            }
            "--adb-arg" => {
                i += 1;
                if i >= args.len() {
                    eprintln!("--adb-arg 缺少参数值");
                    eprintln!("usage: service_daemon [run] [--event <device>] [--adb-cmd <path>] [--adb-arg <arg>]...");
                    std::process::exit(2);
                }
                adb_args.push(args[i].clone());
            }
            other => {
                eprintln!("unknown argument: {other}");
                eprintln!("usage: service_daemon [run] [--event <device>] [--adb-cmd <path>] [--adb-arg <arg>]...");
                std::process::exit(2);
            }
        }
        i += 1;
    }

    kmsg("starting");
    // panic 钩子：线程 panic 默认只写 stderr（init 重定向 /dev/null，完全不可见），
    // 曾导致 hvc-input 线程无声死亡。统一转到 kmsg。
    std::panic::set_hook(Box::new(|info| {
        let loc = info
            .location()
            .map(|l| format!("{}:{}", l.file(), l.line()))
            .unwrap_or_default();
        kmsg(&format!(
            "panic thread={:?} at {loc}: {}",
            std::thread::current().name(),
            if let Some(s) = info.payload().downcast_ref::<&str>() {
                *s
            } else if let Some(s) = info.payload().downcast_ref::<String>() {
                s.as_str()
            } else {
                "?"
            },
        ));
    }));
    resetprop::init();

    // 版本探针：确认 guest 执行的确实是 patch.img 里这份二进制
    // （cdh_redirect 把 /vendor/bin 重定向到 /mnt/vendorpatch/vendor/bin）。
    kmsg(&format!(
        "daemon: 版本探针 adbkey-handshake v2（ADB_SECURE={}）",
        adbd::ADB_SECURE as u8
    ));

    // adbd 认证开关：ro.adb.secure=1 时 APEX adbd 用 /data/misc/adb/adb_keys
    // 里的宿主公钥做 RSA 验签（公钥由 hvc2 ADBKEY1 握手注入，见 adbd/adbkey.rs）。
    adbd::apply_props();

    // miui
    props::miui::fix();

    // 启动身份（chipid/cpuid/serialno）：按实例生成 + 缓存复用，
    // 见 props/（真机由 bootloader/efuse、cmdline serialno 提供，云机无对应来源；
    // 机型差异口径集中在 props/profile.rs 的机型档案表）
    props::apply();
    props::spawn_post_boot_scrub();

    // 管理网卡隔离桥：将 mgmt0 移入独立网络命名空间并桥接 TCP 5555 -> 主命名空间 adbd，
    // 须在派生长期子进程（如 PTY shell）前完成主命名空间网卡清理。
    mgmt_bridge::spawn();

    // virtio-serial 宿主事件通道: 推送/控制（--event 指定设备）
    channel::spawn(event_dev);

    // B 方案 adb 链：先经 hvc2 ADBKEY1 握手收宿主公钥，再 fork 子进程进
    // adb_net netns 并 exec 传入的 adbd 命令（adbd_launch 常驻管理 adbd 生命周期）。
    {
        let adb_cmd = adb_cmd.clone();
        let adb_args = adb_args.clone();
        if let Err(e) = std::thread::Builder::new()
            .name("adb".into())
            .spawn(move || {
                if let Err(e) = adbd::adbkey::run_handshake_retry() {
                    kmsg(&format!("adbkey: 握手循环异常退出 {e}"));
                    std::process::exit(1);
                }
                adbd::adbd_launch::launch_forever(&adb_cmd, &adb_args);
            })
        {
            kmsg(&format!("adbkey: 线程创建失败 {e}"));
        }
    }

    // 心跳: 每 10 秒打一条内核日志并回收僵尸子进程（shell/adbd child）。
    // B 方案 adbd 由 adbd_launch 线程自行 waitpid 管理，不再依赖平台 adbd
    // 服务的启停（原 init.svc.adbd 保活/CNXN 探针逻辑随自研 adbd 一并移除）。
    let mut n: u64 = 0;
    loop {
        kmsg(&format!("heartbeat {}", n));
        reap_children();
        n += 1;
        std::thread::sleep(std::time::Duration::from_secs(10));
    }
}