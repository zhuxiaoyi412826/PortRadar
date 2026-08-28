# 贡献指南

感谢关注 PortLens！欢迎通过 Issue 反馈问题、通过 Pull Request 参与开发。

## 开发环境

- Windows 10 及以上
- MinGW-w64 GCC（需支持 C++17），推荐 [WinLibs](https://winlibs.com/) 或 MSYS2
- 无需其他依赖

## 本地构建

双击 `build.bat`，或手动执行：

```bash
windres version.rc -O coff -o version.res
g++ -Os -s -std=c++17 -static -static-libgcc -static-libstdc++ \
    main.cpp port_checker.cpp process_manager.cpp version.res \
    -o port_checker.exe \
    -lws2_32 -liphlpapi -lpsapi -lversion -lshell32 -mconsole
```

## 测试

提交前请运行冒烟测试并确保全部通过：

```powershell
.\tests\run_tests.ps1
```

CI 会在每次 push / PR 时自动执行同样的构建与测试。

## 提交规范

- Commit message 用中文，格式：`类型: 描述`，例如 `新增: 导出报告功能`、`修复: 端口扫描越界`
- 一个 PR 只做一件事，新功能请附带测试

## 代码风格

- 4 空格缩进
- 函数名 camelCase，功能函数之间用 `// ========== 分组注释 ==========` 分隔
- 界面文案保持中文，输出统一 UTF-8（代码页 65001）
- 新增交互式功能时：功能函数 + 主菜单项 + README/CHANGELOG 同步更新

## 目录结构

```
├── main.cpp               # 主程序：菜单、界面、CLI、9 大功能
├── port_checker.h/.cpp    # 端口检测：TCP/UDP 表、扫描、连接查询
├── process_manager.h/.cpp # 进程管理：进程信息、文件路径、结束进程
├── console_util.h         # 控制台工具：颜色、清屏、格式化输出
├── version.rc             # exe 版本信息资源
├── build.bat              # 一键编译脚本
└── tests/run_tests.ps1    # 冒烟测试
```
