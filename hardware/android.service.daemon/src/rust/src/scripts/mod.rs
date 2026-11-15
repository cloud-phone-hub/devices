//! JS <-> native 桥接模块：按业务拆分，每桥接一个文件，`register_all`
//! 在每次脚本执行（jsengine::eval）前统一注册到 JS 全局作用域。
//!
//! 新增桥接：在 scripts/ 建 `<name>.rs`，实现 `pub fn register(ctx: Ctx) -> Result<(), String>`
//! （向 ctx.globals() 挂 JS 对象/函数），并在下方 register_all 中追加一行。

mod cmdbridge;
mod resetpropbridge;

use rquickjs::Ctx;

/// 注册全部桥接；任一失败即整体失败（脚本不执行）
pub fn register_all(ctx: Ctx) -> Result<(), String> {
    cmdbridge::register(ctx.clone())?;
    resetpropbridge::register(ctx.clone())?;
    Ok(())
}