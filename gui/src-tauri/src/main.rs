// PortLens - GUI 与 CLI 双模式入口
// 双击 exe → GUI；CMD 中执行 port / port.exe <参数> → 命令行
#![allow(non_snake_case)]

use std::ffi::{c_char, CStr, CString};

use serde_json::Value;
use tauri::menu::{MenuBuilder, MenuItemBuilder};
use tauri::tray::{MouseButton, MouseButtonState, TrayIconBuilder, TrayIconEvent};
use tauri::{Manager, WindowEvent};

extern "C" {
    fn portlens_cli_main(argc: i32, argv: *const *const c_char) -> i32;

    fn pl_init() -> i32;
    fn pl_cleanup();
    fn pl_should_gui() -> i32;
    fn pl_free_console();
    fn pl_install_path() -> i32;

    fn pl_list_ports(proto: i32) -> *mut c_char;
    fn pl_check_port(port: i32) -> *mut c_char;
    fn pl_scan_ports(start: i32, end: i32) -> *mut c_char;
    fn pl_process_detail(pid: u32) -> *mut c_char;
    fn pl_find_process(name: *const c_char, portFilter: i32) -> *mut c_char;
    fn pl_port_connections(port: i32) -> *mut c_char;
    fn pl_kill_pid(pid: u32) -> i32;
    fn pl_find_free_port(start: i32) -> i32;
    fn pl_export(path: *const c_char, proto: i32, fmt: i32) -> i32;
    fn pl_config_info() -> *mut c_char;
    fn pl_favorites() -> *mut c_char;
    fn pl_fav_add(port: i32) -> i32;
    fn pl_fav_remove(port: i32) -> i32;
    fn pl_history() -> *mut c_char;
    fn pl_history_clear() -> i32;
    fn pl_free_string(s: *mut c_char);
}

fn take(p: *mut c_char) -> String {
    if p.is_null() {
        return String::new();
    }
    let s = unsafe { CStr::from_ptr(p) }.to_string_lossy().into_owned();
    unsafe { pl_free_string(p) };
    s
}

fn as_json(s: String) -> Value {
    serde_json::from_str(&s).unwrap_or(Value::Null)
}

fn cstring(s: &str) -> CString {
    CString::new(s.replace('\0', "")).unwrap_or_default()
}

fn run_cli(args: &[String]) -> i32 {
    let cargs: Vec<CString> = args.iter().map(|a| cstring(a)).collect();
    let mut ptrs: Vec<*const c_char> = cargs.iter().map(|c| c.as_ptr()).collect();
    ptrs.push(std::ptr::null());
    unsafe { portlens_cli_main(cargs.len() as i32, ptrs.as_ptr()) }
}

// ---------- Tauri 命令 ----------

#[tauri::command]
fn list_ports(proto: i32) -> Value {
    as_json(take(unsafe { pl_list_ports(proto) }))
}

#[tauri::command]
fn check_port(port: i32) -> Value {
    as_json(take(unsafe { pl_check_port(port) }))
}

#[tauri::command]
fn scan_ports(start: i32, end: i32) -> Value {
    as_json(take(unsafe { pl_scan_ports(start, end) }))
}

#[tauri::command]
fn process_detail(pid: u32) -> Value {
    as_json(take(unsafe { pl_process_detail(pid) }))
}

#[tauri::command]
fn find_process(name: String, port_filter: i32) -> Value {
    let c = cstring(&name);
    as_json(take(unsafe { pl_find_process(c.as_ptr(), port_filter) }))
}

#[tauri::command]
fn port_connections(port: i32) -> Value {
    as_json(take(unsafe { pl_port_connections(port) }))
}

#[tauri::command]
fn kill_pid(pid: u32) -> bool {
    unsafe { pl_kill_pid(pid) == 1 }
}

#[tauri::command]
fn find_free_port(start: i32) -> i32 {
    unsafe { pl_find_free_port(start) }
}

#[tauri::command]
fn export_ports(path: String, proto: i32, fmt: i32) -> bool {
    let c = cstring(&path);
    unsafe { pl_export(c.as_ptr(), proto, fmt) == 1 }
}

