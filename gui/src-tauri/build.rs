fn main() {
    // 复用 C++ 内核：CLI 全部逻辑 + 端口检测 / 进程 / 配置 / Docker 标注
    // 注意：cc 的工作目录是 src-tauri，C++ 源码在项目根目录（../../）
    cc::Build::new()
        .cpp(true)
        .std("c++17")
        .opt_level(2)
        .cpp_link_stdlib(None) // 手动指定静态 libstdc++，避免依赖 libstdc++-6.dll
        .include("../..")
        .file("../../main.cpp")
        .file("../../port_checker.cpp")
        .file("../../process_manager.cpp")
        .file("../../config_manager.cpp")
        .file("../../special_process.cpp")
        .file("../../watch_tray.cpp")
        .file("../../gui_api.cpp")
        .define("PORTLENS_NO_MAIN", None)
        .flag_if_supported("-w")
        .compile("portlens_core");

    for lib in ["ws2_32", "iphlpapi", "psapi", "version", "shell32", "user32", "gdi32"] {
        println!("cargo:rustc-link-lib={}", lib);
    }
    println!("cargo:rustc-link-arg=-static-libgcc");
    println!("cargo:rustc-link-arg=-l:libstdc++.a");
    // rustc 使用 -nodefaultlibs，需补回 MinGW 运行时库
    println!("cargo:rustc-link-arg=-lmingwex");
    println!("cargo:rustc-link-arg=-lmingw32");
    println!("cargo:rustc-link-arg=-l:libwinpthread.a");
    println!("cargo:rustc-link-arg=-lgcc");
    println!("cargo:rustc-link-arg=-lgcc_eh");
    println!("cargo:rustc-link-arg=-lkernel32");
    println!("cargo:rustc-link-arg=-lmsvcrt");

    // C++ 源码变更时重新编译
    for f in [
        "main.cpp",
        "gui_api.cpp",
        "port_checker.cpp",
        "process_manager.cpp",
        "config_manager.cpp",
        "special_process.cpp",
        "watch_tray.cpp",
    ] {
        println!("cargo:rerun-if-changed=../../{}", f);
    }

    tauri_build::build()
}
