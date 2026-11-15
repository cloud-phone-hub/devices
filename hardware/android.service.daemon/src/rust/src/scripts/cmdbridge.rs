//! cmd 桥接：JS 全局 `cmd` 对象 —— 在 guest 侧执行 shell 命令。
//!
//!   cmd.exec(cmdline[, timeoutMs]) -> string
//!       同步执行 `sh -c cmdline`，返回 stdout+stderr 合并文本（去尾随空白）。
//!       超时/无法执行时返回带标记的错误文本，不抛 JS 异常。
//!   cmd.run(cmdline[, timeoutMs]) -> string（JSON）
//!       结构化结果 JSON 字符串：{"ok":bool,"code":i32,"out":str,"err":str,
//!       "timed_out":bool}；`ok` = 未超时且退出码 0；超时时 `code=-1`。
//!       JS 侧 `JSON.parse(cmd.run(...))` 还原对象。不直接返回 JS 对象：
//!       闭包内构造 Object 需捕获 Ctx 存进 JS 函数，rquickjs 0.9 会在
//!       JS_FreeRuntime 断言失败（gc_obj_list 非空）导致 daemon crash
//!       （实测 typeof cmd 即触发）；返回字符串与 exec 同款，无生命周期纠缠。
//!
//! 超时默认 5s、上限 30s，到点 kill 子进程。hvc-event 线程遵循"通道永不
//! 阻塞"：命令卡死（等待输入/挂起）不能拖死事件循环，因此执行是轮询式
//! 的 —— poll 非阻塞读输出 + try_wait 查退出，全程可中断，kill 后仍有收尾
//! 窗口兜底（后台孙进程继承 pipe 写端时不会永久卡死）。
//!
//! 注意：jsengine 单条脚本限时 2s（字节码中断），原生命令执行时长不计入，
//! 但执行返回后若脚本总耗已超 2s 该次执行仍会被判超时丢弃结果 —— 长命令
//! 请自行控制脚本总耗时。

use crate::log::kmsg;
use rquickjs::prelude::Rest;
use rquickjs::{Ctx, Function, Object};
use std::io::{ErrorKind, Read};
use std::os::unix::io::AsRawFd;
use std::process::{Command, Stdio};
use std::time::{Duration, Instant};

/// 默认命令超时（毫秒）；脚本可用第二参覆盖，但不得超过上限
const DEFAULT_TIMEOUT_MS: u64 = 5000;
const MAX_TIMEOUT_MS: u64 = 30000;
/// kill 后继续收尾输出的窗口：正常命令 kill 后立即 EOF；后台孙进程继承
/// pipe 写端时 poll 一直可读，靠该窗口兜底，不让线程永久卡死
const DRAIN_MS: u64 = 300;

// ---- 轮询式子进程输出收集（直连 FFI poll/fcntl，不引 libc crate，
//      与 channel.rs 的 tcgetattr 同款约定）----

#[repr(C)]
struct PollFd {
    fd: i32,
    events: i16,
    revents: i16,
}

extern "C" {
    fn poll(fds: *mut PollFd, nfds: u64, timeout_ms: i32) -> i32;
    fn fcntl(fd: i32, cmd: i32, arg: i32) -> i32;
}

const POLLIN: i16 = 0x0001;
const F_GETFL: i32 = 3;
const F_SETFL: i32 = 4;
const O_NONBLOCK: i32 = 0o4000;

/// 管道 fd 切非阻塞：poll 就绪后 read 读到 EAGAIN 即退出，避免阻塞 read
/// 挂在"已 poll 可读但当前无更多数据"的空档
fn set_nonblock(f: &impl AsRawFd) {
    unsafe {
        let fl = fcntl(f.as_raw_fd(), F_GETFL, 0);
        if fl >= 0 {
            fcntl(f.as_raw_fd(), F_SETFL, fl | O_NONBLOCK);
        }
    }
}

struct RunOut {
    code: i32,
    out: String,
    err: String,
    timed_out: bool,
}

