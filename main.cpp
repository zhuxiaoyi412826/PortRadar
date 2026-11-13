#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cwctype>
#include <utility>
#include <windows.h>
#include <shellapi.h>
#include <conio.h>
#include <stdlib.h>
#include "console_util.h"
#include "port_checker.h"
#include "process_manager.h"
#include "config_manager.h"
#include "special_process.h"
#include "watch_tray.h"

// 版本号（同步更新 version.rc）
#define PORTLENS_VERSION "0.8.0"

// ========== 输入工具函数（全部用 _getch，避免 cin 缓冲问题） ==========

static bool readInt(const char* prompt, int defaultValue, int minVal, int maxVal, int& result) {
    std::cout << "  " << prompt << " [默认 " << defaultValue << "]: ";
    std::cout.flush();
    
    char buf[32] = {0};
    int i = 0;
    
    while (i < 30) {
        int ch = _getch();
        if (ch == '\r') {
            std::cout << std::endl;
            break;
        }
        if (ch == '\b' && i > 0) {
            i--;
            buf[i] = 0;
            std::cout << "\b \b";
            continue;
        }
        if (ch >= '0' && ch <= '9') {
            buf[i++] = (char)ch;
            std::cout << (char)ch;
        }
    }
    
    if (i == 0) {
        result = defaultValue;
        return false;
    }
    
    int val = atoi(buf);
    if (val < minVal || val > maxVal) {
        printError("输入无效 (范围: " + std::to_string(minVal) + "-" + std::to_string(maxVal) + ")");
        return false;
    }
    
    result = val;
    return true;
}

static bool readYesNo(const char* prompt) {
    std::cout << "  " << prompt << " (y/N): ";
    std::cout.flush();
    
    while (true) {
        int ch = _getch();
        if (ch == 'y' || ch == 'Y') {
            std::cout << (char)ch << std::endl;
            return true;
        }
        if (ch == 'n' || ch == 'N' || ch == '\r' || ch == '\n') {
            std::cout << "n" << std::endl;
            return false;
        }
    }
}

// 读取字符串输入（字母、数字、常见符号，用于进程名等）
static std::string readString(const char* prompt, int maxLen = 64) {
    std::cout << "  " << prompt << ": ";
    std::cout.flush();
    
    std::string result;
    while ((int)result.size() < maxLen) {
        int ch = _getch();
        if (ch == '\r') {
            std::cout << std::endl;
            break;
        }
        if (ch == '\b' && !result.empty()) {
            result.pop_back();
            std::cout << "\b \b";
            continue;
        }
        if (ch >= 32 && ch < 127) {
            result += (char)ch;
            std::cout << (char)ch;
        }
    }
    return result;
}

