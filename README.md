# PortLens

轻量级 Windows 端口占用检测工具（C++ 命令行版）。

输入端口号即可查看是否被占用、占用它的程序是谁、文件在哪个目录。C++ 原生开发，静态链接单文件约 1 MB、零依赖、免安装，是开发调试与运维排障的得力助手。

## 功能特性

| # | 功能 | 说明 |
|---|------|------|
| 1 | 单端口检测 | 同时检测 TCP/UDP，显示占用进程名、PID、文件完整路径，附带常见端口说明（如 3306=MySQL） |
| 2 | 全部端口列表 | 列出系统所有监听端口，可按 TCP / UDP / 全部过滤 |
| 3 | 端口扫描 | 扫描一段端口范围（如 1-1000），批量找出被占用的端口 |
| 4 | 杀死进程 | 一键结束占用端口的进程，释放端口（带二次确认） |
| 5 | 进程详情 | 进程名、PID、父进程、线程数、启动时间、内存占用、文件版本信息，支持一键在资源管理器中定位文件 |
| 6 | 端口连接详情 | 显示该端口上的所有连接（LISTENING / ESTABLISHED / TIME_WAIT 等）及远程地址 |
| 7 | 进程反查端口 | 输入进程名（支持部分匹配），列出该进程占用的所有端口 |
| 8 | 实时监控 | 按设定间隔自动刷新端口状态，被占用 / 释放 / 进程变化时高亮提醒并响铃 |
| 9 | 自动找可用端口 | 从首选端口开始依次尝试（如 5400→5401→5402），找到第一个可用端口 |

## 环境要求

- **运行**：Windows 7 及以上，无需安装任何运行库
- **编译**：MinGW-w64 GCC（需支持 C++17），如 [WinLibs](https://winlibs.com/)

## 快速开始

### 一键编译

双击 `build.bat`，或命令行运行：

```bat
build.bat
```

脚本会自动查找 PATH 中的 g++；找不到时使用脚本顶部的 `GCC_PATH`（按需修改为你本机的 MinGW-w64 路径）。编译完成后自动运行程序。

### 手动编译

```bash
g++ -Os -s -std=c++17 -static -static-libgcc -static-libstdc++ \
    main.cpp port_checker.cpp process_manager.cpp \
    -o port_checker.exe \
    -lws2_32 -liphlpapi -lpsapi -lversion -lshell32 -mconsole
```

`-static` 静态链接生成单文件 exe，拷到任何 Windows 机器上都能直接运行。

## 使用说明

| 按键 | 作用 |
|------|------|
| `1` ~ `9` | 主菜单选择功能 |
| `0` | 退出程序 |
| `Enter` / `空格` | 功能执行完毕后返回主菜单 |
| `y` / `n` | 危险操作确认（如杀死进程） |
| `ESC` / `Q` | 停止实时监控 |

输入端口号、PID 时直接按回车即可使用方括号里的默认值，支持退格修改。

## 项目结构

```
├── main.cpp               # 主程序：菜单、界面、输入处理、9 大功能
├── port_checker.h/.cpp    # 端口检测：TCP/UDP 表、扫描、连接查询
├── process_manager.h/.cpp # 进程管理：进程信息、文件路径、结束进程
├── console_util.h         # 控制台工具：颜色、清屏、格式化输出
└── build.bat              # 一键编译脚本
```

## 技术要点

- 全部基于 Windows 原生 API：`GetExtendedTcpTable` / `GetExtendedUdpTable`（端口表）、`psapi`（进程信息）、`ShellExecuteW`（定位文件）
- 按键输入统一走 `_getch()` 直读，无行缓冲残留问题，响应可靠
- UTF-8 输出（65001 代码页）+ Win32 控制台颜色 API，无第三方依赖

## License

MIT