/// 同步执行 `sh -c cmdline`：轮询读输出并等待退出，超时 kill。
/// 不会永久阻塞：poll 带超时、kill 后有收尾窗口。
fn run_cmd(cmdline: &str, timeout_ms: u64) -> RunOut {
    let mut child = match Command::new("sh")
        .arg("-c")
        .arg(cmdline)
        .env("PATH", "/system/bin:/vendor/bin:/system/xbin")
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
    {
        Ok(c) => c,
        Err(e) => {
            return RunOut { code: -1, out: String::new(), err: format!("spawn: {e}"), timed_out: false }
        }
    };

    let mut out_f = child.stdout.take();
    let mut err_f = child.stderr.take();
    if let Some(f) = out_f.as_ref() {
        set_nonblock(f);
    }
    if let Some(f) = err_f.as_ref() {
        set_nonblock(f);
    }
    let mut out = Vec::new();
    let mut err = Vec::new();
    let deadline = Instant::now() + Duration::from_millis(timeout_ms);
    let mut timed_out = false;
    let mut buf = [0u8; 4096];

    loop {
        // 超时 kill 后只再收尾 DRAIN_MS，防孙进程持写端拖死
        if timed_out && Instant::now() > deadline + Duration::from_millis(DRAIN_MS) {
            break;
        }
        let remain = deadline.saturating_duration_since(Instant::now());
        let poll_ms: i32 = if timed_out { DRAIN_MS as i32 } else { remain.as_millis().min(100) as i32 };
        let mut fds = [
            PollFd { fd: out_f.as_ref().map(|f| f.as_raw_fd()).unwrap_or(-1), events: POLLIN, revents: 0 },
            PollFd { fd: err_f.as_ref().map(|f| f.as_raw_fd()).unwrap_or(-1), events: POLLIN, revents: 0 },
        ];
        unsafe {
            poll(fds.as_mut_ptr(), fds.len() as u64, poll_ms);
        }
        for (i, target) in [(0usize, &mut out), (1, &mut err)] {
            if fds[i].revents & POLLIN != 0 {
                let f: Option<&mut dyn Read> = match i {
                    0 => out_f.as_mut().map(|f| f as &mut dyn Read),
                    _ => err_f.as_mut().map(|f| f as &mut dyn Read),
                };
                if let Some(f) = f {
                    loop {
                        match f.read(&mut buf) {
                            Ok(0) => break, // EOF
                            Ok(n) => target.extend_from_slice(&buf[..n]),
                            Err(e) if e.kind() == ErrorKind::Interrupted => continue,
                            Err(_) => break, // EAGAIN/其他：等下一轮 poll
                        }
                    }
                }
            }
        }
        if timed_out {
            continue;
        }
        match child.try_wait() {
            Ok(Some(st)) => {
                return RunOut {
                    code: st.code().unwrap_or(-1),
                    out: String::from_utf8_lossy(&out).into_owned(),
                    err: String::from_utf8_lossy(&err).into_owned(),
                    timed_out: false,
                };
            }
            Ok(None) => {
                if Instant::now() >= deadline {
                    timed_out = true;
                    let _ = child.kill();
                    kmsg(&format!("cmd: kill after {timeout_ms}ms: {cmdline}"));
                }
            }
            Err(e) => {
                let _ = child.kill();
                let _ = child.wait();
                return RunOut {
                    code: -1,
                    out: String::from_utf8_lossy(&out).into_owned(),
                    err: format!("wait: {e}"),
                    timed_out: false,
                };
            }
        }
    }

    // 收尾窗口耗尽（子进程已 kill），按超时返回；wait 立即回收避免悬尸
    let _ = child.wait();
    RunOut {
        code: -1,
        out: String::from_utf8_lossy(&out).into_owned(),
        err: String::from_utf8_lossy(&err).into_owned(),
        timed_out: true,
    }
}

/// 注册 `cmd` 到全局作用域
pub fn register(ctx: Ctx) -> Result<(), String> {
    let obj = Object::new(ctx.clone()).map_err(js_err)?;

    // rquickjs 0.9 的 Option<T> 参数仍算必需占位（缺参会 TypeError），
    // 可选第二参用 Rest<u64> 收集：`cmd.exec(cmdline)` 或 `cmd.exec(cmdline, 3000)`。
    let exec = Function::new(ctx.clone(), |cmdline: String, timeout: Rest<u64>| -> String {
        let t = timeout.first().copied().unwrap_or(DEFAULT_TIMEOUT_MS).min(MAX_TIMEOUT_MS);
        let r = run_cmd(&cmdline, t);
        let merged = format!("{}{}", r.out, r.err).trim_end().to_string();
        if r.timed_out {
            format!("[timed out after {t}ms] {merged}")
        } else {
            merged
        }
    })
    .map_err(js_err)?;

    let run = Function::new(ctx.clone(), |cmdline: String, timeout: Rest<u64>| -> String {
        let t = timeout.first().copied().unwrap_or(DEFAULT_TIMEOUT_MS).min(MAX_TIMEOUT_MS);
        let r = run_cmd(&cmdline, t);
        // JSON 序列化输出，避免闭包内构造 JS 对象（捕获 Ctx 会触发
        // JS_FreeRuntime 断言崩溃，见文件头注释）。
        let ok = !r.timed_out && r.code == 0;
        let code = if r.timed_out { -1 } else { r.code };
        format!(
            r#"{{"ok":{ok},"code":{code},"out":{},"err":{},"timed_out":{}}}"#,
            serde_json::to_string(&r.out).unwrap_or_else(|_| "null".into()),
            serde_json::to_string(&r.err).unwrap_or_else(|_| "null".into()),
            r.timed_out,
        )
    })
    .map_err(js_err)?;

    obj.set("exec", exec).map_err(js_err)?;
    obj.set("run", run).map_err(js_err)?;
    ctx.globals().set("cmd", obj).map_err(js_err)?;
    Ok(())
}

fn js_err(e: impl std::fmt::Display) -> String {
    format!("{e}")
}
