# PortLens

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-Windows-0078D4.svg)
![Size](https://img.shields.io/badge/size-~1MB-brightgreen.svg)
![C++](https://img.shields.io/badge/C%2B%2B-17-00599C.svg)

[English](README_EN.md) | **简体中文**

轻量级 Windows 端口占用检测工具。输入端口号即可查看是否被占用、占用它的程序是谁、文件在哪个目录。C++ 原生开发，静态链接单文件约 1 MB、零依赖、免安装，是开发调试与运维排障的得力助手。

## 演示

**交互式菜单**（双击运行，无参数启动）：

```text
    +-------------------------------------------------------+
    |                                                       |
    |    PortLens v0.3.0 - 端口占用检测工具                 |
    |                                                       |
    +-------------------------------------------------------+

 -------------------------------------------------------------
  主菜单
 -------------------------------------------------------------
  [1] 单端口检测
  [2] 全部端口列表
  [3] 端口扫描（范围）
  [4] 杀死进程（释放端口）
  [5] 查看进程详情
  [6] 查看端口连接详情
  [7] 进程反查端口（输入进程名查占用）
  [8] 实时监控（端口变化高亮提醒）
  [9] 自动找可用端口（被占自动试下一个）
  [0] 退出程序
 -------------------------------------------------------------
  请选择功能 (输入数字):
```

**命令行模式**（带参数启动，执行完退出，可写脚本）：

```text
> port -c 5000
端口 5000:
  [TCP] 被占用  node.exe (PID: 21624)
        路径:   D:\software\node\node-v24.16.0-win-x64\node.exe
  [UDP] 空闲

> port -a 5000
可用端口: 5001 (从 5000 起第 2 个)
```

## 安装 port 命令

**双击运行一次 exe 即可**：首次启动会自动把程序所在目录加入用户 PATH（无需管理员权限），之后新开一个 CMD 窗口，在任意目录输入 `port` 就能使用。

也可手动管理：

```bat
port --install-path      :: 加入用户 PATH
port --uninstall-path    :: 从用户 PATH 移除
```

说明：已打开的 CMD 窗口不会感知变化，请新开窗口验证；修改的是当前用户的 PATH，不影响系统和其他账户。

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

## 下载安装

- 从 [Releases](../../releases) 页面下载 `port.exe`，双击即可运行（Windows 7 及以上，无需安装任何运行库）；双击一次即自动安装 `port` 命令，详见上文"安装 port 命令"
- 或自行编译，见下文

## 使用说明

### 命令行模式

| 参数 | 说明 |
|------|------|
| `-c <端口>` | 检测单个端口（TCP+UDP） |
| `-l [协议]` | 列出所有监听端口（`tcp` / `udp` / `all`，默认 `all`） |
| `-f <进程名>` | 按进程名反查端口（部分匹配，不区分大小写） |
| `-s <起> <止>` | 扫描端口范围 |
| `-a <端口>` | 从指定端口开始自动找可用端口 |
| `--install-path` | 将程序目录加入用户 PATH（`port` 命令） |
| `--uninstall-path` | 从用户 PATH 移除程序目录 |
| `-v, --version` | 显示版本 |
| `-h, --help` | 显示帮助 |

退出码（可脚本化判断）：`0` = 端口空闲/成功，`1` = 端口被占用/无结果，`2` = 参数或运行错误。

```bat
:: 检测端口被谁占用
port -c 3306

:: 找一个 8000 起步的可用端口，直接用于启动参数
port -a 8000

:: 查看哪些端口被 node 占用
port -f node

:: 扫描 1-1000 并导出结果
port -s 1 1000 > scan_result.txt
```

### 交互式模式

| 按键 | 作用 |
|------|------|
| `1` ~ `9` | 主菜单选择功能 |
| `0` | 退出程序 |
| `Enter` / `空格` | 功能执行完毕后返回主菜单 |
| `y` / `n` | 危险操作确认（如杀死进程） |
| `ESC` / `Q` | 停止实时监控 |

输入端口号、PID 时直接按回车即可使用方括号里的默认值，支持退格修改。

## 环境要求

- **运行**：Windows 7 及以上，无需安装任何运行库
- **编译**：MinGW-w64 GCC（需支持 C++17），如 [WinLibs](https://winlibs.com/) 或 MSYS2

## 编译

### 一键编译

双击 `build.bat`，或命令行运行：

```bat
build.bat
```

脚本会自动查找 PATH 中的 g++；找不到时使用脚本顶部的 `GCC_PATH`（按需修改为你本机的 MinGW-w64 路径）。编译完成后自动运行程序。

### 手动编译

```bash
windres version.rc -O coff -o version.res
g++ -Os -s -std=c++17 -static -static-libgcc -static-libstdc++ \
    main.cpp port_checker.cpp process_manager.cpp version.res \
    -o port.exe \
    -lws2_32 -liphlpapi -lpsapi -lversion -lshell32 -mconsole
```

`-static` 静态链接生成单文件 exe，拷到任何 Windows 机器上都能直接运行。

## 测试

```powershell
.\tests\run_tests.ps1
```

冒烟测试覆盖版本输出、各命令行参数、PATH 安装/卸载、退出码约定（16 项断言）。CI 会在每次 push / PR 时自动执行构建与测试；推送 `v*` 标签会自动编译并把 exe 发布到 Release。

## 项目结构

```
├── main.cpp               # 主程序：菜单、界面、输入处理、命令行模式、9 大功能
├── port_checker.h/.cpp    # 端口检测：TCP/UDP 表、扫描、连接查询
├── process_manager.h/.cpp # 进程管理：进程信息、文件路径、结束进程
├── console_util.h         # 控制台工具：颜色、清屏、格式化输出
├── version.rc             # exe 版本信息资源
├── build.bat              # 一键编译脚本
├── tests/run_tests.ps1    # 冒烟测试
└── .github/workflows/     # CI 自动构建与发布
```

## 技术要点

- 全部基于 Windows 原生 API：`GetExtendedTcpTable` / `GetExtendedUdpTable`（端口表）、`psapi`（进程信息）、`ShellExecuteW`（定位文件）
- 按键输入统一走 `_getch()` 直读，无行缓冲残留问题，响应可靠
- UTF-8 输出（65001 代码页）+ Win32 控制台颜色 API，无第三方依赖

## 参与贡献

欢迎提交 Issue 与 Pull Request，详见 [贡献指南](CONTRIBUTING.md)。

## 更新日志

见 [CHANGELOG.md](CHANGELOG.md)。

## License

[MIT](LICENSE)
