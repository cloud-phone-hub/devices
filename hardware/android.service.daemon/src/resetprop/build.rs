extern crate cc;

use cc::Build;
use std::path::PathBuf;

fn main() {
    // 仅支持 Android 目标（bionic system_properties + Linux 头）
    let target_os = std::env::var("CARGO_CFG_TARGET_OS").unwrap_or_default();
    if target_os != "android" {
        panic!("resetprop only supports android target (got {target_os}); build with --target aarch64-linux-android");
    }

    let manifest_dir = PathBuf::from(std::env::var("CARGO_MANIFEST_DIR").unwrap());
    let external_dir = manifest_dir.join("external");

    let sp_dir = external_dir.join("system_properties");
    if !sp_dir.join("CMakeLists.txt").exists() {
        panic!("missing {} or patch not applied; run just fetch-external", sp_dir.display());
    }
    let nanopb_dir = external_dir.join("nanopb");
    if !nanopb_dir.join("pb.h").exists() {
        panic!("missing {}/pb.h; run just fetch-external", nanopb_dir.display());
    }

    println!("cargo:rerun-if-changed=src/resetprop.cpp");
    println!("cargo:rerun-if-changed=src/persist.cpp");
    println!("cargo:rerun-if-changed=src/util.cpp");
    println!("cargo:rerun-if-changed=src/wrapper.cpp");

    // Compile system_properties library (all sources, including system_property_api.cpp)
    let sp_sources = [
        "context_node.cpp",
        "contexts_serialized.cpp",
        "contexts_split.cpp",
        "prop_area.cpp",
        "prop_info.cpp",
        "system_properties.cpp",
        "system_property_api.cpp",
        "system_property_set.cpp",
        "property_info_parser.cpp",
    ];
    let mut sp_build = Build::new();
    sp_build.cpp(true)
        .cpp_link_stdlib("c++_static")
        .std("c++20")
        .flag("-Wno-unused-function")
        .include(sp_dir.join("include"));
    for src in &sp_sources {
        sp_build.file(sp_dir.join(src));
    }
    sp_build.compile("systemproperties");

    // Compile nanopb C sources
    let nanopb_sources = ["pb_common.c", "pb_decode.c", "pb_encode.c"];
    let mut nanopb_build = Build::new();
    nanopb_build.include(&nanopb_dir);
    for src in &nanopb_sources {
        nanopb_build.file(nanopb_dir.join(src));
    }
    nanopb_build.compile("nanopb");

    // Compile resetprop sources
    let local_sources = [
        "src/resetprop.cpp",
        "src/persist.cpp",
        "src/util.cpp",
        "src/wrapper.cpp",
    ];
    let mut rp_build = Build::new();
    rp_build.cpp(true)
        .cpp_link_stdlib("c++_static")
        .std("c++20")
        .define("RESETPROP_LIB", None) // 剔除 resetprop.cpp 内置 CLI main，避免与 daemon 冲突
        .flag("-Wno-unused-function")
        .include(manifest_dir.join("src"))
        .include(&nanopb_dir)
        .include(sp_dir.join("include"));
    for f in &local_sources {
        rp_build.file(manifest_dir.join(f));
    }
    rp_build.compile("resetprop");

    // 静态链接 C++ 运行时，避免最终二进制依赖 libc++_shared.so
    // -nostdlib++ 阻止 clang++ 自动加 -lc++_shared，显式链接静态库
    let sysroot = std::env::var("SYSROOT").unwrap_or_else(|_| {
        let tc = std::env::var("CARGO_TARGET_AARCH64_LINUX_ANDROID_LINKER").unwrap_or_default();
        let mut p = std::path::PathBuf::from(tc);
        p.pop();
        p.pop();
        p.pop();
        p.pop();
        p.push("sysroot/usr/lib/aarch64-linux-android");
        p.to_string_lossy().into_owned()
    });
    println!("cargo:rustc-link-arg=-nostdlib++");
    println!("cargo:rustc-link-arg=-L{sysroot}");
    println!("cargo:rustc-link-arg=-l:libc++_static.a");
    println!("cargo:rustc-link-arg=-l:libc++abi.a");
}