#[tauri::command]
fn config_info() -> Value {
    as_json(take(unsafe { pl_config_info() }))
}

#[tauri::command]
fn favorites() -> Value {
    as_json(take(unsafe { pl_favorites() }))
}

#[tauri::command]
fn fav_add(port: i32) -> bool {
    unsafe { pl_fav_add(port) == 1 }
}

#[tauri::command]
fn fav_remove(port: i32) -> bool {
    unsafe { pl_fav_remove(port) == 1 }
}

#[tauri::command]
fn history() -> Value {
    as_json(take(unsafe { pl_history() }))
}

#[tauri::command]
fn history_clear() -> bool {
    unsafe { pl_history_clear() == 1 }
}

#[tauri::command]
fn install_path() -> bool {
    unsafe { pl_install_path() == 1 }
}

#[tauri::command]
fn app_version() -> String {
    env!("CARGO_PKG_VERSION").to_string()
}

#[tauri::command]
fn quit_app(app: tauri::AppHandle) {
    app.exit(0);
}

fn main() {
    let args: Vec<String> = std::env::args().collect();

    let force_cli = args.iter().any(|a| a == "--cli");
    let force_gui = args.iter().any(|a| a == "--gui");

    // 有参数 → CLI；无参数 → 双击判定
    let gui_mode = if force_gui {
        true
    } else if force_cli || args.len() > 1 {
        false
    } else {
        unsafe { pl_should_gui() == 1 }
    };

    if !gui_mode {
        let cli_args: Vec<String> = args
            .iter()
            .filter(|a| a.as_str() != "--cli" && a.as_str() != "--gui")
            .cloned()
            .collect();
        let code = run_cli(&cli_args);
        std::process::exit(code);
    }

    // GUI 模式：脱离控制台，初始化内核（WebView2Loader 已静态链接，无需 DLL）
    unsafe {
        pl_free_console();
        pl_init();
    }

    // 首次启动自动安装 port 命令与 port: 协议（幂等，失败无影响）
    std::thread::spawn(|| unsafe {
        pl_install_path();
    });

    tauri::Builder::default()
        .invoke_handler(tauri::generate_handler![
            list_ports,
            check_port,
            scan_ports,
            process_detail,
            find_process,
            port_connections,
            kill_pid,
            find_free_port,
            export_ports,
            config_info,
            favorites,
            fav_add,
            fav_remove,
            history,
            history_clear,
            install_path,
            app_version,
            quit_app
        ])
        .setup(|app| {
            let show = MenuItemBuilder::with_id("show", "显示主窗口").build(app)?;
            let quit = MenuItemBuilder::with_id("quit", "退出 PortLens").build(app)?;
            let menu = MenuBuilder::new(app).items(&[&show, &quit]).build()?;

            let _tray = TrayIconBuilder::with_id("main-tray")
                .tooltip("PortLens - 端口监控")
                .menu(&menu)
                .show_menu_on_left_click(false)
                .on_menu_event(|app, event| match event.id.as_ref() {
                    "show" => {
                        if let Some(w) = app.get_webview_window("main") {
                            let _ = w.show();
                            let _ = w.unminimize();
                            let _ = w.set_focus();
                        }
                    }
                    "quit" => app.exit(0),
                    _ => {}
                })
                .on_tray_icon_event(|tray, event| {
                    if let TrayIconEvent::Click {
                        button: MouseButton::Left,
                        button_state: MouseButtonState::Up,
                        ..
                    } = event
                    {
                        let app = tray.app_handle();
                        if let Some(w) = app.get_webview_window("main") {
                            let _ = w.show();
                            let _ = w.unminimize();
                            let _ = w.set_focus();
                        }
                    }
                })
                .build(app)?;

            Ok(())
        })
        .on_window_event(|window, event| {
            if let WindowEvent::CloseRequested { api, .. } = event {
                // 关闭窗口最小化到托盘，从托盘菜单退出
                api.prevent_close();
                let _ = window.hide();
            }
        })
        .run(tauri::generate_context!())
        .expect("PortLens GUI 启动失败");

    unsafe { pl_cleanup() };
}
