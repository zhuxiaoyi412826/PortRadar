#pragma once
// C++ 内核 → Rust GUI 的桥接接口
// 所有返回字符串的函数均为 UTF-8 JSON，使用后必须调用 pl_free_string 释放

#ifdef __cplusplus
extern "C" {
#endif

// 初始化 / 清理 Winsock 与检测模块
int   pl_init(void);
void  pl_cleanup(void);

// 启动模式判定：1 = GUI（双击），0 = CLI（CMD / 参数启动）
int   pl_should_gui(void);
// 释放当前控制台（GUI 模式隐藏黑窗）
void  pl_free_console(void);

// proto: 0=TCP 1=UDP 2=全部
char* pl_list_ports(int proto);
char* pl_check_port(int port);
char* pl_scan_ports(int startPort, int endPort);
char* pl_process_detail(unsigned int pid);
// portFilter 为 0 表示不过滤
char* pl_find_process(const char* name, int portFilter);
char* pl_port_connections(int port);

int   pl_kill_pid(unsigned int pid);
int   pl_find_free_port(int startPort);

// fmt: 0=CSV 1=JSON；path 为 UTF-8 路径
int   pl_export(const char* path, int proto, int fmt);

char* pl_config_info(void);
char* pl_favorites(void);
int   pl_fav_add(int port);
int   pl_fav_remove(int port);

char* pl_history(void);
int   pl_history_clear(void);

int   pl_install_path(void);

void  pl_free_string(char* s);

#ifdef __cplusplus
}
#endif
