fn main() {
    // resetprop 是独立 crate，它的 rustc-link-arg 不会传递到最终二进制 room-daemon。
    // 这里在最终二进制链接阶段补上静态 C++ 运行时链接：
    //   - -nostdlib++ 阻止 clang++ 自动加 -lc++_shared（vendor 镜像无此库）
    //   - libc++abi.a 提供 __cxa_throw/__cxa_guard_* 等异常/守卫符号
    // libc++_static.a 由 resetprop 的 cpp_link_stdlib("c++_static") 通过 rustc-link-lib 传递。
    let tc = std::env::var("CARGO_TARGET_AARCH64_LINUX_ANDROID_LINKER").unwrap_or_default();
    let mut p = std::path::PathBuf::from(tc);
    p.pop();
    p.pop();
    p.pop();
    p.pop();
    p.push("sysroot/usr/lib/aarch64-linux-android");
    let sysroot = p.to_string_lossy().into_owned();

    println!("cargo:rustc-link-arg=-nostdlib++");
    println!("cargo:rustc-link-arg=-L{sysroot}");
    println!("cargo:rustc-link-arg=-l:libc++abi.a");
    println!("cargo:rerun-if-env-changed=CARGO_TARGET_AARCH64_LINUX_ANDROID_LINKER");
}
