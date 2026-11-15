//! virtio-serial 宿主通道: 事件/控制单端口，设备路径由 `--event` 参数注入
//! （默认 /dev/hvc7；cph_daemon.rc 显式传 /dev/hvc7）。
//!
//! 端口由宿主 qemu.go virtconArgs 追加，顺序即 guest /dev/hvcN 编号：
//!   v3 顺延布局（DenseVirtconsole，note13）: hvc7 = event（hvc0/1 留给固件，
//!                                             hvc2 预留，keymint 占 hvc3 顺延）
//!   旧布局（pixel6 等未迁移设备）:            hvc7 = event
//!   帧: [u32 BE len][JSON]
//!        guest 收: keyevent{code} / text{text} / ping / js{script}
//!                  / shell{input}（PTY sh -i 终端输入）
//!        guest 发: {"type":"status",...} / {"type":"ack","for":..,"ok":..,"detail":..}
//!                  / {"type":"shell_output","data":..}（终端输出流式回传）
//!
//! shell 终端（参考 kernel daemon src/sys/shell.rs）：openpty + fork，child
//! setsid/dup2/TIOCSCTTY 后 exec /system/bin/sh -i，master 输出由独立线程
//! 轮询读回并组帧回传；输入经事件通道 "shell" 帧写入 master。全部直连 FFI，
//! 不引 libc crate（fork 在事件线程启动前单线程执行，child 分支只做
//! async-signal-safe 调用）。
//!
//! 触摸/按键输入已改走 VNC（virtio-tablet/virtio-keyboard）与 QMP
//! input-send-event，本通道只承载推送/控制 JSON。

use crate::log::kmsg;
use serde_json::{json, Value};
use std::ffi::{c_char, CString};
use std::fs::{File, OpenOptions};
use std::io::{self, ErrorKind, Read, Write};
use std::os::unix::io::{AsRawFd, FromRawFd};
use std::ptr::{null, null_mut};
use std::sync::Mutex;
use std::time::Duration;

// 默认事件通道设备（v3 顺延布局与旧布局均为 hvc7）；rc 显式传参覆盖。
pub const HVC_EVENT_DEFAULT: &str = "/dev/hvc7";

// ---- 终端 raw 模式 ----
// hvcN 是 tty 设备，默认 N_TTY 规范模式：输出 ONLCR 把 \n(0x0A) 转 \r\n，
// 输入按行缓冲且 ICRNL 转 \r，对二进制帧/H.264 流都是灾难。打开后立即
// cfmakeraw 等价处理。直接 FFI bionic tcgetattr/tcsetattr，避免引入 libc crate。

#[repr(C)]
#[derive(Clone, Copy)]
struct Termios {
    c_iflag: u32,
    c_oflag: u32,
    c_cflag: u32,
    c_lflag: u32,
    c_line: u8,
    c_cc: [u8; 32],
    c_ispeed: u32,
    c_ospeed: u32,
}

extern "C" {
    fn tcgetattr(fd: i32, t: *mut Termios) -> i32;
    fn tcsetattr(fd: i32, actions: i32, t: *const Termios) -> i32;
}

const TCSANOW: i32 = 0;
// termbits（aarch64 linux 通用八进制值）
const IGNBRK: u32 = 0o000001;
const BRKINT: u32 = 0o000002;
const PARMRK: u32 = 0o000010;
const ISTRIP: u32 = 0o000040;
const INLCR: u32 = 0o000100;
const IGNCR: u32 = 0o000200;
const ICRNL: u32 = 0o000400;
const IXON: u32 = 0o002000;
const OPOST: u32 = 0o000001;
const ECHO: u32 = 0o000010;
const ECHONL: u32 = 0o000100;
const ICANON: u32 = 0o000002;
const ISIG: u32 = 0o000001;
const IEXTEN: u32 = 0o100000;
const CSIZE: u32 = 0o000060;
const PARENB: u32 = 0o000400;
const CS8: u32 = 0o000060;
const VTIME: usize = 5;
const VMIN: usize = 6;

// ---- shell 终端：poll/openpty/fork 等 FFI（bionic 直连，与 tcgetattr 同款约定）----

#[repr(C)]
#[derive(Clone, Copy)]
struct PollFd {
    fd: i32,
    events: i16,
    revents: i16,
}

#[repr(C)]
#[derive(Clone, Copy)]
struct Winsize {
    ws_row: u16,
    ws_col: u16,
    ws_xpixel: u16,
    ws_ypixel: u16,
}

