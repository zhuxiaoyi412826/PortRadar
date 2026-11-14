# PortLens

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-Windows-0078D4.svg)
![Size](https://img.shields.io/badge/size-~1MB-brightgreen.svg)
![C++](https://img.shields.io/badge/C%2B%2B-17-00599C.svg)

[English](README_EN.md) | **简体中文**

轻量级 Windows 端口占用检测工具。输入端口号即可查看是否被占用、占用它的程序是谁、文件在哪个目录。C++ 原生开发，静态链接单文件约 1 MB、零依赖、免安装，是开发调试与运维排障的得力助手。

## 演示

**`port:端口号` 直达检测**（Win+R 运行窗口、浏览器地址栏直接输入，弹出检测窗口）：

```text
port:8001

    +-------------------------------------------------------+
    |    PortLens v0.7.0 - 端口占用检测工具                 |
    +-------------------------------------------------------+

  正在检测端口 8001 ...

  [TCP] 端口 8001 被占用
    进程名: python.exe
    PID:    28712
    路径:   C:\...\Python313\python.exe

  按 [K] 一键结束占用进程 (释放端口)，按 回车/空格 退出
```

**`port 进程名` 进程批量管理**（CMD 输入 `port python`，列出所有相关进程及其端口，按编号或 K 批量结束）：

```text
> port python

  进程名包含 "python" 的端口占用 (共 2 个进程, 2 个监听端口):

  [1] python.exe  (PID: 21600)
      路径: C:\...\Python313\python.exe
      端口: TCP:54333

  [2] python.exe  (PID: 1084)
      路径: C:\...\Python313\python.exe
      端口: TCP:54334

  按 [K] 结束全部进程，按 [编号] 结束对应进程，按 回车/空格 退出
```

**交互式菜单**（双击运行，无参数启动）：

```text
    +-------------------------------------------------------+
    |                                                       |
    |    PortLens v0.7.0 - 端口占用检测工具                 |
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

  提示: 直接输入端口号 (2 位以上) 可快速检测，如 8001
        Win+R 输入 port:端口号 可在任意界面直达检测，如 port:8001
 -------------------------------------------------------------
  请选择功能 (输入数字或直接输入端口号):
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

## 安装 port 命令与 port: 协议

**双击运行一次 exe 即可**：首次启动会自动把程序所在目录加入用户 PATH（无需管理员权限），并注册 `port:` URI 协议。之后：

- 新开一个 CMD 窗口，任意目录输入 `port` 就能使用
- **Win+R 运行窗口、浏览器地址栏、开始菜单搜索直接输入 `port:8001`**，立即弹出检测窗口——显示占用进程名、PID、文件路径，按 `K` 一键释放端口，无需先打开 CMD

也可手动管理：

```bat
port --install-path      :: 加入用户 PATH + 注册 port: 协议
port --uninstall-path    :: 从用户 PATH 移除 + 注销 port: 协议
```

说明：已打开的 CMD 窗口不会感知变化，请新开窗口验证；修改的是当前用户的 PATH，不影响系统和其他账户；exe 移动位置后重新双击一次即可自动更新协议指向。

## 功能特性

| # | 功能 | 说明 |
|---|------|------|
| 1 | 单端口检测 | 同时检测 TCP/UDP，显示占用进程名、PID、文件完整路径，附带常见端口说明（如 3306=MySQL）；支持按 K 一键结束占用进程释放端口，主菜单可直接输入端口号快速检测 |
| 2 | 全部端口列表 | 列出系统所有监听端口，可按 TCP / UDP / 全部过滤 |
| 3 | 端口扫描 | 扫描一段端口范围（如 1-1000），批量找出被占用的端口 |
| 4 | 杀死进程 | 一键结束占用端口的进程，释放端口（带二次确认） |
| 5 | 进程详情 | 进程名、PID、父进程、线程数、启动时间、内存占用、文件版本信息，支持一键在资源管理器中定位文件 |
| 6 | 端口连接详情 | 显示该端口上的所有连接（LISTENING / ESTABLISHED / TIME_WAIT 等）及远程地址 |
| 7 | 进程反查端口 | 输入进程名（支持部分匹配），列出该进程占用的所有端口 |
| 8 | 实时监控 | 按设定间隔自动刷新端口状态，被占用 / 释放 / 进程变化时高亮提醒并响铃 |
| 9 | 自动找可用端口 | 从首选端口开始依次尝试（如 5400→5401→5402），找到第一个可用端口 |
| 10 | 定向端口监控 | `port -w 8001`：占用 / 空闲状态变化时提醒（含进程名、PID、路径、响铃），`--popup` 弹桌面通知 |
| 11 | 批量释放 | `port -k 8001,8002,8003` 一条命令释放多个端口，适合 CI 清理脚本 |
| 12 | 列表导出 | `port -l --export csv` / `json`：端口快照导出留档，CSV 带 UTF-8 BOM，Excel 打开不乱码 |
| 13 | 配置文件与别名 | `port.conf` 自定义默认扫描范围、轮询间隔、历史条数、端口别名（`port mysql` ≡ `port 3306`）与端口说明 |
| 14 | 操作历史 | 自动记录最近 N 次结束进程操作（时间 / PID / 进程 / 端口 / 路径），`--history` 查看，误杀可追溯 |
| 15 | 收藏夹 | `port fav add 8001`，`port fav` 一键查看常用端口状态 |
| 16 | 托盘常驻 | `port --tray` 最小化到系统托盘，端口被占时气泡提醒，右键菜单可显示控制台 / 刷新 / 退出 |
| 17 | Docker / WSL 感知 | 标注端口被哪个 Docker 容器、WSL 或 Hyper-V 占用（如 `[Docker: redis]`） |
| 18 | 进程列表过滤排序 | `port java 8080` 进程名 + 端口双重过滤，`--sort pid\|port\|name` 排序 |
| 19 | 桌面端 GUI | 双击 exe 打开图形界面：端口地图、端口列表、检测、监控时间轴、收藏夹、历史、设置七个页面，覆盖命令行全部能力 |
| 20 | 端口地图 | 以进程为中心绘制「端口 → 进程 → 容器环境」连线图，一眼看清谁占着哪些端口 |

## 下载安装

- 从 [Releases](../../releases) 页面下载 `port.exe`，双击即可运行（Windows 7 及以上，无需安装任何运行库）；双击一次即自动安装 `port` 命令，详见上文"安装 port 命令"
- 或自行编译，见下文

## 使用说明

### 双模式：一个 exe，两种用法

同一个 `port.exe` 会根据启动方式自动选择模式，命令行与图形界面共用同一套 C++ 内核：

| 启动方式 | 模式 |
|----------|------|
| 双击 exe / 资源管理器打开 | 图形界面（GUI） |
| CMD / PowerShell / Windows Terminal 中执行 `port` | 命令行（交互式菜单） |
| `port 8001`、`port java`、`port -l` 等带参数 | 命令行（执行完退出） |
| `port --gui` | 强制图形界面 |
| `port --cli` | 强制命令行 |

图形界面包含七个页面：**端口地图**（端口 → 进程 → 容器连线图 + 统计）、**端口列表**（排序 / 搜索 / 勾选批量释放 / 导出 CSV、JSON）、**端口检测**（TCP+UDP 状态、进程详情、连接列表、别名快捷检测、查找可用端口）、**监控时间轴**（端口占用/释放事件记录 + 闪烁与提示音）、**收藏夹**、**历史记录**、**设置**。关闭窗口会最小化到系统托盘，托盘菜单可重新显示或退出。

> GUI 版本依赖系统 WebView2 运行时（Windows 10/11 自带）；v0.8.1 起 `WebView2Loader` 已静态链接进 exe，单文件即可运行，无需附带 DLL。

### 命令行模式

| 参数 | 说明 |
|------|------|
| `port <端口>` | CMD 直达检测（如 `port 8001`），显示占用进程名、PID、路径，支持 K 键一键释放 |
| `port <进程名>` | CMD 直达管理（如 `port java`）：列出进程名匹配的所有进程及其监听端口（按进程编号），`K` 结束全部、编号结束对应进程、删除后自动刷新列表 |
| `port <进程名> <端口>` | 进程名 + 端口双重过滤（如 `port java 8080`） |
| `port <别名>` | 端口别名检测（如 `port mysql` ≡ `port 3306`，别名在 `port.conf` 配置） |
| `port :<端口>` | 同上，冒号写法（如 `port :8001`） |
| `port:<端口>` | URI 协议形式：Win+R / 浏览器地址栏 / 开始菜单搜索直接输 `port:8001` 弹出检测窗口。注意 CMD 中无法执行此写法（Windows 判定语法错误），CMD 里请用 `port 8001` |
| `-c <端口>` | 检测单个端口（TCP+UDP） |
| `-l [协议]` | 列出所有监听端口（`tcp` / `udp` / `all`，默认 `all`） |
| `-l --export csv\|json[:文件]` | 端口列表导出为 CSV / JSON（不指定文件时自动按时间命名） |
| `-s [起 止]` | 扫描端口范围（省略范围时使用 `port.conf` 中的默认值） |
| `-a <端口>` | 从指定端口开始自动找可用端口 |
| `-w <端口> [--interval 毫秒] [--popup]` | 定向监控端口，占用 / 空闲变化时提醒，Ctrl+C 退出 |
| `-k <端口,...>` | 批量释放端口，如 `-k 8001,8002,8003` |
| `--history` | 查看最近结束的进程记录（时间 / PID / 进程 / 端口 / 路径） |
| `--clear-history` | 清空历史记录 |
| `fav [list\|add <端口>\|remove <端口>]` | 端口收藏夹，如 `fav add 8001` |
| `--tray [端口...]` | 系统托盘常驻监控（不填端口时监控收藏夹） |
| `--config` | 查看配置项与路径，缺失时自动生成 `port.conf` 模板 |
| `-f <进程名>` | 按进程名反查端口（部分匹配，不区分大小写） |
| `--install-path` | 将程序目录加入用户 PATH（`port` 命令） |
| `--uninstall-path` | 从用户 PATH 移除程序目录 |
| `-v, --version` | 显示版本 |
| `-h, --help` | 显示帮助 |

退出码（可脚本化判断）：`0` = 端口空闲/成功，`1` = 端口被占用/无结果，`2` = 参数或运行错误。

```bat
:: CMD 中直接查端口（被占用时显示进程详情，可按 K 释放）
port 8001

:: 列出所有 java 相关进程及其端口，按编号或 K 批量结束
port java

:: 检测端口被谁占用
port -c 3306

:: 找一个 8000 起步的可用端口，直接用于启动参数
port -a 8000

:: 查看哪些端口被 node 占用
port -f node

:: 扫描 1-1000 并导出结果
port -s 1 1000 > scan_result.txt

:: 只列 java 在 8080 上的端口，按 PID 排序
port java 8080 --sort pid

:: 等 8001 释放：占用 / 空闲变化时提醒（Ctrl+C 退出）
port -w 8001

:: 批量释放 CI 用到的端口
port -k 8001,8002,8003 > nul

:: 导出端口快照留档（CSV 带 BOM，Excel 直接打开）
port -l --export csv:ports.csv
port -l tcp --export json

:: 收藏常用端口，一键看状态
port fav add 3306
port fav

:: 托盘常驻，端口被占时气泡提醒（不填端口则监控收藏夹）
port --tray
```

### 交互式模式

| 按键 | 作用 |
|------|------|
| `1` ~ `9` | 主菜单选择功能 |
| `0` | 退出程序 |
| 直接输入端口号 | 主菜单处输入 2 位以上数字（如 `8001`）直接检测该端口，无需先进菜单 [1] |
| `K` | 单端口检测结果后，一键结束占用进程释放端口 |
| `Enter` / `空格` | 功能执行完毕后返回主菜单 |
| `y` / `n` | 危险操作确认（如杀死进程） |
| `ESC` / `Q` | 停止实时监控 |
| `F` | 端口收藏夹（查看收藏端口状态） |
| `H` | 操作历史（最近结束的进程记录） |
| `W` | 定向监控某个端口 |
| `T` | 托盘常驻监控 |

输入端口号、PID 时直接按回车即可使用方括号里的默认值，支持退格修改。

## 配置文件 port.conf

运行 `port --config` 可查看当前配置与路径；文件不存在时会自动生成模板。查找顺序：exe 同目录 `port.conf` → `%LOCALAPPDATA%\PortLens\port.conf`。

```ini
[settings]
; 默认扫描范围（port -s 不带参数时使用）
scan_start=1
scan_end=1024
; 历史记录条数上限
history_limit=50
; 监控 / 托盘轮询间隔（毫秒）
watch_interval=1000

[alias]
; port mysql 等价于 port 3306
mysql=3306
redis=6379
mongo=27017
postgres=5432
nginx=80

[desc]
; 自定义端口说明（覆盖内置说明）
8001=本地测试服务
```

未提供配置文件时，内置 mysql / redis / mongo / postgres / nginx 五个常用别名，开箱即用。

收藏夹与操作历史保存在 `%LOCALAPPDATA%\PortLens\`（`favorites.txt`、`history.tsv`）。

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
    main.cpp port_checker.cpp process_manager.cpp config_manager.cpp \
    special_process.cpp watch_tray.cpp version.res \
    -o port.exe \
    -lws2_32 -liphlpapi -lpsapi -lversion -lshell32 -luser32 -lgdi32 -mconsole
```

`-static` 静态链接生成单文件 exe，拷到任何 Windows 机器上都能直接运行。

### 图形界面编译

需要 Rust 工具链（`x86_64-pc-windows-gnu`）与 Node（可选，仅当前端资源独立构建时需要，本项目前端为原生 HTML/CSS/JS，直接嵌入 exe）：

```powershell
.\build_gui.ps1          # 输出单文件 dist\port.exe
.\build_gui.ps1 -Clean   # 先清理再编译
```

脚本会自动用 `subst` 映射无空格盘符（MinGW 的 `windres` 不支持含空格路径），编译完成后把单文件 `port.exe` 收集到 `dist\`（`WebView2Loader` 已静态链接，无需 DLL）。

## 测试

```powershell
.\tests\run_tests.ps1
```

冒烟测试覆盖版本输出、各命令行参数、`port:` 形式、进程名直达、PATH / 协议安装卸载、退出码约定，以及 v0.7 的配置、收藏夹、导出、批量释放、历史、托盘等新命令（38 项断言）。CI 会在每次 push / PR 时自动执行构建与测试；推送 `v*` 标签会自动编译并把 exe 发布到 Release。

## 项目结构

```
├── main.cpp               # 主程序：菜单、界面、输入处理、命令行模式
├── port_checker.h/.cpp    # 端口检测：TCP/UDP 表、扫描、连接查询
├── process_manager.h/.cpp # 进程管理：进程信息、文件路径、结束进程
├── config_manager.h/.cpp  # 配置（port.conf）、操作历史、收藏夹
├── special_process.h/.cpp # Docker / WSL / Hyper-V 归属识别
├── watch_tray.h/.cpp      # 定向端口监控、系统托盘常驻
├── gui_api.h/.cpp         # GUI 桥接层：以 JSON 形式向 Rust 暴露内核能力
├── gui/                   # 桌面端（Tauri 2）
│   ├── src/               # 前端：index.html / styles.css / app.js
│   └── src-tauri/         # Rust 入口（双模式判定、命令、托盘）与构建脚本
├── build_gui.ps1          # 图形界面一键编译（输出 dist/）
├── console_util.h         # 控制台工具：颜色、清屏、格式化输出
├── version.rc             # exe 版本信息资源
├── build.bat              # 一键编译脚本（命令行版）
├── tests/run_tests.ps1    # 冒烟测试
└── .github/workflows/     # CI 自动构建与发布
```

## 技术要点

- 全部基于 Windows 原生 API：`GetExtendedTcpTable` / `GetExtendedUdpTable`（端口表）、`psapi`（进程信息）、`ShellExecuteW`（定位文件）
- 桌面端使用 Tauri 2（系统 WebView2，不使用 Qt / Electron），Rust 入口通过 `gui_api.cpp` 的 JSON 接口复用 C++ 内核，命令行与界面零逻辑重复
- 双模式判定：无参数时看父进程（`explorer.exe` 即双击）与所在控制台进程数，GUI 模式下 `FreeConsole()` 隐藏黑窗；仍保留 console 子系统，保证 CMD 中输出与退出码正确
- 按键输入统一走 `_getch()` 直读，无行缓冲残留问题，响应可靠
- UTF-8 输出（65001 代码页）+ Win32 控制台颜色 API，无第三方依赖

## 参与贡献

欢迎提交 Issue 与 Pull Request，详见 [贡献指南](CONTRIBUTING.md)。

## 更新日志

见 [CHANGELOG.md](CHANGELOG.md)。

## License

[MIT](LICENSE)
