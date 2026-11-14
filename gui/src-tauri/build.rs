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

    // MSVC 兼容符号（WebView2LoaderStatic 是 MSVC 编译的）：
    // GAS 无法汇编含 ?/@ 的 MSVC 修饰名，因此先用 g++ 编译出普通符号，
    // 再用 objcopy 重命名为 MSVC 修饰名，最后手工打包成静态库并整体强制链接
    {
        use std::path::PathBuf;
        use std::process::Command;

        let out_dir = PathBuf::from(std::env::var("OUT_DIR").unwrap());
        let obj = out_dir.join("msvc_compat.o");
        let obj_renamed = out_dir.join("msvc_compat_renamed.o");
        let lib = out_dir.join("libmsvc_compat.a");

        let cxx = std::env::var("CXX").unwrap_or_else(|_| "g++".to_string());
        let status = Command::new(&cxx)
            .args(["-c", "-O2", "-std=c++17", "-w", "msvc_compat.cpp", "-o"])
            .arg(&obj)
            .status()
            .expect("failed to run g++ for msvc_compat.cpp");
        assert!(status.success(), "g++ failed for msvc_compat.cpp");

        let status = Command::new("objcopy")
            .arg("--redefine-sym")
            .arg("msvc_new_nothrow=??2@YAPEAX_KAEBUnothrow_t@std@@@Z")
            .arg("--redefine-sym")
            .arg("msvc_delete=??3@YAXPEAX@Z")
            .arg("--redefine-sym")
            .arg("msvc_delete_sized=??3@YAXPEAX_K@Z")
            .arg("--redefine-sym")
            .arg("msvc_nothrow_obj_storage=?nothrow@std@@3Unothrow_t@1@B")
            .arg(&obj)
            .arg(&obj_renamed)
            .status()
            .expect("failed to run objcopy");
        assert!(status.success(), "objcopy failed for msvc_compat.o");

        let status = Command::new("ar")
            .arg("rcs")
            .arg(&lib)
            .arg(&obj_renamed)
            .status()
            .expect("failed to run ar");
        assert!(status.success(), "ar failed for libmsvc_compat.a");

        println!("cargo:rustc-link-search=native={}", out_dir.display());
        println!("cargo:rustc-link-arg=-Wl,--whole-archive");
        println!("cargo:rustc-link-arg=-l:libmsvc_compat.a");
        println!("cargo:rustc-link-arg=-Wl,--no-whole-archive");
        println!("cargo:rerun-if-changed=msvc_compat.cpp");
    }

    for lib in ["ws2_32", "iphlpapi", "psapi", "version", "shell32", "user32", "gdi32"] {
        println!("cargo:rustc-link-lib={}", lib);
    }

    // WebView2Loader 静态链接：本目录放有 GNU 格式静态存档 libWebView2Loader.dll.a
    // （源自 NuGet 的 WebView2LoaderStatic.lib），优先于 webview2-com-sys 的 DLL 导入库被搜到，
    // 链接后 exe 不再依赖 WebView2Loader.dll，实现单文件分发
    println!("cargo:rustc-link-search=native=webview2-static");
    println!("cargo:rerun-if-changed=webview2-static/libWebView2Loader.dll.a");
    println!("cargo:rustc-link-arg=-static-libgcc");
    // rustc 使用 -nodefaultlibs，需补回 MinGW 运行时库；
    // 用 --start-group 包住，避免不同 GCC 版本下静态库依赖顺序不同导致未定义符号
    println!("cargo:rustc-link-arg=-Wl,--start-group");
    for arg in [
        "-l:libstdc++.a",
        "-l:libwinpthread.a",
        "-lmingwex",
        "-lmingw32",
        "-lgcc",
        "-lgcc_eh",
        "-lmsvcrt",
        "-lkernel32",
    ] {
        println!("cargo:rustc-link-arg={}", arg);
    }
    println!("cargo:rustc-link-arg=-Wl,--end-group");

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