extern "C" {
    fn poll(fds: *mut PollFd, nfds: u64, timeout_ms: i32) -> i32;
    fn fcntl(fd: i32, cmd: i32, arg: i32) -> i32;
    fn openpty(
        amaster: *mut i32,
        aslave: *mut i32,
        name: *mut c_char,
        termp: *const Termios,
        winp: *const Winsize,
    ) -> i32;
    fn fork() -> i32;
    fn setsid() -> i32;
    fn dup2(oldfd: i32, newfd: i32) -> i32;
    fn ioctl(fd: i32, request: u64, ...) -> i32;
    fn execve(path: *const c_char, argv: *const *const c_char, envp: *const *const c_char) -> i32;
    fn _exit(status: i32) -> !;
}

const POLLIN: i16 = 0x0001;
const POLLERR: i16 = 0x0008;
const POLLHUP: i16 = 0x0010;
const F_GETFL: i32 = 3;
const F_SETFL: i32 = 4;
const O_NONBLOCK: i32 = 0o4000;
const TIOCSCTTY: u64 = 0x540E;

/// 把 fd 切非阻塞（PTY master 轮询读用）
fn set_nonblock(fd: i32) {
    unsafe {
        let fl = fcntl(fd, F_GETFL, 0);
        if fl >= 0 {
            fcntl(fd, F_SETFL, fl | O_NONBLOCK);
        }
    }
}

