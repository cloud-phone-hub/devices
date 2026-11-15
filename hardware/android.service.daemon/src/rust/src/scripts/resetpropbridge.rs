//! resetprop 桥接：JS 全局 `prop` 对象。
//!
//!   prop.get(name)              -> string | null
//!   prop.set(name, value)       -> bool（经 property_service，触发 prop 变更通知）
//!   prop.set_direct(name,value) -> bool（绕过 property_service 直写 prop area）
//!   prop.delete(name)           -> bool

use rquickjs::{Ctx, Function, Object};

/// 注册 `prop` 到全局作用域
pub fn register(ctx: Ctx) -> Result<(), String> {
    let obj = Object::new(ctx.clone()).map_err(js_err)?;

    let get = Function::new(ctx.clone(), |name: String| -> Option<String> {
        resetprop::get(&name)
    })
    .map_err(js_err)?;
    let set = Function::new(ctx.clone(), |name: String, value: String| -> bool {
        resetprop::set(&name, &value).is_ok()
    })
    .map_err(js_err)?;
    let set_direct = Function::new(ctx.clone(), |name: String, value: String| -> bool {
        resetprop::set_direct(&name, &value).is_ok()
    })
    .map_err(js_err)?;
    let del = Function::new(ctx.clone(), |name: String| -> bool {
        resetprop::delete(&name).is_ok()
    })
    .map_err(js_err)?;

    obj.set("get", get).map_err(js_err)?;
    obj.set("set", set).map_err(js_err)?;
    obj.set("set_direct", set_direct).map_err(js_err)?;
    obj.set("delete", del).map_err(js_err)?;
    ctx.globals().set("prop", obj).map_err(js_err)?;
    Ok(())
}

fn js_err(e: impl std::fmt::Display) -> String {
    format!("{e}")
}