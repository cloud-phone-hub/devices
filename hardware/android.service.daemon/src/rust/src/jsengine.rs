//! quickjs 嵌入脚本引擎：宿主经 hvc7 下发 JS 脚本，guest 执行并回传结果。
//!
//! 引擎只负责 Runtime/Context/执行/序列化，与 native 桥接解耦：
//! 桥接按业务拆分在 `scripts/` 子模块（如 scripts/resetpropbridge），
//! 执行前统一注册。
//!
//! 每次执行新建 Runtime/Context（毫秒级），脚本间无状态共享，避免跨命令
//! 泄漏与累积内存。中断处理器限定执行时长：hvc-event 线程遵循"通道永不
//! 阻塞"原则，死循环脚本不能拖死事件循环。

#[path = "scripts/mod.rs"]
mod scripts;

use rquickjs::{Context, Ctx, Function, Object, Runtime, Value};
use std::time::{Duration, Instant};

/// 单条脚本执行上限：属性操作正常毫秒级返回，2s 足够覆盖批量循环；
/// 超限中断返回错误，保证 hvc-event 线程即时回到读循环。
const EVAL_TIMEOUT: Duration = Duration::from_secs(2);

/// 执行脚本，返回结果值的 JSON 序列化字符串；执行错误/超时返回 Err。
/// 脚本可通过 `return <expr>` 或直接末表达式返回值。
pub fn eval(script: &str) -> Result<String, String> {
    let rt = Runtime::new().map_err(|e| format!("runtime: {e}"))?;
    // 超时中断：interrupt handler 返回 true 即抛中断异常终止执行
    let start = Instant::now();
    rt.set_interrupt_handler(Some(Box::new(move || start.elapsed() > EVAL_TIMEOUT)));
    let ctx = Context::full(&rt).map_err(|e| format!("context: {e}"))?;

    ctx.with(|ctx| {
        // 注册全部 native 桥接到全局作用域（见 scripts/ 下各 bridge 模块）
        scripts::register_all(ctx.clone())?;
        let val: Value = ctx.eval(script.as_bytes()).map_err(js_err)?;
        stringify(ctx, &val)
    })
}

/// 把 JS 值序列化为 JSON 文本：基本类型直转，对象/数组走 JSON.stringify
fn stringify<'js>(ctx: Ctx<'js>, v: &Value<'js>) -> Result<String, String> {
    let out = match v.type_of() {
        rquickjs::Type::Undefined | rquickjs::Type::Null => "null".to_string(),
        rquickjs::Type::Bool => (v.as_bool().unwrap_or(false)).to_string(),
        rquickjs::Type::Int => v.as_int().unwrap_or(0).to_string(),
        rquickjs::Type::Float => serde_json::to_string(&v.as_float().unwrap_or(0.0))
            .unwrap_or_else(|_| "null".into()),
        rquickjs::Type::String => v
            .as_string()
            .and_then(|s| s.to_string().ok())
            .map(|s| serde_json::to_string(&s).unwrap_or_else(|_| "null".into()))
            .unwrap_or_else(|| "null".to_string()),
        _ => {
            // 对象/数组/其他：借 JS 自己的 JSON.stringify 序列化
            let json: Object = ctx.globals().get("JSON").map_err(js_err)?;
            let fn_str: Function = json.get("stringify").map_err(js_err)?;
            let s: String = fn_str.call((v.clone(),)).map_err(js_err)?;
            s
        }
    };
    Ok(out)
}

fn js_err(e: impl std::fmt::Display) -> String {
    format!("{e}")
}