// 转小写（不区分大小写匹配用）
static std::string toLowerStr(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

// 是否系统关键进程（误杀会导致系统不稳定，删除操作需跳过）
static bool isCriticalSystemProcess(const std::string& procName) {
    std::string ln = toLowerStr(procName);
    return ln.find("svchost") != std::string::npos ||
           ln.find("lsass") != std::string::npos ||
           ln.find("csrss") != std::string::npos ||
           ln.find("services") != std::string::npos ||
           ln.find("wininit") != std::string::npos ||
           ln.find("winlogon") != std::string::npos ||
           ln.find("smss") != std::string::npos ||
           ln == "system";
}

// 结束单个进程并输出结果（系统关键进程自动跳过）
// portsText: 该进程涉及的端口，用于写入历史记录
static bool killOneProcess(uint32_t pid, const std::string& procName,
                           const std::string& portsText = "") {
    if (isCriticalSystemProcess(procName)) {
        printWarning("已跳过系统关键进程 " + procName + " (PID " + std::to_string(pid) + ")，结束它可能导致系统不稳定");
        return false;
    }
    if (ProcessManager::killProcess(pid)) {
        printSuccess("已结束 " + procName + " (PID " + std::to_string(pid) + ")，其监听端口已释放");
        HistoryManager::record(pid, procName, ProcessManager::getProcessPath(pid), portsText);
        return true;
    }
    printError("结束 " + procName + " (PID " + std::to_string(pid) + ") 失败（可能权限不足）");
    return false;
}

// UTF-8 转 UTF-16
static std::wstring utf8ToWstring(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
    std::wstring ws(len - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &ws[0], len);
    return ws;
}

// 在资源管理器中打开并选中文件
static void openFileLocation(const std::string& path) {
    std::wstring wpath = utf8ToWstring(path);
    std::wstring params = L"/select,\"" + wpath + L"\"";
    HINSTANCE h = ShellExecuteW(NULL, L"open", L"explorer.exe", params.c_str(), NULL, SW_SHOWNORMAL);
    if ((INT_PTR)h <= 32) {
        printError("打开文件位置失败");
    } else {
        printSuccess("已在资源管理器中打开");
    }
}

// ========== 端口说明数据 ==========

struct PortDesc {
    int port;
    const char* name;
    const char* desc;
};

PortDesc PORT_DESCS[] = {
    {21, "FTP", "文件传输协议"},
    {22, "SSH", "安全外壳协议"},
    {23, "Telnet", "远程登录协议"},
    {25, "SMTP", "简单邮件传输协议"},
    {53, "DNS", "域名系统"},
    {80, "HTTP", "超文本传输协议"},
    {110, "POP3", "邮局协议v3"},
    {135, "RPC", "远程过程调用"},
    {139, "NetBIOS", "Windows文件共享"},
    {143, "IMAP", "互联网消息访问协议"},
    {443, "HTTPS", "安全超文本传输协议"},
    {445, "SMB", "Windows服务器消息块"},
    {465, "SMTPS", "加密SMTP"},
    {587, "SMTP-submission", "邮件提交端口"},
    {993, "IMAPS", "加密IMAP"},
    {995, "POP3S", "加密POP3"},
    {1080, "SOCKS", "SOCKS代理"},
    {1433, "MSSQL", "Microsoft SQL Server"},
    {1521, "Oracle", "Oracle数据库"},
    {3306, "MySQL", "MySQL数据库"},
    {3389, "RDP", "远程桌面协议"},
    {5432, "PostgreSQL", "PostgreSQL数据库"},
    {5900, "VNC", "VNC远程桌面"},
    {6379, "Redis", "Redis缓存"},
    {7000, "Cassandra", "Cassandra数据库"},
    {8080, "HTTP-Proxy", "HTTP代理/备用端口"},
    {8443, "HTTPS-Alt", "HTTPS备用端口"},
    {27017, "MongoDB", "MongoDB数据库"},
    {0, nullptr, nullptr}
};

std::string getPortDesc(int port) {
    // 配置文件中的自定义说明优先
    std::string custom = ConfigManager::getCustomPortDesc(port);
    if (!custom.empty()) {
        return "自定义 - " + custom;
    }
    for (int i = 0; PORT_DESCS[i].name; i++) {
        if (PORT_DESCS[i].port == port) {
            return std::string(PORT_DESCS[i].name) + " - " + PORT_DESCS[i].desc;
        }
    }
    return "";
}

// ========== 界面函数 ==========

void printHeader() {
    clearScreen();
    setColor(CYAN);
    std::cout << "\n";
    std::cout << "    +-------------------------------------------------------+\n";
    std::cout << "    |                                                       |\n";
    std::cout << "    |    PortLens v" << PORTLENS_VERSION << " - 端口占用检测工具                 |\n";
    std::cout << "    |                                                       |\n";
    std::cout << "    +-------------------------------------------------------+\n";
    std::cout << "\n";
    restoreColor();
}

void printMenu() {
    printSeparator();
    setColor(YELLOW);
    std::cout << "  主菜单\n";
    restoreColor();
    printSeparator();
    
    setColor(GREEN);
    std::cout << "  [1] ";
    restoreColor();
    std::cout << "单端口检测\n";
    
    setColor(GREEN);
    std::cout << "  [2] ";
    restoreColor();
    std::cout << "全部端口列表\n";
    
    setColor(GREEN);
    std::cout << "  [3] ";
    restoreColor();
    std::cout << "端口扫描（范围）\n";
    
    setColor(GREEN);
    std::cout << "  [4] ";
    restoreColor();
    std::cout << "杀死进程（释放端口）\n";
    
    setColor(GREEN);
    std::cout << "  [5] ";
    restoreColor();
    std::cout << "查看进程详情\n";
    
    setColor(GREEN);
    std::cout << "  [6] ";
    restoreColor();
    std::cout << "查看端口连接详情\n";
    
    setColor(GREEN);
    std::cout << "  [7] ";
    restoreColor();
    std::cout << "进程反查端口（输入进程名查占用）\n";
    
    setColor(GREEN);
    std::cout << "  [8] ";
    restoreColor();
    std::cout << "实时监控（端口变化高亮提醒）\n";
    
    setColor(GREEN);
    std::cout << "  [9] ";
    restoreColor();
    std::cout << "自动找可用端口（被占自动试下一个）\n";
    
    setColor(GREEN);
    std::cout << "  [0] ";
    restoreColor();
    std::cout << "退出程序\n";

    setColor(GREEN);
    std::cout << "  [F] ";
    restoreColor();
    std::cout << "端口收藏夹（查看收藏端口状态）\n";

    setColor(GREEN);
    std::cout << "  [H] ";
    restoreColor();
    std::cout << "结束进程历史（误杀可追溯）\n";

    setColor(GREEN);
    std::cout << "  [W] ";
    restoreColor();
    std::cout << "定向端口监控（状态变化提醒）\n";

    setColor(GREEN);
    std::cout << "  [T] ";
    restoreColor();
    std::cout << "系统托盘常驻监控\n";

    setColor(DARK_GRAY);
    std::cout << "\n  提示: 直接输入端口号 (2 位以上) 可快速检测，如 8001\n";
    std::cout << "        Win+R 输入 port:端口号 可在任意界面直达检测，如 port:8001\n";
    std::cout << "        port <进程名> 可批量管理，如 port java\n";
    restoreColor();

    printSeparator();
}

void printPortTable(const std::vector<PortInfo>& ports, bool showProcess = true) {
    if (ports.empty()) {
        printWarning("没有找到匹配的端口");
        return;
    }

    setColor(YELLOW);
    printf("  %-7s %-8s %-15s %-7s %-10s %-20s\n",
        "端口", "协议", "本地地址", "PID", "状态", "进程名");
    restoreColor();
    setColor(DARK_GRAY);
    std::cout << "  " << std::string(70, '-') << "\n";
    restoreColor();

    for (const auto& p : ports) {
        std::string procName = "未知";
        if (showProcess && p.pid > 0) {
            procName = ProcessManager::getProcessName(p.pid);
            // 标注 Docker 容器 / WSL / 虚拟机来源
            std::string tag = SpecialProcessDetector::detect(procName, p.port);
            if (!tag.empty()) procName += " " + tag;
        }

        setColor(CYAN);
        printf("  %-7d", p.port);
        restoreColor();

        if (p.protocol == "TCP") {
            setColor(GREEN);
        } else {
            setColor(MAGENTA);
        }
        printf(" %-8s", p.protocol.c_str());
        restoreColor();

        printf(" %-15s %-7d ", p.localAddr.c_str(), p.pid);
        
        if (p.state == "LISTENING") {
            setColor(GREEN);
        } else if (p.state == "ESTABLISHED") {
            setColor(CYAN);
        } else if (p.state == "TIME_WAIT") {
            setColor(DARK_GRAY);
        } else {
            setColor(YELLOW);
        }
        printf("%-10s", p.state.c_str());
        restoreColor();

        setColor(WHITE);
        printf(" %-20s", procName.c_str());
        restoreColor();
        
        std::cout << "\n";
    }

    setColor(DARK_GRAY);
    std::cout << "  " << std::string(70, '-') << "\n";
    restoreColor();
    
    setColor(CYAN);
    std::cout << "  共 " << ports.size() << " 条记录\n";
    restoreColor();
}

void pauseForKey(const char* action = "返回菜单") {
    setColor(DARK_GRAY);
    std::cout << "\n  按 回车键 或 空格键 " << action << "...";
    restoreColor();
    std::cout.flush();
    
    while (true) {
        int ch = _getch();
        if (ch == '\r' || ch == ' ' || ch == '\n') {
            break;
        }
    }
}

// ========== 功能函数 ==========

// 检测端口占用；presetPort > 0 时跳过输入直接检测（主菜单快捷入口用）
// 返回 0=端口空闲（外层需 pauseForKey），1=被占用（内部已处理按键）
int checkSinglePort(int presetPort = -1, bool fromMenu = true) {
    printTitle("单端口检测");

    int portVal;
    if (presetPort > 0) {
        portVal = presetPort;
    } else {
        readInt("请输入端口号", 5400, 1, 65535, portVal);
    }
    uint16_t port = static_cast<uint16_t>(portVal);

    std::string desc = getPortDesc(port);
    if (!desc.empty()) {
        printInfo("端口说明: " + desc);
    }

    std::cout << "\n  正在检测端口 " << port << " ...\n\n";

    PortInfo tcpInfo = PortChecker::checkPort(port, "TCP");
    PortInfo udpInfo = PortChecker::checkPort(port, "UDP");

    bool tcpOccupied = tcpInfo.pid > 0;
    bool udpOccupied = udpInfo.pid > 0;

    if (!tcpOccupied && !udpOccupied) {
        printSuccess("端口 " + std::to_string(port) + " 未被占用");
        return 0;
    }

    if (tcpOccupied) {
        setColor(RED);
        std::cout << "  [TCP] 端口 " << port << " 被占用\n";
        restoreColor();

        std::string procName = ProcessManager::getProcessName(tcpInfo.pid);
        std::string procPath = ProcessManager::getProcessPath(tcpInfo.pid);
        std::string tag = SpecialProcessDetector::detect(procName, port);

        printf("    进程名: "); setColor(YELLOW); printf("%s%s\n", procName.c_str(), tag.c_str()); restoreColor();
        printf("    PID:    "); setColor(YELLOW); printf("%d\n", tcpInfo.pid); restoreColor();
        if (!procPath.empty()) {
            printf("    路径:   "); setColor(CYAN); printf("%s\n", procPath.c_str()); restoreColor();
        }
        std::cout << "\n";
    }

    if (udpOccupied) {
        setColor(RED);
        std::cout << "  [UDP] 端口 " << port << " 被占用\n";
        restoreColor();

        std::string procName = ProcessManager::getProcessName(udpInfo.pid);
        std::string procPath = ProcessManager::getProcessPath(udpInfo.pid);
        std::string tag = SpecialProcessDetector::detect(procName, port);

        printf("    进程名: "); setColor(YELLOW); printf("%s%s\n", procName.c_str(), tag.c_str()); restoreColor();
        printf("    PID:    "); setColor(YELLOW); printf("%d\n", udpInfo.pid); restoreColor();
        if (!procPath.empty()) {
            printf("    路径:   "); setColor(CYAN); printf("%s\n", procPath.c_str()); restoreColor();
        }
        std::cout << "\n";
    }

    // 一键释放: K 结束占用进程，回车/空格 返回菜单
    // 记录 PID 与涉及端口，便于写入历史记录
    std::vector<std::pair<uint32_t, std::string>> targets;
    if (tcpOccupied) {
        targets.push_back(std::make_pair(tcpInfo.pid, "TCP:" + std::to_string(port)));
    }
    if (udpOccupied) {
        bool merged = false;
        for (auto& t : targets) {
            if (t.first == udpInfo.pid) {
                t.second += ",UDP:" + std::to_string(port);
                merged = true;
                break;
            }
        }
        if (!merged) {
            targets.push_back(std::make_pair(udpInfo.pid, "UDP:" + std::to_string(port)));
        }
    }

    setColor(YELLOW);
    std::cout << "  按 [K] 一键结束占用进程 (释放端口)，按 回车/空格 " << (fromMenu ? "返回菜单" : "退出") << "\n";
    restoreColor();
    std::cout.flush();

    while (true) {
        int ch = _getch();
        if (ch == '\r' || ch == ' ' || ch == '\n') {
            return 1;  // 直接返回主菜单
        }
        if (ch == 'k' || ch == 'K') {
            std::cout << "\n";
            for (const auto& t : targets) {
                killOneProcess(t.first, ProcessManager::getProcessName(t.first), t.second);
            }
            pauseForKey(fromMenu ? "返回菜单" : "退出");
            return 1;
        }
    }
}

void listAllPorts() {
    printTitle("全部监听端口列表");
    
    std::cout << "  请选择协议: \n";
    std::cout << "  [1] TCP + UDP (全部)\n";
    std::cout << "  [2] 仅 TCP\n";
    std::cout << "  [3] 仅 UDP\n";
    std::cout << "  请选择 [1]: ";
    std::cout.flush();
    
    int ch = _getch();
    int choice = (ch == '2') ? 2 : ((ch == '3') ? 3 : 1);
    std::cout << choice << "\n";

    std::string protocol = "ALL";
    if (choice == 2) protocol = "TCP";
    else if (choice == 3) protocol = "UDP";

    std::cout << "\n  正在获取端口列表...\n\n";

    auto ports = PortChecker::getAllListeningPorts(protocol);
    
    std::sort(ports.begin(), ports.end(), [](const PortInfo& a, const PortInfo& b) {
        if (a.port != b.port) return a.port < b.port;
        return a.protocol < b.protocol;
    });

    printPortTable(ports);
}

void scanPorts() {
    printTitle("端口范围扫描");
    
    int startPort, endPort;
    readInt("起始端口", 1, 1, 65535, startPort);
    readInt("结束端口", 1000, 1, 65535, endPort);

    if (startPort > endPort) {
        printError("起始端口不能大于结束端口");
        return;
    }

    if (endPort - startPort > 10000) {
        printWarning("扫描范围超过10000个端口，可能需要较长时间");
        if (!readYesNo("确定继续吗")) return;
    }

    std::cout << "\n  正在扫描 " << startPort << " - " << endPort << " ...\n\n";

    auto ports = PortChecker::scanPorts(static_cast<uint16_t>(startPort), static_cast<uint16_t>(endPort));
    
    std::sort(ports.begin(), ports.end(), [](const PortInfo& a, const PortInfo& b) {
        if (a.port != b.port) return a.port < b.port;
        return a.protocol < b.protocol;
    });

    printPortTable(ports);
}

void killProcessMenu() {
    printTitle("杀死进程（释放端口）");
    
    int pid;
    if (!readInt("请输入进程 PID", 0, 1, 999999, pid)) {
        printError("请输入有效的 PID");
        return;
    }

    std::string procName = ProcessManager::getProcessName(pid);
    if (procName == "未知进程") {
        printError("找不到 PID 为 " + std::to_string(pid) + " 的进程");
        return;
    }

    setColor(YELLOW);
    std::cout << "\n  ! 确定要杀死以下进程吗？\n\n";
    restoreColor();
    printf("    进程名: "); setColor(RED); printf("%s\n", procName.c_str()); restoreColor();
    printf("    PID:    "); setColor(RED); printf("%d\n", pid); restoreColor();
    
    std::string path = ProcessManager::getProcessPath(pid);
    if (!path.empty()) {
        printf("    路径:   %s\n", path.c_str());
    }
    
    std::cout << "\n";
    if (!readYesNo("此操作不可恢复！输入 y 确认")) {
        printInfo("已取消操作");
        return;
    }

    if (ProcessManager::killProcess(pid)) {
        printSuccess("进程已终止");
    } else {
        printError("杀死进程失败（可能权限不足）");
    }
}

void showProcessDetail() {
    printTitle("进程详情");
    
    int pid;
    if (!readInt("请输入进程 PID", 0, 1, 999999, pid)) {
        printError("请输入有效的 PID");
        return;
    }

    std::cout << "\n  正在获取进程信息...\n\n";

    ProcessDetail detail = ProcessManager::getProcessDetail(pid);
    if (detail.name == "未知进程" && detail.path.empty()) {
        printError("找不到该进程（可能已退出或权限不足）");
        return;
    }

    printSeparator();
    setColor(YELLOW);
    std::cout << "  进程基本信息\n";
    restoreColor();
    printSeparator();
    
    printf("  进程名:   "); setColor(CYAN); printf("%s\n", detail.name.c_str()); restoreColor();
    printf("  PID:      "); setColor(CYAN); printf("%d\n", detail.pid); restoreColor();
    printf("  父进程ID: "); setColor(CYAN); printf("%d\n", detail.parentPid); restoreColor();
    printf("  线程数:   "); setColor(CYAN); printf("%d\n", detail.threadCount); restoreColor();
    printf("  启动时间: "); setColor(CYAN); printf("%s\n", detail.startTime.c_str()); restoreColor();
    
    printSeparator();
    setColor(YELLOW);
    std::cout << "  内存使用\n";
    restoreColor();
    printSeparator();
    
    printf("  当前内存: "); setColor(GREEN); printf("%.2f MB\n", detail.memoryKb / 1024.0); restoreColor();
    printf("  峰值内存: "); setColor(YELLOW); printf("%.2f MB\n", detail.peakMemoryKb / 1024.0); restoreColor();
    
    if (!detail.path.empty()) {
        printSeparator();
        setColor(YELLOW);
        std::cout << "  文件信息\n";
        restoreColor();
        printSeparator();
        
        printf("  完整路径: "); setColor(CYAN); printf("%s\n", detail.path.c_str()); restoreColor();
        if (!detail.company.empty()) {
            printf("  公司:     "); setColor(WHITE); printf("%s\n", detail.company.c_str()); restoreColor();
        }
        if (!detail.description.empty()) {
            printf("  描述:     "); setColor(WHITE); printf("%s\n", detail.description.c_str()); restoreColor();
        }
    }
    
    printSeparator();
    
    // 打开文件位置选项
    if (!detail.path.empty()) {
        std::cout << "\n";
        if (readYesNo("是否在资源管理器中打开文件位置")) {
            openFileLocation(detail.path);
        }
    }
}

void showConnectionDetail() {
    printTitle("端口连接详情");
    
    int portVal;
    if (!readInt("请输入端口号", 80, 1, 65535, portVal)) {
        printError("请输入有效的端口号");
        return;
    }
    uint16_t port = static_cast<uint16_t>(portVal);

    std::cout << "\n  正在获取连接信息...\n\n";

    auto connections = PortChecker::getPortConnections(port);
    
    if (connections.empty()) {
        printInfo("端口 " + std::to_string(port) + " 没有活跃连接");
        return;
    }

    setColor(YELLOW);
    printf("  %-22s %-22s %-15s %-10s %-6s\n",
        "本地地址:端口", "远程地址:端口", "状态", "进程名", "PID");
    restoreColor();
    setColor(DARK_GRAY);
    std::cout << "  " << std::string(85, '-') << "\n";
    restoreColor();

    for (const auto& c : connections) {
        char local[32], remote[32];
        sprintf_s(local, "%s:%d", c.localAddr.c_str(), c.port);
        sprintf_s(remote, "%s:%d", c.remoteAddr.c_str(), c.remotePort);
        
        std::string procName = c.pid > 0 ? ProcessManager::getProcessName(c.pid) : "N/A";

        printf("  %-22s %-22s ", local, remote);
        
        if (c.state == "LISTENING") {
            setColor(GREEN);
        } else if (c.state == "ESTABLISHED") {
            setColor(CYAN);
        } else if (c.state == "TIME_WAIT") {
            setColor(DARK_GRAY);
        } else if (c.state == "CLOSE_WAIT") {
            setColor(RED);
        } else {
            setColor(YELLOW);
        }
        printf("%-15s", c.state.c_str());
        restoreColor();
        
        printf(" %-10s %-6d\n", procName.c_str(), c.pid);
    }

    setColor(DARK_GRAY);
    std::cout << "  " << std::string(85, '-') << "\n";
    restoreColor();
    
    int listenCount = 0, estabCount = 0, timeWaitCount = 0, otherCount = 0;
    for (const auto& c : connections) {
        if (c.state == "LISTENING") listenCount++;
        else if (c.state == "ESTABLISHED") estabCount++;
        else if (c.state == "TIME_WAIT") timeWaitCount++;
        else otherCount++;
    }
    
    setColor(CYAN);
    printf("  总计: %d 条连接  (LISTENING: %d, ESTABLISHED: %d, TIME_WAIT: %d, 其他: %d)\n",
        (int)connections.size(), listenCount, estabCount, timeWaitCount, otherCount);
    restoreColor();
}

// ========== 功能7: 进程反查端口 ==========

void findPortsByProcess() {
    printTitle("进程反查端口");
    
    std::string name = readString("请输入进程名（支持部分匹配，如 python）");
    if (name.empty()) {
        printError("进程名不能为空");
        return;
    }
    
    std::cout << "\n  正在查询进程包含 '" << name << "' 的端口占用...\n\n";
    
    std::string lowerQuery = toLowerStr(name);
    auto ports = PortChecker::getAllListeningPorts("ALL");
    
    std::vector<PortInfo> results;
    for (const auto& p : ports) {
        if (p.pid == 0) continue;
        std::string pname = ProcessManager::getProcessName(p.pid);
        if (toLowerStr(pname).find(lowerQuery) != std::string::npos) {
            results.push_back(p);
        }
    }
    
    if (results.empty()) {
        printInfo("没有找到进程名包含 '" + name + "' 的端口占用");
        return;
    }
    
    printPortTable(results);
}

// ========== 功能8: 实时监控 ==========

void monitorPort() {
    printTitle("实时端口监控");
    
    int portVal;
    readInt("请输入要监控的端口", 5400, 1, 65535, portVal);
    uint16_t port = static_cast<uint16_t>(portVal);
    
    int interval;
    readInt("刷新间隔(秒)", 2, 1, 60, interval);
    
    printInfo("监控中... 按 ESC 或 Q 键停止");
    Sleep(800);
    
    bool prevOccupied = false;
    uint32_t prevPid = 0;
    bool first = true;
    int refreshCount = 0;
    
    while (true) {
        PortInfo tcpInfo = PortChecker::checkPort(port, "TCP");
        PortInfo udpInfo = PortChecker::checkPort(port, "UDP");
        
        bool occupied = tcpInfo.pid > 0 || udpInfo.pid > 0;
        uint32_t curPid = tcpInfo.pid > 0 ? tcpInfo.pid : udpInfo.pid;
        
        // 状态变化检测
        std::string changeMsg;
        if (!first) {
            if (!prevOccupied && occupied) {
                changeMsg = "端口被占用!";
            } else if (prevOccupied && !occupied) {
                changeMsg = "端口已释放!";
            } else if (occupied && curPid != prevPid) {
                changeMsg = "占用进程已变化!";
            }
        }
        bool changed = !changeMsg.empty();
        
        refreshCount++;
        
        // 显示当前状态
        clearScreen();
        printTitle("实时端口监控 - 端口 " + std::to_string(port));
        
        SYSTEMTIME st;
        GetLocalTime(&st);
        setColor(DARK_GRAY);
        printf("  刷新: %d 次    时间: %02d:%02d:%02d    间隔: %d 秒    [ESC/Q 停止]\n\n",
            refreshCount, st.wHour, st.wMinute, st.wSecond, interval);
        restoreColor();
        
        if (changed) {
            setColor(YELLOW);
            std::cout << "  *** 状态变化: " << changeMsg << " ***\n\n";
            restoreColor();
            std::cout << "\a";  // 响铃提醒
        }
        
        if (tcpInfo.pid > 0) {
            setColor(RED);
            std::cout << "  [TCP] 被占用\n";
            restoreColor();
            printf("        进程: ");
            setColor(YELLOW);
            printf("%s (PID: %d)\n", ProcessManager::getProcessName(tcpInfo.pid).c_str(), tcpInfo.pid);
            restoreColor();
        } else {
            setColor(GREEN);
            std::cout << "  [TCP] 空闲\n";
            restoreColor();
        }
        
        if (udpInfo.pid > 0) {
            setColor(RED);
            std::cout << "  [UDP] 被占用\n";
            restoreColor();
            printf("        进程: ");
            setColor(YELLOW);
            printf("%s (PID: %d)\n", ProcessManager::getProcessName(udpInfo.pid).c_str(), udpInfo.pid);
            restoreColor();
        } else {
            setColor(GREEN);
            std::cout << "  [UDP] 空闲\n";
            restoreColor();
        }
        
        std::cout << "\n";
        
        prevOccupied = occupied;
        prevPid = curPid;
        first = false;
        
        // 等待间隔，期间检测退出键
        bool quit = false;
        for (int i = 0; i < interval * 10 && !quit; i++) {
            if (_kbhit()) {
                int ch = _getch();
                if (ch == 27 || ch == 'q' || ch == 'Q') quit = true;
            }
            if (!quit) Sleep(100);
        }
        if (quit) break;
    }
    
    printInfo("监控已停止");
}

// ========== 功能9: 自动找可用端口 ==========

void findAvailablePortMenu() {
    printTitle("自动查找可用端口");
    
    int preferred;
    readInt("首选端口", 5400, 1, 65535, preferred);
    
    int maxTries;
    readInt("最多尝试次数", 10, 1, 100, maxTries);
    
    std::cout << "\n";
    
    for (int i = 0; i < maxTries; i++) {
        int tryPort = preferred + i;
        if (tryPort > 65535) break;
        uint16_t port = static_cast<uint16_t>(tryPort);
        
        PortInfo tcpInfo = PortChecker::checkPort(port, "TCP");
        PortInfo udpInfo = PortChecker::checkPort(port, "UDP");
        
        if (tcpInfo.pid > 0 || udpInfo.pid > 0) {
            uint32_t pid = tcpInfo.pid > 0 ? tcpInfo.pid : udpInfo.pid;
            std::string who = ProcessManager::getProcessName(pid);
            setColor(YELLOW);
            printf("  端口 %d: 已被占用 (%s)\n", tryPort, who.c_str());
            restoreColor();
        } else {
            setColor(GREEN);
            printf("  端口 %d: 可用\n", tryPort);
            restoreColor();
            
            std::cout << "\n";
            printSuccess("推荐使用端口: " + std::to_string(tryPort));
            printInfo("共尝试 " + std::to_string(i + 1) + " 个端口");
            return;
        }
    }
    
    printError("从 " + std::to_string(preferred) + " 起连续 " + std::to_string(maxTries) + " 个端口都被占用");
}

// ========== PATH 管理（port 命令支持） ==========

// 获取 exe 所在目录（绝对路径）
static std::wstring getExeDirW() {
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(NULL, path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return L"";
    std::wstring p(path);
    size_t pos = p.find_last_of(L"\\/");
    return (pos == std::wstring::npos) ? L"" : p.substr(0, pos);
}

static std::wstring trimTrailingSlash(std::wstring s) {
    while (!s.empty() && (s.back() == L'\\' || s.back() == L'/')) s.pop_back();
    return s;
}

static bool equalsIgnoreCaseW(const std::wstring& a, const std::wstring& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++)
        if (towlower(a[i]) != towlower(b[i])) return false;
    return true;
}

static std::vector<std::wstring> splitPathEntries(const std::wstring& s) {
    std::vector<std::wstring> out;
    std::wstring cur;
    for (wchar_t c : s) {
        if (c == L';') {
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

// 读取用户 Path 原始值（保留变量形式）；不存在返回 false
static bool readUserPathRaw(std::wstring& raw, DWORD& type) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return false;

    DWORD size = 0;
    if (RegQueryValueExW(hKey, L"Path", NULL, &type, NULL, &size) != ERROR_SUCCESS || size == 0) {
        RegCloseKey(hKey);
        return false;
    }

    std::vector<wchar_t> buf(size / sizeof(wchar_t) + 1);
    if (RegQueryValueExW(hKey, L"Path", NULL, &type, (LPBYTE)buf.data(), &size) != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return false;
    }
    RegCloseKey(hKey);
    raw.assign(buf.data());
    return true;
}

// 通知系统环境变量已变化（新开的 CMD 立即可用，无需注销）
static void broadcastEnvChange() {
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
        (LPARAM)L"Environment", SMTO_ABORTIFHUNG, 5000, NULL);
}

// exe 目录是否已在用户 PATH 中（各项展开变量后比较）
static bool isExeDirInPath() {
    std::wstring exeDir = trimTrailingSlash(getExeDirW());
    if (exeDir.empty()) return false;

    std::wstring raw;
    DWORD type;
    if (!readUserPathRaw(raw, type)) return false;

    for (const auto& entry : splitPathEntries(raw)) {
        wchar_t expBuf[2048];
        DWORD m = ExpandEnvironmentStringsW(entry.c_str(), expBuf, 2048);
        std::wstring cmp = (m > 0 && m < 2048) ? std::wstring(expBuf) : entry;
        if (equalsIgnoreCaseW(trimTrailingSlash(cmp), exeDir)) return true;
    }
    return false;
}

// 将 exe 目录加入用户 PATH（已存在则跳过）。写回时保留原有变量形式
static bool installToPath() {
    std::wstring exeDir = trimTrailingSlash(getExeDirW());
    if (exeDir.empty()) return false;
    if (isExeDirInPath()) return true;

    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_READ | KEY_WRITE, &hKey) != ERROR_SUCCESS)
        return false;

    std::wstring raw;
    DWORD type;
    bool has = readUserPathRaw(raw, type);

    std::wstring newVal = raw;
    if (!newVal.empty() && newVal.back() != L';') newVal += L';';
    newVal += exeDir;

    bool ok = RegSetValueExW(hKey, L"Path", 0,
        has ? type : REG_EXPAND_SZ,
        (const BYTE*)newVal.c_str(),
        (DWORD)((newVal.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
    RegCloseKey(hKey);

    if (ok) broadcastEnvChange();
    return ok;
}

// 从用户 PATH 中移除 exe 目录
static bool removeFromPath() {
    std::wstring exeDir = trimTrailingSlash(getExeDirW());
    if (exeDir.empty()) return false;

    std::wstring raw;
    DWORD type;
    if (!readUserPathRaw(raw, type)) return false;

    std::wstring result;
    bool changed = false;
    for (const auto& entry : splitPathEntries(raw)) {
        wchar_t expBuf[2048];
        DWORD m = ExpandEnvironmentStringsW(entry.c_str(), expBuf, 2048);
        std::wstring cmp = (m > 0 && m < 2048) ? std::wstring(expBuf) : entry;
        if (equalsIgnoreCaseW(trimTrailingSlash(cmp), exeDir)) {
            changed = true;
            continue;
        }
        if (!result.empty()) result += L';';
        result += entry;
    }
    if (!changed) return false;

    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_WRITE, &hKey) != ERROR_SUCCESS)
        return false;

    bool ok = RegSetValueExW(hKey, L"Path", 0, type,
        (const BYTE*)result.c_str(),
        (DWORD)((result.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
    RegCloseKey(hKey);

    if (ok) broadcastEnvChange();
    return ok;
}

// ========== port: URI 协议支持（port:8001 直达检测） ==========

// 分类"直接检测"形式的参数:
//   0 = 不是直接检测形式（交给 runCli 处理）
//   1 = 有效端口，写入 out
//   2 = 形式正确但端口无效（非数字/超出 1-65535）
// 支持: "80" / ":80" / "port:80" / "port://80" / "PORT:80"
static int classifyDirectPort(const char* s, uint16_t& out) {
    if (!s || !*s) return 0;
    std::string str(s);
    while (!str.empty() && (str.back() == '/' || str.back() == ' ' ||
                            str.back() == '\r' || str.back() == '\n')) {
        str.pop_back();
    }
    if (str.empty()) return 0;

    size_t i = 0;
    bool directForm = false;
    if (str.size() >= 5 && toLowerStr(str.substr(0, 5)) == "port:") {
        i = 5;
        directForm = true;
    } else if (str[0] == ':') {
        i = 1;
        directForm = true;
    } else if (!isdigit((unsigned char)str[0])) {
        return 0;
    }

    while (i < str.size() && (str[i] == '/' || str[i] == ':')) i++;
    if (i >= str.size()) return directForm ? 2 : 0;

    for (size_t j = i; j < str.size(); j++) {
        if (!isdigit((unsigned char)str[j])) return directForm ? 2 : 0;
    }

    long v = strtol(str.c_str() + i, NULL, 10);
    if (v < 1 || v > 65535) return 2;
    out = static_cast<uint16_t>(v);
    return 1;
}

// 判定参数是否为进程名形式（用于 port java 直达检测）:
// 非 '-' 开头（选项）、必须含字母、只含字母数字 . _ 空格
static bool isProcessNameForm(const char* s) {
    if (!s || !*s) return false;
    if (s[0] == '-') return false;
    bool hasAlpha = false;
    for (const char* p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (isalpha(c)) {
            hasAlpha = true;
        } else if (isdigit(c) || c == '.' || c == '_' || c == ' ') {
            ;
        } else {
            return false;
        }
    }
    return hasAlpha;
}

// 注册 port: 协议到当前用户。
// 注册后 Win+R / 浏览器地址栏 / 开始菜单搜索 输入 port:8001 可直接启动检测
static bool registerPortProtocol() {
    wchar_t exe[MAX_PATH];
    if (GetModuleFileNameW(NULL, exe, MAX_PATH) == 0) return false;

    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\port", 0, NULL, 0,
        KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS)
        return false;

    bool ok = true;
    ok = ok && RegSetValueExW(hKey, NULL, 0, REG_SZ,
        (const BYTE*)L"URL:PortLens Protocol",
        (DWORD)(wcslen(L"URL:PortLens Protocol") + 1) * sizeof(wchar_t)) == ERROR_SUCCESS;
    ok = ok && RegSetValueExW(hKey, L"URL Protocol", 0, REG_SZ,
        (const BYTE*)L"", sizeof(wchar_t)) == ERROR_SUCCESS;

    HKEY hCmd;
    if (RegCreateKeyExW(hKey, L"shell\\open\\command", 0, NULL, 0,
        KEY_WRITE, NULL, &hCmd, NULL) == ERROR_SUCCESS) {
        std::wstring cmd = L"\"" + std::wstring(exe) + L"\" \"%1\"";
        ok = ok && RegSetValueExW(hCmd, NULL, 0, REG_SZ,
            (const BYTE*)cmd.c_str(),
            (DWORD)((cmd.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
        RegCloseKey(hCmd);
    } else {
        ok = false;
    }
    RegCloseKey(hKey);
    return ok;
}

// 递归删除注册表键（用于注销 port: 协议）
static void deleteRegKeyRecursive(HKEY parent, const wchar_t* name) {
    HKEY hKey;
    if (RegOpenKeyExW(parent, name, 0, KEY_READ, &hKey) != ERROR_SUCCESS) return;
    wchar_t child[256];
    while (RegEnumKeyW(hKey, 0, child, 256) == ERROR_SUCCESS) {
        deleteRegKeyRecursive(hKey, child);
    }
    RegCloseKey(hKey);
    RegDeleteKeyW(parent, name);
}

// 注销 port: 协议
static bool unregisterPortProtocol() {
    deleteRegKeyRecursive(HKEY_CURRENT_USER, L"Software\\Classes\\port");
    return true;
}

// ========== 命令行模式 ==========

static void printCliHelp() {
    printf("PortLens v%s - Windows 端口占用检测工具\n\n", PORTLENS_VERSION);
    printf("用法: port [选项]  （已安装到 PATH 后任意目录可用）\n\n");
    printf("  port <端口>     直接检测端口（如 port 80），显示详情，按 K 释放端口\n");
    printf("  port <别名>     端口别名检测（如 port mysql，别名在 port.conf 配置）\n");
    printf("  port <进程名> [端口] [--sort pid|port|name]\n");
    printf("                  按进程名查相关端口（如 port java），可叠加端口过滤: port java 8080\n");
    printf("  port:<端口>     直达检测（如 port:8001，Win+R / 浏览器地址栏直接输入）\n");
    printf("  （无参数）      启动交互式菜单\n");
    printf("  -c <端口>      检测单个端口 (TCP+UDP)\n");
    printf("  -l [协议] [--export csv|json[:文件]]\n");
    printf("                  列出所有监听端口 (tcp/udp/all，默认 all)，可导出留档\n");
    printf("  -f <进程名>    按进程名反查端口 (部分匹配)\n");
    printf("  -s [起 止]     扫描端口范围（省略范围时使用 port.conf 默认值）\n");
    printf("  -a <端口>      从指定端口开始自动找可用端口\n");
    printf("  -w <端口> [--interval 毫秒] [--popup]\n");
    printf("                  定向监控端口，占用/空闲变化时提醒，Ctrl+C 退出\n");
    printf("  -k <端口,...>  批量释放端口，如 -k 8001,8002,8003\n");
    printf("  --history      查看最近结束的进程记录\n");
    printf("  --clear-history 清空历史记录\n");
    printf("  fav [list|add <端口>|remove <端口>]\n");
    printf("                  端口收藏夹，如 fav add 8001\n");
    printf("  --tray [端口...] 系统托盘常驻监控（不填端口则监控收藏夹）\n");
    printf("  --config       查看/生成配置文件\n");
    printf("  --install-path    将本程序目录加入用户 PATH (port 命令)\n");
    printf("  --uninstall-path  从用户 PATH 移除本程序目录\n");
    printf("  --gui          强制打开图形界面（直接双击 exe 也会打开）\n");
    printf("  --cli          强制使用命令行模式\n");
    printf("  -v, --version  显示版本\n");
    printf("  -h, --help     显示帮助\n\n");
    printf("退出码: 0=空闲/成功  1=被占用/无结果  2=参数或运行错误\n");
}

static int cliCheckPort(uint16_t port) {
    PortInfo infos[2] = {
        PortChecker::checkPort(port, "TCP"),
        PortChecker::checkPort(port, "UDP")
    };
    const char* labels[2] = {"TCP", "UDP"};

    bool occupied = false;
    printf("端口 %d:\n", port);
    for (int i = 0; i < 2; i++) {
        if (infos[i].pid > 0) {
            occupied = true;
            std::string name = ProcessManager::getProcessName(infos[i].pid);
            printf("  [%s] 被占用  %s (PID: %u)\n", labels[i], name.c_str(), infos[i].pid);
            std::string path = ProcessManager::getProcessPath(infos[i].pid);
            if (!path.empty()) {
                printf("        路径:   %s\n", path.c_str());
            }
        } else {
            printf("  [%s] 空闲\n", labels[i]);
        }
    }
    return occupied ? 1 : 0;
}

static int cliList(const std::string& protocol) {
    auto ports = PortChecker::getAllListeningPorts(protocol);
    std::sort(ports.begin(), ports.end(), [](const PortInfo& a, const PortInfo& b) {
        if (a.port != b.port) return a.port < b.port;
        return a.protocol < b.protocol;
    });
    printPortTable(ports);
    return ports.empty() ? 1 : 0;
}

// 直接按进程名检测: port java → 列出进程名包含 java 的所有进程及其监听端口（按进程分组编号）。
// 交互: K 结束全部进程（跳过系统关键进程）、数字编号结束对应进程、回车/空格退出；每次删除后刷新列表。
// 返回 0=找到相关进程，1=无匹配
static int checkByProcessName(const std::string& name, int portFilter = 0,
                              const std::string& sortMode = "port") {
    std::string lowerQuery = toLowerStr(name);

    struct ProcEntry {
        uint32_t pid;
        std::string name;
        std::string path;
        std::string portList;
        int portCount;
        int minPort;
    };

    bool firstRound = true;
    while (true) {
        auto ports = PortChecker::getAllListeningPorts("ALL");

        std::vector<ProcEntry> entries;
        int totalPorts = 0;
        for (const auto& p : ports) {
            if (p.pid == 0) continue;
            // 端口过滤: port java 8080 只保留 8080
            if (portFilter > 0 && p.port != portFilter) continue;

            std::string pname = ProcessManager::getProcessName(p.pid);
            if (toLowerStr(pname).find(lowerQuery) == std::string::npos) continue;

            std::string item = p.protocol + ":" + std::to_string(p.port);
            ProcEntry* found = NULL;
            for (auto& e : entries) {
                if (e.pid == p.pid) { found = &e; break; }
            }
            if (found) {
                found->portList += ", " + item;
                found->portCount++;
                if ((int)p.port < found->minPort) found->minPort = p.port;
            } else {
                ProcEntry e;
                e.pid = p.pid;
                e.name = pname;
                e.path = ProcessManager::getProcessPath(p.pid);
                e.portList = item;
                e.portCount = 1;
                e.minPort = p.port;
                entries.push_back(e);
            }
            totalPorts++;
        }

        // 排序: port(默认，按最小端口) / pid / name
        if (sortMode == "pid") {
            std::sort(entries.begin(), entries.end(),
                      [](const ProcEntry& a, const ProcEntry& b) { return a.pid < b.pid; });
        } else if (sortMode == "name") {
            std::sort(entries.begin(), entries.end(),
                      [](const ProcEntry& a, const ProcEntry& b) {
                          std::string an = toLowerStr(a.name);
                          std::string bn = toLowerStr(b.name);
                          if (an != bn) return an < bn;
                          return a.pid < b.pid;
                      });
        } else {
            std::sort(entries.begin(), entries.end(),
                      [](const ProcEntry& a, const ProcEntry& b) {
                          if (a.minPort != b.minPort) return a.minPort < b.minPort;
                          return a.pid < b.pid;
                      });
        }

        if (entries.empty()) {
            std::string scope = "进程名包含 '" + name + "'";
            if (portFilter > 0) scope += " 且端口为 " + std::to_string(portFilter);
            if (firstRound) {
                printInfo("没有找到" + scope + " 的端口占用");
                return 1;
            }
            printSuccess(scope + " 的占用已全部结束");
            pauseForKey("退出");
            return 0;
        }
        firstRound = false;

        std::cout << "\n  进程名包含 \"" << name << "\"";
        if (portFilter > 0) std::cout << " 且端口为 " << portFilter;
        std::cout << " 的端口占用 (共 " << entries.size()
                  << " 个进程, " << totalPorts << " 个监听端口):\n\n";
        for (size_t i = 0; i < entries.size(); i++) {
            const ProcEntry& e = entries[i];
            setColor(GREEN);
            printf("  [%d] ", (int)(i + 1));
            setColor(YELLOW);
            printf("%s", e.name.c_str());
            restoreColor();
            printf("  (PID: %d)\n", e.pid);
            if (!e.path.empty()) {
                printf("      路径: "); setColor(CYAN); printf("%s\n", e.path.c_str()); restoreColor();
            }
            printf("      端口: "); setColor(YELLOW); printf("%s\n", e.portList.c_str()); restoreColor();
            std::cout << "\n";
        }

        setColor(YELLOW);
        std::cout << "  按 [K] 结束全部进程，按 [编号] 结束对应进程，按 回车/空格 退出\n";
        restoreColor();
        std::cout.flush();

        int ch = _getch();
        if (ch == '\r' || ch == ' ' || ch == '\n' || ch == 27) {
            return 0;
        }
        if (ch == 'k' || ch == 'K') {
            std::cout << "\n";
            for (const auto& e : entries) {
                killOneProcess(e.pid, e.name, e.portList);
            }
            Sleep(300);  // 等待端口释放后刷新列表
            continue;
        }
        if (ch >= '1' && ch <= '9') {
            // 编号输入: 首键后等 600ms，期间继续按键则视为多位编号（最多 2 位）
            std::string num(1, (char)ch);
            std::cout << (char)ch;
            std::cout.flush();
            while (num.size() < 2) {
                bool more = false;
                for (int t = 0; t < 60; t++) {
                    if (_kbhit()) { more = true; break; }
                    Sleep(10);
                }
                if (!more) break;
                int c2 = _getch();
                if (c2 >= '0' && c2 <= '9') {
                    num += (char)c2;
                    std::cout << (char)c2;
                    std::cout.flush();
                } else {
                    break;
                }
            }
            std::cout << std::endl;
            int idx = atoi(num.c_str());
            if (idx < 1 || idx > (int)entries.size()) {
                printError("无效编号: " + num + " (范围 1-" + std::to_string(entries.size()) + ")");
                Sleep(800);
                continue;
            }
            const ProcEntry& e = entries[idx - 1];
            killOneProcess(e.pid, e.name, e.portList);
            Sleep(300);
            continue;
        }
        // 其他按键忽略，重新等待
    }
}

static int cliFindByProcess(const std::string& name) {
    std::string lowerQuery = toLowerStr(name);
    auto ports = PortChecker::getAllListeningPorts("ALL");

    std::vector<PortInfo> results;
    for (const auto& p : ports) {
        if (p.pid == 0) continue;
        std::string pname = ProcessManager::getProcessName(p.pid);
        if (toLowerStr(pname).find(lowerQuery) != std::string::npos) {
            results.push_back(p);
        }
    }

    printPortTable(results);
    if (results.empty()) {
        printf("未找到进程名包含 \"%s\" 的端口占用\n", name.c_str());
        return 1;
    }
    return 0;
}

static int cliScan(uint16_t startPort, uint16_t endPort) {
    auto ports = PortChecker::scanPorts(startPort, endPort);
    std::sort(ports.begin(), ports.end(), [](const PortInfo& a, const PortInfo& b) {
        if (a.port != b.port) return a.port < b.port;
        return a.protocol < b.protocol;
    });
    printPortTable(ports);
    printf("扫描 %d-%d: %d 个端口被占用\n", startPort, endPort, (int)ports.size());
    return ports.empty() ? 1 : 0;
}

static int cliFindAvailable(uint16_t preferred) {
    for (int i = 0; i < 10; i++) {
        uint16_t port = preferred + i;
        if (port < preferred) break;  // 溢出
        PortInfo tcpInfo = PortChecker::checkPort(port, "TCP");
        PortInfo udpInfo = PortChecker::checkPort(port, "UDP");
        if (tcpInfo.pid == 0 && udpInfo.pid == 0) {
            printf("可用端口: %d (从 %d 起第 %d 个)\n", port, preferred, i + 1);
            return 0;
        }
    }
    printf("从 %d 起连续 10 个端口均被占用\n", preferred);
    return 1;
}

static bool parsePort(const char* s, uint16_t& out) {
    if (!s || !*s) return false;
    char* end = NULL;
    long v = strtol(s, &end, 10);
    if (*end != '\0' || v < 1 || v > 65535) return false;
    out = static_cast<uint16_t>(v);
    return true;
}

// ========== 扩展命令 ==========

// 解析逗号分隔的端口列表: "8001,8002, 8003"
static std::vector<uint16_t> parsePortList(const std::string& s) {
    std::vector<uint16_t> result;
    size_t start = 0;
    while (start <= s.size()) {
        size_t comma = s.find(',', start);
        std::string item = (comma == std::string::npos)
                               ? s.substr(start)
                               : s.substr(start, comma - start);
        start = (comma == std::string::npos) ? s.size() + 1 : comma + 1;

        item.erase(std::remove_if(item.begin(), item.end(), ::isspace), item.end());
        if (item.empty()) continue;

        int v = atoi(item.c_str());
        if (v >= 1 && v <= 65535) result.push_back((uint16_t)v);
    }
    return result;
}

// 批量释放端口: port -k 8001,8002,8003
static int cliBatchKill(const std::string& portsArg) {
    auto wanted = parsePortList(portsArg);
    if (wanted.empty()) {
        printError("无效的端口列表，用法: -k 8001,8002,8003");
        return 2;
    }

    printTitle("批量释放端口");
    auto all = PortChecker::getAllListeningPorts("ALL");
    std::vector<uint32_t> handled;
    int failed = 0;

    for (uint16_t port : wanted) {
        uint32_t pid = 0;
        std::string proto;
        for (const auto& info : all) {
            if (info.port == port && info.pid > 0) {
                pid = info.pid;
                proto = info.protocol;
                break;
            }
        }
        if (pid == 0) {
            setColor(GREEN);
            printf("  [空闲] 端口 %d 无需处理\n", port);
            restoreColor();
            continue;
        }
        if (std::find(handled.begin(), handled.end(), pid) != handled.end()) {
            printf("  [跳过] 端口 %d 的进程 PID %u 已处理\n", port, pid);
            continue;
        }
        handled.push_back(pid);
        if (!killOneProcess(pid, ProcessManager::getProcessName(pid),
                            proto + ":" + std::to_string(port))) {
            failed++;
        }
    }

    Sleep(300);  // 等待系统回收

    int still = 0;
    for (uint16_t port : wanted) {
        if (PortChecker::checkPort(port, "TCP").pid > 0 ||
            PortChecker::checkPort(port, "UDP").pid > 0) {
            still++;
        }
    }

    printf("\n  合计 %d 个端口，仍被占用 %d 个\n", (int)wanted.size(), still);
    return (failed == 0 && still == 0) ? 0 : 1;
}

static std::string csvEscape(const std::string& s) {
    if (s.find_first_of(",\"\r\n") == std::string::npos) return s;
    std::string r = "\"";
    for (char c : s) {
        if (c == '"') r += "\"\"";
        else r += c;
    }
    r += "\"";
    return r;
}

static std::string jsonEscape(const std::string& s) {
    std::string r;
    for (char c : s) {
        switch (c) {
            case '"': r += "\\\""; break;
            case '\\': r += "\\\\"; break;
            case '\n': r += "\\n"; break;
            case '\r': r += "\\r"; break;
            case '\t': r += "\\t"; break;
            default:
                if ((unsigned char)c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                    r += buf;
                } else {
                    r += c;
                }
        }
    }
    return r;
}

// 导出端口列表为 CSV / JSON
static int cliExportPorts(const std::vector<PortInfo>& ports, const std::string& format,
                          const std::string& outFile) {
    std::string fmt = toLowerStr(format);
    if (fmt != "csv" && fmt != "json") {
        printError("导出格式仅支持 csv / json");
        return 2;
    }

    std::string path = outFile;
    if (path.empty()) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        char name[96];
        snprintf(name, sizeof(name), "ports_%04d%02d%02d_%02d%02d%02d.%s",
                 st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
                 fmt == "json" ? "json" : "csv");
        path = name;
    }

    std::string data;
    if (fmt == "json") {
        data = "[\n";
        for (size_t i = 0; i < ports.size(); i++) {
            const PortInfo& p = ports[i];
            std::string name = p.pid > 0 ? ProcessManager::getProcessName(p.pid) : "";
            std::string procPath = p.pid > 0 ? ProcessManager::getProcessPath(p.pid) : "";
            data += "  {\"port\": " + std::to_string(p.port) +
                    ", \"protocol\": \"" + jsonEscape(p.protocol) +
                    "\", \"localAddr\": \"" + jsonEscape(p.localAddr) +
                    "\", \"state\": \"" + jsonEscape(p.state) +
                    "\", \"pid\": " + std::to_string(p.pid) +
                    ", \"process\": \"" + jsonEscape(name) +
                    "\", \"path\": \"" + jsonEscape(procPath) + "\"}";
            if (i + 1 < ports.size()) data += ",";
            data += "\n";
        }
        data += "]\n";
    } else {
        data = "\xEF\xBB\xBF";  // BOM: 便于 Excel 正确识别 UTF-8
        data += "端口,协议,本地地址,状态,PID,进程名,进程路径\n";
        for (const auto& p : ports) {
            std::string name = p.pid > 0 ? ProcessManager::getProcessName(p.pid) : "";
            std::string procPath = p.pid > 0 ? ProcessManager::getProcessPath(p.pid) : "";
            data += std::to_string(p.port) + "," + csvEscape(p.protocol) + "," +
                    csvEscape(p.localAddr) + "," + csvEscape(p.state) + "," +
                    std::to_string(p.pid) + "," + csvEscape(name) + "," +
                    csvEscape(procPath) + "\n";
        }
    }

    if (!ConfigManager::writeTextFile(path, data)) {
        printError("写入文件失败: " + path);
        return 2;
    }
    printSuccess("已导出 " + std::to_string(ports.size()) + " 条记录到 " + path);
    return 0;
}

static int cliHistory() {
    auto records = HistoryManager::list(0);
    printTitle("结束进程历史");
    printf("  历史文件: %s\n", HistoryManager::getHistoryPath().c_str());
    printf("  保留条数: %d\n\n", ConfigManager::historyLimit());

    if (records.empty()) {
        printInfo("暂无记录（结束进程后会自动记录）");
        return 1;
    }

    printf("  %-19s %-8s %-20s %-18s %s\n", "时间", "PID", "进程", "端口", "路径");
    for (const auto& r : records) {
        printf("  %-19s %-8u %-20s %-18s %s\n", r.time.c_str(), r.pid,
               r.procName.c_str(), r.ports.c_str(), r.path.c_str());
    }
    return 0;
}

static int cliClearHistory() {
    if (!HistoryManager::clear()) {
        printError("清空失败");
        return 2;
    }
    printSuccess("历史记录已清空");
    return 0;
}

static int cliFavorites(int argc, char* argv[]) {
    std::string sub = (argc > 2) ? toLowerStr(argv[2]) : "list";

    if (sub == "add") {
        uint16_t port;
        if (!parsePort(argc > 3 ? argv[3] : NULL, port)) {
            printError("用法: port fav add <端口>");
            return 2;
        }
        if (!FavoritesManager::add(port)) {
            printError("保存收藏失败");
            return 2;
        }
        printSuccess("已收藏端口 " + std::to_string(port));
        return 0;
    }

    if (sub == "remove" || sub == "rm" || sub == "del") {
        uint16_t port;
        if (!parsePort(argc > 3 ? argv[3] : NULL, port)) {
            printError("用法: port fav remove <端口>");
            return 2;
        }
        if (!FavoritesManager::remove(port)) {
            printError("该端口不在收藏夹中");
            return 1;
        }
        printSuccess("已移除收藏端口 " + std::to_string(port));
        return 0;
    }

    if (sub != "list" && sub != "ls") {
        printError("用法: port fav [list | add <端口> | remove <端口>]");
        return 2;
    }

    auto ports = FavoritesManager::list();
    printTitle("端口收藏夹");
    if (ports.empty()) {
        printInfo("收藏夹为空，添加: port fav add 8001");
        return 1;
    }

    printf("  %-8s %-8s %s\n", "端口", "状态", "详情");
    int occupied = 0;
    for (int port : ports) {
        PortInfo tcp = PortChecker::checkPort((uint16_t)port, "TCP");
        PortInfo udp = PortChecker::checkPort((uint16_t)port, "UDP");
        bool occ = tcp.pid > 0 || udp.pid > 0;

        std::string detail = getPortDesc(port);
        if (occ) {
            uint32_t pid = tcp.pid > 0 ? tcp.pid : udp.pid;
            std::string name = ProcessManager::getProcessName(pid);
            std::string tag = SpecialProcessDetector::detect(name, port);
            detail = name + tag + " (PID: " + std::to_string(pid) + ")";
            occupied++;
            setColor(RED);
        } else {
            setColor(GREEN);
        }
        printf("  %-8d %-8s %s\n", port, occ ? "被占用" : "空闲", detail.c_str());
        restoreColor();
    }

    printf("\n  共 %d 个收藏，%d 个被占用\n", (int)ports.size(), occupied);
    return 0;
}

static int cliConfigInfo() {
    bool created = ConfigManager::writeDefaultConfigIfMissing();
    ConfigManager::reload();

    printTitle("配置文件");
    printf("  路径: %s\n", ConfigManager::getConfigPath().c_str());
    if (created) {
        printSuccess("已生成配置模板，编辑后立即生效");
    } else {
        printInfo("配置文件已存在");
    }

    auto aliases = ConfigManager::listAliases();
    if (!aliases.empty()) {
        printf("\n  端口别名 (%d 个):\n", (int)aliases.size());
        for (const auto& a : aliases) {
            printf("    %-14s → %d\n", a.first.c_str(), a.second);
        }
    }

    printf("\n  默认扫描范围: %d-%d\n", ConfigManager::defaultScanStart(), ConfigManager::defaultScanEnd());
    printf("  历史保留条数: %d\n", ConfigManager::historyLimit());
    printf("  监控刷新间隔: %d ms\n", ConfigManager::watchIntervalMs());
    printf("  历史文件: %s\n", HistoryManager::getHistoryPath().c_str());
    printf("  收藏文件: %s\n", FavoritesManager::getFavoritesPath().c_str());
    return 0;
}

static int cliWatch(int argc, char* argv[]) {
    uint16_t port;
    if (!parsePort(argc > 2 ? argv[2] : NULL, port)) {
        printError("用法: -w <端口> [--interval 毫秒] [--popup]");
        return 2;
    }

    int interval = ConfigManager::watchIntervalMs();
    bool popup = false;
    for (int i = 3; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--popup") {
            popup = true;
        } else if (a == "--interval" && i + 1 < argc) {
            int v = atoi(argv[++i]);
            if (v >= 200) interval = v;
        }
    }
    return runPortWatch(port, interval, popup);
}

static int cliTray(int argc, char* argv[]) {
    std::vector<uint16_t> ports;
    for (int i = 2; i < argc; i++) {
        uint16_t p;
        if (parsePort(argv[i], p)) ports.push_back(p);
    }
    // 未指定端口时默认监控收藏夹
    if (ports.empty()) {
        for (int p : FavoritesManager::list()) {
            ports.push_back((uint16_t)p);
        }
    }
    return runTray(ports, ConfigManager::watchIntervalMs());
}

static int runCli(int argc, char* argv[]) {
    std::string cmd = argv[1];

    if (cmd == "-h" || cmd == "--help") {
        printCliHelp();
        return 0;
    }
    if (cmd == "-v" || cmd == "--version") {
        printf("PortLens v%s\n", PORTLENS_VERSION);
        return 0;
    }

    if (cmd == "--install-path") {
        bool pathOk = installToPath();
        bool uriOk = registerPortProtocol();
        if (pathOk || uriOk) {
            if (pathOk) printf("已将本程序目录加入用户 PATH\n");
            if (uriOk) printf("已注册 port: 协议 (Win+R 输入 port:8001 可直达检测)\n");
            printf("新开一个 CMD 窗口，输入 port 即可使用\n");
            return 0;
        }
        printError("安装失败（注册表写入被拒绝）");
        return 2;
    }

    if (cmd == "--uninstall-path") {
        bool pathRemoved = removeFromPath();
        bool uriRemoved = unregisterPortProtocol();
        if (pathRemoved || uriRemoved) {
            printf("已卸载: PATH 条目%s，port: 协议%s\n",
                pathRemoved ? "已移除" : "(未找到)",
                uriRemoved ? "已注销" : "(未找到)");
            return 0;
        }
        printError("未找到本程序的 PATH 条目和 port: 协议，无需卸载");
        return 2;
    }

    if (cmd == "-c") {
        uint16_t port;
        if (!parsePort(argc > 2 ? argv[2] : NULL, port)) {
            printError("无效的端口号，用法: -c <端口> (1-65535)");
            return 2;
        }
        return cliCheckPort(port);
    }

    if (cmd == "-l") {
        std::string protocol = "ALL";
        int idx = 2;
        if (argc > idx && argv[idx][0] != '-') {
            std::string p = argv[idx];
            if (p == "tcp" || p == "TCP") { protocol = "TCP"; idx++; }
            else if (p == "udp" || p == "UDP") { protocol = "UDP"; idx++; }
            else if (p == "all" || p == "ALL") { protocol = "ALL"; idx++; }
            else {
                printError("无效的协议，可选: tcp / udp / all");
                return 2;
            }
        }

        // 解析 --export <csv|json[:文件]> 或 --export=<...>
        std::string exportFmt, exportFile;
        for (int i = 2; i < argc; i++) {
            std::string a = argv[i];
            std::string val;
            if (a == "--export") {
                if (i + 1 >= argc) {
                    printError("用法: --export csv|json[:文件]");
                    return 2;
                }
                val = argv[++i];
            } else if (a.rfind("--export=", 0) == 0) {
                val = a.substr(9);
            } else {
                continue;
            }
            size_t colon = val.find(':');
            exportFmt = (colon == std::string::npos) ? val : val.substr(0, colon);
            if (colon != std::string::npos) exportFile = val.substr(colon + 1);
        }

        if (!exportFmt.empty()) {
            auto ports = PortChecker::getAllListeningPorts(protocol);
            std::sort(ports.begin(), ports.end(), [](const PortInfo& a, const PortInfo& b) {
                if (a.port != b.port) return a.port < b.port;
                return a.protocol < b.protocol;
            });
            printPortTable(ports);
            return cliExportPorts(ports, exportFmt, exportFile);
        }
        return cliList(protocol);
    }

    if (cmd == "-f") {
        if (argc < 3 || !argv[2][0]) {
            printError("缺少进程名，用法: -f <进程名>");
            return 2;
        }
        return cliFindByProcess(argv[2]);
    }

    if (cmd == "-s") {
        uint16_t startPort, endPort;
        if (argc <= 2) {
            // 未指定范围时使用配置文件中的默认范围
            startPort = (uint16_t)ConfigManager::defaultScanStart();
            endPort = (uint16_t)ConfigManager::defaultScanEnd();
            return cliScan(startPort, endPort);
        }
        if (!parsePort(argc > 2 ? argv[2] : NULL, startPort) ||
            !parsePort(argc > 3 ? argv[3] : NULL, endPort)) {
            printError("无效的端口范围，用法: -s <起> <止>");
            return 2;
        }
        if (startPort > endPort) {
            printError("起始端口不能大于结束端口");
            return 2;
        }
        return cliScan(startPort, endPort);
    }

    if (cmd == "-a") {
        uint16_t preferred;
        if (!parsePort(argc > 2 ? argv[2] : NULL, preferred)) {
            printError("无效的端口号，用法: -a <端口>");
            return 2;
        }
        return cliFindAvailable(preferred);
    }

    if (cmd == "-w" || cmd == "--watch") {
        return cliWatch(argc, argv);
    }

    if (cmd == "-k" || cmd == "--kill") {
        if (argc < 3 || !argv[2][0]) {
            printError("缺少端口列表，用法: -k 8001,8002,8003");
            return 2;
        }
        return cliBatchKill(argv[2]);
    }

    if (cmd == "--history") {
        return cliHistory();
    }

    if (cmd == "--clear-history") {
        return cliClearHistory();
    }

    if (cmd == "--tray") {
        return cliTray(argc, argv);
    }

    if (cmd == "fav" || cmd == "--fav") {
        return cliFavorites(argc, argv);
    }

    if (cmd == "config" || cmd == "--config") {
        return cliConfigInfo();
    }

    printError("未知选项: " + cmd);
    printCliHelp();
    return 2;
}

// ========== 主函数 ==========

static int runMain(int argc, char* argv[]) {
    // 设置 UTF-8 代码页
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);

    // 初始化控制台颜色
    saveColor();

    // 纯命令行构建下 --cli 只是模式提示，直接忽略该参数
    if (argc > 1) {
        std::vector<char*> keep;
        keep.push_back(argv[0]);
        for (int i = 1; i < argc; i++) {
            if (std::string(argv[i]) != "--cli") keep.push_back(argv[i]);
        }
        argc = (int)keep.size();
        for (int i = 0; i < argc; i++) argv[i] = keep[i];
    }

    if (!PortChecker::init()) {
        printError("Winsock 初始化失败");
        return 2;
    }

    // 带参数: 命令行模式，执行完直接退出
    if (argc > 1) {
        std::string first = argv[1];

        // 子命令优先（fav / config 这类单词不能被当作进程名）
        if (first == "fav" || first == "--fav" || first == "config" || first == "--config") {
            int rc = runCli(argc, argv);
            PortChecker::cleanup();
            return rc;
        }

        // 直接检测形式: port 80 / port :80 / port port:80 / port:80（URI 协议启动）
        uint16_t directPort;
        int dc = classifyDirectPort(argv[1], directPort);
        if (dc == 1) {
            printHeader();
            int occ = checkSinglePort(directPort, false);
            if (!occ) pauseForKey("退出");
            PortChecker::cleanup();
            return occ;
        }
        if (dc == 2) {
            printError("无效的端口号: " + std::string(argv[1]) + " (范围 1-65535)");
            PortChecker::cleanup();
            return 2;
        }

        // 端口别名: port mysql → 3306（配置文件 [alias] 段）
        int aliasPort = ConfigManager::resolveAlias(first);
        if (aliasPort > 0) {
            printHeader();
            printf("  端口别名: %s → %d\n", first.c_str(), aliasPort);
            int occ = checkSinglePort((uint16_t)aliasPort, false);
            if (!occ) pauseForKey("退出");
            PortChecker::cleanup();
            return occ;
        }

        // 进程名直达: port java [端口] [--sort pid|port|name]
        if (isProcessNameForm(argv[1])) {
            int portFilter = 0;
            std::string sortMode = "port";
            for (int i = 2; i < argc; i++) {
                std::string a = argv[i];
                if (a == "--sort" && i + 1 < argc) {
                    sortMode = toLowerStr(argv[++i]);
                } else if (!a.empty() && a[0] != '-') {
                    uint16_t p;
                    if (parsePort(a.c_str(), p)) portFilter = p;
                }
            }
            printHeader();
            int rc = checkByProcessName(argv[1], portFilter, sortMode);
            PortChecker::cleanup();
            return rc;
        }

        int rc = runCli(argc, argv);
        PortChecker::cleanup();
        return rc;
    }

    // 双击启动（交互模式）: 首次运行自动把本目录加入 PATH，
    // 之后在任意 CMD 输入 port 即可调用；已安装则静默跳过
    if (!isExeDirInPath()) {
        if (installToPath()) {
            printSuccess("已安装 port 命令: 新开 CMD 窗口后，任意目录输入 port 即可使用");
            printInfo("卸载: port --uninstall-path");
        }
    }
    // 注册/刷新 port: URI 协议（exe 移动位置后自动指向新路径）
    registerPortProtocol();

    while (true) {
        printHeader();
        printMenu();

        setColor(GREEN);
        std::cout << "  请选择功能 (输入数字或直接输入端口号): ";
        restoreColor();
        std::cout.flush();

        // 单键 0-9 = 菜单项；连续多位数字 = 端口号快速检测。
        // 首键后等待 600ms：期间继续按键则视为端口输入，超时则执行菜单项
        std::string input;
        while (true) {
            int ch = _getch();
            if (ch >= '0' && ch <= '9') {
                if (input.size() >= 5) continue;  // 端口最多 5 位
                input += (char)ch;
                std::cout << (char)ch;
                std::cout.flush();
                if (input.size() == 5) break;
                bool more = false;
                for (int t = 0; t < 60; t++) {
                    if (_kbhit()) { more = true; break; }
                    Sleep(10);
                }
                if (more) continue;
                break;  // 超时，输入结束
            } else if ((ch == '\r' || ch == '\n') && !input.empty()) {
                break;  // 回车确认端口输入
            } else if (input.empty() && ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z'))) {
                // 字母快捷键: F/H/W/T
                input += (char)tolower(ch);
                std::cout << (char)toupper(ch);
                std::cout.flush();
                break;
            } else if (ch == '\b' && !input.empty()) {
                input.pop_back();
                std::cout << "\b \b";
                std::cout.flush();
            }
            // 其他按键忽略
        }
        std::cout << std::endl;

        if (input.size() >= 2) {
            // 多位数字: 判定为端口号，直接检测
            int port = atoi(input.c_str());
            if (port >= 1 && port <= 65535) {
                if (!checkSinglePort(port)) pauseForKey();
            } else {
                printError("无效端口号: " + input + " (范围 1-65535)");
                pauseForKey();
            }
            continue;
        }

        if (input.empty()) continue;

        char choice = input[0];

        switch (choice) {
            case '1':
                if (!checkSinglePort()) pauseForKey();
                break;
            case '2':
                listAllPorts();
                pauseForKey();
                break;
            case '3':
                scanPorts();
                pauseForKey();
                break;
            case '4':
                killProcessMenu();
                pauseForKey();
                break;
            case '5':
                showProcessDetail();
                pauseForKey();
                break;
            case '6':
                showConnectionDetail();
                pauseForKey();
                break;
            case '7':
                findPortsByProcess();
                pauseForKey();
                break;
            case '8':
                monitorPort();
                pauseForKey();
                break;
            case '9':
                findAvailablePortMenu();
                pauseForKey();
                break;
            case '0':
                printInfo("正在退出...");
                PortChecker::cleanup();
                Sleep(300);
                return 0;
            case 'f': {
                char* args[] = {(char*)"port", (char*)"fav", (char*)"list", NULL};
                cliFavorites(3, args);
                pauseForKey();
                break;
            }
            case 'h':
                cliHistory();
                pauseForKey();
                break;
            case 'w': {
                int port = 0;
                readInt("要监控的端口", 8001, 1, 65535, port);
                if (port > 0) {
                    runPortWatch((uint16_t)port, ConfigManager::watchIntervalMs(), false);
                    pauseForKey();
                }
                break;
            }
            case 't': {
                char* args[] = {(char*)"port", (char*)"--tray", NULL};
                cliTray(2, args);
                pauseForKey();
                break;
            }
            default:
                // 无效输入，不做任何事，重新显示菜单
                break;
        }
    }

    PortChecker::cleanup();
    return 0;
}

// ========== GUI 桥接（Rust 入口调用） ==========

#ifndef PORTLENS_NO_MAIN
// 纯 CLI 构建时保留标准 main
int main(int argc, char* argv[]) {
    return runMain(argc, argv);
}
#endif

extern "C" int portlens_cli_main(int argc, char* argv[]) {
    return runMain(argc, argv);
}

extern "C" int portlens_kill_one(unsigned int pid, const char* procName, const char* portsText) {
    return killOneProcess(pid, procName ? procName : "", portsText ? portsText : "") ? 1 : 0;
}

extern "C" int portlens_install_path(void) {
    bool pathOk = installToPath();
    bool uriOk = registerPortProtocol();
    return (pathOk || uriOk) ? 1 : 0;
}