/// 把 fd 对应 tty 切到 raw 模式；失败仅记录（非 tty 时跳过即可）
pub(crate) fn make_raw(f: &File, tag: &str) {
    let fd = f.as_raw_fd();
    unsafe {
        let mut t: Termios = std::mem::zeroed();
        if tcgetattr(fd, &mut t) != 0 {
            kmsg(&format!("{tag}: tcgetattr 失败(非tty?) errno"));
            return;
        }
        t.c_iflag &= !(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
        t.c_oflag &= !OPOST;
        t.c_lflag &= !(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
        t.c_cflag &= !(CSIZE | PARENB);
        t.c_cflag |= CS8;
        t.c_cc[VMIN] = 1;
        t.c_cc[VTIME] = 0;
        if tcsetattr(fd, TCSANOW, &t) != 0 {
            kmsg(&format!("{tag}: tcsetattr 失败"));
        } else {
            kmsg(&format!("{tag}: raw 模式已设置"));
        }
    }
}

// 帧长上限：超限视为流失步（丢弃重开），防止垃圾数据打爆内存
const MAX_EVENT_FRAME: u32 = 65536;

// 事件通道写端全局共享：event_loop 打开后写入，shell-pump 线程输出回传复用。
// 断线重连时 event_loop 置空，各线程写入静默丢弃（send_event 语义，不阻塞）。
static EVENT_WRITER: Mutex<Option<File>> = Mutex::new(None);

// shell 终端全局实例（PTY master 写入端；pump 线程持 try_clone 读端）
struct Shell {
    master: Option<File>,
}
static SHELL: Mutex<Option<Shell>> = Mutex::new(None);

/// 拉起事件通道线程（常驻，断线自动重连）；同时启动 PTY shell 终端。
/// shell 先于事件线程 fork：此时进程仅主线程，child 分支干净（无遗留锁）。
/// event_dev：事件通道设备路径（v2 紧凑布局为 /dev/hvc6，旧布局 /dev/hvc7）。
pub fn spawn(event_dev: String) {
    if let Err(e) = shell_start() {
        kmsg(&format!("shell: start 失败: {e}"));
    }
    if let Err(e) = std::thread::Builder::new()
        .name("hvc-event".into())
        .spawn(move || event_loop(event_dev))
    {
        kmsg(&format!("channel: spawn hvc-event: {e}"));
    }
}

// ---- 事件通道: 推送/控制 ----

fn event_loop(dev_path: String) {
    loop {
        // 必须读写打开：writer 由该 fd 复制而来，只读打开会让所有 status/ack
        // 写入 EBADF 静默失败（历史上 status 从未到达宿主）。
        let mut dev = match OpenOptions::new().read(true).write(true).open(&dev_path) {
            Ok(f) => {
                make_raw(&f, "hvc-event");
                f
            }
            Err(e) => {
                kmsg(&format!("hvc-event: open {dev_path}: {e}"));
                std::thread::sleep(Duration::from_secs(3));
                continue;
            }
        };
        // 写端全局共享（shell-pump 等线程回传复用）；QEMU 无消费者时写入被
        // 丢弃，永不阻塞。断线重连时置空，旧引用写入失效但被静默忽略。
        {
            let mut w = EVENT_WRITER.lock().unwrap();
            *w = dev.try_clone().ok();
            if w.is_none() {
                kmsg("hvc-event: writer clone 失败");
            }
        }
        send_event(&json!({"type": "status", "daemon": "up", "proto": 1}));
        kmsg("hvc-event: 进入读循环");
        loop {
            match read_frame(&mut dev, MAX_EVENT_FRAME) {
                Ok(Some(payload)) => match serde_json::from_slice::<Value>(&payload) {
                    Ok(v) => handle_event(&v),
                    Err(e) => kmsg(&format!("hvc-event: bad json: {e}")),
                },
                Ok(None) => continue,
                Err(e) => {
                    kmsg(&format!("hvc-event: reopen: {e}"));
                    break;
                }
            }
        }
        *EVENT_WRITER.lock().unwrap() = None;
    }
}

fn handle_event(v: &Value) {
    let typ = v.get("type").and_then(Value::as_str).unwrap_or("");
    kmsg(&format!("hvc-event: 收到 {typ}"));
    // keyevent/text 一律 spawn 不等待：`input` 底层是 app_process binder call，
    // 可能卡数秒甚至挂死（实测 instance27 text 分支 status() 未返回，event 线程
    // 整体阻塞），等待违背"通道永不阻塞"。子进程由 init 收养，自生自灭。
    let (ok, detail) = match typ {
        "keyevent" => {
            let code = v.get("code").and_then(Value::as_i64).unwrap_or(-1);
            if (0..=65535).contains(&code) {
                spawn_bg(["keyevent", &code.to_string()]);
                (true, String::new())
            } else {
                (false, "bad code".into())
            }
        }
        "text" => {
            // `input text` 不接受空格，按 adb 惯例以 %s 代替
            let text = v.get("text").and_then(Value::as_str).unwrap_or("");
            let escaped = text.replace(' ', "%s");
            spawn_bg(["text", &escaped]);
            (true, String::new())
        }
        "ping" => (true, String::new()),
        // quickjs 脚本执行：结果/错误经 ack.detail 回传（2s 超时保护，见 jsengine.rs）
        // 属性修改统一走脚本内的 prop 桥接（scripts/resetpropbridge.rs），通道层不再开
        // 原生命令，host 单个/批量改属性都通过构造 JS 脚本经此入口执行。
        "js" => match v.get("script").and_then(Value::as_str) {
            Some(script) => match crate::jsengine::eval(script) {
                Ok(out) => (true, out),
                Err(e) => (false, e),
            },
            None => (false, "missing script".into()),
        },
        // shell 终端输入：写入 PTY master（sh -i 的 stdin）。调用方自带换行。
        "shell" => {
            let input = v.get("input").and_then(Value::as_str).unwrap_or("");
            let mut guard = SHELL.lock().unwrap();
            match guard.as_mut().and_then(|s| s.master.as_mut()) {
                Some(m) => match m.write_all(input.as_bytes()) {
                    Ok(_) => (true, String::new()),
                    Err(e) => (false, format!("shell 写入失败: {e}")),
                },
                None => (false, "shell 未启动".into()),
            }
        }
        // wifi 连接指定（host 决定连哪个模拟 AP）：daemon 以 root 运行，代理执行
        // `cmd wifi connect-network <ssid> <security>`。wifi HAL（uid wifi/1010）没有
        // NETWORK_SETTINGS 权限、无法触发框架连接，只有 root/shell 可以，故放在本通道。
        "wifi_connect" => {
            let ssid = v.get("ssid").and_then(Value::as_str).unwrap_or("");
            let security = v
                .get("security")
                .and_then(Value::as_str)
                .unwrap_or("open");
            if ssid.is_empty()
                || !matches!(security, "open" | "owe" | "wpa2" | "wpa3" | "wep")
            {
                (false, "bad wifi_connect args".into())
            } else {
                spawn_wifi_connect(ssid, security);
                (true, format!("connecting {ssid} ({security})"))
            }
        }
        "" => (false, "missing type".into()),
        other => (false, format!("unknown type {other}")),
    };
    kmsg(&format!("hvc-event: {typ} 处理完成 ok={ok} {detail}"));
    send_event(&json!({"type": "ack", "for": typ, "ok": ok, "detail": detail}));
}

/// 后台执行 `cmd wifi connect-network`：不等待结果（连接可能耗时数秒），
/// 完成/失败只记录内核日志，避免拖住事件循环。
fn spawn_wifi_connect(ssid: &str, security: &str) {
    let (ssid, security) = (ssid.to_string(), security.to_string());
    let spawned = std::thread::Builder::new()
        .name("wifi-connect".into())
        .spawn(move || {
            match std::process::Command::new("cmd")
                .args(["wifi", "connect-network", &ssid, &security])
                .output()
            {
                Ok(o) => kmsg(&format!(
                    "wifi_connect: {ssid} {security} -> code={:?} out={} err={}",
                    o.status.code(),
                    String::from_utf8_lossy(&o.stdout).trim(),
                    String::from_utf8_lossy(&o.stderr).trim(),
                )),
                Err(e) => kmsg(&format!("wifi_connect: {ssid} spawn 失败: {e}")),
            }
        });
    if let Err(e) = spawned {
        kmsg(&format!("wifi_connect: 线程创建失败: {e}"));
    }
}

/// 后台执行 `input` 子命令（不等待、不接管道，避免任何阻塞与 fd 纠缠）
fn spawn_bg(args: [&str; 2]) {
    match std::process::Command::new("input").args(args).spawn() {
        Ok(_) => kmsg(&format!("hvc-event: input {} spawned", args[0])),
        Err(e) => kmsg(&format!("hvc-event: input {} spawn: {e}", args[0])),
    }
}

fn send_event(v: &Value) {
    let mut payload = match serde_json::to_vec(v) {
        Ok(p) => p,
        Err(_) => return,
    };
    let mut frame = (payload.len() as u32).to_be_bytes().to_vec();
    frame.append(&mut payload);
    if let Ok(mut guard) = EVENT_WRITER.lock() {
        if let Some(f) = guard.as_mut() {
            // 无消费者/断流时静默丢弃，由状态推送/ack 机制弥补，不阻塞
            let _ = f.write_all(&frame);
        }
    }
}

// ---- shell 终端（PTY + /system/bin/sh -i，参考 kernel daemon sys/shell.rs）----

/// 启动 shell：openpty + fork。child 设会话/控制终端后 exec sh -i；
/// parent 保存 master 写入端并起 pump 线程轮询回传输出。
fn shell_start() -> Result<(), String> {
    let mut mfd: i32 = 0;
    let mut sfd: i32 = 0;
    if unsafe { openpty(&mut mfd, &mut sfd, null_mut(), null(), null()) } != 0 {
        return Err("openpty".into());
    }
    set_nonblock(mfd);
    // exec 参数与 env 在 fork 前备好：child 分支不触碰任何分配（async-signal-safe）
    let shell_path = CString::new("/system/bin/sh").unwrap();
    let sh_arg = CString::new("/system/bin/sh").unwrap();
    let dash_i = CString::new("-i").unwrap();
    let argv: [*const c_char; 3] = [sh_arg.as_ptr(), dash_i.as_ptr(), null()];
    let term = CString::new("TERM=xterm-256color").unwrap();
    let path = CString::new(
        "PATH=/sbin:/system/sbin:/system/bin:/system/xbin:/vendor/bin:/vendor/xbin",
    )
    .unwrap();
    let envp: [*const c_char; 3] = [term.as_ptr(), path.as_ptr(), null()];

    let pid = unsafe { fork() };
    if pid < 0 {
        // 回收已创建的 master/slave fd
        unsafe {
            drop(File::from_raw_fd(mfd));
            drop(File::from_raw_fd(sfd));
        }
        return Err("fork".into());
    }
    if pid == 0 {
        // child：set 会话 + 把 slave 挂到 0/1/2 + 控制终端，随后 exec（不返回）
        child_exec(sfd, shell_path.as_ptr(), argv.as_ptr(), envp.as_ptr());
    }

    // parent：slave 归 child，master 保留供写入；起 pump 线程读回输出
    unsafe { drop(File::from_raw_fd(sfd)) };
    let master = unsafe { File::from_raw_fd(mfd) };
    let pump = master.try_clone().map_err(|e| format!("master clone: {e}"))?;
    *SHELL.lock().unwrap() = Some(Shell { master: Some(master) });
    if let Err(e) = std::thread::Builder::new()
        .name("shell-pump".into())
        .spawn(move || shell_pump(pump))
    {
        kmsg(&format!("shell: spawn pump: {e}"));
    }
    kmsg(&format!("shell: sh -i 已启动 pid={pid}"));
    Ok(())
}

/// child 分支：仅 async-signal-safe 调用，不返回（成功 exec / 失败 _exit）
fn child_exec(slave: i32, path: *const c_char, argv: *const *const c_char, envp: *const *const c_char) -> ! {
    unsafe {
        setsid();
        dup2(slave, 0);
        dup2(slave, 1);
        dup2(slave, 2);
        ioctl(slave, TIOCSCTTY, 0);
        // 交互 shell 需要的行规（sh -i 也会自设，这里按参考实现预置）
        let mut t: Termios = std::mem::zeroed();
        if tcgetattr(slave, &mut t) == 0 {
            t.c_lflag |= ECHO | ICANON | ISIG;
            tcsetattr(slave, TCSANOW, &t);
        }
        execve(path, argv, envp);
    }
    unsafe { _exit(1) }
}

/// 轮询读 PTY master 输出并回传 shell_output 帧；EOF/挂断（sh 退出）时退场
fn shell_pump(mut master: File) {
    let mut buf = [0u8; 4096];
    let mut pending: Vec<u8> = Vec::new(); // 跨 read 边界的 UTF-8 残尾
    let mut fds = [PollFd { fd: master.as_raw_fd(), events: POLLIN, revents: 0 }];
    loop {
        fds[0].revents = 0;
        let n = unsafe { poll(fds.as_mut_ptr(), 1, 200) };
        if n < 0 {
            kmsg("shell-pump: poll 失败");
            break;
        }
        if n == 0 {
            continue; // 超时：heartbeat 循环
        }
        if fds[0].revents & (POLLERR | POLLHUP) != 0 {
            // 先读尽残留再退场（shell 退出前往往还有最后一段输出）
            drain_pty(&mut master, &mut buf, &mut pending);
            kmsg("shell-pump: PTY 挂断，shell 已退出");
            break;
        }
        drain_pty(&mut master, &mut buf, &mut pending);
    }
    flush_pending(&mut pending); // 退出前把残尾按替换符发掉，不丢数据
    send_event(&json!({"type": "shell_output", "data": "\r\n[shell exited]\r\n"}));
    *SHELL.lock().unwrap() = None;
}

/// 把 PTY master 当前可读输出累积进 pending 并组帧回传（非阻塞 read 到 EAGAIN）
fn drain_pty(master: &mut File, buf: &mut [u8; 4096], pending: &mut Vec<u8>) {
    loop {
        match master.read(buf) {
            Ok(0) => break, // EOF
            Ok(n) => {
                pending.extend_from_slice(&buf[..n]);
                flush_pending(pending);
            }
            Err(e) if e.kind() == ErrorKind::Interrupted => continue,
            _ => break, // WouldBlock 或错误：等下一轮 poll
        }
    }
}

/// 把 pending 中完整的 UTF-8 前缀发出，非法字节替换为 U+FFFD，不完整残尾
/// 留到下一轮（避免多字节字符被 read 边界切断而乱码）。
fn flush_pending(pending: &mut Vec<u8>) {
    if pending.is_empty() {
        return;
    }
    match std::str::from_utf8(pending) {
        Ok(s) => {
            send_event(&json!({"type": "shell_output", "data": s}));
            pending.clear();
        }
        Err(e) => {
            let valid = e.valid_up_to();
            if valid > 0 {
                // 前缀完整：先发出
                let s = std::str::from_utf8(&pending[..valid]).unwrap();
                send_event(&json!({"type": "shell_output", "data": s}));
                pending.drain(..valid);
            }
            match e.error_len() {
                Some(len) => {
                    // 真正的非法字节（非残尾）：替换后丢弃
                    send_event(&json!({"type": "shell_output", "data": "\u{FFFD}"}));
                    pending.drain(..len);
                }
                None => {} // 不完整序列：等更多数据
            }
        }
    }
}

// ---- 帧读取 ----

/// 读取 [u32 BE len][payload]；len==0 返回 None；超上限视为失步返回 Err
fn read_frame(dev: &mut File, cap: u32) -> io::Result<Option<Vec<u8>>> {
    let mut len_buf = [0u8; 4];
    dev.read_exact(&mut len_buf)?;
    let len = u32::from_be_bytes(len_buf);
    if len == 0 {
        return Ok(None);
    }
    if len > cap {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            format!("frame too large: {len}"),
        ));
    }
    let mut payload = vec![0u8; len as usize];
    dev.read_exact(&mut payload)?;
    Ok(Some(payload))
}
